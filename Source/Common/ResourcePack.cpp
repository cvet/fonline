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

FO_BEGIN_NAMESPACE

static constexpr uint64_t HASH_SEED = UINT64_C(0xcbf29ce484222325);

// FNV-1a 64, the same digest the updater already computes over whole files, applied here to a byte span so a
// pack body can be hashed while it is still being written
static auto HashBytes(uint64_t hash, const_span<uint8_t> data) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr uint64_t prime = UINT64_C(0x100000001b3);

    for (size_t i = 0; i < data.size(); ++i) {
        hash = (hash ^ data[i]) * prime;
    }

    return hash;
}

static void WriteUint16(span<uint8_t> buf, size_t offset, uint16_t value) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    buf[offset + 0] = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

static void WriteUint32(span<uint8_t> buf, size_t offset, uint32_t value) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < 4; ++i) {
        buf[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

static void WriteUint64(span<uint8_t> buf, size_t offset, uint64_t value) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < 8; ++i) {
        buf[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

static auto ReadUint16(const_span<uint8_t> buf, size_t offset) noexcept -> uint16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return static_cast<uint16_t>(buf[offset + 0]) | static_cast<uint16_t>(static_cast<uint16_t>(buf[offset + 1]) << 8);
}

static auto ReadUint32(const_span<uint8_t> buf, size_t offset) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t value = 0;

    for (size_t i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(buf[offset + i]) << (i * 8);
    }

    return value;
}

static auto ReadUint64(const_span<uint8_t> buf, size_t offset) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t value = 0;

    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(buf[offset + i]) << (i * 8);
    }

    return value;
}

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
static constexpr size_t HEADER_OFFSET_CHECKSUM = 64;

static void BuildHeaderBytes(const ResourcePackHeader& header, span<uint8_t> buf) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteUint32(buf, HEADER_OFFSET_MAGIC, RESOURCE_PACK_MAGIC);
    WriteUint16(buf, HEADER_OFFSET_VERSION_MAJOR, header.VersionMajor);
    WriteUint16(buf, HEADER_OFFSET_VERSION_MINOR, header.VersionMinor);
    WriteUint64(buf, HEADER_OFFSET_PACK_HASH, header.PackHash);
    WriteUint64(buf, HEADER_OFFSET_INDEX_OFFSET, header.IndexOffset);
    WriteUint64(buf, HEADER_OFFSET_INDEX_STORED_SIZE, header.IndexStoredSize);
    WriteUint64(buf, HEADER_OFFSET_INDEX_DECODED_SIZE, header.IndexDecodedSize);
    WriteUint32(buf, HEADER_OFFSET_INDEX_CODEC, header.IndexCodec);
    WriteUint32(buf, HEADER_OFFSET_ENTRY_COUNT, header.EntryCount);
    WriteUint64(buf, HEADER_OFFSET_DATA_OFFSET, header.DataOffset);
    WriteUint64(buf, HEADER_OFFSET_DATA_SIZE, header.DataSize);
    WriteUint64(buf, HEADER_OFFSET_CHECKSUM, HashBytes(HASH_SEED, const_span<uint8_t> {buf.data(), HEADER_OFFSET_CHECKSUM}));
}

static auto ParseHeaderBytes(const_span<uint8_t> buf, ResourcePackHeader& header) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (ReadUint32(buf, HEADER_OFFSET_MAGIC) != RESOURCE_PACK_MAGIC) {
        return false;
    }

    uint64_t checksum = HashBytes(HASH_SEED, const_span<uint8_t> {buf.data(), HEADER_OFFSET_CHECKSUM});

    if (ReadUint64(buf, HEADER_OFFSET_CHECKSUM) != checksum) {
        return false;
    }

    header.VersionMajor = ReadUint16(buf, HEADER_OFFSET_VERSION_MAJOR);
    header.VersionMinor = ReadUint16(buf, HEADER_OFFSET_VERSION_MINOR);
    header.PackHash = ReadUint64(buf, HEADER_OFFSET_PACK_HASH);
    header.IndexOffset = ReadUint64(buf, HEADER_OFFSET_INDEX_OFFSET);
    header.IndexStoredSize = ReadUint64(buf, HEADER_OFFSET_INDEX_STORED_SIZE);
    header.IndexDecodedSize = ReadUint64(buf, HEADER_OFFSET_INDEX_DECODED_SIZE);
    header.IndexCodec = ReadUint32(buf, HEADER_OFFSET_INDEX_CODEC);
    header.EntryCount = ReadUint32(buf, HEADER_OFFSET_ENTRY_COUNT);
    header.DataOffset = ReadUint64(buf, HEADER_OFFSET_DATA_OFFSET);
    header.DataSize = ReadUint64(buf, HEADER_OFFSET_DATA_SIZE);
    return true;
}

