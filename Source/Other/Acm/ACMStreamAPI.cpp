/*
 * ACM decompression engine source code: DLL API stuff
 *
 * Copyright (C) 2000 ANX Software
 * E-mail: anxsoftware@avn.mccme.ru
 *
 * Author: Valery V. Anisimovsky (samael@avn.mccme.ru)
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

#include <windows.h>

#include "ACMStreamUnpack.h"

#define ID_ACM (0x01032897)

#pragma pack(1)

typedef struct tagACMHeader
{
	DWORD magic;
	DWORD size;
	WORD  channels;
	WORD  rate;
	WORD  attrs;
} ACMHeader;

#pragma pack()

CACMUnpacker *acm = NULL;

BOOL ACMStreamGetInfo (DWORD *rate, WORD *channels, WORD *bits, DWORD *size) {
	ACMHeader acmhdr;
	DWORD     read;

	read=acm->readBuf(&acmhdr, sizeof(ACMHeader));

	if ((read<sizeof(ACMHeader)) || (acmhdr.magic!=ID_ACM))
		return FALSE;
	*rate=acmhdr.rate;
	*size=2*acmhdr.size;
	*channels=acmhdr.channels;
	*bits=16; // ???

	return TRUE;
}

void ACMStreamGetCurInfo (DWORD *rate, WORD *channels, WORD *bits, DWORD *size) {
	*bits = 16;
	if (acm) {
		*rate = acm->acm_rate;
		*channels = acm->acm_channels;
		*size = acm->acm_size;
	} else {
		*rate = 22050;
		*channels = 2;
		*size = 0;
	}
}

BOOL ACMStreamInit (BYTE* file_buf, int size_buf) {
	BOOL res = FALSE;
	if ((!acm) && file_buf) {
		acm = new CACMUnpacker (file_buf, size_buf);
		res = acm->init_unpacker();
		if (!res)
			ACMStreamShutdown();
	}
	return res;
}

void ACMStreamShutdown() {
	if (acm) delete (acm);
	acm = NULL;
}

DWORD ACMStreamReadAndDecompress(char  *buff, DWORD  count) {
	DWORD ret=0L,read;
	CACMUnpacker *pacm=(CACMUnpacker*)acm;

	if (!acm)
		return 0L;

	while ((pacm->canRead()) && (count)) {
		read = pacm->readAndDecompress ((unsigned short*)buff, count);
		ret += read;
		buff += read;
		count -= read;
	}
	return ret;
}

DWORD ACMStreamSeek (DWORD new_pos) {
	if (acm)
		return acm->seek_to (new_pos);
	else
		return 0;
}
