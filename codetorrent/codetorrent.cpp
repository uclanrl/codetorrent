/*
 * codetorrent.cpp
 *
 * Main CodeTorrent module
 *
 * Coded by Joe Yeh/Uichin Lee
 * Modified and extended by Seunghoon Lee/Sung Chul Choi
 * University of California, Los Angeles
 *
 * Last modified: 11/21/06
 *
 */
#include <errno.h>
#include "codetorrent.h"
//#include "main.cpp"
//for time-stamp
#include <sys/timeb.h>
#include <sys/time.h>
//#include <time.h>

struct timeval ct_tv_beg, ct_tv_end;
struct timeval ct_encode_beg, ct_encode_end;
struct timeval ct_vector_beg, ct_vector_end;

//void* tz;
struct timezone ct_tz;

long CT_GetTimeDifference(struct timeval tv_beg, struct timeval tv_end) {
	return ((tv_end.tv_usec - tv_beg.tv_usec) + (tv_end.tv_sec - tv_beg.tv_sec) * 1000000);
}

/*
 void gettimeofday(struct timeval, void* time_zone){
 struct _timeb cur;
 _ftime(&cur);
 tv->tv_sec =cur.time;
 tv->tv_usec = cur.millitm * 1000;
 }
 */
CodeTorrent::~CodeTorrent() {

	CleanMGData();

	if (identity == CT_SERVER)
		CleanData();

	if (identity == CT_CLIENT) {

		CleanMHelpful();
		CleanBuffer();
	}

	if (identity == CT_CLIENT) {

		delete[] rank_vec;
		delete[] rank_vec_in_buf;
	}

	if (cb)
		FreeCodedBlock(cb);

	delete[] num_blocks_gen;
	delete nc;
}

void CodeTorrent::CleanData() {

	int i;

	if (identity == CT_SERVER) {

		for (i = 0; i < (int) data.size(); i++) {
			FreeBlock(data[i]);
		}
		data.clear();
	}
}

void CodeTorrent::CleanBuffer() {

	if (identity == CT_CLIENT) {
		buf.clear();
	}
}

void CodeTorrent::CleanMGData() {

	int i, j;

	// free m_gdata
	for (j = 0; j < num_blocks_gen[0]; ++j) {

		delete[] m_gdata[j];
	}

	delete[] m_gdata;
}

void CodeTorrent::CleanMHelpful() {

	int i, j;

	if (!m_helpful) {

		for (i = 0; i < num_gens; ++i)
			for (j = 0; j < (is_sim ? 1 : rank_vec[i]); ++j)
				delete[] (m_helpful[i][j]);

		for (i = 0; i < num_gens; ++i)
			delete[] (m_helpful[i]);

		delete[] m_helpful;
	}
}

void CodeTorrent::CleanTempFiles() {

	int gen;
	char *ext = ".temp";
	char *ext2 = new char[3]; // what if more than 100 gens?

	int fname_len = strlen(filename);
	int ext_len = strlen(ext);
	int ext2_len = strlen(ext2);

	char *filename_read = new char[fname_len + ext_len + ext2_len + 1];

	for (gen = 0; gen < num_gens; gen++) {
#ifdef WIN32
		itoa(gen, ext2, 10); // base : 10
#else
		sprintf(ext2, "%d", gen);
#endif	
		memcpy(filename_read, filename, fname_len);
		memcpy(filename_read + fname_len, ext, ext_len);
		memcpy(filename_read + fname_len + ext_len, ext2, ext2_len);

		filename_read[fname_len + ext_len + ext2_len] = '\0';

		remove(filename_read);
	}

	delete[] ext2;
	delete[] filename_read;
}

