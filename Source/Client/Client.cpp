//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Client.h"

#include "ClientScripting.h"
#include "GenericUtils.h"
#include "Log.h"
#include "NetCommand.h"
#include "StringUtils.h"
#include "Version-Include.h"

#define CHECK_IN_BUFF_ERROR() \
    do { \
        if (_netIn.IsError()) { \
            WriteLog("Wrong network data!\n"); \
            NetDisconnect(); \
            return; \
        } \
    } while (0)

FOClient::FOClient(GlobalSettings& settings, ScriptSystem* script_sys) : FOEngineBase(false), Settings {settings}, GeomHelper(Settings), ScriptSys {script_sys}, GameTime(Settings), EffectMngr(Settings, FileMngr, GameTime), SprMngr(Settings, FileMngr, EffectMngr, GameTime, *this, *this), ResMngr(FileMngr, SprMngr, *this, *this), HexMngr(this), SndMngr(Settings, FileMngr), Keyb(Settings, SprMngr), Cache("Data/Cache.fobin"), _worldmapFog(GM_MAXZONEX, GM_MAXZONEY, nullptr)
{
    _incomeBuf.resize(NetBuffer::DEFAULT_BUF_SIZE);
    _netSock = INVALID_SOCKET;
    _initNetReason = INIT_NET_REASON_NONE;
    _ifaceAnimations.resize(10000);
    _lmapZoom = 2;
    _screenModeMain = SCREEN_WAIT;
    _drawLookBorders = true;
    _fpsTick = GameTime.FrameTick();

    const auto [w, h] = SprMngr.GetWindowSize();
    const auto [x, y] = SprMngr.GetMousePosition();
    Settings.MouseX = std::clamp(x, 0, w - 1);
    Settings.MouseY = std::clamp(y, 0, h - 1);

    SetGameColor(COLOR_IFACE);

    // Data sources
    FileMngr.AddDataSource("$Embedded", true);
#if FO_IOS
    FileMngr.AddDataSource("../../Documents/", true);
#elif FO_ANDROID
    FileMngr.AddDataSource("$AndroidAssets", true);
    // FileMngr.AddDataSource(SDL_AndroidGetInternalStoragePath(), true);
    // FileMngr.AddDataSource(SDL_AndroidGetExternalStoragePath(), true);
#elif FO_WEB
    FileMngr.AddDataSource("Data/", true);
    FileMngr.AddDataSource("PersistentData/", true);
#else
    FileMngr.AddDataSource("Data/", true);
#endif

    EffectMngr.LoadDefaultEffects();

    // Wait screen
    _waitPic = ResMngr.GetRandomSplash();
    SprMngr.BeginScene(COLOR_RGB(0, 0, 0));
    WaitDraw();
    SprMngr.EndScene();

    // Recreate static atlas
    SprMngr.AccumulateAtlasData();
    SprMngr.PushAtlasType(AtlasType::Static);

#if !FO_SINGLEPLAYER
    // Modules initialization
    OnStart.Fire();
#endif

    // Flush atlas data
    SprMngr.PopAtlasType();
    SprMngr.FlushAccumulatedAtlasData();

    // Finish fonts
    SprMngr.BuildFonts();
    SprMngr.SetDefaultFont(FONT_DEFAULT, COLOR_TEXT);

    // Preload 3d files
    if (Settings.Enable3dRendering && !Preload3dFiles.empty()) {
        WriteLog("Preload 3d files...\n");
        for (const auto& name : Preload3dFiles) {
            SprMngr.Preload3dModel(name);
        }
        WriteLog("Preload 3d files complete.\n");
    }

    // Auto login
    ProcessAutoLogin();
}

FOClient::~FOClient()
{
    if (_netSock != INVALID_SOCKET) {
        ::closesocket(_netSock);
    }
    if (_zStream != nullptr) {
        ::inflateEnd(_zStream);
    }

    delete ScriptSys;
}

auto FOClient::ResolveCritterAnimation(hstring arg1, uint arg2, uint arg3, uint& arg4, uint& arg5, int& arg6, int& arg7, string& arg8) -> bool
{
    return OnCritterAnimation.Fire(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

auto FOClient::ResolveCritterAnimationSubstitute(hstring arg1, uint arg2, uint arg3, hstring& arg4, uint& arg5, uint& arg6) -> bool
{
    return OnCritterAnimationSubstitute.Fire(arg1, arg2, arg3, arg4, arg5, arg6);
}

auto FOClient::ResolveCritterAnimationFallout(hstring arg1, uint& arg2, uint& arg3, uint& arg4, uint& arg5, uint& arg6) -> bool
{
    return OnCritterAnimationFallout.Fire(arg1, arg2, arg3, arg4, arg5, arg6);
}

auto FOClient::GetChosen() -> CritterView*
{
    NON_CONST_METHOD_HINT();

    return _chosen;
}

void FOClient::ProcessAutoLogin()
{
    auto auto_login = Settings.AutoLogin;

#if FO_WEB
    char* auto_login_web = (char*)EM_ASM_INT({
        if ('foAutoLogin' in Module) {
            var len = lengthBytesUTF8(Module.foAutoLogin) + 1;
            var str = _malloc(len);
            stringToUTF8(Module.foAutoLogin, str, len + 1);
            return str;
        }
        return null;
    });

    if (auto_login_web) {
        auto_login = auto_login_web;
        free(auto_login_web);
        auto_login_web = nullptr;
    }

#else
    // Non-const hint
    auto_login = auto_login.substr(0);
#endif

    if (auto_login.empty()) {
        return;
    }

    const auto auto_login_args = _str(auto_login).split(' ');
    if (auto_login_args.size() != 2) {
        return;
    }

    _isAutoLogin = true;

#if !FO_SINGLEPLAYER
    if (OnAutoLogin.Fire(auto_login_args[0], auto_login_args[1]) && _initNetReason == INIT_NET_REASON_NONE) {
        _loginName = auto_login_args[0];
        _loginPassword = auto_login_args[1];
        _initNetReason = INIT_NET_REASON_LOGIN;
    }
#endif
}

void FOClient::UpdateFilesLoop()
{
    // Was aborted
    if (_updateData->Aborted) {
        return;
    }

    // Update indication
    if (_initCalls < 2) {
        // Load font
        if (!_updateData->FontLoaded) {
            SprMngr.PushAtlasType(AtlasType::Static);
            _updateData->FontLoaded = SprMngr.LoadFontFO(FONT_DEFAULT, "Default", false, true);
            if (!_updateData->FontLoaded) {
                _updateData->FontLoaded = SprMngr.LoadFontBmf(FONT_DEFAULT, "Default");
            }
            if (_updateData->FontLoaded) {
                SprMngr.BuildFonts();
            }
            SprMngr.PopAtlasType();
        }

        // Running dots
        string dots_str;
        const auto dots = (GameTime.FrameTick() - _updateData->Duration) / 100u % 50u + 1u;
        for ([[maybe_unused]] const auto i : xrange(dots)) {
            dots_str += ".";
        }

        // State
        SprMngr.BeginScene(COLOR_RGB(50, 50, 50));
        const auto update_text = _updateData->Messages + _updateData->Progress + dots_str;
        SprMngr.DrawStr(IRect(0, 0, Settings.ScreenWidth, Settings.ScreenHeight), update_text, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT);
        SprMngr.EndScene();
    }
    else {
        // Wait screen
        SprMngr.BeginScene(COLOR_RGB(0, 0, 0));
        DrawIface();
        SprMngr.EndScene();
    }

    // Logic
    if (!_isConnected) {
        if (_updateData->Connecting) {
            _updateData->Connecting = false;
            UpdateFilesAddText(STR_CANT_CONNECT_TO_SERVER, "Can't connect to server!");
        }

        if (GameTime.FrameTick() < _updateData->ConnectionTimeout) {
            return;
        }
        _updateData->ConnectionTimeout = GameTime.FrameTick() + 5000;

        // Connect to server
        UpdateFilesAddText(STR_CONNECT_TO_SERVER, "Connect to server...");
        NetConnect(Settings.ServerHost, static_cast<ushort>(Settings.ServerPort));
        _updateData->Connecting = true;
    }
    else {
        if (_updateData->Connecting) {
            _updateData->Connecting = false;
            UpdateFilesAddText(STR_CONNECTION_ESTABLISHED, "Connection established.");

            // Update
            UpdateFilesAddText(STR_CHECK_UPDATES, "Check updates...");
            _updateData->Messages = "";

            // Data synchronization
            UpdateFilesAddText(STR_DATA_SYNCHRONIZATION, "Data synchronization...");

            // Clean up
            _updateData->ClientOutdated = false;
            _updateData->CacheChanged = false;
            _updateData->FilesChanged = false;
            _updateData->FileListReceived = false;
            _updateData->FileList.clear();
            _updateData->FileDownloading = false;
            FileMngr.DeleteFile("Update.fobin");
            _updateData->Duration = GameTime.FrameTick();

            Net_SendUpdate();
        }

        _updateData->Progress = "";
        if (!_updateData->FileList.empty()) {
            _updateData->Progress += "\n";
            for (const auto& update_file : _updateData->FileList) {
                const auto cur = static_cast<float>(update_file.Size - update_file.RemaningSize) / (1024.0f * 1024.0f);
                const auto max = std::max(static_cast<float>(update_file.Size) / (1024.0f * 1024.0f), 0.01f);
                const string name = _str(update_file.Name).formatPath();
                _updateData->Progress += _str("{} {:.2f} / {:.2f} MB\n", name, cur, max);
            }
            _updateData->Progress += "\n";
        }

        if (_updateData->FileListReceived && !_updateData->FileDownloading) {
            if (!_updateData->FileList.empty()) {
                const auto& update_file = _updateData->FileList.front();

                if (_updateData->TempFile) {
                    _updateData->TempFile = nullptr;
                }

                if (update_file.Name[0] == '$') {
                    _updateData->TempFile.reset(new OutputFile {FileMngr.WriteFile("Update.fobin", false)});
                    _updateData->CacheChanged = true;
                }
                else {
                    // Web client can receive only cache updates
                    // Resources must be packed in main bundle
#if FO_WEB
                    UpdateFilesAddText(STR_CLIENT_OUTDATED, "Client outdated!");
                    NetDisconnect();
                    return;
#endif

                    FileMngr.DeleteFile(update_file.Name);
                    _updateData->TempFile.reset(new OutputFile {FileMngr.WriteFile("Update.fobin", false)});
                    _updateData->FilesChanged = true;
                }

                if (!_updateData->TempFile) {
                    UpdateFilesAddText(STR_FILESYSTEM_ERROR, "File system error!");
                    NetDisconnect();
                    return;
                }

                _updateData->FileDownloading = true;

                _netOut << NETMSG_GET_UPDATE_FILE;
                _netOut << _updateData->FileList.front().Index;
            }
            else {
                // Done
                _updateData.reset();

                // Disconnect
                if (_initCalls < 2) {
                    NetDisconnect();
                }

                // Update binaries
                if (_updateData->ClientOutdated) {
                    throw GenericException("Client outdated");
                }

                // Reinitialize data
                if (_initCalls >= 2 && (_updateData->CacheChanged || _updateData->FilesChanged)) {
                    throw ClientRestartException("Restart");
                }

                if (_updateData->CacheChanged) {
                    _curLang.LoadFromCache(Cache, *this, _curLang.Name);
                }

                // if (Update->FilesChanged)
                //    Settings.Init(0, {});

                // ScriptSys = new ClientScriptSystem(this, Settings);

                return;
            }
        }

        ProcessSocket();

        if (!_isConnected && !_updateData->Aborted) {
            UpdateFilesAddText(STR_CONNECTION_FAILTURE, "Connection failure!");
        }
    }
}

void FOClient::UpdateFilesAddText(uint num_str, string_view num_str_str)
{
    if (_updateData->FontLoaded) {
        const auto text = (_curLang.Msg[TEXTMSG_GAME].Count(num_str) != 0u ? _curLang.Msg[TEXTMSG_GAME].GetStr(num_str) : string(num_str_str));
        _updateData->Messages += _str("{}\n", text);
    }
}

void FOClient::UpdateFilesAbort(uint num_str, string_view num_str_str)
{
    _updateData->Aborted = true;

    UpdateFilesAddText(num_str, num_str_str);
    NetDisconnect();

    if (_updateData->TempFile) {
        _updateData->TempFile = nullptr;
    }

    SprMngr.BeginScene(COLOR_RGB(255, 0, 0));
    SprMngr.DrawStr(IRect(0, 0, Settings.ScreenWidth, Settings.ScreenHeight), _updateData->Messages, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.EndScene();
}

void FOClient::DeleteCritters()
{
    HexMngr.DeleteCritters();
    _chosen = nullptr;
}

void FOClient::AddCritter(CritterView* cr)
{
    uint fading = 0;
    auto* cr_ = GetCritter(cr->GetId());
    if (cr_ != nullptr) {
        fading = cr_->FadingTick;
    }

    DeleteCritter(cr->GetId());

    if (HexMngr.IsMapLoaded()) {
        auto& f = HexMngr.GetField(cr->GetHexX(), cr->GetHexY());
        if (f.Crit != nullptr && f.Crit->IsFinishing()) {
            DeleteCritter(f.Crit->GetId());
        }
    }

    if (cr->IsChosen()) {
        _chosen = cr;
    }

    HexMngr.AddCritter(cr);
    cr->FadingTick = GameTime.GameTick() + FADING_PERIOD - (fading > GameTime.GameTick() ? fading - GameTime.GameTick() : 0);
}

void FOClient::DeleteCritter(uint crid)
{
    if (_chosen != nullptr && _chosen->GetId() == crid) {
        _chosen = nullptr;
    }

    HexMngr.DeleteCritter(crid);
}

void FOClient::LookBordersPrepare()
{
    _lookBorders.clear();
    _shootBorders.clear();

    if (_chosen == nullptr || !HexMngr.IsMapLoaded() || (!_drawLookBorders && !_drawShootBorders)) {
        HexMngr.SetFog(_lookBorders, _shootBorders, nullptr, nullptr);
        return;
    }

    const auto dist = _chosen->GetLookDistance();
    const auto base_hx = _chosen->GetHexX();
    const auto base_hy = _chosen->GetHexY();
    int hx = base_hx;
    int hy = base_hy;
    const int chosen_dir = _chosen->GetDir();
    const auto dist_shoot = _chosen->GetAttackDist();
    const auto maxhx = HexMngr.GetWidth();
    const auto maxhy = HexMngr.GetHeight();
    auto seek_start = true;
    for (auto i = 0; i < (Settings.MapHexagonal ? 6 : 4); i++) {
        const auto dir = (Settings.MapHexagonal ? (i + 2) % 6 : ((i + 1) * 2) % 8);

        for (uint j = 0, jj = (Settings.MapHexagonal ? dist : dist * 2); j < jj; j++) {
            if (seek_start) {
                // Move to start position
                for (uint l = 0; l < dist; l++) {
                    GeomHelper.MoveHexByDirUnsafe(hx, hy, Settings.MapHexagonal ? 0 : 7);
                }
                seek_start = false;
                j = -1;
            }
            else {
                // Move to next hex
                GeomHelper.MoveHexByDirUnsafe(hx, hy, static_cast<uchar>(dir));
            }

            auto hx_ = static_cast<ushort>(std::clamp(hx, 0, maxhx - 1));
            auto hy_ = static_cast<ushort>(std::clamp(hy, 0, maxhy - 1));
            if (IsBitSet(Settings.LookChecks, LOOK_CHECK_DIR)) {
                const int dir_ = GeomHelper.GetFarDir(base_hx, base_hy, hx_, hy_);
                auto ii = (chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir);
                if (ii > static_cast<int>(Settings.MapDirCount / 2)) {
                    ii = Settings.MapDirCount - ii;
                }
                const auto dist_ = dist - dist * Settings.LookDir[ii] / 100;
                pair<ushort, ushort> block;
                HexMngr.TraceBullet(base_hx, base_hy, hx_, hy_, dist_, 0.0f, nullptr, false, nullptr, CritterFindType::Any, nullptr, &block, nullptr, false);
                hx_ = block.first;
                hy_ = block.second;
            }

            if (IsBitSet(Settings.LookChecks, LOOK_CHECK_TRACE)) {
                pair<ushort, ushort> block;
                HexMngr.TraceBullet(base_hx, base_hy, hx_, hy_, 0, 0.0f, nullptr, false, nullptr, CritterFindType::Any, nullptr, &block, nullptr, true);
                hx_ = block.first;
                hy_ = block.second;
            }

            auto dist_look = GeomHelper.DistGame(base_hx, base_hy, hx_, hy_);
            if (_drawLookBorders) {
                auto x = 0;
                auto y = 0;
                HexMngr.GetHexCurrentPosition(hx_, hy_, x, y);
                auto* ox = (dist_look == dist ? &_chosen->SprOx : nullptr);
                auto* oy = (dist_look == dist ? &_chosen->SprOy : nullptr);
                _lookBorders.push_back({x + (Settings.MapHexWidth / 2), y + (Settings.MapHexHeight / 2), COLOR_RGBA(0, 255, dist_look * 255 / dist, 0), ox, oy});
            }

            if (_drawShootBorders) {
                pair<ushort, ushort> block;
                const auto max_shoot_dist = std::max(std::min(dist_look, dist_shoot), 0u) + 1u;
                HexMngr.TraceBullet(base_hx, base_hy, hx_, hy_, max_shoot_dist, 0.0f, nullptr, false, nullptr, CritterFindType::Any, nullptr, &block, nullptr, true);
                const auto hx_2 = block.first;
                const auto hy_2 = block.second;

                auto x_ = 0;
                auto y_ = 0;
                HexMngr.GetHexCurrentPosition(hx_2, hy_2, x_, y_);
                const auto result_shoot_dist = GeomHelper.DistGame(base_hx, base_hy, hx_2, hy_2);
                auto* ox = (result_shoot_dist == max_shoot_dist ? &_chosen->SprOx : nullptr);
                auto* oy = (result_shoot_dist == max_shoot_dist ? &_chosen->SprOy : nullptr);
                _shootBorders.push_back({x_ + (Settings.MapHexWidth / 2), y_ + (Settings.MapHexHeight / 2), COLOR_RGBA(255, 255, result_shoot_dist * 255 / max_shoot_dist, 0), ox, oy});
            }
        }
    }

    auto base_x = 0;
    auto base_y = 0;
    HexMngr.GetHexCurrentPosition(base_hx, base_hy, base_x, base_y);
    if (!_lookBorders.empty()) {
        _lookBorders.push_back(*_lookBorders.begin());
        _lookBorders.insert(_lookBorders.begin(), {base_x + (Settings.MapHexWidth / 2), base_y + (Settings.MapHexHeight / 2), COLOR_RGBA(0, 0, 0, 0), &_chosen->SprOx, &_chosen->SprOy});
    }
    if (!_shootBorders.empty()) {
        _shootBorders.push_back(*_shootBorders.begin());
        _shootBorders.insert(_shootBorders.begin(), {base_x + (Settings.MapHexWidth / 2), base_y + (Settings.MapHexHeight / 2), COLOR_RGBA(255, 0, 0, 0), &_chosen->SprOx, &_chosen->SprOy});
    }

    HexMngr.SetFog(_lookBorders, _shootBorders, &_chosen->SprOx, &_chosen->SprOy);
}

void FOClient::MainLoop()
{
    const auto time_changed = GameTime.FrameAdvance();

    // FPS counter
    if (GameTime.FrameTick() - _fpsTick >= 1000) {
        Settings.FPS = _fpsCounter;
        _fpsCounter = 0;
        _fpsTick = GameTime.FrameTick();
    }
    else {
        _fpsCounter++;
    }

    // Poll pending events
    if (_initCalls < 2) {
        InputEvent event;
        while (App->Input.PollEvent(event)) {
        }
    }

    // Game end
    if (Settings.Quit) {
        return;
    }

    // Network connection
    if (_isConnecting) {
        if (!CheckSocketStatus(true)) {
            return;
        }
    }

    if (_updateData) {
        UpdateFilesLoop();
        return;
    }

    if (_initCalls < 2) {
        if (_initCalls == 0) {
            _initCalls = 1;
        }

        _initCalls++;

        if (_initCalls == 1) {
            _updateData = ClientUpdate();
        }
        else if (_initCalls == 2) {
            ScreenFadeOut();
        }

        return;
    }

    // Input events
    InputEvent event;
    while (App->Input.PollEvent(event)) {
        if (event.Type == InputEvent::EventType::MouseMoveEvent) {
            const auto [w, h] = SprMngr.GetWindowSize();
            const auto x = static_cast<int>(static_cast<float>(event.MouseMove.MouseX) / static_cast<float>(w) * static_cast<float>(Settings.ScreenWidth));
            const auto y = static_cast<int>(static_cast<float>(event.MouseMove.MouseY) / static_cast<float>(h) * static_cast<float>(Settings.ScreenHeight));
            Settings.MouseX = std::clamp(x, 0, Settings.ScreenWidth - 1);
            Settings.MouseY = std::clamp(y, 0, Settings.ScreenHeight - 1);
        }
    }

    // Network
    if (_initNetReason != INIT_NET_REASON_NONE && !_updateData) {
        // Connect to server
        if (!_isConnected) {
            if (!NetConnect(Settings.ServerHost, static_cast<ushort>(Settings.ServerPort))) {
                ShowMainScreen(SCREEN_LOGIN, {});
                AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_FAIL));
            }
        }
        else {
            // Reason
            const auto reason = _initNetReason;
            _initNetReason = INIT_NET_REASON_NONE;

            // After connect things
            if (reason == INIT_NET_REASON_LOGIN) {
                Net_SendLogIn();
            }
            else if (reason == INIT_NET_REASON_REG) {
                Net_SendCreatePlayer();
            }
            else if (reason == INIT_NET_REASON_LOAD) {
                // Net_SendSaveLoad( false, SaveLoadFileName.c_str(), nullptr );
            }
            else if (reason != INIT_NET_REASON_CUSTOM) {
                throw UnreachablePlaceException(LINE_STR);
            }
        }
    }

    // Parse Net
    if (_isConnected) {
        ProcessSocket();
    }

    // Exit in Login screen if net disconnect
    if (!_isConnected && !IsMainScreen(SCREEN_LOGIN)) {
        ShowMainScreen(SCREEN_LOGIN, {});
    }

    // Input
    ProcessInputEvents();

    // Process
    AnimProcess();

    // Game time
    if (time_changed) {
        const auto st = GameTime.GetGameTime(GameTime.GetFullSecond());

        SetYear(st.Year);
        SetMonth(st.Month);
        SetDay(st.Day);
        SetHour(st.Hour);
        SetMinute(st.Minute);
        SetSecond(st.Second);

        SetDayTime(false);
    }

    if (IsMainScreen(SCREEN_GLOBAL_MAP)) {
        CrittersProcess();
    }
    else if (IsMainScreen(SCREEN_GAME) && HexMngr.IsMapLoaded()) {
        if (HexMngr.Scroll()) {
            LookBordersPrepare();
        }
        CrittersProcess();
        HexMngr.ProcessItems();
        HexMngr.ProcessRain();
    }

    // Start render
    SprMngr.BeginScene(COLOR_RGB(0, 0, 0));

#if !FO_SINGLEPLAYER
    // Script loop
    OnLoop.Fire();
#endif

    // Quake effect
    ProcessScreenEffectQuake();

    // Render
    if (GetMainScreen() == SCREEN_GAME && HexMngr.IsMapLoaded()) {
        GameDraw();
    }

    DrawIface();

    ProcessScreenEffectFading();

    SprMngr.EndScene();
}

void FOClient::DrawIface()
{
    // Make dirty offscreen surfaces
    if (!PreDirtyOffscreenSurfaces.empty()) {
        DirtyOffscreenSurfaces.insert(DirtyOffscreenSurfaces.end(), PreDirtyOffscreenSurfaces.begin(), PreDirtyOffscreenSurfaces.end());
        PreDirtyOffscreenSurfaces.clear();
    }

    CanDrawInScripts = true;
    OnRenderIface.Fire();
    CanDrawInScripts = false;
}

void FOClient::ScreenFade(uint time, uint from_color, uint to_color, bool push_back)
{
    if (!push_back || _screenEffects.empty()) {
        _screenEffects.push_back({GameTime.FrameTick(), time, from_color, to_color});
    }
    else {
        uint last_tick = 0;
        for (auto& e : _screenEffects) {
            if (e.BeginTick + e.Time > last_tick) {
                last_tick = e.BeginTick + e.Time;
            }
        }
        _screenEffects.push_back({last_tick, time, from_color, to_color});
    }
}

void FOClient::ScreenQuake(int noise, uint time)
{
    Settings.ScrOx -= _screenOffsX;
    Settings.ScrOy -= _screenOffsY;
    _screenOffsX = (GenericUtils::Random(0, 1) != 0 ? noise : -noise);
    _screenOffsY = (GenericUtils::Random(0, 1) != 0 ? noise : -noise);
    _screenOffsXf = static_cast<float>(_screenOffsX);
    _screenOffsYf = static_cast<float>(_screenOffsY);
    _screenOffsStep = std::fabs(_screenOffsXf) / (static_cast<float>(time) / 30.0f);
    Settings.ScrOx += _screenOffsX;
    Settings.ScrOy += _screenOffsY;
    _screenOffsNextTick = GameTime.GameTick() + 30u;
}

void FOClient::ProcessScreenEffectFading()
{
    SprMngr.Flush();

    PrimitivePoints full_screen_quad;
    SprMngr.PrepareSquare(full_screen_quad, IRect(0, 0, Settings.ScreenWidth, Settings.ScreenHeight), 0);

    for (auto it = _screenEffects.begin(); it != _screenEffects.end();) {
        auto& screen_effect = *it;

        if (GameTime.FrameTick() >= screen_effect.BeginTick + screen_effect.Time) {
            it = _screenEffects.erase(it);
            continue;
        }

        if (GameTime.FrameTick() >= screen_effect.BeginTick) {
            const auto proc = GenericUtils::Percent(screen_effect.Time, GameTime.FrameTick() - screen_effect.BeginTick) + 1u;
            int res[4];

            for (auto i = 0; i < 4; i++) {
                const int sc = (reinterpret_cast<uchar*>(&screen_effect.StartColor))[i];
                const int ec = (reinterpret_cast<uchar*>(&screen_effect.EndColor))[i];
                const auto dc = ec - sc;
                res[i] = sc + dc * static_cast<int>(proc) / 100;
            }

            const auto color = COLOR_RGBA(res[3], res[2], res[1], res[0]);
            for (auto i = 0; i < 6; i++) {
                full_screen_quad[i].PointColor = color;
            }

            SprMngr.DrawPoints(full_screen_quad, RenderPrimitiveType::TriangleList, nullptr, nullptr, nullptr);
        }

        ++it;
    }
}

void FOClient::ProcessScreenEffectQuake()
{
    if ((_screenOffsX != 0 || _screenOffsY != 0) && GameTime.GameTick() >= _screenOffsNextTick) {
        Settings.ScrOx -= _screenOffsX;
        Settings.ScrOy -= _screenOffsY;

        if (_screenOffsXf < 0.0f) {
            _screenOffsXf += _screenOffsStep;
        }
        else if (_screenOffsXf > 0.0f) {
            _screenOffsXf -= _screenOffsStep;
        }

        if (_screenOffsYf < 0.0f) {
            _screenOffsYf += _screenOffsStep;
        }
        else if (_screenOffsYf > 0.0f) {
            _screenOffsYf -= _screenOffsStep;
        }

        _screenOffsXf = -_screenOffsXf;
        _screenOffsYf = -_screenOffsYf;
        _screenOffsX = static_cast<int>(_screenOffsXf);
        _screenOffsY = static_cast<int>(_screenOffsYf);

        Settings.ScrOx += _screenOffsX;
        Settings.ScrOy += _screenOffsY;

        _screenOffsNextTick = GameTime.GameTick() + 30;
    }
}

void FOClient::ProcessInputEvents()
{
    // Stop processing if window not active
    if (!SprMngr.IsWindowFocused()) {
        InputEvent event;
        while (App->Input.PollEvent(event)) {
            // Nop
        }

        Keyb.Lost();
        OnInputLost.Fire();
        return;
    }

    InputEvent event;
    while (App->Input.PollEvent(event)) {
        ProcessInputEvent(event);
    }
}

void FOClient::ProcessInputEvent(const InputEvent& event)
{
    if (event.Type == InputEvent::EventType::KeyDownEvent) {
        const auto key_code = event.KeyDown.Code;
        const auto key_text = event.KeyDown.Text;

        if (key_code == KeyCode::Rcontrol || key_code == KeyCode::Lcontrol) {
            Keyb.CtrlDwn = true;
        }
        else if (key_code == KeyCode::Lmenu || key_code == KeyCode::Rmenu) {
            Keyb.AltDwn = true;
        }
        else if (key_code == KeyCode::Lshift || key_code == KeyCode::Rshift) {
            Keyb.ShiftDwn = true;
        }

        OnKeyDown.Fire(key_code, key_text);
    }
    else if (event.Type == InputEvent::EventType::KeyUpEvent) {
        const auto key_code = event.KeyUp.Code;

        if (key_code == KeyCode::Rcontrol || key_code == KeyCode::Lcontrol) {
            Keyb.CtrlDwn = false;
        }
        else if (key_code == KeyCode::Lmenu || key_code == KeyCode::Rmenu) {
            Keyb.AltDwn = false;
        }
        else if (key_code == KeyCode::Lshift || key_code == KeyCode::Rshift) {
            Keyb.ShiftDwn = false;
        }

        OnKeyUp.Fire(key_code);
    }
    else if (event.Type == InputEvent::EventType::MouseMoveEvent) {
        const auto mouse_x = event.MouseMove.MouseX;
        const auto mouse_y = event.MouseMove.MouseY;
        const auto delta_x = event.MouseMove.DeltaX;
        const auto delta_y = event.MouseMove.DeltaY;

        Settings.MouseX = mouse_x;
        Settings.MouseY = mouse_y;

        OnMouseMove.Fire(delta_x, delta_y);
    }
    else if (event.Type == InputEvent::EventType::MouseDownEvent) {
        const auto mouse_button = event.MouseDown.Button;

        OnMouseDown.Fire(mouse_button);
    }
    else if (event.Type == InputEvent::EventType::MouseUpEvent) {
        const auto mouse_button = event.MouseUp.Button;

        OnMouseUp.Fire(mouse_button);
    }
    else if (event.Type == InputEvent::EventType::MouseWheelEvent) {
        const auto wheel_delta = event.MouseWheel.Delta;

        // Todo: handle mouse wheel
        UNUSED_VARIABLE(wheel_delta);
    }
}

static auto GetLastSocketError() -> string
{
#if FO_WINDOWS
    const auto error_code = ::WSAGetLastError();
    wchar_t* ws = nullptr;
    ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&ws), 0, nullptr);
    const string error_str = _str().parseWideChar(ws);
    ::LocalFree(ws);

    return _str("{} ({})", error_str, error_code);
