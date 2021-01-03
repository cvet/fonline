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
#include "WinApi-Include.h"

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

struct DiskFileSystemData
{
    DiskFileSystemData()
    {
        const auto buf_len = 16384 * 2;
#if FO_WINDOWS
        const auto dir_buf = std::make_unique<wchar_t[]>(buf_len);
        ::GetCurrentDirectoryW(buf_len, dir_buf.get());
        InitialDir = _str().parseWideChar(dir_buf.get());
#else
        const auto dir_buf = std::make_unique<char[]>(buf_len);
        char* r = getcwd(dir_buf.get(), buf_len);
        UNUSED_VARIABLE(r);
        InitialDir = dir_buf.get();
#endif
    }

    string InitialDir {};
};
GLOBAL_DATA(DiskFileSystemData, Data);

auto DiskFileSystem::OpenFile(const string& fname, bool write) -> DiskFile
{
    return DiskFile(fname, write, false);
}

auto DiskFileSystem::OpenFile(const string& fname, bool write, bool write_through) -> DiskFile
{
    return DiskFile(fname, write, write_through);
}

auto DiskFileSystem::FindFiles(const string& path, const string& ext) -> DiskFind
{
    return DiskFind(path, ext);
}

#if FO_WINDOWS
static auto FileTimeToUInt64(FILETIME ft) -> uint64
{
    return static_cast<uint64>(ft.dwHighDateTime) << 32 | static_cast<uint64>(ft.dwLowDateTime);
}

static auto WinMultiByteToWideChar(const string& mb) -> std::wstring
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

DiskFile::DiskFile(const string& fname, bool write, bool write_through)
{
    HANDLE h;

    if (write) {
        auto try_create = [&fname, &write_through]() { return ::CreateFileW(WinMultiByteToWideChar(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr); };

        h = try_create();
        if (h == INVALID_HANDLE_VALUE) {
            DiskFileSystem::MakeDirTree(fname);
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

auto DiskFile::Read(void* buf, uint len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);
    RUNTIME_ASSERT(len);

    DWORD br = 0;
    return ::ReadFile(_pImpl->FileHandle, buf, len, &br, nullptr) != 0 && br == len;
}

auto DiskFile::Write(const void* buf, uint len) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);
    RUNTIME_ASSERT(len);

    DWORD bw = 0;
    return ::WriteFile(_pImpl->FileHandle, buf, len, &bw, nullptr) != 0 && bw == len;
}

auto DiskFile::Write(const string& str) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (str.empty()) {
        return true;
    }

    const auto len = static_cast<DWORD>(str.length());
    DWORD bw = 0;
    return ::WriteFile(_pImpl->FileHandle, str.c_str(), len, &bw, nullptr) != 0 && bw == len;
}

auto DiskFile::SetPos(int offset, DiskFileSeek origin) -> bool
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::SetFilePointer(_pImpl->FileHandle, offset, nullptr, static_cast<DWORD>(origin)) != INVALID_SET_FILE_POINTER;
}

auto DiskFile::GetPos() const -> uint
{
    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    const auto pos = ::SetFilePointer(_pImpl->FileHandle, 0, nullptr, FILE_CURRENT);
    return static_cast<uint>(pos);
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

auto DiskFile::GetSize() const -> uint
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

DiskFile::DiskFile(const string& fname, bool write, bool write_through)
{
    SDL_RWops* ops = SDL_RWFromFile(fname.c_str(), write ? "wb" : "rb");
    if (!ops) {
        if (write) {
            DiskFileSystem::MakeDirTree(fname);
        }

        ops = SDL_RWFromFile(fname.c_str(), write ? "wb" : "rb");
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

bool DiskFile::Read(void* buf, uint len)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);
    RUNTIME_ASSERT(len);

    return static_cast<uint>(SDL_RWread(_pImpl->Ops, buf, sizeof(char), len)) == len;
}

bool DiskFile::Write(const void* buf, uint len)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);
    RUNTIME_ASSERT(len);

    SDL_RWops* ops = _pImpl->Ops;
    bool result = (static_cast<uint>(SDL_RWwrite(ops, buf, sizeof(char), len)) == len);
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

bool DiskFile::Write(const string& str)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (!str.empty()) {
        return Write(str.c_str(), static_cast<uint>(str.length()));
    }
    return true;
}

bool DiskFile::SetPos(int offset, DiskFileSeek origin)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return SDL_RWseek(_pImpl->Ops, offset, static_cast<int>(origin)) != -1;
}

