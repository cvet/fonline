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

auto FileHeader::GetNameNoExt() const -> string_view
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

auto FileHeader::GetDiskPath() const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_isLoaded);
    FO_RUNTIME_ASSERT(!_filePath.empty());
    FO_RUNTIME_ASSERT(_dataSource->IsDiskDir());

    return strex(_dataSource->GetPackName()).combine_path(_filePath);
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

auto File::Load(const FileHeader& fh) -> File
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(fh);
    auto size = fh.GetSize();
    auto write_time = fh.GetWriteTime();
    auto buf = fh.GetDataSource()->OpenFile(fh.GetPath(), size, write_time);
    FO_RUNTIME_ASSERT(buf);

    return File(fh.GetPath(), size, write_time, fh.GetDataSource(), std::move(buf));
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

FileReader::FileReader(span<const uint8> buf) :
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
        if (_buf[i] == std::bit_cast<uint8>(fragment[0])) {
            bool not_match = false;

            for (size_t j = 1; j < fragment.size(); j++) {
                if (_buf[numeric_cast<size_t>(i) + j] != std::bit_cast<uint8>(fragment[j])) {
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

    uint32 len = 0;

    while (*(_buf.data() + _curPos + len) != 0) {
        len++;

        if (_curPos + len > _buf.size()) {
            throw FileSystemExeption("Invalid null terminated string length");
        }
    }

    string str(reinterpret_cast<const char*>(_buf.data() + _curPos), len);
    _curPos += len + 1;
    return str;
}

auto FileReader::GetUInt8() -> uint8
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint8) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    return _buf[_curPos++];
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetBEUInt16() -> uint16
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
auto FileReader::GetLEUInt16() -> uint16
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
auto FileReader::GetBEUInt32() -> uint32
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint32) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint32 res = 0;
    auto* cres = reinterpret_cast<uint8*>(&res);

    for (auto i = 3; i >= 0; i--) {
        cres[i] = _buf[_curPos++];
    }

    return res;
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetLEUInt32() -> uint32
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint32) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint32 res = 0;
    auto* cres = reinterpret_cast<uint8*>(&res);

    for (auto i = 0; i <= 3; i++) {
        cres[i] = _buf[_curPos++];
    }

    return res;
}

FileCollection::FileCollection(initializer_list<FileHeader> files)
{
    FO_STACK_TRACE_ENTRY();

    _files.reserve(files.size());
    _nameToIndex.reserve(_files.size());
    _pathToIndex.reserve(_files.size());

    for (const auto& fh : files) {
        _files.emplace_back(fh.Copy());
        _nameToIndex.emplace(_files.back().GetNameNoExt(), _files.size() - 1);
        _pathToIndex.emplace(_files.back().GetPath(), _files.size() - 1);
    }
}

FileCollection::FileCollection(vector<FileHeader> files) :
    _files {std::move(files)}
{
    FO_STACK_TRACE_ENTRY();

    _nameToIndex.reserve(_files.size());
    _pathToIndex.reserve(_files.size());

    size_t index = 0;

    for (const auto& fh : _files) {
        _nameToIndex.emplace(fh.GetNameNoExt(), index);
        _pathToIndex.emplace(fh.GetPath(), index);
        ++index;
    }
}

auto FileCollection::GetFilesCount() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _files.size();
}

auto FileCollection::GetFileByIndex(size_t index) const -> const FileHeader&
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(index < _files.size());
    return _files[index];
}

auto FileCollection::FindFileByName(string_view name_no_ext) const -> File
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _nameToIndex.find(name_no_ext); it != _nameToIndex.end()) {
        const auto& fh = _files[it->second];
        return File::Load(fh);
    }

    return {};
}

auto FileCollection::FindFileByPath(string_view path) const -> File
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _pathToIndex.find(path); it != _pathToIndex.end()) {
        const auto& fh = _files[it->second];
        return File::Load(fh);
    }

    return {};
}

void FileSystem::AddDirSource(string_view dir, bool recursive, bool non_cached, bool maybe_not_available)
{
    FO_STACK_TRACE_ENTRY();

    auto ds = DataSource::MountDir(dir, recursive, non_cached, maybe_not_available);
    _dataSources.emplace(_dataSources.begin(), std::move(ds));
}

void FileSystem::AddPackSource(string_view dir, string_view pack, bool maybe_not_available)
{
    FO_STACK_TRACE_ENTRY();

    if (IsPackaged()) {
        auto ds = DataSource::MountPack(dir, pack, maybe_not_available);
        _dataSources.emplace(_dataSources.begin(), std::move(ds));
    }
    else {
        auto ds = DataSource::MountDir(strex(dir).combine_path(pack), true, false, maybe_not_available);
        _dataSources.emplace(_dataSources.begin(), std::move(ds));
    }
}

void FileSystem::AddPacksSource(string_view dir, const vector<string>& packs)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& pack : packs) {
        AddPackSource(dir, pack);
    }
}

void FileSystem::AddCustomSource(unique_ptr<DataSource> data_source)
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
            if (!processed_files.emplace(path).second) {
                continue;
            }

            size_t size = 0;
            uint64 write_time = 0;
            const auto ok = ds->GetFileInfo(path, size, write_time);
            FO_RUNTIME_ASSERT(ok);
            auto file_header = FileHeader(path, size, write_time, ds.get());
            files.emplace_back(std::move(file_header));
        }
    }

    return FileCollection(std::move(files));
}

auto FileSystem::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!path.empty());
    FO_RUNTIME_ASSERT(path[0] != '.' && path[0] != '/');

    for (const auto& ds : _dataSources) {
        if (ds->IsFileExists(path)) {
            return true;
        }
    }

    return false;
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

        if (ds->GetFileInfo(path, size, write_time)) {
            return FileHeader(path, size, write_time, ds.get());
        }
    }

    return {};
}

FO_END_NAMESPACE();
