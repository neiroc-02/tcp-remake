#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	// for non-blocking sockets/stdin
#include <stdlib.h> // for atoi
#include <stdio.h>	//for printf

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

	/* Looping to send/recieve the data */
	while (1)
	{
		/* 3. Send data to server */
		char client_buf[1024];
		int stdin_bytes = read(STDIN_FILENO, client_buf, sizeof(client_buf));
		if (stdin_bytes < 0)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				fprintf(stderr, "FAILED TO READ FROM STDIN");
				close(sockfd);
				return errno;
			}
			continue;
		}
		else
		{
			// client_buf[stdin_bytes] = '\0';
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
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				fprintf(stderr, "ERROR RECIEVING DATA");
				close(sockfd);
				return errno;
			}
			continue;
		}
		else
		{
			// Print out data
			write(1, server_buf, bytes_recvd);
		}
	}
	/* 6. You're done! Terminate the connection */
	close(sockfd);
	return 0;
}
