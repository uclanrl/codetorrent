/*
 * main.cpp
 *
 * Main CodeTorrent routines
 *
 * Coded by Joe Yeh/Uichin Lee
 * Modified and extended by Seung-Hoon Lee(shlee@cs.ucla.edu)/Sung Chul Choi(schoi@cs.ucla.edu)
 * University of California, Los Angeles
 *
 * 
 *
 * Last modified: 04/02/07
 *
 */

//#define _CRTDBG_MAP_ALLOC 1
#include <stdlib.h>
//#include <crtdbg.h>

//#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
//#include <cmath>
#include <math.h>
#include <errno.h>

/*
#ifdef WIN32
#include <winsock2.h>	// socket library for win32
#include "ws2tcpip.h"	// tcp/up library
#else
*/
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
//#endif

// compatibility issue with time.h
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif

#include <sys/timeb.h>
#include "pthread.h"

#include "codetorrent.h"

#define CT_FILEINFO	0			// File info
#define CT_DATA		1			// CodedBlock
#define CT_REQUEST	2			// Request
#define CT_GOSSIP	3			// Gossip

#define MAX_RANK 10000
#define BACKLOG 10				// how many connections?
#define MAX_PACKET_SIZE 300000	// temporary

#define S_Port 7788
#define L_Port 7789

#define BACKLOG 10		// how many connections?

typedef unsigned int  SOCKET;
typedef struct sockaddr SOCKADDR;

bool server_mode = false;
bool decoding = false;
bool verbose = false;
int gossip_freq = 2000;		// frequency of gossips in ms

// define packet
/*
struct CT_packet
{
	int header;
	int gen;
	unsigned long address;
	CodedBlock cb;
};
*/

typedef struct _CT_header {

	int type;				// type
	int file_id;			// file id
	int gen;				// generation
	unsigned long address;	// internet address
	int size;				// size of the packet (including the header)
	int nodeId;
} CT_header;

// file info struct
typedef struct _CT_FileInfo {
	int filesize;						// file size
	int blocksize;						// block size
	unsigned long num_blocks_per_gen;	// number
	char filename[MAX_FILE_NAME_LEN];	// file name
	
} CT_FileInfo;


// define a vector to manage my neighbor nodes.
struct CT_neighbor
{
	unsigned long address;
	bool* gen_vector;

#ifdef WIN32
	int timestamp;
#else
	struct timeval timestamp;
#endif
	bool helpful;
};


#ifdef WIN32
	int tv_beg, tv_end, tv_last, tv_lastgossip, tv_curr;
	void *tz;
#else
	struct timeval ct_beg, ct_end;
	struct timeval tv_beg, tv_end, tv_lastgossip, tv_curr;
	struct timezone tz;
#endif

// public variables
CodeTorrent *ct;						// main CodeTorrent module
std::vector<CT_neighbor> ct_neighbors;	// neighbors list

int helpful_neighbors=0;
int unhelpful_neighbors=0;

pthread_mutex_t send_mutex;
int nodeId = 1;
///////////////////////////////////////////////////////////////////////
void* incoming_packet(void *buf);
void reEncode(CT_header* ct_header);
void send_codedBlock(CodedBlock* cb, unsigned long address);
void send_fileInfo_request(unsigned long address);
void* send_gossip(void *voidptr);
void* send_request(void *voidptr);
void send_request();
void store_block(CT_header* ct_header, char* data);
void update_neighbors(CT_header* ct_header);
void send_fileInfo(CT_header* ct_header);
char *pack(int type, int gen, unsigned long address, int fileId, CodedBlockPtr cb);
void* receive_gossip(void *in_buf);
long GetTimeDifference(struct timeval tv_beg, struct timeval tv_end);
bool ap_detection();

/*
unsigned short checksum(unsigned short *buf,int nwords)
{
	unsigned long sum;
	for (sum = 0; nwords > 0; nwords--)
		sum += *(buf)++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}
*/

//store_block(CT_header ct_header, char* data)
//void* store_block(void *in_packet)

void print_coeffs(char *data) {
	
	printf("coeffs begin with : ");

	for(int i=0; i< 5 ; i++ ) {
		printf("%x ", *(data+i));
	}

	printf("\n");
}


void store_block(CT_header* ct_header, char* data)
{
	bool check_file = false;

	// check file = ture: this client already has the file
	// check file = false : does not have the file, so request file_info to the sender!
/*
	for(int i=0; i< ct->GetNumFiles(); i++)	{
		if( ct->GetFileID(i) == ct_header->file_id ) check_file = true;
	}
	*/

	check_file = (ct_header->file_id == 0);

	if ( check_file == true ){

		CodedBlock temp_cb;
		temp_cb.gen = ct_header->gen;
		temp_cb.num_blocks_gen = ct->GetNumBlocksGen(ct_header->gen);
		temp_cb.block_size = ct->GetBlockSize();
		temp_cb.coeffs = (CoeffsPtr)data;
		temp_cb.sums = (BlockPtr)(data+ct->GetNumBlocksGen(ct_header->gen));

		printf("[store_block] Trying to store block in gen %d...\n", ct_header->gen);

		ct->StoreBlock(&temp_cb);

		printf("::Currently received::\n| ");

		for(int i=0; i < ct->GetNumGens() ; i++) {
			printf("%2d | ", ct->GetRankVec()[i]);
		}
		
		printf("\n");

		//update_neighbors(ct_header);

		//send_request(); // will be implemented
	}
	else { // request file_info	
		send_fileInfo_request(ct_header->address);
	}
}

