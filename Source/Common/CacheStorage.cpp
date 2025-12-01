//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#if FO_HAVE_UNQLITE
#include "unqlite.h"
#endif

FO_BEGIN_NAMESPACE();

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
    [[nodiscard]] virtual auto GetData(string_view entry_name) const -> vector<uint8> = 0;

    virtual void SetString(string_view entry_name, string_view str) = 0;
    virtual void SetData(string_view entry_name, span<const uint8> data) = 0;
    virtual void RemoveEntry(string_view entry_name) = 0;
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
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8> override;

    auto CreateCacheStorage() const -> bool;
    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, span<const uint8> data) override;
    void RemoveEntry(string_view entry_name) override;

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
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8> override;

    auto InitCacheStorage() -> bool;
    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, span<const uint8> data) override;
    void RemoveEntry(string_view entry_name) override;

private:
    unique_del_ptr<unqlite> _db {};
    string _workPath {};
};
#endif

CacheStorage::CacheStorage(string_view path)
{
#if FO_HAVE_UNQLITE
    // Todo: add engine hook to allow user to override cache storage
    if (string_view(path).find("unqlite") != string::npos) {
        _impl = SafeAlloc::MakeUnique<UnqliteCacheStorage>(path);
    }
#endif

    if (!_impl) {
        _impl = SafeAlloc::MakeUnique<FileCacheStorage>(path);
    }
}

CacheStorage::CacheStorage(CacheStorage&&) noexcept = default;
CacheStorage::~CacheStorage() = default;

auto CacheStorage::HasEntry(string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _impl->HasEntry(entry_name);
}

auto CacheStorage::GetString(string_view entry_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetString(entry_name);
}

auto CacheStorage::GetData(string_view entry_name) const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetData(entry_name);
}

void CacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetString(entry_name, str);
}

void CacheStorage::SetData(string_view entry_name, span<const uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetData(entry_name, data);
}

void CacheStorage::RemoveEntry(string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    _impl->RemoveEntry(entry_name);
}

auto FileCacheStorage::MakeCacheEntryPath(string_view work_path, string_view data_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(work_path).combine_path(strex(data_name).replace('/', '_').replace('\\', '_'));
}

FileCacheStorage::FileCacheStorage(string_view real_path)
{
    FO_STACK_TRACE_ENTRY();

    _workPath = DiskFileSystem::ResolvePath(real_path);
}

auto FileCacheStorage::CreateCacheStorage() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!DiskFileSystem::IsDir(_workPath)) {
        DiskFileSystem::MakeDirTree(_workPath);

        if (!DiskFileSystem::IsDir(_workPath)) {
            WriteLog(LogType::Warning, "Can't create dir for cache '{}'", _workPath);
            return false;
        }
    }

    return true;
}

auto FileCacheStorage::HasEntry(string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    return !!DiskFileSystem::OpenFile(path, false);
}

auto FileCacheStorage::GetString(string_view entry_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, false);

    if (!file) {
        return {};
    }

    string str;
    str.resize(file.GetSize());

    if (!file.Read(str.data(), str.length())) {
        WriteLog(LogType::Warning, "Can't read cache at '{}'", path);
        return {};
    }

    return str;
}

auto FileCacheStorage::GetData(string_view entry_name) const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, false);

    if (!file) {
        return {};
    }

    vector<uint8> data(file.GetSize());

    if (!file.Read(data.data(), data.size())) {
        WriteLog(LogType::Warning, "Can't read cache at '{}'", path);
        return {};
    }

    return data;
}

void FileCacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    if (!CreateCacheStorage()) {
        return;
    }

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, true);

    if (!file) {
        WriteLog(LogType::Warning, "Can't open write cache at '{}'", path);
        return;
    }

    if (!file.Write(str)) {
        DiskFileSystem::DeleteFile(path);
        WriteLog(LogType::Warning, "Can't write cache at '{}'", path);
    }
}

void FileCacheStorage::SetData(string_view entry_name, span<const uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    if (!CreateCacheStorage()) {
        return;
    }

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto file = DiskFileSystem::OpenFile(path, true);

    if (!file) {
        WriteLog(LogType::Warning, "Can't open write cache at '{}'", path);
        return;
    }

    if (!file.Write(data)) {
        DiskFileSystem::DeleteFile(path);
        WriteLog(LogType::Warning, "Can't write cache at '{}'", path);
    }
}

