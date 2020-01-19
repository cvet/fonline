#pragma once

#include "Common.h"

DECLARE_EXCEPTION(CacheStorageException);

class CacheStorage : public NonCopyable
{
public:
    CacheStorage(const string& real_path);
    ~CacheStorage();
    CacheStorage(CacheStorage&&);
    bool IsCache(const string& data_name);
    void EraseCache(const string& data_name);
    void SetCache(const string& data_name, const uchar* data, uint data_len);
    void SetCache(const string& data_name, const string& str);
    void SetCache(const string& data_name, UCharVec& data);
    uchar* GetCache(const string& data_name, uint& data_len);
    string GetCache(const string& data_name);
    bool GetCache(const string& data_name, UCharVec& data);

private:
    struct Impl;
    unique_ptr<Impl> pImpl {};
    string workPath {};
};
