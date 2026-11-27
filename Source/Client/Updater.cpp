// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Updater.h"

#include "Application.h"
#include "Client.h"
#include "DefaultSprites.h"
#include "MetadataRegistration.h"
#include "ResourceDefaults.h"
#include "ResourceIndex.h"
#include "ResourcePack.h"
#include "UpdateDescriptor.h"
#include "WebRelated.h"

FO_BEGIN_NAMESPACE

static constexpr string_view StrCheckUpdates = "Check updates";
static constexpr string_view StrConnectToServer = "Connect to the server";
static constexpr string_view StrCantConnectToServer = "Can't connect to the server!";
static constexpr string_view StrConnectionEstablished = "Connection established";
static constexpr string_view StrConnectionFailure = "Connection failure!";
static constexpr string_view StrFilesystemError = "File system error!";
static constexpr string_view StrServerMissingNativeUpdate = "Server doesn't provide a native client update for binary target {}. Please update the client manually";
static constexpr string_view StrUpdaterOutdated = "Client updater is incompatible with this server. Please install the latest full client package.";
static constexpr string_view StrPlatformUnsupported = "Client outdated, please update via your app store";
static constexpr string_view StrNativeUpdateFailed = "Failed to update native client modules for binary target {}. Please update the client manually";
static constexpr string_view StrRestartRequired = "Update downloaded. Please restart the client to apply the update.";
static constexpr string_view StrMetadataMismatch = "Game data on the server does not match the data it distributes. The server is probably mid-update, please try again later.";
static constexpr string_view StrErrorMessageCaption = "";
static constexpr string_view StrUpdateFailed {"Client update failed. Please install the latest full client package."};
static constexpr string_view StrServerUnavailable {"Can't connect to the server. It may be offline or restarting, please try again later."};
static constexpr string_view StrWaitingForAnotherClient {"Waiting for another game window to finish its update"};
static constexpr string_view StrCheckLocalFiles {"Check local game files"};
static constexpr string_view StrUpdaterDescriptorError = "Update descriptor error!";
static constexpr string_view StrExternalUpdateStarted = "External updater: download from mirror";
static constexpr string_view StrExternalUpdateFallback = "External mirror failed, trying fallback";
static constexpr string_view StrFastUpdateStarted = "Fast updater: download from UDP mirrors";
static constexpr string_view StrFastUpdateFallback = "Fast updater failed, fallback to game channel";

static constexpr string_view ClientBinaryStagingSuffix {"-staging"};
static constexpr string_view ClientRuntimeBootstrapExtension {".path"};
static constexpr uint64_t ClientRuntimeBootstrapMaxSize {4096};
// Retried rather than blocked on, so the screen keeps drawing while another client finishes its update
static constexpr int32_t DirectoryRetryPeriodMs {250};
// A frame reads packs for at most this long, so a first check over gigabytes still draws its progress
static constexpr int32_t VerificationFrameBudgetMs {20};
static constexpr uint64_t VerificationStepBytes {4 * 1024 * 1024};

static auto NormalizeClientRuntimeBootstrapTarget(string_view runtime_path, string_view expected_runtime_file_name) -> optional<string>;
static auto MakeVerifiedIdentityKey(string_view path) -> string;
static auto UpdaterResultToString(UpdaterResult result) noexcept -> string_view;
static void ReportUpdaterFailure(UpdaterResult result, string_view target_name) noexcept;
static auto IsResumablePackPrefix(string_view temp_path, const ResourcePackHeader& advertised) -> bool;

extern void SetupContentUpdateTransportsHook(ContentUpdateTransportRegistry& registry, GlobalSettings& settings);

