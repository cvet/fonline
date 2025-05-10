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

#if !FO_IOS
#include <filesystem>
#endif

#if FO_WINDOWS
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#if FO_ANDROID
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_iostream.h"
#endif

#if FO_WINDOWS && FO_UWP
#define CreateFileW CreateFileFromAppW
#endif

auto DiskFileSystem::OpenFile(string_view fname, bool write) -> DiskFile
{
    STACK_TRACE_ENTRY();

    return {fname, write, false};
}

auto DiskFileSystem::OpenFile(string_view fname, bool write, bool write_through) -> DiskFile
{
    STACK_TRACE_ENTRY();

    return {fname, write, write_through};
}

auto DiskFileSystem::FindFiles(string_view path, string_view ext) -> DiskFind
{
    STACK_TRACE_ENTRY();

    return {path, ext};
}

#if FO_WINDOWS
static auto FileTimeToUInt64(FILETIME ft) -> uint64
{
    STACK_TRACE_ENTRY();

    return static_cast<uint64>(ft.dwHighDateTime) << 32 | static_cast<uint64>(ft.dwLowDateTime);
}

static auto WinMultiByteToWideChar(string_view mb) -> std::wstring
{
    STACK_TRACE_ENTRY();

    return strex(mb).toWideChar();
}

static auto WinWideCharToMultiByte(const wchar_t* wc) -> string
{
    STACK_TRACE_ENTRY();

    return strex().parseWideChar(wc).normalizePathSlashes();
}

struct DiskFile::Impl
{
    HANDLE FileHandle {};
};

DiskFile::DiskFile(string_view fname, bool write, bool write_through)
{
    STACK_TRACE_ENTRY();

    HANDLE h;

    if (write) {
        auto try_create = [&fname, &write_through]() { return ::CreateFileW(WinMultiByteToWideChar(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr); };

        h = try_create();

        if (h == INVALID_HANDLE_VALUE) {
            DiskFileSystem::MakeDirTree(strex(fname).extractDir());
            h = try_create();
        }
    }
    else {
        auto try_create = [&fname]() { return ::CreateFileW(WinMultiByteToWideChar(fname).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr); };

        h = try_create();

        if (h == INVALID_HANDLE_VALUE) {
            h = try_create();
        }
    }

    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    _impl = SafeAlloc::MakeUnique<Impl>();
    _impl->FileHandle = h;
    _openedForWriting = write;
}

DiskFile::~DiskFile()
{
    STACK_TRACE_ENTRY();

    if (_impl) {
        ::CloseHandle(_impl->FileHandle);
    }
}

DiskFile::DiskFile(DiskFile&&) noexcept = default;

DiskFile::operator bool() const
{
    STACK_TRACE_ENTRY();

    return !!_impl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0) {
        return true;
    }

    DWORD br = 0;

    return ::ReadFile(_impl->FileHandle, buf, static_cast<DWORD>(len), &br, nullptr) != 0 && static_cast<size_t>(br) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0) {
        return true;
    }

    DWORD bw = 0;

    return ::WriteFile(_impl->FileHandle, buf, static_cast<DWORD>(len), &bw, nullptr) != 0 && static_cast<size_t>(bw) == len;
}

auto DiskFile::Clear() -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    if (::SetFilePointer(_impl->FileHandle, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        return false;
    }

    return ::SetEndOfFile(_impl->FileHandle) != FALSE;
}

auto DiskFile::SetReadPos(int offset, DiskFileSeek origin) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::SetFilePointer(_impl->FileHandle, offset, nullptr, static_cast<DWORD>(origin)) != INVALID_SET_FILE_POINTER;
}

auto DiskFile::GetReadPos() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    const auto result = ::SetFilePointer(_impl->FileHandle, 0, nullptr, FILE_CURRENT);
    RUNTIME_ASSERT(result != INVALID_SET_FILE_POINTER);

    return static_cast<size_t>(result);
}

auto DiskFile::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);

    FILETIME tc;
    FILETIME ta;
    FILETIME tw;

    if (::GetFileTime(_impl->FileHandle, &tc, &ta, &tw) == 0) {
        return 0;
    }

    return ::FileTimeToUInt64(tw);
}

auto DiskFile::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);

    LARGE_INTEGER li;
    const auto get_file_size_ok = ::GetFileSizeEx(_impl->FileHandle, &li);
    RUNTIME_ASSERT(get_file_size_ok);
    RUNTIME_ASSERT(li.HighPart == 0);

    return li.LowPart;
}

