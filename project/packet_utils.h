#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	 //for non-blocking sockets
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
void print_diag(Packet *pkt, int diag);   //for printing packets
void serialize(Packet &pkt);
void deserialize(Packet &pkt);
/* TODO: Work on later...
vector<Packet> clean_send_buffer(int ACK, vector<Packet> send_buffer);
vector<Packet> clean_recv_buffer(int ACK, vector<Packet> recv_buffer);
void handle_ack(int ACK, int ack_count, vector<Packet> send_buffer);
void handle_data(int ACK, vector<Packet> recv_buffer);
*/
