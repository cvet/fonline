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
	virtual uchar* OpenFile(const char* fname, uint& len) = 0;
	virtual void GetFileNames(const char* path, bool include_subdirs, const char* ext, StrVec& result) = 0;
	virtual void GetTime(FILETIME* create, FILETIME* access, FILETIME* write) = 0;
	virtual ~DataFile(){}
};
typedef vector<DataFile*> DataFileVec;
typedef vector<DataFile*>::iterator DataFileVecIt;

DataFile* OpenDataFile(const char* fname);

#endif // _DATA_FILE_