uint DiskFile::GetPos() const
{
    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return static_cast<uint>(SDL_RWtell(_pImpl->Ops));
}

uint64 DiskFile::GetWriteTime() const
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

uint DiskFile::GetSize() const
{
    Sint64 size = SDL_RWsize(_pImpl->Ops);
    return size <= 0 ? 0u : static_cast<uint>(size);
}

#else

struct DiskFile::Impl
{
    FILE* File {};
    bool WriteThrough {};
};

DiskFile::DiskFile(const string& fname, bool write, bool write_through)
{
    FILE* f = ::fopen(fname.c_str(), write ? "wb" : "rb");
    if (!f) {
        if (write) {
            DiskFileSystem::MakeDirTree(fname);
        }
        f = ::fopen(fname.c_str(), write ? "wb" : "rb");
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

bool DiskFile::Read(void* buf, uint len)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);
    RUNTIME_ASSERT(len);

    return ::fread(buf, sizeof(char), len, _pImpl->File) == len;
}

bool DiskFile::Write(const void* buf, uint len)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);
    RUNTIME_ASSERT(len);

    bool result = (::fwrite(buf, len, 1, _pImpl->File) == 1);
    if (result && _pImpl->WriteThrough) {
        ::fflush(_pImpl->File);
    }
    return result;
}

bool DiskFile::Write(const string& str)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(_openedForWriting);

    if (!str.empty()) {
        return Write(str.c_str(), (uint)str.length());
    }
    return true;
}

bool DiskFile::SetPos(int offset, DiskFileSeek origin)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return ::fseek(_pImpl->File, offset, (int)origin) == 0;
}

uint DiskFile::GetPos() const
{
    RUNTIME_ASSERT(_pImpl);
    RUNTIME_ASSERT(!_openedForWriting);

    return static_cast<uint>(::ftell(_pImpl->File));
}

uint64 DiskFile::GetWriteTime() const
{
    RUNTIME_ASSERT(_pImpl);

    int fd = ::fileno(_pImpl->File);
    struct stat st;
    ::fstat(fd, &st);
    return static_cast<uint64>(st.st_mtime);
}

uint DiskFile::GetSize() const
{
    RUNTIME_ASSERT(_pImpl);

    int fd = ::fileno(_pImpl->File);
    struct stat st;
    ::fstat(fd, &st);
    return static_cast<uint>(st.st_size);
}
#endif

#if FO_WINDOWS
auto DiskFileSystem::DeleteFile(const string& fname) -> bool
{
    return ::DeleteFileW(WinMultiByteToWideChar(fname).c_str()) != FALSE;
}

auto DiskFileSystem::IsFileExists(const string& fname) -> bool
{
    return ::_waccess(WinMultiByteToWideChar(fname).c_str(), 0) == 0;
}

auto DiskFileSystem::CopyFile(const string& fname, const string& copy_fname) -> bool
{
    return ::CopyFileW(WinMultiByteToWideChar(fname).c_str(), WinMultiByteToWideChar(copy_fname).c_str(), FALSE) != FALSE;
}

auto DiskFileSystem::RenameFile(const string& fname, const string& new_fname) -> bool
{
    const DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
    return !(::MoveFileExW(WinMultiByteToWideChar(fname).c_str(), WinMultiByteToWideChar(new_fname).c_str(), flags) == 0);
}

struct DiskFind::Impl
{
    HANDLE FindHandle {};
    WIN32_FIND_DATAW FindData {};
};

DiskFind::DiskFind(const string& path, const string& ext)
{
    auto query = path + "*";
    if (!ext.empty()) {
        query = "." + ext;
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
        ::CloseHandle(_pImpl->FindHandle);
    }
}

DiskFind::DiskFind(DiskFind&&) noexcept = default;

DiskFind& DiskFind::operator++(int)
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

auto DiskFind::GetFileSize() const -> uint
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

bool DiskFileSystem::DeleteFile(const string& fname)
{
    return remove(fname.c_str()) != 0;
}

bool DiskFileSystem::IsFileExists(const string& fname)
{
    return access(fname.c_str(), 0) == 0;
}