Updater::Updater(ptr<GlobalSettings> settings, ptr<IAppWindow> window) :
    _settings {settings},
    _conn(settings, ClientConnection::ResourceMode::Platform),
    _cache(fs::make_writable_path(settings->Common.UserWritablePath, settings->Common.CachePath)),
    _binaryDir {GetClientBinaryDir(settings->Common.UserWritablePath)},
    _gameTime(settings),
    _effectMngr(settings, make_ptr(&_resources), window->GetRender()),
    _sprMngr(settings, window, make_ptr(&_gameTime), make_ptr(&_effectMngr), make_ptr(&_hashStorage)),
    _resMngr(make_ptr(&_resources), make_ptr(&_sprMngr)),
    _fontMngr(make_ptr(&_sprMngr), make_ptr(&_resMngr))
{
    FO_TRACE_ZONE(Engine);

    logging::write("Client updater: created for {}:{}, compatibility {}, binary dir {}, resources {}", _settings->ClientNetwork.ServerHost, _settings->Network.ServerPort, _settings->Network.CompatibilityVersion, _binaryDir, _settings->Common.ResourcesPath);

    try {
        SetupContentUpdateTransportsHook(_contentUpdateTransports, *settings);
    }
    catch (const std::exception&) {
        logging::write(logging::type::warning, "External updater transports are unavailable for this session: setup failure");
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    _startTime = nanotime::now();

    _resources.AddPackSource(settings->Common.Packaged, settings->Common.Packaged ? settings->Common.ResourcesPath : settings->Baking.BakeOutput, EMBEDDED_PACK_NAME);
    _resources.AddDirSource(_settings->Common.ResourcesPath, false, true, true);

    if (!settings->Common.UserWritablePath.empty()) {
        _resources.AddDirSource(GetClientWritableResourceDir(*settings), false, true, true);
    }

    _effectMngr.LoadMinimalEffects();

    _resMngr.RegisterSpriteFactory(safe_alloc::make_unique<DefaultSpriteFactory>(&_sprMngr, &_resMngr));

    // Wait screen. The splash pack is one of the packs this run repairs, so a damaged copy costs the picture, never the run
    try {
        if (!_settings->Client.DefaultSplashPack.empty()) {
            AddClientPackSource(_resources, *_settings, _settings->Client.DefaultSplashPack, true);
        }

        if (!_settings->Client.DefaultSplash.empty()) {
            _splashPic = _resMngr.LoadSprite(_settings->Client.DefaultSplash, AtlasType::OneImage);
        }
    }
    catch (const std::exception& ex) {
        logging::write(logging::type::warning, "Client updater: splash is unusable until the resources are synced, {}", ex.what());
        _splashPic.reset();
    }

    if (_splashPic) {
        _splashPic->PlayDefault();
    }

    _sprMngr.BeginScene();

    if (_splashPic) {
        _sprMngr.DrawSpriteSize(_splashPic, {0, 0}, _sprMngr.GetScreenSize(), true, true, Color::Neutral);
    }

    _sprMngr.EndScene();

    // Load font
    _fontMngr.BindDefaultFont(true);

    // Network handlers
    _conn.SetConnectHandler([this](ClientConnection::ConnectResult result) FO_DEFERRED { Net_OnConnect(result); });
    _conn.SetDisconnectHandler([this]() FO_DEFERRED { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NetMessage::InitData, [this]() FO_DEFERRED { Net_OnInitData(); });
    _conn.AddMessageHandler(NetMessage::TimeSync, [this]() FO_DEFERRED { Net_OnTimeSync(); });
    _conn.AddMessageHandler(NetMessage::HashList, [this]() FO_DEFERRED { Net_OnHashList(); });
    _conn.AddMessageHandler(NetMessage::UpdateFileData, [this]() FO_DEFERRED { Net_OnUpdateFileData(); });

    // Unlock all resources to prevent collision with new files
    _resources.CleanDataSources();

    TryStart();
}

Updater::~Updater() = default;

auto Updater::Process() -> bool
{
    FO_TRACE_ZONE(Engine);

    _gameTime.FrameAdvance(is_run_in_debugger());

    InputEvent ev {};

    ProcessExternalUpdate();
    ProcessFastUpdate();

    while (_sprMngr.GetInput()->PollEvent(ev)) {
        if (ev.Type == InputEvent::EventType::KeyDownEvent) {
            if (ev.KeyDown.Code == KeyCode::Escape) {
                GetApp()->RequestQuit();
            }
        }
    }

    // Update indication
    string update_text {};

    for (const auto& message : _messages) {
        update_text += message;
        update_text += "\n";
    }

    if (!_filesToUpdate.empty()) {
        update_text += "\n";

        for (const auto& update_file : _filesToUpdate) {
            uint64_t cur_bytes {update_file.Size - update_file.RemaningSize};
            uint64_t total_bytes {update_file.Size};

            if (&update_file == &_filesToUpdate.front() && _externalUpdate) {
                cur_bytes = _externalUpdate->GetDownloadedBytes();
            }
            else if (&update_file == &_filesToUpdate.front() && _fastUpdateClient) {
                cur_bytes = _fastUpdateClient->GetVerifiedBytes();
            }
            else if (&update_file == &_filesToUpdate.front() && _resourceRange != ResourceRange::None) {
                total_bytes = update_file.PackHeader.IndexStoredSize;
                cur_bytes = _rangeData.size();

                if (_patchWriter) {
                    cur_bytes += update_file.PackHeader.IndexStoredSize;
                    const auto& downloads {_patchWriter->GetDownloads()};

                    for (size_t i {0}; i < downloads.size(); ++i) {
                        total_bytes += downloads[i].StoredSize;

                        if (i < _patchDownloadIndex) {
                            cur_bytes += downloads[i].StoredSize;
                        }
                    }
                }
            }
            else if (&update_file == &_filesToUpdate.front()) {
                cur_bytes += _conn.GetUnpackedBytesReceived() - _bytesRealReceivedCheckpoint;
            }

            cur_bytes = std::min(cur_bytes, total_bytes);
            float32_t cur {numeric_cast<float32_t>(cur_bytes) / (1024.0f * 1024.0f)};
            float32_t max {std::max(numeric_cast<float32_t>(total_bytes) / (1024.0f * 1024.0f), 0.01f)};
            string name {static_cast<string&&>(strex(update_file.Name).format_path())};

            update_text += strex("{} {:.2f} / {:.2f} MB\n", name, cur, max);
        }

        update_text += "\n";
    }

    if (_stage == Stage::VerifyingResources && _verificationIndex < _verifications.size()) {
        const PackVerification& verification {_verifications[_verificationIndex]};
        float32_t cur {numeric_cast<float32_t>(verification.Verifier->GetCheckedBytes()) / (1024.0f * 1024.0f)};
        float32_t max {std::max(numeric_cast<float32_t>(verification.Verifier->GetTotalBytes()) / (1024.0f * 1024.0f), 0.01f)};

        update_text += strex("\n{} {:.2f} / {:.2f} MB\n\n", verification.PackName, cur, max);
    }

    int32_t elapsed_time {(nanotime::now() - _startTime).to_ms<int32_t>()};
    int32_t dots {iround<int32_t>(std::fmod((nanotime::now() - _startTime).to_ms<float64_t>() / 100.0, 50.0)) + 1};

    for (int32_t i {0}; i < dots; i++) {
        update_text += ".";
    }

    _effectMngr.UpdateEffects(_gameTime);
    _fontMngr.FrameUpdate();
    _sprMngr.BeginScene();

    if (_splashPic) {
        _sprMngr.DrawSpriteSize(_splashPic, {0, 0}, _sprMngr.GetScreenSize(), true, true, Color::Neutral);
    }

    if (elapsed_time >= _settings->Client.UpdaterInfoDelay) {
        auto text_format {TextFormat {.Font = FontType::Default, .Flags = combine_enum(FontFlag::CenterX, FontFlag::CenterY, FontFlag::Bordered)}};

        if (_settings->Client.UpdaterInfoPos < 0) {
            _fontMngr.DrawText(irect32 {0, 0, _sprMngr.GetScreenSize().width, _sprMngr.GetScreenSize().height / 2}, update_text, Color::TextWhite, text_format);
        }
        else if (_settings->Client.UpdaterInfoPos == 0) {
            _fontMngr.DrawText(irect32 {0, 0, _sprMngr.GetScreenSize().width, _sprMngr.GetScreenSize().height}, update_text, Color::TextWhite, text_format);
        }
        else {
            _fontMngr.DrawText(irect32 {0, _sprMngr.GetScreenSize().height / 2, _sprMngr.GetScreenSize().width, _sprMngr.GetScreenSize().height / 2}, update_text, Color::TextWhite, text_format);
        }
    }

    _sprMngr.EndScene();

    try {
        if (_stage == Stage::WaitingForDirectory && nanotime::now() >= _nextDirectoryAttempt) {
            TryStart();
        }
        else if (_stage == Stage::VerifyingResources) {
            ProcessResourceVerification();
        }

        _conn.Process();
    }
    catch (const std::exception& ex) {
        logging::write(logging::type::warning, "Client updater: update failed, {}", ex.what());
        // The connection disconnects before it rethrows a handler's failure, and that disconnect already recorded
        // a lost server: the throw is the cause, so a full disk or a broken pack is not blamed on the server
        _result = UpdaterResult::Failed;
        Abort(UpdaterResult::Failed, StrUpdateFailed);
    }

    // A restart prompt keeps this object alive after the outcome, and another client must not wait on it meanwhile
    if (_result.has_value()) {
        _directoryLock.reset();
    }

    if (_restartPrompt && !GetApp()->IsQuitRequested()) {
        return false;
    }

    return _result.has_value() || IsFinished();
}

void Updater::AddText(string_view text)
{
    _messages.emplace_back(text);
}

void Updater::Abort(UpdaterResult result, string_view text)
{
    _aborted = true;

    if (!_result.has_value()) {
        _result = result;
    }

    AddText(text);
    _conn.Disconnect();

    _tempFile.close();
    _patchWriter.reset();
    _verifications.clear();
    _directoryLock.reset();
}

void Updater::TryStart()
{
    FO_TRACE_ZONE(Network);

    string directory {GetClientWritableResourceDir(*_settings)};
    bool directory_created {fs::create_directories(directory)};
    FO_VERIFY_AND_THROW(directory_created, "Can't create writable resource directory", directory);
    unique_ptr<fs::disk_directory_lock> lock {safe_alloc::make_unique<fs::disk_directory_lock>(directory)};

    if (!*lock) {
        if (!_waitingNoticeShown) {
            _waitingNoticeShown = true;
            logging::write("Client updater: resource directory {} is being updated by another client, waiting for it", directory);
            AddText(StrWaitingForAnotherClient);
        }

        _nextDirectoryAttempt = nanotime::now() + std::chrono::milliseconds {DirectoryRetryPeriodMs};
        return;
    }

    if (_waitingNoticeShown) {
        logging::write("Client updater: resource directory {} is free again", directory);
    }

    _directoryLock = std::move(lock);
    RecoverInterruptedReplacements();
    PlanResourceVerification();

    if (_verifications.empty()) {
        StartSynchronization();
        return;
    }

    _stage = Stage::VerifyingResources;
    AddText(StrCheckLocalFiles);
}

void Updater::StartSynchronization()
{
    FO_TRACE_ZONE(Network);

    _stage = Stage::Synchronizing;
    AddText(StrConnectToServer);

    _conn.SetMetadataVersion(ReadLocalMetadataVersion());
    _conn.Connect();
}

void Updater::PlanResourceVerification()
{
    FO_TRACE_ZONE(FileSystem);

    // A browser session starts from the packs it has just fetched and keeps nothing, and an unpackaged client reads a
    // bake output rather than packs: neither holds anything older to distrust
    if (build_condition<FO_WEB>() || !_settings->Common.Packaged) {
        return;
    }

    uint64_t total_bytes {0};

    for (const string& pack_name : _settings->GetClientResourcePacks()) {
        if (pack_name == EMBEDDED_PACK_NAME) {
            continue;
        }

        PackVerification verification {};
        verification.PackName = pack_name;
        verification.BasePath = GetClientResourcePackPath(*_settings, pack_name);
        verification.PatchPath = GetClientResourcePatchPath(*_settings, pack_name);
        verification.BaseIdentity = ReadBaseIdentity(verification.BasePath);

        // A pack that is missing or has no readable header is not current either, and the sync replaces it unread
        if (!verification.BaseIdentity.has_value()) {
            continue;
        }

        verification.PatchIdentity = ReadPatchIdentity(verification.PatchPath, verification.BasePath);

        if (IsIdentityVerified(verification.BasePath, verification.BaseIdentity.value())) {
            verification.BaseIdentity.reset();
        }
        if (verification.PatchIdentity.has_value() && IsIdentityVerified(verification.PatchPath, verification.PatchIdentity.value())) {
            verification.PatchIdentity.reset();
        }

        if (!verification.BaseIdentity.has_value() && !verification.PatchIdentity.has_value()) {
            continue;
        }

        verification.Verifier = safe_alloc::make_unique<ResourcePairVerifier>(verification.BasePath, verification.PatchPath, verification.BaseIdentity.has_value(), verification.PatchIdentity.has_value());
        total_bytes += verification.Verifier->GetTotalBytes();
        _verifications.emplace_back(std::move(verification));
    }

    if (!_verifications.empty()) {
        logging::write("Client updater: checking {} local packs no earlier run has proved intact, bytes {}", _verifications.size(), total_bytes);
    }
}

void Updater::ProcessResourceVerification()
{
    FO_TRACE_ZONE(FileSystem);

    nanotime deadline {nanotime::now() + std::chrono::milliseconds {VerificationFrameBudgetMs}};

    while (_verificationIndex < _verifications.size()) {
        PackVerification& verification {_verifications[_verificationIndex]};

        if (!verification.Verifier->IsFinished()) {
            verification.Verifier->Step(VerificationStepBytes);
        }

        if (verification.Verifier->IsFinished()) {
            FinishPackVerification(verification);
            ++_verificationIndex;
        }

        if (nanotime::now() >= deadline) {
            break;
        }
    }

    if (_verificationIndex == _verifications.size()) {
        logging::write("Client updater: local packs checked, damaged {}", _damagedPacks.size());
        _verifications.clear();
        StartSynchronization();
    }
}

void Updater::FinishPackVerification(const PackVerification& verification)
{
    FO_TRACE_ZONE(FileSystem);

    const ResourcePairVerifier& verifier {*verification.Verifier};

    // The pair's catalog still names the right content, so without this check the sync would call it current and
    // every launch would read the same damaged bytes again
    if (!verifier.IsBaseIntact()) {
        logging::write(logging::type::warning, "Client updater: local pack {} is damaged, the whole pack will be downloaded again", verification.PackName);
        _damagedPacks[verification.PackName] = LocalDamage::Base;
        return;
    }

    if (verification.BaseIdentity.has_value()) {
        RecordVerifiedIdentity(verification.BasePath, verification.BaseIdentity.value());
    }

    if (!verifier.IsPatchIntact()) {
        logging::write(logging::type::warning, "Client updater: local pack {} has a damaged patch payload, it will be fetched again", verification.PackName);
        _damagedPacks[verification.PackName] = LocalDamage::Patch;
        return;
    }

    if (verification.PatchIdentity.has_value()) {
        RecordVerifiedIdentity(verification.PatchPath, verification.PatchIdentity.value());
    }

    if (_fastUpdateClient) {
        _fastUpdateClient->Cleanup();
        _fastUpdateClient.reset();
    }

    if (_externalUpdate) {
        _externalUpdate->Cancel();
        _externalUpdate.reset();
    }
}

void Updater::ProcessExternalUpdate()
{
    FO_TRACE_ZONE(Core);

    if (!_externalUpdate || _filesToUpdate.empty()) {
        return;
    }

    auto& update_file = _filesToUpdate.front();
    auto& download = *_externalUpdate;

    try {
        download.Process();
    }
    catch (const std::exception&) {
        FallbackFromExternalUpdate(update_file, ContentUpdateSourceResult::TransportFailure, "transport processing failed");
        return;
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    const ContentUpdateTransportStatus status = download.GetStatus();

    if (status == ContentUpdateTransportStatus::InProgress) {
        return;
    }

    if (status == ContentUpdateTransportStatus::Failed) {
        FallbackFromExternalUpdate(update_file, ContentUpdateSourceResult::TransportFailure, "transport failed");
        return;
    }

    if (!IsDiskFileSha256Match(_externalCandidatePath, update_file.Size, update_file.Sha256)) {
        FallbackFromExternalUpdate(update_file, ContentUpdateSourceResult::IntegrityFailure, "candidate size or SHA-256 mismatch");
        return;
    }

    const auto& source = _updateManifest.Files[update_file.ManifestIndex].Sources[*_activeExternalSourceIndex];
    const string file_name = update_file.Name;
    const string provider = source.Provider;
    const string source_key = source.SourceKey;
    const string temp_path = MakeTempPath(update_file);

    ReportExternalSourceResult(update_file, source, ContentUpdateSourceResult::Success);

    download.Cancel();
    _externalUpdate.reset();
    _activeExternalSourceIndex.reset();

    if (!ReplaceFileSafely(_externalCandidatePath, temp_path)) {
        logging::write(logging::type::warning, "External updater could not promote candidate for '{}' from provider '{}' source '{}'", update_file.Name, provider, source_key);
        (void)fs::remove_file(_externalCandidatePath);
        _externalCandidatePath.clear();
        Abort(UpdaterResult::Failed, StrFilesystemError);
        return;
    }

    _externalCandidatePath.clear();
    update_file.RemaningSize = 0;
    update_file.Sha256AlreadyVerified = true;

    const FinalizeResult finalize_result = FinalizeCurrentFile();

    if (finalize_result != FinalizeResult::Succeeded) {
        Abort(UpdaterResult::Failed, finalize_result == FinalizeResult::FileSystemFailure ? StrFilesystemError : StrUpdaterDescriptorError);
        return;
    }

    logging::write("External updater completed for '{}' from provider '{}' source '{}'", file_name, provider, source_key);
    GetNextFile();
}

auto Updater::TryStartExternalUpdate(UpdateFile& update_file) -> bool
{
    FO_TRACE_ZONE(Core);

    const auto& file_info = _updateManifest.Files[update_file.ManifestIndex];

    while (update_file.NextExternalSource < file_info.Sources.size()) {
        const size_t source_index = update_file.NextExternalSource++;
        const auto& source = file_info.Sources[source_index];

        if (_failedExternalProviders.contains(source.Provider) || _failedExternalTransports.contains(source.Transport)) {
            continue;
        }

        if (IsExternalSourceExpired(source)) {
            logging::write("External updater skipped expired provider '{}' source '{}' for '{}'", source.Provider, source.SourceKey, update_file.Name);
            continue;
        }

        if (!_contentUpdateTransports.IsRegistered(source.Transport)) {
            logging::write("External updater skipped unsupported transport '{}' from provider '{}' source '{}' for '{}'", source.Transport, source.Provider, source.SourceKey, update_file.Name);
            _failedExternalTransports.emplace(source.Transport);
            continue;
        }

        const string candidate_path = MakeExternalCandidatePath(update_file, source_index);
        const string candidate_dir = strex(candidate_path).extract_dir().str();

        if (!candidate_dir.empty() && !fs::create_directories(candidate_dir)) {
            Abort(UpdaterResult::Failed, StrFilesystemError);
            return false;
        }

        unique_nptr<ContentUpdateTransportDownload> download;

        try {
            const ContentUpdateTransportRequest request {source, file_info, candidate_path};
            download = _contentUpdateTransports.Create(source.Transport, request);
        }
        catch (const std::exception&) {
            logging::write(logging::type::warning, "External updater transport '{}' failed to create for provider '{}' source '{}'", source.Transport, source.Provider, source.SourceKey);
            _failedExternalProviders.emplace(source.Provider);
            (void)fs::remove_file(candidate_path);
            continue;
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }

        if (!download) {
            logging::write(logging::type::warning, "External updater transport '{}' declined provider '{}' source '{}' for '{}'", source.Transport, source.Provider, source.SourceKey, update_file.Name);
            (void)fs::remove_file(candidate_path);
            continue;
        }

        _externalCandidatePath = candidate_path;
        _activeExternalSourceIndex = source_index;
        _externalUpdate = std::move(download);
        AddText(StrExternalUpdateStarted);
        logging::write("External updater started for '{}' from provider '{}' source '{}' using '{}'", update_file.Name, source.Provider, source.SourceKey, source.Transport);
        _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
        return true;
    }

    return false;
}

void Updater::FallbackFromExternalUpdate(UpdateFile& update_file, ContentUpdateSourceResult result, string_view reason)
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(_externalUpdate && _activeExternalSourceIndex, "External update must be active for fallback");

    const auto& source = _updateManifest.Files[update_file.ManifestIndex].Sources[*_activeExternalSourceIndex];
    logging::write(logging::type::warning, "External updater failed for '{}' from provider '{}' source '{}': {}", update_file.Name, source.Provider, source.SourceKey, reason);
    AddText(StrExternalUpdateFallback);
    _failedExternalProviders.emplace(source.Provider);
    ReportExternalSourceResult(update_file, source, result);

    _externalUpdate->Cancel();
    _externalUpdate.reset();
    _activeExternalSourceIndex.reset();
    (void)fs::remove_file(_externalCandidatePath);
    _externalCandidatePath.clear();

    GetNextFile();
}

void Updater::ReportExternalSourceResult(const UpdateFile& update_file, const ContentUpdateSource& source, ContentUpdateSourceResult result)
{
    FO_TRACE_ZONE(Core);

    if (_updateManifest.CatalogGeneration == 0 || IsContentUpdateSourceReportTokenEmpty(source.ReportToken)) {
        return;
    }

    _conn.OutBuf->StartMsg(NetMessage::ReportUpdateSource);
    _conn.OutBuf->Write(_updateManifest.CatalogGeneration);
    _conn.OutBuf->Write(update_file.Index);
    _conn.OutBuf->Push(source.ReportToken);
    _conn.OutBuf->Write(result);
    _conn.OutBuf->EndMsg();
}

void Updater::ProcessFastUpdate()
{
    FO_TRACE_ZONE(Core);

    if (!_fastUpdateClient || _filesToUpdate.empty()) {
        return;
    }

    auto& update_file = _filesToUpdate.front();
    _fastUpdateClient->Process();

    if (_fastUpdateClient->IsFinished()) {
        const auto temp_path = MakeTempPath(update_file);

        if (!_fastUpdateClient->AssembleFile(temp_path)) {
            FallbackFromFastUpdate(update_file, "assembled file validation failed");
            return;
        }

        logging::write("Fast updater completed for '{}'", update_file.Name);
        _fastUpdateClient.reset();
        update_file.RemaningSize = 0;

        const FinalizeResult finalize_result = FinalizeCurrentFile();

        if (finalize_result != FinalizeResult::Succeeded) {
            Abort(UpdaterResult::Failed, finalize_result == FinalizeResult::FileSystemFailure ? StrFilesystemError : StrUpdaterDescriptorError);
            return;
        }

        GetNextFile();
        return;
    }

    if (!_fastUpdateClient->IsFailed()) {
        return;
    }

    FallbackFromFastUpdate(update_file, _fastUpdateClient->GetError());
}

void Updater::FallbackFromFastUpdate(UpdateFile& update_file, string_view reason)
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(_fastUpdateClient, "Fast update client must be active for the fallback");

    logging::write(logging::type::warning, "Fast updater failed for '{}': {}", update_file.Name, reason);
    AddText(StrFastUpdateFallback);

    const auto temp_path = MakeTempPath(update_file);
    const auto dir = strex(temp_path).extract_dir().str();
    uint64_t resumed_bytes = 0;

    (void)fs::remove_file(temp_path);

    if (!dir.empty() && !fs::create_directories(dir)) {
        Abort(UpdaterResult::Failed, StrFilesystemError);
        return;
    }

    _tempFile = fs::disk_write_file {temp_path, fs::disk_write_mode::create};

    if (!_tempFile || !_fastUpdateClient->WriteContiguousData(_tempFile, resumed_bytes)) {
        Abort(UpdaterResult::Failed, StrFilesystemError);
        return;
    }

    _fastUpdateClient.reset();
    update_file.UseFastUpdate = false;
    update_file.RemaningSize = update_file.Size - resumed_bytes;
    RequestUpdateFile(update_file);
    _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
}

void Updater::FinishResourcesUpdate()
{
    FO_TRACE_ZONE(FileSystem);

    for (const UpdateFile& file : _resourceTargets) {
        if (!IsLocalResourceCurrent(file)) {
            logging::write(logging::type::warning, "Client updater: resource target is not installed {}", file.Name);
            Abort(UpdaterResult::Failed, StrUpdateFailed);
            return;
        }
    }

    string directory {GetClientWritableResourceDir(*_settings)};

    if (!_resourceTargets.empty() && fs::is_dir(directory)) {
        FO_VERIFY_AND_THROW(_directoryLock, "Stale patches are removed without the resource directory lock", directory);

        for (const UpdateFile& file : _resourceTargets) {
            string patch {strex(directory).combine_path(GetResourcePatchPath(file.Name)).str()};

            if (!fs::exists(patch)) {
                continue;
            }

            string base {GetClientResourcePackPath(*_settings, string_view(file.Name).substr(0, file.Name.size() - 6))};
            ResourcePackHeader header {};
            bool header_read {ReadResourcePackHeader(base, header)};
            FO_VERIFY_AND_THROW(header_read, "Resource base disappeared before readiness", base);

            if (!ReadResourcePatchInfo(patch, header)) {
                bool patch_removed {fs::remove_file(patch) && fs::sync_parent(patch)};
                FO_VERIFY_AND_THROW(patch_removed, "Can't remove stale resource patch", patch);
            }
        }
    }

    string local_metadata_version {ReadLocalMetadataVersion()};

    if (local_metadata_version != _serverMetadataVersion) {
        logging::write(logging::type::warning, "Client updater: synced resources run metadata version {} while the server runs {}, resources {}", local_metadata_version, _serverMetadataVersion, _settings->Common.Packaged ? _settings->Common.ResourcesPath : _settings->Baking.BakeOutput);
        _result = UpdaterResult::MetadataMismatch;
        return;
    }

    RebuildResourceIndex();

    if (_settings->Common.Packaged && std::ranges::any_of(_settings->GetResourcePacks(), [](const ResourcePackInfo& pack) { return pack.Name == METADATA_PACK_NAME; })) {
        FileSystem metadata_source {};
        AddClientPackSource(metadata_source, *_settings, METADATA_PACK_NAME);
        auto content {ReadResourceConfig(metadata_source, true)};

        if (content && _settings->GetResourceConfigContent() != content) {
            auto checked {GlobalSettings(false)};
            checked.SetupResourceConfig(*content);
            checked.ApplyWritableRoot(_settings->Common.UserWritablePath);
            ApplyAppSettingsOverrides(checked, _settings->GetCommandLineArgs());
            checked.ApplyStartupOverrides(*_settings);

            if (_settings->HasEngineSettingsChanges(checked)) {
                PreserveLocalConfig(*_settings, checked);
                WebRelated::FlushPersistentData();
                logging::write("Client updater: engine configuration changed, restart required before gameplay");
                _result = UpdaterResult::SettingsRestartRequired;

                if (!GetApp()->IsHeadless()) {
                    AddText(StrRestartRequired);
                    _restartPrompt = true;
                }

                return;
            }

            _settings->ApplyGameSettings(checked);
            WebRelated::FlushPersistentData();
            logging::write("Client updater: game configuration refreshed before gameplay");
        }
    }

    logging::write("Client updater: resources ready, metadata version {}", local_metadata_version);
    _result = UpdaterResult::ResourcesReady;
}

void Updater::RebuildResourceIndex() const
{
    FO_TRACE_ZONE(FileSystem);

    // The tree only pays by outliving the launch that built it, and the web filesystem starts empty every
    // load - so building it there costs the per-pack index parse it exists to save
    if (build_condition<FO_WEB>()) {
        return;
    }

    string index_path {};

    // The merged tree is an optimization over mounting each pack, so nothing here may fail the update. The
    // whole body is guarded, not just the build, so a throw anywhere leaves the client on the per-pack view
    try {
        vector<string> pack_dirs {GetClientPackDirs(*_settings)};
        index_path = GetClientResourceIndexPath(*_settings);
        vector<string> indexed_packs {GetResourceIndexPackNames(_settings->GetClientResourcePacks())};

        if (indexed_packs.empty() || IsResourceIndexCurrent(index_path, pack_dirs, indexed_packs)) {
            return;
        }

        vector<ResourceIndexPack> packs {};
        vector<string> pack_paths {};

        if (!ResolveResourceIndexPacks(pack_dirs, indexed_packs, packs, pack_paths)) {
            logging::write(logging::type::warning, "Client updater: can't resolve every pack, leaving the merged index to the next run");
            return;
        }

        bool index_dir_ready {fs::create_directories(strex(index_path).extract_dir().str())};
        FO_VERIFY_AND_THROW(index_dir_ready, "Can't create the resource index directory", index_path);
        BuildResourceIndex(index_path, pack_paths, packs);
        logging::write("Client updater: merged index rebuilt over {} packs", packs.size());
    }
    catch (const std::exception& ex) {
        logging::write(logging::type::warning, "Client updater: can't build the merged index, {}", ex.what());

        if (!index_path.empty()) {
            (void)fs::remove_file(index_path);
        }
    }
}

auto Updater::ReadLocalMetadataVersion() const -> string
{
    FO_TRACE_ZONE(FileSystem);

    // The override stands in for a client baked apart from the server, so it has to reach the updater too -
    // otherwise the updater would keep declaring resources ready while the client keeps being rejected
    if (!_settings->Network.ForceMetadataVersion.empty()) {
        return string(_settings->Network.ForceMetadataVersion);
    }

    // Mounted separately from the updater resources, which carry directories rather than the game packs.
    // Built by the gameplay entry point so the version validated here is the one the game will read
    try {
        FileSystem resources {GetClientResources(*_settings)};
        vector<uint8_t> metadata_bin {ReadMetadataBin(&resources, "Client")};
        return ReadMetadataVersion(metadata_bin);
    }
    catch (const std::exception& ex) {
        logging::write(logging::type::warning, "Client updater: can't read local metadata version, {}", ex.what());
        return {};
    }
}

void Updater::GetNextFile()
{
    FO_TRACE_ZONE(Network);

    if (static_cast<bool>(_tempFile)) {
        const FinalizeResult finalize_result = FinalizeCurrentFile();

        if (finalize_result == FinalizeResult::IntegrityMismatch && !_filesToUpdate.front().DirectRetryAttempted) {
            auto& update_file = _filesToUpdate.front();
            const string temp_path = MakeTempPath(update_file);
            const string dir = strex(temp_path).extract_dir().str();

            logging::write(logging::type::warning, "Client updater: direct transfer integrity check failed for '{}', retrying once from zero", update_file.Name);
            (void)fs::remove_file(temp_path);

            if (!dir.empty() && !fs::create_directories(dir)) {
                Abort(UpdaterResult::Failed, StrFilesystemError);
                return;
            }

            _tempFile = fs::disk_write_file {temp_path, fs::disk_write_mode::create};

            if (!_tempFile) {
                Abort(UpdaterResult::Failed, StrFilesystemError);
                return;
            }

            update_file.DirectRetryAttempted = true;
            update_file.RemaningSize = update_file.Size;
            RequestUpdateFile(update_file);
            _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
            return;
        }

        if (finalize_result != FinalizeResult::Succeeded) {
            Abort(UpdaterResult::Failed, finalize_result == FinalizeResult::FileSystemFailure ? StrFilesystemError : StrUpdaterDescriptorError);
            return;
        }
    }

    if (!_filesToUpdate.empty()) {
        auto& next_update_file = _filesToUpdate.front();
        const auto final_path = MakeFinalPath(next_update_file);
        const auto temp_path = MakeTempPath(next_update_file);

        FO_VERIFY_AND_THROW(!_externalUpdate && !_activeExternalSourceIndex, "External updater state must be idle before selecting a source");
        _fastUpdateClient.reset();
        CleanupStaleTempFiles(next_update_file, temp_path);

        const auto temp_file_size = GetDiskFileSize(temp_path);
        bool can_start_fast_update = next_update_file.UseFastUpdate;

        if (temp_file_size.has_value()) {
            if (*temp_file_size > next_update_file.Size) {
                logging::write("Client updater: temp file {} is too large, size {}, expected {}", temp_path, *temp_file_size, next_update_file.Size);
                (void)fs::remove_file(temp_path);
                next_update_file.RemaningSize = next_update_file.Size;
            }
            else if (*temp_file_size == next_update_file.Size) {
                if (!IsDiskFileHashMatch(temp_path, next_update_file.Size, next_update_file.Hash) || !IsDiskFileSha256Match(temp_path, next_update_file.Size, next_update_file.Sha256)) {
                    logging::write("Client updater: complete temp file {} has wrong hash or SHA-256, restarting download", temp_path);
                    (void)fs::remove_file(temp_path);
                    next_update_file.RemaningSize = next_update_file.Size;
                }
                else {
                    if (!ReplaceFileSafely(temp_path, final_path)) {
                        logging::write("Client updater: failed to promote existing temp file from {} to {}", temp_path, final_path);
                        Abort(UpdaterResult::Failed, StrFilesystemError);
                        return;
                    }

                    logging::write("Client updater: promoted existing temp file to {}, binary {}", final_path, next_update_file.IsClientBinary ? "yes" : "no");
                    if (!TryPromoteStagedBinary(next_update_file, final_path)) {
                        Abort(UpdaterResult::Failed, StrFilesystemError);
                        return;
                    }
                    _filesToUpdate.erase(_filesToUpdate.begin());
                    GetNextFile();
                    return;
                }
            }
            else if (!next_update_file.IsClientBinary && !IsResumablePackPrefix(temp_path, next_update_file.PackHeader)) {
                logging::write(logging::type::warning, "Client updater: temp file {} was started from another server build, restarting download", temp_path);
                (void)fs::remove_file(temp_path);
                next_update_file.RemaningSize = next_update_file.Size;
            }
            else {
                next_update_file.RemaningSize = next_update_file.Size - *temp_file_size;
                can_start_fast_update = can_start_fast_update && *temp_file_size == 0;
                logging::write("Client updater: resuming temp file {}, downloaded {}, remaining {}", temp_path, *temp_file_size, next_update_file.RemaningSize);
            }
        }

        if (TryStartExternalUpdate(next_update_file)) {
            return;
        }

        if (_aborted) {
            return;
        }

        if (can_start_fast_update) {
            (void)fs::remove_file(temp_path);
            AddText(StrFastUpdateStarted);
            logging::write("Fast updater started for '{}'", next_update_file.Name);
            _fastUpdateClient.emplace(*_settings, _updateManifest, _updateManifest.Files[next_update_file.ManifestIndex], temp_path);

            if (!_fastUpdateClient->IsFailed()) {
                _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
                return;
            }

            logging::write(logging::type::warning, "Fast updater failed to start for '{}': {}", next_update_file.Name, _fastUpdateClient->GetError());
            AddText(StrFastUpdateFallback);
            next_update_file.UseFastUpdate = false;
            _fastUpdateClient->Cleanup();
            _fastUpdateClient.reset();
        }

        if (!next_update_file.IsClientBinary && next_update_file.TryPatch) {
            next_update_file.TryPatch = false;
            string base_path {GetClientResourcePackPath(*_settings, string_view(next_update_file.Name).substr(0, next_update_file.Name.size() - 6))};
            ResourcePackHeader base_header {};

            if (ReadResourcePackHeader(base_path, base_header)) {
                _resourceRange = ResourceRange::Catalog;
                _rangeOffset = next_update_file.PackHeader.IndexOffset;
                _rangeSize = next_update_file.PackHeader.IndexStoredSize;
                _rangeData.clear();
                RequestResourceRange();
                return;
            }
        }

        const auto dir = strex(temp_path).extract_dir().str();

        if (!dir.empty()) {
            if (!fs::create_directories(dir)) {
                Abort(UpdaterResult::Failed, StrFilesystemError);
                return;
            }

            // Refusing a pack that will not fit leaves the installed one alone, where running the volume dry
            // mid-transfer leaves a temp file and a download to repeat
            auto available {fs::available_space(dir)};

            if (available.has_value() && *available < next_update_file.RemaningSize) {
                logging::write(logging::type::warning, "Client updater: not enough free space for {}, need {}, available {}", next_update_file.Name, next_update_file.RemaningSize, *available);
                Abort(UpdaterResult::Failed, StrFilesystemError);
                return;
            }
        }

        fs::disk_write_mode open_mode {next_update_file.RemaningSize != next_update_file.Size ? fs::disk_write_mode::append : fs::disk_write_mode::create};
        _tempFile = fs::disk_write_file {temp_path, open_mode};

        if (!_tempFile) {
            logging::write(logging::type::warning, "Client updater: failed to open temp file {}", temp_path);
            Abort(UpdaterResult::Failed, StrFilesystemError);
            return;
        }

        logging::write("Client updater: requesting file {}, binary {}, size {}, remaining {}, temp {}, final {}", next_update_file.Name, next_update_file.IsClientBinary ? "yes" : "no", next_update_file.Size, next_update_file.RemaningSize, temp_path, final_path);
        RequestUpdateFile(next_update_file);
    }
    else {
        if (_binariesMode) {
            logging::write("Client updater: finished binary update, binaries staged for reload");
            _result = UpdaterResult::BinariesStaged;

            // Headless clients have no UI / no user to dismiss it, so they finish immediately (no hold)
            if (!GetApp()->IsHeadless()) {
                AddText(StrRestartRequired);
                _restartPrompt = true;
            }
        }
        else {
            logging::write("Client updater: finished resource update");
            FinishResourcesUpdate();
        }
    }

    _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
}

void Updater::RequestUpdateFile(const UpdateFile& update_file)
{
    uint64_t start_offset {update_file.Size - update_file.RemaningSize};

    _conn.OutBuf->StartMsg(NetMessage::GetUpdateFile);
    _conn.OutBuf->Write(update_file.Index);
    _conn.OutBuf->Write(numeric_cast<uint64_t>(start_offset));
    _conn.OutBuf->Write(update_file.RemaningSize);
    _conn.OutBuf->Write(update_file.Hash);
    _conn.OutBuf->EndMsg();
}

auto Updater::IsLocalResourceCurrent(const UpdateFile& file) const -> bool
{
    if (file.PackHeader.PackHash == file.Hash) {
        return IsClientResourcePackCurrent(*_settings, strex(file.Name).erase_file_extension().str(), file.PackHeader.ContentHash);
    }

    const string resource_path {GetClientResourcePackPath(*_settings, strex(file.Name).erase_file_extension().str())};
    return VerifyResourcePackFile(resource_path, file.Hash);
}

void Updater::RequestResourceRange()
{
    FO_VERIFY_AND_THROW(!_filesToUpdate.empty() && _rangeData.size() <= _rangeSize, "No pending resource range");
    const UpdateFile& file {_filesToUpdate.front()};
    _conn.OutBuf->StartMsg(NetMessage::GetUpdateFile);
    _conn.OutBuf->Write(file.Index);
    _conn.OutBuf->Write(_rangeOffset + _rangeData.size());
    _conn.OutBuf->Write(_rangeSize - _rangeData.size());
    _conn.OutBuf->Write(file.Hash);
    _conn.OutBuf->EndMsg();
}

void Updater::FinishResourceRange()
{
    FO_TRACE_ZONE(FileSystem);

    UpdateFile& file {_filesToUpdate.front()};

    if (_resourceRange == ResourceRange::Catalog) {
        vector<ResourcePackEntryRef> entries {DecodeResourcePackIndex(_rangeData, file.PackHeader)};
        string pack_name {strex(file.Name).erase_file_extension().str()};
        string base {GetClientResourcePackPath(*_settings, pack_name)};
        string patch {GetClientResourcePatchPath(*_settings, pack_name)};
        bool patch_directory_created {fs::create_directories(strex(patch).extract_dir().str())};
        FO_VERIFY_AND_THROW(patch_directory_created, "Can't create patch directory", patch);

        try {
            _patchWriter = safe_alloc::make_unique<ResourcePatchWriter>(base, patch, std::move(entries), file.PackHeader.ContentHash);
        }
        catch (const std::exception& ex) {
            logging::write(logging::type::warning, "Client updater: replacing unusable resource pair {}, {}", file.Name, ex.what());
            _resourceRange = ResourceRange::None;
            _rangeData.clear();
            GetNextFile();
            return;
        }

        // Every append keeps what it supersedes, so a patch that would outgrow its pack is mostly dead payloads: the
        // pack itself replaces the pair and leaves nothing superseded behind
        if (_patchWriter->GetFinalSize() >= file.Size) {
            logging::write("Client updater: patch {} would reach {} bytes over a {} byte pack, downloading the pack instead", patch, _patchWriter->GetFinalSize(), file.Size);
            _patchWriter.reset();
            _resourceRange = ResourceRange::None;
            _rangeData.clear();
            GetNextFile();
            return;
        }

        auto available {fs::available_space(strex(patch).extract_dir().str())};
        FO_VERIFY_AND_THROW(!available || *available >= _patchWriter->GetAppendSize(), "Not enough space for resource patch", patch, _patchWriter->GetAppendSize());
        size_t resumed {_patchWriter->GetResumedDownloads()};
        logging::write("Client updater: appending {} missing resources to {}, bytes {}", _patchWriter->GetDownloads().size() - resumed, patch, _patchWriter->GetAppendSize());

        if (resumed != 0) {
            logging::write("Client updater: resuming resource patch {}, {} of {} payloads were written by an interrupted run", patch, resumed, _patchWriter->GetDownloads().size());
        }

        _patchWriter->Begin(*_directoryLock);
        _patchDownloadIndex = resumed;
    }
    else {
        FO_VERIFY_AND_THROW(_patchWriter, "No active resource patch writer");
        _patchWriter->AddEncodedFile(_rangeData);
        ++_patchDownloadIndex;
    }

    AdvanceResourcePatch();
}

void Updater::AdvanceResourcePatch()
{
    FO_TRACE_ZONE(FileSystem);

    FO_VERIFY_AND_THROW(_patchWriter, "No resource patch to advance");
    const auto& downloads {_patchWriter->GetDownloads()};
    _rangeData.clear();

    if (_patchDownloadIndex < downloads.size()) {
        const ResourcePackEntryRef& entry {downloads[_patchDownloadIndex]};
        _resourceRange = ResourceRange::Payload;
        _rangeOffset = entry.DataOffset;
        _rangeSize = entry.StoredSize;
        RequestResourceRange();
        return;
    }

    _patchWriter->Finish();
    logging::write("Client updater: committed resource patch {}", _filesToUpdate.front().Name);
    _patchWriter.reset();

    // Planning decoded every payload the new catalog reuses and each download was decoded before it was written
    RecordVerifiedPatch(strex(_filesToUpdate.front().Name).erase_file_extension().str());

    _resourceRange = ResourceRange::None;
    _filesToUpdate.erase(_filesToUpdate.begin());
    GetNextFile();
}

void Updater::Net_OnConnect(ClientConnection::ConnectResult result)
{
    FO_TRACE_ZONE(Network);

    string_view result_str {};

    switch (result) {
    case ClientConnection::ConnectResult::Success:
        result_str = "Success";
        break;
    case ClientConnection::ConnectResult::CompatibilityOutdated:
        result_str = "CompatibilityOutdated";
        break;
    case ClientConnection::ConnectResult::MetadataOutdated:
        result_str = "MetadataOutdated";
        break;
    case ClientConnection::ConnectResult::UpdaterOutdated:
        result_str = "UpdaterOutdated";
        break;
    case ClientConnection::ConnectResult::Failed:
        result_str = "Failed";
        break;
    default:
        result_str = "Unknown";
        break;
    }

    _serverMetadataVersion = string(_conn.GetServerMetadataVersion());
    logging::write("Client updater: server answered {}, client compatibility {}, server metadata version {}", result_str, _settings->Network.CompatibilityVersion, _serverMetadataVersion);

    if (result == ClientConnection::ConnectResult::Success || result == ClientConnection::ConnectResult::MetadataOutdated) {
        AddText(StrConnectionEstablished);
        AddText(StrCheckUpdates);
        _binariesMode = false;
        logging::write("Client updater: client is compatible, checking resources");
    }
    else if (result == ClientConnection::ConnectResult::CompatibilityOutdated) {
        AddText(StrConnectionEstablished);
        AddText(StrCheckUpdates);

        bool can_self_update_binaries {CanSelfUpdateNativeModules(GetCurrentUpdatePlatform())};
        logging::write("Client updater: server reported CompatibilityOutdated, native self-update {} for {}", can_self_update_binaries ? "supported" : "unsupported", GetCurrentBinaryUpdateTargetName());

        if (!can_self_update_binaries) {
            _result = UpdaterResult::PlatformUnsupported;
            _fileListReceived = true;
            _conn.Disconnect();
            return;
        }

        _binariesMode = true;
        logging::write("Client updater: switched to native binary update mode");
    }
    else if (result == ClientConnection::ConnectResult::UpdaterOutdated) {
        logging::write(logging::type::warning, "Client updater: protocol is outdated, aborting");
        Abort(UpdaterResult::UpdaterOutdated, StrUpdaterOutdated);
    }
    else {
        logging::write(logging::type::warning, "Client updater: connection failed");
        Abort(UpdaterResult::ConnectionFailed, StrCantConnectToServer);
    }
}

void Updater::Net_OnDisconnect()
{
    FO_TRACE_ZONE(Network);

    if (!_aborted && (!_fileListReceived || !_filesToUpdate.empty())) {
        // A drop while the transfer was still in flight is the server going away, not this client failing
        Abort(UpdaterResult::ConnectionFailed, StrConnectionFailure);
    }
}

void Updater::Net_OnInitData()
{
    FO_TRACE_ZONE(Network);

    auto data_size {_conn.InBuf->Read<uint32_t>()};

    if (data_size > ContentUpdateMaxDescriptorSize || numeric_cast<size_t>(data_size) > _conn.InBuf->GetUnreadSize()) {
        logging::write(logging::type::warning, "Rejected invalid content update descriptor length: {} bytes, {} bytes remain, maximum {}", data_size, _conn.InBuf->GetUnreadSize(), ContentUpdateMaxDescriptorSize);
        Abort(UpdaterResult::Failed, StrUpdaterDescriptorError);
        return;
    }

    vector<uint8_t> data;
    data.resize(data_size);

    _conn.InBuf->Pop(data.data(), data_size);

    vector<vector<uint8_t>> globals_properties_data {};
    _conn.InBuf->ReadPropsData(globals_properties_data);
    auto time {_conn.InBuf->Read<synctime>()};
    ignore_unused(globals_properties_data);

    _gameTime.SetSynchronizedTime(time);

    FO_VERIFY_AND_THROW(!_fileListReceived, "Update file list was already received");
    _fileListReceived = true;

    auto our_target {_binariesMode ? UpdateFileTarget::ClientBinaries : UpdateFileTarget::ClientResources};
    logging::write("Client updater: received update list, bytes {}, mode {}, target {}", data_size, _binariesMode ? "binaries" : "resources", _binariesMode ? "ClientBinaries" : "ClientResources");

    if (data.empty()) {
        if (_binariesMode) {
            logging::write(logging::type::warning, "Client updater: native update list is empty");
            _result = UpdaterResult::ServerMissingNativeUpdate;
        }
        else {
            logging::write("Client updater: resource update list is empty");
            FinishResourcesUpdate();
        }

        return;
    }

    // Parsed and validated whole before anything acts on it, so a malformed list changes nothing on disk
    bool accept_binaries {_binariesMode || CanSelfUpdateNativeModules(GetCurrentUpdatePlatform())};
    string runtime_local_prefix {accept_binaries ? GetCurrentClientRuntimeLibraryName() : string {}};

    try {
        if (_settings->Network.UpdateManifestSignatureRequired) {
            if (_settings->Network.UpdateManifestMinimumReleaseSequence <= 0) {
                throw ContentUpdaterException("Content update minimum release sequence must be positive", _settings->Network.UpdateManifestMinimumReleaseSequence);
            }

            vector<ContentUpdateTrustedPublicKey> trusted_keys;
            trusted_keys.reserve(_settings->Network.UpdateManifestTrustedPublicKeys.size());

            for (const auto& trusted_key_entry : _settings->Network.UpdateManifestTrustedPublicKeys) {
                ContentUpdateTrustedPublicKey trusted_key;

                if (!TryParseContentUpdateTrustedPublicKey(trusted_key_entry, trusted_key)) {
                    throw ContentUpdaterException("Invalid trusted content update public key entry");
                }
                if (std::ranges::any_of(trusted_keys, [&trusted_key](const ContentUpdateTrustedPublicKey& key) { return key.KeyId == trusted_key.KeyId; })) {
                    throw ContentUpdaterException("Duplicate trusted content update public key id", trusted_key.KeyId);
                }

                trusted_keys.emplace_back(trusted_key);
            }

            const uint64_t minimum_release_sequence = numeric_cast<uint64_t>(_settings->Network.UpdateManifestMinimumReleaseSequence);
            const VerifiedContentUpdateManifest verified = VerifyContentUpdateManifestDescriptor(data, GetCurrentBinaryUpdateTargetName(), minimum_release_sequence, trusted_keys);

            if (!AcceptContentUpdateReleaseSequence(_settings->Common.UserWritablePath, verified.ReleaseSequence, minimum_release_sequence)) {
                throw ContentUpdaterException("Content update release sequence was rolled back or could not be persisted", verified.ReleaseSequence);
            }

            _updateManifest = verified.Manifest;
            _signedUpdateDescriptor = data;
        }
        else {
            _updateManifest = DeserializeContentUpdateManifest(data);
            _signedUpdateDescriptor.clear();
        }
    }
    catch (const std::exception& ex) {
        logging::write(logging::type::warning, "Invalid content update descriptor: {}", ex.what());
        Abort(UpdaterResult::Failed, StrUpdaterDescriptorError);
        return;
    }

    string runtime_server_prefix;

    if (accept_binaries) {
        runtime_server_prefix = _settings->GetPackagedRuntimeName();

        if (runtime_server_prefix.empty()) {
            runtime_server_prefix = runtime_local_prefix;
        }

        logging::write("Client updater: binary name remap from server prefix {} to local prefix {}", runtime_server_prefix, runtime_local_prefix);
    }

    auto remap_runtime_name {[&](const string& fname_basename) -> optional<string> {
        if (!fname_basename.starts_with(runtime_server_prefix)) {
            return std::nullopt;
        }

        string_view rest {string_view(fname_basename).substr(runtime_server_prefix.size())};

        if (!rest.empty() && rest[0] != '.') {
            return std::nullopt;
        }

        return strex("{}{}", runtime_local_prefix, rest).str();
    }};

    for (size_t manifest_index = 0; manifest_index < _updateManifest.Files.size(); manifest_index++) {
        const auto& file_info = _updateManifest.Files[manifest_index];
        const auto& fname = file_info.Name;
        const auto size = file_info.Size;
        const auto hash = file_info.Hash;
        const auto target = file_info.Target;
        const auto data_index = file_info.FileIndex;

        string local_name = fname;
        bool is_client_binary = false;
        bool try_patch {true};
        ResourcePackHeader pack_header {file_info.PackHeader};

        if (target == UpdateFileTarget::ClientBinaries) {
            if (!accept_binaries) {
                continue;
            }

            string fname_basename {strex(fname).extract_file_name().str()};
            auto remapped {remap_runtime_name(fname_basename)};

            if (!remapped.has_value()) {
                continue;
            }

            auto remapped_basename {*remapped};
            string fname_dir {strex(fname).extract_dir().str()};
            local_name = fname_dir.empty() ? remapped_basename : strex(fname_dir).combine_path(remapped_basename).str();

            string file_path {strex(_binaryDir).combine_path(local_name).str()};

            if (remapped_basename == strex("{}.pdb", runtime_local_prefix).str()) {
                if (fs::exists(file_path)) {
                    continue;
                }
            }
            else {
                if (!_binariesMode) {
                    continue;
                }

                _hasMatchingEntries = true;
                logging::write("Client updater: matched binary entry {}, local {}, size {}, hash {}", fname, local_name, size, hash);
            }

            is_client_binary = true;

            if (IsDiskFileHashMatch(file_path, size, hash)) {
                logging::write("Client updater: binary already matches {}", file_path);
                continue;
            }
        }
        else if (target == our_target) {
            FO_VERIFY_AND_THROW(size >= RESOURCE_PACK_HEADER_SIZE && pack_header.PackHash == hash && pack_header.DataOffset == RESOURCE_PACK_HEADER_SIZE && pack_header.DataSize <= size - RESOURCE_PACK_HEADER_SIZE && pack_header.IndexOffset == pack_header.DataOffset + pack_header.DataSize && pack_header.IndexOffset <= size && pack_header.IndexStoredSize == size - pack_header.IndexOffset && pack_header.IndexStoredSize <= std::numeric_limits<uint32_t>::max() && pack_header.IndexDecodedSize <= std::numeric_limits<uint32_t>::max(), "Invalid advertised resource pack", fname);
            UpdateFile resource {};
            resource.Index = numeric_cast<int32_t>(data_index);
            resource.Name = fname;
            resource.Size = size;
            resource.RemaningSize = size;
            resource.Hash = hash;
            resource.PackHeader = pack_header;
            _resourceTargets.push_back(resource);
            auto damage {_damagedPacks.find(strex(fname).erase_file_extension().str())};

            if (damage != _damagedPacks.end()) {
                logging::write(logging::type::warning, "Client updater: repairing damaged local pack {}", fname);
                try_patch = damage->second == LocalDamage::Patch;
            }
            else if (IsLocalResourceCurrent(resource)) {
                continue;
            }
        }
        else {
            continue;
        }

        if (!fs::is_contained_relative_path(local_name)) {
            logging::write(logging::type::warning, "Client updater: server listed a file the client cannot place, name {}, local {}", fname, local_name);
            Abort(UpdaterResult::Failed, StrUpdateFailed);
            return;
        }

        UpdateFile update_file {};
        update_file.Index = numeric_cast<int32_t>(data_index);
        update_file.ManifestIndex = manifest_index;
        update_file.Name = local_name;
        update_file.Size = size;
        update_file.RemaningSize = size;
        update_file.Hash = hash;
        update_file.Sha256 = file_info.Sha256;
        update_file.IsClientBinary = is_client_binary;
        update_file.TryPatch = try_patch;
        update_file.PackHeader = pack_header;
        update_file.UseFastUpdate = _settings->Network.FastUpdateEnabled && _updateManifest.FastUpdateEnabled && !_updateManifest.Endpoints.empty() && _updateManifest.ChunkSize != 0 && _updateManifest.ChunkSize <= ContentUpdateMaxChunkPayloadSize && !file_info.ChunkHashes.empty();
        _filesToUpdate.emplace_back(std::move(update_file));
    }

    if (!_filesToUpdate.empty()) {
        logging::write("Client updater: {} files need update in {} mode", _filesToUpdate.size(), _binariesMode ? "binaries" : "resources");
        GetNextFile();
    }
    else if (_binariesMode) {
        if (_hasMatchingEntries) {
            logging::write("Client updater: binaries already match, requesting reload");
            _result = UpdaterResult::BinariesStaged;
        }
        else {
            logging::write(logging::type::warning, "Client updater: server has no matching native update payload");
            _result = UpdaterResult::ServerMissingNativeUpdate;
        }
    }
    else {
        logging::write("Client updater: no files to update");
        FinishResourcesUpdate();
    }
}

void Updater::Net_OnTimeSync()
{
    FO_TRACE_ZONE(Core);

    auto time {_conn.InBuf->Read<synctime>()};
    _gameTime.SetSynchronizedTimeMonotonic(time);
}

void Updater::Net_OnHashList()
{
    FO_TRACE_ZONE(Core);

    uint32_t count {_conn.InBuf->Read<uint32_t>()};

    for (uint32_t i {0}; i < count; i++) {
        string str {_conn.InBuf->Read<string>()};

        (void)_hashStorage.to_hashed_string(str);
    }

    if (count != 0) {
        logging::write("Client updater: learned {} previously unresolved hash(es) from server", count);
    }
}

void Updater::Net_OnUpdateFileData()
{
    FO_TRACE_ZONE(Network);

    int32_t data_size_raw {_conn.InBuf->Read<int32_t>()};

    if (data_size_raw < 0) {
        Abort(UpdaterResult::Failed, StrFilesystemError);
        return;
    }

    auto data_size {numeric_cast<size_t>(data_size_raw)};

    if (data_size > _conn.InBuf->GetUnreadSize()) {
        Abort(UpdaterResult::Failed, StrUpdateFailed);
        return;
    }

    _updateFileBuf.resize(data_size);

    _conn.InBuf->Pop(_updateFileBuf.data(), data_size);

    if (_resourceRange != ResourceRange::None) {
        if (_filesToUpdate.empty() || _rangeData.size() > _rangeSize || data_size > _rangeSize - _rangeData.size() || (data_size == 0 && _rangeData.size() != _rangeSize)) {
            Abort(UpdaterResult::Failed, StrUpdateFailed);
            return;
        }

        _rangeData.insert(_rangeData.end(), _updateFileBuf.begin(), _updateFileBuf.end());

        if (_rangeData.size() == _rangeSize) {
            FinishResourceRange();
        }
        else {
            RequestResourceRange();
        }

        return;
    }

    if (_filesToUpdate.empty() || !static_cast<bool>(_tempFile)) {
        Abort(UpdaterResult::Failed, StrFilesystemError);
        return;
    }

    auto& update_file {_filesToUpdate.front()};

    if (numeric_cast<uint64_t>(data_size) > update_file.RemaningSize) {
        Abort(UpdaterResult::Failed, StrFilesystemError);
        return;
    }

    // Write data to temp file
    size_t write_size {GetUpdateWriteSize(update_file.RemaningSize, _updateFileBuf.size())};

    if (!_tempFile.write({_updateFileBuf.data(), write_size})) {
        Abort(UpdaterResult::Failed, StrFilesystemError);
        return;
    }

    update_file.RemaningSize -= data_size;

    if (update_file.RemaningSize > 0) {
        if (data_size == 0) {
            Abort(UpdaterResult::Failed, StrFilesystemError);
            return;
        }

        RequestUpdateFile(update_file);
        _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
    }
    else {
        GetNextFile();
    }
}

auto Updater::FinalizeCurrentFile() -> FinalizeResult
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(!_filesToUpdate.empty(), "No update file to finalize");

    if (_tempFile) {
        if (!_tempFile.flush()) {
            return FinalizeResult::FileSystemFailure;
        }

        _tempFile.close();
    }

    const auto& update_file = _filesToUpdate.front();
    const auto final_path = MakeFinalPath(update_file);
    const auto temp_path = MakeTempPath(update_file);

    if (!IsDiskFileHashMatch(temp_path, update_file.Size, update_file.Hash) || (!update_file.Sha256AlreadyVerified && !IsDiskFileSha256Match(temp_path, update_file.Size, update_file.Sha256))) {
        logging::write("Client updater: downloaded file hash or SHA-256 mismatch, temp {}, file {}", temp_path, update_file.Name);
        return FinalizeResult::IntegrityMismatch;
    }

    if (!ReplaceFileSafely(temp_path, final_path)) {
        logging::write("Client updater: failed to promote downloaded file from {} to {}", temp_path, final_path);
        return FinalizeResult::FileSystemFailure;
    }

    logging::write("Client updater: promoted downloaded file to {}, binary {}", final_path, update_file.IsClientBinary ? "yes" : "no");
    if (!TryPromoteStagedBinary(update_file, final_path)) {
        return FinalizeResult::FileSystemFailure;
    }
    _filesToUpdate.erase(_filesToUpdate.begin());
    return FinalizeResult::Succeeded;
}

