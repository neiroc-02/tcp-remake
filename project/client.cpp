#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	// for non-blocking sockets/stdin
#include <stdlib.h> // for atoi, rand()
#include <stdio.h>	// for fprintf()
#include <algorithm>
#include <chrono>
#include <vector> // for buffers
#include <string>
#include "packet_utils.h"
using namespace std;

#define DEBUG
//#define DROP

/* Defining some necessary constants */
#define RECV 0
#define SEND 1
#define RTOS 2
#define DUPA 3
#define MSS 1012 // MSS = Maximum Segment Size (aka max length)

/* PERFORM HANDSHAKE CLIENT SIDE */
/* Note: Don't worry about timer within handshake, client side is clean */
bool handshake(int sockfd, struct sockaddr *serveraddr, uint32_t *ACK, uint32_t *SEQ)
{
	/* Getting the socklen */
	socklen_t addr_len = sizeof(*serveraddr);
	/* Initial SYN */
	Packet syn = {0};
	syn.flags |= 1;			 // set the syn flag
	syn.seq = rand() % 1000; // half of the max sequence number
	serialize(syn);
	#ifdef DEBUG
	print_diag(&syn, SEND);
	#endif
	/* 1. Try sending the SYN packet*/
	if (sendto(sockfd, &syn, sizeof(syn), 0, serveraddr, addr_len) < 0)
	{
		fprintf(stderr, "FAILED SENDING OF SYN %s\n", strerror(errno));
		close(sockfd);
		return false;
	}
	/* 2. If we successfully sent a packet, check that we recieve a SYN-ACK*/
	Packet syn_ack = {0};
	while (1)
	{
		int bytes_recv = recvfrom(sockfd, &syn_ack, sizeof(syn_ack), 0, serveraddr, &addr_len);
		if (bytes_recv < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "FAILED RECV OF SYN-ACK %s\n", strerror(errno));
			close(sockfd);
			return false;
		}
		else if (bytes_recv > 0)
		{
			/* Update the SEQ and ACK numbers for general passing*/
			#ifdef DEBUG
			print_diag(&syn_ack, RECV);
			#endif
			deserialize(syn_ack);
			*ACK = syn_ack.seq + 1;
			*SEQ = syn_ack.ack;

			/* 3. Send the final ACK */
			Packet pkt = {0};
			pkt.seq = *SEQ;
			pkt.ack = *ACK;
			pkt.flags |= 2;
			serialize(pkt);
			#ifdef DEBUG	
			print_diag(&pkt, SEND);
			#endif
			if (sendto(sockfd, &pkt, sizeof(pkt), 0, serveraddr, addr_len) < 0)
			{
				fprintf(stderr, "FAILED SENDING ACK\n");
				close(sockfd);
				return false;
			}
			*SEQ += 1;
			return true;
		}
	}
}