#else
    return _str("{} ({})", strerror(errno), errno);
#endif
}

auto FOClient::CheckSocketStatus(bool for_write) -> bool
{
    timeval tv {0, 0};

    FD_ZERO(&_netSockSet);
    FD_SET(_netSock, &_netSockSet);

    const auto r = ::select(static_cast<int>(_netSock) + 1, for_write ? nullptr : &_netSockSet, for_write ? &_netSockSet : nullptr, nullptr, &tv);
    if (r == 1) {
        if (_isConnecting) {
            WriteLog("Connection established.\n");
            _isConnecting = false;
            _isConnected = true;
        }
        return true;
    }

    if (r == 0) {
        auto error = 0;
#if FO_WINDOWS
        int len = sizeof(error);
#else
        socklen_t len = sizeof(error);
#endif
        if (::getsockopt(_netSock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) != SOCKET_ERROR && error == 0) {
            return false;
        }

        WriteLog("Socket error '{}'.\n", GetLastSocketError());
    }
    else {
        // Error
        WriteLog("Socket select error '{}'.\n", GetLastSocketError());
    }

    if (_isConnecting) {
        WriteLog("Can't connect to server.\n");
        _isConnecting = false;
    }

    NetDisconnect();
    return false;
}

auto FOClient::NetConnect(string_view host, ushort port) -> bool
{
#if FO_WEB
    port++;
    if (!Settings.SecuredWebSockets) {
        EM_ASM(Module['websocket']['url'] = 'ws://');
        WriteLog("Connecting to server 'ws://{}:{}'.\n", host, port);
    }
    else {
        EM_ASM(Module['websocket']['url'] = 'wss://');
        WriteLog("Connecting to server 'wss://{}:{}'.\n", host, port);
    }
#else
    WriteLog("Connecting to server '{}:{}'.\n", host, port);
#endif

    _isConnecting = false;
    _isConnected = false;

    if (_zStream == nullptr) {
        _zStream = new z_stream();
        _zStream->zalloc = [](voidpf, uInt items, uInt size) { return calloc(items, size); };
        _zStream->zfree = [](voidpf, voidpf address) { free(address); };
        _zStream->opaque = nullptr;
        const auto inf_init = ::inflateInit(_zStream);
        RUNTIME_ASSERT(inf_init == Z_OK);
    }

#if FO_WINDOWS
    WSADATA wsa;
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        WriteLog("WSAStartup error '{}'.\n", GetLastSocketError());
        return false;
    }
#endif

    if (!FillSockAddr(_sockAddr, host, port)) {
        return false;
    }
    if (Settings.ProxyType != 0u && !FillSockAddr(_proxyAddr, Settings.ProxyHost, static_cast<ushort>(Settings.ProxyPort))) {
        return false;
    }

    _netIn.SetEncryptKey(0);
    _netOut.SetEncryptKey(0);

#if FO_LINUX
    const auto sock_type = SOCK_STREAM | SOCK_CLOEXEC;
#else
    const auto sock_type = SOCK_STREAM;
#endif
    if ((_netSock = ::socket(PF_INET, sock_type, IPPROTO_TCP)) == INVALID_SOCKET) {
        WriteLog("Create socket error '{}'.\n", GetLastSocketError());
        return false;
    }

    // Nagle
#if !FO_WEB
    if (Settings.DisableTcpNagle) {
        auto optval = 1;
        if (::setsockopt(_netSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval), sizeof(optval)) != 0) {
            WriteLog("Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'.\n", GetLastSocketError());
        }
    }