void FileCacheStorage::RemoveEntry(string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    DiskFileSystem::DeleteFile(path);
}

#if FO_HAVE_UNQLITE
UnqliteCacheStorage::UnqliteCacheStorage(string_view real_path)
{
    FO_STACK_TRACE_ENTRY();

    _workPath = DiskFileSystem::ResolvePath(real_path);
    _workPath = strex(_workPath).combine_path("Cache.db");

    if (DiskFileSystem::IsExists(_workPath)) {
        InitCacheStorage();
    }
}

auto UnqliteCacheStorage::InitCacheStorage() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        DiskFileSystem::MakeDirTree(strex(_workPath).extract_dir());

        unqlite* db = nullptr;

        if (unqlite_open(&db, _workPath.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK) {
            WriteLog(LogType::Warning, "Can't open unqlite db '{}'", _workPath);
            return false;
        }

        _db = unique_del_ptr<unqlite> {db, [](unqlite* del_db) { unqlite_close(del_db); }};
    }

    return true;
}

auto UnqliteCacheStorage::HasEntry(string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return false;
    }

    auto* db_non_const = const_cast<unqlite*>(_db.get());
    const auto r = unqlite_kv_fetch_callback(db_non_const, entry_name.data(), numeric_cast<int32>(entry_name.length()), [](const void*, unsigned, void*) { return UNQLITE_OK; }, nullptr);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return false;
    }

    return r == UNQLITE_OK;
}

void UnqliteCacheStorage::RemoveEntry(string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return;
    }

    auto r = unqlite_kv_delete(_db.get(), entry_name.data(), numeric_cast<int32>(entry_name.length()));

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't delete cache entry '{}'", entry_name);
        return;
    }

    if (r == UNQLITE_OK) {
        r = unqlite_commit(_db.get());

        if (r != UNQLITE_OK) {
            WriteLog(LogType::Warning, "Can't commit deleted cache entry '{}'", entry_name);
        }
    }
}

auto UnqliteCacheStorage::GetString(string_view entry_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return {};
    }

    auto* db_non_const = const_cast<unqlite*>(_db.get());

    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(db_non_const, entry_name.data(), numeric_cast<int32>(entry_name.length()), nullptr, &size);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }
    if (r == UNQLITE_NOTFOUND) {
        return {};
    }

    string str;
    str.resize(numeric_cast<size_t>(size));

    r = unqlite_kv_fetch(db_non_const, entry_name.data(), numeric_cast<int32>(entry_name.length()), str.data(), &size);

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }

    return str;
}

auto UnqliteCacheStorage::GetData(string_view entry_name) const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return {};
    }

    auto* db_non_const = const_cast<unqlite*>(_db.get());

    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(db_non_const, entry_name.data(), numeric_cast<int32>(entry_name.length()), nullptr, &size);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }
    if (r == UNQLITE_NOTFOUND) {
        return {};
    }

    vector<uint8> data;
    data.resize(numeric_cast<size_t>(size));

    r = unqlite_kv_fetch(db_non_const, entry_name.data(), numeric_cast<int32>(entry_name.length()), data.data(), &size);

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }

    return data;
}

void UnqliteCacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    if (!InitCacheStorage()) {
        return;
    }

    auto r = unqlite_kv_store(_db.get(), entry_name.data(), numeric_cast<int32>(entry_name.length()), str.data(), numeric_cast<unqlite_int64>(str.length()));

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't store cache entry '{}'", entry_name);
        return;
    }

    r = unqlite_commit(_db.get());

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't commit stored cache entry '{}'", entry_name);
    }
}

void UnqliteCacheStorage::SetData(string_view entry_name, span<const uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    if (!InitCacheStorage()) {
        return;
    }

    auto r = unqlite_kv_store(_db.get(), entry_name.data(), numeric_cast<int32>(entry_name.length()), data.data(), numeric_cast<unqlite_int64>(data.size()));

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't store cache entry '{}'", entry_name);
        return;
    }

    r = unqlite_commit(_db.get());

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't commit stored cache entry '{}'", entry_name);
    }
}
#endif

FO_END_NAMESPACE();
