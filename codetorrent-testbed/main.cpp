/*
 * main.cpp
 *
 * Main CodeTorrent routines
 *
 * Coded by Uichin Lee
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

#ifdef WIN32
#include <winsock2.h>	// socket library for win32
#include "ws2tcpip.h"	// tcp/up library
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#endif

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

//typedef unsigned int  SOCKET;

#ifndef WIN32
	typedef int  SOCKET;
	typedef struct sockaddr SOCKADDR;
#endif

bool server_mode = false;
bool decoding = false;
bool verbose = false;

#ifdef WIN32
int gossip_freq = 2000;		// frequency of gossips in ms
#else
int gossip_freq = 2;
#endif

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
	int seq;
} CT_header;

// file info struct
typedef struct _CT_FileInfo {
	int filesize;						// file size
	int blocksize;						// block size
	unsigned long num_blocks_per_gen;	// number
	char filename[MAX_FILE_NAME_LEN];	// file name
	
} CT_FileInfo;

struct request_timestamp{
	struct timeval time;
	int seq;
};

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
	struct request_timestamp re_timestamp;
};


#ifdef WIN32
	int vanet_beg;
	int tv_beg, tv_end, tv_last, tv_lastgossip, tv_curr;
	int ct_beg, ct_end;
	int current;
	int request_beg;
	void *tz;
#else
	struct timeval vanet_beg;
	struct timeval ct_beg, ct_end;
	struct timeval tv_beg, tv_end, tv_lastgossip, tv_curr;
	struct timeval current;
	struct timeval request_beg;
	struct timezone tz;
#endif

#ifdef WIN32
long GetTimeDifference(struct timeval tv_beg, struct timeval tv_end);
long GetTimeDifference(int beg, int end);
void gettimeofday(int *time, void *tz);
#endif


// 5/6: uclee
struct con_str {
	int fd;	// file descriptor
	long int addr; // sender's address
};

// public variables
CodeTorrent *ct;						// main CodeTorrent module
std::vector<CT_neighbor> ct_neighbors;	// neighbors list

//added
//std::vector<CodedBlock*> *ct_coded_blocks;

std::vector<CodedBlock*> ct_coded_blocks;

//std::vector<struct timeval> send_request_time[10];

std::vector<struct request_timestamp> send_request_time[10];
int request_seq=0;


int helpful_neighbors=0;
int unhelpful_neighbors=0;

pthread_mutex_t send_mutex;
pthread_mutex_t codedBlocks_mutex;
int nodeId;
bool ct_ap= false;
///////////////////////////////////////////////////////////////////////
void* incoming_packet(void *buf);
//void reEncode(CT_header* ct_header);
//void send_codedBlock(CodedBlock* cb, unsigned long address);
void send_codedBlock(CodedBlock* cb, unsigned long address, int seq);
void send_fileInfo_request(unsigned long address);
void* send_gossip(void *voidptr);
void* send_request(void *voidptr);
//void send_request();
void send_request(CT_header* ct_header);
void store_block(CT_header* ct_header, char* data);
void update_neighbors(CT_header* ct_header);
void send_fileInfo(CT_header* ct_header);
char *pack(int type, int gen, unsigned long address, int fileId, CodedBlockPtr cb);
void* receive_gossip(void *in_buf);
long GetTimeDifference(struct timeval tv_beg, struct timeval tv_end);
bool ap_detection();
void codedBlock_pthreads(int num_gens);
void* send_block(void* voidptr);
void send_gossip();
void* listen_data(void* voidptr);

//added
void* generateCodedBlock(void *voidptr);
CodedBlockPtr popCodedBlock(int gen);
void pushCodedBlock(CodedBlockPtr cb, int gen);

CodedBlockPtr reEncode(CT_header* ct_header);
void check_neighbors();
void ct_closeSocket(int sockfd);

long encoding_time=0;
long decoding_time=0;
long block_download_time=0;
long total_download_time=0;

int num_encode=0;
int num_blocks=0;

int helpful_requests=0;
int unhelpful_requests=0;

bool unhelpful=false;
int  unhelpful_counter=0;

void print_summary();

struct sockaddr_in ct_node_addr;	// connector's address information
int ct_node_fd;

// uclee: 4/22/07
bool busy = false;
time_t last_req_time;


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

		if(verbose)
		printf("[store_block] Trying to store block in gen %d...\n", ct_header->gen);

		if (ct->StoreBlock(&temp_cb) == false){
			unhelpful_counter++;
			unhelpful_requests++;
		}
		else{
			helpful_requests++;

			fprintf(stderr, "# of DownloadBlocks: %d\n", ct->GetRankVec()[0]); //print out!!
		
			if(!server_mode) {
				ct_coded_blocks.clear();
				//busy = true;
			}
		}

		if(unhelpful_counter >= 1){
			unhelpful_counter =0;
#ifdef WIN32
			Sleep(10000);
#else
			sleep(10);
#endif
			return;
		}


		printf("::Currently received::\n| ");



		for(int i=0; i < ct->GetNumGens() ; i++) {
			printf("%2d | ", ct->GetRankVec()[i]);
		}
		
		printf("\n");

		gettimeofday( &current, &tz);

		printf("[CD] %d %ld.%03ld #ofDownloadBlocks %d\n", //CD NodeId CurrentTime Description Value
			nodeId,
			GetTimeDifference(vanet_beg, current)/1000, GetTimeDifference(vanet_beg, current)%1000,
			ct->GetRankVec()[0]);

		//update_neighbors(ct_header);

		send_request(ct_header); // will be implemented
	}
	else { // request file_info	
		//send_fileInfo_request(ct_header->address);
	}
}

/*
//reEncode(temp_header);
//void* reEncode(void *in_packet)
void reEncode(CT_header* ct_header)
{
	CodedBlockPtr temp_codedBlock;

#ifdef WIN32
	int encoding_beg, encoding_end;
#else
	struct timeval encoding_beg, encoding_end;
#endif

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
		
		num_encode++;
	}


	encoding_time = GetTimeDifference(encoding_beg, encoding_end);

	printf("[ReEncode] Encoding Time: %ld.%03ld\n", GetTimeDifference(encoding_beg, encoding_end)/1000, GetTimeDifference(encoding_beg, encoding_end)%1000);

	send_codedBlock(temp_codedBlock, ct_header->address);
	//ct->FreeCodedBlock(temp_codedBlock);
	//pthread_exit("reEncode done\n");

}
*/


