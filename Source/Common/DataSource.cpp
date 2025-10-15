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

#include "DataSource.h"
#include "EmbeddedResources-Include.h"

#include "minizip/unzip.h"

FO_BEGIN_NAMESPACE();

static auto GetFileNamesGeneric(const vector<string>& fnames, string_view dir, bool recursive, string_view ext) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    string dir_fixed = strex(dir).normalizePathSlashes();

    if (!dir_fixed.empty() && dir_fixed.back() != '/') {
        dir_fixed += "/";
    }

    vector<string> result;
    const auto len = dir_fixed.length();

    for (const auto& fname : fnames) {
        bool add = false;

        if (fname.compare(0, len, dir_fixed) == 0 && (recursive || (len > 0 && fname.find_last_of('/') < len) || (len == 0 && fname.find_last_of('/') == string::npos))) {
            if (ext.empty() || strex(fname).getFileExtension() == ext) {
                add = true;
            }
        }

        if (add && std::ranges::find(result, fname) == result.end()) {
            result.push_back(fname);
        }
    }

    return result;
}

class DummySpace final : public DataSource
{
public:
    DummySpace() = default;
    DummySpace(const DummySpace&) = delete;
    DummySpace(DummySpace&&) noexcept = default;
    auto operator=(const DummySpace&) = delete;
    auto operator=(DummySpace&&) noexcept = delete;
    ~DummySpace() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Dummy"; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;
};

class NonCachedDir final : public DataSource
{
public:
    explicit NonCachedDir(string_view fname, bool recursive);
    NonCachedDir(const NonCachedDir&) = delete;
    NonCachedDir(NonCachedDir&&) noexcept = default;
    auto operator=(const NonCachedDir&) = delete;
    auto operator=(NonCachedDir&&) noexcept = delete;
    ~NonCachedDir() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return true; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _baseDir; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    bool _recusive;
    string _baseDir {};
};

class CachedDir final : public DataSource
{
public:
    CachedDir(string_view fname, bool recursive);
    CachedDir(const CachedDir&) = delete;
    CachedDir(CachedDir&&) noexcept = default;
    auto operator=(const CachedDir&) = delete;
    auto operator=(CachedDir&&) noexcept = delete;
    ~CachedDir() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return true; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _baseDir; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    struct FileEntry
    {
        string FileName {};
        size_t FileSize {};
        uint64 WriteTime {};
    };

    unordered_map<string, FileEntry> _filesTree {};
    vector<string> _filesTreeNames {};
    string _baseDir {};
};

class FalloutDat final : public DataSource
{
public:
    explicit FalloutDat(string_view fname);
    FalloutDat(const FalloutDat&) = delete;
    FalloutDat(FalloutDat&&) noexcept = default;
    auto operator=(const FalloutDat&) = delete;
    auto operator=(FalloutDat&&) noexcept = delete;
    ~FalloutDat() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext); }

private:
    auto ReadTree() -> bool;

    mutable DiskFile _datFile;
    unordered_map<string, uint8*> _filesTree {};
    vector<string> _filesTreeNames {};
    string _fileName {};
    unique_ptr<uint8[]> _memTree {};
    uint64 _writeTime {};
    mutable vector<uint8> _readBuf {};
};

class ZipFile final : public DataSource
{
public:
    explicit ZipFile(string_view fname);
    ZipFile(const ZipFile&) = delete;
    ZipFile(ZipFile&&) noexcept = default;
    auto operator=(const ZipFile&) = delete;
    auto operator=(ZipFile&&) noexcept = delete;
    ~ZipFile() override;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext); }

private:
    struct ZipFileInfo
    {
        unz_file_pos Pos {};
        int32 UncompressedSize {};
    };

    auto ReadTree() -> bool;

    unordered_map<string, ZipFileInfo> _filesTree {};
    vector<string> _filesTreeNames {};
    string _fileName {};
    unzFile _zipHandle {};
    uint64 _writeTime {};
};

class FilesList final : public DataSource
{
public:
    FilesList();
    FilesList(const FilesList&) = delete;
    FilesList(FilesList&&) noexcept = default;
    auto operator=(const FilesList&) = delete;
    auto operator=(FilesList&&) noexcept = delete;
    ~FilesList() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _packName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    struct FileEntry
    {
        string FileName {};
        size_t FileSize {};
        uint64 WriteTime {};
    };

