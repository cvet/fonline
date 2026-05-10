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
#include "NetCommand.h"

FO_BEGIN_NAMESPACE

static auto* StrCheckUpdates = "Check updates";
static auto* StrConnectToServer = "Connect to the server";
static auto* StrCantConnectToServer = "Can't connect to the server!";
static auto* StrConnectionEstablished = "Connection established";
static auto* StrConnectionFailure = "Connection failure!";
static auto* StrFilesystemError = "File system error!";
static auto* StrUpdaterOutdated = "Client updater outdated, please update the base client";

static constexpr string_view ClientBinaryStagingSuffix = "-staging";

Updater::Updater(GlobalSettings& settings, IAppWindow& window) :
    _settings {&settings},
    _conn(*_settings),
    _cache(settings.CacheResources),
    _binaryDir {GetClientBinaryDir()},
    _gameTime(settings),
    _effectMngr(settings, _resources, window.GetRender()),
    _sprMngr(settings, window, _resources, _gameTime, _effectMngr, _hashStorage),
    _fontMngr(_sprMngr)
{
    FO_STACK_TRACE_ENTRY();

    _startTime = nanotime::now();

    _resources.AddPackSource(IsPackaged() ? settings.ClientResources : settings.BakeOutput, "Embedded");
    _resources.AddDirSource(_settings->ClientResources, false, true, true);

    if (!_settings->DefaultSplashPack.empty()) {
        _resources.AddPackSource(IsPackaged() ? _settings->ClientResources : _settings->BakeOutput, _settings->DefaultSplashPack, true);
    }

    _effectMngr.LoadMinimalEffects();
    _sprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<DefaultSpriteFactory>(_sprMngr));

    // Wait screen
    if (!_settings->DefaultSplash.empty()) {
        _splashPic = _sprMngr.LoadSprite(_settings->DefaultSplash, AtlasType::OneImage);

        if (_splashPic) {
            _splashPic->PlayDefault();
        }
    }

    _sprMngr.BeginScene();
    if (_splashPic) {
        _sprMngr.DrawSpriteSize(_splashPic.get(), {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, COLOR_NEUTRAL);
    }
    _sprMngr.EndScene();

    // Load font
    _fontMngr.BindFoFont(FontType::Default, "Fonts/Default.fofnt", AtlasType::IfaceSprites, false, true);

    // Network handlers
    _conn.SetConnectHandler([this](ClientConnection::ConnectResult result) FO_DEFERRED { Net_OnConnect(result); });
    _conn.SetDisconnectHandler([this]() FO_DEFERRED { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NetMessage::InitData, [this]() FO_DEFERRED { Net_OnInitData(); });
    _conn.AddMessageHandler(NetMessage::TimeSync, [this]() FO_DEFERRED { Net_OnTimeSync(); });
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

    InputEvent ev;
    while (_sprMngr.GetInput().PollEvent(ev)) {
        if (ev.Type == InputEvent::EventType::KeyDownEvent) {
            if (ev.KeyDown.Code == KeyCode::Escape) {
                App->RequestQuit();
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
            auto cur_bytes = update_file.Size - update_file.RemaningSize;

            if (&update_file == &_filesToUpdate.front()) {
                cur_bytes += _conn.GetUnpackedBytesReceived() - _bytesRealReceivedCheckpoint;
            }

            const auto cur = numeric_cast<float32_t>(cur_bytes) / (1024.0f * 1024.0f);
            const auto max = std::max(numeric_cast<float32_t>(update_file.Size) / (1024.0f * 1024.0f), 0.01f);
            const string name = strex(update_file.Name).format_path();

            update_text += strex("{} {:.2f} / {:.2f} MB\n", name, cur, max);
        }

        update_text += "\n";
    }

    const auto elapsed_time = (nanotime::now() - _startTime).to_ms<int32_t>();
    const auto dots = iround<int32_t>(std::fmod((nanotime::now() - _startTime).to_ms<float64_t>() / 100.0, 50.0)) + 1;

    for ([[maybe_unused]] const auto i : iterate_range(dots)) {
        update_text += ".";
    }

    _effectMngr.UpdateEffects(_gameTime);
    _fontMngr.FrameUpdate();
    _sprMngr.BeginScene();

    if (_splashPic) {
        _sprMngr.DrawSpriteSize(_splashPic.get(), {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, COLOR_NEUTRAL);
    }

    if (elapsed_time >= _settings->UpdaterInfoDelay) {
        const auto text_format = TextFormat {.Font = FontType::Default, .Flags = CombineEnum(FontFlag::CenterX, FontFlag::CenterY, FontFlag::Bordered)};

        if (_settings->UpdaterInfoPos < 0) {
            _fontMngr.DrawText(irect32 {0, 0, _settings->ScreenWidth, _settings->ScreenHeight / 2}, update_text, COLOR_TEXT_WHITE, text_format);
        }
        else if (_settings->UpdaterInfoPos == 0) {
            _fontMngr.DrawText(irect32 {0, 0, _settings->ScreenWidth, _settings->ScreenHeight}, update_text, COLOR_TEXT_WHITE, text_format);
        }
        else {
            _fontMngr.DrawText(irect32 {0, _settings->ScreenHeight / 2, _settings->ScreenWidth, _settings->ScreenHeight / 2}, update_text, COLOR_TEXT_WHITE, text_format);
        }
    }

    _sprMngr.EndScene();
    _conn.Process();

    return !_aborted && IsFinished();
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

    AddText(text);
    _conn.Disconnect();

    if (_tempFile.is_open()) {
        _tempFile.close();
    }
}

void Updater::GetNextFile()
{
    FO_STACK_TRACE_ENTRY();

    const auto output_dir = _binariesMode ? _binaryDir : string(_settings->ClientResources);
    const auto make_temp_path = [&output_dir](string_view fname) -> string { return strex(output_dir).combine_path(strex("~{}", fname)).str(); };
    const auto make_final_path = [&](string_view fname) -> string {
        const auto live_path = strex(output_dir).combine_path(fname).str();
        // Binary updates land next to the live module under a -staging suffix so the host
        // can swap them at the next boot without an extra directory.
        return _binariesMode ? strex("{}{}", live_path, ClientBinaryStagingSuffix).str() : live_path;
    };

    if (_tempFile.is_open()) {
        _tempFile.close();

        if (_tempFile.fail()) {
            Abort(StrFilesystemError);
            return;
        }

        const auto& prev_update_file = _filesToUpdate.front();
        const auto prev_path_str = make_final_path(prev_update_file.Name);
        const auto temp_path_str = make_temp_path(prev_update_file.Name);

        if (!IsDiskFileHashMatch(temp_path_str, prev_update_file.Size, prev_update_file.Hash)) {
            Abort(StrFilesystemError);
            return;
        }

        if (!ReplaceFileSafely(temp_path_str, prev_path_str)) {
            Abort(StrFilesystemError);
            return;
        }

        _updatedAnyFile = true;
        _filesToUpdate.erase(_filesToUpdate.begin());
    }

    if (!_filesToUpdate.empty()) {
        auto& next_update_file = _filesToUpdate.front();
        const auto prev_path_str = make_final_path(next_update_file.Name);
        const auto temp_path = make_temp_path(next_update_file.Name);
        const auto temp_file_size = GetDiskFileSize(temp_path);

        if (temp_file_size.has_value()) {
            if (*temp_file_size > next_update_file.Size) {
                (void)fs_remove_file(temp_path);
                next_update_file.RemaningSize = next_update_file.Size;
            }
            else if (*temp_file_size == next_update_file.Size) {
                if (!IsDiskFileHashMatch(temp_path, next_update_file.Size, next_update_file.Hash)) {
                    (void)fs_remove_file(temp_path);
                    next_update_file.RemaningSize = next_update_file.Size;
                }
                else {
                    if (!ReplaceFileSafely(temp_path, prev_path_str)) {
                        Abort(StrFilesystemError);
                        return;
                    }

                    _updatedAnyFile = true;
                    _filesToUpdate.erase(_filesToUpdate.begin());
                    GetNextFile();
                    return;
                }
            }
            else {
                next_update_file.RemaningSize = next_update_file.Size - *temp_file_size;
            }
        }

        const auto dir = strex(temp_path).extract_dir().str();

        if (!dir.empty()) {
            if (!fs_create_directories(dir)) {
                Abort(StrFilesystemError);
                return;
            }
        }

        const auto open_mode = std::ios::binary | (next_update_file.RemaningSize != next_update_file.Size ? std::ios::app : std::ios::trunc);
        _tempFile.open(std::filesystem::path {fs_make_path(temp_path)}, open_mode);

        if (!_tempFile) {
            Abort(StrFilesystemError);
            return;
        }

        RequestUpdateFile(next_update_file);
    }
    else {
        // All queued files processed successfully; mark terminal result for the caller.
        _result = _binariesMode ? UpdaterResult::BinariesStaged : UpdaterResult::ResourcesReady;
    }

    _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
}

void Updater::RequestUpdateFile(const UpdateFile& update_file)
{
    FO_STACK_TRACE_ENTRY();

    const auto start_offset = update_file.Size - update_file.RemaningSize;

    _conn.OutBuf->StartMsg(NetMessage::GetUpdateFile);
    _conn.OutBuf->Write(update_file.Index);
    _conn.OutBuf->Write(numeric_cast<uint64_t>(start_offset));
    _conn.OutBuf->EndMsg();
}

void Updater::Net_OnConnect(ClientConnection::ConnectResult result)
{
    FO_STACK_TRACE_ENTRY();

    _connectResult = result;

    if (result == ClientConnection::ConnectResult::Success) {
        AddText(StrConnectionEstablished);
        AddText(StrCheckUpdates);
        _binariesMode = false;
    }
    else if (result == ClientConnection::ConnectResult::CompatibilityOutdated) {
        AddText(StrConnectionEstablished);
        AddText(StrCheckUpdates);

        if (!CanSelfUpdateNativeModules(GetCurrentUpdatePlatform())) {
            _result = UpdaterResult::PlatformUnsupported;
            _fileListReceived = true;
            _conn.Disconnect();
            return;
        }

        _binariesMode = true;
    }
    else if (result == ClientConnection::ConnectResult::UpdaterOutdated) {
        _result = UpdaterResult::UpdaterOutdated;
        Abort(StrUpdaterOutdated);
    }
    else {
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

    const auto data_size = _conn.InBuf->Read<uint32_t>();

    vector<uint8_t> data;
    data.resize(data_size);
    _conn.InBuf->Pop(data.data(), data_size);

    vector<vector<uint8_t>> globals_properties_data;
    _conn.InBuf->ReadPropsData(globals_properties_data);
    const auto time = _conn.InBuf->Read<synctime>();
    ignore_unused(globals_properties_data);

    _gameTime.SetSynchronizedTime(time);

    FO_RUNTIME_ASSERT(!_fileListReceived);
    _fileListReceived = true;

    const auto our_target = _binariesMode ? UpdateFileTarget::ClientBinaries : UpdateFileTarget::ClientResources;

    if (data.empty()) {
        _result = _binariesMode ? UpdaterResult::ServerMissingNativeUpdate : UpdaterResult::ResourcesReady;
        return;
    }

    FileSystem resources;

    if (!_binariesMode) {
        resources.AddDirSource(_settings->ClientResources, false, true, true);
    }

    auto reader = DataReader(data);

    for (int32_t file_index = 0;; file_index++) {
        const auto name_len = reader.Read<int16_t>();

        if (name_len == -1) {
            break;
        }

        FO_RUNTIME_ASSERT(name_len > 0);
        const auto fname = string(reader.ReadPtr<char>(name_len), name_len);
        const auto size = reader.Read<uint64_t>();
        const auto hash = reader.Read<uint64_t>();
        const auto target = reader.Read<UpdateFileTarget>();
        const auto data_index = reader.Read<uint32_t>();

        if (target != our_target) {
            continue;
        }

        if (_binariesMode) {
            _hasMatchingEntries = true;

            const auto file_path = strex(_binaryDir).combine_path(fname).str();

            if (IsDiskFileHashMatch(file_path, size, hash)) {
                continue;
            }
        }
        else {
            auto file_header = resources.ReadFileHeader(fname);

            if (file_header) {
                if (file_header.GetSize() == size) {
                    if (file_header.GetDataSource()->IsDiskDir() && IsDiskFileHashMatch(file_header.GetDiskPath(), size, hash)) {
                        continue;
                    }

                    const auto file = resources.ReadFile(fname);

                    if (file && IsDataHashMatch(file.GetData(), size, hash)) {
                        continue;
                    }
                }
            }
        }

        UpdateFile update_file;
        update_file.Index = numeric_cast<int32_t>(data_index);
        update_file.Name = fname;
        update_file.Size = size;
        update_file.RemaningSize = size;
        update_file.Hash = hash;
        _filesToUpdate.push_back(update_file);
        _filesWholeSize += size;
    }

    reader.VerifyEnd();

    if (!_filesToUpdate.empty()) {
        GetNextFile();
    }
    else if (_binariesMode) {
        _result = _hasMatchingEntries ? UpdaterResult::Failed : UpdaterResult::ServerMissingNativeUpdate;
    }
    else {
        _result = UpdaterResult::ResourcesReady;
    }
}

void Updater::Net_OnTimeSync()
{
    FO_STACK_TRACE_ENTRY();

    const auto time = _conn.InBuf->Read<synctime>();
    _gameTime.SetSynchronizedTimeMonotonic(time);
}

void Updater::Net_OnUpdateFileData()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size = numeric_cast<size_t>(_conn.InBuf->Read<int32_t>());

    _updateFileBuf.resize(data_size);
    _conn.InBuf->Pop(_updateFileBuf.data(), data_size);

    auto& update_file = _filesToUpdate.front();

    // Write data to temp file
    const auto write_size = GetUpdateWriteSize(update_file.RemaningSize, _updateFileBuf.size());

    if (write_size != 0) {
        _tempFile.write(reinterpret_cast<const char*>(_updateFileBuf.data()), static_cast<std::streamsize>(write_size));
    }

    if (!_tempFile) {
        Abort(StrFilesystemError);
        return;
    }

    // Get next portion or finalize data
    FO_RUNTIME_ASSERT(update_file.RemaningSize >= data_size);
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

auto Updater::IsDiskFileHashMatch(string_view file_path, uint64_t expected_size, uint64_t expected_hash) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto local_size = fs_file_size(file_path);

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

    const auto local_mtime = fs_last_write_time(file_path);

    if (_cache.HasEntry(file_path)) {
        const auto data = _cache.GetData(file_path);

        if (data.size() == sizeof(CachedHash)) {
            CachedHash cached;
            MemCopy(&cached, data.data(), sizeof(cached));

            if (cached.Size == *local_size && cached.Mtime == local_mtime) {
                return cached.Hash == expected_hash;
            }
        }
    }

    const auto local_hash = fs_hash_file(file_path);

    if (!local_hash.has_value()) {
        return false;
    }

    const CachedHash entry {*local_size, local_mtime, *local_hash};
    _cache.SetData(file_path, {reinterpret_cast<const uint8_t*>(&entry), sizeof(entry)});

    return *local_hash == expected_hash;
}

auto Updater::IsDataHashMatch(const vector<uint8_t>& data, uint64_t expected_size, uint64_t expected_hash) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<uint64_t>(data.size()) == expected_size && fs_hash_data(data.data(), data.size()) == expected_hash;
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

    const auto backup_path = strex("{}.bak", final_path).str();
    const auto final_exists = fs_exists(final_path);

    (void)fs_remove_file(backup_path);

    if (final_exists && !fs_rename(final_path, backup_path)) {
        return false;
    }

    if (!fs_rename(temp_path, final_path)) {
        if (final_exists) {
            (void)fs_rename(backup_path, final_path);
        }

        return false;
    }

    if (final_exists) {
        (void)fs_remove_file(backup_path);
    }

    return true;
}

auto Updater::GetClientBinaryDir() -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto exe_path = Platform::GetExePath();
    FO_RUNTIME_ASSERT(exe_path.has_value());
    return strex(exe_path.value()).extract_dir().str();
}

auto GetCurrentUpdatePlatform() noexcept -> UpdatePlatform
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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

    const auto exe_path = Platform::GetExePath();
    FO_RUNTIME_ASSERT(exe_path.has_value());
    return strex(exe_path.value()).extract_dir().combine_path(GetCurrentClientRuntimeLibraryName()).str();
}

