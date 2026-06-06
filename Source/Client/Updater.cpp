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

    WriteLog("Client updater: created for {}:{}, compatibility {}, binary dir {}, resources {}", _settings->ServerHost, _settings->ServerPort, _settings->CompatibilityVersion, _binaryDir, _settings->ClientResources);

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
        _sprMngr.DrawSpriteSize(_splashPic.get(), {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, Color::Neutral);
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
        _sprMngr.DrawSpriteSize(_splashPic.get(), {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, Color::Neutral);
    }

    if (elapsed_time >= _settings->UpdaterInfoDelay) {
        const auto text_format = TextFormat {.Font = FontType::Default, .Flags = CombineEnum(FontFlag::CenterX, FontFlag::CenterY, FontFlag::Bordered)};

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
}

void Updater::GetNextFile()
{
    FO_STACK_TRACE_ENTRY();

    const auto file_uses_binary_dir = [&](const UpdateFile& f) { return f.IsClientBinary; };
    const auto file_output_dir = [&](const UpdateFile& f) -> string { return file_uses_binary_dir(f) ? _binaryDir : string(_settings->ClientResources); };
    const auto make_temp_path = [&](const UpdateFile& f) -> string { return strex(file_output_dir(f)).combine_path(strex("~{}", f.Name)).str(); };
    const auto make_live_path = [&](const UpdateFile& f) -> string { return strex(file_output_dir(f)).combine_path(f.Name).str(); };
    const auto make_final_path = [&](const UpdateFile& f) -> string {
        const auto live_path = make_live_path(f);
        return file_uses_binary_dir(f) ? strex("{}{}", live_path, ClientBinaryStagingSuffix).str() : live_path;
    };
    const auto try_promote_staged_binary = [&](const UpdateFile& f, string_view staged_path) {
        if (file_uses_binary_dir(f)) {
            ReplaceFileSafely(staged_path, make_live_path(f));
        }
    };

    if (_tempFile.is_open()) {
        _tempFile.close();

        if (_tempFile.fail()) {
            Abort(StrFilesystemError);
            return;
        }

        const auto& prev_update_file = _filesToUpdate.front();
        const auto prev_path_str = make_final_path(prev_update_file);
        const auto temp_path_str = make_temp_path(prev_update_file);

        if (!IsDiskFileHashMatch(temp_path_str, prev_update_file.Size, prev_update_file.Hash)) {
            WriteLog("Client updater: downloaded file hash mismatch, temp {}, file {}", temp_path_str, prev_update_file.Name);
            Abort(StrFilesystemError);
            return;
        }

        if (!ReplaceFileSafely(temp_path_str, prev_path_str)) {
            WriteLog("Client updater: failed to promote downloaded file from {} to {}", temp_path_str, prev_path_str);
            Abort(StrFilesystemError);
            return;
        }

        WriteLog("Client updater: promoted downloaded file to {}, binary {}", prev_path_str, prev_update_file.IsClientBinary ? "yes" : "no");
        try_promote_staged_binary(prev_update_file, prev_path_str);
        _filesToUpdate.erase(_filesToUpdate.begin());
    }

    if (!_filesToUpdate.empty()) {
        auto& next_update_file = _filesToUpdate.front();
        const auto prev_path_str = make_final_path(next_update_file);
        const auto temp_path = make_temp_path(next_update_file);
        const auto temp_file_size = GetDiskFileSize(temp_path);

        if (temp_file_size.has_value()) {
            if (*temp_file_size > next_update_file.Size) {
                WriteLog("Client updater: temp file {} is too large, size {}, expected {}", temp_path, *temp_file_size, next_update_file.Size);
                fs_remove_file(temp_path);
                next_update_file.RemaningSize = next_update_file.Size;
            }
            else if (*temp_file_size == next_update_file.Size) {
                if (!IsDiskFileHashMatch(temp_path, next_update_file.Size, next_update_file.Hash)) {
                    WriteLog("Client updater: complete temp file {} has wrong hash, restarting download", temp_path);
                    fs_remove_file(temp_path);
                    next_update_file.RemaningSize = next_update_file.Size;
                }
                else {
                    if (!ReplaceFileSafely(temp_path, prev_path_str)) {
                        WriteLog("Client updater: failed to promote existing temp file from {} to {}", temp_path, prev_path_str);
                        Abort(StrFilesystemError);
                        return;
                    }

                    WriteLog("Client updater: promoted existing temp file to {}, binary {}", prev_path_str, next_update_file.IsClientBinary ? "yes" : "no");
                    try_promote_staged_binary(next_update_file, prev_path_str);
                    _filesToUpdate.erase(_filesToUpdate.begin());
                    GetNextFile();
                    return;
                }
            }
            else {
                next_update_file.RemaningSize = next_update_file.Size - *temp_file_size;
                WriteLog("Client updater: resuming temp file {}, downloaded {}, remaining {}", temp_path, *temp_file_size, next_update_file.RemaningSize);
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
            WriteLog("Client updater: failed to open temp file {}", temp_path);
            Abort(StrFilesystemError);
            return;
        }

        WriteLog("Client updater: requesting file {}, binary {}, size {}, remaining {}, temp {}, final {}", next_update_file.Name, next_update_file.IsClientBinary ? "yes" : "no", next_update_file.Size, next_update_file.RemaningSize, temp_path, prev_path_str);
        RequestUpdateFile(next_update_file);
    }
    else {
        if (_binariesMode) {
            WriteLog("Client updater: finished binary update, binaries staged for reload");
            _result = UpdaterResult::BinariesStaged;
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

    const auto start_offset = update_file.Size - update_file.RemaningSize;

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
        WriteLog("Client updater: server reported CompatibilityOutdated, native self-update {} for {}", CanSelfUpdateNativeModules(GetCurrentUpdatePlatform()) ? "supported" : "unsupported", GetCurrentBinaryUpdateTargetName());

        if (!CanSelfUpdateNativeModules(GetCurrentUpdatePlatform())) {
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

    auto reader = DataReader(data);
    const auto accept_binaries = _binariesMode || CanSelfUpdateNativeModules(GetCurrentUpdatePlatform());
    const string runtime_local_prefix = accept_binaries ? GetCurrentClientRuntimeLibraryName() : string {};

    string runtime_server_prefix;

    if (accept_binaries) {
        runtime_server_prefix = GetPackagedRuntimeName();

        if (runtime_server_prefix.empty()) {
            runtime_server_prefix = runtime_local_prefix;
        }

        WriteLog("Client updater: binary name remap from server prefix {} to local prefix {}", runtime_server_prefix, runtime_local_prefix);
    }

    const auto remap_runtime_name = [&](const string& fname_basename) -> optional<string> {
        if (!fname_basename.starts_with(runtime_server_prefix)) {
            return std::nullopt;
        }

        const string_view rest = string_view(fname_basename).substr(runtime_server_prefix.size());

        if (!rest.empty() && rest[0] != '.') {
            return std::nullopt;
        }

        return strex("{}{}", runtime_local_prefix, rest).str();
    };

    while (true) {
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

        string local_name = fname;
        bool is_client_binary = false;

        if (target == UpdateFileTarget::ClientBinaries) {
            if (!accept_binaries) {
                continue;
            }

            const auto fname_basename = strex(fname).extract_file_name().str();
            const auto remapped = remap_runtime_name(fname_basename);

            if (!remapped.has_value()) {
                continue;
            }

            const auto remapped_basename = *remapped;
            const auto fname_dir = strex(fname).extract_dir().str();
            local_name = fname_dir.empty() ? remapped_basename : strex(fname_dir).combine_path(remapped_basename).str();

            const auto file_path = strex(_binaryDir).combine_path(local_name).str();

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

                    const auto file = resources.ReadFile(fname);

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

    const auto time = _conn.InBuf->Read<synctime>();
    _gameTime.SetSynchronizedTimeMonotonic(time);
}

void Updater::Net_OnUpdateFileData()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size_raw = _conn.InBuf->Read<int32_t>();

    if (data_size_raw < 0) {
        Abort(StrFilesystemError);
        return;
    }

    const auto data_size = numeric_cast<size_t>(data_size_raw);

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
    const auto write_size = GetUpdateWriteSize(update_file.RemaningSize, _updateFileBuf.size());

    if (write_size != 0) {
        _tempFile.write(reinterpret_cast<const char*>(_updateFileBuf.data()), numeric_cast<std::streamsize>(write_size));
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
    const auto cache_key = strex("{}.hash", strex(file_path).extract_file_name()).str();

    if (_cache.HasEntry(cache_key)) {
        const auto data = _cache.GetData(cache_key);

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
    _cache.SetData(cache_key, {reinterpret_cast<const uint8_t*>(&entry), sizeof(entry)});

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

    fs_remove_file(backup_path);

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
        fs_remove_file(backup_path);
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

static auto GetClientRuntimeLibraryExtension() noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

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

auto GetClientRuntimeLivePath() -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto exe_path = Platform::GetExePath();
    FO_RUNTIME_ASSERT(exe_path.has_value());
    return strex("{}{}", strex(exe_path.value()).extract_dir().combine_path(GetCurrentClientRuntimeLibraryName()), GetClientRuntimeLibraryExtension()).str();
}

auto GetClientRuntimeStagingPath() -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}{}", GetClientRuntimeLivePath(), ClientBinaryStagingSuffix).str();
}

void PromoteStagedRuntimeCompanions() noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto runtime_live = GetClientRuntimeLivePath();
        const auto binary_dir = strex(runtime_live).extract_dir().str();
        const auto runtime_primary_name = strex(runtime_live).extract_file_name().str();
        const auto runtime_name = GetCurrentClientRuntimeLibraryName();

        vector<pair<string, string>> renames;

        fs_iterate_dir(binary_dir, false, [&](string_view path, size_t, uint64_t) {
            const auto file_name = strex(path).extract_file_name().str();

            if (!file_name.ends_with(ClientBinaryStagingSuffix)) {
                return;
            }

            const auto unstaged_name = file_name.substr(0, file_name.size() - ClientBinaryStagingSuffix.size());

            if (unstaged_name == runtime_primary_name) {
                return;
            }

            const bool matches_runtime = unstaged_name == runtime_name || (unstaged_name.size() > runtime_name.size() && unstaged_name.starts_with(runtime_name) && unstaged_name[runtime_name.size()] == '.');

            if (!matches_runtime) {
                return;
            }

            renames.emplace_back(string(path), strex(binary_dir).combine_path(unstaged_name).str());
        });

        for (const auto& [staged, final_path] : renames) {
            fs_remove_file(final_path);
            fs_rename(staged, final_path);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
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
