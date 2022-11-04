#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "monitor_neighbors.hpp"
#include <map>
#include <iostream>
#include <vector>
#include <thread>

using namespace std;

#define DEBUG 1
#define DEBUG_PRINTF(fmt, ...)\
	do {if (DEBUG) fprintf(stderr, "node%d:%s:%d:%s(): " fmt "\n", globalMyID, __FILE__,\
	__LINE__,__func__,__VA_ARGS__); } while (0)

#define DEBUG_PRINT(text)\
	do {if (DEBUG) fprintf(stderr, "node%d:%s:%d:%s(): " text "\n", globalMyID, __FILE__,\
	__LINE__,__func__);} while (0)

void listenForNeighbors();
void* announceToNeighbors();
void* checkHeartbeat();


int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];
map<int, map<int, int>> LSA;
map<int, pair<int, int>> SP;
int seq;
map<int, int> seq_nums;
string f_name;
ofstream file;
 
int main(int argc, char** argv)
{
	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", argv[0]);
		exit(1);
	}
	
	f_name = argv[3];
	file.open(f_name);
	// file.open(argv[3]);
	//initialization: get this process's node ID, record what time it is, 
	//and set up our sockaddr_in's for sending to the other nodes.


	globalMyID = atoi(argv[1]);
	int i;
	for(i=0;i<256;i++)
	{
		seq_nums[i] = -1;
		gettimeofday(&globalLastHeartbeat[i], 0);
		
		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
	}
	seq_nums[globalMyID] = 0;

	

	// map<int, int> Adj;


	int node;
	int cost;
	seq = 0;
	FILE* f = fopen(argv[2], "r");
	while (fscanf(f, "%d %d", &node, &cost) != EOF) {
		LSA[globalMyID][node] = cost;
	}
	
	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}
	
	
	//start threads... feel free to add your own, and to remove the provided ones.
	// pthread_t announcerThread;
	// pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);
	thread ann(announceToNeighbors);
	thread lis(listenForNeighbors);
	thread check(checkHeartbeat);
	ann.join();
	lis.join();
	check.join();

	
	
	//good luck, have fun!
	// listenForNeighbors();
	
	
	
}