#endif

    // Direct connect
    if (Settings.ProxyType == 0u) {
        // Set non blocking mode
#if FO_WINDOWS
        unsigned long mode = 1;
        if (::ioctlsocket(_netSock, FIONBIO, &mode) != 0)
#else
        int flags = fcntl(_netSock, F_GETFL, 0);
        RUNTIME_ASSERT(flags >= 0);
        if (::fcntl(_netSock, F_SETFL, flags | O_NONBLOCK))
#endif
        {
            WriteLog("Can't set non-blocking mode to socket, error '{}'.\n", GetLastSocketError());
            return false;
        }

        const auto r = ::connect(_netSock, reinterpret_cast<sockaddr*>(&_sockAddr), sizeof(sockaddr_in));
#if FO_WINDOWS
        if (r == SOCKET_ERROR && ::WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (r == SOCKET_ERROR && errno != EINPROGRESS)
#endif
        {
            WriteLog("Can't connect to game server, error '{}'.\n", GetLastSocketError());
            return false;
        }
    }
    else {
#if !FO_IOS && !FO_ANDROID && !FO_WEB
        // Proxy connect
        if (::connect(_netSock, reinterpret_cast<sockaddr*>(&_proxyAddr), sizeof(sockaddr_in)) != 0) {
            WriteLog("Can't connect to proxy server, error '{}'.\n", GetLastSocketError());
            return false;
        }

        auto send_recv = [this]() {
            if (!NetOutput()) {
                WriteLog("Net output error.\n");
                return false;
            }

            const auto tick = GameTime.FrameTick();
            while (true) {
                const auto receive = NetInput(false);
                if (receive > 0) {
                    break;
                }

                if (receive < 0) {
                    WriteLog("Net input error.\n");
                    return false;
                }

                if (GameTime.FrameTick() - tick > 10000) {
                    WriteLog("Proxy answer timeout.\n");
                    return false;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return true;
        };

        uchar b1 = 0;
        uchar b2 = 0;
        _netIn.ResetBuf();
        _netOut.ResetBuf();

        // Authentication
        if (Settings.ProxyType == PROXY_SOCKS4) {
            // Connect
            _netOut << static_cast<uchar>(4); // Socks version
            _netOut << static_cast<uchar>(1); // Connect command
            _netOut << static_cast<ushort>(_sockAddr.sin_port);
            _netOut << static_cast<uint>(_sockAddr.sin_addr.s_addr);
            _netOut << static_cast<uchar>(0);

            if (!send_recv()) {
                return false;
            }

            _netIn >> b1; // Null byte
            _netIn >> b2; // Answer code
            if (b2 != 0x5A) {
                switch (b2) {
                case 0x5B:
                    WriteLog("Proxy connection error, request rejected or failed.\n");
                    break;
                case 0x5C:
                    WriteLog("Proxy connection error, request failed because client is not running identd (or not reachable from the server).\n");
                    break;
                case 0x5D:
                    WriteLog("Proxy connection error, request failed because client's identd could not confirm the user ID string in the request.\n");
                    break;
                default:
                    WriteLog("Proxy connection error, Unknown error {}.\n", b2);
                    break;
                }
                return false;
            }
        }
        else if (Settings.ProxyType == PROXY_SOCKS5) {
            _netOut << static_cast<uchar>(5); // Socks version
            _netOut << static_cast<uchar>(1); // Count methods
            _netOut << static_cast<uchar>(2); // Method

            if (!send_recv()) {
                return false;
            }

            _netIn >> b1; // Socks version
            _netIn >> b2; // Method
            if (b2 == 2) // User/Password
            {
                _netOut << static_cast<uchar>(1); // Subnegotiation version
                _netOut << static_cast<uchar>(Settings.ProxyUser.length()); // Name length
                _netOut.Push(Settings.ProxyUser.c_str(), static_cast<uint>(Settings.ProxyUser.length())); // Name
                _netOut << static_cast<uchar>(Settings.ProxyPass.length()); // Pass length
                _netOut.Push(Settings.ProxyPass.c_str(), static_cast<uint>(Settings.ProxyPass.length())); // Pass

                if (!send_recv()) {
                    return false;
                }

                _netIn >> b1; // Subnegotiation version
                _netIn >> b2; // Status
                if (b2 != 0) {
                    WriteLog("Invalid proxy user or password.\n");
                    return false;
                }
            }
            else if (b2 != 0) // Other authorization
            {
                WriteLog("Socks server connect fail.\n");
                return false;
            }

            // Connect
            _netOut << static_cast<uchar>(5); // Socks version
            _netOut << static_cast<uchar>(1); // Connect command
            _netOut << static_cast<uchar>(0); // Reserved
            _netOut << static_cast<uchar>(1); // IP v4 address
            _netOut << static_cast<uint>(_sockAddr.sin_addr.s_addr);
            _netOut << static_cast<ushort>(_sockAddr.sin_port);

            if (!send_recv()) {
                return false;
            }

            _netIn >> b1; // Socks version
            _netIn >> b2; // Answer code

            if (b2 != 0) {
                switch (b2) {
                case 1:
                    WriteLog("Proxy connection error, SOCKS-server error.\n");
                    break;
                case 2:
                    WriteLog("Proxy connection error, connections fail by proxy rules.\n");
                    break;
                case 3:
                    WriteLog("Proxy connection error, network is not aviable.\n");
                    break;
                case 4:
                    WriteLog("Proxy connection error, host is not aviable.\n");
                    break;
                case 5:
                    WriteLog("Proxy connection error, connection denied.\n");
                    break;
                case 6:
                    WriteLog("Proxy connection error, TTL expired.\n");
                    break;
                case 7:
                    WriteLog("Proxy connection error, command not supported.\n");
                    break;
                case 8:
                    WriteLog("Proxy connection error, address type not supported.\n");
                    break;
                default:
                    WriteLog("Proxy connection error, unknown error {}.\n", b2);
                    break;
                }
                return false;
            }
        }
        else if (Settings.ProxyType == PROXY_HTTP) {
            string buf = _str("CONNECT {}:{} HTTP/1.0\r\n\r\n", inet_ntoa(_sockAddr.sin_addr), port);
            _netOut.Push(buf.c_str(), static_cast<uint>(buf.length()));

            if (!send_recv()) {
                return false;
            }

            buf = reinterpret_cast<const char*>(_netIn.GetData() + _netIn.GetReadPos());
            if (buf.find(" 200 ") == string::npos) {
                WriteLog("Proxy connection error, receive message '{}'.\n", buf);
                return false;
            }
        }
        else {
            WriteLog("Unknown proxy type {}.\n", Settings.ProxyType);
            return false;
        }

        _netIn.ResetBuf();
        _netOut.ResetBuf();

        _isConnected = true;

#else
        throw GenericException("Proxy connection is not supported on this platform");
#endif
    }

    _isConnecting = true;
    return true;
}

auto FOClient::FillSockAddr(sockaddr_in& saddr, string_view host, ushort port) -> bool
{
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    if ((saddr.sin_addr.s_addr = ::inet_addr(string(host).c_str())) == static_cast<uint>(-1)) {
        const auto* h = ::gethostbyname(string(host).c_str());
        if (h == nullptr) {
            WriteLog("Can't resolve remote host '{}', error '{}'.", host, GetLastSocketError());
            return false;
        }

        std::memcpy(&saddr.sin_addr, h->h_addr, sizeof(in_addr));
    }
    return true;
}

void FOClient::NetDisconnect()
{
    _isConnecting = false;

    if (_netSock != INVALID_SOCKET) {
        ::closesocket(_netSock);
        _netSock = INVALID_SOCKET;
    }

    if (_zStream != nullptr) {
        ::inflateEnd(_zStream);
        _zStream = nullptr;
    }

    if (_isConnected) {
        _isConnected = false;

        WriteLog("Disconnect. Session traffic: send {}, receive {}, whole {}, receive real {}.\n", _bytesSend, _bytesReceive, _bytesReceive + _bytesSend, _bytesRealReceive);

        HexMngr.UnloadMap();
        DeleteCritters();
        _netIn.ResetBuf();
        _netOut.ResetBuf();
        _netIn.SetEncryptKey(0);
        _netOut.SetEncryptKey(0);
        _netIn.SetError(false);
        _netOut.SetError(false);

        if (_curPlayer != nullptr) {
            _curPlayer->MarkAsDestroyed();
            _curPlayer->Release();
            _curPlayer = nullptr;
        }

        ProcessAutoLogin();
    }
}

void FOClient::ProcessSocket()
{
    if (_netSock == INVALID_SOCKET) {
        return;
    }

    if (NetInput(true) >= 0) {
        NetProcess();

        if (_isConnected && Settings.HelpInfo && _netOut.IsEmpty() && _pingTick == 0u && Settings.PingPeriod != 0u && GameTime.FrameTick() >= _pingCallTick) {
            Net_SendPing(PING_PING);
            _pingTick = GameTime.FrameTick();
        }

        NetOutput();
    }
    else {
        NetDisconnect();
    }
}

auto FOClient::NetOutput() -> bool
{
    if (!_isConnected) {
        return false;
    }
    if (_netOut.IsEmpty()) {
        return true;
    }
    if (!CheckSocketStatus(true)) {
        return _isConnected;
    }

    const int tosend = _netOut.GetEndPos();
    auto sendpos = 0;
    while (sendpos < tosend) {
#if FO_WINDOWS
        DWORD len = 0;
        WSABUF buf;
        buf.buf = reinterpret_cast<char*>(_netOut.GetData()) + sendpos;
        buf.len = tosend - sendpos;
        if (::WSASend(_netSock, &buf, 1, &len, 0, nullptr, nullptr) == SOCKET_ERROR || len == 0)
#else
        int len = (int)::send(_netSock, _netOut.GetData() + sendpos, tosend - sendpos, 0);
        if (len <= 0)
#endif
        {
            WriteLog("Socket error while send to server, error '{}'.\n", GetLastSocketError());
            NetDisconnect();
            return false;
        }

        sendpos += len;
        _bytesSend += len;
    }

    _netOut.ResetBuf();
    return true;
}

auto FOClient::NetInput(bool unpack) -> int
{
    if (!CheckSocketStatus(false)) {
        return 0;
    }

#if FO_WINDOWS
    DWORD len = 0;
    DWORD flags = 0;
    WSABUF buf;
    buf.buf = reinterpret_cast<char*>(_incomeBuf.data());
    buf.len = static_cast<uint>(_incomeBuf.size());
    if (::WSARecv(_netSock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
    int len = static_cast<int>(::recv(_netSock, _incomeBuf.data(), _incomeBuf.size(), 0));
    if (len == SOCKET_ERROR)
#endif
    {
        WriteLog("Socket error while receive from server, error '{}'.\n", GetLastSocketError());
        return -1;
    }
    if (len == 0) {
        WriteLog("Socket is closed.\n");
        return -2;
    }

    auto whole_len = static_cast<uint>(len);
    while (whole_len == static_cast<uint>(_incomeBuf.size())) {
        _incomeBuf.resize(_incomeBuf.size() * 2);

#if FO_WINDOWS
        flags = 0;
        buf.buf = reinterpret_cast<char*>(_incomeBuf.data()) + whole_len;
        buf.len = static_cast<uint>(_incomeBuf.size()) - whole_len;
        if (::WSARecv(_netSock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
        len = static_cast<int>(::recv(_netSock, _incomeBuf.data() + whole_len, _incomeBuf.size() - whole_len, 0));
        if (len == SOCKET_ERROR)
#endif
        {
#if FO_WINDOWS
            if (::WSAGetLastError() == WSAEWOULDBLOCK) {
#else
            if (errno == EINPROGRESS) {
#endif
                break;
            }

            WriteLog("Socket error (2) while receive from server, error '{}'.\n", GetLastSocketError());
            return -1;
        }
        if (len == 0) {
            WriteLog("Socket is closed (2).\n");
            return -2;
        }

        whole_len += len;
    }

    _netIn.ShrinkReadBuf();

    const auto old_pos = _netIn.GetEndPos();

    if (unpack && !Settings.DisableZlibCompression) {
        _zStream->next_in = _incomeBuf.data();
        _zStream->avail_in = whole_len;
        _zStream->next_out = _netIn.GetData() + _netIn.GetEndPos();
        _zStream->avail_out = _netIn.GetAvailLen();

        const auto first_inflate = ::inflate(_zStream, Z_SYNC_FLUSH);
        RUNTIME_ASSERT(first_inflate == Z_OK);

        auto uncompr = static_cast<uint>(reinterpret_cast<size_t>(_zStream->next_out) - reinterpret_cast<size_t>(_netIn.GetData()));
        _netIn.SetEndPos(uncompr);

        while (_zStream->avail_in != 0u) {
            _netIn.GrowBuf(NetBuffer::DEFAULT_BUF_SIZE);

            _zStream->next_out = _netIn.GetData() + _netIn.GetEndPos();
            _zStream->avail_out = _netIn.GetAvailLen();

            const auto next_inflate = ::inflate(_zStream, Z_SYNC_FLUSH);
            RUNTIME_ASSERT(next_inflate == Z_OK);

            uncompr = static_cast<uint>(reinterpret_cast<size_t>(_zStream->next_out) - reinterpret_cast<size_t>(_netIn.GetData()));
            _netIn.SetEndPos(uncompr);
        }
    }
    else {
        _netIn.AddData(_incomeBuf.data(), whole_len);
    }

    _bytesReceive += whole_len;
    _bytesRealReceive += _netIn.GetEndPos() - old_pos;
    return static_cast<int>(_netIn.GetEndPos() - old_pos);
}

void FOClient::NetProcess()
{
    while (_isConnected && _netIn.NeedProcess()) {
        uint msg = 0;
        _netIn >> msg;

        CHECK_IN_BUFF_ERROR();

        if (Settings.DebugNet) {
            static uint count = 0;
            AddMess(SAY_NETMSG, _str("{:04}) Input net message {}.", count, (msg >> 8) & 0xFF));
            WriteLog("{}) Input net message {}.\n", count, (msg >> 8) & 0xFF);
            count++;
        }

        switch (msg) {
        case NETMSG_DISCONNECT:
            NetDisconnect();
            break;
        case NETMSG_WRONG_NET_PROTO:
            Net_OnWrongNetProto();
            break;
        case NETMSG_LOGIN_SUCCESS:
            Net_OnLoginSuccess();
            break;
        case NETMSG_REGISTER_SUCCESS:
            WriteLog("Registration success.\n");
            break;

        case NETMSG_PING:
            Net_OnPing();
            break;
        case NETMSG_END_PARSE_TO_GAME:
            Net_OnEndParseToGame();
            break;

        case NETMSG_ADD_PLAYER:
            Net_OnAddCritter(false);
            break;
        case NETMSG_ADD_NPC:
            Net_OnAddCritter(true);
            break;
        case NETMSG_REMOVE_CRITTER:
            Net_OnRemoveCritter();
            break;
        case NETMSG_SOME_ITEM:
            Net_OnSomeItem();
            break;
        case NETMSG_CRITTER_ACTION:
            Net_OnCritterAction();
            break;
        case NETMSG_CRITTER_MOVE_ITEM:
            Net_OnCritterMoveItem();
            break;
        case NETMSG_CRITTER_ANIMATE:
            Net_OnCritterAnimate();
            break;
        case NETMSG_CRITTER_SET_ANIMS:
            Net_OnCritterSetAnims();
            break;
        case NETMSG_CUSTOM_COMMAND:
            Net_OnCustomCommand();
            break;
        case NETMSG_CRITTER_MOVE:
            Net_OnCritterMove();
            break;
        case NETMSG_CRITTER_DIR:
            Net_OnCritterDir();
            break;
        case NETMSG_CRITTER_XY:
            Net_OnCritterXY();
            break;

        case NETMSG_POD_PROPERTY(1, 0):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(1, 1):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(1, 2):
            Net_OnProperty(1);
            break;
        case NETMSG_POD_PROPERTY(2, 0):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(2, 1):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(2, 2):
            Net_OnProperty(2);
            break;
        case NETMSG_POD_PROPERTY(4, 0):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(4, 1):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(4, 2):
            Net_OnProperty(4);
            break;
        case NETMSG_POD_PROPERTY(8, 0):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(8, 1):
            [[fallthrough]];
        case NETMSG_POD_PROPERTY(8, 2):
            Net_OnProperty(8);
            break;
        case NETMSG_COMPLEX_PROPERTY:
            Net_OnProperty(0);
            break;

        case NETMSG_CRITTER_TEXT:
            Net_OnText();
            break;
        case NETMSG_MSG:
            Net_OnTextMsg(false);
            break;
        case NETMSG_MSG_LEX:
            Net_OnTextMsg(true);
            break;
        case NETMSG_MAP_TEXT:
            Net_OnMapText();
            break;
        case NETMSG_MAP_TEXT_MSG:
            Net_OnMapTextMsg();
            break;
        case NETMSG_MAP_TEXT_MSG_LEX:
            Net_OnMapTextMsgLex();
            break;

        case NETMSG_ALL_PROPERTIES:
            Net_OnAllProperties();
            break;

        case NETMSG_CLEAR_ITEMS:
            Net_OnChosenClearItems();
            break;
        case NETMSG_ADD_ITEM:
            Net_OnChosenAddItem();
            break;
        case NETMSG_REMOVE_ITEM:
            Net_OnChosenEraseItem();
            break;
        case NETMSG_ALL_ITEMS_SEND:
            Net_OnAllItemsSend();
            break;

        case NETMSG_TALK_NPC:
            Net_OnChosenTalk();
            break;

        case NETMSG_GAME_INFO:
            Net_OnGameInfo();
            break;

        case NETMSG_AUTOMAPS_INFO:
            Net_OnAutomapsInfo();
            break;
        case NETMSG_VIEW_MAP:
            Net_OnViewMap();
            break;

        case NETMSG_LOADMAP:
            Net_OnLoadMap();
            break;
        case NETMSG_MAP:
            Net_OnMap();
            break;
        case NETMSG_GLOBAL_INFO:
            Net_OnGlobalInfo();
            break;
        case NETMSG_SOME_ITEMS:
            Net_OnSomeItems();
            break;
        case NETMSG_RPC:
            // ScriptSys.HandleRpc(&Bin);
            break;

        case NETMSG_ADD_ITEM_ON_MAP:
            Net_OnAddItemOnMap();
            break;
        case NETMSG_ERASE_ITEM_FROM_MAP:
            Net_OnEraseItemFromMap();
            break;
        case NETMSG_ANIMATE_ITEM:
            Net_OnAnimateItem();
            break;
        case NETMSG_COMBAT_RESULTS:
            Net_OnCombatResult();
            break;
        case NETMSG_EFFECT:
            Net_OnEffect();
            break;
        case NETMSG_FLY_EFFECT:
            Net_OnFlyEffect();
            break;
        case NETMSG_PLAY_SOUND:
            Net_OnPlaySound();
            break;

        case NETMSG_UPDATE_FILES_LIST:
            Net_OnUpdateFilesList();
            break;
        case NETMSG_UPDATE_FILE_DATA:
            Net_OnUpdateFileData();
            break;

        default:
            _netIn.SkipMsg(msg);
            break;
        }
    }

    if (_netIn.IsError()) {
        if (Settings.DebugNet) {
            AddMess(SAY_NETMSG, "Invalid network message. Disconnect.");
        }

        WriteLog("Invalid network message. Disconnect.\n");
        NetDisconnect();
    }
}

void FOClient::Net_SendUpdate()
{
    // Header
    _netOut << NETMSG_UPDATE;

    // Protocol version
    _netOut << static_cast<ushort>(FO_COMPATIBILITY_VERSION);

    // Data encrypting
    const uint encrypt_key = 0x00420042;
    _netOut << encrypt_key;
    _netOut.SetEncryptKey(encrypt_key + 521);
    _netIn.SetEncryptKey(encrypt_key + 3491);
}

void FOClient::Net_SendLogIn()
{
    WriteLog("Player login.\n");

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(ushort) + NetBuffer::STRING_LEN_SIZE * 2u + static_cast<uint>(_loginName.length() + _loginPassword.length());

    _netOut << NETMSG_LOGIN;
    _netOut << msg_len;
    _netOut << FO_COMPATIBILITY_VERSION;

    // Begin data encrypting
    _netOut.SetEncryptKey(12345);
    _netIn.SetEncryptKey(12345);

    _netOut << _loginName;
    _netOut << _loginPassword;
    _netOut << _curLang.NameCode;

    AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_SUCCESS));
}

void FOClient::Net_SendCreatePlayer()
{
    WriteLog("Player registration.\n");

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(ushort) + NetBuffer::STRING_LEN_SIZE * 2u + static_cast<uint>(_loginName.length() + _loginPassword.length());

    _netOut << NETMSG_REGISTER;
    _netOut << msg_len;
    _netOut << FO_COMPATIBILITY_VERSION;

    // Begin data encrypting
    _netOut.SetEncryptKey(1234567890);
    _netIn.SetEncryptKey(1234567890);

    _netOut << _loginName;
    _netOut << _loginPassword;
}

void FOClient::Net_SendText(string_view send_str, uchar how_say)
{
    int say_type = how_say;
    auto str = string(send_str);
    const auto result = OnOutMessage.Fire(str, say_type);

    how_say = static_cast<uchar>(say_type);

    if (!result || str.empty()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(how_say) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(str.length());

    _netOut << NETMSG_SEND_TEXT;
    _netOut << msg_len;
    _netOut << how_say;
    _netOut << str;
}

void FOClient::Net_SendDir()
{
    if (_chosen == nullptr) {
        return;
    }

    _netOut << NETMSG_DIR;
    _netOut << _chosen->GetDir();
}

void FOClient::Net_SendMove(vector<uchar> steps)
{
    if (_chosen == nullptr) {
        return;
    }

    uint move_params = 0;
    for (uint i = 0; i < MOVE_PARAM_STEP_COUNT; i++) {
        const auto next_i = i + 1;
        if (next_i >= steps.size()) {
            break; // Send next steps
        }

        SetBit(move_params, static_cast<uint>(steps[next_i] << (i * MOVE_PARAM_STEP_BITS)));
        SetBit(move_params, MOVE_PARAM_STEP_ALLOW << (i * MOVE_PARAM_STEP_BITS));
    }

    if (_chosen->IsRunning) {
        SetBit(move_params, MOVE_PARAM_RUN);
    }
    if (steps.empty()) {
        SetBit(move_params, MOVE_PARAM_STEP_DISALLOW); // Inform about stopping
    }

    _netOut << (_chosen->IsRunning ? NETMSG_SEND_MOVE_RUN : NETMSG_SEND_MOVE_WALK);
    _netOut << move_params;
    _netOut << _chosen->GetHexX();
    _netOut << _chosen->GetHexY();
}

void FOClient::Net_SendProperty(NetProperty type, const Property* prop, Entity* entity)
{
    RUNTIME_ASSERT(entity);

    auto* client_entity = dynamic_cast<ClientEntity*>(entity);
    if (client_entity == nullptr) {
        return;
    }

    uint additional_args = 0;
    switch (type) {
    case NetProperty::Critter:
        additional_args = 1;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        break;
    case NetProperty::CritterItem:
        additional_args = 2;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        break;
    default:
        break;
    }

    uint data_size = 0;
    const void* data = client_entity->GetProperties().GetRawData(prop, data_size);

    const auto is_pod = prop->IsPlainData();
    if (is_pod) {
        _netOut << NETMSG_SEND_POD_PROPERTY(data_size, additional_args);
    }
    else {
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(char) + additional_args * sizeof(uint) + sizeof(ushort) + data_size;
        _netOut << NETMSG_SEND_COMPLEX_PROPERTY;
        _netOut << msg_len;
    }

    _netOut << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        _netOut << dynamic_cast<ItemView*>(client_entity)->GetCritId();
        _netOut << client_entity->GetId();
        break;
    case NetProperty::Critter:
        _netOut << client_entity->GetId();
        break;
    case NetProperty::MapItem:
        _netOut << client_entity->GetId();
        break;
    case NetProperty::ChosenItem:
        _netOut << client_entity->GetId();
        break;
    default:
        break;
    }

    if (is_pod) {
        _netOut << prop->GetRegIndex();
        _netOut.Push(data, data_size);
    }
    else {
        _netOut << prop->GetRegIndex();
        if (data_size != 0u) {
            _netOut.Push(data, data_size);
        }
    }
}

void FOClient::Net_SendTalk(uchar is_npc, uint id_to_talk, uchar answer)
{
    _netOut << NETMSG_SEND_TALK_NPC;
    _netOut << is_npc;
    _netOut << id_to_talk;
    _netOut << answer;
}

void FOClient::Net_SendGetGameInfo()
{
    _netOut << NETMSG_SEND_GET_INFO;
}

void FOClient::Net_SendGiveMap(bool automap, hstring map_pid, uint loc_id, uint tiles_hash, uint scen_hash)
{
    _netOut << NETMSG_SEND_GIVE_MAP;
    _netOut << automap;
    _netOut << map_pid;
    _netOut << loc_id;
    _netOut << tiles_hash;
    _netOut << scen_hash;
}

void FOClient::Net_SendLoadMapOk()
{
    _netOut << NETMSG_SEND_LOAD_MAP_OK;
}

void FOClient::Net_SendPing(uchar ping)
{
    _netOut << NETMSG_PING;
    _netOut << ping;
}

void FOClient::Net_SendRefereshMe()
{
    _netOut << NETMSG_SEND_REFRESH_ME;

    WaitPing();
}

void FOClient::Net_OnWrongNetProto()
{
    if (_updateData) {
        UpdateFilesAbort(STR_CLIENT_OUTDATED, "Client outdated!");
    }
    else {
        AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_CLIENT_OUTDATED));
    }
}

void FOClient::Net_OnLoginSuccess()
{
    WriteLog("Authentication success.\n");

    AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_LOGINOK));

    // Set encrypt keys
    uint msg_len;
    uint bin_seed;
    uint bout_seed; // Server bin/bout == client bout/bin
    uint player_id;
    _netIn >> msg_len;
    _netIn >> bin_seed;
    _netIn >> bout_seed;
    _netIn >> player_id;

    NET_READ_PROPERTIES(_netIn, _globalsPropertiesData);
    NET_READ_PROPERTIES(_netIn, _playerPropertiesData);

    CHECK_IN_BUFF_ERROR();

    _netOut.SetEncryptKey(bin_seed);
    _netIn.SetEncryptKey(bout_seed);

    RestoreData(_globalsPropertiesData);

    if (_curPlayer != nullptr) {
        _curPlayer->MarkAsDestroyed();
        _curPlayer->Release();
        _curPlayer = nullptr;
    }

    _curPlayer = new PlayerView(this, player_id, nullptr); // Todo: proto player?
    _curPlayer->RestoreData(_playerPropertiesData);
}

void FOClient::Net_OnAddCritter(bool is_npc)
{
    uint msg_len;
    _netIn >> msg_len;

    uint crid;
    ushort hx;
    ushort hy;
    uchar dir;
    _netIn >> crid;
    _netIn >> hx;
    _netIn >> hy;
    _netIn >> dir;

    CritterCondition cond;
    uint anim1_alive;
    uint anim1_ko;
    uint anim1_dead;
    uint anim2_alive;
    uint anim2_ko;
    uint anim2_dead;
    uint flags;
    _netIn >> cond;
    _netIn >> anim1_alive;
    _netIn >> anim1_ko;
    _netIn >> anim1_dead;
    _netIn >> anim2_alive;
    _netIn >> anim2_ko;
    _netIn >> anim2_dead;
    _netIn >> flags;

    // Npc
    hstring npc_pid;
    if (is_npc) {
        npc_pid = _netIn.ReadHashedString(*this);
    }

    // Player
    string cl_name;
    if (!is_npc) {
        _netIn >> cl_name;
    }

    // Properties
    NET_READ_PROPERTIES(_netIn, _tempPropertiesData);

    CHECK_IN_BUFF_ERROR();

    if (crid == 0u) {
        WriteLog("Critter id is zero.\n");
    }
    else if (HexMngr.IsMapLoaded() && (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight() || dir >= Settings.MapDirCount)) {
        WriteLog("Invalid positions hx {}, hy {}, dir {}.\n", hx, hy, dir);
    }
    else {
        const auto* proto = ProtoMngr->GetProtoCritter(is_npc ? npc_pid : ToHashedString("Player"));
        RUNTIME_ASSERT(proto);

        auto* cr = new CritterView(this, crid, proto, false);
        cr->RestoreData(_tempPropertiesData);
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->SetDir(dir);
        cr->SetCond(cond);
        cr->SetAnim1Alive(anim1_alive);
        cr->SetAnim1Knockout(anim1_ko);
        cr->SetAnim1Dead(anim1_dead);
        cr->SetAnim2Alive(anim2_alive);
        cr->SetAnim2Knockout(anim2_ko);
        cr->SetAnim2Dead(anim2_dead);
        cr->Flags = flags;

        if (is_npc) {
            if (cr->GetDialogId() && (_curLang.Msg[TEXTMSG_DLG].Count(STR_NPC_NAME(cr->GetDialogId().as_uint())) != 0u)) {
                cr->AlternateName = _curLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_NAME(cr->GetDialogId().as_uint()));
            }
            else {
                cr->AlternateName = _curLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_PID_NAME(npc_pid.as_uint()));
            }
            if (_curLang.Msg[TEXTMSG_DLG].Count(STR_NPC_AVATAR(cr->GetDialogId().as_uint()))) {
                cr->Avatar = _curLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_AVATAR(cr->GetDialogId().as_uint()));
            }
        }
        else {
            cr->AlternateName = cl_name;
        }

        cr->Init();

        AddCritter(cr);

        OnCritterIn.Fire(cr);

        if (cr->IsChosen()) {
            _rebuildLookBordersRequest = true;
        }
    }
}

