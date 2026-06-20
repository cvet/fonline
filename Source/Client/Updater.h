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

#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "ClientConnection.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "FontManager.h"
#include "Settings.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

enum class UpdaterResult : uint8_t
{
    ResourcesReady = 0, // Gameplay compat OK; resources are now in sync, caller may start the game.
    BinariesStaged = 1, // Gameplay compat outdated; native modules are ready on disk, caller must reload.
    PlatformUnsupported = 2, // Compat outdated and CanSelfUpdateNativeModules() == false (Web / iOS / Android).
    ServerMissingNativeUpdate = 3, // Compat outdated but server has no binaries for our target — config bug.
    UpdaterOutdated = 4, // FO_UPDATER_VERSION mismatch; protocol is unusable.
    Failed = 5, // Any other failure: connection, disk, etc.
};

extern auto GetCurrentUpdatePlatform() noexcept -> UpdatePlatform;
extern auto GetUpdatePlatformName(UpdatePlatform platform) noexcept -> string_view;
extern auto CanSelfUpdateNativeModules(UpdatePlatform platform) noexcept -> bool;
extern auto GetCurrentBinaryUpdateTargetName() noexcept -> string_view;
extern auto GetClientRuntimeLivePath() -> string;
extern auto MakeClientRuntimeStagingPath(string_view runtime_live_path) -> string;
extern auto GetCurrentClientRuntimeLibraryName() -> string;
extern void PromoteStagedRuntimeCompanions(string_view binary_dir) noexcept;
extern void ShowUpdaterFailure(UpdaterResult result);
extern auto GetClientRuntimeLibraryExtension() noexcept -> string_view;

class Updater final
{
public:
    Updater() = delete;
    Updater(GlobalSettings& settings, IAppWindow& window);
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
    // reached a terminal state and the caller should inspect GetResult().
    auto Process() -> bool;

private:
    struct UpdateFile
    {
        int32_t Index {};
        string Name;
        uint64_t Size {};
        uint64_t RemaningSize {};
        uint64_t Hash {};
        bool IsClientBinary {};
    };

    void AddText(string_view text);
    void Abort(string_view text);
    void GetNextFile();
    void RequestUpdateFile(const UpdateFile& update_file);

    void Net_OnConnect(ClientConnection::ConnectResult result);
    void Net_OnDisconnect();
    void Net_OnInitData();
    void Net_OnTimeSync();
    void Net_OnUpdateFileData();

    auto IsDiskFileHashMatch(string_view file_path, uint64_t expected_size, uint64_t expected_hash) -> bool;

    static auto IsDataHashMatch(const vector<uint8_t>& data, uint64_t expected_size, uint64_t expected_hash) noexcept -> bool;
    static auto GetDiskFileSize(string_view file_path) -> optional<uint64_t>;
    static auto GetUpdateWriteSize(uint64_t remaining_size, size_t received_size) -> size_t;
    static auto ReplaceFileSafely(string_view temp_path, string_view final_path) -> bool;
    static auto GetClientBinaryDir() -> string;

    raw_ptr<ClientSettings> _settings;
    ClientConnection _conn;
    CacheStorage _cache;
    string _binaryDir;
    optional<UpdaterResult> _result;
    bool _binariesMode {};
    bool _aborted {};
    bool _fileListReceived {};
    bool _hasMatchingEntries {};
    vector<UpdateFile> _filesToUpdate {};
    std::ofstream _tempFile {};
    vector<uint8_t> _updateFileBuf {};
    vector<string> _messages {};
    FileSystem _resources {};
    GameTimer _gameTime;
    EffectManager _effectMngr;
    HashStorage _hashStorage {};
    SpriteManager _sprMngr;
    FontManager _fontMngr;
    nanotime _startTime {};
    shared_ptr<Sprite> _splashPic {};
    size_t _bytesRealReceivedCheckpoint {};
};

FO_END_NAMESPACE
