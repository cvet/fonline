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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Updater.h"
#include "Application.h"
#include "DefaultSprites.h"

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
static constexpr string_view StrErrorMessageCaption = "";
static constexpr string_view StrUpdaterDescriptorError = "Update descriptor error!";
static constexpr string_view StrExternalUpdateStarted = "External updater: download from mirror";
static constexpr string_view StrExternalUpdateFallback = "External mirror failed, trying fallback";
static constexpr string_view StrFastUpdateStarted = "Fast updater: download from UDP mirrors";
static constexpr string_view StrFastUpdateFallback = "Fast updater failed, fallback to game channel";

static constexpr string_view ClientBinaryStagingSuffix = "-staging";
static constexpr uint64_t ClientRuntimeBootstrapMaxSize = 4096;

static auto NormalizeClientRuntimeBootstrapTarget(string_view runtime_path, string_view expected_runtime_file_name) -> optional<string>;

extern void SetupContentUpdateTransportsHook(ContentUpdateTransportRegistry& registry, GlobalSettings& settings);

Updater::Updater(ptr<GlobalSettings> settings, ptr<IAppWindow> window) :
    _settings {settings},
    _conn(settings),
    _cache(fs_make_writable_path(settings->UserWritablePath, settings->CacheResources)),
    _binaryDir {settings->UserWritablePath.empty() ? strex(GetClientRuntimeLivePath()).extract_dir().str() : string(settings->UserWritablePath)},
    _gameTime(settings),
    _effectMngr(settings, make_ptr(&_resources), window->GetRender()),
    _sprMngr(settings, window, make_ptr(&_resources), make_ptr(&_gameTime), make_ptr(&_effectMngr), make_ptr(&_hashStorage)),
    _fontMngr(make_ptr(&_sprMngr))
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Client updater: created for {}:{}, compatibility {}, binary dir {}, resources {}", _settings->ServerHost, _settings->ServerPort, _settings->CompatibilityVersion, _binaryDir, _settings->ClientResources);

    try {
        SetupContentUpdateTransportsHook(_contentUpdateTransports, *settings);
    }
    catch (const std::exception&) {
        WriteLog(LogType::Warning, "External updater transports are unavailable for this session: setup failure");
    }
    catch (...) {
        WriteLog(LogType::Warning, "External updater transports are unavailable for this session: unknown setup failure");
    }

    _startTime = nanotime::now();

    _resources.AddPackSource(IsPackaged() ? settings->ClientResources : settings->BakeOutput, "Embedded");
    _resources.AddDirSource(_settings->ClientResources, false, true, true);

    if (!settings->UserWritablePath.empty()) {
        _resources.AddDirSource(fs_make_writable_path(settings->UserWritablePath, settings->ClientResources), false, true, true);
    }
    if (!_settings->DefaultSplashPack.empty()) {
        _resources.AddPackSource(IsPackaged() ? _settings->ClientResources : _settings->BakeOutput, _settings->DefaultSplashPack, true);
    }

    _effectMngr.LoadMinimalEffects();

    _sprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<DefaultSpriteFactory>(&_sprMngr));

    // Wait screen
    if (!_settings->DefaultSplash.empty()) {
        _splashPic = _sprMngr.LoadSprite(_settings->DefaultSplash, AtlasType::OneImage);

        if (_splashPic) {
            _splashPic->PlayDefault();
        }
    }

    _sprMngr.BeginScene();

    if (_splashPic) {
        _sprMngr.DrawSpriteSize(_splashPic, {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, Color::Neutral);
    }

    _sprMngr.EndScene();

    // Load font
    _fontMngr.BindFoFont(FontType::Default, "Fonts/Default.fofnt", AtlasType::IfaceSprites, false, true);

    // Network handlers
    _conn.SetConnectHandler([this](ClientConnection::ConnectResult result) FO_DEFERRED { Net_OnConnect(result); });
    _conn.SetDisconnectHandler([this]() FO_DEFERRED { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NetMessage::InitData, [this]() FO_DEFERRED { Net_OnInitData(); });
    _conn.AddMessageHandler(NetMessage::TimeSync, [this]() FO_DEFERRED { Net_OnTimeSync(); });
    _conn.AddMessageHandler(NetMessage::HashList, [this]() FO_DEFERRED { Net_OnHashList(); });
    _conn.AddMessageHandler(NetMessage::UpdateFileData, [this]() FO_DEFERRED { Net_OnUpdateFileData(); });

    // Connect
    AddText(StrConnectToServer);
    _conn.Connect();

    // Unlock all resources to prevent collision with new files
    _resources.CleanDataSources();
}

Updater::~Updater() = default;

auto Updater::Process() -> bool
{
    FO_STACK_TRACE_ENTRY();

    _gameTime.FrameAdvance(IsRunInDebugger());

    ProcessExternalUpdate();
    ProcessFastUpdate();

    InputEvent ev;
    while (_sprMngr.GetInput()->PollEvent(ev)) {
        if (ev.Type == InputEvent::EventType::KeyDownEvent) {
            if (ev.KeyDown.Code == KeyCode::Escape) {
                GetApp()->RequestQuit();
            }
        }
    }

    // Update indication
    string update_text;

    for (const auto& message : _messages) {
        update_text += message;
        update_text += "\n";
    }

    if (!_filesToUpdate.empty()) {
        update_text += "\n";

        for (const auto& update_file : _filesToUpdate) {
            uint64_t cur_bytes = update_file.Size - update_file.RemaningSize;

            if (&update_file == &_filesToUpdate.front() && _externalUpdate) {
                cur_bytes = _externalUpdate->GetDownloadedBytes();
            }
            else if (&update_file == &_filesToUpdate.front() && _fastUpdateClient) {
                cur_bytes = _fastUpdateClient->GetVerifiedBytes();
            }
            else if (&update_file == &_filesToUpdate.front()) {
                cur_bytes += _conn.GetUnpackedBytesReceived() - _bytesRealReceivedCheckpoint;
            }

            float32_t cur = numeric_cast<float32_t>(cur_bytes) / (1024.0f * 1024.0f);
            float32_t max = std::max(numeric_cast<float32_t>(update_file.Size) / (1024.0f * 1024.0f), 0.01f);
            string name = strex(update_file.Name).format_path();

            update_text += strex("{} {:.2f} / {:.2f} MB\n", name, cur, max);
        }

        update_text += "\n";
    }

    int32_t elapsed_time = (nanotime::now() - _startTime).to_ms<int32_t>();
    int32_t dots = iround<int32_t>(std::fmod((nanotime::now() - _startTime).to_ms<float64_t>() / 100.0, 50.0)) + 1;

    for (int32_t i = 0; i < dots; i++) {
        update_text += ".";
    }

    _effectMngr.UpdateEffects(_gameTime);
    _fontMngr.FrameUpdate();
    _sprMngr.BeginScene();

    if (_splashPic) {
        _sprMngr.DrawSpriteSize(_splashPic, {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, Color::Neutral);
    }

    if (elapsed_time >= _settings->UpdaterInfoDelay) {
        auto text_format = TextFormat {.Font = FontType::Default, .Flags = CombineEnum(FontFlag::CenterX, FontFlag::CenterY, FontFlag::Bordered)};

        if (_settings->UpdaterInfoPos < 0) {
            _fontMngr.DrawText(irect32 {0, 0, _settings->ScreenWidth, _settings->ScreenHeight / 2}, update_text, Color::TextWhite, text_format);
        }
        else if (_settings->UpdaterInfoPos == 0) {
            _fontMngr.DrawText(irect32 {0, 0, _settings->ScreenWidth, _settings->ScreenHeight}, update_text, Color::TextWhite, text_format);
        }
        else {
            _fontMngr.DrawText(irect32 {0, _settings->ScreenHeight / 2, _settings->ScreenWidth, _settings->ScreenHeight / 2}, update_text, Color::TextWhite, text_format);
        }
    }

    _sprMngr.EndScene();
    _conn.Process();

    if (_restartPrompt && !GetApp()->IsQuitRequested()) {
        return false;
    }

    return _result.has_value() || IsFinished();
}

void Updater::AddText(string_view text)
{
    FO_STACK_TRACE_ENTRY();

    _messages.emplace_back(text);
}

void Updater::Abort(string_view text)
{
    FO_STACK_TRACE_ENTRY();

    _aborted = true;

    if (!_result.has_value()) {
        _result = UpdaterResult::Failed;
    }

    AddText(text);
    _conn.Disconnect();

    if (_tempFile.is_open()) {
        _tempFile.close();
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
    FO_STACK_TRACE_ENTRY();

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
        FallbackFromExternalUpdate(update_file, ContentUpdateSourceResult::TransportFailure, "transport processing failed");
        return;
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
        WriteLog(LogType::Warning, "External updater could not promote candidate for '{}' from provider '{}' source '{}'", update_file.Name, provider, source_key);
        (void)fs_remove_file(_externalCandidatePath);
        _externalCandidatePath.clear();
        Abort(StrFilesystemError);
        return;
    }

    _externalCandidatePath.clear();
    update_file.RemaningSize = 0;
    update_file.Sha256AlreadyVerified = true;

    const FinalizeResult finalize_result = FinalizeCurrentFile();

    if (finalize_result != FinalizeResult::Succeeded) {
        Abort(finalize_result == FinalizeResult::FileSystemFailure ? StrFilesystemError : StrUpdaterDescriptorError);
        return;
    }

    WriteLog("External updater completed for '{}' from provider '{}' source '{}'", file_name, provider, source_key);
    GetNextFile();
}

auto Updater::TryStartExternalUpdate(UpdateFile& update_file) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto& file_info = _updateManifest.Files[update_file.ManifestIndex];

    while (update_file.NextExternalSource < file_info.Sources.size()) {
        const size_t source_index = update_file.NextExternalSource++;
        const auto& source = file_info.Sources[source_index];

        if (_failedExternalProviders.contains(source.Provider) || _failedExternalTransports.contains(source.Transport)) {
            continue;
        }

        if (IsExternalSourceExpired(source)) {
            WriteLog("External updater skipped expired provider '{}' source '{}' for '{}'", source.Provider, source.SourceKey, update_file.Name);
            continue;
        }

        if (!_contentUpdateTransports.IsRegistered(source.Transport)) {
            WriteLog("External updater skipped unsupported transport '{}' from provider '{}' source '{}' for '{}'", source.Transport, source.Provider, source.SourceKey, update_file.Name);
            _failedExternalTransports.emplace(source.Transport);
            continue;
        }

        const string candidate_path = MakeExternalCandidatePath(update_file, source_index);
        const string candidate_dir = strex(candidate_path).extract_dir().str();

        if (!candidate_dir.empty() && !fs_create_directories(candidate_dir)) {
            Abort(StrFilesystemError);
            return false;
        }

        unique_nptr<ContentUpdateTransportDownload> download;

        try {
            const ContentUpdateTransportRequest request {source, file_info, candidate_path};
            download = _contentUpdateTransports.Create(source.Transport, request);
        }
        catch (const std::exception&) {
            WriteLog(LogType::Warning, "External updater transport '{}' failed to create for provider '{}' source '{}'", source.Transport, source.Provider, source.SourceKey);
            _failedExternalProviders.emplace(source.Provider);
            (void)fs_remove_file(candidate_path);
            continue;
        }
        catch (...) {
            WriteLog(LogType::Warning, "External updater transport '{}' failed to create for provider '{}' source '{}'", source.Transport, source.Provider, source.SourceKey);
            _failedExternalProviders.emplace(source.Provider);
            (void)fs_remove_file(candidate_path);
            continue;
        }

        if (!download) {
            WriteLog(LogType::Warning, "External updater transport '{}' declined provider '{}' source '{}' for '{}'", source.Transport, source.Provider, source.SourceKey, update_file.Name);
            (void)fs_remove_file(candidate_path);
            continue;
        }

        _externalCandidatePath = candidate_path;
        _activeExternalSourceIndex = source_index;
        _externalUpdate = std::move(download);
        AddText(StrExternalUpdateStarted);
        WriteLog("External updater started for '{}' from provider '{}' source '{}' using '{}'", update_file.Name, source.Provider, source.SourceKey, source.Transport);
        _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
        return true;
    }

    return false;
}

void Updater::FallbackFromExternalUpdate(UpdateFile& update_file, ContentUpdateSourceResult result, string_view reason)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_externalUpdate && _activeExternalSourceIndex, "External update must be active for fallback");

    const auto& source = _updateManifest.Files[update_file.ManifestIndex].Sources[*_activeExternalSourceIndex];
    WriteLog(LogType::Warning, "External updater failed for '{}' from provider '{}' source '{}': {}", update_file.Name, source.Provider, source.SourceKey, reason);
    AddText(StrExternalUpdateFallback);
    _failedExternalProviders.emplace(source.Provider);
    ReportExternalSourceResult(update_file, source, result);

    _externalUpdate->Cancel();
    _externalUpdate.reset();
    _activeExternalSourceIndex.reset();
    (void)fs_remove_file(_externalCandidatePath);
    _externalCandidatePath.clear();

    GetNextFile();
}