auto Updater::MakeFileOutputDir(const UpdateFile& update_file) const -> string
{
    FO_TRACE_ZONE(Core);

    return update_file.IsClientBinary ? _binaryDir : GetClientWritableResourceDir(*_settings);
}

auto Updater::MakeTempPath(const UpdateFile& update_file) const -> string
{
    FO_TRACE_ZONE(Core);

    return strex(MakeFileOutputDir(update_file)).combine_path(strex("~{}", update_file.Name)).str();
}

auto Updater::MakeExternalCandidatePath(const UpdateFile& update_file, size_t source_index) const -> string
{
    FO_TRACE_ZONE(Core);

    return strex("{}.__external.{}", MakeTempPath(update_file), source_index).str();
}

void Updater::CleanupStaleTempFiles(const UpdateFile& update_file, string_view current_temp_path) const
{
    FO_TRACE_ZONE(Core);

    const string legacy_temp_path = strex(MakeFileOutputDir(update_file)).combine_path(strex("~{}", update_file.Name)).str();
    const std::filesystem::path legacy_path {fs::make_path(legacy_temp_path)};
    const std::filesystem::path current_path {fs::make_path(current_temp_path)};
    const std::filesystem::path directory = legacy_path.parent_path();

    if (directory.empty()) {
        return;
    }

    const string legacy_name = fs::path_to_string(legacy_path.filename());
    const string current_name = fs::path_to_string(current_path.filename());
    const string digest_prefix = strex("{}.__sha256.", legacy_name).str();
    const string legacy_external_prefix = strex("{}.__external.", legacy_name).str();
    const string legacy_fast_prefix = strex("{}.__fastupd.", legacy_name).str();
    std::error_code error;
    std::filesystem::directory_iterator entry {directory, error};
    const std::filesystem::directory_iterator end;

    while (!error && entry != end) {
        std::error_code type_error;

        if (entry->is_regular_file(type_error)) {
            const string name = fs::path_to_string(entry->path().filename());
            const bool stale_digest_file = name != current_name && name.starts_with(digest_prefix);
            const bool stale_legacy_file = name != current_name && (name == legacy_name || name.starts_with(legacy_external_prefix) || name.starts_with(legacy_fast_prefix));

            if (stale_digest_file || stale_legacy_file) {
                (void)fs::remove_file(fs::path_to_string(entry->path()));
            }
        }

        entry.increment(error);
    }
}

