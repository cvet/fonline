#ifndef _DATFILE_H_
#define _DATFILE_H_

// Added by ABel: Seamless reading from both Fallouts' DAT
#include "cfile.h"
#include <windef.h>
#include <map>
#include <vector>
#include <string>

enum
{
   RES_OK,
   ERR_CANNOT_OPEN_FILE,
   ERR_FILE_TRUNCATED,
   ERR_FILE_NOT_SUPPORTED,
   ERR_ALLOC_MEMORY,
   ERR_FILE_NOT_SUPPORTED2,
};

typedef std::map<std::string,BYTE*> IndexMap;
typedef std::map<std::string,BYTE*>::iterator IndexMapIt;
typedef std::map<std::string,BYTE*>::value_type IndexMapVal;

class TDatFile
{
public:
	IndexMap Index;
	std::string Datname;

	bool IsError;
	DWORD ErrorType;

	BYTE* Tree;
	CFile* Reader; // Reader for current file in DAT-archive
	HANDLE DatHandle; // Handles of DAT file
	ULONG FileSizeFromDat;
	ULONG FilesTotal;
	ULONG TreeSize;

	// Result of FindFile
	BYTE  FileType; // Если там 1, то файл считается компрессированым(не всегда).
	DWORD RealSize; // Размер файла без декомпрессии
	DWORD PackedSize; // Размер сжатого файла
	DWORD Offset; // Адрес файла в виде смещения от начала DAT-файла.

	int ReadTree();
	void IndexingDAT();
	HANDLE DATOpenFile(const char* fname);
	bool FindFile(const char* fname);
	bool DATReadFile(LPVOID lpBuffer, DWORD nNumberOfBytesToRead);
	bool DATSetFilePointer(LONG lDistanceToMove, DWORD dwMoveMethod);
	DWORD DATGetFileSize();
	void RevDw(DWORD* addr);
	static const char* GetError(int err);

	void GetFileNames(const char* path, const char* ext, std::vector<std::string>& result); // Note: include subdirs

	TDatFile(const char* filename);
	~TDatFile();
};

typedef std::vector<TDatFile*> TDatFilePtrVec;
typedef std::vector<TDatFile*>::iterator TDatFilePtrVecIt;

#endif