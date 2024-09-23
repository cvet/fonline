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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "SDL.h"
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

    return format(mb).toWideChar();
}

static auto WinWideCharToMultiByte(const wchar_t* wc) -> string
{
    STACK_TRACE_ENTRY();

    return format().parseWideChar(wc).normalizePathSlashes();
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
            DiskFileSystem::MakeDirTree(format(fname).extractDir());
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

    _pImpl = std::make_unique<Impl>();
    _pImpl->FileHandle = h;
    _openedForWriting = write;
}

DiskFile::~DiskFile()
{
    STACK_TRACE_ENTRY();

    if (_pImpl) {
        ::CloseHandle(_pImpl->FileHandle);
    }
}

DiskFile::DiskFile(DiskFile&&) noexcept = default;

DiskFile::operator bool() const
{
    STACK_TRACE_ENTRY();

    return !!_pImpl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0) {
        return true;
    }

    DWORD br = 0;

    return ::ReadFile(_pImpl->FileHandle, buf, static_cast<DWORD>(len), &br, nullptr) != 0 && static_cast<size_t>(br) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0) {
        return true;
    }

    DWORD bw = 0;

    return ::WriteFile(_pImpl->FileHandle, buf, static_cast<DWORD>(len), &bw, nullptr) != 0 && static_cast<size_t>(bw) == len;
}

auto DiskFile::Clear() -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (::SetFilePointer(_pImpl->FileHandle, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        return false;
    }

    return ::SetEndOfFile(_pImpl->FileHandle) != FALSE;
}

auto DiskFile::SetReadPos(int offset, DiskFileSeek origin) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::SetFilePointer(_pImpl->FileHandle, offset, nullptr, static_cast<DWORD>(origin)) != INVALID_SET_FILE_POINTER;
}

auto DiskFile::GetReadPos() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    const auto result = ::SetFilePointer(_pImpl->FileHandle, 0, nullptr, FILE_CURRENT);
    RUNTIME_ASSERT(result != INVALID_SET_FILE_POINTER);

    return static_cast<size_t>(result);
}

auto DiskFile::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);

    FILETIME tc;
    FILETIME ta;
    FILETIME tw;

    if (::GetFileTime(_pImpl->FileHandle, &tc, &ta, &tw) == 0) {
        return 0;
    }

    return ::FileTimeToUInt64(tw);
}

auto DiskFile::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);

    LARGE_INTEGER li;
    const auto get_file_size_ok = ::GetFileSizeEx(_pImpl->FileHandle, &li);
    RUNTIME_ASSERT(get_file_size_ok);
    RUNTIME_ASSERT(li.HighPart == 0);

    return li.LowPart;
}

#elif FO_ANDROID

struct DiskFile::Impl
{
    SDL_RWops* Ops {};
    bool WriteThrough {};
};

DiskFile::DiskFile(string_view fname, bool write, bool write_through)
{
    STACK_TRACE_ENTRY();

    SDL_RWops* ops = SDL_RWFromFile(string(fname).c_str(), write ? "wb" : "rb");

    if (!ops) {
        if (write) {
            DiskFileSystem::MakeDirTree(format(fname).extractDir());
        }

        ops = SDL_RWFromFile(string(fname).c_str(), write ? "wb" : "rb");
    }

    if (!ops) {
        return;
    }

    RUNTIME_ASSERT(ops->type == SDL_RWOPS_STDFILE);

    _pImpl = std::make_unique<Impl>();
    _pImpl->Ops = ops;
    _pImpl->WriteThrough = write_through;
    _openedForWriting = write;
}

DiskFile::~DiskFile()
{
    STACK_TRACE_ENTRY();

    if (_pImpl) {
        SDL_RWclose(_pImpl->Ops);
    }
}

DiskFile::DiskFile(DiskFile&&) noexcept = default;

DiskFile::operator bool() const
{
    STACK_TRACE_ENTRY();

    return !!_pImpl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0) {
        return true;
    }

    return SDL_RWread(_pImpl->Ops, buf, sizeof(char), len) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0) {
        return true;
    }

    const bool result = SDL_RWwrite(_pImpl->Ops, buf, len, 1) == 1;

    if (result && _pImpl->WriteThrough) {
        ::fflush(_pImpl->Ops->hidden.stdio.fp);
    }

    return result;
}

auto DiskFile::Clear() -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (SDL_RWseek(_pImpl->Ops, 0, RW_SEEK_SET) != 0) {
        return false;
    }

    const int fd = ::fileno(_pImpl->Ops->hidden.stdio.fp);
    RUNTIME_ASSERT(fd != -1);

    return ::ftruncate(fd, 0) == 0;
}

