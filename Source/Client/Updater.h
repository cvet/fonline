// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "ClientConnection.h"
#include "ContentUpdateTransport.h"
#include "ContentUpdater.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "FontManager.h"
#include "ResourceManager.h"
#include "ResourcePack.h"
#include "Settings.h"
#include "SpriteManager.h"
#include "UpdaterFastClient.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ClientUpdateException);

enum class UpdaterResult : uint8_t
{
    ResourcesReady = 0, // Gameplay compat OK; resources are now in sync, caller may start the game
    BinariesStaged = 1, // Gameplay compat outdated; native modules are ready on disk, caller must reload
    PlatformUnsupported = 2, // Compat outdated and CanSelfUpdateNativeModules() == false (Web / iOS / Android)
    ServerMissingNativeUpdate = 3, // Compat outdated but server has no binaries for our target — config bug
    UpdaterOutdated = 4, // FO_UPDATER_VERSION mismatch; protocol is unusable
    Failed = 5, // Any other failure of this client: disk, bad data, etc
    MetadataMismatch = 6, // Resources are in sync with the descriptor, yet the server runs another metadata version
    ConnectionFailed = 7, // Server unreachable or the connection dropped mid-update; an environment state, not a client defect
    SettingsRestartRequired = 8, // Resources are synchronized; changed engine settings require restart before creating game subsystems
};

auto GetCurrentUpdatePlatform() noexcept -> UpdatePlatform;
auto GetUpdatePlatformName(UpdatePlatform platform) noexcept -> string_view;
auto CanSelfUpdateNativeModules(UpdatePlatform platform) noexcept -> bool;
auto GetCurrentBinaryUpdateTargetName() noexcept -> string_view;
// Where this client keeps the binaries it may replace itself: the writable root when it has one, and
// the executable's own directory otherwise
auto GetClientBinaryDir(string_view user_writable_path) -> string;
auto GetClientRuntimeLivePath() -> string;
auto MakeClientRuntimeStagingPath(string_view runtime_live_path) -> string;
// Empty without a writable root: the binaries then sit next to the exe and are replaced in place
auto MakeClientRuntimeBootstrapPath(string_view user_writable_path) -> optional<string>;
auto ResolveClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view expected_runtime_file_name, string_view fallback_runtime_path) -> string;
auto ReadClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view expected_runtime_file_name) -> optional<string>;
auto WriteClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view runtime_path, string_view expected_runtime_file_name) -> bool;
auto MakeClientRuntimeAuthorizationPath(string_view runtime_live_path) -> string;
auto GetCurrentClientRuntimeLibraryName() -> string;
void PromoteStagedRuntimeCompanions(string_view binary_dir) noexcept;
// Whether a terminal result says this client is broken. A server that is down, restarting or
// unreachable from the player's network is not, so it is deliberately kept out of the crash reporter
auto IsUpdaterFailureReportable(UpdaterResult result) noexcept -> bool;
void ShowUpdaterFailure(UpdaterResult result);
auto GetClientRuntimeLibraryExtension() noexcept -> string_view;

class Updater final
{
public:
    Updater() = delete;
    Updater(ptr<GlobalSettings> settings, ptr<IAppWindow> window);
    Updater(const Updater&) = delete;
    Updater(Updater&&) noexcept = delete;
    auto operator=(const Updater&) = delete;
    auto operator=(Updater&&) noexcept = delete;
    ~Updater();

    [[nodiscard]] auto IsFinished() const noexcept -> bool { return _fileListReceived && _filesToUpdate.empty(); }
    [[nodiscard]] auto IsAborted() const noexcept -> bool { return _aborted; }
    [[nodiscard]] auto GetResult() const noexcept -> UpdaterResult { return _result.value_or(UpdaterResult::Failed); }
    [[nodiscard]] auto GetRuntimeLivePath() const -> string;

