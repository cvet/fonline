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

#include "DataSource.h"
#include "EmbeddedResources-Include.h"

#include "minizip/unzip.h"

FO_BEGIN_NAMESPACE

static auto GetFileNamesGeneric(const vector<string>& fnames, string_view dir, bool recursive, string_view ext) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    string dir_fixed = strex(dir).normalize_path_slashes();

    if (!dir_fixed.empty() && dir_fixed.back() != '/') {
        dir_fixed += "/";
    }

    vector<string> result;
    const auto len = dir_fixed.length();

    for (const auto& fname : fnames) {
        bool add = false;

        if (fname.compare(0, len, dir_fixed) == 0 && (recursive || (len > 0 && fname.find_last_of('/') < len) || (len == 0 && fname.find_last_of('/') == string::npos))) {
            if (ext.empty() || strex(fname).get_file_extension() == ext) {
                add = true;
            }
        }

        if (add && std::ranges::find(result, fname) == result.end()) {
            result.push_back(fname);
        }
    }

    return result;
}

static void CleanupFileBuffer(ptr<const uint8_t> p) FO_DEFERRED
{
    FO_STACK_TRACE_ENTRY();

    unique_arr_ptr<const uint8_t> owned_buf {p.get()};
    ignore_unused(owned_buf);
}

static auto MakeFileBufferHolder(unique_arr_ptr<uint8_t>&& buf) -> unique_del_ptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    ptr<const uint8_t> released_buf = std::move(buf).release();
    return make_unique_del_ptr(released_buf, CleanupFileBuffer);
}

class DummySpace final : public DataSource
{
public:
    DummySpace() = default;
    DummySpace(const DummySpace&) = delete;
    DummySpace(DummySpace&&) noexcept = delete;
    auto operator=(const DummySpace&) = delete;
    auto operator=(DummySpace&&) noexcept = delete;
    ~DummySpace() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Dummy"; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;
};

class NonCachedDir final : public DataSource
{
public:
    explicit NonCachedDir(string_view fname, bool recursive);
    NonCachedDir(const NonCachedDir&) = delete;
    NonCachedDir(NonCachedDir&&) noexcept = delete;
    auto operator=(const NonCachedDir&) = delete;
    auto operator=(NonCachedDir&&) noexcept = delete;
    ~NonCachedDir() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return true; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _baseDir; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    bool _recursive;
    string _baseDir {};
};

class CachedDir final : public DataSource
{
public:
    CachedDir(string_view fname, bool recursive);
    CachedDir(const CachedDir&) = delete;
    CachedDir(CachedDir&&) noexcept = delete;
    auto operator=(const CachedDir&) = delete;
    auto operator=(CachedDir&&) noexcept = delete;
    ~CachedDir() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return true; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _baseDir; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    struct FileEntry
    {
        string FileName {};
        size_t FileSize {};
        uint64_t WriteTime {};
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
    FalloutDat(FalloutDat&&) noexcept = delete;
    auto operator=(const FalloutDat&) = delete;
    auto operator=(FalloutDat&&) noexcept = delete;
    ~FalloutDat() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext); }

private:
    bool ReadTree() FO_TSA_REQUIRES(_datFileLocker);

    mutable mutex _datFileLocker {};
    mutable std::ifstream _datFile FO_TSA_GUARDED_BY(_datFileLocker) {};
    unordered_map<string, ptr<const uint8_t>> _filesTree {};
    vector<string> _filesTreeNames {};
    string _fileName {};
    unique_arr_ptr<uint8_t> _memTree {};
    uint64_t _writeTime {};
    mutable vector<uint8_t> _readBuf FO_TSA_GUARDED_BY(_datFileLocker) {};
};

class ZipFile final : public DataSource
{
public:
    explicit ZipFile(string_view fname);
    ZipFile(const ZipFile&) = delete;
    ZipFile(ZipFile&&) noexcept = delete;
    auto operator=(const ZipFile&) = delete;
    auto operator=(ZipFile&&) noexcept = delete;
    ~ZipFile() override;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext); }

private:
    struct ZipFileInfo
    {
        unz_file_pos Pos {};
        int32_t UncompressedSize {};
    };

    unordered_map<string, ZipFileInfo> _filesTree {};
    vector<string> _filesTreeNames {};
    string _fileName {};
    mutable mutex _zipHandleLocker {};
    mutable nptr<void> _zipHandle FO_TSA_GUARDED_BY(_zipHandleLocker) {};
    uint64_t _writeTime {};
};

