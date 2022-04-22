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

#if FO_HAVE_UNQLITE
#include "unqlite.h"
#endif

class CacheStorageImpl
{
public:
    CacheStorageImpl() = default;
    CacheStorageImpl(const CacheStorageImpl&) = delete;
    CacheStorageImpl(CacheStorageImpl&&) noexcept = delete;
    auto operator=(const CacheStorageImpl&) = delete;
    auto operator=(CacheStorageImpl&&) noexcept = delete;
    virtual ~CacheStorageImpl() = default;

    [[nodiscard]] virtual auto HasEntry(string_view entry_name) const -> bool = 0;
    [[nodiscard]] virtual auto GetString(string_view entry_name) const -> string = 0;
    [[nodiscard]] virtual auto GetData(string_view entry_name) const -> vector<uchar> = 0;

    virtual void SetString(string_view entry_name, string_view str) = 0;
    virtual void SetData(string_view entry_name, const_span<uchar> data) = 0;
    virtual void EraseEntry(string_view entry_name) = 0;
};

class FileCacheStorage final : public CacheStorageImpl
{
public:
    explicit FileCacheStorage(string_view real_path);
    FileCacheStorage(const FileCacheStorage&) = delete;
    FileCacheStorage(FileCacheStorage&&) noexcept = delete;
    auto operator=(const FileCacheStorage&) = delete;
    auto operator=(FileCacheStorage&&) noexcept = delete;
    ~FileCacheStorage() override = default;

    [[nodiscard]] auto HasEntry(string_view entry_name) const -> bool override;
    [[nodiscard]] auto GetString(string_view entry_name) const -> string override;
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uchar> override;

    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, const_span<uchar> data) override;
    void EraseEntry(string_view entry_name) override;

private:
    [[nodiscard]] auto MakeCacheEntryPath(string_view work_path, string_view data_name) const -> string;

    string _workPath {};
};

#if FO_HAVE_UNQLITE
class UnqliteCacheStorage final : public CacheStorageImpl
{
public:
    explicit UnqliteCacheStorage(string_view real_path);
    UnqliteCacheStorage(const UnqliteCacheStorage&) = delete;
    UnqliteCacheStorage(UnqliteCacheStorage&&) noexcept = delete;
    auto operator=(const UnqliteCacheStorage&) = delete;
    auto operator=(UnqliteCacheStorage&&) noexcept = delete;
    ~UnqliteCacheStorage() override = default;

    [[nodiscard]] auto HasEntry(string_view entry_name) const -> bool override;
    [[nodiscard]] auto GetString(string_view entry_name) const -> string override;
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uchar> override;

    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, const_span<uchar> data) override;
    void EraseEntry(string_view entry_name) override;

private:
    unique_ptr<unqlite, std::function<void(unqlite*)>> _db {};
    string _workPath {};
};
#endif

CacheStorage::CacheStorage(string_view path)
{
#if FO_HAVE_UNQLITE
    _impl = std::make_unique<UnqliteCacheStorage>(path);
#else
    _impl = std::make_unique<FileCacheStorage>(path);
#endif
}

CacheStorage::CacheStorage(CacheStorage&&) noexcept = default;
CacheStorage::~CacheStorage() = default;

auto CacheStorage::HasEntry(string_view entry_name) const -> bool
{
    return _impl->HasEntry(entry_name);
}

auto CacheStorage::GetString(string_view entry_name) const -> string
{
    return _impl->GetString(entry_name);
}
auto CacheStorage::GetData(string_view entry_name) const -> vector<uchar>
{
    return _impl->GetData(entry_name);
}

void CacheStorage::SetString(string_view entry_name, string_view str)
{
    NON_CONST_METHOD_HINT();

    _impl->SetString(entry_name, str);
}

void CacheStorage::SetData(string_view entry_name, const_span<uchar> data)
{
    NON_CONST_METHOD_HINT();

    _impl->SetData(entry_name, data);
}

void CacheStorage::EraseEntry(string_view entry_name)
{
    NON_CONST_METHOD_HINT();

    _impl->EraseEntry(entry_name);
}

auto FileCacheStorage::MakeCacheEntryPath(string_view work_path, string_view data_name) const -> string
{
    return _str("{}/{}", work_path, _str(data_name).replace('/', '_').replace('\\', '_'));
}

FileCacheStorage::FileCacheStorage(string_view real_path)
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

auto FileCacheStorage::HasEntry(string_view entry_name) const -> bool
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    return !!DiskFileSystem::OpenFile(path, false);
}

auto FileCacheStorage::GetString(string_view entry_name) const -> string
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, false);
    if (!file) {
        return {};
    }

    string str;
    str.resize(file.GetSize());

    if (!file.Read(str.data(), str.length())) {
        throw CacheStorageException("Can't read cache", path);
    }

    return str;
}

