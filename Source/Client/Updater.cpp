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

static constexpr u8string_view StrCheckUpdates = u8"Check updates";
static constexpr u8string_view StrConnectToServer = u8"Connect to the server";
static constexpr u8string_view StrCantConnectToServer = u8"Can't connect to the server!";
static constexpr u8string_view StrConnectionEstablished = u8"Connection established";
static constexpr u8string_view StrConnectionFailure = u8"Connection failure!";
static constexpr u8string_view StrFilesystemError = u8"File system error!";
static constexpr u8string_view StrUpdaterOutdated = u8"Client updater is incompatible with this server. Please install the latest full client package.";
static constexpr u8string_view StrPlatformUnsupported = u8"Client outdated, please update via your app store";
static constexpr u8string_view StrRestartRequired = u8"Update downloaded. Please restart the client to apply the update.";

static constexpr string_view ClientBinaryStagingSuffix {"-staging"};
static constexpr uint64_t ClientRuntimeBootstrapMaxSize = 4096;

static auto NormalizeClientRuntimeBootstrapTarget(u8string_view runtime_path, u8string_view expected_runtime_file_name) -> optional<u8string>;

Updater::Updater(ptr<GlobalSettings> settings, ptr<IAppWindow> window) :
    _settings {settings},
    _conn(settings),
    _cache(fs_make_writable_path(settings->UserWritablePath, settings->CacheResources)),
    _binaryDir {settings->UserWritablePath.empty() ? GetClientBinaryDir() : settings->UserWritablePath},
    _gameTime(settings),
    _effectMngr(settings, make_ptr(&_resources), window->GetRender()),
    _sprMngr(settings, window, make_ptr(&_resources), make_ptr(&_gameTime), make_ptr(&_effectMngr), make_ptr(&_hashStorage)),
    _fontMngr(make_ptr(&_sprMngr))
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Client updater: created for {}:{}, compatibility {}, binary dir {}, resources {}", _settings->ServerHost, _settings->ServerPort, _settings->CompatibilityVersion, _binaryDir, _settings->ClientResources);

    _startTime = nanotime::now();

    const u8string& resources_dir = IsPackaged() ? settings->ClientResources : settings->BakeOutput;
    _resources.AddPackSource(resources_dir, "Embedded");
    _resources.AddDirSource(_settings->ClientResources, false, true, true);

    if (!settings->UserWritablePath.empty()) {
        u8string writable_resources = fs_make_writable_path(settings->UserWritablePath, settings->ClientResources);
        _resources.AddDirSource(writable_resources, false, true, true);
    }
    if (!_settings->DefaultSplashPack.empty()) {
        _resources.AddPackSource(resources_dir, _settings->DefaultSplashPack, true);
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

    InputEvent ev;
    while (_sprMngr.GetInput()->PollEvent(ev)) {
        if (ev.Type == InputEvent::EventType::KeyDownEvent) {
            if (ev.KeyDown.Code == KeyCode::Escape) {
                GetApp()->RequestQuit();
            }
        }
    }

    // Update indication
    u8string update_text;

    for (const auto& message : _messages) {
        update_text.append(message);
        update_text.append("\n");
    }

    if (!_filesToUpdate.empty()) {
        update_text.append("\n");

        for (const auto& update_file : _filesToUpdate) {
            uint64_t cur_bytes = update_file.Size - update_file.RemaningSize;

            if (&update_file == &_filesToUpdate.front()) {
                cur_bytes += _conn.GetUnpackedBytesReceived() - _bytesRealReceivedCheckpoint;
            }

            auto cur = numeric_cast<float32_t>(cur_bytes) / (1024.0f * 1024.0f);
            auto max = std::max(numeric_cast<float32_t>(update_file.Size) / (1024.0f * 1024.0f), 0.01f);
            u8string name = u8strex(update_file.Name).format_path();

            update_text.append(u8strex("{} {:.2f} / {:.2f} MB\n", name, cur, max));
        }

        update_text.append("\n");
    }

    int32_t elapsed_time = (nanotime::now() - _startTime).to_ms<int32_t>();
    int32_t dots = iround<int32_t>(std::fmod((nanotime::now() - _startTime).to_ms<float64_t>() / 100.0, 50.0)) + 1;

    for (int32_t i = 0; i < dots; i++) {
        update_text.append(".");
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

void Updater::AddText(u8string_view text)
{
    FO_STACK_TRACE_ENTRY();

    _messages.emplace_back(text);
}

void Updater::Abort(u8string_view text)
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
}

void Updater::GetNextFile()
{
    FO_STACK_TRACE_ENTRY();

    auto file_uses_binary_dir = [&](const UpdateFile& f) { return f.IsClientBinary; };
    auto file_output_dir = [&](const UpdateFile& f) -> u8string { return file_uses_binary_dir(f) ? _binaryDir : fs_make_writable_path(_settings->UserWritablePath, _settings->ClientResources); };
    auto make_temp_path = [&](const UpdateFile& f) -> u8string {
        u8string output_dir = file_output_dir(f);
        u8string temp_name = FormatUtf8(u8"~{}", f.Name);
        return fs_combine_path(output_dir.view(), temp_name.view());
    };
    auto make_live_path = [&](const UpdateFile& f) -> u8string {
        u8string output_dir = file_output_dir(f);
        return fs_combine_path(output_dir.view(), f.Name.view());
    };
    auto make_final_path = [&](const UpdateFile& f) -> u8string {
        u8string live_path = make_live_path(f);

        if (file_uses_binary_dir(f)) {
            live_path.append(ClientBinaryStagingSuffix);
        }

        return live_path;
    };
    auto try_promote_staged_binary = [&](const UpdateFile& f, u8string_view staged_path) {
        if (file_uses_binary_dir(f)) {
            u8string live_path = make_live_path(f);
            (void)ReplaceFileSafely(staged_path, live_path.view());
        }
    };

    if (_tempFile.is_open()) {
        _tempFile.close();

        if (_tempFile.fail()) {
            Abort(StrFilesystemError);
            return;
        }

        const auto& prev_update_file = _filesToUpdate.front();
        u8string prev_path = make_final_path(prev_update_file);
        u8string temp_path = make_temp_path(prev_update_file);

        if (!IsDiskFileHashMatch(temp_path.view(), prev_update_file.Size, prev_update_file.Hash)) {
            WriteLog("Client updater: downloaded file hash mismatch, temp {}, file {}", temp_path.view(), prev_update_file.Name);
            Abort(StrFilesystemError);
            return;
        }

        if (!ReplaceFileSafely(temp_path.view(), prev_path.view())) {
            WriteLog("Client updater: failed to promote downloaded file from {} to {}", temp_path.view(), prev_path.view());
            Abort(StrFilesystemError);
            return;
        }

        string_view binary_status = prev_update_file.IsClientBinary ? "yes" : "no";
        WriteLog("Client updater: promoted downloaded file to {}, binary {}", prev_path.view(), binary_status);
        try_promote_staged_binary(prev_update_file, prev_path.view());
        _filesToUpdate.erase(_filesToUpdate.begin());
    }

    if (!_filesToUpdate.empty()) {
        auto& next_update_file = _filesToUpdate.front();
        u8string final_path = make_final_path(next_update_file);
        u8string temp_path = make_temp_path(next_update_file);
        auto temp_file_size = GetDiskFileSize(temp_path.view());

        if (temp_file_size.has_value()) {
            if (*temp_file_size > next_update_file.Size) {
                WriteLog("Client updater: temp file {} is too large, size {}, expected {}", temp_path.view(), *temp_file_size, next_update_file.Size);
                (void)fs_remove_file(temp_path.view());
                next_update_file.RemaningSize = next_update_file.Size;
            }
            else if (*temp_file_size == next_update_file.Size) {
                if (!IsDiskFileHashMatch(temp_path.view(), next_update_file.Size, next_update_file.Hash)) {
                    WriteLog("Client updater: complete temp file {} has wrong hash, restarting download", temp_path.view());
                    (void)fs_remove_file(temp_path.view());
                    next_update_file.RemaningSize = next_update_file.Size;
                }
                else {
                    if (!ReplaceFileSafely(temp_path.view(), final_path.view())) {
                        WriteLog("Client updater: failed to promote existing temp file from {} to {}", temp_path.view(), final_path.view());
                        Abort(StrFilesystemError);
                        return;
                    }

                    string_view binary_status = next_update_file.IsClientBinary ? "yes" : "no";
                    WriteLog("Client updater: promoted existing temp file to {}, binary {}", final_path.view(), binary_status);
                    try_promote_staged_binary(next_update_file, final_path.view());
                    _filesToUpdate.erase(_filesToUpdate.begin());
                    GetNextFile();
                    return;
                }
            }
            else {
                next_update_file.RemaningSize = next_update_file.Size - *temp_file_size;
                WriteLog("Client updater: resuming temp file {}, downloaded {}, remaining {}", temp_path.view(), *temp_file_size, next_update_file.RemaningSize);
            }
        }

        u8string dir = fs_path_to_u8string(std::filesystem::path {fs_make_path(temp_path.view())}.parent_path());

        if (!dir.empty()) {
            if (!fs_create_directories(dir.view())) {
                Abort(StrFilesystemError);
                return;
            }
        }

        std::ios_base::openmode open_mode = std::ios::binary | (next_update_file.RemaningSize != next_update_file.Size ? std::ios::app : std::ios::trunc);
        _tempFile.open(std::filesystem::path {fs_make_path(temp_path.view())}, open_mode);

        if (!_tempFile) {
            WriteLog("Client updater: failed to open temp file {}", temp_path.view());
            Abort(StrFilesystemError);
            return;
        }

        string_view binary_status = next_update_file.IsClientBinary ? "yes" : "no";
        WriteLog("Client updater: requesting file {}, binary {}, size {}, remaining {}, temp {}, final {}", next_update_file.Name, binary_status, next_update_file.Size, next_update_file.RemaningSize, temp_path.view(), final_path.view());
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
        string_view self_update_status = can_self_update_binaries ? "supported" : "unsupported";
        WriteLog("Client updater: server reported CompatibilityOutdated, native self-update {} for {}", self_update_status, GetCurrentBinaryUpdateTargetName());

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

    vector<byte> data;
    data.resize(data_size);

    _conn.InBuf->Pop(data.data(), data_size);

    vector<vector<byte>> globals_properties_data;
    _conn.InBuf->ReadPropsData(globals_properties_data);
    auto time = _conn.InBuf->Read<synctime>();
    ignore_unused(globals_properties_data);

    _gameTime.SetSynchronizedTime(time);

    FO_VERIFY_AND_THROW(!_fileListReceived, "Update file list was already received");
    _fileListReceived = true;

    auto our_target = _binariesMode ? UpdateFileTarget::ClientBinaries : UpdateFileTarget::ClientResources;
    string_view update_mode = _binariesMode ? "binaries" : "resources";
    string_view update_target = _binariesMode ? "ClientBinaries" : "ClientResources";
    WriteLog("Client updater: received update list, bytes {}, mode {}, target {}", data_size, update_mode, update_target);

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
        u8string client_resources = _settings->ClientResources;
        resources.AddDirSource(client_resources, false, true, true);
    }

    auto reader = DataReader(data);
    bool accept_binaries = _binariesMode || CanSelfUpdateNativeModules(GetCurrentUpdatePlatform());
    string runtime_local_prefix = accept_binaries ? GetCurrentClientRuntimeLibraryName() : string {};

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

    while (true) {
        int16_t name_len = reader.Read<int16_t>();

        if (name_len == -1) {
            break;
        }

        FO_VERIFY_AND_THROW(name_len > 0, "Update file name length must be positive", name_len);
        size_t fname_size = numeric_cast<size_t>(name_len);
        string fname;
        fname.resize(fname_size);
        reader.ReadStringBytes(fname);
        auto size = reader.Read<uint64_t>();
        auto hash = reader.Read<uint64_t>();
        auto target = reader.Read<UpdateFileTarget>();
        auto data_index = reader.Read<uint32_t>();

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

            u8string file_path = fs_combine_path(_binaryDir.view(), local_name);

            if (remapped_basename == strex("{}.pdb", runtime_local_prefix).str()) {
                if (fs_exists(file_path.view())) {
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

            if (IsDiskFileHashMatch(file_path.view(), size, hash)) {
                WriteLog("Client updater: binary already matches {}", file_path.view());
                continue;
            }
        }
        else if (target == our_target) {
            auto file_header = resources.ReadFileHeader(fname);

            if (file_header) {
                if (file_header.GetSize() == size) {
                    if (file_header.GetDataSource()->IsDiskDir()) {
                        u8string disk_path = file_header.GetDiskPath();

                        if (IsDiskFileHashMatch(disk_path.view(), size, hash)) {
                            continue;
                        }
                    }

                    auto file = resources.ReadFile(fname);

                    if (file && IsDataHashMatch(file.GetDataSpan(), size, hash)) {
                        continue;
                    }
                }
            }
        }
        else {
            continue;
        }

        UpdateFile update_file;
        update_file.Index = numeric_cast<int32_t>(data_index);
        update_file.Name = local_name;
        update_file.Size = size;
        update_file.RemaningSize = size;
        update_file.Hash = hash;
        update_file.IsClientBinary = is_client_binary;
        _filesToUpdate.emplace_back(std::move(update_file));
    }

    reader.VerifyEnd();

    if (!_filesToUpdate.empty()) {
        string_view update_mode = _binariesMode ? "binaries" : "resources";
        WriteLog("Client updater: {} files need update in {} mode", _filesToUpdate.size(), update_mode);
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

    if (data_size_raw < 0) {
        Abort(StrFilesystemError);
        return;
    }

    auto data_size = numeric_cast<size_t>(data_size_raw);

    _updateFileBuf.resize(data_size);

    _conn.InBuf->Pop(_updateFileBuf.data(), data_size);

    if (_filesToUpdate.empty() || !_tempFile.is_open()) {
        Abort(StrFilesystemError);
        return;
    }

    auto& update_file = _filesToUpdate.front();

    if (numeric_cast<uint64_t>(data_size) > update_file.RemaningSize) {
        Abort(StrFilesystemError);
        return;
    }

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

auto Updater::IsDiskFileHashMatch(u8string_view file_path, uint64_t expected_size, uint64_t expected_hash) -> bool
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

    // Keyed by the whole path: two same-named files in different directories would otherwise share one
    // entry, and a size plus write-time collision would answer this check with the other file's hash.
    u8string cache_key = u8strex("{}-{:016x}.hash", u8strvex(file_path).extract_file_name(), hashing::hash<std::u8string_view> {}(file_path.native_view()));

    if (_cache.HasEntry(cache_key)) {
        auto data = _cache.GetBytes(cache_key);

        if (data.size() == sizeof(CachedHash)) {
            CachedHash cached {};
            auto target = make_ptr(&cached).reinterpret_as<byte>();
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
    _cache.SetBytes(cache_key, make_byte_span(&entry, sizeof(entry)));

    return *local_hash == expected_hash;
}

auto Updater::IsDataHashMatch(const_span<byte> data, uint64_t expected_size, uint64_t expected_hash) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<uint64_t>(data.size()) == expected_size && fs_hash_bytes(data) == expected_hash;
}

auto Updater::GetDiskFileSize(u8string_view file_path) -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    return fs_file_size(file_path);
}

auto Updater::GetUpdateWriteSize(uint64_t remaining_size, size_t received_size) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return remaining_size < numeric_cast<uint64_t>(received_size) ? numeric_cast<size_t>(remaining_size) : received_size;
}

auto Updater::ReplaceFileSafely(u8string_view temp_path, u8string_view final_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    u8string backup_path {final_path};
    backup_path.append(u8".bak");
    auto final_exists = fs_exists(final_path);

    (void)fs_remove_file(backup_path.view());

    if (final_exists && !fs_rename(final_path, backup_path.view())) {
        return false;
    }

    if (!fs_rename(temp_path, final_path)) {
        if (final_exists) {
            (void)fs_rename(backup_path.view(), final_path);
        }

        return false;
    }

    if (final_exists) {
        (void)fs_remove_file(backup_path.view());
    }

    return true;
}

auto Updater::GetClientBinaryDir() -> u8string
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (FO_WEB) {
        // The web client runs from the virtual filesystem root and has no on-disk exe path.
        return u8string {u8"/"};
    }
    else {
        auto exe_path = Platform::GetExePath();
        FO_VERIFY_AND_THROW(exe_path.has_value(), "Executable path could not be resolved");
        return fs_path_to_u8string(std::filesystem::path {fs_make_path(exe_path->view())}.parent_path());
    }
}

auto Updater::GetRuntimeLivePath() const -> u8string
{
    FO_STACK_TRACE_ENTRY();

    u8string runtime_name = GetCurrentClientRuntimeLibraryName();
    u8string runtime_path = fs_combine_path(_binaryDir.view(), runtime_name.view());
    runtime_path.append(GetClientRuntimeLibraryExtension());
    return runtime_path;
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

auto GetClientRuntimeLivePath() -> u8string
{
    FO_STACK_TRACE_ENTRY();

    u8string binary_dir;

    if constexpr (FO_WEB) {
        // No on-disk runtime companion on web; the runtime lives at the virtual filesystem root.
        binary_dir = "/";
    }
    else {
        auto exe_path = Platform::GetExePath();
        FO_VERIFY_AND_THROW(exe_path.has_value(), "Executable path could not be resolved");
        binary_dir = fs_path_to_u8string(std::filesystem::path {fs_make_path(exe_path->view())}.parent_path());
    }

    u8string runtime_file_name = GetCurrentClientRuntimeLibraryName();
    runtime_file_name.append(GetClientRuntimeLibraryExtension());
    return fs_path_to_u8string(std::filesystem::path {fs_make_path(binary_dir.view())} / std::filesystem::path {fs_make_path(runtime_file_name.view())});
}

auto MakeClientRuntimeStagingPath(u8string_view runtime_live_path) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    u8string staging_path {runtime_live_path};
    staging_path.append(ClientBinaryStagingSuffix);
    return staging_path;
}

auto ResolveClientRuntimeBootstrapTarget(u8string_view bootstrap_file_path, u8string_view expected_runtime_file_name, u8string_view fallback_runtime_path) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    optional<u8string> target = ReadClientRuntimeBootstrapTarget(bootstrap_file_path, expected_runtime_file_name);

    if (!target.has_value()) {
        return u8string {fallback_runtime_path};
    }

    u8string staging_path = MakeClientRuntimeStagingPath(target->view());
    bool live_exists = fs_exists(target->view()) && !fs_is_dir(target->view());
    bool staging_exists = fs_exists(staging_path.view()) && !fs_is_dir(staging_path.view());
    return live_exists || staging_exists ? target.value() : u8string {fallback_runtime_path};
}