void Updater::ReportExternalSourceResult(const UpdateFile& update_file, const ContentUpdateSource& source, ContentUpdateSourceResult result)
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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

        WriteLog("Fast updater completed for '{}'", update_file.Name);
        _fastUpdateClient.reset();
        update_file.RemaningSize = 0;

        const FinalizeResult finalize_result = FinalizeCurrentFile();

        if (finalize_result != FinalizeResult::Succeeded) {
            Abort(finalize_result == FinalizeResult::FileSystemFailure ? StrFilesystemError : StrUpdaterDescriptorError);
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
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_fastUpdateClient, "Fast update client must be active for the fallback");

    WriteLog(LogType::Warning, "Fast updater failed for '{}': {}", update_file.Name, reason);
    AddText(StrFastUpdateFallback);

    const auto temp_path = MakeTempPath(update_file);
    const auto dir = strex(temp_path).extract_dir().str();
    uint64_t resumed_bytes = 0;

    (void)fs_remove_file(temp_path);

    if (!dir.empty() && !fs_create_directories(dir)) {
        Abort(StrFilesystemError);
        return;
    }

    _tempFile.open(std::filesystem::path {fs_make_path(temp_path)}, std::ios::binary | std::ios::trunc);

    if (!_tempFile || !_fastUpdateClient->WriteContiguousData(_tempFile, resumed_bytes)) {
        Abort(StrFilesystemError);
        return;
    }

    _fastUpdateClient.reset();
    update_file.UseFastUpdate = false;
    update_file.RemaningSize = update_file.Size - resumed_bytes;
    RequestUpdateFile(update_file);
    _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
}

