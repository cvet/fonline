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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#if !FO_SINGLEPLAYER

#include "ClientScripting.h"
#include "Log.h"
#include "NetCommand.h"
#include "StringUtils.h"

Updater::Updater(GlobalSettings& settings, AppWindow* window) : _settings {settings}, _conn(settings), _effectMngr(_settings, _resources), _sprMngr(_settings, window, _resources, _effectMngr)
{
    PROFILER_ENTRY();

    _startTick = Timer::RealtimeTick();

    _resources.AddDataSource(_settings.EmbeddedResources);
    _resources.AddDataSource(_settings.ResourcesDir, DataSourceType::DirRoot);

    if (!_settings.DefaultSplashPack.empty()) {
        _resources.AddDataSource(_str(_settings.ResourcesDir).combinePath(_settings.DefaultSplashPack), DataSourceType::MaybeNotAvailable);
    }

    _effectMngr.LoadMinimalEffects();

    // Wait screen
    if (!_settings.DefaultSplash.empty()) {
        _sprMngr.PushAtlasType(AtlasType::Splash, true);
        _splashPic = _sprMngr.LoadAnimation(_settings.DefaultSplash, false);
        _sprMngr.PopAtlasType();
    }

    _sprMngr.BeginScene(COLOR_RGB(0, 0, 0));
    if (_splashPic != nullptr) {
        _sprMngr.DrawSpriteSize(_splashPic->GetSprId(), 0, 0, _settings.ScreenWidth, _settings.ScreenHeight, true, true, 0);
    }
    _sprMngr.EndScene();

    // Load font
    _sprMngr.PushAtlasType(AtlasType::Static);
    _sprMngr.LoadFontFO(FONT_DEFAULT, "Default", false, true);
    _sprMngr.PopAtlasType();
    _sprMngr.BuildFonts();
    _sprMngr.SetDefaultFont(FONT_DEFAULT, COLOR_TEXT);

    // Network handlers
    _conn.AddConnectHandler([this](bool success) { Net_OnConnect(success); });
    _conn.AddDisconnectHandler([this] { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NETMSG_UPDATE_FILES_LIST, [this] { Net_OnUpdateFilesResponse(); });
    _conn.AddMessageHandler(NETMSG_UPDATE_FILE_DATA, [this] { Net_OnUpdateFileData(); });

    // Connect
    AddText(STR_CONNECT_TO_SERVER, "Connect to server...");
    _conn.Connect();
}

void Updater::Net_OnConnect(bool success)
{
    PROFILER_ENTRY();

    if (success) {
        AddText(STR_CONNECTION_ESTABLISHED, "Connection established.");
        AddText(STR_CHECK_UPDATES, "Check updates...");
    }
    else {
        Abort(STR_CANT_CONNECT_TO_SERVER, "Can't connect to server!");
    }
}

void Updater::Net_OnDisconnect()
{
    PROFILER_ENTRY();

    if (!_aborted && (!_fileListReceived || !_filesToUpdate.empty())) {
        Abort(STR_CONNECTION_FAILTURE, "Connection failture!");
    }
}

auto Updater::Process() -> bool
{
    PROFILER_ENTRY();

    InputEvent ev;
    while (App->Input.PollEvent(ev)) {
        if (ev.Type == InputEvent::EventType::KeyDownEvent) {
            if (ev.KeyDown.Code == KeyCode::Escape) {
                App->Settings.Quit = true;
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

            const auto cur = static_cast<float>(cur_bytes) / (1024.0f * 1024.0f);
            const auto max = std::max(static_cast<float>(update_file.Size) / (1024.0f * 1024.0f), 0.01f);
            const string name = _str(update_file.Name).formatPath();

            update_text += _str("{} {:.2f} / {:.2f} MB\n", name, cur, max);
        }

        update_text += "\n";
    }

    const auto elapsed_time = Timer::RealtimeTick() - _startTick;
    const auto dots = static_cast<int>(std::fmod((Timer::RealtimeTick() - _startTick) / 100.0, 50.0)) + 1;
    for ([[maybe_unused]] const auto i : xrange(dots)) {
        update_text += ".";
    }

    _sprMngr.BeginScene(COLOR_RGB(0, 0, 0));
    {
        if (_splashPic != nullptr) {
            _sprMngr.DrawSpriteSize(_splashPic->GetSprId(), 0, 0, _settings.ScreenWidth, _settings.ScreenHeight, true, true, 0);
        }

        if (elapsed_time >= _settings.UpdaterInfoDelay) {
            if (_settings.UpdaterInfoPos < 0) {
                _sprMngr.DrawStr(IRect(0, 0, _settings.ScreenWidth, _settings.ScreenHeight / 2), update_text, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
            else if (_settings.UpdaterInfoPos == 0) {
                _sprMngr.DrawStr(IRect(0, 0, _settings.ScreenWidth, _settings.ScreenHeight), update_text, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
            else {
                _sprMngr.DrawStr(IRect(0, _settings.ScreenHeight / 2, _settings.ScreenWidth, _settings.ScreenHeight), update_text, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT);
            }
        }
    }
    _sprMngr.EndScene();

    _conn.Process();

    return !_aborted && _fileListReceived && _filesToUpdate.empty();
}

auto Updater::MakeWritePath(string_view fname) const -> string
{
    PROFILER_ENTRY();

    return _str(_settings.ResourcesDir).combinePath(fname).str();
}

void Updater::AddText(uint num_str, string_view num_str_str)
{
    PROFILER_ENTRY();

    _messages.emplace_back(num_str_str);
}

void Updater::Abort(uint num_str, string_view num_str_str)
{
    PROFILER_ENTRY();

    _aborted = true;

    AddText(num_str, num_str_str);
    _conn.Disconnect();

    if (_tempFile) {
        _tempFile = nullptr;
    }
}

void Updater::Net_OnUpdateFilesResponse()
{
    PROFILER_ENTRY();

    uint msg_len;
    bool outdated;
    uint data_size;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> outdated;
    _conn.InBuf >> data_size;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    vector<uchar> data;
    data.resize(data_size);
    _conn.InBuf.Pop(data.data(), data_size);

    if (!outdated) {
        NET_READ_PROPERTIES(_conn.InBuf, _globalsPropertiesData);
    }

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (outdated) {
        Abort(STR_CLIENT_OUTDATED, "Client binary outdated");
        return;
    }

    RUNTIME_ASSERT(!_fileListReceived);
    _fileListReceived = true;

    if (!data.empty()) {
        auto reader = DataReader(data);

        for (uint file_index = 0;; file_index++) {
            const auto name_len = reader.Read<short>();
            if (name_len == -1) {
                break;
            }

            RUNTIME_ASSERT(name_len > 0);
            const auto fname = string(reader.ReadPtr<char>(name_len), name_len);
            const auto size = reader.Read<uint>();
            const auto hash = reader.Read<uint>();

            // Check hash
            if (auto file = _resources.ReadFileHeader(fname)) {
                // Todo: add update file files checking by hashes
                /*const auto file_hash = _resources.ReadFileText(_str("{}.hash", fname));
                if (file_hash.empty()) {
                    // Hashing::MurmurHash2(file2.GetBuf(), file2.GetSize());
                }

                if (_str(file_hash).toUInt() == hash) {
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
    PROFILER_ENTRY();

    uint msg_len;
    uint data_size;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> data_size;

    _updateFileBuf.resize(data_size);
    _conn.InBuf.Pop(_updateFileBuf.data(), data_size);

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    auto& update_file = _filesToUpdate.front();

    // Write data to temp file
    if (!_tempFile->Write(_updateFileBuf.data(), std::min(update_file.RemaningSize, _updateFileBuf.size()))) {
        Abort(STR_FILESYSTEM_ERROR, "Can't write temp file!");
        return;
    }

    // Get next portion or finalize data
    RUNTIME_ASSERT(update_file.RemaningSize >= data_size);
    update_file.RemaningSize -= data_size;
    if (update_file.RemaningSize > 0u) {
        _conn.OutBuf << NETMSG_GET_UPDATE_FILE_DATA;
        _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
    }
    else {
        GetNextFile();
    }
}

void Updater::GetNextFile()
{
    PROFILER_ENTRY();

    if (_tempFile) {
        _tempFile = nullptr;

        const auto& prev_update_file = _filesToUpdate.front();

        if (!DiskFileSystem::DeleteFile(MakeWritePath(prev_update_file.Name))) {
            Abort(STR_FILESYSTEM_ERROR, "File system error!");
            return;
        }
        if (!DiskFileSystem::RenameFile(MakeWritePath(_str("~{}", prev_update_file.Name)), MakeWritePath(prev_update_file.Name))) {
            Abort(STR_FILESYSTEM_ERROR, "File system error!");
            return;
        }

        _filesToUpdate.erase(_filesToUpdate.begin());
    }

    if (!_filesToUpdate.empty()) {
        const auto& next_update_file = _filesToUpdate.front();

        _conn.OutBuf << NETMSG_GET_UPDATE_FILE;
        _conn.OutBuf << next_update_file.Index;

        DiskFileSystem::DeleteFile(MakeWritePath(_str("~{}", next_update_file.Name)));
        _tempFile = std::make_unique<DiskFile>(DiskFileSystem::OpenFile(MakeWritePath(_str("~{}", next_update_file.Name)), true));

        if (!*_tempFile) {
            Abort(STR_FILESYSTEM_ERROR, "File system error!");
        }
    }

    _bytesRealReceivedCheckpoint = _conn.GetUnpackedBytesReceived();
}

#endif
