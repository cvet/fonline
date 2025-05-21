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

#include "FileSystem.h"
#include "Log.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE();

FileHeader::FileHeader(string_view path, size_t size, uint64 write_time, const DataSource* ds) :
    _isLoaded {true},
    _filePath {path},
    _fileSize {size},
    _writeTime {write_time},
    _dataSource {ds}
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_dataSource);
}

auto FileHeader::GetName() const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(!_filePath.empty());

    string_view name = _filePath;
    auto pos = name.find_last_of("/\\");

    if (pos != string::npos) {
        name = name.substr(pos + 1);
    }

    pos = name.find_last_of('.');

    if (pos != string::npos) {
        name = name.substr(0, pos);
    }

    return name;
}

auto FileHeader::GetPath() const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(!_filePath.empty());

    return _filePath;
}

auto FileHeader::GetFullPath() const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(!_filePath.empty());

    return strex(_dataSource->GetPackName()).combinePath(_filePath);
}

auto FileHeader::GetSize() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);

    return _fileSize;
}

auto FileHeader::GetWriteTime() const -> uint64
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);

    return _writeTime;
}

auto FileHeader::GetDataSource() const -> const DataSource*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);

    return _dataSource.get();
}

auto FileHeader::Copy() const -> FileHeader
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);

    return FileHeader(_filePath, _fileSize, _writeTime, _dataSource.get());
}

File::File(string_view path, size_t size, uint64 write_time, const DataSource* ds, unique_del_ptr<const uint8>&& buf) :
    FileHeader(path, size, write_time, ds),
    _fileBuf {std::move(buf)}
{
    FO_STACK_TRACE_ENTRY();
}

File::File(string_view path, uint64 write_time, const DataSource* ds, const_span<uint8> buf, bool make_copy) :
    FileHeader(path, static_cast<uint>(buf.size()), write_time, ds)
{
    FO_STACK_TRACE_ENTRY();

    if (make_copy) {
        auto buf_copy = SafeAlloc::MakeUniqueArr<uint8>(buf.size());
        MemCopy(buf_copy.get(), buf.data(), buf.size());
        _fileBuf = unique_del_ptr<const uint8> {buf_copy.release(), [](const uint8* p) { delete[] p; }};
    }
    else {
        _fileBuf = unique_del_ptr<const uint8> {buf.data(), [](const uint8* p) { ignore_unused(p); }};
    }
}

auto File::GetStr() const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(_fileBuf);

    return {reinterpret_cast<const char*>(_fileBuf.get()), _fileSize};
}

auto File::GetData() const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(_fileBuf);

    vector<uint8> result;
    result.resize(_fileSize);
    MemCopy(result.data(), _fileBuf.get(), _fileSize);
    return result;
}

auto File::GetBuf() const -> const uint8*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(_fileBuf);

    return _fileBuf.get();
}

auto File::GetReader() const -> FileReader
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(_fileBuf);

    return FileReader({_fileBuf.get(), _fileSize});
}

FileReader::FileReader(const_span<uint8> buf) :
    _buf {buf}
{
    FO_STACK_TRACE_ENTRY();
}

auto FileReader::GetStr() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return {reinterpret_cast<const char*>(_buf.data()), _buf.size()};
}

auto FileReader::GetData() const -> vector<uint8>
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<uint8> result;
    result.resize(_buf.size());
    MemCopy(result.data(), _buf.data(), _buf.size());
    return result;
}

auto FileReader::GetBuf() const -> const uint8*
{
    FO_NO_STACK_TRACE_ENTRY();

    return _buf.data();
}

auto FileReader::GetSize() const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _buf.size();
}

auto FileReader::GetCurBuf() const -> const uint8*
{
    FO_NO_STACK_TRACE_ENTRY();

    return _buf.data() + _curPos;
}

auto FileReader::GetCurPos() const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _curPos;
}

void FileReader::SetCurPos(size_t pos)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(pos <= _buf.size());

    _curPos = pos;
}

void FileReader::GoForward(size_t offs)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curPos + offs <= _buf.size());

    _curPos += offs;
}

void FileReader::GoBack(size_t offs)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(offs <= _curPos);

    _curPos -= offs;
}