//reEncode(temp_header);
//void* reEncode(void *in_packet)
void reEncode(CT_header* ct_header)
{
	CodedBlockPtr temp_codedBlock;
	struct timeval encoding_beg, encoding_end;
	//gettimeofday( &(*it).timestamp, &tz);


	if( !server_mode ) {
		temp_codedBlock = ct->ReEncode(ct_header->gen);
		printf("[reEncode] Re-encode a packet from gen %d...\n", ct_header->gen);
	}
	else {
		gettimeofday( &encoding_beg, &tz);
		temp_codedBlock = ct->Encode(ct_header->gen);
		gettimeofday( &encoding_end, &tz);
		printf("[reEncode] Encode a packet from gen %d...\n", ct_header->gen);
	}


	printf("[ReEncode] Encoding Time: %ld.%03ld\n", GetTimeDifference(encoding_beg, encoding_end)/1000, GetTimeDifference(encoding_beg, encoding_end)%1000);

	send_codedBlock(temp_codedBlock, ct_header->address);
	//ct->FreeCodedBlock(temp_codedBlock);
	//pthread_exit("reEncode done\n");

}


void* incoming_packet(void *in_buf) {

	//case CT_FILEINFO: file_info
	//					ct->update

	//case CT_DATA:		data
	//					store_block(CT_header* ct_header, char* data)

	//case CT_REQUEST:  request
	//					reEncode(CT_header* ct_header)

	char *buf = (char *)in_buf;
	struct timeval decoding_beg, decoding_end;

	if ( decoding ) {
		
		delete [] buf;
		pthread_exit((void*)NULL); // terminate this thread.
		return NULL;
	}

	CT_header *temp_header = (CT_header*)buf;

	if( verbose )
		printf("[incoming_packet] Received %d: ", temp_header->type);

	switch( temp_header->type )
	{

		case CT_FILEINFO: 
			{
				if( verbose )
					printf("file info.\n");
		
				if( !server_mode ) {
					if( ct == NULL ) {	// NOTE: we are dealing with only one file: just init once.
						CT_FileInfo *temp_fileInfo = (CT_FileInfo*)(buf+sizeof(CT_header));
						ct = new CodeTorrent( 8, temp_header->file_id, temp_fileInfo->num_blocks_per_gen, temp_fileInfo->blocksize, 
																					temp_fileInfo->filename, temp_fileInfo->filesize, false);
						gettimeofday( &ct_beg, &tz);
						update_neighbors(temp_header);
						//send_request();
					}
				}
				break;
			}
		case CT_DATA:	
			{	
				if( verbose )
					printf("data.\n");
				if( !server_mode ) {
					
					//char* data = new char[]; //
					//char *data = (char *)(buf + sizeof(temp_header));
					char *data = (char *)(buf + sizeof(CT_header));

					//added
					update_neighbors(temp_header);

					store_block(temp_header, data);

					if( ct->DownloadCompleted() ) {
						decoding = true;

						gettimeofday( &ct_end, &tz);
						printf("[main] Total Download Time: %ld.%03ld\n", GetTimeDifference(ct_beg, ct_end)/1000, GetTimeDifference(ct_beg, ct_end)%1000);

						gettimeofday( &decoding_beg, &tz);
						ct->Decode();
						gettimeofday( &decoding_end, &tz);
						printf("[main] Download Time/ block: %ld.%03ld\n", GetTimeDifference(decoding_beg, decoding_end)/1000, GetTimeDifference(decoding_beg, decoding_end)%1000);
						server_mode = true;
						decoding = false;
					}
				}
				break;
			}
		case CT_REQUEST:
			{
				if ( temp_header->gen < 0 )  // file_info is requested.
				{
					if( verbose )
						printf("file info request.\n");		
					if( server_mode )		// NOTE:temporary
						send_fileInfo(temp_header);
				}
				else
				{						//else, coded block is requested.
					if( verbose )
						printf("data request.\n");

					reEncode(temp_header);
				}	
				break;
			}

		//case CT_GOSSIP:
		//	break;

		default:
			if( verbose )
				printf("unknown packet.\n");
	};

	delete[] buf;
	pthread_exit((void*)NULL); // terminate this thread.

	return NULL;
}
void send_fileInfo(CT_header* ct_header)
{
	SOCKET send_sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in add;
	memset(&add, 0, sizeof(add));
	add.sin_family = AF_INET;
#ifdef WIN32
	add.sin_addr.S_un.S_addr = ct_header->address;
#else
	add.sin_addr.s_addr = ct_header->address;
#endif
	add.sin_port = htons(S_Port);

	int yes = 1;

	if (setsockopt(send_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) == -1) {
		printf("[send_fileInfo] setsockopt error");
	}

	int res;

	res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); //connect
	if(res != 0)
		printf("[send_fileInfo] connect failed\n");

	//call pack()
	char* temp = pack(CT_FILEINFO, 0, ct_header->address, 0, NULL); // the second parameter -1 means requesting file_info

	CT_header *header = (CT_header *)temp;

	//res = send(send_sock, (char *)temp, sizeof(temp), 0);   //send	// NOTE: Can I do this?
	pthread_mutex_lock(&send_mutex);
	res = send(send_sock, (char *)temp, header->size, 0);   //send	// NOTE: Can I do this?
	pthread_mutex_unlock(&send_mutex);

	if( verbose )
		printf("[send_fileInfo] Sent file info (%d/%d) to %s.\n", res, header->size, inet_ntoa(add.sin_addr));

	if (res != header->size)
		printf("[send_fileInfo] send_codedBlock failed.\n");

	free(temp);
	//closesocket(send_sock);
	close(send_sock);

}

