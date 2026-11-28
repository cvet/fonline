// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ContentUpdater.h"
#include "ResourcePack.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(UpdaterException);

class Player;
class ContentUpdateSnapshotOwner;

struct ContentUpdateSourceFeedbackPolicy
{
    bool Enabled {};
    uint32_t MinReports {};
    uint32_t FailurePercent {};
    int64_t WindowMilliseconds {};
};

enum class ContentUpdateSourceFeedbackDecision : uint8_t
{
    Ignored,
    Recorded,
};

class ContentUpdateFileStorage final
{
public:
    ContentUpdateFileStorage(string disk_path, shared_ptr<const ContentUpdateSnapshotOwner> snapshot_owner, uint32_t file_id);
    ContentUpdateFileStorage(const ContentUpdateFileStorage&) = delete;
    ContentUpdateFileStorage(ContentUpdateFileStorage&&) noexcept = delete;
    auto operator=(const ContentUpdateFileStorage&) -> ContentUpdateFileStorage& = delete;
    auto operator=(ContentUpdateFileStorage&&) noexcept -> ContentUpdateFileStorage& = delete;
    ~ContentUpdateFileStorage();

    [[nodiscard]] auto GetDiskPath() const noexcept -> string_view { return _diskPath; }
    [[nodiscard]] auto GetSize() const noexcept -> uint64_t { return _size; }
    [[nodiscard]] auto Read(uint64_t offset, span<uint8_t> data) const -> bool;

private:
    string _diskPath;
    shared_ptr<const ContentUpdateSnapshotOwner> _snapshotOwner;
    uint64_t _size {};
    mutable mutex _fileLocker {};
    mutable std::ifstream _file FO_TSA_GUARDED_BY(_fileLocker) {};
};

// Immutable snapshot metadata for one content update artifact
///@ ExportRefType Server RefCounted Export = GetGeneration, GetFileId, GetName, GetTarget, GetSize, GetHash, GetSha256, GetBinaryTargets
class ContentUpdateArtifact final : public refcounted<ContentUpdateArtifact>
{
public:
    ContentUpdateArtifact(uint64_t generation, uint32_t file_id, string name, UpdateFileTarget target, uint64_t size, uint64_t hash, Sha256Digest sha256, vector<string> binary_targets);
    ContentUpdateArtifact(const ContentUpdateArtifact&) = delete;
    ContentUpdateArtifact(ContentUpdateArtifact&&) noexcept = delete;
    auto operator=(const ContentUpdateArtifact&) -> ContentUpdateArtifact& = delete;
    auto operator=(ContentUpdateArtifact&&) noexcept -> ContentUpdateArtifact& = delete;
    ~ContentUpdateArtifact() = default;

    // Catalog generation that owns this artifact
    [[nodiscard]] auto GetGeneration() const noexcept -> uint64_t { return _generation; }
    // Stable file identifier within the catalog generation
    [[nodiscard]] auto GetFileId() const noexcept -> uint32_t { return _fileId; }
    // Client-relative artifact path
    [[nodiscard]] auto GetName() const -> string { return _name; }
    // Artifact destination category
    [[nodiscard]] auto GetTarget() const noexcept -> uint8_t { return static_cast<uint8_t>(_target); }
    // Artifact size in bytes
    [[nodiscard]] auto GetSize() const noexcept -> uint64_t { return _size; }
    // Resource pack hash or legacy content hash
    [[nodiscard]] auto GetHash() const noexcept -> uint64_t { return _hash; }
    // Lowercase SHA-256 digest of the complete artifact
    [[nodiscard]] auto GetSha256() const -> string { return Sha256DigestToHex(_sha256); }
    // Binary targets for which the artifact is published
    [[nodiscard]] auto GetBinaryTargets() const -> vector<string> { return _binaryTargets; }

    [[nodiscard]] auto GetTargetNative() const noexcept -> UpdateFileTarget { return _target; }
    [[nodiscard]] auto GetSha256Native() const noexcept -> const Sha256Digest& { return _sha256; }

private:
    uint64_t _generation {};
    uint32_t _fileId {};
    string _name;
    UpdateFileTarget _target {UpdateFileTarget::ClientResources};
    uint64_t _size {};
    uint64_t _hash {};
    Sha256Digest _sha256 {};
    vector<string> _binaryTargets {};
};

