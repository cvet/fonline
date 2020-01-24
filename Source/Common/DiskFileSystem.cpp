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
#include "Testing.h"
#include "WinApi_Include.h"

#ifdef FO_WINDOWS
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef FO_ANDROID
#include "SDL.h"
#endif

const string DiskFileSystem::InitialDir {};
struct InitialDirInit
{
    InitialDirInit()
    {
#ifdef FO_WINDOWS
        wchar_t dir_buf[16384];
        GetCurrentDirectoryW(sizeof(dir_buf), dir_buf);
        const_cast<string&>(DiskFileSystem::InitialDir) = _str().parseWideChar(dir_buf);
#else
        char dir_buf[16384];
        char* r = getcwd(dir_buf, sizeof(dir_buf));
        UNUSED_VARIABLE(r);
        const_cast<string&>(DiskFileSystem::InitialDir) = dir_buf;
#endif
    }
};
static InitialDirInit CrtInitialDirInit;

DiskFile DiskFileSystem::OpenFile(const string& fname, bool write, bool write_through)
{
    return DiskFile(fname, write, write_through);
}

DiskFind DiskFileSystem::FindFiles(const string& path, const string& ext)
{
    return DiskFind(path, ext);
}

#ifdef FO_WINDOWS
static uint64 FileTimeToUInt64(FILETIME ft)
{
    union
    {
        FILETIME ft;
        ULARGE_INTEGER ul;
    } t;
    t.ft = ft;
    return PACKUINT64(t.ul.HighPart, t.ul.LowPart);
}

static std::wstring MBtoWC(const string& mb)
{
    return _str(mb).replace('/', '\\').toWideChar();
}

static string WCtoMB(const wchar_t* wc)
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
    if (write)
    {
        auto try_create = [&fname, &write_through]() {
            return ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr);
        };

        h = try_create();
        if (h == INVALID_HANDLE_VALUE)
        {
            DiskFileSystem::MakeDirTree(fname);
            h = try_create();
        }
    }
    else
    {
        auto try_create = [&fname]() {
            return ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        };

        h = try_create();
        if (h == INVALID_HANDLE_VALUE)
            h = try_create();
    }
    if (h == INVALID_HANDLE_VALUE)
        return;

    pImpl = std::make_unique<Impl>();
    pImpl->FileHandle = h;
    openedForWriting = write;
}

DiskFile::~DiskFile()
{
    if (pImpl)
        ::CloseHandle(pImpl->FileHandle);
}

DiskFile::DiskFile(DiskFile&&)
{
}

DiskFile::operator bool() const
{
    return !!pImpl;
}

bool DiskFile::Read(void* buf, uint len)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);
    RUNTIME_ASSERT(len);

    DWORD br = 0;
    return ::ReadFile(pImpl->FileHandle, buf, len, &br, nullptr) && br == len;
}

bool DiskFile::Write(const void* buf, uint len)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(openedForWriting);
    RUNTIME_ASSERT(len);

    DWORD bw = 0;
    return ::WriteFile(pImpl->FileHandle, buf, len, &bw, nullptr) && bw == len;
}

bool DiskFile::Write(const string& str)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(openedForWriting);

    if (str.empty())
        return true;

    DWORD len = (DWORD)str.length();
    DWORD bw = 0;
    return ::WriteFile(pImpl->FileHandle, str.c_str(), len, &bw, nullptr) && bw == len;
}

bool DiskFile::SetPos(int offset, DiskFileSeek origin)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);

    return ::SetFilePointer(pImpl->FileHandle, offset, nullptr, (int)origin) != INVALID_SET_FILE_POINTER;
}

uint DiskFile::GetPos()
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);

    return (uint)::SetFilePointer(pImpl->FileHandle, 0, nullptr, FILE_CURRENT);
}

uint64 DiskFile::GetWriteTime()
{
    RUNTIME_ASSERT(pImpl);

    FILETIME tc, ta, tw;
    if (!::GetFileTime(pImpl->FileHandle, &tc, &ta, &tw))
        return 0;
    return FileTimeToUInt64(tw);
}

