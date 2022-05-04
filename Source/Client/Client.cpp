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

// clang-format off
FOClient::FOClient(GlobalSettings& settings, ScriptSystem* script_sys) :
    FOEngineBase(false),
    Settings {settings},
    GeomHelper(Settings),
    ScriptSys {script_sys},
    GameTime(Settings),
    ProtoMngr(this),
    EffectMngr(Settings, FileSys),
    SprMngr(Settings, FileSys, EffectMngr),
    ResMngr(FileSys, SprMngr, *this, *this),
    SndMngr(Settings, FileSys),
    Keyb(Settings, SprMngr),
    Cache("Data/Cache.fobin"),
    _conn(Settings),
    _worldmapFog(GM_MAXZONEX, GM_MAXZONEY, nullptr)
// clang-format on
{
    FileSys.AddDataSource("$Embedded");
    for (const auto& entry : Settings.ResourceEntries) {
        FileSys.AddDataSource(_str(Settings.ResourcesDir).combinePath(entry));
    }

#if FO_IOS
    FileSys.AddDataSource("../../Documents", true);
#elif FO_ANDROID
    FileSys.AddDataSource("$AndroidAssets", true);
    // AddDataSource(SDL_AndroidGetInternalStoragePath(), true);
    // AddDataSource(SDL_AndroidGetExternalStoragePath(), true);
#elif FO_WEB
    FileSys.AddDataSource("PersistentData", true);
#endif

    _fpsTick = GameTime.FrameTick();

    const auto [w, h] = SprMngr.GetWindowSize();
    const auto [x, y] = SprMngr.GetMousePosition();
    Settings.MouseX = std::clamp(x, 0, w - 1);
    Settings.MouseY = std::clamp(y, 0, h - 1);

    // Language Packs
    _curLang.LoadFromFiles(FileSys, *this, Settings.Language);

    SprMngr.SetSpritesColor(COLOR_IFACE);

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

    // Init 3d subsystem
    if (Settings.Enable3dRendering) {
        SprMngr.Init3dSubsystem(GameTime, *this, *this);

        if (!Preload3dFiles.empty()) {
            WriteLog("Preload 3d files...\n");
            for (const auto& name : Preload3dFiles) {
                SprMngr.Preload3dModel(name);
            }
            WriteLog("Preload 3d files complete.\n");
        }
    }

    // Connection handlers
    _conn.AddConnectHandler(std::bind(&FOClient::Net_OnConnect, this, std::placeholders::_1));
    _conn.AddDisconnectHandler(std::bind(&FOClient::Net_OnDisconnect, this));
    _conn.AddMessageHandler(NETMSG_WRONG_NET_PROTO, std::bind(&FOClient::Net_OnWrongNetProto, this));
    _conn.AddMessageHandler(NETMSG_LOGIN_SUCCESS, std::bind(&FOClient::Net_OnLoginSuccess, this));
    _conn.AddMessageHandler(NETMSG_REGISTER_SUCCESS, [] { WriteLog("Registration success.\n"); });
    _conn.AddMessageHandler(NETMSG_END_PARSE_TO_GAME, std::bind(&FOClient::Net_OnEndParseToGame, this));
    _conn.AddMessageHandler(NETMSG_ADD_PLAYER, std::bind(&FOClient::Net_OnAddCritter, this, false));
    _conn.AddMessageHandler(NETMSG_ADD_NPC, std::bind(&FOClient::Net_OnAddCritter, this, true));
    _conn.AddMessageHandler(NETMSG_REMOVE_CRITTER, std::bind(&FOClient::Net_OnRemoveCritter, this));
    _conn.AddMessageHandler(NETMSG_SOME_ITEM, std::bind(&FOClient::Net_OnSomeItem, this));
    _conn.AddMessageHandler(NETMSG_CRITTER_ACTION, std::bind(&FOClient::Net_OnCritterAction, this));
    _conn.AddMessageHandler(NETMSG_CRITTER_MOVE_ITEM, std::bind(&FOClient::Net_OnCritterMoveItem, this));
    _conn.AddMessageHandler(NETMSG_CRITTER_ANIMATE, std::bind(&FOClient::Net_OnCritterAnimate, this));
    _conn.AddMessageHandler(NETMSG_CRITTER_SET_ANIMS, std::bind(&FOClient::Net_OnCritterSetAnims, this));
    _conn.AddMessageHandler(NETMSG_CUSTOM_COMMAND, std::bind(&FOClient::Net_OnCustomCommand, this));
    _conn.AddMessageHandler(NETMSG_CRITTER_MOVE, std::bind(&FOClient::Net_OnCritterMove, this));
    _conn.AddMessageHandler(NETMSG_CRITTER_DIR, std::bind(&FOClient::Net_OnCritterDir, this));
    _conn.AddMessageHandler(NETMSG_CRITTER_XY, std::bind(&FOClient::Net_OnCritterCoords, this));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(1, 0), std::bind(&FOClient::Net_OnProperty, this, 1));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(1, 1), std::bind(&FOClient::Net_OnProperty, this, 1));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(1, 2), std::bind(&FOClient::Net_OnProperty, this, 1));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(2, 0), std::bind(&FOClient::Net_OnProperty, this, 2));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(2, 1), std::bind(&FOClient::Net_OnProperty, this, 2));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(2, 2), std::bind(&FOClient::Net_OnProperty, this, 2));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(4, 0), std::bind(&FOClient::Net_OnProperty, this, 4));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(4, 1), std::bind(&FOClient::Net_OnProperty, this, 4));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(4, 2), std::bind(&FOClient::Net_OnProperty, this, 4));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(8, 0), std::bind(&FOClient::Net_OnProperty, this, 8));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(8, 1), std::bind(&FOClient::Net_OnProperty, this, 8));
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(8, 2), std::bind(&FOClient::Net_OnProperty, this, 8));
    _conn.AddMessageHandler(NETMSG_COMPLEX_PROPERTY, std::bind(&FOClient::Net_OnProperty, this, 0));
    _conn.AddMessageHandler(NETMSG_CRITTER_TEXT, std::bind(&FOClient::Net_OnText, this));
    _conn.AddMessageHandler(NETMSG_MSG, std::bind(&FOClient::Net_OnTextMsg, this, false));
    _conn.AddMessageHandler(NETMSG_MSG_LEX, std::bind(&FOClient::Net_OnTextMsg, this, true));
    _conn.AddMessageHandler(NETMSG_MAP_TEXT, std::bind(&FOClient::Net_OnMapText, this));
    _conn.AddMessageHandler(NETMSG_MAP_TEXT_MSG, std::bind(&FOClient::Net_OnMapTextMsg, this));
    _conn.AddMessageHandler(NETMSG_MAP_TEXT_MSG_LEX, std::bind(&FOClient::Net_OnMapTextMsgLex, this));
    _conn.AddMessageHandler(NETMSG_ALL_PROPERTIES, std::bind(&FOClient::Net_OnAllProperties, this));
    _conn.AddMessageHandler(NETMSG_CLEAR_ITEMS, std::bind(&FOClient::Net_OnChosenClearItems, this));
    _conn.AddMessageHandler(NETMSG_ADD_ITEM, std::bind(&FOClient::Net_OnChosenAddItem, this));
    _conn.AddMessageHandler(NETMSG_REMOVE_ITEM, std::bind(&FOClient::Net_OnChosenEraseItem, this));
    _conn.AddMessageHandler(NETMSG_ALL_ITEMS_SEND, std::bind(&FOClient::Net_OnAllItemsSend, this));
    _conn.AddMessageHandler(NETMSG_TALK_NPC, std::bind(&FOClient::Net_OnChosenTalk, this));
    _conn.AddMessageHandler(NETMSG_GAME_INFO, std::bind(&FOClient::Net_OnGameInfo, this));
    _conn.AddMessageHandler(NETMSG_AUTOMAPS_INFO, std::bind(&FOClient::Net_OnAutomapsInfo, this));
    _conn.AddMessageHandler(NETMSG_VIEW_MAP, std::bind(&FOClient::Net_OnViewMap, this));
    _conn.AddMessageHandler(NETMSG_LOADMAP, std::bind(&FOClient::Net_OnLoadMap, this));
    _conn.AddMessageHandler(NETMSG_MAP, std::bind(&FOClient::Net_OnMap, this));
    _conn.AddMessageHandler(NETMSG_GLOBAL_INFO, std::bind(&FOClient::Net_OnGlobalInfo, this));
    _conn.AddMessageHandler(NETMSG_SOME_ITEMS, std::bind(&FOClient::Net_OnSomeItems, this));
    // _conn.AddMessageHandler(NETMSG_RPC, // ScriptSys.HandleRpc(&Bin);
    _conn.AddMessageHandler(NETMSG_ADD_ITEM_ON_MAP, std::bind(&FOClient::Net_OnAddItemOnMap, this));
    _conn.AddMessageHandler(NETMSG_ERASE_ITEM_FROM_MAP, std::bind(&FOClient::Net_OnEraseItemFromMap, this));
    _conn.AddMessageHandler(NETMSG_ANIMATE_ITEM, std::bind(&FOClient::Net_OnAnimateItem, this));
    _conn.AddMessageHandler(NETMSG_COMBAT_RESULTS, std::bind(&FOClient::Net_OnCombatResult, this));
    _conn.AddMessageHandler(NETMSG_EFFECT, std::bind(&FOClient::Net_OnEffect, this));
    _conn.AddMessageHandler(NETMSG_FLY_EFFECT, std::bind(&FOClient::Net_OnFlyEffect, this));
    _conn.AddMessageHandler(NETMSG_PLAY_SOUND, std::bind(&FOClient::Net_OnPlaySound, this));
    _conn.AddMessageHandler(NETMSG_UPDATE_FILES_LIST, std::bind(&FOClient::Net_OnUpdateFilesResponse, this));

    ScreenFadeOut();

    // Auto login
    ProcessAutoLogin();
}