auto ReadResourcePackHeader(string_view path, ResourcePackHeader& header) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    disk_read_file file {path};

    if (!file || file.get_size() < RESOURCE_PACK_HEADER_SIZE) {
        return false;
    }

    array<uint8_t, RESOURCE_PACK_HEADER_SIZE> buf = {};

    if (!file.read_at(0, span<uint8_t> {buf.data(), buf.size()})) {
        return false;
    }

    return ParseHeaderBytes(const_span<uint8_t> {buf.data(), buf.size()}, header);
}

// Deflates the payload and keeps the result only when it gives back the configured minimum, so data that is
// already compressed is stored as it is and costs nothing to read back
static auto EncodeBlob(const_span<uint8_t> data, const ResourcePackWriteSettings& settings, uint32_t& codec) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    codec = static_cast<uint32_t>(ResourcePackCodec::Stored);

    if (data.size() < 64) {
        return {data.begin(), data.end()};
    }

    vector<uint8_t> compressed = Compressor::Compress(data, settings.CompressLevel);
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
    _file = disk_write_file {_path};

    if (!_file) {
        throw ResourcePackException("Can't create pack file", _path);
    }

    // The header is patched at the end, once the index offset and the body hash are known
    array<uint8_t, RESOURCE_PACK_HEADER_SIZE> placeholder = {};

    if (!_file.write(const_span<uint8_t> {placeholder.data(), placeholder.size()})) {
        throw ResourcePackException("Can't write pack header placeholder", _path);
    }

    _bodyHash = HASH_SEED;
    _bodyOffset = RESOURCE_PACK_HEADER_SIZE;
}

ResourcePackWriter::~ResourcePackWriter()
{
    FO_STACK_TRACE_ENTRY();

    // An abandoned writer leaves no half-written pack behind for a later mount to trip over
    if (!_finished && _file) {
        _file.close();
        (void)fs_remove_file(_path);
    }
}

void ResourcePackWriter::AddFile(string_view path, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_finished, "Pack writer already finished", _path, path);
    FO_VERIFY_AND_THROW(!path.empty(), "Pack entry path is empty", _path);

    uint32_t codec = 0;
    vector<uint8_t> blob = EncodeBlob(data, _settings, codec);

    Entry entry;
    entry.Path = strex(path).normalize_path_slashes();
    entry.DataOffset = _bodyOffset;
    entry.StoredSize = numeric_cast<uint64_t>(blob.size());
    entry.DecodedSize = numeric_cast<uint64_t>(data.size());
    entry.Codec = codec;

    WriteBody(const_span<uint8_t> {blob.data(), blob.size()});
    _entries.push_back(std::move(entry));
}

void ResourcePackWriter::WriteBody(const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    if (!_file.write(data)) {
        throw ResourcePackException("Can't write pack body", _path, data.size());
    }

    _bodyHash = HashBytes(_bodyHash, data);
    _bodyOffset += numeric_cast<uint64_t>(data.size());
}

void ResourcePackWriter::Finish()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_finished, "Pack writer already finished", _path);

    // Sorted paths make the reader lookup a binary search and give the file one canonical byte layout
    std::sort(_entries.begin(), _entries.end(), [](const Entry& left, const Entry& right) { return left.Path < right.Path; });

    for (size_t i = 1; i < _entries.size(); ++i) {
        FO_VERIFY_AND_THROW(_entries[i - 1].Path != _entries[i].Path, "Pack holds the same path twice", _path, _entries[i].Path);
    }

    uint64_t data_size = _bodyOffset - RESOURCE_PACK_HEADER_SIZE;
    size_t pool_size = 0;

    for (const Entry& entry : _entries) {
        pool_size += entry.Path.size();
    }

    vector<uint8_t> index(_entries.size() * RESOURCE_PACK_ENTRY_SIZE + pool_size);
    auto index_span = span<uint8_t> {index.data(), index.size()};
    size_t pool_offset = _entries.size() * RESOURCE_PACK_ENTRY_SIZE;
    size_t entry_offset = 0;

    for (const Entry& entry : _entries) {
        WriteUint32(index_span, entry_offset + 0, numeric_cast<uint32_t>(pool_offset));
        WriteUint32(index_span, entry_offset + 4, numeric_cast<uint32_t>(entry.Path.size()));
        WriteUint64(index_span, entry_offset + 8, entry.DataOffset);
        WriteUint64(index_span, entry_offset + 16, entry.StoredSize);
        WriteUint64(index_span, entry_offset + 24, entry.DecodedSize);
        WriteUint32(index_span, entry_offset + 32, entry.Codec);
        WriteUint32(index_span, entry_offset + 36, 0);

        std::memcpy(index.data() + pool_offset, entry.Path.data(), entry.Path.size());
        pool_offset += entry.Path.size();
        entry_offset += RESOURCE_PACK_ENTRY_SIZE;
    }

    uint32_t index_codec = 0;
    vector<uint8_t> stored_index = EncodeBlob(const_span<uint8_t> {index.data(), index.size()}, _settings, index_codec);

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

    WriteBody(const_span<uint8_t> {stored_index.data(), stored_index.size()});
    header.PackHash = _bodyHash;

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