    // One iteration of network processing + UI rendering. Returns true once the updater
    // reached a terminal state and the caller should inspect GetResult()
    auto Process() -> bool;

private:
    enum class FinalizeResult : uint8_t
    {
        Succeeded,
        IntegrityMismatch,
        FileSystemFailure,
    };

    struct UpdateFile
    {
        int32_t Index {};
        size_t ManifestIndex {};
        string Name {};
        uint64_t Size {};
        uint64_t RemaningSize {};
        uint64_t Hash {};
        Sha256Digest Sha256 {};
        size_t NextExternalSource {};
        bool IsClientBinary {};
        bool UseFastUpdate {};
        bool DirectRetryAttempted {};
        bool Sha256AlreadyVerified {};
        bool RetriedWholeFile {};
        bool TryPatch {true};
        ResourcePackHeader PackHeader {};
    };

    enum class ResourceRange
    {
        None,
        Catalog,
        Payload,
    };

    // The run holds the writable resource directory from the first stage to its result, so a second client started on
    // the same folder waits for this one instead of failing or interleaving with it
    enum class Stage : uint8_t
    {
        WaitingForDirectory,
        VerifyingResources,
        Synchronizing,
    };

    enum class LocalDamage : uint8_t
    {
        Base,
        Patch,
    };

    // What one verified file was when it was proved intact, so a later run can skip reading it again
    struct VerifiedFileIdentity
    {
        uint64_t Size {};
        uint64_t WriteTime {};
        uint64_t Content {};
    };

    struct PackVerification
    {
        string PackName {};
        string BasePath {};
        string PatchPath {};
        optional<VerifiedFileIdentity> BaseIdentity {};
        optional<VerifiedFileIdentity> PatchIdentity {};
        unique_nptr<ResourcePairVerifier> Verifier {};
    };

    void AddText(string_view text);
    void Abort(UpdaterResult result, string_view text);
    void TryStart();
    void StartSynchronization();
    void PlanResourceVerification();
    void ProcessResourceVerification();
    void FinishPackVerification(const PackVerification& verification);
    void GetNextFile();
    void FinishResourcesUpdate();
    void RebuildResourceIndex() const;
    auto ReadLocalMetadataVersion() const -> string;
    void RecoverInterruptedReplacements() const;
    void RemoveStaleTempPacks() const;
    void ProcessExternalUpdate();
    [[nodiscard]] auto TryStartExternalUpdate(UpdateFile& update_file) -> bool;
    void FallbackFromExternalUpdate(UpdateFile& update_file, ContentUpdateSourceResult result, string_view reason);
    void ReportExternalSourceResult(const UpdateFile& update_file, const ContentUpdateSource& source, ContentUpdateSourceResult result);
    void ProcessFastUpdate();
    void FallbackFromFastUpdate(UpdateFile& update_file, string_view reason);
    void RequestUpdateFile(const UpdateFile& update_file);
    void RequestResourceRange();
    void FinishResourceRange();
    void AdvanceResourcePatch();
    auto IsLocalResourceCurrent(const UpdateFile& file) const -> bool;
    auto IsIdentityVerified(string_view path, const VerifiedFileIdentity& identity) const -> bool;
    void RecordVerifiedIdentity(string_view path, const VerifiedFileIdentity& identity);
    void RecordVerifiedBase(string_view base_path);
    void RecordVerifiedPatch(string_view pack_name);

    [[nodiscard]] auto FinalizeCurrentFile() -> FinalizeResult;
    [[nodiscard]] auto MakeFileOutputDir(const UpdateFile& update_file) const -> string;
    [[nodiscard]] auto MakeTempPath(const UpdateFile& update_file) const -> string;
    [[nodiscard]] auto MakeExternalCandidatePath(const UpdateFile& update_file, size_t source_index) const -> string;
    void CleanupStaleTempFiles(const UpdateFile& update_file, string_view current_temp_path) const;
    [[nodiscard]] auto MakeLivePath(const UpdateFile& update_file) const -> string;
    [[nodiscard]] auto MakeFinalPath(const UpdateFile& update_file) const -> string;
    [[nodiscard]] auto TryPromoteStagedBinary(const UpdateFile& update_file, string_view staged_path) -> bool;

