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

#include "FileSystem.h"

#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"

FileHeader::FileHeader(string_view name, string_view path, uint size, uint64 write_time, DataSource* ds) : _isLoaded {true}, _fileName {name}, _filePath {path}, _fileSize {size}, _writeTime {write_time}, _dataSource {ds}
{
}

FileHeader::operator bool() const
{
    return _isLoaded;
}

auto FileHeader::GetName() const -> string_view
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(!_fileName.empty());

    return _fileName;
}

auto FileHeader::GetPath() const -> string_view
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(!_filePath.empty());

    return _filePath;
}

auto FileHeader::GetFsize() const -> uint
{
    RUNTIME_ASSERT(_isLoaded);

    return _fileSize;
}

auto FileHeader::GetWriteTime() const -> uint64
{
    RUNTIME_ASSERT(_isLoaded);

    return _writeTime;
}

File::File(string_view name, string_view path, uint size, uint64 write_time, DataSource* ds, uchar* buf) : FileHeader(name, path, size, write_time, ds), _fileBuf {buf}
{
    RUNTIME_ASSERT(_fileBuf[_fileSize] == 0);
}

File::File(uchar* buf, uint size) : FileHeader("", "", size, 0, nullptr), _fileBuf {buf}
{
}

auto File::GetCStr() const -> const char*
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    return reinterpret_cast<const char*>(_fileBuf.get());
}

auto File::GetBuf() const -> const uchar*
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    return _fileBuf.get();
}

auto File::GetCurBuf() const -> const uchar*
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    return _fileBuf.get() + _curPos;
}

auto File::GetCurPos() const -> uint
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    return _curPos;
}

auto File::ReleaseBuffer() -> uchar*
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    return _fileBuf.release();
}

void File::SetCurPos(uint pos)
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(pos <= _fileSize);

    _curPos = pos;
}

void File::GoForward(uint offs)
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(_curPos + offs <= _fileSize);

    _curPos += offs;
}

void File::GoBack(uint offs)
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(offs <= _curPos);

    _curPos -= offs;
}

auto File::FindFragment(const uchar* fragment, uint fragment_len, uint begin_offs) -> bool
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    for (auto i = begin_offs; i < _fileSize - fragment_len; i++) {
        if (_fileBuf[i] == fragment[0]) {
            auto not_match = false;
            for (uint j = 1; j < fragment_len; j++) {
                if (_fileBuf[static_cast<size_t>(i) + j] != fragment[j]) {
                    not_match = true;
                    break;
                }
            }

            if (!not_match) {
                _curPos = i;
                return true;
            }
        }
    }
    return false;
}

auto File::GetNonEmptyLine() -> string
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    while (_curPos < _fileSize) {
        const auto start = _curPos;
        uint len = 0;
        while (_curPos < _fileSize) {
            if (_fileBuf[_curPos] == '\r' || _fileBuf[_curPos] == '\n' || _fileBuf[_curPos] == '#' || _fileBuf[_curPos] == ';') {
                for (; _curPos < _fileSize; _curPos++) {
                    if (_fileBuf[_curPos] == '\n') {
                        break;
                    }
                }
                _curPos++;
                break;
            }

            _curPos++;
            len++;
        }

        if (len != 0u) {
            string line = _str(string(reinterpret_cast<const char*>(&_fileBuf[start]), len)).trim();
            if (!line.empty()) {
                return line;
            }
        }
    }

    return "";
}

void File::CopyMem(void* ptr, uint size)
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(size);

    if (_curPos + size > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    std::memcpy(ptr, _fileBuf.get() + _curPos, size);
    _curPos += size;
}

// ReSharper disable once CppInconsistentNaming
auto File::GetStrNT() -> string
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + 1 > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    uint len = 0;
    while (*(_fileBuf.get() + _curPos + len) != 0u) {
        len++;
    }

    string str(reinterpret_cast<const char*>(&_fileBuf[_curPos]), len);
    _curPos += len + 1;
    return str;
}

auto File::GetUChar() -> uchar
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(uchar) > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    return _fileBuf[_curPos++];
}

