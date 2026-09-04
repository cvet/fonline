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

#pragma once

#include "Common.h"

#include "DataSource.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ResourcePackException);

// The engine resource pack format: a header, the payload blobs and the index over them in one file, carried
// by a `.fores` file. Full contract: Docs/ResourcePackFormat.md
constexpr uint32_t RESOURCE_PACK_MAGIC = 0x53524F46; // "FORS"
constexpr uint16_t RESOURCE_PACK_VERSION_MAJOR = 1;
constexpr uint16_t RESOURCE_PACK_VERSION_MINOR = 0;
constexpr size_t RESOURCE_PACK_HEADER_SIZE = 72;
constexpr size_t RESOURCE_PACK_ENTRY_SIZE = 40;

// A blob and an index section each choose their own storage, so data that will not shrink is never deflated
// and never pays a decode on the way back
enum class ResourcePackCodec : uint32_t
{
    Stored = 0,
    Deflate = 1,
};

// The header comes first in the file, so a pack identity is one small read away
struct ResourcePackHeader
{
    uint64_t PackHash {}; // Over [RESOURCE_PACK_HEADER_SIZE, end of file): the identity the updater compares
    uint64_t IndexOffset {};
    uint64_t IndexStoredSize {};
    uint64_t IndexDecodedSize {};
    uint64_t DataOffset {};
    uint64_t DataSize {};
    uint32_t IndexCodec {};
    uint32_t EntryCount {};
    uint16_t VersionMajor {};
    uint16_t VersionMinor {};
};

struct ResourcePackWriteSettings
{
    int32_t CompressLevel {6};
    // A blob is deflated only when it gives back at least this much, so the decode cost buys something
    int32_t MinCompressGainPercent {5};
};

// Reads only the header, without touching the index or the payloads
auto ReadResourcePackHeader(string_view path, ResourcePackHeader& header) noexcept -> bool;

// Builds a pack by streaming: every blob is encoded and written as it arrives, the index is appended at the
// end, and the header is patched last once the offsets and the body hash are known
class ResourcePackWriter final
{
public:
    ResourcePackWriter() = delete;
    explicit ResourcePackWriter(string_view path, ResourcePackWriteSettings settings = {});
    ResourcePackWriter(const ResourcePackWriter&) = delete;
    ResourcePackWriter(ResourcePackWriter&&) noexcept = delete;
    auto operator=(const ResourcePackWriter&) = delete;
    auto operator=(ResourcePackWriter&&) noexcept = delete;
    ~ResourcePackWriter();

    void AddFile(string_view path, const_span<uint8_t> data);
    void Finish();

private:
    struct Entry
    {
        string Path;
        uint64_t DataOffset {};
        uint64_t StoredSize {};
        uint64_t DecodedSize {};
        uint32_t Codec {};
    };

    void WriteBody(const_span<uint8_t> data);

    string _path;
    ResourcePackWriteSettings _settings;
    disk_write_file _file;
    vector<Entry> _entries {};
    uint64_t _bodyHash {};
    uint64_t _bodyOffset {};
    bool _finished {};
};

// A mounted pack. The index is resident after one contiguous read, and every payload read is positional, so
// concurrent opens never queue behind a shared file cursor
class ResourcePackSource final : public DataSource
{
public:
    ResourcePackSource() = delete;
    explicit ResourcePackSource(string_view path);
    ResourcePackSource(const ResourcePackSource&) = delete;
    ResourcePackSource(ResourcePackSource&&) noexcept = delete;
    auto operator=(const ResourcePackSource&) = delete;
    auto operator=(ResourcePackSource&&) noexcept = delete;
    ~ResourcePackSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _packName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override;
    [[nodiscard]] auto GetPackHash() const noexcept -> uint64_t { return _header.PackHash; }

private:
    struct FileEntry
    {
        string_view Path; // Points into _index, which outlives every entry
        uint64_t DataOffset {};
        uint64_t StoredSize {};
        uint64_t DecodedSize {};
        uint32_t Codec {};
    };

    void ParseIndex();
    auto FindEntry(string_view path) const -> nptr<const FileEntry>;
    auto ReadEntryData(const FileEntry& entry) const -> vector<uint8_t>;

    string _fileName;
    string _packName;
    disk_read_file _file;
    ResourcePackHeader _header {};
    vector<uint8_t> _index {};
    vector<FileEntry> _entries {};
    unordered_map<string_view, size_t> _entryLookup {};
    uint64_t _writeTime {};
};

FO_END_NAMESPACE