ResourcePackSource::ResourcePackSource(string_view path) :
    _fileName {path},
    _packName {strex(path).extract_file_name().erase_file_extension()},
    _file {path}
{
    FO_STACK_TRACE_ENTRY();

    if (!_file) {
        throw DataSourceException("Can't open resource pack file", _fileName);
    }

    _writeTime = fs_last_write_time(_fileName);

    if (_file.get_size() < RESOURCE_PACK_HEADER_SIZE) {
        throw DataSourceException("Resource pack file is shorter than its header", _fileName, _file.get_size());
    }

    array<uint8_t, RESOURCE_PACK_HEADER_SIZE> header_bytes = {};

    if (!_file.read_at(0, span<uint8_t> {header_bytes.data(), header_bytes.size()})) {
        throw DataSourceException("Can't read resource pack header", _fileName);
    }
    if (!ParseHeaderBytes(const_span<uint8_t> {header_bytes.data(), header_bytes.size()}, _header)) {
        throw DataSourceException("Resource pack header is not valid", _fileName);
    }

    // A minor bump stays readable by design, a major one changes what the fields mean
    if (_header.VersionMajor != RESOURCE_PACK_VERSION_MAJOR) {
        throw DataSourceException("Resource pack major version is not supported", _fileName, _header.VersionMajor, RESOURCE_PACK_VERSION_MAJOR);
    }

    ParseIndex();
}