    string _packName {"@FilesList"};
    unordered_map<string, FileEntry> _filesTree {};
    vector<string> _filesTreeNames {};
};

auto DataSource::MountDir(string_view dir, bool recursive, bool non_cached, bool maybe_not_available) -> unique_ptr<DataSource>
{
    FO_STACK_TRACE_ENTRY();

    if (!DiskFileSystem::IsDir(dir)) {
        if (maybe_not_available) {
            return SafeAlloc::MakeUnique<DummySpace>();
        }

        throw DataSourceException("Directory not found", dir);
    }

    if (non_cached) {
        return SafeAlloc::MakeUnique<NonCachedDir>(dir, recursive);
    }
    else {
        return SafeAlloc::MakeUnique<CachedDir>(dir, recursive);
    }
}

auto DataSource::MountPack(string_view dir, string_view name, bool maybe_not_available) -> unique_ptr<DataSource>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!name.empty());

    const auto is_file_present = [](string_view path) -> bool {
        const auto file = DiskFileSystem::OpenFile(path, false);
        return !!file;
    };

    const string path = strex(dir).combinePath(name);

    if (name == "Embedded") {
        return SafeAlloc::MakeUnique<ZipFile>("@Embedded");
    }
    else if (name == "FilesList") {
        return SafeAlloc::MakeUnique<FilesList>();
    }
    else if (is_file_present(strex("{}.zip", path))) {
        return SafeAlloc::MakeUnique<ZipFile>(strex("{}.zip", path));
    }
    else if (is_file_present(strex("{}.bos", path))) {
        return SafeAlloc::MakeUnique<ZipFile>(strex("{}.bos", path));
    }
    else if (is_file_present(strex("{}.dat", path))) {
        return SafeAlloc::MakeUnique<FalloutDat>(strex("{}.dat", path));
    }

    if (maybe_not_available) {
        return SafeAlloc::MakeUnique<DummySpace>();
    }

    throw DataSourceException("Data pack not found", path);
}

auto DummySpace::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(path);
    return false;
}

auto DummySpace::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(path, size, write_time);
    return false;
}

auto DummySpace::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(path, size, write_time);
    return nullptr;
}

auto DummySpace::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(dir, recursive, ext);
    return {};
}

NonCachedDir::NonCachedDir(string_view fname, bool recursive) :
    _recusive {recursive}
{
    FO_STACK_TRACE_ENTRY();

    _baseDir = fname;
    _baseDir = DiskFileSystem::ResolvePath(_baseDir);
    _baseDir += "/";
}

auto NonCachedDir::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_recusive && !strex(path).extractDir().empty()) {
        return false;
    }

    const string full_path = strex(_baseDir).combinePath(path);

    if (!DiskFileSystem::IsExists(full_path)) {
        return false;
    }
    if (DiskFileSystem::IsDir(full_path)) {
        return false;
    }

    return true;
}

auto NonCachedDir::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_recusive && !strex(path).extractDir().empty()) {
        return false;
    }

    const string full_path = strex(_baseDir).combinePath(path);
    auto file = DiskFileSystem::OpenFile(full_path, false);

    if (!file) {
        return false;
    }

    size = file.GetSize();
    write_time = DiskFileSystem::GetWriteTime(full_path);
    return true;
}

auto NonCachedDir::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    if (!_recusive && !strex(path).extractDir().empty()) {
        return nullptr;
    }

    const string full_path = strex(_baseDir).combinePath(path);
    auto file = DiskFileSystem::OpenFile(full_path, false);

    if (!file) {
        return nullptr;
    }

    size = file.GetSize();
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size);

    if (!file.Read(buf.get(), size)) {
        throw DataSourceException("Can't read file from non cached dir", _baseDir, path);
    }

    write_time = DiskFileSystem::GetWriteTime(full_path);
    return unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) { delete[] p; }};
}

auto NonCachedDir::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    if (!_recusive && !dir.empty()) {
        return {};
    }

    vector<string> fnames;

    DiskFileSystem::IterateDir(strex(_baseDir).combinePath(dir), recursive && _recusive, [&fnames](string_view path2, size_t size, uint64 write_time) {
        ignore_unused(size, write_time);
        fnames.emplace_back(path2);
    });

    return GetFileNamesGeneric(fnames, dir, recursive, ext);
}