// packs a packet upon request by other functions
char *pack(int type, int gen, unsigned long address, int fileId, CodedBlockPtr cb) {

	// WARNING: *pkt is not freed in this function! Make sure you free it somewhere!
	char *pkt;
	CT_header *header;
	int size;

	switch(type) {

		case CT_FILEINFO:
		{
			CT_FileInfo *info;
			int rand_gen = rand() % ct->GetNumGens();

			while(ct->GetRankVec()[rand_gen] <= 0) {
				rand_gen = rand() % ct->GetNumGens();
			}

			// set the size and allocate memory
			//size = sizeof(CT_header)+12+strlen(ct->GetFileName())+1;
			size = sizeof(CT_header)+sizeof(CT_FileInfo);
			pkt = (char *)malloc(size);

			// header
			header = (CT_header *)pkt;
			header->type = type;
			header->file_id = fileId;
			//header->gen = -1;
			header->gen = rand_gen;
			header->address = address;
			header->size = size;

			if( verbose )
				printf("[pack:file info] rand_gen = %d\n", rand_gen);

			// content
			info = (CT_FileInfo *)(pkt + sizeof(CT_header));
			info->filesize = ct->GetFileSize();
			info->blocksize = ct->GetBlockSize();
			info->num_blocks_per_gen = ct->GetNumBlocksGen(0);
			strcpy(info->filename, ct->GetFileName());

			break;
		}
		case CT_DATA:
			
			// set the size and allocate memory
			size = sizeof(CT_header)+ct->GetNumBlocksGen(gen)+ct->GetBlockSize();
			pkt = (char *)malloc(size);

			// header
			header = (CT_header *)pkt;
			header->type = type;
			header->file_id = fileId;
			header->gen = gen;
			header->address = address;
			header->size = size;
			
			// content
			memcpy((char *)(pkt + sizeof(CT_header)), cb->coeffs, ct->GetNumBlocksGen(gen));
			memcpy((char *)(pkt + sizeof(CT_header) + ct->GetNumBlocksGen(gen)), cb->sums, ct->GetBlockSize());

			ct->FreeCodedBlock(cb);

			break;

		case CT_REQUEST:
			// set the size and allocate memory
			size = sizeof(CT_header);
			pkt = (char *)malloc(size);

			// header
			header = (CT_header *)pkt;
			header->type = type;
			header->file_id = fileId;
			header->gen = gen;
			header->address = address;
			header->size = size;
			
			// content

			break;

		case CT_GOSSIP:

			// set the size and allocate memory
			size = sizeof(CT_header);
			pkt = (char *)malloc(size);
			
			// header
			header = (CT_header *)pkt;
			header->type = type;
			header->file_id = fileId;
			header->gen = gen;
			header->address = address;
			header->size = size;
			header->nodeId = nodeId;
			break;
	}

	return pkt;
}

void send_codedBlock(CodedBlock* cb, unsigned long address)
{
	SOCKET send_sock;
	
	// set destination address
	struct sockaddr_in add;

	memset(&add, 0, sizeof(add));
	add.sin_family = AF_INET;
#ifdef WIN32
	add.sin_addr.S_un.S_addr = address;
#else
	add.sin_addr.s_addr = address;
#endif
	add.sin_port = htons(S_Port);

	int yes = 1, res;
	
	// if this is requested data, then send it via TCP
	send_sock = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(send_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) == -1) {
		printf("[send_codedBlock] setsockopt\n");
	}

	// connect to the client
	res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); // connect
		
	if(res != 0) {
		printf("[send_codedBlock] connect() failed\n");
	}
	
	// form a packet
	char * temp = pack(CT_DATA, cb->gen, address, 0, cb); // tyep = data, gen, address
	CT_header *header = (CT_header *)temp;

	// send this out
	pthread_mutex_lock(&send_mutex);
	//res = send(send_sock, (char *)temp, sizeof(temp), 0);   // send the data packet
	res = send(send_sock, (char *)temp, header->size, 0);
	pthread_mutex_unlock(&send_mutex);

	if( verbose )
		printf("[send_codedBlock] Sent %d/%d bytes from gen %d to %s\n", res, header->size, header->gen, inet_ntoa(add.sin_addr));
		
	if(res != header->size)
		printf("send() failed\n");

	res = 0;

	free(temp);
	//ct->FreeCodedBlock(cb);

	if (res)
		printf("[send_codedBlock] send_codedBlock() failed\n");

	close(send_sock);
}