FOClient::~FOClient()
{
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

    if (_chosen != nullptr && _chosen->IsDestroyed()) {
        _chosen->Release();
        _chosen = nullptr;
    }

    return _chosen;
}

auto FOClient::GetMapChosen() -> CritterHexView*
{
    return dynamic_cast<CritterHexView*>(GetChosen());
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

void FOClient::LookBordersPrepare()
{
    _lookBorders.clear();
    _shootBorders.clear();

    auto* chosen = GetMapChosen();
    if (chosen == nullptr || CurMap == nullptr || (!_drawLookBorders && !_drawShootBorders)) {
        CurMap->SetFog(_lookBorders, _shootBorders, nullptr, nullptr);
        return;
    }

    const auto dist = chosen->GetLookDistance();
    const auto base_hx = chosen->GetHexX();
    const auto base_hy = chosen->GetHexY();
    int hx = base_hx;
    int hy = base_hy;
    const int chosen_dir = chosen->GetDir();
    const auto dist_shoot = chosen->GetAttackDist();
    const auto maxhx = CurMap->GetWidth();
    const auto maxhy = CurMap->GetHeight();
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
                pair<ushort, ushort> block = {};
                CurMap->TraceBullet(base_hx, base_hy, hx_, hy_, dist_, 0.0f, nullptr, false, nullptr, CritterFindType::Any, nullptr, &block, nullptr, false);
                hx_ = block.first;
                hy_ = block.second;
            }

            if (IsBitSet(Settings.LookChecks, LOOK_CHECK_TRACE)) {
                pair<ushort, ushort> block = {};
                CurMap->TraceBullet(base_hx, base_hy, hx_, hy_, 0, 0.0f, nullptr, false, nullptr, CritterFindType::Any, nullptr, &block, nullptr, true);
                hx_ = block.first;
                hy_ = block.second;
            }

            auto dist_look = GeomHelper.DistGame(base_hx, base_hy, hx_, hy_);
            if (_drawLookBorders) {
                auto x = 0;
                auto y = 0;
                CurMap->GetHexCurrentPosition(hx_, hy_, x, y);
                auto* ox = (dist_look == dist ? &chosen->SprOx : nullptr);
                auto* oy = (dist_look == dist ? &chosen->SprOy : nullptr);
                _lookBorders.push_back({x + (Settings.MapHexWidth / 2), y + (Settings.MapHexHeight / 2), COLOR_RGBA(0, 255, dist_look * 255 / dist, 0), ox, oy});
            }

            if (_drawShootBorders) {
                pair<ushort, ushort> block = {};
                const auto max_shoot_dist = std::max(std::min(dist_look, dist_shoot), 0u) + 1u;
                CurMap->TraceBullet(base_hx, base_hy, hx_, hy_, max_shoot_dist, 0.0f, nullptr, false, nullptr, CritterFindType::Any, nullptr, &block, nullptr, true);
                const auto hx_2 = block.first;
                const auto hy_2 = block.second;

                auto x_ = 0;
                auto y_ = 0;
                CurMap->GetHexCurrentPosition(hx_2, hy_2, x_, y_);
                const auto result_shoot_dist = GeomHelper.DistGame(base_hx, base_hy, hx_2, hy_2);
                auto* ox = (result_shoot_dist == max_shoot_dist ? &chosen->SprOx : nullptr);
                auto* oy = (result_shoot_dist == max_shoot_dist ? &chosen->SprOy : nullptr);
                _shootBorders.push_back({x_ + (Settings.MapHexWidth / 2), y_ + (Settings.MapHexHeight / 2), COLOR_RGBA(255, 255, result_shoot_dist * 255 / max_shoot_dist, 0), ox, oy});
            }
        }
    }

    auto base_x = 0;
    auto base_y = 0;
    CurMap->GetHexCurrentPosition(base_hx, base_hy, base_x, base_y);
    if (!_lookBorders.empty()) {
        _lookBorders.push_back(*_lookBorders.begin());
        _lookBorders.insert(_lookBorders.begin(), {base_x + (Settings.MapHexWidth / 2), base_y + (Settings.MapHexHeight / 2), COLOR_RGBA(0, 0, 0, 0), &chosen->SprOx, &chosen->SprOy});
    }
    if (!_shootBorders.empty()) {
        _shootBorders.push_back(*_shootBorders.begin());
        _shootBorders.insert(_shootBorders.begin(), {base_x + (Settings.MapHexWidth / 2), base_y + (Settings.MapHexHeight / 2), COLOR_RGBA(255, 0, 0, 0), &chosen->SprOx, &chosen->SprOy});
    }

    CurMap->SetFog(_lookBorders, _shootBorders, &chosen->SprOx, &chosen->SprOy);
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

    // Game end
    if (Settings.Quit) {
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
    if (_initNetReason != INIT_NET_REASON_NONE) {
        _conn.Connect();
    }

    _conn.Process();

    // Exit in Login screen if net disconnect
    if (!_conn.IsConnected() && !IsMainScreen(SCREEN_LOGIN)) {
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
        ProcessGlobalMap();
    }

    if (CurMap != nullptr) {
        CurMap->Process();
    }

    // Start render
    EffectMngr.UpdateEffects(GameTime);
    SprMngr.BeginScene(COLOR_RGB(0, 0, 0));

#if !FO_SINGLEPLAYER
    // Script loop
    OnLoop.Fire();
#endif

    // Quake effect
    ProcessScreenEffectQuake();

    // Render
    if (GetMainScreen() == SCREEN_GAME && CurMap != nullptr) {
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

void FOClient::Net_OnConnect(bool success)
{
    if (success) {
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
    else {
        ShowMainScreen(SCREEN_LOGIN, {});
        AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_FAIL));
    }
}

void FOClient::Net_OnDisconnect()
{
    if (CurMap != nullptr) {
        CurMap->MarkAsDestroyed();
        CurMap->Release();
        CurMap = nullptr;
    }

    if (_curPlayer != nullptr) {
        _curPlayer->MarkAsDestroyed();
        _curPlayer->Release();
        _curPlayer = nullptr;
    }

    ProcessAutoLogin();
}

void FOClient::Net_SendUpdate()
{
    // Header
    _conn.OutBuf << NETMSG_UPDATE;

    // Protocol version
    _conn.OutBuf << static_cast<ushort>(FO_COMPATIBILITY_VERSION);

    // Data encrypting
    const uint encrypt_key = NetBuffer::GenerateEncryptKey();
    _conn.OutBuf << encrypt_key;
    _conn.OutBuf.SetEncryptKey(encrypt_key);
    _conn.InBuf.SetEncryptKey(encrypt_key);
}

void FOClient::Net_SendLogIn()
{
    WriteLog("Player login.\n");

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(ushort) + NetBuffer::STRING_LEN_SIZE * 2u + static_cast<uint>(_loginName.length() + _loginPassword.length());

    _conn.OutBuf << NETMSG_LOGIN;
    _conn.OutBuf << msg_len;
    _conn.OutBuf << FO_COMPATIBILITY_VERSION;

    // Begin data encrypting
    _conn.OutBuf.SetEncryptKey(12345);
    _conn.InBuf.SetEncryptKey(12345);

    _conn.OutBuf << _loginName;
    _conn.OutBuf << _loginPassword;
    _conn.OutBuf << _curLang.NameCode;

    AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_SUCCESS));
}