uint DiskFile::GetSize()
{
    RUNTIME_ASSERT(pImpl);

    DWORD high;
    return ::GetFileSize(pImpl->FileHandle, &high);
}

#elif defined(FO_ANDROID)

struct DiskFile::Impl
{
    SDL_RWops* Ops {};
    bool WriteThrough {};
};

DiskFile::DiskFile(const string& fname, bool write, bool write_through)
{
    SDL_RWops* ops = SDL_RWFromFile(fname.c_str(), write ? "wb" : "rb");
    if (!ops)
    {
        if (write)
            DiskFileSystem::MakeDirTree(fname);

        ops = SDL_RWFromFile(fname.c_str(), write ? "wb" : "rb");
    }
    if (!ops)
        return;

    pImpl = std::make_unique<Impl>();
    pImpl->Ops = ops;
    pImpl->WriteThrough = write_through;
    openedForWriting = write;
}

DiskFile::~DiskFile()
{
    if (pImpl)
        SDL_RWclose(pImpl->Ops);
}

DiskFile::DiskFile(DiskFile&&)
{
}

DiskFile::operator bool() const
{
    return !!pImpl;
}

bool DiskFile::Read(void* buf, uint len)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);
    RUNTIME_ASSERT(len);

    return (uint)SDL_RWread(pImpl->Ops, buf, sizeof(char), len) == len;
}

bool DiskFile::Write(const void* buf, uint len)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(openedForWriting);
    RUNTIME_ASSERT(len);

    SDL_RWops* ops = pImpl->Ops;
    bool result = ((uint)SDL_RWwrite(ops, buf, sizeof(char), len) == len);
    if (result && pImpl->WriteThrough)
    {
        if (ops->type == SDL_RWOPS_WINFILE)
        {
#ifdef __WIN32__
            FlushFileBuffers((HANDLE)ops->hidden.windowsio.h);
#endif
        }
        else if (ops->type == SDL_RWOPS_STDFILE)
        {
#ifdef HAVE_STDIO_H
            fflush(ops->hidden.stdio.fp);
#endif
        }
    }
    return result;
}

bool DiskFile::Write(const string& str)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(openedForWriting);

    if (!str.empty())
        return Write(str.c_str(), (uint)str.length());
    return true;
}

bool DiskFile::SetPos(int offset, DiskFileSeek origin)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);

    return SDL_RWseek(pImpl->Ops, offset, (int)origin) != -1;
}

uint DiskFile::GetPos()
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);

    return (uint)SDL_RWtell(pImpl->Ops);
}

uint64 DiskFile::GetWriteTime()
{
    RUNTIME_ASSERT(pImpl);

#if !defined(__WIN32__) && !defined(HAVE_STDIO_H)
#error __WIN32__ or HAVE_STDIO_H must be defined
#endif

    SDL_RWops* ops = pImpl->Ops;
    if (ops->type == SDL_RWOPS_WINFILE)
    {
#ifdef __WIN32__
        FILETIME tc, ta, tw;
        GetFileTime((HANDLE)ops->hidden.windowsio.h, &tc, &ta, &tw);
        return FileTimeToUInt64(tw);
#endif
    }
    else if (ops->type == SDL_RWOPS_STDFILE)
    {
#ifdef HAVE_STDIO_H
        int fd = fileno(ops->hidden.stdio.fp);
        struct stat st;
        fstat(fd, &st);
        return (uint64)st.st_mtime;
#endif
    }

    throw UnreachablePlaceException("Unreachable place");
}

uint DiskFile::GetSize()
{
    Sint64 size = SDL_RWsize(pImpl->Ops);
    return (uint)(size <= 0 ? 0 : size);
}

#else

struct DiskFile::Impl
{
    FILE* File {};
    bool WriteThrough {};
};

