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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

FileHeader::FileHeader(string_view path, size_t size, uint64_t write_time, ptr<const DataSource> ds) :
    _isLoaded {true},
    _filePath {path},
    _fileSize {size},
    _writeTime {write_time},
    _dataSource {ds}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_dataSource, "Missing required data source");
}

auto FileHeader::GetNameNoExt() const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(!_filePath.empty(), "Loaded file header has an empty path while extracting the resource name", _fileSize, _writeTime);

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

auto FileHeader::GetPath() const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(!_filePath.empty(), "Loaded file header has an empty path while returning the resource path", _fileSize, _writeTime);

    return _filePath;
}

auto FileHeader::GetDiskPath() const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(!_filePath.empty(), "Loaded file header has an empty path while building a disk path", _dataSource->GetPackName(), _fileSize, _writeTime);
    FO_VERIFY_AND_THROW(_dataSource->IsDiskDir(), "File header disk path requested from a non-directory data source", _filePath, _dataSource->GetPackName());

    auto data_source = _dataSource.as_ptr();
    return strex(data_source->GetPackName()).combine_path(_filePath);
}

auto FileHeader::GetSize() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");

    return _fileSize;
}

auto FileHeader::GetWriteTime() const -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");

    return _writeTime;
}

auto FileHeader::GetDataSource() const -> ptr<const DataSource>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");

    return _dataSource.as_ptr();
}

auto FileHeader::Copy() const -> FileHeader
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");

    auto data_source = _dataSource.as_ptr();
    return FileHeader(_filePath, _fileSize, _writeTime, data_source);
}

static auto TakeFileBuffer(auto&& buf) -> unique_del_ptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(buf, "File buffer holder is empty");

    function<void(const uint8_t*)> deleter = std::move(buf.get_underlying().get_deleter());
    nptr<const uint8_t> nullable_file_buf = buf.release();
    FO_VERIFY_AND_THROW(nullable_file_buf, "Released file buffer pointer is null");

    auto file_buf = nullable_file_buf.as_ptr();
    return make_unique_del_ptr(file_buf, std::move(deleter));
}

File::File(string_view path, size_t size, uint64_t write_time, ptr<const DataSource> ds, unique_del_ptr<const uint8_t>&& buf) :
    FileHeader(path, size, write_time, ds),
    _fileBuf {std::move(buf)}
{
    FO_STACK_TRACE_ENTRY();
}

auto File::Load(const FileHeader& fh) -> File
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(fh, "File header is null");
    auto size = fh.GetSize();
    auto write_time = fh.GetWriteTime();
    auto data_source = fh.GetDataSource();
    auto buf = data_source->OpenFile(fh.GetPath(), size, write_time);
    FO_VERIFY_AND_THROW(buf, "Missing required buffer");

    return File(fh.GetPath(), size, write_time, data_source, TakeFileBuffer(std::move(buf)));
}

auto File::GetStr() const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(_fileBuf, "Input file buffer is empty");

    string result;
    result.resize(_fileSize);

    if (!result.empty()) {
        ptr<char> target = result.data();
        auto source = _fileBuf.as_ptr();
        MemCopy(target.get(), source.get(), result.size());
    }

    return result;
}

auto File::GetData() const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(_fileBuf, "Input file buffer is empty");

    vector<uint8_t> result;
    result.resize(_fileSize);

    if (!result.empty()) {
        ptr<uint8_t> target = result.data();
        auto source = _fileBuf.as_ptr();
        MemCopy(target.get(), source.get(), result.size());
    }

    return result;
}

auto File::GetBuf() const -> ptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(_fileBuf, "Input file buffer is empty");

    auto file_data = _fileBuf.as_ptr();
    return file_data;
}

auto File::GetDataSpan() const -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(_fileBuf, "Input file buffer is empty");

    auto file_data = _fileBuf.as_ptr();
    return const_span<uint8_t> {file_data.get(), _fileSize};
}

auto File::GetReader() const -> FileReader
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_isLoaded, "Resource is not loaded");
    FO_VERIFY_AND_THROW(_fileBuf, "Input file buffer is empty");

    auto file_data = _fileBuf.as_ptr();
    const_span<uint8_t> file_span = {file_data.get(), _fileSize};
    return FileReader(file_span);
}

