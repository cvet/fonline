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

#include "ConfigFile.h"
#include "DataSource.h"
#include "DiskFileSystem.h"

DECLARE_EXCEPTION(FileSystemExeption);

class FileHeader
{
    friend class FileManager;
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
    [[nodiscard]] auto GetFsize() const -> uint;
    [[nodiscard]] auto GetWriteTime() const -> uint64;

protected:
    FileHeader() = default;
    FileHeader(string_view name, string_view path, uint size, uint64 write_time, DataSource* ds);

    bool _isLoaded {};
    string _fileName {};
    string _filePath {};
    uint _fileSize {};
    uint64 _writeTime {};
    DataSource* _dataSource {};
};

class File final : public FileHeader
{
    friend class FileManager;
    friend class FileCollection;

public:
    File() = default;
    File(uchar* buf, uint size);
    File(const File&) = delete;
    File(File&&) noexcept = default;
    auto operator=(const File&) = delete;
    auto operator=(File&&) noexcept -> File& = default;
    ~File() = default;

    [[nodiscard]] auto GetCStr() const -> const char*;
    [[nodiscard]] auto GetBuf() const -> const uchar*;
    [[nodiscard]] auto GetCurBuf() const -> const uchar*;
    [[nodiscard]] auto GetCurPos() const -> uint;

    [[nodiscard]] auto FindFragment(const uchar* fragment, uint fragment_len, uint begin_offs) -> bool;
    [[nodiscard]] auto GetNonEmptyLine() -> string;
    // ReSharper disable CppInconsistentNaming
    [[nodiscard]] auto GetStrNT() -> string; // Null terminated
    [[nodiscard]] auto GetUChar() -> uchar;
    [[nodiscard]] auto GetBEUShort() -> ushort;
    [[nodiscard]] auto GetBEShort() -> short { return static_cast<short>(GetBEUShort()); }
    [[nodiscard]] auto GetLEUShort() -> ushort;
    [[nodiscard]] auto GetLEShort() -> short { return static_cast<short>(GetLEUShort()); }
    [[nodiscard]] auto GetBEUInt() -> uint;
    [[nodiscard]] auto GetLEUInt() -> uint;
    [[nodiscard]] auto GetLE3UChar() -> uint;
    [[nodiscard]] auto GetBEFloat() -> float;
    [[nodiscard]] auto GetLEFloat() -> float;
    // ReSharper restore CppInconsistentNaming
    [[nodiscard]] auto ReleaseBuffer() -> uchar*;

    void CopyMem(void* ptr, uint size);
    void SetCurPos(uint pos);
    void GoForward(uint offs);
    void GoBack(uint offs);

private:
    File(string_view name, string_view path, uint size, uint64 write_time, DataSource* ds, uchar* buf);

    unique_ptr<uchar[]> _fileBuf {};
    uint _curPos {};
};

class OutputFile final
{
    friend class FileManager;

public:
    OutputFile(const OutputFile&) = delete;
    OutputFile(OutputFile&&) noexcept = default;
    auto operator=(const OutputFile&) = delete;
    auto operator=(OutputFile&&) noexcept = delete;
    ~OutputFile() = default;

    [[nodiscard]] auto GetOutBuf() const -> const uchar*;
    [[nodiscard]] auto GetOutBufLen() const -> uint;

    void SetData(const void* data, uint len);
    void SetStr(string_view str);
    // ReSharper disable CppInconsistentNaming
    void SetStrNT(string_view str);
    void SetUChar(uchar data);
    void SetBEUShort(ushort data);
    void SetBEShort(short data) { SetBEUShort(static_cast<ushort>(data)); }
    void SetLEUShort(ushort data);
    void SetBEUInt(uint data);
    void SetLEUInt(uint data);
    // ReSharper restore CppInconsistentNaming
    void Save();

private:
    explicit OutputFile(DiskFile file);

    DiskFile _diskFile;
    vector<uchar> _dataBuf {};
    DataWriter _dataWriter {_dataBuf};
};

class FileCollection final
{
    friend class FileManager;

public:
    FileCollection(const FileCollection&) = delete;
    FileCollection(FileCollection&&) noexcept = default;
    auto operator=(const FileCollection&) = delete;
    auto operator=(FileCollection&&) noexcept = delete;
    ~FileCollection() = default;

    [[nodiscard]] auto GetPath() const -> string_view;
    [[nodiscard]] auto GetCurFile() const -> File;
    [[nodiscard]] auto GetCurFileHeader() const -> FileHeader;
    [[nodiscard]] auto FindFile(string_view name) const -> File;
    [[nodiscard]] auto FindFileHeader(string_view name) const -> FileHeader;
    [[nodiscard]] auto GetFilesCount() const -> uint;

    [[nodiscard]] auto MoveNext() -> bool;

    void ResetCounter();

private:
    FileCollection(string_view path, vector<FileHeader> files);

    string _filterPath {};
    vector<FileHeader> _allFiles {};
    int _curFileIndex {-1};
};

class FileManager final
{
public:
    FileManager() = default;
    FileManager(const FileManager&) = delete;
    FileManager(FileManager&&) noexcept = default;
    auto operator=(const FileManager&) = delete;
    auto operator=(FileManager&&) noexcept = delete;
    ~FileManager() = default;

    [[nodiscard]] auto FilterFiles(string_view ext) -> FileCollection;
    [[nodiscard]] auto FilterFiles(string_view ext, string_view dir, bool include_subdirs) -> FileCollection;
    [[nodiscard]] auto ReadFile(string_view path) -> File;
    [[nodiscard]] auto ReadFileHeader(string_view path) -> FileHeader;
    [[nodiscard]] auto ReadConfigFile(string_view path, NameResolver& name_resolver) -> ConfigFile;
    [[nodiscard]] auto WriteFile(string_view path, bool apply) -> OutputFile;

    void AddDataSource(string_view path, bool cache_dirs);
    void DeleteFile(string_view path);
    void DeleteDir(string_view path);
    void RenameFile(string_view from_path, string_view to_path);

    EventObserver<DataSource*> OnDataSourceAdded {};

private:
    string _rootPath {};
    vector<DataSource> _dataSources {};
    EventDispatcher<DataSource*> _dataSourceAddedDispatcher {OnDataSourceAdded};
    bool _nonConstHelper {};
};
