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

#include "ResourcePack.h"

#include "minizip/unzip.h"

FO_BEGIN_NAMESPACE

// Header layout, little endian throughout. The checksum covers everything before it, so a header that survived
// a truncated write is rejected before any offset it carries is believed
static constexpr size_t HEADER_OFFSET_MAGIC = 0;
static constexpr size_t HEADER_OFFSET_VERSION_MAJOR = 4;
static constexpr size_t HEADER_OFFSET_VERSION_MINOR = 6;
static constexpr size_t HEADER_OFFSET_PACK_HASH = 8;
static constexpr size_t HEADER_OFFSET_INDEX_OFFSET = 16;
static constexpr size_t HEADER_OFFSET_INDEX_STORED_SIZE = 24;
static constexpr size_t HEADER_OFFSET_INDEX_DECODED_SIZE = 32;
static constexpr size_t HEADER_OFFSET_INDEX_CODEC = 40;
static constexpr size_t HEADER_OFFSET_ENTRY_COUNT = 44;
static constexpr size_t HEADER_OFFSET_DATA_OFFSET = 48;
static constexpr size_t HEADER_OFFSET_DATA_SIZE = 56;
static constexpr size_t HEADER_OFFSET_CONTENT_HASH = 64;
static constexpr size_t HEADER_OFFSET_CHECKSUM = 72;

// Entry layout inside the decoded index
static constexpr size_t ENTRY_OFFSET_PATH_OFFSET = 0;
static constexpr size_t ENTRY_OFFSET_PATH_LENGTH = 4;
static constexpr size_t ENTRY_OFFSET_DATA_OFFSET = 8;
static constexpr size_t ENTRY_OFFSET_STORED_SIZE = 16;
static constexpr size_t ENTRY_OFFSET_DECODED_SIZE = 24;
static constexpr size_t ENTRY_OFFSET_CODEC = 32;
static constexpr size_t ENTRY_OFFSET_FLAGS = 36;
static constexpr size_t ENTRY_OFFSET_CONTENT_HASH = 40;
static constexpr uint32_t PATCH_MAGIC = 0x50524F46;
static constexpr uint32_t PATCH_FOOTER_MAGIC = 0x54524F46;

static auto BuildIndexBytes(const_span<ResourcePackEntryRef> entries) -> vector<uint8_t>;
static auto ReadPatchCatalog(const fs::disk_read_file& file, const ResourcePackHeader& base_header, ResourcePatchInfo& info, vector<ResourcePackEntryRef>& entries) -> bool;
static auto IsPatchHeaderBound(const_span<uint8_t> header, uint64_t base_pack_hash) noexcept -> bool;
static auto DecodeResourceData(const_span<uint8_t> stored, const ResourcePackEntryRef& entry) -> vector<uint8_t>;
static auto IsEncodedResourceIntact(const_span<uint8_t> stored, const ResourcePackEntryRef& entry) -> bool;
static auto FindArchiveSeparator(string_view path) noexcept -> size_t;

// The offsets above are the format. These pin the record sizes to them, so widening a field without widening
// the record it sits in stops compiling instead of writing a file nothing can read
static_assert(HEADER_OFFSET_CHECKSUM + sizeof(uint64_t) == RESOURCE_PACK_HEADER_SIZE);
static_assert(ENTRY_OFFSET_CONTENT_HASH + sizeof(uint64_t) == RESOURCE_PACK_ENTRY_SIZE);
static_assert(RESOURCE_PACK_MAGIC == (uint32_t {'F'} | uint32_t {'O'} << 8 | uint32_t {'R'} << 16 | uint32_t {'S'} << 24));

static void BuildHeaderBytes(const ResourcePackHeader& header, span<uint8_t> buf) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    span_write_uint32(buf, HEADER_OFFSET_MAGIC, RESOURCE_PACK_MAGIC);
    span_write_uint16(buf, HEADER_OFFSET_VERSION_MAJOR, header.VersionMajor);
    span_write_uint16(buf, HEADER_OFFSET_VERSION_MINOR, header.VersionMinor);
    span_write_uint64(buf, HEADER_OFFSET_PACK_HASH, header.PackHash);
    span_write_uint64(buf, HEADER_OFFSET_INDEX_OFFSET, header.IndexOffset);
    span_write_uint64(buf, HEADER_OFFSET_INDEX_STORED_SIZE, header.IndexStoredSize);
    span_write_uint64(buf, HEADER_OFFSET_INDEX_DECODED_SIZE, header.IndexDecodedSize);
    span_write_uint32(buf, HEADER_OFFSET_INDEX_CODEC, header.IndexCodec);
    span_write_uint32(buf, HEADER_OFFSET_ENTRY_COUNT, header.EntryCount);
    span_write_uint64(buf, HEADER_OFFSET_DATA_OFFSET, header.DataOffset);
    span_write_uint64(buf, HEADER_OFFSET_DATA_SIZE, header.DataSize);
    span_write_uint64(buf, HEADER_OFFSET_CONTENT_HASH, header.ContentHash);
    span_write_uint64(buf, HEADER_OFFSET_CHECKSUM, HashResourceBytes(RESOURCE_PACK_HASH_SEED, const_span<uint8_t> {buf.data(), HEADER_OFFSET_CHECKSUM}));
}

static auto ParseHeaderBytes(const_span<uint8_t> buf, ResourcePackHeader& header) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (span_read_uint32(buf, HEADER_OFFSET_MAGIC) != RESOURCE_PACK_MAGIC) {
        return false;
    }

    uint64_t checksum = HashResourceBytes(RESOURCE_PACK_HASH_SEED, const_span<uint8_t> {buf.data(), HEADER_OFFSET_CHECKSUM});

    if (span_read_uint64(buf, HEADER_OFFSET_CHECKSUM) != checksum) {
        return false;
    }

    header.VersionMajor = span_read_uint16(buf, HEADER_OFFSET_VERSION_MAJOR);
    header.VersionMinor = span_read_uint16(buf, HEADER_OFFSET_VERSION_MINOR);
    header.PackHash = span_read_uint64(buf, HEADER_OFFSET_PACK_HASH);
    header.IndexOffset = span_read_uint64(buf, HEADER_OFFSET_INDEX_OFFSET);
    header.IndexStoredSize = span_read_uint64(buf, HEADER_OFFSET_INDEX_STORED_SIZE);
    header.IndexDecodedSize = span_read_uint64(buf, HEADER_OFFSET_INDEX_DECODED_SIZE);
    header.IndexCodec = span_read_uint32(buf, HEADER_OFFSET_INDEX_CODEC);
    header.EntryCount = span_read_uint32(buf, HEADER_OFFSET_ENTRY_COUNT);
    header.DataOffset = span_read_uint64(buf, HEADER_OFFSET_DATA_OFFSET);
    header.DataSize = span_read_uint64(buf, HEADER_OFFSET_DATA_SIZE);
    header.ContentHash = span_read_uint64(buf, HEADER_OFFSET_CONTENT_HASH);
    return header.VersionMajor == RESOURCE_PACK_VERSION_MAJOR && header.VersionMinor == RESOURCE_PACK_VERSION_MINOR;
}