auto FileCacheStorage::GetData(string_view entry_name) const -> vector<uchar>
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, false);
    if (!file) {
        return {};
    }

    vector<uchar> data(file.GetSize());

    if (!file.Read(data.data(), data.size())) {
        throw CacheStorageException("Can't read cache", path);
    }

    return data;
}

void FileCacheStorage::SetString(string_view entry_name, string_view str)
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, true);
    if (!file) {
        throw CacheStorageException("Can't write cache", path);
    }

    if (!file.Write(str)) {
        DiskFileSystem::DeleteFile(path);
        throw CacheStorageException("Can't write cache", path);
    }
}

void FileCacheStorage::SetData(string_view entry_name, const_span<uchar> data)
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, true);
    if (!file) {
        throw CacheStorageException("Can't write cache", path);
    }

    if (!file.Write(data)) {
        DiskFileSystem::DeleteFile(path);
        throw CacheStorageException("Can't write cache", path);
    }
}

void FileCacheStorage::EraseEntry(string_view entry_name)
{
    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    DiskFileSystem::DeleteFile(path);
}

#if FO_HAVE_UNQLITE
UnqliteCacheStorage::UnqliteCacheStorage(string_view real_path)
{
    _workPath = real_path;

    DiskFileSystem::ResolvePath(_workPath);
    DiskFileSystem::MakeDirTree(_workPath);

    unqlite* db = nullptr;
    if (unqlite_open(&db, _workPath.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK) {
        throw CacheStorageException("Can't open unqlite db", _workPath);
    }

    _db = {db, [](unqlite* del_db) { unqlite_close(del_db); }};

    constexpr uint ping = 42;
    if (unqlite_kv_store(_db.get(), &ping, sizeof(ping), &ping, sizeof(ping)) != UNQLITE_OK) {
        throw CacheStorageException("Can't store ping unqlite db", _workPath);
    }

    if (unqlite_commit(_db.get()) != UNQLITE_OK) {
        throw CacheStorageException("Can't commit ping unqlite db", _workPath);
    }
}

auto UnqliteCacheStorage::HasEntry(string_view entry_name) const -> bool
{
    const auto r = unqlite_kv_fetch_callback(
        _db.get(), entry_name.data(), static_cast<int>(entry_name.length()), [](const void*, unsigned int, void*) { return UNQLITE_OK; }, nullptr);
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        throw CacheStorageException("Can't fetch cache entry", entry_name);
    }

    return r == UNQLITE_OK;
}

void UnqliteCacheStorage::EraseEntry(string_view entry_name)
{
    auto r = unqlite_kv_delete(_db.get(), entry_name.data(), static_cast<int>(entry_name.length()));
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        throw CacheStorageException("Can't delete cache entry", entry_name);
    }

    if (r == UNQLITE_OK) {
        r = unqlite_commit(_db.get());
        if (r != UNQLITE_OK) {
            throw CacheStorageException("Can't commit deleted cache entry", entry_name);
        }
    }
}

auto UnqliteCacheStorage::GetString(string_view entry_name) const -> string
{
    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(_db.get(), entry_name.data(), static_cast<int>(entry_name.length()), nullptr, &size);
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        throw CacheStorageException("Can't fetch cache entry", entry_name);
    }
    if (r == UNQLITE_NOTFOUND) {
        return {};
    }

    string str;
    str.resize(size);

    r = unqlite_kv_fetch(_db.get(), entry_name.data(), static_cast<int>(entry_name.length()), str.data(), &size);
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't fetch cache entry", entry_name);
    }

    return str;
}

auto UnqliteCacheStorage::GetData(string_view entry_name) const -> vector<uchar>
{
    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(_db.get(), entry_name.data(), static_cast<int>(entry_name.length()), nullptr, &size);
    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        throw CacheStorageException("Can't fetch cache entry", entry_name);
    }
    if (r == UNQLITE_NOTFOUND) {
        return {};
    }

    vector<uchar> data;
    data.resize(size);

    r = unqlite_kv_fetch(_db.get(), entry_name.data(), static_cast<int>(entry_name.length()), data.data(), &size);
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't fetch cache entry", entry_name);
    }

    return data;
}

void UnqliteCacheStorage::SetString(string_view entry_name, string_view str)
{
    auto r = unqlite_kv_store(_db.get(), entry_name.data(), static_cast<int>(entry_name.length()), str.data(), static_cast<unqlite_int64>(str.length()));
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't store cache entry", entry_name);
    }

    r = unqlite_commit(_db.get());
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't commit stored cache entry", entry_name);
    }
}

void UnqliteCacheStorage::SetData(string_view entry_name, const_span<uchar> data)
{
    auto r = unqlite_kv_store(_db.get(), entry_name.data(), static_cast<int>(entry_name.length()), data.data(), static_cast<unqlite_int64>(data.size()));
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't store cache entry", entry_name);
    }

    r = unqlite_commit(_db.get());
    if (r != UNQLITE_OK) {
        throw CacheStorageException("Can't commit stored cache entry", entry_name);
    }
}
#endif
