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

#include "ResourceIndex.h"

FO_BEGIN_NAMESPACE

// Header layout, little endian throughout. The checksum covers everything before it, so a header that survived
// a truncated write is rejected before any offset it carries is believed
static constexpr size_t HEADER_OFFSET_MAGIC = 0;
static constexpr size_t HEADER_OFFSET_VERSION_MAJOR = 4;
static constexpr size_t HEADER_OFFSET_VERSION_MINOR = 6;
static constexpr size_t HEADER_OFFSET_PACK_LIST_HASH = 8;
static constexpr size_t HEADER_OFFSET_INDEX_OFFSET = 16;
static constexpr size_t HEADER_OFFSET_INDEX_STORED_SIZE = 24;
static constexpr size_t HEADER_OFFSET_INDEX_DECODED_SIZE = 32;
static constexpr size_t HEADER_OFFSET_INDEX_CODEC = 40;
static constexpr size_t HEADER_OFFSET_ENTRY_COUNT = 44;
static constexpr size_t HEADER_OFFSET_PACK_COUNT = 48;
static constexpr size_t HEADER_OFFSET_CHECKSUM = 64;

// Pack record layout inside the decoded index
static constexpr size_t PACK_OFFSET_NAME_OFFSET = 0;
static constexpr size_t PACK_OFFSET_NAME_LENGTH = 4;
static constexpr size_t PACK_OFFSET_PACK_HASH = 8;

// Entry layout inside the decoded index
static constexpr size_t ENTRY_OFFSET_PATH_OFFSET = 0;
static constexpr size_t ENTRY_OFFSET_PATH_LENGTH = 4;
static constexpr size_t ENTRY_OFFSET_PACK_INDEX = 8;
static constexpr size_t ENTRY_OFFSET_CODEC = 12;
static constexpr size_t ENTRY_OFFSET_DATA_OFFSET = 16;
static constexpr size_t ENTRY_OFFSET_STORED_SIZE = 24;
static constexpr size_t ENTRY_OFFSET_DECODED_SIZE = 32;

// The offsets above are the format. These pin the record sizes to them, so widening a field without widening
// the record it sits in stops compiling instead of writing a file nothing can read
static_assert(HEADER_OFFSET_CHECKSUM + sizeof(uint64_t) == RESOURCE_INDEX_HEADER_SIZE);
static_assert(PACK_OFFSET_PACK_HASH + sizeof(uint64_t) == RESOURCE_INDEX_PACK_SIZE);
static_assert(ENTRY_OFFSET_DECODED_SIZE + sizeof(uint64_t) == RESOURCE_INDEX_ENTRY_SIZE);
static_assert(RESOURCE_INDEX_MAGIC == (uint32_t {'F'} | uint32_t {'O'} << 8 | uint32_t {'I'} << 16 | uint32_t {'X'} << 24));

auto IsResourceIndexCurrent(string_view path, const vector<string>& pack_dirs, const vector<string>& pack_names) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ResourceIndexHeader header;

    if (!ReadResourceIndexHeader(path, header)) {
        return false;
    }

    vector<ResourceIndexPack> packs;
    vector<string> pack_paths;

    if (!ResolveResourceIndexPacks(pack_dirs, pack_names, packs, pack_paths)) {
        return false;
    }

    return header.PackCount == packs.size() && ComputeResourceIndexPackListHash(packs) == header.PackListHash;
}

// What the merge keeps per path while it folds the packs in order
struct ResourceIndexEntryRecord
{
    uint32_t PackIndex {};
    uint32_t Codec {};
    uint64_t DataOffset {};
    uint64_t StoredSize {};
    uint64_t DecodedSize {};
};

