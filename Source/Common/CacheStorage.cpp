//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "CacheStorage.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "StringUtils.h"
#include "Testing.h"

#ifndef FO_WEB
#include "unqlite.h"
#endif

#ifndef FO_WEB
struct CacheStorage::Impl
{
    unqlite* Db {};
};

CacheStorage::CacheStorage(const string& real_path)
{
    workPath = real_path;
    DiskFileSystem::ResolvePath(workPath);
    DiskFileSystem::MakeDirTree(workPath);

    unqlite* db = nullptr;
    if (unqlite_open(&db, workPath.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK)
        throw CacheStorageException("Can't open unqlite db", workPath);

    pImpl = std::make_unique<Impl>();
    pImpl->Db = db;

    uint ping = 42;
    if (unqlite_kv_store(pImpl->Db, &ping, sizeof(ping), &ping, sizeof(ping)) != UNQLITE_OK)
        throw CacheStorageException("Can't store ping unqlite db", workPath);

    if (unqlite_commit(pImpl->Db) != UNQLITE_OK)
        throw CacheStorageException("Can't commit ping unqlite db", workPath);
}

CacheStorage::~CacheStorage()
{
    if (pImpl)
        unqlite_close(pImpl->Db);
}

CacheStorage::CacheStorage(CacheStorage&&)
{
}

bool CacheStorage::IsCache(const string& data_name)
{
    int r = unqlite_kv_fetch_callback(
        pImpl->Db, data_name.c_str(), (int)data_name.length(),
        [](const void*, unsigned int, void*) { return UNQLITE_OK; }, nullptr);
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND)
        throw CacheStorageException("Can't fetch cache entry", data_name);

    return r == UNQLITE_OK;
}

void CacheStorage::EraseCache(const string& data_name)
{
    int r = unqlite_kv_delete(pImpl->Db, data_name.c_str(), (int)data_name.length());
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND)
        throw CacheStorageException("Can't delete cache entry", data_name);

    if (r == UNQLITE_OK)
    {
        r = unqlite_commit(pImpl->Db);
        if (r != UNQLITE_OK)
            throw CacheStorageException("Can't commit deleted cache entry", data_name);
    }
}

void CacheStorage::SetCache(const string& data_name, const uchar* data, uint data_len)
{
    int r = unqlite_kv_store(pImpl->Db, data_name.c_str(), (int)data_name.length(), data, data_len);
    if (r != UNQLITE_OK)
        throw CacheStorageException("Can't store cache entry", data_name);

    r = unqlite_commit(pImpl->Db);
    if (r != UNQLITE_OK)
        throw CacheStorageException("Can't commit stored cache entry", data_name);
}

void CacheStorage::SetCache(const string& data_name, const string& str)
{
    SetCache(data_name, !str.empty() ? (uchar*)str.c_str() : (uchar*)"", (uint)str.length());
}

void CacheStorage::SetCache(const string& data_name, UCharVec& data)
{
    SetCache(data_name, !data.empty() ? (uchar*)&data[0] : (uchar*)"", (uint)data.size());
}

uchar* CacheStorage::GetCache(const string& data_name, uint& data_len)
{
    unqlite_int64 size;
    int r = unqlite_kv_fetch(pImpl->Db, data_name.c_str(), (int)data_name.length(), nullptr, &size);
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND)
        throw CacheStorageException("Can't fetch cache entry", data_name);

    if (r == UNQLITE_NOTFOUND)
        return nullptr;

    uchar* data = new uchar[(uint)size];
    r = unqlite_kv_fetch(pImpl->Db, data_name.c_str(), (int)data_name.length(), data, &size);
    if (r != UNQLITE_OK)
        throw CacheStorageException("Can't fetch cache entry", data_name);

    data_len = (uint)size;
    return data;
}

string CacheStorage::GetCache(const string& data_name)
{
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return "";

    string str;
    if (result_len)
    {
        str.resize(result_len);
        memcpy(&str[0], result, result_len);
    }
    delete[] result;
    return str;
}

bool CacheStorage::GetCache(const string& data_name, UCharVec& data)
{
    data.clear();
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return false;

    data.resize(result_len);
    if (result_len)
        memcpy(&data[0], result, result_len);
    delete[] result;
    return true;
}

#else
struct CacheStorage::Impl
{
};

static string MakeCacheEntryPath(const string& work_path, const string& data_name)
{
    return _str("{}{}", work_path, _str(data_name).replace('/', '_').replace('\\', '_'));
}

CacheStorage::CacheStorage(const string& real_path)
{
    workPath = _str(real_path).eraseFileExtension() + "/";
    DiskFileSystem::ResolvePath(workPath);
    DiskFileSystem::MakeDirTree(workPath);

    DiskFile file = DiskFileSystem::OpenFile(workPath + "Ping.txt", true);
    if (!file)
        throw CacheStorageException("Can't init ping file", workPath);

    if (!file.Write("Ping", 4))
        throw CacheStorageException("Can't write ping file", workPath);
}

CacheStorage::~CacheStorage()
{
}

CacheStorage::CacheStorage(CacheStorage&&)
{
}

bool CacheStorage::IsCache(const string& data_name)
{
    string path = MakeCacheEntryPath(workPath, data_name);
    return !!DiskFileSystem::OpenFile(path, false);
}

void CacheStorage::EraseCache(const string& data_name)
{
    string path = MakeCacheEntryPath(workPath, data_name);
    DiskFileSystem::DeleteFile(path);
}

void CacheStorage::SetCache(const string& data_name, const uchar* data, uint data_len)
{
    string path = MakeCacheEntryPath(workPath, data_name);
    DiskFile file = DiskFileSystem::OpenFile(path, true);
    if (!file)
        throw CacheStorageException("Can't write cache", path);

    if (!file.Write(data, data_len))
    {
        DiskFileSystem::DeleteFile(path);
        throw CacheStorageException("Can't write cache", path);
    }
}

void CacheStorage::SetCache(const string& data_name, const string& str)
{
    SetCache(data_name, !str.empty() ? (uchar*)str.c_str() : (uchar*)"", (uint)str.length());
}

void CacheStorage::SetCache(const string& data_name, UCharVec& data)
{
    SetCache(data_name, !data.empty() ? (uchar*)&data[0] : (uchar*)"", (uint)data.size());
}

uchar* CacheStorage::GetCache(const string& data_name, uint& data_len)
{
    string path = MakeCacheEntryPath(workPath, data_name);
    DiskFile file = DiskFileSystem::OpenFile(path, false);
    if (!file)
        return nullptr;

    data_len = file.GetSize();
    uchar* data = new uchar[data_len];
    if (!file.Read(data, data_len))
    {
        delete[] data;
        data_len = 0;
        throw CacheStorageException("Can't read cache", path);
    }

    return data;
}

string CacheStorage::GetCache(const string& data_name)
{
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return "";

    string str;
    if (result_len)
    {
        str.resize(result_len);
        memcpy(&str[0], result, result_len);
    }
    delete[] result;
    return str;
}

bool CacheStorage::GetCache(const string& data_name, UCharVec& data)
{
    data.clear();
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return false;

    data.resize(result_len);
    if (result_len)
        memcpy(&data[0], result, result_len);
    delete[] result;
    return true;
}
#endif
