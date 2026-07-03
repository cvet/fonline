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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

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
    [[nodiscard]] virtual auto GetData(string_view entry_name) const -> vector<uint8_t> = 0;

    virtual void SetString(string_view entry_name, string_view str) = 0;
    virtual void SetData(string_view entry_name, const_span<uint8_t> data) = 0;
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
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8_t> override;

    auto CreateCacheStorage() const -> bool;
    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, const_span<uint8_t> data) override;
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
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8_t> override;

    auto InitCacheStorage() -> bool;
    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, const_span<uint8_t> data) override;
    void RemoveEntry(string_view entry_name) override;

private:
    mutable unique_del_nptr<unqlite> _db {};
    string _workPath {};
};

#endif

static auto CreateCacheStorageBackend(string_view path) -> unique_ptr<CacheStorageImpl>
{
    FO_STACK_TRACE_ENTRY();

#if FO_HAVE_UNQLITE
    if (string_view(path).find("unqlite") != string::npos) {
        return SafeAlloc::MakeUnique<UnqliteCacheStorage>(path);
    }
#endif

    return SafeAlloc::MakeUnique<FileCacheStorage>(path);
}

CacheStorage::CacheStorage(string_view path) :
    _impl {CreateCacheStorageBackend(path)}
{
    FO_STACK_TRACE_ENTRY();
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

auto CacheStorage::GetData(string_view entry_name) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetData(entry_name);
}

void CacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetString(entry_name, str);
}

void CacheStorage::SetData(string_view entry_name, const_span<uint8_t> data)
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

    _workPath = fs_resolve_path(real_path);
}

auto FileCacheStorage::CreateCacheStorage() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_dir(_workPath)) {
        fs_create_directories(_workPath);

        if (!fs_is_dir(_workPath)) {
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
    return fs_exists(path);
}

auto FileCacheStorage::GetString(string_view entry_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto str = fs_read_file(path);

    if (!str) {
        return {};
    }

    return *str;
}

auto FileCacheStorage::GetData(string_view entry_name) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    auto data = fs_read_file(path);

    if (!data) {
        return {};
    }

    return vector<uint8_t>(data->begin(), data->end());
}

void FileCacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    if (!CreateCacheStorage()) {
        return;
    }

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    if (!fs_write_file(path, str)) {
        fs_remove_file(path);
        WriteLog(LogType::Warning, "Can't write cache at '{}'", path);
    }
}

void FileCacheStorage::SetData(string_view entry_name, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    if (!CreateCacheStorage()) {
        return;
    }

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    if (!fs_write_file(path, data)) {
        fs_remove_file(path);
        WriteLog(LogType::Warning, "Can't write cache at '{}'", path);
    }
}

void FileCacheStorage::RemoveEntry(string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    const auto path = MakeCacheEntryPath(_workPath, entry_name);
    fs_remove_file(path);
}

#if FO_HAVE_UNQLITE
UnqliteCacheStorage::UnqliteCacheStorage(string_view real_path)
{
    FO_STACK_TRACE_ENTRY();

    _workPath = fs_resolve_path(real_path);
    _workPath = strex(_workPath).combine_path("Cache.db");

    if (fs_exists(_workPath)) {
        InitCacheStorage();
    }
}

auto UnqliteCacheStorage::InitCacheStorage() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        fs_create_directories(strex(_workPath).extract_dir().str());

        nptr<unqlite> db;
        ptr<const char> work_path = _workPath.c_str();

        if (unqlite_open(db.get_pp(), work_path.get(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK) {
            WriteLog(LogType::Warning, "Can't open unqlite db '{}'", _workPath);
            return false;
        }

        _db = make_unique_del_ptr(db.as_ptr(), [](unqlite* raw_db) noexcept {
            ptr<unqlite> closing_db = raw_db;
            unqlite_close(closing_db.get());
        });
    }

    return true;
}

auto UnqliteCacheStorage::HasEntry(string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return false;
    }

    auto db = _db.as_ptr();
    const string entry_name_text = string(entry_name);
    ptr<const char> entry_name_data = entry_name_text.c_str();
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_text.length());
    const auto r = unqlite_kv_fetch_callback(db.get(), entry_name_data.get(), entry_name_len, [](const void*, unsigned, void*) { return UNQLITE_OK; }, nullptr);

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

    auto db = _db.as_ptr();
    const string entry_name_text = string(entry_name);
    ptr<const char> entry_name_data = entry_name_text.c_str();
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_text.length());
    auto r = unqlite_kv_delete(db.get(), entry_name_data.get(), entry_name_len);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't delete cache entry '{}'", entry_name);
        return;
    }

    if (r == UNQLITE_OK) {
        r = unqlite_commit(db.get());

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

    auto db = _db.as_ptr();
    const string entry_name_text = string(entry_name);
    ptr<const char> entry_name_data = entry_name_text.c_str();
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_text.length());

    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(db.get(), entry_name_data.get(), entry_name_len, nullptr, &size);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }
    if (r == UNQLITE_NOTFOUND) {
        return {};
    }

    string str;
    str.resize(numeric_cast<size_t>(size));

    r = unqlite_kv_fetch(db.get(), entry_name_data.get(), entry_name_len, str.data(), &size);

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }

    return str;
}

auto UnqliteCacheStorage::GetData(string_view entry_name) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return {};
    }

    auto db = _db.as_ptr();
    const string entry_name_text = string(entry_name);
    ptr<const char> entry_name_data = entry_name_text.c_str();
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_text.length());

    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(db.get(), entry_name_data.get(), entry_name_len, nullptr, &size);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }
    if (r == UNQLITE_NOTFOUND) {
        return {};
    }

    vector<uint8_t> data;
    data.resize(numeric_cast<size_t>(size));

    r = unqlite_kv_fetch(db.get(), entry_name_data.get(), entry_name_len, data.data(), &size);

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

    auto db = _db.as_ptr();
    const string entry_name_text = string(entry_name);
    ptr<const char> entry_name_data = entry_name_text.c_str();
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_text.length());
    const string value_text = string(str);
    ptr<const char> value_data = value_text.c_str();
    auto r = unqlite_kv_store(db.get(), entry_name_data.get(), entry_name_len, value_data.get(), numeric_cast<unqlite_int64>(value_text.length()));

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't store cache entry '{}'", entry_name);
        return;
    }

    r = unqlite_commit(db.get());

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't commit stored cache entry '{}'", entry_name);
    }
}

void UnqliteCacheStorage::SetData(string_view entry_name, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    if (!InitCacheStorage()) {
        return;
    }

    auto db = _db.as_ptr();
    const string entry_name_text = string(entry_name);
    ptr<const char> entry_name_data = entry_name_text.c_str();
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_text.length());
    auto r = unqlite_kv_store(db.get(), entry_name_data.get(), entry_name_len, data.data(), numeric_cast<unqlite_int64>(data.size()));

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't store cache entry '{}'", entry_name);
        return;
    }

    r = unqlite_commit(db.get());

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't commit stored cache entry '{}'", entry_name);
    }
}
#endif

FO_END_NAMESPACE
