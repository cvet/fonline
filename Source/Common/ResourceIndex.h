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
#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ResourceIndexException);

// The merged resource tree: one file naming every path the packs present and where its bytes live, carried by
// a `.foindex` file. It holds references only - no payload. Full contract: Docs/ResourcePackFormat.md
constexpr uint32_t RESOURCE_INDEX_MAGIC = 0x58494F46; // "FOIX"
constexpr uint16_t RESOURCE_INDEX_VERSION_MAJOR = 1;
constexpr uint16_t RESOURCE_INDEX_VERSION_MINOR = 0;
constexpr size_t RESOURCE_INDEX_HEADER_SIZE = 72;
constexpr size_t RESOURCE_INDEX_PACK_SIZE = 16;
constexpr size_t RESOURCE_INDEX_ENTRY_SIZE = 40;

// One pack the merged tree draws from. The file records the name rather than a path, so an installed client
// that moved on disk still resolves, and the hash is what proves the resolved file is the one merged
struct ResourceIndexPack
{
    string Name {};
    uint64_t PackHash {};
};

// The parsed form and not the on-disk image: the file is written field by field, so member order and sizeof
// are free here, and the layout is pinned by the static asserts beside the offsets in the source
struct ResourceIndexHeader
{
    uint64_t PackListHash {}; // Over the ordered pack list: the whole divergence question in one field
    uint64_t IndexOffset {};
    uint64_t IndexStoredSize {};
    uint64_t IndexDecodedSize {};
    uint32_t IndexCodec {};
    uint32_t EntryCount {};
    uint32_t PackCount {};
    uint16_t VersionMajor {};
    uint16_t VersionMinor {};
};

// The divergence key. Folds each pack's name and hash in order, so an edited, added, removed or reordered
// pack all change it
auto ComputeResourceIndexPackListHash(const vector<ResourceIndexPack>& packs) noexcept -> uint64_t;
// Header only. A file that cannot be read or does not validate answers false, which the caller treats as a
// rebuild trigger rather than an error
auto ReadResourceIndexHeader(string_view path, ResourceIndexHeader& header) noexcept -> bool;
// Resolves each name against the directories in priority order and reads the hash from each pack's header,
// so deciding whether the index still holds costs one small read per pack instead of a mount
auto ResolveResourceIndexPacks(const vector<string>& pack_dirs, const vector<string>& pack_names, vector<ResourceIndexPack>& packs, vector<string>& pack_paths) noexcept -> bool;

// Merges the packs in the given order, last one winning a shared path, and replaces the index atomically. The
// packs are read and released; nothing is ever written back into a `.fores`
void BuildResourceIndex(string_view path, const vector<string>& pack_paths, const vector<ResourceIndexPack>& packs, ResourcePackWriteSettings settings = {});

// The merged tree as one mounted source. A lookup is one hash probe here and one positional read in the pack
// that owns the bytes; no per-pack index is consulted again
class ResourceIndexSource final : public DataSource
{
public:
    ResourceIndexSource() = delete;
    explicit ResourceIndexSource(string_view path, const vector<string>& pack_dirs);
    ResourceIndexSource(const ResourceIndexSource&) = delete;
    ResourceIndexSource(ResourceIndexSource&&) noexcept = delete;
    auto operator=(const ResourceIndexSource&) = delete;
    auto operator=(ResourceIndexSource&&) noexcept = delete;
    ~ResourceIndexSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _indexName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override;
    [[nodiscard]] auto GetPackListHash() const noexcept -> uint64_t { return _header.PackListHash; }

private:
    struct FileEntry
    {
        string_view Path; // Points into _index, which outlives every entry
        uint64_t DataOffset {};
        uint64_t StoredSize {};
        uint64_t DecodedSize {};
        uint32_t PackIndex {};
        uint32_t Codec {};
    };

    void ParseIndex(const vector<string>& pack_dirs);
    auto FindEntry(string_view path) const -> nptr<const FileEntry>;
    auto ReadEntryData(const FileEntry& entry) const -> vector<uint8_t>;

    string _fileName;
    string _indexName;
    disk_read_file _file;
    ResourceIndexHeader _header {};
    vector<uint8_t> _index {};
    vector<FileEntry> _entries {};
    unordered_map<string_view, size_t> _entryLookup {};
    vector<disk_read_file> _packFiles {};
    vector<string> _fileNames {};
    uint64_t _writeTime {};
};

FO_END_NAMESPACE
