#!/usr/bin/perl
use Term::ANSIColor;
use Term::ReadLine;

my $infile = undef;
my $symfile = undef;
my $binary = undef;

my $instr_count = 0;
my $running = 1;
my $executing = 0;
my $halted = 0;
my @breakpoints;

my $pc = 0;
my @mem;
my @regs;
$regs[$_] = 0 foreach 0 .. 15;

my @reg_names = ( 'zero', 'at', 'v0', 'a0', 'a1', 'a2', 't0', 't1', 't2',
	's0', 's1', 's2', 'k0', 'sp', 'fp', 'ra' );
my @instr_names = ( 'add', 'nand', 'addi', 'lw', 'sw', 'beq', 'jalr', 'spop' );



# ---------- parse arguments ----------

foreach (@ARGV) {
	if($_ eq "-b" || $_ eq "--binary") {
		$binary = 1;
	} elsif($_ eq "-h" || $_ eq "--help") {
		&help_and_exit();
	} elsif ($_ !~ /^(\-.*)/) {
		$infile = $_;
	} else {
		print STDERR "Unrecognized flag: $1\n";
		&help_and_exit();
	}
}

if(!(defined $infile)) {
	&help_and_exit();
}



# ---------- load machine code into memory ----------

&sim_msg('black',"Loading $infile...");
unless (open(ASM, "< $infile")) {
	&sim_msg('red',"Could not open file $infile.\n   ($!)\n");
	&help_and_exit;
}

if (defined $binary) {
	binmode(ASM);
	my $word_count = -s ASM;
	@mem = unpack "n$word_count", <ASM>;
} else {
	@mem = split " ", <ASM>;
	$mem[$_] = hex($mem[$_]) foreach 0 .. $#mem;
}
close ASM;



# ---------- set up symbol table ----------

my %symbols;
if ($infile =~ /(.+\/)?([^.]+)(\.|$)/) {
	$symfile = $1.$2.'.sym';
}

if (defined $symfile) {
	if (open(SYM, "< $symfile")) {
		&sim_msg('black',"Loading labels from $symfile.");
		while (<SYM>) {
			$symbols{$2} = $1 if $_ =~ /^\/\/\t(\w+) (\w{4})/;
		}
		close SYM;
		&sim_msg('black','Labels loaded successfully.');

	} else {
		&sim_msg('yellow',"Could not open symbol file $symfile;".
			" labels not displayed.\n   ($!)\n");
	}

} else {
	&sim_msg('yellow',"Could not determine symbol file name (from $infile).");
}



# ---------- run simulator ----------

print STDERR <<EOF;

Welcome to the new LC-2200 text interface. (version 1.0)

		 byte = 16 bits; address = 16 bits
		 word = 16 bits (2 bytes), instruction = 16 bits (2 bytes)

For help, type in this command: help
EOF

my $term;
eval {
	$term = new Term::ReadLine 'LC2200';
	$term->Attribs->ornaments(0);
	print "\n";
}
	or print "For command history, install Term::ReadLine::Gnu or appropriate".
" module\n";

&dump_regs;
while ($running) {
	my $cmd = $term->readline('(sim) ');
	chomp $cmd;
	my @args = split " ", $cmd;
	$cmd = $args[0];
	if ($cmd eq 'q' or $cmd eq 'quit' or $cmd eq 'exit') {
		print "\nHave a nice day!\n";
		$running = 0;
	} elsif ($cmd eq 'r' or $cmd eq 'run' or $cmd eq 'c' or $cmd eq 'continue') {
		&run;
	} elsif ($cmd eq 's' or $cmd eq 'step') {
		&step;
	} elsif ($cmd eq 'm' or $cmd eq 'mem') {
		if (@args == 1){
			&mem
		}elsif (@args == 2){
			&mem(@args[1])
		}else{
			&mem(@args[1], @args[2])
		}
	} elsif ($cmd eq 'break') {
		&set_breakpoint($args[1]);
	} elsif ($cmd eq 'h' or $cmd eq 'help' or $cmd eq '?') {
		&print_help;
	} else {
		&sim_msg('red',"Unknown command: $cmd.");
		&sim_msg('black','For help on commands and usage, use the "help" command.');
	}
}



