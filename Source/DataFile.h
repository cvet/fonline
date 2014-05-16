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
    virtual bool          IsFilePresent( const char* fname, const char* original_fname, uint& size, uint64& write_time ) = 0;
    virtual uchar*        OpenFile( const char* fname, const char* original_fname, uint& size, uint64& write_time ) = 0;
    virtual void          GetFileNames( const char* path, bool include_subdirs, const char* ext, StrVec& result ) = 0;
    virtual ~DataFile() {}
};
typedef vector< DataFile* > DataFileVec;

DataFile* OpenDataFile( const char* fname );

#endif // _DATA_FILE_