auto ReadClientRuntimeBootstrapTarget(u8string_view bootstrap_file_path, u8string_view expected_runtime_file_name) -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_absolute_path(bootstrap_file_path)) {
        return std::nullopt;
    }

    optional<uint64_t> file_size = fs_file_size(bootstrap_file_path);

    if (!file_size.has_value() || file_size.value() == 0 || file_size.value() > ClientRuntimeBootstrapMaxSize) {
        return std::nullopt;
    }

    optional<u8string> content = fs_read_file_text(bootstrap_file_path);

    if (!content.has_value() || content->size() > ClientRuntimeBootstrapMaxSize) {
        return std::nullopt;
    }

    return NormalizeClientRuntimeBootstrapTarget(content->view(), expected_runtime_file_name);
}

auto WriteClientRuntimeBootstrapTarget(u8string_view bootstrap_file_path, u8string_view runtime_path, u8string_view expected_runtime_file_name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_absolute_path(bootstrap_file_path)) {
        return false;
    }

    optional<u8string> normalized_path = NormalizeClientRuntimeBootstrapTarget(runtime_path, expected_runtime_file_name);

    if (!normalized_path.has_value() || normalized_path->size() + 1 > ClientRuntimeBootstrapMaxSize) {
        return false;
    }

    // Write-then-rename so a crash or a concurrent reader never observes a partially written selector
    u8string temp_path = FormatUtf8("{}.tmp", bootstrap_file_path);
    u8string selector_content = FormatUtf8("{}\n", normalized_path->view());

    if (!fs_write_file_text(temp_path.view(), selector_content.view())) {
        return false;
    }

    if (!fs_rename(temp_path.view(), bootstrap_file_path)) {
        fs_remove_file(temp_path.view());
        return false;
    }

    return true;
}

