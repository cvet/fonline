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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "DataSource.h"

DECLARE_EXCEPTION(FileSystemExeption);

class FileHeader
{
public:
    FileHeader() = default;
    FileHeader(string_view name, string_view path, size_t size, uint64 write_time, DataSource* ds);
    FileHeader(const FileHeader&) = delete;
    FileHeader(FileHeader&&) noexcept = default;
    auto operator=(const FileHeader&) = delete;
    auto operator=(FileHeader&&) noexcept -> FileHeader& = default;
    explicit operator bool() const;
    ~FileHeader() = default;

    [[nodiscard]] auto GetName() const -> const string&;
    [[nodiscard]] auto GetPath() const -> const string&;
    [[nodiscard]] auto GetFullPath() const -> string;
    [[nodiscard]] auto GetSize() const -> size_t;
    [[nodiscard]] auto GetWriteTime() const -> uint64;
    [[nodiscard]] auto GetDataSource() const -> DataSource*;
    [[nodiscard]] auto Duplicate() const -> FileHeader;

protected:
    bool _isLoaded {};
    string _fileName {};
    string _filePath {};
    size_t _fileSize {};
    uint64 _writeTime {};
    DataSource* _dataSource {};
};

class File final : public FileHeader
{
public:
    File() = default;
    File(string_view name, string_view path, size_t size, uint64 write_time, DataSource* ds, unique_del_ptr<const uint8>&& buf);
    File(string_view name, string_view path, uint64 write_time, DataSource* ds, const_span<uint8> buf, bool make_copy);
    File(const File&) = delete;
    File(File&&) noexcept = default;
    auto operator=(const File&) = delete;
    auto operator=(File&&) noexcept -> File& = default;
    ~File() = default;

    [[nodiscard]] auto GetStr() const -> string;
    [[nodiscard]] auto GetData() const -> vector<uint8>;
    [[nodiscard]] auto GetBuf() const -> const uint8*;
    [[nodiscard]] auto GetCurBuf() const -> const uint8*;
    [[nodiscard]] auto GetCurPos() const -> size_t;

    [[nodiscard]] auto FindFragment(string_view fragment) -> bool;
    // ReSharper disable CppInconsistentNaming
    [[nodiscard]] auto GetStrNT() -> string; // Null terminated
    [[nodiscard]] auto GetUChar() -> uint8;
    [[nodiscard]] auto GetChar() -> char { return static_cast<char>(GetUChar()); }
    [[nodiscard]] auto GetBEUShort() -> uint16;
    [[nodiscard]] auto GetBEShort() -> int16 { return static_cast<int16>(GetBEUShort()); }
    [[nodiscard]] auto GetLEUShort() -> uint16;
    [[nodiscard]] auto GetLEShort() -> int16 { return static_cast<int16>(GetLEUShort()); }
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
    unique_del_ptr<const uint8> _fileBuf {};
    size_t _curPos {};
};

class FileCollection final
{
public:
    explicit FileCollection(initializer_list<FileHeader> files);
    explicit FileCollection(vector<FileHeader> files);
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

    auto MoveNext() -> bool;
    void ResetCounter();

private:
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

    [[nodiscard]] auto GetAllFiles() const -> FileCollection;
    [[nodiscard]] auto FilterFiles(string_view ext, string_view dir = "", bool include_subdirs = true) const -> FileCollection;
    [[nodiscard]] auto ReadFile(string_view path) const -> File;
    [[nodiscard]] auto ReadFileText(string_view path) const -> string;
    [[nodiscard]] auto ReadFileHeader(string_view path) const -> FileHeader;

    void AddDataSource(string_view path, DataSourceType type = DataSourceType::Default);
    void AddDataSource(unique_ptr<DataSource> data_source);
    void CleanDataSources();

private:
    string _rootPath {};
    vector<unique_ptr<DataSource>> _dataSources {};
};