auto SerializeResourcePackHeader(const ResourcePackHeader& header) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data(RESOURCE_PACK_HEADER_SIZE);
    BuildHeaderBytes(header, data);
    return data;
}

auto ParseResourcePackHeader(const_span<uint8_t> data, ResourcePackHeader& header) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return data.size() == RESOURCE_PACK_HEADER_SIZE && ParseHeaderBytes(data, header);
}

auto ResolveResourcePackPath(const vector<string>& directories, string_view name) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!directories.empty() && IsResourcePathCanonical(name), "Invalid resource pack search", name);
    string writable = strex(directories.back()).combine_path(strex("{}.fores", name)).str();
    string backup = strex("{}{}", writable, REPLACED_FILE_BACKUP_SUFFIX).str();

    if (!fs::exists(writable) && fs::exists(backup)) {
        fs::disk_directory_lock lock {directories.back()};
        FO_VERIFY_AND_THROW(lock, "Resource directory is being updated", writable);

        if (!fs::exists(writable) && fs::exists(backup)) {
            bool restored = fs::rename_durable(backup, writable);
            FO_VERIFY_AND_THROW(restored, "Can't restore resource base backup", writable);
        }
    }

    for (auto directory = directories.rbegin(); directory != directories.rend(); ++directory) {
        string candidate = strex(*directory).combine_path(strex("{}.fores", name)).str();

        if (OpenResourcePackFile(candidate)) {
            return candidate;
        }
    }

    return strex(directories.front()).combine_path(strex("{}.fores", name)).str();
}

auto OpenResourcePackFile(string_view path) noexcept -> fs::disk_read_file
{
    FO_STACK_TRACE_ENTRY();

    size_t separator = FindArchiveSeparator(path);

    if (separator == string_view::npos) {
        return fs::disk_read_file {path};
    }

    string archive_path {path.substr(0, separator)};
    string asset_path {path.substr(separator + 2)};
    auto archive = make_nptr(unzOpen64(archive_path.c_str()));

    if (!archive) {
        return {};
    }

    auto close_archive = scope_exit([&]() noexcept { unzClose(archive.get()); });
    unz_file_info64 info {};

    if (unzLocateFile(archive.get(), asset_path.c_str(), 1) != UNZ_OK || unzGetCurrentFileInfo64(archive.get(), &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK || info.compression_method != 0 || (info.flag & 1) != 0 || info.compressed_size != info.uncompressed_size || unzOpenCurrentFile(archive.get()) != UNZ_OK) {
        return {};
    }

    uint64_t offset = numeric_cast<uint64_t>(unzGetCurrentFileZStreamPos64(archive.get()));
    return fs::disk_read_file {archive_path, offset, numeric_cast<uint64_t>(info.uncompressed_size)};
}

auto GetResourcePackWriteTime(string_view path) noexcept -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    size_t separator = FindArchiveSeparator(path);
    return fs::last_write_time(path.substr(0, separator));
}

static auto FindArchiveSeparator(string_view path) noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    // A directory may itself end in '!', so "!/" separates an archive only where the prefix is a file
    for (size_t separator = path.find("!/"); separator != string_view::npos; separator = path.find("!/", separator + 2)) {
        string_view archive_path = path.substr(0, separator);

        if (fs::exists(archive_path) && !fs::is_dir(archive_path)) {
            return separator;
        }
    }

    return string_view::npos;
}

auto ReadResourcePackHeader(string_view path, ResourcePackHeader& header) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    fs::disk_read_file file = OpenResourcePackFile(path);
    return ReadResourcePackHeader(file, header);
}

auto ReadResourcePackHeader(const fs::disk_read_file& file, ResourcePackHeader& header) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!file || file.get_size() < RESOURCE_PACK_HEADER_SIZE) {
        return false;
    }

    array<uint8_t, RESOURCE_PACK_HEADER_SIZE> buf = {};

    if (!file.read_at(0, span<uint8_t> {buf.data(), buf.size()})) {
        return false;
    }

    uint64_t size = file.get_size();
    return ParseHeaderBytes(buf, header) && header.DataOffset == RESOURCE_PACK_HEADER_SIZE && header.DataSize <= size - RESOURCE_PACK_HEADER_SIZE && header.IndexOffset == header.DataOffset + header.DataSize && header.IndexOffset <= size && header.IndexStoredSize == size - header.IndexOffset;
}

auto VerifyResourcePackFile(string_view path, uint64_t expected_pack_hash) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    ResourcePackHeader header;
    fs::disk_read_file file = OpenResourcePackFile(path);

    if (!ReadResourcePackHeader(file, header) || header.PackHash != expected_pack_hash) {
        return false;
    }

    // The body is hashed in bounded slices so a multi-gigabyte pack never has to sit in memory to be checked
    constexpr size_t SLICE_SIZE = 1024 * 1024;
    auto slice = vector<uint8_t>(SLICE_SIZE);
    uint64_t hash = RESOURCE_PACK_HASH_SEED;
    uint64_t offset = RESOURCE_PACK_HEADER_SIZE;
    uint64_t remaining = file.get_size() - RESOURCE_PACK_HEADER_SIZE;

    while (remaining != 0) {
        size_t chunk = numeric_cast<size_t>(std::min<uint64_t>(remaining, SLICE_SIZE));

        if (!file.read_at(offset, span<uint8_t> {slice.data(), chunk})) {
            return false;
        }

        hash = HashResourceBytes(hash, const_span<uint8_t> {slice.data(), chunk});
        offset += chunk;
        remaining -= chunk;
    }

    return hash == header.PackHash;
}

auto EncodeResourceBlob(const_span<uint8_t> data, const ResourcePackWriteSettings& settings, uint32_t& codec) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    codec = static_cast<uint32_t>(ResourcePackCodec::Stored);

    if (data.size() < RESOURCE_PACK_MIN_COMPRESSED_SIZE) {
        return {data.begin(), data.end()};
    }

    vector<uint8_t> compressed = compressor::compress(data, settings.CompressLevel);
    size_t max_kept_size = data.size() - data.size() * numeric_cast<size_t>(settings.MinCompressGainPercent) / 100;

    if (compressed.size() >= max_kept_size) {
        return {data.begin(), data.end()};
    }

    codec = static_cast<uint32_t>(ResourcePackCodec::Deflate);
    return compressed;
}

