#include "DiskFileSystem.h"
#include "StringUtils.h"
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

#ifdef FO_WINDOWS
struct DiskFileSystem::DiskFile
{
    HANDLE FileHandle {};
};

struct DiskFileSystem::DiskFind
{
    HANDLE FindHandle {};
};

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

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFile(const string& fname, bool write, bool write_through)
{
    HANDLE file;
    if (write)
    {
        file = ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            MakeDirectoryTree(fname);
            file = ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr);
        }
    }
    else
    {
        file = ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    }
    if (file == INVALID_HANDLE_VALUE)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {file}, [](DiskFile* file) {
        ::CloseHandle(file->FileHandle);
        delete file;
    });
}

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFileForAppend(const string& fname, bool write_through)
{
    HANDLE file = ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        MakeDirectoryTree(fname);
        file = ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr);
    }
    if (file == INVALID_HANDLE_VALUE)
        return nullptr;
    if (::SetFilePointer(file, 0, nullptr, SeekEnd) == INVALID_SET_FILE_POINTER)
    {
        ::CloseHandle(file);
        return nullptr;
    }

    return shared_ptr<DiskFile>(new DiskFile {file}, [](DiskFile* file) {
        ::CloseHandle(file->FileHandle);
        delete file;
    });
}

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFileForReadWrite(const string& fname, bool write_through)
{
    HANDLE file = ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        MakeDirectoryTree(fname);
        file = ::CreateFileW(MBtoWC(fname).c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr);
    }
    if (file == INVALID_HANDLE_VALUE)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {file}, [](DiskFile* file) {
        ::CloseHandle(file->FileHandle);
        delete file;
    });
}

bool DiskFileSystem::ReadFile(shared_ptr<DiskFile> file, void* buf, uint len, uint* rb)
{
    DWORD dw = 0;
    BOOL result = ::ReadFile(file->FileHandle, buf, len, &dw, nullptr);
    if (rb)
        *rb = dw;
    return result && dw == len;
}

bool DiskFileSystem::WriteFile(shared_ptr<DiskFile> file, const void* buf, uint len)
{
    DWORD dw = 0;
    return ::WriteFile(file->FileHandle, buf, len, &dw, nullptr) && dw == len;
}

bool DiskFileSystem::SetFilePointer(shared_ptr<DiskFile> file, int offset, DiskFileSeek origin)
{
    return ::SetFilePointer(file->FileHandle, offset, nullptr, origin) != INVALID_SET_FILE_POINTER;
}

uint DiskFileSystem::GetFilePointer(shared_ptr<DiskFile> file)
{
    return (uint)::SetFilePointer(file->FileHandle, 0, nullptr, FILE_CURRENT);
}

uint64 DiskFileSystem::GetFileWriteTime(shared_ptr<DiskFile> file)
{
    FILETIME tc, ta, tw;
    ::GetFileTime(file->FileHandle, &tc, &ta, &tw);
    return FileTimeToUInt64(tw);
}

uint DiskFileSystem::GetFileSize(shared_ptr<DiskFile> file)
{
    DWORD high;
    return ::GetFileSize(file->FileHandle, &high);
}

#elif defined(FO_ANDROID)

struct DiskFileSystem::DiskFile
{
    SDL_RWops* Ops;
    bool WriteThrough;
};

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFile(const string& fname, bool write, bool write_through)
{
    SDL_RWops* ops = SDL_RWFromFile(fname.c_str(), write ? "wb" : "rb");
    if (!ops && write)
    {
        MakeDirectoryTree(fname);
        ops = SDL_RWFromFile(fname.c_str(), "wb");
    }
    if (!ops)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {ops, write_through}, [](DiskFile* file) {
        SDL_RWclose(file->Ops);
        delete file;
    });
}

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFileForAppend(const string& fname, bool write_through)
{
    SDL_RWops* ops = SDL_RWFromFile(fname.c_str(), "ab");
    if (!ops)
    {
        MakeDirectoryTree(fname);
        ops = SDL_RWFromFile(fname.c_str(), "ab");
    }
    if (!ops)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {ops, write_through}, [](DiskFile* file) {
        SDL_RWclose(file->Ops);
        delete file;
    });
}

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFileForReadWrite(const string& fname, bool write_through)
{
    SDL_RWops* ops = SDL_RWFromFile(fname.c_str(), "r+b");
    if (!ops)
    {
        MakeDirectoryTree(fname);
        ops = SDL_RWFromFile(fname.c_str(), "r+b");
    }
    if (!ops)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {ops, write_through}, [](DiskFile* file) {
        SDL_RWclose(file->Ops);
        delete file;
    });
}

bool DiskFileSystem::ReadFile(shared_ptr<DiskFile> file, void* buf, uint len, uint* rb)
{
    uint rb_ = (uint)SDL_RWread(file->Ops, buf, sizeof(char), len);
    if (rb)
        *rb = rb_;
    return rb_ == len;
}