FileReader::FileReader(const_span<uint8_t> buf) :
    _buf {buf}
{
    FO_STACK_TRACE_ENTRY();
}

auto FileReader::GetStr() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    string result;
    result.resize(_buf.size());

    if (!result.empty()) {
        ptr<char> target = result.data();
        auto source = ptr<const uint8_t> {_buf.data()};
        MemCopy(target.get(), source.get(), result.size());
    }

    return result;
}

auto FileReader::GetData() const -> vector<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<uint8_t> result;
    result.resize(_buf.size());

    if (!result.empty()) {
        ptr<uint8_t> target = result.data();
        auto source = ptr<const uint8_t> {_buf.data()};
        MemCopy(target.get(), source.get(), result.size());
    }

    return result;
}

auto FileReader::GetBuf() const -> nptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_buf.empty()) {
        return nullptr;
    }

    return ptr<const uint8_t> {_buf.data()};
}

auto FileReader::GetDataSpan() const -> const_span<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _buf;
}

auto FileReader::GetSize() const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _buf.size();
}

auto FileReader::GetCurBuf() const -> nptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_curPos == _buf.size()) {
        return nullptr;
    }

    return ptr<const uint8_t> {_buf.data()}.offset(_curPos);
}

auto FileReader::GetCurDataSpan(size_t size) const -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (size == 0) {
        return {};
    }

    if (size > _buf.size() - _curPos) {
        throw FileSystemExeption("Invalid read size");
    }

    auto data = ptr<const uint8_t> {_buf.data()}.offset(_curPos);
    return const_span<uint8_t> {data.get(), size};
}

auto FileReader::GetCurPos() const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _curPos;
}

void FileReader::SetCurPos(size_t pos)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pos <= _buf.size(), "File reader seek position is outside the loaded buffer", pos, _buf.size(), _curPos);

    _curPos = pos;
}

void FileReader::GoForward(size_t offs)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_curPos + offs <= _buf.size(), "File reader forward seek would move past the loaded buffer", _curPos, offs, _buf.size());

    _curPos += offs;
}

void FileReader::GoBack(size_t offs)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(offs <= _curPos, "File reader cannot move back before the beginning of the buffer", offs, _curPos);

    _curPos -= offs;
}

auto FileReader::SeekFragment(string_view fragment) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!fragment.empty(), "File reader fragment search received an empty pattern", _curPos, _buf.size());

    if (_curPos + fragment.size() > _buf.size()) {
        return false;
    }

    for (size_t i = _curPos; i <= _buf.size() - fragment.size(); i++) {
        if (_buf[i] == std::bit_cast<uint8_t>(fragment[0])) {
            bool not_match = false;

            for (size_t j = 1; j < fragment.size(); j++) {
                if (_buf[numeric_cast<size_t>(i) + j] != std::bit_cast<uint8_t>(fragment[j])) {
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

void FileReader::CopyData(span<uint8_t> buf)
{
    FO_STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return;
    }
    if (_curPos + buf.size() > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    ptr<uint8_t> target = buf.data();
    auto source = ptr<const uint8_t> {_buf.data()}.offset(_curPos);
    MemCopy(target.get(), source.get(), buf.size());
    _curPos += buf.size();
}

void FileReader::ReadBytes(span<uint8_t> out)
{
    FO_STACK_TRACE_ENTRY();

    if (out.empty()) {
        return;
    }

    CopyData(out);
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetStrNT() -> string
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + 1 > _buf.size()) {
        throw FileSystemExeption("Invalid read pos");
    }

    uint32_t len = 0;

    while (true) {
        if (_curPos + len >= _buf.size()) {
            throw FileSystemExeption("Invalid null terminated string length");
        }

        auto cur_byte = ptr<const uint8_t> {_buf.data()}.offset(_curPos + len);

        if (*cur_byte == 0) {
            break;
        }

        len++;
    }

    string str;
    str.resize(numeric_cast<size_t>(len));

    if (!str.empty()) {
        ptr<char> target = str.data();
        auto source = ptr<const uint8_t> {_buf.data()}.offset(_curPos);
        MemCopy(target.get(), source.get(), str.size());
    }

    _curPos += len + 1;
    return str;
}

auto FileReader::GetUInt8() -> uint8_t
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint8_t) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    return _buf[_curPos++];
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetBEUInt16() -> uint16_t
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint16_t) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    const uint32_t high_byte = _buf[_curPos++];
    const uint32_t low_byte = _buf[_curPos++];
    return numeric_cast<uint16_t>((high_byte << 8) | low_byte);
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetLEUInt16() -> uint16_t
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint16_t) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    const uint32_t low_byte = _buf[_curPos++];
    const uint32_t high_byte = _buf[_curPos++];
    return numeric_cast<uint16_t>(low_byte | (high_byte << 8));
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetBEUInt32() -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint32_t) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint32_t res = 0;

    for (size_t i = 0; i != sizeof(uint32_t); i++) {
        res = (res << 8) | _buf[_curPos++];
    }

    return res;
}