// Server initialization
void CodeTorrent::Init(int in_field_size, int in_num_blocks_per_gen, int in_block_size, char *in_filename,
		bool in_is_sim) {

	int j;

	// set internal parameters
	field_size = in_field_size;
	//num_gens = in_num_gens;				// computed in LoadFileInfo()
	block_size = in_block_size;
	strcpy(filename, in_filename);
	is_sim = in_is_sim;
	gen_completed = 0;

	// load the file info and determine num_blocks_gen
	LoadFileInfo(in_num_blocks_per_gen);

	//num_blocks_gen = new int[num_gens];	// moved to LoadFileInfo()

	// num_blocks_gen[0] must be the maximum of all num_blocks_gen[i]
	cb = AllocCodedBlock(num_blocks_gen[0], block_size);

	identity = CT_SERVER; // I'm a server!

	PrintFileInfo();

	// m_gdata : data for each generation
	// allocate memory to m_gdata
	m_gdata = new unsigned char*[num_blocks_gen[0]];

	for (j = 0; j < num_blocks_gen[0]; ++j) {

		m_gdata[j] = new unsigned char[(is_sim ? 1 : block_size)];
		memset(m_gdata[j], 0, block_size);
	}

	gen_in_memory = -1; // no generation's loaded in memory

	// initialize mutex
	if (pthread_mutex_init(&work_mutex, NULL)) {

		perror("Mutex initialization failed\n");
	}

	file_counter = 0; //add 11/20

}

// Client
void CodeTorrent::Init(int in_field_size, int in_file_id, int in_num_blocks_per_gen, int in_block_size,
		char *in_filename, int in_file_size, bool in_is_sim) {

	gettimeofday(&ct_tv_beg, &ct_tz);
	int i, j;

	file_id[0] = in_file_id; // NOTE: only use file_id[0]

	// set internal parameters
	field_size = in_field_size;
	//num_gens = in_num_gens;
	block_size = in_block_size;
	strcpy(filename, in_filename);
	is_sim = in_is_sim;
	file_size = in_file_size;

	gen_completed = 0;

	buffer_size = in_num_blocks_per_gen * 2;
	num_blocks = (int) ceil((double) file_size * 8 / field_size / block_size);
	num_gens = (int) floor((double) num_blocks / in_num_blocks_per_gen);

	// boundary case: generations are evenly divided
	if (in_num_blocks_per_gen * num_gens == num_blocks) {

		num_blocks_gen = new int[num_gens];

		for (i = 0; i < num_gens; ++i)
			num_blocks_gen[i] = in_num_blocks_per_gen;

	} else {

		num_gens++;
		num_blocks_gen = new int[num_gens];

		for (i = 0; i < num_gens - 1; ++i)
			num_blocks_gen[i] = in_num_blocks_per_gen;

		// last generation
		num_blocks_gen[num_gens - 1] = num_blocks - in_num_blocks_per_gen * (num_gens - 1);
	}

	// num_blocks_gen[0] must be the maximum of all num_blocks_gen[i]
	cb = AllocCodedBlock(num_blocks_gen[0], block_size);

	// allocate memory to helpfulness check matrix
	m_helpful = new unsigned char **[num_gens];

	for (i = 0; i < num_gens; ++i) {

		m_helpful[i] = new unsigned char *[num_blocks_gen[i]];

		for (j = 0; j < num_blocks_gen[i]; ++j) {
			m_helpful[i][j] = new unsigned char[(is_sim ? 1 : num_blocks_gen[i])];
			memset(m_helpful[i][j], 0, num_blocks_gen[i]);
		}
	}

	// allocate memory to rank vector and initialize them to zeros
	rank_vec = new int[num_gens];
	rank_vec_in_buf = new int[num_gens];

	for (i = 0; i < num_gens; ++i) {

		rank_vec[i] = 0;
		rank_vec_in_buf[i] = 0;

	}

	identity = CT_CLIENT; // I'm a client!

	PrintFileInfo();

	// m_gdata : data for each generation
	// allocate memory to m_gdata
	m_gdata = new unsigned char*[num_blocks_gen[0]];

	for (j = 0; j < num_blocks_gen[0]; ++j) {

		m_gdata[j] = new unsigned char[(is_sim ? 1 : block_size)];
		memset(m_gdata[j], 0, block_size);
	}

	// output file name setup
	//char *ext = ".rec";
	char *ext = ".received";
	int fname_len = strlen(filename);
	int ext_len = strlen(ext);

	//out_filename = new char[fname_len + ext_len + 1];
	memcpy(out_filename, filename, fname_len);
	memcpy(out_filename + fname_len, ext, ext_len);

	out_filename[fname_len + ext_len] = '\0';

	// TODO: What should we do if the file already exists?
	remove(out_filename);

	gen_in_memory = -1; // no generation's loaded in memory

	// initialize mutex
	if (pthread_mutex_init(&work_mutex, NULL)) {
		perror("Mutex initialization failed\n");
	}

	file_counter = 0; //add 11/20
}