bool DiskFileSystem::WriteFile(shared_ptr<DiskFile> file, const void* buf, uint len)
{
    SDL_RWops* ops = file->Ops;
    bool result = ((uint)SDL_RWwrite(ops, buf, sizeof(char), len) == len);
    if (result && file->WriteThrough)
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

bool DiskFileSystem::SetFilePointer(shared_ptr<DiskFile> file, int offset, DiskFileSeek origin)
{
    return SDL_RWseek(file->Ops, offset, origin) != -1;
}

uint DiskFileSystem::GetFilePointer(shared_ptr<DiskFile> file)
{
    return (uint)SDL_RWtell(file->Ops);
}

uint64 DiskFileSystem::GetFileWriteTime(shared_ptr<DiskFile> file)
{
    SDL_RWops* ops = file->Ops;
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
    return (uint64)1;
}

uint DiskFileSystem::GetFileSize(shared_ptr<DiskFile> file)
{
    Sint64 size = SDL_RWsize(file->Ops);
    return (uint)(size <= 0 ? 0 : size);
}

#else

struct DiskFileSystem::DiskFile
{
    FILE* File;
    bool Write;
    bool WriteThrough;
};

static void CloseFile(DiskFileSystem::DiskFile* file)
{
    fclose(file->File);

#ifdef FO_WEB
    if (file->Write)
        EM_ASM(FS.syncfs(function(err) {}));
#endif

    delete file;
}

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFile(const string& fname, bool write, bool write_through)
{
    FILE* f = fopen(fname.c_str(), write ? "wb" : "rb");
    if (!f && write)
    {
        MakeDirectoryTree(fname);
        f = fopen(fname.c_str(), "wb");
    }
    if (!f)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {f, write, write_through}, CloseFile);
}

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFileForAppend(const string& fname, bool write_through)
{
    FILE* f = fopen(fname.c_str(), "ab");
    if (!f)
    {
        MakeDirectoryTree(fname);
        f = fopen(fname.c_str(), "ab");
    }
    if (!f)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {f, true, write_through}, CloseFile);
}

shared_ptr<DiskFileSystem::DiskFile> DiskFileSystem::OpenFileForReadWrite(const string& fname, bool write_through)
{
    FILE* f = fopen(fname.c_str(), "r+b");
    if (!f)
    {
        MakeDirectoryTree(fname);
        f = fopen(fname.c_str(), "r+b");
    }
    if (!f)
        return nullptr;

    return shared_ptr<DiskFile>(new DiskFile {f, true, write_through}, CloseFile);
}

bool DiskFileSystem::ReadFile(shared_ptr<DiskFile> file, void* buf, uint len, uint* rb)
{
    uint rb_ = (uint)fread(buf, sizeof(char), len, file->File);
    if (rb)
        *rb = rb_;
    return rb_ == len;
}

bool DiskFileSystem::WriteFile(shared_ptr<DiskFile> file, const void* buf, uint len)
{
    bool result = ((uint)fwrite(buf, sizeof(char), len, file->File) == len);
    if (result && file->WriteThrough)
        fflush(file->File);
    return result;
}

bool DiskFileSystem::SetFilePointer(shared_ptr<DiskFile> file, int offset, DiskFileSeek origin)
{
    return fseek(file->File, offset, origin) == 0;
}

uint DiskFileSystem::GetFilePointer(shared_ptr<DiskFile> file)
{
    return (uint)ftell(file->File);
}

uint64 DiskFileSystem::GetFileWriteTime(shared_ptr<DiskFile> file)
{
    int fd = fileno(file->File);
    struct stat st;
    fstat(fd, &st);
    return (uint64)st.st_mtime;
}

uint DiskFileSystem::GetFileSize(shared_ptr<DiskFile> file)
{
    int fd = fileno(file->File);
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

shared_ptr<DiskFileSystem::DiskFind> DiskFileSystem::FindFirstFile(
    const string& path, const string& extension, string* fname, uint* fsize, uint64* wtime, bool* is_dir)
{
    string query = path + "*";
    if (!extension.empty())
        query = "." + extension;

    WIN32_FIND_DATAW wfd;
    HANDLE h = ::FindFirstFileW(MBtoWC(query).c_str(), &wfd);
    if (h == INVALID_HANDLE_VALUE)
        return nullptr;

    auto find = shared_ptr<DiskFind>(new DiskFind {h}, [](DiskFind* find) {
        ::CloseHandle(find->FindHandle);
        delete find;
    });

    if (fname)
        *fname = WCtoMB(wfd.cFileName);
    if (is_dir)
        *is_dir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (fsize)
        *fsize = wfd.nFileSizeLow;
    if (wtime)
        *wtime = FileTimeToUInt64(wfd.ftLastWriteTime);
    if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        (!wcscmp(wfd.cFileName, L".") || !wcscmp(wfd.cFileName, L"..")))
        if (!FindNextFile(find, fname, fsize, wtime, is_dir))
            return nullptr;

    return find;
}