void FOClient::Net_OnRemoveCritter()
{
    uint remove_crid;
    _netIn >> remove_crid;

    CHECK_IN_BUFF_ERROR();

    auto* cr = GetCritter(remove_crid);
    if (cr == nullptr) {
        return;
    }

    cr->Finish();

    OnCritterOut.Fire(cr);
}

void FOClient::Net_OnText()
{
    uint msg_len;
    uint crid;
    uchar how_say;
    string text;
    bool unsafe_text;
    _netIn >> msg_len;
    _netIn >> crid;
    _netIn >> how_say;
    _netIn >> text;
    _netIn >> unsafe_text;

    CHECK_IN_BUFF_ERROR();

    if (how_say == SAY_FLASH_WINDOW) {
        FlashGameWindow();
        return;
    }

    if (unsafe_text) {
        Keyb.EraseInvalidChars(text, KIF_NO_SPEC_SYMBOLS);
    }
    OnText(text, crid, how_say);
}

void FOClient::Net_OnTextMsg(bool with_lexems)
{
    if (with_lexems) {
        uint msg_len;
        _netIn >> msg_len;
    }

    uint crid;
    uchar how_say;
    ushort msg_num;
    uint num_str;
    _netIn >> crid;
    _netIn >> how_say;
    _netIn >> msg_num;
    _netIn >> num_str;

    string lexems;
    if (with_lexems) {
        _netIn >> lexems;
    }

    CHECK_IN_BUFF_ERROR();

    if (how_say == SAY_FLASH_WINDOW) {
        FlashGameWindow();
        return;
    }

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value {}.\n", msg_num);
        return;
    }

    auto& msg = _curLang.Msg[msg_num];
    if (msg.Count(num_str) != 0u) {
        auto str = msg.GetStr(num_str);
        FormatTags(str, _chosen, GetCritter(crid), lexems);
        OnText(str, crid, how_say);
    }
}

void FOClient::OnText(string_view str, uint crid, int how_say)
{
    auto fstr = string(str);
    if (fstr.empty()) {
        return;
    }

    auto text_delay = Settings.TextDelay + static_cast<uint>(fstr.length()) * 100u;
    const auto sstr = fstr;
    if (!OnInMessage.Fire(sstr, how_say, crid, text_delay)) {
        return;
    }

    fstr = sstr;
    if (fstr.empty()) {
        return;
    }

    // Type stream
    uint fstr_cr = 0;
    uint fstr_mb = 0;

    switch (how_say) {
    case SAY_NORM:
        fstr_mb = STR_MBNORM;
        fstr_cr = STR_CRNORM;
        break;
    case SAY_NORM_ON_HEAD:
        fstr_cr = STR_CRNORM;
        break;
    case SAY_SHOUT:
        fstr_mb = STR_MBSHOUT;
        fstr_cr = STR_CRSHOUT;
        fstr = _str(fstr).upperUtf8();
        break;
    case SAY_SHOUT_ON_HEAD:
        fstr_cr = STR_CRSHOUT;
        fstr = _str(fstr).upperUtf8();
        break;
    case SAY_EMOTE:
        fstr_mb = STR_MBEMOTE;
        fstr_cr = STR_CREMOTE;
        break;
    case SAY_EMOTE_ON_HEAD:
        fstr_cr = STR_CREMOTE;
        break;
    case SAY_WHISP:
        fstr_mb = STR_MBWHISP;
        fstr_cr = STR_CRWHISP;
        fstr = _str(fstr).lowerUtf8();
        break;
    case SAY_WHISP_ON_HEAD:
        fstr_cr = STR_CRWHISP;
        fstr = _str(fstr).lowerUtf8();
        break;
    case SAY_SOCIAL:
        fstr_cr = STR_CRSOCIAL;
        fstr_mb = STR_MBSOCIAL;
        break;
    case SAY_RADIO:
        fstr_mb = STR_MBRADIO;
        break;
    case SAY_NETMSG:
        fstr_mb = STR_MBNET;
        break;
    default:
        break;
    }

    const auto get_format = [this](uint str_num) -> string { return _str(_curLang.Msg[TEXTMSG_GAME].GetStr(str_num)).replace('\\', 'n', '\n'); };

    auto* cr = (how_say != SAY_RADIO ? GetCritter(crid) : nullptr);

    // Critter text on head
    if (fstr_cr != 0u && cr != nullptr) {
        cr->SetText(_str(get_format(fstr_cr), fstr), COLOR_TEXT, text_delay);
    }

    // Message box text
    if (fstr_mb != 0u) {
        if (how_say == SAY_NETMSG) {
            AddMess(how_say, _str(get_format(fstr_mb), fstr));
        }
        else if (how_say == SAY_RADIO) {
            ushort channel = 0u;
            if (_chosen != nullptr) {
                const auto* radio = _chosen->GetItem(crid);
                if (radio != nullptr) {
                    channel = radio->GetRadioChannel();
                }
            }
            AddMess(how_say, _str(get_format(fstr_mb), channel, fstr));
        }
        else {
            const auto cr_name = (cr != nullptr ? cr->GetName() : "?");
            AddMess(how_say, _str(get_format(fstr_mb), cr_name, fstr));
        }
    }

    FlashGameWindow();
}

void FOClient::OnMapText(string_view str, ushort hx, ushort hy, uint color)
{
    auto text_delay = Settings.TextDelay + static_cast<uint>(str.length()) * 100;

    auto sstr = _str(str).str();
    OnMapMessage.Fire(sstr, hx, hy, color, text_delay);

    MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = (color != 0u ? color : COLOR_TEXT);
    map_text.Fade = false;
    map_text.StartTick = GameTime.GameTick();
    map_text.Tick = text_delay;
    map_text.Text = sstr;
    map_text.Pos = HexMngr.GetRectForText(hx, hy);
    map_text.EndPos = map_text.Pos;

    const auto it = std::find_if(GameMapTexts.begin(), GameMapTexts.end(), [&map_text](const MapText& t) { return map_text.HexX == t.HexX && map_text.HexY == t.HexY; });
    if (it != GameMapTexts.end()) {
        GameMapTexts.erase(it);
    }

    GameMapTexts.push_back(map_text);

    FlashGameWindow();
}

void FOClient::Net_OnMapText()
{
    uint msg_len;
    ushort hx;
    ushort hy;
    uint color;
    string text;
    bool unsafe_text;
    _netIn >> msg_len;
    _netIn >> hx;
    _netIn >> hy;
    _netIn >> color;
    _netIn >> text;
    _netIn >> unsafe_text;

    CHECK_IN_BUFF_ERROR();

    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight()) {
        WriteLog("Invalid coords, hx {}, hy {}, text '{}'.\n", hx, hy, text);
        return;
    }

    if (unsafe_text) {
        Keyb.EraseInvalidChars(text, KIF_NO_SPEC_SYMBOLS);
    }

    OnMapText(text, hx, hy, color);
}

void FOClient::Net_OnMapTextMsg()
{
    ushort hx;
    ushort hy;
    uint color;
    ushort msg_num;
    uint num_str;
    _netIn >> hx;
    _netIn >> hy;
    _netIn >> color;
    _netIn >> msg_num;
    _netIn >> num_str;

    CHECK_IN_BUFF_ERROR();

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}.\n", msg_num);
        return;
    }

    auto str = _curLang.Msg[msg_num].GetStr(num_str);
    FormatTags(str, _chosen, nullptr, "");
    OnMapText(str, hx, hy, color);
}

void FOClient::Net_OnMapTextMsgLex()
{
    uint msg_len;
    ushort hx;
    ushort hy;
    uint color;
    ushort msg_num;
    uint num_str;
    string lexems;
    _netIn >> msg_len;
    _netIn >> hx;
    _netIn >> hy;
    _netIn >> color;
    _netIn >> msg_num;
    _netIn >> num_str;
    _netIn >> lexems;

    CHECK_IN_BUFF_ERROR();

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}.\n", msg_num);
        return;
    }

    auto str = _curLang.Msg[msg_num].GetStr(num_str);
    FormatTags(str, _chosen, nullptr, lexems);
    OnMapText(str, hx, hy, color);
}

void FOClient::Net_OnCritterDir()
{
    uint crid;
    uchar dir;
    _netIn >> crid;
    _netIn >> dir;

    CHECK_IN_BUFF_ERROR();

    if (dir >= Settings.MapDirCount) {
        WriteLog("Invalid dir {}.\n", dir);
        dir = 0;
    }

    auto* cr = GetCritter(crid);
    if (cr != nullptr) {
        cr->ChangeDir(dir, false);
    }
}

void FOClient::Net_OnCritterMove()
{
    uint crid;
    uint move_params;
    ushort new_hx;
    ushort new_hy;
    _netIn >> crid;
    _netIn >> move_params;
    _netIn >> new_hx;
    _netIn >> new_hy;

    CHECK_IN_BUFF_ERROR();

    if (new_hx >= HexMngr.GetWidth() || new_hy >= HexMngr.GetHeight()) {
        return;
    }

    auto* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    cr->IsRunning = IsBitSet(move_params, MOVE_PARAM_RUN);

    if (cr != _chosen) {
        cr->MoveSteps.resize(cr->CurMoveStep > 0 ? cr->CurMoveStep : 0);
        if (cr->CurMoveStep >= 0) {
            cr->MoveSteps.push_back(std::make_pair(new_hx, new_hy));
        }

        for (auto i = 0, j = cr->CurMoveStep + 1; i < static_cast<int>(MOVE_PARAM_STEP_COUNT); i++, j++) {
            const auto step = (move_params >> (i * MOVE_PARAM_STEP_BITS)) & (MOVE_PARAM_STEP_DIR | MOVE_PARAM_STEP_ALLOW | MOVE_PARAM_STEP_DISALLOW);
            const uchar dir = step & MOVE_PARAM_STEP_DIR;

            const auto disallow = (!IsBitSet(step, MOVE_PARAM_STEP_ALLOW) || IsBitSet(step, MOVE_PARAM_STEP_DISALLOW));
            if (disallow) {
                if (j <= 0 && (cr->GetHexX() != new_hx || cr->GetHexY() != new_hy)) {
                    HexMngr.TransitCritter(cr, new_hx, new_hy, true, true);
                    cr->CurMoveStep = -1;
                }
                break;
            }

            GeomHelper.MoveHexByDir(new_hx, new_hy, dir, HexMngr.GetWidth(), HexMngr.GetHeight());

            if (j < 0) {
                continue;
            }

            cr->MoveSteps.push_back(std::make_pair(new_hx, new_hy));
        }
        cr->CurMoveStep++;
    }
    else {
        cr->MoveSteps.clear();
        cr->CurMoveStep = 0;
        HexMngr.TransitCritter(cr, new_hx, new_hy, true, true);
    }
}

void FOClient::Net_OnSomeItem()
{
    uint msg_len;
    uint item_id;
    hstring item_pid;
    _netIn >> msg_len;
    _netIn >> item_id;
    item_pid = _netIn.ReadHashedString(*this);

    NET_READ_PROPERTIES(_netIn, _tempPropertiesData);

    CHECK_IN_BUFF_ERROR();

    if (_someItem != nullptr) {
        _someItem->Release();
        _someItem = nullptr;
    }

    const auto* proto_item = ProtoMngr->GetProtoItem(item_pid);
    RUNTIME_ASSERT(proto_item);
    _someItem = new ItemView(this, item_id, proto_item);
    _someItem->RestoreData(_tempPropertiesData);
}

void FOClient::Net_OnCritterAction()
{
    uint crid;
    int action;
    int action_ext;
    bool is_item;
    _netIn >> crid;
    _netIn >> action;
    _netIn >> action_ext;
    _netIn >> is_item;

    CHECK_IN_BUFF_ERROR();

    auto* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    cr->Action(action, action_ext, is_item ? _someItem : nullptr, false);
}