# ---------- LC2200 instructions ----------

sub add {
	my ($rx, $ry, $rz) = @_;
	$regs[$rx] = $regs[$ry] + $regs[$rz];
}

sub nand {
	my ($rx, $ry, $rz) = @_;
	$regs[$rx] = ~($regs[$ry] & $regs[$rz]);
}

sub addi {
	my ($rx, $ry, $offset) = @_;
	$regs[$rx] = $regs[$ry] + $offset;
}

sub lw {
	my ($rx, $ry, $offset) = @_;
	my $data = $mem[$regs[$ry] + $offset];
	if (!(defined $data)) {
		$data = 0;
	}
	$regs[$rx] = $data;
}

sub sw {
	my ($rx, $ry, $offset) = @_;
	$mem[$regs[$ry] + $offset] = $regs[$rx];
}

sub beq {
	my ($rx, $ry, $offset) = @_;
	$pc = $pc + $offset if &trim($regs[$rx]) == &trim($regs[$ry]);
}

sub jalr {
	my ($rx, $ry) = @_;
	$regs[$ry] = $pc + 1;
	$pc = $regs[$rx] - 1;
}

sub spop {
	my $cc = shift;
	if ($cc == 0) {
		&halt;
	}
}

sub halt {
	$halted = 1;
	$executing = 0;
}



# ---------- simulator instructions ----------

sub step {
	my ($opcode, $rx, $ry, $rz, $offset, $cc) = &decode($mem[$pc]);

	if ($opcode == 0) {
		&add($rx, $ry, $rz);

	} elsif ($opcode == 1) {
		&nand($rx, $ry, $rz);

	} elsif ($opcode == 2) {
		&addi($rx, $ry, $offset);

	} elsif ($opcode == 3) {
		&lw($rx, $ry, $offset);

	} elsif ($opcode == 4) {
		&sw($rx, $ry, $offset);

	} elsif ($opcode == 5) {
		&beq($rx, $ry, $offset);

	} elsif ($opcode == 6) {
		&jalr($rx, $ry);

	} else {
		&spop($cc);
	}

	$regs[0] = 0;
	$instr_count += 1;
	$pc += 1 if not $halted;

	foreach (@breakpoints) {
		if ($pc == $_) {
			&sim_msg('black','Encountered breakpoint.');
			$executing = 0;
		}
	}
	&dump_regs unless $executing;
	if ($halted) {
		&sim_msg('black',"Machine halted.  Total executed: $instr_count.");
		$halted = 0;
	}
}

sub run {
	$executing = 1;
	while ($executing) {
		&step;
	}
}