CodedBlockPtr reEncode(CT_header* ct_header)
{
	CodedBlockPtr temp_codedBlock;

#ifdef WIN32
	int encoding_beg, encoding_end;
#else
	struct timeval encoding_beg, encoding_end;
#endif

	//gettimeofday( &(*it).timestamp, &tz);

	gettimeofday( &encoding_beg, &tz);
	if( !server_mode ) {
		temp_codedBlock = ct->ReEncode(ct_header->gen);

		if(verbose)
			printf("[reEncode] Re-encode a packet from gen %d...\n", ct_header->gen);
	}
	else {
		temp_codedBlock = ct->Encode(ct_header->gen);

		if(verbose)
			printf("[reEncode] Encode a packet from gen %d...\n", ct_header->gen);
		num_encode++;
	}
	gettimeofday( &encoding_end, &tz);

	//encoding_time = GetTimeDifference(encoding_beg, encoding_end);

	gettimeofday( &current, &tz);

	printf("[reEncode] Encoding Time: %ld.%03ld\n", GetTimeDifference(encoding_beg, encoding_end)/1000, GetTimeDifference(encoding_beg, encoding_end)%1000);
	printf("[CD] %d %ld.%03ld EncodingTime %ld.%03ld\n", //CD NodeId CurrentTime Description Value
				nodeId,
				GetTimeDifference(vanet_beg, current)/1000, GetTimeDifference(vanet_beg, current)%1000,
				GetTimeDifference(encoding_beg, encoding_end)/1000, GetTimeDifference(encoding_beg, encoding_end)%1000);

	return temp_codedBlock;
	//send_codedBlock(temp_codedBlock, ct_header->address);
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
#ifdef WIN32
	int decoding_beg, decoding_end;
	int serve_beg, serve_end;
#else
	struct timeval decoding_beg, decoding_end;
	struct timeval serve_beg, serve_end;
#endif

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

						num_blocks = temp_fileInfo->num_blocks_per_gen;

						codedBlock_pthreads( temp_header->gen);


						//0507: gossip thread
						pthread_t gossip_send_thread;						// thread
						pthread_attr_t attr;
						pthread_attr_init(&attr);
						pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
						int res;
						//void* voidptr;
						res = pthread_create(&gossip_send_thread, &attr, send_gossip, NULL); //(void *)voidptr);

						//send_request();
					}
				}
				break;
			}
		case CT_DATA:	
			{	
				if( verbose )
					printf("data.\n");
				if( ct != NULL )
				if( !server_mode) {
					
					//char* data = new char[]; //
					//char *data = (char *)(buf + sizeof(temp_header));
					char *data = (char *)(buf + sizeof(CT_header));

					//added
					update_neighbors(temp_header);

					store_block(temp_header, data);


					//shlee: 5/7/07
					for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {

						if ((*it).address == temp_header->address) {
							//gettimeofday( &(*it).re_timestamp.time);
							(*it).re_timestamp.seq =0; // request sent ==> received!!
						}
					}


					if( ct->DownloadCompleted() ) {
						decoding = true;
						
						//server_mode= true;
						//ct_coded_blocks.clear();
						//codedBlock_pthreads( temp_header->gen );						

						gettimeofday( &ct_end, &tz);
						printf("[main] Total Download Time: %ld.%03ld\n", GetTimeDifference(ct_beg, ct_end)/1000, GetTimeDifference(ct_beg, ct_end)%1000);
						gettimeofday( &decoding_beg, &tz);
						total_download_time = GetTimeDifference(ct_beg, ct_end);

						gettimeofday( &current, &tz);
						printf("[CD] %d %ld.%03ld GoodPut %ld.%03ld\n", //CD NodeId CurrentTime Description Value
						nodeId,
						GetTimeDifference(vanet_beg, current)/1000, GetTimeDifference(vanet_beg, current)%1000,
						GetTimeDifference(ct_beg, ct_end)/1000, GetTimeDifference(ct_beg, ct_end)%1000);

						print_summary();

						//ct->Decode();
						gettimeofday( &decoding_end, &tz);
						printf("[main] Download Time/ block: %ld.%03ld\n", GetTimeDifference(decoding_beg, decoding_end)/1000, GetTimeDifference(decoding_beg, decoding_end)%1000);
						decoding_time = GetTimeDifference(decoding_beg, decoding_end);
	

						//server_mode = true;
						//codedBlock_pthreads( temp_header->gen );
						decoding = false;
					}
				}
				break;
			}
		case CT_REQUEST:
			{
			if ( ct != NULL && ct->GetRankVec()[0] ){
				if ( temp_header->gen < 0 )  // file_info is requested.
				{
					if( verbose )
						printf("file info request.\n");		
					//if( server_mode )		// NOTE:temporary
						send_fileInfo(temp_header);
				}
				else
				{						//else, coded block is requested.
					if( verbose )
						printf("data request.\n");

					// uclee: 4/22/07
					busy = false; // request received... and we gotta do some work.

					CodedBlockPtr cb;
					//reEncode(temp_header);
					gettimeofday( &serve_beg, &tz);
				if(server_mode){
					if( ct_coded_blocks.size() > 0)
						 cb = popCodedBlock( temp_header->gen);
					else
						 cb = reEncode( temp_header);
				}
				else
						cb = reEncode( temp_header );
					gettimeofday( &serve_end, &tz);

					if(verbose)
					printf("[incoming] Serve Time: %ld.%03ld Seq#: %d\n", 
									GetTimeDifference(serve_beg, serve_end)/1000, 
									GetTimeDifference(serve_beg, serve_end)%1000, 
									temp_header->seq);
					
					send_codedBlock(cb, temp_header->address, temp_header->seq);
				}
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

	/*
	int yes = 1;

	if (setsockopt(send_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) == -1) {
		printf("[send_fileInfo] setsockopt error");
	}
	*/

	int res;

	res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); //connect
	if(res != 0){
		printf("[send_fileInfo] connect failed\n");
		printf("ERROR: %s\n", strerror(errno));
		ct_closeSocket(send_sock);
		return;
	}


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
#ifdef WIN32
	closesocket(send_sock);
#else
	close(send_sock);
#endif
}

