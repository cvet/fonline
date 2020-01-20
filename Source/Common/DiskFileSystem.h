#pragma once

#include "Common.h"

enum class DiskFileSeek
{
    Set = 0,
    Cur = 1,
    End = 2,
};

class DiskFile : public NonCopyable
{
    friend class DiskFileSystem;

public:
    ~DiskFile();
    DiskFile(DiskFile&&);
    operator bool() const;
    bool Read(void* buf, uint len);
    bool Write(const void* buf, uint len);
    bool Write(const string& str);
    bool SetPos(int offset, DiskFileSeek origin);
    uint GetPos();
    uint64 GetWriteTime();
    uint GetSize();

private:
    DiskFile(const string& fname, bool write, bool write_through);

    struct Impl;
    unique_ptr<Impl> pImpl {};
    bool openedForWriting {};
};

class DiskFind : public NonCopyable
{
    friend class DiskFileSystem;

public:
    ~DiskFind();
    DiskFind(DiskFind&&);
    DiskFind& operator++(int);
    operator bool() const;
    bool IsDir();
    string GetPath();
    uint GetFileSize();
    uint64 GetWriteTime();

private:
    DiskFind(const string& path, const string& ext);

    struct Impl;
    unique_ptr<Impl> pImpl {};
    bool findDataValid {};
};

class DiskFileSystem : public StaticClass
{
public:
    static const string InitialDir;

    static DiskFile OpenFile(const string& fname, bool write, bool write_through = false);
    static DiskFind FindFiles(const string& path, const string& ext);

    static bool DeleteFile(const string& fname);
    static bool IsFileExists(const string& fname);
    static bool CopyFile(const string& fname, const string& copy_fname);
    static bool RenameFile(const string& fname, const string& new_fname);

    static void ResolvePath(string& path);
    static void MakeDirTree(const string& path);
    static bool DeleteDir(const string& dir);
    static bool SetCurrentDir(const string& dir);
    static void ResetCurDir();
    static string GetExePath();

    using FileVisitor = std::function<void(const string&, uint, uint64)>;
    static void IterateDir(const string& path, const string& ext, bool include_subdirs, FileVisitor visitor);
};
