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
#include "DiskFileSystem.h"
#include "EmbeddedResources-Include.h"
#include "Log.h"
#include "StringUtils.h"

#include "minizip/unzip.h"

using FileNameVec = vector<string>;

static auto GetFileNamesGeneric(const FileNameVec& fnames, string_view path, bool include_subdirs, string_view ext) -> vector<string>
{
    STACK_TRACE_ENTRY();

    string path_fixed = strex(path).normalizePathSlashes();
    if (!path_fixed.empty() && path_fixed.back() != '/') {
        path_fixed += "/";
    }

    const auto len = path_fixed.length();
    vector<string> result;

    for (const auto& fname : fnames) {
        auto add = false;
        if (fname.compare(0, len, path_fixed) == 0 && (include_subdirs || (len > 0 && fname.find_last_of('/') < len) || (len == 0 && fname.find_last_of('/') == string::npos))) {
            if (ext.empty() || strex(fname).getFileExtension() == ext) {
                add = true;
            }
        }
        if (add && std::find(result.begin(), result.end(), fname) == result.end()) {
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

    [[nodiscard]] auto IsDiskDir() const -> bool override;
    [[nodiscard]] auto GetPackName() const -> string_view override;
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override;
};

class NonCachedDir final : public DataSource
{
public:
    explicit NonCachedDir(string_view fname);
    NonCachedDir(const NonCachedDir&) = delete;
    NonCachedDir(NonCachedDir&&) noexcept = default;
    auto operator=(const NonCachedDir&) = delete;
    auto operator=(NonCachedDir&&) noexcept = delete;
    ~NonCachedDir() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return true; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _basePath; }
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override;

private:
    string _basePath {};
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
    [[nodiscard]] auto GetPackName() const -> string_view override { return _basePath; }
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override;

private:
    struct FileEntry
    {
        string FileName {};
        size_t FileSize {};
        uint64 WriteTime {};
    };

    unordered_map<string, FileEntry> _filesTree {};
    FileNameVec _filesTreeNames {};
    string _basePath {};
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
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, path, include_subdirs, ext); }

private:
    auto ReadTree() -> bool;

    mutable DiskFile _datFile;
    unordered_map<string, uint8*> _filesTree {};
    FileNameVec _filesTreeNames {};
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
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, path, include_subdirs, ext); }

private:
    struct ZipFileInfo
    {
        unz_file_pos Pos {};
        int UncompressedSize {};
    };

    auto ReadTree() -> bool;

    unordered_map<string, ZipFileInfo> _filesTree {};
    FileNameVec _filesTreeNames {};
    string _fileName {};
    unzFile _zipHandle {};
    uint64 _writeTime {};
};

class AndroidAssets final : public DataSource
{
public:
    AndroidAssets();
    AndroidAssets(const AndroidAssets&) = delete;
    AndroidAssets(AndroidAssets&&) noexcept = default;
    auto operator=(const AndroidAssets&) = delete;
    auto operator=(AndroidAssets&&) noexcept = delete;
    ~AndroidAssets() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _packName; }
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override;

private:
    struct FileEntry
    {
        string FileName {};
        size_t FileSize {};
        uint64 WriteTime {};
    };

    string _packName {"@AndroidAssets"};
    unordered_map<string, FileEntry> _filesTree {};
    FileNameVec _filesTreeNames {};
};

auto DataSource::Create(string_view path, DataSourceType type) -> unique_ptr<DataSource>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!path.empty());

    // Special entries
    if (path.front() == '@') {
        RUNTIME_ASSERT(type == DataSourceType::Default);

        if (path == "@Disabled") {
            return SafeAlloc::MakeUnique<DummySpace>();
        }
        else if (path == "@Embedded") {
            return SafeAlloc::MakeUnique<ZipFile>(path);
        }
        else if (path == "@AndroidAssets") {
            return SafeAlloc::MakeUnique<AndroidAssets>();
        }

        throw DataSourceException("Invalid magic path", path, type);
    }

    // Dir without subdirs
    if (type == DataSourceType::DirRoot || type == DataSourceType::NonCachedDirRoot) {
        if (!DiskFileSystem::IsDir(path)) {
            WriteLog(LogType::Warning, "Directory '{}' not found", path);
        }

        if (type == DataSourceType::DirRoot) {
            return SafeAlloc::MakeUnique<CachedDir>(path, false);
        }
        else {
            return SafeAlloc::MakeUnique<NonCachedDir>(path);
        }
    }

    // Raw view
    if (DiskFileSystem::IsDir(path)) {
        return SafeAlloc::MakeUnique<CachedDir>(path, true);
    }

    // Packed view
    const auto is_file_present = [](string_view file_path) -> bool {
        const auto file = DiskFileSystem::OpenFile(file_path, false);
        return !!file;
    };

    if (is_file_present(path)) {
        const string ext = strex(path).getFileExtension();

        if (ext == "dat") {
            return SafeAlloc::MakeUnique<FalloutDat>(path);
        }
        else if (ext == "zip" || ext == "bos") {
            return SafeAlloc::MakeUnique<ZipFile>(path);
        }

        throw DataSourceException("Unknown file extension", ext, path, type);
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

    if (type == DataSourceType::MaybeNotAvailable) {
        return SafeAlloc::MakeUnique<DummySpace>();
    }
    else {
        throw DataSourceException("Data pack not found", path, type);
    }
}