// packs a packet upon request by other functions
char *pack(int type, int gen, unsigned long address, int fileId, CodedBlockPtr cb) {

	// WARNING: *pkt is not freed in this function! Make sure you free it somewhere!
	char *pkt = NULL;
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
			header->nodeId = nodeId;

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
			header->nodeId = nodeId;

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
			header->nodeId = nodeId;
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

void send_codedBlock(CodedBlock* cb, unsigned long address, int seq)
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

	int res;
	
	// if this is requested data, then send it via TCP
	send_sock = socket(AF_INET, SOCK_STREAM, 0);
	if ( send_sock == -1 ){
		printf("[send_codedBlock] socket creation() failed\n");
		printf("ERROR: %s\n", strerror(errno));
		free(cb);
		return;
	}

	/*
	int yes = 1;
	if (setsockopt(send_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) == -1) {
		printf("[send_codedBlock] setsockopt\n");
	}
	*/

	// connect to the client
	res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); // connect
	if(res != 0) {
		printf("[send_codedBlock] connect() failed\n");
		printf("ERROR: %s\n", strerror(errno));
		free(cb);
		ct_closeSocket(send_sock);
		return;
	}
	
	// form a packet
	char * temp = pack(CT_DATA, cb->gen, address, 0, cb); // tyep = data, gen, address
	CT_header *header = (CT_header *)temp;

	header->seq = seq;

	// send this out
	pthread_mutex_lock(&send_mutex);
	//res = send(send_sock, (char *)temp, sizeof(temp), 0);   // send the data packet
	res = send(send_sock, (char *)temp, header->size, 0);
	pthread_mutex_unlock(&send_mutex);

	if( verbose ){
		printf("[send_codedBlock] Sent %d/%d bytes from gen %d to %s\n", res, header->size, header->gen, inet_ntoa(add.sin_addr));
		fprintf(stderr, "[send_codedBlock] Sent %d bytes, Seq#: %d\n", res, seq);		
	}

	if(res != header->size){
		printf("send() failed\n");
		res=0;
	}
	
	free(temp);

	if (!res)
		printf("[send_codedBlock] send_codedBlock() failed\n");

	ct_closeSocket(send_sock);
	return;
}

void* send_gossip(void *voidptr)
{
	SOCKET send_sock;
	
	// set destination address
	struct sockaddr_in add;

	memset(&add, 0, sizeof(add));
	add.sin_family = AF_INET;

#ifdef WIN32
	if ( ct_ap == true)
		add.sin_addr.S_un.S_addr = inet_addr("192.168.0.255");
	else
		add.sin_addr.S_un.S_addr = inet_addr("255.255.255.255");
#else
	if ( ct_ap == true)
		add.sin_addr.s_addr = inet_addr("192.168.120.255");
	else
		add.sin_addr.s_addr = inet_addr("192.168.0.255");
#endif
	add.sin_port = htons(L_Port);

	int yes = 1, res = 0, num_gens;
	static int gen = 0;
	
	// if this is gossiping, then send it via UDP

	while(1){

		if( ct != NULL ) {

			send_sock = socket(AF_INET, SOCK_DGRAM, 0);

			if (setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(int)) == -1) {
				printf("[send_gossip] setsockopt error\n");
				printf("ERROR: %s\n", strerror(errno));
				ct_closeSocket(send_sock);
				//return;
				continue;
			}
	
			num_gens = ct->GetNumGens();
			gen =0;

			while(1){
			
				if( verbose )
					printf("[send_gossip] Gossiping generation %d ...\n", (gen)%num_gens);

				unsigned long address = inet_addr("255.255.255.255"); //broadcasting

				// form a packet
				char * temp = pack(CT_GOSSIP, gen, address, 0, NULL); // tyep = data, gen, address
				CT_header *header = (CT_header *)temp;

				// send this out


				//res = sendto(send_sock, (char *)temp, sizeof(temp), 0, (SOCKADDR *)&add, sizeof(struct sockaddr));
				//pthread_mutex_lock(&send_mutex);
				
				res = sendto(send_sock, (char *)temp, header->size, 0, (SOCKADDR *)&add, sizeof(struct sockaddr));
				//pthread_mutex_unlock(&send_mutex);
			

				if( verbose )
					printf("[send_gossip] Sent %d/%d bytes to %s\n", res, header->size, inet_ntoa(add.sin_addr));
				
				if( verbose )
					fprintf(stderr, "[send_gossip]gossip_sent!\n ");
					
				if(res != header->size){
					printf("[send_gossip] sendto() failed\n");
					fprintf(stderr, "[send_gossip] size fail\n");
				}

				free(temp);	

#ifdef WIN32
				Sleep(5000);
#else
				usleep(gossip_freq * 100000); //gossip freq.
#endif

			}//while(1)

			if( verbose )
				fprintf(stderr, "[send_gossip] gossip_socket terminated\n");
#ifdef WIN32
	closesocket(send_sock);
#else
	close(send_sock);
#endif

		}//ct

#ifdef WIN32
	Sleep(5000);
#else
	usleep(gossip_freq * 200000);         // wait ct init..
#endif


		
	} //while(1)

	if( verbose)
		fprintf(stderr, "[send_gossip] thread terminated\n");
	pthread_exit(NULL);
	return NULL;
}

