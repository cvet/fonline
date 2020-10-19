#ifndef __ACM_STREAM_UNPACK
#define __ACM_STREAM_UNPACK

#include <stdlib.h>

class CACMUnpacker {
private:
	unsigned char* fileBuf;
	int fileLen;
	int fileCur;

	unsigned char *fileBuffPtr, *buffPos; // pointer to file buffer and current position
	int bufferSize; // size of file buffer
	int availBytes; // new (not yet processed) unsigned chars in file buffer

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
private:
	// Unpacking functions
	int createAmplitudeDictionary();
	void unpackValues(); // unpack current block
	int makeNewValues(); // prepare new block, then unpack it
public:
	CACMUnpacker (unsigned char* file_buf, int file_len, int &channels, int &frequency, int &samples);
		// samples = count of sound samples (one sample is 16bits)
	~CACMUnpacker() {
//		if (fileBuffPtr) delete (fileBuffPtr);
		if (decompBuff) free (decompBuff);
		if (someBuff) free (someBuff);
	}

	int readAndDecompress (unsigned short* buff, int count);
		// read sound samples from ACM stream
		// buff  - buffer to place decompressed samples
		// count - size of buffer (in unsigned chars)
		// return: count of actually decompressed unsigned chars
};

typedef int (CACMUnpacker::* FillerProc) (int pass, int ind);

#endif