static void BuildHeaderBytes(const ResourceIndexHeader& header, span<uint8_t> buf) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    std::fill(buf.begin(), buf.end(), uint8_t {0});

    span_write_uint32(buf, HEADER_OFFSET_MAGIC, RESOURCE_INDEX_MAGIC);
    span_write_uint16(buf, HEADER_OFFSET_VERSION_MAJOR, header.VersionMajor);
    span_write_uint16(buf, HEADER_OFFSET_VERSION_MINOR, header.VersionMinor);
    span_write_uint64(buf, HEADER_OFFSET_PACK_LIST_HASH, header.PackListHash);
    span_write_uint64(buf, HEADER_OFFSET_INDEX_OFFSET, header.IndexOffset);
    span_write_uint64(buf, HEADER_OFFSET_INDEX_STORED_SIZE, header.IndexStoredSize);
    span_write_uint64(buf, HEADER_OFFSET_INDEX_DECODED_SIZE, header.IndexDecodedSize);
    span_write_uint32(buf, HEADER_OFFSET_INDEX_CODEC, header.IndexCodec);
    span_write_uint32(buf, HEADER_OFFSET_ENTRY_COUNT, header.EntryCount);
    span_write_uint32(buf, HEADER_OFFSET_PACK_COUNT, header.PackCount);
    span_write_uint64(buf, HEADER_OFFSET_CHECKSUM, HashResourceBytes(RESOURCE_PACK_HASH_SEED, {buf.data(), HEADER_OFFSET_CHECKSUM}));
}

static auto ParseHeaderBytes(const_span<uint8_t> buf, ResourceIndexHeader& header) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (span_read_uint32(buf, HEADER_OFFSET_MAGIC) != RESOURCE_INDEX_MAGIC) {
        return false;
    }

    if (span_read_uint64(buf, HEADER_OFFSET_CHECKSUM) != HashResourceBytes(RESOURCE_PACK_HASH_SEED, {buf.data(), HEADER_OFFSET_CHECKSUM})) {
        return false;
    }

    header.VersionMajor = span_read_uint16(buf, HEADER_OFFSET_VERSION_MAJOR);
    header.VersionMinor = span_read_uint16(buf, HEADER_OFFSET_VERSION_MINOR);

    if (header.VersionMajor != RESOURCE_INDEX_VERSION_MAJOR) {
        return false;
    }

    header.PackListHash = span_read_uint64(buf, HEADER_OFFSET_PACK_LIST_HASH);
    header.IndexOffset = span_read_uint64(buf, HEADER_OFFSET_INDEX_OFFSET);
    header.IndexStoredSize = span_read_uint64(buf, HEADER_OFFSET_INDEX_STORED_SIZE);
    header.IndexDecodedSize = span_read_uint64(buf, HEADER_OFFSET_INDEX_DECODED_SIZE);
    header.IndexCodec = span_read_uint32(buf, HEADER_OFFSET_INDEX_CODEC);
    header.EntryCount = span_read_uint32(buf, HEADER_OFFSET_ENTRY_COUNT);
    header.PackCount = span_read_uint32(buf, HEADER_OFFSET_PACK_COUNT);

    return true;
}

auto ComputeResourceIndexPackListHash(const vector<ResourceIndexPack>& packs) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = RESOURCE_PACK_HASH_SEED;

    for (const auto& pack : packs) {
        hash = HashResourceBytes(hash, {reinterpret_cast<const uint8_t*>(pack.Name.data()), pack.Name.size()});

        array<uint8_t, 8> pack_hash_bytes = {};
        span_write_uint64(pack_hash_bytes, 0, pack.PackHash);
        hash = HashResourceBytes(hash, pack_hash_bytes);
    }

    return hash;
}

auto ReadResourceIndexHeader(string_view path, ResourceIndexHeader& header) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    disk_read_file file {path};

    if (!file) {
        return false;
    }

    array<uint8_t, RESOURCE_INDEX_HEADER_SIZE> buf = {};

    if (!file.read_at(0, buf)) {
        return false;
    }

    return ParseHeaderBytes(buf, header);
}

auto ResolveResourceIndexPacks(const vector<string>& pack_dirs, const vector<string>& pack_names, vector<ResourceIndexPack>& packs, vector<string>& pack_paths) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    packs.clear();
    pack_paths.clear();

    for (const auto& name : pack_names) {
        string resolved_path;
        uint64_t resolved_hash = 0;

        // Later directories win, so a downloaded pack overrides the shipped one, and the hash comes from the
        // read that resolved rather than from a later failed one that may have half-filled the header
        for (const auto& dir : pack_dirs) {
            string candidate = strex(dir).combine_path(strex("{}.fores", name)).str();
            ResourcePackHeader pack_header;

            if (ReadResourcePackHeader(candidate, pack_header)) {
                resolved_path = candidate;
                resolved_hash = pack_header.PackHash;
            }
        }

        if (resolved_path.empty()) {
            return false;
        }

        packs.emplace_back(ResourceIndexPack {name, resolved_hash});
        pack_paths.emplace_back(std::move(resolved_path));
    }

    return true;
}