void* send_gossip(void *voidptr)
{
	SOCKET send_sock;
	
	// set destination address
	struct sockaddr_in add;

	memset(&add, 0, sizeof(add));
	add.sin_family = AF_INET;
#ifdef WIN32
	add.sin_addr.S_un.S_addr = inet_addr("255.255.255.255");
#else
	add.sin_addr.s_addr = inet_addr("192.168.0.255");
#endif
	add.sin_port = htons(L_Port);

	int yes = 1, res = 0, num_gens;
	static int gen = 0;
	
	// if this is gossiping, then send it via UDP
	send_sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(int)) == -1) {
		printf("[send_gossip] setsockopt error\n");
	}

	while(1) {

		if( ct != NULL ) {
/*
			gen = rand() % ct->GetNumGens();
			
			while( ct->GetRankVec()[gen] <= 0)
				gen = rand() % ct->GetNumGens();
*/
			num_gens = ct->GetNumGens();

			do {
				
				gen++;
				
			} while ( ct->GetRankVec()[gen%num_gens] <= 0 );

			gen = gen%num_gens;
			
			if( verbose )
			printf("[send_gossip] Gossiping generation %d ...\n", (gen)%num_gens);

			unsigned long address = inet_addr("255.255.255.255"); //broadcasting

			// form a packet
			char * temp = pack(CT_GOSSIP, gen, address, 0, NULL); // tyep = data, gen, address
			CT_header *header = (CT_header *)temp;

			// send this out

			//res = sendto(send_sock, (char *)temp, sizeof(temp), 0, (SOCKADDR *)&add, sizeof(struct sockaddr));
			pthread_mutex_lock(&send_mutex);
			res = sendto(send_sock, (char *)temp, header->size, 0, (SOCKADDR *)&add, sizeof(struct sockaddr));
			pthread_mutex_unlock(&send_mutex);

			if( verbose )
				printf("[send_gossip] Sent %d/%d bytes to %s\n", res, header->size, inet_ntoa(add.sin_addr));
					
			if(res != header->size)
				printf("[send_gossip] sendto() failed\n");

			free(temp);	
		}

		sleep(gossip_freq); // gossip every 5 secs.
	}

	close(send_sock);

	pthread_exit(NULL);
	return NULL;
}

void* listen_gossip(void* voidptr) {

	SOCKET listenfd;	// receive on sock_fd, new connection on new_fd, listen(for gossips) on listenfd
	struct sockaddr_in my_addr_listen;	// my address information for listener (L_Port)
	struct sockaddr_in sender_addr;
    	socklen_t sin_size;
	int yes = 1;
	int res;

	/////// SOCKET
	if ((listenfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		printf("socket (listen)\n");
	}

	/////// SOCKET OPTIONS
   	if (setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int)) == -1) {
        	printf("setsockopt (listen)\n");
    	}

	/////// BIND
   	char hostname[255];
	gethostname(hostname, 255);
	hostent *he;
	if ((he = gethostbyname(hostname)) == 0) {
		printf("[listen_gossip] gethostbyname error!\\n");
		//pthread_exit("listen_gossip unexpectedly exits");
		pthread_exit(NULL);

		return NULL;
	}

	my_addr_listen.sin_family = AF_INET;			// host byte order
    my_addr_listen.sin_port = htons(L_Port);		// short, network byte order
    my_addr_listen.sin_addr.s_addr = htonl(INADDR_ANY);// automatically fill with my IP
    memset(&(my_addr_listen.sin_zero), '\0', 8);	// zero the rest of the struct

	if (bind(listenfd, (struct sockaddr *)&my_addr_listen, sizeof(struct sockaddr)) == -1) {
        printf("[listen_gossip] bind (listen)\n");
    }

	// assumes buf will not be used too long; not until the next gossip
	sin_size = sizeof(struct sockaddr);
	pthread_t incoming_thread;

	while(1) {

		char *buf = new char[MAX_PACKET_SIZE];
		buf[0] = '\0';
	
		res = recvfrom(listenfd, buf, MAX_PACKET_SIZE, 0, (sockaddr *)&sender_addr, &sin_size);

#ifdef WIN32
		//if( my_addr_listen.sin_addr.S_un.S_addr == sender_addr.sin_addr.S_un.S_addr ) {
#else
		//if( my_addr_listen.sin_addr.s_addr == sender_addr.sin_addr.s_addr ) {
#endif
		
		printf("[listen_gossip] # of Helpful neighbors: %d\n", helpful_neighbors);
		printf("[listen_gossip] # of UnHelpful neighbors: %d\n", unhelpful_neighbors);

		if(0){//if( verbose )
				printf("[listen_gossip] This is gossip from myself, drop it.\n");

		} else {

			//if( server_mode ) 
			//	break;

			//pthread_t incoming_thread;
			CT_header *header = (CT_header *)buf;
#ifdef WIN32
			header->address = sender_addr.sin_addr.S_un.S_addr;
#else
			header->address = sender_addr.sin_addr.s_addr;
#endif
		if( nodeId != header->nodeId){		
			if( verbose )
				printf("[listen_gossip] Gossip received from %s, gen=%d.\n", inet_ntoa(sender_addr.sin_addr), header->gen);

			res = pthread_create(&incoming_thread, NULL, receive_gossip, (void *)buf);

			if (res != 0)	{
				printf("receive_gossip() Thread Creation failed");
				return 0;	
			}
		}
		else{
			printf("[listen_gosip] Gossip from myself\n");
		}
		}
	}

	pthread_exit(NULL);
	return 0;
}

void* receive_gossip(void *in_buf) {
	
	char *buf = (char *)in_buf;
	CT_header *temp_header = (CT_header*)buf;

	if ( ct == NULL ) {

		struct in_addr tempaddr;
#ifdef WIN32
		tempaddr.S_un.S_addr = temp_header->address;
#else
		tempaddr.s_addr = temp_header->address;
#endif

		if( verbose )
			printf("[receive_gossip] Trying to send file info request to %s ...\n", inet_ntoa(tempaddr));

		send_fileInfo_request(temp_header->address);

	} else  {

		update_neighbors(temp_header);
		//send_request();
	}

	delete[] buf;
	pthread_exit((void *)NULL);
	return NULL;
}

