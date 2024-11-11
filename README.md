# CS 118 Fall 24 Project 1
## Design Decisions
I initially decided to design it all in main with only an outside handshake function for both server and client. 
This strategy did not work as I was unable to test my code throughly and ran into a large number of bugs.
I then decided to modularize my code to make it easier to debug by making a `packet_utils.h` and `packet_utils.cpp` file. 
This made it easier to test my code and I was able to find and fix bugs more easily. 
My main function was broken into four parts: handshake, stdin handling, packet handing, and retransmission. 
The handshake part was responsible for setting up the connection between the server and client.
The stdin handling part was responsible for reading from stdin and sending packets to the server.
The packet handling part was responsible for receiving packets from the server, which could be ACKs, Data packets, or both.
The retransmission part was responsible for retransmitting packets that were not ACKed by the server by tracking the timer and ACK counts. 

## Problems and Solutions
Some of the problems I had included not properly converting between endianness.
I initially converted the length using `htonl()` and `ntohl()` but I was not able to properly convert the length of the packet.
This was because I was treating it as a long instead of a short, which ended up 0ing out the length value.
I have also had problems with looping when handling duplicate packets.
Sometimes my code would continue retransmitting and not resend the needed ACK to break the infinite loop.
I have continued to try to fix this issue but it is still present in my code.
I have been debugging my recieve function and believe it has issues when the ACK gets behind the expected sequence number.