#elif FO_ANDROID

struct DiskFile::Impl
{
    SDL_IOStream* Ops {};
    bool WriteThrough {};
    string FileName {};
};

DiskFile::DiskFile(string_view fname, bool write, bool write_through)
{
    STACK_TRACE_ENTRY();

    auto fname_str = string(fname);
    SDL_IOStream* ops = SDL_IOFromFile(fname_str.c_str(), write ? "wb" : "rb");

    if (ops == nullptr) {
        if (write) {
            DiskFileSystem::MakeDirTree(strex(fname).extractDir());
        }

        ops = SDL_IOFromFile(fname_str.c_str(), write ? "wb" : "rb");
    }

    if (ops == nullptr) {
        return;
    }

    _impl = SafeAlloc::MakeUnique<Impl>();
    _impl->Ops = ops;
    _impl->WriteThrough = write_through;
    _impl->FileName = std::move(fname_str);
    _openedForWriting = write;
}

DiskFile::~DiskFile()
{
    STACK_TRACE_ENTRY();

    if (_impl) {
        SDL_CloseIO(_impl->Ops);
    }
}

DiskFile::DiskFile(DiskFile&&) noexcept = default;

DiskFile::operator bool() const
{
    STACK_TRACE_ENTRY();

    return !!_impl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0) {
        return true;
    }

    return SDL_ReadIO(_impl->Ops, buf, len) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0) {
        return true;
    }

    const bool result = SDL_WriteIO(_impl->Ops, buf, len) == len;

    if (result && _impl->WriteThrough) {
        SDL_FlushIO(_impl->Ops);
    }

    return result;
}

auto DiskFile::Clear() -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    SDL_CloseIO(_impl->Ops);
    _impl->Ops = SDL_IOFromFile(_impl->FileName.c_str(), "w");
    RUNTIME_ASSERT(_impl->Ops);
    SDL_CloseIO(_impl->Ops);
    _impl->Ops = SDL_IOFromFile(_impl->FileName.c_str(), "wb");
    RUNTIME_ASSERT(_impl->Ops);

    return true;
}

auto DiskFile::SetReadPos(int offset, DiskFileSeek origin) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    SDL_IOWhence sdl_origin = SDL_IO_SEEK_SET;

    switch (origin) {
    case DiskFileSeek::Set:
        sdl_origin = SDL_IO_SEEK_SET;
        break;
    case DiskFileSeek::Cur:
        sdl_origin = SDL_IO_SEEK_CUR;
        break;
    case DiskFileSeek::End:
        sdl_origin = SDL_IO_SEEK_END;
        break;
    }

    return SDL_SeekIO(_impl->Ops, offset, sdl_origin) != -1;
}

auto DiskFile::GetReadPos() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    const auto result = SDL_TellIO(_impl->Ops);
    RUNTIME_ASSERT(result != -1);

    return static_cast<size_t>(result);
}

auto DiskFile::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);

    SDL_PathInfo path_info;
    const auto r = SDL_GetPathInfo(_impl->FileName.c_str(), &path_info);
    RUNTIME_ASSERT(r);

    return static_cast<uint64>(path_info.modify_time);
}

auto DiskFile::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    const Sint64 size = SDL_GetIOSize(_impl->Ops);

    return size == -1 ? 0 : static_cast<size_t>(size);
}

#else

struct DiskFile::Impl
{
    FILE* File {};
    bool WriteThrough {};
};

DiskFile::DiskFile(string_view fname, bool write, bool write_through)
{
    STACK_TRACE_ENTRY();

    FILE* f = ::fopen(string(fname).c_str(), write ? "wb" : "rb");

    if (!f) {
        if (write) {
            DiskFileSystem::MakeDirTree(strex(fname).extractDir());
        }

        f = ::fopen(string(fname).c_str(), write ? "wb" : "rb");
    }

    if (!f) {
        return;
    }

    _impl = SafeAlloc::MakeUnique<Impl>();
    _impl->File = f;
    _impl->WriteThrough = write_through;
    _openedForWriting = write;
}

DiskFile::~DiskFile()
{
    STACK_TRACE_ENTRY();

    if (_impl) {
        ::fclose(_impl->File);

#if FO_WEB
        if (_openedForWriting) {
            EM_ASM(FS.syncfs(function(err) {}));
        }
#endif
    }
}