void Updater::GetNextFile()
{
    FO_STACK_TRACE_ENTRY();

    if (_tempFile.is_open()) {
        const FinalizeResult finalize_result = FinalizeCurrentFile();

        if (finalize_result == FinalizeResult::IntegrityMismatch && !_filesToUpdate.front().DirectRetryAttempted) {
            auto& update_file = _filesToUpdate.front();
            const string temp_path = MakeTempPath(update_file);
            const string dir = strex(temp_path).extract_dir().str();

            WriteLog(LogType::Warning, "Client updater: direct transfer integrity check failed for '{}', retrying once from zero", update_file.Name);
            (void)fs_remove_file(temp_path);

            if (!dir.empty() && !fs_create_directories(dir)) {
                Abort(StrFilesystemError);
                return;
            }

            _tempFile.clear();
            _tempFile.open(std::filesystem::path {fs_make_path(temp_path)}, std::ios::binary | std::ios::trunc);

            if (!_tempFile) {
                Abort(StrFilesystemError);
                return;
            }

            update_file.DirectRetryAttempted = true;
            update_file.RemaningSize = update_file.Size;
            RequestUpdateFile(update_file);
            _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
            return;
        }

        if (finalize_result != FinalizeResult::Succeeded) {
            Abort(finalize_result == FinalizeResult::FileSystemFailure ? StrFilesystemError : StrUpdaterDescriptorError);
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
                WriteLog("Client updater: temp file {} is too large, size {}, expected {}", temp_path, *temp_file_size, next_update_file.Size);
                (void)fs_remove_file(temp_path);
                next_update_file.RemaningSize = next_update_file.Size;
            }
            else if (*temp_file_size == next_update_file.Size) {
                if (!IsDiskFileHashMatch(temp_path, next_update_file.Size, next_update_file.Hash) || !IsDiskFileSha256Match(temp_path, next_update_file.Size, next_update_file.Sha256)) {
                    WriteLog("Client updater: complete temp file {} has wrong hash or SHA-256, restarting download", temp_path);
                    (void)fs_remove_file(temp_path);
                    next_update_file.RemaningSize = next_update_file.Size;
                }
                else {
                    if (!ReplaceFileSafely(temp_path, final_path)) {
                        WriteLog("Client updater: failed to promote existing temp file from {} to {}", temp_path, final_path);
                        Abort(StrFilesystemError);
                        return;
                    }

                    WriteLog("Client updater: promoted existing temp file to {}, binary {}", final_path, next_update_file.IsClientBinary ? "yes" : "no");
                    if (!TryPromoteStagedBinary(next_update_file, final_path)) {
                        Abort(StrFilesystemError);
                        return;
                    }
                    _filesToUpdate.erase(_filesToUpdate.begin());
                    GetNextFile();
                    return;
                }
            }
            else {
                next_update_file.RemaningSize = next_update_file.Size - *temp_file_size;
                can_start_fast_update = can_start_fast_update && *temp_file_size == 0;
                WriteLog("Client updater: resuming temp file {}, downloaded {}, remaining {}", temp_path, *temp_file_size, next_update_file.RemaningSize);
            }
        }

        if (TryStartExternalUpdate(next_update_file)) {
            return;
        }

        if (_aborted) {
            return;
        }

        if (can_start_fast_update) {
            (void)fs_remove_file(temp_path);
            AddText(StrFastUpdateStarted);
            WriteLog("Fast updater started for '{}'", next_update_file.Name);
            _fastUpdateClient.emplace(*_settings, _updateManifest, _updateManifest.Files[next_update_file.ManifestIndex], temp_path);

            if (!_fastUpdateClient->IsFailed()) {
                _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
                return;
            }

            WriteLog(LogType::Warning, "Fast updater failed to start for '{}': {}", next_update_file.Name, _fastUpdateClient->GetError());
            AddText(StrFastUpdateFallback);
            next_update_file.UseFastUpdate = false;
            _fastUpdateClient->Cleanup();
            _fastUpdateClient.reset();
        }

        const auto dir = strex(temp_path).extract_dir().str();

        if (!dir.empty()) {
            if (!fs_create_directories(dir)) {
                Abort(StrFilesystemError);
                return;
            }
        }

        std::ios_base::openmode open_mode = std::ios::binary | (next_update_file.RemaningSize != next_update_file.Size ? std::ios::app : std::ios::trunc);
        _tempFile.open(std::filesystem::path {fs_make_path(temp_path)}, open_mode);

        if (!_tempFile) {
            WriteLog("Client updater: failed to open temp file {}", temp_path);
            Abort(StrFilesystemError);
            return;
        }

        WriteLog("Client updater: requesting file {}, binary {}, size {}, remaining {}, temp {}, final {}", next_update_file.Name, next_update_file.IsClientBinary ? "yes" : "no", next_update_file.Size, next_update_file.RemaningSize, temp_path, final_path);
        RequestUpdateFile(next_update_file);
    }
    else {
        if (_binariesMode) {
            WriteLog("Client updater: finished binary update, binaries staged for reload");
            _result = UpdaterResult::BinariesStaged;

            // Headless clients have no UI / no user to dismiss it, so they finish immediately (no hold).
            if (!GetApp()->IsHeadless()) {
                AddText(StrRestartRequired);
                _restartPrompt = true;
            }
        }
        else {
            WriteLog("Client updater: finished resource update, resources ready");
            _result = UpdaterResult::ResourcesReady;
        }
    }

    _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
}

