/*
 * galois.cpp
 *
 * Implements GALOIS module for Galois Field arithematic operations
 *
 * Coded by Joe Yeh and Uichin Lee
 * Modified and extended by Seunghoon Lee and Sung Chul Choi
 * University of California, Los Angeles
 *
 * Last modified: 10/30/06
 *
 */

#include "galois.h"

void Galois::Init()
{
	int b, j;

	for (j = 0; j < GF; j++) {
		Log[j] = GF-1;
		ALog[j] = 0;
	} 

	b = 1;
	for (j = 0; j < GF-1; j++) {
		Log[b] = j;
		ALog[j] = b;
		b = b*2;
		if (b & GF) 
			b = (b ^ PP) & (GF-1);
	}

	for(;j<=(GF-1)*2-2; j++) {
		ALog[j] = ALog[j%(GF-1)];
	}

	ALog[(GF-1)*2-1] = ALog[254];

}
/*
unsigned char Galois::Mul(unsigned char a, unsigned char b, int ff) 
{
	if( a == 0 || b == 0 )
		return 0;
	else
		//return ALog[(Log[a]+Log[b])%(GF-1)]; w/o optimization
		return ALog[Log[a]+Log[b]];
}

unsigned char Galois::Div(unsigned char a, unsigned char b, int ff) 
{
	if( a == 0 || b == 0 )
		return 0;
	else
		//return ALog[(Log[a]-Log[b]+GF-1)%(GF-1)]; w/o optimization
		return ALog[Log[a]-Log[b]+GF-1];
}
*/
