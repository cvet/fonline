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

#include "DiskFileSystem.h"
#include "StringUtils.h"
#include "Version-Include.h"
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
    return DiskFile(fname, write, false);
}

auto DiskFileSystem::OpenFile(string_view fname, bool write, bool write_through) -> DiskFile
{
    return DiskFile(fname, write, write_through);
}

auto DiskFileSystem::FindFiles(string_view path, string_view ext) -> DiskFind
{
    return DiskFind(path, ext);
}

#if FO_WINDOWS
static auto FileTimeToUInt64(FILETIME ft) -> uint64
{
    return static_cast<uint64>(ft.dwHighDateTime) << 32 | static_cast<uint64>(ft.dwLowDateTime);
}

static auto WinMultiByteToWideChar(string_view mb) -> std::wstring
{
    return _str(mb).replace('/', '\\').toWideChar();
}

static auto WinWideCharToMultiByte(const wchar_t* wc) -> string
{
    return _str().parseWideChar(wc).normalizePathSlashes();
}

struct DiskFile::Impl
{
    HANDLE FileHandle {};
};

DiskFile::DiskFile(string_view fname, bool write, bool write_through)
{
    HANDLE h;

    if (write) {
        auto try_create = [&fname, &write_through]() { return ::CreateFileW(WinMultiByteToWideChar(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr); };

        h = try_create();
        if (h == INVALID_HANDLE_VALUE) {
            DiskFileSystem::MakeDirTree(_str(fname).extractDir());
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
    if (_pImpl) {
        ::CloseHandle(_pImpl->FileHandle);
    }
}

DiskFile::DiskFile(DiskFile&&) noexcept = default;

DiskFile::operator bool() const
{
    return !!_pImpl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0u) {
        return true;
    }

    DWORD br = 0;
    return ::ReadFile(_pImpl->FileHandle, buf, static_cast<DWORD>(len), &br, nullptr) != 0 && static_cast<size_t>(br) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0u) {
        return true;
    }

    DWORD bw = 0;
    return ::WriteFile(_pImpl->FileHandle, buf, static_cast<DWORD>(len), &bw, nullptr) != 0 && static_cast<size_t>(bw) == len;
}

auto DiskFile::SetPos(int offset, DiskFileSeek origin) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::SetFilePointer(_pImpl->FileHandle, offset, nullptr, static_cast<DWORD>(origin)) != INVALID_SET_FILE_POINTER;
}

auto DiskFile::GetPos() const -> size_t
{
    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::SetFilePointer(_pImpl->FileHandle, 0, nullptr, FILE_CURRENT);
}

auto DiskFile::GetWriteTime() const -> uint64
{
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
    SDL_RWops* ops = SDL_RWFromFile(string(fname).c_str(), write ? "wb" : "rb");
    if (!ops) {
        if (write) {
            DiskFileSystem::MakeDirTree(_str(fname).extractDir());
        }
        ops = SDL_RWFromFile(string(fname).c_str(), write ? "wb" : "rb");
    }
    if (!ops) {
        return;
    }

    _pImpl = std::make_unique<Impl>();
    _pImpl->Ops = ops;
    _pImpl->WriteThrough = write_through;
    _openedForWriting = write;
}

DiskFile::~DiskFile()
{
    if (_pImpl) {
        SDL_RWclose(_pImpl->Ops);
    }
}

DiskFile::DiskFile(DiskFile&&) noexcept = default;

DiskFile::operator bool() const
{
    return !!_pImpl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0u) {
        return true;
    }

    return SDL_RWread(_pImpl->Ops, buf, sizeof(char), len) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0u) {
        return true;
    }

    SDL_RWops* ops = _pImpl->Ops;
    bool result = SDL_RWwrite(ops, buf, sizeof(char), len) == len;
    if (result && _pImpl->WriteThrough) {
        if (ops->type == SDL_RWOPS_WINFILE) {
#ifdef __WIN32__
            ::FlushFileBuffers((HANDLE)ops->hidden.windowsio.h);
#endif
        }
        else if (ops->type == SDL_RWOPS_STDFILE) {
#ifdef HAVE_STDIO_H
            ::fflush(ops->hidden.stdio.fp);
#endif
        }
    }
    return result;
}

auto DiskFile::SetPos(int offset, DiskFileSeek origin) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return SDL_RWseek(_pImpl->Ops, offset, static_cast<int>(origin)) != -1;
}

auto DiskFile::GetPos() const -> size_t
{
    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return SDL_RWtell(_pImpl->Ops);
}