void ResourcePackSource::ParseIndex()
{
    FO_STACK_TRACE_ENTRY();

    uint64_t file_size = _file.get_size();

    auto fits_in_file = [file_size](uint64_t offset, uint64_t size) noexcept { return offset >= RESOURCE_PACK_HEADER_SIZE && size <= file_size && offset <= file_size - size; };

    if (!fits_in_file(_header.DataOffset, _header.DataSize) || !fits_in_file(_header.IndexOffset, _header.IndexStoredSize)) {
        throw DataSourceException("Resource pack section lies outside the file", _fileName, file_size, _header.DataOffset, _header.DataSize, _header.IndexOffset, _header.IndexStoredSize);
    }

    uint64_t entries_size = numeric_cast<uint64_t>(_header.EntryCount) * RESOURCE_PACK_ENTRY_SIZE;

    if (_header.IndexDecodedSize < entries_size) {
        throw DataSourceException("Resource pack index is too small for its entry count", _fileName, _header.IndexDecodedSize, _header.EntryCount);
    }

    // One contiguous read brings the whole index in, and it stays resident so every path below points into it
    auto stored_index = vector<uint8_t>(numeric_cast<size_t>(_header.IndexStoredSize));

    if (!_file.read_at(_header.IndexOffset, span<uint8_t> {stored_index.data(), stored_index.size()})) {
        throw DataSourceException("Can't read resource pack index", _fileName, _header.IndexOffset, _header.IndexStoredSize);
    }

    if (_header.IndexCodec == static_cast<uint32_t>(ResourcePackCodec::Stored)) {
        if (_header.IndexStoredSize != _header.IndexDecodedSize) {
            throw DataSourceException("Stored resource pack index declares two different sizes", _fileName, _header.IndexStoredSize, _header.IndexDecodedSize);
        }

        _index = std::move(stored_index);
    }
    else if (_header.IndexCodec == static_cast<uint32_t>(ResourcePackCodec::Deflate)) {
        _index = Compressor::DecompressExact(const_span<uint8_t> {stored_index.data(), stored_index.size()}, numeric_cast<size_t>(_header.IndexDecodedSize));
    }
    else {
        throw DataSourceException("Resource pack index uses an unknown codec", _fileName, _header.IndexCodec);
    }

    auto index_span = const_span<uint8_t> {_index.data(), _index.size()};
    size_t pool_begin = numeric_cast<size_t>(entries_size);
    _entries.reserve(_header.EntryCount);
    _entryLookup.reserve(_header.EntryCount);

    for (uint32_t i = 0; i < _header.EntryCount; ++i) {
        size_t entry_offset = numeric_cast<size_t>(i) * RESOURCE_PACK_ENTRY_SIZE;

        FileEntry entry;
        uint32_t path_offset = ReadUint32(index_span, entry_offset + 0);
        uint32_t path_length = ReadUint32(index_span, entry_offset + 4);
        entry.DataOffset = ReadUint64(index_span, entry_offset + 8);
        entry.StoredSize = ReadUint64(index_span, entry_offset + 16);
        entry.DecodedSize = ReadUint64(index_span, entry_offset + 24);
        entry.Codec = ReadUint32(index_span, entry_offset + 32);

        if (path_length == 0 || path_offset < pool_begin || numeric_cast<uint64_t>(path_offset) + path_length > _index.size()) {
            throw DataSourceException("Resource pack entry path lies outside the string pool", _fileName, i, path_offset, path_length);
        }

        // Checked in an order that cannot overflow: the size fits the region before the offset is added to it
        if (entry.StoredSize > _header.DataSize || entry.DataOffset < _header.DataOffset || entry.DataOffset - _header.DataOffset > _header.DataSize - entry.StoredSize) {
            throw DataSourceException("Resource pack entry extent lies outside the data region", _fileName, i, entry.DataOffset, entry.StoredSize);
        }
        if (entry.Codec == static_cast<uint32_t>(ResourcePackCodec::Stored) && entry.StoredSize != entry.DecodedSize) {
            throw DataSourceException("Stored resource pack entry declares two different sizes", _fileName, i, entry.StoredSize, entry.DecodedSize);
        }
        if (entry.Codec != static_cast<uint32_t>(ResourcePackCodec::Stored) && entry.Codec != static_cast<uint32_t>(ResourcePackCodec::Deflate)) {
            throw DataSourceException("Resource pack entry uses an unknown codec", _fileName, i, entry.Codec);
        }

        entry.Path = string_view {reinterpret_cast<const char*>(_index.data()) + path_offset, path_length};

        if (!_entryLookup.emplace(entry.Path, _entries.size()).second) {
            throw DataSourceException("Resource pack holds the same path twice", _fileName, entry.Path);
        }

        _entries.push_back(entry);
    }
}

auto ResourcePackSource::FindEntry(string_view path) const -> nptr<const FileEntry>
{
    FO_STACK_TRACE_ENTRY();

    auto it = _entryLookup.find(path);

    if (it == _entryLookup.end()) {
        return nullptr;
    }

    return make_ptr(&_entries[it->second]);
}

auto ResourcePackSource::ReadEntryData(const FileEntry& entry) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto stored = vector<uint8_t>(numeric_cast<size_t>(entry.StoredSize));

    if (!_file.read_at(entry.DataOffset, span<uint8_t> {stored.data(), stored.size()})) {
        throw DataSourceException("Can't read file from resource pack", _fileName, entry.Path, entry.DataOffset, entry.StoredSize);
    }

    if (entry.Codec == static_cast<uint32_t>(ResourcePackCodec::Stored)) {
        return stored;
    }

    return Compressor::DecompressExact(const_span<uint8_t> {stored.data(), stored.size()}, numeric_cast<size_t>(entry.DecodedSize));
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
    auto buf = SafeAlloc::MakeUniqueArr<uint8_t>(data.size());
    std::memcpy(buf.get(), data.data(), data.size());

    size = data.size();
    write_time = _writeTime;
    return MakeFileBufferHolder(std::move(buf));
}

auto ResourcePackSource::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> names;
    names.reserve(_entries.size());

    for (const FileEntry& entry : _entries) {
        names.emplace_back(entry.Path);
    }

    return GetFileNamesGeneric(names, dir, recursive, ext);
}

auto ResourcePackSource::GetIndexSnapshot() const -> optional<vector<IndexedFile>>
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
