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
static constexpr size_t PACK_OFFSET_PATCH_HASH = 16;
static constexpr size_t PACK_OFFSET_PATCH_END = 24;

// Entry layout inside the decoded index
static constexpr size_t ENTRY_OFFSET_PATH_OFFSET = 0;
static constexpr size_t ENTRY_OFFSET_PATH_LENGTH = 4;
static constexpr size_t ENTRY_OFFSET_PACK_INDEX = 8;
static constexpr size_t ENTRY_OFFSET_CODEC = 12;
static constexpr size_t ENTRY_OFFSET_DATA_OFFSET = 16;
static constexpr size_t ENTRY_OFFSET_STORED_SIZE = 24;
static constexpr size_t ENTRY_OFFSET_DECODED_SIZE = 32;
static constexpr size_t ENTRY_OFFSET_CONTENT_HASH = 40;
static constexpr size_t ENTRY_OFFSET_SOURCE = 48;

// The offsets above are the format. These pin the record sizes to them, so widening a field without widening
// the record it sits in stops compiling instead of writing a file nothing can read
static_assert(HEADER_OFFSET_CHECKSUM + sizeof(uint64_t) == RESOURCE_INDEX_HEADER_SIZE);
static_assert(PACK_OFFSET_PATCH_END + sizeof(uint64_t) == RESOURCE_INDEX_PACK_SIZE);
static_assert(ENTRY_OFFSET_SOURCE + sizeof(uint64_t) == RESOURCE_INDEX_ENTRY_SIZE);
static_assert(RESOURCE_INDEX_MAGIC == (uint32_t {'F'} | uint32_t {'O'} << 8 | uint32_t {'I'} << 16 | uint32_t {'X'} << 24));

auto IsResourceIndexCurrent(string_view path, const vector<string>& pack_dirs, const vector<string>& pack_names) noexcept -> bool
{
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
    uint64_t FileContentHash {};
    uint32_t Source {};
};