auto DiskFile::GetWriteTime() const -> uint64
{
    RUNTIME_ASSERT(_pImpl);

#if !defined(__WIN32__) && !defined(HAVE_STDIO_H)
#error __WIN32__ or HAVE_STDIO_H must be defined
#endif

    auto* ops = _pImpl->Ops;
    if (ops->type == SDL_RWOPS_WINFILE) {
#ifdef __WIN32__
        FILETIME tc, ta, tw;
        ::GetFileTime((HANDLE)ops->hidden.windowsio.h, &tc, &ta, &tw);
        return FileTimeToUInt64(tw);
#endif
    }
    else if (ops->type == SDL_RWOPS_STDFILE) {
#if HAVE_STDIO_H
        int fd = ::fileno(ops->hidden.stdio.fp);
        struct stat st;
        ::fstat(fd, &st);
        return (uint64)st.st_mtime;
#endif
    }

    throw UnreachablePlaceException(LINE_STR);
}

auto DiskFile::GetSize() const -> size_t
{
    Sint64 size = SDL_RWsize(_pImpl->Ops);
    return size <= 0 ? 0u : static_cast<size_t>(size);
}

#else

struct DiskFile::Impl
{
    FILE* File {};
    bool WriteThrough {};
};

DiskFile::DiskFile(string_view fname, bool write, bool write_through)
{
    FILE* f = ::fopen(string(fname).c_str(), write ? "wb" : "rb");
    if (!f) {
        if (write) {
            DiskFileSystem::MakeDirTree(_str(fname).extractDir());
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
    return !!_pImpl;
}

auto DiskFile::Read(void* buf, size_t len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    if (len == 0u) {
        return true;
    }

    return ::fread(buf, sizeof(char), len, _pImpl->File) == len;
}

auto DiskFile::Write(const void* buf, size_t len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (len == 0u) {
        return true;
    }

    bool result = (::fwrite(buf, len, 1, _pImpl->File) == 1);
    if (result && _pImpl->WriteThrough) {
        ::fflush(_pImpl->File);
    }
    return result;
}

auto DiskFile::SetPos(int offset, DiskFileSeek origin) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::fseek(_pImpl->File, offset, (int)origin) == 0;
}

auto DiskFile::GetPos() const -> size_t
{
    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return static_cast<size_t>(::ftell(_pImpl->File));
}

auto DiskFile::GetWriteTime() const -> uint64
{
    RUNTIME_ASSERT(_pImpl);

    int fd = ::fileno(_pImpl->File);
    struct stat st;
    ::fstat(fd, &st);
    return static_cast<uint64>(st.st_mtime);
}

auto DiskFile::GetSize() const -> size_t
{
    RUNTIME_ASSERT(_pImpl);

    int fd = ::fileno(_pImpl->File);
    struct stat st;
    ::fstat(fd, &st);
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
    auto query = string(path) + "*";
    if (!ext.empty()) {
        query = "." + string(ext);
    }

    WIN32_FIND_DATAW fd;
    auto* h = ::FindFirstFileW(WinMultiByteToWideChar(query).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    _pImpl = std::make_unique<Impl>();
    _pImpl->FindHandle = h;
    _pImpl->FindData = fd;

    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u && (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)) {
        (*this)++;
    }
    else {
        _findDataValid = true;
    }
}

DiskFind::~DiskFind()
{
    if (_pImpl) {
        ::FindClose(_pImpl->FindHandle);
    }
}

DiskFind::DiskFind(DiskFind&&) noexcept = default;

auto DiskFind::operator++(int) -> DiskFind&
{
    _findDataValid = false;

    if (::FindNextFileW(_pImpl->FindHandle, &_pImpl->FindData) == 0) {
        return *this;
    }

    if ((_pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u && (wcscmp(_pImpl->FindData.cFileName, L".") == 0 || wcscmp(_pImpl->FindData.cFileName, L"..") == 0)) {
        return (*this)++;
    }

    _findDataValid = true;
    return *this;
}

DiskFind::operator bool() const
{
    return _findDataValid;
}

auto DiskFind::IsDir() const -> bool
{
    RUNTIME_ASSERT(_findDataValid);

    return (_pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u;
}

auto DiskFind::GetPath() const -> string
{
    RUNTIME_ASSERT(_findDataValid);

    return WinWideCharToMultiByte(_pImpl->FindData.cFileName);
}

auto DiskFind::GetFileSize() const -> size_t
{
    RUNTIME_ASSERT(_findDataValid);
    RUNTIME_ASSERT(!(_pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));

    return _pImpl->FindData.nFileSizeLow;
}

auto DiskFind::GetWriteTime() const -> uint64
{
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
    if (_pImpl) {
        closedir(_pImpl->Dir);
    }
}

DiskFind::DiskFind(DiskFind&&) noexcept = default;

auto DiskFind::operator++(int) -> DiskFind&
{
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
    return _findDataValid;
}

auto DiskFind::IsDir() const -> bool
{
    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->IsDir;
}

auto DiskFind::GetPath() const -> string
{
    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->Ent->d_name;
}

auto DiskFind::GetFileSize() const -> size_t
{
    RUNTIME_ASSERT(_findDataValid);
    RUNTIME_ASSERT(!_pImpl->IsDir);

    return _pImpl->Size;
}

auto DiskFind::GetWriteTime() const -> uint64
{
    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->WriteTime;
}
#endif

auto DiskFile::Write(string_view str) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (!str.empty()) {
        return Write(str.data(), str.length());
    }
    return true;
}

auto DiskFile::Write(const_span<uchar> data) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (!data.empty()) {
        return Write(data.data(), data.size());
    }
    return true;
}

#if !FO_IOS
auto DiskFileSystem::DeleteFile(string_view fname) -> bool
{
    std::error_code ec;
    std::filesystem::remove(fname, ec);
    return !std::filesystem::exists(fname, ec) && !ec;
}

auto DiskFileSystem::CopyFile(string_view fname, string_view copy_fname) -> bool
{
    std::error_code ec;
    return std::filesystem::copy_file(fname, copy_fname, ec);
}

auto DiskFileSystem::RenameFile(string_view fname, string_view new_fname) -> bool
{
    std::error_code ec;
    std::filesystem::rename(fname, new_fname, ec);
    return !ec;
}

void DiskFileSystem::ResolvePath(string& path)
{
    std::error_code ec;
    const auto resolved = std::filesystem::absolute(path, ec);
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
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
}

auto DiskFileSystem::DeleteDir(string_view dir) -> bool
{
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return !std::filesystem::exists(dir, ec) && !ec;
}

#else

auto DiskFileSystem::DeleteFile(string_view fname) -> bool
{
    return remove(string(fname).c_str()) != 0;
}

auto DiskFileSystem::CopyFile(string_view fname, string_view copy_fname) -> bool
{
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
    return rename(string(fname).c_str(), string(new_fname).c_str()) == 0;
}

void DiskFileSystem::ResolvePath(string& path)
{
    char* buf = realpath(path.c_str(), nullptr);
    if (buf) {
        path = buf;
        free(buf);
    }
}

void DiskFileSystem::MakeDirTree(string_view path)
{
    const string work = _str(path).normalizePathSlashes();
    for (size_t i = 0; i < work.length(); i++) {
        if (work[i] == '/') {
            auto path_part = work.substr(0, i);
            mkdir(path_part.c_str(), 0777);
        }
    }
}

auto DiskFileSystem::DeleteDir(string_view dir) -> bool
{
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
    for (auto find = DiskFileSystem::FindFiles(_str("{}{}", base_dir, cur_dir), ""); find; find++) {
        auto path = find.GetPath();
        if (path[0] != '.' && path[0] != '~') {
            if (find.IsDir()) {
                if (path[0] != '_' && include_subdirs) {
                    RecursiveDirLook(base_dir, _str("{}{}/", cur_dir, path), include_subdirs, ext, visitor);
                }
            }
            else {
                if (ext.empty() || _str(path).getFileExtension() == ext) {
                    visitor(_str("{}{}", cur_dir, path), find.GetFileSize(), find.GetWriteTime());
                }
            }
        }
    }
}

void DiskFileSystem::IterateDir(string_view path, string_view ext, bool include_subdirs, FileVisitor visitor)
{
    RecursiveDirLook(path, "", include_subdirs, ext, visitor);
}

void DiskFileSystem::RemoveBuildHashFile(string_view hash_name)
{
    const auto build_hash_deleted = DeleteFile(_str("{}.build-hash", hash_name));
    RUNTIME_ASSERT(build_hash_deleted);
}

void DiskFileSystem::CreateBuildHashFile(string_view hash_name)
{
    auto build_hash_file = OpenFile(_str("{}.build-hash", hash_name), true, true);
    RUNTIME_ASSERT(build_hash_file);
    const auto build_hash = string(FO_BUILD_HASH);
    const auto build_hash_writed = build_hash_file.Write(build_hash);
    RUNTIME_ASSERT(build_hash_writed);
}
