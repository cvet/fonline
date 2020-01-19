#pragma once

#include "Common.h"

DECLARE_EXCEPTION(DataSourceException);

class DataSource : public Pointable
{
public:
    class Impl;

    DataSource(const string& path);
    ~DataSource();
    DataSource(DataSource&&);
    bool IsDiskDir();
    const string& GetPackName();
    bool IsFilePresent(const string& path, const string& path_lower, uint& size, uint64& write_time);
    uchar* OpenFile(const string& path, const string& path_lower, uint& size, uint64& write_time);
    void GetFileNames(const string& path, bool include_subdirs, const string& ext, StrVec& result);

private:
    unique_ptr<Impl> pImpl {};
};
