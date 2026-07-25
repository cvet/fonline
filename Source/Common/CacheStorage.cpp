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

    [[nodiscard]] virtual auto HasEntry(u8string_view entry_name) const -> bool = 0;
    [[nodiscard]] virtual auto GetBytes(u8string_view entry_name) const -> vector<byte> = 0;

    virtual void SetBytes(u8string_view entry_name, const_span<byte> bytes) = 0;
    virtual void RemoveEntry(u8string_view entry_name) = 0;
};

class FileCacheStorage final : public CacheStorageImpl
{
public:
    explicit FileCacheStorage(u8string_view real_path);
    FileCacheStorage(const FileCacheStorage&) = delete;
    FileCacheStorage(FileCacheStorage&&) noexcept = delete;
    auto operator=(const FileCacheStorage&) = delete;
    auto operator=(FileCacheStorage&&) noexcept = delete;
    ~FileCacheStorage() override = default;

    [[nodiscard]] auto HasEntry(u8string_view entry_name) const -> bool override;
    [[nodiscard]] auto GetBytes(u8string_view entry_name) const -> vector<byte> override;

    auto CreateCacheStorage() const -> bool;
    void SetBytes(u8string_view entry_name, const_span<byte> bytes) override;
    void RemoveEntry(u8string_view entry_name) override;

private:
    [[nodiscard]] auto MakeCacheEntryPath(u8string_view work_path, u8string_view data_name) const -> u8string;

    u8string _workPath {};
};

#if FO_HAVE_UNQLITE
class UnqliteCacheStorage final : public CacheStorageImpl
{
public:
    explicit UnqliteCacheStorage(u8string_view real_path);
    UnqliteCacheStorage(const UnqliteCacheStorage&) = delete;
    UnqliteCacheStorage(UnqliteCacheStorage&&) noexcept = delete;
    auto operator=(const UnqliteCacheStorage&) = delete;
    auto operator=(UnqliteCacheStorage&&) noexcept = delete;
    ~UnqliteCacheStorage() override = default;

    [[nodiscard]] auto HasEntry(u8string_view entry_name) const -> bool override;
    [[nodiscard]] auto GetBytes(u8string_view entry_name) const -> vector<byte> override;

    auto InitCacheStorage() -> bool;
    void SetBytes(u8string_view entry_name, const_span<byte> bytes) override;
    void RemoveEntry(u8string_view entry_name) override;

private:
    mutable unique_del_nptr<unqlite> _db {};
    u8string _workPath {};
};

#endif

static auto CreateCacheStorageBackend(u8string_view path) -> unique_ptr<CacheStorageImpl>
{
    FO_STACK_TRACE_ENTRY();

#if FO_HAVE_UNQLITE
    if (path.native_view().find(u8"unqlite") != std::u8string_view::npos) {
        return SafeAlloc::MakeUnique<UnqliteCacheStorage>(path);
    }
#endif

    return SafeAlloc::MakeUnique<FileCacheStorage>(path);
}

CacheStorage::CacheStorage(u8string_view path) :
    _impl {CreateCacheStorageBackend(path)}
{
    FO_STACK_TRACE_ENTRY();
}

CacheStorage::CacheStorage(u8string path) :
    CacheStorage {path.view()}
{
    FO_STACK_TRACE_ENTRY();
}

CacheStorage::CacheStorage(CacheStorage&&) noexcept = default;
CacheStorage::~CacheStorage() = default;

auto CacheStorage::HasEntry(string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_entry_name = entry_name;
    return HasEntry(utf8_entry_name);
}

auto CacheStorage::HasEntry(u8string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _impl->HasEntry(entry_name);
}

auto CacheStorage::GetText(string_view entry_name) const -> u8string
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_entry_name = entry_name;
    return GetText(utf8_entry_name);
}

auto CacheStorage::GetText(u8string_view entry_name) const -> u8string
{
    FO_STACK_TRACE_ENTRY();

    const vector<byte> bytes = _impl->GetBytes(entry_name);
    return utf8_from_byte_span(bytes);
}

auto CacheStorage::GetBytes(string_view entry_name) const -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_entry_name = entry_name;
    return GetBytes(utf8_entry_name);
}

auto CacheStorage::GetBytes(u8string_view entry_name) const -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetBytes(entry_name);
}

void CacheStorage::SetText(string_view entry_name, u8string_view text)
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_entry_name = entry_name;
    SetText(utf8_entry_name, text);
}

void CacheStorage::SetText(u8string_view entry_name, u8string_view text)
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_utf8_text(text.native_view())) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    _impl->SetBytes(entry_name, utf8_to_byte_span(text));
}

void CacheStorage::SetBytes(string_view entry_name, const_span<byte> bytes)
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_entry_name = entry_name;
    SetBytes(utf8_entry_name, bytes);
}

void CacheStorage::SetBytes(u8string_view entry_name, const_span<byte> bytes)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetBytes(entry_name, bytes);
}

void CacheStorage::RemoveEntry(string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_entry_name = entry_name;
    RemoveEntry(utf8_entry_name);
}

void CacheStorage::RemoveEntry(u8string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    _impl->RemoveEntry(entry_name);
}

auto FileCacheStorage::MakeCacheEntryPath(u8string_view work_path, u8string_view data_name) const -> u8string
{
    FO_STACK_TRACE_ENTRY();

    const u8string safe_name = u8strex(data_name).replace(u8'/', u8'_').replace(u8'\\', u8'_');
    return fs_path_to_u8string(std::filesystem::path {fs_make_path(work_path)} / std::filesystem::path {fs_make_path(safe_name.view())});
}

