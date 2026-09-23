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

// Startup recovery sweeps the whole binary directory for it, so it must never be a name a player would pick
constexpr string_view REPLACED_FILE_BACKUP_SUFFIX = ".fobackup";

// The engine resource pack format: a header, the payload blobs and the index over them in one file, carried
// by a `.fores` file
constexpr uint32_t RESOURCE_PACK_MAGIC = 0x53524F46; // "FORS"
constexpr uint16_t RESOURCE_PACK_VERSION_MAJOR = 2;
constexpr uint16_t RESOURCE_PACK_VERSION_MINOR = 0;
constexpr size_t RESOURCE_PACK_HEADER_SIZE = 80;
constexpr size_t RESOURCE_PACK_ENTRY_SIZE = 48;
constexpr size_t RESOURCE_PATCH_HEADER_SIZE = 32;
constexpr size_t RESOURCE_PATCH_FOOTER_SIZE = 80;
// Below this a deflate stream's own header costs more than the stream can save, so nothing smaller is tried
constexpr size_t RESOURCE_PACK_MIN_COMPRESSED_SIZE = 64;

// FNV-1a 64, the digest the updater already computes over whole files, applied to a byte span so a body can
// be hashed while it is still being written
constexpr uint64_t RESOURCE_PACK_HASH_SEED = UINT64_C(0xcbf29ce484222325);
constexpr uint64_t RESOURCE_PACK_HASH_PRIME = UINT64_C(0x100000001b3);

constexpr auto HashResourceBytes(uint64_t hash, const_span<uint8_t> data) noexcept -> uint64_t
{
    for (size_t i = 0; i < data.size(); ++i) {
        hash = (hash ^ data[i]) * RESOURCE_PACK_HASH_PRIME;
    }

    return hash;
}

// A blob and an index section each choose their own storage, so data that will not shrink is never deflated
// and never pays a decode on the way back
enum class ResourcePackCodec : uint32_t
{
    Stored = 0,
    Deflate = 1,
};

// The header comes first in the file, so a pack identity is one small read away. This is the parsed form and
// not the on-disk image: the file is written field by field, so member order and sizeof are free here
struct ResourcePackHeader
{
    uint64_t PackHash {}; // Over [RESOURCE_PACK_HEADER_SIZE, end of file): the identity the updater compares
    uint64_t ContentHash {};
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

// One entry as the pack stores it, so a merged index can point at the bytes without re-reading the pack
struct ResourcePackEntryRef
{
    string Path {};
    uint64_t DataOffset {};
    uint64_t StoredSize {};
    uint64_t DecodedSize {};
    uint32_t Codec {};
    uint32_t Source {};
    uint64_t FileContentHash {};
};

struct ResourcePatchInfo
{
    uint64_t BasePackHash {};
    uint64_t ContentHash {};
    uint64_t IndexOffset {};
    uint64_t IndexStoredSize {};
    uint64_t IndexDecodedSize {};
    uint64_t CommittedSize {};
    uint64_t IndexHash {};
    uint32_t IndexCodec {};
    uint32_t EntryCount {};
};

auto ResolveResourcePackPath(const vector<string>& directories, string_view name) -> string;
auto OpenResourcePackFile(string_view path) noexcept -> fs::disk_read_file;
auto GetResourcePackWriteTime(string_view path) noexcept -> uint64_t;
auto IsResourcePathCanonical(string_view path) noexcept -> bool;
auto GetResourcePatchPath(string_view base_path) -> string;
auto ComputeResourcePackContentHash(const_span<ResourcePackEntryRef> entries) -> uint64_t;
auto SerializeResourcePackHeader(const ResourcePackHeader& header) -> vector<uint8_t>;
auto ParseResourcePackHeader(const_span<uint8_t> data, ResourcePackHeader& header) noexcept -> bool;
auto DecodeResourcePackIndex(const_span<uint8_t> stored, const ResourcePackHeader& header, uint64_t patch_data_end = 0) -> vector<ResourcePackEntryRef>;
auto ReadResourcePatchInfo(string_view path, const ResourcePackHeader& base_header) -> optional<ResourcePatchInfo>;
auto ReadResourcePatchInfo(const fs::disk_read_file& file, const ResourcePackHeader& base_header) -> optional<ResourcePatchInfo>;

// Deflates a blob and keeps the result only when it gives back the configured minimum. Both formats encode
// through this, so an index section and a payload obey one rule
auto EncodeResourceBlob(const_span<uint8_t> data, const ResourcePackWriteSettings& settings, uint32_t& codec) -> vector<uint8_t>;

// Reads only the header, without touching the index or the payloads
auto ReadResourcePackHeader(string_view path, ResourcePackHeader& header) noexcept -> bool;
auto ReadResourcePackHeader(const fs::disk_read_file& file, ResourcePackHeader& header) noexcept -> bool;
// The one place a pack body is hashed: after a download, to prove the file carries the hash it was fetched
// for. The header hash is trusted from then on, so a startup never pays this over gigabytes
auto VerifyResourcePackFile(string_view path, uint64_t expected_pack_hash) noexcept -> bool;

// Builds a pack by streaming: blobs are encoded and written as they arrive, the index is appended at the end,
// and the header is patched last. Add paths sorted for a canonical file - the index sorts, the payloads do not
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
    void WriteBody(const_span<uint8_t> data);

    string _path;
    ResourcePackWriteSettings _settings;
    fs::disk_write_file _file;
    vector<ResourcePackEntryRef> _entries {};
    uint64_t _bodyHash {};
    uint64_t _bodyOffset {};
    bool _finished {};
    bool _failed {};
};