static void BuildHeaderBytes(const ResourceIndexHeader& header, span<uint8_t> buf) noexcept
{
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
    if (span_read_uint32(buf, HEADER_OFFSET_MAGIC) != RESOURCE_INDEX_MAGIC) {
        return false;
    }

    if (span_read_uint64(buf, HEADER_OFFSET_CHECKSUM) != HashResourceBytes(RESOURCE_PACK_HASH_SEED, {buf.data(), HEADER_OFFSET_CHECKSUM})) {
        return false;
    }

    header.VersionMajor = span_read_uint16(buf, HEADER_OFFSET_VERSION_MAJOR);
    header.VersionMinor = span_read_uint16(buf, HEADER_OFFSET_VERSION_MINOR);

    if (header.VersionMajor != RESOURCE_INDEX_VERSION_MAJOR || header.VersionMinor != RESOURCE_INDEX_VERSION_MINOR) {
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
    uint64_t hash = RESOURCE_PACK_HASH_SEED;

    for (const auto& pack : packs) {
        hash = HashResourceBytes(hash, {reinterpret_cast<const uint8_t*>(pack.Name.data()), pack.Name.size()});

        array<uint8_t, 24> pack_hash_bytes = {};
        span_write_uint64(pack_hash_bytes, 0, pack.PackHash);
        span_write_uint64(pack_hash_bytes, 8, pack.PatchHash);
        span_write_uint64(pack_hash_bytes, 16, pack.PatchEnd);
        hash = HashResourceBytes(hash, pack_hash_bytes);
    }

    return hash;
}

auto ReadResourceIndexHeader(string_view path, ResourceIndexHeader& header) noexcept -> bool
{
    fs::disk_read_file file {path};

    if (!file) {
        return false;
    }

    array<uint8_t, RESOURCE_INDEX_HEADER_SIZE> buf = {};

    if (!file.read_at(0, buf)) {
        return false;
    }

    if (!ParseHeaderBytes(buf, header)) {
        return false;
    }

    uint64_t file_size = file.get_size();
    return header.IndexOffset >= RESOURCE_INDEX_HEADER_SIZE && header.IndexStoredSize <= file_size && header.IndexOffset <= file_size - header.IndexStoredSize && header.IndexCodec <= static_cast<uint32_t>(ResourcePackCodec::Deflate);
}

auto GetResourceIndexPackNames(const vector<string>& pack_names) -> vector<string>
{
    auto first = pack_names.begin();

    for (auto it = pack_names.begin(); it != pack_names.end(); ++it) {
        if (*it == EMBEDDED_PACK_NAME) {
            first = std::next(it);
        }
    }

    return {first, pack_names.end()};
}

auto ResolveResourceIndexPacks(const vector<string>& pack_dirs, const vector<string>& pack_names, vector<ResourceIndexPack>& packs, vector<string>& pack_paths) noexcept -> bool
{
    packs.clear();
    pack_paths.clear();

    if (pack_dirs.empty()) {
        return pack_names.empty();
    }

    try {
        for (const string& name : pack_names) {
            FO_VERIFY_AND_THROW(fs::is_contained_relative_path(name), "Invalid indexed pack name", name);
            string resolved_path = ResolveResourcePackPath(pack_dirs, name);
            ResourcePackHeader header;

            if (!ReadResourcePackHeader(resolved_path, header)) {
                return false;
            }

            string patch_path = strex(pack_dirs.back()).combine_path(strex("{}.patch.fores", name)).str();
            auto patch = ReadResourcePatchInfo(patch_path, header);
            packs.emplace_back(ResourceIndexPack {name, header.PackHash, patch ? patch->IndexHash : 0, patch ? patch->CommittedSize : 0, std::move(patch_path)});
            pack_paths.emplace_back(std::move(resolved_path));
        }
    }
    catch (const std::exception&) {
        packs.clear();
        pack_paths.clear();
        return false;
    }

    return true;
}

void BuildResourceIndex(string_view path, const vector<string>& pack_paths, const vector<ResourceIndexPack>& packs, ResourcePackWriteSettings settings)
{
    FO_TRACE_ZONE(FileSystem);

    FO_VERIFY_AND_THROW(pack_paths.size() == packs.size(), "Resource index pack paths and pack records disagree", pack_paths.size(), packs.size());

    // One entry per path, folded in the given order with the last pack winning, which is the same precedence
    // the per-pack mounts have today
    unordered_map<string, ResourceIndexEntryRecord> merged;

    for (size_t pack_index = 0; pack_index < pack_paths.size(); ++pack_index) {
        ResourcePackSource pack {pack_paths[pack_index], packs[pack_index].PatchPath};
        FO_VERIFY_AND_THROW(pack.GetPackHash() == packs[pack_index].PackHash && (pack.GetPatchInfo() ? pack.GetPatchInfo()->IndexHash : 0) == packs[pack_index].PatchHash && (pack.GetPatchInfo() ? pack.GetPatchInfo()->CommittedSize : 0) == packs[pack_index].PatchEnd, "Resource pair changed during index build");

        for (auto& entry : pack.GetEntryRefs()) {
            ResourceIndexEntryRecord record {numeric_cast<uint32_t>(pack_index), entry.Codec, entry.DataOffset, entry.StoredSize, entry.DecodedSize, entry.FileContentHash, entry.Source};
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
        span_write_uint64(index, record_offset + PACK_OFFSET_PATCH_HASH, packs[i].PatchHash);
        span_write_uint64(index, record_offset + PACK_OFFSET_PATCH_END, packs[i].PatchEnd);
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
        span_write_uint64(index, entry_offset + ENTRY_OFFSET_CONTENT_HASH, record.FileContentHash);
        span_write_uint32(index, entry_offset + ENTRY_OFFSET_SOURCE, record.Source);
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

    // The temporary keeps partial writes out of the live name; interruption during promotion requires a rebuild
    string temp_path = strex("{}.tmp", path).str();

    // A full disk throws mid-write and nothing revisits this name, so the partial file goes out with the throw
    auto remove_on_fail = scope_fail([&temp_path]() noexcept { (void)fs::remove_file(temp_path); });

    {
        fs::disk_write_file file {temp_path};
        FO_VERIFY_AND_THROW(!!file, "Can't create resource index file", temp_path);
        bool header_written = file.write(header_bytes);
        FO_VERIFY_AND_THROW(header_written, "Can't write resource index header", temp_path);

        bool index_written = file.write(stored_index);
        FO_VERIFY_AND_THROW(index_written, "Can't write resource index", temp_path);

        bool flushed = file.flush();
        FO_VERIFY_AND_THROW(flushed, "Can't finish resource index file", temp_path);
        file.close();
    }

    (void)fs::remove_file(path);
    bool renamed = fs::rename(temp_path, path);
    FO_VERIFY_AND_THROW(renamed, "Can't put the resource index in place", temp_path, path);
}

ResourceIndexSource::ResourceIndexSource(string_view path, const vector<string>& pack_dirs) :
    _fileName {path},
    _file {path}
{
    FO_VERIFY_AND_THROW(!!_file, "Can't open resource index", _fileName);

    array<uint8_t, RESOURCE_INDEX_HEADER_SIZE> header_bytes = {};
    bool header_read = _file.read_at(0, header_bytes);
    FO_VERIFY_AND_THROW(header_read, "Can't read resource index header", _fileName);

    bool header_valid = ParseHeaderBytes(header_bytes, _header);
    FO_VERIFY_AND_THROW(header_valid, "Resource index header is not valid", _fileName);

    ParseIndex(pack_dirs);

    // Every read goes to the pack handles, and an open index would keep another instance from replacing it on Windows
    _file.close();
}

void ResourceIndexSource::ParseIndex(const vector<string>& pack_dirs)
{
    FO_TRACE_ZONE(FileSystem);

    // Sized through the handle the bytes are read from, so a replacement in between cannot pair one file's bounds
    // with another's contents
    uint64_t file_size = _file.get_size();
    auto fits_in_file = [&](uint64_t offset, uint64_t size) { return offset >= RESOURCE_INDEX_HEADER_SIZE && size <= file_size && offset <= file_size - size; };
    FO_VERIFY_AND_THROW(fits_in_file(_header.IndexOffset, _header.IndexStoredSize), "Resource index extent is outside the file", _fileName);

    FO_VERIFY_AND_THROW(_header.IndexStoredSize <= std::numeric_limits<uint32_t>::max() && _header.IndexDecodedSize <= std::numeric_limits<uint32_t>::max(), "Merged resource catalog exceeds its size limit", _fileName);
    uint64_t tables_size = numeric_cast<uint64_t>(_header.PackCount) * RESOURCE_INDEX_PACK_SIZE + numeric_cast<uint64_t>(_header.EntryCount) * RESOURCE_INDEX_ENTRY_SIZE;
    FO_VERIFY_AND_THROW(_header.IndexDecodedSize >= tables_size, "Resource index is too small for what it declares", _fileName);
    size_t pool_begin = numeric_cast<size_t>(tables_size);
    size_t pack_table_size = numeric_cast<size_t>(numeric_cast<uint64_t>(_header.PackCount) * RESOURCE_INDEX_PACK_SIZE);
    FO_VERIFY_AND_THROW(_header.IndexCodec <= static_cast<uint32_t>(ResourcePackCodec::Deflate), "Resource index uses an unknown codec", _fileName);

    vector<uint8_t> stored_index(numeric_cast<size_t>(_header.IndexStoredSize));
    bool index_read = _file.read_at(_header.IndexOffset, stored_index);
    FO_VERIFY_AND_THROW(index_read, "Can't read the resource index", _fileName);

    if (_header.IndexCodec == static_cast<uint32_t>(ResourcePackCodec::Deflate)) {
        _index = compressor::decompress_exact(stored_index, numeric_cast<size_t>(_header.IndexDecodedSize));
    }
    else {
        FO_VERIFY_AND_THROW(_header.IndexStoredSize == _header.IndexDecodedSize, "Stored resource index declares two sizes", _fileName);
        _index = std::move(stored_index);
    }

    FO_VERIFY_AND_THROW(_index.size() == _header.IndexDecodedSize, "Resource index did not decode to its declared size", _fileName);

    // The packs come first: every entry names one by position, so they must all resolve before any entry does
    _packFiles.reserve(_header.PackCount);
    _patchFiles.reserve(_header.PackCount);
    vector<uint64_t> patch_data_ends;
    _packWriteTimes.reserve(_header.PackCount);
    vector<ResourceIndexPack> packs;
    packs.reserve(_header.PackCount);
    vector<ResourcePackHeader> pack_headers;
    pack_headers.reserve(_header.PackCount);

    for (uint32_t i = 0; i < _header.PackCount; ++i) {
        size_t record_offset = numeric_cast<size_t>(i) * RESOURCE_INDEX_PACK_SIZE;
        uint32_t name_offset = span_read_uint32(_index, record_offset + PACK_OFFSET_NAME_OFFSET);
        uint32_t name_length = span_read_uint32(_index, record_offset + PACK_OFFSET_NAME_LENGTH);
        FO_VERIFY_AND_THROW(name_length != 0 && name_offset >= pool_begin, "Resource index pack name is outside the pool", _fileName);
        FO_VERIFY_AND_THROW(name_offset <= _index.size() && name_length <= _index.size() - name_offset, "Resource index pack name runs past the pool", _fileName);

        string pack_name {reinterpret_cast<const char*>(_index.data()) + name_offset, name_length};
        uint64_t pack_hash = span_read_uint64(_index, record_offset + PACK_OFFSET_PACK_HASH);

        uint64_t patch_hash = span_read_uint64(_index, record_offset + PACK_OFFSET_PATCH_HASH);
        uint64_t patch_end = span_read_uint64(_index, record_offset + PACK_OFFSET_PATCH_END);
        vector<ResourceIndexPack> resolved;
        vector<string> paths;
        bool pair_resolved = ResolveResourceIndexPacks(pack_dirs, {pack_name}, resolved, paths);
        FO_VERIFY_AND_THROW(pair_resolved && resolved.front().PackHash == pack_hash && resolved.front().PatchHash == patch_hash && resolved.front().PatchEnd == patch_end, "Resource index names a different resource pair", pack_name);
        ResourcePackHeader resolved_header;
        fs::disk_read_file pack_file = OpenResourcePackFile(paths.front());
        bool header_read = ReadResourcePackHeader(pack_file, resolved_header);
        FO_VERIFY_AND_THROW(header_read && resolved_header.PackHash == pack_hash, "Indexed base changed before opening", pack_name);
        pack_headers.emplace_back(resolved_header);
        _packFiles.emplace_back(std::move(pack_file));
        fs::disk_read_file patch_file {resolved.front().PatchPath};
        auto patch = ReadResourcePatchInfo(patch_file, resolved_header);
        FO_VERIFY_AND_THROW((patch ? patch->IndexHash : 0) == patch_hash && (patch ? patch->CommittedSize : 0) == patch_end, "Indexed patch changed before opening", pack_name);

        if (!patch) {
            patch_file.close();
        }

        _patchFiles.emplace_back(std::move(patch_file));
        patch_data_ends.emplace_back(patch ? patch->IndexOffset : 0);
        _packWriteTimes.emplace_back(GetResourcePackWriteTime(patch ? resolved.front().PatchPath : paths.front()));
        packs.emplace_back(std::move(resolved.front()));
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
        FO_VERIFY_AND_THROW(path_length != 0 && path_offset >= pool_begin, "Resource index path is outside the pool", _fileName);
        FO_VERIFY_AND_THROW(path_offset <= _index.size() && path_length <= _index.size() - path_offset, "Resource index path runs past the pool", _fileName);

        entry.Path = string_view {reinterpret_cast<const char*>(_index.data()) + path_offset, path_length};
        FO_VERIFY_AND_THROW(IsResourcePathCanonical(entry.Path), "Indexed resource path is not canonical", entry.Path);
        entry.PackIndex = span_read_uint32(_index, entry_offset + ENTRY_OFFSET_PACK_INDEX);
        entry.Codec = span_read_uint32(_index, entry_offset + ENTRY_OFFSET_CODEC);
        entry.DataOffset = span_read_uint64(_index, entry_offset + ENTRY_OFFSET_DATA_OFFSET);
        entry.StoredSize = span_read_uint64(_index, entry_offset + ENTRY_OFFSET_STORED_SIZE);
        entry.DecodedSize = span_read_uint64(_index, entry_offset + ENTRY_OFFSET_DECODED_SIZE);
        entry.FileContentHash = span_read_uint64(_index, entry_offset + ENTRY_OFFSET_CONTENT_HASH);
        entry.Source = span_read_uint32(_index, entry_offset + ENTRY_OFFSET_SOURCE);
        FO_VERIFY_AND_THROW(entry.Source <= 1 && span_read_uint32(_index, entry_offset + ENTRY_OFFSET_SOURCE + 4) == 0, "Invalid indexed resource source", entry.Path);

        FO_VERIFY_AND_THROW(entry.PackIndex < _header.PackCount, "Resource index entry names a pack that is not listed", _fileName, entry.Path);
        FO_VERIFY_AND_THROW(entry.Codec <= static_cast<uint32_t>(ResourcePackCodec::Deflate), "Resource index entry has an unknown codec", _fileName, entry.Path);
        const ResourcePackHeader& pack_header = pack_headers[entry.PackIndex];
        FO_VERIFY_AND_THROW(entry.Source == 0 || patch_data_ends[entry.PackIndex] >= RESOURCE_PATCH_HEADER_SIZE, "Indexed patch is missing", entry.Path);
        uint64_t data_begin = entry.Source == 0 ? pack_header.DataOffset : RESOURCE_PATCH_HEADER_SIZE;
        uint64_t data_size = entry.Source == 0 ? pack_header.DataSize : patch_data_ends[entry.PackIndex] - RESOURCE_PATCH_HEADER_SIZE;
        FO_VERIFY_AND_THROW(entry.StoredSize <= data_size && entry.DataOffset >= data_begin && entry.DataOffset - data_begin <= data_size - entry.StoredSize, "Resource index entry extent is outside its source data", _fileName, entry.Path);

        if (entry.Codec == static_cast<uint32_t>(ResourcePackCodec::Stored)) {
            FO_VERIFY_AND_THROW(entry.StoredSize == entry.DecodedSize, "Stored resource index entry declares two sizes", _fileName, entry.Path);
        }

        bool first_of_its_path = _entryLookup.emplace(entry.Path, _entries.size()).second;
        FO_VERIFY_AND_THROW(first_of_its_path, "Resource index holds the same path twice", _fileName, entry.Path);
        _entries.emplace_back(entry);
    }

    std::ranges::sort(_entries, [](const FileEntry& a, const FileEntry& b) { return a.PackIndex != b.PackIndex ? a.PackIndex > b.PackIndex : a.Path < b.Path; });
    _entryLookup.clear();

    for (size_t i = 0; i < _entries.size(); ++i) {
        _entryLookup.emplace(_entries[i].Path, i);
    }
}

auto ResourceIndexSource::FindEntry(string_view path) const -> nptr<const FileEntry>
{
    auto it = _entryLookup.find(path);

    if (it == _entryLookup.end()) {
        return nullptr;
    }

    return make_ptr(&_entries[it->second]);
}

auto ResourceIndexSource::ReadEntryData(const FileEntry& entry) const -> vector<uint8_t>
{
    vector<uint8_t> stored(numeric_cast<size_t>(entry.StoredSize));
    const fs::disk_read_file& pack_file = entry.Source == 0 ? _packFiles[entry.PackIndex] : _patchFiles[entry.PackIndex];
    bool payload_read = pack_file.read_at(entry.DataOffset, stored);
    FO_VERIFY_AND_THROW(payload_read, "Can't read a resource through the index", _fileName, entry.Path);

    if (entry.Codec == static_cast<uint32_t>(ResourcePackCodec::Deflate)) {
        vector<uint8_t> decoded = compressor::decompress_exact(stored, numeric_cast<size_t>(entry.DecodedSize));
        FO_VERIFY_AND_THROW(decoded.size() == entry.DecodedSize && HashResourceBytes(RESOURCE_PACK_HASH_SEED, decoded) == entry.FileContentHash, "Resource did not decode to its declared size", _fileName, entry.Path);
        return decoded;
    }

    FO_VERIFY_AND_THROW(HashResourceBytes(RESOURCE_PACK_HASH_SEED, stored) == entry.FileContentHash, "Indexed resource content hash mismatch", entry.Path);
    return stored;
}

auto ResourceIndexSource::IsFileExists(string_view path) const -> bool
{
    return !!FindEntry(path);
}

auto ResourceIndexSource::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    nptr<const FileEntry> entry = FindEntry(path);

    if (!entry) {
        return false;
    }

    size = numeric_cast<size_t>(entry->DecodedSize);
    write_time = _packWriteTimes[entry->PackIndex];

    return true;
}

auto ResourceIndexSource::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_TRACE_ZONE(FileSystem);

    nptr<const FileEntry> entry = FindEntry(path);

    if (!entry) {
        return nullptr;
    }

    vector<uint8_t> data = ReadEntryData(*entry);
    size = numeric_cast<size_t>(entry->DecodedSize);
    write_time = _packWriteTimes[entry->PackIndex];

    auto buf = unique_arr_ptr<uint8_t> {safe_alloc::make_unique_arr<uint8_t>(data.size())};
    std::copy(data.begin(), data.end(), buf.get());

    return MakeFileBufferHolder(std::move(buf));
}

auto ResourceIndexSource::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_TRACE_ZONE(FileSystem);

    vector<string_view> names;
    names.reserve(_entries.size());

    for (const FileEntry& entry : _entries) {
        names.emplace_back(entry.Path);
    }

    return GetFileNamesGeneric(names, dir, recursive, ext);
}

auto ResourceIndexSource::GetIndexSnapshot() const -> optional<vector<IndexedFile>>
{
    vector<IndexedFile> snapshot;
    snapshot.reserve(_entries.size());

    for (const FileEntry& entry : _entries) {
        snapshot.emplace_back(IndexedFile {string(entry.Path), numeric_cast<size_t>(entry.DecodedSize), _packWriteTimes[entry.PackIndex]});
    }

    return snapshot;
}

FO_END_NAMESPACE
