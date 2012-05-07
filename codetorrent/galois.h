/*
 * galois.h
 *
 * Implements GALOIS module for Galois Field arithematic operations
 *
 * Coded by Joe Yeh/Uichin Lee
 * Modified and extended by Seunghoon Lee/Sung Chul Choi
 * University of California, Los Angeles
 *
 * Last modified: 10/30/06
 *
 */

#ifndef __GALOIS_H__
#define __GALOIS_H__

#define GF8  256
#define PP8 0435 // n.b. octal value.

// Galois module
class Galois {

public:
	////////////////////////////
	// constructor
	Galois() : GF(GF8), PP(PP8) {

		Init();
	}

	// destructor
	~Galois() {

	}

	// operations
	inline unsigned char Add(unsigned char a, unsigned char b, int ff) { return a^b; };	// a+b
	inline unsigned char Sub(unsigned char a, unsigned char b, int ff) { return a^b; };	// a-b
//	unsigned char Mul(unsigned char a, unsigned char b, int ff);		// a*b
//	unsigned char Div(unsigned char a, unsigned char b, int ff);		// a/b
	inline unsigned char Mul(unsigned char a, unsigned char b, int ff) {
		if( a == 0 || b == 0 )
			return 0;
		else
		//return ALog[(Log[a]+Log[b])%(GF-1)]; w/o optimization
			return ALog[Log[a]+Log[b]];
	}

	inline unsigned char Div(unsigned char a, unsigned char b, int ff) {
		if( a == 0 || b == 0 )
			return 0;
		else
		//return ALog[(Log[a]-Log[b]+GF-1)%(GF-1)]; w/o optimization
			return ALog[Log[a]-Log[b]+GF-1];
	}
private:

	////////////////////////////
	// member variables

	// log tables
	unsigned char Log[GF8];				
	unsigned char ALog[GF8*2];

	const int GF;
	const int PP;

	////////////////////////////
	// private functions

	// Initializes log tables
	void Init();		

};


#endif
