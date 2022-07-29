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

#include "FileSystem.h"
#include "Log.h"
#include "StringUtils.h"

FileHeader::FileHeader(string_view name, string_view path, size_t size, uint64 write_time, DataSource* ds) : _isLoaded {true}, _fileName {name}, _filePath {path}, _fileSize {size}, _writeTime {write_time}, _dataSource {ds}
{
}

FileHeader::operator bool() const
{
    return _isLoaded;
}

auto FileHeader::GetName() const -> const string&
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(!_fileName.empty());

    return _fileName;
}

auto FileHeader::GetPath() const -> const string&
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(!_filePath.empty());

    return _filePath;
}

auto FileHeader::GetSize() const -> size_t
{
    RUNTIME_ASSERT(_isLoaded);

    return _fileSize;
}

auto FileHeader::GetWriteTime() const -> uint64
{
    RUNTIME_ASSERT(_isLoaded);

    return _writeTime;
}

File::File(string_view name, string_view path, size_t size, uint64 write_time, DataSource* ds, unique_del_ptr<uchar>&& buf) : FileHeader(name, path, size, write_time, ds), _fileBuf {std::move(buf)}
{
}

File::File(const vector<uchar>& buf) : FileHeader("", "", static_cast<uint>(buf.size()), 0, nullptr), _fileBuf(new uchar[buf.size()])
{
    std::memcpy(_fileBuf.get(), buf.data(), buf.size());
}

auto File::GetStr() const -> string
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    return {reinterpret_cast<const char*>(_fileBuf.get()), _fileSize};
}

auto File::GetData() const -> vector<uchar>
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    vector<uchar> result;
    result.resize(_fileSize);
    std::memcpy(result.data(), _fileBuf.get(), _fileSize);
    return result;
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

auto File::GetCurPos() const -> size_t
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);

    return _curPos;
}

void File::SetCurPos(size_t pos)
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(pos <= _fileSize);

    _curPos = pos;
}

void File::GoForward(size_t offs)
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(_curPos + offs <= _fileSize);

    _curPos += offs;
}

void File::GoBack(size_t offs)
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(offs <= _curPos);

    _curPos -= offs;
}