ResourcePackWriter::ResourcePackWriter(string_view path, ResourcePackWriteSettings settings) :
    _path {path},
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(settings.CompressLevel >= 0 && settings.CompressLevel <= 9, "Pack compression level is out of the zlib range", settings.CompressLevel);
    FO_VERIFY_AND_THROW(settings.MinCompressGainPercent >= 0 && settings.MinCompressGainPercent <= 100, "Pack minimum compression gain is not a percentage", settings.MinCompressGainPercent);

    // Creating the target truncates it, so it is the first thing to touch the disk and the settings go first
    _file = fs::disk_write_file {_path};

    if (!_file) {
        throw ResourcePackException("Can't create pack file", _path);
    }

    // A constructor that throws runs no destructor, so the truncated file it just created has to be cleared
    // here or a later mount finds a pack with no valid header and refuses to start
    auto remove_on_fail = scope_fail([this]() noexcept {
        _file.close();
        (void)fs::remove_file(_path);
    });

    // The header is patched at the end, once the index offset and the body hash are known
    array<uint8_t, RESOURCE_PACK_HEADER_SIZE> placeholder = {};

    if (!_file.write(const_span<uint8_t> {placeholder.data(), placeholder.size()})) {
        throw ResourcePackException("Can't write pack header placeholder", _path);
    }

    _bodyHash = RESOURCE_PACK_HASH_SEED;
    _bodyOffset = RESOURCE_PACK_HEADER_SIZE;
}

ResourcePackWriter::~ResourcePackWriter()
{
    FO_STACK_TRACE_ENTRY();

    // An abandoned writer leaves no half-written pack behind for a later mount to trip over
    if (!_finished && _file) {
        _file.close();
        (void)fs::remove_file(_path);
    }
}

void ResourcePackWriter::AddFile(string_view path, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_finished && !_failed, "Pack writer is not accepting entries", _path, path);
    FO_VERIFY_AND_THROW(!path.empty(), "Pack entry path is empty", _path);

    uint32_t codec = 0;
    vector<uint8_t> blob = EncodeResourceBlob(data, _settings, codec);

    ResourcePackEntryRef entry;
    entry.Path = strex(path).normalize_path_slashes();
    FO_VERIFY_AND_THROW(IsResourcePathCanonical(entry.Path), "Resource path is not canonical", entry.Path);
    entry.DataOffset = _bodyOffset;
    entry.StoredSize = numeric_cast<uint64_t>(blob.size());
    entry.DecodedSize = numeric_cast<uint64_t>(data.size());
    entry.Codec = codec;
    entry.FileContentHash = HashResourceBytes(RESOURCE_PACK_HASH_SEED, data);

    WriteBody(const_span<uint8_t> {blob.data(), blob.size()});
    _entries.push_back(std::move(entry));
}

void ResourcePackWriter::WriteBody(const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(data.size() <= std::numeric_limits<uint64_t>::max() - _bodyOffset, "Resource base size overflow", _path);
    _failed = true;

    if (!_file.write(data)) {
        throw ResourcePackException("Can't write pack body", _path, data.size());
    }

    _bodyHash = HashResourceBytes(_bodyHash, data);
    _bodyOffset += numeric_cast<uint64_t>(data.size());
    _failed = false;
}

void ResourcePackWriter::Finish()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_finished && !_failed, "Pack writer is not accepting a commit", _path);

    std::sort(_entries.begin(), _entries.end(), [](const ResourcePackEntryRef& left, const ResourcePackEntryRef& right) { return left.Path < right.Path; });

    for (size_t i = 1; i < _entries.size(); ++i) {
        FO_VERIFY_AND_THROW(_entries[i - 1].Path != _entries[i].Path, "Pack holds the same path twice", _path, _entries[i].Path);
    }

    uint64_t data_size = _bodyOffset - RESOURCE_PACK_HEADER_SIZE;
    vector<uint8_t> index = BuildIndexBytes(_entries);

    uint32_t index_codec = 0;
    vector<uint8_t> stored_index = EncodeResourceBlob(const_span<uint8_t> {index.data(), index.size()}, _settings, index_codec);

    ResourcePackHeader header;
    header.VersionMajor = RESOURCE_PACK_VERSION_MAJOR;
    header.VersionMinor = RESOURCE_PACK_VERSION_MINOR;
    header.IndexOffset = _bodyOffset;
    header.IndexStoredSize = numeric_cast<uint64_t>(stored_index.size());
    header.IndexDecodedSize = numeric_cast<uint64_t>(index.size());
    header.IndexCodec = index_codec;
    header.EntryCount = numeric_cast<uint32_t>(_entries.size());
    header.DataOffset = RESOURCE_PACK_HEADER_SIZE;
    header.DataSize = data_size;
    header.ContentHash = ComputeResourcePackContentHash(_entries);

    WriteBody(const_span<uint8_t> {stored_index.data(), stored_index.size()});
    header.PackHash = _bodyHash;

    _failed = true;
    array<uint8_t, RESOURCE_PACK_HEADER_SIZE> header_bytes = {};
    BuildHeaderBytes(header, span<uint8_t> {header_bytes.data(), header_bytes.size()});

    if (!_file.seek_to_begin() || !_file.write(const_span<uint8_t> {header_bytes.data(), header_bytes.size()})) {
        throw ResourcePackException("Can't write pack header", _path);
    }

    if (!_file.flush()) {
        throw ResourcePackException("Can't flush pack file", _path);
    }

    _file.close();
    _finished = true;
}

ResourcePackSource::ResourcePackSource(string_view path, string_view patch_path) :
    _fileName {path},
    _file {OpenResourcePackFile(path)}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_file, "Can't open resource pack file", path);
    array<uint8_t, RESOURCE_PACK_HEADER_SIZE> bytes {};
    bool header_read = _file.get_size() >= bytes.size() && _file.read_at(0, bytes) && ParseResourcePackHeader(bytes, _header);
    FO_VERIFY_AND_THROW(header_read, "Invalid resource pack header", path);
    _writeTime = GetResourcePackWriteTime(path);
    ParseIndex();

    if (!patch_path.empty()) {
        _patchFile = fs::disk_read_file {patch_path};

        if (_patchFile) {
            ResourcePatchInfo info;
            vector<ResourcePackEntryRef> entries;

            if (ReadPatchCatalog(_patchFile, _header, info, entries)) {
                _patchInfo = info;
                _header.ContentHash = info.ContentHash;
                _entries = std::move(entries);
                _writeTime = fs::last_write_time(patch_path);
            }
            else {
                _patchFile.close();
            }
        }
    }

    _entryLookup.reserve(_entries.size());

    for (size_t i = 0; i < _entries.size(); ++i) {
        _entryLookup.emplace(_entries[i].Path, i);
    }
}

