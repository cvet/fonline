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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "DataSource-Dat.h"

#if FO_DATA_SOURCE_DAT

#include "zlib.h"

FO_BEGIN_NAMESPACE

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
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override;

private:
    void ReadTree() FO_TSA_REQUIRES(_datFileLocker);

    mutable mutex _datFileLocker {};
    mutable std::ifstream _datFile FO_TSA_GUARDED_BY(_datFileLocker) {};
    unordered_map<string, ptr<const uint8_t>> _filesTree {};
    vector<string> _filesTreeNames {};
    string _fileName {};
    unique_arr_ptr<uint8_t> _memTree {};
    uint64_t _writeTime {};
    mutable vector<uint8_t> _readBuf FO_TSA_GUARDED_BY(_datFileLocker) {};
};

FalloutDat::FalloutDat(string_view fname)
{
    _fileName = fname;

    scoped_lock locker {_datFileLocker};

    _readBuf.resize(0x40000);
    _datFile = fs::open_ifstream(fname);

    if (!_datFile) {
        throw DataSourceException("Cannot open fallout dat file", fname);
    }

    _writeTime = fs::last_write_time(fname);

    ReadTree();
}

void FalloutDat::ReadTree()
{
    FO_TRACE_ZONE(FileSystem);

    size_t file_size = fs::stream_get_size(_datFile);

    if (file_size < 12) {
        throw DataSourceException("Fallout DAT file is too short to contain a footer", _fileName, file_size);
    }

    uint32_t version = 0;

    if (!fs::stream_set_read_pos(_datFile, -12, std::ios_base::end)) {
        throw DataSourceException("Can't seek to fallout DAT version marker", _fileName, file_size);
    }

    auto version_buf = object_to_bytes(version);

    if (!fs::stream_read_exact(_datFile, version_buf)) {
        throw DataSourceException("Can't read fallout DAT version marker", _fileName, file_size);
    }

    // DAT 2.1 Arcanum
    if (version == 0x44415431) { // 1TAD
        if (!fs::stream_set_read_pos(_datFile, -4, std::ios_base::end)) {
            throw DataSourceException("Can't seek to Arcanum DAT 2.1 tree size", _fileName);
        }

        uint32_t tree_size = 0;

        auto tree_size_buf = object_to_bytes(tree_size);

        if (!fs::stream_read_exact(_datFile, tree_size_buf)) {
            throw DataSourceException("Can't read Arcanum DAT 2.1 tree size", _fileName);
        }

        if (tree_size < 32 || tree_size > file_size || tree_size > numeric_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
            throw DataSourceException("Arcanum DAT 2.1 tree size is invalid", _fileName, tree_size, file_size);
        }

        // Read tree
        if (!fs::stream_set_read_pos(_datFile, -numeric_cast<int32_t>(tree_size), std::ios_base::end)) {
            throw DataSourceException("Can't seek to Arcanum DAT 2.1 tree", _fileName, tree_size);
        }

        uint32_t files_total = 0;

        auto files_total_buf = object_to_bytes(files_total);

        if (!fs::stream_read_exact(_datFile, files_total_buf)) {
            throw DataSourceException("Can't read Arcanum DAT 2.1 file count", _fileName, tree_size);
        }

        tree_size -= 28 + 4; // Subtract information block and files total
        _memTree = safe_alloc::make_unique_arr<uint8_t>(tree_size);
        memory::fill(_memTree, 0, tree_size);
        ptr<uint8_t> tree_data = _memTree.get();

        if (!fs::stream_read_exact(_datFile, make_span(tree_data, tree_size))) {
            throw DataSourceException("Can't read Arcanum DAT 2.1 tree entries", _fileName, tree_size, files_total);
        }

        // Indexing tree
        size_t tree_pos = 0;
        auto tree_span = make_span(tree_data, tree_size);

        while (tree_pos < tree_size) {
            if (tree_size - tree_pos < 24) {
                throw DataSourceException("Arcanum DAT 2.1 tree entry is truncated", _fileName, tree_pos, tree_size, files_total);
            }

            auto tree_entry = make_ptr(tree_span.data()).offset(tree_pos);
            uint32_t fnsz = 0;
            auto fnsz_target = make_ptr(&fnsz).reinterpret_as<uint8_t>();
            ptr<const uint8_t> fnsz_source = tree_entry;
            memory::copy(fnsz_target, fnsz_source, sizeof(fnsz));

            if (fnsz > tree_size - tree_pos - 24) {
                throw DataSourceException("Arcanum DAT 2.1 file name exceeds the tree entry", _fileName, tree_pos, fnsz, tree_size);
            }

            uint32_t type = 0;
            auto type_target = make_ptr(&type).reinterpret_as<uint8_t>();
            auto type_source = tree_entry.offset(4 + fnsz + 4);
            memory::copy(type_target, type_source, sizeof(type));

            // Entry flags: 0x1 plain, 0x2 zlib, 0x400 directory, 0x800 ignored
            if ((type & 0x400) == 0 && (type & 0x800) == 0) {
                string raw_name;
                raw_name.resize(numeric_cast<size_t>(fnsz));
                auto raw_name_target = make_ptr(raw_name.data()).reinterpret_as<uint8_t>();
                auto raw_name_source = tree_entry.offset(4);
                memory::copy(raw_name_target, raw_name_source, raw_name.size());

                // The stored name length includes its C-string terminator
                while (!raw_name.empty() && raw_name.back() == '\0') {
                    raw_name.pop_back();
                }

                if (raw_name.empty() || raw_name.find('\0') != string::npos) {
                    throw DataSourceException("Arcanum DAT 2.1 file name is empty or contains an embedded terminator", _fileName, tree_pos, fnsz);
                }

                string name = strex(raw_name).normalize_path_slashes().lower();

                auto compressed_flag = tree_entry.offset(4 + fnsz + 7);
                *compressed_flag = (type & 0x2) != 0 ? 1 : 0;

                auto file_info = tree_entry.offset(4 + fnsz + 7);
                _filesTree.emplace(name, file_info);
                _filesTreeNames.emplace_back(std::move(name));
            }

            tree_pos += numeric_cast<size_t>(fnsz) + 24;
        }

        return;
    }

    // DAT 2.0 Fallout2
    if (!fs::stream_set_read_pos(_datFile, -8, std::ios_base::end)) {
        throw DataSourceException("Can't seek to fallout DAT 2.0 footer", _fileName);
    }

    uint32_t tree_size = 0;
    auto tree_size_buf = object_to_bytes(tree_size);

    if (!fs::stream_read_exact(_datFile, tree_size_buf)) {
        throw DataSourceException("Can't read fallout DAT 2.0 tree size", _fileName);
    }

    uint32_t dat_size = 0;
    auto dat_size_buf = object_to_bytes(dat_size);

    if (!fs::stream_read_exact(_datFile, dat_size_buf)) {
        throw DataSourceException("Can't read fallout DAT 2.0 file size", _fileName);
    }

    // Check for DAT1.0 Fallout1 dat file
    if (!fs::stream_set_read_pos(_datFile, 0, std::ios_base::beg)) {
        throw DataSourceException("Can't seek to fallout DAT header", _fileName);
    }

    uint32_t dir_count = 0;
    auto dir_count_buf = object_to_bytes(dir_count);

    if (!fs::stream_read_exact(_datFile, dir_count_buf)) {
        throw DataSourceException("Can't read fallout DAT header", _fileName);
    }

    dir_count >>= 24;

    if (dir_count == 0x01 || dir_count == 0x33) {
        throw DataSourceException("Fallout DAT 1.0 is not supported", _fileName);
    }

    // Check for truncated
    if (file_size != dat_size) {
        throw DataSourceException("Fallout DAT 2.0 file size does not match its footer", _fileName, dat_size, file_size);
    }
    if (tree_size < 4 || tree_size > file_size - 8 || tree_size > numeric_cast<uint32_t>(std::numeric_limits<int32_t>::max() - 8)) {
        throw DataSourceException("Fallout DAT 2.0 tree size is invalid", _fileName, tree_size, file_size);
    }

    // Read tree
    if (!fs::stream_set_read_pos(_datFile, -(numeric_cast<int32_t>(tree_size) + 8), std::ios_base::end)) {
        throw DataSourceException("Can't seek to fallout DAT 2.0 tree", _fileName, tree_size);
    }

    uint32_t files_total = 0;
    auto files_total_buf = object_to_bytes(files_total);

    if (!fs::stream_read_exact(_datFile, files_total_buf)) {
        throw DataSourceException("Can't read fallout DAT 2.0 file count", _fileName, tree_size);
    }

    tree_size -= 4;
    _memTree = safe_alloc::make_unique_arr<uint8_t>(tree_size);
    ptr<uint8_t> tree_data = _memTree.get();

    if (!fs::stream_read_exact(_datFile, make_span(tree_data, tree_size))) {
        throw DataSourceException("Can't read fallout DAT 2.0 tree entries", _fileName, tree_size, files_total);
    }

    // Indexing tree
    size_t tree_pos = 0;
    auto tree_span = make_span(tree_data, tree_size);

    while (tree_pos < tree_size) {
        if (tree_size - tree_pos < 17) {
            throw DataSourceException("Fallout DAT 2.0 tree entry is truncated", _fileName, tree_pos, tree_size, files_total);
        }

        auto tree_entry = make_ptr(tree_span.data()).offset(tree_pos);
        uint32_t name_len = 0;
        auto name_len_target = make_ptr(&name_len).reinterpret_as<uint8_t>();
        ptr<const uint8_t> name_len_source = tree_entry;
        memory::copy(name_len_target, name_len_source, sizeof(name_len));

        if (name_len > tree_size - tree_pos - 17) {
            throw DataSourceException("Fallout DAT 2.0 file name exceeds the tree entry", _fileName, tree_pos, name_len, tree_size);
        }

        if (name_len != 0) {
            string raw_name;
            raw_name.resize(numeric_cast<size_t>(name_len));
            auto raw_name_target = make_ptr(raw_name.data()).reinterpret_as<uint8_t>();
            auto raw_name_source = tree_entry.offset(4);
            memory::copy(raw_name_target, raw_name_source, raw_name.size());
            string name = strex(raw_name).normalize_path_slashes();

            auto file_info = tree_entry.offset(4 + name_len);
            _filesTree.emplace(name, file_info);
            _filesTreeNames.emplace_back(std::move(name));
        }

        tree_pos += numeric_cast<size_t>(4) + name_len + 13;
    }
}

