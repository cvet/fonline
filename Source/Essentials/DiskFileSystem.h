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

#pragma once

#include "BasicCore.h"
#include "Containers.h"

FO_BEGIN_NAMESPACE();

enum class DiskFileSeek : uint8
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
    DiskFile(DiskFile&&) noexcept = default;
    auto operator=(const DiskFile&) -> DiskFile& = delete;
    auto operator=(DiskFile&&) -> DiskFile& = delete;
    explicit operator bool() const;
    ~DiskFile() = default;

    [[nodiscard]] auto GetReadPos() -> size_t;
    [[nodiscard]] auto GetSize() -> size_t;

    auto Read(void* buf, size_t len) -> bool;
    auto Write(const void* buf, size_t len) -> bool;
    auto Write(string_view str) -> bool;
    auto Write(span<const uint8> data) -> bool;
    auto SetReadPos(int32 offset, DiskFileSeek origin) -> bool;

private:
    explicit DiskFile(string_view path, bool write, bool write_through);

    std::fstream _file {};
    bool _openedForWriting {};
    bool _writeThrough {};
};

class DiskFileSystem final
{
public:
    using FileVisitor = function<void(string_view, size_t, uint64)>;

    DiskFileSystem() = delete;

    static auto OpenFile(string_view path, bool write) -> DiskFile;
    static auto OpenFile(string_view path, bool write, bool write_through) -> DiskFile;
    static auto ReadFile(string_view path) -> optional<string>;
    static auto WriteFile(string_view path, string_view content) -> bool;
    static auto WriteFile(string_view path, span<const uint8> content) -> bool;
    static auto GetWriteTime(string_view path) -> uint64;
    static auto IsExists(string_view path) -> bool;
    static auto IsDir(string_view path) -> bool;
    static auto DeleteFile(string_view path) -> bool;
    static auto CopyFile(string_view path, string_view copy_path) -> bool;
    static auto RenameFile(string_view path, string_view new_path) -> bool;
    static auto ResolvePath(string_view path) -> string;
    static auto MakeDirTree(string_view dir) -> bool;
    static auto DeleteDir(string_view dir) -> bool;
    static void IterateDir(string_view dir, bool recursive, FileVisitor visitor);
    static auto CompareFileContent(string_view path, span<const uint8> buf) -> bool;
    static auto TouchFile(string_view path) -> bool;
};

FO_END_NAMESPACE();