auto GetClientRuntimeStagingPath() -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}{}", GetClientRuntimeLivePath(), ClientBinaryStagingSuffix).str();
}

auto GetCurrentClientRuntimeLibraryName() -> string
{
    FO_STACK_TRACE_ENTRY();

    if (const auto exe_path = Platform::GetExePath(); exe_path.has_value()) {
        const auto name = strex(exe_path.value()).extract_file_name().erase_file_extension().str();

        if (!name.empty()) {
            return name;
        }
    }

    return string(FO_DEV_NAME);
}

void ShowUpdaterFailure(UpdaterResult result)
{
    FO_STACK_TRACE_ENTRY();

    const auto target_name = GetCurrentBinaryUpdateTargetName();

    switch (result) {
    case UpdaterResult::ServerMissingNativeUpdate:
        Application::ShowErrorMessage(strex("Server doesn't provide a native client update for binary target {}. Please update the client manually", target_name).str(), "", true);
        break;
    case UpdaterResult::UpdaterOutdated:
        Application::ShowErrorMessage("Client updater outdated, please update the base client", "", true);
        break;
    case UpdaterResult::PlatformUnsupported:
        Application::ShowErrorMessage("Client outdated, please update via your app store", "", true);
        break;
    case UpdaterResult::Failed:
        Application::ShowErrorMessage(strex("Failed to update native client modules for binary target {}. Please update the client manually", target_name).str(), "", true);
        break;
    case UpdaterResult::ResourcesReady:
    case UpdaterResult::BinariesStaged:
    default:
        break;
    }
}

FO_END_NAMESPACE