auto Updater::MakeLivePath(const UpdateFile& update_file) const -> string
{
    FO_TRACE_ZONE(Core);

    return strex(MakeFileOutputDir(update_file)).combine_path(update_file.Name).str();
}

auto Updater::MakeFinalPath(const UpdateFile& update_file) const -> string
{
    FO_TRACE_ZONE(Core);

    const auto live_path = MakeLivePath(update_file);
    return update_file.IsClientBinary ? strex("{}{}", live_path, ClientBinaryStagingSuffix).str() : live_path;
}

auto Updater::TryPromoteStagedBinary(const UpdateFile& update_file, string_view staged_path) -> bool
{
    FO_TRACE_ZONE(Core);

    if (!update_file.IsClientBinary) {
        return true;
    }

    const string live_path = MakeLivePath(update_file);
    const bool is_runtime = fs::resolve_path(live_path) == fs::resolve_path(GetRuntimeLivePath());
    string authorization_path;

    if (is_runtime && _settings->Network.UpdateManifestSignatureRequired) {
        authorization_path = MakeClientRuntimeAuthorizationPath(live_path);

        if (_signedUpdateDescriptor.empty() || !fs::write_file(authorization_path, _signedUpdateDescriptor)) {
            logging::write(logging::type::warning, "Client updater: failed to persist signed authorization for staged runtime {}", staged_path);
            (void)fs::remove_file(staged_path);
            return false;
        }
    }

    if (ReplaceFileSafely(staged_path, live_path) && !authorization_path.empty()) {
        (void)fs::remove_file(authorization_path);
    }

    return true;
}