auto DiskFile::SetReadPos(int offset, DiskFileSeek origin) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return SDL_RWseek(_pImpl->Ops, offset, static_cast<int>(origin)) != -1;
}

auto DiskFile::GetReadPos() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    const auto result = SDL_RWtell(_pImpl->Ops);
    RUNTIME_ASSERT(result != -1);

    return static_cast<size_t>(result);
}

auto DiskFile::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);

    const int fd = ::fileno(_pImpl->Ops->hidden.stdio.fp);
    RUNTIME_ASSERT(fd != -1);

    struct stat st;
    const int st_result = ::fstat(fd, &st);
    RUNTIME_ASSERT(st_result != -1);

    return static_cast<uint64>(st.st_mtime);
}

auto DiskFile::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    const Sint64 size = SDL_RWsize(_pImpl->Ops);

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
            DiskFileSystem::MakeDirTree(format(fname).extractDir());
        }

        f = ::fopen(string(fname).c_str(), write ? "wb" : "rb");
    }

    if (!f) {
        return;
    }

    _pImpl = std::make_unique<Impl>();
    _pImpl->File = f;
    _pImpl->WriteThrough = write_through;
    _openedForWriting = write;
}

DiskFile::~DiskFile()
{
    STACK_TRACE_ENTRY();

    if (_pImpl) {
        ::fclose(_pImpl->File);

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

    return !!_pImpl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0) {
        return true;
    }

    return ::fread(buf, sizeof(char), len, _pImpl->File) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0) {
        return true;
    }

    const bool result = ::fwrite(buf, len, 1, _pImpl->File) == 1;

    if (result && _pImpl->WriteThrough) {
        ::fflush(_pImpl->File);
    }

    return result;
}

auto DiskFile::Clear() -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (::fseek(_pImpl->File, 0, SEEK_SET) != 0) {
        return false;
    }

    const int fd = ::fileno(_pImpl->File);
    RUNTIME_ASSERT(fd != -1);

    return ::ftruncate(fd, 0) == 0;
}

auto DiskFile::SetReadPos(int offset, DiskFileSeek origin) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::fseek(_pImpl->File, offset, static_cast<int>(origin)) != -1;
}

auto DiskFile::GetReadPos() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    const auto result = ::ftell(_pImpl->File);
    RUNTIME_ASSERT(result != -1);

    return static_cast<size_t>(result);
}

auto DiskFile::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);

    const int fd = ::fileno(_pImpl->File);
    RUNTIME_ASSERT(fd != -1);

    struct stat st;
    const int st_result = ::fstat(fd, &st);
    RUNTIME_ASSERT(st_result != -1);

    return static_cast<uint64>(st.st_mtime);
}

auto DiskFile::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_pImpl);

    const int fd = ::fileno(_pImpl->File);
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

    string query = format(path).combinePath("*");
    if (!ext.empty()) {
        query += "." + string(ext);
    }

    WIN32_FIND_DATAW fd;
    auto* h = ::FindFirstFileW(WinMultiByteToWideChar(query).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    _pImpl = std::make_unique<Impl>();
    _pImpl->FindHandle = h;
    _pImpl->FindData = fd;

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

    if (_pImpl) {
        ::FindClose(_pImpl->FindHandle);
    }
}

DiskFind::DiskFind(DiskFind&&) noexcept = default;

auto DiskFind::operator++(int) -> DiskFind&
{
    STACK_TRACE_ENTRY();

    _findDataValid = false;

    if (::FindNextFileW(_pImpl->FindHandle, &_pImpl->FindData) == 0) {
        return *this;
    }

    if ((_pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (wcscmp(_pImpl->FindData.cFileName, L".") == 0 || wcscmp(_pImpl->FindData.cFileName, L"..") == 0)) {
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

    return (_pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

auto DiskFind::GetPath() const -> string
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return WinWideCharToMultiByte(_pImpl->FindData.cFileName);
}

auto DiskFind::GetFileSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);
    RUNTIME_ASSERT(!(_pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));

    return _pImpl->FindData.nFileSizeLow;
}

auto DiskFind::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return FileTimeToUInt64(_pImpl->FindData.ftLastWriteTime);
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

    _pImpl = std::make_unique<Impl>();
    _pImpl->Dir = d;
    _pImpl->Path = path;
    _pImpl->Ext = ext;

    if (!_pImpl->Path.empty() && _pImpl->Path.back() != '/') {
        _pImpl->Path += "/";
    }

    if (!ext.empty()) {
        _pImpl->Ext = ext;
    }

    // Read first entry
    (*this)++;
}

DiskFind::~DiskFind()
{
    STACK_TRACE_ENTRY();

    if (_pImpl) {
        closedir(_pImpl->Dir);
    }
}

