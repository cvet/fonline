/*
 * ACM decompression engine source code: ACM seeking
 *
 * Copyright (C) 2000 ANX Software
 * E-mail: anxsoftware@avn.mccme.ru
 *
 * Author: Alexander Belyakov (abel@krasu.ru)
 *
 * Adapted for ACMStream.DLL by Valery V. Anisimovsky (samael@avn.mccme.ru)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <memory.h>
#include <stdio.h>

#include <windows.h>

#include "ACMStreamUnpack.h"

FillerProc s_Fillers[32] = {
	&CACMUnpacker::s_ZeroFill,
	&CACMUnpacker::s_Return0,
	&CACMUnpacker::s_Return0,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_LinearFill,
	&CACMUnpacker::s_k1_3bits,
	&CACMUnpacker::s_k1_2bits,
	&CACMUnpacker::s_t1_5bits,
	&CACMUnpacker::s_k2_4bits,
	&CACMUnpacker::s_k2_3bits,
	&CACMUnpacker::s_t2_7bits,
	&CACMUnpacker::s_k3_5bits,
	&CACMUnpacker::s_k3_4bits,
	&CACMUnpacker::s_Return0,
	&CACMUnpacker::s_k4_5bits,
	&CACMUnpacker::s_k4_4bits,
	&CACMUnpacker::s_Return0,
	&CACMUnpacker::s_t3_7bits,
	&CACMUnpacker::s_Return0,
	&CACMUnpacker::s_Return0
};

int CACMUnpacker::seek_to (int new_pos) {
	int cur_pos = acm_samples - valsToGo - valCnt,
		cur_block = cur_pos / someSize2,
		new_block = new_pos / someSize2;
	if (cur_block == new_block) {
		// If we need to seek to current block, we simply start from its beginning
		values = someBuff;
		valsToGo = acm_samples - cur_block * someSize2;
		valCnt = (someSize2 > valsToGo)? valsToGo: someSize2;
		valsToGo -= valCnt;
	} else {
		if (cur_block > new_block) {
			// To seek backward we need to go from very start of stream.
			// Skipping header
			posBuf=14;
			//SetFilePointer (hFile, 14, 0, FILE_BEGIN);
			valCnt = 0;
			availBytes = 0; nextBits = 0; availBits = 0;
			cur_block = 0;
			valsToGo = acm_samples;
		}

		// Dropping all the prepared values in curent block, if there are any
		if (valCnt) {
			cur_block++;
		}
		valCnt = 0;

		// Skipping blocks
		for (int i=cur_block; i<new_block; i++) {
			int block_hdr = getBits (20); // pwr and val

			for (int pass=0; pass<someSize; pass++) {
				int ind = getBits (5) & 0x1F;
				if (! ((this->*s_Fillers[ind]) (pass, ind)) ) {
					return cur_pos;
				}
			}
		}
		valsToGo = acm_samples - new_block * someSize2;
	}
	return new_block * someSize2;
}

// s_Filling functions:
// int CACMUnpacker::FillerProc (int pass, int ind)
int CACMUnpacker::s_Return0 (int pass, int ind) {
	return 0;
}
int CACMUnpacker::s_ZeroFill (int pass, int ind) {
	return 1;
}
int CACMUnpacker::s_LinearFill (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++)
		int dumb = getBits (ind);
	return 1;
}
int CACMUnpacker::s_k1_3bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (3);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
			i++;
		} else if ((nextBits & 2) == 0) {
			availBits -= 2;
			nextBits >>= 2;
		} else {
			availBits -= 3;
			nextBits >>= 3;
		}
	}
	return 1;
}
int CACMUnpacker::s_k1_2bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (2);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
		} else {
			availBits -= 2;
			nextBits >>= 2;
		}
	}
	return 1;
}
int CACMUnpacker::s_t1_5bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		char bits = getBits (5) & 0x1f;
		i += 2;
	}
	return 1;
}
int CACMUnpacker::s_k2_4bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (4);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
			i++;
		} else if ((nextBits & 2) == 0) {
			availBits -= 2;
			nextBits >>= 2;
		} else {
			availBits -= 4;
			nextBits >>= 4;
		}
	}
	return 1;
}
int CACMUnpacker::s_k2_3bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (3);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
		} else {
			availBits -= 3;
			nextBits >>= 3;
		}
	}
	return 1;
}
int CACMUnpacker::s_t2_7bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		char bits = getBits (7) & 0x7f;
		i += 2;
	}
	return 1;
}
int CACMUnpacker::s_k3_5bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (5);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
			i++;
		} else if ((nextBits & 2) == 0) {
			availBits -= 2;
			nextBits >>= 2;
		} else if ((nextBits & 4) == 0) {
			availBits -= 4;
			nextBits >>= 4;
		} else {
			availBits -= 5;
			nextBits >>= 5;
		}
	}
	return 1;
}
int CACMUnpacker::s_k3_4bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (4);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
		} else if ((nextBits & 2) == 0) {
			availBits -= 3;
			nextBits >>= 3;
		} else {
			availBits -= 4;
			nextBits >>= 4;
		}
	}
	return 1;
}
int CACMUnpacker::s_k4_5bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (5);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
			i++;
		} else if ((nextBits & 2) == 0) {
			availBits -= 2;
			nextBits >>= 2;
		} else {
			availBits -= 5;
			nextBits >>= 5;
		}
	}
	return 1;
}
int CACMUnpacker::s_k4_4bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		prepareBits (4);
		if ((nextBits & 1) == 0) {
			availBits--;
			nextBits >>= 1;
		} else {
			availBits -= 4;
			nextBits >>= 4;
		}
	}
	return 1;
}
int CACMUnpacker::s_t3_7bits (int pass, int ind) {
	for (int i=0; i<packAttrs2; i++) {
		char bits = getBits (7) & 0x7f;
		i++;
	}
	return 1;
}