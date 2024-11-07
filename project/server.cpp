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

/* Defining some necessary constants */
#define RECV 0
#define SEND 1
#define RTOS 2
#define DUPA 3
#define MSS 1012
/*
NOTE TESTS THAT SHOULD PASS WHEN THIS IS DONE...
test4
test6
test8
prolly shoulda done development on client side first rip
*/

/* PERFORM HANDSHAKE SERVER SIDE */
/* Note: Don't worry about the timer, just assume everything will not drop */
int handshake(int sockfd, struct sockaddr *serveraddr, int *ACK, int *SEQ)
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

	/* Setting all the flags for syn-ack */
	Packet syn_ack = {0};
	syn_ack.flags = htonl(syn_ack.flags | 3); // turn on the syn and ack flags
	syn_ack.seq = htonl(rand() % (__UINT32_MAX__ / 2));
	syn_ack.ack = htonl(ntohl(syn.seq) + 1);

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
			*ACK = ntohl(retval.ack);
			*SEQ = ntohl(retval.seq);
			return 1;
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

	int PORT = atoi(argv[1]);		 // Set recieving port
	servaddr.sin_port = htons(PORT); // Big endian

	/* 3. Let operating system know about our config */
	int did_bind = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); // Error if did_bind < 0 :(
	if (did_bind < 0)
	{
		fprintf(stderr, "DID NOT BIND TO OS");
		close(sockfd);
		return errno;
	}

	/* CALL HANDSHAKE HERE */
	int SEQ = 0, ACK = 0;
	srand(time(NULL));
	int result = handshake(sockfd, (struct sockaddr *)&servaddr, &ACK, &SEQ);
	if (!result)
	{
		fprintf(stderr, "HANDSHAKE FAILED\n");
		close(sockfd);
		return result;
	}
	fprintf(stdout, "FINISHED HANDSHAKE: ACK %i, SEQ %d\n", ACK, SEQ);

	/* Setting up some constants for the loop */
	int ack_count = 0;
	vector<Packet> recv_buffer;
	vector<Packet> send_buffer;
	auto last_time = std::chrono::steady_clock::now();

	while (1)
	{
		/* 1. Handle incoming data from stdin */
		uint8_t payload[MSS];
		struct sockaddr_in clientaddr;
		socklen_t clientsize = sizeof(clientaddr);
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
			pkt.length = stdin_bytes;				   // length is given by the number of bytes read
			pkt.seq = SEQ + stdin_bytes;			   // new seq number is SEQ + stdin_bytes
			memcpy(pkt.payload, payload, stdin_bytes);
			serialize(pkt);
			print_diag(&pkt, SEND);
			printf("Packet length: %i", pkt.length);
			if (sendto(sockfd, &pkt, sizeof(Packet), 0, 
						(struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
			{
				fprintf(stderr, "FAILED TO SEND DATA PACKET: %s\n", strerror(errno));
				close(sockfd);
				return errno;
			}
		}
		/* 2. Read data in and print it*/
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
			print_diag(&client_pkt, RECV);
			deserialize(client_pkt);
		}
	}
	#ifdef OLDCODE
	while (1)
	{
		/* 1. Handle incoming data from stdin */
		uint8_t payload[MSS];
		struct sockaddr_in clientaddr;
		socklen_t clientsize = sizeof(clientaddr);
		/* Try to read from stdin, if nothing just move on */
		int stdin_bytes = read(STDIN_FILENO, payload, sizeof(payload));
		if (stdin_bytes < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "FAILED TO READ FROM STDIN: %s\n", strerror(errno));
			close(sockfd);
			return errno;
		}
		else if (stdin_bytes > 0)
		{
			/* Prepare packet for sending */
			Packet pkt = {0};
			pkt.length = stdin_bytes;				   // length is given by the number of bytes read
			pkt.seq = SEQ + stdin_bytes;			   // new seq number is SEQ + stdin_bytes
			memcpy(pkt.payload, payload, stdin_bytes); // place the payload in the arr
			/* Check if the packet has been seen before */
			auto it = find_if(send_buffer.begin(), send_buffer.end(),
							  [&](const Packet &temp)
							  {
								  return pkt.seq == temp.seq && pkt.length == temp.length;
							  });
			pkt.length = htons(pkt.length); // length is given by the number of bytes read
			pkt.seq = htonl(pkt.seq);		// new seq number is SEQ + stdin_bytes
			/* Send the packet over the network if it isn't in the buffer*/
			if (it == send_buffer.end() && sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&clientaddr, clientsize) < 0)
			{
				fprintf(stderr, "FAILED TO SEND DATA PACKET: %s\n", strerror(errno));
				close(sockfd);
				return errno;
			}
			/* Convert the packet back to host byte order and store in the send_buf*/
			/* Note: No need to sort sending buffer since it is pushed in order */
			pkt.length = ntohs(pkt.length);
			pkt.seq = ntohl(pkt.seq);
			send_buffer.push_back(pkt);
		}

		/* 2. Handle incoming packets from client: could be data/ack */
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
			/* Check and see if the packet is an ACK or Data Packet */
			bool is_ack = ntohl(client_pkt.flags & 2); // Anding with 2 checks the ACK flag
			/* Handle ACK Packets */
			if (is_ack)
			{
				if (ntohl(client_pkt.ack) == ACK)
				{
					/* Increment ACK count when its equal */
					ack_count++;
				}
				else if (ntohl(client_pkt.ack) > ACK)
				{
					/* When your new ACK is bigger than your old, update the global ACK*/
					ACK = ntohl(client_pkt.ack) + ntohs(client_pkt.length);
					ack_count = 1;
					/* Update the sending buffer */
					while (send_buffer.size() == 0 && send_buffer.at(0).seq < ACK)
					{
						send_buffer.erase(send_buffer.begin());
					}
				}
			}
			/* Handle Data Packets */
			else
			{
				/* Check if the current packet exists in the recv */
				auto it = find_if(recv_buffer.begin(), recv_buffer.end(), [&](const Packet &temp)
								  { return temp.seq == ntohl(client_pkt.seq); });
				/* If we have a new packet...*/
				if (it == recv_buffer.end())
				{
					/* Push the new packet into the recv_buffer */
					recv_buffer.push_back(client_pkt);
					/* Sort the recv_buffer */
					sort(recv_buffer.begin(), recv_buffer.end());
					/* Check if you can print, as you print delete from buf and update ACK */
					while (recv_buffer.size() > 1 && (recv_buffer.at(0).seq + recv_buffer.at(0).length) == ACK)
					{
						write(1, recv_buffer.at(0).payload, recv_buffer.at(0).length);
						ACK += recv_buffer.at(1).length;
						recv_buffer.erase(recv_buffer.begin());
					}
					if (recv_buffer.size() == 1 && recv_buffer.at(0).seq == ACK)
					{
						write(1, recv_buffer.at(0).payload, recv_buffer.at(0).length);
						ACK += recv_buffer.at(0).length;
						recv_buffer.erase(recv_buffer.begin());
					}
					/* Send an ACK */
					Packet pkt = {0};
					pkt.flags = htonl(pkt.flags | 2); // set the ack flag
					pkt.ack = htonl(ACK);
					if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&clientaddr, clientsize) < 0)
					{
						fprintf(stderr, "FAILED TO SEND ACK: %s", strerror(errno));
						close(sockfd);
						return errno;
					}
				}
				/* Otherwise, just do nothing since we already have the packet in the recv_buffer*/
			}
		}
		auto current_time = std::chrono::steady_clock::now();
		if (send_buffer.size() == 0)
		{
			last_time = current_time;
		}
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_time).count();
		/* 3. Handle retransmission: send a packet when the timer goes off; reset the timer when the send_buf is empty */
		if (ack_count >= 3 || elapsed >= 1)
		{
			/* Resend what is at the top of the sending buffer */
			if (send_buffer.size() > 0)
			{
				Packet retransmit = send_buffer.at(0);
				if (sendto(sockfd, &retransmit, sizeof(retransmit), 0, (struct sockaddr *)&clientaddr, clientsize) < 0)
				{
					fprintf(stderr, "FAILED TO SEND RETRANSMISSION: %s\n", strerror(errno));
					close(sockfd);
					return errno;
				}
			}
			if (ack_count >= 3)
			{
				ack_count = 0;
			}
			if (elapsed >= 1)
			{
				last_time = current_time;
			}
		}
	}
	#endif
	/* 8. You're done! Terminate the connection */
	close(sockfd);
	return 0;
}