void Updater::RequestUpdateFile(const UpdateFile& update_file)
{
    FO_STACK_TRACE_ENTRY();

    uint64_t start_offset = update_file.Size - update_file.RemaningSize;

    _conn.OutBuf->StartMsg(NetMessage::GetUpdateFile);
    _conn.OutBuf->Write(update_file.Index);
    _conn.OutBuf->Write(numeric_cast<uint64_t>(start_offset));
    _conn.OutBuf->EndMsg();
}

void Updater::Net_OnConnect(ClientConnection::ConnectResult result)
{
    FO_STACK_TRACE_ENTRY();

    string_view result_str;

    switch (result) {
    case ClientConnection::ConnectResult::Success:
        result_str = "Success";
        break;
    case ClientConnection::ConnectResult::CompatibilityOutdated:
        result_str = "CompatibilityOutdated";
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

    WriteLog("Client updater: server answered {}, client compatibility {}", result_str, _settings->CompatibilityVersion);

    if (result == ClientConnection::ConnectResult::Success) {
        AddText(StrConnectionEstablished);
        AddText(StrCheckUpdates);
        _binariesMode = false;
        WriteLog("Client updater: client is compatible, checking resources");
    }
    else if (result == ClientConnection::ConnectResult::CompatibilityOutdated) {
        AddText(StrConnectionEstablished);
        AddText(StrCheckUpdates);

        bool can_self_update_binaries = CanSelfUpdateNativeModules(GetCurrentUpdatePlatform());
        WriteLog("Client updater: server reported CompatibilityOutdated, native self-update {} for {}", can_self_update_binaries ? "supported" : "unsupported", GetCurrentBinaryUpdateTargetName());

        if (!can_self_update_binaries) {
            _result = UpdaterResult::PlatformUnsupported;
            _fileListReceived = true;
            _conn.Disconnect();
            return;
        }

        _binariesMode = true;
        WriteLog("Client updater: switched to native binary update mode");
    }
    else if (result == ClientConnection::ConnectResult::UpdaterOutdated) {
        _result = UpdaterResult::UpdaterOutdated;
        WriteLog("Client updater: protocol is outdated, aborting");
        Abort(StrUpdaterOutdated);
    }
    else {
        WriteLog("Client updater: connection failed");
        Abort(StrCantConnectToServer);
    }
}

void Updater::Net_OnDisconnect()
{
    FO_STACK_TRACE_ENTRY();

    if (!_aborted && (!_fileListReceived || !_filesToUpdate.empty())) {
        Abort(StrConnectionFailure);
    }
}

void Updater::Net_OnInitData()
{
    FO_STACK_TRACE_ENTRY();

    auto data_size = _conn.InBuf->Read<uint32_t>();

    if (data_size > ContentUpdateMaxDescriptorSize || numeric_cast<size_t>(data_size) > _conn.InBuf->GetUnreadSize()) {
        WriteLog(LogType::Warning, "Rejected invalid content update descriptor length: {} bytes, {} bytes remain, maximum {}", data_size, _conn.InBuf->GetUnreadSize(), ContentUpdateMaxDescriptorSize);
        Abort(StrUpdaterDescriptorError);
        return;
    }

    vector<uint8_t> data;
    data.resize(data_size);

    _conn.InBuf->Pop(data.data(), data_size);

    vector<vector<uint8_t>> globals_properties_data;
    _conn.InBuf->ReadPropsData(globals_properties_data);
    auto time = _conn.InBuf->Read<synctime>();
    ignore_unused(globals_properties_data);

    _gameTime.SetSynchronizedTime(time);

    FO_VERIFY_AND_THROW(!_fileListReceived, "Update file list was already received");
    _fileListReceived = true;

    auto our_target = _binariesMode ? UpdateFileTarget::ClientBinaries : UpdateFileTarget::ClientResources;
    WriteLog("Client updater: received update list, bytes {}, mode {}, target {}", data_size, _binariesMode ? "binaries" : "resources", _binariesMode ? "ClientBinaries" : "ClientResources");

    if (data.empty()) {
        if (_binariesMode) {
            WriteLog("Client updater: native update list is empty");
            _result = UpdaterResult::ServerMissingNativeUpdate;
        }
        else {
            WriteLog("Client updater: resource update list is empty, resources ready");
            _result = UpdaterResult::ResourcesReady;
        }

        return;
    }

    FileSystem resources;

    if (!_binariesMode) {
        resources.AddDirSource(_settings->ClientResources, false, true, true);
    }

    try {
        if (_settings->UpdateManifestSignatureRequired) {
            if (_settings->UpdateManifestMinimumReleaseSequence <= 0) {
                throw ContentUpdaterException("Content update minimum release sequence must be positive", _settings->UpdateManifestMinimumReleaseSequence);
            }

            vector<ContentUpdateTrustedPublicKey> trusted_keys;
            trusted_keys.reserve(_settings->UpdateManifestTrustedPublicKeys.size());

            for (const auto& trusted_key_entry : _settings->UpdateManifestTrustedPublicKeys) {
                ContentUpdateTrustedPublicKey trusted_key;

                if (!TryParseContentUpdateTrustedPublicKey(trusted_key_entry, trusted_key)) {
                    throw ContentUpdaterException("Invalid trusted content update public key entry");
                }
                if (std::ranges::any_of(trusted_keys, [&trusted_key](const ContentUpdateTrustedPublicKey& key) { return key.KeyId == trusted_key.KeyId; })) {
                    throw ContentUpdaterException("Duplicate trusted content update public key id", trusted_key.KeyId);
                }

                trusted_keys.emplace_back(trusted_key);
            }

            const uint64_t minimum_release_sequence = numeric_cast<uint64_t>(_settings->UpdateManifestMinimumReleaseSequence);
            const VerifiedContentUpdateManifest verified = VerifyContentUpdateManifestDescriptor(data, GetCurrentBinaryUpdateTargetName(), minimum_release_sequence, trusted_keys);

            if (!AcceptContentUpdateReleaseSequence(_settings->UserWritablePath, verified.ReleaseSequence, minimum_release_sequence)) {
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
        WriteLog(LogType::Warning, "Invalid content update descriptor: {}", ex.what());
        Abort(StrUpdaterDescriptorError);
        return;
    }

    const auto accept_binaries = _binariesMode || CanSelfUpdateNativeModules(GetCurrentUpdatePlatform());
    const string runtime_local_prefix = accept_binaries ? strex(GetRuntimeLivePath()).extract_file_name().erase_file_extension().str() : string {};

    string runtime_server_prefix;

    if (accept_binaries) {
        runtime_server_prefix = GetPackagedRuntimeName();

        if (runtime_server_prefix.empty()) {
            runtime_server_prefix = runtime_local_prefix;
        }

        WriteLog("Client updater: binary name remap from server prefix {} to local prefix {}", runtime_server_prefix, runtime_local_prefix);
    }

    auto remap_runtime_name = [&](const string& fname_basename) -> optional<string> {
        if (!fname_basename.starts_with(runtime_server_prefix)) {
            return std::nullopt;
        }

        string_view rest = string_view(fname_basename).substr(runtime_server_prefix.size());

        if (!rest.empty() && rest[0] != '.') {
            return std::nullopt;
        }

        return strex("{}{}", runtime_local_prefix, rest).str();
    };

    for (size_t manifest_index = 0; manifest_index < _updateManifest.Files.size(); manifest_index++) {
        const auto& file_info = _updateManifest.Files[manifest_index];
        const auto& fname = file_info.Name;
        const auto size = file_info.Size;
        const auto hash = file_info.Hash;
        const auto target = file_info.Target;
        const auto data_index = file_info.FileIndex;

        string local_name = fname;
        bool is_client_binary = false;

        if (target == UpdateFileTarget::ClientBinaries) {
            if (!accept_binaries) {
                continue;
            }

            string fname_basename = strex(fname).extract_file_name().str();
            auto remapped = remap_runtime_name(fname_basename);

            if (!remapped.has_value()) {
                continue;
            }

            auto remapped_basename = *remapped;
            string fname_dir = strex(fname).extract_dir().str();
            local_name = fname_dir.empty() ? remapped_basename : strex(fname_dir).combine_path(remapped_basename).str();

            string file_path = strex(_binaryDir).combine_path(local_name).str();

            if (remapped_basename == strex("{}.pdb", runtime_local_prefix).str()) {
                if (fs_exists(file_path)) {
                    continue;
                }
            }
            else {
                if (!_binariesMode) {
                    continue;
                }

                _hasMatchingEntries = true;
                WriteLog("Client updater: matched binary entry {}, local {}, size {}, hash {}", fname, local_name, size, hash);
            }

            is_client_binary = true;

            if (IsDiskFileHashMatch(file_path, size, hash)) {
                WriteLog("Client updater: binary already matches {}", file_path);
                continue;
            }
        }
        else if (target == our_target) {
            auto file_header = resources.ReadFileHeader(fname);

            if (file_header) {
                if (file_header.GetSize() == size) {
                    if (file_header.GetDataSource()->IsDiskDir() && IsDiskFileHashMatch(file_header.GetDiskPath(), size, hash)) {
                        continue;
                    }

                    auto file = resources.ReadFile(fname);

                    if (file && IsDataHashMatch(file.GetData(), size, hash)) {
                        continue;
                    }
                }
            }
        }
        else {
            continue;
        }

        UpdateFile update_file;
        update_file.Index = data_index;
        update_file.ManifestIndex = manifest_index;
        update_file.Name = local_name;
        update_file.Size = size;
        update_file.RemaningSize = size;
        update_file.Hash = hash;
        update_file.Sha256 = file_info.Sha256;
        update_file.IsClientBinary = is_client_binary;
        update_file.UseFastUpdate = _settings->FastUpdateEnabled && _updateManifest.FastUpdateEnabled && !_updateManifest.Endpoints.empty() && _updateManifest.ChunkSize != 0 && _updateManifest.ChunkSize <= ContentUpdateMaxChunkPayloadSize && !file_info.ChunkHashes.empty();
        _filesToUpdate.emplace_back(std::move(update_file));
    }

    if (!_filesToUpdate.empty()) {
        WriteLog("Client updater: {} files need update in {} mode", _filesToUpdate.size(), _binariesMode ? "binaries" : "resources");
        GetNextFile();
    }
    else if (_binariesMode) {
        if (_hasMatchingEntries) {
            WriteLog("Client updater: binaries already match, requesting reload");
            _result = UpdaterResult::BinariesStaged;
        }
        else {
            WriteLog("Client updater: server has no matching native update payload");
            _result = UpdaterResult::ServerMissingNativeUpdate;
        }
    }
    else {
        WriteLog("Client updater: resources ready");
        _result = UpdaterResult::ResourcesReady;
    }
}

void Updater::Net_OnTimeSync()
{
    FO_STACK_TRACE_ENTRY();

    auto time = _conn.InBuf->Read<synctime>();
    _gameTime.SetSynchronizedTimeMonotonic(time);
}

void Updater::Net_OnHashList()
{
    FO_STACK_TRACE_ENTRY();

    uint32_t count = _conn.InBuf->Read<uint32_t>();

    for (uint32_t i = 0; i < count; i++) {
        string str = _conn.InBuf->Read<string>();

        _hashStorage.ToHashedString(str);
    }

    if (count != 0) {
        WriteLog("Client updater: learned {} previously unresolved hash(es) from server", count);
    }
}

void Updater::Net_OnUpdateFileData()
{
    FO_STACK_TRACE_ENTRY();

    int32_t data_size_raw = _conn.InBuf->Read<int32_t>();

    if (data_size_raw < 0 || _filesToUpdate.empty() || !_tempFile.is_open() || _settings->UpdateFileMaxPortionSize <= 0) {
        Abort(StrFilesystemError);
        return;
    }

    const auto data_size = numeric_cast<size_t>(data_size_raw);
    auto& update_file = _filesToUpdate.front();

    if (data_size > numeric_cast<size_t>(_settings->UpdateFileMaxPortionSize) || data_size > _conn.InBuf->GetUnreadSize() || numeric_cast<uint64_t>(data_size) > update_file.RemaningSize) {
        Abort(StrFilesystemError);
        return;
    }

    _updateFileBuf.resize(data_size);

    _conn.InBuf->Pop(_updateFileBuf.data(), data_size);

    // Write data to temp file
    size_t write_size = GetUpdateWriteSize(update_file.RemaningSize, _updateFileBuf.size());

    if (write_size != 0) {
        _tempFile.write(make_ptr(_updateFileBuf.data()).reinterpret_as<char>().get(), numeric_cast<std::streamsize>(write_size));
    }

    if (!_tempFile) {
        Abort(StrFilesystemError);
        return;
    }

    update_file.RemaningSize -= data_size;

    if (update_file.RemaningSize > 0) {
        if (data_size == 0) {
            Abort(StrFilesystemError);
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
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_filesToUpdate.empty(), "No update file to finalize");

    if (_tempFile.is_open()) {
        _tempFile.close();

        if (_tempFile.fail()) {
            return FinalizeResult::FileSystemFailure;
        }
    }

    const auto& update_file = _filesToUpdate.front();
    const auto final_path = MakeFinalPath(update_file);
    const auto temp_path = MakeTempPath(update_file);

    if (!IsDiskFileHashMatch(temp_path, update_file.Size, update_file.Hash) || (!update_file.Sha256AlreadyVerified && !IsDiskFileSha256Match(temp_path, update_file.Size, update_file.Sha256))) {
        WriteLog("Client updater: downloaded file hash or SHA-256 mismatch, temp {}, file {}", temp_path, update_file.Name);
        return FinalizeResult::IntegrityMismatch;
    }

    if (!ReplaceFileSafely(temp_path, final_path)) {
        WriteLog("Client updater: failed to promote downloaded file from {} to {}", temp_path, final_path);
        return FinalizeResult::FileSystemFailure;
    }

    WriteLog("Client updater: promoted downloaded file to {}, binary {}", final_path, update_file.IsClientBinary ? "yes" : "no");
    if (!TryPromoteStagedBinary(update_file, final_path)) {
        return FinalizeResult::FileSystemFailure;
    }
    _filesToUpdate.erase(_filesToUpdate.begin());
    return FinalizeResult::Succeeded;
}

auto Updater::MakeFileOutputDir(const UpdateFile& update_file) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return update_file.IsClientBinary ? _binaryDir : fs_make_writable_path(_settings->UserWritablePath, _settings->ClientResources);
}

auto Updater::MakeTempPath(const UpdateFile& update_file) const -> string
{
    FO_STACK_TRACE_ENTRY();

    const string base_path = strex(MakeFileOutputDir(update_file)).combine_path(strex("~{}", update_file.Name)).str();
    return strex("{}.__sha256.{}", base_path, Sha256DigestToHex(update_file.Sha256)).str();
}

auto Updater::MakeExternalCandidatePath(const UpdateFile& update_file, size_t source_index) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}.__external.{}", MakeTempPath(update_file), source_index).str();
}

void Updater::CleanupStaleTempFiles(const UpdateFile& update_file, string_view current_temp_path) const
{
    FO_STACK_TRACE_ENTRY();

    const string legacy_temp_path = strex(MakeFileOutputDir(update_file)).combine_path(strex("~{}", update_file.Name)).str();
    const std::filesystem::path legacy_path {fs_make_path(legacy_temp_path)};
    const std::filesystem::path current_path {fs_make_path(current_temp_path)};
    const std::filesystem::path directory = legacy_path.parent_path();

    if (directory.empty()) {
        return;
    }

    const string legacy_name = fs_path_to_string(legacy_path.filename());
    const string current_name = fs_path_to_string(current_path.filename());
    const string digest_prefix = strex("{}.__sha256.", legacy_name).str();
    const string legacy_external_prefix = strex("{}.__external.", legacy_name).str();
    const string legacy_fast_prefix = strex("{}.__fastupd.", legacy_name).str();
    std::error_code error;
    std::filesystem::directory_iterator entry {directory, error};
    const std::filesystem::directory_iterator end;

    while (!error && entry != end) {
        std::error_code type_error;

        if (entry->is_regular_file(type_error)) {
            const string name = fs_path_to_string(entry->path().filename());
            const bool stale_digest_file = name.starts_with(digest_prefix) && !name.starts_with(current_name);
            const bool stale_legacy_file = name == legacy_name || name.starts_with(legacy_external_prefix) || name.starts_with(legacy_fast_prefix);

            if (stale_digest_file || stale_legacy_file) {
                (void)fs_remove_file(fs_path_to_string(entry->path()));
            }
        }

        entry.increment(error);
    }
}

auto Updater::MakeLivePath(const UpdateFile& update_file) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(MakeFileOutputDir(update_file)).combine_path(update_file.Name).str();
}

