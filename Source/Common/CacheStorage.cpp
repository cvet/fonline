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

CacheStorage::CacheStorage(u8string_view path) :
    _impl {SafeAlloc::MakeUnique<FileCacheStorage>(path)}
{
    FO_STACK_TRACE_ENTRY();
}

CacheStorage::CacheStorage(u8string path) :
    _impl {SafeAlloc::MakeUnique<FileCacheStorage>(path)}
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

void CacheStorage::SetText(string_view entry_name, string_view text)
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_text = text;
    SetText(entry_name, utf8_text);
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
    return fs_path_to_u8string(std::filesystem::path {fs_make_path(work_path)} / std::filesystem::path {fs_make_path(safe_name)});
}

FileCacheStorage::FileCacheStorage(u8string_view real_path) :
    _workPath {fs_resolve_path(real_path)}
{
    FO_STACK_TRACE_ENTRY();
}

auto FileCacheStorage::CreateCacheStorage() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_dir(_workPath)) {
        (void)fs_create_directories(_workPath);

        if (!fs_is_dir(_workPath)) {
            WriteLog(LogType::Warning, "Can't create dir for cache '{}'", _workPath);
            return false;
        }
    }

    return true;
}

auto FileCacheStorage::HasEntry(u8string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    u8string path = MakeCacheEntryPath(_workPath, entry_name);
    return fs_exists(path);
}

auto FileCacheStorage::GetBytes(u8string_view entry_name) const -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    u8string path = MakeCacheEntryPath(_workPath, entry_name);
    auto bytes = fs_read_file_bytes(path);

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

    u8string path = MakeCacheEntryPath(_workPath, entry_name);

    if (!fs_write_file_bytes(path, bytes)) {
        (void)fs_remove_file(path);
        WriteLog(LogType::Warning, "Can't write cache at '{}'", path);
    }
}

void FileCacheStorage::RemoveEntry(u8string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    u8string path = MakeCacheEntryPath(_workPath, entry_name);
    (void)fs_remove_file(path);
}

FO_END_NAMESPACE
