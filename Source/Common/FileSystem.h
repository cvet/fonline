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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ConfigFile.h"
#include "DataSource.h"

DECLARE_EXCEPTION(FileSystemExeption);

class FileHeader
{
    friend class FileSystem;
    friend class FileCollection;
    friend class File;

public:
    FileHeader(const FileHeader&) = delete;
    FileHeader(FileHeader&&) noexcept = default;
    auto operator=(const FileHeader&) = delete;
    auto operator=(FileHeader&&) noexcept -> FileHeader& = default;
    explicit operator bool() const;
    ~FileHeader() = default;

    [[nodiscard]] auto GetName() const -> string_view;
    [[nodiscard]] auto GetPath() const -> string_view;
    [[nodiscard]] auto GetSize() const -> size_t;
    [[nodiscard]] auto GetWriteTime() const -> uint64;

protected:
    FileHeader() = default;
    FileHeader(string_view name, string_view path, size_t size, uint64 write_time, DataSource* ds);

    bool _isLoaded {};
    string _fileName {};
    string _filePath {};
    size_t _fileSize {};
    uint64 _writeTime {};
    DataSource* _dataSource {};
};

class File final : public FileHeader
{
    friend class FileSystem;
    friend class FileCollection;

public:
    File() = default;
    explicit File(const vector<uchar>& buf);
    File(const File&) = delete;
    File(File&&) noexcept = default;
    auto operator=(const File&) = delete;
    auto operator=(File&&) noexcept -> File& = default;
    ~File() = default;

    [[nodiscard]] auto GetStr() const -> string;
    [[nodiscard]] auto GetData() const -> vector<uchar>;
    [[nodiscard]] auto GetBuf() const -> const uchar*;
    [[nodiscard]] auto GetCurBuf() const -> const uchar*;
    [[nodiscard]] auto GetCurPos() const -> size_t;

    [[nodiscard]] auto FindFragment(string_view fragment) -> bool;
    // ReSharper disable CppInconsistentNaming
    [[nodiscard]] auto GetStrNT() -> string; // Null terminated
    [[nodiscard]] auto GetUChar() -> uchar;
    [[nodiscard]] auto GetChar() -> char { return static_cast<char>(GetUChar()); }
    [[nodiscard]] auto GetBEUShort() -> ushort;
    [[nodiscard]] auto GetBEShort() -> short { return static_cast<short>(GetBEUShort()); }
    [[nodiscard]] auto GetLEUShort() -> ushort;
    [[nodiscard]] auto GetLEShort() -> short { return static_cast<short>(GetLEUShort()); }
    [[nodiscard]] auto GetBEUInt() -> uint;
    [[nodiscard]] auto GetBEInt() -> int { return static_cast<int>(GetBEUInt()); }
    [[nodiscard]] auto GetLEUInt() -> uint;
    [[nodiscard]] auto GetLEInt() -> int { return static_cast<int>(GetLEUInt()); }
    // ReSharper restore CppInconsistentNaming

    void CopyData(void* ptr, size_t size);
    void SetCurPos(size_t pos);
    void GoForward(size_t offs);
    void GoBack(size_t offs);

private:
    File(string_view name, string_view path, size_t size, uint64 write_time, DataSource* ds, unique_del_ptr<uchar>&& buf);

    unique_del_ptr<uchar> _fileBuf {};
    size_t _curPos {};
};

class FileCollection final
{
    friend class FileSystem;

public:
    FileCollection(const FileCollection&) = delete;
    FileCollection(FileCollection&&) noexcept = default;
    auto operator=(const FileCollection&) = delete;
    auto operator=(FileCollection&&) noexcept = delete;
    ~FileCollection() = default;

    [[nodiscard]] auto GetCurFile() const -> File;
    [[nodiscard]] auto GetCurFileHeader() const -> FileHeader;
    [[nodiscard]] auto FindFileByName(string_view name) const -> File;
    [[nodiscard]] auto FindFileByPath(string_view path) const -> File;
    [[nodiscard]] auto GetFilesCount() const -> size_t;

    [[nodiscard]] auto MoveNext() -> bool;

    void ResetCounter();

private:
    explicit FileCollection(vector<FileHeader> files);

    vector<FileHeader> _allFiles {};
    int _curFileIndex {-1};
    mutable unordered_map<string, size_t> _nameToIndex {};
    mutable unordered_map<string, size_t> _pathToIndex {};
};

class FileSystem final
{
public:
    FileSystem() = default;
    FileSystem(const FileSystem&) = delete;
    FileSystem(FileSystem&&) noexcept = default;
    auto operator=(const FileSystem&) = delete;
    auto operator=(FileSystem&&) noexcept = delete;
    ~FileSystem() = default;

    [[nodiscard]] auto FilterFiles(string_view ext) -> FileCollection;
    [[nodiscard]] auto FilterFiles(string_view ext, string_view dir, bool include_subdirs) -> FileCollection;
    [[nodiscard]] auto ReadFile(string_view path) -> File;
    [[nodiscard]] auto ReadFileText(string_view path) -> string;
    [[nodiscard]] auto ReadFileHeader(string_view path) -> FileHeader;
    [[nodiscard]] auto ReadConfigFile(string_view path, NameResolver& name_resolver) -> ConfigFile;

    void AddDataSource(string_view path, DataSourceType type = DataSourceType::Default);

private:
    string _rootPath {};
    vector<DataSource> _dataSources {};
};