// ReSharper disable once CppInconsistentNaming
auto File::GetBEUShort() -> ushort
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(ushort) > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    ushort res = 0;
    auto* cres = reinterpret_cast<uchar*>(&res);
    cres[1] = _fileBuf[_curPos++];
    cres[0] = _fileBuf[_curPos++];
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto File::GetLEUShort() -> ushort
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(ushort) > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    ushort res = 0;
    auto* cres = reinterpret_cast<uchar*>(&res);
    cres[0] = _fileBuf[_curPos++];
    cres[1] = _fileBuf[_curPos++];
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto File::GetBEUInt() -> uint
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(uint) > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    uint res = 0;
    auto* cres = reinterpret_cast<uchar*>(&res);
    for (auto i = 3; i >= 0; i--) {
        cres[i] = _fileBuf[_curPos++];
    }
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto File::GetLEUInt() -> uint
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(uint) > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    uint res = 0;
    auto* cres = reinterpret_cast<uchar*>(&res);
    for (auto i = 0; i <= 3; i++) {
        cres[i] = _fileBuf[_curPos++];
    }
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto File::GetLE3UChar() -> uint
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(uchar) * 3 > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    uint res = 0;
    auto* cres = reinterpret_cast<uchar*>(&res);
    for (auto i = 0; i <= 2; i++) {
        cres[i] = _fileBuf[_curPos++];
    }
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto File::GetBEFloat() -> float
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(float) > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    auto res = NAN;
    auto* cres = reinterpret_cast<uchar*>(&res);
    for (auto i = 3; i >= 0; i--) {
        cres[i] = _fileBuf[_curPos++];
    }
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto File::GetLEFloat() -> float
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    if (_curPos + sizeof(float) > _fileSize) {
        throw FileSystemExeption("Read file error", _fileName);
    }

    auto res = NAN;
    auto* cres = reinterpret_cast<uchar*>(&res);
    for (auto i = 0; i <= 3; i++) {
        cres[i] = _fileBuf[_curPos++];
    }
    return res;
}

auto OutputBuffer::GetOutBuf() const -> const uchar*
{
    RUNTIME_ASSERT(!_dataBuf.empty());

    return &_dataBuf[0];
}

auto OutputBuffer::GetOutBufLen() const -> uint
{
    RUNTIME_ASSERT(!_dataBuf.empty());

    return static_cast<uint>(_dataBuf.size());
}

void OutputBuffer::SetData(const void* data, uint len)
{
    if (len == 0u) {
        return;
    }

    _dataWriter.WritePtr(data, len);
}

void OutputBuffer::SetStr(string_view str)
{
    SetData(str.data(), static_cast<uint>(str.length()));
}

// ReSharper disable once CppInconsistentNaming
void OutputBuffer::SetStrNT(string_view str)
{
    SetData(str.data(), static_cast<uint>(str.length()) + 1);
}

void OutputBuffer::SetUChar(uchar data)
{
    _dataWriter.Write(data);
}

// ReSharper disable once CppInconsistentNaming
void OutputBuffer::SetBEUShort(ushort data)
{
    auto* pdata = reinterpret_cast<uchar*>(&data);
    _dataWriter.Write(pdata[1]);
    _dataWriter.Write(pdata[0]);
}

// ReSharper disable once CppInconsistentNaming
void OutputBuffer::SetLEUShort(ushort data)
{
    auto* pdata = reinterpret_cast<uchar*>(&data);
    _dataWriter.Write(pdata[0]);
    _dataWriter.Write(pdata[1]);
}

// ReSharper disable once CppInconsistentNaming
void OutputBuffer::SetBEUInt(uint data)
{
    auto* pdata = reinterpret_cast<uchar*>(&data);
    _dataWriter.Write(pdata[3]);
    _dataWriter.Write(pdata[2]);
    _dataWriter.Write(pdata[1]);
    _dataWriter.Write(pdata[0]);
}

// ReSharper disable once CppInconsistentNaming
void OutputBuffer::SetLEUInt(uint data)
{
    auto* pdata = reinterpret_cast<uchar*>(&data);
    _dataWriter.Write(pdata[0]);
    _dataWriter.Write(pdata[1]);
    _dataWriter.Write(pdata[2]);
    _dataWriter.Write(pdata[3]);
}

void OutputBuffer::Clear()
{
    _dataBuf.clear();
}

OutputFile::OutputFile(DiskFile file) : _diskFile {std::move(file)}
{
    RUNTIME_ASSERT(_diskFile);
}

void OutputFile::Save()
{
    if (GetOutBufLen() > 0u) {
        const auto save_ok = _diskFile.Write(GetOutBuf(), GetOutBufLen());
        RUNTIME_ASSERT(save_ok);
        Clear();
    }
}

FileCollection::FileCollection(string_view path, vector<FileHeader> files) : _filterPath {path}, _allFiles {std::move(files)}
{
}

auto FileCollection::MoveNext() -> bool
{
    RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    return ++_curFileIndex < static_cast<int>(_allFiles.size());
}

void FileCollection::ResetCounter()
{
    _curFileIndex = -1;
}

auto FileCollection::GetPath() const -> string_view
{
    return _filterPath;
}

auto FileCollection::GetCurFile() const -> File
{
    RUNTIME_ASSERT(_curFileIndex >= 0);
    RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    const auto& fh = _allFiles[_curFileIndex];
    auto fs = fh._fileSize;
    auto wt = fh._writeTime;
    auto* buf = fh._dataSource->OpenFile(fh._filePath, _str(fh._filePath).lower(), fs, wt);
    RUNTIME_ASSERT(buf);
    return File(fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource, buf);
}