auto FalloutDat::IsFileExists(string_view path) const -> bool
{
    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    return true;
}

auto FalloutDat::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return false;
    }

    auto file_info = it->second;
    uint32_t real_size = 0;
    auto real_size_target = make_ptr(&real_size).reinterpret_as<uint8_t>();
    auto real_size_source = file_info.offset(1);
    memory::copy(real_size_target, real_size_source, sizeof(real_size));

    size = real_size;
    write_time = _writeTime;
    return true;
}

auto FalloutDat::GetIndexSnapshot() const -> optional<vector<IndexedFile>>
{
    FO_TRACE_ZONE(FileSystem);

    vector<IndexedFile> files;
    files.reserve(_filesTree.size());

    for (const auto& [path, entry] : _filesTree) {
        size_t size = 0;
        uint64_t write_time = 0;
        bool found = GetFileInfo(path, size, write_time);
        FO_VERIFY_AND_THROW(found, "Dat pack file tree holds an entry its own metadata lookup misses", _fileName, path);

        files.emplace_back(IndexedFile {path, size, write_time});
    }

    return files;
}

auto FalloutDat::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_TRACE_ZONE(FileSystem);

    auto it = _filesTree.find(path);

    if (it == _filesTree.end()) {
        return nullptr;
    }

    scoped_lock locker {_datFileLocker};

    auto file_info = it->second;
    uint8_t type = 0;
    auto type_target = make_ptr(&type).reinterpret_as<uint8_t>();
    auto type_source = file_info;
    memory::copy(type_target, type_source, sizeof(type));

    uint32_t real_size = 0;
    auto real_size_target = make_ptr(&real_size).reinterpret_as<uint8_t>();
    auto real_size_source = file_info.offset(1);
    memory::copy(real_size_target, real_size_source, sizeof(real_size));

    uint32_t packed_size = 0;
    auto packed_size_target = make_ptr(&packed_size).reinterpret_as<uint8_t>();
    auto packed_size_source = file_info.offset(5);
    memory::copy(packed_size_target, packed_size_source, sizeof(packed_size));

    int32_t offset = 0;
    auto offset_target = make_ptr(&offset).reinterpret_as<uint8_t>();
    auto offset_source = file_info.offset(9);
    memory::copy(offset_target, offset_source, sizeof(offset));

    if (!fs::stream_set_read_pos(_datFile, offset, std::ios_base::beg)) {
        throw DataSourceException("Can't seek to file in fallout dat", _fileName, path, offset);
    }

    size = real_size;
    auto buf = safe_alloc::make_unique_arr<uint8_t>(size);
    ptr<uint8_t> buf_data = buf.get();

    if (type == 0) {
        // Plain data
        if (!fs::stream_read_exact(_datFile, make_span(buf_data, size))) {
            throw DataSourceException("Can't read uncompressed file from fallout dat", _fileName, path, offset, real_size);
        }
    }
    else {
        // Packed data
        z_stream stream = {};
        stream.zalloc = [](voidpf, uInt items, uInt size_) -> void* {
            constexpr safe_allocator<uint8_t> allocator;
            return allocator.allocate(numeric_cast<size_t>(items) * size_);
        };
        stream.zfree = [](voidpf, voidpf address) {
            constexpr safe_allocator<uint8_t> allocator;
            allocator.deallocate(cast_from_void<uint8_t*>(address).get(), 0);
        };

        int32_t init_result = inflateInit(&stream);

        if (init_result != Z_OK) {
            throw DataSourceException("Can't initialize fallout dat decompression", _fileName, path, init_result);
        }

        auto end_inflate = scope_exit([&stream]() noexcept { inflateEnd(&stream); });

        stream.next_in = nullptr;
        stream.avail_in = 0;
        stream.next_out = buf_data.get();
        stream.avail_out = real_size;

        uint32_t left = packed_size;
        uint8_t overflow_byte = 0;

        while (true) {
            if (stream.avail_out == 0) {
                stream.next_out = &overflow_byte;
                stream.avail_out = 1;
            }

            if (stream.avail_in == 0 && left > 0) {
                uint32_t len = std::min(left, numeric_cast<uint32_t>(_readBuf.size()));
                auto read_buf = make_nptr(_readBuf.data());
                stream.next_in = read_buf.get();

                if (!fs::stream_read_exact(_datFile, make_span(read_buf, len))) {
                    throw DataSourceException("Can't read compressed file from fallout dat", _fileName, path, offset, packed_size, left, len);
                }

                stream.avail_in = len;
                left -= len;
            }

            int32_t r = inflate(&stream, Z_NO_FLUSH);

            if (r != Z_OK && r != Z_STREAM_END) {
                throw DataSourceException("Can't decompress file from fallout dat", _fileName, path, r, packed_size, real_size, stream.total_in, stream.total_out);
            }
            if (stream.total_out > real_size) {
                throw DataSourceException("Fallout DAT decompressed size exceeds the tree entry", _fileName, path, real_size, stream.total_out);
            }
            if (r == Z_STREAM_END) {
                if (stream.total_out != real_size) {
                    throw DataSourceException("Fallout DAT decompressed size does not match the tree entry", _fileName, path, real_size, stream.total_out);
                }

                break;
            }
        }
    }

    write_time = _writeTime;
    return MakeFileBufferHolder(std::move(buf));
}

auto DataSourceDat::Mount(string_view fname) -> unique_ptr<DataSource>
{
    FO_TRACE_ZONE(FileSystem);

    return safe_alloc::make_unique<FalloutDat>(fname);
}

FO_END_NAMESPACE

#endif
