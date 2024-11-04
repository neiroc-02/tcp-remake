#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	//for non-blocking sockets
#include <stdlib.h> //for atoi
#include <stdio.h>	//for fprintf
#include <time.h>	//for timer
#include <math.h>	//for pow()
#include <poll.h>	//for poll()

#define MSS 1012 // MSS = Maximum Segment Size (aka max length)

// #define DEBUG false

typedef struct
{
	uint32_t ack;
	uint32_t seq;
	uint16_t length;
	uint8_t flags;
	uint8_t unused;
	uint8_t payload[MSS];
} Packet;

/* PERFORM HANDSHAKE SERVER SIDE */
Packet handshake(int sockfd, struct sockaddr *serveraddr, int *ACK, int *SEQ)
{
	socklen_t addr_len = sizeof(*serveraddr);
	Packet retval = {0};
	time_t start;
	int bytes_recv = 0;
	Packet syn = {0};
	const int RETRANSMIT = 1;
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
	/* 2. Send the SYN-ACK flag */
	Packet syn_ack = {0};
	syn_ack.flags |= 0b11;
	syn_ack.seq = htonl(rand() % (__UINT32_MAX__ / 2));
	syn_ack.ack = htonl(ntohl(syn.seq) + 1);

	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;

	start = time(NULL);
	while (1)
	{
		/* Sending the syn-ack flag */
		fprintf(stdout, "Retransmitting...");
		fflush(stdout);
		if (sendto(sockfd, &syn_ack, sizeof(syn_ack), 0, serveraddr, addr_len) < 0)
		{
			fprintf(stderr, "FAILED SENDING SYN-ACK");
			close(sockfd);
			exit(1);
		}
		/* Waiting for data to exit the loop with*/
		int poll_count = poll(&pfd, 1, 1000);
		if (poll_count < 0)
		{
			fprintf(stderr, "POLL ERROR");
			close(sockfd);
			exit(1);
		}
		else if (poll_count == 0)
		{
			continue;
		}
		else
		{
			int data = recvfrom(sockfd, &retval, sizeof(retval), 0, serveraddr, &addr_len);
			if (data < 0 && errno != EAGAIN)
			{
				fprintf(stderr, "FAILED TO RECIEVE FIRST DATA PACKET");
				close(sockfd);
				exit(1);
			}
			else if (data > 0 && retval.payload[0] != '\0')
			{
				*ACK = ntohl(retval.ack);
				*SEQ = ntohl(retval.seq);
				return retval;
			}
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
	fprintf(stdout, "Entering handshake...");
	Packet result = handshake(sockfd, (struct sockaddr *)&servaddr, &SEQ, &ACK);
	fprintf(stdout, "FINISHED HANDSHAKE");

#ifdef DEBUG
	/* Looping to check for and send data */
	int hasRead = 0;
	while (1)
	{
		/* 4. Create buffer to store incoming data */
		int BUF_SIZE = 1024;
		char client_buf[BUF_SIZE];
		struct sockaddr_in clientaddr; // Same information, but about client
		socklen_t clientsize = sizeof(clientaddr);

		/* 5. Listen for data from clients */
		int bytes_recvd = recvfrom(sockfd, client_buf, BUF_SIZE,
								   // socket  store data  how much
								   0, (struct sockaddr *)&clientaddr,
								   &clientsize);
		if (bytes_recvd < 0)
		{
			if (errno != EAGAIN)
			{
				fprintf(stderr, "ERROR RECEIVING DATA");
				close(sockfd);
				return errno;
			}
		}
		else
		{
			hasRead = 1;
			/* 6. Inspect data from client */
			char *client_ip = inet_ntoa(clientaddr.sin_addr); // "Network bytes to address string"
			int client_port = ntohs(clientaddr.sin_port);	  // Little endian
			write(1, client_buf, bytes_recvd);
		}
		/* 7. Read data from stdin, sending to client */
		if (hasRead == 1)
		{
			char server_buf[1024];
			int stdin_bytes = read(STDIN_FILENO, server_buf, sizeof(server_buf)); // Read the bytes
			if (stdin_bytes < 0)
			{
				if (errno != EAGAIN)
				{
					fprintf(stderr, "FAILED TO READ FROM STDIN");
					close(sockfd);
					return errno;
				}
			}
			else
			{
				int did_send = sendto(sockfd, server_buf, stdin_bytes /*strlen(server_buf)*/,
									  // socket  send data   how much to send
									  0, (struct sockaddr *)&clientaddr,
									  // flags   where to send
									  sizeof(clientaddr));

				if (did_send < 0)
				{
					fprintf(stderr, "ERROR READING FROM STDIN");
					close(sockfd);
					return errno;
				}
			}
		}
	}
#endif
	/* 8. You're done! Terminate the connection */
	close(sockfd);
	return 0;
}