void BuildResourceIndex(string_view path, const vector<string>& pack_paths, const vector<ResourceIndexPack>& packs, ResourcePackWriteSettings settings)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pack_paths.size() == packs.size(), "Resource index pack paths and pack records disagree", pack_paths.size(), packs.size());

    // One entry per path, folded in the given order with the last pack winning, which is the same precedence
    // the per-pack mounts have today
    unordered_map<string, ResourceIndexEntryRecord> merged;

    for (size_t pack_index = 0; pack_index < pack_paths.size(); ++pack_index) {
        ResourcePackSource pack {pack_paths[pack_index]};

        for (auto& entry : pack.GetEntryRefs()) {
            ResourceIndexEntryRecord record {numeric_cast<uint32_t>(pack_index), entry.Codec, entry.DataOffset, entry.StoredSize, entry.DecodedSize};
            merged.insert_or_assign(std::move(entry.Path), record);
        }
    }

    vector<string> sorted_paths;
    sorted_paths.reserve(merged.size());

    for (const auto& [entry_path, record] : merged) {
        sorted_paths.emplace_back(entry_path);
    }

    std::sort(sorted_paths.begin(), sorted_paths.end());

    size_t pack_table_size = packs.size() * RESOURCE_INDEX_PACK_SIZE;
    size_t entry_table_size = sorted_paths.size() * RESOURCE_INDEX_ENTRY_SIZE;
    size_t pool_size = 0;

    for (const auto& pack : packs) {
        pool_size += pack.Name.size();
    }

    for (const auto& entry_path : sorted_paths) {
        pool_size += entry_path.size();
    }

    vector<uint8_t> index(pack_table_size + entry_table_size + pool_size);
    size_t pool_offset = pack_table_size + entry_table_size;

    for (size_t i = 0; i < packs.size(); ++i) {
        size_t record_offset = i * RESOURCE_INDEX_PACK_SIZE;
        span_write_uint32(index, record_offset + PACK_OFFSET_NAME_OFFSET, numeric_cast<uint32_t>(pool_offset));
        span_write_uint32(index, record_offset + PACK_OFFSET_NAME_LENGTH, numeric_cast<uint32_t>(packs[i].Name.size()));
        span_write_uint64(index, record_offset + PACK_OFFSET_PACK_HASH, packs[i].PackHash);
        std::copy(packs[i].Name.begin(), packs[i].Name.end(), index.begin() + numeric_cast<ptrdiff_t>(pool_offset));
        pool_offset += packs[i].Name.size();
    }

    for (size_t i = 0; i < sorted_paths.size(); ++i) {
        const auto& entry_path = sorted_paths[i];
        const ResourceIndexEntryRecord& record = merged[entry_path];
        size_t entry_offset = pack_table_size + i * RESOURCE_INDEX_ENTRY_SIZE;

        span_write_uint32(index, entry_offset + ENTRY_OFFSET_PATH_OFFSET, numeric_cast<uint32_t>(pool_offset));
        span_write_uint32(index, entry_offset + ENTRY_OFFSET_PATH_LENGTH, numeric_cast<uint32_t>(entry_path.size()));
        span_write_uint32(index, entry_offset + ENTRY_OFFSET_PACK_INDEX, record.PackIndex);
        span_write_uint32(index, entry_offset + ENTRY_OFFSET_CODEC, record.Codec);
        span_write_uint64(index, entry_offset + ENTRY_OFFSET_DATA_OFFSET, record.DataOffset);
        span_write_uint64(index, entry_offset + ENTRY_OFFSET_STORED_SIZE, record.StoredSize);
        span_write_uint64(index, entry_offset + ENTRY_OFFSET_DECODED_SIZE, record.DecodedSize);
        std::copy(entry_path.begin(), entry_path.end(), index.begin() + numeric_cast<ptrdiff_t>(pool_offset));
        pool_offset += entry_path.size();
    }

    uint32_t index_codec = 0;
    vector<uint8_t> stored_index = EncodeResourceBlob(index, settings, index_codec);

    ResourceIndexHeader header;
    header.VersionMajor = RESOURCE_INDEX_VERSION_MAJOR;
    header.VersionMinor = RESOURCE_INDEX_VERSION_MINOR;
    header.PackListHash = ComputeResourceIndexPackListHash(packs);
    header.IndexOffset = RESOURCE_INDEX_HEADER_SIZE;
    header.IndexStoredSize = stored_index.size();
    header.IndexDecodedSize = index.size();
    header.IndexCodec = index_codec;
    header.EntryCount = numeric_cast<uint32_t>(sorted_paths.size());
    header.PackCount = numeric_cast<uint32_t>(packs.size());

    array<uint8_t, RESOURCE_INDEX_HEADER_SIZE> header_bytes = {};
    BuildHeaderBytes(header, header_bytes);

    // Written beside the target and renamed over it, so a reader never meets a half-written tree and an
    // interrupted rebuild leaves the previous index in place
    string temp_path = strex("{}.tmp", path).str();
    {
        disk_write_file file {temp_path};
        FO_VERIFY_AND_THROW(!!file, "Can't create resource index file", temp_path);
        bool header_written = file.write(header_bytes);
        FO_VERIFY_AND_THROW(header_written, "Can't write resource index header", temp_path);

        bool index_written = file.write(stored_index);
        FO_VERIFY_AND_THROW(index_written, "Can't write resource index", temp_path);

        bool flushed = file.flush();
        FO_VERIFY_AND_THROW(flushed, "Can't finish resource index file", temp_path);
        file.close();
    }

    (void)fs_remove_file(path);
    bool renamed = fs_rename(temp_path, path);
    FO_VERIFY_AND_THROW(renamed, "Can't put the resource index in place", temp_path, path);
}

