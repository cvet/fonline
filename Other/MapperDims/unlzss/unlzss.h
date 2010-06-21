#ifndef _GAP_FDAT_UNLZSS_H
#define _GAP_FDAT_UNLZSS_H


/* number of bits for index and length respectively */
#define INDEX_BC                12
#define LENGTH_BC               4

/* window size and raw lookahead size */
#define WINSIZE                 (1 << INDEX_BC)
#define RLOOK_SIZE              (1 << LENGTH_BC)

/* number of bytes to break even in compressing */
#define BREAK_EVEN              2
//((1 + INDEX_BC + LENGTH_BC) / 9)
#define LOOK_SIZE               (RLOOK_SIZE + BREAK_EVEN)

#define MOD_WINDOW( a )		(( a ) & (WINSIZE - 1))

#define MAX_UNPACK_SIZE 0x44000

class CunLZSS {
protected:
	unsigned char* window;
	long inBufPtr, inBufSize, outBufPtr;
	long unpackedLen;
	unsigned char *inBuf, *outBuf;

	int getC();
	void putC (unsigned char c);
	void decode();
public:
	CunLZSS():
		unpackedLen(0),
		window (NULL),
		outBuf (NULL) {};
	virtual ~CunLZSS() {
		if (window) delete (window);
		if (outBuf) delete (outBuf);
	}

	void takeNewData (unsigned char* in, long availIn, int doUnpack);
	long getUnpacked (unsigned char* to, long count);
	long left() {return unpackedLen;};
	void clear() {unpackedLen = 0;};
};

#endif