auto Updater::IsDiskFileHashMatch(string_view file_path, uint64_t expected_size, uint64_t expected_hash) -> bool
{
    FO_TRACE_ZONE(FileSystem);

    auto local_size {fs::file_size(file_path)};

    if (!local_size.has_value() || *local_size != expected_size) {
        return false;
    }

    if (IsResourcePackName(file_path)) {
        return VerifyResourcePackFile(file_path, expected_hash);
    }

    struct CachedHash
    {
        uint64_t Size {};
        uint64_t Mtime {};
        uint64_t Hash {};
    };
    static_assert(std::is_trivially_copyable_v<CachedHash>);

    uint64_t local_mtime {fs::last_write_time(file_path)};

    // Keyed by the whole path: two same-named files in different directories would otherwise share one
    // entry, and a size plus write-time collision would answer this check with the other file's hash
    string cache_key {strex("{}-{:016x}.hash", strex(file_path).extract_file_name(), hashing::hash<string_view> {}(file_path)).str()};

    if (_cache.HasEntry(cache_key)) {
        auto data {_cache.GetData(cache_key)};

        if (data.size() == sizeof(CachedHash)) {
            CachedHash cached {};
            auto target {make_ptr(&cached).reinterpret_as<uint8_t>()};
            memory::copy(target, data.data(), sizeof(cached));

            if (cached.Size == *local_size && cached.Mtime == local_mtime) {
                return cached.Hash == expected_hash;
            }
        }
    }

    auto local_hash {fs::hash_file(file_path)};

    if (!local_hash.has_value()) {
        return false;
    }

    CachedHash entry {*local_size, local_mtime, *local_hash};
    _cache.SetData(cache_key, const_span<uint8_t> {make_ptr(&entry).reinterpret_as<uint8_t>().get(), sizeof(CachedHash)});

    return *local_hash == expected_hash;
}

