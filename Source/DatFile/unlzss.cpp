/*
 * fDAT Plug-in source code: LZSS unpacking class
 *
 * Copyright (C) 2000 Alexander Belyakov
 * E-mail: abel@krasu.ru
 *
 * Decompression function (decode) is based on LZSS32 Delphi unit:
 *    Assembler Programmer: Andy Tam, Pascal Conversion: Douglas Webb,
 *    Unit Conversion and Dynamic Memory Allocation: Andrew Eigus,
 *    Delphi 2 Porting: C.J.Rankin.
 * Variable and constant names are taken from Kevin Doherty's LZSS project.
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

#include <string.h>
#include "unlzss.h"


int CunLZSS::getC() {
	if (inBufPtr >= inBufSize)
		return -1;
	return inBuf [inBufPtr++];
}

void CunLZSS::putC (unsigned char c) {
	if (outBufPtr < MAX_UNPACK_SIZE) {
	// this condition must always be true, but who known for sure
		outBuf [outBufPtr++] = c;
		unpackedLen++;
	}
}

void CunLZSS::decode() {
	long command,
		current_position,
		match_position,
		match_length;
	int c, i;

	command = 0;
	current_position = WINSIZE - LOOK_SIZE;

	while (1) {
		command >>= 1;
		if ((command & 0xFF00) == 0) {
			if ( (c = getC()) == -1)
				break;
			command = 0xFF00 + c;
		}
		if ( (c = getC()) == -1)
			break;
		if (command & 1) {
			window [current_position] = c;
			current_position = MOD_WINDOW (current_position + 1);
			putC (c);
		} else {
			match_position = c;
			if ( (c = getC()) == -1)
				break;
			match_position += ((c & 0xF0) << 4);
			match_length = (c & 0xF) + BREAK_EVEN;
			for (i=0; i<=match_length; i++) {
				c = window [MOD_WINDOW (match_position + i)];
				window [current_position] = c;
				putC (c);
				current_position = MOD_WINDOW (current_position + 1);
			}
		}

	}
}

void CunLZSS::takeNewData (unsigned char* in, long availIn, int doUnpack) {
	if (!window) window = new unsigned char [WINSIZE + LOOK_SIZE - 1];
	if (!outBuf) outBuf = new unsigned char [MAX_UNPACK_SIZE];

	if (doUnpack) {
		inBuf = in;
		inBufSize = availIn;
		inBufPtr = outBufPtr = 0;
		memset (window, 0x20, WINSIZE + LOOK_SIZE - 1);
		unpackedLen = 0;
		decode ();
	} else {
		memcpy (outBuf, in, availIn);
		unpackedLen = availIn;
	}
	outBufPtr = 0;
}

long CunLZSS::getUnpacked (unsigned char* to, long count) {
	long res = (count>unpackedLen)? unpackedLen: count;
	if (res) {
		memcpy (to, outBuf+outBufPtr, res);
		outBufPtr += res;
		unpackedLen -= res;
	}
	return res;
}