auto File::FindFragment(string_view fragment) -> bool
{
    RUNTIME_ASSERT(_isLoaded);
    RUNTIME_ASSERT(_fileBuf);
    RUNTIME_ASSERT(!fragment.empty());

    if (_curPos + fragment.size() > _fileSize) {
        return false;
    }

    for (auto i = _curPos; i < _fileSize - fragment.size(); i++) {
        if (_fileBuf.get()[i] == static_cast<uchar>(fragment[0])) {
            auto not_match = false;
            for (uint j = 1; j < fragment.size(); j++) {
                if (_fileBuf.get()[static_cast<size_t>(i) + j] != static_cast<uchar>(fragment[j])) {
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

void File::CopyData(void* ptr, size_t size)
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

    string str(reinterpret_cast<const char*>(&_fileBuf.get()[_curPos]), len);
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

    return _fileBuf.get()[_curPos++];
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
    cres[1] = _fileBuf.get()[_curPos++];
    cres[0] = _fileBuf.get()[_curPos++];
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
    cres[0] = _fileBuf.get()[_curPos++];
    cres[1] = _fileBuf.get()[_curPos++];
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
        cres[i] = _fileBuf.get()[_curPos++];
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
        cres[i] = _fileBuf.get()[_curPos++];
    }
    return res;
}

FileCollection::FileCollection(vector<FileHeader> files) : _allFiles {std::move(files)}
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

auto FileCollection::GetCurFile() const -> File
{
    RUNTIME_ASSERT(_curFileIndex >= 0);
    RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    const auto& fh = _allFiles[_curFileIndex];
    auto fs = fh._fileSize;
    auto wt = fh._writeTime;
    auto&& buf = fh._dataSource->OpenFile(fh._filePath, fs, wt);
    RUNTIME_ASSERT(buf);
    return {fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource, std::move(buf)};
}

auto FileCollection::GetCurFileHeader() const -> FileHeader
{
    RUNTIME_ASSERT(_curFileIndex >= 0);
    RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    const auto& fh = _allFiles[_curFileIndex];
    return {fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource};
}

auto FileCollection::FindFileByName(string_view name) const -> File
{
    if (_allFiles.empty()) {
        return {};
    }

    if (_nameToIndex.empty()) {
        size_t index = 0u;
        for (const auto& fh : _allFiles) {
            _nameToIndex.emplace(fh._fileName, index);
            ++index;
        }
    }

    if (const auto it = _nameToIndex.find(string(name)); it != _nameToIndex.end()) {
        const auto& fh = _allFiles[it->second];
        auto fs = fh._fileSize;
        auto wt = fh._writeTime;
        auto&& buf = fh._dataSource->OpenFile(fh._filePath, fs, wt);
        RUNTIME_ASSERT(buf);
        return {fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource, std::move(buf)};
    }

    return {};
}

auto FileCollection::FindFileByPath(string_view path) const -> File
{
    if (_allFiles.empty()) {
        return {};
    }

    if (_pathToIndex.empty()) {
        size_t index = 0u;
        for (const auto& fh : _allFiles) {
            _pathToIndex.emplace(fh._filePath, index);
            ++index;
        }
    }

    if (const auto it = _pathToIndex.find(string(path)); it != _pathToIndex.end()) {
        const auto& fh = _allFiles[it->second];
        auto fs = fh._fileSize;
        auto wt = fh._writeTime;
        auto&& buf = fh._dataSource->OpenFile(fh._filePath, fs, wt);
        RUNTIME_ASSERT(buf);
        return {fh._fileName, fh._filePath, fh._fileSize, fh._writeTime, fh._dataSource, std::move(buf)};
    }

    return {};
}

auto FileCollection::GetFilesCount() const -> size_t
{
    return _allFiles.size();
}

void FileSystem::AddDataSource(string_view path, DataSourceType type)
{
    _dataSources.emplace(_dataSources.begin(), path, type);
}

auto FileSystem::FilterFiles(string_view ext) -> FileCollection
{
    return FilterFiles(ext, "", true);
}

auto FileSystem::FilterFiles(string_view ext, string_view dir, bool include_subdirs) -> FileCollection
{
    vector<FileHeader> files;
    unordered_set<string> processed_files;

    for (auto& ds : _dataSources) {
        for (const auto& fname : ds.GetFileNames(dir, include_subdirs, ext)) {
            if (!processed_files.insert(fname).second) {
                continue;
            }

            size_t size = 0u;
            uint64 write_time = 0u;
            const auto ok = ds.IsFilePresent(fname, size, write_time);
            RUNTIME_ASSERT(ok);
            const string name = _str(fname).extractFileName().eraseFileExtension();
            auto file_header = FileHeader(name, fname, size, write_time, &ds);
            files.push_back(std::move(file_header));
        }
    }

    return FileCollection({std::move(files)});
}

auto FileSystem::ReadFile(string_view path) -> File
{
    RUNTIME_ASSERT(!path.empty());
    RUNTIME_ASSERT(path[0] != '.' && path[0] != '/');

    const string name = _str(path).extractFileName().eraseFileExtension();

    for (auto& ds : _dataSources) {
        size_t size = 0u;
        uint64 write_time = 0u;
        if (auto&& buf = ds.OpenFile(path, size, write_time)) {
            return {name, path, size, write_time, &ds, std::move(buf)};
        }
    }

    return {};
}

auto FileSystem::ReadFileText(string_view path) -> string
{
    const auto file = ReadFile(path);
    return file ? file.GetStr() : string();
}

auto FileSystem::ReadFileHeader(string_view path) -> FileHeader
{
    RUNTIME_ASSERT(!path.empty());
    RUNTIME_ASSERT(path[0] != '.' && path[0] != '/');

    const string name = _str(path).extractFileName().eraseFileExtension();

    for (auto& ds : _dataSources) {
        size_t size = 0u;
        uint64 write_time = 0u;
        if (ds.IsFilePresent(path, size, write_time)) {
            return {name, path, size, write_time, &ds};
        }
    }

    return {};
}