class EmbeddedFile final : public DataSource
{
public:
    explicit EmbeddedFile();
    EmbeddedFile(const EmbeddedFile&) = delete;
    EmbeddedFile(EmbeddedFile&&) noexcept = delete;
    auto operator=(const EmbeddedFile&) = delete;
    auto operator=(EmbeddedFile&&) noexcept = delete;
    ~EmbeddedFile() override;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Embedded"; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext); }

private:
    struct EmbeddedFileInfo
    {
        unz_file_pos Pos {};
        int32_t UncompressedSize {};
    };

    unordered_map<string, EmbeddedFileInfo> _filesTree {};
    vector<string> _filesTreeNames {};
    mutable mutex _zipHandleLocker {};
    mutable nptr<void> _zipHandle FO_TSA_GUARDED_BY(_zipHandleLocker) {};
    uint64_t _writeTime {};
};

class FilesList final : public DataSource
{
public:
    FilesList();
    FilesList(const FilesList&) = delete;
    FilesList(FilesList&&) noexcept = delete;
    auto operator=(const FilesList&) = delete;
    auto operator=(FilesList&&) noexcept = delete;
    ~FilesList() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _packName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    struct FileEntry
    {
        string FileName {};
        size_t FileSize {};
        uint64_t WriteTime {};
    };

    string _packName {"@FilesList"};
    unordered_map<string, FileEntry> _filesTree {};
    vector<string> _filesTreeNames {};
};

auto DataSource::MountDir(string_view dir, bool recursive, bool non_cached, bool maybe_not_available) -> unique_ptr<DataSource>
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_dir(dir)) {
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

    FO_VERIFY_AND_THROW(!name.empty(), "Pack data source mount requested an empty pack name", dir, maybe_not_available);

    const auto is_file_present = [](string_view path) -> bool { return static_cast<bool>(fs_open_ifstream(path)); };

    const string path = strex(dir).combine_path(name);

    if (name == "Embedded") {
        return SafeAlloc::MakeUnique<EmbeddedFile>();
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

auto DummySpace::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(path, size, write_time);
    return false;
}

auto DummySpace::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
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
    _recursive {recursive}
{
    FO_STACK_TRACE_ENTRY();

    _baseDir = fname;
    _baseDir = fs_resolve_path(_baseDir);
    _baseDir += "/";
}

auto NonCachedDir::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_recursive && !strex(path).extract_dir().empty()) {
        return false;
    }

    const string full_path = strex(_baseDir).combine_path(path);

    if (!fs_exists(full_path)) {
        return false;
    }
    if (fs_is_dir(full_path)) {
        return false;
    }

    return true;
}

auto NonCachedDir::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_recursive && !strex(path).extract_dir().empty()) {
        return false;
    }

    const string full_path = strex(_baseDir).combine_path(path);
    auto file = fs_open_ifstream(full_path);

    if (!file) {
        return false;
    }

    size = stream_get_size(file);
    write_time = fs_last_write_time(full_path);
    return true;
}

auto NonCachedDir::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (!_recursive && !strex(path).extract_dir().empty()) {
        return nullptr;
    }

    const string full_path = strex(_baseDir).combine_path(path);
    auto file = fs_open_ifstream(full_path);

    if (!file) {
        return nullptr;
    }

    size = stream_get_size(file);
    unique_arr_ptr<uint8_t> buf = SafeAlloc::MakeUniqueArr<uint8_t>(size);
    ptr<uint8_t> buf_data = buf.get();

    if (!stream_read_exact(file, make_span(buf_data, size))) {
        throw DataSourceException("Can't read file from non cached dir", _baseDir, path);
    }

    write_time = fs_last_write_time(full_path);
    return MakeFileBufferHolder(std::move(buf));
}

auto NonCachedDir::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    if (!_recursive && !dir.empty()) {
        return {};
    }

    vector<string> fnames;

    fs_iterate_dir(strex(_baseDir).combine_path(dir), recursive && _recursive, [&fnames](string_view path2, size_t size, uint64_t write_time) {
        ignore_unused(size, write_time);
        fnames.emplace_back(path2);
    });

    return GetFileNamesGeneric(fnames, dir, recursive, ext);
}