auto Updater::MakeFinalPath(const UpdateFile& update_file) const -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto live_path = MakeLivePath(update_file);
    return update_file.IsClientBinary ? strex("{}{}", live_path, ClientBinaryStagingSuffix).str() : live_path;
}

auto Updater::TryPromoteStagedBinary(const UpdateFile& update_file, string_view staged_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!update_file.IsClientBinary) {
        return true;
    }

    const string live_path = MakeLivePath(update_file);
    const bool is_runtime = fs_resolve_path(live_path) == fs_resolve_path(GetRuntimeLivePath());
    string authorization_path;

    if (is_runtime && _settings->UpdateManifestSignatureRequired) {
        authorization_path = MakeClientRuntimeAuthorizationPath(live_path);

        if (_signedUpdateDescriptor.empty() || !fs_write_file(authorization_path, _signedUpdateDescriptor)) {
            WriteLog(LogType::Warning, "Client updater: failed to persist signed authorization for staged runtime {}", staged_path);
            (void)fs_remove_file(staged_path);
            return false;
        }
    }

    if (ReplaceFileSafely(staged_path, live_path) && !authorization_path.empty()) {
        (void)fs_remove_file(authorization_path);
    }

    return true;
}

auto Updater::IsDiskFileHashMatch(string_view file_path, uint64_t expected_size, uint64_t expected_hash) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto local_size = fs_file_size(file_path);

    if (!local_size.has_value() || *local_size != expected_size) {
        return false;
    }

    struct CachedHash
    {
        uint64_t Size;
        uint64_t Mtime;
        uint64_t Hash;
    };
    static_assert(std::is_trivially_copyable_v<CachedHash>);

    uint64_t local_mtime = fs_last_write_time(file_path);
    string cache_key = strex("{}.hash", strex(file_path).extract_file_name()).str();

    if (_cache.HasEntry(cache_key)) {
        auto data = _cache.GetData(cache_key);

        if (data.size() == sizeof(CachedHash)) {
            CachedHash cached {};
            auto target = make_ptr(&cached).reinterpret_as<uint8_t>();
            MemCopy(target, data.data(), sizeof(cached));

            if (cached.Size == *local_size && cached.Mtime == local_mtime) {
                return cached.Hash == expected_hash;
            }
        }
    }

    auto local_hash = fs_hash_file(file_path);

    if (!local_hash.has_value()) {
        return false;
    }

    CachedHash entry {*local_size, local_mtime, *local_hash};
    _cache.SetData(cache_key, const_span<uint8_t> {make_ptr(&entry).reinterpret_as<uint8_t>().get(), sizeof(CachedHash)});

    return *local_hash == expected_hash;
}