void CodeTorrent::LoadFileInfo(int in_num_blocks_per_gen) {

	//FILE *fp;

	int i;
	int n_items = 0;

	struct stat sbuf;

	// get the file size
	if (stat(filename, &sbuf) == -1) {
#ifdef WIN32
		fprintf(stderr, "%s: stat(2) \n", strerror(errno));
#else
		printf("error from LoadFileInfo()\n");
#endif
		abort();
	}

	// set the file size
	file_size = sbuf.st_size;

	//fclose(fp);

	num_blocks = (int) ceil((double) file_size * 8 / field_size / block_size);
	num_gens = (int) floor((double) num_blocks / in_num_blocks_per_gen);

	// boundary case: generations are evenly divided
	if (in_num_blocks_per_gen * num_gens == num_blocks) {

		num_blocks_gen = new int[num_gens];

		for (i = 0; i < num_gens; ++i)
			num_blocks_gen[i] = in_num_blocks_per_gen;

	} else {

		num_gens++;
		num_blocks_gen = new int[num_gens];

		for (i = 0; i < num_gens - 1; ++i)
			num_blocks_gen[i] = in_num_blocks_per_gen;

		// last generation
		num_blocks_gen[num_gens - 1] = num_blocks - in_num_blocks_per_gen * (num_gens - 1);
	}
}

void CodeTorrent::LoadFile(int gen) {

	BlockPtr pblk;

	if (!(fp = fopen(filename, "rb"))) {
		printf("ERROR: cannot open %s\n", filename);
		return;
	}

	int i, j;
	int n_items = 0;
	int pos = 0;
	int temp;
	bool last_gen = false;
	int last_block_size;

	int gen_size = block_size * num_blocks_gen[gen]; // set gen_size for this gen
	unsigned char *fbuf = new unsigned char[gen_size];

	for (i = 0; i < gen; i++)
		pos += num_blocks_gen[i];

	pos *= block_size;

	fseek(fp, pos, SEEK_SET); // move file cursor to begining of n-th generation

	// read one generation
	temp = fread(fbuf, 1, gen_size, fp);

	if (temp + pos == file_size && gen == num_gens - 1) {

		last_gen = true;

	} else if (temp != gen_size) {

#ifdef WIN32
		fprintf(stderr, "%s: fread(2) \n", strerror(errno));
#else
		printf("error from LoadFile\n");
#endif
		printf("Press <enter>...");
		getchar();
		abort();
	}

	fclose(fp);

	// before begining, erase everything in data
	CleanData();

	// N.B. data is stored "unpacked" e.g., 4bit symbol => 8bit. 
	for (i = 0; i < num_blocks_gen[gen]; i++) {

		pblk = AllocBlock((is_sim ? 1 : block_size));
		memset(pblk, 0, (is_sim ? 1 : block_size));

		// if this is the last block
		if (last_gen && i == num_blocks_gen[gen] - 1) {

			last_block_size = temp - (num_blocks_gen[gen] - 1) * block_size;

			for (j = 0; j < (is_sim ? 1 : last_block_size); j++) {
				pblk[j] = NthSymbol(fbuf, field_size, block_size * i + j);
			}

			if (!is_sim) {

				for (; j < block_size; j++) {
					pblk[j] = 0; // padding zeros
				}
			}
		} else {

			for (j = 0; j < (is_sim ? 1 : block_size); j++) {
				pblk[j] = NthSymbol(fbuf, field_size, block_size * i + j);
			}
		}

		data.push_back(pblk);
	}

	// record which gen is in memory!
	gen_in_memory = gen;

	delete[] fbuf;
}