CachedDir::CachedDir(string_view fname, bool recursive)
{
    FO_STACK_TRACE_ENTRY();

    _baseDir = fname;
    _baseDir = fs_resolve_path(_baseDir);
    _baseDir += "/";

    fs_iterate_dir(_baseDir, recursive, [this](string_view path, size_t size, uint64_t write_time) {
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

auto CachedDir::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
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

auto CachedDir::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = fs_open_ifstream(fe.FileName);

    if (!file) {
        throw DataSourceException("Can't read file from cached dir", _baseDir, path);
    }

    size = fe.FileSize;
    unique_arr_ptr<uint8_t> buf = SafeAlloc::MakeUniqueArr<uint8_t>(size);
    ptr<uint8_t> buf_data = buf.get();

    if (!stream_read_exact(file, make_span(buf_data, size))) {
        return nullptr;
    }

    write_time = fe.WriteTime;
    return MakeFileBufferHolder(std::move(buf));
}

auto CachedDir::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext);
}

FalloutDat::FalloutDat(string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    _fileName = fname;

    scoped_lock locker {_datFileLocker};

    _readBuf.resize(0x40000);
    _datFile = fs_open_ifstream(fname);

    if (!_datFile) {
        throw DataSourceException("Cannot open fallout dat file", fname);
    }

    _writeTime = fs_last_write_time(fname);

    if (!ReadTree()) {
        throw DataSourceException("Read fallout dat file tree failed");
    }
}

