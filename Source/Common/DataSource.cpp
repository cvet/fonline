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

#include "DataSource.h"
#include "DiskFileSystem.h"
#include "EmbeddedResources-Include.h"
#include "FileSystem.h"
#include "Log.h"
#include "StringUtils.h"

#include "minizip/unzip.h"

using FileNameVec = vector<string>;

static auto GetFileNamesGeneric(const FileNameVec& fnames, string_view path, bool include_subdirs, string_view ext) -> vector<string>
{
    STACK_TRACE_ENTRY();

    string path_fixed = _str(path).normalizePathSlashes();
    if (!path_fixed.empty() && path_fixed.back() != '/') {
        path_fixed += "/";
    }

    const auto len = path_fixed.length();
    vector<string> result;

    for (const auto& fname : fnames) {
        auto add = false;
        if (fname.compare(0, len, path_fixed) == 0 && (include_subdirs || (len > 0 && fname.find_last_of('/') < len) || (len == 0 && fname.find_last_of('/') == string::npos))) {
            if (ext.empty() || _str(fname).getFileExtension() == ext) {
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
    using IndexMap = unordered_map<string, FileEntry>;

    IndexMap _filesTree {};
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
    ~FalloutDat() override;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, path, include_subdirs, ext); }

private:
    using IndexMap = unordered_map<string, uint8*>;

    auto ReadTree() -> bool;

    mutable DiskFile _datFile;
    IndexMap _filesTree {};
    FileNameVec _filesTreeNames {};
    string _fileName {};
    uint8* _memTree {};
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
    using IndexMap = unordered_map<string, ZipFileInfo>;

    auto ReadTree() -> bool;

    IndexMap _filesTree {};
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
    using IndexMap = unordered_map<string, FileEntry>;

    string _packName {"@AndroidAssets"};
    IndexMap _filesTree {};
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
            return std::make_unique<DummySpace>();
        }
        else if (path == "@Embedded") {
            return std::make_unique<ZipFile>(path);
        }
        else if (path == "@AndroidAssets") {
            return std::make_unique<AndroidAssets>();
        }

        throw DataSourceException("Invalid magic path", path, type);
    }

    // Dir without subdirs
    if (type == DataSourceType::DirRoot || type == DataSourceType::NonCachedDirRoot) {
        if (!DiskFileSystem::IsDir(path)) {
            WriteLog(LogType::Warning, "Directory '{}' not found", path);
        }

        if (type == DataSourceType::DirRoot) {
            return std::make_unique<CachedDir>(path, false);
        }
        else {
            return std::make_unique<NonCachedDir>(path);
        }
    }

    // Raw view
    if (DiskFileSystem::IsDir(path)) {
        return std::make_unique<CachedDir>(path, true);
    }

    // Packed view
    const auto is_file_present = [](string_view file_path) -> bool {
        const auto file = DiskFileSystem::OpenFile(file_path, false);
        return !!file;
    };

    if (is_file_present(path)) {
        const string ext = _str(path).getFileExtension();
        if (ext == "dat") {
            return std::make_unique<FalloutDat>(path);
        }
        else if (ext == "zip" || ext == "bos") {
            return std::make_unique<ZipFile>(path);
        }

        throw DataSourceException("Unknown file extension", ext, path, type);
    }
    else if (is_file_present(_str("{}.zip", path))) {
        return std::make_unique<ZipFile>(_str("{}.zip", path));
    }
    else if (is_file_present(_str("{}.bos", path))) {
        return std::make_unique<ZipFile>(_str("{}.bos", path));
    }
    else if (is_file_present(_str("{}.dat", path))) {
        return std::make_unique<FalloutDat>(_str("{}.dat", path));
    }

    if (type == DataSourceType::MaybeNotAvailable) {
        return std::make_unique<DummySpace>();
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

    const auto file = DiskFileSystem::OpenFile(_str("{}{}", _basePath, path), false);
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

    auto file = DiskFileSystem::OpenFile(_str("{}{}", _basePath, path), false);
    if (!file) {
        return nullptr;
    }

    size = file.GetSize();
    auto* buf = new uint8[static_cast<size_t>(size) + 1];
    if (!file.Read(buf, size)) {
        delete[] buf;
        throw DataSourceException("Can't read file from non cached dir", _basePath, path);
    }

    write_time = file.GetWriteTime();
    buf[size] = 0;
    return {buf, [](auto* p) { delete[] p; }};
}

auto NonCachedDir::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    FileNameVec fnames;
    DiskFileSystem::IterateDir(_str(_basePath).combinePath(path), "", include_subdirs, [&fnames](string_view path2, size_t size, uint64 write_time) {
        UNUSED_VARIABLE(size);
        UNUSED_VARIABLE(write_time);

        fnames.push_back(string(path2));
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
        fe.FileName = _str("{}{}", _basePath, path);
        fe.FileSize = size;
        fe.WriteTime = write_time;

        _filesTree.insert(std::make_pair(string(path), fe));
        _filesTreeNames.push_back(string(path));
    });
}