CachedDir::CachedDir(string_view fname, bool recursive)
{
    FO_STACK_TRACE_ENTRY();

    _baseDir = fname;
    _baseDir = DiskFileSystem::ResolvePath(_baseDir);
    _baseDir += "/";

    DiskFileSystem::IterateDir(_baseDir, recursive, [this](string_view path, size_t size, uint64 write_time) {
        FileEntry fe;
        fe.FileName = strex("{}{}", _baseDir, path);
        fe.FileSize = size;
        fe.WriteTime = write_time;

        _filesTree.emplace(path, std::move(fe));
        _filesTreeNames.emplace_back(path);
    });
}

auto CachedDir::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto CachedDir::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;
}

auto CachedDir::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = DiskFileSystem::OpenFile(fe.FileName, false);

    if (!file) {
        throw DataSourceException("Can't read file from cached dir", _baseDir, path);
    }

    size = fe.FileSize;
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size);

    if (!file.Read(buf.get(), size)) {
        return nullptr;
    }

    write_time = fe.WriteTime;
    return unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) { delete[] p; }};
}

auto CachedDir::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext);
}

FalloutDat::FalloutDat(string_view fname) :
    _datFile {DiskFileSystem::OpenFile(fname, false)}
{
    FO_STACK_TRACE_ENTRY();

    _fileName = fname;
    _readBuf.resize(0x40000);

    if (!_datFile) {
        throw DataSourceException("Cannot open fallout dat file", fname);
    }

    _writeTime = DiskFileSystem::GetWriteTime(fname);

    if (!ReadTree()) {
        throw DataSourceException("Read fallout dat file tree failed");
    }
}

auto FalloutDat::ReadTree() -> bool
{
    FO_STACK_TRACE_ENTRY();

    uint32 version = 0;

    if (!_datFile.SetReadPos(-12, DiskFileSeek::End)) {
        return false;
    }
    if (!_datFile.Read(&version, 4)) {
        return false;
    }

    // DAT 2.1 Arcanum
    if (version == 0x44415431) { // 1TAD
        if (!_datFile.SetReadPos(-4, DiskFileSeek::End)) {
            return false;
        }

        uint32 tree_size = 0;

        if (!_datFile.Read(&tree_size, 4)) {
            return false;
        }

        // Read tree
        if (!_datFile.SetReadPos(-numeric_cast<int32>(tree_size), DiskFileSeek::End)) {
            return false;
        }

        uint32 files_total = 0;

        if (!_datFile.Read(&files_total, 4)) {
            return false;
        }

        tree_size -= 28 + 4; // Subtract information block and files total
        _memTree = SafeAlloc::MakeUniqueArr<uint8>(tree_size);
        MemFill(_memTree.get(), 0, tree_size);

        if (!_datFile.Read(_memTree.get(), tree_size)) {
            return false;
        }

        // Indexing tree
        auto* ptr = _memTree.get();
        const auto* end_ptr = _memTree.get() + tree_size;

        while (ptr < end_ptr) {
            uint32 fnsz = 0;
            MemCopy(&fnsz, ptr, sizeof(fnsz));
            uint32 type = 0;
            MemCopy(&type, ptr + 4 + fnsz + 4, sizeof(type));

            if (fnsz != 0 && type != 0x400) { // Not folder
                string name = strex(string(reinterpret_cast<const char*>(ptr) + 4, fnsz)).normalizePathSlashes();

                if (type == 2) {
                    *(ptr + 4 + fnsz + 7) = 1; // Compressed
                }

                _filesTree.emplace(name, ptr + 4 + fnsz + 7);
                _filesTreeNames.emplace_back(std::move(name));
            }

            ptr += numeric_cast<size_t>(fnsz) + 24;
        }

        return true;
    }

    // DAT 2.0 Fallout2
    if (!_datFile.SetReadPos(-8, DiskFileSeek::End)) {
        return false;
    }

    uint32 tree_size = 0;
    if (!_datFile.Read(&tree_size, 4)) {
        return false;
    }

    uint32 dat_size = 0;
    if (!_datFile.Read(&dat_size, 4)) {
        return false;
    }

    // Check for DAT1.0 Fallout1 dat file
    if (!_datFile.SetReadPos(0, DiskFileSeek::Set)) {
        return false;
    }

    uint32 dir_count = 0;
    if (!_datFile.Read(&dir_count, 4)) {
        return false;
    }

    dir_count >>= 24;
    if (dir_count == 0x01 || dir_count == 0x33) {
        return false;
    }

    // Check for truncated
    if (_datFile.GetSize() != dat_size) {
        return false;
    }

    // Read tree
    if (!_datFile.SetReadPos(-(numeric_cast<int32>(tree_size) + 8), DiskFileSeek::End)) {
        return false;
    }

    uint32 files_total = 0;
    if (!_datFile.Read(&files_total, 4)) {
        return false;
    }

    tree_size -= 4;
    _memTree = SafeAlloc::MakeUniqueArr<uint8>(tree_size);

    if (!_datFile.Read(_memTree.get(), tree_size)) {
        return false;
    }

    // Indexing tree
    auto* ptr = _memTree.get();
    const auto* end_ptr = _memTree.get() + tree_size;

    while (ptr < end_ptr) {
        uint32 name_len = 0;
        MemCopy(&name_len, ptr, 4);

        if (ptr + 4 + name_len > end_ptr) {
            return false;
        }

        if (name_len != 0) {
            string name = strex(string(reinterpret_cast<const char*>(ptr) + 4, name_len)).normalizePathSlashes();

            _filesTree.emplace(name, ptr + 4 + name_len);
            _filesTreeNames.emplace_back(std::move(name));
        }

        ptr += numeric_cast<size_t>(4) + name_len + 13;
    }

    return true;
}