DiskFind::DiskFind(DiskFind&&) noexcept = default;

auto DiskFind::operator++(int) -> DiskFind&
{
    STACK_TRACE_ENTRY();

    _findDataValid = false;

    _pImpl->Ent = readdir(_pImpl->Dir);
    if (_pImpl->Ent == nullptr) {
        return *this;
    }

    // Skip '.' and '..'
    if (!strcmp(_pImpl->Ent->d_name, ".") || !strcmp(_pImpl->Ent->d_name, "..")) {
        return (*this)++;
    }

    // Read entire information
    struct stat st;
    if (stat((_pImpl->Path + _pImpl->Ent->d_name).c_str(), &st)) {
        return (*this)++;
    }
    if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) {
        return (*this)++;
    }

    // Check extension
    if (!_pImpl->Ext.empty()) {
        // Skip dirs
        if (S_ISDIR(st.st_mode)) {
            return (*this)++;
        }

        // Compare extension
        const char* ext = nullptr;
        for (const char* s = _pImpl->Ent->d_name; *s; s++) {
            if (*s == '.') {
                ext = s + 1;
            }
        }

        if (!ext || !*ext || strcasecmp(ext, _pImpl->Ext.c_str())) {
            return (*this)++;
        }
    }

    _pImpl->IsDir = S_ISDIR(st.st_mode);
    _pImpl->Size = st.st_size;
    _pImpl->WriteTime = (uint64)st.st_mtime;
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

    return _pImpl->IsDir;
}

auto DiskFind::GetPath() const -> string
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->Ent->d_name;
}

auto DiskFind::GetFileSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);
    RUNTIME_ASSERT(!_pImpl->IsDir);

    return _pImpl->Size;
}

auto DiskFind::GetWriteTime() const -> uint64
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->WriteTime;
}
#endif

auto DiskFile::Write(string_view str) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (!str.empty()) {
        return Write(str.data(), str.length());
    }

    return true;
}

auto DiskFile::Write(const_span<uint8> data) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
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
auto DiskFileSystem::IsExists(string_view path) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    return !std::filesystem::exists(std::filesystem::u8path(path), ec) && !ec;
}

auto DiskFileSystem::IsDir(string_view path) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    return std::filesystem::is_directory(std::filesystem::u8path(path), ec) && !ec;
}

auto DiskFileSystem::DeleteFile(string_view fname) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::remove(std::filesystem::u8path(fname), ec);

    return !std::filesystem::exists(std::filesystem::u8path(fname), ec) && !ec;
}

auto DiskFileSystem::CopyFile(string_view fname, string_view copy_fname) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    return std::filesystem::copy_file(std::filesystem::u8path(fname), copy_fname, ec);
}

auto DiskFileSystem::RenameFile(string_view fname, string_view new_fname) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::rename(std::filesystem::u8path(fname), new_fname, ec);

    return !ec;
}

void DiskFileSystem::ResolvePath(string& path)
{
    STACK_TRACE_ENTRY();

    std::error_code ec;
    const auto resolved = std::filesystem::absolute(std::filesystem::u8path(path), ec);

    if (!ec) {
#if FO_WINDOWS
        path = WinWideCharToMultiByte(resolved.native().c_str());
#else
        path = resolved.native();
#endif
    }
}

void DiskFileSystem::MakeDirTree(string_view path)
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::create_directories(std::filesystem::u8path(path), ec);
}

auto DiskFileSystem::DeleteDir(string_view dir) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code ec;

    std::filesystem::remove_all(std::filesystem::u8path(dir), ec);

    return !std::filesystem::exists(std::filesystem::u8path(dir), ec) && !ec;
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

void DiskFileSystem::ResolvePath(string& path)
{
    STACK_TRACE_ENTRY();

    char* buf = realpath(path.c_str(), nullptr);

    if (buf) {
        path = buf;
        free(buf);
    }
}

void DiskFileSystem::MakeDirTree(string_view path)
{
    STACK_TRACE_ENTRY();

    const string work = format(path).normalizePathSlashes();

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

    for (auto find = DiskFileSystem::FindFiles(format(base_dir).combinePath(cur_dir), ""); find; find++) {
        auto path = find.GetPath();
        RUNTIME_ASSERT(!path.empty());

        if (path[0] != '.' && path[0] != '~') {
            if (find.IsDir()) {
                if (path[0] != '_' && include_subdirs) {
                    RecursiveDirLook(base_dir, format(cur_dir).combinePath(path), include_subdirs, ext, visitor);
                }
            }
            else {
                if (ext.empty() || format(path).getFileExtension() == ext) {
                    visitor(format(cur_dir).combinePath(path), find.GetFileSize(), find.GetWriteTime());
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
