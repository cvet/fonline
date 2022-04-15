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
#include "StringUtils.h"

#if 0
#include "unqlite.h"
#endif

#if 0
struct CacheStorage::Impl
{
    unqlite* Db {};
};

CacheStorage::CacheStorage(string_view real_path)
{
    _workPath = real_path;

    DiskFileSystem::ResolvePath(_workPath);
    DiskFileSystem::MakeDirTree(_workPath);

    unqlite* db = nullptr;
    if (unqlite_open(&db, _workPath.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK) {
        throw CacheStorageException("Can't open unqlite db", _workPath);
    }

    _pImpl = std::make_unique<Impl>();
    _pImpl->Db = db;

    uint ping = 42;
    if (unqlite_kv_store(_pImpl->Db, &ping, sizeof(ping), &ping, sizeof(ping)) != UNQLITE_OK) {
        throw CacheStorageException("Can't store ping unqlite db", _workPath);
    }

    if (unqlite_commit(_pImpl->Db) != UNQLITE_OK) {
        throw CacheStorageException("Can't commit ping unqlite db", _workPath);
    }
}

CacheStorage::~CacheStorage()
{
    if (_pImpl) {
        unqlite_close(_pImpl->Db);
    }
}

CacheStorage::CacheStorage(CacheStorage&&) noexcept = default;

auto CacheStorage::HasEntry(string_view data_name) const -> bool
{
    const auto r = unqlite_kv_fetch_callback(
        _pImpl->Db, data_name.c_str(), static_cast<int>(data_name.length()), [](const void* , unsigned int , void* ) { return UNQLITE_OK; }, nullptr);
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        throw CacheStorageException("Can't fetch cache entry", data_name);
    }

    return r == UNQLITE_OK;
}

void CacheStorage::EraseEntry(string_view data_name)
{
    NON_CONST_METHOD_HINT();

    auto r = unqlite_kv_delete(_pImpl->Db, data_name.c_str(), static_cast<int>(data_name.length()));
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        throw CacheStorageException("Can't delete cache entry", data_name);
    }

    if (r == UNQLITE_OK) {
        r = unqlite_commit(_pImpl->Db);
        if (r != UNQLITE_OK) {
            throw CacheStorageException("Can't commit deleted cache entry", data_name);
        }
    }
}

void CacheStorage::SetRawData(string_view data_name, const uchar* data, uint data_len)
{
    NON_CONST_METHOD_HINT();

    auto r = unqlite_kv_store(_pImpl->Db, data_name.c_str(), static_cast<int>(data_name.length()), data, data_len);
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't store cache entry", data_name);
    }

    r = unqlite_commit(_pImpl->Db);
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't commit stored cache entry", data_name);
    }
}

void CacheStorage::SetRawData(string_view data_name, string_view str)
{
    SetRawData(data_name, !str.empty() ? reinterpret_cast<const uchar*>(str.c_str()) : reinterpret_cast<const uchar*>(""), static_cast<uint>(str.length()));
}

void CacheStorage::SetRawData(string_view data_name, const UCharVec& data)
{
    SetRawData(data_name, !data.empty() ? &data[0] : reinterpret_cast<const uchar*>(""), static_cast<uint>(data.size()));
}

auto CacheStorage::GetData(string_view data_name, uint& data_len) const -> uchar*
{
    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(_pImpl->Db, data_name.c_str(), static_cast<int>(data_name.length()), nullptr, &size);
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        throw CacheStorageException("Can't fetch cache entry", data_name);
    }
    if (r == UNQLITE_NOTFOUND) {
        return nullptr;
    }

    auto* data = new uchar[static_cast<size_t>(size)];
    r = unqlite_kv_fetch(_pImpl->Db, data_name.c_str(), static_cast<int>(data_name.length()), data, &size);
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't fetch cache entry", data_name);
    }

    data_len = static_cast<uint>(size);
    return data;
}