void FOClient::Net_SendCreatePlayer()
{
    WriteLog("Player registration.\n");

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(ushort) + NetBuffer::STRING_LEN_SIZE * 2u + static_cast<uint>(_loginName.length() + _loginPassword.length());

    _conn.OutBuf << NETMSG_REGISTER;
    _conn.OutBuf << msg_len;
    _conn.OutBuf << FO_COMPATIBILITY_VERSION;

    // Begin data encrypting
    _conn.OutBuf.SetEncryptKey(1234567890);
    _conn.InBuf.SetEncryptKey(1234567890);

    _conn.OutBuf << _loginName;
    _conn.OutBuf << _loginPassword;
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

    _conn.OutBuf << NETMSG_SEND_TEXT;
    _conn.OutBuf << msg_len;
    _conn.OutBuf << how_say;
    _conn.OutBuf << str;
}

void FOClient::Net_SendDir()
{
    const auto* chosen = GetMapChosen();
    if (chosen == nullptr) {
        return;
    }

    _conn.OutBuf << NETMSG_DIR;
    _conn.OutBuf << chosen->GetDir();
}

void FOClient::Net_SendMove(vector<uchar> steps)
{
    const auto* chosen = GetMapChosen();
    if (chosen == nullptr) {
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

    if (chosen->IsRunning) {
        SetBit(move_params, MOVE_PARAM_RUN);
    }
    if (steps.empty()) {
        SetBit(move_params, MOVE_PARAM_STEP_DISALLOW); // Inform about stopping
    }

    _conn.OutBuf << (chosen->IsRunning ? NETMSG_SEND_MOVE_RUN : NETMSG_SEND_MOVE_WALK);
    _conn.OutBuf << move_params;
    _conn.OutBuf << chosen->GetHexX();
    _conn.OutBuf << chosen->GetHexY();
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
        _conn.OutBuf << NETMSG_SEND_POD_PROPERTY(data_size, additional_args);
    }
    else {
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(char) + additional_args * sizeof(uint) + sizeof(ushort) + data_size;
        _conn.OutBuf << NETMSG_SEND_COMPLEX_PROPERTY;
        _conn.OutBuf << msg_len;
    }

    _conn.OutBuf << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        _conn.OutBuf << dynamic_cast<ItemView*>(client_entity)->GetCritId();
        _conn.OutBuf << client_entity->GetId();
        break;
    case NetProperty::Critter:
        _conn.OutBuf << client_entity->GetId();
        break;
    case NetProperty::MapItem:
        _conn.OutBuf << client_entity->GetId();
        break;
    case NetProperty::ChosenItem:
        _conn.OutBuf << client_entity->GetId();
        break;
    default:
        break;
    }

    if (is_pod) {
        _conn.OutBuf << prop->GetRegIndex();
        _conn.OutBuf.Push(data, data_size);
    }
    else {
        _conn.OutBuf << prop->GetRegIndex();
        if (data_size != 0u) {
            _conn.OutBuf.Push(data, data_size);
        }
    }
}

void FOClient::Net_SendTalk(uchar is_npc, uint id_to_talk, uchar answer)
{
    _conn.OutBuf << NETMSG_SEND_TALK_NPC;
    _conn.OutBuf << is_npc;
    _conn.OutBuf << id_to_talk;
    _conn.OutBuf << answer;
}

void FOClient::Net_SendGetGameInfo()
{
    _conn.OutBuf << NETMSG_SEND_GET_INFO;
}

void FOClient::Net_SendGiveMap(bool automap, hstring map_pid, uint loc_id, uint tiles_hash, uint scen_hash)
{
    _conn.OutBuf << NETMSG_SEND_GIVE_MAP;
    _conn.OutBuf << automap;
    _conn.OutBuf << map_pid;
    _conn.OutBuf << loc_id;
    _conn.OutBuf << tiles_hash;
    _conn.OutBuf << scen_hash;
}

void FOClient::Net_SendLoadMapOk()
{
    _conn.OutBuf << NETMSG_SEND_LOAD_MAP_OK;
}

void FOClient::Net_SendPing(uchar ping)
{
    _conn.OutBuf << NETMSG_PING;
    _conn.OutBuf << ping;
}

void FOClient::Net_SendRefereshMe()
{
    _conn.OutBuf << NETMSG_SEND_REFRESH_ME;
}

void FOClient::Net_OnUpdateFilesResponse()
{
    // Todo: run updater if resources changed
    AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_CLIENT_OUTDATED));
}

void FOClient::Net_OnWrongNetProto()
{
    AddMess(SAY_NETMSG, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_CLIENT_OUTDATED));
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
    _conn.InBuf >> msg_len;
    _conn.InBuf >> bin_seed;
    _conn.InBuf >> bout_seed;
    _conn.InBuf >> player_id;

    NET_READ_PROPERTIES(_conn.InBuf, _globalsPropertiesData);
    NET_READ_PROPERTIES(_conn.InBuf, _playerPropertiesData);

    CHECK_IN_BUF_ERROR(_conn);

    _conn.OutBuf.SetEncryptKey(bin_seed);
    _conn.InBuf.SetEncryptKey(bout_seed);

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
    _conn.InBuf >> msg_len;

    uint crid;
    ushort hx;
    ushort hy;
    uchar dir;
    _conn.InBuf >> crid;
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> dir;

    CritterCondition cond;
    uint anim1_alive;
    uint anim1_ko;
    uint anim1_dead;
    uint anim2_alive;
    uint anim2_ko;
    uint anim2_dead;
    uint flags;
    _conn.InBuf >> cond;
    _conn.InBuf >> anim1_alive;
    _conn.InBuf >> anim1_ko;
    _conn.InBuf >> anim1_dead;
    _conn.InBuf >> anim2_alive;
    _conn.InBuf >> anim2_ko;
    _conn.InBuf >> anim2_dead;
    _conn.InBuf >> flags;

    // Npc
    hstring npc_pid;
    if (is_npc) {
        npc_pid = _conn.InBuf.ReadHashedString(*this);
    }

    // Player
    string cl_name;
    if (!is_npc) {
        _conn.InBuf >> cl_name;
    }

    // Properties
    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    const auto* proto = ProtoMngr.GetProtoCritter(is_npc ? npc_pid : ToHashedString("Player"));
    RUNTIME_ASSERT(proto);

    auto* cr = CurMap->AddCritter(crid, proto, hx, hy, _tempPropertiesData);
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
        if (cr->GetDialogId() && _curLang.Msg[TEXTMSG_DLG].Count(STR_NPC_NAME(cr->GetDialogId().as_uint())) != 0u) {
            cr->AlternateName = _curLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_NAME(cr->GetDialogId().as_uint()));
        }
        else {
            cr->AlternateName = _curLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_PID_NAME(npc_pid.as_uint()));
        }
    }
    else {
        cr->AlternateName = cl_name;
    }

    if (cr->IsChosen()) {
        _chosen = cr;
        _chosen->AddRef();
    }

    OnCritterIn.Fire(cr);

    if (cr->IsChosen()) {
        _rebuildLookBordersRequest = true;
    }
}