void ResourcePackSource::ParseIndex()
{
    FO_STACK_TRACE_ENTRY();

    uint64_t file_size = _file.get_size();
    FO_VERIFY_AND_THROW(_header.DataOffset == RESOURCE_PACK_HEADER_SIZE && _header.DataSize <= file_size - RESOURCE_PACK_HEADER_SIZE, "Invalid resource pack payload extent", _fileName);
    FO_VERIFY_AND_THROW(_header.IndexOffset == _header.DataOffset + _header.DataSize && _header.IndexOffset <= file_size && _header.IndexStoredSize == file_size - _header.IndexOffset, "Invalid resource pack catalog extent", _fileName);
    FO_VERIFY_AND_THROW(_header.IndexStoredSize <= std::numeric_limits<uint32_t>::max() && _header.IndexDecodedSize <= std::numeric_limits<uint32_t>::max(), "Resource catalog exceeds its size limit", _fileName);
    vector<uint8_t> stored(numeric_cast<size_t>(_header.IndexStoredSize));
    bool catalog_read = _file.read_at(_header.IndexOffset, stored);
    FO_VERIFY_AND_THROW(catalog_read, "Can't read resource pack catalog", _fileName);
    _entries = DecodeResourcePackIndex(stored, _header);
}

auto ResourcePackSource::FindEntry(string_view path) const -> nptr<const ResourcePackEntryRef>
{
    FO_STACK_TRACE_ENTRY();

    auto it = _entryLookup.find(path);

    if (it == _entryLookup.end()) {
        return nullptr;
    }

    return make_ptr(&_entries[it->second]);
}

auto ResourcePackSource::ReadEntryData(const ResourcePackEntryRef& entry) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const fs::disk_read_file& file = entry.Source == 0 ? _file : _patchFile;
    vector<uint8_t> stored(numeric_cast<size_t>(entry.StoredSize));
    bool payload_read = file.read_at(entry.DataOffset, stored);
    FO_VERIFY_AND_THROW(payload_read, "Can't read resource pack payload", _fileName, entry.Path);
    return DecodeResourceData(stored, entry);
}

auto ResourcePackSource::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !!FindEntry(path);
}

auto ResourcePackSource::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto entry = FindEntry(path);

    if (!entry) {
        return false;
    }

    size = numeric_cast<size_t>(entry->DecodedSize);
    write_time = _writeTime;
    return true;
}

auto ResourcePackSource::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto entry = FindEntry(path);

    if (!entry) {
        return nullptr;
    }

    vector<uint8_t> data = ReadEntryData(*entry);
    auto buf = safe_alloc::make_unique_arr<uint8_t>(data.size());
    memory::copy(buf.get(), data.data(), data.size());

    size = data.size();
    write_time = _writeTime;
    return MakeFileBufferHolder(std::move(buf));
}

auto ResourcePackSource::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string_view> names;
    names.reserve(_entries.size());

    for (const ResourcePackEntryRef& entry : _entries) {
        names.emplace_back(entry.Path);
    }

    return GetFileNamesGeneric(names, dir, recursive, ext);
}

auto ResourcePackSource::GetEntryRefs() const -> vector<ResourcePackEntryRef>
{
    FO_STACK_TRACE_ENTRY();

    return _entries;
}

auto ResourcePackSource::GetIndexSnapshot() const -> optional<vector<IndexedFile>>
{
    FO_STACK_TRACE_ENTRY();

    vector<IndexedFile> snapshot;
    snapshot.reserve(_entries.size());

    for (const ResourcePackEntryRef& entry : _entries) {
        snapshot.emplace_back(IndexedFile {string(entry.Path), numeric_cast<size_t>(entry.DecodedSize), _writeTime});
    }

    return snapshot;
}

auto IsResourcePathCanonical(string_view path) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (path.empty() || !strvex(path).is_valid_utf8() || path.find('\\') != string_view::npos || path.find(':') != string_view::npos || path.find('\0') != string_view::npos) {
        return false;
    }

    size_t begin = 0;

    while (begin <= path.size()) {
        size_t end = path.find('/', begin);
        string_view component = path.substr(begin, end == string_view::npos ? path.size() - begin : end - begin);

        if (component.empty() || component == "." || component == "..") {
            return false;
        }
        if (end == string_view::npos) {
            return true;
        }

        begin = end + 1;
    }

    return false;
}

auto GetResourcePatchPath(string_view base_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_path.ends_with(".fores"), "Resource base path has no pack extension", base_path);
    return strex("{}.patch.fores", base_path.substr(0, base_path.size() - 6)).str();
}

auto ComputeResourcePackContentHash(const_span<ResourcePackEntryRef> entries) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    array<uint8_t, 20> fields {};
    span_write_uint32(fields, 0, numeric_cast<uint32_t>(entries.size()));
    uint64_t hash = HashResourceBytes(RESOURCE_PACK_HASH_SEED, const_span<uint8_t> {fields.data(), 4});

    for (const ResourcePackEntryRef& entry : entries) {
        span_write_uint32(fields, 0, numeric_cast<uint32_t>(entry.Path.size()));
        span_write_uint64(fields, 4, entry.DecodedSize);
        span_write_uint64(fields, 12, entry.FileContentHash);
        hash = HashResourceBytes(hash, fields);
        hash = HashResourceBytes(hash, {reinterpret_cast<const uint8_t*>(entry.Path.data()), entry.Path.size()});
    }

    return hash;
}

static auto BuildIndexBytes(const_span<ResourcePackEntryRef> entries) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    uint64_t size = numeric_cast<uint64_t>(entries.size()) * RESOURCE_PACK_ENTRY_SIZE;

    string_view previous;

    for (const ResourcePackEntryRef& entry : entries) {
        FO_VERIFY_AND_THROW(IsResourcePathCanonical(entry.Path) && (previous.empty() || previous < entry.Path), "Resource catalog paths are not canonical, unique and sorted", entry.Path);
        previous = entry.Path;
        FO_VERIFY_AND_THROW(entry.Path.size() <= std::numeric_limits<uint32_t>::max() && size <= std::numeric_limits<uint32_t>::max() - entry.Path.size(), "Resource catalog exceeds its string pool limit");
        size += entry.Path.size();
    }

    vector<uint8_t> index(numeric_cast<size_t>(size));
    size_t pool_offset = entries.size() * RESOURCE_PACK_ENTRY_SIZE;

    for (size_t i = 0; i < entries.size(); ++i) {
        const ResourcePackEntryRef& entry = entries[i];
        size_t offset = i * RESOURCE_PACK_ENTRY_SIZE;
        span_write_uint32(index, offset + ENTRY_OFFSET_PATH_OFFSET, numeric_cast<uint32_t>(pool_offset));
        span_write_uint32(index, offset + ENTRY_OFFSET_PATH_LENGTH, numeric_cast<uint32_t>(entry.Path.size()));
        span_write_uint64(index, offset + ENTRY_OFFSET_DATA_OFFSET, entry.DataOffset);
        span_write_uint64(index, offset + ENTRY_OFFSET_STORED_SIZE, entry.StoredSize);
        span_write_uint64(index, offset + ENTRY_OFFSET_DECODED_SIZE, entry.DecodedSize);
        span_write_uint32(index, offset + ENTRY_OFFSET_CODEC, entry.Codec);
        span_write_uint32(index, offset + ENTRY_OFFSET_FLAGS, entry.Source);
        span_write_uint64(index, offset + ENTRY_OFFSET_CONTENT_HASH, entry.FileContentHash);
        memory::copy(index.data() + pool_offset, entry.Path.data(), entry.Path.size());
        pool_offset += entry.Path.size();
    }

    return index;
}

