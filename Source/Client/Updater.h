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

#pragma once

#include "Common.h"

#include "ClientConnection.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "Settings.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

class Updater final
{
public:
    Updater() = delete;
    explicit Updater(GlobalSettings& settings, AppWindow* window);
    Updater(const Updater&) = delete;
    Updater(Updater&&) noexcept = delete;
    auto operator=(const Updater&) = delete;
    auto operator=(Updater&&) noexcept = delete;
    ~Updater() = default;

    [[nodiscard]] auto Process() -> bool;

private:
    struct UpdateFile
    {
        int32 Index {};
        string Name;
        size_t Size {};
        size_t RemaningSize {};
        uint32 Hash {};
    };

    void AddText(string_view text);
    void Abort(string_view text);
    void GetNextFile();

    void Net_OnConnect(ClientConnection::ConnectResult result);
    void Net_OnDisconnect();
    void Net_OnInitData();
    void Net_OnUpdateFileData();

    raw_ptr<ClientSettings> _settings;
    ClientConnection _conn;
    FileSystem _resources {};
    GameTimer _gameTime;
    EffectManager _effectMngr;
    HashStorage _hashStorage {};
    SpriteManager _sprMngr;
    nanotime _startTime {};
    bool _aborted {};
    vector<string> _messages {};
    bool _fileListReceived {};
    vector<UpdateFile> _filesToUpdate {};
    size_t _filesWholeSize {};
    unique_ptr<DiskFile> _tempFile {};
    vector<uint8> _updateFileBuf {};
    shared_ptr<Sprite> _splashPic {};
    vector<vector<uint8>> _globalsPropertiesData {};
    size_t _bytesRealReceivedCheckpoint {};
};

FO_END_NAMESPACE();