// A mounted pack. The index is resident after one contiguous read, and every payload read is positional, so
// concurrent opens never queue behind a shared file cursor
class ResourcePackSource final : public DataSource
{
public:
    ResourcePackSource() = delete;
    explicit ResourcePackSource(string_view path, string_view patch_path = {});
    ResourcePackSource(const ResourcePackSource&) = delete;
    ResourcePackSource(ResourcePackSource&&) noexcept = delete;
    auto operator=(const ResourcePackSource&) = delete;
    auto operator=(ResourcePackSource&&) noexcept = delete;
    ~ResourcePackSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _fileName; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;
    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override;
    [[nodiscard]] auto GetPackHash() const noexcept -> uint64_t { return _header.PackHash; }
    [[nodiscard]] auto GetContentHash() const noexcept -> uint64_t { return _header.ContentHash; }
    [[nodiscard]] auto GetPatchInfo() const noexcept -> const optional<ResourcePatchInfo>& { return _patchInfo; }
    [[nodiscard]] auto GetEntryRefs() const -> vector<ResourcePackEntryRef>;

private:
    void ParseIndex();
    auto FindEntry(string_view path) const -> nptr<const ResourcePackEntryRef>;
    auto ReadEntryData(const ResourcePackEntryRef& entry) const -> vector<uint8_t>;

    string _fileName;
    fs::disk_read_file _file;
    fs::disk_read_file _patchFile;
    optional<ResourcePatchInfo> _patchInfo {};
    ResourcePackHeader _header {};
    vector<ResourcePackEntryRef> _entries {};
    unordered_map<string_view, size_t> _entryLookup {};
    uint64_t _writeTime {};
};

class ResourcePatchWriter final
{
public:
    ResourcePatchWriter(string_view base_path, string_view patch_path, vector<ResourcePackEntryRef> target_entries, uint64_t content_hash, ResourcePackWriteSettings settings = {});
    ResourcePatchWriter(const ResourcePatchWriter&) = delete;
    ResourcePatchWriter(ResourcePatchWriter&&) = delete;
    auto operator=(const ResourcePatchWriter&) = delete;
    auto operator=(ResourcePatchWriter&&) = delete;
    ~ResourcePatchWriter() = default;

    [[nodiscard]] auto GetDownloads() const noexcept -> const vector<ResourcePackEntryRef>& { return _downloads; }
    // Payloads an interrupted append already wrote in this plan's order: Begin keeps them and the next AddEncodedFile follows them
    [[nodiscard]] auto GetResumedDownloads() const noexcept -> size_t { return _resumedDownloads; }
    [[nodiscard]] auto GetFinalSize() const noexcept -> uint64_t { return _info.CommittedSize; }
    [[nodiscard]] auto GetAppendSize() const noexcept -> uint64_t { return _info.CommittedSize - _keptSize; }

    // The caller holds the directory lock across the whole append, so one lock can cover a longer update session
    void Begin(const fs::disk_directory_lock& directory_lock);
    void AddEncodedFile(const_span<uint8_t> data);
    void Finish();

private:
    string _basePath;
    string _patchPath;
    fs::disk_write_file _file;
    ResourcePatchInfo _info {};
    vector<ResourcePackEntryRef> _downloads {};
    vector<uint8_t> _index {};
    uint64_t _startOffset {};
    uint64_t _startIndexHash {};
    uint64_t _keptSize {};
    uint64_t _originalSize {};
    size_t _resumedDownloads {};
    size_t _nextDownload {};
    bool _hasCommit {};
    bool _recreate {};
    bool _failed {};
    bool _finished {};
};

// Proves that a local pair reads back as what it claims, a bounded slice at a time, so a caller can keep a screen and a
// connection alive over a large pack
class ResourcePairVerifier final
{
public:
    ResourcePairVerifier(string_view base_path, string_view patch_path, bool check_base, bool check_patch);
    ResourcePairVerifier(const ResourcePairVerifier&) = delete;
    ResourcePairVerifier(ResourcePairVerifier&&) = delete;
    auto operator=(const ResourcePairVerifier&) = delete;
    auto operator=(ResourcePairVerifier&&) = delete;
    ~ResourcePairVerifier() = default;

    [[nodiscard]] auto IsFinished() const noexcept -> bool { return _finished; }
    [[nodiscard]] auto IsBaseIntact() const noexcept -> bool { return _baseIntact; }
    [[nodiscard]] auto IsPatchIntact() const noexcept -> bool { return _patchIntact; }
    [[nodiscard]] auto GetCheckedBytes() const noexcept -> uint64_t { return _checkedBytes; }
    [[nodiscard]] auto GetTotalBytes() const noexcept -> uint64_t { return _totalBytes; }

    // Reads at least one slice or payload and stops once the budget is spent, so a large payload may overrun it
    void Step(uint64_t byte_budget);

private:
    void StepBase(uint64_t& byte_budget);
    void StepPatch(uint64_t& byte_budget);

    string _basePath;
    string _patchPath;
    fs::disk_read_file _baseFile;
    fs::disk_read_file _patchFile {};
    ResourcePackHeader _baseHeader {};
    uint64_t _baseOffset {};
    uint64_t _baseHash {RESOURCE_PACK_HASH_SEED};
    vector<ResourcePackEntryRef> _patchEntries {};
    size_t _nextPatchEntry {};
    vector<uint8_t> _slice {};
    uint64_t _checkedBytes {};
    uint64_t _totalBytes {};
    bool _baseRemaining {};
    bool _baseIntact {true};
    bool _patchIntact {true};
    bool _finished {};
};

FO_END_NAMESPACE