auto Updater::IsDiskFileSha256Match(string_view file_path, uint64_t expected_size, const Sha256Digest& expected_sha256) const -> bool
{
    FO_TRACE_ZONE(Core);

    const auto local_size = fs::file_size(file_path);

    if (!local_size.has_value() || *local_size != expected_size) {
        return false;
    }

    const auto local_sha256 = fs::sha256_file(file_path);
    return local_sha256.has_value() && *local_sha256 == expected_sha256;
}

auto Updater::IsExternalSourceExpired(const ContentUpdateSource& source) const -> bool
{
    FO_TRACE_ZONE(Core);

    return source.ExpiresAt != 0 && source.ExpiresAt <= _gameTime.GetSynchronizedTime().milliseconds();
}

auto Updater::IsIdentityVerified(string_view path, const VerifiedFileIdentity& identity) const -> bool
{
    static_assert(std::is_trivially_copyable_v<VerifiedFileIdentity>);
    string cache_key {MakeVerifiedIdentityKey(path)};

    if (!_cache.HasEntry(cache_key)) {
        return false;
    }

    vector<uint8_t> data {_cache.GetData(cache_key)};

    if (data.size() != sizeof(VerifiedFileIdentity)) {
        return false;
    }

    VerifiedFileIdentity cached {};
    memory::copy(make_ptr(&cached).reinterpret_as<uint8_t>(), data.data(), sizeof(cached));
    return cached.Size == identity.Size && cached.WriteTime == identity.WriteTime && cached.Content == identity.Content;
}

