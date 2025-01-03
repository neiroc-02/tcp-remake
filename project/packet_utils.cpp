#include "packet_utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>	 //for atoi
#include <stdio.h>	 //for fprintf
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
	/* Convert packet to network order */
    pkt.ack = htonl(pkt.ack);
    pkt.seq = htonl(pkt.seq);
    pkt.length = htons(pkt.length);
}

void deserialize(Packet &pkt){
	/* Convert packet to host order */
    pkt.ack = ntohl(pkt.ack);
    pkt.seq = ntohl(pkt.seq);
    pkt.length = ntohs(pkt.length);
}

void clean_send_buffer(uint32_t ACK, vector<Packet> &send_buffer){
	/* Iterate through the send buffer to delete any element less than the ACK number */
	while (!send_buffer.empty() && send_buffer.at(0).seq < ACK){
		send_buffer.erase(send_buffer.begin());
	}
}

void handle_ack(uint32_t &ACK, uint32_t &ack_count, const Packet &pkt, vector<Packet> &send_buffer){
	if (send_buffer.empty()) return;
	/* If we have duplicate acks, update the counter */
	fprintf(stderr, "IN HANDLE ACK...\n");
	fprintf(stderr, "ACK: %d\n", ACK);
	fprintf(stderr, "PKT_ACK: %d\n", pkt.ack);
	uint32_t expected_ack = send_buffer.at(0).seq; //the lowest seq number you haven't recieved an ack for is at the top of send_buffer
	if (pkt.ack == expected_ack){
		ack_count++;
	}
	/* If we have a larger ACK, update ACK and clean the send buffer */
	else if (pkt.ack > expected_ack){
		ack_count = 0;
		clean_send_buffer(pkt.ack, send_buffer);
	}
}

void clean_recv_buffer(uint32_t &ACK, vector<Packet> &recv_buffer){
	/* Sort the recv buffer before you attempt to clean up */
	sort(recv_buffer.begin(), recv_buffer.end());
	
	/* Printing the buffer for debugging */
	fprintf(stderr, "BUFF SIZE: %d\n", recv_buffer.size());
	for (int i = 0; i < recv_buffer.size(); i++){
		fprintf(stderr, "%d ", recv_buffer.at(i).seq);
	}
	fprintf(stderr, "\n");

	/* Check if you can print/delete elements if the seq/ack numbers are aligned */
	while (!recv_buffer.empty() && recv_buffer.at(0).seq == ACK){
		write(1, recv_buffer.at(0).payload, recv_buffer.at(0).length);
		ACK += recv_buffer.at(0).length;
		recv_buffer.erase(recv_buffer.begin());
	}
}