DiskFile::DiskFile(DiskFile&&) noexcept = default;

DiskFile::operator bool() const
{
    STACK_TRACE_ENTRY();

    return !!_impl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0) {
        return true;
    }

    return ::fread(buf, sizeof(char), len, _impl->File) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0) {
        return true;
    }

    const bool result = ::fwrite(buf, len, 1, _impl->File) == 1;

    if (result && _impl->WriteThrough) {
        ::fflush(_impl->File);
    }

    return result;
}

auto DiskFile::Clear() -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    if (::fseek(_impl->File, 0, SEEK_SET) != 0) {
        return false;
    }

    const int fd = ::fileno(_impl->File);
    RUNTIME_ASSERT(fd != -1);

    return ::ftruncate(fd, 0) == 0;
}

auto DiskFile::SetReadPos(int offset, DiskFileSeek origin) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::fseek(_impl->File, offset, static_cast<int>(origin)) != -1;
}

auto DiskFile::GetReadPos() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(!_openedForWriting);

    const auto result = ::ftell(_impl->File);
    RUNTIME_ASSERT(result != -1);

    return static_cast<size_t>(result);
}

auto DiskFile::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);

    const int fd = ::fileno(_impl->File);
    RUNTIME_ASSERT(fd != -1);

    struct stat st;
    const int st_result = ::fstat(fd, &st);
    RUNTIME_ASSERT(st_result != -1);

    return static_cast<uint64>(st.st_mtime);
}

auto DiskFile::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);

    const int fd = ::fileno(_impl->File);
    RUNTIME_ASSERT(fd != -1);

    struct stat st;
    const int st_result = ::fstat(fd, &st);
    RUNTIME_ASSERT(st_result != -1);

    return static_cast<size_t>(st.st_size);
}
#endif

#if FO_WINDOWS
struct DiskFind::Impl
{
    HANDLE FindHandle {};
    WIN32_FIND_DATAW FindData {};
};

DiskFind::DiskFind(string_view path, string_view ext)
{
    STACK_TRACE_ENTRY();

    string query = strex(path).combinePath("*");
    if (!ext.empty()) {
        query += "." + string(ext);
    }

    WIN32_FIND_DATAW fd;
    auto* h = ::FindFirstFileW(WinMultiByteToWideChar(query).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    _impl = SafeAlloc::MakeUnique<Impl>();
    _impl->FindHandle = h;
    _impl->FindData = fd;

    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)) {
        (*this)++;
    }
    else {
        _findDataValid = true;
    }
}

DiskFind::~DiskFind()
{
    STACK_TRACE_ENTRY();

    if (_impl) {
        ::FindClose(_impl->FindHandle);
    }
}

DiskFind::DiskFind(DiskFind&&) noexcept = default;

auto DiskFind::operator++(int) -> DiskFind&
{
    STACK_TRACE_ENTRY();

    _findDataValid = false;

    if (::FindNextFileW(_impl->FindHandle, &_impl->FindData) == 0) {
        return *this;
    }

    if ((_impl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (wcscmp(_impl->FindData.cFileName, L".") == 0 || wcscmp(_impl->FindData.cFileName, L"..") == 0)) {
        return (*this)++;
    }

    _findDataValid = true;

    return *this;
}

DiskFind::operator bool() const
{
    STACK_TRACE_ENTRY();

    return _findDataValid;
}

auto DiskFind::IsDir() const -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return (_impl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

auto DiskFind::GetPath() const -> string
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return WinWideCharToMultiByte(_impl->FindData.cFileName);
}

auto DiskFind::GetFileSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);
    RUNTIME_ASSERT(!(_impl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));

    return _impl->FindData.nFileSizeLow;
}

auto DiskFind::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return FileTimeToUInt64(_impl->FindData.ftLastWriteTime);
}

#else

struct DiskFind::Impl
{
    DIR* Dir {};
    string Path {};
    string Ext {};
    dirent* Ent {};
    bool IsDir {};
    size_t Size {};
    uint64 WriteTime {};
};

DiskFind::DiskFind(string_view path, string_view ext)
{
    STACK_TRACE_ENTRY();

    DIR* d = opendir(string(path).c_str());

    if (!d) {
        return;
    }

    _impl = SafeAlloc::MakeUnique<Impl>();
    _impl->Dir = d;
    _impl->Path = path;
    _impl->Ext = ext;

    if (!_impl->Path.empty() && _impl->Path.back() != '/') {
        _impl->Path += "/";
    }

    if (!ext.empty()) {
        _impl->Ext = ext;
    }

    // Read first entry
    (*this)++;
}