ResourceIndexSource::ResourceIndexSource(string_view path, const vector<string>& pack_dirs) :
    _fileName {path},
    _indexName {strex(path).extract_file_name().erase_file_extension()},
    _file {path}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!!_file, "Can't open resource index", _fileName);

    array<uint8_t, RESOURCE_INDEX_HEADER_SIZE> header_bytes = {};
    bool header_read = _file.read_at(0, header_bytes);
    FO_VERIFY_AND_THROW(header_read, "Can't read resource index header", _fileName);

    bool header_valid = ParseHeaderBytes(header_bytes, _header);
    FO_VERIFY_AND_THROW(header_valid, "Resource index header is not valid", _fileName);

    ParseIndex(pack_dirs);

    _writeTime = fs_last_write_time(_fileName);
}

void ResourceIndexSource::ParseIndex(const vector<string>& pack_dirs)
{
    FO_STACK_TRACE_ENTRY();

    optional<uint64_t> file_size = fs_file_size(_fileName);
    FO_VERIFY_AND_THROW(file_size.has_value(), "Can't size the resource index", _fileName);

    auto fits_in_file = [&](uint64_t offset, uint64_t size) { return offset >= RESOURCE_INDEX_HEADER_SIZE && size <= *file_size && offset <= *file_size - size; };
    FO_VERIFY_AND_THROW(fits_in_file(_header.IndexOffset, _header.IndexStoredSize), "Resource index extent is outside the file", _fileName);

    size_t pack_table_size = numeric_cast<size_t>(_header.PackCount) * RESOURCE_INDEX_PACK_SIZE;
    size_t entry_table_size = numeric_cast<size_t>(_header.EntryCount) * RESOURCE_INDEX_ENTRY_SIZE;
    FO_VERIFY_AND_THROW(_header.IndexDecodedSize >= pack_table_size + entry_table_size, "Resource index is too small for what it declares", _fileName);

    vector<uint8_t> stored_index(numeric_cast<size_t>(_header.IndexStoredSize));
    bool index_read = _file.read_at(_header.IndexOffset, stored_index);
    FO_VERIFY_AND_THROW(index_read, "Can't read the resource index", _fileName);

    if (_header.IndexCodec == static_cast<uint32_t>(ResourcePackCodec::Deflate)) {
        _index = Compressor::DecompressExact(stored_index, numeric_cast<size_t>(_header.IndexDecodedSize));
    }
    else {
        FO_VERIFY_AND_THROW(_header.IndexStoredSize == _header.IndexDecodedSize, "Stored resource index declares two sizes", _fileName);
        _index = std::move(stored_index);
    }

    FO_VERIFY_AND_THROW(_index.size() == _header.IndexDecodedSize, "Resource index did not decode to its declared size", _fileName);

    // The packs come first: every entry names one by position, so they must all resolve before any entry does
    _packFiles.reserve(_header.PackCount);
    _fileNames.reserve(_header.EntryCount);
    vector<ResourceIndexPack> packs;
    packs.reserve(_header.PackCount);

    for (uint32_t i = 0; i < _header.PackCount; ++i) {
        size_t record_offset = numeric_cast<size_t>(i) * RESOURCE_INDEX_PACK_SIZE;
        uint32_t name_offset = span_read_uint32(_index, record_offset + PACK_OFFSET_NAME_OFFSET);
        uint32_t name_length = span_read_uint32(_index, record_offset + PACK_OFFSET_NAME_LENGTH);
        FO_VERIFY_AND_THROW(name_length != 0 && name_offset >= pack_table_size + entry_table_size, "Resource index pack name is outside the pool", _fileName);
        FO_VERIFY_AND_THROW(name_length <= _index.size() - name_offset, "Resource index pack name runs past the pool", _fileName);

        string pack_name {reinterpret_cast<const char*>(_index.data()) + name_offset, name_length};
        uint64_t pack_hash = span_read_uint64(_index, record_offset + PACK_OFFSET_PACK_HASH);

        // Resolved the same way the build resolved it, and refused when the file on disk is not the one the
        // tree was merged from - a stale index must be rebuilt, never read through
        string resolved_path;

        for (const auto& dir : pack_dirs) {
            string candidate = strex(dir).combine_path(strex("{}.fores", pack_name)).str();
            ResourcePackHeader pack_header;

            if (ReadResourcePackHeader(candidate, pack_header) && pack_header.PackHash == pack_hash) {
                resolved_path = candidate;
            }
        }

        FO_VERIFY_AND_THROW(!resolved_path.empty(), "Resource index names a pack that is not on disk with that hash", _fileName, pack_name);

        disk_read_file pack_file {resolved_path};
        FO_VERIFY_AND_THROW(!!pack_file, "Can't open a pack the resource index names", resolved_path);
        _packFiles.emplace_back(std::move(pack_file));
        packs.emplace_back(ResourceIndexPack {std::move(pack_name), pack_hash});
    }

    uint64_t resolved_list_hash = ComputeResourceIndexPackListHash(packs);
    FO_VERIFY_AND_THROW(resolved_list_hash == _header.PackListHash, "Resource index pack list disagrees with its own key", _fileName);

    _entries.reserve(_header.EntryCount);
    _entryLookup.reserve(_header.EntryCount);

    for (uint32_t i = 0; i < _header.EntryCount; ++i) {
        size_t entry_offset = pack_table_size + numeric_cast<size_t>(i) * RESOURCE_INDEX_ENTRY_SIZE;

        FileEntry entry;
        uint32_t path_offset = span_read_uint32(_index, entry_offset + ENTRY_OFFSET_PATH_OFFSET);
        uint32_t path_length = span_read_uint32(_index, entry_offset + ENTRY_OFFSET_PATH_LENGTH);
        FO_VERIFY_AND_THROW(path_length != 0 && path_offset >= pack_table_size + entry_table_size, "Resource index path is outside the pool", _fileName);
        FO_VERIFY_AND_THROW(path_length <= _index.size() - path_offset, "Resource index path runs past the pool", _fileName);

        entry.Path = string_view {reinterpret_cast<const char*>(_index.data()) + path_offset, path_length};
        entry.PackIndex = span_read_uint32(_index, entry_offset + ENTRY_OFFSET_PACK_INDEX);
        entry.Codec = span_read_uint32(_index, entry_offset + ENTRY_OFFSET_CODEC);
        entry.DataOffset = span_read_uint64(_index, entry_offset + ENTRY_OFFSET_DATA_OFFSET);
        entry.StoredSize = span_read_uint64(_index, entry_offset + ENTRY_OFFSET_STORED_SIZE);
        entry.DecodedSize = span_read_uint64(_index, entry_offset + ENTRY_OFFSET_DECODED_SIZE);

        FO_VERIFY_AND_THROW(entry.PackIndex < _header.PackCount, "Resource index entry names a pack that is not listed", _fileName, entry.Path);
        FO_VERIFY_AND_THROW(entry.Codec <= static_cast<uint32_t>(ResourcePackCodec::Deflate), "Resource index entry has an unknown codec", _fileName, entry.Path);

        if (entry.Codec == static_cast<uint32_t>(ResourcePackCodec::Stored)) {
            FO_VERIFY_AND_THROW(entry.StoredSize == entry.DecodedSize, "Stored resource index entry declares two sizes", _fileName, entry.Path);
        }

        bool first_of_its_path = _entryLookup.emplace(entry.Path, _entries.size()).second;
        FO_VERIFY_AND_THROW(first_of_its_path, "Resource index holds the same path twice", _fileName, entry.Path);
        _fileNames.emplace_back(entry.Path);
        _entries.emplace_back(entry);
    }
}