DiskFile::DiskFile(const string& fname, bool write, bool write_through)
{
    FILE* f = fopen(fname.c_str(), write ? "wb" : "rb");
    if (!f)
    {
        if (write)
            DiskFileSystem::MakeDirTree(fname);
        f = fopen(fname.c_str(), write ? "wb" : "rb");
    }
    if (!f)
        return;

    pImpl = std::make_unique<Impl>();
    pImpl->File = f;
    pImpl->WriteThrough = write_through;
    openedForWriting = write;
}

DiskFile::~DiskFile()
{
    if (pImpl)
    {
        fclose(pImpl->File);

#ifdef FO_WEB
        if (openedForWriting)
            EM_ASM(FS.syncfs(function(err) {}));
#endif
    }
}

DiskFile::DiskFile(DiskFile&&)
{
}

DiskFile::operator bool() const
{
    return !!pImpl;
}

bool DiskFile::Read(void* buf, uint len)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);
    RUNTIME_ASSERT(len);

    return fread(buf, sizeof(char), len, pImpl->File) == len;
}

bool DiskFile::Write(const void* buf, uint len)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(openedForWriting);
    RUNTIME_ASSERT(len);

    bool result = ((uint)fwrite(buf, len, 1, pImpl->File) == 1);
    if (result && pImpl->WriteThrough)
        fflush(pImpl->File);
    return result;
}

bool DiskFile::Write(const string& str)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(openedForWriting);

    if (!str.empty())
        return Write(str.c_str(), (uint)str.length());
    return true;
}

bool DiskFile::SetPos(int offset, DiskFileSeek origin)
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);

    return fseek(pImpl->File, offset, (int)origin) == 0;
}

uint DiskFile::GetPos()
{
    RUNTIME_ASSERT(pImpl);
    RUNTIME_ASSERT(!openedForWriting);

    return (uint)ftell(pImpl->File);
}

uint64 DiskFile::GetWriteTime()
{
    RUNTIME_ASSERT(pImpl);

    int fd = fileno(pImpl->File);
    struct stat st;
    fstat(fd, &st);
    return (uint64)st.st_mtime;
}

uint DiskFile::GetSize()
{
    RUNTIME_ASSERT(pImpl);

    int fd = fileno(pImpl->File);
    struct stat st;
    fstat(fd, &st);
    return (uint)st.st_size;
}
#endif

#ifdef FO_WINDOWS
bool DiskFileSystem::DeleteFile(const string& fname)
{
    return ::DeleteFileW(MBtoWC(fname).c_str()) != FALSE;
}

bool DiskFileSystem::IsFileExists(const string& fname)
{
    return !_waccess(MBtoWC(fname).c_str(), 0);
}

bool DiskFileSystem::CopyFile(const string& fname, const string& copy_fname)
{
    return ::CopyFileW(MBtoWC(fname).c_str(), MBtoWC(copy_fname).c_str(), FALSE) != FALSE;
}

bool DiskFileSystem::RenameFile(const string& fname, const string& new_fname)
{
    return ::MoveFileW(MBtoWC(fname).c_str(), MBtoWC(new_fname).c_str()) != FALSE;
}

struct DiskFind::Impl
{
    HANDLE FindHandle {};
    WIN32_FIND_DATAW FindData {};
};