unsigned char CodeTorrent::NthSymbol(unsigned char *in_buf, int fsize, int at) {

	unsigned char sym;

	// field size must be 2^x
	assert( fsize%2 == 0);

	// total number of symbols per CHAR. 
	unsigned int numsyms = (unsigned int) 8 / fsize;
	sym = in_buf[(int) at / numsyms];
	sym >>= ((numsyms - at % numsyms - 1) * fsize);
	sym &= (((unsigned char) -1) >> (8 - fsize));

	return sym;
}

// This function is obsolete
/*
 void CodeTorrent::WriteFile() {

 char *ext = ".rec";
 int fname_len = strlen(filename);
 int ext_len = strlen(ext);

 char *filename_write = new char[fname_len + ext_len + 1];
 memcpy(filename_write, filename, fname_len);
 memcpy(filename_write+fname_len, ext, ext_len);

 filename_write[fname_len+ext_len] = '\0';

 FILE *fp_write;

 if( !(fp_write = fopen(filename_write, "wb")) ) {
 fprintf(stderr, "Cannot write to $s.\n", filename_write);
 exit(1);	// TODO: instead of exiting, we should ...
 }

 int i,j,k;
 int pos=0;

 for( i=0 ; i < num_gens ; i++ ) {

 for( j=0 ; j < num_blocks_gen[i] ; j++ ) {

 if( i == num_gens-1 && j == num_blocks_gen[i]-1 ) {

 for( k=0 ; k < num_gens-1 ; k++ ) {
 pos += num_blocks_gen[k];
 }

 int last_block_size = file_size - (pos+num_blocks_gen[i]-1)*block_size;

 fwrite(m_data[i][j], 1, last_block_size, fp_write);

 } else {

 fwrite(m_data[i][j], 1, block_size, fp_write);
 }
 }
 }

 fclose(fp_write);
 }
 */

void CodeTorrent::WriteFile(int gen) {

	fp_write = fopen(out_filename, "ab");

	//fseek(fp_write, 1, SEEK_END);

	int j, k;
	int pos = 0;

	for (j = 0; j < num_blocks_gen[gen]; j++) {

		if (gen == num_gens - 1 && j == num_blocks_gen[gen] - 1) {

			for (k = 0; k < num_gens - 1; k++) {
				pos += num_blocks_gen[k];
			}

			int last_block_size = file_size - (pos + num_blocks_gen[gen] - 1) * block_size;

			fwrite(m_gdata[j], 1, last_block_size, fp_write);
		} else {
			fwrite(m_gdata[j], 1, block_size, fp_write);
		}
	}

	fclose(fp_write);
}

BlockPtr CodeTorrent::AllocBlock(int in_block_size) {

	return (BlockPtr) malloc(in_block_size);
}

void CodeTorrent::FreeBlock(BlockPtr blk) {

	free(blk);
}

CoeffsPtr CodeTorrent::AllocCoeffs(int numblks) {
	return (CoeffsPtr) malloc(numblks);
}

void CodeTorrent::FreeCoeffs(CoeffsPtr blk) {
	free(blk);
}

