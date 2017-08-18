#ifndef _DATA_FILE_
#define _DATA_FILE_

// Support:
// - Plane folder
// - Fallout DAT
// - Zip

#include "Defines.h"

class DataFile
{
public:
    virtual const string& GetPackName() = 0;
    virtual bool          IsFilePresent( const string& path, const string& path_lower, uint& size, uint64& write_time ) = 0;
    virtual uchar*        OpenFile( const string& path, const string& path_lower, uint& size, uint64& write_time ) = 0;
    virtual void          GetFileNames( const string& path, bool include_subdirs, const string& ext, StrVec& result ) = 0;
    virtual ~DataFile() = default;
};
typedef vector< DataFile* > DataFileVec;

DataFile* OpenDataFile( const string& fname );

#endif // _DATA_FILE_