void Updater::RecordVerifiedIdentity(string_view path, const VerifiedFileIdentity& identity)
{
    // Only saves the next run a read, so a record that cannot be written costs that read and nothing else
    (void)_cache.SetDataChecked(MakeVerifiedIdentityKey(path), const_span<uint8_t> {make_ptr(&identity).reinterpret_as<const uint8_t>().get(), sizeof(identity)});
}

void Updater::RecordVerifiedBase(string_view base_path)
{
    if (optional<VerifiedFileIdentity> identity {ReadBaseIdentity(base_path)}; identity.has_value()) {
        RecordVerifiedIdentity(base_path, identity.value());
    }
}

void Updater::RecordVerifiedPatch(string_view pack_name)
{
    string base_path {GetClientResourcePackPath(*_settings, pack_name)};
    string patch_path {GetClientResourcePatchPath(*_settings, pack_name)};

    if (optional<VerifiedFileIdentity> identity {ReadPatchIdentity(patch_path, base_path)}; identity.has_value()) {
        RecordVerifiedIdentity(patch_path, identity.value());
    }
}

auto Updater::ReadBaseIdentity(string_view base_path) -> optional<VerifiedFileIdentity>
{
    fs::disk_read_file file {OpenResourcePackFile(base_path)};
    ResourcePackHeader header {};

    if (!ReadResourcePackHeader(file, header)) {
        return std::nullopt;
    }

    return VerifiedFileIdentity {.Size = file.get_size(), .WriteTime = GetResourcePackWriteTime(base_path), .Content = header.PackHash};
}

auto Updater::ReadPatchIdentity(string_view patch_path, string_view base_path) -> optional<VerifiedFileIdentity>
{
    ResourcePackHeader header {};
    fs::disk_read_file file {patch_path};

    if (!file || !ReadResourcePackHeader(base_path, header)) {
        return std::nullopt;
    }

    // The commit's catalog hash names the commit; a patch the reader cannot parse has no commit to prove, and mounting
    // the pair reports it as not current instead
    try {
        optional<ResourcePatchInfo> info {ReadResourcePatchInfo(file, header)};

        if (!info.has_value()) {
            return std::nullopt;
        }

        return VerifiedFileIdentity {.Size = file.get_size(), .WriteTime = fs::last_write_time(patch_path), .Content = info->IndexHash};
    }
    catch (const std::exception& ex) {
        logging::write(logging::type::warning, "Client updater: can't read resource patch {}, {}", patch_path, ex.what());
        return std::nullopt;
    }
}

void Updater::RecoverInterruptedReplacements() const
{
    FO_TRACE_ZONE(FileSystem);

    auto recover_in_dir {[](string_view dir) {
        if (!fs::is_dir(dir)) {
            return;
        }

        for (const string& name : fs::list_dir_file_names(dir, true)) {
            if (!name.ends_with(REPLACED_FILE_BACKUP_SUFFIX) || strex(name).extract_file_name().str() == REPLACED_FILE_BACKUP_SUFFIX) {
                continue;
            }

            string_view live_name {string_view {name}.substr(0, name.size() - REPLACED_FILE_BACKUP_SUFFIX.size())};

            string backup_path {strex(dir).combine_path(name).str()};
            string live_path {strex(dir).combine_path(live_name).str()};

            // ReplaceFileSafely moves the installed file aside before it renames the new one over it, so a
            // backup with nothing in its place is the only copy left and putting it back is the repair
            if (fs::exists(live_path)) {
                logging::write("Client updater: removing obsolete backup {}", backup_path);
                (void)fs::remove_file(backup_path);
            }
            else if (fs::rename_durable(backup_path, live_path)) {
                logging::write(logging::type::warning, "Client updater: restored {} from an interrupted replacement", live_path);
            }
            else {
                logging::write(logging::type::warning, "Client updater: can't restore {} from {}", live_path, backup_path);
            }
        }
    }};

    // An installed binary root also contains Resources, so the run's lock covers both recursive scans
    FO_VERIFY_AND_THROW(_directoryLock, "Interrupted replacements are recovered without the resource directory lock");
    string resources_dir {GetClientWritableResourceDir(*_settings)};
    recover_in_dir(resources_dir);

    if (_binaryDir != resources_dir) {
        recover_in_dir(_binaryDir);
    }
}

void Updater::RemoveStaleTempPacks() const
{
    FO_TRACE_ZONE(FileSystem);

    string resources_dir {GetClientWritableResourceDir(*_settings)};

    if (!fs::is_dir(resources_dir)) {
        return;
    }

    FO_VERIFY_AND_THROW(_directoryLock, "Stale temp packs are swept without the resource directory lock", resources_dir);
    unordered_set<string> wanted {};

    for (const auto& update_file : _filesToUpdate) {
        wanted.emplace(strex("~{}", update_file.Name).str());
    }

    // fs::iterate_dir hides names starting with '~', which is exactly the set this sweep is looking for
    for (const auto& name : fs::list_dir_file_names(resources_dir)) {
        // A temp pack is kept only while its pack still has to be updated: that is the resume point this run is
        // about to continue from, and nothing ever resumes any other
        if (!name.starts_with('~') || !IsResourcePackName(name) || wanted.count(name) != 0) {
            continue;
        }

        string stale_path {strex(resources_dir).combine_path(name).str()};
        logging::write("Client updater: removing stale temp pack {}", stale_path);
        (void)fs::remove_file(stale_path);
    }
}

auto Updater::IsDownloadedFileHashMatch(string_view file_path, const UpdateFile& update_file) -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (IsResourcePackName(update_file.Name)) {
        auto local_size {fs::file_size(file_path)};

        if (!local_size || *local_size != update_file.Size || !VerifyResourcePackFile(file_path, update_file.Hash)) {
            return false;
        }

        try {
            ResourcePackSource resource {file_path};
            return resource.GetContentHash() == update_file.PackHeader.ContentHash;
        }
        catch (const std::exception& ex) {
            logging::write(logging::type::warning, "Client updater: invalid downloaded resource catalog {}, {}", file_path, ex.what());
            return false;
        }
    }

    auto local_size {fs::file_size(file_path)};

    if (!local_size || *local_size != update_file.Size) {
        return false;
    }

    auto local_hash {fs::hash_file(file_path)};
    return local_hash.has_value() && *local_hash == update_file.Hash;
}

auto Updater::IsResourcePackName(string_view file_name) noexcept -> bool
{
    return strex(file_name).get_file_extension() == "fores";
}

auto Updater::IsDataHashMatch(const vector<uint8_t>& data, uint64_t expected_size, uint64_t expected_hash) noexcept -> bool
{
    return numeric_cast<uint64_t>(data.size()) == expected_size && fs::hash_data(data) == expected_hash;
}

auto Updater::GetDiskFileSize(string_view file_path) -> optional<uint64_t>
{
    return fs::file_size(file_path);
}

auto Updater::GetUpdateWriteSize(uint64_t remaining_size, size_t received_size) -> size_t
{
    return remaining_size < numeric_cast<uint64_t>(received_size) ? numeric_cast<size_t>(remaining_size) : received_size;
}

auto Updater::ReplaceFileSafely(string_view temp_path, string_view final_path) -> bool
{
    FO_TRACE_ZONE(FileSystem);

    string backup_path {strex("{}{}", final_path, REPLACED_FILE_BACKUP_SUFFIX).str()};
    bool final_exists {fs::exists(final_path)};

    (void)fs::remove_file(backup_path);

    if (final_exists && !fs::rename_durable(final_path, backup_path)) {
        return false;
    }

    if (!fs::rename_durable(temp_path, final_path)) {
        if (final_exists) {
            (void)fs::rename_durable(backup_path, final_path);
        }

        return false;
    }

    if (final_exists) {
        (void)fs::remove_file(backup_path);
    }

    return true;
}

auto Updater::GetRuntimeLivePath() const -> string
{
    return strex("{}{}", strex(_binaryDir).combine_path(GetCurrentClientRuntimeLibraryName()), GetClientRuntimeLibraryExtension()).str();
}

auto GetCurrentUpdatePlatform() noexcept -> UpdatePlatform
{
#if FO_WINDOWS
    return UpdatePlatform::Windows;
#elif FO_LINUX
    return UpdatePlatform::Linux;
#elif FO_ANDROID
    return UpdatePlatform::Android;
#elif FO_MAC
    return UpdatePlatform::MacOS;
#elif FO_IOS
    return UpdatePlatform::IOS;
#elif FO_WEB
    return UpdatePlatform::Web;
#else
#error "Unknown update platform"
#endif
}

auto GetUpdatePlatformName(UpdatePlatform platform) noexcept -> string_view
{
    switch (platform) {
    case UpdatePlatform::Windows:
        return "Windows";
    case UpdatePlatform::Linux:
        return "Linux";
    case UpdatePlatform::Android:
        return "Android";
    case UpdatePlatform::MacOS:
        return "macOS";
    case UpdatePlatform::IOS:
        return "iOS";
    case UpdatePlatform::Web:
        return "Web";
    case UpdatePlatform::Unknown:
    default:
        return "Unknown";
    }
}

auto GetCurrentBinaryUpdateTargetName() noexcept -> string_view
{
#if FO_WINDOWS

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    return "Windows-win64";
#elif defined(_M_IX86) || defined(__i386__)
    return "Windows-win32";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "Windows-arm64";
#else
    return "Windows-unknown";
#endif

#elif FO_LINUX

#if defined(__x86_64__)
    return "Linux-x64";
#elif defined(__aarch64__)
    return "Linux-arm64";
#elif defined(__i386__)
    return "Linux-x86";
#elif defined(__arm__)
    return "Linux-arm";
#else
    return "Linux-unknown";
#endif

#elif FO_ANDROID

#if defined(__aarch64__)
    return "Android-arm64";
#elif defined(__i386__)
    return "Android-x86";
#elif defined(__arm__)
    return "Android-arm32";
#else
    return "Android-unknown";
#endif

#elif FO_MAC

#if defined(__aarch64__)
    return "macOS-arm64";
#elif defined(__x86_64__)
    return "macOS-x64";
#else
    return "macOS-unknown";
#endif

#elif FO_IOS

#if defined(__aarch64__)
    return "iOS-arm64";
#elif defined(__x86_64__)
    return "iOS-simulator";
#else
    return "iOS-unknown";
#endif

#elif FO_WEB
    return "Web-wasm";
#else
#error "Unknown binary update target"
#endif
}