DiskFind::DiskFind(const string& path, const string& ext)
{
    string query = path + "*";
    if (!ext.empty())
        query = "." + ext;

    WIN32_FIND_DATAW fd;
    HANDLE h = ::FindFirstFileW(MBtoWC(query).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;

    pImpl = std::make_unique<Impl>();
    pImpl->FindHandle = h;
    pImpl->FindData = fd;

    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")))
    {
        (*this)++;
    }
    else
    {
        findDataValid = true;
    }
}

DiskFind::~DiskFind()
{
    if (pImpl)
        ::CloseHandle(pImpl->FindHandle);
}

DiskFind::DiskFind(DiskFind&&)
{
}

DiskFind& DiskFind::operator++(int)
{
    findDataValid = false;

    if (!::FindNextFileW(pImpl->FindHandle, &pImpl->FindData))
        return *this;

    if ((pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        (!wcscmp(pImpl->FindData.cFileName, L".") || !wcscmp(pImpl->FindData.cFileName, L"..")))
        return (*this)++;

    findDataValid = true;
    return *this;
}

DiskFind::operator bool() const
{
    return findDataValid;
}

bool DiskFind::IsDir()
{
    RUNTIME_ASSERT(findDataValid);

    return pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

string DiskFind::GetPath()
{
    RUNTIME_ASSERT(findDataValid);

    return WCtoMB(pImpl->FindData.cFileName);
}

uint DiskFind::GetFileSize()
{
    RUNTIME_ASSERT(findDataValid);
    RUNTIME_ASSERT(!(pImpl->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));

    return pImpl->FindData.nFileSizeLow;
}

uint64 DiskFind::GetWriteTime()
{
    RUNTIME_ASSERT(findDataValid);

    return FileTimeToUInt64(pImpl->FindData.ftLastWriteTime);
}

#else

bool DiskFileSystem::DeleteFile(const string& fname)
{
    return remove(fname.c_str());
}

bool DiskFileSystem::IsFileExists(const string& fname)
{
    return !access(fname.c_str(), 0);
}

bool DiskFileSystem::CopyFile(const string& fname, const string& copy_fname)
{
    bool ok = false;
    FILE* from = fopen(fname.c_str(), "rb");
    if (from)
    {
        FILE* to = fopen(copy_fname.c_str(), "wb");
        if (to)
        {
            ok = true;
            char buf[BUFSIZ];
            while (!feof(from))
            {
                size_t rb = fread(buf, 1, BUFSIZ, from);
                size_t rw = fwrite(buf, 1, rb, to);
                if (!rb || rb != rw)
                {
                    ok = false;
                    break;
                }
            }
            fclose(to);
            if (!ok)
                DeleteFile(copy_fname);
        }
        fclose(from);
    }
    return ok;
}

bool DiskFileSystem::RenameFile(const string& fname, const string& new_fname)
{
    return !rename(fname.c_str(), new_fname.c_str());
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
    if (!d)
        return;

    pImpl = std::make_unique<Impl>();
    pImpl->Dir = d;
    pImpl->Path = path;
    pImpl->Ext = ext;

    if (!pImpl->Path.empty() && pImpl->Path.back() != '/')
        pImpl->Path += "/";
    if (!ext.empty())
        pImpl->Ext = ext;

    // Read first entry
    (*this)++;
}

DiskFind::~DiskFind()
{
    if (pImpl)
        closedir(pImpl->Dir);
}

DiskFind::DiskFind(DiskFind&&)
{
}

DiskFind& DiskFind::operator++(int)
{
    findDataValid = false;

    pImpl->Ent = readdir(pImpl->Dir);
    if (!pImpl->Ent)
        return *this;

    // Skip '.' and '..'
    if (!strcmp(pImpl->Ent->d_name, ".") || !strcmp(pImpl->Ent->d_name, ".."))
        return (*this)++;

    // Read entire information
    struct stat st;
    if (stat((pImpl->Path + pImpl->Ent->d_name).c_str(), &st))
        return (*this)++;
    if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode))
        return (*this)++;

    // Check extension
    if (!pImpl->Ext.empty())
    {
        // Skip dirs
        if (S_ISDIR(st.st_mode))
            return (*this)++;

        // Compare extension
        const char* ext = nullptr;
        for (const char* s = pImpl->Ent->d_name; *s; s++)
            if (*s == '.')
                ext = s + 1;

        if (!ext || !*ext || strcasecmp(ext, pImpl->Ext.c_str()))
            return (*this)++;
    }

    pImpl->IsDir = S_ISDIR(st.st_mode);
    pImpl->Size = (uint)st.st_size;
    pImpl->WriteTime = (uint64)st.st_mtime;
    findDataValid = true;
    return *this;
}

DiskFind::operator bool() const
{
    return findDataValid;
}

bool DiskFind::IsDir()
{
    RUNTIME_ASSERT(findDataValid);

    return pImpl->IsDir;
}

string DiskFind::GetPath()
{
    RUNTIME_ASSERT(findDataValid);

    return pImpl->Ent->d_name;
}

uint DiskFind::GetFileSize()
{
    RUNTIME_ASSERT(findDataValid);
    RUNTIME_ASSERT(!pImpl->IsDir);

    return pImpl->Size;
}

uint64 DiskFind::GetWriteTime()
{
    RUNTIME_ASSERT(findDataValid);

    return pImpl->WriteTime;
}
#endif

void DiskFileSystem::ResolvePath(string& path)
{
#ifdef FO_WINDOWS
    DWORD len = ::GetFullPathNameW(MBtoWC(path).c_str(), 0, nullptr, nullptr);
    vector<wchar_t> buf(len);
    if (::GetFullPathNameW(MBtoWC(path).c_str(), len, &buf[0], nullptr))
    {
        path = WCtoMB(&buf[0]);
        path = _str(path).normalizePathSlashes();
    }

#else
    char* buf = realpath(path.c_str(), nullptr);
    if (buf)
    {
        path = buf;
        free(buf);
    }
#endif
}

void DiskFileSystem::MakeDirTree(const string& path)
{
    string work = _str(path).normalizePathSlashes();
    for (size_t i = 0; i < work.length(); i++)
    {
        if (work[i] == '/')
        {
            string path_part = work.substr(0, i);
#ifdef FO_WINDOWS
            ::CreateDirectoryW(MBtoWC(path_part).c_str(), nullptr);
#else
            mkdir(path_part.c_str(), 0777);
#endif
        }
    }
}

bool DiskFileSystem::DeleteDir(const string& dir)
{
    MakeDirTree(dir);

    vector<string> file_paths;
    for (auto find = DiskFind(dir, ""); find; find++)
        if (!find.IsDir())
            file_paths.emplace_back(find.GetPath());

    for (auto& fp : file_paths)
        if (!DeleteFile(fp))
            return false;

#ifdef FO_WINDOWS
    return RemoveDirectoryW(_str(dir).toWideChar().c_str()) != FALSE;
#else
    return rmdir(dir.c_str()) == 0;
#endif
}

bool DiskFileSystem::SetCurrentDir(const string& dir)
{
    string resolved_dir = _str(dir).formatPath();
    ResolvePath(resolved_dir);

#ifdef FO_WINDOWS
    return SetCurrentDirectoryW(_str(resolved_dir).toWideChar().c_str()) != FALSE;
#else
    return chdir(resolved_dir.c_str()) == 0;
#endif
}

void DiskFileSystem::ResetCurDir()
{
#ifdef FO_WINDOWS
    SetCurrentDirectoryW(_str(InitialDir).toWideChar().c_str());
#else
    int r = chdir(InitialDir.c_str());
    UNUSED_VARIABLE(r);
#endif
}

string DiskFileSystem::GetExePath()
{
#ifdef FO_WINDOWS
    wchar_t buf[4096] {};
    DWORD r = GetModuleFileNameW(GetModuleHandle(nullptr), buf, sizeof(buf));
    return r ? _str().parseWideChar(buf) : "";
#else
    return "";
#endif
}

static void RecursiveDirLook(const string& base_dir, const string& cur_dir, bool include_subdirs, const string& ext,
    DiskFileSystem::FileVisitor& visitor)
{
    for (auto find = DiskFileSystem::FindFiles(base_dir + cur_dir, ""); find; find++)
    {
        string path = find.GetPath();
        if (path[0] != '.' && path[0] != '~')
        {
            if (find.IsDir())
            {
                if (path[0] != '_' && include_subdirs)
                    RecursiveDirLook(base_dir, cur_dir + path + "/", include_subdirs, ext, visitor);
            }
            else
            {
                if (ext.empty() || _str(path).getFileExtension() == ext)
                    visitor(cur_dir + path + "/", find.GetFileSize(), find.GetWriteTime());
            }
        }
    }
}

void DiskFileSystem::IterateDir(const string& path, const string& ext, bool include_subdirs, FileVisitor visitor)
{
    RecursiveDirLook(_str(path).formatPath(), "", include_subdirs, ext, visitor);
}