void PromoteStagedRuntimeCompanions(u8string_view binary_dir) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        u8string runtime_name = GetCurrentClientRuntimeLibraryName();
        u8string runtime_primary_name = runtime_name;
        runtime_primary_name.append(GetClientRuntimeLibraryExtension());
        u8string staging_suffix {ClientBinaryStagingSuffix};

        vector<pair<u8string, u8string>> renames;

        fs_iterate_dir(binary_dir, false, [&](u8string_view path, size_t, uint64_t) {
            u8string file_name = fs_path_to_u8string(std::filesystem::path {fs_make_path(path)}.filename());
            std::u8string_view file_name_view = file_name.view().native_view();
            std::u8string_view staging_suffix_view = staging_suffix.view().native_view();

            if (!file_name_view.ends_with(staging_suffix_view)) {
                return;
            }

            u8string_view unstaged_name = u8string_view::FromChecked(file_name_view.substr(0, file_name_view.size() - staging_suffix_view.size()));

            if (unstaged_name == runtime_primary_name.view()) {
                return;
            }

            std::u8string_view unstaged_name_view = unstaged_name.native_view();
            std::u8string_view runtime_name_view = runtime_name.view().native_view();
            bool matches_runtime = unstaged_name == runtime_name.view() || (unstaged_name_view.size() > runtime_name_view.size() && unstaged_name_view.starts_with(runtime_name_view) && unstaged_name_view[runtime_name_view.size()] == u8'.');

            if (!matches_runtime) {
                return;
            }

            renames.emplace_back(path, fs_path_to_u8string(std::filesystem::path {fs_make_path(binary_dir)} / std::filesystem::path {fs_make_path(unstaged_name)}));
        });

        for (const auto& [staged, final_path] : renames) {
            (void)fs_remove_file(final_path.view());
            (void)fs_rename(staged.view(), final_path.view());
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
        u8string name = u8strex(*exe_path).extract_file_name().erase_file_extension();

        if (!name.empty()) {
            return utf8_to_string(name.view());
        }
    }

    return string {FO_DEV_NAME};
}

