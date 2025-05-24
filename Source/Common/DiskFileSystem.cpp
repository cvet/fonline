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

#include "DiskFileSystem.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include <filesystem>

FO_BEGIN_NAMESPACE();

static auto MakeFilesystemPath(string_view path) -> std::filesystem::path
{
    FO_STACK_TRACE_ENTRY();

    return std::u8string(path.begin(), path.end());
}

auto DiskFileSystem::OpenFile(string_view fname, bool write) -> DiskFile
{
    FO_STACK_TRACE_ENTRY();

    return {fname, write, false};
}

auto DiskFileSystem::OpenFile(string_view fname, bool write, bool write_through) -> DiskFile
{
    FO_STACK_TRACE_ENTRY();

    return {fname, write, write_through};
}

DiskFile::DiskFile(string_view fname, bool write, bool write_through)
{
    FO_STACK_TRACE_ENTRY();

    _openedForWriting = write;
    _writeThrough = write_through;

    const auto path = MakeFilesystemPath(fname);

    if (write) {
        constexpr auto flags = std::ios::out | std::ios::binary | std::ios::trunc;
        _file = std::fstream(path, flags);

        if (!_file) {
            DiskFileSystem::MakeDirTree(strex(fname).extractDir());
            _file = std::fstream(path, flags);
        }
    }
    else {
        constexpr auto flags = std::ios::in | std::ios::binary;
        _file = std::fstream(path, flags);

        if (!_file) {
            _file = std::fstream(path, flags);
        }
    }
}

DiskFile::operator bool() const
{
    FO_STACK_TRACE_ENTRY();

    return !!_file;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_file);
    FO_RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0) {
        return true;
    }

    _file.read(static_cast<char*>(buf), static_cast<std::streamsize>(len));
    return !!_file && _file.gcount() == static_cast<std::streamsize>(len);
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_file);
    FO_RUNTIME_ASSERT(_openedForWriting);

    if (len == 0) {
        return true;
    }

    _file.write(static_cast<const char*>(buf), static_cast<std::streamsize>(len));

    if (_writeThrough && _file) {
        _file.flush();
    }

    return !!_file;
}

auto DiskFile::SetReadPos(int offset, DiskFileSeek origin) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_file);
    FO_RUNTIME_ASSERT(!_openedForWriting);

    switch (origin) {
    case DiskFileSeek::Set:
        _file.seekg(static_cast<std::streamoff>(offset), std::ios_base::beg);
        break;
    case DiskFileSeek::Cur:
        _file.seekg(static_cast<std::streamoff>(offset), std::ios_base::cur);
        break;
    case DiskFileSeek::End:
        _file.seekg(static_cast<std::streamoff>(offset), std::ios_base::end);
        break;
    }

    return !!_file;
}

auto DiskFile::GetReadPos() -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_file);
    FO_RUNTIME_ASSERT(!_openedForWriting);

    const auto cur_pos = static_cast<size_t>(_file.tellg());
    FO_RUNTIME_ASSERT(_file);
    return cur_pos;
}

auto DiskFile::GetSize() -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_file);

    size_t file_len;

    if (_openedForWriting) {
        file_len = static_cast<size_t>(_file.tellp());
    }
    else {
        const auto cur_pos = _file.tellg();
        _file.seekg(0, std::ios_base::end);
        file_len = static_cast<size_t>(_file.tellg());
        _file.seekg(cur_pos, std::ios_base::beg);
    }

    FO_RUNTIME_ASSERT(_file);
    return file_len;
}

auto DiskFile::Write(string_view str) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_file);
    FO_RUNTIME_ASSERT(_openedForWriting);

    if (!str.empty()) {
        return Write(str.data(), str.length());
    }

    return true;
}

auto DiskFile::Write(const_span<uint8> data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_file);
    FO_RUNTIME_ASSERT(_openedForWriting);

    if (!data.empty()) {
        return Write(data.data(), data.size());
    }

    return true;
}

auto DiskFileSystem::GetWriteTime(string_view path) -> uint64
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto wt = std::filesystem::last_write_time(MakeFilesystemPath(path), ec);
    return !ec ? wt.time_since_epoch().count() : 0;
}

auto DiskFileSystem::IsExists(string_view path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return !std::filesystem::exists(MakeFilesystemPath(path), ec) && !ec;
}

auto DiskFileSystem::IsDir(string_view path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::is_directory(MakeFilesystemPath(path), ec) && !ec;
}

auto DiskFileSystem::DeleteFile(string_view path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto fs_path = MakeFilesystemPath(path);

    std::error_code ec;
    std::filesystem::remove(fs_path, ec);
    return !std::filesystem::exists(fs_path, ec) && !ec;
}

auto DiskFileSystem::CopyFile(string_view path, string_view copy_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::copy_file(MakeFilesystemPath(path), copy_path, ec);
}

auto DiskFileSystem::RenameFile(string_view path, string_view new_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    std::filesystem::rename(MakeFilesystemPath(path), new_path, ec);
    return !ec;
}

auto DiskFileSystem::ResolvePath(string_view path) -> string
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto resolved = std::filesystem::absolute(MakeFilesystemPath(path), ec);

    if (!ec) {
        const auto u8_str = resolved.u8string();
        return string(u8_str.begin(), u8_str.end());
    }
    else {
        return string(path);
    }
}

void DiskFileSystem::MakeDirTree(string_view path)
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    std::filesystem::create_directories(MakeFilesystemPath(path), ec);
}

auto DiskFileSystem::DeleteDir(string_view dir) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto fs_dir = MakeFilesystemPath(dir);

    std::error_code ec;
    std::filesystem::remove_all(fs_dir, ec);
    return !std::filesystem::exists(fs_dir, ec) && !ec;
}

static void RecursiveDirLook(string_view base_dir, string_view cur_dir, bool recursive, DiskFileSystem::FileVisitor& visitor)
{
    FO_STACK_TRACE_ENTRY();

    const auto full_dir = MakeFilesystemPath(strex(base_dir).combinePath(cur_dir));
    const auto dir_iterator = std::filesystem::directory_iterator(full_dir, std::filesystem::directory_options::follow_directory_symlink);

    for (const auto& dir_entry : dir_iterator) {
        const auto u8_str = dir_entry.path().filename().u8string();
        const auto path = string(u8_str.begin(), u8_str.end());
        FO_RUNTIME_ASSERT(!path.empty());

        if (path.front() != '.' && path.front() != '~') {
            if (dir_entry.is_directory()) {
                if (path.front() != '_' && recursive) {
                    RecursiveDirLook(base_dir, strex(cur_dir).combinePath(path), recursive, visitor);
                }
            }
            else {
                visitor(strex(cur_dir).combinePath(path), dir_entry.file_size(), dir_entry.last_write_time().time_since_epoch().count());
            }
        }
    }
}

void DiskFileSystem::IterateDir(string_view dir, bool recursive, FileVisitor visitor)
{
    FO_STACK_TRACE_ENTRY();

    RecursiveDirLook(dir, "", recursive, visitor);
}

auto DiskFileSystem::CompareFileContent(string_view path, const_span<uint8> buf) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto file = OpenFile(path, false);

    if (!file) {
        return false;
    }

    const auto file_size = file.GetSize();

    if (file_size != buf.size()) {
        return false;
    }

    vector<uint8> file_buf(file_size);

    if (!file.Read(file_buf.data(), file_size)) {
        return false;
    }

    return MemCompare(file_buf.data(), buf.data(), buf.size());
}

FO_END_NAMESPACE();