auto DecodeResourcePackIndex(const_span<uint8_t> stored, const ResourcePackHeader& header, uint64_t patch_data_end) -> vector<ResourcePackEntryRef>
{
    FO_STACK_TRACE_ENTRY();

    uint64_t pool_begin = numeric_cast<uint64_t>(header.EntryCount) * RESOURCE_PACK_ENTRY_SIZE;
    FO_VERIFY_AND_THROW(header.IndexStoredSize == stored.size() && pool_begin <= header.IndexDecodedSize && header.IndexDecodedSize <= std::numeric_limits<uint32_t>::max(), "Invalid resource catalog size", header.IndexDecodedSize, header.EntryCount);
    vector<uint8_t> index;

    if (header.IndexCodec == static_cast<uint32_t>(ResourcePackCodec::Stored)) {
        FO_VERIFY_AND_THROW(stored.size() == header.IndexDecodedSize, "Stored resource catalog size mismatch");
        index.assign(stored.begin(), stored.end());
    }
    else {
        FO_VERIFY_AND_THROW(header.IndexCodec == static_cast<uint32_t>(ResourcePackCodec::Deflate), "Unknown resource catalog codec", header.IndexCodec);
        index = compressor::decompress_exact(stored, numeric_cast<size_t>(header.IndexDecodedSize));
    }

    vector<ResourcePackEntryRef> entries;
    entries.reserve(header.EntryCount);

    for (uint32_t i = 0; i < header.EntryCount; ++i) {
        size_t offset = numeric_cast<size_t>(i) * RESOURCE_PACK_ENTRY_SIZE;
        uint32_t path_offset = span_read_uint32(index, offset + ENTRY_OFFSET_PATH_OFFSET);
        uint32_t path_length = span_read_uint32(index, offset + ENTRY_OFFSET_PATH_LENGTH);
        FO_VERIFY_AND_THROW(path_length > 0 && path_offset >= pool_begin && path_offset <= index.size() && path_length <= index.size() - path_offset, "Resource path lies outside catalog", i);
        ResourcePackEntryRef entry;
        entry.Path.assign(reinterpret_cast<const char*>(index.data() + path_offset), path_length);
        FO_VERIFY_AND_THROW(IsResourcePathCanonical(entry.Path), "Resource path is not canonical", entry.Path);
        FO_VERIFY_AND_THROW(entries.empty() || entries.back().Path < entry.Path, "Resource catalog paths are not unique and sorted", entry.Path);
        entry.DataOffset = span_read_uint64(index, offset + ENTRY_OFFSET_DATA_OFFSET);
        entry.StoredSize = span_read_uint64(index, offset + ENTRY_OFFSET_STORED_SIZE);
        entry.DecodedSize = span_read_uint64(index, offset + ENTRY_OFFSET_DECODED_SIZE);
        entry.Codec = span_read_uint32(index, offset + ENTRY_OFFSET_CODEC);
        entry.Source = span_read_uint32(index, offset + ENTRY_OFFSET_FLAGS);
        entry.FileContentHash = span_read_uint64(index, offset + ENTRY_OFFSET_CONTENT_HASH);
        FO_VERIFY_AND_THROW(entry.Source <= 1 && (entry.Source == 0 || patch_data_end >= RESOURCE_PATCH_HEADER_SIZE), "Invalid resource source", entry.Path, entry.Source);
        uint64_t begin = entry.Source == 0 ? header.DataOffset : RESOURCE_PATCH_HEADER_SIZE;
        uint64_t extent = entry.Source == 0 ? header.DataSize : patch_data_end - RESOURCE_PATCH_HEADER_SIZE;
        FO_VERIFY_AND_THROW(entry.DataOffset >= begin && entry.StoredSize <= extent && entry.DataOffset - begin <= extent - entry.StoredSize, "Resource extent lies outside payload", entry.Path);
        FO_VERIFY_AND_THROW(entry.Codec <= static_cast<uint32_t>(ResourcePackCodec::Deflate) && (entry.Codec != 0 || entry.StoredSize == entry.DecodedSize), "Invalid resource codec or lengths", entry.Path);
        entries.emplace_back(std::move(entry));
    }

    FO_VERIFY_AND_THROW(ComputeResourcePackContentHash(entries) == header.ContentHash, "Resource catalog content hash mismatch");
    return entries;
}

static auto DecodeResourceData(const_span<uint8_t> stored, const ResourcePackEntryRef& entry) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(stored.size() == entry.StoredSize, "Resource payload length mismatch", entry.Path);
    vector<uint8_t> data;

    if (entry.Codec == 0) {
        data.assign(stored.begin(), stored.end());
    }
    else {
        FO_VERIFY_AND_THROW(entry.Codec == static_cast<uint32_t>(ResourcePackCodec::Deflate), "Unknown resource payload codec", entry.Path, entry.Codec);
        data = compressor::decompress_exact(stored, numeric_cast<size_t>(entry.DecodedSize));
    }

    FO_VERIFY_AND_THROW(data.size() == entry.DecodedSize && HashResourceBytes(RESOURCE_PACK_HASH_SEED, data) == entry.FileContentHash, "Resource payload content hash mismatch", entry.Path);
    return data;
}

static auto IsEncodedResourceIntact(const_span<uint8_t> stored, const ResourcePackEntryRef& entry) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Decoding is the only check a payload has, and it answers with an exception; here the answer is the result
    try {
        (void)DecodeResourceData(stored, entry);
        return true;
    }
    catch (const std::exception& ex) {
        logging::write("Resource pack: stored payload of {} does not decode to its content, {}", entry.Path, ex.what());
        return false;
    }
}