class ContentUpdateArtifactLease final
{
public:
    ContentUpdateArtifactLease(uint64_t generation, uint32_t file_id, string name, UpdateFileTarget target, uint64_t size, uint64_t hash, Sha256Digest sha256, vector<string> binary_targets, shared_ptr<const vector<uint8_t>> memory_data, shared_ptr<const ContentUpdateFileStorage> disk_storage);
    ContentUpdateArtifactLease(const ContentUpdateArtifactLease&) = delete;
    ContentUpdateArtifactLease(ContentUpdateArtifactLease&&) noexcept = delete;
    auto operator=(const ContentUpdateArtifactLease&) -> ContentUpdateArtifactLease& = delete;
    auto operator=(ContentUpdateArtifactLease&&) noexcept -> ContentUpdateArtifactLease& = delete;
    ~ContentUpdateArtifactLease() = default;

    [[nodiscard]] auto GetGeneration() const noexcept -> uint64_t { return _generation; }
    [[nodiscard]] auto GetFileId() const noexcept -> uint32_t { return _fileId; }
    [[nodiscard]] auto GetName() const noexcept -> string_view { return _name; }
    [[nodiscard]] auto GetTarget() const noexcept -> UpdateFileTarget { return _target; }
    [[nodiscard]] auto GetSize() const noexcept -> uint64_t { return _size; }
    [[nodiscard]] auto GetHash() const noexcept -> uint64_t { return _hash; }
    [[nodiscard]] auto GetSha256() const noexcept -> const Sha256Digest& { return _sha256; }
    [[nodiscard]] auto GetBinaryTargets() const noexcept -> const vector<string>& { return _binaryTargets; }
    [[nodiscard]] auto Read(uint64_t offset, span<uint8_t> data) const -> bool;

private:
    uint64_t _generation {};
    uint32_t _fileId {};
    string _name;
    UpdateFileTarget _target {UpdateFileTarget::ClientResources};
    uint64_t _size {};
    uint64_t _hash {};
    Sha256Digest _sha256 {};
    vector<string> _binaryTargets {};
    shared_ptr<const vector<uint8_t>> _memoryData {};
    shared_ptr<const ContentUpdateFileStorage> _diskStorage {};
};

class UpdaterBackend final
{
public:
    UpdaterBackend();
    UpdaterBackend(const UpdaterBackend&) = delete;
    UpdaterBackend(UpdaterBackend&&) = delete;
    auto operator=(const UpdaterBackend&) -> UpdaterBackend& = delete;
    auto operator=(UpdaterBackend&&) -> UpdaterBackend& = delete;

    [[nodiscard]] auto GetUpdateDescriptor(string_view binary_target_name, int64_t current_synchronized_time_ms, uint64_t feedback_session_id = 0) -> shared_ptr<const vector<uint8_t>>;
    [[nodiscard]] auto BeginContentUpdateFeedbackSession() -> uint64_t;
    [[nodiscard]] auto GetFastUpdateSessionId() const noexcept -> uint32_t;
    [[nodiscard]] auto IsFastUpdateEnabled() const noexcept -> bool;
    [[nodiscard]] auto GetContentUpdateCatalogGeneration() const noexcept -> uint64_t;
    [[nodiscard]] auto GetContentUpdateCatalog() const -> vector<refcount_ptr<ContentUpdateArtifact>>;
    [[nodiscard]] auto AcquireContentUpdateArtifact(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256) const -> shared_ptr<ContentUpdateArtifactLease>;

    [[nodiscard]] auto UpsertContentUpdateSource(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256, ContentUpdateSource source) -> bool;
    [[nodiscard]] auto RemoveContentUpdateSource(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256, string_view provider, string_view source_key) -> bool;
    [[nodiscard]] auto ClearContentUpdateSources(uint64_t generation, string_view provider) -> bool;
    [[nodiscard]] auto ReportContentUpdateSourceResult(uint64_t generation, uint32_t file_id, const ContentUpdateSourceReportToken& report_token, uint64_t feedback_session_id, ContentUpdateSourceResult result, uint64_t reporter_id, int64_t current_synchronized_time_ms, const ContentUpdateSourceFeedbackPolicy& policy) -> ContentUpdateSourceFeedbackDecision;

    void LoadFromClientResources(const GlobalSettings& settings, string_view server_metadata_version);
    void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);
    void ProcessContentUpdateSourceReport(ptr<Player> player, int64_t current_synchronized_time_ms, const ServerSettings& settings);
    [[nodiscard]] auto ReadFastUpdateChunk(uint32_t file_index, uint32_t chunk_index, vector<uint8_t>& data, uint64_t& chunk_hash) const -> bool;