//void update_neighbors(CT_packet *in_packet)
void update_neighbors(CT_header* ct_header)
{
	bool found = false;
	int i;

	
	if( verbose )
		printf("[update_neighbors] Updating neighbor list with gen %d\n", ct_header->gen);

	// find and update the right neighbor
	for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {

		if ((*it).address == ct_header->address) {
		
			(*it).gen_vector[ct_header->gen] = true;

			//added
			(*it).helpful = true;					//helpful!		  
			gettimeofday( &(*it).timestamp, &tz);   //updateing timestamp

			printf("[update_neighbors] timestamp is updated!!!\n");
			return;
		}
	}

	// if not found, update it
	CT_neighbor temp;
	temp.address = ct_header->address;
	temp.gen_vector = new bool[ct->GetNumGens()];

	// init
	for( i=0; i < ct->GetNumGens() ; i++ )
		temp.gen_vector[i] = false;

	temp.gen_vector[ct_header->gen] = true;

	//added
	gettimeofday( &temp.timestamp, &tz);
	temp.helpful = true;

	ct_neighbors.push_back(temp);
}


void send_request()
{
	printf("[send_request] Send Request Called\n");

	int i=0, j;
	int min = ct->GetNumBlocksGen(0);
	int request_gen;
	int num_gens = ct->GetNumGens();
	bool* has = new bool[num_gens];

	//added
	helpful_neighbors=0;
	unhelpful_neighbors=0;

#ifdef WIN32
	int current_time;
#else
	struct timeval current_time;
#endif

	for( i=0; i < num_gens; i++ )
		has[i] = false;

	//added
	gettimeofday( &current_time, &tz);
	for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {
		if ( GetTimeDifference( (*it).timestamp, current_time) > 5000000 ){
			(*it).helpful = false; //timeout!, 2sec
			unhelpful_neighbors++;
		}
		else helpful_neighbors++;
	}

	printf("[send_request] # of Helpful neighbors: %d\n", helpful_neighbors);
	printf("[send_request] # of UnHelpful neighbors: %d\n", unhelpful_neighbors);

        //AP range?
        bool ap = ap_detection();
        if (ap) printf("[send_request] AP is detected!!\n");
        else printf("[send_request]AP is not detected!!\n");


	if (helpful_neighbors > 0 || ap == true ) {
		for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {
			
			if ( (*it).helpful == true ) //only check with a helpful neighbor
				for ( j=0; j < num_gens; j++)
					has[j] = (*it).gen_vector[j] || has[j];
		}

		// if the last generation is full
		if( ct->GetNumBlocksGen(num_gens-1) == ct->GetRankVec()[num_gens-1] ) {
			has[num_gens - 1] = false;
		}

		//check rank_vec
		for( j=0 ; j < num_gens ; j++ )
		{
			if(has[j]) {
				if ((ct->GetRankVec()[j] <= min) && (ct->GetRankVec()[j] <= ct->GetNumBlocksGen(j))){ 
					min = ct->GetRankVec()[j];
					request_gen = j;
				}
			}
		}

		if ( ct->GetRankVec()[request_gen] == ct->GetNumBlocksGen(request_gen) )
			return;

		//look up the ct_neighbor table to find a node who has the gen
		i=0;
		unsigned long temp_address;
		struct sockaddr_in add_m;

		std::vector<unsigned long> addresses;

		for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {

			if ( (*it).helpful == true){ //inspect only for helpful neighbor

			#ifdef WIN32
				add_m.sin_addr.S_un.S_addr = (*it).address;
			#else
				add_m.sin_addr.s_addr = (*it).address;
			#endif
				printf("[send_request] inspecting %s for request_gen = %d\n", inet_ntoa(add_m.sin_addr), request_gen);
				printf("[send_request] gen_vector : ");

				for( int z = 0 ; z < num_gens ; z++ ) {
					if((*it).gen_vector[z] == true)
						printf("T");
					else
						printf("F");
				}
				printf("\n");	

				if ( (*it).gen_vector[request_gen] == true ) {
					//temp_address = (*it).address;
					addresses.push_back((*it).address);
				}
			}
		}

		if( addresses.size() > 0 || ap == true){ // only send a request when helpful neighbor is around me

		if( addresses.size() > 0){
			temp_address = addresses[rand()%(addresses.size())];

		#ifdef WIN32
			add_m.sin_addr.S_un.S_addr = temp_address;
		#else
			add_m.sin_addr.s_addr = temp_address;
		#endif

			printf("[send_request] selected %s\n", inet_ntoa(add_m.sin_addr));
		}
			//make a request packet looking for i-th gen
			/*
			CT_packet temp;
			temp.header = R;
			temp.gen = request_gen;
			temp.address = temp_address;
			*/

			//socket, 
			struct sockaddr_in add;
			memset(&add, 0, sizeof(add));
			add.sin_family = AF_INET;


			//AP range?
			/*
			bool ap = ap_detection();
			if (ap) printf("[send_request] AP is detected!!\n");
			else printf("[send_request]AP is not detected!!\n");
			*/

			printf("[send_request] APAPAPAP!!!!\n");
		#ifdef WIN32
			add.sin_addr.S_un.S_addr = temp_address;
			//add.sin_addr.S_un.S_addr = inet_addr("131.179.136.180");
		#else
			if (ap == true) //download from server through AP
				add.sin_addr.s_addr = inet_addr("192.168.120.101");
			else //download from peer
				add.sin_addr.s_addr = temp_address;
		#endif
			add.sin_port = htons(S_Port);// listening port

			int res, yes = 1;

			SOCKET send_sock = socket(AF_INET, SOCK_STREAM, 0);
			
			/////// SOCKET OPTIONS
			if (setsockopt(send_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int)) == -1) {
				printf("[send_request] setsockopt\n");
			}

			printf("[send_request] Trying to connect to %s to request data.\n", inet_ntoa(add.sin_addr));

			res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); //connect
			if(res != 0)
				printf("[send_request] connect failed\n");

			else{
			//call pack()
			char * temp = pack(CT_REQUEST, request_gen, temp_address, 0, NULL); // tyep = request, gen, address

			CT_header *header = (CT_header *)temp;

			//res = send(send_sock, (char *)&temp, sizeof(temp), 0);	// will this work?
			pthread_mutex_lock(&send_mutex);
			res = send(send_sock, (char *)temp, header->size, 0);	// will this work?
			pthread_mutex_unlock(&send_mutex);

			if ( res != header->size )
				printf("send_request Fail");
			free(temp);

			close(send_sock);}
		}//checking # of helpful neighbors > 0
	}
	delete[] has;
}

