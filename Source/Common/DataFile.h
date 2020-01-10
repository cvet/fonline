#pragma once

#include "Common.h"

class IDataFile
{
public:
    virtual const string& GetPackName() = 0;
    virtual bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time) = 0;
    virtual uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time) = 0;
    virtual void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result) = 0;
    virtual ~IDataFile() = default;
};
using DataFile = std::shared_ptr<IDataFile>;
using DataFileVec = vector<DataFile>;

namespace Fabric
{
    DataFile OpenDataFile(const string& path);
}
