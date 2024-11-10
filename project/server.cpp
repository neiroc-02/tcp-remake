#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	 //for non-blocking sockets
#include <stdlib.h>	 //for atoi
#include <stdio.h>	 //for fprintf
#include <time.h>	 //for timer
#include <vector>	 //for std::vector
#include <algorithm> //for searching
#include <chrono>	 //for timer
#include "packet_utils.h"
using namespace std;

#define DEBUG
//#define DROP

/* Defining some necessary constants */
#define RECV 0
#define SEND 1
#define RTOS 2
#define DUPA 3
#define MSS 1012

/* PERFORM HANDSHAKE SERVER SIDE */
/* Note: Don't worry about the timer, just assume everything will not drop */
Packet handshake(int sockfd, struct sockaddr *serveraddr, uint32_t *ACK, uint32_t *SEQ)
{
	/* Precursor setup... */
	socklen_t addr_len = sizeof(*serveraddr);
	Packet retval = {0};
	Packet syn = {0};
	int bytes_recv = 0;

	/* 1. Wait to recieve SYN */
	while (bytes_recv <= 0)
	{
		bytes_recv = recvfrom(sockfd, &syn, sizeof(syn), 0, serveraddr, &addr_len);
		if (bytes_recv < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "FAILED RECV OF SYN");
			close(sockfd);
			exit(1);
		}
	}
	#ifdef DEBUG
	print_diag(&syn, RECV);
	#endif 
	/* Setting all the flags for syn-ack */
	Packet syn_ack = {0};
	syn_ack.flags = syn_ack.flags | 3; // turn on the syn and ack flags
	syn_ack.seq = rand() % 1000;
	syn_ack.ack = ntohl(syn.seq) + 1;
	serialize(syn_ack);
	#ifdef DEBUG
	print_diag(&syn_ack, SEND);
	#endif
	/* 2. Send the SYN-ACK flag */
	if (sendto(sockfd, &syn_ack, sizeof(syn_ack), 0, serveraddr, addr_len) < 0)
	{
		fprintf(stderr, "FAILED SENDING SYN-ACK");
		close(sockfd);
		exit(1);
	}

	/* 3. Continue trying to recieve data until you get something with a payload */
	while (1)
	{
		int data = recvfrom(sockfd, &retval, sizeof(retval), 0, serveraddr, &addr_len);
		if (data < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "FAILED TO RECV ACK");
			close(sockfd);
			exit(1);
		}
		else if (data > 0)
		{
			deserialize(retval);
			*ACK = retval.seq; //+ 1;
			*SEQ = retval.ack;
			return retval;
		}
	}
}