auto FalloutDat::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_datFile) {
        return false;
    }

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto FalloutDat::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_datFile) {
        return false;
    }

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    uint32 real_size = 0;
    MemCopy(&real_size, it->second + 1, sizeof(real_size));

    size = real_size;
    write_time = _writeTime;
    return true;
}

auto FalloutDat::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto* ptr = it->second;
    uint8 type = 0;
    MemCopy(&type, ptr, sizeof(type));
    uint32 real_size = 0;
    MemCopy(&real_size, ptr + 1, sizeof(real_size));
    uint32 packed_size = 0;
    MemCopy(&packed_size, ptr + 5, sizeof(packed_size));
    int32 offset = 0;
    MemCopy(&offset, ptr + 9, sizeof(offset));

    if (!_datFile.SetReadPos(offset, DiskFileSeek::Set)) {
        throw DataSourceException("Can't read file from fallout dat (1)", path);
    }

    size = real_size;
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size);

    if (type == 0) {
        // Plane data
        if (!_datFile.Read(buf.get(), size)) {
            throw DataSourceException("Can't read file from fallout dat (2)", path);
        }
    }
    else {
        // Packed data
        z_stream stream = {};
        stream.zalloc = [](voidpf, uInt items, uInt size_) -> void* {
            constexpr SafeAllocator<uint8> allocator;
            return allocator.allocate(numeric_cast<size_t>(items) * size_);
        };
        stream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8> allocator;
            allocator.deallocate(static_cast<uint8*>(address), 0);
        };

        if (inflateInit(&stream) != Z_OK) {
            throw DataSourceException("Can't read file from fallout dat (3)", path);
        }

        stream.next_in = nullptr;
        stream.avail_in = 0;
        stream.next_out = buf.get();
        stream.avail_out = real_size;

        auto left = packed_size;

        while (stream.avail_out != 0) {
            if (stream.avail_in == 0 && left > 0) {
                stream.next_in = _readBuf.data();
                const auto len = std::min(left, numeric_cast<uint32>(_readBuf.size()));

                if (!_datFile.Read(_readBuf.data(), len)) {
                    throw DataSourceException("Can't read file from fallout dat (4)", path);
                }

                stream.avail_in = len;
                left -= len;
            }

            const auto r = inflate(&stream, Z_NO_FLUSH);

            if (r != Z_OK && r != Z_STREAM_END) {
                throw DataSourceException("Can't read file from fallout dat (5)", path);
            }
            if (r == Z_STREAM_END) {
                break;
            }
        }

        inflateEnd(&stream);
    }

    write_time = _writeTime;
    return unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) { delete[] p; }};
}