// ReSharper disable once CppInconsistentNaming
auto FileReader::GetLEUInt32() -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (_curPos + sizeof(uint32_t) > _buf.size()) {
        throw FileSystemExeption("Invalid read size");
    }

    uint32_t res = 0;

    for (size_t i = 0; i != sizeof(uint32_t); i++) {
        res |= numeric_cast<uint32_t>(_buf[_curPos++]) << (i * 8);
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
        _pathToIndex.emplace(string(_files.back().GetPath()), _files.size() - 1);
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
        _pathToIndex.emplace(string(fh.GetPath()), index);
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

    FO_VERIFY_AND_THROW(index < _files.size(), "File collection index is outside the collected file list", index, _files.size());
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

    for (size_t i = 0; i != _dataSources.size(); ++i) {
        auto ds = _dataSources[i].as_ptr();

        for (const auto& path : ds->GetFileNames(dir, recursive, ext)) {
            if (!processed_files.emplace(path).second) {
                continue;
            }

            size_t size = 0;
            uint64_t write_time = 0;
            const auto ok = ds->GetFileInfo(path, size, write_time);
            FO_VERIFY_AND_THROW(ok, "Data source listed a file but did not return its metadata", ds->GetPackName(), path);
            auto file_header = FileHeader(path, size, write_time, ds);
            files.emplace_back(std::move(file_header));
        }
    }

    return FileCollection(std::move(files));
}

auto FileSystem::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!path.empty(), "File existence check received an empty resource path", _dataSources.size());
    FO_VERIFY_AND_THROW(path[0] != '.' && path[0] != '/', "File existence check received a non-relative resource path", path);

    for (size_t i = 0; i != _dataSources.size(); ++i) {
        auto ds = _dataSources[i].as_ptr();

        if (ds->IsFileExists(path)) {
            return true;
        }
    }

    return false;
}

auto FileSystem::ReadFile(string_view path) const -> File
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!path.empty(), "File read requested an empty resource path", _dataSources.size());
    FO_VERIFY_AND_THROW(path[0] != '.' && path[0] != '/', "File read requested a non-relative resource path", path);

    for (size_t i = 0; i != _dataSources.size(); ++i) {
        auto ds = _dataSources[i].as_ptr();
        size_t size = 0;
        uint64_t write_time = 0;

        if (auto buf = ds->OpenFile(path, size, write_time)) {
            return File(path, size, write_time, ds, TakeFileBuffer(std::move(buf)));
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

    FO_VERIFY_AND_THROW(!path.empty(), "File header read requested an empty resource path", _dataSources.size());
    FO_VERIFY_AND_THROW(path[0] != '.' && path[0] != '/', "File header read requested a non-relative resource path", path);

    for (size_t i = 0; i != _dataSources.size(); ++i) {
        auto ds = _dataSources[i].as_ptr();
        size_t size = 0;
        uint64_t write_time = 0;

        if (ds->GetFileInfo(path, size, write_time)) {
            return FileHeader(path, size, write_time, ds);
        }
    }

    return {};
}

FO_END_NAMESPACE