auto DummySpace::IsDiskDir() const -> bool
{
    STACK_TRACE_ENTRY();

    return false;
}

auto DummySpace::GetPackName() const -> string_view
{
    STACK_TRACE_ENTRY();

    return "Dummy";
}

auto DummySpace::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(write_time);

    return false;
}

auto DummySpace::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(write_time);

    return nullptr;
}

auto DummySpace::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);
    UNUSED_VARIABLE(include_subdirs);
    UNUSED_VARIABLE(ext);

    return {};
}

NonCachedDir::NonCachedDir(string_view fname)
{
    STACK_TRACE_ENTRY();

    _basePath = fname;
    DiskFileSystem::ResolvePath(_basePath);
    _basePath += "/";
}

auto NonCachedDir::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto file = DiskFileSystem::OpenFile(strex("{}{}", _basePath, path), false);

    if (!file) {
        return false;
    }

    size = file.GetSize();
    write_time = file.GetWriteTime();
    return true;
}

auto NonCachedDir::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    STACK_TRACE_ENTRY();

    auto file = DiskFileSystem::OpenFile(strex("{}{}", _basePath, path), false);

    if (!file) {
        return nullptr;
    }

    size = file.GetSize();
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size + 1);

    if (!file.Read(buf.get(), size)) {
        throw DataSourceException("Can't read file from non cached dir", _basePath, path);
    }

    write_time = file.GetWriteTime();
    buf[size] = 0;
    return {buf.release(), [](const auto* p) { delete[] p; }};
}

auto NonCachedDir::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    FileNameVec fnames;
    DiskFileSystem::IterateDir(strex(_basePath).combinePath(path), "", include_subdirs, [&fnames](string_view path2, size_t size, uint64 write_time) {
        UNUSED_VARIABLE(size);
        UNUSED_VARIABLE(write_time);

        fnames.emplace_back(path2);
    });

    return GetFileNamesGeneric(fnames, path, include_subdirs, ext);
}

CachedDir::CachedDir(string_view fname, bool recursive)
{
    STACK_TRACE_ENTRY();

    _basePath = fname;
    DiskFileSystem::ResolvePath(_basePath);
    _basePath += "/";

    DiskFileSystem::IterateDir(_basePath, "", recursive, [this](string_view path, size_t size, uint64 write_time) {
        FileEntry fe;
        fe.FileName = strex("{}{}", _basePath, path);
        fe.FileSize = size;
        fe.WriteTime = write_time;

        _filesTree.emplace(path, std::move(fe));
        _filesTreeNames.emplace_back(path);
    });
}

auto CachedDir::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);

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
    STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = DiskFileSystem::OpenFile(fe.FileName, false);

    if (!file) {
        throw DataSourceException("Can't read file from cached dir", _basePath, path);
    }

    size = fe.FileSize;
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size + 1);

    if (!file.Read(buf.get(), size)) {
        return nullptr;
    }

    buf[size] = 0;
    write_time = fe.WriteTime;
    return {buf.release(), [](const auto* p) { delete[] p; }};
}

auto CachedDir::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_filesTreeNames, path, include_subdirs, ext);
}

FalloutDat::FalloutDat(string_view fname) :
    _datFile {DiskFileSystem::OpenFile(fname, false)}
{
    STACK_TRACE_ENTRY();

    _fileName = fname;
    _readBuf.resize(0x40000);

    if (!_datFile) {
        throw DataSourceException("Cannot open fallout dat file", fname);
    }

    _writeTime = _datFile.GetWriteTime();

    if (!ReadTree()) {
        throw DataSourceException("Read fallout dat file tree failed");
    }
}

