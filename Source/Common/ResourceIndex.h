// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "DataSource.h"
#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ResourceIndexException);

// The merged resource tree: one file naming every path the packs present and where its bytes live, carried by
// a `.foindex` file. It holds references only - no payload
constexpr uint32_t RESOURCE_INDEX_MAGIC {0x58494F46}; // "FOIX"
constexpr uint16_t RESOURCE_INDEX_VERSION_MAJOR {2};
constexpr uint16_t RESOURCE_INDEX_VERSION_MINOR {0};
constexpr size_t RESOURCE_INDEX_HEADER_SIZE {72};
constexpr size_t RESOURCE_INDEX_PACK_SIZE {32};
constexpr size_t RESOURCE_INDEX_ENTRY_SIZE {56};

// The merged tree a client mounts, kept beside the packs under the writable root because it is built there
constexpr string_view RESOURCE_INDEX_FILE_NAME {"Resources.foindex"};

// One pack the merged tree draws from. The file records the name rather than a path, so an installed client
// that moved on disk still resolves, and the hash is what proves the resolved file is the one merged
struct ResourceIndexPack
{
    string Name {};
    uint64_t PackHash {};
    uint64_t PatchHash {};
    uint64_t PatchEnd {};
    string PatchPath {};
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
// Only the suffix after Embedded can be replaced by one disk-backed mount without changing its precedence
auto GetResourceIndexPackNames(const vector<string>& pack_names) -> vector<string>;
// Freshness binds the selected base header and the latest valid patch catalog
auto ResolveResourceIndexPacks(const vector<string>& pack_dirs, const vector<string>& pack_names, vector<ResourceIndexPack>& packs, vector<string>& pack_paths) noexcept -> bool;
auto IsResourceIndexCurrent(string_view path, const vector<string>& pack_dirs, const vector<string>& pack_names) noexcept -> bool;
// Merges the packs in the given order, last one winning a shared path, and publishes the completed index. The
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
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override;
    [[nodiscard]] auto GetPackListHash() const noexcept -> uint64_t { return _header.PackListHash; }

private:
    struct FileEntry
    {
        string_view Path {}; // Points into _index, which outlives every entry
        uint64_t DataOffset {};
        uint64_t StoredSize {};
        uint64_t DecodedSize {};
        uint32_t PackIndex {};
        uint32_t Codec {};
        uint32_t Source {};
        uint64_t FileContentHash {};
    };

    void ParseIndex(const vector<string>& pack_dirs);
    auto FindEntry(string_view path) const -> nptr<const FileEntry>;
    auto ReadEntryData(const FileEntry& entry) const -> vector<uint8_t>;

    string _fileName;
    fs::disk_read_file _file;
    ResourceIndexHeader _header {};
    vector<uint8_t> _index {};
    vector<FileEntry> _entries {};
    unordered_map<string_view, size_t> _entryLookup {};
    vector<fs::disk_read_file> _packFiles {};
    vector<fs::disk_read_file> _patchFiles {};
    // Per pack pair: the committed patch's mtime when there is one, otherwise the base's - the same answer a
    // direct pair mount gives, which is what keeps the two views of one file indistinguishable
    vector<uint64_t> _packWriteTimes {};
};

FO_END_NAMESPACE