CodedBlockPtr CodeTorrent::AllocCodedBlock(int numblks, int blksize) {
	CodedBlock *blk = NULL;

	blk = (CodedBlock *) malloc(sizeof(CodedBlock));
	assert(blk);

	blk->coeffs = AllocCoeffs(numblks);
	blk->sums = AllocBlock(blksize);
	blk->block_size = blksize;
	blk->num_blocks_gen = numblks;

	assert(blk->coeffs);
	assert(blk->sums);

	return blk;
}

void CodeTorrent::FreeCodedBlock(CodedBlockPtr ptr) {
	FreeCoeffs(ptr->coeffs);
	FreeBlock(ptr->sums);
	free(ptr);
}

CodedBlock* CodeTorrent::CopyCodedBlock(CodedBlockPtr ptr) {
	CodedBlock *new_blk = AllocCodedBlock(ptr->num_blocks_gen, ptr->block_size);
	new_blk->gen = ptr->gen;
	new_blk->num_blocks_gen = ptr->num_blocks_gen;
	new_blk->block_size = ptr->block_size;

	memcpy(new_blk->coeffs, ptr->coeffs, new_blk->num_blocks_gen);
	memcpy(new_blk->sums, ptr->sums, new_blk->block_size);

	return new_blk;
}

// encode a block from generation "gen"
CodedBlockPtr CodeTorrent::Encode(int gen) {

	pthread_mutex_lock(&work_mutex);

	// create a new copy
	CodedBlockPtr cb_to = AllocCodedBlock(num_blocks_gen[gen], block_size);

	// if the data's not already in memory, load it
	if (gen_in_memory != gen) {
		LoadFile(gen);
	}

	cb_to->gen = gen;
	cb_to->num_blocks_gen = num_blocks_gen[gen];
	cb_to->block_size = block_size;

	gettimeofday(&ct_encode_beg, &ct_tz);
	nc->EncodeBlock(data, cb_to);
	gettimeofday(&ct_encode_end, &ct_tz);
	printf("[CT_Encode] Encode time: %ld.%03ld\n", CT_GetTimeDifference(ct_encode_beg, ct_encode_end) / 1000,
			CT_GetTimeDifference(ct_encode_beg, ct_encode_end) % 1000);

	pthread_mutex_unlock(&work_mutex);

	return cb_to;
}

// re-encode a block from generation "gen"
// return NULL pointer if failed (e.g. no data in buffer)
CodedBlockPtr CodeTorrent::ReEncode(int gen) {

	pthread_mutex_lock(&work_mutex);

	int j;

	// TODO: Make this work with file buffers
	cb->gen = gen;
	cb->num_blocks_gen = num_blocks_gen[gen];
	cb->block_size = block_size;

	//std::vector<CodedBlockPtr> *tempBuf = new std::vector<CodedBlockPtr>[GetRankVec()[gen]];
	std::vector<CodedBlockPtr> tempBuf;

	gettimeofday(&ct_vector_beg, &ct_tz);

	for (std::vector<CodedBlockPtr>::iterator it = buf.begin(); it != buf.end(); ++it) {
		if ((*it)->gen == gen)
			tempBuf.push_back(CopyCodedBlock((*it)));
	}

	int blocks_in_file = rank_vec[gen] - rank_vec_in_buf[gen];

	for (j = 0; j < blocks_in_file; j++) {
		tempBuf.push_back(ReadGenBuf(gen, j));
	}

	gettimeofday(&ct_vector_end, &ct_tz);
	printf("[CT_Encode] Vector time: %ld.%03ld\n", CT_GetTimeDifference(ct_vector_beg, ct_vector_end) / 1000,
			CT_GetTimeDifference(ct_vector_beg, ct_vector_end) % 1000);

	//gettimeofday(&ct_encode_beg, &ct_tz);

	if (!nc->ReEncodeBlock(tempBuf, cb)) {
		return NULL;
	}

	//gettimeofday(&ct_encode_end, &ct_tz);
	//printf("[CT_Encode] Encode time: %ld.%03ld\n", CT_GetTimeDifference(ct_encode_beg, ct_encode_end)/1000, CT_GetTimeDifference(ct_encode_beg, ct_encode_end)%1000);

	for (j = 0; j < tempBuf.size(); j++)
		FreeCodedBlock(tempBuf[j]);
	tempBuf.clear();

	pthread_mutex_unlock(&work_mutex);

	return CopyCodedBlock(cb);
}