auto FalloutDat::ReadTree() -> bool
{
    STACK_TRACE_ENTRY();

    uint version = 0;

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

        uint tree_size = 0;

        if (!_datFile.Read(&tree_size, 4)) {
            return false;
        }

        // Read tree
        if (!_datFile.SetReadPos(-static_cast<int>(tree_size), DiskFileSeek::End)) {
            return false;
        }

        uint files_total = 0;

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
            uint fnsz = 0;
            MemCopy(&fnsz, ptr, sizeof(fnsz));
            uint type = 0;
            MemCopy(&type, ptr + 4 + fnsz + 4, sizeof(type));

            if (fnsz != 0 && type != 0x400) { // Not folder
                string name = strex(string(reinterpret_cast<const char*>(ptr) + 4, fnsz)).normalizePathSlashes();

                if (type == 2) {
                    *(ptr + 4 + fnsz + 7) = 1; // Compressed
                }

                _filesTree.emplace(name, ptr + 4 + fnsz + 7);
                _filesTreeNames.emplace_back(std::move(name));
            }

            ptr += static_cast<size_t>(fnsz) + 24;
        }

        return true;
    }

    // DAT 2.0 Fallout2
    if (!_datFile.SetReadPos(-8, DiskFileSeek::End)) {
        return false;
    }

    uint tree_size = 0;
    if (!_datFile.Read(&tree_size, 4)) {
        return false;
    }

    uint dat_size = 0;
    if (!_datFile.Read(&dat_size, 4)) {
        return false;
    }

    // Check for DAT1.0 Fallout1 dat file
    if (!_datFile.SetReadPos(0, DiskFileSeek::Set)) {
        return false;
    }

    uint dir_count = 0;
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
    if (!_datFile.SetReadPos(-(static_cast<int>(tree_size) + 8), DiskFileSeek::End)) {
        return false;
    }

    uint files_total = 0;
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
        uint name_len = 0;
        MemCopy(&name_len, ptr, 4);

        if (ptr + 4 + name_len > end_ptr) {
            return false;
        }

        if (name_len != 0) {
            string name = strex(string(reinterpret_cast<const char*>(ptr) + 4, name_len)).normalizePathSlashes();

            _filesTree.emplace(name, ptr + 4 + name_len);
            _filesTreeNames.emplace_back(std::move(name));
        }

        ptr += static_cast<size_t>(4) + name_len + 13;
    }

    return true;
}

auto FalloutDat::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);

    if (!_datFile) {
        return false;
    }

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    uint real_size = 0;
    MemCopy(&real_size, it->second + 1, sizeof(real_size));

    size = real_size;
    write_time = _writeTime;
    return true;
}

auto FalloutDat::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto* ptr = it->second;
    uint8 type = 0;
    MemCopy(&type, ptr, sizeof(type));
    uint real_size = 0;
    MemCopy(&real_size, ptr + 1, sizeof(real_size));
    uint packed_size = 0;
    MemCopy(&packed_size, ptr + 5, sizeof(packed_size));
    int offset = 0;
    MemCopy(&offset, ptr + 9, sizeof(offset));

    if (!_datFile.SetReadPos(offset, DiskFileSeek::Set)) {
        throw DataSourceException("Can't read file from fallout dat (1)", path);
    }

    size = real_size;
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size + 1);

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
            return allocator.allocate(static_cast<size_t>(items) * size_);
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
                const auto len = std::min(left, static_cast<uint>(_readBuf.size()));

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
    buf[size] = 0;
    return {buf.release(), [](const auto* p) { delete[] p; }};
}