bool FalloutDat::ReadTree()
{
    FO_STACK_TRACE_ENTRY();

    uint32_t version = 0;

    if (!stream_set_read_pos(_datFile, -12, std::ios_base::end)) {
        return false;
    }
    auto version_buf = make_span(ptr<uint32_t> {&version}, sizeof(version));
    if (!stream_read_exact(_datFile, version_buf)) {
        return false;
    }

    // DAT 2.1 Arcanum
    if (version == 0x44415431) { // 1TAD
        if (!stream_set_read_pos(_datFile, -4, std::ios_base::end)) {
            return false;
        }

        uint32_t tree_size = 0;

        auto tree_size_buf = make_span(ptr<uint32_t> {&tree_size}, sizeof(tree_size));
        if (!stream_read_exact(_datFile, tree_size_buf)) {
            return false;
        }

        // Read tree
        if (!stream_set_read_pos(_datFile, -numeric_cast<int32_t>(tree_size), std::ios_base::end)) {
            return false;
        }

        uint32_t files_total = 0;

        auto files_total_buf = make_span(ptr<uint32_t> {&files_total}, sizeof(files_total));
        if (!stream_read_exact(_datFile, files_total_buf)) {
            return false;
        }

        tree_size -= 28 + 4; // Subtract information block and files total
        _memTree = SafeAlloc::MakeUniqueArr<uint8_t>(tree_size);
        MemFill(_memTree.get(), 0, tree_size);
        ptr<uint8_t> tree_data = _memTree.get();

        if (!stream_read_exact(_datFile, make_span(tree_data, tree_size))) {
            return false;
        }

        // Indexing tree
        size_t tree_pos = 0;
        auto tree_span = make_span(tree_data, tree_size);

        while (tree_pos < tree_size) {
            auto tree_entry = ptr<uint8_t> {tree_span.data()}.offset(tree_pos);
            uint32_t fnsz = 0;
            auto fnsz_target = ptr<uint32_t> {&fnsz}.reinterpret_as<uint8_t>();
            ptr<const uint8_t> fnsz_source = tree_entry;
            MemCopy(fnsz_target.get(), fnsz_source.get(), sizeof(fnsz));

            uint32_t type = 0;
            auto type_target = ptr<uint32_t> {&type}.reinterpret_as<uint8_t>();
            auto type_source = tree_entry.offset(4 + fnsz + 4);
            MemCopy(type_target.get(), type_source.get(), sizeof(type));

            if (fnsz != 0 && type != 0x400) { // Not folder
                string raw_name;
                raw_name.resize(numeric_cast<size_t>(fnsz));
                auto raw_name_target = ptr<char> {raw_name.data()}.reinterpret_as<uint8_t>();
                auto raw_name_source = tree_entry.offset(4);
                MemCopy(raw_name_target.get(), raw_name_source.get(), raw_name.size());
                string name = strex(raw_name).normalize_path_slashes();

                if (type == 2) {
                    auto compressed_flag = tree_entry.offset(4 + fnsz + 7);
                    *compressed_flag = 1; // Compressed
                }

                auto file_info = tree_entry.offset(4 + fnsz + 7);
                _filesTree.emplace(name, file_info);
                _filesTreeNames.emplace_back(std::move(name));
            }

            tree_pos += numeric_cast<size_t>(fnsz) + 24;
        }

        return true;
    }

    // DAT 2.0 Fallout2
    if (!stream_set_read_pos(_datFile, -8, std::ios_base::end)) {
        return false;
    }

    uint32_t tree_size = 0;
    auto tree_size_buf = make_span(ptr<uint32_t> {&tree_size}, sizeof(tree_size));
    if (!stream_read_exact(_datFile, tree_size_buf)) {
        return false;
    }

    uint32_t dat_size = 0;
    auto dat_size_buf = make_span(ptr<uint32_t> {&dat_size}, sizeof(dat_size));
    if (!stream_read_exact(_datFile, dat_size_buf)) {
        return false;
    }

    // Check for DAT1.0 Fallout1 dat file
    if (!stream_set_read_pos(_datFile, 0, std::ios_base::beg)) {
        return false;
    }

    uint32_t dir_count = 0;
    auto dir_count_buf = make_span(ptr<uint32_t> {&dir_count}, sizeof(dir_count));
    if (!stream_read_exact(_datFile, dir_count_buf)) {
        return false;
    }

    dir_count >>= 24;
    if (dir_count == 0x01 || dir_count == 0x33) {
        return false;
    }

    // Check for truncated
    if (stream_get_size(_datFile) != dat_size) {
        return false;
    }

    // Read tree
    if (!stream_set_read_pos(_datFile, -(numeric_cast<int32_t>(tree_size) + 8), std::ios_base::end)) {
        return false;
    }

    uint32_t files_total = 0;
    auto files_total_buf = make_span(ptr<uint32_t> {&files_total}, sizeof(files_total));
    if (!stream_read_exact(_datFile, files_total_buf)) {
        return false;
    }

    tree_size -= 4;
    _memTree = SafeAlloc::MakeUniqueArr<uint8_t>(tree_size);
    ptr<uint8_t> tree_data = _memTree.get();

    if (!stream_read_exact(_datFile, make_span(tree_data, tree_size))) {
        return false;
    }

    // Indexing tree
    size_t tree_pos = 0;
    auto tree_span = make_span(tree_data, tree_size);

    while (tree_pos < tree_size) {
        auto tree_entry = ptr<uint8_t> {tree_span.data()}.offset(tree_pos);
        uint32_t name_len = 0;
        auto name_len_target = ptr<uint32_t> {&name_len}.reinterpret_as<uint8_t>();
        ptr<const uint8_t> name_len_source = tree_entry;
        MemCopy(name_len_target.get(), name_len_source.get(), sizeof(name_len));

        if (tree_pos + 4 + name_len > tree_size) {
            return false;
        }

        if (name_len != 0) {
            string raw_name;
            raw_name.resize(numeric_cast<size_t>(name_len));
            auto raw_name_target = ptr<char> {raw_name.data()}.reinterpret_as<uint8_t>();
            auto raw_name_source = tree_entry.offset(4);
            MemCopy(raw_name_target.get(), raw_name_source.get(), raw_name.size());
            string name = strex(raw_name).normalize_path_slashes();

            auto file_info = tree_entry.offset(4 + name_len);
            _filesTree.emplace(name, file_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        tree_pos += numeric_cast<size_t>(4) + name_len + 13;
    }

    return true;
}

auto FalloutDat::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto FalloutDat::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    ptr<const uint8_t> file_info = it->second;
    uint32_t real_size = 0;
    auto real_size_target = ptr<uint32_t> {&real_size}.reinterpret_as<uint8_t>();
    auto real_size_source = file_info.offset(1);
    MemCopy(real_size_target.get(), real_size_source.get(), sizeof(real_size));

    size = real_size;
    write_time = _writeTime;
    return true;
}

auto FalloutDat::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    scoped_lock locker {_datFileLocker};

    ptr<const uint8_t> file_info = it->second;
    uint8_t type = 0;
    auto type_target = ptr<uint8_t> {&type}.reinterpret_as<uint8_t>();
    ptr<const uint8_t> type_source = file_info;
    MemCopy(type_target.get(), type_source.get(), sizeof(type));

    uint32_t real_size = 0;
    auto real_size_target = ptr<uint32_t> {&real_size}.reinterpret_as<uint8_t>();
    auto real_size_source = file_info.offset(1);
    MemCopy(real_size_target.get(), real_size_source.get(), sizeof(real_size));

    uint32_t packed_size = 0;
    auto packed_size_target = ptr<uint32_t> {&packed_size}.reinterpret_as<uint8_t>();
    auto packed_size_source = file_info.offset(5);
    MemCopy(packed_size_target.get(), packed_size_source.get(), sizeof(packed_size));

    int32_t offset = 0;
    auto offset_target = ptr<int32_t> {&offset}.reinterpret_as<uint8_t>();
    auto offset_source = file_info.offset(9);
    MemCopy(offset_target.get(), offset_source.get(), sizeof(offset));

    if (!stream_set_read_pos(_datFile, offset, std::ios_base::beg)) {
        throw DataSourceException("Can't read file from fallout dat (1)", path);
    }

    size = real_size;
    unique_arr_ptr<uint8_t> buf = SafeAlloc::MakeUniqueArr<uint8_t>(size);
    ptr<uint8_t> buf_data = buf.get();

    if (type == 0) {
        // Plane data
        if (!stream_read_exact(_datFile, make_span(buf_data, size))) {
            throw DataSourceException("Can't read file from fallout dat (2)", path);
        }
    }
    else {
        // Packed data
        z_stream stream = {};
        stream.zalloc = [](voidpf, uInt items, uInt size_) -> void* {
            constexpr SafeAllocator<uint8_t> allocator;
            return allocator.allocate(numeric_cast<size_t>(items) * size_);
        };
        stream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8_t> allocator;
            allocator.deallocate(cast_from_void<uint8_t*>(address), 0);
        };

        if (inflateInit(&stream) != Z_OK) {
            throw DataSourceException("Can't read file from fallout dat (3)", path);
        }

        stream.next_in = nullptr;
        stream.avail_in = 0;
        stream.next_out = buf_data.get();
        stream.avail_out = real_size;

        auto left = packed_size;

        while (stream.avail_out != 0) {
            if (stream.avail_in == 0 && left > 0) {
                const auto len = std::min(left, numeric_cast<uint32_t>(_readBuf.size()));
                ptr<uint8_t> read_buf = _readBuf.data();
                stream.next_in = read_buf.get();

                if (!stream_read_exact(_datFile, make_span(read_buf, len))) {
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
    return MakeFileBufferHolder(std::move(buf));
}

static auto ReturnZipStream(ptr<void> stream) noexcept -> voidpf
{
    FO_NO_STACK_TRACE_ENTRY();

    return stream.get_no_const();
}

static void CleanupZipInputFile(ptr<std::ifstream> file) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_file = adopt_unique_ptr(file);
    ignore_unused(owned_file);
}

struct EmbeddedZipMemStream
{
    span<const volatile uint8_t> Buf;
    uint32_t Pos;
};

static void CleanupEmbeddedZipMemStream(ptr<EmbeddedZipMemStream> mem_stream) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_mem_stream = adopt_unique_ptr(mem_stream);
    ignore_unused(owned_mem_stream);
}

ZipFile::ZipFile(string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    _fileName = fname;

    scoped_lock locker {_zipHandleLocker};

    zlib_filefunc_def ffunc;

    unique_ptr<std::ifstream> p_file = SafeAlloc::MakeUnique<std::ifstream>(fs_open_ifstream(_fileName));

    if (!*p_file) {
        throw DataSourceException("Can't open zip file", _fileName);
    }

    _writeTime = fs_last_write_time(_fileName);

    ffunc.zopen_file = [](voidpf opaque, const char*, int32_t) -> voidpf {
        nptr<void> stream = opaque;
        FO_VERIFY_AND_THROW(stream, "Zip open callback received a null stream handle");
        return ReturnZipStream(stream.as_ptr());
    };
    ffunc.zread_file = [](voidpf, voidpf stream, void* buf, uLong size) -> uLong {
        nptr<std::ifstream> nullable_file = cast_from_void<std::ifstream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_file, "Zip read callback received a null file stream");
        auto file = nullable_file.as_ptr();
        if (size == 0) {
            return 0;
        }

        FO_VERIFY_AND_THROW(buf != nullptr, "Zip read callback received a null output buffer");
        return stream_read_exact(*file, make_span(buf, size)) ? size : 0;
    };
    ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
    ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
        nptr<std::ifstream> nullable_file = cast_from_void<std::ifstream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_file, "Zip tell callback received a null file stream");
        auto file = nullable_file.as_ptr();
        return numeric_cast<long>(stream_get_read_pos(*file));
    };
    ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int32_t origin) -> long {
        nptr<std::ifstream> nullable_file = cast_from_void<std::ifstream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_file, "Zip seek callback received a null file stream");
        auto file = nullable_file.as_ptr();
        switch (origin) {
        case ZLIB_FILEFUNC_SEEK_SET:
            return stream_set_read_pos(*file, numeric_cast<int32_t>(offset), std::ios_base::beg) ? 0 : -1;
        case ZLIB_FILEFUNC_SEEK_CUR:
            return stream_set_read_pos(*file, numeric_cast<int32_t>(offset), std::ios_base::cur) ? 0 : -1;
        case ZLIB_FILEFUNC_SEEK_END:
            return stream_set_read_pos(*file, numeric_cast<int32_t>(offset), std::ios_base::end) ? 0 : -1;
        default:
            return -1;
        }
    };
    ffunc.zclose_file = [](voidpf, voidpf stream) -> int32_t {
        nptr<std::ifstream> nullable_file = cast_from_void<std::ifstream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_file, "Zip close callback received a null file stream");
        auto file = nullable_file.as_ptr();
        CleanupZipInputFile(file);
        return 0;
    };
    ffunc.zerror_file = [](voidpf, voidpf stream) -> int32_t {
        if (!stream) {
            return 1;
        }
        return 0;
    };

    ptr<std::ifstream> released_file = std::move(p_file).release();
    ptr<void> file_stream = cast_to_void(released_file.get());
    ffunc.opaque = file_stream.get();

    const string file_name = string(_fileName);
    ptr<const char> file_name_ptr = file_name.c_str();
    _zipHandle = unzOpen2(file_name_ptr.get(), &ffunc);

    if (!_zipHandle) {
        throw DataSourceException("Can't read zip file", _fileName);
    }

    unz_global_info gi;

    if (unzGetGlobalInfo(_zipHandle.get(), &gi) != UNZ_OK || gi.number_entry == 0) {
        throw DataSourceException("Read zip file tree failed (unzGetGlobalInfo)", _fileName);
    }

    ZipFileInfo zip_info;
    unz_file_pos pos;
    unz_file_info info;
    char name_buf[4096];

    for (uLong i = 0; i < gi.number_entry; i++) {
        if (unzGetFilePos(_zipHandle.get(), &pos) != UNZ_OK) {
            throw DataSourceException("Read zip file tree failed (unzGetFilePos)", _fileName);
        }

        if (unzGetCurrentFileInfo(_zipHandle.get(), &info, name_buf, sizeof(name_buf), nullptr, 0, nullptr, 0) != UNZ_OK) {
            throw DataSourceException("Read zip file tree failed (unzGetCurrentFileInfo)", _fileName);
        }

        if ((info.external_fa & 0x10) == 0) { // Not folder
            string name = strex(name_buf).normalize_path_slashes();

            zip_info.Pos = pos;
            zip_info.UncompressedSize = numeric_cast<int32_t>(info.uncompressed_size);
            _filesTree.emplace(name, zip_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        if (i + 1 < gi.number_entry && unzGoToNextFile(_zipHandle.get()) != UNZ_OK) {
            throw DataSourceException("Read zip file tree failed (unzGoToNextFile)", _fileName);
        }
    }
}

ZipFile::~ZipFile()
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_zipHandleLocker};

    if (_zipHandle) {
        unzClose(_zipHandle.get());
    }
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