ZipFile::ZipFile(string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    _fileName = fname;
    _zipHandle = nullptr;

    zlib_filefunc_def ffunc;

    if (fname[0] != '@') {
        auto p_file = SafeAlloc::MakeUnique<DiskFile>(DiskFileSystem::OpenFile(fname, false));

        if (!*p_file) {
            throw DataSourceException("Can't open zip file", fname);
        }

        _writeTime = DiskFileSystem::GetWriteTime(fname);

        ffunc.zopen_file = [](voidpf opaque, const char*, int32) -> voidpf { return opaque; };
        ffunc.zread_file = [](voidpf, voidpf stream, void* buf, uLong size) -> uLong {
            auto* file = static_cast<DiskFile*>(stream);
            return file->Read(buf, size) ? size : 0;
        };
        ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
        ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
            auto* file = static_cast<DiskFile*>(stream);
            return numeric_cast<long>(file->GetReadPos());
        };
        ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int32 origin) -> long {
            auto* file = static_cast<DiskFile*>(stream);
            switch (origin) {
            case ZLIB_FILEFUNC_SEEK_SET:
                file->SetReadPos(numeric_cast<int32>(offset), DiskFileSeek::Set);
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                file->SetReadPos(numeric_cast<int32>(offset), DiskFileSeek::Cur);
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                file->SetReadPos(numeric_cast<int32>(offset), DiskFileSeek::End);
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [](voidpf, voidpf stream) -> int32 {
            const auto* file = static_cast<DiskFile*>(stream);
            delete file;
            return 0;
        };
        ffunc.zerror_file = [](voidpf, voidpf stream) -> int32 {
            if (stream == nullptr) {
                return 1;
            }
            return 0;
        };

        ffunc.opaque = p_file.release();
    }
    else {
        _writeTime = 0;

        struct MemStream
        {
            const volatile uint8* Buf;
            uint32 Length;
            uint32 Pos;
        };

        ffunc.zopen_file = [](voidpf, const char* filename, int32) -> voidpf {
            if (string(filename) == "@Embedded") {
                static_assert(sizeof(EMBEDDED_RESOURCES) > 100);
                auto default_array = true;

                for (size_t i = 0; i < sizeof(EMBEDDED_RESOURCES) && default_array; i++) {
                    if (EMBEDDED_RESOURCES[i] != numeric_cast<uint8>((i + 42) % 200)) {
                        default_array = false;
                    }
                }

                if (default_array) {
                    throw DataSourceException("Embedded resources not really embed");
                }

                auto* mem_stream = SafeAlloc::MakeRaw<MemStream>();
                mem_stream->Buf = EMBEDDED_RESOURCES + sizeof(uint32);
                mem_stream->Length = *reinterpret_cast<volatile const uint32*>(EMBEDDED_RESOURCES);
                mem_stream->Pos = 0;
                return mem_stream;
            }
            return nullptr;
        };
        ffunc.zread_file = [](voidpf, voidpf stream, void* buf, uLong size) -> uLong {
            auto* mem_stream = static_cast<MemStream*>(stream);
            for (size_t i = 0; i < size; i++) {
                static_cast<uint8*>(buf)[i] = mem_stream->Buf[mem_stream->Pos + i];
            }
            mem_stream->Pos += static_cast<uint32>(size);
            return size;
        };
        ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
        ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
            const auto* mem_stream = static_cast<MemStream*>(stream);
            return numeric_cast<long>(mem_stream->Pos);
        };
        ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int32 origin) -> long {
            auto* mem_stream = static_cast<MemStream*>(stream);
            switch (origin) {
            case ZLIB_FILEFUNC_SEEK_SET:
                mem_stream->Pos = numeric_cast<uint32>(offset);
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                mem_stream->Pos += numeric_cast<uint32>(offset);
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                mem_stream->Pos = mem_stream->Length + numeric_cast<uint32>(offset);
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [](voidpf, voidpf stream) -> int32 {
            const auto* mem_stream = static_cast<MemStream*>(stream);
            delete mem_stream;
            return 0;
        };
        ffunc.zerror_file = [](voidpf, voidpf stream) -> int32 {
            if (stream == nullptr) {
                return 1;
            }
            return 0;
        };
        ffunc.opaque = nullptr;
    }

    _zipHandle = unzOpen2(string(fname).c_str(), &ffunc);

    if (_zipHandle == nullptr) {
        throw DataSourceException("Can't read zip file", fname);
    }

    if (!ReadTree()) {
        throw DataSourceException("Read zip file tree failed", fname);
    }
}

