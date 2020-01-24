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