void FOClient::Net_OnRemoveCritter()
{
    uint remove_crid;
    _conn.InBuf >> remove_crid;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(remove_crid);
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
    _conn.InBuf >> msg_len;
    _conn.InBuf >> crid;
    _conn.InBuf >> how_say;
    _conn.InBuf >> text;
    _conn.InBuf >> unsafe_text;

    CHECK_IN_BUF_ERROR(_conn);

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
        _conn.InBuf >> msg_len;
    }

    uint crid;
    uchar how_say;
    ushort msg_num;
    uint num_str;
    _conn.InBuf >> crid;
    _conn.InBuf >> how_say;
    _conn.InBuf >> msg_num;
    _conn.InBuf >> num_str;

    string lexems;
    if (with_lexems) {
        _conn.InBuf >> lexems;
    }

    CHECK_IN_BUF_ERROR(_conn);

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
        FormatTags(str, GetChosen(), CurMap != nullptr ? CurMap->GetCritter(crid) : nullptr, lexems);
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

    auto* cr = (how_say != SAY_RADIO ? (CurMap != nullptr ? CurMap->GetCritter(crid) : nullptr) : nullptr);

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
            if (auto* chosen = GetChosen(); chosen != nullptr) {
                const auto* radio = chosen->GetItem(crid);
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
    auto show_time = Settings.TextDelay + static_cast<uint>(str.length()) * 100;

    auto sstr = _str(str).str();
    OnMapMessage.Fire(sstr, hx, hy, color, show_time);

    CurMap->AddMapText(sstr, hx, hy, color, show_time, false, 0, 0);

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
    _conn.InBuf >> msg_len;
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> color;
    _conn.InBuf >> text;
    _conn.InBuf >> unsafe_text;

    CHECK_IN_BUF_ERROR(_conn);

    if (hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight()) {
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
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> color;
    _conn.InBuf >> msg_num;
    _conn.InBuf >> num_str;

    CHECK_IN_BUF_ERROR(_conn);

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}.\n", msg_num);
        return;
    }

    auto str = _curLang.Msg[msg_num].GetStr(num_str);
    FormatTags(str, GetChosen(), nullptr, "");
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
    _conn.InBuf >> msg_len;
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> color;
    _conn.InBuf >> msg_num;
    _conn.InBuf >> num_str;
    _conn.InBuf >> lexems;

    CHECK_IN_BUF_ERROR(_conn);

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}.\n", msg_num);
        return;
    }

    auto str = _curLang.Msg[msg_num].GetStr(num_str);
    FormatTags(str, GetChosen(), nullptr, lexems);
    OnMapText(str, hx, hy, color);
}

void FOClient::Net_OnCritterDir()
{
    uint crid;
    uchar dir;
    _conn.InBuf >> crid;
    _conn.InBuf >> dir;

    CHECK_IN_BUF_ERROR(_conn);

    if (dir >= Settings.MapDirCount) {
        WriteLog("Invalid dir {}.\n", dir);
        dir = 0;
    }

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
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
    _conn.InBuf >> crid;
    _conn.InBuf >> move_params;
    _conn.InBuf >> new_hx;
    _conn.InBuf >> new_hy;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }
    if (new_hx >= CurMap->GetWidth() || new_hy >= CurMap->GetHeight()) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    cr->IsRunning = IsBitSet(move_params, MOVE_PARAM_RUN);

    if (cr->IsChosen()) {
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
                    CurMap->TransitCritter(cr, new_hx, new_hy, true, true);
                    cr->CurMoveStep = -1;
                }
                break;
            }

            GeomHelper.MoveHexByDir(new_hx, new_hy, dir, CurMap->GetWidth(), CurMap->GetHeight());

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
        CurMap->TransitCritter(cr, new_hx, new_hy, true, true);
    }
}

void FOClient::Net_OnSomeItem()
{
    uint msg_len;
    uint item_id;
    hstring item_pid;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> item_id;
    item_pid = _conn.InBuf.ReadHashedString(*this);

    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_IN_BUF_ERROR(_conn);

    if (_someItem != nullptr) {
        _someItem->Release();
        _someItem = nullptr;
    }

    const auto* proto_item = ProtoMngr.GetProtoItem(item_pid);
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
    _conn.InBuf >> crid;
    _conn.InBuf >> action;
    _conn.InBuf >> action_ext;
    _conn.InBuf >> is_item;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
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
    _conn.InBuf >> msg_len;
    _conn.InBuf >> crid;
    _conn.InBuf >> action;
    _conn.InBuf >> prev_slot;
    _conn.InBuf >> is_item;

    // Slot items
    ushort slots_data_count;
    _conn.InBuf >> slots_data_count;

    CHECK_IN_BUF_ERROR(_conn);

    vector<uchar> slots_data_slot;
    vector<uint> slots_data_id;
    vector<hstring> slots_data_pid;
    vector<vector<vector<uchar>>> slots_data_data;
    for ([[maybe_unused]] const auto i : xrange(slots_data_count)) {
        uchar slot;
        uint id;
        hstring pid;
        _conn.InBuf >> slot;
        _conn.InBuf >> id;
        pid = _conn.InBuf.ReadHashedString(*this);
        NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);
        slots_data_slot.push_back(slot);
        slots_data_id.push_back(id);
        slots_data_pid.push_back(pid);
        slots_data_data.push_back(_tempPropertiesData);
    }

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (!cr->IsChosen()) {
        int64 prev_hash_sum = 0;
        for (const auto* item : cr->InvItems) {
            prev_hash_sum += item->LightGetHash();
        }

        cr->DeleteAllItems();

        for (ushort i = 0; i < slots_data_count; i++) {
            const auto* proto_item = ProtoMngr.GetProtoItem(slots_data_pid[i]);
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
            CurMap->RebuildLight();
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
    _conn.InBuf >> crid;
    _conn.InBuf >> anim1;
    _conn.InBuf >> anim2;
    _conn.InBuf >> is_item;
    _conn.InBuf >> clear_sequence;
    _conn.InBuf >> delay_play;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
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
    _conn.InBuf >> crid;
    _conn.InBuf >> cond;
    _conn.InBuf >> anim1;
    _conn.InBuf >> anim2;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
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
    _conn.InBuf >> crid;
    _conn.InBuf >> index;
    _conn.InBuf >> value;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
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
            cr->TickStart(value);
            cr->MoveSteps.clear();
            cr->CurMoveStep = 0;
        } break;
        case OTHER_FLAGS: {
            cr->Flags = value;
        } break;
        case OTHER_TELEPORT: {
            const ushort hx = (value >> 16) & 0xFFFF;
            const ushort hy = value & 0xFFFF;
            if (hx < CurMap->GetWidth() && hy < CurMap->GetHeight()) {
                auto* hex_cr = CurMap->GetField(hx, hy).Crit;
                if (cr == hex_cr) {
                    break;
                }
                if (!cr->IsDead() && hex_cr != nullptr) {
                    CurMap->DestroyCritter(hex_cr);
                }
                CurMap->MoveCritter(cr, hx, hy);
                CurMap->ScrollToHex(cr->GetHexX(), cr->GetHexY(), 0.1f, true);
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

void FOClient::Net_OnCritterCoords()
{
    uint crid;
    ushort hx;
    ushort hy;
    uchar dir;
    _conn.InBuf >> crid;
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> dir;

    CHECK_IN_BUF_ERROR(_conn);

    if (Settings.DebugNet) {
        AddMess(SAY_NETMSG, _str(" - crid {} hx {} hy {} dir {}.", crid, hx, hy, dir));
    }

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight() || dir >= Settings.MapDirCount) {
        WriteLog("Error data, hx {}, hy {}, dir {}.\n", hx, hy, dir);
        return;
    }

    if (cr->GetDir() != dir) {
        cr->ChangeDir(dir, false);
    }

    if (cr->GetHexX() != hx || cr->GetHexY() != hy) {
        if (const auto& field = CurMap->GetField(hx, hy); field.Crit != nullptr && field.Crit->IsFinishing()) {
            CurMap->DestroyCritter(field.Crit);
        }

        CurMap->TransitCritter(cr, hx, hy, false, true);
        cr->AnimateStay();

        if (cr->IsChosen()) {
            // Chosen->TickStart(Settings.Breaktime);
            cr->TickStart(200);
            // SetAction(CHOSEN_NONE);
            cr->MoveSteps.clear();
            cr->CurMoveStep = 0;
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
    _conn.InBuf >> msg_len;

    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_IN_BUF_ERROR(_conn);

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen not created, skip.\n");
        return;
    }

    chosen->RestoreData(_tempPropertiesData);

    // Animate
    if (auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr && !hex_chosen->IsAnim()) {
        hex_chosen->AnimateStay();
    }

    // Refresh borders
    _rebuildLookBordersRequest = true;
}

void FOClient::Net_OnChosenClearItems()
{
    _initialItemsSend = true;

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in clear items.\n");
        return;
    }

    if (const auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr && hex_chosen->IsHaveLightSources()) {
        CurMap->RebuildLight();
    }

    chosen->DeleteAllItems();
}

void FOClient::Net_OnChosenAddItem()
{
    uint msg_len;
    uint item_id;
    hstring pid;
    uchar slot;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> item_id;
    pid = _conn.InBuf.ReadHashedString(*this);
    _conn.InBuf >> slot;

    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_IN_BUF_ERROR(_conn);

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in add item.\n");
        return;
    }

    auto* prev_item = chosen->GetItem(item_id);
    uchar prev_slot = 0;
    uint prev_light_hash = 0;
    if (prev_item != nullptr) {
        prev_slot = prev_item->GetCritSlot();
        prev_light_hash = prev_item->LightGetHash();
        chosen->DeleteItem(prev_item, false);
    }

    const auto* proto_item = ProtoMngr.GetProtoItem(pid);
    RUNTIME_ASSERT(proto_item);

    auto* item = new ItemView(this, item_id, proto_item);
    item->RestoreData(_tempPropertiesData);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritId(chosen->GetId());
    item->SetCritSlot(slot);
    RUNTIME_ASSERT(!item->GetIsHidden());

    item->AddRef();

    chosen->AddItem(item);

    _rebuildLookBordersRequest = true;
    if (item->LightGetHash() != prev_light_hash && (slot != 0u || prev_slot != 0u)) {
        CurMap->RebuildLight();
    }

    if (!_initialItemsSend) {
        OnItemInvIn.Fire(item);
    }

    item->Release();
}

void FOClient::Net_OnChosenEraseItem()
{
    uint item_id;
    _conn.InBuf >> item_id;

    CHECK_IN_BUF_ERROR(_conn);

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in erase item.\n");
        return;
    }

    auto* item = chosen->GetItem(item_id);
    if (item == nullptr) {
        WriteLog("Item not found, id {}.\n", item_id);
        return;
    }

    auto* item_clone = item->Clone();

    const auto rebuild_light = (item->GetIsLight() && item->GetCritSlot() != 0u);
    chosen->DeleteItem(item, true);
    if (rebuild_light) {
        CurMap->RebuildLight();
    }

    OnItemInvOut.Fire(item_clone);
    item_clone->Release();
}

