#ifndef __ACM_STREAM_UNPACK
#define __ACM_STREAM_UNPACK

#include <stdlib.h>
#include <windows.h>

#define ACM_BUFF_SIZE 0x10000

extern short Amplitude_Buffer[0x10000];
extern short *Buffer_Middle;
extern unsigned char fileBuffPtr[ACM_BUFF_SIZE];

class CACMUnpacker {
private:
// File reading
//	HANDLE hFile; // file handle, can be anything, e.g. ptr to reader-object
	BYTE* fileBuf;
	int posBuf;
	int sizeBuf;

	unsigned char *buffPos; // pointer to file buffer and current position
	int bufferSize; // size of file buffer
	int availBytes; // new (not yet processed) bytes in file buffer

// Bits
	unsigned nextBits; // new bits
	int availBits; // count of new bits

// Parameters of ACM stream
	int	packAttrs, someSize, packAttrs2, someSize2;

// Unpacking buffers
	int *decompBuff, *someBuff;
	int blocks, totBlSize;
	int valsToGo; // samples left to decompress
	int *values; // pointer to decompressed samples
	int valCnt; // count of decompressed samples

// Reading routines
	unsigned char readNextPortion(); // read next block of data
	void prepareBits (int bits); // request bits
	int getBits (int bits); // request and return next bits

public:
// These functions are used to fill the buffer with the amplitude values
	int Return0 (int pass, int ind);
	int ZeroFill (int pass, int ind);
	int LinearFill (int pass, int ind);

	int k1_3bits (int pass, int ind);
	int k1_2bits (int pass, int ind);
	int t1_5bits (int pass, int ind);

	int k2_4bits (int pass, int ind);
	int k2_3bits (int pass, int ind);
	int t2_7bits (int pass, int ind);

	int k3_5bits (int pass, int ind);
	int k3_4bits (int pass, int ind);

	int k4_5bits (int pass, int ind);
	int k4_4bits (int pass, int ind);

	int t3_7bits (int pass, int ind);

// These functions are used during stream seeking
	int s_Return0 (int pass, int ind);
	int s_ZeroFill (int pass, int ind);
	int s_LinearFill (int pass, int ind);

	int s_k1_3bits (int pass, int ind);
	int s_k1_2bits (int pass, int ind);
	int s_t1_5bits (int pass, int ind);

	int s_k2_4bits (int pass, int ind);
	int s_k2_3bits (int pass, int ind);
	int s_t2_7bits (int pass, int ind);

	int s_k3_5bits (int pass, int ind);
	int s_k3_4bits (int pass, int ind);

	int s_k4_5bits (int pass, int ind);
	int s_k4_4bits (int pass, int ind);

	int s_t3_7bits (int pass, int ind);
private:
// Unpacking functions
	int createAmplitudeDictionary();
	void unpackValues(); // unpack current block
	int makeNewValues(); // prepare new block, then unpack it
public:
	WORD acm_rate, acm_channels;
	DWORD acm_size, acm_samples;

	CACMUnpacker (BYTE* file_buf, int size_buf):
		decompBuff (NULL), someBuff (NULL),
		fileBuf (file_buf), posBuf(0), sizeBuf(size_buf),
		bufferSize (ACM_BUFF_SIZE),
		availBytes (0), nextBits (0), availBits (0) {};
	BOOL init_unpacker();
	~CACMUnpacker() {
		if (decompBuff) GlobalFree (decompBuff);
		if (someBuff) GlobalFree (someBuff);
	}

	int readAndDecompress (unsigned short* buff, int count);
		// read sound samples from ACM stream
		// buff  - buffer to place decompressed samples
		// count - size of buffer (in bytes)
		// return: count of actually decompressed bytes
	
	int canRead() {
		return valsToGo;
	};
	int seek_to (int new_pos);

	int readBuf(void* to_buf, int count)
	{
		if(posBuf+count>=sizeBuf) count=sizeBuf-posBuf;
		if(count<=0) return 0;
		memcpy(to_buf,fileBuf+posBuf,count);
		posBuf+=count;
		return count;
	}
};

typedef int (CACMUnpacker::* FillerProc) (int pass, int ind);

BOOL ACMStreamGetInfo (DWORD *rate, WORD *channels, WORD *bits, DWORD *size);
void ACMStreamGetCurInfo (DWORD *rate, WORD *channels, WORD *bits, DWORD *size);
BOOL ACMStreamInit (BYTE* file_buf, int size_buf);
void ACMStreamShutdown();
DWORD ACMStreamReadAndDecompress(char  *buff, DWORD  count);
DWORD ACMStreamSeek (DWORD new_pos);

#endif