static auto IsPatchHeaderBound(const_span<uint8_t> header, uint64_t base_pack_hash) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return header.size() == RESOURCE_PATCH_HEADER_SIZE && span_read_uint32(header, 0) == PATCH_MAGIC && span_read_uint16(header, 4) == RESOURCE_PACK_VERSION_MAJOR && span_read_uint16(header, 6) == RESOURCE_PACK_VERSION_MINOR && span_read_uint64(header, 8) == base_pack_hash && span_read_uint64(header, 16) == 0 && span_read_uint64(header, 24) == HashResourceBytes(RESOURCE_PACK_HASH_SEED, header.first(24));
}

static auto ReadPatchCatalog(const fs::disk_read_file& file, const ResourcePackHeader& base_header, ResourcePatchInfo& info, vector<ResourcePackEntryRef>& entries) -> bool
{
    FO_STACK_TRACE_ENTRY();

    array<uint8_t, RESOURCE_PATCH_HEADER_SIZE> header {};

    if (file.get_size() < header.size()) {
        return false;
    }

    bool header_read = file.read_at(0, header);
    FO_VERIFY_AND_THROW(header_read, "Can't read resource patch header");

    // A torn header commits nothing, like a patch without a footer: the base stays the view and the updater
    // recreates the file. Refusing it would keep the client from starting, and a reinstall does not reach it
    if (!IsPatchHeaderBound(header, base_header.PackHash)) {
        return false;
    }

    auto try_footer = [&](const_span<uint8_t> footer, uint64_t offset) {
        if (span_read_uint32(footer, 0) != PATCH_FOOTER_MAGIC || span_read_uint16(footer, 4) != RESOURCE_PACK_VERSION_MAJOR || span_read_uint16(footer, 6) != RESOURCE_PACK_VERSION_MINOR || span_read_uint64(footer, 8) != base_header.PackHash || span_read_uint64(footer, 48) != offset + RESOURCE_PATCH_FOOTER_SIZE || span_read_uint64(footer, 72) != HashResourceBytes(RESOURCE_PACK_HASH_SEED, footer.first(72))) {
            return false;
        }

        ResourcePatchInfo candidate;
        candidate.BasePackHash = base_header.PackHash;
        candidate.ContentHash = span_read_uint64(footer, 16);
        candidate.IndexOffset = span_read_uint64(footer, 24);
        candidate.IndexStoredSize = span_read_uint64(footer, 32);
        candidate.IndexDecodedSize = span_read_uint64(footer, 40);
        candidate.CommittedSize = span_read_uint64(footer, 48);
        candidate.IndexCodec = span_read_uint32(footer, 56);
        candidate.EntryCount = span_read_uint32(footer, 60);
        candidate.IndexHash = span_read_uint64(footer, 64);

        if (candidate.IndexOffset < RESOURCE_PATCH_HEADER_SIZE || candidate.IndexOffset > offset || candidate.IndexStoredSize != offset - candidate.IndexOffset || candidate.IndexDecodedSize > std::numeric_limits<uint32_t>::max() || candidate.IndexStoredSize > std::numeric_limits<uint32_t>::max()) {
            return false;
        }

        vector<uint8_t> stored(numeric_cast<size_t>(candidate.IndexStoredSize));

        if (!file.read_at(candidate.IndexOffset, stored) || HashResourceBytes(RESOURCE_PACK_HASH_SEED, stored) != candidate.IndexHash) {
            return false;
        }

        ResourcePackHeader effective = base_header;
        effective.ContentHash = candidate.ContentHash;
        effective.IndexStoredSize = candidate.IndexStoredSize;
        effective.IndexDecodedSize = candidate.IndexDecodedSize;
        effective.IndexCodec = candidate.IndexCodec;
        effective.EntryCount = candidate.EntryCount;

        try {
            entries = DecodeResourcePackIndex(stored, effective, candidate.IndexOffset);
        }
        catch (const std::exception&) {
            return false;
        }

        info = candidate;
        return true;
    };

    uint64_t end = file.get_size();
    array<uint8_t, RESOURCE_PATCH_FOOTER_SIZE> footer {};

    if (end >= RESOURCE_PATCH_HEADER_SIZE + footer.size() && file.read_at(end - footer.size(), footer) && try_footer(footer, end - footer.size())) {
        return true;
    }

    constexpr uint64_t SCAN_SIZE = 64 * 1024;
    vector<uint8_t> buffer(numeric_cast<size_t>(std::min(end, SCAN_SIZE)));

    while (end >= RESOURCE_PATCH_HEADER_SIZE + RESOURCE_PATCH_FOOTER_SIZE) {
        uint64_t begin = end - std::min(end - RESOURCE_PATCH_HEADER_SIZE, SCAN_SIZE);
        size_t size = numeric_cast<size_t>(end - begin);
        bool window_read = file.read_at(begin, span<uint8_t> {buffer.data(), size});
        FO_VERIFY_AND_THROW(window_read, "Can't read resource patch recovery window", begin, size);

        for (size_t i = size - RESOURCE_PATCH_FOOTER_SIZE + 1; i != 0; --i) {
            const_span<uint8_t> candidate {buffer.data() + i - 1, RESOURCE_PATCH_FOOTER_SIZE};

            if (try_footer(candidate, begin + i - 1)) {
                return true;
            }
        }

        if (begin == RESOURCE_PATCH_HEADER_SIZE) {
            break;
        }

        end = begin + RESOURCE_PATCH_FOOTER_SIZE - 1;
    }

    return false;
}

auto ReadResourcePatchInfo(string_view path, const ResourcePackHeader& base_header) -> optional<ResourcePatchInfo>
{
    FO_STACK_TRACE_ENTRY();

    fs::disk_read_file file = OpenResourcePackFile(path);
    return ReadResourcePatchInfo(file, base_header);
}

auto ReadResourcePatchInfo(const fs::disk_read_file& file, const ResourcePackHeader& base_header) -> optional<ResourcePatchInfo>
{
    FO_STACK_TRACE_ENTRY();

    ResourcePatchInfo info;
    vector<ResourcePackEntryRef> entries;
    return file && ReadPatchCatalog(file, base_header, info, entries) ? optional<ResourcePatchInfo> {info} : std::nullopt;
}