int main(int argc, char *argv[])
{
	/* Check for correct number of args */
	if (argc < 2)
	{
		fprintf(stderr, "NO PORT GIVEN!\n");
		return errno;
	}

	/* 1. Create socket */
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // use IPv4  use UDP
	if (sockfd < 0)
	{
		fprintf(stderr, "SOCKET NOT CREATED");
		close(sockfd);
		return errno;
	}

	/* Making the socket non-blocking*/
	int socket_flags = fcntl(sockfd, F_GETFL, 0);
	if (socket_flags < 0)
	{
		fprintf(stderr, "ERROR GETTING SOCKET FLAGS");
		close(sockfd);
		return errno;
	}
	socket_flags |= O_NONBLOCK;
	if (fcntl(sockfd, F_SETFL, socket_flags) < 0)
	{
		fprintf(stderr, "ERROR SETTING SOCKET FLAGS");
		close(sockfd);
		return errno;
	}

	/* Making stdin non-blocking */
	int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	if (stdin_flags < 0)
	{
		fprintf(stderr, "ERROR GETTING STDIN FLAGS");
		close(sockfd);
		return errno;
	}
	stdin_flags |= O_NONBLOCK;
	if (fcntl(STDIN_FILENO, F_SETFL, stdin_flags) < 0)
	{
		fprintf(stderr, "ERROR SETTING STDIN FLAGS");
		close(sockfd);
		return errno;
	}

	/* 2. Construct our address */
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;		   // use IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY; // accept all connections; same as inet_addr("0.0.0.0"); "Address string to network bytes"
	int PORT = atoi(argv[1]);			   // Set recieving port
	servaddr.sin_port = htons(PORT);	   // Big endian

	/* 3. Let operating system know about our config */
	int did_bind = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); // Error if did_bind < 0 :(
	if (did_bind < 0)
	{
		fprintf(stderr, "DID NOT BIND TO OS");
		close(sockfd);
		return errno;
	}

	/* CALL HANDSHAKE HERE */
	uint32_t SEQ = 0, ACK = 0;
	srand(time(NULL));
	Packet result = handshake(sockfd, (struct sockaddr *)&servaddr, &ACK, &SEQ);

	/* Setting up some constants for the loop */
	uint32_t ack_count = 0;
	vector<Packet> recv_buffer;
	vector<Packet> send_buffer;

	/* If the returned packet contained data, print and update ACK */
	if (result.length > 0)
	{
		/* Write the packet to stdout and update ACK */
		write(1, result.payload, result.length);
		ACK += result.length;
		/* Send back the proper ACK */
		Packet pkt = {0};
		pkt.flags |= 2; 
		pkt.ack = ACK;
		serialize(pkt);
		print_diag(&pkt, SEND);
		if (sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		{
			fprintf(stderr, "FAILED TO SEND ACK: %s\n", strerror(errno));
			close(sockfd);
			return errno;
		}
	}

	auto last_time = std::chrono::steady_clock::now();
	#ifdef DEBUG
		fprintf(stderr, "FINISHED HANDSHAKE-> ACK: %i, SEQ %i\n", ACK, SEQ);
	#endif
	while (1)
	{
		/* 1. Handle incoming data from stdin */
		uint8_t payload[MSS];
		struct sockaddr_in clientaddr;
		socklen_t clientsize = sizeof(clientaddr);
		/* NOTE: Don't worry about splitting the stdin buffer, the OS does that for you if you have extra bytes */
		int stdin_bytes = read(STDIN_FILENO, payload, sizeof(payload));
		if (stdin_bytes < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "FAILED TO READ FROM STDIN: %s\n", strerror(errno));
			close(sockfd);
			return errno;
		}
		else if (stdin_bytes > 0)
		{
			/* If we recieve data, send the packet*/
			Packet pkt = {0};
			pkt.length = stdin_bytes; // length is given by the number of bytes read
			pkt.seq = SEQ;			  // new seq number is SEQ + stdin_bytes
			SEQ = pkt.seq + pkt.length;
			memcpy(pkt.payload, payload, stdin_bytes);
			/* Check if we have seen the packet before */
			auto it = find_if(send_buffer.begin(), send_buffer.end(), [&](const Packet &temp)
							  { return pkt.seq == temp.seq; });
			bool new_pkt = (it == send_buffer.end());
			bool DROP_SIM = true;
			serialize(pkt);
			#ifdef DEBUG
				print_diag(&pkt, SEND);
				//DROP_SIM = (rand() % 100) > 10;
			#endif
			/* If the packet hasn't been seen, send it */
			if (DROP_SIM && new_pkt && sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
			{
				fprintf(stderr, "FAILED TO SEND DATA PACKET: %s\n", strerror(errno));
				close(sockfd);
				return errno;
			}
			/* Add the new packet to the send buffer */
			if (new_pkt)
			{
				deserialize(pkt);
				send_buffer.push_back(pkt);
			}
		}
		/* 2. Handle incoming data from socket */
		Packet client_pkt = {0};
		int client_bytes = recvfrom(sockfd, &client_pkt, sizeof(client_pkt), 0, (struct sockaddr *)&clientaddr, &clientsize);
		if (client_bytes < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "COULD NOT RECIEVE BYTES: %s\n", strerror(errno));
			close(sockfd);
			return errno;
		}
		else if (client_bytes > 0)
		{	
			/* Deserialize the packet and check if its an ACK */
			#ifdef DEBUG
				print_diag(&client_pkt, RECV);
			#endif
			deserialize(client_pkt);
			bool is_ack = (client_pkt.flags & 2);
			if (is_ack)
			{
				/* If we have an ACK, call handle ACK*/
				handle_ack(ack_count, client_pkt, send_buffer);
			}
			if (client_pkt.length > 0)
			{
				#ifdef DEBUG
					fprintf(stderr, "ACK: %d\n", ACK);
				#endif
				/* If we have a data packet, first check if we've seen it before */
				auto it = find_if(recv_buffer.begin(), recv_buffer.end(), [&](const Packet &temp)
								  { return temp.seq == client_pkt.seq; });
				/* If its not a duplicate, push it to the recv buffer*/
				if (client_pkt.seq >= ACK && it == recv_buffer.end())
				{
					recv_buffer.push_back(client_pkt);
				}
				clean_recv_buffer(ACK, recv_buffer);
				/* Send an ACK */
				Packet pkt = {0};
				pkt.flags |= 2;
				pkt.ack = ACK;
				serialize(pkt);
				bool DROP_SIM = true;
				#ifdef DEBUG 
					//DROP_SIM = (rand() % 100) > 10;
				#endif
				if (DROP_SIM && sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
				{
					fprintf(stderr, "FAILED TO SEND ACK: %s\n", strerror(errno));
					close(sockfd);
					return errno;
				}
			}
		}
		auto current_time = std::chrono::steady_clock::now();
		if (send_buffer.size() == 0)
		{
			last_time = current_time;
		}
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time).count();
		/* 3. Handle retransmission: send a packet when the timer goes off; reset the timer when the send_buf is empty */
		if (ack_count >= 3 || elapsed >= 1000)
		{
			#ifdef DEBUG
				//fprintf(stderr, "RETRANSMITTING...\n");
			#endif
			/* Resend what is at the top of the sending buffer */
			if (send_buffer.size() > 0)
			{
				Packet retransmit = send_buffer.at(0);
				serialize(retransmit);
				bool DROP_SIM = true;
			#ifdef DEBUG
				fprintf(stderr, "ACK: %d\n", ACK);
				fprintf(stderr, "SEQ: %d\n", SEQ);
				fprintf(stderr, "ELAPSED: %ld\n", elapsed);
				fprintf(stderr, "ACK_COUNT: %d\n", ack_count);
				//DROP_SIM = (rand() % 100) > 10;
				print_diag(&retransmit, RTOS);
			#endif
				if (DROP_SIM && sendto(sockfd, &retransmit, sizeof(Packet), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
				{
					fprintf(stderr, "FAILED TO SEND RETRANSMISSION: %s\n", strerror(errno));
					close(sockfd);
					return errno;
				}
			}
			if (ack_count >= 3)
			{
				/* Reset the ack counter */
				ack_count = 0;
			}
			if (elapsed >= 1000)
			{	
				/* Reset the timer */
				last_time = current_time;
			}
		}
	}
	/* 8. You're done! Terminate the connection */
	close(sockfd);
	return 0;
}