auto CanSelfUpdateNativeModules(UpdatePlatform platform) noexcept -> bool
{
    switch (platform) {
    case UpdatePlatform::Windows:
    case UpdatePlatform::Linux:
    case UpdatePlatform::MacOS:
        return true;
    case UpdatePlatform::Android:
    case UpdatePlatform::IOS:
    case UpdatePlatform::Web:
    case UpdatePlatform::Unknown:
    default:
        return false;
    }
}

auto GetClientBinaryDir(string_view user_writable_path) -> string
{
    // A writable root holds everything this client writes, the modules it replaces included; without one
    // the client is portable and owns its own directory
    if (!user_writable_path.empty()) {
        return string(user_writable_path);
    }

    if constexpr (FO_WEB) {
        // The web client runs from the virtual filesystem root and has no on-disk exe path
        return "/";
    }
    else {
        auto exe_path {platform::get_exe_path()};
        FO_VERIFY_AND_THROW(exe_path.has_value(), "Executable path could not be resolved");
        return strex(exe_path.value()).extract_dir().str();
    }
}

auto GetClientRuntimeLivePath() -> string
{
    // Always the module shipped beside the executable: an update never replaces the host, so this stays
    // the base runtime a selector may point away from
    string binary_dir {GetClientBinaryDir("")};
    return strex("{}{}", strex(binary_dir).combine_path(GetCurrentClientRuntimeLibraryName()), GetClientRuntimeLibraryExtension()).str();
}

auto MakeClientRuntimeBootstrapPath(string_view user_writable_path) -> optional<string>
{
    // Nothing to select without a writable root: the module then lives beside the exe and is replaced
    // in place, which the host finds on its own
    if (user_writable_path.empty()) {
        return std::nullopt;
    }

    string selector_name {strex("{}{}{}", GetCurrentClientRuntimeLibraryName(), GetClientRuntimeLibraryExtension(), ClientRuntimeBootstrapExtension).str()};
    return fs::resolve_path(fs::make_writable_path(user_writable_path, selector_name));
}

auto MakeClientRuntimeStagingPath(string_view runtime_live_path) -> string
{
    return strex("{}{}", runtime_live_path, ClientBinaryStagingSuffix).str();
}

auto ResolveClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view expected_runtime_file_name, string_view fallback_runtime_path) -> string
{
    optional<string> target {ReadClientRuntimeBootstrapTarget(bootstrap_file_path, expected_runtime_file_name)};

    if (!target.has_value()) {
        return string(fallback_runtime_path);
    }

    string staging_path {MakeClientRuntimeStagingPath(target.value())};
    bool live_exists {fs::exists(target.value()) && !fs::is_dir(target.value())};
    bool staging_exists {fs::exists(staging_path) && !fs::is_dir(staging_path)};
    return live_exists || staging_exists ? target.value() : string(fallback_runtime_path);
}

auto ReadClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view expected_runtime_file_name) -> optional<string>
{
    FO_TRACE_ZONE(FileSystem);

    if (!fs::is_absolute_path(bootstrap_file_path)) {
        return std::nullopt;
    }

    optional<uint64_t> file_size {fs::file_size(bootstrap_file_path)};

    if (!file_size.has_value() || file_size.value() == 0 || file_size.value() > ClientRuntimeBootstrapMaxSize) {
        return std::nullopt;
    }

    optional<string> content {fs::read_file(bootstrap_file_path)};

    if (!content.has_value() || content->size() > ClientRuntimeBootstrapMaxSize) {
        return std::nullopt;
    }

    return NormalizeClientRuntimeBootstrapTarget(content.value(), expected_runtime_file_name);
}

auto WriteClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view runtime_path, string_view expected_runtime_file_name) -> bool
{
    FO_TRACE_ZONE(FileSystem);

    if (!fs::is_absolute_path(bootstrap_file_path)) {
        return false;
    }

    optional<string> normalized_path {NormalizeClientRuntimeBootstrapTarget(runtime_path, expected_runtime_file_name)};

    if (!normalized_path.has_value() || normalized_path->size() + 1 > ClientRuntimeBootstrapMaxSize) {
        return false;
    }

    return fs::write_file(bootstrap_file_path, strex("{}\n", normalized_path.value()).str());
}

auto MakeClientRuntimeAuthorizationPath(string_view runtime_live_path) -> string
{
    FO_TRACE_ZONE(Core);

    return strex("{}.auth", MakeClientRuntimeStagingPath(runtime_live_path)).str();
}

void PromoteStagedRuntimeCompanions(string_view binary_dir) noexcept
{
    FO_TRACE_ZONE(FileSystem);

    try {
        string runtime_name {GetCurrentClientRuntimeLibraryName()};
        string runtime_primary_name {strex("{}{}", runtime_name, GetClientRuntimeLibraryExtension()).str()};

        vector<pair<string, string>> renames {};

        fs::iterate_dir(binary_dir, false, [&](string_view path, size_t, uint64_t) {
            string file_name {strex(path).extract_file_name().str()};

            if (!file_name.ends_with(ClientBinaryStagingSuffix)) {
                return;
            }

            auto unstaged_name {file_name.substr(0, file_name.size() - ClientBinaryStagingSuffix.size())};

            if (unstaged_name == runtime_primary_name) {
                return;
            }

            bool matches_runtime {unstaged_name == runtime_name || (unstaged_name.size() > runtime_name.size() && unstaged_name.starts_with(runtime_name) && unstaged_name[runtime_name.size()] == '.')};

            if (!matches_runtime) {
                return;
            }

            renames.emplace_back(string(path), strex(binary_dir).combine_path(unstaged_name).str());
        });

        for (const auto& [staged, final_path] : renames) {
            (void)fs::remove_file(final_path);
            (void)fs::rename(staged, final_path);
        }
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
}

auto GetCurrentClientRuntimeLibraryName() -> string
{
    if (auto exe_path {platform::get_exe_path()}; exe_path.has_value()) {
        string name {strex(exe_path.value()).extract_file_name().erase_file_extension().str()};

        if (!name.empty()) {
            return name;
        }
    }

    return string(FO_DEV_NAME);
}

static auto UpdaterResultToString(UpdaterResult result) noexcept -> string_view
{
    switch (result) {
    case UpdaterResult::ResourcesReady:
        return "ResourcesReady";
    case UpdaterResult::BinariesStaged:
        return "BinariesStaged";
    case UpdaterResult::SettingsRestartRequired:
        return "SettingsRestartRequired";
    case UpdaterResult::PlatformUnsupported:
        return "PlatformUnsupported";
    case UpdaterResult::ServerMissingNativeUpdate:
        return "ServerMissingNativeUpdate";
    case UpdaterResult::UpdaterOutdated:
        return "UpdaterOutdated";
    case UpdaterResult::Failed:
        return "Failed";
    case UpdaterResult::MetadataMismatch:
        return "MetadataMismatch";
    case UpdaterResult::ConnectionFailed:
        return "ConnectionFailed";
    default:
        return "Unknown";
    }
}

auto IsUpdaterFailureReportable(UpdaterResult result) noexcept -> bool
{
    // Client-local updater failures are reportable; transient server downtime would otherwise emit one crash
    // per player for every restart
    return result != UpdaterResult::ConnectionFailed && result != UpdaterResult::SettingsRestartRequired;
}

// Reported, not thrown: the caller still owns the dialog and the quit that follows. Constructing the
// exception is what carries a fixed message, context values and a stack trace into the crash reporter
static void ReportUpdaterFailure(UpdaterResult result, string_view target_name) noexcept
{
    safe_call([&] {
        ClientUpdateException ex {"Client update did not complete", UpdaterResultToString(result), target_name, GetUpdatePlatformName(GetCurrentUpdatePlatform()), FO_BUILD_HASH, FO_COMPATIBILITY_VERSION};
        exceptions::report_and_continue(ex);
    });
}

void ShowUpdaterFailure(UpdaterResult result)
{
    string_view target_name {GetCurrentBinaryUpdateTargetName()};

    logging::write(logging::type::warning, "Client updater: terminal result {}, binary target {}", UpdaterResultToString(result), target_name);

    // Report terminal client failures before showing the dialog. The unconditional log still records
    // deliberately unreported failures
    if (IsUpdaterFailureReportable(result)) {
        ReportUpdaterFailure(result, target_name);
    }

    switch (result) {
    case UpdaterResult::ServerMissingNativeUpdate:
        Application::ShowErrorMessage(strex(strex::dynamic_format, StrServerMissingNativeUpdate, target_name).str(), StrErrorMessageCaption, true);
        break;
    case UpdaterResult::UpdaterOutdated:
        Application::ShowErrorMessage(StrUpdaterOutdated, StrErrorMessageCaption, true);
        break;
    case UpdaterResult::PlatformUnsupported:
        Application::ShowErrorMessage(StrPlatformUnsupported, StrErrorMessageCaption, true);
        break;
    case UpdaterResult::MetadataMismatch:
        Application::ShowErrorMessage(StrMetadataMismatch, StrErrorMessageCaption, true);
        break;
    case UpdaterResult::Failed:
        Application::ShowErrorMessage(StrUpdateFailed, StrErrorMessageCaption, true);
        break;
    case UpdaterResult::ConnectionFailed:
        Application::ShowErrorMessage(StrServerUnavailable, StrErrorMessageCaption, true);
        break;
    case UpdaterResult::ResourcesReady:
    case UpdaterResult::BinariesStaged:
    case UpdaterResult::SettingsRestartRequired:
    default:
        break;
    }
}

auto GetClientRuntimeLibraryExtension() noexcept -> string_view
{
#if FO_WINDOWS
    return ".dll";
#elif FO_LINUX
    return ".so";
#elif FO_MAC
    return ".dylib";
#else
    return {};
#endif
}

static auto NormalizeClientRuntimeBootstrapTarget(string_view runtime_path, string_view expected_runtime_file_name) -> optional<string>
{
    string trimmed_path {strex(runtime_path).trim().str()};

    if (trimmed_path.empty() || expected_runtime_file_name.empty() || !fs::is_absolute_path(trimmed_path) || trimmed_path.find('\0') != string::npos || trimmed_path.find('\r') != string::npos || trimmed_path.find('\n') != string::npos) {
        return std::nullopt;
    }

    if (strex(trimmed_path).extract_file_name().str() != expected_runtime_file_name) {
        return std::nullopt;
    }

    return fs::resolve_path(trimmed_path);
}

static auto MakeVerifiedIdentityKey(string_view path) -> string
{
    // Keyed by the whole path, since an installed and a downloaded base share a file name
    return strex("{}-{:016x}.verified", strex(path).extract_file_name(), hashing::hash<string_view> {}(path)).str();
}

static auto IsResumablePackPrefix(string_view temp_path, const ResourcePackHeader& advertised) -> bool
{
    // A download resumes by length alone, so a prefix another server build left behind would be completed into a
    // pack that fails verification. Its first bytes are that build's header, which says whose prefix it is
    vector<uint8_t> expected {SerializeResourcePackHeader(advertised)};
    fs::disk_read_file file {temp_path};
    vector<uint8_t> prefix {vector<uint8_t>(numeric_cast<size_t>(std::min<uint64_t>(file.get_size(), expected.size())))};
    return file && file.read_at(0, prefix) && std::equal(prefix.begin(), prefix.end(), expected.begin());
}

FO_END_NAMESPACE
