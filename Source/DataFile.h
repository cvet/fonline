#ifndef _DATA_FILE_
#define _DATA_FILE_

// Supports:
//  - Fallout DAT
//  - Zip

#include "Defines.h"

class DataFile
{
public:
	virtual const string& GetPackName() = 0;
	virtual BYTE* OpenFile(const char* fname, DWORD& len) = 0;
	virtual void GetFileNames(const char* path, const char* ext, StrVec& result) = 0;
	virtual void GetTime(FILETIME* create, FILETIME* access, FILETIME* write) = 0;
	virtual ~DataFile(){}
};
typedef vector<DataFile*> DataFileVec;
typedef vector<DataFile*>::iterator DataFileVecIt;

DataFile* OpenDataFile(const char* fname);

#endif // _DATA_FILE_