void send_gossip()
{
	SOCKET send_sock;
	
	// set destination address
	struct sockaddr_in add;

	memset(&add, 0, sizeof(add));
	add.sin_family = AF_INET;

#ifdef WIN32
	if ( ct_ap == true)
		add.sin_addr.S_un.S_addr = inet_addr("192.168.0.255");
	else
		add.sin_addr.S_un.S_addr = inet_addr("255.255.255.255");
#else
	if ( ct_ap == true)
		add.sin_addr.s_addr = inet_addr("192.168.120.255");
	else
		add.sin_addr.s_addr = inet_addr("192.168.0.255");
#endif
	add.sin_port = htons(L_Port);

	int yes = 1, res = 0, num_gens;
	static int gen = 0;
	
	// if this is gossiping, then send it via UDP
	send_sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(int)) == -1) {
		printf("[send_gossip] setsockopt error\n");
		printf("ERROR: %s\n", strerror(errno));
		ct_closeSocket(send_sock);
		return;
	}


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

#ifdef WIN32
	closesocket(send_sock);
#else
	close(send_sock);
#endif

	if(verbose)
		printf("[send_gossip] Current # of CodedBlocks in buffer: %d\n", ct_coded_blocks.size());
}

void* listen_gossip(void* voidptr) {

	SOCKET listenfd;	// receive on sock_fd, new connection on new_fd, listen(for gossips) on listenfd
	struct sockaddr_in my_addr_listen;	// my address information for listener (L_Port)
	struct sockaddr_in sender_addr;
    	socklen_t sin_size;
	//int yes = 1;
	int res;

	while(1){
		/////// SOCKET
		if ((listenfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
			printf("socket (listen)\n");
			printf("ERROR: %s\n", strerror(errno));
		}

		/*
		/////// SOCKET OPTIONS
   		if (setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int)) == -1) {
        		printf("setsockopt (listen)\n");
    		}
		*/

		/////// BIND
   		char hostname[255];
		gethostname(hostname, 255);
		hostent *he;
		if ((he = gethostbyname(hostname)) == 0) {
			printf("[listen_gossip] gethostbyname error!\\n");
			printf("ERROR: %s\n", strerror(errno));
			continue;
			//pthread_exit("listen_gossip unexpectedly exits");
			//pthread_exit(NULL);
		}

		my_addr_listen.sin_family = AF_INET;			// host byte order
		my_addr_listen.sin_port = htons(L_Port);		// short, network byte order
		my_addr_listen.sin_addr.s_addr = htonl(INADDR_ANY);// automatically fill with my IP
		memset(&(my_addr_listen.sin_zero), '\0', 8);	// zero the rest of the struct

		if (bind(listenfd, (struct sockaddr *)&my_addr_listen, sizeof(struct sockaddr)) == -1) {
			printf("[listen_gossip] bind (listen)\n");
			printf("ERROR: %s\n", strerror(errno));
			continue;
		}

		// assumes buf will not be used too long; not until the next gossip
		sin_size = sizeof(struct sockaddr);

		pthread_t incoming_thread;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


		while(1) {

			char *buf = new char[MAX_PACKET_SIZE];
			buf[0] = '\0';
		
			res = recvfrom(listenfd, buf, MAX_PACKET_SIZE, 0, (sockaddr *)&sender_addr, &sin_size);
			if(res < sizeof(CT_header)){
				printf("[listen_gossip] recvfrom (listen)\n");
				printf("ERROR: %s\n", strerror(errno));
				break;
			}

	#ifdef WIN32
			//if( my_addr_listen.sin_addr.S_un.S_addr == sender_addr.sin_addr.S_un.S_addr ) {
	#else
			//if( my_addr_listen.sin_addr.s_addr == sender_addr.sin_addr.s_addr ) {
	#endif
			

			if(0){//if( verbose )
				if(verbose)
					printf("[listen_gossip] This is gossip from myself, drop it.\n");

			} 
			else {

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

					res = pthread_create(&incoming_thread, &attr, receive_gossip, (void *)buf);

					if (res != 0)	{
						printf("receive_gossip() Thread Creation failed");
						break;
					}
				}
				else{
					printf("[listen_gosip] Gossip from myself\n");
				}
			}
		}

		ct_closeSocket(listenfd);
	}// while(1)

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

int release[10] = {0,0,0,0,0,0,0,0,0,0};

//void update_neighbors(CT_packet *in_packet)
void update_neighbors(CT_header* ct_header)
{
	//bool found = false;
	int i;
	int count=0;

	if( verbose )
		printf("[update_neighbors] Updating neighbor list with gen %d\n", ct_header->gen);


	gettimeofday( &current, &tz);


	// find and update the right neighbor
	for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {
		
		if ((*it).address == ct_header->address) {
		
			(*it).gen_vector[ct_header->gen] = true;

			//added
			
			release[count]++;
			if( release[count] > 100000){
				 (*it).helpful = false;
				 release[count] =0;
			}
			

			//shlee: 5/4/07
			if ( GetTimeDifference( (*it).re_timestamp.time, current) > 5000000 && (*it).re_timestamp.seq == 1 )
			(*it).helpful = false;


			//fixed
			if ( (*it).helpful == false) 
				send_request(ct_header);

			(*it).helpful = true;					//helpful!		  
			gettimeofday( &(*it).timestamp, &tz);   //updateing timestamp

			if(verbose)
				printf("[update_neighbors] timestamp is updated!!!\n");
			check_neighbors();

			//
			//send_request(ct_header);

			return;
		}
		count++;
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
	check_neighbors();

	//
	send_request(ct_header);

}


void send_request(CT_header* ct_header)
{

	if( ct != NULL && !server_mode){

		if( ct->GetNumGens() != ct->GetGenCompleted()){
				if(verbose)
					printf("[send_request] Send Request Called\n");
	
				int i=0;
				int request_gen =0;
				unsigned long temp_address = ct_header->address;
				struct sockaddr_in add_m;

				//socket, 
				struct sockaddr_in add;
				memset(&add, 0, sizeof(add));
				add.sin_family = AF_INET;


				#ifdef WIN32
					add.sin_addr.S_un.S_addr = temp_address;
				#else
					add.sin_addr.s_addr = temp_address;
				#endif

					add.sin_port = htons(S_Port);// listening port

					int res;//, yes = 1;

					SOCKET send_sock = socket(AF_INET, SOCK_STREAM, 0);
					
					/////// SOCKET OPTIONS
					/*
					if (setsockopt(send_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int)) == -1) {
						printf("[send_request] setsockopt\n");
					}
					*/
					if(verbose) {
						//printf("[send_request] Trying to connect to %s to request data (nodeId: %d).\n", inet_ntoa(add.sin_addr), ct_header->nodeId);
						printf("[send_request] Trying to connect to %ld to request data (nodeId: %d).\n", add.sin_addr.s_addr, ct_header->nodeId);
						printf("[send_request] Trying to connect to %ld to request data (nodeId: %d).\n", temp_address, ct_header->nodeId);
				}

					res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); //connect
					if(res != 0){
						printf("[send_request] connect failed\n");
						printf("ERROR: %s\n", strerror(errno));
						ct_closeSocket(send_sock);
						//delete[] has;
						pthread_exit(NULL);
						return;
					}
					//else{
					//call pack()
			
					char * temp = pack(CT_REQUEST, request_gen, temp_address, 0, NULL); // tyep = request, gen, address

					CT_header *header = (CT_header *)temp;
				
					gettimeofday(&request_beg, &tz);

					struct request_timestamp tempTimestamp;
					struct timeval temp_request = request_beg;

					tempTimestamp.time = request_beg;
					tempTimestamp.seq = request_seq;

					//printf("[send_request] nodeId:%d\n", ct_header->nodeId);
					send_request_time[ct_header->nodeId].push_back(tempTimestamp);
					//send_request_time.push_back(tempTimestamp);			

					header->seq = request_seq;

					request_seq++;

					//res = send(send_sock, (char *)&temp, sizeof(temp), 0);	// will this work?
					pthread_mutex_lock(&send_mutex);
					res = send(send_sock, (char *)temp, header->size, 0);	// will this work?
					pthread_mutex_unlock(&send_mutex);


					// shlee : 5/4/07
					if(verbose){
						printf("[send_request]seq: %d\n", request_seq);
						fprintf(stderr,"[send_request]seq: %d\n", request_seq);
					}
					for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {

						if ((*it).address == ct_header->address) {
							gettimeofday( &(*it).re_timestamp.time, &tz);
							(*it).re_timestamp.seq =1; // request sent
						}
					}

					// uclee: 4/22/07
					busy = true;	// sent the req == I'll be BUSY!
					last_req_time = time(NULL); // record the current time

					if ( res != header->size )
						printf("send_request Fail");
					free(temp);
				
					ct_closeSocket(send_sock);
					return;
			
			//}
		}//if( ct->GetNumGens() != ct->GetGenCompleted()){
	}//if( ct != NULL && !server_mode){
}

void* send_request(void* voidptr)
{

	char* buf = (char*) voidptr;
	CT_header *temp_header = (CT_header*) buf;


	if( ct->GetNumGens() != ct->GetGenCompleted()){
		if(verbose)
			printf("[send_request] Send Request Called\n");
	}
	
}

void* periodic_request(void* voidptr) {

	while(1){
#ifdef WIN32
	Sleep(5000);
#else
	usleep(gossip_freq * 100000);
#endif

	if(!server_mode)
		check_neighbors();

	//send_gossip();
	if(verbose)
		printf("[send_gossip] Current # of CodedBlocks in buffer: %d\n", ct_coded_blocks.size());

	}
	pthread_exit(NULL);
	return NULL;

/*
	*/
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

	printf("[send_fileInfo_request] addr %s", inet_ntoa(add.sin_addr));

	res = connect(send_sock, (SOCKADDR *)&add, sizeof(add)); //connect
	if(res != 0){
		printf("[send_fileInfo_request] connect failed\n");
		printf("ERROR: %s\n", strerror(errno));
		ct_closeSocket(send_sock);
		return;
	}

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

#ifdef WIN32
	closesocket(send_sock);
#else
	close(send_sock);
#endif
	return;

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

void sigint(int sig)
{
	fprintf(stderr, "goodbye~\n");
	exit(1);
}


int main(int argc, char **argv)
{
	void *voidptr = NULL;
	int res;
#ifdef WIN32
	srand((unsigned int)GetTickCount());
#else
	srand(time(NULL));
#endif
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
		printf(" Press <enter>...");
		getchar();
		exit(1);
	}

	nodeId = atoi(argv[1]);		//nodeId

	if ( atoi(argv[2]) == 0 ) { // seed

		if( argc < 6 ) {
			printUsage();
			// Just to pause the application before terminating...
			printf(" Press <enter>...");
			getchar();
			exit(1);
		}

		server_mode = true;
		ct_ap = true;

		gossip_freq = atoi(argv[6]);

		if( argc == 8 && !strcmp(argv[7],"-v"))
			verbose = true;

	}
	else {

		server_mode = false; // non-seed

		gossip_freq = atoi(argv[3]);

		if( argc == 5 && !strcmp(argv[4], "-v")) 
			verbose = true;
	}

	// CTRL+C 
#ifndef WIN32
	(void) signal(SIGINT, sigint);
#endif


	pthread_mutex_init(&send_mutex, NULL);
	pthread_mutex_init(&codedBlocks_mutex, NULL);


	gettimeofday( &vanet_beg, &tz);

	/////// Send Gossips
	if ( server_mode ) {

		// Gossiping thread
		
		pthread_t gossip_send_thread;						// thread
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


		res = pthread_create(&gossip_send_thread, &attr, send_gossip, (void *)voidptr);
		if (res != 0)	{
			
			printf("[main] send_gossip() Thread Creation failed");
			return 0;
		}
		
	}

	/////// Listen for Gossips
	// server doesn't have to listen for gossips (at least for one file)
	//if( !server_mode ) {

		pthread_t gossip_listen_thread;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


		// listen gossiped messages!
		res = pthread_create(&gossip_listen_thread, &attr, listen_gossip, (void *)voidptr);

		if (res != 0)	{
			printf("[main] listen_gossip() Thread Creation failed");
			return 0;
		}
	//}

	/////// Request Thread (for now, just the client)
	//if( !server_mode ) {

		pthread_t request_thread;
		//pthread_attr_t attr;
		//pthread_attr_init(&attr);
		//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


		res = pthread_create(&request_thread, &attr, periodic_request, (void *)voidptr);

		if (res != 0) {
			printf("[main] periodic_request() thread creation failed");
			return 0;
		}
	//}



	//pthread_attr_t attr;
	//pthread_attr_init(&attr);
	//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	////////////////////////////////////////////////////////////////////////////
	
	// We have two traffics : one for regular CT messages, and one for gossiping messages
	//                        CT messages (TCP) come through S_Port
	//                        gossiping messages (UDP) come through L_Port

	int sockfd, ct_node_fd;	// receive on sock_fd, new connection on new_fd, listen(for gossips) on listenfd
    struct sockaddr_in my_addr;			// my address information (S_Port)
//	struct sockaddr_in my_addr_listen;	// my address information for listener (L_Port)
//	struct sockaddr_in ct_node_addr;	// connector's address information
    socklen_t sin_size;
	int yes = 1;


	while(1){

		/////// SOCKETS
		if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
			printf("[main] socket creation error\n");
			printf("ERROR: %s\n", strerror(errno));
		}

		/////// SOCKET OPTIONS
		if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int)) == -1) {
			printf("[main] setsockopt\n");
			ct_closeSocket(sockfd);
			continue;
		}
		

		/////// BIND
		my_addr.sin_family = AF_INET;					// host byte order
		my_addr.sin_port = htons(S_Port);				// short, network byte order
		my_addr.sin_addr.s_addr = INADDR_ANY;			// automatically fill with my IP
		memset(&(my_addr.sin_zero), '\0', 8);			// zero the rest of the struct

		if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
			printf("[main] bind\n");
			printf("ERROR: %s\n", strerror(errno));
			ct_closeSocket(sockfd);
			continue; //re_create socket
		}

		/////// START LISTENING
		if (listen(sockfd, BACKLOG) == -1) {
			printf("[main] listen\n");
			printf("ERROR: %s\n", strerror(errno));
			ct_closeSocket(sockfd);
			continue;
		}

		// Initiate CodeTorrent module, if you have to
		if ( server_mode ) {
			ct = new CodeTorrent(8, atoi(argv[4]), atoi(argv[5]), argv[3], false);
			codedBlock_pthreads( ct->GetNumGens());

			//ct_coded_blocks = new std::vector<CodedBlock*>[ ct->GetNumGens() ];
			gossip_freq = atoi(argv[6]);
		}
		else
			printf("[main] CodeTorrent client started...\n");


		pthread_t listen_data_thread;
		/////// Main Thread
		while(1)
		{

			sin_size = sizeof(struct sockaddr_in);

			//ct_node_fd = new SOCKET();

			if ((ct_node_fd = accept(sockfd, (struct sockaddr *)&ct_node_addr, &sin_size)) == -1) {
				printf("[main] accept() thread creation failed");
				printf("ERROR: %s\n", strerror(errno));
				break;
			}
			
			//int *ct_listen_fd_ptr = new int(ct_node_fd);
			//res = pthread_create(&listen_data_thread, &attr, listen_data, (void *)ct_listen_fd_ptr);

			// 5/6: uclee
			//struct con_str* new_con_str= (struct con_str *)malloc(sizeof(struct con_str));
			struct con_str* new_con_str = new struct con_str; 
			memset(new_con_str, 0, sizeof(struct con_str));
			new_con_str->fd = ct_node_fd;
			new_con_str->addr = ct_node_addr.sin_addr.s_addr; 

			res = pthread_create(&listen_data_thread, &attr, listen_data, (void *)new_con_str);

			if (res != 0) {
				printf("[main] listen_data() thread creation failed\n");
				ct_closeSocket(ct_node_fd);
				break;
			}

		}

		ct_closeSocket(sockfd);
	}