auto ZipFile::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
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

auto ZipFile::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    scoped_lock locker {_zipHandleLocker};

    const auto& info = it->second;
    auto pos = info.Pos;

    if (unzGoToFilePos(_zipHandle.get(), &pos) != UNZ_OK) {
        throw DataSourceException("Can't read file from zip (unzGoToFilePos)", path);
    }
    if (unzOpenCurrentFile(_zipHandle.get()) != UNZ_OK) {
        throw DataSourceException("Can't read file from zip (unzOpenCurrentFile)", path);
    }

    unique_arr_ptr<uint8_t> buf = SafeAlloc::MakeUniqueArr<uint8_t>(numeric_cast<size_t>(info.UncompressedSize));
    const auto read = unzReadCurrentFile(_zipHandle.get(), buf.get(), info.UncompressedSize);

    if (unzCloseCurrentFile(_zipHandle.get()) != UNZ_OK || read != info.UncompressedSize) {
        throw DataSourceException("Can't read file from zip (unzCloseCurrentFile)", path);
    }

    write_time = _writeTime;
    size = info.UncompressedSize;
    return MakeFileBufferHolder(std::move(buf));
}

EmbeddedFile::EmbeddedFile()
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_zipHandleLocker};

    zlib_filefunc_def ffunc;

    static_assert(sizeof(EMBEDDED_RESOURCES) > 100);
    bool default_array = true;

    for (size_t i = 0; i < sizeof(EMBEDDED_RESOURCES) && default_array; i++) {
        if (EMBEDDED_RESOURCES[i] != numeric_cast<uint8_t>((i + 42) % 200)) {
            default_array = false;
        }
    }

    if (default_array) {
        WriteLog("Embedded resources not really embed");
        return;
    }

    ffunc.zopen_file = [](voidpf, const char*, int32_t) -> voidpf {
        array<uint8_t, sizeof(uint32_t)> embedded_size_bytes {};

        for (size_t i = 0; i < embedded_size_bytes.size(); i++) {
            embedded_size_bytes[i] = EMBEDDED_RESOURCES[i];
        }

        uint32_t embedded_size = 0;
        ptr<const uint8_t> embedded_size_source = embedded_size_bytes.data();

        auto embedded_size_target = ptr<uint32_t> {&embedded_size}.reinterpret_as<uint8_t>();
        MemCopy(embedded_size_target.get(), embedded_size_source.get(), embedded_size_bytes.size());

        ptr<EmbeddedZipMemStream> mem_stream = SafeAlloc::MakeRaw<EmbeddedZipMemStream>(span<const volatile uint8_t> {EMBEDDED_RESOURCES + sizeof(uint32_t), numeric_cast<size_t>(embedded_size)}, 0);
        ptr<void> stream = cast_to_void(mem_stream.get());
        return ReturnZipStream(stream);
    };
    ffunc.zread_file = [](voidpf, voidpf stream, void* buf, uLong size) -> uLong {
        nptr<EmbeddedZipMemStream> nullable_mem_stream = cast_from_void<EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_mem_stream, "Embedded zip read callback received a null memory stream");
        auto mem_stream = nullable_mem_stream.as_ptr();
        nptr<uint8_t> nullable_out_buf = cast_from_void<uint8_t*>(buf);

        if (size == 0) {
            return 0;
        }

        FO_VERIFY_AND_THROW(!!nullable_out_buf, "Embedded zip read callback received a null output buffer");
        auto out_buf = nullable_out_buf.as_ptr();
        for (size_t i = 0; i < size; i++) {
            out_buf[i] = mem_stream->Buf[mem_stream->Pos + i];
        }
        mem_stream->Pos += numeric_cast<uint32_t>(size);
        return size;
    };
    ffunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong { return 0; };
    ffunc.ztell_file = [](voidpf, voidpf stream) -> long {
        nptr<const EmbeddedZipMemStream> nullable_mem_stream = cast_from_void<EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_mem_stream, "Embedded zip tell callback received a null memory stream");
        auto mem_stream = nullable_mem_stream.as_ptr();
        return numeric_cast<long>(mem_stream->Pos);
    };
    ffunc.zseek_file = [](voidpf, voidpf stream, uLong offset, int32_t origin) -> long {
        nptr<EmbeddedZipMemStream> nullable_mem_stream = cast_from_void<EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_mem_stream, "Embedded zip seek callback received a null memory stream");
        auto mem_stream = nullable_mem_stream.as_ptr();
        switch (origin) {
        case ZLIB_FILEFUNC_SEEK_SET:
            mem_stream->Pos = numeric_cast<uint32_t>(offset);
            break;
        case ZLIB_FILEFUNC_SEEK_CUR:
            mem_stream->Pos += numeric_cast<uint32_t>(offset);
            break;
        case ZLIB_FILEFUNC_SEEK_END:
            mem_stream->Pos = numeric_cast<uint32_t>(mem_stream->Buf.size()) + numeric_cast<uint32_t>(offset);
            break;
        default:
            return -1;
        }
        return 0;
    };
    ffunc.zclose_file = [](voidpf, voidpf stream) -> int32_t {
        nptr<EmbeddedZipMemStream> nullable_mem_stream = cast_from_void<EmbeddedZipMemStream*>(stream);
        FO_VERIFY_AND_THROW(!!nullable_mem_stream, "Embedded zip close callback received a null memory stream");
        auto mem_stream = nullable_mem_stream.as_ptr();
        CleanupEmbeddedZipMemStream(mem_stream);
        return 0;
    };
    ffunc.zerror_file = [](voidpf, voidpf stream) -> int32_t {
        if (!stream) {
            return 1;
        }
        return 0;
    };
    ffunc.opaque = nullptr;

    _zipHandle = unzOpen2("", &ffunc);

    if (!_zipHandle) {
        throw DataSourceException("Can't read embedded file");
    }

    unz_global_info gi;

    if (unzGetGlobalInfo(_zipHandle.get(), &gi) != UNZ_OK || gi.number_entry == 0) {
        throw DataSourceException("Read embedded file tree failed (unzGetGlobalInfo)");
    }

    EmbeddedFileInfo zip_info;
    unz_file_pos pos;
    unz_file_info info;
    char name_buf[4096];

    for (uLong i = 0; i < gi.number_entry; i++) {
        if (unzGetFilePos(_zipHandle.get(), &pos) != UNZ_OK) {
            throw DataSourceException("Read embedded file tree failed (unzGetFilePos)");
        }

        if (unzGetCurrentFileInfo(_zipHandle.get(), &info, name_buf, sizeof(name_buf), nullptr, 0, nullptr, 0) != UNZ_OK) {
            throw DataSourceException("Read embedded file tree failed (unzGetCurrentFileInfo)");
        }

        if ((info.external_fa & 0x10) == 0) { // Not folder
            string name = strex(name_buf).normalize_path_slashes();

            zip_info.Pos = pos;
            zip_info.UncompressedSize = numeric_cast<int32_t>(info.uncompressed_size);
            _filesTree.emplace(name, zip_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        if (i + 1 < gi.number_entry && unzGoToNextFile(_zipHandle.get()) != UNZ_OK) {
            throw DataSourceException("Read embedded file tree failed (unzGoToNextFile)");
        }
    }
}

EmbeddedFile::~EmbeddedFile()
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_zipHandleLocker};

    if (_zipHandle) {
        unzClose(_zipHandle.get());
    }
}