private:
    struct SourceIdentity
    {
        uint32_t FileId {};
        string Provider;
        string SourceKey;

        [[nodiscard]] auto operator<(const SourceIdentity& other) const noexcept -> bool { return std::tie(FileId, Provider, SourceKey) < std::tie(other.FileId, other.Provider, other.SourceKey); }
    };

    struct SourceFeedbackState
    {
        int64_t WindowStart {};
        map<uint64_t, bool> ReporterFailures {};
        bool ThresholdLogged {};
    };

    struct UpdateFileData
    {
        shared_ptr<const vector<uint8_t>> MemoryData {};
        shared_ptr<const ContentUpdateFileStorage> DiskStorage {};
        uint64_t Size {};
        uint64_t Hash {};
        Sha256Digest Sha256 {};
        ResourcePackHeader PackHeader {};
        vector<uint64_t> ChunkHashes {};
    };

    struct UpdateFileInfo
    {
        uint32_t FileIndex {};
        string ClientPath {};
        UpdateFileTarget Target {UpdateFileTarget::ClientResources};
        vector<string> BinaryTargets {};
    };

    struct CatalogState
    {
        uint64_t Generation {};
        shared_ptr<const ContentUpdateSnapshotOwner> SnapshotOwner {};
        vector<shared_ptr<const UpdateFileData>> UpdateFiles {};
        vector<UpdateFileInfo> CommonUpdateFiles {};
        map<string, vector<UpdateFileInfo>> BinaryTargetUpdateFiles {};
        vector<shared_ptr<const vector<ContentUpdateSource>>> UpdateFileSources {};
        vector<refcount_ptr<ContentUpdateArtifact>> Artifacts {};
        shared_ptr<const vector<uint8_t>> CommonUpdateFilesDesc {};
        shared_ptr<const vector<uint8_t>> CommonSignedUpdateFilesDesc {};
        map<string, shared_ptr<const vector<uint8_t>>> BinaryTargetUpdateFilesDesc {};
        map<string, shared_ptr<const vector<uint8_t>>> BinaryTargetSignedUpdateFilesDesc {};
        vector<ContentUpdateEndpoint> FastUpdateEndpoints {};
        int64_t NextSourceExpiry {};
        uint32_t FastUpdateSessionId {};
        uint32_t FastUpdateChunkSize {};
        bool FastUpdateEnabled {};
        bool SelfHostedFastUpdateEnabled {};
        bool HasSources {};
        bool ManifestSignatureRequired {};
        uint64_t ManifestReleaseSequence {};
        optional<ContentUpdateSigningKey> ManifestSigningKey {};
    };

    [[nodiscard]] auto GetCatalogSnapshot() const noexcept -> shared_ptr<const CatalogState>;
    [[nodiscard]] auto BuildDescriptorSnapshot(const CatalogState& catalog, nptr<const vector<UpdateFileInfo>> platform_files) const -> shared_ptr<const vector<uint8_t>>;
    [[nodiscard]] auto BuildSignedDescriptorSnapshot(const CatalogState& catalog, const shared_ptr<const vector<uint8_t>>& descriptor, string_view binary_target_name) const -> shared_ptr<const vector<uint8_t>>;
    void BuildDescriptorSnapshots(CatalogState& catalog) const;
    void BuildAffectedDescriptorSnapshots(CatalogState& catalog, const vector<uint32_t>& affected_file_ids) const;
    void BuildCommonDescriptorSnapshot(CatalogState& catalog) const;
    void BuildBinaryTargetDescriptorSnapshot(CatalogState& catalog, string_view binary_target_name) const;
    void RecalculateNextSourceExpiry(CatalogState& catalog) const;
    void PruneExpiredSourcesLocked(int64_t current_synchronized_time_ms) FO_TSA_REQUIRES(_catalogLocker);
    [[nodiscard]] auto MakeSourceReportToken(uint64_t feedback_session_id, const CatalogState& catalog, uint32_t file_id, const ContentUpdateSource& source) const -> ContentUpdateSourceReportToken;
    static void VerifyClientResourcesMetadata(const GlobalSettings& settings, string_view server_metadata_version);
    mutable mutex _catalogLocker {};
    shared_ptr<const CatalogState> _catalog FO_TSA_GUARDED_BY(_catalogLocker) {};
    uint64_t _catalogGenerationCounter FO_TSA_GUARDED_BY(_catalogLocker) {};
    uint64_t _sourceFeedbackSessionCounter FO_TSA_GUARDED_BY(_catalogLocker) {};
    Sha256Digest _sourceFeedbackSecret {};
    map<SourceIdentity, SourceFeedbackState> _sourceFeedback FO_TSA_GUARDED_BY(_catalogLocker) {};
};

FO_END_NAMESPACE
