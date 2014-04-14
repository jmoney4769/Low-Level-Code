#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rtp.h"

void print_use_and_exit(){
	fprintf(stderr,"Usage:  prj5-client host port\n\n");
	fprintf(stderr,"   example ./prj5-client 127.0.0.1 4000\n\n");
	exit (EXIT_FAILURE);
}

int main(int argc, char**argv){
	char *host,*port;
	char* messages[5];
	messages[0] = "C is quirky flawed and an enormous success - Dennis M. Ritchie";
	messages[1] = "The question of whether computers can think is like the question of whether submarines can swim - Edsger W. Dijkstra";
	messages[2] = "The citys central computer told you?  R2D2, you know better than to trust a strange computer! - C3P0";
	messages[3] = "There are two major products that come out of Berkeley, LSD and UNIX.  We dont believe this to be a coincidence - Jeremy S. Anderson";
	messages[4] = "The Internet?  Is that thing still around? - Homer Simpson";
	int num_msgs = 5;

	if(argc < 3 || argc > 3){
		printf("Incorrect number of args.\n");
		print_use_and_exit();
	}else{
		host = argv[1];
		port = argv[2];
	}

	CONN_INFO* connection = setup_socket(host, port);
	if(connection == NULL) print_use_and_exit();

	for(int i = 0; i < num_msgs; i++){
		MESSAGE*msg = malloc(sizeof(MESSAGE));
		msg->buffer = messages[i];
		msg->length = strlen(messages[i])+1;

		rtp_send_message(connection, msg);
		printf("Sent message %i to server...\n", i);

		MESSAGE*response = rtp_receive_message(connection);
		if(response) printf("Received reponse:\n\t%s\n\n", response->buffer);
		else{
			perror("Error must have occurred while receiving response. No message returned\n");
			exit(EXIT_FAILURE);
		}
	}
	shutdown_socket(connection);

	return 0;
}
