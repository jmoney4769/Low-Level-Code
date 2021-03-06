!============================================================
! CS-2200 Homework 1
! Please do not change main, except to change the argument for factorial
!============================================================

main:	la	$sp, stack		! load address of stack label into $sp
		la	$at, factorial	! load address of factorial label into $at
		addi $a0, $zero, 4 	! $a0 = 4, the number to factorialize
		jalr $at, $ra		! jump to factorial, set $ra to return addr
		halt			! when we return, just halt

factorial:	addi $t0, $zero, 1	! set t0 = 1
		beq $t0, $t2, fpExists	! frame pointer has already been intialized, so skip init
		add $pr, $sp, $zero	! set pr = sp

fpExists:	beq $zero, $a0, endZero	! end if a0 == 0
		beq $t0, $a0, end	! end if a0 == 1
				
		sw $ra, 0x00($pr)	! push the return address onto the stack
		sw $a0, 0x01($pr)	! push the current value of n onto the stack
		addi $pr, $pr, 2	! increment the frame pointer
		addi $a0, $a0, -1	! decrement a0
		addi $t2, $zero, 1	! set t2 so we know we've been through at least once
		jalr $at, $ra		! recursive call

endZero:	addi $a0, $a0, 1	! set a0 to 1 because fact(0) is 1		
end:		beq $pr, $sp, allDone	! return when the pr == sp, as all values will be multiplied	
		addi $pr, $pr, -2	! decrement pr so we can reach the values we want
		lw $a1, 0x01($pr) 	! get the value from pr
		la $t0, mult		! load the address of mult into t0
		jalr $t0, $ra		! multiply the two numbers
		add $a0, $v0, $zero	! set a0 to the result
		la $t0, end		! load the address of end into t0
		jalr $t0, $ra		! go back to end

allDone:	add $v0, $a0, $zero	! set the return value to the end value
		lw $ra, 0x00($sp)	! get the original return address from the stack
		jalr $ra, $t0		! return to the main function

mult:		add $v0, $a0, $zero 	! save the first argument
		add $t1, $a1, $zero	! save the other argument
		addi $t1, $t1, -1 	! decrement the other variable:  in factorials, we don't have to worry about zero

loop:		beq $t1, $zero, endMult	! if we are done, go back
		add $v0, $a0, $v0	! do one step of multiply
		addi $t1, $t1, -1	! decrement other variable
		beq $zero, $zero, loop	! loop until done 

endMult:	jalr $ra, $t0		! go back

stack:	.byte 100 			! the stack begins here (for example, that is)

! factorial C code, for reference:
! fact(int num) {
! 	if (num <= 1) {
!		return 1;
!	}
!	return num * fact(num - 1);
! }
