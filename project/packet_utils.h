#pragma once
#include <vector>
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
#define MSS 1012 // MSS = Maximum Segment Size (aka max length)
using namespace std;

typedef struct
{
	uint32_t ack;
	uint32_t seq;
	uint16_t length;
	uint8_t flags;
	uint8_t unused;
	uint8_t payload[MSS];
} Packet;

bool operator<(const Packet&a, const Packet &b);        //for comparing packets
void print_diag(Packet *pkt, int diag);   				//for printing packets
void serialize(Packet &pkt);							//for changing packets to network order
void deserialize(Packet &pkt);							//for changing packets to host order
void clean_send_buffer(uint32_t ACK, vector<Packet> &send_buffer); //
void clean_recv_buffer(uint32_t &ACK, vector<Packet> &recv_buffer);
void handle_ack(uint32_t &ACK, uint32_t &ack_count, const Packet &pkt, vector<Packet> &send_buffer);