void FOClient::Net_OnAllItemsSend()
{
    _initialItemsSend = false;

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in all items send.\n");
        return;
    }

    if (auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr && hex_chosen->IsModel()) {
        hex_chosen->GetModel()->StartMeshGeneration();
    }

    OnItemInvAllIn.Fire();
}

void FOClient::Net_OnAddItemOnMap()
{
    uint msg_len;
    uint item_id;
    ushort item_hx;
    ushort item_hy;
    bool is_added;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> item_id;
    const auto item_pid = _conn.InBuf.ReadHashedString(*this);
    _conn.InBuf >> item_hx;
    _conn.InBuf >> item_hy;
    _conn.InBuf >> is_added;

    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* item = CurMap->AddItem(item_id, item_pid, item_hx, item_hy, is_added, &_tempPropertiesData);
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
    _conn.InBuf >> item_id;
    _conn.InBuf >> is_deleted;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* item = CurMap->GetItem(item_id);
    if (item != nullptr) {
        OnItemMapOut.Fire(item);

        // Refresh borders
        if (!item->GetIsShootThru()) {
            _rebuildLookBordersRequest = true;
        }

        if (is_deleted) {
            item->SetHideAnim();
        }

        item->Finish();
    }
}

void FOClient::Net_OnAnimateItem()
{
    uint item_id;
    uchar from_frm;
    uchar to_frm;
    _conn.InBuf >> item_id;
    _conn.InBuf >> from_frm;
    _conn.InBuf >> to_frm;

    CHECK_IN_BUF_ERROR(_conn);

    auto* item = CurMap->GetItem(item_id);
    if (item != nullptr) {
        item->SetAnim(from_frm, to_frm);
    }
}

void FOClient::Net_OnCombatResult()
{
    uint msg_len;
    uint data_count;
    vector<uint> data_vec;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> data_count;

    if (data_count != 0u) {
        data_vec.resize(data_count);
        _conn.InBuf.Pop(data_vec.data(), data_count * sizeof(uint));
    }

    CHECK_IN_BUF_ERROR(_conn);

    OnCombatResult.Fire(data_vec);
}