ResourcePatchWriter::ResourcePatchWriter(string_view base_path, string_view patch_path, vector<ResourcePackEntryRef> target_entries, uint64_t content_hash, ResourcePackWriteSettings settings) :
    _basePath {base_path},
    _patchPath {patch_path}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(settings.CompressLevel >= 0 && settings.CompressLevel <= 9 && settings.MinCompressGainPercent >= 0 && settings.MinCompressGainPercent <= 100, "Invalid resource patch compression settings");
    ResourcePackSource base {base_path};
    ResourcePackSource current {base_path, patch_path};
    FO_VERIFY_AND_THROW(ComputeResourcePackContentHash(target_entries) == content_hash, "Patch target content hash mismatch");
    _info.BasePackHash = base.GetPackHash();
    _info.ContentHash = content_hash;
    _hasCommit = current.GetPatchInfo().has_value();
    _startOffset = _hasCommit ? current.GetPatchInfo()->CommittedSize : RESOURCE_PATCH_HEADER_SIZE;
    _startIndexHash = _hasCommit ? current.GetPatchInfo()->IndexHash : 0;
    _originalSize = fs::file_size(patch_path).value_or(0);
    fs::disk_read_file existing {patch_path};

    // Without a commit the file is kept only when its own header binds it to this base: that is a first append cut
    // short, whose payloads the resume below can still use. Anything else is rebuilt from an empty file
    if (!_hasCommit) {
        array<uint8_t, RESOURCE_PATCH_HEADER_SIZE> header {};
        _recreate = !existing || existing.get_size() < header.size() || !existing.read_at(0, header) || !IsPatchHeaderBound(header, _info.BasePackHash);
    }

    map<pair<uint64_t, uint64_t>, ResourcePackEntryRef> available;

    for (ResourcePackEntryRef& entry : base.GetEntryRefs()) {
        available.emplace(pair {entry.FileContentHash, entry.DecodedSize}, std::move(entry));
    }

    for (ResourcePackEntryRef& entry : current.GetEntryRefs()) {
        available.emplace(pair {entry.FileContentHash, entry.DecodedSize}, std::move(entry));
    }

    set<pair<uint64_t, uint64_t>> verified;
    uint64_t offset = _startOffset;

    for (ResourcePackEntryRef& entry : target_entries) {
        auto key = pair {entry.FileContentHash, entry.DecodedSize};
        auto found = available.find(key);

        if (found != available.end() && !verified.contains(key)) {
            try {
                size_t decoded_size = 0;
                uint64_t write_time = 0;
                auto data = (found->second.Source == 0 ? base : current).OpenFile(found->second.Path, decoded_size, write_time);
                FO_VERIFY_AND_THROW(data && decoded_size == entry.DecodedSize, "Reusable resource is missing", entry.Path);
                verified.emplace(key);
            }
            catch (const std::exception& ex) {
                logging::write("Resource patch: repairing damaged local content {}, {}", entry.Path, ex.what());
                available.erase(found);
                found = available.end();
            }
        }

        if (found != available.end()) {
            string path = std::move(entry.Path);
            entry = found->second;
            entry.Path = std::move(path);
        }
        else {
            _downloads.push_back(entry);
            entry.Source = 1;
            entry.DataOffset = offset;
            FO_VERIFY_AND_THROW(entry.StoredSize <= std::numeric_limits<uint64_t>::max() - offset, "Resource patch size overflow");
            offset += entry.StoredSize;
            available.emplace(key, entry);
            verified.emplace(key);
        }
    }

    vector<uint8_t> decoded = BuildIndexBytes(target_entries);
    _info.IndexOffset = offset;
    _info.IndexDecodedSize = decoded.size();
    _info.EntryCount = numeric_cast<uint32_t>(target_entries.size());
    _index = EncodeResourceBlob(decoded, settings, _info.IndexCodec);
    _info.IndexStoredSize = _index.size();
    _info.IndexHash = HashResourceBytes(RESOURCE_PACK_HASH_SEED, _index);
    FO_VERIFY_AND_THROW(offset <= std::numeric_limits<uint64_t>::max() - RESOURCE_PATCH_FOOTER_SIZE && _index.size() <= std::numeric_limits<uint64_t>::max() - RESOURCE_PATCH_FOOTER_SIZE - offset, "Resource patch final size overflow");
    _info.CommittedSize = offset + _index.size() + RESOURCE_PATCH_FOOTER_SIZE;

    if (!_recreate) {
        // An interrupted append left its payloads in exactly this plan's order, so each one that still decodes to its
        // planned entry is kept; the first that does not ends the reusable run, and Begin cuts the tail there
        _keptSize = _startOffset;

        for (const ResourcePackEntryRef& download : _downloads) {
            if (existing.get_size() < _keptSize || existing.get_size() - _keptSize < download.StoredSize) {
                break;
            }

            vector<uint8_t> stored(numeric_cast<size_t>(download.StoredSize));

            if (!existing.read_at(_keptSize, stored) || !IsEncodedResourceIntact(stored, download)) {
                break;
            }

            _keptSize += download.StoredSize;
            ++_resumedDownloads;
        }
    }
}

void ResourcePatchWriter::Begin(const fs::disk_directory_lock& directory_lock)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_file && !_finished && !_failed, "Resource patch writer already started");
    FO_VERIFY_AND_THROW(directory_lock, "Resource patch is written without the resource directory lock", _patchPath);
    _failed = true;
    ResourcePackHeader base;
    bool base_read = ReadResourcePackHeader(_basePath, base);
    FO_VERIFY_AND_THROW(base_read && base.PackHash == _info.BasePackHash, "Resource base changed during patch preparation", _basePath);
    FO_VERIFY_AND_THROW(fs::file_size(_patchPath).value_or(0) == _originalSize, "Resource patch changed during preparation", _patchPath);

    if (_recreate && fs::exists(_patchPath)) {
        bool stale_removed = fs::remove_file(_patchPath);
        FO_VERIFY_AND_THROW(stale_removed, "Can't remove an uncommitted or stale patch", _patchPath);
    }

    _file = fs::disk_write_file {_patchPath, fs::disk_write_mode::append};
    FO_VERIFY_AND_THROW(_file, "Can't open resource patch for appending", _patchPath);

    if (_recreate) {
        array<uint8_t, RESOURCE_PATCH_HEADER_SIZE> header {};
        span_write_uint32(header, 0, PATCH_MAGIC);
        span_write_uint16(header, 4, RESOURCE_PACK_VERSION_MAJOR);
        span_write_uint16(header, 6, RESOURCE_PACK_VERSION_MINOR);
        span_write_uint64(header, 8, _info.BasePackHash);
        span_write_uint64(header, 24, HashResourceBytes(RESOURCE_PACK_HASH_SEED, {header.data(), 24}));
        bool header_written = _file.write(header) && _file.flush();
        FO_VERIFY_AND_THROW(header_written, "Can't initialize resource patch", _patchPath);
    }
    else {
        if (_hasCommit) {
            auto current = ReadResourcePatchInfo(_patchPath, base);
            FO_VERIFY_AND_THROW(current && current->CommittedSize == _startOffset && current->IndexHash == _startIndexHash, "Resource patch commit changed during preparation", _patchPath);
        }

        bool tail_removed = _file.truncate_to(_keptSize);
        FO_VERIFY_AND_THROW(tail_removed, "Can't remove incomplete resource patch tail", _patchPath);
    }

    _nextDownload = _resumedDownloads;
    _failed = false;
}

