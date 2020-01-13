#pragma once

#include "Common.h"

namespace DiskFileSystem
{
    enum DiskFileSeek
    {
        SeekSet = 0,
        SeekCur = 1,
        SeekEnd = 2,
    };

    struct DiskFile;
    struct DiskFind;

    shared_ptr<DiskFile> OpenFile(const string& fname, bool write, bool write_through = false);
    shared_ptr<DiskFile> OpenFileForAppend(const string& fname, bool write_through = false);
    shared_ptr<DiskFile> OpenFileForReadWrite(const string& fname, bool write_through = false);
    bool ReadFile(shared_ptr<DiskFile> file, void* buf, uint len, uint* rb = nullptr);
    bool WriteFile(shared_ptr<DiskFile> file, const void* buf, uint len);
    bool SetFilePointer(shared_ptr<DiskFile> file, int offset, DiskFileSeek origin);
    uint GetFilePointer(shared_ptr<DiskFile> file);
    uint64 GetFileWriteTime(shared_ptr<DiskFile> file);
    uint GetFileSize(shared_ptr<DiskFile> file);

    bool DeleteFile(const string& fname);
    bool IsFileExists(const string& fname);
    bool CopyFile(const string& fname, const string& copy_fname);
    bool RenameFile(const string& fname, const string& new_fname);

    shared_ptr<DiskFind> FindFirstFile(
        const string& path, const string& extension, string* fname, uint* fsize, uint64* wtime, bool* is_dir);
    bool FindNextFile(shared_ptr<DiskFind> find, string* fname, uint* fsize, uint64* wtime, bool* is_dir);

    void NormalizePathSlashesInplace(string& path);
    void ResolvePathInplace(string& path);
    void MakeDirectory(const string& path);
    void MakeDirectoryTree(const string& path);
}
