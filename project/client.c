#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	// for non-blocking sockets/stdin
#include <stdlib.h> // for atoi, rand()
#include <stdio.h>	// for printf()
#include <time.h>	// for timer
#include <math.h>	// for pow()
#include <poll.h>	// for poll()

// #define DEBUG // for debugging
#define MSS 1012 // MSS = Maximum Segment Size (aka max length)
typedef struct
{
	uint32_t ack;
	uint32_t seq;
	uint16_t length;
	uint8_t flags;
	uint8_t unused;
	uint8_t payload[MSS];
} Packet;

/* PERFORM HANDSHAKE CLIENT SIDE */
/* Note: Don't worry about timer within handshake */
int handshake(int sockfd, struct sockaddr *serveraddr, int *ACK, int *SEQ)
{
	/* Getting the socklen */
	socklen_t addr_len = sizeof(*serveraddr);

	/* Initial SYN */
	Packet syn = {0};
	syn.flags |= 1;									// set the syn flag
	syn.seq = htonl(rand() % (__UINT32_MAX__ / 2)); // half of the max sequence number

	/* 1. Try sending the SYN packet*/
	if (sendto(sockfd, &syn, sizeof(syn), 0, serveraddr, addr_len) < 0)
	{
		fprintf(stderr, "FAILED SENDING OF SYN %s\n", strerror(errno));
		close(sockfd);
		return errno;
	}
	/* 2. If we successfully sent a packet, check that we recieve a SYN-ACK*/
	Packet syn_ack = {0};
	/* Use the poll to track if a second */
	while (1)
	{
		int bytes_recv = recvfrom(sockfd, &syn_ack, sizeof(syn_ack), 0, serveraddr, &addr_len);
		if (bytes_recv < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "FAILED RECV OF SYN-ACK %s\n", strerror(errno));
			close(sockfd);
			return errno;
		}
		else if (bytes_recv > 0)
		{
			/* Update the SEQ and ACK numbers for general passing*/
			*ACK = ntohl(syn_ack.seq) + 1;
			*SEQ = ntohl(syn_ack.ack) + 1;
			Packet pkt = {0};
			pkt.ack = htonl(*ACK);
			pkt.seq = htonl(*SEQ);
			pkt.flags |= 2; // set the ack flag
			/* 3. Send the final ACK */
			if (sendto(sockfd, &pkt, sizeof(pkt), 0, serveraddr, addr_len) < 0){
				fprintf(stderr, "FAILED SENDING ACK\n");
				close(sockfd);
				return errno;
			}
			return 1;
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

	// Set sending port
	int SEND_PORT = atoi(argv[2]);
	serveraddr.sin_port = htons(SEND_PORT); // Big endian

	/* CALL HANDSHAKE HERE */
	srand(time(NULL));
	int SEQ = 0, ACK = 0;
	//fprintf(stdout, "Entering handshake...");
	int result = handshake(sockfd, (struct sockaddr *)&serveraddr, &SEQ, &ACK);
	if (!result)
	{
		fprintf(stderr, "HANDSHAKE FAILED\n");
		close(sockfd);
		return result;
	}
	//fprintf(stdout, "Finished Handshake -> SEQ: %i, ACK: %i\n", SEQ, ACK);

	//Use a buffer of 40 packets to be safe...
	Packet recv_buffer[40] = {0};
	Packet send_buffer[40] = {0};
#ifdef DEBUG
	
	/* Looping to send/recieve the data */
	while (1)
	{
		/* 3. Send data to server */
		char client_buf[1024];
		int stdin_bytes = read(STDIN_FILENO, client_buf, sizeof(client_buf));
		if (stdin_bytes < 0 && errno != EAGAIN)
		{
			fprintf(stderr, "FAILED TO READ FROM STDIN");
			close(sockfd);
			return errno;
		}
		else
		{
			int did_send = sendto(sockfd, client_buf, stdin_bytes /*strlen(client_buf)*/,
								  // socket  send data   how much to send
								  0, (struct sockaddr *)&serveraddr,
								  // flags   where to send
								  sizeof(serveraddr));
			if (did_send < 0)
			{
				fprintf(stderr, "ERROR SENDING DATA");
				close(sockfd);
				return errno;
			}
		}
		/* 4. Create buffer to store incoming data */
		int BUF_SIZE = 1024;
		char server_buf[BUF_SIZE];
		socklen_t serversize = sizeof(socklen_t); // Temp buffer for recvfrom API
		/* 5. Listen for response from server */
		int bytes_recvd = recvfrom(sockfd, server_buf, BUF_SIZE,
								   // socket  store data  how much
								   0, (struct sockaddr *)&serveraddr,
								   &serversize);
		// Execution will stop here until `BUF_SIZE` is read or termination/error
		// Error if bytes_recvd < 0 :(
		if (bytes_recvd < 0)
		{
			if (errno != EAGAIN)
			{
				fprintf(stderr, "ERROR RECIEVING DATA");
				close(sockfd);
				return errno;
			}
		}
		else
		{
			// Print out data
			write(1, server_buf, bytes_recvd);
		}
	}
#endif
	/* 6. You're done! Terminate the connection */
	close(sockfd);
	return 0;
}