void FOClient::Net_OnCritterMoveItem()
{
    uint msg_len;
    uint crid;
    uchar action;
    uchar prev_slot;
    bool is_item;
    _netIn >> msg_len;
    _netIn >> crid;
    _netIn >> action;
    _netIn >> prev_slot;
    _netIn >> is_item;

    // Slot items
    ushort slots_data_count;
    _netIn >> slots_data_count;

    CHECK_IN_BUFF_ERROR();

    vector<uchar> slots_data_slot;
    vector<uint> slots_data_id;
    vector<hstring> slots_data_pid;
    vector<vector<vector<uchar>>> slots_data_data;
    for ([[maybe_unused]] const auto i : xrange(slots_data_count)) {
        uchar slot;
        uint id;
        hstring pid;
        _netIn >> slot;
        _netIn >> id;
        pid = _netIn.ReadHashedString(*this);
        NET_READ_PROPERTIES(_netIn, _tempPropertiesData);
        slots_data_slot.push_back(slot);
        slots_data_id.push_back(id);
        slots_data_pid.push_back(pid);
        slots_data_data.push_back(_tempPropertiesData);
    }

    CHECK_IN_BUFF_ERROR();

    auto* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (cr != _chosen) {
        int64 prev_hash_sum = 0;
        for (const auto* item : cr->InvItems) {
            prev_hash_sum += item->LightGetHash();
        }

        cr->DeleteAllItems();

        for (ushort i = 0; i < slots_data_count; i++) {
            const auto* proto_item = ProtoMngr->GetProtoItem(slots_data_pid[i]);
            if (proto_item != nullptr) {
                auto* item = new ItemView(this, slots_data_id[i], proto_item);
                item->RestoreData(slots_data_data[i]);
                item->SetCritSlot(slots_data_slot[i]);
                cr->AddItem(item);
            }
        }

        int64 hash_sum = 0;
        for (const auto* item : cr->InvItems) {
            hash_sum += item->LightGetHash();
        }
        if (hash_sum != prev_hash_sum) {
            HexMngr.RebuildLight();
        }
    }

    cr->Action(action, prev_slot, is_item ? _someItem : nullptr, false);
}

void FOClient::Net_OnCritterAnimate()
{
    uint crid;
    uint anim1;
    uint anim2;
    bool is_item;
    bool clear_sequence;
    bool delay_play;
    _netIn >> crid;
    _netIn >> anim1;
    _netIn >> anim2;
    _netIn >> is_item;
    _netIn >> clear_sequence;
    _netIn >> delay_play;

    CHECK_IN_BUFF_ERROR();

    auto* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (clear_sequence) {
        cr->ClearAnim();
    }
    if (delay_play || !cr->IsAnim()) {
        cr->Animate(anim1, anim2, is_item ? _someItem : nullptr);
    }
}

void FOClient::Net_OnCritterSetAnims()
{
    uint crid;
    CritterCondition cond;
    uint anim1;
    uint anim2;
    _netIn >> crid;
    _netIn >> cond;
    _netIn >> anim1;
    _netIn >> anim2;

    CHECK_IN_BUFF_ERROR();

    auto* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (cond == CritterCondition::Alive) {
        cr->SetAnim1Alive(anim1);
        cr->SetAnim2Alive(anim2);
    }
    if (cond == CritterCondition::Knockout) {
        cr->SetAnim1Knockout(anim1);
        cr->SetAnim2Knockout(anim2);
    }
    if (cond == CritterCondition::Dead) {
        cr->SetAnim1Dead(anim1);
        cr->SetAnim2Dead(anim2);
    }

    if (!cr->IsAnim()) {
        cr->AnimateStay();
    }
}

void FOClient::Net_OnCustomCommand()
{
    uint crid;
    ushort index;
    int value;
    _netIn >> crid;
    _netIn >> index;
    _netIn >> value;

    CHECK_IN_BUFF_ERROR();

    if (Settings.DebugNet) {
        AddMess(SAY_NETMSG, _str(" - crid {} index {} value {}.", crid, index, value));
    }

    auto* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (cr->IsChosen()) {
        if (Settings.DebugNet) {
            AddMess(SAY_NETMSG, _str(" - index {} value {}.", index, value));
        }

        switch (index) {
        case OTHER_BREAK_TIME: {
            if (value < 0) {
                value = 0;
            }
            _chosen->TickStart(value);
            _chosen->MoveSteps.clear();
            _chosen->CurMoveStep = 0;
        } break;
        case OTHER_FLAGS: {
            _chosen->Flags = value;
        } break;
        case OTHER_CLEAR_MAP: {
            auto crits = HexMngr.GetCritters();
            for (auto& [id, cr] : crits) {
                if (cr != _chosen) {
                    DeleteCritter(cr->GetId());
                }
            }
            auto items = HexMngr.GetItems();
            for (auto* item : items) {
                if (!item->IsStatic()) {
                    HexMngr.DeleteItem(item, true, nullptr);
                }
            }
        } break;
        case OTHER_TELEPORT: {
            const ushort hx = (value >> 16) & 0xFFFF;
            const ushort hy = value & 0xFFFF;
            if (hx < HexMngr.GetWidth() && hy < HexMngr.GetHeight()) {
                auto* hex_cr = HexMngr.GetField(hx, hy).Crit;
                if (_chosen == hex_cr) {
                    break;
                }
                if (!_chosen->IsDead() && hex_cr != nullptr) {
                    DeleteCritter(hex_cr->GetId());
                }
                HexMngr.RemoveCritter(_chosen);
                _chosen->SetHexX(hx);
                _chosen->SetHexY(hy);
                HexMngr.SetCritter(_chosen);
                HexMngr.ScrollToHex(_chosen->GetHexX(), _chosen->GetHexY(), 0.1f, true);
            }
        } break;
        default:
            break;
        }

        // Maybe changed some parameter influencing on look borders
        _rebuildLookBordersRequest = true;
    }
    else {
        if (index == OTHER_FLAGS) {
            cr->Flags = value;
        }
    }
}

void FOClient::Net_OnCritterXY()
{
    uint crid;
    ushort hx;
    ushort hy;
    uchar dir;
    _netIn >> crid;
    _netIn >> hx;
    _netIn >> hy;
    _netIn >> dir;

    CHECK_IN_BUFF_ERROR();

    if (Settings.DebugNet) {
        AddMess(SAY_NETMSG, _str(" - crid {} hx {} hy {} dir {}.", crid, hx, hy, dir));
    }

    if (!HexMngr.IsMapLoaded()) {
        return;
    }

    auto* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight() || dir >= Settings.MapDirCount) {
        WriteLog("Error data, hx {}, hy {}, dir {}.\n", hx, hy, dir);
        return;
    }

    if (cr->GetDir() != dir) {
        cr->ChangeDir(dir, false);
    }

    if (cr->GetHexX() != hx || cr->GetHexY() != hy) {
        auto& f = HexMngr.GetField(hx, hy);
        if (f.Crit != nullptr && f.Crit->IsFinishing()) {
            DeleteCritter(f.Crit->GetId());
        }

        HexMngr.TransitCritter(cr, hx, hy, false, true);
        cr->AnimateStay();
        if (cr == _chosen) {
            // Chosen->TickStart(Settings.Breaktime);
            _chosen->TickStart(200);
            // SetAction(CHOSEN_NONE);
            _chosen->MoveSteps.clear();
            _chosen->CurMoveStep = 0;
            _rebuildLookBordersRequest = true;
        }
    }

    cr->MoveSteps.clear();
    cr->CurMoveStep = 0;
}

void FOClient::Net_OnAllProperties()
{
    WriteLog("Chosen properties.\n");

    uint msg_len;
    _netIn >> msg_len;

    NET_READ_PROPERTIES(_netIn, _tempPropertiesData);

    CHECK_IN_BUFF_ERROR();

    if (_chosen == nullptr) {
        WriteLog("Chosen not created, skip.\n");
        return;
    }

    _chosen->RestoreData(_tempPropertiesData);

    // Animate
    if (!_chosen->IsAnim()) {
        _chosen->AnimateStay();
    }

    // Refresh borders
    _rebuildLookBordersRequest = true;
}

void FOClient::Net_OnChosenClearItems()
{
    _initialItemsSend = true;

    if (_chosen == nullptr) {
        WriteLog("Chosen is not created in clear items.\n");
        return;
    }

    if (_chosen->IsHaveLightSources()) {
        HexMngr.RebuildLight();
    }

    _chosen->DeleteAllItems();
}

void FOClient::Net_OnChosenAddItem()
{
    uint msg_len;
    uint item_id;
    hstring pid;
    uchar slot;
    _netIn >> msg_len;
    _netIn >> item_id;
    pid = _netIn.ReadHashedString(*this);
    _netIn >> slot;

    NET_READ_PROPERTIES(_netIn, _tempPropertiesData);

    CHECK_IN_BUFF_ERROR();

    if (_chosen == nullptr) {
        WriteLog("Chosen is not created in add item.\n");
        return;
    }

    auto* prev_item = _chosen->GetItem(item_id);
    uchar prev_slot = 0;
    uint prev_light_hash = 0;
    if (prev_item != nullptr) {
        prev_slot = prev_item->GetCritSlot();
        prev_light_hash = prev_item->LightGetHash();
        _chosen->DeleteItem(prev_item, false);
    }

    const auto* proto_item = ProtoMngr->GetProtoItem(pid);
    RUNTIME_ASSERT(proto_item);

    auto* item = new ItemView(this, item_id, proto_item);
    item->RestoreData(_tempPropertiesData);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritId(_chosen->GetId());
    item->SetCritSlot(slot);
    RUNTIME_ASSERT(!item->GetIsHidden());

    item->AddRef();

    _chosen->AddItem(item);

    _rebuildLookBordersRequest = true;
    if (item->LightGetHash() != prev_light_hash && (slot != 0u || prev_slot != 0u)) {
        HexMngr.RebuildLight();
    }

    if (!_initialItemsSend) {
        OnItemInvIn.Fire(item);
    }

    item->Release();
}

void FOClient::Net_OnChosenEraseItem()
{
    uint item_id;
    _netIn >> item_id;

    CHECK_IN_BUFF_ERROR();

    if (_chosen == nullptr) {
        WriteLog("Chosen is not created in erase item.\n");
        return;
    }

    auto* item = _chosen->GetItem(item_id);
    if (item == nullptr) {
        WriteLog("Item not found, id {}.\n", item_id);
        return;
    }

    auto* item_clone = item->Clone();

    const auto rebuild_light = (item->GetIsLight() && item->GetCritSlot() != 0u);
    _chosen->DeleteItem(item, true);
    if (rebuild_light) {
        HexMngr.RebuildLight();
    }

    OnItemInvOut.Fire(item_clone);
    item_clone->Release();
}

void FOClient::Net_OnAllItemsSend()
{
    _initialItemsSend = false;

    if (_chosen == nullptr) {
        WriteLog("Chosen is not created in all items send.\n");
        return;
    }

    if (_chosen->IsModel()) {
        _chosen->GetModel()->StartMeshGeneration();
    }

    OnItemInvAllIn.Fire();
}

void FOClient::Net_OnAddItemOnMap()
{
    uint msg_len;
    uint item_id;
    hstring item_pid;
    ushort item_hx;
    ushort item_hy;
    bool is_added;
    _netIn >> msg_len;
    _netIn >> item_id;
    item_pid = _netIn.ReadHashedString(*this);
    _netIn >> item_hx;
    _netIn >> item_hy;
    _netIn >> is_added;

    NET_READ_PROPERTIES(_netIn, _tempPropertiesData);

    CHECK_IN_BUFF_ERROR();

    if (HexMngr.IsMapLoaded()) {
        HexMngr.AddItem(item_id, item_pid, item_hx, item_hy, is_added, &_tempPropertiesData);
    }

    ItemView* item = HexMngr.GetItemById(item_id);
    if (item != nullptr) {
        OnItemMapIn.Fire(item);

        // Refresh borders
        if (!item->GetIsShootThru()) {
            _rebuildLookBordersRequest = true;
        }
    }
}

void FOClient::Net_OnEraseItemFromMap()
{
    uint item_id;
    bool is_deleted;
    _netIn >> item_id;
    _netIn >> is_deleted;

    CHECK_IN_BUFF_ERROR();

    ItemView* item = HexMngr.GetItemById(item_id);
    if (item != nullptr) {
        OnItemMapOut.Fire(item);

        // Refresh borders
        if (!item->GetIsShootThru()) {
            _rebuildLookBordersRequest = true;
        }
    }

    HexMngr.FinishItem(item_id, is_deleted);
}

void FOClient::Net_OnAnimateItem()
{
    uint item_id;
    uchar from_frm;
    uchar to_frm;
    _netIn >> item_id;
    _netIn >> from_frm;
    _netIn >> to_frm;

    CHECK_IN_BUFF_ERROR();

    auto* item = HexMngr.GetItemById(item_id);
    if (item != nullptr) {
        item->SetAnim(from_frm, to_frm);
    }
}

void FOClient::Net_OnCombatResult()
{
    uint msg_len;
    uint data_count;
    vector<uint> data_vec;
    _netIn >> msg_len;
    _netIn >> data_count;

    if (data_count != 0u) {
        data_vec.resize(data_count);
        _netIn.Pop(data_vec.data(), data_count * sizeof(uint));
    }

    CHECK_IN_BUFF_ERROR();

    OnCombatResult.Fire(data_vec);
}

void FOClient::Net_OnEffect()
{
    hstring eff_pid;
    ushort hx;
    ushort hy;
    ushort radius;
    eff_pid = _netIn.ReadHashedString(*this);
    _netIn >> hx;
    _netIn >> hy;
    _netIn >> radius;

    CHECK_IN_BUFF_ERROR();

    // Base hex effect
    HexMngr.RunEffect(eff_pid, hx, hy, hx, hy);

    // Radius hexes effect
    if (radius > MAX_HEX_OFFSET) {
        radius = MAX_HEX_OFFSET;
    }

    const auto [sx, sy] = GeomHelper.GetHexOffsets((hx % 2) != 0);
    const auto maxhx = HexMngr.GetWidth();
    const auto maxhy = HexMngr.GetHeight();
    const auto count = GenericUtils::NumericalNumber(radius) * Settings.MapDirCount;

    for (uint i = 0; i < count; i++) {
        const auto ex = static_cast<short>(hx) + sx[i];
        const auto ey = static_cast<short>(hy) + sy[i];
        if (ex >= 0 && ey >= 0 && ex < maxhx && ey < maxhy) {
            HexMngr.RunEffect(eff_pid, static_cast<ushort>(ex), static_cast<ushort>(ey), static_cast<ushort>(ex), static_cast<ushort>(ey));
        }
    }
}

// Todo: synchronize effects showing (for example shot and kill)
void FOClient::Net_OnFlyEffect()
{
    hstring eff_pid;
    uint eff_cr1_id;
    uint eff_cr2_id;
    ushort eff_cr1_hx;
    ushort eff_cr1_hy;
    ushort eff_cr2_hx;
    ushort eff_cr2_hy;
    eff_pid = _netIn.ReadHashedString(*this);
    _netIn >> eff_cr1_id;
    _netIn >> eff_cr2_id;
    _netIn >> eff_cr1_hx;
    _netIn >> eff_cr1_hy;
    _netIn >> eff_cr2_hx;
    _netIn >> eff_cr2_hy;

    CHECK_IN_BUFF_ERROR();

    const auto* cr1 = GetCritter(eff_cr1_id);
    if (cr1 != nullptr) {
        eff_cr1_hx = cr1->GetHexX();
        eff_cr1_hy = cr1->GetHexY();
    }

    const auto* cr2 = GetCritter(eff_cr2_id);
    if (cr2 != nullptr) {
        eff_cr2_hx = cr2->GetHexX();
        eff_cr2_hy = cr2->GetHexY();
    }

    if (!HexMngr.RunEffect(eff_pid, eff_cr1_hx, eff_cr1_hy, eff_cr2_hx, eff_cr2_hy)) {
        WriteLog("Run effect '{}' failed.\n", eff_pid);
    }
}

void FOClient::Net_OnPlaySound()
{
    uint msg_len;
    uint synchronize_crid;
    string sound_name;
    _netIn >> msg_len;
    _netIn >> synchronize_crid;
    _netIn >> sound_name;

    CHECK_IN_BUFF_ERROR();

    SndMngr.PlaySound(ResMngr.GetSoundNames(), sound_name);
}

void FOClient::Net_OnPing()
{
    uchar ping;
    _netIn >> ping;

    CHECK_IN_BUFF_ERROR();

    if (ping == PING_WAIT) {
        Settings.WaitPing = false;
    }
    else if (ping == PING_CLIENT) {
        _netOut << NETMSG_PING;
        _netOut << PING_CLIENT;
    }
    else if (ping == PING_PING) {
        Settings.Ping = GameTime.FrameTick() - _pingTick;
        _pingTick = 0;
        _pingCallTick = GameTime.FrameTick() + Settings.PingPeriod;
    }
}

void FOClient::Net_OnEndParseToGame()
{
    if (_chosen == nullptr) {
        WriteLog("Chosen is not created in end parse to game.\n");
        return;
    }

    FlashGameWindow();
    ScreenFadeOut();

    if (CurMapPid) {
        HexMngr.SkipItemsFade();
        HexMngr.FindSetCenter(_chosen->GetHexX(), _chosen->GetHexY());
        _chosen->AnimateStay();
        ShowMainScreen(SCREEN_GAME, {});
        HexMngr.RebuildLight();
    }
    else {
        ShowMainScreen(SCREEN_GLOBAL_MAP, {});
    }

    WriteLog("Entering to game complete.\n");
}

