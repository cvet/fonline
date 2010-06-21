#ifndef _DATFILE_H_
#define _DATFILE_H_

// Added by ABel: Seamless reading from both Fallouts' DAT
#include "cfile/cfile.h"

enum
{
   RES_OK,
   ERR_CANNOT_OPEN_FILE,
   ERR_FILE_TRUNCATED,
   ERR_FILE_NOT_SUPPORTED,
   ERR_ALLOC_MEMORY,
   ERR_FILE_NOT_SUPPORTED2,
};


class compare
{
public:
	bool operator() (const char *s, const char *t)const
	{
		return strcmp(s,t)<0;
	}
};

typedef map<char*, BYTE*, compare> index_map;

struct IndexMap 
{
	index_map index;
};

typedef map<char*, IndexMap*, compare> find_map;

class TDatFile
{
public:

   //index_map index;
   static find_map* fmap;

   char Datname[1024];

   BYTE  FileType; //если там 1, то файл считается компрессированым(не всегда).
   DWORD RealSize; //Размер файла без декомпрессии
   DWORD PackedSize; //Размер сжатого файла
   DWORD Offset; //Адрес файла в виде смещения от начала DAT-файла.

   bool lError;
   UINT ErrorType;

   HANDLE h_in; //Handles: (DAT) files

   BYTE *m_pInBuf;

   ULONG FileSizeFromDat;
   ULONG TreeSize;
   ULONG FilesTotal;

   BYTE* ptr, *buff,*ptr_end;
   //in buff - DATtree, ptr - pointer

   CFile* reader; // reader for current file in DAT-archive

   int ReadTree();
   void IndexingDAT();

   HANDLE DATOpenFile(char* fname);
   bool FindFile(char* fname);

   bool DATReadFile(LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                                                   LPDWORD lpNumberOfBytesRead);
   bool DATSetFilePointer(LONG lDistanceToMove, DWORD dwMoveMethod);
   
   DWORD DATGetFileSize();

   void RevDw(DWORD *addr);
   void ShowError();

   TDatFile(char* filename);
   virtual ~TDatFile();
};


#endif