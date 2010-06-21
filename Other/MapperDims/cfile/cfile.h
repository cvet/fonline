#ifndef _GAP_FDAT_CFILE_H
#define _GAP_FDAT_CFILE_H

#include <windows.h>
#include "../zlib/zlib.h"
#include "../unlzss/unlzss.h"

using ZLIB::z_stream;

// Since no audio plugin for Fallout performs backward seek and large
// forward moves, we do not need block caching when reading LZ_Block Files.
// If you think blocks are necessary, define USE_LZ_BLOCKS identifier:
// #define USE_LZ_BLOCKS


// size of buffer used during decompression and skipping
#define BUFF_SIZE 0x40000

// abstract class, parent to different File Handlers
class CFile {
protected:
	HANDLE hFile; // archive file descriptor
	long beginPos, fileSize;
		// starting position in archive and size of real file image
public:
	CFile (HANDLE file, long pos, long size):
		hFile (file),
		beginPos (pos),
		fileSize (size) { SetFilePointer (hFile, beginPos, NULL, FILE_BEGIN); };
	virtual ~CFile() {};

	virtual long getSize() { return fileSize; };
	virtual int eof() { return tell() >= fileSize; };
	virtual long seek (long dist, int from) = 0;
	virtual long tell() = 0;
	virtual int read (void* buf, long toRead, long* read) = 0;
};

class CPlainFile: public CFile {
public:
	CPlainFile (HANDLE file, long pos, long size):
		CFile (file, pos, size) {};
	virtual long seek (long dist, int from);
	virtual long tell() { return SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - beginPos; };
	virtual int read (void* buf, long toRead, long* read);
};

class CPackedFile: public CFile {
protected:
	BYTE *skipper,
		*inBuf;
	long packedSize;
	long curPos; // position from beginning of unpacked image
	virtual void skip (long dist);
	virtual void reset() = 0;
public:
	CPackedFile (HANDLE file, long pos, long size, long packed):
		CFile (file, pos, size),
		packedSize (packed),
		curPos (0),
		inBuf (NULL),
		skipper (NULL) {};
	virtual ~CPackedFile() {
		if (skipper) delete[] skipper;
		if (inBuf) delete[] inBuf;
	};

	virtual long seek (long dist, int from);
	virtual long tell() { return curPos; };
};

class C_Z_PackedFile: public CPackedFile {
protected:
	z_stream stream;
public:
	C_Z_PackedFile (HANDLE file, long pos, long size, long packed):
		CPackedFile (file, pos, size, packed) {
			stream. zalloc = Z_NULL;
			stream. zfree = Z_NULL;
			stream. opaque = Z_NULL;
			stream. next_in = Z_NULL;
			stream. avail_in = 0;
			inflateInit (&stream);
	}
	virtual ~C_Z_PackedFile() {
		inflateEnd (&stream);
	};

	virtual int read (void* buf, long toRead, long* read);
protected:
	virtual void reset() {
		inflateReset (&stream);
		SetFilePointer (hFile, beginPos, NULL, FILE_BEGIN);
		curPos = 0;
		stream. next_in = Z_NULL;
		stream. avail_in = 0;
	};
};


struct block {
	long filePos, // position in unpacked image
		archPos; // offset of block in archive file
};

class C_LZ_BlockFile: public CPackedFile {
protected:
	#ifdef USE_LZ_BLOCKS
		int knownBlocks; // count of explored blocks
		int currentBlock;
		block* blocks; // block index
	#endif
	CunLZSS* decompressor;
public:
	C_LZ_BlockFile (HANDLE file, long pos, long size, long packed):
		CPackedFile (file, pos, size, packed)
		#ifdef USE_LZ_BLOCKS
			,
			knownBlocks (0),
			currentBlock (0),
			blocks (NULL)
		#endif
		{decompressor = new CunLZSS();}
	virtual ~C_LZ_BlockFile() {
		#ifdef USE_LZ_BLOCKS
			if (blocks) delete[] blocks;
		#endif
		if (decompressor) delete (decompressor);
	};

	virtual int read (void* buf, long toRead, long* read);
protected:
	virtual void reset() {
		SetFilePointer (hFile, beginPos, NULL, FILE_BEGIN);
		curPos = 0;
		#ifdef USE_LZ_BLOCKS
			currentBlock = 0;
		#endif
		decompressor->clear();
	};
	#ifdef USE_LZ_BLOCKS
		virtual void skip (long dist);
		void allocateBlocks();
	#endif
};

#endif