void FOClient::Net_OnProperty(uint data_size)
{
    uint msg_len;
    if (data_size == 0u) {
        _netIn >> msg_len;
    }
    else {
        msg_len = 0;
    }

    char type_ = 0;
    _netIn >> type_;
    const auto type = static_cast<NetProperty>(type_);

    uint cr_id;
    uint item_id;

    uint additional_args = 0;
    switch (type) {
    case NetProperty::CritterItem:
        additional_args = 2;
        _netIn >> cr_id;
        _netIn >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        _netIn >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        _netIn >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        _netIn >> item_id;
        break;
    default:
        break;
    }

    ushort property_index;
    _netIn >> property_index;

    if (data_size != 0u) {
        _tempPropertyData.resize(data_size);
        _netIn.Pop(_tempPropertyData.data(), data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
        _tempPropertyData.resize(len);
        if (len != 0u) {
            _netIn.Pop(_tempPropertyData.data(), len);
        }
    }

    CHECK_IN_BUFF_ERROR();

    Entity* entity = nullptr;
    switch (type) {
    case NetProperty::Game:
        entity = this;
        break;
    case NetProperty::Player:
        entity = _curPlayer;
        break;
    case NetProperty::Critter:
        entity = GetCritter(cr_id);
        break;
    case NetProperty::Chosen:
        entity = _chosen;
        break;
    case NetProperty::MapItem:
        entity = HexMngr.GetItemById(item_id);
        break;
    case NetProperty::CritterItem:
        if (auto* cr = GetCritter(cr_id); cr != nullptr) {
            entity = cr->GetItem(item_id);
        }
        break;
    case NetProperty::ChosenItem:
        entity = (_chosen != nullptr ? _chosen->GetItem(item_id) : nullptr);
        break;
    case NetProperty::Map:
        entity = _curMap;
        break;
    case NetProperty::Location:
        entity = _curLocation;
        break;
    default:
        throw UnreachablePlaceException(LINE_STR);
    }
    if (entity == nullptr) {
        return;
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(property_index);
    if (prop == nullptr) {
        return;
    }

    entity->SetValueFromData(prop, _tempPropertyData, true);

    if (type == NetProperty::MapItem) {
        OnItemMapChanged.Fire(dynamic_cast<ItemView*>(entity), dynamic_cast<ItemView*>(entity));
    }

    if (type == NetProperty::ChosenItem) {
        auto* item = dynamic_cast<ItemView*>(entity);
        item->AddRef();
        OnItemInvChanged.Fire(item, item);
        item->Release();
    }
}

void FOClient::Net_OnChosenTalk()
{
    uint msg_len;
    uchar is_npc;
    uint talk_id;
    uchar count_answ;
    uint text_id;
    uint talk_time;
    _netIn >> msg_len;
    _netIn >> is_npc;
    _netIn >> talk_id;
    _netIn >> count_answ;

    CHECK_IN_BUFF_ERROR();

    if (count_answ == 0u) {
        // End dialog
        if (IsScreenPresent(SCREEN_DIALOG)) {
            HideScreen(SCREEN_DIALOG);
        }
        return;
    }

    // Text params
    string lexems;
    _netIn >> lexems;

    CHECK_IN_BUFF_ERROR();

    // Find critter
    auto* npc = (is_npc != 0u ? GetCritter(talk_id) : nullptr);
    _dlgIsNpc = is_npc;
    _dlgNpcId = talk_id;

    // Main text
    _netIn >> text_id;

    // Answers
    vector<uint> answers_texts;
    uint answ_text_id = 0;
    for (auto i = 0; i < count_answ; i++) {
        _netIn >> answ_text_id;
        answers_texts.push_back(answ_text_id);
    }

    CHECK_IN_BUFF_ERROR();

    auto str = _curLang.Msg[TEXTMSG_DLG].GetStr(text_id);
    FormatTags(str, _chosen, npc, lexems);
    auto text_to_script = str;
    vector<string> answers_to_script;
    for (auto answers_text : answers_texts) {
        str = _curLang.Msg[TEXTMSG_DLG].GetStr(answers_text);
        FormatTags(str, _chosen, npc, lexems);
        answers_to_script.push_back(str);
    }

    _netIn >> talk_time;

    CHECK_IN_BUFF_ERROR();

    /*CScriptDictionary* dict = CScriptDictionary::Create(ScriptSys.GetEngine());
    dict->Set("TalkerIsNpc", &is_npc, asTYPEID_BOOL);
    dict->Set("TalkerId", &talk_id, asTYPEID_UINT32);
    dict->Set("Text", &text_to_script, ScriptSys.GetEngine()->GetTypeIdByDecl("string"));
    dict->Set("Answers", &answers_to_script, ScriptSys.GetEngine()->GetTypeIdByDecl("string[]@"));
    dict->Set("TalkTime", &talk_time, asTYPEID_UINT32);
    ShowScreen(SCREEN__DIALOG, dict);
    answers_to_script->Release();
    dict->Release();*/
}

void FOClient::Net_OnGameInfo()
{
    ushort year;
    ushort month;
    ushort day;
    ushort hour;
    ushort minute;
    ushort second;
    ushort multiplier;
    int time;
    uchar rain;
    bool no_log_out;
    _netIn >> year;
    _netIn >> month;
    _netIn >> day;
    _netIn >> hour;
    _netIn >> minute;
    _netIn >> second;
    _netIn >> multiplier;
    _netIn >> time;
    _netIn >> rain;
    _netIn >> no_log_out;

    CHECK_IN_BUFF_ERROR();

    auto* day_time = HexMngr.GetMapDayTime();
    auto* day_color = HexMngr.GetMapDayColor();
    _netIn.Pop(day_time, sizeof(int) * 4);
    _netIn.Pop(day_color, sizeof(uchar) * 12);

    CHECK_IN_BUFF_ERROR();

    GameTime.Reset(year, month, day, hour, minute, second, multiplier);

    SetYear(year);
    SetMonth(month);
    SetDay(day);
    SetHour(hour);
    SetMinute(minute);
    SetSecond(second);
    SetTimeMultiplier(multiplier);

    HexMngr.SetWeather(time, rain);
    SetDayTime(true);
    _noLogOut = no_log_out;
}

void FOClient::Net_OnLoadMap()
{
    WriteLog("Change map...\n");

    uint msg_len;
    uchar map_index_in_loc;
    int map_time;
    uchar map_rain;
    uint hash_tiles;
    uint hash_scen;
    _netIn >> msg_len;
    const auto map_pid = _netIn.ReadHashedString(*this);
    const auto loc_pid = _netIn.ReadHashedString(*this);
    _netIn >> map_index_in_loc;
    _netIn >> map_time;
    _netIn >> map_rain;
    _netIn >> hash_tiles;
    _netIn >> hash_scen;

    CHECK_IN_BUFF_ERROR();

    if (map_pid) {
        NET_READ_PROPERTIES(_netIn, _tempPropertiesData);
        NET_READ_PROPERTIES(_netIn, _tempPropertiesDataExt);
    }

    CHECK_IN_BUFF_ERROR();

    if (_curMap != nullptr) {
        _curMap->MarkAsDestroyed();
        _curMap->Release();
        _curMap = nullptr;
    }

    if (_curLocation != nullptr) {
        _curLocation->MarkAsDestroyed();
        _curLocation->Release();
        _curLocation = nullptr;
    }

    if (map_pid) {
        _curLocation = new LocationView(this, 0, ProtoMngr->GetProtoLocation(loc_pid));
        _curLocation->RestoreData(_tempPropertiesDataExt);
        _curMap = new MapView(this, 0, ProtoMngr->GetProtoMap(map_pid));
        _curMap->RestoreData(_tempPropertiesData);
    }

    Settings.SpritesZoom = 1.0f;

    CurMapPid = map_pid;
    _curMapLocPid = loc_pid;
    _curMapIndexInLoc = map_index_in_loc;
    GameMapTexts.clear();
    HexMngr.UnloadMap();
    SndMngr.StopSounds();
    ShowMainScreen(SCREEN_WAIT, {});
    DeleteCritters();
    ResMngr.ReinitializeDynamicAtlas();

    if (map_pid) {
        uint hash_tiles_cl;
        uint hash_scen_cl;
        HexMngr.GetMapHash(Cache, map_pid, hash_tiles_cl, hash_scen_cl);

        if (hash_tiles != hash_tiles_cl || hash_scen != hash_scen_cl) {
            WriteLog("Obsolete map data (hashes {}:{}, {}:{}).\n", hash_tiles, hash_tiles_cl, hash_scen, hash_scen_cl);
            Net_SendGiveMap(false, map_pid, 0, hash_tiles_cl, hash_scen_cl);
            return;
        }

        if (!HexMngr.LoadMap(Cache, map_pid)) {
            WriteLog("Map not loaded. Disconnect.\n");
            NetDisconnect();
            return;
        }

        HexMngr.SetWeather(map_time, map_rain);
        SetDayTime(true);
        _lookBorders.clear();
        _shootBorders.clear();

        WriteLog("Local map loaded.\n");
    }
    else {
        GmapNullParams();

        WriteLog("Global map loaded.\n");
    }

    Net_SendLoadMapOk();
}

void FOClient::Net_OnMap()
{
    uint msg_len;
    ushort maxhx;
    ushort maxhy;
    bool send_tiles;
    bool send_scenery;
    _netIn >> msg_len;
    const auto map_pid = _netIn.ReadHashedString(*this);
    _netIn >> maxhx;
    _netIn >> maxhy;
    _netIn >> send_tiles;
    _netIn >> send_scenery;

    CHECK_IN_BUFF_ERROR();

    WriteLog("Map {} received...\n", map_pid);

    const string map_name = _str("{}.map", map_pid);
    auto tiles = false;
    char* tiles_data = nullptr;
    uint tiles_len = 0;
    auto scen = false;
    char* scen_data = nullptr;
    uint scen_len = 0;

    if (send_tiles) {
        _netIn >> tiles_len;
        if (tiles_len != 0u) {
            tiles_data = new char[tiles_len];
            _netIn.Pop(tiles_data, tiles_len);
        }
        tiles = true;
    }

    CHECK_IN_BUFF_ERROR();

    if (send_scenery) {
        _netIn >> scen_len;
        if (scen_len != 0u) {
            scen_data = new char[scen_len];
            _netIn.Pop(scen_data, scen_len);
        }
        scen = true;
    }

    CHECK_IN_BUFF_ERROR();

    auto cache_len = 0u;
    auto* cache = Cache.GetRawData(map_name, cache_len);
    if (cache != nullptr) {
        const auto compressed_file = File(cache, cache_len);
        auto buf_len = compressed_file.GetFsize();
        auto* buf = Compressor::Uncompress(compressed_file.GetBuf(), buf_len, 50);
        if (buf != nullptr) {
            auto file = File(buf, buf_len);
            delete[] buf;

            if (file.GetBEUInt() == CLIENT_MAP_FORMAT_VER) {
                file.GoForward(4);
                file.GoForward(2);
                file.GoForward(2);
                const auto old_tiles_len = file.GetBEUInt();
                const auto old_scen_len = file.GetBEUInt();

                if (!tiles) {
                    tiles_len = old_tiles_len;
                    tiles_data = new char[tiles_len];
                    file.CopyMem(tiles_data, tiles_len);
                    tiles = true;
                }

                if (!scen) {
                    scen_len = old_scen_len;
                    scen_data = new char[scen_len];
                    file.CopyMem(scen_data, scen_len);
                    scen = true;
                }
            }
        }
    }
    delete[] cache;

    if (tiles && scen) {
        OutputBuffer output_buf;
        output_buf.SetBEUInt(CLIENT_MAP_FORMAT_VER);
        output_buf.SetBEUInt(map_pid.as_uint());
        output_buf.SetBEUShort(maxhx);
        output_buf.SetBEUShort(maxhy);
        output_buf.SetBEUInt(tiles_len);
        output_buf.SetBEUInt(scen_len);
        output_buf.SetData(tiles_data, tiles_len);
        output_buf.SetData(scen_data, scen_len);

        uint obuf_len = output_buf.GetOutBufLen();
        uchar* buf = Compressor::Compress(output_buf.GetOutBuf(), obuf_len);
        if (!buf) {
            WriteLog("Failed to compress data '{}', disconnect.\n", map_name);
            NetDisconnect();
            return;
        }

        Cache.SetRawData(map_name, buf, obuf_len);
        delete[] buf;
    }
    else {
        WriteLog("Not for all data of map, disconnect.\n");
        NetDisconnect();
        delete[] tiles_data;
        delete[] scen_data;
        return;
    }

    delete[] tiles_data;
    delete[] scen_data;

    WriteLog("Map saved.\n");
}

void FOClient::Net_OnGlobalInfo()
{
    uint msg_len;
    uchar info_flags;
    _netIn >> msg_len;
    _netIn >> info_flags;

    CHECK_IN_BUFF_ERROR();

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        _worldmapLoc.clear();

        ushort count_loc;
        _netIn >> count_loc;

        for (auto i = 0; i < count_loc; i++) {
            GmapLocation loc;
            _netIn >> loc.LocId;
            loc.LocPid = _netIn.ReadHashedString(*this);
            _netIn >> loc.LocWx;
            _netIn >> loc.LocWy;
            _netIn >> loc.Radius;
            _netIn >> loc.Color;
            _netIn >> loc.Entrances;

            if (loc.LocId != 0u) {
                _worldmapLoc.push_back(loc);
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_LOCATION)) {
        bool add;
        GmapLocation loc;
        _netIn >> add;
        _netIn >> loc.LocId;
        loc.LocPid = _netIn.ReadHashedString(*this);
        _netIn >> loc.LocWx;
        _netIn >> loc.LocWy;
        _netIn >> loc.Radius;
        _netIn >> loc.Color;
        _netIn >> loc.Entrances;

        const auto it = std::find_if(_worldmapLoc.begin(), _worldmapLoc.end(), [&loc](const GmapLocation& l) { return loc.LocId == l.LocId; });
        if (add) {
            if (it != _worldmapLoc.end()) {
                *it = loc;
            }
            else {
                _worldmapLoc.push_back(loc);
            }
        }
        else {
            if (it != _worldmapLoc.end()) {
                _worldmapLoc.erase(it);
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        auto* fog_data = _worldmapFog.GetData();
        _netIn.Pop(fog_data, GM_ZONES_FOG_SIZE);
    }

    if (IsBitSet(info_flags, GM_INFO_FOG)) {
        ushort zx;
        ushort zy;
        uchar fog;
        _netIn >> zx;
        _netIn >> zy;
        _netIn >> fog;

        _worldmapFog.Set2Bit(zx, zy, fog);
    }

    if (IsBitSet(info_flags, GM_INFO_CRITTERS)) {
        DeleteCritters();
        // After wait AddCritters
    }

    CHECK_IN_BUFF_ERROR();
}

void FOClient::Net_OnSomeItems()
{
    uint msg_len;
    int param;
    bool is_null;
    uint items_count;
    _netIn >> msg_len;
    _netIn >> param;
    _netIn >> is_null;
    _netIn >> items_count;

    CHECK_IN_BUFF_ERROR();

    vector<ItemView*> item_container;
    for (uint i = 0; i < items_count; i++) {
        uint item_id;
        hstring item_pid;
        _netIn >> item_id;
        item_pid = _netIn.ReadHashedString(*this);
        NET_READ_PROPERTIES(_netIn, _tempPropertiesData);

        const auto* proto_item = ProtoMngr->GetProtoItem(item_pid);
        if (item_id != 0u && proto_item != nullptr) {
            auto* item = new ItemView(this, item_id, proto_item);
            item->RestoreData(_tempPropertiesData);
            item_container.push_back(item);
        }
    }

    CHECK_IN_BUFF_ERROR();

    OnReceiveItems.Fire(item_container, param);
}

void FOClient::Net_OnUpdateFilesList()
{
    uint msg_len;
    bool outdated;
    uint data_size;
    _netIn >> msg_len;
    _netIn >> outdated;
    _netIn >> data_size;

    CHECK_IN_BUFF_ERROR();

    vector<uchar> data;
    data.resize(data_size);
    if (data_size > 0u) {
        _netIn.Pop(data.data(), data_size);
    }

    if (!outdated) {
        NET_READ_PROPERTIES(_netIn, _globalsPropertiesData);
    }

    CHECK_IN_BUFF_ERROR();

    File file(data.data(), data_size);

    _updateData->FileList.clear();
    _updateData->FilesWholeSize = 0;
    _updateData->ClientOutdated = outdated;

#if FO_WINDOWS
    /*bool have_exe = false;
    string exe_name;
    if (outdated)
        exe_name = _str(File::GetExePath()).extractFileName();*/
#endif

    for (uint file_index = 0;; file_index++) {
        auto name_len = file.GetLEShort();
        if (name_len == -1) {
            break;
        }

        RUNTIME_ASSERT(name_len > 0);
        auto name = string(reinterpret_cast<const char*>(file.GetCurBuf()), name_len);
        file.GoForward(name_len);
        auto size = file.GetLEUInt();
        auto hash = file.GetLEUInt();

        // Skip platform depended
#if FO_WINDOWS
        // if (outdated && _str(exe_name).compareIgnoreCase(name))
        //    have_exe = true;
        string ext = _str(name).getFileExtension();
        if (!outdated && (ext == "exe" || ext == "pdb")) {
            continue;
        }
#else
        string ext = _str(name).getFileExtension();
        if (ext == "exe" || ext == "pdb")
            continue;
#endif

        // Check hash
        uint cur_hash_len = 0;
        auto cur_hash = reinterpret_cast<uint*>(Cache.GetRawData(_str("{}.hash", name), cur_hash_len));
        auto cached_hash_same = ((cur_hash != nullptr) && cur_hash_len == sizeof(hash) && *cur_hash == hash);
        if (name[0] != '$') {
            // Real file, human can disturb file consistency, make base recheck
            auto file_header = FileMngr.ReadFileHeader(name);
            if (file_header && file_header.GetFsize() == size) {
                auto file2 = FileMngr.ReadFile(name);
                if (cached_hash_same || (file2 && hash == Hashing::MurmurHash2(file2.GetBuf(), file2.GetFsize()))) {
                    if (!cached_hash_same) {
                        Cache.SetRawData(_str("{}.hash", name), reinterpret_cast<uchar*>(&hash), sizeof(hash));
                    }
                    continue;
                }
            }
        }
        else {
            // In cache, consistency granted
            if (cached_hash_same) {
                continue;
            }
        }

        // Get this file
        ClientUpdate::UpdateFile update_file;
        update_file.Index = file_index;
        update_file.Name = name;
        update_file.Size = size;
        update_file.RemaningSize = size;
        update_file.Hash = hash;
        _updateData->FileList.push_back(update_file);
        _updateData->FilesWholeSize += size;
    }

#if FO_WINDOWS
    // if (_updateData->ClientOutdated && !have_exe)
    //    UpdateFilesAbort(STR_CLIENT_OUTDATED, "Client outdated!");
#else
    if (_updateData->ClientOutdated)
        UpdateFilesAbort(STR_CLIENT_OUTDATED, "Client outdated!");
#endif
}

void FOClient::Net_OnUpdateFileData()
{
    // Get portion
    uchar data[FILE_UPDATE_PORTION];
    _netIn.Pop(data, sizeof(data));

    CHECK_IN_BUFF_ERROR();

    auto& update_file = _updateData->FileList.front();

    // Write data to temp file
    _updateData->TempFile->SetData(data, std::min(update_file.RemaningSize, static_cast<uint>(sizeof(data))));
    _updateData->TempFile->Save();

    // Get next portion or finalize data
    update_file.RemaningSize -= std::min(update_file.RemaningSize, static_cast<uint>(sizeof(data)));
    if (update_file.RemaningSize > 0) {
        // Request next portion
        _netOut << NETMSG_GET_UPDATE_FILE_DATA;
    }
    else {
        // Finalize received data
        _updateData->TempFile->Save();
        _updateData->TempFile = nullptr;

        // Cache
        if (update_file.Name[0] == '$') {
            const auto temp_file = FileMngr.ReadFile("Update.fobin");
            if (!temp_file) {
                UpdateFilesAbort(STR_FILESYSTEM_ERROR, "Can't load update file!");
                return;
            }

            Cache.SetRawData(update_file.Name, temp_file.GetBuf(), temp_file.GetFsize());
            Cache.SetRawData(update_file.Name + ".hash", reinterpret_cast<uchar*>(&update_file.Hash), sizeof(update_file.Hash));
            FileMngr.DeleteFile("Update.fobin");
        }
        // File
        else {
            FileMngr.DeleteFile(update_file.Name);
            FileMngr.RenameFile("Update.fobin", update_file.Name);
        }

        _updateData->FileList.erase(_updateData->FileList.begin());
        _updateData->FileDownloading = false;
    }
}

void FOClient::Net_OnAutomapsInfo()
{
    uint msg_len;
    bool clear;
    _netIn >> msg_len;
    _netIn >> clear;

    if (clear) {
        _automaps.clear();
    }

    ushort locs_count;
    _netIn >> locs_count;

    CHECK_IN_BUFF_ERROR();

    for (ushort i = 0; i < locs_count; i++) {
        uint loc_id;
        hstring loc_pid;
        ushort maps_count;
        _netIn >> loc_id;
        loc_pid = _netIn.ReadHashedString(*this);
        _netIn >> maps_count;

        auto it = std::find_if(_automaps.begin(), _automaps.end(), [&loc_id](const Automap& m) { return loc_id == m.LocId; });

        // Delete from collection
        if (maps_count == 0u) {
            if (it != _automaps.end()) {
                _automaps.erase(it);
            }
        }
        // Add or modify
        else {
            Automap amap;
            amap.LocId = loc_id;
            amap.LocPid = loc_pid;
            amap.LocName = _curLang.Msg[TEXTMSG_LOCATIONS].GetStr(STR_LOC_NAME(loc_pid.as_uint()));

            for (ushort j = 0; j < maps_count; j++) {
                hstring map_pid;
                uchar map_index_in_loc;
                map_pid = _netIn.ReadHashedString(*this);
                _netIn >> map_index_in_loc;

                amap.MapPids.push_back(map_pid);
                amap.MapNames.push_back(_curLang.Msg[TEXTMSG_LOCATIONS].GetStr(STR_LOC_MAP_NAME(loc_pid.as_uint(), map_index_in_loc)));
            }

            if (it != _automaps.end()) {
                *it = amap;
            }
            else {
                _automaps.push_back(amap);
            }
        }
    }

    CHECK_IN_BUFF_ERROR();
}

void FOClient::Net_OnViewMap()
{
    ushort hx;
    ushort hy;
    uint loc_id;
    uint loc_ent;
    _netIn >> hx;
    _netIn >> hy;
    _netIn >> loc_id;
    _netIn >> loc_ent;

    CHECK_IN_BUFF_ERROR();

    if (!HexMngr.IsMapLoaded()) {
        return;
    }

    HexMngr.FindSetCenter(hx, hy);
    ShowMainScreen(SCREEN_GAME, {});
    ScreenFadeOut();
    HexMngr.RebuildLight();

    /*CScriptDictionary* dict = CScriptDictionary::Create(ScriptSys.GetEngine());
    dict->Set("LocationId", &loc_id, asTYPEID_UINT32);
    dict->Set("LocationEntrance", &loc_ent, asTYPEID_UINT32);
    ShowScreen(SCREEN__TOWN_VIEW, dict);
    dict->Release();*/
}

void FOClient::SetGameColor(uint color)
{
    SprMngr.SetSpritesColor(color);

    if (HexMngr.IsMapLoaded()) {
        HexMngr.RefreshMap();
    }
}

void FOClient::SetDayTime(bool refresh)
{
    if (refresh) {
        _prevDayTimeColor.reset();
    }

    if (!HexMngr.IsMapLoaded()) {
        return;
    }

    auto color = GenericUtils::GetColorDay(HexMngr.GetMapDayTime(), HexMngr.GetMapDayColor(), HexMngr.GetMapTime(), nullptr);
    color = COLOR_GAME_RGB(static_cast<int>((color >> 16) & 0xFF), static_cast<int>((color >> 8) & 0xFF), static_cast<int>(color & 0xFF));

    if (!_prevDayTimeColor.has_value() || _prevDayTimeColor != color) {
        _prevDayTimeColor = color;
        SetGameColor(color);
    }
}

void FOClient::WaitPing()
{
    Settings.WaitPing = true;

    Net_SendPing(PING_WAIT);
}

void FOClient::CrittersProcess()
{
    auto need_delete = false;
    for (auto& [id, cr] : HexMngr.GetCritters()) {
        cr->Process();

        if (cr->IsNeedReset()) {
            HexMngr.RemoveCritter(cr);
            HexMngr.SetCritter(cr);
            cr->ResetOk();
        }

        if (!cr->IsChosen() && cr->IsNeedMove() && HexMngr.TransitCritter(cr, cr->MoveSteps[0].first, cr->MoveSteps[0].second, true, cr->CurMoveStep > 0)) {
            cr->MoveSteps.erase(cr->MoveSteps.begin());
            cr->CurMoveStep--;
        }

        if (cr->IsFinish()) {
            need_delete = true;
        }
    }

    if (need_delete) {
        const auto critters = HexMngr.GetCritters(); // Make copy
        for (auto& [id, cr] : critters) {
            if (cr->IsFinish()) {
                DeleteCritter(cr->GetId());
            }
        }
    }
}

void FOClient::TryExit()
{
    const auto active = GetActiveScreen(nullptr);
    if (active != SCREEN_NONE) {
        switch (active) {
        case SCREEN_TOWN_VIEW:
            Net_SendRefereshMe();
            break;
        default:
            HideScreen(SCREEN_NONE);
            break;
        }
    }
    else {
        switch (GetMainScreen()) {
        case SCREEN_LOGIN:
            Settings.Quit = true;
            break;
        case SCREEN_WAIT:
            NetDisconnect();
            break;
        default:
            break;
        }
    }
}

void FOClient::FlashGameWindow()
{
    if (SprMngr.IsWindowFocused()) {
        return;
    }

    if (Settings.WinNotify) {
        SprMngr.BlinkWindow();
    }

#if FO_WINDOWS
    if (Settings.SoundNotify) {
        ::Beep(100, 200);
    }
#endif
}

auto FOClient::AnimLoad(hstring name, AtlasType res_type) -> uint
{
    auto* anim = ResMngr.GetAnim(name, res_type);
    if (anim == nullptr) {
        return 0u;
    }

    auto* ianim = new IfaceAnim {anim, res_type, GameTime.GameTick()};

    size_t index = 1;
    for (; index < _ifaceAnimations.size(); index++) {
        if (_ifaceAnimations[index] == nullptr) {
            break;
        }
    }

    if (index < _ifaceAnimations.size()) {
        _ifaceAnimations[index] = ianim;
    }
    else {
        _ifaceAnimations.push_back(ianim);
    }

    return static_cast<uint>(index);
}

auto FOClient::AnimGetCurSpr(uint anim_id) -> uint
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->Frames->Ind[_ifaceAnimations[anim_id]->CurSpr];
}

auto FOClient::AnimGetCurSprCnt(uint anim_id) -> uint
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->CurSpr;
}

auto FOClient::AnimGetSprCount(uint anim_id) -> uint
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->Frames->CntFrm;
}

auto FOClient::AnimGetFrames(uint anim_id) -> AnyFrames*
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->Frames;
}