void FOClient::Net_OnEffect()
{
    hstring eff_pid;
    ushort hx;
    ushort hy;
    ushort radius;
    eff_pid = _conn.InBuf.ReadHashedString(*this);
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> radius;

    CHECK_IN_BUF_ERROR(_conn);

    // Base hex effect
    CurMap->RunEffect(eff_pid, hx, hy, hx, hy);

    // Radius hexes effect
    if (radius > MAX_HEX_OFFSET) {
        radius = MAX_HEX_OFFSET;
    }

    const auto [sx, sy] = GeomHelper.GetHexOffsets((hx % 2) != 0);
    const auto maxhx = CurMap->GetWidth();
    const auto maxhy = CurMap->GetHeight();
    const auto count = GenericUtils::NumericalNumber(radius) * Settings.MapDirCount;

    for (uint i = 0; i < count; i++) {
        const auto ex = static_cast<short>(hx) + sx[i];
        const auto ey = static_cast<short>(hy) + sy[i];
        if (ex >= 0 && ey >= 0 && ex < maxhx && ey < maxhy) {
            CurMap->RunEffect(eff_pid, static_cast<ushort>(ex), static_cast<ushort>(ey), static_cast<ushort>(ex), static_cast<ushort>(ey));
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
    eff_pid = _conn.InBuf.ReadHashedString(*this);
    _conn.InBuf >> eff_cr1_id;
    _conn.InBuf >> eff_cr2_id;
    _conn.InBuf >> eff_cr1_hx;
    _conn.InBuf >> eff_cr1_hy;
    _conn.InBuf >> eff_cr2_hx;
    _conn.InBuf >> eff_cr2_hy;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    const auto* cr1 = CurMap->GetCritter(eff_cr1_id);
    if (cr1 != nullptr) {
        eff_cr1_hx = cr1->GetHexX();
        eff_cr1_hy = cr1->GetHexY();
    }

    const auto* cr2 = CurMap->GetCritter(eff_cr2_id);
    if (cr2 != nullptr) {
        eff_cr2_hx = cr2->GetHexX();
        eff_cr2_hy = cr2->GetHexY();
    }

    if (!CurMap->RunEffect(eff_pid, eff_cr1_hx, eff_cr1_hy, eff_cr2_hx, eff_cr2_hy)) {
        WriteLog("Run effect '{}' failed.\n", eff_pid);
    }
}

void FOClient::Net_OnPlaySound()
{
    uint msg_len;
    uint synchronize_crid;
    string sound_name;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> synchronize_crid;
    _conn.InBuf >> sound_name;

    CHECK_IN_BUF_ERROR(_conn);

    SndMngr.PlaySound(ResMngr.GetSoundNames(), sound_name);
}

void FOClient::Net_OnEndParseToGame()
{
    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in end parse to game.\n");
        return;
    }

    FlashGameWindow();
    ScreenFadeOut();

    if (CurMapPid) {
        CurMap->SkipItemsFade();
        CurMap->FindSetCenter(chosen->GetHexX(), chosen->GetHexY());
        dynamic_cast<CritterHexView*>(chosen)->AnimateStay();
        ShowMainScreen(SCREEN_GAME, {});
        CurMap->RebuildLight();
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
        _conn.InBuf >> msg_len;
    }
    else {
        msg_len = 0;
    }

    char type_ = 0;
    _conn.InBuf >> type_;
    const auto type = static_cast<NetProperty>(type_);

    uint cr_id;
    uint item_id;

    uint additional_args = 0;
    switch (type) {
    case NetProperty::CritterItem:
        additional_args = 2;
        _conn.InBuf >> cr_id;
        _conn.InBuf >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        _conn.InBuf >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        _conn.InBuf >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        _conn.InBuf >> item_id;
        break;
    default:
        break;
    }

    ushort property_index;
    _conn.InBuf >> property_index;

    if (data_size != 0u) {
        _tempPropertyData.resize(data_size);
        _conn.InBuf.Pop(_tempPropertyData.data(), data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
        _tempPropertyData.resize(len);
        if (len != 0u) {
            _conn.InBuf.Pop(_tempPropertyData.data(), len);
        }
    }

    CHECK_IN_BUF_ERROR(_conn);

    Entity* entity = nullptr;
    switch (type) {
    case NetProperty::Game:
        entity = this;
        break;
    case NetProperty::Player:
        entity = _curPlayer;
        break;
    case NetProperty::Critter:
        entity = CurMap != nullptr ? CurMap->GetCritter(cr_id) : nullptr;
        break;
    case NetProperty::Chosen:
        entity = GetChosen();
        break;
    case NetProperty::MapItem:
        entity = CurMap->GetItem(item_id);
        break;
    case NetProperty::CritterItem:
        if (CurMap != nullptr) {
            if (auto* cr = CurMap->GetCritter(cr_id); cr != nullptr) {
                entity = cr->GetItem(item_id);
            }
        }
        break;
    case NetProperty::ChosenItem:
        if (auto* chosen = GetChosen(); chosen != nullptr) {
            entity = chosen->GetItem(item_id);
        }
        break;
    case NetProperty::Map:
        entity = CurMap;
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
    _conn.InBuf >> msg_len;
    _conn.InBuf >> is_npc;
    _conn.InBuf >> talk_id;
    _conn.InBuf >> count_answ;

    CHECK_IN_BUF_ERROR(_conn);

    if (count_answ == 0u) {
        // End dialog
        if (IsScreenPresent(SCREEN_DIALOG)) {
            HideScreen(SCREEN_DIALOG);
        }
        return;
    }

    // Text params
    string lexems;
    _conn.InBuf >> lexems;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    // Find critter
    auto* npc = (is_npc != 0u ? CurMap->GetCritter(talk_id) : nullptr);
    _dlgIsNpc = is_npc;
    _dlgNpcId = talk_id;

    // Main text
    _conn.InBuf >> text_id;

    // Answers
    vector<uint> answers_texts;
    uint answ_text_id = 0;
    for (auto i = 0; i < count_answ; i++) {
        _conn.InBuf >> answ_text_id;
        answers_texts.push_back(answ_text_id);
    }

    CHECK_IN_BUF_ERROR(_conn);

    auto str = _curLang.Msg[TEXTMSG_DLG].GetStr(text_id);
    FormatTags(str, GetChosen(), npc, lexems);
    auto text_to_script = str;
    vector<string> answers_to_script;
    for (auto answers_text : answers_texts) {
        str = _curLang.Msg[TEXTMSG_DLG].GetStr(answers_text);
        FormatTags(str, GetChosen(), npc, lexems);
        answers_to_script.push_back(str);
    }

    _conn.InBuf >> talk_time;

    CHECK_IN_BUF_ERROR(_conn);

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
    _conn.InBuf >> year;
    _conn.InBuf >> month;
    _conn.InBuf >> day;
    _conn.InBuf >> hour;
    _conn.InBuf >> minute;
    _conn.InBuf >> second;
    _conn.InBuf >> multiplier;

    CHECK_IN_BUF_ERROR(_conn);

    auto* day_time = CurMap->GetMapDayTime();
    auto* day_color = CurMap->GetMapDayColor();
    _conn.InBuf.Pop(day_time, sizeof(int) * 4);
    _conn.InBuf.Pop(day_color, sizeof(uchar) * 12);

    CHECK_IN_BUF_ERROR(_conn);

    GameTime.Reset(year, month, day, hour, minute, second, multiplier);

    SetYear(year);
    SetMonth(month);
    SetDay(day);
    SetHour(hour);
    SetMinute(minute);
    SetSecond(second);
    SetTimeMultiplier(multiplier);

    SetDayTime(true);
}

void FOClient::Net_OnLoadMap()
{
    WriteLog("Change map...\n");

    uint msg_len;
    uint loc_id;
    uint map_id;
    uchar map_index_in_loc;
    uint hash_tiles;
    uint hash_scen;

    _conn.InBuf >> msg_len;
    _conn.InBuf >> loc_id;
    _conn.InBuf >> map_id;
    const auto loc_pid = _conn.InBuf.ReadHashedString(*this);
    const auto map_pid = _conn.InBuf.ReadHashedString(*this);
    _conn.InBuf >> map_index_in_loc;
    _conn.InBuf >> hash_tiles;
    _conn.InBuf >> hash_scen;

    CHECK_IN_BUF_ERROR(_conn);

    if (map_pid) {
        NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);
        NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesDataExt);
    }

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap != nullptr) {
        CurMap->MarkAsDestroyed();
        CurMap->Release();
        CurMap = nullptr;
    }

    if (_curLocation != nullptr) {
        _curLocation->MarkAsDestroyed();
        _curLocation->Release();
        _curLocation = nullptr;
    }

    if (map_pid) {
        _curLocation = new LocationView(this, loc_id, ProtoMngr.GetProtoLocation(loc_pid));
        _curLocation->RestoreData(_tempPropertiesDataExt);
        CurMap = new MapView(this, map_id, ProtoMngr.GetProtoMap(map_pid));
        CurMap->RestoreData(_tempPropertiesData);
    }

    Settings.SpritesZoom = 1.0f;

    CurMapPid = map_pid;
    _curMapLocPid = loc_pid;
    _curMapIndexInLoc = map_index_in_loc;
    SndMngr.StopSounds();
    ShowMainScreen(SCREEN_WAIT, {});
    ResMngr.ReinitializeDynamicAtlas();

    if (map_pid) {
        /*uint hash_tiles_cl;
        uint hash_scen_cl;
        CurMap->GetMapHash(Cache, map_pid, hash_tiles_cl, hash_scen_cl);

        if (hash_tiles != hash_tiles_cl || hash_scen != hash_scen_cl) {
            WriteLog("Obsolete map data (hashes {}:{}, {}:{}).\n", hash_tiles, hash_tiles_cl, hash_scen, hash_scen_cl);
            Net_SendGiveMap(false, map_pid, 0, hash_tiles_cl, hash_scen_cl);
            return;
        }

        CurMap = new MapView(this, )
        if (!CurMap->LoadMap(Cache, map_pid)) {
            WriteLog("Map not loaded. Disconnect.\n");
            NetDisconnect();
            return;
        }*/

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
    _conn.InBuf >> msg_len;
    const auto map_pid = _conn.InBuf.ReadHashedString(*this);
    _conn.InBuf >> maxhx;
    _conn.InBuf >> maxhy;
    _conn.InBuf >> send_tiles;
    _conn.InBuf >> send_scenery;

    CHECK_IN_BUF_ERROR(_conn);

    WriteLog("Map {} received...\n", map_pid);

    const string map_name = _str("{}.map", map_pid);
    auto tiles = false;
    char* tiles_data = nullptr;
    uint tiles_len = 0;
    auto scen = false;
    char* scen_data = nullptr;
    uint scen_len = 0;

    if (send_tiles) {
        _conn.InBuf >> tiles_len;
        if (tiles_len != 0u) {
            tiles_data = new char[tiles_len];
            _conn.InBuf.Pop(tiles_data, tiles_len);
        }
        tiles = true;
    }

    CHECK_IN_BUF_ERROR(_conn);

    if (send_scenery) {
        _conn.InBuf >> scen_len;
        if (scen_len != 0u) {
            scen_data = new char[scen_len];
            _conn.InBuf.Pop(scen_data, scen_len);
        }
        scen = true;
    }

    CHECK_IN_BUF_ERROR(_conn);

    if (auto compressed_cache = Cache.GetData(map_name); !compressed_cache.empty()) {
        if (const auto cache = Compressor::Uncompress(compressed_cache, 50); !cache.empty()) {
            auto caqche_reader = DataReader(cache);

            if (caqche_reader.Read<int>() == CLIENT_MAP_FORMAT_VER) {
                caqche_reader.Read<uint>();
                caqche_reader.Read<ushort>();
                caqche_reader.Read<ushort>();
                const auto old_tiles_len = caqche_reader.Read<uint>();
                const auto old_scen_len = caqche_reader.Read<uint>();

                if (!tiles) {
                    tiles_len = old_tiles_len;
                    tiles_data = new char[tiles_len];
                    caqche_reader.ReadPtr(tiles_data, tiles_len);
                    tiles = true;
                }
                else {
                    caqche_reader.ReadPtr<uchar>(tiles_len);
                }

                if (!scen) {
                    scen_len = old_scen_len;
                    scen_data = new char[scen_len];
                    caqche_reader.ReadPtr(scen_data, scen_len);
                    scen = true;
                }
            }
        }
    }

    if (tiles && scen) {
        vector<uchar> buf;
        auto buf_writer = DataWriter(buf);

        buf_writer.Write<int>(CLIENT_MAP_FORMAT_VER);
        buf_writer.Write<uint>(map_pid.as_uint());
        buf_writer.Write<ushort>(maxhx);
        buf_writer.Write<ushort>(maxhy);
        buf_writer.Write<uint>(tiles_len);
        buf_writer.Write<uint>(scen_len);
        buf_writer.WritePtr(tiles_data, tiles_len);
        buf_writer.WritePtr(scen_data, scen_len);

        Cache.SetData(map_name, buf);
    }
    else {
        WriteLog("Not for all data of map, disconnect.\n");
        _conn.Disconnect();
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
    _conn.InBuf >> msg_len;
    _conn.InBuf >> info_flags;

    CHECK_IN_BUF_ERROR(_conn);

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        _worldmapLoc.clear();

        ushort count_loc;
        _conn.InBuf >> count_loc;

        for (auto i = 0; i < count_loc; i++) {
            GmapLocation loc;
            _conn.InBuf >> loc.LocId;
            loc.LocPid = _conn.InBuf.ReadHashedString(*this);
            _conn.InBuf >> loc.LocWx;
            _conn.InBuf >> loc.LocWy;
            _conn.InBuf >> loc.Radius;
            _conn.InBuf >> loc.Color;
            _conn.InBuf >> loc.Entrances;

            if (loc.LocId != 0u) {
                _worldmapLoc.push_back(loc);
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_LOCATION)) {
        bool add;
        GmapLocation loc;
        _conn.InBuf >> add;
        _conn.InBuf >> loc.LocId;
        loc.LocPid = _conn.InBuf.ReadHashedString(*this);
        _conn.InBuf >> loc.LocWx;
        _conn.InBuf >> loc.LocWy;
        _conn.InBuf >> loc.Radius;
        _conn.InBuf >> loc.Color;
        _conn.InBuf >> loc.Entrances;

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
        _conn.InBuf.Pop(fog_data, GM_ZONES_FOG_SIZE);
    }

    if (IsBitSet(info_flags, GM_INFO_FOG)) {
        ushort zx;
        ushort zy;
        uchar fog;
        _conn.InBuf >> zx;
        _conn.InBuf >> zy;
        _conn.InBuf >> fog;

        _worldmapFog.Set2Bit(zx, zy, fog);
    }

    if (IsBitSet(info_flags, GM_INFO_CRITTERS)) {
        for (auto* cr : _worldmapCritters) {
            cr->MarkAsDestroyed();
            cr->Release();
        }
        _worldmapCritters.clear();
    }

    CHECK_IN_BUF_ERROR(_conn);
}

void FOClient::Net_OnSomeItems()
{
    uint msg_len;
    int param;
    bool is_null;
    uint items_count;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> param;
    _conn.InBuf >> is_null;
    _conn.InBuf >> items_count;

    CHECK_IN_BUF_ERROR(_conn);

    vector<ItemView*> item_container;
    for (uint i = 0; i < items_count; i++) {
        uint item_id;
        hstring item_pid;
        _conn.InBuf >> item_id;
        item_pid = _conn.InBuf.ReadHashedString(*this);
        NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

        const auto* proto_item = ProtoMngr.GetProtoItem(item_pid);
        if (item_id != 0u && proto_item != nullptr) {
            auto* item = new ItemView(this, item_id, proto_item);
            item->RestoreData(_tempPropertiesData);
            item_container.push_back(item);
        }
    }

    CHECK_IN_BUF_ERROR(_conn);

    OnReceiveItems.Fire(item_container, param);
}

void FOClient::Net_OnAutomapsInfo()
{
    uint msg_len;
    bool clear;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> clear;

    if (clear) {
        _automaps.clear();
    }

    ushort locs_count;
    _conn.InBuf >> locs_count;

    CHECK_IN_BUF_ERROR(_conn);

    for (ushort i = 0; i < locs_count; i++) {
        uint loc_id;
        hstring loc_pid;
        ushort maps_count;
        _conn.InBuf >> loc_id;
        loc_pid = _conn.InBuf.ReadHashedString(*this);
        _conn.InBuf >> maps_count;

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
                map_pid = _conn.InBuf.ReadHashedString(*this);
                _conn.InBuf >> map_index_in_loc;

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

    CHECK_IN_BUF_ERROR(_conn);
}

void FOClient::Net_OnViewMap()
{
    ushort hx;
    ushort hy;
    uint loc_id;
    uint loc_ent;
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> loc_id;
    _conn.InBuf >> loc_ent;

    CHECK_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    CurMap->FindSetCenter(hx, hy);
    ShowMainScreen(SCREEN_GAME, {});
    ScreenFadeOut();
    CurMap->RebuildLight();

    /*CScriptDictionary* dict = CScriptDictionary::Create(ScriptSys.GetEngine());
    dict->Set("LocationId", &loc_id, asTYPEID_UINT32);
    dict->Set("LocationEntrance", &loc_ent, asTYPEID_UINT32);
    ShowScreen(SCREEN__TOWN_VIEW, dict);
    dict->Release();*/
}

void FOClient::SetDayTime(bool refresh)
{
    if (refresh) {
        _prevDayTimeColor.reset();
    }

    if (CurMap == nullptr) {
        return;
    }

    auto color = GenericUtils::GetColorDay(CurMap->GetMapDayTime(), CurMap->GetMapDayColor(), CurMap->GetMapTime(), nullptr);
    color = COLOR_GAME_RGB(static_cast<int>((color >> 16) & 0xFF), static_cast<int>((color >> 8) & 0xFF), static_cast<int>(color & 0xFF));

    if (!_prevDayTimeColor.has_value() || _prevDayTimeColor != color) {
        _prevDayTimeColor = color;
        SprMngr.SetSpritesColor(color);
        CurMap->RefreshMap();
    }
}

void FOClient::ProcessGlobalMap()
{
    // Todo: global map critters
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
            _conn.Disconnect();
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

    auto* cr = dynamic_cast<CritterHexView*>(entity);
    cr->RefreshModel();
    cr->Action(ACTION_REFRESH, 0, nullptr, false);
}

void FOClient::OnSetCritterContourColor(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(old_value);

    auto* cr = dynamic_cast<CritterHexView*>(entity);
    if (cr->SprDrawValid) {
        cr->SprDraw->SetContour(cr->SprDraw->ContourType, *static_cast<uint*>(cur_value));
    }
}

void FOClient::OnSendItemValue(Entity* entity, const Property* prop)
{
    if (auto* item = dynamic_cast<ItemView*>(entity); item != nullptr && item->GetId() != 0u) {
        if (item->GetOwnership() == ItemOwnership::CritterInventory) {
            const auto* cr = CurMap->GetCritter(item->GetCritId());
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
    if (item->GetOwnership() == ItemOwnership::MapHex && CurMap != nullptr) {
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
            CurMap->RebuildLight();
            rebuild_cache = true;
        }
        else if (prop == hex_item->GetPropertyIsNoBlock()) {
            rebuild_cache = true;
        }
        if (rebuild_cache) {
            CurMap->GetField(hex_item->GetHexX(), hex_item->GetHexY()).ProcessCache();
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

    if (CurMap != nullptr) {
        CurMap->RebuildLight();
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

void FOClient::OnSetItemOffsetCoords(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    // OffsetX, OffsetY

    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(cur_value);
    UNUSED_VARIABLE(old_value);

    auto* item = dynamic_cast<ItemView*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex && CurMap != nullptr) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->SetAnimOffs();
        CurMap->ProcessHexBorders(hex_item);
    }
}

void FOClient::OnSetItemOpened(Entity* entity, const Property* prop, void* cur_value, void* old_value)
{
    UNUSED_VARIABLE(prop);

    auto* item = dynamic_cast<ItemView*>(entity);
    const auto cur = *static_cast<bool*>(cur_value);
    const auto old = *static_cast<bool*>(old_value);

    if (item->GetIsCanOpen()) {
        auto* hex_item = dynamic_cast<ItemHexView*>(item);
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
    RUNTIME_ASSERT(entity == CurMap);

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Map, prop, CurMap);
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
        CurMap->SetCursorPos(GetMapChosen(), Settings.MouseX, Settings.MouseY, Keyb.CtrlDwn, false);
    }

    // Look borders
    if (_rebuildLookBordersRequest) {
        LookBordersPrepare();
        _rebuildLookBordersRequest = false;
    }

    // Map
    CurMap->DrawMap();
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

    const auto* chosen = GetMapChosen();
    if (chosen == nullptr) {
        return;
    }

    const auto maxpixx = (_lmapWMap[2] - _lmapWMap[0]) / 2 / _lmapZoom;
    const auto maxpixy = (_lmapWMap[3] - _lmapWMap[1]) / 2 / _lmapZoom;
    const auto bx = chosen->GetHexX() - maxpixx;
    const auto by = chosen->GetHexY() - maxpixy;
    const auto ex = chosen->GetHexX() + maxpixx;
    const auto ey = chosen->GetHexY() + maxpixy;

    const auto vis = chosen->GetLookDistance();
    auto pix_x = _lmapWMap[2] - _lmapWMap[0];
    auto pix_y = 0;

    for (auto i1 = bx; i1 < ex; i1++) {
        for (auto i2 = by; i2 < ey; i2++) {
            pix_y += _lmapZoom;
            if (i1 < 0 || i2 < 0 || i1 >= CurMap->GetWidth() || i2 >= CurMap->GetHeight()) {
                continue;
            }

            auto is_far = false;
            const auto dist = GeomHelper.DistGame(chosen->GetHexX(), chosen->GetHexY(), i1, i2);
            if (dist > vis) {
                is_far = true;
            }

            auto& f = CurMap->GetField(static_cast<ushort>(i1), static_cast<ushort>(i2));
            uint cur_color;

            if (f.Crit != nullptr) {
                cur_color = (f.Crit == chosen ? 0xFF0000FF : 0xFFFF0000);
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

    for (auto* cr : _worldmapCritters) {
        cr->MarkAsDestroyed();
        cr->Release();
    }
    _worldmapCritters.clear();
}

void FOClient::WaitDraw()
{
    SprMngr.DrawSpriteSize(_waitPic->GetCurSprId(GameTime.GameTick()), 0, 0, Settings.ScreenWidth, Settings.ScreenHeight, true, true, 0);
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
        }
    }
    else if (cmd == "DumpAtlases") {
        SprMngr.DumpAtlases();
    }
    else if (cmd == "SwitchShowTrack") {
        CurMap->SwitchShowTrack();
    }
    else if (cmd == "SwitchShowHex") {
        CurMap->SwitchShowHex();
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
        if (CurMap != nullptr) {
            CurMap->SetCursorPos(GetMapChosen(), Settings.MouseX, Settings.MouseY, Keyb.CtrlDwn, true);
        }
    }
    else if (cmd == "NetDisconnect") {
        _conn.Disconnect();

        if (!_conn.IsConnected() && !IsMainScreen(SCREEN_LOGIN)) {
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
        return _str("{}", _conn.GetBytesSend());
    }
    else if (cmd == "BytesReceive") {
        return _str("{}", _conn.GetBytesReceived());
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
        if (CurMap != nullptr) {
            CurMap->OnResolutionChanged();
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
                str, &_conn.OutBuf,
                [&buf, &separator](auto s) {
                    buf += s;
                    buf += separator;
                },
                *this)) {
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
        CurMap->SetCrittersContour(countour_type);
    }
    else if (cmd == "DrawWait") {
        WaitDraw();
    }
    else if (cmd == "ChangeDir" && args.size() == 2) {
        auto dir = _str(args[1]).toInt();
        GetMapChosen()->ChangeDir(static_cast<uchar>(dir), true);
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
            GetMapChosen()->Action(ACTION_DROP_ITEM, from_slot, item, true);
            if (item->GetStackable() && item_count < item->GetCount()) {
                item->SetCount(item->GetCount() - item_count);
            }
            else {
                GetMapChosen()->DeleteItem(item, true);
                item = nullptr;
            }
        }
        else {
            item->SetCritSlot(static_cast<uchar>(to_slot));
            if (item_swap) {
                item_swap->SetCritSlot(static_cast<uchar>(from_slot));
            }

            GetMapChosen()->Action(ACTION_MOVE_ITEM, from_slot, item, true);
            if (item_swap) {
                GetMapChosen()->Action(ACTION_MOVE_ITEM_SWAP, to_slot, item_swap, true);
            }
        }

        // Light
        _rebuildLookBordersRequest = true;
        if (is_light && (!to_slot || (!from_slot && to_slot != -1))) {
            CurMap->RebuildLight();
        }

        // Notify scripts about item changing
        OnItemInvChanged.Fire(item, old_item);
        old_item->Release();
    }
    else if (cmd == "SkipRoof" && args.size() == 3) {
        auto hx = _str(args[1]).toUInt();
        auto hy = _str(args[2]).toUInt();
        CurMap->SetSkipRoof(hx, hy);
    }
    else if (cmd == "RebuildLookBorders") {
        _rebuildLookBordersRequest = true;
    }
    else if (cmd == "TransitCritter" && args.size() == 5) {
        auto hx = _str(args[1]).toInt();
        auto hy = _str(args[2]).toInt();
        auto animate = _str(args[3]).toBool();
        auto force = _str(args[4]).toBool();

        CurMap->TransitCritter(GetMapChosen(), hx, hy, animate, force);
    }
    else if (cmd == "SendMove") {
        vector<uchar> dirs;
        for (size_t i = 1; i < args.size(); i++) {
            dirs.push_back(static_cast<uchar>(_str(args[i]).toInt()));
        }

        Net_SendMove(dirs);

        if (dirs.size() > 1) {
            GetMapChosen()->MoveSteps.resize(1);
        }
        else {
            GetMapChosen()->MoveSteps.resize(0);
            if (!GetMapChosen()->IsAnim()) {
                GetMapChosen()->AnimateStay();
            }
        }
    }
    else if (cmd == "ChosenAlpha" && args.size() == 2) {
        auto alpha = _str(args[1]).toInt();

        GetMapChosen()->Alpha = static_cast<uchar>(alpha);
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
