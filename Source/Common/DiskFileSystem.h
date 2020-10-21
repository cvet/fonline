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

class DiskFile final
{
    friend class DiskFileSystem;

public:
    DiskFile() = delete;
    DiskFile(const DiskFile&) = delete;
    DiskFile(DiskFile&&) noexcept;
    auto operator=(const DiskFile&) -> DiskFile& = delete;
    auto operator=(DiskFile &&) -> DiskFile& = delete;
    explicit operator bool() const;
    ~DiskFile();

    [[nodiscard]] auto GetPos() const -> uint;
    [[nodiscard]] auto GetWriteTime() const -> uint64;
    [[nodiscard]] auto GetSize() const -> uint;

    auto Read(void* buf, uint len) -> bool;
    auto Write(const void* buf, uint len) -> bool;
    auto Write(const string& str) -> bool;
    auto SetPos(int offset, DiskFileSeek origin) -> bool;

private:
    DiskFile(const string& fname, bool write, bool write_through);

    struct Impl;
    unique_ptr<Impl> _pImpl {};
    bool _openedForWriting {};
    bool _nonConstHelper {};
};

class DiskFind final
{
    friend class DiskFileSystem;

public:
    DiskFind() = delete;
    DiskFind(DiskFind&&) noexcept;
    DiskFind(const DiskFind&) = delete;
    auto operator=(const DiskFind&) -> DiskFind& = delete;
    auto operator=(DiskFind &&) -> DiskFind& = delete;
    DiskFind& operator++(int);
    explicit operator bool() const;
    ~DiskFind();

    [[nodiscard]] auto IsDir() const -> bool;
    [[nodiscard]] auto GetPath() const -> string;
    [[nodiscard]] auto GetFileSize() const -> uint;
    [[nodiscard]] auto GetWriteTime() const -> uint64;

private:
    DiskFind(const string& path, const string& ext);

    struct Impl;
    unique_ptr<Impl> _pImpl {};
    bool _findDataValid {};
};

class DiskFileSystem final
{
public:
    using FileVisitor = std::function<void(const string&, uint, uint64)>;

    DiskFileSystem() = delete;

    [[nodiscard]] static auto IsFileExists(const string& fname) -> bool;
    [[nodiscard]] static auto GetExePath() -> string;

    [[nodiscard]] static auto OpenFile(const string& fname, bool write) -> DiskFile;
    [[nodiscard]] static auto OpenFile(const string& fname, bool write, bool write_through) -> DiskFile;
    [[nodiscard]] static auto FindFiles(const string& path, const string& ext) -> DiskFind;

    static auto DeleteFile(const string& fname) -> bool;
    static auto CopyFile(const string& fname, const string& copy_fname) -> bool;
    static auto RenameFile(const string& fname, const string& new_fname) -> bool;
    static void ResolvePath(string& path);
    static void MakeDirTree(const string& path);
    static auto DeleteDir(const string& dir) -> bool;
    static auto SetCurrentDir(const string& dir) -> bool;
    static void ResetCurDir();
    static void IterateDir(const string& path, const string& ext, bool include_subdirs, FileVisitor visitor);
};