DiskFind::~DiskFind()
{
    STACK_TRACE_ENTRY();

    if (_impl) {
        closedir(_impl->Dir);
    }
}

DiskFind::DiskFind(DiskFind&&) noexcept = default;

auto DiskFind::operator++(int) -> DiskFind&
{
    STACK_TRACE_ENTRY();

    _findDataValid = false;

    _impl->Ent = readdir(_impl->Dir);
    if (_impl->Ent == nullptr) {
        return *this;
    }

    // Skip '.' and '..'
    if (!strcmp(_impl->Ent->d_name, ".") || !strcmp(_impl->Ent->d_name, "..")) {
        return (*this)++;
    }

    // Read entire information
    struct stat st;
    if (stat((_impl->Path + _impl->Ent->d_name).c_str(), &st)) {
        return (*this)++;
    }
    if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) {
        return (*this)++;
    }

    // Check extension
    if (!_impl->Ext.empty()) {
        // Skip dirs
        if (S_ISDIR(st.st_mode)) {
            return (*this)++;
        }

        // Compare extension
        const char* ext = nullptr;
        for (const char* s = _impl->Ent->d_name; *s; s++) {
            if (*s == '.') {
                ext = s + 1;
            }
        }

        if (!ext || !*ext || strcasecmp(ext, _impl->Ext.c_str())) {
            return (*this)++;
        }
    }

    _impl->IsDir = S_ISDIR(st.st_mode);
    _impl->Size = st.st_size;
    _impl->WriteTime = (uint64)st.st_mtime;
    _findDataValid = true;

    return *this;
}

DiskFind::operator bool() const
{
    STACK_TRACE_ENTRY();

    return _findDataValid;
}

auto DiskFind::IsDir() const -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return _impl->IsDir;
}

auto DiskFind::GetPath() const -> string
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return _impl->Ent->d_name;
}

auto DiskFind::GetFileSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);
    RUNTIME_ASSERT(!_impl->IsDir);

    return _impl->Size;
}

auto DiskFind::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return _impl->WriteTime;
}
#endif

auto DiskFile::Write(string_view str) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    if (!str.empty()) {
        return Write(str.data(), str.length());
    }

    return true;
}

auto DiskFile::Write(const_span<uint8> data) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_impl);
    RUNTIME_ASSERT(_openedForWriting);

    if (!data.empty()) {
        return Write(data.data(), data.size());
    }

    return true;
}

auto DiskFileSystem::GetWriteTime(string_view path) -> uint64
{
    STACK_TRACE_ENTRY();

    if (const auto file = OpenFile(path, false)) {
        return file.GetWriteTime();
    }

    return 0;
}

#if !FO_IOS
static auto MakeFilesystemPath(string_view path)
{
#if CPLUSPLUS_20
    return std::u8string(path.begin(), path.end());
#else
    return std::filesystem::u8path(path);
#endif
}

auto DiskFileSystem::IsExists(string_view path) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    return !std::filesystem::exists(MakeFilesystemPath(path), ec) && !ec;
}

auto DiskFileSystem::IsDir(string_view path) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    return std::filesystem::is_directory(MakeFilesystemPath(path), ec) && !ec;
}

auto DiskFileSystem::DeleteFile(string_view fname) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::remove(MakeFilesystemPath(fname), ec);

    return !std::filesystem::exists(MakeFilesystemPath(fname), ec) && !ec;
}

auto DiskFileSystem::CopyFile(string_view fname, string_view copy_fname) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    return std::filesystem::copy_file(MakeFilesystemPath(fname), copy_fname, ec);
}

auto DiskFileSystem::RenameFile(string_view fname, string_view new_fname) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::rename(MakeFilesystemPath(fname), new_fname, ec);

    return !ec;
}

auto DiskFileSystem::ResolvePath(string_view path) -> string
{
    STACK_TRACE_ENTRY();

    string result;

    std::error_code ec;
    const auto resolved = std::filesystem::absolute(MakeFilesystemPath(path), ec);

    if (!ec) {
#if FO_WINDOWS
        result = WinWideCharToMultiByte(resolved.native().c_str());
#else
        result = resolved.native();
#endif
    }
    else {
        result = path;
    }

    return result;
}