auto CachedDir::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);

    const auto it = _filesTree.find(string(path));
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

    const auto it = _filesTree.find(string(path));
    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = DiskFileSystem::OpenFile(fe.FileName, false);
    if (!file) {
        throw DataSourceException("Can't read file from cached dir", _basePath, path);
    }

    size = fe.FileSize;
    auto* buf = new uint8[size + 1];
    if (!file.Read(buf, size)) {
        delete[] buf;
        return nullptr;
    }

    buf[size] = 0;
    write_time = fe.WriteTime;
    return {buf, [](auto* p) { delete[] p; }};
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

FalloutDat::~FalloutDat()
{
    STACK_TRACE_ENTRY();

    delete[] _memTree;
}

auto FalloutDat::ReadTree() -> bool
{
    STACK_TRACE_ENTRY();

    uint version = 0;
    if (!_datFile.SetPos(-12, DiskFileSeek::End)) {
        return false;
    }
    if (!_datFile.Read(&version, 4)) {
        return false;
    }

    // DAT 2.1 Arcanum
    if (version == 0x44415431) // 1TAD
    {
        if (!_datFile.SetPos(-4, DiskFileSeek::End)) {
            return false;
        }

        uint tree_size = 0;
        if (!_datFile.Read(&tree_size, 4)) {
            return false;
        }

        // Read tree
        if (!_datFile.SetPos(-static_cast<int>(tree_size), DiskFileSeek::End)) {
            return false;
        }

        uint files_total = 0;
        if (!_datFile.Read(&files_total, 4)) {
            return false;
        }

        tree_size -= 28 + 4; // Subtract information block and files total
        _memTree = new uint8[tree_size];
        std::memset(_memTree, 0, tree_size);
        if (!_datFile.Read(_memTree, tree_size)) {
            return false;
        }

        // Indexing tree
        auto* ptr = _memTree;
        const auto* end_ptr = _memTree + tree_size;

        while (ptr < end_ptr) {
            uint fnsz = 0;
            std::memcpy(&fnsz, ptr, sizeof(fnsz));
            uint type = 0;
            std::memcpy(&type, ptr + 4 + fnsz + 4, sizeof(type));

            if (fnsz != 0u && type != 0x400) // Not folder
            {
                string name = _str(string(reinterpret_cast<const char*>(ptr) + 4, fnsz)).normalizePathSlashes();

                if (type == 2) {
                    *(ptr + 4 + fnsz + 7) = 1; // Compressed
                }

                _filesTree.insert(std::make_pair(name, ptr + 4 + fnsz + 7));
                _filesTreeNames.push_back(name);
            }

            ptr += static_cast<size_t>(fnsz) + 24;
        }

        return true;
    }

    // DAT 2.0 Fallout2
    if (!_datFile.SetPos(-8, DiskFileSeek::End)) {
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
    if (!_datFile.SetPos(0, DiskFileSeek::Set)) {
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
    if (!_datFile.SetPos(-(static_cast<int>(tree_size) + 8), DiskFileSeek::End)) {
        return false;
    }

    uint files_total = 0;
    if (!_datFile.Read(&files_total, 4)) {
        return false;
    }

    tree_size -= 4;
    _memTree = new uint8[tree_size];
    if (!_datFile.Read(_memTree, tree_size)) {
        return false;
    }

    // Indexing tree
    auto* ptr = _memTree;
    const auto* end_ptr = _memTree + tree_size;

    while (ptr < end_ptr) {
        uint fnsz = 0;
        std::memcpy(&fnsz, ptr, sizeof(fnsz));

        if (fnsz != 0u) {
            string name = _str(string(reinterpret_cast<const char*>(ptr) + 4, fnsz)).normalizePathSlashes();

            _filesTree.insert(std::make_pair(name, ptr + 4 + fnsz));
            _filesTreeNames.push_back(name);
        }

        ptr += static_cast<size_t>(fnsz) + 17;
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

    const auto it = _filesTree.find(string(path));
    if (it == _filesTree.end()) {
        return false;
    }

    uint real_size = 0;
    std::memcpy(&real_size, it->second + 1, sizeof(real_size));

    size = real_size;
    write_time = _writeTime;
    return true;
}

auto FalloutDat::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(string(path));
    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto* ptr = it->second;
    uint8 type = 0;
    std::memcpy(&type, ptr, sizeof(type));
    uint real_size = 0;
    std::memcpy(&real_size, ptr + 1, sizeof(real_size));
    uint packed_size = 0;
    std::memcpy(&packed_size, ptr + 5, sizeof(packed_size));
    int offset = 0;
    std::memcpy(&offset, ptr + 9, sizeof(offset));

    if (!_datFile.SetPos(offset, DiskFileSeek::Set)) {
        throw DataSourceException("Can't read file from fallout dat (1)", path);
    }

    size = real_size;
    auto* buf = new uint8[static_cast<size_t>(size) + 1];

    if (type == 0u) {
        // Plane data
        if (!_datFile.Read(buf, size)) {
            delete[] buf;
            throw DataSourceException("Can't read file from fallout dat (2)", path);
        }
    }
    else {
        // Packed data
        z_stream stream;
        stream.zalloc = nullptr;
        stream.zfree = nullptr;
        stream.opaque = nullptr;
        stream.next_in = nullptr;
        stream.avail_in = 0;
        if (inflateInit(&stream) != Z_OK) {
            delete[] buf;
            throw DataSourceException("Can't read file from fallout dat (3)", path);
        }

        stream.next_out = buf;
        stream.avail_out = real_size;

        auto left = packed_size;
        while (stream.avail_out != 0u) {
            if (stream.avail_in == 0u && left > 0) {
                stream.next_in = _readBuf.data();
                const auto len = std::min(left, static_cast<uint>(_readBuf.size()));
                if (!_datFile.Read(_readBuf.data(), len)) {
                    delete[] buf;
                    throw DataSourceException("Can't read file from fallout dat (4)", path);
                }
                stream.avail_in = len;
                left -= len;
            }

            const auto r = inflate(&stream, Z_NO_FLUSH);
            if (r != Z_OK && r != Z_STREAM_END) {
                delete[] buf;
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
    return {buf, [](auto* p) { delete[] p; }};
}

ZipFile::ZipFile(string_view fname)
{
    STACK_TRACE_ENTRY();

    _fileName = fname;
    _zipHandle = nullptr;

    zlib_filefunc_def ffunc;
    if (fname[0] != '@') {
        auto* p_file = new DiskFile {DiskFileSystem::OpenFile(fname, false)};
        if (!*p_file) {
            delete p_file;
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
            return static_cast<long>(file->GetPos());
        };
        ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int origin) -> long {
            auto* file = static_cast<DiskFile*>(stream);
            switch (origin) {
            case ZLIB_FILEFUNC_SEEK_SET:
                file->SetPos(static_cast<int>(offset), DiskFileSeek::Set);
                break;
            case ZLIB_FILEFUNC_SEEK_CUR:
                file->SetPos(static_cast<int>(offset), DiskFileSeek::Cur);
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                file->SetPos(static_cast<int>(offset), DiskFileSeek::End);
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
        ffunc.opaque = p_file;
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

                auto* mem_stream = new MemStream();
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

        if ((info.external_fa & 0x10) == 0u) // Not folder
        {
            string name = _str(buf).normalizePathSlashes();

            zip_info.Pos = pos;
            zip_info.UncompressedSize = static_cast<int>(info.uncompressed_size);
            _filesTree.insert(std::make_pair(name, zip_info));
            _filesTreeNames.push_back(name);
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

    const auto it = _filesTree.find(string(path));
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

    const auto it = _filesTree.find(string(path));
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

    auto* buf = new uint8[static_cast<size_t>(info.UncompressedSize) + 1];
    const auto read = unzReadCurrentFile(_zipHandle, buf, info.UncompressedSize);
    if (unzCloseCurrentFile(_zipHandle) != UNZ_OK || read != info.UncompressedSize) {
        delete[] buf;
        throw DataSourceException("Can't read file from zip (3)", path);
    }

    write_time = _writeTime;
    size = info.UncompressedSize;
    buf[size] = 0;
    return {buf, [](auto* p) { delete[] p; }};
}

AndroidAssets::AndroidAssets()
{
    STACK_TRACE_ENTRY();

    _filesTree.clear();
    _filesTreeNames.clear();

    // Read tree
    auto file_tree = DiskFileSystem::OpenFile("FilesTree.txt", false);
    if (!file_tree) {
        throw DataSourceException("Can't open 'FilesTree.txt' in android assets");
    }

    const auto len = file_tree.GetSize() + 1;
    auto* buf = new char[len];
    buf[len - 1] = 0;

    if (!file_tree.Read(buf, len)) {
        delete[] buf;
        throw DataSourceException("Can't read 'FilesTree.txt' in android assets");
    }

    const auto names = _str(buf).normalizeLineEndings().split('\n');
    delete[] buf;

    // Parse
    for (const auto& name : names) {
        auto file = DiskFileSystem::OpenFile(name, false);
        if (!file) {
            throw DataSourceException("Can't open file in android assets", name);
        }

        FileEntry fe;
        fe.FileName = name;
        fe.FileSize = file.GetSize();
        fe.WriteTime = file.GetWriteTime();

        _filesTree.insert(std::make_pair(name, fe));
        _filesTreeNames.push_back(name);
    }
}

auto AndroidAssets::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);

    const auto it = _filesTree.find(string(path));
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

    const auto it = _filesTree.find(string(path));
    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = DiskFileSystem::OpenFile(fe.FileName, false);
    if (!file) {
        throw DataSourceException("Can't open file in android assets", path);
    }

    size = fe.FileSize;
    auto* buf = new uint8[size + 1];
    if (!file.Read(buf, size)) {
        delete[] buf;
        throw DataSourceException("Can't read file in android assets", path);
    }

    buf[size] = 0;
    write_time = fe.WriteTime;
    return {buf, [](auto* p) { delete[] p; }};
}

auto AndroidAssets::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_filesTreeNames, path, include_subdirs, ext);
}
