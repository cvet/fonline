#pragma once

#include "Common.h"

class DataFile : public NonCopyable
{
public:
    class Impl;

    DataFile(const string& path);
    ~DataFile();
    static DataFile* TryLoad(const string& path);
    const string& GetPackName();
    bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time);
    uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time);
    void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result);

private:
    unique_ptr<Impl> pImpl;
};