void ShowUpdaterFailure(UpdaterResult result)
{
    FO_STACK_TRACE_ENTRY();

    string_view target_name = GetCurrentBinaryUpdateTargetName();

    switch (result) {
    case UpdaterResult::ServerMissingNativeUpdate:
        Application::ShowErrorMessage(u8strex("Server doesn't provide a native client update for binary target {}. Please update the client manually", target_name), u8"", true);
        break;
    case UpdaterResult::UpdaterOutdated:
        Application::ShowErrorMessage(StrUpdaterOutdated, u8"", true);
        break;
    case UpdaterResult::PlatformUnsupported:
        Application::ShowErrorMessage(StrPlatformUnsupported, u8"", true);
        break;
    case UpdaterResult::Failed:
        Application::ShowErrorMessage(u8strex("Failed to update native client modules for binary target {}. Please update the client manually", target_name), u8"", true);
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
    return string_view {};
#endif
}

static auto NormalizeClientRuntimeBootstrapTarget(u8string_view runtime_path, u8string_view expected_runtime_file_name) -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

    std::u8string_view trimmed_path_view = runtime_path.native_view();

    while (!trimmed_path_view.empty() && (trimmed_path_view.front() == u8' ' || trimmed_path_view.front() == u8'\t' || trimmed_path_view.front() == u8'\r' || trimmed_path_view.front() == u8'\n')) {
        trimmed_path_view.remove_prefix(1);
    }
    while (!trimmed_path_view.empty() && (trimmed_path_view.back() == u8' ' || trimmed_path_view.back() == u8'\t' || trimmed_path_view.back() == u8'\r' || trimmed_path_view.back() == u8'\n')) {
        trimmed_path_view.remove_suffix(1);
    }

    u8string trimmed_path = u8string::FromChecked(trimmed_path_view);

    if (trimmed_path.empty() || expected_runtime_file_name.empty() || !fs_is_absolute_path(trimmed_path.view()) || trimmed_path_view.find(u8'\0') != std::u8string_view::npos || trimmed_path_view.find(u8'\r') != std::u8string_view::npos || trimmed_path_view.find(u8'\n') != std::u8string_view::npos) {
        return std::nullopt;
    }

    u8string file_name = fs_path_to_u8string(std::filesystem::path {fs_make_path(trimmed_path.view())}.filename());

    if (file_name.view() != expected_runtime_file_name) {
        return std::nullopt;
    }

    return fs_resolve_path(trimmed_path.view());
}

FO_END_NAMESPACE