// store a new block into the buffer
bool CodeTorrent::StoreBlock(CodedBlockPtr in) {

	pthread_mutex_lock(&work_mutex);

	int gen = in->gen;

	if (nc->IsHelpful(rank_vec, m_helpful, in)) {

		buf.push_back(CopyCodedBlock(in));
		rank_vec[gen]++; // if helpful, raise the rank
		rank_vec_in_buf[gen]++;

		if (buf.size() >= buffer_size) { // if the buf goes above the threshold
			FlushBuf();
		}

	} else {

		pthread_mutex_unlock(&work_mutex);
		return false; // if not helpful, return false
	}

	// if full-rank, this generation is complete
	if (rank_vec[gen] == GetNumBlocksGen(gen)) {

		IncrementGenCompleted();
	}

	pthread_mutex_unlock(&work_mutex);

	return true;
}

// decode!
bool CodeTorrent::Decode() {

	int i;
	struct timeval start_dec, end_dec;

	pthread_mutex_lock(&work_mutex);

	if (identity != CT_CLIENT) // if I'm a server, why do it?
		return false;

	identity = CT_SERVER;

	if (GetGenCompleted() != GetNumGens()) { // make sure all generations are in!

		fprintf(stderr, "The file download is not complete, decoding failed.\n");
		pthread_mutex_unlock(&work_mutex);
		return false;
	}

	//gettimeofday(&start_dec, &ct_tz);
	printf("Decoding: ");

	for (i = 0; i < GetNumGens(); i++) {

		printf("%d  :  ", i);
		DecodeGen(i);
	}
	/*
	 gettimeofday(&end_dec, &ct_tz);
	 printf("Decoding time:  %ld.%ld\n", CT_GetTimeDifference(start_dec, end_dec)/1000,
	 CT_GetTimeDifference(start_dec, end_dec)%1000);
	 */
	printf("done!\n");
	//gettimeofday(&ct_tv_end, &ct_tz);
	//printf("TIME!!! %ld.%ld\n", CT_GetTimeDifference(ct_tv_beg, ct_tv_end)/1000, CT_GetTimeDifference(ct_tv_beg, ct_tv_end)%1000);

	// we don't need this stuff anymore
	CleanBuffer();
	CleanTempFiles();

	pthread_mutex_unlock(&work_mutex);

	return true;
}

bool CodeTorrent::DecodeGen(int gen) {

	int j;

	// allocate m_data
	//m_gdata = new unsigned char *[num_gens];

	std::vector<CodedBlockPtr> tempBuf; // buffer to save encoded_blocks from file and current buffer

	// fill in buffer! from file and current buffer
	//for( j=0 ; j <  buf.size() ; j++ ) {
	for (std::vector<CodedBlockPtr>::iterator it = buf.begin(); it != buf.end(); ++it) {
		if ((*it)->gen == gen)
			tempBuf.push_back(CopyCodedBlock((*it)));
	}

	int blocks_in_file = rank_vec[gen] - rank_vec_in_buf[gen];

	for (j = 0; j < blocks_in_file; j++) {
		tempBuf.push_back(ReadGenBuf(gen, j));
	}

	nc->Decode(tempBuf, m_gdata, gen);

	// when done decoding, write it into a file
	WriteFile(gen);

	// free tempBuf
	for (j = 0; j < tempBuf.size(); j++)
		FreeCodedBlock(tempBuf[j]);
	tempBuf.clear();

	return true;
}