ZipFile::ZipFile(string_view fname)
{
    STACK_TRACE_ENTRY();

    _fileName = fname;
    _zipHandle = nullptr;

    zlib_filefunc_def ffunc;
    if (fname[0] != '@') {
        auto p_file = SafeAlloc::MakeUnique<DiskFile>(DiskFileSystem::OpenFile(fname, false));

        if (!*p_file) {
            throw DataSourceException("Can't open zip file", fname);
        }

        _writeTime = p_file->GetWriteTime();

        ffunc.zopen_file = [](voidpf opaque, const char*, int) -> voidpf { return opaque; };
        ffunc.zread_file = [](voidpf, voidpf stream, void* buf, uLong size) -> uLong {
            auto* file = static_cast<DiskFile*>(stream);
            return file->Read(buf, size) ? size : 0;
        };
        ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
        ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
            const auto* file = static_cast<DiskFile*>(stream);
            return static_cast<long>(file->GetReadPos());
        };
        ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int origin) -> long {
            auto* file = static_cast<DiskFile*>(stream);
            switch (origin) {
            case ZLIB_FILEFUNC_SEEK_SET:
                file->SetReadPos(static_cast<int>(offset), DiskFileSeek::Set);
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                file->SetReadPos(static_cast<int>(offset), DiskFileSeek::Cur);
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                file->SetReadPos(static_cast<int>(offset), DiskFileSeek::End);
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [](voidpf, voidpf stream) -> int {
            const auto* file = static_cast<DiskFile*>(stream);
            delete file;
            return 0;
        };
        ffunc.zerror_file = [](voidpf, voidpf stream) -> int {
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
            uint Length;
            uint Pos;
        };

        ffunc.zopen_file = [](voidpf, const char* filename, int) -> voidpf {
            if (string(filename) == "@Embedded") {
                static_assert(sizeof(EMBEDDED_RESOURCES) > 100);
                auto default_array = true;

                for (size_t i = 0; i < sizeof(EMBEDDED_RESOURCES) && default_array; i++) {
                    if (EMBEDDED_RESOURCES[i] != static_cast<uint8>((i + 42) % 200)) {
                        default_array = false;
                    }
                }

                if (default_array) {
                    throw DataSourceException("Embedded resources not really embed");
                }

                auto* mem_stream = SafeAlloc::MakeRaw<MemStream>();
                mem_stream->Buf = EMBEDDED_RESOURCES + sizeof(uint);
                mem_stream->Length = *reinterpret_cast<volatile const uint*>(EMBEDDED_RESOURCES);
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
            mem_stream->Pos += static_cast<uint>(size);
            return size;
        };
        ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
        ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
            const auto* mem_stream = static_cast<MemStream*>(stream);
            return static_cast<long>(mem_stream->Pos);
        };
        ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int origin) -> long {
            auto* mem_stream = static_cast<MemStream*>(stream);
            switch (origin) {
            case ZLIB_FILEFUNC_SEEK_SET:
                mem_stream->Pos = static_cast<uint>(offset);
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                mem_stream->Pos += static_cast<uint>(offset);
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                mem_stream->Pos = mem_stream->Length + static_cast<uint>(offset);
                break;
            default:
                return -1;
            }
            return 0;
        };
        ffunc.zclose_file = [](voidpf, voidpf stream) -> int {
            const auto* mem_stream = static_cast<MemStream*>(stream);
            delete mem_stream;
            return 0;
        };
        ffunc.zerror_file = [](voidpf, voidpf stream) -> int {
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
    STACK_TRACE_ENTRY();

    if (_zipHandle != nullptr) {
        unzClose(_zipHandle);
    }
}

auto ZipFile::ReadTree() -> bool
{
    STACK_TRACE_ENTRY();

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
            zip_info.UncompressedSize = static_cast<int>(info.uncompressed_size);
            _filesTree.emplace(name, zip_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        if (i + 1 < gi.number_entry && unzGoToNextFile(_zipHandle) != UNZ_OK) {
            return false;
        }
    }

    return true;
}

auto ZipFile::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);

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
    STACK_TRACE_ENTRY();

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

    auto buf = SafeAlloc::MakeUniqueArr<uint8>(static_cast<size_t>(info.UncompressedSize) + 1);
    const auto read = unzReadCurrentFile(_zipHandle, buf.get(), info.UncompressedSize);

    if (unzCloseCurrentFile(_zipHandle) != UNZ_OK || read != info.UncompressedSize) {
        throw DataSourceException("Can't read file from zip (3)", path);
    }

    write_time = _writeTime;
    size = info.UncompressedSize;
    buf[size] = 0;
    return {buf.release(), [](const auto* p) { delete[] p; }};
}

AndroidAssets::AndroidAssets()
{
    STACK_TRACE_ENTRY();

    _filesTree.clear();
    _filesTreeNames.clear();

    auto file_tree = DiskFileSystem::OpenFile("FilesTree.txt", false);

    if (!file_tree) {
        throw DataSourceException("Can't open 'FilesTree.txt' in android assets");
    }

    const auto len = file_tree.GetSize() + 1;
    string str;
    str.resize(len);

    if (!file_tree.Read(str.data(), len)) {
        throw DataSourceException("Can't read 'FilesTree.txt' in android assets");
    }

    auto names = strex(str).normalizeLineEndings().split('\n');

    for (auto&& name : names) {
        auto file = DiskFileSystem::OpenFile(name, false);

        if (!file) {
            throw DataSourceException("Can't open file in android assets", name);
        }

        FileEntry fe;
        fe.FileName = name;
        fe.FileSize = file.GetSize();
        fe.WriteTime = file.GetWriteTime();

        _filesTree.emplace(name, std::move(fe));
        _filesTreeNames.emplace_back(std::move(name));
    }
}

auto AndroidAssets::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& fe = it->second;
    size = fe.FileSize;
    write_time = fe.WriteTime;
    return true;
}

auto AndroidAssets::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = DiskFileSystem::OpenFile(fe.FileName, false);

    if (!file) {
        throw DataSourceException("Can't open file in android assets", path);
    }

    size = fe.FileSize;
    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size + 1);

    if (!file.Read(buf.get(), size)) {
        throw DataSourceException("Can't read file in android assets", path);
    }

    buf[size] = 0;
    write_time = fe.WriteTime;
    return {buf.release(), [](const auto* p) { delete[] p; }};
}

auto AndroidAssets::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_filesTreeNames, path, include_subdirs, ext);
}