auto FileReader::SeekFragment(string_view fragment) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!fragment.empty());

    if (_curPos + fragment.size() > _buf.size()) {
        return false;
    }

    for (size_t i = _curPos; i < _buf.size() - fragment.size(); i++) {
        if (_buf[i] == static_cast<uint8>(fragment[0])) {
            bool not_match = false;

            for (uint j = 1; j < fragment.size(); j++) {
                if (_buf[static_cast<size_t>(i) + j] != static_cast<uint8>(fragment[j])) {
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

void FileReader::CopyData(void* ptr, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    if (size == 0) {
        return;
    }
    if (_curPos + size > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    MemCopy(ptr, _buf.data() + _curPos, size);
    _curPos += size;
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetStrNT() -> string
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + 1 > _buf.size()) {
        throw FileSystemExeption("Invalid read pos");
    }

    uint len = 0;

    while (*(_buf.data() + _curPos + len) != 0) {
        len++;
    }

    string str(reinterpret_cast<const char*>(_buf.data() + _curPos), len);
    _curPos += len + 1;
    return str;
}

auto FileReader::GetUChar() -> uint8
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint8) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    return _buf[_curPos++];
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetBEUShort() -> uint16
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint16) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint16 res = 0;
    auto* cres = reinterpret_cast<uint8*>(&res);
    cres[1] = _buf[_curPos++];
    cres[0] = _buf[_curPos++];
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetLEUShort() -> uint16
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint16) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint16 res = 0;
    auto* cres = reinterpret_cast<uint8*>(&res);
    cres[0] = _buf[_curPos++];
    cres[1] = _buf[_curPos++];
    return res;
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetBEUInt() -> uint
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint res = 0;
    auto* cres = reinterpret_cast<uint8*>(&res);

    for (auto i = 3; i >= 0; i--) {
        cres[i] = _buf[_curPos++];
    }

    return res;
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetLEUInt() -> uint
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint res = 0;
    auto* cres = reinterpret_cast<uint8*>(&res);

    for (auto i = 0; i <= 3; i++) {
        cres[i] = _buf[_curPos++];
    }

    return res;
}

FileCollection::FileCollection(initializer_list<FileHeader> files)
{
    FO_STACK_TRACE_ENTRY();

    _allFiles.reserve(files.size());

    for (const auto& file : files) {
        _allFiles.emplace_back(file.Copy());
    }
}

FileCollection::FileCollection(vector<FileHeader> files) :
    _allFiles {std::move(files)}
{
    FO_STACK_TRACE_ENTRY();
}

auto FileCollection::MoveNext() -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    return ++_curFileIndex < static_cast<int>(_allFiles.size());
}

void FileCollection::ResetCounter()
{
    FO_STACK_TRACE_ENTRY();

    _curFileIndex = -1;
}

auto FileCollection::GetCurFile() const -> File
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curFileIndex >= 0);
    FO_RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    const auto& fh = _allFiles[_curFileIndex];
    auto fs = fh.GetSize();
    auto wt = fh.GetWriteTime();
    auto buf = fh.GetDataSource()->OpenFile(fh.GetPath(), fs, wt);
    FO_RUNTIME_ASSERT(buf);
    return File(fh.GetPath(), fh.GetSize(), fh.GetWriteTime(), fh.GetDataSource(), std::move(buf));
}

auto FileCollection::GetCurFileHeader() const -> FileHeader
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_curFileIndex >= 0);
    FO_RUNTIME_ASSERT(_curFileIndex < static_cast<int>(_allFiles.size()));

    const auto& fh = _allFiles[_curFileIndex];
    return FileHeader(fh.GetPath(), fh.GetSize(), fh.GetWriteTime(), fh.GetDataSource());
}

auto FileCollection::FindFileByName(string_view name) const -> File
{
    FO_STACK_TRACE_ENTRY();

    if (_allFiles.empty()) {
        return {};
    }

    if (_nameToIndex.empty()) {
        size_t index = 0;

        for (const auto& fh : _allFiles) {
            _nameToIndex.emplace(fh.GetName(), index);
            ++index;
        }
    }

    if (const auto it = _nameToIndex.find(name); it != _nameToIndex.end()) {
        const auto& fh = _allFiles[it->second];
        auto fs = fh.GetSize();
        auto wt = fh.GetWriteTime();
        auto buf = fh.GetDataSource()->OpenFile(fh.GetPath(), fs, wt);
        FO_RUNTIME_ASSERT(buf);
        return File(fh.GetPath(), fh.GetSize(), fh.GetWriteTime(), fh.GetDataSource(), std::move(buf));
    }

    return {};
}