// each file buffer stores an array of coded blocks
// +-------+------------+-------+------------+-----
// | coeff | coded data | coeff | coded data | ...
// +-------+------------+-------+------------+-----
void CodeTorrent::PushGenBuf(CodedBlockPtr in) {

	int gen = in->gen;

	char *ext = ".temp";
	char *ext2 = new char[3]; // what if more than 100 gens?
#ifdef WIN32
	itoa(gen, ext2, 10); // files are to be named "~~~.temp0", "~~~.temp1", etc.
#else
	sprintf(ext2, "%d", gen);
#endif	

	int fname_len = strlen(filename);
	int ext_len = strlen(ext);
	int ext2_len = strlen(ext2);

	// this is dumb but works
	char *filename_write = new char[fname_len + ext_len + ext2_len + 1];

	memcpy(filename_write, filename, fname_len);
	memcpy(filename_write + fname_len, ext, ext_len);
	memcpy(filename_write + fname_len + ext_len, ext2, ext2_len);

	filename_write[fname_len + ext_len + ext2_len] = '\0';

	fp_write = fopen(filename_write, "ab"); // append to the file
											// TODO: error checking?
	int cf = fwrite(in->coeffs, 1, num_blocks_gen[gen], fp_write); // write the coeffs first
	int sm = fwrite(in->sums, 1, block_size, fp_write); // write actual coded block

	if (cf != num_blocks_gen[gen] || sm != block_size) {
		fprintf(stderr, "cache writing error!\n");
	}

	delete[] ext2;
	delete[] filename_write;

	fclose(fp_write); // close the file
}

// each file buffer stores an array of coded blocks
// +-------+------------+-------+------------+-----
// | coeff | coded data | coeff | coded data | ...
// +-------+------------+-------+------------+-----
CodedBlockPtr CodeTorrent::ReadGenBuf(int gen, int k) {

	// NOTE: error checking must be done prior to invoking this method!
	//		(i.e. are there strictly less than k blocks in this file buffer?)
	// NOTE: k begins at 0

	CodedBlockPtr tempBlock;
	tempBlock = AllocCodedBlock(num_blocks_gen[gen], block_size);
	tempBlock->gen = gen;

	char *ext = ".temp";
	char *ext2 = new char[3]; // what if more than 100 gens?

#ifdef WIN32
	itoa(gen, ext2, 10); // base : 10
#else
	sprintf(ext2, "%d", gen);
#endif

	int fname_len = strlen(filename);
	int ext_len = strlen(ext);
	int ext2_len = strlen(ext2);

	// this is dumb but works
	char *filename_read = new char[fname_len + ext_len + ext2_len + 1];

	memcpy(filename_read, filename, fname_len);
	memcpy(filename_read + fname_len, ext, ext_len);
	memcpy(filename_read + fname_len + ext_len, ext2, ext2_len);

	filename_read[fname_len + ext_len + ext2_len] = '\0';

	fp = fopen(filename_read, "rb");

	if (!fp) {
		fprintf(stderr, "cache access error!\n");
	}

	if (fseek(fp, (num_blocks_gen[gen] + block_size) * k, SEEK_SET)) {
		fprintf(stderr, "cache access error!\n");
	}

	int cf = fread(tempBlock->coeffs, 1, num_blocks_gen[gen], fp);
	int sm = fread(tempBlock->sums, 1, block_size, fp);

	if (cf != num_blocks_gen[gen] || sm != block_size) {
		fprintf(stderr, "cache reading error!\n");
	}

	fclose(fp);

	delete[] filename_read;
	delete[] ext2;

	return tempBlock;
}

