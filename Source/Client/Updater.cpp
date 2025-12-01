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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: support restoring file downloading from interrupted position

#include "Updater.h"
#include "DefaultSprites.h"
#include "NetCommand.h"

FO_BEGIN_NAMESPACE();

static auto* StrCheckUpdates = "Check updates";
static auto* StrConnectToServer = "Connect to the server";
static auto* StrCantConnectToServer = "Can't connect to the server!";
static auto* StrConnectionEstablished = "Connection established";
static auto* StrConnectionFailure = "Connection failure!";
static auto* StrFilesystemError = "File system error!";
static auto* StrClientOutdated = "Client outdated, please update it";

Updater::Updater(GlobalSettings& settings, AppWindow* window) :
    _settings {&settings},
    _conn(*_settings),
    _gameTime(*_settings),
    _effectMngr(*_settings, _resources),
    _sprMngr(*_settings, window, _resources, _gameTime, _effectMngr, _hashStorage)
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
        _splashPic.reset();
        _splashPic = _sprMngr.LoadSprite(_settings->DefaultSplash, AtlasType::OneImage);

        if (_splashPic) {
            _splashPic->PlayDefault();
        }
    }

    _sprMngr.BeginScene();
    if (_splashPic) {
        _sprMngr.DrawSpriteSize(_splashPic.get(), {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, COLOR_SPRITE);
    }
    _sprMngr.EndScene();

    // Load font
    _sprMngr.LoadFontFO(0, "Default", AtlasType::IfaceSprites, false, true);

    // Network handlers
    _conn.SetConnectHandler([this](ClientConnection::ConnectResult result) { Net_OnConnect(result); });
    _conn.SetDisconnectHandler([this] { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NetMessage::InitData, [this] { Net_OnInitData(); });
    _conn.AddMessageHandler(NetMessage::UpdateFileData, [this] { Net_OnUpdateFileData(); });

    // Connect
    AddText(StrConnectToServer);
    _conn.Connect();

    // Unlock all resources to prevent collision with new files
    _resources.CleanDataSources();
}

void Updater::Net_OnConnect(ClientConnection::ConnectResult result)
{
    FO_STACK_TRACE_ENTRY();

    if (result == ClientConnection::ConnectResult::Success) {
        AddText(StrConnectionEstablished);
        AddText(StrCheckUpdates);
    }
    else if (result == ClientConnection::ConnectResult::Outdated) {
        Abort(StrClientOutdated);
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

auto Updater::Process() -> bool
{
    FO_STACK_TRACE_ENTRY();

    _gameTime.FrameAdvance();

    InputEvent ev;
    while (App->Input.PollEvent(ev)) {
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

            const auto cur = numeric_cast<float32>(cur_bytes) / (1024.0f * 1024.0f);
            const auto max = std::max(numeric_cast<float32>(update_file.Size) / (1024.0f * 1024.0f), 0.01f);
            const string name = strex(update_file.Name).format_path();

            update_text += strex("{} {:.2f} / {:.2f} MB\n", name, cur, max);
        }

        update_text += "\n";
    }

    const auto elapsed_time = (nanotime::now() - _startTime).to_ms<int32>();
    const auto dots = iround<int32>(std::fmod((nanotime::now() - _startTime).to_ms<float64>() / 100.0, 50.0)) + 1;

    for ([[maybe_unused]] const auto i : iterate_range(dots)) {
        update_text += ".";
    }

    {
        _sprMngr.BeginScene();

        if (_splashPic) {
            _sprMngr.DrawSpriteSize(_splashPic.get(), {0, 0}, {_settings->ScreenWidth, _settings->ScreenHeight}, true, true, COLOR_SPRITE);
        }

        if (elapsed_time >= _settings->UpdaterInfoDelay) {
            if (_settings->UpdaterInfoPos < 0) {
                _sprMngr.DrawText({0, 0, _settings->ScreenWidth, _settings->ScreenHeight / 2}, update_text, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, 0);
            }
            else if (_settings->UpdaterInfoPos == 0) {
                _sprMngr.DrawText({0, 0, _settings->ScreenWidth, _settings->ScreenHeight}, update_text, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, 0);
            }
            else {
                _sprMngr.DrawText({0, _settings->ScreenHeight / 2, _settings->ScreenWidth, _settings->ScreenHeight / 2}, update_text, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, 0);
            }
        }

        _sprMngr.EndScene();
    }

    _conn.Process();

    return !_aborted && _fileListReceived && _filesToUpdate.empty();
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

    if (_tempFile) {
        _tempFile = nullptr;
    }
}

void Updater::Net_OnInitData()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size = _conn.InBuf.Read<uint32>();

    vector<uint8> data;
    data.resize(data_size);
    _conn.InBuf.Pop(data.data(), data_size);

    _conn.InBuf.ReadPropsData(_globalsPropertiesData);
    const auto time = _conn.InBuf.Read<synctime>();

    _gameTime.SetSynchronizedTime(time);

    FO_RUNTIME_ASSERT(!_fileListReceived);
    _fileListReceived = true;

    if (!data.empty()) {
        FileSystem resources;
        resources.AddDirSource(_settings->ClientResources, false, true, true);

        auto reader = DataReader(data);

        for (int32 file_index = 0;; file_index++) {
            const auto name_len = reader.Read<int16>();

            if (name_len == -1) {
                break;
            }

            FO_RUNTIME_ASSERT(name_len > 0);
            const auto fname = string(reader.ReadPtr<char>(name_len), name_len);
            const auto size = reader.Read<uint32>();
            const auto hash = reader.Read<uint32>();

            // Check hash
            if (auto file = resources.ReadFileHeader(fname)) {
                // Todo: add update file files checking by hashes
                /*const auto file_hash = resources.ReadFileText(strex("{}.hash", fname));
                if (file_hash.empty()) {
                    // Hashing::MurmurHash2(file2.GetBuf(), file2.GetSize());
                }

                if (strex(file_hash).to_uint32() == hash) {
                    continue;
                }*/

                if (file.GetSize() == size) {
                    continue;
                }
            }

            // Get this file
            UpdateFile update_file;
            update_file.Index = file_index;
            update_file.Name = fname;
            update_file.Size = size;
            update_file.RemaningSize = size;
            update_file.Hash = hash;
            _filesToUpdate.push_back(update_file);
            _filesWholeSize += size;
        }

        if (!_filesToUpdate.empty()) {
            GetNextFile();
        }
    }
}

void Updater::Net_OnUpdateFileData()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size = numeric_cast<size_t>(_conn.InBuf.Read<int32>());

    _updateFileBuf.resize(data_size);
    _conn.InBuf.Pop(_updateFileBuf.data(), data_size);

    auto& update_file = _filesToUpdate.front();

    // Write data to temp file
    if (!_tempFile->Write(_updateFileBuf.data(), std::min(update_file.RemaningSize, _updateFileBuf.size()))) {
        Abort(StrFilesystemError);
        return;
    }

    // Get next portion or finalize data
    FO_RUNTIME_ASSERT(update_file.RemaningSize >= data_size);
    update_file.RemaningSize -= data_size;

    if (update_file.RemaningSize > 0u) {
        _conn.OutBuf.StartMsg(NetMessage::GetUpdateFileData);
        _conn.OutBuf.EndMsg();
        _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
    }
    else {
        GetNextFile();
    }
}