auto FileCollection::GetCurFileHeader() const -> FileHeader
{
    RUNTIME_ASSERT(_curFileIndex >= 0);
    RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    const auto& fh = _allFiles[_curFileIndex];
    return FileHeader(fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource);
}

auto FileCollection::FindFileByName(string_view name) const -> File
{
    for (const auto& fh : _allFiles) {
        if (fh._fileName == name) {
            auto fs = fh._fileSize;
            auto wt = fh._writeTime;
            auto* buf = fh._dataSource->OpenFile(fh._filePath, _str(fh._filePath).lower(), fs, wt);
            RUNTIME_ASSERT(buf);
            return File(fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource, buf);
        }
    }
    return File();
}

auto FileCollection::FindFileByPath(string_view name) const -> File
{
    for (const auto& fh : _allFiles) {
        if (fh._filePath == name) {
            auto fs = fh._fileSize;
            auto wt = fh._writeTime;
            auto* buf = fh._dataSource->OpenFile(fh._filePath, _str(fh._filePath).lower(), fs, wt);
            RUNTIME_ASSERT(buf);
            return File(fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource, buf);
        }
    }
    return File();
}

auto FileCollection::GetFilesCount() const -> uint
{
    return static_cast<uint>(_allFiles.size());
}

void FileManager::AddDataSource(string_view path, bool cache_dirs)
{
    _dataSources.emplace_back(path, cache_dirs);
    _dataSourceAddedDispatcher(&_dataSources.back());
}

auto FileManager::FilterFiles(string_view ext) -> FileCollection
{
    return FilterFiles(ext, "", true);
}

auto FileManager::FilterFiles(string_view ext, string_view dir, bool include_subdirs) -> FileCollection
{
    vector<FileHeader> files;

    for (auto& ds : _dataSources) {
        for (auto& fname : ds.GetFileNames(dir, include_subdirs, ext)) {
            uint size = 0;
            uint64 write_time = 0;
            const auto ok = ds.IsFilePresent(fname, _str(fname).lower(), size, write_time);
            RUNTIME_ASSERT(ok);
            const string name = _str(fname).extractFileName().eraseFileExtension();
            auto file_header = FileHeader(name, fname, size, write_time, &ds);
            files.push_back(std::move(file_header));
        }
    }

    return FileCollection(dir, std::move(files));
}

auto FileManager::ReadFile(string_view path) -> File
{
    RUNTIME_ASSERT(!path.empty());
    RUNTIME_ASSERT(path[0] != '.' && path[0] != '/');

    const string path_lower = _str(path).lower();
    const string name = _str(path).extractFileName().eraseFileExtension();

    for (auto& ds : _dataSources) {
        uint file_size = 0;
        uint64 write_time = 0;
        auto* buf = ds.OpenFile(path, path_lower, file_size, write_time);
        if (buf != nullptr) {
            return File(name, path, file_size, write_time, &ds, buf);
        }
    }
    return File(name, path, 0, 0, nullptr, nullptr);
}

auto FileManager::ReadFileHeader(string_view path) -> FileHeader
{
    RUNTIME_ASSERT(!path.empty());
    RUNTIME_ASSERT(path[0] != '.' && path[0] != '/');

    const string path_lower = _str(path).lower();
    const string name = _str(path).extractFileName().eraseFileExtension();

    for (auto& ds : _dataSources) {
        uint file_size = 0;
        uint64 write_time = 0;
        if (ds.IsFilePresent(path, path_lower, file_size, write_time)) {
            return FileHeader(name, path, file_size, write_time, &ds);
        }
    }
    return FileHeader(name, path, 0, 0, nullptr);
}

auto FileManager::ReadConfigFile(string_view path, NameResolver& name_resolver) -> ConfigFile
{
    if (const auto file = ReadFile(path)) {
        return ConfigFile(file.GetCStr(), name_resolver);
    }
    return ConfigFile("", name_resolver);
}

auto FileManager::WriteFile(string_view path, bool apply) -> OutputFile
{
    NON_CONST_METHOD_HINT();

    auto file = DiskFileSystem::OpenFile(_str("{}/{}", _rootPath, path), true); // Todo: handle apply file writing
    if (!file) {
        throw FileSystemExeption("Can't open file for writing", path, apply);
    }
    return OutputFile(std::move(file));
}

void FileManager::DeleteFile(string_view path)
{
    NON_CONST_METHOD_HINT();

    DiskFileSystem::DeleteFile(_str("{}/{}", _rootPath, path));
}

void FileManager::DeleteDir(string_view path)
{
    NON_CONST_METHOD_HINT();

    DiskFileSystem::DeleteDir(_str("{}/{}", _rootPath, path));
}

void FileManager::RenameFile(string_view from_path, string_view to_path)
{
    NON_CONST_METHOD_HINT();

    DiskFileSystem::RenameFile(_str("{}/{}", _rootPath, from_path), _str("{}/{}", _rootPath, to_path));
}