#ifdef WIN32
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}

//detecting AP
bool ap_detection()
{
	char add;
	FILE* fp;

	char* filename = "ap.txt";
	if ( ! (fp = fopen(filename,"rb") )) {
		printf("Error: cannot open ap.txt\n");
		abort();
	}

	fread(&add, 1, 1, fp);

	printf("%c\n", add);

	fclose(fp);

	if ( add == 'T' ) return true;
	else return false;
}


void print_summary(){
	
	//long avg_encoding_time = encoding_time / num_encode;
	//long avg_block_download_time = block_download_time / num_blocks;
	
	//printf("[main] Download Time/ block: %ld.%03ld\n", GetTimeDifference(decoding_beg, decoding_end)/1000, GetTimeDifference(decoding_beg, decoding_end)%1000);
	printf("=============================================\n");
	printf("[CD] %d UsefulDownload %d\n",nodeId, helpful_requests);
	printf("[CD] %d UnUsefulDownload %d\n",nodeId, unhelpful_requests);
	printf("=============================================\n");
	fprintf(stderr, "[DONE]\n");
}


void codedBlock_pthreads(int num_gens){

	pthread_t generate_cb_thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	//ct_coded_blocks = new std::vector<CodedBlock*>[ num_gens ];

	CT_header* buf = new CT_header;
	int res;

	//for(int i=0; i < num_gens ; i++){
	int i=0;
		buf->gen = i;
		res = pthread_create(&generate_cb_thread, &attr, generateCodedBlock, (void *)buf);

		if (res != 0)
		printf("receive_gossip() Thread Creation failed");
	//}
}