sub set_breakpoint {
	my $loc = shift;
	chomp $loc;

	my $is_sym = 0;
	my $valid = 1;

	if ($loc =~ /\D/) {
		foreach my $addr (keys %symbols) {
			if ($symbols{$addr} eq $loc) {
				$loc = hex($addr);
				$is_sym = 1;
			}
		}
		unless ($is_sym) {
			if ($loc =~ /^0?x(\d|[AaBbCcDdEeFf])+/) {
				$loc = hex($loc);
			} else {
				&sim_msg('black',"::  -->$loc<--");
				&sim_msg('red'," `-> The string '$loc' is not a valid label".
					' or expression.');
				$valid = 0;
			}
		}
	} elsif ($loc =~ /^0\d+/) {
		$loc = oct($loc);
	}

	if ($valid) {
		my $found = 0;
		foreach my $i (keys @breakpoints) {
			if ($loc == $breakpoints[$i]) {
				splice(@breakpoints, $i, 1);
				$found = 1;
				&sim_msg('black',sprintf('Breakpoint at x%0.4X is now CLEARED.', $loc));
			}
		}
		unless ($found) {
			$breakpoints[$#breakpoints + 1] = $loc;
			&sim_msg('black',sprintf('Breakpoint at x%0.4X is now set.', $loc));
		}
	}
}

sub mem {
	$start = 0;
	$end = 15;
	if (@_ == 0){
		#print "0 args\n"
	}
	elsif (@_ == 1){
		$start = &addr_val($_[0]);
		$end = $start;
	}elsif (@_ == 2){
		$start = &addr_val($_[0]);
		$end = &addr_val($_[1]);
	}else{
		&sim_msg('red', "Incorrect usage, should only be 0-2 mem addresses\n");
	}
	if ($start == -1 or $end == -1 or $start > $end){
		return 0;
	}

	foreach $addr ($start .. $end){
		printf "MEM[%X]:   \t%X\n", $addr, $mem[$addr];
	}
}

sub addr_val {
	$val = shift;
	if ($val =~ /\D/) {
		if ($val =~ /^0?x(\d|[AaBbCcDdEeFf])+/) {
			$val = hex($val);
		} else {
			&sim_msg('black',"::  -->$val<--");
			&sim_msg('red'," `-> The string '$val' is not a valid address");
			return -1;
		}
	} elsif ($val =~ /^0\d+/) {
		$val = oct($val);
	}
	return $val
}

sub decode {
	my $instr = shift;
	my $opcode = ($instr >> 13) & 7;

	my $rx = ($instr >> 9) & 15;
	my $ry = ($instr >> 5) & 15;
	my $rz = ($instr >> 1) & 15;
	my $offset = $instr & 15;
	if ($instr & 16) {
		$offset = 0 - (($offset ^ 15) + 1);
	}
	my $cc = $instr & 3;

	return ($opcode, $rx, $ry, $rz, $offset, $cc);
}

sub dump_regs {
	foreach my $row (0 .. 3) {
		foreach my $col (0 .. 3) {
			printf " %.2s", $reg_names[$row + $col * 4];
			my $reg_val = substr(sprintf("%0.4X", $regs[$row + $col * 4]), -4);
			print ' '.$reg_val;
			if (defined $symbols{$reg_val}) {
				printf " %-10.10s", $symbols{$reg_val};
			} else {
				printf " %-10d", &trim($regs[$row + $col * 4]);
			}
		}
		print "\n";
	}

	printf " - Now at x%0.4X: ", $pc;
	my ($opcode, $rx, $ry, $rz, $offset, $cc) = &decode($mem[$pc]);
	if ($opcode == 7) {
		if ($cc == 0) {
			print 'halt';
		} else {
			print 'unknown spop';
		}
	} elsif ($opcode == 6) {
		print $instr_names[$opcode].' $'.$reg_names[$rx].', $'.$reg_names[$ry];
	} elsif ($opcode == 5) {
		my $dest = sprintf("%0.4X", $offset + $pc + 1);
		print $instr_names[$opcode].' $'.
		$reg_names[$rx].', $'.$reg_names[$ry].', '.
		(defined $symbols{$dest} ? $symbols{$dest} : sprintf("x%0.4X", $offset));
	} elsif ($opcode > 2) {
		print $instr_names[$opcode].' $'.$reg_names[$rx].', '.
		'#'.$offset.'($'.$reg_names[$ry].')';
	} elsif ($opcode == 2) {
		print $instr_names[$opcode].' $'.$reg_names[$rx].', $'.$reg_names[$ry].
		', #'.$offset;
	} else {
		print $instr_names[$opcode].' $'.$reg_names[$rx].', $'.$reg_names[$ry].
		', $'.$reg_names[$rz];
	}

	print "\n";
}

# this subroutine trims a value to only it's least 4 significant hex digits.
# easily the sketchiest part of the simulator, this emulates 16-bit values 
#   even though the acutal values used by perl are of unknown size.
# currently (v1.1) only used for printing register contents and for comparing 
#   register contents during beq instruction.
sub trim {
	my $val = shift;
	return $val % hex('0x10000');
}

sub sim_msg {
	my ($color, $msg) = @_;
	print color "bold $color";
	print ' [sim]';
	print color 'reset';
	print " $msg\n";
}

sub print_help {
	print STDERR <<EOF;

 RUNNING THE CODE
   r[un] or c[ontinue]     resume execution until next breakpoint
   s[tep]                  execute one instruction
   break <addr/label>      set or clear a breakpoint
   h[elp] or ?             print this help message
   q[uit] or exit          exit the simulator

EOF
}

sub help_and_exit() {
	print STDERR <<EOF;

Usage:
lctext.pl [options] <input-file>

  List of options:
	-b, --binary      Read input file as binary instead of ASCII hex characters
	-h, --help        Print this help message

EOF
	exit 1;
}

