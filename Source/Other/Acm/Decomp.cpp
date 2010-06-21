/*
 * ACM decompression engine source code: ACM decompressor core
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

void Decode1 (short *decBuff, int *someBuff, int someSize, int blocks) {
	int row_0, row_1, row_2, row_3, db_0, db_1;
	if (blocks == 2) {
		for (int i=0; i<someSize; i++) {
			row_0 = someBuff[0];
			row_1 = someBuff[someSize];
			someBuff [0] = someBuff[0] + decBuff[0] + 2*decBuff[1];
			someBuff [someSize] = 2*row_0 - decBuff[1] - someBuff[someSize];
			decBuff [0] = row_0;
			decBuff [1] = row_1;

			decBuff += 2;
			someBuff++;
//Eng: reverse process most likely will be:
//Rus: обратный процесс будет, по всей видимости:
//	pk[0]  = real[0] - db[0] - 2*db[1];
//	pk[ss] = 2*pk[0] - db[1] - real[ss];
//	db[0]  = pk[0];
//	db[1]  = pk[ss];
//Eng: or even
//Rus: а может даже:
//	db[i][0] = (pk[i]    = real[i] - db[i][0] - 2*db[i][1]);
//	db[i][1] = (pk[i+ss] = 2*pk[i] - db[i][1] - real[i+ss]);
		}
	} else if (blocks == 4) {
		for (int i=0; i<someSize; i++) {
			row_0 = someBuff[0];
			row_1 = someBuff[someSize];
			row_2 = someBuff[2*someSize];
			row_3 = someBuff[3*someSize];

			someBuff [0]          =  decBuff[0] + 2*decBuff[1] + row_0;
			someBuff [someSize]   = -decBuff[1] + 2*row_0      - row_1;
			someBuff [2*someSize] =  row_0      + 2*row_1      + row_2;
			someBuff [3*someSize] = -row_1      + 2*row_2      - row_3;

			decBuff [0] = row_2;
			decBuff [1] = row_3;

			decBuff += 2;
			someBuff++;
//	pk[0][i] = -pk[-2][i] - 2*pk[-1][i] + real[0][i];
//	pk[1][i] = -pk[-1][i] + 2*pk[0][i]  - real[1][i];
//	pk[2][i] = -pk[0][i]  - 2*pk[1][i]  + real[2][i];
//	pk[3][i] = -pk[1][i]  + 2*pk[2][i]  - real[3][i];
		}
	} else {
		for (int i=0; i<someSize; i++) {
			int* someBuff_ptr = someBuff;
			if (((blocks >> 1) & 1) != 0) {
				row_0 = someBuff_ptr[0];
				row_1 = someBuff_ptr[someSize];

				someBuff_ptr [0]        =  decBuff[0] + 2*decBuff[1] + row_0;
				someBuff_ptr [someSize] = -decBuff[1] + 2*row_0      - row_1;
				someBuff_ptr += 2*someSize;

				db_0 = row_0;
				db_1 = row_1;
			} else {
				db_0 = decBuff[0];
				db_1 = decBuff[1];
			}

			for (int j=0; j<blocks >> 2; j++) {
				row_0 = someBuff_ptr[0];  someBuff_ptr [0] =  db_0  + 2*db_1  + row_0;  someBuff_ptr += someSize;
				row_1 = someBuff_ptr[0];  someBuff_ptr [0] = -db_1  + 2*row_0 - row_1;  someBuff_ptr += someSize;
				row_2 = someBuff_ptr[0];  someBuff_ptr [0] =  row_0 + 2*row_1 + row_2;  someBuff_ptr += someSize;
				row_3 = someBuff_ptr[0];  someBuff_ptr [0] = -row_1 + 2*row_2 - row_3;  someBuff_ptr += someSize;

				db_0 = row_2;
				db_1 = row_3;
			}
			decBuff [0] = row_2;
			decBuff [1] = row_3;
//Eng: the same as in previous cases, but larger. The process is seem to be reversible
//Rus: то же самое, что и в предыдущих случа€х, только больше по количеству. –адует, что процесс обратимый
			decBuff += 2;
			someBuff++;
		}
	}
}


void Decode2 (int *decBuff, int *someBuff, int someSize, int blocks) {
	int row_0, row_1, row_2, row_3, db_0, db_1;
	if (blocks == 4) {
		for (int i=0; i<someSize; i++) {
			row_0 = someBuff[0];
			row_1 = someBuff[someSize];
			row_2 = someBuff[2*someSize];
			row_3 = someBuff[3*someSize];

			someBuff [0]          =  decBuff[0] + 2*decBuff[1] + row_0;
			someBuff [someSize]   = -decBuff[1] + 2*row_0      - row_1;
			someBuff [2*someSize] =  row_0      + 2*row_1      + row_2;
			someBuff [3*someSize] = -row_1      + 2*row_2      - row_3;

			decBuff [0] = row_2;
			decBuff [1] = row_3;

			decBuff += 2;
			someBuff++;
		}
	} else {
		for (int i=0; i<someSize; i++) {
			int* someBuff_ptr = someBuff;
			db_0 = decBuff[0]; db_1 = decBuff[1];
			for (int j=0; j<blocks >> 2; j++) {
				row_0 = someBuff_ptr[0];  someBuff_ptr [0] =  db_0  + 2*db_1  + row_0;  someBuff_ptr += someSize;
				row_1 = someBuff_ptr[0];  someBuff_ptr [0] = -db_1  + 2*row_0 - row_1;  someBuff_ptr += someSize;
				row_2 = someBuff_ptr[0];  someBuff_ptr [0] =  row_0 + 2*row_1 + row_2;  someBuff_ptr += someSize;
				row_3 = someBuff_ptr[0];  someBuff_ptr [0] = -row_1 + 2*row_2 - row_3;  someBuff_ptr += someSize;

				db_0 = row_2;
				db_1 = row_3;
			}
			decBuff [0] = row_2;
			decBuff [1] = row_3;

			decBuff += 2;
			someBuff++;
		}
	}
}
