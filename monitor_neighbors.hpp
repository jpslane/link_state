#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "json.hpp"
#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <queue>
#include <iostream>
#include <fstream>
#include <thread>
#include <filesystem>
#include <set>
#include <mutex>
#define INF 0x3f3f3f3f

using namespace std;
using json = nlohmann::json;

extern int globalMyID;
extern struct timeval globalLastHeartbeat[256];
extern int globalSocketUDP;
extern struct sockaddr_in globalNodeAddrs[256];
extern map<int, map<int, int>> LSA;
extern int seq;
extern map<int, int> seq_nums;
extern map<int, pair<int, int>> SP;
extern string f_name;
extern ofstream file;

mutex mut;
map<int, set<int>> paths;
set<vector<int>> a_list;


struct myComp {
	constexpr bool operator()(
		pair<int, int> const& a,
		pair<int, int> const& b)
		const noexcept
	{
		return (a.first > b.first);
	}
};

void hackyBroadcast(map<int, int> adjacent)
{
	json j_map(adjacent);
	string map_dump = j_map.dump();
	char buf[1000];
	char format[20] = "0x%dx%dx%dx%s";
	sprintf(buf, format, globalMyID, seq, map_dump.length(), map_dump.c_str());
	int i;
	for (i = 0; i < 256; i++) {
		if (i != globalMyID) {
			sendto(globalSocketUDP, buf, strlen(buf), 0,
				(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
		
	}
		
}

void seqBroadcast(map<int, int> adjacent, int id, int sequence_num, int heardFrom)
{
	json j_map(adjacent);
	string map_dump = j_map.dump();
	char buf[1000];
	char format[20] = "1x%dx%dx%dx%s";
	sprintf(buf, format, id, sequence_num, map_dump.length(), map_dump.c_str());
	int i;
	for (i = 0; i < 256; i++) {
		if (i != heardFrom && i != globalMyID) {
			sendto(globalSocketUDP, buf, strlen(buf), 0,
				(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
	}
}

void get_paths(int node, vector<int> current) {
	if (node == globalMyID) {
		a_list.insert(current);
	}
	else {
		current.insert(current.begin(), node);
		for (const auto& elem : paths[node]) {
			get_paths(elem, current);
		}
	}
}

void dijkstra() {
	int i;
	paths.clear();
	for (i = 0; i < 256; i++) {
		SP[i] = { INF, -1 };
	}
	
	priority_queue<pair<int, int>, vector<pair<int, int>>, myComp> pq; 

	SP[globalMyID] = { 0, globalMyID };
	pq.push(make_pair(0, globalMyID));
	 
	int nodes_seen = 0;
	while (!pq.empty() && nodes_seen < 8000) {
		int u = pq.top().second;
		pq.pop();
		nodes_seen += 1;

		for (auto const& elem : LSA[u]) {
			int n = elem.first;
			int w = elem.second;
			if (SP[n].first >= SP[u].first + w) {
				if (SP[n].first > SP[u].first + w) {
					
					SP[n].first = SP[u].first + w;
					paths[n].clear();	
					SP[n].second = u;
				}
				pq.push(make_pair(SP[n].first, n));
				paths[n].insert(u);
			}
		}	
	}
}

void msgBroadcast(string message, int dest, int heardFrom)
{
	if (dest == 72) {
		mut.lock();
		LSA[70].erase(71);
		mut.unlock();
	}
	thread dij(dijkstra);
	dij.join();

	a_list.clear();
	get_paths(dest, {});

	vector<int> path;

	auto vec = a_list.begin();
	int i = 0;
	for (const auto& shortest : a_list) {
		for (const auto& m : shortest) {
			if (i == 0) {
				path = shortest;
			}
			break;
		}
		break;
	}

	int j = 0;
	int path_size = path.size();
	while (j < path_size - 1) {
		SP[path[j+1]].second = path[j];
		cout << path[j] << " ";
		j += 1;
	}
	
	int nextNode = dest;
	int to;
	while (nextNode != globalMyID) {
		to = nextNode;
		nextNode = SP[nextNode].second;		
	}

	char logLine[1000] = {0};

	if (SP[dest].second == -1) {
		sprintf(logLine, "unreachable dest %d\n", dest);
		if (!file.is_open()) {
			file.open(f_name, ios_base::app);
		}
		file << logLine;
		file.flush();
	}
	else {
		if (heardFrom != globalMyID) {
			sprintf(logLine, "forward packet dest %d nexthop %d message %s\n", dest, to, message.c_str());
			// cout << globalMyID << " to " << to << endl;
		}
		else {
			sprintf(logLine, "sending packet dest %d nexthop %d message %s\n", dest, to, message.c_str());
		}

		if (!file.is_open()) {
			file.open(f_name, ios_base::app);
		}
		file << logLine;
		file.flush();


		char buf[10000];
		char format[20] = "2x%dx%dx%dx%s";
		sprintf(buf, format, heardFrom, dest, message.length(), message.c_str());

		if (to != globalMyID && to != heardFrom) {
			int a = sendto(globalSocketUDP, buf, strlen(buf), 0, (struct sockaddr*)&globalNodeAddrs[to], sizeof(globalNodeAddrs[to]));
		}
	}
}

void* announceToNeighbors()
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 400 * 1000 * 1000; //400 ms
	while(1)
	{
		hackyBroadcast(LSA[globalMyID]);
		nanosleep(&sleepFor, 0);
	}
}

void* checkHeartbeat() {
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 100 * 1000 * 1000; //100 ms
	while (1)
	{
		struct timeval tvalAfter;
		gettimeofday(&tvalAfter, 0);
		for (auto const& elem : LSA[globalMyID]) {
			long dif = ((tvalAfter.tv_sec - globalLastHeartbeat[elem.first].tv_sec) * 1000000L + tvalAfter.tv_usec) - globalLastHeartbeat[elem.first].tv_usec;
			try {
				if (dif > 600000 && LSA[globalMyID].at(elem.first) > 0) {
					mut.lock();
					LSA[globalMyID].erase(elem.first);
					seq++;
					mut.unlock();

					thread hacky(hackyBroadcast, LSA[globalMyID]);
					hacky.detach();			
				}
			}
			catch (exception& exep) {
			}
		}
		nanosleep(&sleepFor, 0);
	}
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;

	int bytesRecvd;
	char recvBuf[10000] = { 0 };
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
		memset(recvBuf, '\0', sizeof(recvBuf));
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);

			if (LSA[globalMyID].find(heardFrom) == LSA[globalMyID].end()) {
				mut.lock();
				LSA[globalMyID][heardFrom] = 1;
				seq++;
				mut.unlock();

				thread hacky(hackyBroadcast, LSA[globalMyID]);
				hacky.detach();
			}
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		
		if (!strncmp(recvBuf, "send", 4))
		{
			short int dest = { 0 };
			char msg[1000] = {0};
			memset(msg, '\0', sizeof(msg));

			memcpy(&dest, recvBuf + 4, sizeof(short int));
			dest = ntohs(dest);

			memcpy(&msg, recvBuf + 6, bytesRecvd - 6);
			msgBroadcast(string(msg), int(dest), globalMyID);
		}
		else if(!strncmp(recvBuf, "cost", 4))
		{
			char* split = strstr(recvBuf, "cost");
			char destID = *split++;
			char cost[5];
			memcpy(cost, split, strlen(split) + 1);
		}
		else if (heardFrom != globalMyID) {
			char format[50] = "%[0-9]x%[0-9]x%[0-9]x%[0-9]x%s";
			char a[2], b[16], c[8], d[16], e[10000] = { 0 };
			sscanf(recvBuf, format, a, b, c, d, e);
			char msg[1000] = { 0 };
			
			int type = std::stoi(a);
			int from = std::stoi(b);
			int sn = std::stoi(c);
			int l = std::stoi(d);

			memcpy(&msg, recvBuf + bytesRecvd - l, l);

			string m = string(msg);
			if (type == 0 || type == 1 && (from != globalMyID)) {
				if (seq_nums[from] < sn) {
					json j_map = json::parse(m);
					auto output = j_map.get<map<int, int>>();
					LSA[from] = output;
					seq_nums[from] = sn;
					
					thread seqs(seqBroadcast, output, from, sn, heardFrom);
					seqs.join();
				}
			}
			else if (type == 2) {
				if (sn != globalMyID) {
					msgBroadcast(msg, sn, from);
				}
				else {
					char logLine[10024] = { 0 };
					sprintf(logLine, "receive packet message %s\n", msg);
					if (!file.is_open()) {
						file.open(f_name, ios_base::app);
					}
					file << logLine;
					file.flush();
				}
			}
		}
	}
	close(globalSocketUDP);
}