    void Net_OnConnect(ClientConnection::ConnectResult result);
    void Net_OnDisconnect();
    void Net_OnInitData();
    void Net_OnTimeSync();
    void Net_OnHashList();
    void Net_OnUpdateFileData();

    auto IsDiskFileHashMatch(string_view file_path, uint64_t expected_size, uint64_t expected_hash) -> bool;
    [[nodiscard]] auto IsDiskFileSha256Match(string_view file_path, uint64_t expected_size, const Sha256Digest& expected_sha256) const -> bool;
    [[nodiscard]] auto IsExternalSourceExpired(const ContentUpdateSource& source) const -> bool;
    auto IsDownloadedFileHashMatch(string_view file_path, const UpdateFile& update_file) -> bool;

    static auto ReadBaseIdentity(string_view base_path) -> optional<VerifiedFileIdentity>;
    static auto ReadPatchIdentity(string_view patch_path, string_view base_path) -> optional<VerifiedFileIdentity>;
    static auto IsDataHashMatch(const vector<uint8_t>& data, uint64_t expected_size, uint64_t expected_hash) noexcept -> bool;
    static auto IsResourcePackName(string_view file_name) noexcept -> bool;
    static auto GetDiskFileSize(string_view file_path) -> optional<uint64_t>;
    static auto GetUpdateWriteSize(uint64_t remaining_size, size_t received_size) -> size_t;
    static auto ReplaceFileSafely(string_view temp_path, string_view final_path) -> bool;

    ptr<GlobalSettings> _settings;
    ClientConnection _conn;
    CacheStorage _cache;
    string _binaryDir {};
    string _serverMetadataVersion {};
    optional<UpdaterResult> _result {};
    Stage _stage {Stage::WaitingForDirectory};
    nanotime _nextDirectoryAttempt {};
    bool _binariesMode {};
    bool _aborted {};
    bool _fileListReceived {};
    bool _hasMatchingEntries {};
    bool _restartPrompt {};
    bool _waitingNoticeShown {};
    vector<PackVerification> _verifications {};
    size_t _verificationIndex {};
    unordered_map<string, LocalDamage> _damagedPacks {};
    vector<UpdateFile> _filesToUpdate {};
    vector<UpdateFile> _resourceTargets {};
    unique_nptr<ResourcePatchWriter> _patchWriter {};
    ResourceRange _resourceRange {ResourceRange::None};
    uint64_t _rangeOffset {};
    uint64_t _rangeSize {};
    vector<uint8_t> _rangeData {};
    size_t _patchDownloadIndex {};
    unique_nptr<fs::disk_directory_lock> _directoryLock {};
    fs::disk_write_file _tempFile {};
    vector<uint8_t> _updateFileBuf {};
    vector<string> _messages {};
    FileSystem _resources {};
    GameTimer _gameTime;
    EffectManager _effectMngr;
    hash_storage _hashStorage {};
    SpriteManager _sprMngr;
    ResourceManager _resMngr;
    FontManager _fontMngr;
    nanotime _startTime {};
    ContentUpdateManifest _updateManifest {};
    vector<uint8_t> _signedUpdateDescriptor {};
    ContentUpdateTransportRegistry _contentUpdateTransports {};
    unique_nptr<ContentUpdateTransportDownload> _externalUpdate {};
    optional<size_t> _activeExternalSourceIndex {};
    string _externalCandidatePath {};
    set<string> _failedExternalProviders {};
    set<string> _failedExternalTransports {};
    optional<UpdaterFastClient> _fastUpdateClient {};
    shared_ptr<Sprite> _splashPic {};
    size_t _bytesRealReceivedCheckpoint {};
};

FO_END_NAMESPACE