bool DiskFileSystem::CopyFile(const string& fname, const string& copy_fname)
{
    bool ok = false;
    FILE* from = fopen(fname.c_str(), "rb");
    if (from) {
        FILE* to = fopen(copy_fname.c_str(), "wb");
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

bool DiskFileSystem::RenameFile(const string& fname, const string& new_fname)
{
    return rename(fname.c_str(), new_fname.c_str()) == 0;
}

struct DiskFind::Impl
{
    DIR* Dir {};
    string Path {};
    string Ext {};
    dirent* Ent {};
    bool IsDir {};
    uint Size {};
    uint64 WriteTime {};
};

DiskFind::DiskFind(const string& path, const string& ext)
{
    DIR* d = opendir(path.c_str());
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

DiskFind& DiskFind::operator++(int)
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
    _pImpl->Size = (uint)st.st_size;
    _pImpl->WriteTime = (uint64)st.st_mtime;
    _findDataValid = true;
    return *this;
}

DiskFind::operator bool() const
{
    return _findDataValid;
}

bool DiskFind::IsDir() const
{
    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->IsDir;
}

string DiskFind::GetPath() const
{
    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->Ent->d_name;
}

uint DiskFind::GetFileSize() const
{
    RUNTIME_ASSERT(_findDataValid);
    RUNTIME_ASSERT(!_pImpl->IsDir);

    return _pImpl->Size;
}

uint64 DiskFind::GetWriteTime() const
{
    RUNTIME_ASSERT(_findDataValid);

    return _pImpl->WriteTime;
}
#endif

void DiskFileSystem::ResolvePath(string& path)
{
#if FO_WINDOWS
    const auto len = ::GetFullPathNameW(WinMultiByteToWideChar(path).c_str(), 0, nullptr, nullptr);
    vector<wchar_t> buf(len);
    if (::GetFullPathNameW(WinMultiByteToWideChar(path).c_str(), len, &buf[0], nullptr) != 0u) {
        path = WinWideCharToMultiByte(&buf[0]);
        path = _str(path).normalizePathSlashes();
    }

#else
    char* buf = realpath(path.c_str(), nullptr);
    if (buf) {
        path = buf;
        free(buf);
    }
#endif
}

void DiskFileSystem::MakeDirTree(const string& path)
{
    string work = _str(path).normalizePathSlashes();
    for (size_t i = 0; i < work.length(); i++) {
        if (work[i] == '/') {
            auto path_part = work.substr(0, i);
#if FO_WINDOWS
            ::CreateDirectoryW(WinMultiByteToWideChar(path_part).c_str(), nullptr);
#else
            mkdir(path_part.c_str(), 0777);
#endif
        }
    }
}

auto DiskFileSystem::DeleteDir(const string& dir) -> bool
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

#if FO_WINDOWS
    return ::RemoveDirectoryW(_str(dir).toWideChar().c_str()) != FALSE;
#else
    return rmdir(dir.c_str()) == 0;
#endif
}

auto DiskFileSystem::SetCurrentDir(const string& dir) -> bool
{
    string resolved_dir = _str(dir).formatPath();
    ResolvePath(resolved_dir);

#if FO_WINDOWS
    return ::SetCurrentDirectoryW(_str(resolved_dir).toWideChar().c_str()) != FALSE;
#else
    return chdir(resolved_dir.c_str()) == 0;
#endif
}

void DiskFileSystem::ResetCurDir()
{
#if FO_WINDOWS
    ::SetCurrentDirectoryW(_str(Data->InitialDir).toWideChar().c_str());
#else
    int r = chdir(Data->InitialDir.c_str());
    UNUSED_VARIABLE(r);
#endif
}

auto DiskFileSystem::GetExePath() -> string
{
#if FO_WINDOWS
    const DWORD buf_len = 4096;
    wchar_t buf[buf_len] {};
    const auto r = ::GetModuleFileNameW(nullptr, buf, buf_len);
    return r != 0u ? _str().parseWideChar(buf).str() : string();
#else
    return string();
#endif
}

static void RecursiveDirLook(const string& base_dir, const string& cur_dir, bool include_subdirs, const string& ext, DiskFileSystem::FileVisitor& visitor)
{
    for (auto find = DiskFileSystem::FindFiles(base_dir + cur_dir, ""); find; find++) {
        auto path = find.GetPath();
        if (path[0] != '.' && path[0] != '~') {
            if (find.IsDir()) {
                if (path[0] != '_' && include_subdirs) {
                    RecursiveDirLook(base_dir, cur_dir + path + "/", include_subdirs, ext, visitor);
                }
            }
            else {
                if (ext.empty() || _str(path).getFileExtension() == ext) {
                    visitor(cur_dir + path, find.GetFileSize(), find.GetWriteTime());
                }
            }
        }
    }
}

void DiskFileSystem::IterateDir(const string& path, const string& ext, bool include_subdirs, FileVisitor visitor)
{
    RecursiveDirLook(path, "", include_subdirs, ext, visitor);
}