auto ResourceIndexSource::FindEntry(string_view path) const -> nptr<const FileEntry>
{
    FO_STACK_TRACE_ENTRY();

    auto it = _entryLookup.find(path);

    if (it == _entryLookup.end()) {
        return nullptr;
    }

    return make_ptr(&_entries[it->second]);
}

auto ResourceIndexSource::ReadEntryData(const FileEntry& entry) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> stored(numeric_cast<size_t>(entry.StoredSize));
    const disk_read_file& pack_file = _packFiles[entry.PackIndex];
    bool payload_read = pack_file.read_at(entry.DataOffset, stored);
    FO_VERIFY_AND_THROW(payload_read, "Can't read a resource through the index", _fileName, entry.Path);

    if (entry.Codec == static_cast<uint32_t>(ResourcePackCodec::Deflate)) {
        vector<uint8_t> decoded = Compressor::DecompressExact(stored, numeric_cast<size_t>(entry.DecodedSize));
        FO_VERIFY_AND_THROW(decoded.size() == entry.DecodedSize, "Resource did not decode to its declared size", _fileName, entry.Path);
        return decoded;
    }

    return stored;
}

auto ResourceIndexSource::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !!FindEntry(path);
}

auto ResourceIndexSource::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<const FileEntry> entry = FindEntry(path);

    if (!entry) {
        return false;
    }

    size = numeric_cast<size_t>(entry->DecodedSize);
    write_time = _writeTime;

    return true;
}

auto ResourceIndexSource::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    nptr<const FileEntry> entry = FindEntry(path);

    if (!entry) {
        return nullptr;
    }

    vector<uint8_t> data = ReadEntryData(*entry);
    size = numeric_cast<size_t>(entry->DecodedSize);
    write_time = _writeTime;

    auto buf = unique_arr_ptr<uint8_t> {SafeAlloc::MakeUniqueArr<uint8_t>(data.size())};
    std::copy(data.begin(), data.end(), buf.get());

    return MakeFileBufferHolder(std::move(buf));
}

auto ResourceIndexSource::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return GetFileNamesGeneric(_fileNames, dir, recursive, ext);
}

auto ResourceIndexSource::GetIndexSnapshot() const -> optional<vector<IndexedFile>>
{
    FO_STACK_TRACE_ENTRY();

    vector<IndexedFile> snapshot;
    snapshot.reserve(_entries.size());

    for (const FileEntry& entry : _entries) {
        snapshot.emplace_back(IndexedFile {string(entry.Path), numeric_cast<size_t>(entry.DecodedSize), _writeTime});
    }

    return snapshot;
}

FO_END_NAMESPACE