auto CacheStorage::GetData(string_view data_name) const -> string
{
    uint result_len = 0;
    auto* result = GetData(data_name, result_len);
    if (result == nullptr) {
        return "";
    }

    string str;
    if (result_len != 0u) {
        str.resize(result_len);
        std::memcpy(&str[0], result, result_len);
    }

    delete[] result;
    return str;
}

auto CacheStorage::GetData(string_view data_name, UCharVec& data) const -> bool
{
    data.clear();

    uint result_len = 0;
    const auto* result = GetData(data_name, result_len);
    if (result == nullptr) {
        return false;
    }

    data.resize(result_len);
    if (result_len != 0u) {
        std::memcpy(&data[0], result, result_len);
    }

    delete[] result;
    return true;
}
#endif

struct CacheStorage::Impl
{
};

static auto MakeCacheEntryPath(string_view work_path, string_view data_name) -> string
{
    return _str("{}/{}", work_path, _str(data_name).replace('/', '_').replace('\\', '_'));
}

CacheStorage::CacheStorage(string_view real_path)
{
    _workPath = _str(real_path).eraseFileExtension();

    DiskFileSystem::ResolvePath(_workPath);
    DiskFileSystem::MakeDirTree(_workPath);

    auto file = DiskFileSystem::OpenFile(_workPath + "/Ping.txt", true);
    if (!file) {
        throw CacheStorageException("Can't init ping file", _workPath);
    }

    if (!file.Write("Ping", 4u)) {
        throw CacheStorageException("Can't write ping file", _workPath);
    }
}

CacheStorage::~CacheStorage() = default;
CacheStorage::CacheStorage(CacheStorage&&) noexcept = default;

auto CacheStorage::HasEntry(string_view entry_name) const -> bool
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    return !!DiskFileSystem::OpenFile(path, false);
}

auto CacheStorage::GetRawData(string_view entry_name, uint& data_len) const -> uchar*
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, false);
    if (!file) {
        return nullptr;
    }

    data_len = file.GetSize();
    auto* data = new uchar[data_len];
    if (!file.Read(data, data_len)) {
        delete[] data;
        data_len = 0;
        throw CacheStorageException("Can't read cache", path);
    }

    return data;
}

auto CacheStorage::GetString(string_view entry_name) const -> string
{
    auto result_len = 0u;
    auto* result = GetRawData(entry_name, result_len);
    if (result == nullptr) {
        return "";
    }

    string str;
    if (result_len != 0u) {
        str.resize(result_len);
        std::memcpy(&str[0], result, result_len);
    }

    delete[] result;
    return str;
}

auto CacheStorage::GetData(string_view entry_name) const -> vector<uchar>
{
    vector<uchar> data;

    auto result_len = 0u;
    auto* result = GetRawData(entry_name, result_len);
    if (result == nullptr) {
        return data;
    }

    if (result_len != 0u) {
        data.resize(result_len);
        std::memcpy(&data[0], result, result_len);
    }

    delete[] result;
    return data;
}

void CacheStorage::SetRawData(string_view entry_name, const uchar* data, uint data_len)
{
    NON_CONST_METHOD_HINT();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, true);
    if (!file) {
        throw CacheStorageException("Can't write cache", path);
    }

    if (!file.Write(data, data_len)) {
        DiskFileSystem::DeleteFile(path);
        throw CacheStorageException("Can't write cache", path);
    }
}

void CacheStorage::SetString(string_view entry_name, string_view str)
{
    SetRawData(entry_name, !str.empty() ? reinterpret_cast<const uchar*>(str.data()) : reinterpret_cast<const uchar*>(""), static_cast<uint>(str.length()));
}

void CacheStorage::SetData(string_view entry_name, const vector<uchar>& data)
{
    SetRawData(entry_name, !data.empty() ? &data[0] : reinterpret_cast<const uchar*>(""), static_cast<uint>(data.size()));
}

void CacheStorage::EraseEntry(string_view entry_name)
{
    NON_CONST_METHOD_HINT();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    DiskFileSystem::DeleteFile(path);
}