void* send_request(void* voidptr)
{
	printf("[send_request] Send Request Called\n");

	int i=0, j;
	int min = ct->GetNumBlocksGen(0);
	int request_gen;
	int num_gens = ct->GetNumGens();
	bool* has = new bool[num_gens];

	//added
	helpful_neighbors=0;
	unhelpful_neighbors=0;

#ifdef WIN32
	int current_time;
#else
	struct timeval current_time;
#endif

	for( i=0; i < num_gens; i++ )
		has[i] = false;

	//added
	gettimeofday( &current_time, &tz);
	for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {
		if ( GetTimeDifference( (*it).timestamp, current_time) > 5000000 ){
			(*it).helpful = false; //timeout!, 2sec
			unhelpful_neighbors++;
		}
		else helpful_neighbors++;
	}

	printf("[send_request] # of Helpful neighbors: %d\n", helpful_neighbors);
	printf("[send_request] # of UnHelpful neighbors: %d\n", unhelpful_neighbors);


        //AP range?
        bool ap = ap_detection();
        if (ap) printf("[send_request] AP is detected!!\n");
        else printf("[send_request]AP is not detected!!\n");

	if (helpful_neighbors > 0 || ap == true ) {
		for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {
			
			if ( (*it).helpful == true ) //only check with a helpful neighbor
				for ( j=0; j < num_gens; j++)
					has[j] = (*it).gen_vector[j] || has[j];
		}

		// if the last generation is full
		if( ct->GetNumBlocksGen(num_gens-1) == ct->GetRankVec()[num_gens-1] ) {
			has[num_gens - 1] = false;
		}

		//check rank_vec
		for( j=0 ; j < num_gens ; j++ )
		{
			if(has[j]) {
				if ((ct->GetRankVec()[j] <= min) && (ct->GetRankVec()[j] <= ct->GetNumBlocksGen(j))){ 
					min = ct->GetRankVec()[j];
					request_gen = j;
				}
			}
		}

		if ( ct->GetRankVec()[request_gen] == ct->GetNumBlocksGen(request_gen) )
			return NULL;

		//look up the ct_neighbor table to find a node who has the gen
		i=0;
		unsigned long temp_address;
		struct sockaddr_in add_m;

		std::vector<unsigned long> addresses;

		for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {

			if ( (*it).helpful == true){ //inspect only for helpful neighbor

			#ifdef WIN32
				add_m.sin_addr.S_un.S_addr = (*it).address;
			#else
				add_m.sin_addr.s_addr = (*it).address;
			#endif
				printf("[send_request] inspecting %s for request_gen = %d\n", inet_ntoa(add_m.sin_addr), request_gen);
				printf("[send_request] gen_vector : ");

				for( int z = 0 ; z < num_gens ; z++ ) {
					if((*it).gen_vector[z] == true)
						printf("T");
					else
						printf("F");
				}
				printf("\n");	

				if ( (*it).gen_vector[request_gen] == true ) {
					//temp_address = (*it).address;
					addresses.push_back((*it).address);
				}
			}
		}

		if( addresses.size() > 0 || ap == true){ // only send a request when helpful neighbor is around me

		if (addresses.size() > 0 ){
		temp_address = addresses[rand()%(addresses.size())];

		#ifdef WIN32
			add_m.sin_addr.S_un.S_addr = temp_address;
		#else
			add_m.sin_addr.s_addr = temp_address;
		#endif

			printf("[send_request] selected %s\n", inet_ntoa(add_m.sin_addr));
		}
			//make a request packet looking for i-th gen
			/*
			CT_packet temp;
			temp.header = R;
			temp.gen = request_gen;
			temp.address = temp_address;
			*/

			//socket, 
			struct sockaddr_in add;
			memset(&add, 0, sizeof(add));
			add.sin_family = AF_INET;

			//AP range?
			/*
			bool ap = ap_detection();
                        if (ap) printf("[send_request] AP is detected!!\n");
                        else printf("[send_request]AP is not detected!!\n");
			*/

		#ifdef WIN32
			add.sin_addr.S_un.S_addr = temp_address;
			//add.sin_addr.S_un.S_addr = inet_addr("131.179.136.180");
		#else
			if (ap == true) //download from server through AP
				add.sin_addr.s_addr = inet_addr("192.168.120.101");
			else //download from peer
				add.sin_addr.s_addr = temp_address;
		#endif

			add.sin_port = htons(S_Port);// listening port

			int res, yes = 1;

			SOCKET send_sock = socket(AF_INET, SOCK_STREAM, 0);
			
			/////// SOCKET OPTIONS
			if (setsockopt(send_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int)) == -1) {
				printf("[send_request] setsockopt\n");
			}

			printf("[send_request] Trying to connect to %s to request data.\n", inet_ntoa(add.sin_addr));

			res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); //connect
			if(res != 0)
				printf("[send_request] connect failed\n");

			else{
			//call pack()
			char * temp = pack(CT_REQUEST, request_gen, temp_address, 0, NULL); // tyep = request, gen, address

			CT_header *header = (CT_header *)temp;

			//res = send(send_sock, (char *)&temp, sizeof(temp), 0);	// will this work?
			pthread_mutex_lock(&send_mutex);
			res = send(send_sock, (char *)temp, header->size, 0);	// will this work?
			pthread_mutex_unlock(&send_mutex);

			if ( res != header->size )
				printf("send_request Fail");
			free(temp);

			close(send_sock);}
		}//checking # of helpful neighbors > 0
	}
	delete[] has;
	pthread_exit(NULL);
}