FileCacheStorage::FileCacheStorage(u8string_view real_path) :
    _workPath {fs_resolve_path(real_path)}
{
    FO_STACK_TRACE_ENTRY();
}

auto FileCacheStorage::CreateCacheStorage() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_dir(_workPath.view())) {
        (void)fs_create_directories(_workPath.view());

        if (!fs_is_dir(_workPath.view())) {
            WriteLog(LogType::Warning, "Can't create dir for cache '{}'", _workPath.view());
            return false;
        }
    }

    return true;
}

auto FileCacheStorage::HasEntry(u8string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const u8string path = MakeCacheEntryPath(_workPath.view(), entry_name);
    return fs_exists(path.view());
}

auto FileCacheStorage::GetBytes(u8string_view entry_name) const -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    const u8string path = MakeCacheEntryPath(_workPath.view(), entry_name);
    auto bytes = fs_read_file_bytes(path.view());

    if (!bytes) {
        return {};
    }

    return *bytes;
}

void FileCacheStorage::SetBytes(u8string_view entry_name, const_span<byte> bytes)
{
    FO_STACK_TRACE_ENTRY();

    if (!CreateCacheStorage()) {
        return;
    }

    const u8string path = MakeCacheEntryPath(_workPath.view(), entry_name);
    if (!fs_write_file_bytes(path.view(), bytes)) {
        (void)fs_remove_file(path.view());
        WriteLog(LogType::Warning, "Can't write cache at '{}'", path.view());
    }
}

void FileCacheStorage::RemoveEntry(u8string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    const u8string path = MakeCacheEntryPath(_workPath.view(), entry_name);
    (void)fs_remove_file(path.view());
}

#if FO_HAVE_UNQLITE
UnqliteCacheStorage::UnqliteCacheStorage(u8string_view real_path)
{
    FO_STACK_TRACE_ENTRY();

    _workPath = fs_resolve_path(real_path);
    _workPath = fs_path_to_u8string(std::filesystem::path {fs_make_path(_workPath.view())} / std::filesystem::path {u8"Cache.db"});

    if (fs_exists(_workPath.view())) {
        (void)InitCacheStorage();
    }
}

auto UnqliteCacheStorage::InitCacheStorage() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        const u8string work_dir = fs_path_to_u8string(std::filesystem::path {fs_make_path(_workPath.view())}.parent_path());
        (void)fs_create_directories(work_dir.view());

        nptr<unqlite> db;
        const ptr<const char> work_path = utf8_to_c_str(_workPath.view_nt());

        if (unqlite_open(db.get_pp(), work_path.get(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK) {
            WriteLog(LogType::Warning, "Can't open unqlite db '{}'", _workPath.view());
            return false;
        }

        _db = make_unique_del_ptr(db, [](unqlite* raw_db) noexcept {
            auto closing_db = make_ptr(raw_db);
            unqlite_close(closing_db.get());
        });
    }

    return true;
}

auto UnqliteCacheStorage::HasEntry(u8string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return false;
    }

    const const_span<char> entry_name_chars = utf8_to_char_span(entry_name);
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_chars.size());
    const auto r = unqlite_kv_fetch_callback(_db.get(), entry_name_chars.data(), entry_name_len, [](const void*, unsigned, void*) { return UNQLITE_OK; }, nullptr);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return false;
    }

    return r == UNQLITE_OK;
}

void UnqliteCacheStorage::RemoveEntry(u8string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return;
    }

    const const_span<char> entry_name_chars = utf8_to_char_span(entry_name);
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_chars.size());
    auto r = unqlite_kv_delete(_db.get(), entry_name_chars.data(), entry_name_len);

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

auto UnqliteCacheStorage::GetBytes(u8string_view entry_name) const -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    if (!_db) {
        return {};
    }

    const const_span<char> entry_name_chars = utf8_to_char_span(entry_name);
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_chars.size());

    unqlite_int64 size = 0;
    auto r = unqlite_kv_fetch(_db.get(), entry_name_chars.data(), entry_name_len, nullptr, &size);

    if (r != UNQLITE_OK && r != UNQLITE_NOTFOUND) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }
    if (r == UNQLITE_NOTFOUND) {
        return {};
    }

    vector<byte> bytes;
    bytes.resize(numeric_cast<size_t>(size));

    r = unqlite_kv_fetch(_db.get(), entry_name_chars.data(), entry_name_len, bytes.data(), &size);

    if (r != UNQLITE_OK) {
        WriteLog(LogType::Warning, "Can't fetch cache entry '{}'", entry_name);
        return {};
    }

    return bytes;
}

void UnqliteCacheStorage::SetBytes(u8string_view entry_name, const_span<byte> bytes)
{
    FO_STACK_TRACE_ENTRY();

    if (!InitCacheStorage()) {
        return;
    }

    const const_span<char> entry_name_chars = utf8_to_char_span(entry_name);
    const int32_t entry_name_len = numeric_cast<int32_t>(entry_name_chars.size());
    auto r = unqlite_kv_store(_db.get(), entry_name_chars.data(), entry_name_len, bytes.data(), numeric_cast<unqlite_int64>(bytes.size()));

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

FO_END_NAMESPACE