ZipFile::~ZipFile()
{
    FO_STACK_TRACE_ENTRY();

    if (_zipHandle != nullptr) {
        unzClose(_zipHandle);
    }
}

auto ZipFile::ReadTree() -> bool
{
    FO_STACK_TRACE_ENTRY();

    unz_global_info gi;

    if (unzGetGlobalInfo(_zipHandle, &gi) != UNZ_OK || gi.number_entry == 0) {
        return false;
    }

    ZipFileInfo zip_info;
    unz_file_pos pos;
    unz_file_info info;

    for (uLong i = 0; i < gi.number_entry; i++) {
        if (unzGetFilePos(_zipHandle, &pos) != UNZ_OK) {
            return false;
        }

        char buf[4096];

        if (unzGetCurrentFileInfo(_zipHandle, &info, buf, sizeof(buf), nullptr, 0, nullptr, 0) != UNZ_OK) {
            return false;
        }

        if ((info.external_fa & 0x10) == 0) { // Not folder
            string name = strex(buf).normalizePathSlashes();

            zip_info.Pos = pos;
            zip_info.UncompressedSize = numeric_cast<int32>(info.uncompressed_size);
            _filesTree.emplace(name, zip_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        if (i + 1 < gi.number_entry && unzGoToNextFile(_zipHandle) != UNZ_OK) {
            return false;
        }
    }

    return true;
}

auto ZipFile::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto ZipFile::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& info = it->second;
    write_time = _writeTime;
    size = info.UncompressedSize;
    return true;
}

auto ZipFile::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& info = it->second;
    auto pos = info.Pos;

    if (unzGoToFilePos(_zipHandle, &pos) != UNZ_OK) {
        throw DataSourceException("Can't read file from zip (1)", path);
    }
    if (unzOpenCurrentFile(_zipHandle) != UNZ_OK) {
        throw DataSourceException("Can't read file from zip (2)", path);
    }

    auto buf = SafeAlloc::MakeUniqueArr<uint8>(numeric_cast<size_t>(info.UncompressedSize));
    const auto read = unzReadCurrentFile(_zipHandle, buf.get(), info.UncompressedSize);

    if (unzCloseCurrentFile(_zipHandle) != UNZ_OK || read != info.UncompressedSize) {
        throw DataSourceException("Can't read file from zip (3)", path);
    }

    write_time = _writeTime;
    size = info.UncompressedSize;
    return unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) { delete[] p; }};
}

FilesList::FilesList()
{
    FO_STACK_TRACE_ENTRY();

    _filesTree.clear();
    _filesTreeNames.clear();

    auto file_tree = DiskFileSystem::OpenFile("FilesTree.txt", false);

    if (!file_tree) {
        throw DataSourceException("Can't open 'FilesTree.txt' in file list assets");
    }

    const auto len = file_tree.GetSize() + 1;
    string str;
    str.resize(len);

    if (!file_tree.Read(str.data(), len)) {
        throw DataSourceException("Can't read 'FilesTree.txt' in file list assets");
    }

    auto names = strex(str).normalizeLineEndings().split('\n');

    for (auto& name : names) {
        auto file = DiskFileSystem::OpenFile(name, false);

        if (!file) {
            throw DataSourceException("Can't open file in file list assets", name);
        }

        FileEntry fe;
        fe.FileName = name;
        fe.FileSize = file.GetSize();
        fe.WriteTime = DiskFileSystem::GetWriteTime(name);

        _filesTree.emplace(name, std::move(fe));
        _filesTreeNames.emplace_back(std::move(name));
    }
}

auto FilesList::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto FilesList::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;
}

auto FilesList::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = DiskFileSystem::OpenFile(fe.FileName, false);

    if (!file) {
        throw DataSourceException("Can't open file in file list assets", path);
    }

    size = fe.FileSize;
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size);

    if (!file.Read(buf.get(), size)) {
        throw DataSourceException("Can't read file in file list assets", path);
    }

    write_time = fe.WriteTime;
    return unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) { delete[] p; }};
}

auto FilesList::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext);
}

FO_END_NAMESPACE();