void Updater::GetNextFile()
{
    FO_STACK_TRACE_ENTRY();

    const auto make_write_path = [this](string_view fname) -> string { return strex(_settings->ClientResources).combine_path(fname); };

    if (_tempFile) {
        _tempFile = nullptr;

        const auto& prev_update_file = _filesToUpdate.front();

        if (!DiskFileSystem::DeleteFile(make_write_path(prev_update_file.Name))) {
            Abort(StrFilesystemError);
            return;
        }
        if (!DiskFileSystem::RenameFile(make_write_path(strex("~{}", prev_update_file.Name)), make_write_path(prev_update_file.Name))) {
            Abort(StrFilesystemError);
            return;
        }

        _filesToUpdate.erase(_filesToUpdate.begin());
    }

    if (!_filesToUpdate.empty()) {
        const auto& next_update_file = _filesToUpdate.front();

        _conn.OutBuf.StartMsg(NetMessage::GetUpdateFile);
        _conn.OutBuf.Write(next_update_file.Index);
        _conn.OutBuf.EndMsg();

        DiskFileSystem::DeleteFile(make_write_path(strex("~{}", next_update_file.Name)));
        _tempFile = SafeAlloc::MakeUnique<DiskFile>(DiskFileSystem::OpenFile(make_write_path(strex("~{}", next_update_file.Name)), true));

        if (!*_tempFile) {
            Abort(StrFilesystemError);
        }
    }

    _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
}

FO_END_NAMESPACE();
