/*
* Copyright (c) 2006-2013,  University of California, Los Angeles
* Coded by Joe Yeh/Uichin Lee
* Modified and extended by Seunghoon Lee/Sung Chul Choi
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
* in the documentation and/or other materials provided with the distribution.
* Neither the name of the University of California, Los Angeles nor the names of its contributors 
* may be used to endorse or promote products derived from this software without specific prior written permission.
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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

void Galois::Init() {
	int b, j;

	for (j = 0; j < GF; j++) {
		Log[j] = GF - 1;
		ALog[j] = 0;
	}

	b = 1;
	for (j = 0; j < GF - 1; j++) {
		Log[b] = j;
		ALog[j] = b;
		b = b * 2;
		if (b & GF)
			b = (b ^ PP) & (GF - 1);
	}

	for (; j <= (GF - 1) * 2 - 2; j++) {
		ALog[j] = ALog[j % (GF - 1)];
	}

	ALog[(GF - 1) * 2 - 1] = ALog[254];

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