void FOClient::AnimRun(uint anim_id, uint flags)
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return;
    }

    auto* anim = _ifaceAnimations[anim_id];

    // Set flags
    anim->Flags = flags & 0xFFFF;
    flags >>= 16;

    // Set frm
    uchar cur_frm = (flags & 0xFF);
    if (cur_frm > 0u) {
        cur_frm--;
        if (cur_frm >= anim->Frames->CntFrm) {
            if (anim->Frames->CntFrm > 0u) {
                cur_frm = static_cast<uchar>(anim->Frames->CntFrm - 1u);
            }
            else {
                cur_frm = 0u;
            }
        }
        anim->CurSpr = cur_frm;
    }
}

void FOClient::AnimProcess()
{
    const auto cur_tick = GameTime.GameTick();
    for (auto* anim : _ifaceAnimations) {
        if (anim == nullptr || anim->Flags == 0u) {
            continue;
        }

        if (IsBitSet(anim->Flags, ANIMRUN_STOP)) {
            anim->Flags = 0;
            continue;
        }

        if (IsBitSet(anim->Flags, ANIMRUN_TO_END) || IsBitSet(anim->Flags, ANIMRUN_FROM_END)) {
            if (cur_tick - anim->LastTick < anim->Frames->Ticks / anim->Frames->CntFrm) {
                continue;
            }

            anim->LastTick = cur_tick;
            auto end_spr = anim->Frames->CntFrm - 1;
            if (IsBitSet(anim->Flags, ANIMRUN_FROM_END)) {
                end_spr = 0;
            }

            if (anim->CurSpr < end_spr) {
                anim->CurSpr++;
            }
            else if (anim->CurSpr > end_spr) {
                anim->CurSpr--;
            }
            else {
                if (IsBitSet(anim->Flags, ANIMRUN_CYCLE)) {
                    if (IsBitSet(anim->Flags, ANIMRUN_TO_END)) {
                        anim->CurSpr = 0;
                    }
                    else {
                        anim->CurSpr = end_spr;
                    }
                }
                else {
                    anim->Flags = 0;
                }
            }
        }
    }
}

void FOClient::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    UNUSED_VARIABLE(entity);
    RUNTIME_ASSERT(entity == this);

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Game, prop, this);
    }
    else {
        throw GenericException("Unable to send global modifiable property", prop->GetName());
    }
}

void FOClient::OnSendPlayerValue(Entity* entity, const Property* prop)
{
    UNUSED_VARIABLE(entity);
    RUNTIME_ASSERT(entity == _curPlayer);

    Net_SendProperty(NetProperty::Player, prop, _curPlayer);
}

void FOClient::OnSendCritterValue(Entity* entity, const Property* prop)
{
    auto* cr = dynamic_cast<CritterView*>(entity);
    if (cr->IsChosen()) {
        Net_SendProperty(NetProperty::Chosen, prop, cr);
    }
    else if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Critter, prop, cr);
    }
    else {
        throw GenericException("Unable to send critter modifiable property", prop->GetName());
    }
}

void FOClient::OnSetCritterModelName(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(cur_value);
    UNUSED_VARIABLE(old_value);

    auto* cr = dynamic_cast<CritterView*>(entity);
    cr->RefreshModel();
    cr->Action(ACTION_REFRESH, 0, nullptr, false);
}

void FOClient::OnSetCritterContourColor(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(old_value);

    auto* cr = dynamic_cast<CritterView*>(entity);
    if (cr->SprDrawValid) {
        cr->SprDraw->SetContour(cr->SprDraw->ContourType, *static_cast<uint*>(cur_value));
    }
}

void FOClient::OnSendItemValue(Entity* entity, const Property* prop)
{
    if (auto* item = dynamic_cast<ItemView*>(entity); item != nullptr && item->GetId() != 0u) {
        if (item->GetOwnership() == ItemOwnership::CritterInventory) {
            auto* cr = GetCritter(item->GetCritId());
            if (cr != nullptr && cr->IsChosen()) {
                Net_SendProperty(NetProperty::ChosenItem, prop, item);
            }
            else if (cr != nullptr && prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
                Net_SendProperty(NetProperty::CritterItem, prop, item);
            }
            else {
                throw GenericException("Unable to send item (a critter) modifiable property", prop->GetName());
            }
        }
        else if (item->GetOwnership() == ItemOwnership::MapHex) {
            if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
                Net_SendProperty(NetProperty::MapItem, prop, item);
            }
            else {
                throw GenericException("Unable to send item (a map) modifiable property", prop->GetName());
            }
        }
        else {
            throw GenericException("Unable to send item (a container) modifiable property", prop->GetName());
        }
    }
}

void FOClient::OnSetItemFlags(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    // IsColorize, IsBadItem, IsShootThru, IsLightThru, IsNoBlock

    UNUSED_VARIABLE(cur_value);
    UNUSED_VARIABLE(old_value);

    auto* item = dynamic_cast<ItemView*>(entity);
    if (item->GetOwnership() == ItemOwnership::MapHex && HexMngr.IsMapLoaded()) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
        auto rebuild_cache = false;
        if (prop == hex_item->GetPropertyIsColorize()) {
            hex_item->RefreshAlpha();
        }
        else if (prop == hex_item->GetPropertyIsBadItem()) {
            hex_item->SetSprite(nullptr);
        }
        else if (prop == hex_item->GetPropertyIsShootThru()) {
            _rebuildLookBordersRequest = true;
            rebuild_cache = true;
        }
        else if (prop == hex_item->GetPropertyIsLightThru()) {
            HexMngr.RebuildLight();
            rebuild_cache = true;
        }
        else if (prop == hex_item->GetPropertyIsNoBlock()) {
            rebuild_cache = true;
        }
        if (rebuild_cache) {
            HexMngr.GetField(hex_item->GetHexX(), hex_item->GetHexY()).ProcessCache();
        }
    }
}

void FOClient::OnSetItemSomeLight(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(cur_value);
    UNUSED_VARIABLE(old_value);

    if (HexMngr.IsMapLoaded()) {
        HexMngr.RebuildLight();
    }
}

void FOClient::OnSetItemPicMap(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(cur_value);
    UNUSED_VARIABLE(old_value);

    auto* item = dynamic_cast<ItemView*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->RefreshAnim();
    }
}

void FOClient::OnSetItemOffsetXY(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    // OffsetX, OffsetY

    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(cur_value);
    UNUSED_VARIABLE(old_value);

    auto* item = dynamic_cast<ItemView*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex && HexMngr.IsMapLoaded()) {
        auto hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->SetAnimOffs();
        HexMngr.ProcessHexBorders(hex_item);
    }
}

void FOClient::OnSetItemOpened(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    UNUSED_VARIABLE(prop);

    auto* item = dynamic_cast<ItemView*>(entity);
    const auto cur = *static_cast<bool*>(cur_value);
    const auto old = *static_cast<bool*>(old_value);

    if (item->GetIsCanOpen()) {
        auto hex_item = dynamic_cast<ItemHexView*>(item);
        if (!old && cur) {
            hex_item->SetAnimFromStart();
        }
        if (old && !cur) {
            hex_item->SetAnimFromEnd();
        }
    }
}

void FOClient::OnSendMapValue(Entity* entity, const Property* prop)
{
    UNUSED_VARIABLE(entity);
    RUNTIME_ASSERT(entity == _curMap);

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Map, prop, _curMap);
    }
    else {
        throw GenericException("Unable to send map modifiable property", prop->GetName());
    }
}

void FOClient::OnSendLocationValue(Entity* entity, const Property* prop)
{
    UNUSED_VARIABLE(entity);
    RUNTIME_ASSERT(entity == _curLocation);

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Location, prop, _curLocation);
    }
    else {
        throw GenericException("Unable to send location modifiable property", prop->GetName());
    }
}

void FOClient::GameDraw()
{
    // Move cursor
    if (Settings.ShowMoveCursor) {
        HexMngr.SetCursorPos(Settings.MouseX, Settings.MouseY, Keyb.CtrlDwn, false);
    }

    // Look borders
    if (_rebuildLookBordersRequest) {
        LookBordersPrepare();
        _rebuildLookBordersRequest = false;
    }

    // Map
    HexMngr.DrawMap();

    // Text on head
    for (auto& [id, cr] : HexMngr.GetCritters()) {
        cr->DrawTextOnHead();
    }

    // Texts on map
    const auto tick = GameTime.GameTick();
    for (auto it = GameMapTexts.begin(); it != GameMapTexts.end();) {
        auto& mt = *it;
        if (tick < mt.StartTick + mt.Tick) {
            const auto dt = tick - mt.StartTick;
            const auto procent = GenericUtils::Percent(mt.Tick, dt);
            const auto r = mt.Pos.Interpolate(mt.EndPos, procent);
            auto& f = HexMngr.GetField(mt.HexX, mt.HexY);
            const auto half_hex_width = Settings.MapHexWidth / 2;
            const auto half_hex_height = Settings.MapHexHeight / 2;
            const auto x = static_cast<int>(static_cast<float>(f.ScrX + half_hex_width + Settings.ScrOx) / Settings.SpritesZoom - 100.0f - static_cast<float>(mt.Pos.Left - r.Left));
            const auto y = static_cast<int>(static_cast<float>(f.ScrY + half_hex_height - mt.Pos.Height() - (mt.Pos.Top - r.Top) + Settings.ScrOy) / Settings.SpritesZoom - 70.0f);

            auto color = mt.Color;
            if (mt.Fade) {
                color = (color ^ 0xFF000000) | ((0xFF * (100 - procent) / 100) << 24);
            }
            else if (mt.Tick > 500) {
                const auto hide = mt.Tick - 200;
                if (dt >= hide) {
                    const auto alpha = 255u * (100u - GenericUtils::Percent(mt.Tick - hide, dt - hide)) / 100u;
                    color = (alpha << 24) | (color & 0xFFFFFF);
                }
            }

            SprMngr.DrawStr(IRect(x, y, x + 200, y + 70), mt.Text, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, FONT_DEFAULT);

            ++it;
        }
        else {
            it = GameMapTexts.erase(it);
        }
    }
}

void FOClient::AddMess(uchar mess_type, string_view msg)
{
    OnMessageBox.Fire(string(msg), mess_type, false);
}

void FOClient::AddMess(uchar mess_type, string_view msg, bool script_call)
{
    OnMessageBox.Fire(string(msg), mess_type, script_call);
}

// Todo: move targs formatting to scripts
void FOClient::FormatTags(string& text, CritterView* cr, CritterView* npc, string_view lexems)
{
    if (text == "error") {
        text = "Text not found!";
        return;
    }

    vector<string> dialogs;
    auto sex = 0;
    auto sex_tags = false;
    string tag;
    tag[0] = 0;

    for (size_t i = 0; i < text.length();) {
        switch (text[i]) {
        case '#': {
            text[i] = '\n';
        } break;
        case '|': {
            if (sex_tags) {
                tag = _str(text.substr(i + 1)).substringUntil('|');
                text.erase(i, tag.length() + 2);

                if (sex != 0) {
                    if (sex == 1) {
                        text.insert(i, tag);
                    }
                    sex--;
                }
                continue;
            }
        } break;
        case '@': {
            if (text[i + 1] == '@') {
                dialogs.push_back(text.substr(0, i));
                text.erase(0, i + 2);
                i = 0;
                continue;
            }

            tag = _str(text.substr(i + 1)).substringUntil('@');
            text.erase(i, tag.length() + 2);
            if (tag.empty()) {
                break;
            }

            // Player name
            if (_str(tag).compareIgnoreCase("pname")) {
                tag = (cr != nullptr ? cr->GetName() : "");
            }
            // Npc name
            else if (_str(tag).compareIgnoreCase("nname")) {
                tag = (npc != nullptr ? npc->GetName() : "");
            }
            // Sex
            else if (_str(tag).compareIgnoreCase("sex")) {
                // sex = (cr != nullptr ? cr->GetGender() + 1 : 1);
                sex_tags = true;
                continue;
            }
            // Random
            else if (_str(tag).compareIgnoreCase("rnd")) {
                auto first = text.find_first_of('|', i);
                auto last = text.find_last_of('|', i);
                auto rnd = _str(text.substr(first, last - first + 1)).split('|');
                text.erase(first, last - first + 1);
                if (!rnd.empty()) {
                    text.insert(first, rnd[GenericUtils::Random(0, static_cast<int>(rnd.size()) - 1)]);
                }
            }
            // Lexems
            else if (tag.length() > 4 && tag[0] == 'l' && tag[1] == 'e' && tag[2] == 'x' && tag[3] == ' ') {
                auto lex = "$" + tag.substr(4);
                auto pos = lexems.find(lex);
                if (pos != string::npos) {
                    pos += lex.length();
                    tag = _str(lexems.substr(pos)).substringUntil('$').trim();
                }
                else {
                    tag = "<lexem not found>";
                }
            }
            // Msg text
            else if (tag.length() > 4 && tag[0] == 'm' && tag[1] == 's' && tag[2] == 'g' && tag[3] == ' ') {
                tag = tag.substr(4);
                tag = _str(tag).erase('(').erase(')');
                istringstream itag(tag);
                string msg_type_name;
                uint str_num = 0;
                if (itag >> msg_type_name >> str_num) {
                    auto msg_type = FOMsg::GetMsgType(msg_type_name);
                    if (msg_type < 0 || msg_type >= TEXTMSG_COUNT) {
                        tag = "<msg tag, unknown type>";
                    }
                    else if (_curLang.Msg[msg_type].Count(str_num) == 0u) {
                        tag = _str("<msg tag, string {} not found>", str_num);
                    }
                    else {
                        tag = _curLang.Msg[msg_type].GetStr(str_num);
                    }
                }
                else {
                    tag = "<msg tag parse fail>";
                }
            }
            // Script
            else if (tag.length() > 7 && tag[0] == 's' && tag[1] == 'c' && tag[2] == 'r' && tag[3] == 'i' && tag[4] == 'p' && tag[5] == 't' && tag[6] == ' ') {
                string func_name = _str(tag.substr(7)).substringUntil('$');
                if (!ScriptSys->CallFunc<string, string>(func_name, string(lexems), tag)) {
                    tag = "<script function not found>";
                }
            }
            // Error
            else {
                tag = "<error>";
            }

            text.insert(i, tag);
        }
            continue;
        default:
            break;
        }

        ++i;
    }

    dialogs.push_back(text);
    text = dialogs[GenericUtils::Random(0u, static_cast<uint>(dialogs.size()) - 1u)];
}