void ResourcePatchWriter::AddEncodedFile(const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_file && !_failed && !_finished && _nextDownload < _downloads.size(), "Resource patch is not accepting payloads");
    (void)DecodeResourceData(data, _downloads[_nextDownload]);
    _failed = true;
    bool appended = _file.write(data);
    FO_VERIFY_AND_THROW(appended, "Can't append resource patch payload", _patchPath);
    ++_nextDownload;
    _failed = false;
}

void ResourcePatchWriter::Finish()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_file && !_failed && !_finished && _nextDownload == _downloads.size(), "Resource patch is incomplete");
    array<uint8_t, RESOURCE_PATCH_FOOTER_SIZE> footer {};
    span_write_uint32(footer, 0, PATCH_FOOTER_MAGIC);
    span_write_uint16(footer, 4, RESOURCE_PACK_VERSION_MAJOR);
    span_write_uint16(footer, 6, RESOURCE_PACK_VERSION_MINOR);
    span_write_uint64(footer, 8, _info.BasePackHash);
    span_write_uint64(footer, 16, _info.ContentHash);
    span_write_uint64(footer, 24, _info.IndexOffset);
    span_write_uint64(footer, 32, _info.IndexStoredSize);
    span_write_uint64(footer, 40, _info.IndexDecodedSize);
    span_write_uint64(footer, 48, _info.CommittedSize);
    span_write_uint32(footer, 56, _info.IndexCodec);
    span_write_uint32(footer, 60, _info.EntryCount);
    span_write_uint64(footer, 64, _info.IndexHash);
    span_write_uint64(footer, 72, HashResourceBytes(RESOURCE_PACK_HASH_SEED, {footer.data(), 72}));
    _failed = true;
    bool catalog_written = _file.write(_index) && _file.flush();
    FO_VERIFY_AND_THROW(catalog_written, "Can't flush resource patch catalog", _patchPath);
    bool footer_written = _file.write(footer) && _file.flush();
    FO_VERIFY_AND_THROW(footer_written, "Can't commit resource patch footer", _patchPath);
    bool entry_persisted = fs::sync_parent(_patchPath);
    FO_VERIFY_AND_THROW(entry_persisted, "Can't persist resource patch directory entry", _patchPath);
    _file.close();
    _finished = true;
    _failed = false;
}

ResourcePairVerifier::ResourcePairVerifier(string_view base_path, string_view patch_path, bool check_base, bool check_patch) :
    _basePath {base_path},
    _patchPath {patch_path},
    _baseFile {OpenResourcePackFile(base_path)}
{
    FO_STACK_TRACE_ENTRY();

    // A base whose header does not read is no pair at all, and the content check that follows already sends it to a download
    if (!ReadResourcePackHeader(_baseFile, _baseHeader)) {
        logging::write("Resource pack: base {} has no readable header", base_path);
        _baseIntact = false;
        _finished = true;
        return;
    }

    if (check_base) {
        _baseRemaining = true;
        _baseOffset = RESOURCE_PACK_HEADER_SIZE;
        _totalBytes += _baseFile.get_size() - RESOURCE_PACK_HEADER_SIZE;
    }

    if (check_patch) {
        vector<ResourcePackEntryRef> entries;

        try {
            ResourcePackSource pair {base_path, patch_path};

            if (pair.GetPatchInfo().has_value()) {
                entries = pair.GetEntryRefs();
                _patchFile = fs::disk_read_file {patch_path};
            }
        }
        catch (const std::exception& ex) {
            logging::write("Resource pack: base {} does not mount, {}", base_path, ex.what());
            _baseIntact = false;
            _finished = true;
            return;
        }

        // Base bytes are the base check's to prove, and several paths may share one patch extent
        set<pair<uint64_t, uint64_t>> extents;

        for (ResourcePackEntryRef& entry : entries) {
            if (entry.Source == 1 && extents.emplace(entry.DataOffset, entry.StoredSize).second) {
                _totalBytes += entry.StoredSize;
                _patchEntries.emplace_back(std::move(entry));
            }
        }
    }

    _finished = !_baseRemaining && _patchEntries.empty();
}

void ResourcePairVerifier::Step(uint64_t byte_budget)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_finished, "Resource pair verification has already finished", _basePath);

    if (_baseRemaining) {
        StepBase(byte_budget);
    }

    // A damaged base is replaced whole, and its patch goes with it, so the patch is not read behind it
    if (!_baseRemaining && _baseIntact && byte_budget != 0) {
        StepPatch(byte_budget);
    }

    _finished = !_baseIntact || !_patchIntact || (!_baseRemaining && _nextPatchEntry == _patchEntries.size());
}

void ResourcePairVerifier::StepBase(uint64_t& byte_budget)
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t SLICE_SIZE = 1024 * 1024;
    uint64_t file_size = _baseFile.get_size();
    _slice.resize(SLICE_SIZE);

    while (_baseOffset < file_size) {
        size_t chunk = numeric_cast<size_t>(std::min<uint64_t>(file_size - _baseOffset, SLICE_SIZE));

        if (!_baseFile.read_at(_baseOffset, span<uint8_t> {_slice.data(), chunk})) {
            logging::write("Resource pack: can't read base {} at {}", _basePath, _baseOffset);
            _baseIntact = false;
            _baseRemaining = false;
            return;
        }

        _baseHash = HashResourceBytes(_baseHash, const_span<uint8_t> {_slice.data(), chunk});
        _baseOffset += chunk;
        _checkedBytes += chunk;
        byte_budget -= std::min<uint64_t>(byte_budget, chunk);

        if (byte_budget == 0) {
            break;
        }
    }

    if (_baseOffset == file_size) {
        _baseRemaining = false;
        _baseIntact = _baseHash == _baseHeader.PackHash;

        if (!_baseIntact) {
            logging::write("Resource pack: base {} no longer matches the hash its header carries", _basePath);
        }
    }
}

void ResourcePairVerifier::StepPatch(uint64_t& byte_budget)
{
    FO_STACK_TRACE_ENTRY();

    while (_nextPatchEntry < _patchEntries.size()) {
        const ResourcePackEntryRef& entry = _patchEntries[_nextPatchEntry++];
        vector<uint8_t> stored(numeric_cast<size_t>(entry.StoredSize));
        _checkedBytes += entry.StoredSize;

        if (!_patchFile.read_at(entry.DataOffset, stored) || !IsEncodedResourceIntact(stored, entry)) {
            logging::write("Resource pack: committed payload {} of {} is damaged", entry.Path, _patchPath);
            _patchIntact = false;
            return;
        }

        byte_budget -= std::min<uint64_t>(byte_budget, entry.StoredSize);

        if (byte_budget == 0) {
            break;
        }
    }
}

FO_END_NAMESPACE