auto Updater::IsDiskFileSha256Match(string_view file_path, uint64_t expected_size, const Sha256Digest& expected_sha256) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto local_size = fs_file_size(file_path);

    if (!local_size.has_value() || *local_size != expected_size) {
        return false;
    }

    const auto local_sha256 = fs_sha256_file(file_path);
    return local_sha256.has_value() && *local_sha256 == expected_sha256;
}

auto Updater::IsExternalSourceExpired(const ContentUpdateSource& source) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return source.ExpiresAt != 0 && source.ExpiresAt <= _gameTime.GetSynchronizedTime().milliseconds();
}

auto Updater::IsDataHashMatch(const vector<uint8_t>& data, uint64_t expected_size, uint64_t expected_hash) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<uint64_t>(data.size()) == expected_size && fs_hash_data(data) == expected_hash;
}

auto Updater::GetDiskFileSize(string_view file_path) -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    return fs_file_size(file_path);
}

auto Updater::GetUpdateWriteSize(uint64_t remaining_size, size_t received_size) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return remaining_size < numeric_cast<uint64_t>(received_size) ? numeric_cast<size_t>(remaining_size) : received_size;
}

auto Updater::ReplaceFileSafely(string_view temp_path, string_view final_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    string backup_path = strex("{}.bak", final_path).str();
    bool final_exists = fs_exists(final_path);

    (void)fs_remove_file(backup_path);

    if (final_exists && !fs_rename(final_path, backup_path)) {
        return false;
    }

    if (!fs_rename(temp_path, final_path)) {
        if (final_exists) {
            fs_rename(backup_path, final_path);
        }

        return false;
    }

    if (final_exists) {
        (void)fs_remove_file(backup_path);
    }

    return true;
}