int main(int argc, char *argv[])
{
	/* Check for correct number of args */
	if (argc < 3)
	{
		fprintf(stderr, "INCORRECT NUM OF ARGS");
		return errno;
	}
	if (strcmp(argv[1], "localhost") != 0)
	{
		fprintf(stderr, "NOT LOCAL HOST!");
		return errno;
	}

	/* 1. Create socket */
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	// use IPv4  use UDP
	if (sockfd < 0)
	{
		fprintf(stderr, "FAILED TO MAKE SOCKET");
		return errno;
	}

	/* Make socket non-blocking */
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

	/* Make stdin non-blocking */
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

	/* 2. Construct server address */
	struct sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET; // use IPv4
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	socklen_t serversize = sizeof(serveraddr);

	// Set sending port
	int SEND_PORT = atoi(argv[2]);
	serveraddr.sin_port = htons(SEND_PORT); // Big endian

	/* CALL HANDSHAKE HERE */
	srand(time(NULL));
	uint32_t SEQ = 0, ACK = 0;
	bool result = handshake(sockfd, (struct sockaddr *)&serveraddr, &ACK, &SEQ);
	if (!result)
	{
		fprintf(stderr, "HANDSHAKE FAILED\n");
		close(sockfd);
		return result;
	}
#ifdef DEBUG
	fprintf(stderr, "FINISHED HANDSHAKE-> ACK: %i, SEQ %i\n", ACK, SEQ);
#endif
	/* Setting up some constants for the loop */
	uint32_t ack_count = 0;
	vector<Packet> send_buffer;
	vector<Packet> recv_buffer;
	auto last_time = std::chrono::steady_clock::now();
	while (1)
	{
		/* 1. Handle incoming data from stdin */
		uint8_t payload[MSS];
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
			/* Initialize a new packet to send */
			Packet pkt = {0};
			pkt.length = stdin_bytes;
			pkt.seq = SEQ;
			SEQ = pkt.seq + pkt.length;
			memcpy(pkt.payload, payload, stdin_bytes);
			/* Check if we have seen the packet before */
			bool DROP_SIM = true;
			serialize(pkt);
#ifdef DEBUG
			print_diag(&pkt, SEND);
#endif
#ifdef DROP
			DROP_SIM = (rand() % 100) > 10;
#endif
			/* Send the new packet */
			if (DROP_SIM && sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
			{
				fprintf(stderr, "FAILED TO SEND PACKET %s\n", strerror(errno));
				close(sockfd);
				return errno;
			}
			/* Add the new packet to the send buffer */
			deserialize(pkt);
			send_buffer.push_back(pkt);
		}
		/* 2. Handle incoming data from socket*/
		Packet server_pkt = {0};
		int server_bytes = recvfrom(sockfd, &server_pkt, sizeof(server_pkt), 0, (struct sockaddr *)&serveraddr, &serversize);
		if (server_bytes < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "COULD NOT RECIEVE BYTES: %s\n", strerror(errno));
			close(sockfd);
			return errno;
		}
		else if (server_bytes > 0)
		{
		/* Deserialize the packet and check if its an ACK */
#ifdef DEBUG
			print_diag(&server_pkt, RECV);
#endif
			deserialize(server_pkt);
			bool is_ack = (server_pkt.flags & 2);
			if (server_pkt.length > 0)
			{
#ifdef DEBUG
					fprintf(stderr, "ACK: %d\n", ACK);
#endif
				/* If we have a data packet, first check that its not a duplicate*/
				auto it = find_if(recv_buffer.begin(), recv_buffer.end(), [&](const Packet &temp)
								  { return temp.seq == server_pkt.seq; });
				/* If its not a duplicate, call handle data*/
				if (server_pkt.seq >= ACK && it == recv_buffer.end())
				{
					recv_buffer.push_back(server_pkt);
				}
				ACK = (server_pkt.ack == ACK) ? (server_pkt.ack + server_pkt.length) : ACK;
				clean_recv_buffer(ACK, recv_buffer);
				/* Send an ACK*/
				//clean_send_buffer(ACK, send_buffer);
				Packet pkt = {0};
				pkt.flags |= 2;
				pkt.ack = ACK;
				//ACK = pkt.ack;
				serialize(pkt);
				bool DROP_SIM = true;
#ifdef DROP
				DROP_SIM = (rand() % 100) > 10;
#endif
				if (DROP_SIM && sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
				{
					fprintf(stderr, "FAILED TO SEND ACK: %s", strerror(errno));
					close(sockfd);
					return errno;
				}
			}
			if (is_ack)
			{
				/* If we have an ACK, call handle ACK */
				handle_ack(ACK ,ack_count, server_pkt, send_buffer);
			}
		}
		/* 3. Handle retransmission logic */
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
			fprintf(stderr, "RETRANSMITING...\n");
#endif
			if (ack_count >= 3)
			{
				/* Reset the ack counter */
				ack_count = 0;
				#ifdef DEBUG
					fprintf(stderr, "Resetting the ack count...\n");
				#endif 
			}
			if (elapsed >= 1000)
			{
				/* Reset the timer */
				last_time = current_time;
				#ifdef DEBUG
					fprintf(stderr, "Resetting the timer...\n");
				#endif 
			}
			/* Resend what is at the top of the sending buffer */
			if (!send_buffer.empty())
			{
				//clean_send_buffer(ACK, send_buffer);
				Packet retransmit = send_buffer.at(0);
				//retransmit.ack = ACK;
				serialize(retransmit);
				bool DROP_SIM = true;
#ifdef DEBUG
				fprintf(stderr, "ACK: %d\n", ACK);
				fprintf(stderr, "SEQ: %d\n", SEQ);
				fprintf(stderr, "ELAPSED: %ld\n", elapsed);
				fprintf(stderr, "ACK COUNT: %d\n", ack_count);
				for (int i = 0; i < send_buffer.size(); i++){
					fprintf(stderr, "%d ", send_buffer.at(i).seq);
				}
				fprintf(stderr, "\n");
				print_diag(&retransmit, RTOS);
#endif
#ifdef DROP
				DROP_SIM = (rand() % 100) > 10;
#endif
				if (DROP_SIM && sendto(sockfd, &retransmit, sizeof(Packet), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
				{
					fprintf(stderr, "FAILED TO SEND RETRANSMISSION: %s\n", strerror(errno));
					close(sockfd);
					return errno;
				}
			}
		}
	}
	/* 6. You're done! Terminate the connection */
	close(sockfd);
	return 0;
}