auto FileCollection::FindFileByPath(string_view path) const -> File
{
    FO_STACK_TRACE_ENTRY();

    if (_allFiles.empty()) {
        return {};
    }

    if (_pathToIndex.empty()) {
        size_t index = 0;

        for (const auto& fh : _allFiles) {
            _pathToIndex.emplace(fh.GetPath(), index);
            ++index;
        }
    }

    if (const auto it = _pathToIndex.find(path); it != _pathToIndex.end()) {
        const auto& fh = _allFiles[it->second];
        auto fs = fh.GetSize();
        auto wt = fh.GetWriteTime();
        auto buf = fh.GetDataSource()->OpenFile(fh.GetPath(), fs, wt);
        FO_RUNTIME_ASSERT(buf);
        return File(fh.GetPath(), fh.GetSize(), fh.GetWriteTime(), fh.GetDataSource(), std::move(buf));
    }

    return {};
}

auto FileCollection::GetFilesCount() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _allFiles.size();
}

auto FileCollection::Copy() const -> FileCollection
{
    FO_STACK_TRACE_ENTRY();

    vector<FileHeader> files;
    files.reserve(_allFiles.size());

    for (const auto& file : _allFiles) {
        files.emplace_back(file.Copy());
    }

    return FileCollection(std::move(files));
}

void FileSystem::AddDataSource(string_view path, DataSourceType type)
{
    FO_STACK_TRACE_ENTRY();

    auto ds = DataSource::Mount(path, type);

    _dataSources.emplace(_dataSources.begin(), std::move(ds));
}

void FileSystem::AddDataSource(unique_ptr<DataSource> data_source)
{
    FO_STACK_TRACE_ENTRY();

    _dataSources.emplace(_dataSources.begin(), std::move(data_source));
}

void FileSystem::CleanDataSources()
{
    FO_STACK_TRACE_ENTRY();

    _dataSources.clear();
}

auto FileSystem::GetAllFiles() const -> FileCollection
{
    FO_STACK_TRACE_ENTRY();

    return FilterFiles("");
}

auto FileSystem::FilterFiles(string_view ext, string_view dir, bool recursive) const -> FileCollection
{
    FO_STACK_TRACE_ENTRY();

    vector<FileHeader> files;
    unordered_set<string> processed_files;

    for (const auto& ds : _dataSources) {
        for (const auto& path : ds->GetFileNames(dir, recursive, ext)) {
            if (!processed_files.insert(path).second) {
                continue;
            }

            size_t size = 0;
            uint64 write_time = 0;
            const auto ok = ds->IsFilePresent(path, size, write_time);
            FO_RUNTIME_ASSERT(ok);
            auto file_header = FileHeader(path, size, write_time, ds.get());
            files.emplace_back(std::move(file_header));
        }
    }

    return FileCollection(std::move(files));
}

auto FileSystem::ReadFile(string_view path) const -> File
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!path.empty());
    FO_RUNTIME_ASSERT(path[0] != '.' && path[0] != '/');

    for (const auto& ds : _dataSources) {
        size_t size = 0;
        uint64 write_time = 0;

        if (auto buf = ds->OpenFile(path, size, write_time)) {
            return File(path, size, write_time, ds.get(), std::move(buf));
        }
    }

    return {};
}

auto FileSystem::ReadFileText(string_view path) const -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto file = ReadFile(path);
    return file ? file.GetStr() : string();
}

auto FileSystem::ReadFileHeader(string_view path) const -> FileHeader
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!path.empty());
    FO_RUNTIME_ASSERT(path[0] != '.' && path[0] != '/');

    for (const auto& ds : _dataSources) {
        size_t size = 0;
        uint64 write_time = 0;

        if (ds->IsFilePresent(path, size, write_time)) {
            return FileHeader(path, size, write_time, ds.get());
        }
    }

    return {};
}

FO_END_NAMESPACE();