auto Updater::GetRuntimeLivePath() const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}{}", strex(_binaryDir).combine_path(GetCurrentClientRuntimeLibraryName()), GetClientRuntimeLibraryExtension()).str();
}

auto GetCurrentUpdatePlatform() noexcept -> UpdatePlatform
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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

auto GetClientRuntimeLivePath() -> string
{
    FO_STACK_TRACE_ENTRY();

    string binary_dir;

    if constexpr (FO_WEB) {
        // No on-disk runtime companion on web; the runtime lives at the virtual filesystem root.
        binary_dir = "/";
    }
    else {
        auto exe_path = Platform::GetExePath();
        FO_VERIFY_AND_THROW(exe_path.has_value(), "Executable path could not be resolved");
        binary_dir = strex(exe_path.value()).extract_dir().str();
    }

    return strex("{}{}", strex(binary_dir).combine_path(GetCurrentClientRuntimeLibraryName()), GetClientRuntimeLibraryExtension()).str();
}

auto MakeClientRuntimeStagingPath(string_view runtime_live_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}{}", runtime_live_path, ClientBinaryStagingSuffix).str();
}

auto ResolveClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view expected_runtime_file_name, string_view fallback_runtime_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    optional<string> target = ReadClientRuntimeBootstrapTarget(bootstrap_file_path, expected_runtime_file_name);

    if (!target.has_value()) {
        return string(fallback_runtime_path);
    }

    string staging_path = MakeClientRuntimeStagingPath(target.value());
    bool live_exists = fs_exists(target.value()) && !fs_is_dir(target.value());
    bool staging_exists = fs_exists(staging_path) && !fs_is_dir(staging_path);
    return live_exists || staging_exists ? target.value() : string(fallback_runtime_path);
}