auto EmbeddedFile::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_zipHandleLocker};

    if (!_zipHandle) {
        return false;
    }

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto EmbeddedFile::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_zipHandleLocker};

    if (!_zipHandle) {
        return false;
    }

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    const auto& info = it->second;
    write_time = _writeTime;
    size = info.UncompressedSize;
    return true;
}

auto EmbeddedFile::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_zipHandleLocker};

    if (!_zipHandle) {
        return nullptr;
    }

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& info = it->second;
    auto pos = info.Pos;

    if (unzGoToFilePos(_zipHandle.get(), &pos) != UNZ_OK) {
        throw DataSourceException("Can't read embedded file (unzGoToFilePos)", path);
    }
    if (unzOpenCurrentFile(_zipHandle.get()) != UNZ_OK) {
        throw DataSourceException("Can't read embedded file (unzOpenCurrentFile)", path);
    }

    unique_arr_ptr<uint8_t> buf = SafeAlloc::MakeUniqueArr<uint8_t>(numeric_cast<size_t>(info.UncompressedSize));
    const auto read = unzReadCurrentFile(_zipHandle.get(), buf.get(), info.UncompressedSize);

    if (unzCloseCurrentFile(_zipHandle.get()) != UNZ_OK || read != info.UncompressedSize) {
        throw DataSourceException("Can't read embedded file (unzCloseCurrentFile)", path);
    }

    write_time = _writeTime;
    size = info.UncompressedSize;
    return MakeFileBufferHolder(std::move(buf));
}