//std::vector<CodedBlock*> ct_coded_blocks;
void* generateCodedBlock(void *voidptr){
//generating codedBlocks for one generation (gen# comes from voidptr)

	char* buf = (char*) voidptr;
	CT_header *temp_header = (CT_header*) buf;
	//int buffer_limit;

	//if(ct->GetIdentity() == CT_SERVER){
	if(server_mode){
		while(1){
			if(ct_coded_blocks.size() < 1000){
				CodedBlockPtr cb = reEncode(temp_header);
				ct_coded_blocks.push_back(cb);
				printf("[generateCodedBlock] # of EncodedBlocks %d\n", ct_coded_blocks.size());	
			//pushCodedBlock( reEncode(temp_header), temp_header->gen );
			}
			else
				popCodedBlock(temp_header->gen);
		}
	}
	else

		while(1){
				//printf("[generateCodedBlock] vector size: %d\n", ct_coded_blocks.size());
				//printf("[generateCodedBlock] my rank    : %d\n", ct->GetRankVec()[0]);
			
			//if( ct_coded_blocks[temp_header->gen].size() < ct->GetRankVec()[temp_header->gen] * 3)
			//if(!busy)


			// uclee: 4/22/07
			//  -- busy idling routine.
#define REQ_TIME_OUT 3 // 3 seconds
			while( busy ) {
#ifndef WIN32
				usleep(50*1000); // wake up every 50ms and check it's not busy or not.
#endif

				if( (int)(time(NULL) - last_req_time) > REQ_TIME_OUT ) {
					busy = false; // req timeout has happened.
				}
			}

			if( (int)ct_coded_blocks.size() < ct->GetRankVec()[0]){

				CodedBlockPtr cb = reEncode(temp_header);
				ct_coded_blocks.push_back(cb);
				if(verbose)
					printf("[generateCodedBlock] # of ReEncodedBlocks: %d\n", ct_coded_blocks.size());
				//pushCodedBlock( reEncode(temp_header), temp_header->gen );

			}
			else{
				if ((int)ct_coded_blocks.size() > 2)
					popCodedBlock(temp_header->gen);
			}
		}

	if(verbose)
		printf("[generating] ptherad terminated\n");
	pthread_exit(NULL);
	return NULL;
}