auto ReadClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view expected_runtime_file_name) -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_absolute_path(bootstrap_file_path)) {
        return std::nullopt;
    }

    optional<uint64_t> file_size = fs_file_size(bootstrap_file_path);

    if (!file_size.has_value() || file_size.value() == 0 || file_size.value() > ClientRuntimeBootstrapMaxSize) {
        return std::nullopt;
    }

    optional<string> content = fs_read_file(bootstrap_file_path);

    if (!content.has_value() || content->size() > ClientRuntimeBootstrapMaxSize) {
        return std::nullopt;
    }

    return NormalizeClientRuntimeBootstrapTarget(content.value(), expected_runtime_file_name);
}

auto WriteClientRuntimeBootstrapTarget(string_view bootstrap_file_path, string_view runtime_path, string_view expected_runtime_file_name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_absolute_path(bootstrap_file_path)) {
        return false;
    }

    optional<string> normalized_path = NormalizeClientRuntimeBootstrapTarget(runtime_path, expected_runtime_file_name);

    if (!normalized_path.has_value() || normalized_path->size() + 1 > ClientRuntimeBootstrapMaxSize) {
        return false;
    }

    // Write-then-rename so a crash or a concurrent reader never observes a partially written selector
    string temp_path = strex("{}.tmp", bootstrap_file_path).str();

    if (!fs_write_file(temp_path, strex("{}\n", normalized_path.value()).str())) {
        return false;
    }

    if (!fs_rename(temp_path, bootstrap_file_path)) {
        fs_remove_file(temp_path);
        return false;
    }

    return true;
}

auto MakeClientRuntimeAuthorizationPath(string_view runtime_live_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}.auth", MakeClientRuntimeStagingPath(runtime_live_path)).str();
}

void PromoteStagedRuntimeCompanions(string_view binary_dir) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        string runtime_name = GetCurrentClientRuntimeLibraryName();
        string runtime_primary_name = strex("{}{}", runtime_name, GetClientRuntimeLibraryExtension()).str();

        vector<pair<string, string>> renames;

        fs_iterate_dir(binary_dir, false, [&](string_view path, size_t, uint64_t) {
            string file_name = strex(path).extract_file_name().str();

            if (!file_name.ends_with(ClientBinaryStagingSuffix)) {
                return;
            }

            auto unstaged_name = file_name.substr(0, file_name.size() - ClientBinaryStagingSuffix.size());

            if (unstaged_name == runtime_primary_name) {
                return;
            }

            bool matches_runtime = unstaged_name == runtime_name || (unstaged_name.size() > runtime_name.size() && unstaged_name.starts_with(runtime_name) && unstaged_name[runtime_name.size()] == '.');

            if (!matches_runtime) {
                return;
            }

            renames.emplace_back(string(path), strex(binary_dir).combine_path(unstaged_name).str());
        });

        for (const auto& [staged, final_path] : renames) {
            (void)fs_remove_file(final_path);
            (void)fs_rename(staged, final_path);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

auto GetCurrentClientRuntimeLibraryName() -> string
{
    FO_STACK_TRACE_ENTRY();

    if (auto exe_path = Platform::GetExePath(); exe_path.has_value()) {
        string name = strex(exe_path.value()).extract_file_name().erase_file_extension().str();

        if (!name.empty()) {
            return name;
        }
    }

    return string(FO_DEV_NAME);
}

void ShowUpdaterFailure(UpdaterResult result)
{
    FO_STACK_TRACE_ENTRY();

    string_view target_name = GetCurrentBinaryUpdateTargetName();

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
    case UpdaterResult::Failed:
        Application::ShowErrorMessage(strex(strex::dynamic_format, StrNativeUpdateFailed, target_name).str(), StrErrorMessageCaption, true);
        break;
    case UpdaterResult::ResourcesReady:
    case UpdaterResult::BinariesStaged:
    default:
        break;
    }
}

auto GetClientRuntimeLibraryExtension() noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    string trimmed_path = strex(runtime_path).trim().str();

    if (trimmed_path.empty() || expected_runtime_file_name.empty() || !fs_is_absolute_path(trimmed_path) || trimmed_path.find('\0') != string::npos || trimmed_path.find('\r') != string::npos || trimmed_path.find('\n') != string::npos) {
        return std::nullopt;
    }

    if (strex(trimmed_path).extract_file_name().str() != expected_runtime_file_name) {
        return std::nullopt;
    }

    return fs_resolve_path(trimmed_path);
}

FO_END_NAMESPACE