void FOClient::ShowMainScreen(int new_screen, map<string, string> params)
{
    while (GetActiveScreen(nullptr) != SCREEN_NONE) {
        HideScreen(SCREEN_NONE);
    }

    if (_screenModeMain == new_screen) {
        return;
    }
    if (_isAutoLogin && new_screen == SCREEN_LOGIN) {
        return;
    }

    const auto prev_main_screen = _screenModeMain;
    if (_screenModeMain != 0) {
        RunScreenScript(false, _screenModeMain, {});
    }

    _screenModeMain = new_screen;
    RunScreenScript(true, new_screen, std::move(params));

    switch (GetMainScreen()) {
    case SCREEN_LOGIN:
        ScreenFadeOut();
        break;
    case SCREEN_GAME:
        break;
    case SCREEN_GLOBAL_MAP:
        break;
    case SCREEN_WAIT:
        if (prev_main_screen != SCREEN_WAIT) {
            _screenEffects.clear();
            _waitPic = ResMngr.GetRandomSplash();
        }
        break;
    default:
        break;
    }
}

auto FOClient::GetActiveScreen(vector<int>* screens) -> int
{
    vector<int> active_screens;
    OnGetActiveScreens.Fire(active_screens);

    if (screens != nullptr) {
        *screens = active_screens;
    }

    auto active = (!active_screens.empty() ? active_screens.back() : SCREEN_NONE);
    if (active >= SCREEN_LOGIN && active <= SCREEN_WAIT) {
        active = SCREEN_NONE;
    }
    return active;
}

auto FOClient::IsScreenPresent(int screen) -> bool
{
    vector<int> active_screens;
    GetActiveScreen(&active_screens);
    return std::find(active_screens.begin(), active_screens.end(), screen) != active_screens.end();
}

void FOClient::ShowScreen(int screen, map<string, string> params)
{
    RunScreenScript(true, screen, std::move(params));
}

void FOClient::HideScreen(int screen)
{
    if (screen == SCREEN_NONE) {
        screen = GetActiveScreen(nullptr);
    }
    if (screen == SCREEN_NONE) {
        return;
    }

    RunScreenScript(false, screen, {});
}

void FOClient::RunScreenScript(bool show, int screen, map<string, string> params)
{
    OnScreenChange.Fire(show, screen, std::move(params));
}

void FOClient::LmapPrepareMap()
{
    _lmapPrepPix.clear();

    if (_chosen == nullptr) {
        return;
    }

    const auto maxpixx = (_lmapWMap[2] - _lmapWMap[0]) / 2 / _lmapZoom;
    const auto maxpixy = (_lmapWMap[3] - _lmapWMap[1]) / 2 / _lmapZoom;
    const auto bx = _chosen->GetHexX() - maxpixx;
    const auto by = _chosen->GetHexY() - maxpixy;
    const auto ex = _chosen->GetHexX() + maxpixx;
    const auto ey = _chosen->GetHexY() + maxpixy;

    const auto vis = _chosen->GetLookDistance();
    auto pix_x = _lmapWMap[2] - _lmapWMap[0];
    auto pix_y = 0;

    for (auto i1 = bx; i1 < ex; i1++) {
        for (auto i2 = by; i2 < ey; i2++) {
            pix_y += _lmapZoom;
            if (i1 < 0 || i2 < 0 || i1 >= HexMngr.GetWidth() || i2 >= HexMngr.GetHeight()) {
                continue;
            }

            auto is_far = false;
            const auto dist = GeomHelper.DistGame(_chosen->GetHexX(), _chosen->GetHexY(), i1, i2);
            if (dist > vis) {
                is_far = true;
            }

            auto& f = HexMngr.GetField(static_cast<ushort>(i1), static_cast<ushort>(i2));
            uint cur_color;

            if (f.Crit != nullptr) {
                cur_color = (f.Crit == _chosen ? 0xFF0000FF : 0xFFFF0000);
                _lmapPrepPix.push_back({_lmapWMap[0] + pix_x + (_lmapZoom - 1), _lmapWMap[1] + pix_y, cur_color});
                _lmapPrepPix.push_back({_lmapWMap[0] + pix_x, _lmapWMap[1] + pix_y + ((_lmapZoom - 1) / 2), cur_color});
            }
            else if (f.Flags.IsWall || f.Flags.IsScen) {
                if (f.Flags.ScrollBlock) {
                    continue;
                }
                if (!_lmapSwitchHi && !f.Flags.IsWall) {
                    continue;
                }
                cur_color = (f.Flags.IsWall ? 0xFF00FF00 : 0x7F00FF00);
            }
            else {
                continue;
            }

            if (is_far) {
                cur_color = COLOR_CHANGE_ALPHA(cur_color, 0x22);
            }

            _lmapPrepPix.push_back({_lmapWMap[0] + pix_x, _lmapWMap[1] + pix_y, cur_color});
            _lmapPrepPix.push_back({_lmapWMap[0] + pix_x + (_lmapZoom - 1), _lmapWMap[1] + pix_y + ((_lmapZoom - 1) / 2), cur_color});
        }

        pix_x -= _lmapZoom;
        pix_y = 0;
    }

    _lmapPrepareNextTick = GameTime.FrameTick() + MINIMAP_PREPARE_TICK;
}

void FOClient::GmapNullParams()
{
    _worldmapLoc.clear();
    _worldmapFog.Fill(0);
    DeleteCritters();
}

void FOClient::WaitDraw()
{
    SprMngr.DrawSpriteSize(_waitPic, 0, 0, Settings.ScreenWidth, Settings.ScreenHeight, true, true, 0);
    SprMngr.Flush();
}

auto FOClient::CustomCall(string_view command, string_view separator) -> string
{
    // Parse command
    vector<string> args;
    const auto command_str = string(command);
    std::stringstream ss(command_str);
    if (separator.length() > 0) {
        string arg;
        const auto sep = *separator.data();
        while (std::getline(ss, arg, sep)) {
            args.push_back(arg);
        }
    }
    else {
        args.push_back(string(command));
    }
    if (args.empty()) {
        throw ScriptException("Empty custom call command");
    }

    // Execute
    const auto cmd = args[0];
    if (cmd == "Login" && args.size() >= 3) {
        if (_initNetReason == INIT_NET_REASON_NONE) {
            _loginName = args[1];
            _loginPassword = args[2];
            _initNetReason = INIT_NET_REASON_LOGIN;
        }
    }
    else if (cmd == "Register" && args.size() >= 3) {
        if (_initNetReason == INIT_NET_REASON_NONE) {
            _loginName = args[1];
            _loginPassword = args[2];
            _initNetReason = INIT_NET_REASON_REG;
        }
    }
    else if (cmd == "CustomConnect") {
        if (_initNetReason == INIT_NET_REASON_NONE) {
            _initNetReason = INIT_NET_REASON_CUSTOM;
            if (!_updateData) {
                _updateData = FOClient::ClientUpdate();
            }
        }
    }
    else if (cmd == "DumpAtlases") {
        SprMngr.DumpAtlases();
    }
    else if (cmd == "SwitchShowTrack") {
        HexMngr.SwitchShowTrack();
    }
    else if (cmd == "SwitchShowHex") {
        HexMngr.SwitchShowHex();
    }
    else if (cmd == "SwitchFullscreen") {
        if (!Settings.FullScreen) {
            if (SprMngr.EnableFullscreen()) {
                Settings.FullScreen = true;
            }
        }
        else {
            if (SprMngr.DisableFullscreen()) {
                Settings.FullScreen = false;

                if (_windowResolutionDiffX || _windowResolutionDiffY) {
                    const auto [x, y] = SprMngr.GetWindowPosition();
                    SprMngr.SetWindowPosition(x - _windowResolutionDiffX, y - _windowResolutionDiffY);
                    _windowResolutionDiffX = _windowResolutionDiffY = 0;
                }
            }
        }
    }
    else if (cmd == "MinimizeWindow") {
        SprMngr.MinimizeWindow();
    }
    else if (cmd == "SwitchLookBorders") {
        // _drawLookBorders = !_drawLookBorders;
        // _rebuildLookBordersRequest = true;
    }
    else if (cmd == "SwitchShootBorders") {
        // _drawShootBorders = !_drawShootBorders;
        // _rebuildLookBordersRequest = true;
    }
    else if (cmd == "GetShootBorders") {
        return _drawShootBorders ? "true" : "false";
    }
    else if (cmd == "SetShootBorders" && args.size() >= 2) {
        auto set = (args[1] == "true");
        if (_drawShootBorders != set) {
            _drawShootBorders = set;
            _rebuildLookBordersRequest = true;
        }
    }
    else if (cmd == "SetMousePos" && args.size() == 4) {
#if !FO_WEB
        /*int x = _str(args[1]).toInt();
        int y = _str(args[2]).toInt();
        bool motion = _str(args[3]).toBool();
        if (motion)
        {
            SprMngr.SetMousePosition(x, y);
        }
        else
        {
            SDL_EventState(SDL_MOUSEMOTION, SDL_DISABLE);
            SprMngr.SetMousePosition(x, y);
            SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
            Settings.MouseX = Settings.LastMouseX = x;
            Settings.MouseY = Settings.LastMouseY = y;
        }*/
#endif
    }
    else if (cmd == "SetCursorPos") {
        if (HexMngr.IsMapLoaded()) {
            HexMngr.SetCursorPos(Settings.MouseX, Settings.MouseY, Keyb.CtrlDwn, true);
        }
    }
    else if (cmd == "NetDisconnect") {
        NetDisconnect();

        if (!_isConnected && !IsMainScreen(SCREEN_LOGIN)) {
            ShowMainScreen(SCREEN_LOGIN, {});
        }
    }
    else if (cmd == "TryExit") {
        TryExit();
    }
    else if (cmd == "Version") {
        return _str("{}", FO_GAME_VERSION);
    }
    else if (cmd == "BytesSend") {
        return _str("{}", _bytesSend);
    }
    else if (cmd == "BytesReceive") {
        return _str("{}", _bytesReceive);
    }
    else if (cmd == "GetLanguage") {
        return _curLang.Name;
    }
    else if (cmd == "SetLanguage" && args.size() >= 2) {
        if (args[1].length() == 4) {
            _curLang.LoadFromCache(Cache, *this, args[1]);
        }
    }
    else if (cmd == "SetResolution" && args.size() >= 3) {
        auto w = _str(args[1]).toInt();
        auto h = _str(args[2]).toInt();
        auto diff_w = w - Settings.ScreenWidth;
        auto diff_h = h - Settings.ScreenHeight;

        Settings.ScreenWidth = w;
        Settings.ScreenHeight = h;
        SprMngr.SetWindowSize(w, h);

        if (!Settings.FullScreen) {
            const auto [x, y] = SprMngr.GetWindowPosition();
            SprMngr.SetWindowPosition(x - diff_w / 2, y - diff_h / 2);
        }
        else {
            _windowResolutionDiffX += diff_w / 2;
            _windowResolutionDiffY += diff_h / 2;
        }

        SprMngr.OnResolutionChanged();
        if (HexMngr.IsMapLoaded()) {
            HexMngr.OnResolutionChanged();
        }
    }
    else if (cmd == "RefreshAlwaysOnTop") {
        SprMngr.SetAlwaysOnTop(Settings.AlwaysOnTop);
    }
    else if (cmd == "Command" && args.size() >= 2) {
        string str;
        for (size_t i = 1; i < args.size(); i++) {
            str += args[i] + " ";
        }
        str = _str(str).trim();

        string buf;
        if (!PackNetCommand(
                str, &_netOut,
                [&buf, &separator](auto s) {
                    buf += s;
                    buf += separator;
                },
                GetChosen()->AlternateName, *this)) {
            return "UNKNOWN";
        }

        return buf;
    }
    else if (cmd == "ConsoleMessage" && args.size() >= 2) {
        Net_SendText(args[1], SAY_NORM);
    }
    else if (cmd == "SaveLog" && args.size() == 3) {
        //              if( file_name == "" )
        //              {
        //                      DateTime dt;
        //                      Timer::GetCurrentDateTime(dt);
        //                      char     log_path[TEMP_BUF_SIZE];
        //                      X_str(log_path, "messbox_%04d.%02d.%02d_%02d-%02d-%02d.txt",
        //                              dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
        //              }

        //              for (uint i = 0; i < MessBox.size(); ++i)
        //              {
        //                      MessBoxMessage& m = MessBox[i];
        //                      // Skip
        //                      if (IsMainScreen(SCREEN_GAME) && std::find(MessBoxFilters.begin(), MessBoxFilters.end(),
        //                      m.Type) != MessBoxFilters.end())
        //                              continue;
        //                      // Concat
        //                      Str::Copy(cur_mess, m.Mess);
        //                      Str::EraseWords(cur_mess, '|', ' ');
        //                      fmt_log += MessBox[i].Time + string(cur_mess);
        //              }
    }
    else if (cmd == "DialogAnswer" && args.size() >= 4) {
        auto is_npc = (args[1] == "true");
        auto talker_id = _str(args[2]).toUInt();
        auto answer_index = _str(args[3]).toUInt();
        Net_SendTalk(is_npc, talker_id, static_cast<uchar>(answer_index));
    }
    else if (cmd == "DrawMiniMap" && args.size() >= 6) {
        static int zoom;
        static int x;
        static int y;
        static int x2;
        static int y2;
        zoom = _str(args[1]).toInt();
        x = _str(args[2]).toInt();
        y = _str(args[3]).toInt();
        x2 = x + _str(args[4]).toInt();
        y2 = y + _str(args[5]).toInt();

        if (zoom != _lmapZoom || x != _lmapWMap[0] || y != _lmapWMap[1] || x2 != _lmapWMap[2] || y2 != _lmapWMap[3]) {
            _lmapZoom = zoom;
            _lmapWMap[0] = x;
            _lmapWMap[1] = y;
            _lmapWMap[2] = x2;
            _lmapWMap[3] = y2;
            LmapPrepareMap();
        }
        else if (GameTime.FrameTick() >= _lmapPrepareNextTick) {
            LmapPrepareMap();
        }

        SprMngr.DrawPoints(_lmapPrepPix, RenderPrimitiveType::LineList, nullptr, nullptr, nullptr);
    }
    else if (cmd == "RefreshMe") {
        Net_SendRefereshMe();
    }
    else if (cmd == "SetCrittersContour" && args.size() == 2) {
        auto countour_type = _str(args[1]).toInt();
        HexMngr.SetCrittersContour(countour_type);
    }
    else if (cmd == "DrawWait") {
        WaitDraw();
    }
    else if (cmd == "ChangeDir" && args.size() == 2) {
        auto dir = _str(args[1]).toInt();
        GetChosen()->ChangeDir(static_cast<uchar>(dir), true);
        Net_SendDir();
    }
    else if (cmd == "MoveItem" && args.size() == 5) {
        auto item_count = _str(args[1]).toUInt();
        auto item_id = _str(args[2]).toUInt();
        auto item_swap_id = _str(args[3]).toUInt();
        auto to_slot = _str(args[4]).toInt();
        auto* item = GetChosen()->GetItem(item_id);
        auto* item_swap = (item_swap_id ? GetChosen()->GetItem(item_swap_id) : nullptr);
        auto* old_item = item->Clone();
        int from_slot = item->GetCritSlot();

        // Move
        auto is_light = item->GetIsLight();
        if (to_slot == -1) {
            GetChosen()->Action(ACTION_DROP_ITEM, from_slot, item, true);
            if (item->GetStackable() && item_count < item->GetCount()) {
                item->SetCount(item->GetCount() - item_count);
            }
            else {
                GetChosen()->DeleteItem(item, true);
                item = nullptr;
            }
        }
        else {
            item->SetCritSlot(static_cast<uchar>(to_slot));
            if (item_swap) {
                item_swap->SetCritSlot(static_cast<uchar>(from_slot));
            }

            GetChosen()->Action(ACTION_MOVE_ITEM, from_slot, item, true);
            if (item_swap) {
                GetChosen()->Action(ACTION_MOVE_ITEM_SWAP, to_slot, item_swap, true);
            }
        }

        // Light
        _rebuildLookBordersRequest = true;
        if (is_light && (!to_slot || (!from_slot && to_slot != -1))) {
            HexMngr.RebuildLight();
        }

        // Notify scripts about item changing
        OnItemInvChanged.Fire(item, old_item);
        old_item->Release();
    }
    else if (cmd == "SkipRoof" && args.size() == 3) {
        auto hx = _str(args[1]).toUInt();
        auto hy = _str(args[2]).toUInt();
        HexMngr.SetSkipRoof(hx, hy);
    }
    else if (cmd == "RebuildLookBorders") {
        _rebuildLookBordersRequest = true;
    }
    else if (cmd == "TransitCritter" && args.size() == 5) {
        auto hx = _str(args[1]).toInt();
        auto hy = _str(args[2]).toInt();
        auto animate = _str(args[3]).toBool();
        auto force = _str(args[4]).toBool();

        HexMngr.TransitCritter(GetChosen(), hx, hy, animate, force);
    }
    else if (cmd == "SendMove") {
        vector<uchar> dirs;
        for (size_t i = 1; i < args.size(); i++) {
            dirs.push_back(static_cast<uchar>(_str(args[i]).toInt()));
        }

        Net_SendMove(dirs);

        if (dirs.size() > 1) {
            GetChosen()->MoveSteps.resize(1);
        }
        else {
            GetChosen()->MoveSteps.resize(0);
            if (!GetChosen()->IsAnim()) {
                GetChosen()->AnimateStay();
            }
        }
    }
    else if (cmd == "ChosenAlpha" && args.size() == 2) {
        auto alpha = _str(args[1]).toInt();

        GetChosen()->Alpha = static_cast<uchar>(alpha);
    }
    else if (cmd == "SetScreenKeyboard" && args.size() == 2) {
        /*if (SDL_HasScreenKeyboardSupport())
        {
            bool cur = (SDL_IsTextInputActive() != SDL_FALSE);
            bool next = _str(args[1]).toBool();
            if (cur != next)
            {
                if (next)
                    SDL_StartTextInput();
                else
                    SDL_StopTextInput();
            }
        }*/
    }
    else {
        throw ScriptException("Invalid custom call command");
    }
    return "";
}