void pushCodedBlock(CodedBlockPtr cb, int gen){

	//pthread_mutex_lock(&send_mutex);
		ct_coded_blocks.push_back(cb);
		if(verbose)
			printf("[pushCodedBlock] # of EncodedBlocks %d\n", ct_coded_blocks.size() );
	//pthread_mutex_unlock(&send_mutex);
}


CodedBlockPtr popCodedBlock(int gen){

	
	//if (ct_coded_blocks.size() == 0)
	//pthread_mutex_lock(&codedBlocks_mutex);
		CodedBlockPtr tempCodedBlockPtr;
		tempCodedBlockPtr = ct_coded_blocks.front();
		ct_coded_blocks.erase( ct_coded_blocks.begin() );

		if(verbose)
			printf("[popCodedBlock] # of EncodedBlocks %d\n", ct_coded_blocks.size() );
	//pthread_mutex_unlock(&codedBlocks_mutex);

	return tempCodedBlockPtr;
}


void check_neighbors(){

	unhelpful_neighbors=0;
	helpful_neighbors=0;

#ifdef WIN32
	int current_time;
#else
	struct timeval current_time;
#endif

	gettimeofday( &current_time, &tz);
	
	for( std::vector<CT_neighbor>::iterator it = ct_neighbors.begin() ; it != ct_neighbors.end() ; ++it ) {

#ifdef WIN32	
		if ( GetTimeDifference( (*it).timestamp, current_time) > 3000 ){
#else
		if ( GetTimeDifference( (*it).timestamp, current_time) > 10000000 ){
#endif
			(*it).helpful = false; //timeout!, 5sec
			unhelpful_neighbors++;
		}
		else helpful_neighbors++;
	}

// commented by uclee: 4/22/07
/* 
	if(helpful_neighbors == 0){
		busy = false;
	}
	else
		busy = true;
*/
	if(verbose){
		printf("[check_neighbors] # of Helpful neighbors: %d\n", helpful_neighbors);
		printf("[check_neighbors] # of UnHelpful neighbors: %d\n", unhelpful_neighbors);
	}

	
}

void* send_block(void* voidptr){
/*	
	char* buf = (char *)voidptr;
	CT_header *temp_header = (CT_header*)buf;

	if(verbose)
		printf("[send_block] Sended Gen:%d\n", temp_header->gen);
	CodedBlockPtr cb;
	//reEncode(temp_header);
	temp_header->gen =0;
	if( ct_coded_blocks.size() > 0)
		 cb = popCodedBlock( temp_header->gen);
	else
		 cb = reEncode( temp_header);

	send_codedBlock(cb, temp_header->address);



*/
	pthread_exit(NULL);
	return NULL;
}



void* listen_data(void* voidptr){

#ifdef WIN32
{

}
#else
{

	struct con_str *cs = (struct con_str *) voidptr;
	//int *ct_node_fd = (int *)voidptr;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	char* buf = new char[MAX_PACKET_SIZE];			// MAX_PACKET_SIZE ??

#ifdef WIN32
	int block_download_beg, block_download_end;
#else
	struct timeval block_download_beg, block_download_end;
#endif

	pthread_t incoming_thread;

	//bool recv_hdr=false;
	int bytes_rec = 0;
	int bytes_left = MAX_PACKET_SIZE;
	//bool full_packet = false;
	char* rec_buf = buf;
	CT_header *header = (CT_header *)buf;
	int res;

	//int recv_id = header->nodeId;
	
	int result;
	fd_set readfds;
	struct timeval timeout;

	FD_ZERO(&readfds);
	FD_SET(cs->fd, &readfds);

	//printf("file descriptor: %d\n", *ct_node_fd);
	// 2.5s
	timeout.tv_sec = 2;
	timeout.tv_usec = 500000;

	result = select(FD_SETSIZE, &readfds, (fd_set *)NULL, (fd_set *)NULL,  &timeout);

	gettimeofday( &block_download_beg, &tz);

	switch(result) {
	case 0:
		if(verbose)
			printf("[listen_data] ERROR: connection timeout\n");
		busy = false; // if it times out...

		ct_closeSocket(cs->fd);
		delete cs;
		pthread_exit(NULL);
		return NULL;
		break;

	case -1:
		perror("select");
		//exit(1);
		
		ct_closeSocket(cs->fd);
		delete cs;
		pthread_exit(NULL);
		return NULL;	
		break;

	default:
		if(FD_ISSET(cs->fd, &readfds)) {
			bytes_rec = recv(cs->fd, (char *)rec_buf, bytes_left, MSG_WAITALL);

			//assert( bytes_rec == header->size );
 			if ( bytes_rec != header->size){
				ct_closeSocket(cs->fd);
				delete cs;
				pthread_exit(NULL);
				return NULL;
				break;
			}

			#ifdef WIN32
					header->address = cs->addr; //ct_node_addr.sin_addr.S_un.S_addr;
			#else
					header->address = cs->addr; //ct_node_addr.sin_addr.s_addr;
			#endif
			
			if( verbose ) {
				printf("[listen_data] %d bytes received from %s\n", bytes_rec, inet_ntoa(ct_node_addr.sin_addr));
			}

		}
		break;
	}

	gettimeofday( &block_download_end, &tz);

	if(verbose)
	printf("[listen_data] Download Time/ block: %ld.%03ld\n", GetTimeDifference(block_download_beg, block_download_end)/1000, GetTimeDifference(block_download_beg, block_download_end)%1000);

	//pthread_t incoming_thread;						// thread
	if (header->type == CT_DATA){
		gettimeofday( &current, &tz);
	
		bool find = false;
		int id = header->nodeId;
		printf("[CD] %d %ld.%03ld ReceivedDataFrom %d\n", //CD NodeId CurrentTime Description Value
				nodeId,
				GetTimeDifference(vanet_beg, current)/1000, GetTimeDifference(vanet_beg, current)%1000,
				id);

		printf("[CD] %d %ld.%03ld BlockDownloadTime %ld.%03ld\n", //CD NodeId CurrentTime Description Value
				nodeId,
				GetTimeDifference(vanet_beg, current)/1000, GetTimeDifference(vanet_beg, current)%1000,
				GetTimeDifference(block_download_beg, current)/1000, GetTimeDifference(block_download_beg, current)%1000);
	
	
		std::vector<struct request_timestamp>::iterator tempIt;
		if( send_request_time[id].size() > 0){

			//////////////////////////
			for( std::vector<struct request_timestamp>::iterator it = send_request_time[id].begin() ; it != send_request_time[id].end() ; ++it ) {
			
				if ((*it).seq == header->seq) {
					printf("[CD] %d %ld.%03ld BlockRequestTime %ld.%03ld\n", //CD NodeId CurrentTime Description Value
					nodeId,
					GetTimeDifference(vanet_beg, current)/1000, GetTimeDifference(vanet_beg, current)%1000,
					GetTimeDifference((*it).time, current)/1000, GetTimeDifference((*it).time, current)%1000);
					tempIt = it;
					find = true;
					printf("[listen_data] rev seq#:%d\n", header->seq);
				}
			}
				if (find)	
					send_request_time[id].erase( tempIt );
		}

		if(verbose)
			printf("[listen_data] size: %d\n", send_request_time[id].size());
		if (send_request_time[id].size() > 50)
			send_request_time[id].clear();
	}

	res = pthread_create(&incoming_thread, &attr, incoming_packet, (void *)buf);

	if (res != 0)	{

		printf("Incoming_packet() Thread Creation failed");
		delete cs;
		return 0;
	}

	ct_closeSocket(cs->fd);
	delete cs;
	pthread_exit(NULL);
	return NULL;
}
#endif

}

void ct_closeSocket(int sockfd){

#ifdef WIN32
		closesocket(sockfd);
#else
		close(sockfd); 
#endif

}
