#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	//For non-blocking sockets
#include <stdlib.h> //for atoi
#include <stdio.h>	//for fprintf

int main(int argc, char *argv[])
{
	/* Check for correct number of args */
	if (argc < 2){
		fprintf(stderr, "NO PORT GIVEN!\n");
		return errno;
	}

	/* 1. Create socket */
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // use IPv4  use UDP
	if (sockfd < 0){
		fprintf(stderr, "SOCKET NOT CREATED");
		close(sockfd);
		return errno;
	}

	/* Making the socket non-blocking*/
	int socket_flags = fcntl(sockfd, F_GETFL, 0);
	if (socket_flags < 0){
		fprintf(stderr, "ERROR GETTING SOCKET FLAGS");
		close(sockfd);
		return errno;
	}
	socket_flags |= O_NONBLOCK;
	if (fcntl(sockfd, F_SETFL, socket_flags) < 0){
		fprintf(stderr, "ERROR SETTING SOCKET FLAGS");
		close(sockfd);
		return errno;
	}

	/* Making stdin non-blocking */
	int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	if (stdin_flags < 0){
		fprintf(stderr, "ERROR GETTING STDIN FLAGS");
		return errno;
	}
	stdin_flags |= O_NONBLOCK;
	if (fcntl(STDIN_FILENO, F_SETFL, stdin_flags) < 0){
		fprintf(stderr, "ERROR SETTING STDIN FLAGS");
		return errno;
	}

	/* 2. Construct our address */
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;		   // use IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY; // accept all connections; same as inet_addr("0.0.0.0"); "Address string to network bytes"

	int PORT = atoi(argv[1]); 			   // Set recieving port
	servaddr.sin_port = htons(PORT); 	   // Big endian

	/* 3. Let operating system know about our config */
	int did_bind = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); // Error if did_bind < 0 :(
	if (did_bind < 0) return errno;

	/* 4. Create buffer to store incoming data */
	int BUF_SIZE = 1024;
	char client_buf[BUF_SIZE];
	struct sockaddr_in clientaddr; 		   // Same information, but about client
	socklen_t clientsize = sizeof(clientaddr);

	/*Looping to check for and send data*/
	while (1){
		/* 5. Listen for data from clients */
		int bytes_recvd = recvfrom(sockfd, client_buf, BUF_SIZE,
								   // socket  store data  how much
								   0, (struct sockaddr *)&clientaddr,
								   &clientsize);
		if (bytes_recvd < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				sleep(1);
				continue;
			}
			else
			{
				fprintf(stderr, "ERROR RECEIVING DATA");
				close(sockfd);
				return errno;
			}
		}
		/* 6. Inspect data from client */
		char *client_ip = inet_ntoa(clientaddr.sin_addr); // "Network bytes to address string"
		int client_port = ntohs(clientaddr.sin_port);	  // Little endian
		// Print out data
		write(1, client_buf, bytes_recvd);

		//TODO: PULL FROM STDIN INTO THE BUFFER
		/*
		char server_buf[] = "Hello world!";
		int did_send = sendto(sockfd, server_buf, strlen(server_buf),
						  	 // socket  send data   how much to send
						  	 0, (struct sockaddr *)&clientaddr,
						  	 // flags   where to send
						  	 sizeof(clientaddr));

		if (did_send < 0) {
			if (certain error code){
			
			}
			else {
				fprintf(stderr, "ERROR READING FROM STDIN");
				return errno;
			}
		}
		*/ 
	}

	/* 8. You're done! Terminate the connection */
	close(sockfd);
	return 0;
}