void CodeTorrent::FlushBuf() {

	//pthread_mutex_lock(&work_mutex);

	if (!(buf.size() >= buffer_size)) { // Should I really flush it?
		//pthread_mutex_unlock(&work_mutex);
		return;
	}

	// NOTE: For now, we select a random generation and evict it from the buffer
	int i;

	int gen_to_evict = rand() % num_gens;

	CodedBlockPtr tempBlock;

	while (rank_vec_in_buf[gen_to_evict] <= 0) {
		gen_to_evict = rand() % num_gens;
	}

	for (i = buf.size() - 1; i >= 0; i--) {

		tempBlock = buf[i];

		if (tempBlock->gen == gen_to_evict) { // if this block is from the selected generation

			PushGenBuf(tempBlock); // push it to the file buffer
			FreeCodedBlock(tempBlock); // and remove it from memory buffer
			buf.erase(buf.begin() + i);
			rank_vec_in_buf[gen_to_evict]--; // decrement rank of gen i in buf
		}

		if (rank_vec_in_buf[gen_to_evict] <= 0)
			break;
	}

	//pthread_mutex_unlock(&work_mutex);
}

void CodeTorrent::PrintBlocks(int gen) {
	int j, k;

	printf("===== Original Data ====\n");

	for (j = 0; j < num_blocks_gen[gen]; j++) {

		printf("[Gen#%2u Blk#%3u] ", gen, j); // gen ID and block ID. 

		for (k = 0; k < ((is_sim ? 1 : block_size) > PRINT_BLK ? PRINT_BLK : (is_sim ? 1 : block_size)); k++) {
			printf("%x ", data[j][k]); // j-th block, k-th character
		}

		if ((is_sim ? 1 : block_size) > PRINT_BLK * 2) {
			printf(" ... ");
		}

		for (k = (is_sim ? 1 : block_size) - PRINT_BLK; k < (is_sim ? 1 : block_size); k++) {
			printf("%x ", data[j][k]);
		}

		printf("\n");

	}
	printf("=======================\n");

}

// print-out decoded data (member "m_d")
void CodeTorrent::PrintDecoded() {
	/*
	 int i,j,k;

	 printf("===== Decoded Data =====\n");

	 for( i=0 ; i < num_gens ; i++ ) {

	 for ( j=0 ; j < num_blocks_gen[i] ; j++ ) {

	 printf("[Gen#%2u Blk#%3u] ", i, j); // block ID.

	 for ( k=0 ; k < ((is_sim ? 1 : block_size) > PRINT_BLK ? PRINT_BLK : (is_sim ? 1 : block_size)) ; k++ ) {

	 printf("%x ", (char)m_data[i][j][k]);
	 }

	 if( (is_sim?1:block_size) > PRINT_BLK*2 ) {

	 printf(" ... ");

	 for (k=(is_sim ? 1 : block_size)-PRINT_BLK; k < (is_sim? 1 : block_size); k++ ) {

	 printf("%x ", (char)m_data[i][j][k]);
	 }
	 }
	 printf("\n");
	 }
	 }
	 printf("=======================\n");
	 */

}

//added
/*
 void CodeTorrent::Update(int in_file_id, int in_num_blocks_per_gen, int in_block_size, char *in_filename, int in_file_size)
 {
 file_id[0] = in_file_id; //??
 num_blocks_per_gen = in_num_blocks_per_gen;
 block_size = in_block_size;
 strcpy(filename, in_finename);
 file_size = in_file_size;
 }
 */

void CodeTorrent::PrintFileInfo() {

	printf(" ========= File Info =========\n");
	if (identity == CT_SERVER)
		printf(" SERVER\n");
	else
		printf(" CLIENT\n");
	printf(" File name: %s\n", GetFileName());
	printf(" File Size: %d\n", GetFileSize());
	printf(" Number Generations: %d\n", GetNumGens());
	printf(" Block Size: %d\n", GetBlockSize());
	printf(" Num Blocks: %d\n", GetNumBlocks());
	//printf(" Num Blocks per Gen: %d\n", GetNumBlocksGen());
	printf(" Field Size: %d bits\n", GetFieldSize());
	printf(" ============================\n");
}