void DiskFileSystem::MakeDirTree(string_view path)
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::create_directories(MakeFilesystemPath(path), ec);
}

auto DiskFileSystem::DeleteDir(string_view dir) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::remove_all(MakeFilesystemPath(dir), ec);

    return !std::filesystem::exists(MakeFilesystemPath(dir), ec) && !ec;
}

#else

auto DiskFileSystem::IsExists(string_view path) -> bool
{
    STACK_TRACE_ENTRY();

    struct stat st;

    return ::stat(string(path).c_str(), &st) == 0;
}

auto DiskFileSystem::IsDir(string_view path) -> bool
{
    STACK_TRACE_ENTRY();

    struct stat st;

    if (::stat(string(path).c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true;
        }
    }

    return false;
}

auto DiskFileSystem::DeleteFile(string_view fname) -> bool
{
    STACK_TRACE_ENTRY();

    return remove(string(fname).c_str()) != 0;
}

auto DiskFileSystem::CopyFile(string_view fname, string_view copy_fname) -> bool
{
    STACK_TRACE_ENTRY();

    bool ok = false;
    FILE* from = fopen(string(fname).c_str(), "rb");

    if (from) {
        FILE* to = fopen(string(copy_fname).c_str(), "wb");

        if (to) {
            ok = true;
            char buf[BUFSIZ];

            while (!feof(from)) {
                size_t rb = fread(buf, 1, BUFSIZ, from);
                size_t rw = fwrite(buf, 1, rb, to);

                if (!rb || rb != rw) {
                    ok = false;
                    break;
                }
            }

            fclose(to);

            if (!ok) {
                DeleteFile(copy_fname);
            }
        }
    }

    fclose(from);

    return ok;
}

auto DiskFileSystem::RenameFile(string_view fname, string_view new_fname) -> bool
{
    STACK_TRACE_ENTRY();

    return rename(string(fname).c_str(), string(new_fname).c_str()) == 0;
}

auto DiskFileSystem::ResolvePath(string_view path) -> string
{
    STACK_TRACE_ENTRY();

    string result;

    char* buf = realpath(string(path).c_str(), nullptr);

    if (buf != nullptr) {
        result = buf;
        free(buf);
    }
    else {
        result = path;
    }

    return result;
}

void DiskFileSystem::MakeDirTree(string_view path)
{
    STACK_TRACE_ENTRY();

    const string work = strex(path).normalizePathSlashes();

    for (size_t i = 0; i < work.length(); i++) {
        if (work[i] == '/') {
            auto path_part = work.substr(0, i);

            mkdir(path_part.c_str(), 0777);
        }
    }
}

auto DiskFileSystem::DeleteDir(string_view dir) -> bool
{
    STACK_TRACE_ENTRY();

    MakeDirTree(dir);

    vector<string> file_paths;

    for (auto find = DiskFind(dir, ""); find; find++) {
        if (!find.IsDir()) {
            file_paths.emplace_back(find.GetPath());
        }
    }

    for (auto& fp : file_paths) {
        if (!DeleteFile(fp)) {
            return false;
        }
    }

    return rmdir(string(dir).c_str()) == 0;
}
#endif

static void RecursiveDirLook(string_view base_dir, string_view cur_dir, bool include_subdirs, string_view ext, DiskFileSystem::FileVisitor& visitor)
{
    STACK_TRACE_ENTRY();

    for (auto find = DiskFileSystem::FindFiles(strex(base_dir).combinePath(cur_dir), ""); find; find++) {
        auto path = find.GetPath();
        RUNTIME_ASSERT(!path.empty());

        if (path[0] != '.' && path[0] != '~') {
            if (find.IsDir()) {
                if (path[0] != '_' && include_subdirs) {
                    RecursiveDirLook(base_dir, strex(cur_dir).combinePath(path), include_subdirs, ext, visitor);
                }
            }
            else {
                if (ext.empty() || strex(path).getFileExtension() == ext) {
                    visitor(strex(cur_dir).combinePath(path), find.GetFileSize(), find.GetWriteTime());
                }
            }
        }
    }
}

void DiskFileSystem::IterateDir(string_view dir, string_view ext, bool include_subdirs, FileVisitor visitor)
{
    STACK_TRACE_ENTRY();

    RecursiveDirLook(dir, "", include_subdirs, ext, visitor);
}

auto DiskFileSystem::CompareFileContent(string_view path, const_span<uint8> buf) -> bool
{
    STACK_TRACE_ENTRY();

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