bool DiskFileSystem::FindNextFile(shared_ptr<DiskFind> find, string* fname, uint* fsize, uint64* wtime, bool* is_dir)
{
    WIN32_FIND_DATAW wfd;
    if (!::FindNextFileW(find->FindHandle, &wfd))
        return false;

    if (fname)
        *fname = WCtoMB(wfd.cFileName);
    if (is_dir)
        *is_dir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (fsize)
        *fsize = wfd.nFileSizeLow;
    if (wtime)
        *wtime = FileTimeToUInt64(wfd.ftLastWriteTime);
    if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        (!wcscmp(wfd.cFileName, L".") || !wcscmp(wfd.cFileName, L"..")))
        return FindNextFile(find, fname, fsize, wtime, is_dir);

    return true;
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

struct DiskFileSystem::DiskFind
{
    DIR* Dir {};
    string Path {};
    string Ext {};
};

shared_ptr<DiskFileSystem::DiskFind> DiskFileSystem::FindFirstFile(
    const string& path, const string& extension, string* fname, uint* fsize, uint64* wtime, bool* is_dir)
{
    // Open dir
    DIR* h = opendir(path.c_str());
    if (!h)
        return nullptr;

    // Create own descriptor
    auto find = shared_ptr<DiskFind>(new DiskFind {h, path}, [](DiskFind* find) {
        closedir(find->Dir);
        delete find;
    });

    if (!find->Path.empty() && find->Path.back() != '/')
        find->Path += "/";
    if (!extension.empty())
        find->Ext = extension;

    // Find first entire
    if (!FindNextFile(find, fname, fsize, wtime, is_dir))
        return nullptr;
    return find;
}

bool DiskFileSystem::FindNextFile(shared_ptr<DiskFind> find, string* fname, uint* fsize, uint64* wtime, bool* is_dir)
{
    // Read entire
    struct dirent* ent = readdir(find->Dir);
    if (!ent)
        return false;

    // Skip '.' and '..'
    if (Str::Compare(ent->d_name, ".") || Str::Compare(ent->d_name, ".."))
        return FindNextFile(find, fname, fsize, wtime, is_dir);

    // Read entire information
    bool valid = false;
    bool dir = false;
    uint file_size;
    uint64 write_time;
    struct stat st;
    if (!stat((find->Path + ent->d_name).c_str(), &st))
    {
        dir = S_ISDIR(st.st_mode);
        if (dir || S_ISREG(st.st_mode))
        {
            valid = true;
            file_size = (uint)st.st_size;
            write_time = (uint64)st.st_mtime;
        }
    }

    // Skip not dirs and regular files
    if (!valid)
        return FindNextFile(find, fname, fsize, wtime, is_dir);

    // Find by extensions
    if (!find->Ext.empty())
    {
        // Skip dirs
        if (dir)
            return FindNextFile(find, fname, fsize, wtime, is_dir);

        // Compare extension
        const char* ext = nullptr;
        for (const char* name = ent->d_name; *name; name++)
            if (*name == '.')
                ext = name + 1;
        if (!ext || !*ext || strcasecmp(ext, find->Ext.c_str()))
            return FindNextFile(find, fname, fsize, wtime, is_dir);
    }

    // Fill find data
    if (fname)
        *fname = ent->d_name;
    if (is_dir)
        *is_dir = dir;
    if (fsize)
        *fsize = file_size;
    if (wtime)
        *wtime = write_time;
    return true;
}
#endif

void DiskFileSystem::NormalizePathSlashesInplace(string& path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
}

void DiskFileSystem::ResolvePathInplace(string& path)
{
#ifdef FO_WINDOWS
    DWORD len = ::GetFullPathNameW(MBtoWC(path).c_str(), 0, nullptr, nullptr);
    vector<wchar_t> buf(len);
    if (::GetFullPathNameW(MBtoWC(path).c_str(), len, &buf[0], nullptr))
    {
        path = WCtoMB(&buf[0]);
        NormalizePathSlashesInplace(path);
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

void DiskFileSystem::MakeDirectory(const string& path)
{
#ifdef FO_WINDOWS
    ::CreateDirectoryW(MBtoWC(path).c_str(), nullptr);
#else
    mkdir(path.c_str(), 0777);
#endif
}

void DiskFileSystem::MakeDirectoryTree(const string& path)
{
    string work = path;
    NormalizePathSlashesInplace(work);
    for (size_t i = 0; i < work.length(); i++)
        if (work[i] == '/')
            MakeDirectory(work.substr(0, i));
}