void* periodic_request(void* voidptr) {

	bool ct_init = false;

	printf("Periodic request!!\n");
	
	while( !ct_init ) {

		sleep(1);	// check every 1 sec

		if( ct != NULL ) {
			ct_init = true;			
		}
	}

	// once ct's initialized, compute the time interval
	// NOTE: for now, assume 802.11b = 11Mbps
	int period;
	double alpha;
	alpha = 70.0;		// adjust alpha to slow/fasten the requests

	period = (int)( 0.7 * alpha * (double)(ct->GetBlockSize()) / 11000000.0 * 8);			
	//period = (int)(1000 * alpha * (double)(ct->GetBlockSize()) / 800000.0 * 8);
	printf("Period %d\n", period);

	pthread_t send_request_thread;
	while( 1 ) {

		printf("[periodic_request] send_request calling!\n");
		
		//pthread_t send_request_thread;						
// thread

		int res = pthread_create(&send_request_thread, NULL, send_request, (void *)voidptr);
		if (res != 0)	{
			printf("ERROR: %s\n", strerror(errno));
			printf("[periodic_request] send_request() Thread Creation failed");
			return 0;
		}
	
		//send_request();
		sleep(period);

		if(ct->GetIdentity() == CT_SERVER) {
			break;
		}
	}

	pthread_exit(NULL);
	return NULL;
}

void send_fileInfo_request(unsigned long address)
{
// send fileInfo_request to the Address

	SOCKET send_sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in add;
	memset(&add, 0, sizeof(add));
	add.sin_family = AF_INET;
#ifdef WIN32
	add.sin_addr.S_un.S_addr = address;
#else
	add.sin_addr.s_addr = address;
#endif
	add.sin_port = htons(S_Port);

	int res;

	res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); //connect
	if(res != 0)
		printf("[send_fileInfo_request] connect failed\n");

	//call pack()
	char* temp = pack(CT_REQUEST, -1, address, 0, NULL); // the second parameter -1 means requesting file_info

	CT_header *header = (CT_header *)temp;

	//res = send(send_sock, (char *)&temp, sizeof(temp), 0);   //send	
	pthread_mutex_lock(&send_mutex);
	res = send(send_sock, (char *)temp, header->size, 0);   //send	
	pthread_mutex_unlock(&send_mutex);

	printf("[send_fileInfo_request] Sent file info request (%d/%d).\n", res, header->size);

	if (res != header->size)
		printf("send_codedBlock Fail");
	free(temp);
	close(send_sock);

}


// find the difference between tv_beg and tv_end
long GetTimeDifference(struct timeval tv_beg, struct timeval tv_end) {

	return ((tv_end.tv_usec-tv_beg.tv_usec)+(tv_end.tv_sec-tv_beg.tv_sec)*1000000);
}

#ifdef WIN32
long GetTimeDifference(int beg, int end) {

	return (end-beg);
}
#endif

#ifdef WIN32
void gettimeofday(int *time, void *tz) {

	*time = (int)GetTickCount();
	return;
	
}
#endif

void printUsage() {

	fprintf(stderr, "CodeTorrent v0.1\n");
	fprintf(stderr, "University of California, Los Angeles, Computer Science Department\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage(seed init):\n");
	fprintf(stderr, "codetorrent 0 <filename> <#blocks per generation> <block size> <gossip frequency> [verbose -v]\n");
	fprintf(stderr, "Usage(non-seed init):\n");
	fprintf(stderr, "codetorrent 1 <gossip frequency> [verbose -v]\n");
	
}

