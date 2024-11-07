#include "packet_utils.h"
#include <vector>
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
#include <chrono>    //for timer
using namespace std;

#define MSS 1012 // MSS = Maximum Segment Size (aka max length)
// Diagnostic messages
#define RECV 0
#define SEND 1
#define RTOS 2
#define DUPA 3

bool operator<(const Packet &a, const Packet &b)
{
	return a.seq < b.seq;
}

void print_diag(Packet *pkt, int diag)
{
	switch (diag)
	{
	case RECV:
		fprintf(stderr, "RECV");
		break;
	case SEND:
		fprintf(stderr, "SEND");
		break;
	case RTOS:
		fprintf(stderr, "RTOS");
		break;
	case DUPA:
		fprintf(stderr, "DUPS");
		break;
	}

	bool syn = pkt->flags & 0b01;
	bool ack = pkt->flags & 0b10;
	fprintf(stderr, " %u ACK %u SIZE %hu FLAGS ", ntohl(pkt->seq),
			ntohl(pkt->ack), ntohs(pkt->length));
	if (!syn && !ack)
	{
		fprintf(stderr, "NONE");
	}
	else
	{
		if (syn)
		{
			fprintf(stderr, "SYN ");
		}
		if (ack)
		{
			fprintf(stderr, "ACK ");
		}
	}
	fprintf(stderr, "\n");
}

void serialize(Packet &pkt){
    pkt.ack = htonl(pkt.ack);
    pkt.seq = htonl(pkt.seq);
    pkt.length = htons(pkt.length);
}

void deserialize(Packet &pkt){
    pkt.ack = ntohl(pkt.ack);
    pkt.seq = ntohl(pkt.seq);
    pkt.length = ntohs(pkt.length);
}