FilesList::FilesList()
{
    FO_STACK_TRACE_ENTRY();

    _filesTree.clear();
    _filesTreeNames.clear();

    const auto files_tree_content = fs_read_file("FilesTree.txt");

    if (!files_tree_content) {
        throw DataSourceException("Can't open 'FilesTree.txt' in file list assets");
    }

    ptr<const string> str = &*files_tree_content;

    for (string_view name : strvex(*str).split('\n')) {
        name = strvex(name).trim();

        if (name.empty()) {
            continue;
        }

        auto file = fs_open_ifstream(name);

        if (!file) {
            throw DataSourceException("Can't open file in file list assets", name);
        }

        FileEntry fe;
        fe.FileName = name;
        fe.FileSize = stream_get_size(file);
        fe.WriteTime = fs_last_write_time(name);

        _filesTree.emplace(name, std::move(fe));
        _filesTreeNames.emplace_back(name);
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

auto FilesList::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
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

auto FilesList::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    const auto& fe = it->second;
    auto file = fs_open_ifstream(fe.FileName);

    if (!file) {
        throw DataSourceException("Can't open file in file list assets", path);
    }

    size = fe.FileSize;
    unique_arr_ptr<uint8_t> buf = SafeAlloc::MakeUniqueArr<uint8_t>(size);
    ptr<uint8_t> buf_data = buf.get();

    if (!stream_read_exact(file, make_span(buf_data, size))) {
        throw DataSourceException("Can't read file in file list assets", path);
    }

    write_time = fe.WriteTime;
    return MakeFileBufferHolder(std::move(buf));
}

auto FilesList::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_filesTreeNames, dir, recursive, ext);
}

FO_END_NAMESPACE
