/*
 * nc.h
 *
 * Implements NC module for network coding
 *
 * Coded by Joe Yeh/Uichin Lee
 * Modified and extended by Seunghoon Lee/Sung Chul Choi
 * University of California, Los Angeles
 *
 * Last modified: 11/13/06
 *
 */

#ifndef __NC_H__
#define __NC_H__

// libraries
#include "galois.h"
#include <vector>
#include <assert.h>
#include <stdlib.h>
//#include <windows.h>	// for GetTickCount()


typedef unsigned char *BlockPtr;
typedef unsigned char *CoeffsPtr;

// a coded block
typedef struct CodedBlock_ {

	int gen;				// generation it belongs to
	int num_blocks_gen;		// number of blocks in a block
	int block_size;			// size of each block
	CoeffsPtr coeffs;		// coefficients
	BlockPtr sums;			// sums (the linear combination of data)

} CodedBlock;

typedef CodedBlock* CodedBlockPtr;

// NC module
class NC {

public:

	void Init();

	////////////////////////////
	// constructor(s)

	// default constructor
	NC(int in_field_size, bool in_is_sim) { 

		Init();
		field_size = in_field_size; 
		gf = new Galois();
		is_sim = in_is_sim;

		//srand((unsigned int)GetTickCount());
		srand(1);
	};

	// destructor
	~NC() {

		delete gf;
	};

	////////////////////////////
	// accessors

	////////////////////////////
	// other publiic functions

	// given a buffer and index, find the rank (i.e. # of linearly indep. vectors in the buffer)
	int GetRank(std::vector<CodedBlockPtr> *buffer, int gen) { return buffer[gen].size(); }
	int GetRank(std::vector<CodedBlockPtr> *buffer) { return buffer->size(); } //for each gen

	// encode/re-encode
	void EncodeBlock(std::vector<BlockPtr> &data, CodedBlockPtr out);
	bool ReEncodeBlock(std::vector<CodedBlockPtr> &buffer, CodedBlock* out);


	// check if the coded block is innovative
	//bool IsHelpful(std::vector<CodedBlockPtr> *buffer, unsigned char ***m_helpful, CodedBlockPtr in);	
	bool IsHelpful(int *rank_vec, unsigned char ***m_helpful, CodedBlockPtr in);

	// decode
	bool Decode(std::vector<CodedBlockPtr> *buffer, unsigned char ***m_data, int num_gens);
	bool Decode(std::vector<CodedBlockPtr> buffer, unsigned char **m_data, int gen); //for each gen

	unsigned char Add(unsigned char a, unsigned char b, int ff);
	unsigned char Mul(unsigned char a, unsigned char b, int ff);


private:

	////////////////////////////
	// private functions

	// decoding sub-methods
	bool IncrementalDecode(unsigned char ***m_upper, CodedBlockPtr in);
	bool BackSubstitution(std::vector<CodedBlockPtr> *buffer, unsigned char ***m_upper, unsigned char ***m_data, int gen);

	bool IncrementalDecode(unsigned char **m_upper, CodedBlockPtr in);
	bool BackSubstitution(std::vector<CodedBlockPtr> buffer, unsigned char **m_upper, unsigned char **m_data);

	////////////////////////////
	// member variables

	int field_size;		// size of the finite field in which this network coding operates
	Galois *gf;			// Galois field module
	bool is_sim;		// Is this simulation?
};

#endif