int main (int argc, char **argv)
{
	void *voidptr = NULL;
	int res;
	//srand((unsigned int)GetTickCount());
/*
#ifdef WIN32
	int tv_beg, tv_end, tv_last, tv_lastgossip, tv_curr;
	void *tz;
#else
	struct timeval tv_beg, tv_end, tv_lastgossip, tv_curr;
	struct timezone tz;
#endif
*/
#ifdef WIN32
	// windows socket initialization
	WSAData wsaData;

	if(WSAStartup(MAKEWORD(1,1), &wsaData)!=0) {
		fprintf(stderr, "WSAStartup failed.\n");
		exit(1);
	}
#endif	

	if( argc < 3 ) {
		printUsage();
		// Just to pause the application before terminating...
#ifdef WIN32
		std::cout << "Press <enter>...";
#else
		printf(" Press <enter>...");
#endif
		getchar();
		exit(1);
	}

	if ( atoi(argv[1]) == 0 ) { // seed
		nodeId = atoi(argv[1]);

		if( argc < 6 ) {
			printUsage();
			// Just to pause the application before terminating...
#ifdef WIN32
		std::cout << "Press <enter>...";
#else
		printf(" Press <enter>...");
#endif
			getchar();
			exit(1);
		}

		server_mode = true;

		gossip_freq = atoi(argv[5]);

		if( argc == 7 && !strcmp(argv[6],"-v"))
			verbose = true;

	}
	else {

		server_mode = false; // non-seed

		gossip_freq = atoi(argv[2]);

		if( argc == 4 && !strcmp(argv[3], "-v")) 
			verbose = true;
	}


	pthread_mutex_init(&send_mutex, NULL);

	////////////////////////////////////////////////////////////////////////////
	
	// We have two traffics : one for regular CT messages, and one for gossiping messages
	//                        CT messages (TCP) come through S_Port
	//                        gossiping messages (UDP) come through L_Port

	int sockfd, ct_node_fd;	// receive on sock_fd, new connection on new_fd, listen(for gossips) on listenfd
    struct sockaddr_in my_addr;			// my address information (S_Port)
//	struct sockaddr_in my_addr_listen;	// my address information for listener (L_Port)
    struct sockaddr_in ct_node_addr;	// connector's address information
    socklen_t sin_size;
	int yes = 1;

	/////// SOCKETS
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        printf("[main] socket creation error\n");
    }

	/////// SOCKET OPTIONS
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int)) == -1) {
        printf("[main] setsockopt\n");
    }

	/////// BIND
    my_addr.sin_family = AF_INET;					// host byte order
    my_addr.sin_port = htons(S_Port);				// short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY;			// automatically fill with my IP
    memset(&(my_addr.sin_zero), '\0', 8);			// zero the rest of the struct

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        printf("[main] bind\n");
    }

	/////// START LISTENING
    if (listen(sockfd, BACKLOG) == -1) {
        printf("[main] listen\n");
    }

	// Initiate CodeTorrent module, if you have to
	if ( server_mode ) {
		ct = new CodeTorrent(8, atoi(argv[3]), atoi(argv[4]), argv[2], false);
		gossip_freq = atoi(argv[5]);
	}
	else
		printf("[main] CodeTorrent client started...\n");

	/////// Send Gossips
	//if ( server_mode ) {

		// Gossiping thread
		pthread_t gossip_send_thread;						// thread

		res = pthread_create(&gossip_send_thread, NULL, send_gossip, (void *)voidptr);
		if (res != 0)	{
			
			printf("[main] send_gossip() Thread Creation failed");
			return 0;
		}
	//}

	/////// Listen for Gossips
	// server doesn't have to listen for gossips (at least for one file)
	//if( !server_mode ) {

		pthread_t gossip_listen_thread;

		// listen gossiped messages!
		res = pthread_create(&gossip_listen_thread, NULL, listen_gossip, (void *)voidptr);

		if (res != 0)	{
			printf("[main] listen_gossip() Thread Creation failed");
			return 0;
		}
	//}

	/////// Request Thread (for now, just the client)
	if( !server_mode ) {

		pthread_t request_thread;

		res = pthread_create(&request_thread, NULL, periodic_request, (void *)voidptr);

		if (res != 0) {
			printf("[main] periodic_request() thread creation failed");
			return 0;
		}
	}


	struct timeval block_download_beg, block_download_end;

	pthread_t incoming_thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	 
	/////// Main Thread
	while(1)
	{
		char* buf = new char[MAX_PACKET_SIZE];			// MAX_PACKET_SIZE ??

		sin_size = sizeof(struct sockaddr_in);

        if ((ct_node_fd = accept(sockfd, (struct sockaddr *)&ct_node_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }

		int bytes_rec = 0;
		int bytes_left = MAX_PACKET_SIZE;
		bool full_packet = false;
		char* rec_buf = buf;
		CT_header *header = (CT_header *)buf;


		gettimeofday( &block_download_beg, &tz);

		while( !full_packet ) {

			res = recv(ct_node_fd, (char *)(rec_buf+bytes_rec), bytes_left, 0);
#ifdef WIN32
			header->address = ct_node_addr.sin_addr.S_un.S_addr;
#else
			header->address = ct_node_addr.sin_addr.s_addr;
#endif

			if( verbose )
				printf("[main] %d bytes received from %s.\n", res, inet_ntoa(ct_node_addr.sin_addr));

			if(res < 0) {
				printf("[main] recv error!\n");
				break;
			}

			bytes_rec += res;

			if(bytes_rec == header->size) {

				bytes_left = MAX_PACKET_SIZE;		// reset
				full_packet = true;

			} else {

				bytes_left = header->size - bytes_rec;		// just get the remainder of this packet

				if( verbose ) 
					printf("[main] %d more bytes needed to form one packet\n", bytes_left);
			}			
		}

		gettimeofday( &block_download_end, &tz);
		printf("[main] Download Time/ block: %ld.%03ld\n", GetTimeDifference(block_download_beg, block_download_end)/1000, GetTimeDifference(block_download_beg, block_download_end)%1000);

		//pthread_t incoming_thread;						// thread

		if(bytes_rec == header->size) {

			res = pthread_create(&incoming_thread, &attr, incoming_packet, (void *)buf);

			if (res != 0)	{

				printf("Incoming_packet() Thread Creation failed |%d|",res);
				return 0;
			}
		}

		close(ct_node_fd); 
	}
	
#ifdef WIN32
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}

//detecting AP
bool ap_detection()
{
	return false;
}


