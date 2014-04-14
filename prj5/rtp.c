#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "rtp.h"

/* GIVEN Function:
 * Handles creating the client's socket and determining the correct
 * information to communicate to the remote server
 */
CONN_INFO* setup_socket(char* ip, char* port)
{
	struct addrinfo *connections, *conn = NULL;
	struct addrinfo info;
	memset(&info, 0, sizeof(struct addrinfo));
	int sock = 0;

	info.ai_family = AF_INET;
	info.ai_socktype = SOCK_DGRAM;
	info.ai_protocol = IPPROTO_UDP;
	getaddrinfo(ip, port, &info, &connections);

	/*for loop to determine corr addr info*/
	for (conn = connections; conn != NULL; conn = conn->ai_next)
	{
		sock = socket(conn->ai_family, conn->ai_socktype, conn->ai_protocol);
		if (sock < 0)
		{
			if (DEBUG)
				perror("Failed to create socket\n");
			continue;
		}
		if (DEBUG)
			printf("Created a socket to use.\n");
		break;
	}
	if (conn == NULL)
	{
		perror("Failed to find and bind a socket\n");
		return NULL;
	}
	CONN_INFO* conn_info = malloc(sizeof(CONN_INFO));
	conn_info->socket = sock;
	conn_info->remote_addr = conn->ai_addr;
	conn_info->addrlen = conn->ai_addrlen;
	return conn_info;
}

void shutdown_socket(CONN_INFO *connection)
{
	if (connection)
		close(connection->socket);
}

/* 
 * ===========================================================================
 *
 *			STUDENT CODE STARTS HERE. PLEASE COMPLETE ALL FIXMES
 *
 * ===========================================================================
 */

/*
 *  Returns a number computed based on the data in the buffer.
 */
static int checksum(char *buffer, int length)
{
	int sum = 0;
	for (int i = 0; i < length; i++)
	{
		sum += (int) buffer[i];
	}
	/*
	 *
	 *  The goal is to return a number that is determined by the contents
	 *  of the buffer passed in as a parameter.  There a multitude of ways
	 *  to implement this function.  For simplicity, simply sum the ascii
	 *  values of all the characters in the buffer, and return the total.
	 */
	return sum;
}

/*
 *  Converts the given buffer into an array of PACKETs and returns
 *  the array.  The value of (*count) should be updated so that it 
 *  contains the length of the array created.
 */
static PACKET* packetize(char *buffer, int length, int *count)
{
	int numPackets =
			(length % MAX_PAYLOAD_LENGTH == 0) ?
					length / MAX_PAYLOAD_LENGTH + 1 :
					length / MAX_PAYLOAD_LENGTH;

	PACKET *packetArray = calloc(numPackets, sizeof(PACKET));
	for (int i = 0; i < numPackets; i++)
	{
		// find payload length
		if (MAX_PAYLOAD_LENGTH * (i + 1) < length) // can we use the max?
		{
			packetArray[i].payload_length = MAX_PAYLOAD_LENGTH;
		}
		else // use the rest that you can
		{
			packetArray[i].payload_length = length - (MAX_PAYLOAD_LENGTH * i);
		}

		// build data block based on payload length
		for (int j = 0; j < packetArray[i].payload_length; j++)
		{
			packetArray[i].payload[j] = buffer[MAX_PAYLOAD_LENGTH * i + j];
		}

		// set checksum
		packetArray[i].checksum = checksum(packetArray[i].payload,
				packetArray[i].payload_length);

		// set type
		packetArray[i].type = (i == numPackets - 1) ? LAST_DATA : DATA;
	}
	/*
	 *  The goal is to turn the buffer into an array of packets.
	 *  You should allocate the space for an array of packets and
	 *  return a pointer to the first element in that array.  Each 
	 *  packet's type should be set to DATA except the last, as it 
	 *  should be LAST_DATA type. The integer pointed to by 'count' 
	 *  should be updated to indicate the number of packets in the 
	 *  array.
	 */
	*count = numPackets;
	return packetArray;
}

/*
 * Send a message via RTP using the connection information
 * given on UDP socket functions sendto() and recvfrom()
 */
int rtp_send_message(CONN_INFO *connection, MESSAGE*msg)
{
	int *count = malloc(sizeof(int));
	PACKET *packets = packetize(msg->buffer, msg->length, count), *response =
			malloc(sizeof(PACKET));
	int i = 0;

	while (i < *count)
	{
		sendto(connection->socket, (void *) &packets[i], sizeof(PACKET), 0,
				connection->remote_addr, connection->addrlen);
		recvfrom(connection->socket, (void *) response, sizeof(PACKET), 0,
				connection->remote_addr, &connection->addrlen);
		if (response->type == NACK)
		{
			continue;
		}
		i++;
	}
	/*
	 * The goal of this function is to turn the message buffer
	 * into packets and then, using stop-n-wait RTP protocol,
	 * send the packets and re-send if the response is a NACK.
	 * If the response is an ACK, then you may send the next one
	 */
	free(packets);
	free(response);
	return 1;
}

/*
 * Receive a message via RTP using the connection information
 * given on UDP socket functions sendto() and recvfrom()
 */
MESSAGE* rtp_receive_message(CONN_INFO *connection)
{
	// allocate memory
	MESSAGE *message = malloc(sizeof(MESSAGE));
	message->length = 0;

	// loop until all the data is sent
	while (1)
	{
		PACKET *packet = malloc(sizeof(PACKET));
		PACKET *response = malloc(sizeof(PACKET));
		//get the data
		recvfrom(connection->socket, (void *) packet, sizeof(PACKET), 0,
				connection->remote_addr, &connection->addrlen);
		if (checksum(packet->payload, packet->payload_length)
				== packet->checksum)
		{
			// tell the server it worked
			response->type = ACK;
			response->payload_length = 0;
			response->checksum = 0;
			sendto(connection->socket, (void *) response, sizeof(PACKET), 0,
					connection->remote_addr, connection->addrlen);

			// add the data to the message
			char* buffer = calloc(packet->payload_length + message->length,
					sizeof(char));
			for (int i = 0; i < message->length; i++)
			{
				buffer[i] = message->buffer[i];
			}
			for (int i = 0; i < packet->payload_length; i++)
			{
				buffer[i + message->length] = packet->payload[i];
			}
			free(message->buffer);
			message->buffer = buffer;
			message->length = message->length + packet->payload_length;
		}
		else
		{
			// tell the server something went wrong
			response->type = NACK;
			response->payload_length = 0;
			response->checksum = 0;
			sendto(connection->socket, (void *) response, sizeof(PACKET), 0,
					connection->remote_addr, connection->addrlen);
			free(packet);
			free(response);
			continue;
		}

		if (packet->type == LAST_DATA)
		{
			// we have all the data, so leave the loop
			break;
		}
		free(packet);
		free(response);
	}

	/*
	 * The goal of this function is to handle
	 * receiving a message from the remote server using
	 * recvfrom and the connection info given. You must
	 * dynamically resize a buffer as you receive a packet
	 * and only add it to the message if the data is considered
	 * valid. The function should return the full message, so it
	 * must continue receiving packets and sending response
	 * ACK/NACK packets until a LAST_DATA packet is successfully
	 * received.
	 */
	return message;
}
