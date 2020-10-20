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

#include "GenericUtils.h"
#include "Log.h"
#include "MessageBox.h"
#include "NetCommand.h"
#include "StringUtils.h"
#include "Version-Include.h"

#include "sha1.h"
#include "sha2.h"

// Todo: remove CHECK_IN_BUFF_ERROR();
#define CHECK_IN_BUFF_ERROR() \
    do { \
        if (Bin.IsError()) { \
            WriteLog("Wrong network data!\n"); \
            NetDisconnect(); \
            return; \
        } \
    } while (0)

FOClient::FOClient(GlobalSettings& settings) : Settings {settings}, GeomHelper(Settings), ScriptSys(this, settings, FileMngr), GameTime(Settings), ProtoMngr(FileMngr), EffectMngr(Settings, FileMngr, GameTime), SprMngr(Settings, FileMngr, EffectMngr, ScriptSys, GameTime), ResMngr(FileMngr, SprMngr, ScriptSys), HexMngr(false, Settings, ProtoMngr, SprMngr, EffectMngr, ResMngr, ScriptSys, GameTime), SndMngr(Settings, FileMngr), Keyb(Settings, SprMngr), Cache("Data/Cache.fobin"), GmapFog(GM_MAXZONEX, GM_MAXZONEY, nullptr)
{
    Globals = new GlobalVars();
    ComBuf.resize(NetBuffer::DEFAULT_BUF_SIZE);
    Sock = INVALID_SOCKET;
    InitNetReason = INIT_NET_REASON_NONE;
    Animations.resize(10000);
    LmapZoom = 2;
    ScreenModeMain = SCREEN_WAIT;
    DrawLookBorders = true;
    FpsTick = GameTime.FrameTick();

    const auto [w, h] = SprMngr.GetWindowSize();
    const auto [x, y] = SprMngr.GetMousePosition();
    Settings.MouseX = std::clamp(x, 0, w - 1);
    Settings.MouseY = std::clamp(y, 0, h - 1);

    SetGameColor(COLOR_IFACE);

    // Data sources
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
    WaitPic = ResMngr.GetRandomSplash();
    SprMngr.BeginScene(COLOR_RGB(0, 0, 0));
    WaitDraw();
    SprMngr.EndScene();

    // Recreate static atlas
    SprMngr.AccumulateAtlasData();
    SprMngr.PushAtlasType(AtlasType::Static);

#if !FO_SINGLEPLAYER
    // Modules initialization
    ScriptSys.StartEvent();
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
    if (Sock != INVALID_SOCKET) {
        closesocket(Sock);
    }
    if (ZStreamOk) {
        inflateEnd(&ZStream);
    }
}

void FOClient::ProcessAutoLogin()
{
    const auto auto_login = Settings.AutoLogin;

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
#endif

    if (auto_login.empty()) {
        return;
    }

    auto auto_login_args = _str(auto_login).split(' ');
    if (auto_login_args.size() != 2) {
        return;
    }

    IsAutoLogin = true;

#if !FO_SINGLEPLAYER
    if (ScriptSys.AutoLoginEvent(auto_login_args[0], auto_login_args[1]) && InitNetReason == INIT_NET_REASON_NONE) {
        LoginName = auto_login_args[0];
        LoginPassword = auto_login_args[1];
        InitNetReason = INIT_NET_REASON_LOGIN;
    }
#endif
}

void FOClient::UpdateFilesLoop()
{
    // Was aborted
    if (Update->Aborted) {
        return;
    }

    // Update indication
    if (InitCalls < 2) {
        // Load font
        if (!Update->FontLoaded) {
            SprMngr.PushAtlasType(AtlasType::Static);
            Update->FontLoaded = SprMngr.LoadFontFO(FONT_DEFAULT, "Default", false, true);
            if (!Update->FontLoaded) {
                Update->FontLoaded = SprMngr.LoadFontBmf(FONT_DEFAULT, "Default");
            }
            if (Update->FontLoaded) {
                SprMngr.BuildFonts();
            }
            SprMngr.PopAtlasType();
        }

        // Running dots
        const int dots = (GameTime.FrameTick() - Update->Duration) / 100 % 50 + 1;
        string dots_str;
        for (auto i = 0; i < dots; i++) {
            dots_str += ".";
        }

        // State
        SprMngr.BeginScene(COLOR_RGB(50, 50, 50));
        const auto update_text = Update->Messages + Update->Progress + dots_str;
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
    if (!IsConnected) {
        if (Update->Connecting) {
            Update->Connecting = false;
            UpdateFilesAddText(STR_CANT_CONNECT_TO_SERVER, "Can't connect to server!");
        }

        if (GameTime.FrameTick() < Update->ConnectionTimeout) {
            return;
        }
        Update->ConnectionTimeout = GameTime.FrameTick() + 5000;

        // Connect to server
        UpdateFilesAddText(STR_CONNECT_TO_SERVER, "Connect to server...");
        NetConnect(Settings.Host, Settings.Port);
        Update->Connecting = true;
    }
    else {
        if (Update->Connecting) {
            Update->Connecting = false;
            UpdateFilesAddText(STR_CONNECTION_ESTABLISHED, "Connection established.");

            // Update
            UpdateFilesAddText(STR_CHECK_UPDATES, "Check updates...");
            Update->Messages = "";

            // Data synchronization
            UpdateFilesAddText(STR_DATA_SYNCHRONIZATION, "Data synchronization...");

            // Clean up
            Update->ClientOutdated = false;
            Update->CacheChanged = false;
            Update->FilesChanged = false;
            Update->FileListReceived = false;
            Update->FileList.clear();
            Update->FileDownloading = false;
            FileMngr.DeleteFile("Update.fobin");
            Update->Duration = GameTime.FrameTick();

            Net_SendUpdate();
        }

        Update->Progress = "";
        if (!Update->FileList.empty()) {
            Update->Progress += "\n";
            for (auto& update_file : Update->FileList) {
                const auto cur = static_cast<float>(update_file.Size - update_file.RemaningSize) / (1024.0f * 1024.0f);
                const auto max = std::max(static_cast<float>(update_file.Size) / (1024.0f * 1024.0f), 0.01f);
                const string name = _str(update_file.Name).formatPath();
                Update->Progress += _str("{} {:.2f} / {:.2f} MB\n", name, cur, max);
            }
            Update->Progress += "\n";
        }

        if (Update->FileListReceived && !Update->FileDownloading) {
            if (!Update->FileList.empty()) {
                auto& update_file = Update->FileList.front();

                if (Update->TempFile) {
                    Update->TempFile = nullptr;
                }

                if (update_file.Name[0] == '$') {
                    Update->TempFile.reset(new OutputFile {FileMngr.WriteFile("Update.fobin", false)});
                    Update->CacheChanged = true;
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
                    Update->TempFile.reset(new OutputFile {FileMngr.WriteFile("Update.fobin", false)});
                    Update->FilesChanged = true;
                }

                if (!Update->TempFile) {
                    UpdateFilesAddText(STR_FILESYSTEM_ERROR, "File system error!");
                    NetDisconnect();
                    return;
                }

                Update->FileDownloading = true;

                Bout << NETMSG_GET_UPDATE_FILE;
                Bout << Update->FileList.front().Index;
            }
            else {
                // Done
                Update.reset();

                // Disconnect
                if (InitCalls < 2) {
                    NetDisconnect();
                }

                // Update binaries
                if (Update->ClientOutdated) {
                    throw GenericException("Client outdated");
                }

                // Reinitialize data
                if (Update->CacheChanged) {
                    CurLang.LoadFromCache(Cache, CurLang.Name);
                }
                // if (Update->FilesChanged)
                //    Settings.Init(0, {});
                if (InitCalls >= 2 && (Update->CacheChanged || Update->FilesChanged)) {
                    throw ClientRestartException("Restart");
                }

                return;
            }
        }

        ParseSocket();

        if (!IsConnected && !Update->Aborted) {
            UpdateFilesAddText(STR_CONNECTION_FAILTURE, "Connection failure!");
        }
    }
}

void FOClient::UpdateFilesAddText(uint num_str, const string& num_str_str)
{
    if (Update->FontLoaded) {
        const auto text = (CurLang.Msg[TEXTMSG_GAME].Count(num_str) != 0u ? CurLang.Msg[TEXTMSG_GAME].GetStr(num_str) : num_str_str);
        Update->Messages += text + "\n";
    }
}

void FOClient::UpdateFilesAbort(uint num_str, const string& num_str_str)
{
    Update->Aborted = true;

    UpdateFilesAddText(num_str, num_str_str);
    NetDisconnect();

    if (Update->TempFile) {
        Update->TempFile = nullptr;
    }

    SprMngr.BeginScene(COLOR_RGB(255, 0, 0));
    SprMngr.DrawStr(IRect(0, 0, Settings.ScreenWidth, Settings.ScreenHeight), Update->Messages, FT_CENTERX | FT_CENTERY | FT_BORDERED, COLOR_TEXT_WHITE, FONT_DEFAULT);
    SprMngr.EndScene();
}

void FOClient::DeleteCritters()
{
    HexMngr.DeleteCritters();
    Chosen = nullptr;
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
        if ((f.Crit != nullptr) && f.Crit->IsFinishing()) {
            DeleteCritter(f.Crit->GetId());
        }
    }
    if (cr->IsChosen()) {
        Chosen = cr;
    }
    HexMngr.AddCritter(cr);
    cr->FadingTick = GameTime.GameTick() + FADING_PERIOD - (fading > GameTime.GameTick() ? fading - GameTime.GameTick() : 0);
}

void FOClient::DeleteCritter(uint remid)
{
    if ((Chosen != nullptr) && Chosen->GetId() == remid) {
        Chosen = nullptr;
    }
    HexMngr.DeleteCritter(remid);
}

void FOClient::LookBordersPrepare()
{
    LookBorders.clear();
    ShootBorders.clear();

    if (Chosen == nullptr || !HexMngr.IsMapLoaded() || (!DrawLookBorders && !DrawShootBorders)) {
        HexMngr.SetFog(LookBorders, ShootBorders, nullptr, nullptr);
        return;
    }

    const auto dist = Chosen->GetLookDistance();
    const auto base_hx = Chosen->GetHexX();
    const auto base_hy = Chosen->GetHexY();
    int hx = base_hx;
    int hy = base_hy;
    const int chosen_dir = Chosen->GetDir();
    const auto dist_shoot = Chosen->GetAttackDist();
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
                GeomHelper.MoveHexByDirUnsafe(hx, hy, dir);
            }

            ushort hx_ = std::clamp(hx, 0, maxhx - 1);
            ushort hy_ = std::clamp(hy, 0, maxhy - 1);
            if (IsBitSet(Settings.LookChecks, LOOK_CHECK_DIR)) {
                const int dir_ = GeomHelper.GetFarDir(base_hx, base_hy, hx_, hy_);
                auto ii = (chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir);
                if (ii > Settings.MapDirCount / 2) {
                    ii = Settings.MapDirCount - ii;
                }
                const auto dist_ = dist - dist * Settings.LookDir[ii] / 100;
                pair<ushort, ushort> block;
                HexMngr.TraceBullet(base_hx, base_hy, hx_, hy_, dist_, 0.0f, nullptr, false, nullptr, 0, nullptr, &block, nullptr, false);
                hx_ = block.first;
                hy_ = block.second;
            }

            if (IsBitSet(Settings.LookChecks, LOOK_CHECK_TRACE)) {
                pair<ushort, ushort> block;
                HexMngr.TraceBullet(base_hx, base_hy, hx_, hy_, 0, 0.0f, nullptr, false, nullptr, 0, nullptr, &block, nullptr, true);
                hx_ = block.first;
                hy_ = block.second;
            }

            auto dist_look = GeomHelper.DistGame(base_hx, base_hy, hx_, hy_);
            if (DrawLookBorders) {
                int x = 0;
                int y = 0;
                HexMngr.GetHexCurrentPosition(hx_, hy_, x, y);
                auto* ox = (dist_look == dist ? &Chosen->SprOx : nullptr);
                auto* oy = (dist_look == dist ? &Chosen->SprOy : nullptr);
                LookBorders.push_back({x + (Settings.MapHexWidth / 2), y + (Settings.MapHexHeight / 2), COLOR_RGBA(0, 255, dist_look * 255 / dist, 0), ox, oy});
            }

            if (DrawShootBorders) {
                auto hx__ = hx_;
                auto hy__ = hy_;
                pair<ushort, ushort> block;
                const auto max_shoot_dist = std::max(std::min(dist_look, dist_shoot), 0u) + 1u;
                HexMngr.TraceBullet(base_hx, base_hy, hx_, hy_, max_shoot_dist, 0.0f, nullptr, false, nullptr, 0, nullptr, &block, nullptr, true);
                hx__ = block.first;
                hy__ = block.second;

                int x_ = 0;
                int y_ = 0;
                HexMngr.GetHexCurrentPosition(hx__, hy__, x_, y_);
                const auto result_shoot_dist = GeomHelper.DistGame(base_hx, base_hy, hx__, hy__);
                auto* ox = (result_shoot_dist == max_shoot_dist ? &Chosen->SprOx : nullptr);
                auto* oy = (result_shoot_dist == max_shoot_dist ? &Chosen->SprOy : nullptr);
                ShootBorders.push_back({x_ + (Settings.MapHexWidth / 2), y_ + (Settings.MapHexHeight / 2), COLOR_RGBA(255, 255, result_shoot_dist * 255 / max_shoot_dist, 0), ox, oy});
            }
        }
    }

    int base_x = 0;
    int base_y = 0;
    HexMngr.GetHexCurrentPosition(base_hx, base_hy, base_x, base_y);
    if (!LookBorders.empty()) {
        LookBorders.push_back(*LookBorders.begin());
        LookBorders.insert(LookBorders.begin(), {base_x + (Settings.MapHexWidth / 2), base_y + (Settings.MapHexHeight / 2), COLOR_RGBA(0, 0, 0, 0), &Chosen->SprOx, &Chosen->SprOy});
    }
    if (!ShootBorders.empty()) {
        ShootBorders.push_back(*ShootBorders.begin());
        ShootBorders.insert(ShootBorders.begin(), {base_x + (Settings.MapHexWidth / 2), base_y + (Settings.MapHexHeight / 2), COLOR_RGBA(255, 0, 0, 0), &Chosen->SprOx, &Chosen->SprOy});
    }

    HexMngr.SetFog(LookBorders, ShootBorders, &Chosen->SprOx, &Chosen->SprOy);
}

void FOClient::MainLoop()
{
    const auto time_changed = GameTime.FrameAdvance();

    // FPS counter
    if (GameTime.FrameTick() - FpsTick >= 1000) {
        Settings.FPS = FpsCounter;
        FpsCounter = 0;
        FpsTick = GameTime.FrameTick();
    }
    else {
        FpsCounter++;
    }

    // Poll pending events
    if (InitCalls < 2) {
        InputEvent event;
        while (App->Input.PollEvent(event)) {
        }
    }

    // Game end
    if (Settings.Quit) {
        return;
    }

    // Network connection
    if (IsConnecting) {
        if (!CheckSocketStatus(true)) {
            return;
        }
    }

    if (Update) {
        UpdateFilesLoop();
        return;
    }

    if (InitCalls < 2) {
        if (InitCalls == 0) {
            InitCalls = 1;
        }

        InitCalls++;

        if (InitCalls == 1) {
            Update = ClientUpdate();
        }
        else if (InitCalls == 2) {
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
    if (InitNetReason != INIT_NET_REASON_NONE && !Update) {
        // Connect to server
        if (!IsConnected) {
            if (!NetConnect(Settings.Host, Settings.Port)) {
                ShowMainScreen(SCREEN_LOGIN, {});
                AddMess(FOMB_GAME, CurLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_FAIL));
            }
        }
        else {
            // Reason
            const auto reason = InitNetReason;
            InitNetReason = INIT_NET_REASON_NONE;

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
    if (IsConnected) {
        ParseSocket();
    }

    // Exit in Login screen if net disconnect
    if (!IsConnected && !IsMainScreen(SCREEN_LOGIN)) {
        ShowMainScreen(SCREEN_LOGIN, {});
    }

    // Input
    ProcessInputEvents();

    // Process
    AnimProcess();

    // Game time
    if (time_changed) {
        if (Globals != nullptr) {
            const auto st = GameTime.GetGameTime(GameTime.GetFullSecond());
            Globals->SetYear(st.Year);
            Globals->SetMonth(st.Month);
            Globals->SetDay(st.Day);
            Globals->SetHour(st.Hour);
            Globals->SetMinute(st.Minute);
            Globals->SetSecond(st.Second);
        }

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
    ScriptSys.LoopEvent();
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
    ScriptSys.RenderIfaceEvent();
    CanDrawInScripts = false;
}

void FOClient::ScreenFade(uint time, uint from_color, uint to_color, bool push_back)
{
    if (!push_back || ScreenEffects.empty()) {
        ScreenEffects.push_back({GameTime.FrameTick(), time, from_color, to_color});
    }
    else {
        uint last_tick = 0;
        for (auto& e : ScreenEffects) {
            if (e.BeginTick + e.Time > last_tick) {
                last_tick = e.BeginTick + e.Time;
            }
        }
        ScreenEffects.push_back({last_tick, time, from_color, to_color});
    }
}

void FOClient::ScreenQuake(int noise, uint time)
{
    Settings.ScrOx -= ScreenOffsX;
    Settings.ScrOy -= ScreenOffsY;
    ScreenOffsX = GenericUtils::Random(0, 1) != 0 ? noise : -noise;
    ScreenOffsY = GenericUtils::Random(0, 1) != 0 ? noise : -noise;
    ScreenOffsXf = static_cast<float>(ScreenOffsX);
    ScreenOffsYf = static_cast<float>(ScreenOffsY);
    ScreenOffsStep = fabs(ScreenOffsXf) / (time / 30);
    Settings.ScrOx += ScreenOffsX;
    Settings.ScrOy += ScreenOffsY;
    ScreenOffsNextTick = GameTime.GameTick() + 30;
}

void FOClient::ProcessScreenEffectFading()
{
    SprMngr.Flush();

    PrimitivePoints full_screen_quad;
    SprMngr.PrepareSquare(full_screen_quad, IRect(0, 0, Settings.ScreenWidth, Settings.ScreenHeight), 0);

    for (auto it = ScreenEffects.begin(); it != ScreenEffects.end();) {
        auto& screen_effect = *it;

        if (GameTime.FrameTick() >= screen_effect.BeginTick + screen_effect.Time) {
            it = ScreenEffects.erase(it);
            continue;
        }

        if (GameTime.FrameTick() >= screen_effect.BeginTick) {
            const auto proc = GenericUtils::Percent(screen_effect.Time, GameTime.FrameTick() - screen_effect.BeginTick) + 1;
            int res[4];

            for (auto i = 0; i < 4; i++) {
                const int sc = (reinterpret_cast<uchar*>(&screen_effect.StartColor))[i];
                const int ec = (reinterpret_cast<uchar*>(&screen_effect.EndColor))[i];
                const auto dc = ec - sc;
                res[i] = sc + dc * proc / 100;
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
    if ((ScreenOffsX != 0 || ScreenOffsY != 0) && GameTime.GameTick() >= ScreenOffsNextTick) {
        Settings.ScrOx -= ScreenOffsX;
        Settings.ScrOy -= ScreenOffsY;

        if (ScreenOffsXf < 0.0f) {
            ScreenOffsXf += ScreenOffsStep;
        }
        else if (ScreenOffsXf > 0.0f) {
            ScreenOffsXf -= ScreenOffsStep;
        }

        if (ScreenOffsYf < 0.0f) {
            ScreenOffsYf += ScreenOffsStep;
        }
        else if (ScreenOffsYf > 0.0f) {
            ScreenOffsYf -= ScreenOffsStep;
        }

        ScreenOffsXf = -ScreenOffsXf;
        ScreenOffsYf = -ScreenOffsYf;
        ScreenOffsX = static_cast<int>(ScreenOffsXf);
        ScreenOffsY = static_cast<int>(ScreenOffsYf);

        Settings.ScrOx += ScreenOffsX;
        Settings.ScrOy += ScreenOffsY;

        ScreenOffsNextTick = GameTime.GameTick() + 30;
    }
}

void FOClient::ProcessInputEvents()
{
    // Stop processing if window not active
    if (!SprMngr.IsWindowFocused()) {
        InputEvent event;
        while (App->Input.PollEvent(event)) {
        }

        Keyb.Lost();
        ScriptSys.InputLostEvent();
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
        auto key_code = event.KeyDown.Code;
        const auto key_text = event.KeyDown.Text;

        if (key_code == KeyCode::DIK_RCONTROL || key_code == KeyCode::DIK_LCONTROL) {
            Keyb.CtrlDwn = true;
        }
        else if (key_code == KeyCode::DIK_LMENU || key_code == KeyCode::DIK_RMENU) {
            Keyb.AltDwn = true;
        }
        else if (key_code == KeyCode::DIK_LSHIFT || key_code == KeyCode::DIK_RSHIFT) {
            Keyb.ShiftDwn = true;
        }

        ScriptSys.KeyDownEvent(static_cast<int>(key_code), key_text);
    }
    else if (event.Type == InputEvent::EventType::KeyUpEvent) {
        KeyCode key_code = event.KeyUp.Code;

        if (key_code == KeyCode::DIK_RCONTROL || key_code == KeyCode::DIK_LCONTROL) {
            Keyb.CtrlDwn = false;
        }
        else if (key_code == KeyCode::DIK_LMENU || key_code == KeyCode::DIK_RMENU) {
            Keyb.AltDwn = false;
        }
        else if (key_code == KeyCode::DIK_LSHIFT || key_code == KeyCode::DIK_RSHIFT) {
            Keyb.ShiftDwn = false;
        }

        ScriptSys.KeyUpEvent(static_cast<int>(key_code));
    }
    else if (event.Type == InputEvent::EventType::MouseMoveEvent) {
        const int mouse_x = event.MouseMove.MouseX;
        const int mouse_y = event.MouseMove.MouseY;
        const int delta_x = event.MouseMove.DeltaX;
        const int delta_y = event.MouseMove.DeltaY;

        Settings.MouseX = mouse_x;
        Settings.MouseY = mouse_y;

        ScriptSys.MouseMoveEvent(delta_x, delta_y);
    }
    else if (event.Type == InputEvent::EventType::MouseDownEvent) {
        MouseButton mouse_button = event.MouseDown.Button;

        ScriptSys.MouseDownEvent(static_cast<int>(mouse_button));
    }
    else if (event.Type == InputEvent::EventType::MouseUpEvent) {
        MouseButton mouse_button = event.MouseUp.Button;

        ScriptSys.MouseUpEvent(static_cast<int>(mouse_button));
    }
    else if (event.Type == InputEvent::EventType::MouseWheelEvent) {
        int wheel_delta = event.MouseWheel.Delta;

        // Todo: handle mouse wheel
    }
}

static auto GetLastSocketError() -> string
{
#if FO_WINDOWS
    const int error_code = WSAGetLastError();
    wchar_t* ws = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&ws), 0, nullptr);
    const string error_str = _str().parseWideChar(ws);
    LocalFree(ws);

    return _str("{} ({})", error_str, error_code);
#else
    return _str("{} ({})", strerror(errno), errno);
#endif
}

auto FOClient::CheckSocketStatus(bool for_write) -> bool
{
    timeval tv {};
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&SockSet);
    FD_SET(Sock, &SockSet);

    const int r = ::select(static_cast<int>(Sock) + 1, for_write ? nullptr : &SockSet, for_write ? &SockSet : nullptr, nullptr, &tv);
    if (r == 1) {
        if (IsConnecting) {
            WriteLog("Connection established.\n");
            IsConnecting = false;
            IsConnected = true;
        }
        return true;
    }

    if (r == 0) {
        int error = 0;
#if FO_WINDOWS
        int len = sizeof(error);
#else
        socklen_t len = sizeof(error);
#endif
        if (::getsockopt(Sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) != SOCKET_ERROR && (error == 0)) {
            return false;
        }

        WriteLog("Socket error '{}'.\n", GetLastSocketError());
    }
    else {
        // Error
        WriteLog("Socket select error '{}'.\n", GetLastSocketError());
    }

    if (IsConnecting) {
        WriteLog("Can't connect to server.\n");
        IsConnecting = false;
    }

    NetDisconnect();
    return false;
}

auto FOClient::NetConnect(const string& host, ushort port) -> bool
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

    IsConnecting = false;
    IsConnected = false;

    if (!ZStreamOk) {
        ZStream.zalloc = [](voidpf, uInt items, uInt size) { return calloc(items, size); };
        ZStream.zfree = [](voidpf, voidpf address) { free(address); };
        ZStream.opaque = nullptr;
        const auto inf_init = inflateInit(&ZStream);
        RUNTIME_ASSERT(inf_init == Z_OK);
        ZStreamOk = true;
    }

#if FO_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        WriteLog("WSAStartup error '{}'.\n", GetLastSocketError());
        return false;
    }
#endif

    if (!FillSockAddr(SockAddr, host, port)) {
        return false;
    }
    if ((Settings.ProxyType != 0u) && !FillSockAddr(ProxyAddr, Settings.ProxyHost, Settings.ProxyPort)) {
        return false;
    }

    Bin.SetEncryptKey(0);
    Bout.SetEncryptKey(0);

#if FO_WINDOWS
    const auto sock_type = SOCK_STREAM;
#else
    const auto sock_type = SOCK_STREAM | SOCK_CLOEXEC;
#endif
    if ((Sock = socket(PF_INET, sock_type, IPPROTO_TCP)) == INVALID_SOCKET) {
        WriteLog("Create socket error '{}'.\n", GetLastSocketError());
        return false;
    }

    // Nagle
#if !FO_WEB
    if (Settings.DisableTcpNagle) {
        int optval = 1;
        if (setsockopt(Sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval), sizeof(optval)) != 0) {
            WriteLog("Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'.\n", GetLastSocketError());
        }
    }
#endif

    // Direct connect
    if (Settings.ProxyType == 0u) {
        // Set non blocking mode
#if FO_WINDOWS
        unsigned long mode = 1;
        if (ioctlsocket(Sock, FIONBIO, &mode) != 0)
#else
        int flags = fcntl(Sock, F_GETFL, 0);
        RUNTIME_ASSERT(flags >= 0);
        if (fcntl(Sock, F_SETFL, flags | O_NONBLOCK))
#endif
        {
            WriteLog("Can't set non-blocking mode to socket, error '{}'.\n", GetLastSocketError());
            return false;
        }

        const int r = connect(Sock, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(sockaddr_in));
#if FO_WINDOWS
        if (r == INVALID_SOCKET && WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (r == INVALID_SOCKET && errno != EINPROGRESS)
#endif
        {
            WriteLog("Can't connect to game server, error '{}'.\n", GetLastSocketError());
            return false;
        }
    }
    else {
#if !FO_IOS && !FO_ANDROID && !FO_WEB
        // Proxy connect
        if (connect(Sock, reinterpret_cast<sockaddr*>(&ProxyAddr), sizeof(sockaddr_in)) != 0) {
            WriteLog("Can't connect to proxy server, error '{}'.\n", GetLastSocketError());
            return false;
        }

        auto send_recv = [this]() {
            if (!NetOutput()) {
                WriteLog("Net output error.\n");
                return false;
            }

            const uint tick = GameTime.FrameTick();
            while (true) {
                const int receive = NetInput(false);
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
        Bin.Reset();
        Bout.Reset();

        // Authentication
        if (Settings.ProxyType == PROXY_SOCKS4) {
            // Connect
            Bout << uchar(4); // Socks version
            Bout << uchar(1); // Connect command
            Bout << ushort(SockAddr.sin_port);
            Bout << uint(SockAddr.sin_addr.s_addr);
            Bout << uchar(0);

            if (!send_recv()) {
                return false;
            }

            Bin >> b1; // Null byte
            Bin >> b2; // Answer code
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
            Bout << uchar(5); // Socks version
            Bout << uchar(1); // Count methods
            Bout << uchar(2); // Method

            if (!send_recv()) {
                return false;
            }

            Bin >> b1; // Socks version
            Bin >> b2; // Method
            if (b2 == 2) // User/Password
            {
                Bout << uchar(1); // Subnegotiation version
                Bout << uchar(Settings.ProxyUser.length()); // Name length
                Bout.Push(Settings.ProxyUser.c_str(), static_cast<uint>(Settings.ProxyUser.length())); // Name
                Bout << uchar(Settings.ProxyPass.length()); // Pass length
                Bout.Push(Settings.ProxyPass.c_str(), static_cast<uint>(Settings.ProxyPass.length())); // Pass

                if (!send_recv()) {
                    return false;
                }

                Bin >> b1; // Subnegotiation version
                Bin >> b2; // Status
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
            Bout << uchar(5); // Socks version
            Bout << uchar(1); // Connect command
            Bout << uchar(0); // Reserved
            Bout << uchar(1); // IP v4 address
            Bout << uint(SockAddr.sin_addr.s_addr);
            Bout << ushort(SockAddr.sin_port);

            if (!send_recv()) {
                return false;
            }

            Bin >> b1; // Socks version
            Bin >> b2; // Answer code

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
            string buf = _str("CONNECT {}:{} HTTP/1.0\r\n\r\n", inet_ntoa(SockAddr.sin_addr), port);
            Bout.Push(buf.c_str(), static_cast<uint>(buf.length()));

            if (!send_recv()) {
                return false;
            }

            buf = reinterpret_cast<const char*>(Bin.GetCurData());
            if (buf.find(" 200 ") == string::npos) {
                WriteLog("Proxy connection error, receive message '{}'.\n", buf);
                return false;
            }
        }
        else {
            WriteLog("Unknown proxy type {}.\n", Settings.ProxyType);
            return false;
        }

        Bin.Reset();
        Bout.Reset();

        IsConnected = true;

#else
        throw GenericException("Proxy connection is not supported on this platform");
#endif
    }

    IsConnecting = true;
    return true;
}

auto FOClient::FillSockAddr(sockaddr_in& saddr, const string& host, ushort port) -> bool
{
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    if ((saddr.sin_addr.s_addr = inet_addr(host.c_str())) == uint(-1)) {
        hostent* h = gethostbyname(host.c_str());
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
    if (IsConnecting) {
        IsConnecting = false;
        if (Sock != INVALID_SOCKET) {
            closesocket(Sock);
        }
        Sock = INVALID_SOCKET;
    }

    if (!IsConnected) {
        return;
    }

    WriteLog("Disconnect. Session traffic: send {}, receive {}, whole {}, receive real {}.\n", BytesSend, BytesReceive, BytesReceive + BytesSend, BytesRealReceive);

    IsConnected = false;

    if (ZStreamOk) {
        inflateEnd(&ZStream);
    }
    ZStreamOk = false;
    if (Sock != INVALID_SOCKET) {
        closesocket(Sock);
    }
    Sock = INVALID_SOCKET;

    HexMngr.UnloadMap();
    DeleteCritters();
    Bin.Reset();
    Bout.Reset();
    Bin.SetEncryptKey(0);
    Bout.SetEncryptKey(0);
    Bin.SetError(false);
    Bout.SetError(false);

    ProcessAutoLogin();
}

void FOClient::ParseSocket()
{
    if (Sock == INVALID_SOCKET) {
        return;
    }

    if (NetInput(true) < 0) {
        NetDisconnect();
    }
    else {
        NetProcess();

        if (IsConnected && Settings.HelpInfo && Bout.IsEmpty() && (PingTick == 0u) && (Settings.PingPeriod != 0u) && GameTime.FrameTick() >= PingCallTick) {
            Net_SendPing(PING_PING);
            PingTick = GameTime.FrameTick();
        }

        NetOutput();
    }
}

auto FOClient::NetOutput() -> bool
{
    if (!IsConnected) {
        return false;
    }
    if (Bout.IsEmpty()) {
        return true;
    }
    if (!CheckSocketStatus(true)) {
        return IsConnected;
    }

    const int tosend = Bout.GetEndPos();
    int sendpos = 0;
    while (sendpos < tosend) {
#if FO_WINDOWS
        DWORD len = 0;
        WSABUF buf;
        buf.buf = reinterpret_cast<char*>(Bout.GetData()) + sendpos;
        buf.len = tosend - sendpos;
        if (WSASend(Sock, &buf, 1, &len, 0, nullptr, nullptr) == SOCKET_ERROR || len == 0)
#else
        int len = (int)send(Sock, Bout.GetData() + sendpos, tosend - sendpos, 0);
        if (len <= 0)
#endif
        {
            WriteLog("Socket error while send to server, error '{}'.\n", GetLastSocketError());
            NetDisconnect();
            return false;
        }
        sendpos += len;
        BytesSend += len;
    }

    Bout.Reset();
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
    buf.buf = reinterpret_cast<char*>(&ComBuf[0]);
    buf.len = static_cast<uint>(ComBuf.size());
    if (WSARecv(Sock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
    int len = recv(Sock, &ComBuf[0], (uint)ComBuf.size(), 0);
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

    uint whole_len = static_cast<uint>(len);
    while (whole_len == static_cast<uint>(ComBuf.size())) {
        ComBuf.resize(ComBuf.size() * 2);

#if FO_WINDOWS
        flags = 0;
        buf.buf = reinterpret_cast<char*>(&ComBuf[0]) + whole_len;
        buf.len = static_cast<uint>(ComBuf.size()) - whole_len;
        if (WSARecv(Sock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
        len = recv(Sock, &ComBuf[0] + whole_len, (uint)ComBuf.size() - whole_len, 0);
        if (len == SOCKET_ERROR)
#endif
        {
#if FO_WINDOWS
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
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

    Bin.Refresh();
    const uint old_pos = Bin.GetEndPos();

    if (unpack && !Settings.DisableZlibCompression) {
        ZStream.next_in = &ComBuf[0];
        ZStream.avail_in = whole_len;
        ZStream.next_out = Bin.GetData() + Bin.GetEndPos();
        ZStream.avail_out = Bin.GetLen() - Bin.GetEndPos();

        const int first_inflate = inflate(&ZStream, Z_SYNC_FLUSH);
        RUNTIME_ASSERT(first_inflate == Z_OK);

        uint uncompr = static_cast<uint>((size_t)ZStream.next_out - (size_t)Bin.GetData());
        Bin.SetEndPos(uncompr);

        while (ZStream.avail_in != 0u) {
            Bin.GrowBuf(NetBuffer::DEFAULT_BUF_SIZE);

            ZStream.next_out = Bin.GetData() + Bin.GetEndPos();
            ZStream.avail_out = Bin.GetLen() - Bin.GetEndPos();

            const int next_inflate = inflate(&ZStream, Z_SYNC_FLUSH);
            RUNTIME_ASSERT(next_inflate == Z_OK);

            uncompr = static_cast<uint>((size_t)ZStream.next_out - (size_t)Bin.GetData());
            Bin.SetEndPos(uncompr);
        }
    }
    else {
        Bin.Push(&ComBuf[0], whole_len, true);
    }

    BytesReceive += whole_len;
    BytesRealReceive += Bin.GetEndPos() - old_pos;
    return Bin.GetEndPos() - old_pos;
}

void FOClient::NetProcess()
{
    while (IsConnected && Bin.NeedProcess()) {
        uint msg = 0;
        Bin >> msg;

        CHECK_IN_BUFF_ERROR();

        if (Settings.DebugNet) {
            static uint count = 0;
            AddMess(FOMB_GAME, _str("{:04}) Input net message {}.", count, (msg >> 8) & 0xFF));
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
        case NETMSG_POD_PROPERTY(1, 1):
        case NETMSG_POD_PROPERTY(1, 2):
            Net_OnProperty(1);
            break;
        case NETMSG_POD_PROPERTY(2, 0):
        case NETMSG_POD_PROPERTY(2, 1):
        case NETMSG_POD_PROPERTY(2, 2):
            Net_OnProperty(2);
            break;
        case NETMSG_POD_PROPERTY(4, 0):
        case NETMSG_POD_PROPERTY(4, 1):
        case NETMSG_POD_PROPERTY(4, 2):
            Net_OnProperty(4);
            break;
        case NETMSG_POD_PROPERTY(8, 0):
        case NETMSG_POD_PROPERTY(8, 1):
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
            Bin.SkipMsg(msg);
            break;
        }
    }

    if (Bin.IsError()) {
        if (Settings.DebugNet) {
            AddMess(FOMB_GAME, "Invalid network message. Disconnect.");
        }
        WriteLog("Invalid network message. Disconnect.\n");
        NetDisconnect();
    }
}

void FOClient::Net_SendUpdate()
{
    // Header
    Bout << NETMSG_UPDATE;

    // Protocol version
    Bout << static_cast<ushort>(FO_VERSION);

    // Data encrypting
    const uint encrypt_key = 0x00420042;
    Bout << encrypt_key;
    Bout.SetEncryptKey(encrypt_key + 521);
    Bin.SetEncryptKey(encrypt_key + 3491);
}

void FOClient::Net_SendLogIn()
{
    char name_[UTF8_BUF_SIZE(MAX_NAME)] = {0};
    char pass_[UTF8_BUF_SIZE(MAX_NAME)] = {0};
    std::memset(name_, 0, sizeof(name_));
    Str::Copy(name_, LoginName.c_str());
    std::memset(pass_, 0, sizeof(pass_));
    Str::Copy(pass_, LoginPassword.c_str());

    Bout << NETMSG_LOGIN;
    Bout << static_cast<ushort>(FO_VERSION);

    // Begin data encrypting
    Bout.SetEncryptKey(12345);
    Bin.SetEncryptKey(12345);

    Bout.Push(name_, sizeof(name_));
    Bout.Push(pass_, sizeof(pass_));
    Bout << CurLang.NameCode;

    AddMess(FOMB_GAME, CurLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_SUCCESS));
}

void FOClient::Net_SendCreatePlayer()
{
    WriteLog("Player registration...");

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(ushort) + UTF8_BUF_SIZE(MAX_NAME) * 2;

    Bout << NETMSG_CREATE_CLIENT;
    Bout << msg_len;

    Bout << static_cast<ushort>(FO_VERSION);

    // Begin data encrypting
    Bout.SetEncryptKey(1234567890);
    Bin.SetEncryptKey(1234567890);

    char buf[UTF8_BUF_SIZE(MAX_NAME)];
    std::memset(buf, 0, sizeof(buf));
    Str::Copy(buf, LoginName.c_str());
    Bout.Push(buf, UTF8_BUF_SIZE(MAX_NAME));
    std::memset(buf, 0, sizeof(buf));
    Str::Copy(buf, LoginPassword.c_str());
    Bout.Push(buf, UTF8_BUF_SIZE(MAX_NAME));

    WriteLog("complete.\n");
}

void FOClient::Net_SendText(const char* send_str, uchar how_say)
{
    if ((send_str == nullptr) || (send_str[0] == 0)) {
        return;
    }

    bool result = false;
    int say_type = how_say;
    string str = send_str;
    result = ScriptSys.OutMessageEvent(str, say_type);

    how_say = say_type;

    if (!result || str.empty()) {
        return;
    }

    ushort len = static_cast<ushort>(str.length());
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(how_say) + sizeof(len) + len;

    Bout << NETMSG_SEND_TEXT;
    Bout << msg_len;
    Bout << how_say;
    Bout << len;
    Bout.Push(str.c_str(), len);
}

void FOClient::Net_SendDir()
{
    if (Chosen == nullptr) {
        return;
    }

    Bout << NETMSG_DIR;
    Bout << Chosen->GetDir();
}

void FOClient::Net_SendMove(vector<uchar> steps)
{
    if (Chosen == nullptr) {
        return;
    }

    uint move_params = 0;
    for (uint i = 0; i < MOVE_PARAM_STEP_COUNT; i++) {
        if (i + 1 >= steps.size()) {
            break; // Send next steps
        }

        SetBit(move_params, static_cast<uint>(steps[i + 1] << (i * MOVE_PARAM_STEP_BITS)));
        SetBit(move_params, MOVE_PARAM_STEP_ALLOW << (i * MOVE_PARAM_STEP_BITS));
    }

    if (Chosen->IsRunning) {
        SetBit(move_params, MOVE_PARAM_RUN);
    }
    if (steps.empty()) {
        SetBit(move_params, MOVE_PARAM_STEP_DISALLOW); // Inform about stopping
    }

    Bout << (Chosen->IsRunning ? NETMSG_SEND_MOVE_RUN : NETMSG_SEND_MOVE_WALK);
    Bout << move_params;
    Bout << Chosen->GetHexX();
    Bout << Chosen->GetHexY();
}

void FOClient::Net_SendProperty(NetProperty::Type type, Property* prop, Entity* entity)
{
    RUNTIME_ASSERT(entity);

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
    void* data = entity->Props.GetRawData(prop, data_size);

    const bool is_pod = prop->IsPOD();
    if (is_pod) {
        Bout << NETMSG_SEND_POD_PROPERTY(data_size, additional_args);
    }
    else {
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(char) + additional_args * sizeof(uint) + sizeof(ushort) + data_size;
        Bout << NETMSG_SEND_COMPLEX_PROPERTY;
        Bout << msg_len;
    }

    Bout << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        Bout << (dynamic_cast<ItemView*>(entity))->GetCritId();
        Bout << entity->Id;
        break;
    case NetProperty::Critter:
        Bout << entity->Id;
        break;
    case NetProperty::MapItem:
        Bout << entity->Id;
        break;
    case NetProperty::ChosenItem:
        Bout << entity->Id;
        break;
    default:
        break;
    }

    if (is_pod) {
        Bout << static_cast<ushort>(prop->GetRegIndex());
        Bout.Push(data, data_size);
    }
    else {
        Bout << static_cast<ushort>(prop->GetRegIndex());
        if (data_size != 0u) {
            Bout.Push(data, data_size);
        }
    }
}

void FOClient::Net_SendTalk(uchar is_npc, uint id_to_talk, uchar answer)
{
    Bout << NETMSG_SEND_TALK_NPC;
    Bout << is_npc;
    Bout << id_to_talk;
    Bout << answer;
}

void FOClient::Net_SendGetGameInfo()
{
    Bout << NETMSG_SEND_GET_INFO;
}

void FOClient::Net_SendGiveMap(bool automap, hash map_pid, uint loc_id, hash tiles_hash, hash scen_hash)
{
    Bout << NETMSG_SEND_GIVE_MAP;
    Bout << automap;
    Bout << map_pid;
    Bout << loc_id;
    Bout << tiles_hash;
    Bout << scen_hash;
}

void FOClient::Net_SendLoadMapOk()
{
    Bout << NETMSG_SEND_LOAD_MAP_OK;
}

void FOClient::Net_SendPing(uchar ping)
{
    Bout << NETMSG_PING;
    Bout << ping;
}

void FOClient::Net_SendRefereshMe()
{
    Bout << NETMSG_SEND_REFRESH_ME;

    WaitPing();
}

void FOClient::Net_OnWrongNetProto()
{
    if (Update) {
        UpdateFilesAbort(STR_CLIENT_OUTDATED, "Client outdated!");
    }
    else {
        AddMess(FOMB_GAME, CurLang.Msg[TEXTMSG_GAME].GetStr(STR_CLIENT_OUTDATED));
    }
}

void FOClient::Net_OnLoginSuccess()
{
    WriteLog("Authentication success.\n");

    AddMess(FOMB_GAME, CurLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_LOGINOK));

    // Set encrypt keys
    uint msg_len = 0;
    uint bin_seed = 0;
    uint bout_seed = 0; // Server bin/bout == client bout/bin

    Bin >> msg_len;
    Bin >> bin_seed;
    Bin >> bout_seed;
    NET_READ_PROPERTIES(Bin, GlovalVarsPropertiesData)

    CHECK_IN_BUFF_ERROR();

    Bout.SetEncryptKey(bin_seed);
    Bin.SetEncryptKey(bout_seed);
    Globals->Props.RestoreData(GlovalVarsPropertiesData);
}

void FOClient::Net_OnAddCritter(bool is_npc)
{
    uint msg_len = 0;
    Bin >> msg_len;

    uint crid = 0;
    ushort hx = 0;
    ushort hy = 0;
    uchar dir = 0;
    Bin >> crid;
    Bin >> hx;
    Bin >> hy;
    Bin >> dir;

    int cond = 0;
    uint anim1life = 0;
    uint anim1ko = 0;
    uint anim1dead = 0;
    uint anim2life = 0;
    uint anim2ko = 0;
    uint anim2dead = 0;
    uint flags = 0;
    Bin >> cond;
    Bin >> anim1life;
    Bin >> anim1ko;
    Bin >> anim1dead;
    Bin >> anim2life;
    Bin >> anim2ko;
    Bin >> anim2dead;
    Bin >> flags;

    // Npc
    hash npc_pid = 0;
    if (is_npc) {
        Bin >> npc_pid;
    }

    // Player
    char cl_name[UTF8_BUF_SIZE(MAX_NAME)];
    if (!is_npc) {
        Bin.Pop(cl_name, sizeof(cl_name));
        cl_name[sizeof(cl_name) - 1] = 0;
    }

    // Properties
    NET_READ_PROPERTIES(Bin, TempPropertiesData)

    CHECK_IN_BUFF_ERROR();

    if (crid == 0u) {
        WriteLog("Critter id is zero.\n");
    }
    else if (HexMngr.IsMapLoaded() && (hx >= HexMngr.GetWidth() || hy >= HexMngr.GetHeight() || dir >= Settings.MapDirCount)) {
        WriteLog("Invalid positions hx {}, hy {}, dir {}.\n", hx, hy, dir);
    }
    else {
        const ProtoCritter* proto = ProtoMngr.GetProtoCritter(is_npc ? npc_pid : _str("Player").toHash());
        RUNTIME_ASSERT(proto);

        CritterView* cr = new CritterView(crid, proto, Settings, SprMngr, ResMngr, EffectMngr, ScriptSys, GameTime, false);
        cr->Props.RestoreData(TempPropertiesData);
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->SetDir(dir);
        cr->SetCond(cond);
        cr->SetAnim1Life(anim1life);
        cr->SetAnim1Knockout(anim1ko);
        cr->SetAnim1Dead(anim1dead);
        cr->SetAnim2Life(anim2life);
        cr->SetAnim2Knockout(anim2ko);
        cr->SetAnim2Dead(anim2dead);
        cr->Flags = flags;

        if (is_npc) {
            if ((cr->GetDialogId() != 0u) && (CurLang.Msg[TEXTMSG_DLG].Count(STR_NPC_NAME(cr->GetDialogId())) != 0u)) {
                cr->AlternateName = CurLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_NAME(cr->GetDialogId()));
            }
            else {
                cr->AlternateName = CurLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_PID_NAME(npc_pid));
            }
            if (CurLang.Msg[TEXTMSG_DLG].Count(STR_NPC_AVATAR(cr->GetDialogId())) != 0u) {
                cr->Avatar = CurLang.Msg[TEXTMSG_DLG].GetStr(STR_NPC_AVATAR(cr->GetDialogId()));
            }
        }
        else {
            cr->AlternateName = cl_name;
        }

        cr->Init();

        AddCritter(cr);

        ScriptSys.CritterInEvent(cr);

        if (cr->IsChosen()) {
            RebuildLookBorders = true;
        }
    }
}

void FOClient::Net_OnRemoveCritter()
{
    uint remove_crid = 0;
    Bin >> remove_crid;

    CHECK_IN_BUFF_ERROR();

    CritterView* cr = GetCritter(remove_crid);
    if (cr == nullptr) {
        return;
    }

    cr->Finish();

    ScriptSys.CritterOutEvent(cr);
}

void FOClient::Net_OnText()
{
    uint msg_len = 0;
    uint crid = 0;
    uchar how_say = 0;
    string text;
    bool unsafe_text = 0;

    Bin >> msg_len;
    Bin >> crid;
    Bin >> how_say;
    Bin >> text;
    Bin >> unsafe_text;

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
    uint msg_len = 0;
    uint crid = 0;
    uchar how_say = 0;
    ushort msg_num = 0;
    uint num_str = 0;

    if (with_lexems) {
        Bin >> msg_len;
    }
    Bin >> crid;
    Bin >> how_say;
    Bin >> msg_num;
    Bin >> num_str;

    char lexems[MAX_DLG_LEXEMS_TEXT + 1] = {0};
    if (with_lexems) {
        ushort lexems_len = 0;
        Bin >> lexems_len;
        if ((lexems_len != 0u) && lexems_len <= MAX_DLG_LEXEMS_TEXT) {
            Bin.Pop(lexems, lexems_len);
            lexems[lexems_len] = '\0';
        }
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

    FOMsg& msg = CurLang.Msg[msg_num];
    if (msg.Count(num_str) != 0u) {
        string str = msg.GetStr(num_str);
        FormatTags(str, Chosen, GetCritter(crid), lexems);
        OnText(str, crid, how_say);
    }
}

void FOClient::OnText(const string& str, uint crid, int how_say)
{
    string fstr = str;
    if (fstr.empty()) {
        return;
    }

    uint text_delay = Settings.TextDelay + static_cast<uint>(fstr.length()) * 100;
    const string sstr = fstr;
    const bool result = ScriptSys.InMessageEvent(sstr, how_say, crid, text_delay);
    if (!result) {
        return;
    }

    fstr = sstr;
    if (fstr.empty()) {
        return;
    }

    // Type stream
    uint fstr_cr = 0;
    uint fstr_mb = 0;
    int mess_type = FOMB_TALK;

    switch (how_say) {
    case SAY_NORM:
        fstr_mb = STR_MBNORM;
    case SAY_NORM_ON_HEAD:
        fstr_cr = STR_CRNORM;
        break;
    case SAY_SHOUT:
        fstr_mb = STR_MBSHOUT;
    case SAY_SHOUT_ON_HEAD:
        fstr_cr = STR_CRSHOUT;
        fstr = _str(fstr).upperUtf8();
        break;
    case SAY_EMOTE:
        fstr_mb = STR_MBEMOTE;
    case SAY_EMOTE_ON_HEAD:
        fstr_cr = STR_CREMOTE;
        break;
    case SAY_WHISP:
        fstr_mb = STR_MBWHISP;
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
        mess_type = FOMB_GAME;
        fstr_mb = STR_MBNET;
        break;
    default:
        break;
    }

    CritterView* cr = nullptr;
    if (how_say != SAY_RADIO) {
        cr = GetCritter(crid);
    }

    const string crit_name = (cr != nullptr ? cr->GetName() : "?");

    // CritterCl on head text
    if ((fstr_cr != 0u) && (cr != nullptr)) {
        cr->SetText(FmtGameText(fstr_cr, fstr.c_str()).c_str(), COLOR_TEXT, text_delay);
    }

    // Message box text
    if (fstr_mb != 0u) {
        if (how_say == SAY_NETMSG) {
            AddMess(mess_type, FmtGameText(fstr_mb, fstr.c_str()));
        }
        else if (how_say == SAY_RADIO) {
            ushort channel = 0;
            if (Chosen != nullptr) {
                ItemView* radio = Chosen->GetItem(crid);
                if (radio != nullptr) {
                    channel = radio->GetRadioChannel();
                }
            }
            AddMess(mess_type, FmtGameText(fstr_mb, channel, fstr.c_str()));
        }
        else {
            AddMess(mess_type, FmtGameText(fstr_mb, crit_name.c_str(), fstr.c_str()));
        }
    }

    FlashGameWindow();
}

void FOClient::OnMapText(const string& str, ushort hx, ushort hy, uint color)
{
    auto text_delay = Settings.TextDelay + static_cast<uint>(str.length()) * 100;

    auto sstr = str;
    ScriptSys.MapMessageEvent(sstr, hx, hy, color, text_delay);

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
    uint msg_len = 0;
    ushort hx = 0;
    ushort hy = 0;
    uint color = 0;
    string text;
    bool unsafe_text = 0;

    Bin >> msg_len;
    Bin >> hx;
    Bin >> hy;
    Bin >> color;
    Bin >> text;
    Bin >> unsafe_text;

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
    ushort hx = 0;
    ushort hy = 0;
    uint color = 0;
    ushort msg_num = 0;
    uint num_str = 0;

    Bin >> hx;
    Bin >> hy;
    Bin >> color;
    Bin >> msg_num;
    Bin >> num_str;

    CHECK_IN_BUFF_ERROR();

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}.\n", msg_num);
        return;
    }

    string str = CurLang.Msg[msg_num].GetStr(num_str);
    FormatTags(str, Chosen, nullptr, "");
    OnMapText(str, hx, hy, color);
}

void FOClient::Net_OnMapTextMsgLex()
{
    uint msg_len = 0;
    ushort hx = 0;
    ushort hy = 0;
    uint color = 0;
    ushort msg_num = 0;
    uint num_str = 0;
    ushort lexems_len = 0;

    Bin >> msg_len;
    Bin >> hx;
    Bin >> hy;
    Bin >> color;
    Bin >> msg_num;
    Bin >> num_str;
    Bin >> lexems_len;

    char lexems[MAX_DLG_LEXEMS_TEXT + 1] = {0};
    if ((lexems_len != 0u) && lexems_len <= MAX_DLG_LEXEMS_TEXT) {
        Bin.Pop(lexems, lexems_len);
        lexems[lexems_len] = '\0';
    }

    CHECK_IN_BUFF_ERROR();

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}.\n", msg_num);
        return;
    }

    string str = CurLang.Msg[msg_num].GetStr(num_str);
    FormatTags(str, Chosen, nullptr, lexems);
    OnMapText(str, hx, hy, color);
}

void FOClient::Net_OnCritterDir()
{
    uint crid = 0;
    uchar dir = 0;
    Bin >> crid;
    Bin >> dir;

    CHECK_IN_BUFF_ERROR();

    if (dir >= Settings.MapDirCount) {
        WriteLog("Invalid dir {}.\n", dir);
        dir = 0;
    }

    CritterView* cr = GetCritter(crid);
    if (cr != nullptr) {
        cr->ChangeDir(dir, false);
    }
}

void FOClient::Net_OnCritterMove()
{
    uint crid = 0;
    uint move_params = 0;
    ushort new_hx = 0;
    ushort new_hy = 0;
    Bin >> crid;
    Bin >> move_params;
    Bin >> new_hx;
    Bin >> new_hy;

    CHECK_IN_BUFF_ERROR();

    if (new_hx >= HexMngr.GetWidth() || new_hy >= HexMngr.GetHeight()) {
        return;
    }

    CritterView* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    cr->IsRunning = IsBitSet(move_params, MOVE_PARAM_RUN);

    if (cr != Chosen) {
        cr->MoveSteps.resize(cr->CurMoveStep > 0 ? cr->CurMoveStep : 0);
        if (cr->CurMoveStep >= 0) {
            cr->MoveSteps.push_back(std::make_pair(new_hx, new_hy));
        }

        for (int i = 0, j = cr->CurMoveStep + 1; i < MOVE_PARAM_STEP_COUNT; i++, j++) {
            const uint step = (move_params >> (i * MOVE_PARAM_STEP_BITS)) & (MOVE_PARAM_STEP_DIR | MOVE_PARAM_STEP_ALLOW | MOVE_PARAM_STEP_DISALLOW);
            const uchar dir = step & MOVE_PARAM_STEP_DIR;

            const bool disallow = (!IsBitSet(step, MOVE_PARAM_STEP_ALLOW) || IsBitSet(step, MOVE_PARAM_STEP_DISALLOW));
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
    uint msg_len = 0;
    uint item_id = 0;
    hash item_pid = 0;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> item_pid;
    NET_READ_PROPERTIES(Bin, TempPropertiesData)

    CHECK_IN_BUFF_ERROR();

    if (SomeItem != nullptr) {
        SomeItem->Release();
        SomeItem = nullptr;
    }

    const auto* proto_item = ProtoMngr.GetProtoItem(item_pid);
    RUNTIME_ASSERT(proto_item);
    SomeItem = new ItemView(item_id, proto_item);
    SomeItem->Props.RestoreData(TempPropertiesData);
}

void FOClient::Net_OnCritterAction()
{
    uint crid = 0;
    int action = 0;
    int action_ext = 0;
    bool is_item = 0;
    Bin >> crid;
    Bin >> action;
    Bin >> action_ext;
    Bin >> is_item;

    CHECK_IN_BUFF_ERROR();

    CritterView* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    cr->Action(action, action_ext, is_item ? SomeItem : nullptr, false);
}

void FOClient::Net_OnCritterMoveItem()
{
    uint msg_len = 0;
    uint crid = 0;
    uchar action = 0;
    uchar prev_slot = 0;
    bool is_item = 0;
    Bin >> msg_len;
    Bin >> crid;
    Bin >> action;
    Bin >> prev_slot;
    Bin >> is_item;

    // Slot items
    vector<uchar> slots_data_slot;
    vector<uint> slots_data_id;
    vector<hash> slots_data_pid;
    vector<vector<vector<uchar>>> slots_data_data;
    ushort slots_data_count = 0;
    Bin >> slots_data_count;
    for (uchar i = 0; i < slots_data_count; i++) {
        uchar slot = 0;
        uint id = 0;
        hash pid = 0;
        Bin >> slot;
        Bin >> id;
        Bin >> pid;
        NET_READ_PROPERTIES(Bin, TempPropertiesData)

        slots_data_slot.push_back(slot);
        slots_data_id.push_back(id);
        slots_data_pid.push_back(pid);
        slots_data_data.push_back(TempPropertiesData);
    }

    CHECK_IN_BUFF_ERROR();

    CritterView* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (cr != Chosen) {
        int64 prev_hash_sum = 0;
        for (auto* item : cr->InvItems) {
            prev_hash_sum += item->LightGetHash();
        }

        cr->DeleteAllItems();

        for (uchar i = 0; i < slots_data_count; i++) {
            const ProtoItem* proto_item = ProtoMngr.GetProtoItem(slots_data_pid[i]);
            if (proto_item != nullptr) {
                ItemView* item = new ItemView(slots_data_id[i], proto_item);
                item->Props.RestoreData(slots_data_data[i]);
                item->SetCritSlot(slots_data_slot[i]);
                cr->AddItem(item);
            }
        }

        int64 hash_sum = 0;
        for (auto* item : cr->InvItems) {
            hash_sum += item->LightGetHash();
        }
        if (hash_sum != prev_hash_sum) {
            HexMngr.RebuildLight();
        }
    }

    cr->Action(action, prev_slot, is_item ? SomeItem : nullptr, false);
}

void FOClient::Net_OnCritterAnimate()
{
    uint crid = 0;
    uint anim1 = 0;
    uint anim2 = 0;
    bool is_item = 0;
    bool clear_sequence = 0;
    bool delay_play = 0;
    Bin >> crid;
    Bin >> anim1;
    Bin >> anim2;
    Bin >> is_item;
    Bin >> clear_sequence;
    Bin >> delay_play;

    CHECK_IN_BUFF_ERROR();

    CritterView* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (clear_sequence) {
        cr->ClearAnim();
    }
    if (delay_play || !cr->IsAnim()) {
        cr->Animate(anim1, anim2, is_item ? SomeItem : nullptr);
    }
}

void FOClient::Net_OnCritterSetAnims()
{
    uint crid = 0;
    int cond = 0;
    uint anim1 = 0;
    uint anim2 = 0;
    Bin >> crid;
    Bin >> cond;
    Bin >> anim1;
    Bin >> anim2;

    CHECK_IN_BUFF_ERROR();

    CritterView* cr = GetCritter(crid);
    if (cr != nullptr) {
        if (cond == 0 || cond == COND_ALIVE) {
            cr->SetAnim1Life(anim1);
            cr->SetAnim2Life(anim2);
        }
        else if (cond == 0 || cond == COND_KNOCKOUT) {
            cr->SetAnim1Knockout(anim1);
            cr->SetAnim2Knockout(anim2);
        }
        else if (cond == 0 || cond == COND_DEAD) {
            cr->SetAnim1Dead(anim1);
            cr->SetAnim2Dead(anim2);
        }
        if (!cr->IsAnim()) {
            cr->AnimateStay();
        }
    }
}

void FOClient::Net_OnCustomCommand()
{
    uint crid = 0;
    ushort index = 0;
    int value = 0;
    Bin >> crid;
    Bin >> index;
    Bin >> value;

    CHECK_IN_BUFF_ERROR();

    if (Settings.DebugNet) {
        AddMess(FOMB_GAME, _str(" - crid {} index {} value {}.", crid, index, value));
    }

    CritterView* cr = GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (cr->IsChosen()) {
        if (Settings.DebugNet) {
            AddMess(FOMB_GAME, _str(" - index {} value {}.", index, value));
        }

        switch (index) {
        case OTHER_BREAK_TIME: {
            if (value < 0) {
                value = 0;
            }
            Chosen->TickStart(value);
            Chosen->MoveSteps.clear();
            Chosen->CurMoveStep = 0;
        } break;
        case OTHER_FLAGS: {
            Chosen->Flags = value;
        } break;
        case OTHER_CLEAR_MAP: {
            auto crits = HexMngr.GetCritters();
            for (auto& [id, cr] : crits) {
                if (cr != Chosen) {
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
                CritterView* cr = HexMngr.GetField(hx, hy).Crit;
                if (Chosen == cr) {
                    break;
                }
                if (!Chosen->IsDead() && (cr != nullptr)) {
                    DeleteCritter(cr->GetId());
                }
                HexMngr.RemoveCritter(Chosen);
                Chosen->SetHexX(hx);
                Chosen->SetHexY(hy);
                HexMngr.SetCritter(Chosen);
                HexMngr.ScrollToHex(Chosen->GetHexX(), Chosen->GetHexY(), 0.1f, true);
            }
        } break;
        default:
            break;
        }

        // Maybe changed some parameter influencing on look borders
        RebuildLookBorders = true;
    }
    else {
        if (index == OTHER_FLAGS) {
            cr->Flags = value;
        }
    }
}

void FOClient::Net_OnCritterXY()
{
    uint crid = 0;
    ushort hx = 0;
    ushort hy = 0;
    uchar dir = 0;
    Bin >> crid;
    Bin >> hx;
    Bin >> hy;
    Bin >> dir;

    CHECK_IN_BUFF_ERROR();

    if (Settings.DebugNet) {
        AddMess(FOMB_GAME, _str(" - crid {} hx {} hy {} dir {}.", crid, hx, hy, dir));
    }

    if (!HexMngr.IsMapLoaded()) {
        return;
    }

    CritterView* cr = GetCritter(crid);
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
        Field& f = HexMngr.GetField(hx, hy);
        if ((f.Crit != nullptr) && f.Crit->IsFinishing()) {
            DeleteCritter(f.Crit->GetId());
        }

        HexMngr.TransitCritter(cr, hx, hy, false, true);
        cr->AnimateStay();
        if (cr == Chosen) {
            // Chosen->TickStart(Settings.Breaktime);
            Chosen->TickStart(200);
            // SetAction(CHOSEN_NONE);
            Chosen->MoveSteps.clear();
            Chosen->CurMoveStep = 0;
            RebuildLookBorders = true;
        }
    }

    cr->MoveSteps.clear();
    cr->CurMoveStep = 0;
}

void FOClient::Net_OnAllProperties()
{
    WriteLog("Chosen properties...");

    uint msg_len = 0;
    Bin >> msg_len;
    NET_READ_PROPERTIES(Bin, TempPropertiesData)

    if (Chosen == nullptr) {
        WriteLog("Chosen not created, skip.\n");
        return;
    }

    Chosen->Props.RestoreData(TempPropertiesData);

    // Animate
    if (!Chosen->IsAnim()) {
        Chosen->AnimateStay();
    }

    // Refresh borders
    RebuildLookBorders = true;

    WriteLog("complete.\n");
}

void FOClient::Net_OnChosenClearItems()
{
    InitialItemsSend = true;

    if (Chosen == nullptr) {
        WriteLog("Chosen is not created.\n");
        return;
    }

    if (Chosen->IsHaveLightSources()) {
        HexMngr.RebuildLight();
    }
    Chosen->DeleteAllItems();
}

void FOClient::Net_OnChosenAddItem()
{
    uint msg_len = 0;
    uint item_id = 0;
    hash pid = 0;
    uchar slot = 0;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> pid;
    Bin >> slot;
    NET_READ_PROPERTIES(Bin, TempPropertiesData)

    CHECK_IN_BUFF_ERROR();

    if (Chosen == nullptr) {
        WriteLog("Chosen is not created.\n");
        return;
    }

    ItemView* prev_item = Chosen->GetItem(item_id);
    uchar prev_slot = 0;
    uint prev_light_hash = 0;
    if (prev_item != nullptr) {
        prev_slot = prev_item->GetCritSlot();
        prev_light_hash = prev_item->LightGetHash();
        Chosen->DeleteItem(prev_item, false);
    }

    const ProtoItem* proto_item = ProtoMngr.GetProtoItem(pid);
    RUNTIME_ASSERT(proto_item);

    ItemView* item = new ItemView(item_id, proto_item);
    item->Props.RestoreData(TempPropertiesData);
    item->SetAccessory(ITEM_ACCESSORY_CRITTER);
    item->SetCritId(Chosen->GetId());
    item->SetCritSlot(slot);
    RUNTIME_ASSERT(!item->GetIsHidden());

    item->AddRef();

    Chosen->AddItem(item);

    RebuildLookBorders = true;
    if (item->LightGetHash() != prev_light_hash && ((slot != 0u) || (prev_slot != 0u))) {
        HexMngr.RebuildLight();
    }

    if (!InitialItemsSend) {
        ScriptSys.ItemInvInEvent(item);
    }

    item->Release();
}

void FOClient::Net_OnChosenEraseItem()
{
    uint item_id = 0;
    Bin >> item_id;

    CHECK_IN_BUFF_ERROR();

    if (Chosen == nullptr) {
        WriteLog("Chosen is not created.\n");
        return;
    }

    ItemView* item = Chosen->GetItem(item_id);
    if (item == nullptr) {
        WriteLog("Item not found, id {}.\n", item_id);
        return;
    }

    ItemView* clone = item->Clone();
    const bool rebuild_light = (item->GetIsLight() && (item->GetCritSlot() != 0u));
    Chosen->DeleteItem(item, true);
    if (rebuild_light) {
        HexMngr.RebuildLight();
    }
    ScriptSys.ItemInvOutEvent(clone);
    clone->Release();
}

void FOClient::Net_OnAllItemsSend()
{
    InitialItemsSend = false;

    if (Chosen == nullptr) {
        WriteLog("Chosen is not created.\n");
        return;
    }

    if (Chosen->Model != nullptr) {
        Chosen->Model->StartMeshGeneration();
    }

    ScriptSys.ItemInvAllInEvent();
}

void FOClient::Net_OnAddItemOnMap()
{
    uint msg_len = 0;
    uint item_id = 0;
    hash item_pid = 0;
    ushort item_hx = 0;
    ushort item_hy = 0;
    bool is_added = 0;
    Bin >> msg_len;
    Bin >> item_id;
    Bin >> item_pid;
    Bin >> item_hx;
    Bin >> item_hy;
    Bin >> is_added;
    NET_READ_PROPERTIES(Bin, TempPropertiesData)

    CHECK_IN_BUFF_ERROR();

    if (HexMngr.IsMapLoaded()) {
        HexMngr.AddItem(item_id, item_pid, item_hx, item_hy, is_added, &TempPropertiesData);
    }

    ItemView* item = HexMngr.GetItemById(item_id);
    if (item != nullptr) {
        ScriptSys.ItemMapInEvent(item);
    }

    // Refresh borders
    if ((item != nullptr) && !item->GetIsShootThru()) {
        RebuildLookBorders = true;
    }
}

void FOClient::Net_OnEraseItemFromMap()
{
    uint item_id = 0;
    bool is_deleted = 0;
    Bin >> item_id;
    Bin >> is_deleted;

    CHECK_IN_BUFF_ERROR();

    ItemView* item = HexMngr.GetItemById(item_id);
    if (item != nullptr) {
        ScriptSys.ItemMapOutEvent(item);
    }

    // Refresh borders
    if ((item != nullptr) && !item->GetIsShootThru()) {
        RebuildLookBorders = true;
    }

    HexMngr.FinishItem(item_id, is_deleted);
}

void FOClient::Net_OnAnimateItem()
{
    uint item_id = 0;
    uchar from_frm = 0;
    uchar to_frm = 0;
    Bin >> item_id;
    Bin >> from_frm;
    Bin >> to_frm;

    CHECK_IN_BUFF_ERROR();

    ItemHexView* item = HexMngr.GetItemById(item_id);
    if (item != nullptr) {
        item->SetAnim(from_frm, to_frm);
    }
}

void FOClient::Net_OnCombatResult()
{
    uint msg_len = 0;
    uint data_count = 0;
    vector<uint> data_vec;
    Bin >> msg_len;
    Bin >> data_count;
    if (data_count > Settings.FloodSize / sizeof(uint)) {
        return; // Insurance
    }
    if (data_count != 0u) {
        data_vec.resize(data_count);
        Bin.Pop(&data_vec[0], data_count * sizeof(uint));
    }

    CHECK_IN_BUFF_ERROR();

    ScriptSys.CombatResultEvent(data_vec);
}

void FOClient::Net_OnEffect()
{
    hash eff_pid = 0;
    ushort hx = 0;
    ushort hy = 0;
    ushort radius = 0;
    Bin >> eff_pid;
    Bin >> hx;
    Bin >> hy;
    Bin >> radius;

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
    hash eff_pid = 0;
    uint eff_cr1_id = 0;
    uint eff_cr2_id = 0;
    ushort eff_cr1_hx = 0;
    ushort eff_cr1_hy = 0;
    ushort eff_cr2_hx = 0;
    ushort eff_cr2_hy = 0;
    Bin >> eff_pid;
    Bin >> eff_cr1_id;
    Bin >> eff_cr2_id;
    Bin >> eff_cr1_hx;
    Bin >> eff_cr1_hy;
    Bin >> eff_cr2_hx;
    Bin >> eff_cr2_hy;

    CHECK_IN_BUFF_ERROR();

    CritterView* cr1 = GetCritter(eff_cr1_id);
    CritterView* cr2 = GetCritter(eff_cr2_id);

    if (cr1 != nullptr) {
        eff_cr1_hx = cr1->GetHexX();
        eff_cr1_hy = cr1->GetHexY();
    }

    if (cr2 != nullptr) {
        eff_cr2_hx = cr2->GetHexX();
        eff_cr2_hy = cr2->GetHexY();
    }

    if (!HexMngr.RunEffect(eff_pid, eff_cr1_hx, eff_cr1_hy, eff_cr2_hx, eff_cr2_hy)) {
        WriteLog("Run effect '{}' failed.\n", _str().parseHash(eff_pid));
        return;
    }
}

void FOClient::Net_OnPlaySound()
{
    uint msg_len = 0;
    uint synchronize_crid = 0;
    string sound_name;
    Bin >> msg_len;
    Bin >> synchronize_crid;
    Bin >> sound_name;

    CHECK_IN_BUFF_ERROR();

    SndMngr.PlaySound(ResMngr.GetSoundNames(), sound_name);
}

void FOClient::Net_OnPing()
{
    uchar ping = 0;
    Bin >> ping;

    CHECK_IN_BUFF_ERROR();

    if (ping == PING_WAIT) {
        Settings.WaitPing = false;
    }
    else if (ping == PING_CLIENT) {
        Bout << NETMSG_PING;
        Bout << static_cast<uchar> PING_CLIENT;
    }
    else if (ping == PING_PING) {
        Settings.Ping = GameTime.FrameTick() - PingTick;
        PingTick = 0;
        PingCallTick = GameTime.FrameTick() + Settings.PingPeriod;
    }
}

void FOClient::Net_OnEndParseToGame()
{
    if (Chosen == nullptr) {
        WriteLog("Chosen is not created.\n");
        return;
    }

    FlashGameWindow();
    ScreenFadeOut();

    if (CurMapPid != 0u) {
        HexMngr.SkipItemsFade();
        HexMngr.FindSetCenter(Chosen->GetHexX(), Chosen->GetHexY());
        Chosen->AnimateStay();
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
    uint msg_len = 0;
    NetProperty::Type type = NetProperty::None;
    uint cr_id = 0;
    uint item_id = 0;
    ushort property_index = 0;

    if (data_size == 0) {
        Bin >> msg_len;
    }

    char type_ = 0;
    Bin >> type_;
    type = static_cast<NetProperty::Type>(type_);

    uint additional_args = 0;
    switch (type) {
    case NetProperty::CritterItem:
        additional_args = 2;
        Bin >> cr_id;
        Bin >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        Bin >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        Bin >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        Bin >> item_id;
        break;
    default:
        break;
    }

    Bin >> property_index;

    if (data_size != 0) {
        TempPropertyData.resize(data_size);
        Bin.Pop(&TempPropertyData[0], data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
        TempPropertyData.resize(len);
        if (len != 0u) {
            Bin.Pop(&TempPropertyData[0], len);
        }
    }

    CHECK_IN_BUFF_ERROR();

    Property* prop = nullptr;
    Entity* entity = nullptr;
    switch (type) {
    case NetProperty::Global:
        prop = GlobalVars::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = Globals;
        }
        break;
    case NetProperty::Critter:
        prop = CritterView::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = GetCritter(cr_id);
        }
        break;
    case NetProperty::Chosen:
        prop = CritterView::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = Chosen;
        }
        break;
    case NetProperty::MapItem:
        prop = ItemView::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = HexMngr.GetItemById(item_id);
        }
        break;
    case NetProperty::CritterItem:
        prop = ItemView::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            CritterView* cr = GetCritter(cr_id);
            if (cr != nullptr) {
                entity = cr->GetItem(item_id);
            }
        }
        break;
    case NetProperty::ChosenItem:
        prop = ItemView::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = (Chosen != nullptr ? Chosen->GetItem(item_id) : nullptr);
        }
        break;
    case NetProperty::Map:
        prop = MapView::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = CurMap;
        }
        break;
    case NetProperty::Location:
        prop = LocationView::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = CurLocation;
        }
        break;
    default:
        RUNTIME_ASSERT(false);
        break;
    }
    if ((prop == nullptr) || (entity == nullptr)) {
        return;
    }

    entity->Props.SetSendIgnore(prop, entity);
    entity->Props.SetValueFromData(prop, TempPropertyData.data(), static_cast<uint>(TempPropertyData.size()));
    entity->Props.SetSendIgnore(nullptr, nullptr);

    if (type == NetProperty::MapItem) {
        ScriptSys.ItemMapChangedEvent(dynamic_cast<ItemView*>(entity), dynamic_cast<ItemView*>(entity));
    }

    if (type == NetProperty::ChosenItem) {
        ItemView* item = dynamic_cast<ItemView*>(entity);
        item->AddRef();
        OnItemInvChanged(item, item);
    }
}

void FOClient::Net_OnChosenTalk()
{
    uint msg_len = 0;
    uchar is_npc = 0;
    uint talk_id = 0;
    uchar count_answ = 0;
    uint text_id = 0;
    uint talk_time = 0;

    Bin >> msg_len;
    Bin >> is_npc;
    Bin >> talk_id;
    Bin >> count_answ;

    if (count_answ == 0u) {
        // End dialog
        if (IsScreenPresent(SCREEN_DIALOG)) {
            HideScreen(SCREEN_DIALOG);
        }
        return;
    }

    // Text params
    ushort lexems_len = 0;
    char lexems[MAX_DLG_LEXEMS_TEXT + 1] = {0};
    Bin >> lexems_len;
    if ((lexems_len != 0u) && lexems_len <= MAX_DLG_LEXEMS_TEXT) {
        Bin.Pop(lexems, lexems_len);
        lexems[lexems_len] = '\0';
    }

    // Find critter
    DlgIsNpc = is_npc;
    DlgNpcId = talk_id;
    CritterView* npc = (is_npc != 0u ? GetCritter(talk_id) : nullptr);

    // Main text
    Bin >> text_id;

    // Answers
    vector<uint> answers_texts;
    uint answ_text_id = 0;
    for (int i = 0; i < count_answ; i++) {
        Bin >> answ_text_id;
        answers_texts.push_back(answ_text_id);
    }

    string str = CurLang.Msg[TEXTMSG_DLG].GetStr(text_id);
    FormatTags(str, Chosen, npc, lexems);
    string text_to_script = str;
    vector<string> answers_to_script;
    for (uint answers_text : answers_texts) {
        str = CurLang.Msg[TEXTMSG_DLG].GetStr(answers_text);
        FormatTags(str, Chosen, npc, lexems);
        answers_to_script.push_back(str);
    }

    Bin >> talk_time;

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
    int time = 0;
    uchar rain = 0;
    bool no_log_out = 0;
    int* day_time = HexMngr.GetMapDayTime();
    uchar* day_color = HexMngr.GetMapDayColor();
    ushort year = 0;
    ushort month = 0;
    ushort day = 0;
    ushort hour = 0;
    ushort minute = 0;
    ushort second = 0;
    ushort multiplier = 0;
    Bin >> year;
    Bin >> month;
    Bin >> day;
    Bin >> hour;
    Bin >> minute;
    Bin >> second;
    Bin >> multiplier;
    Bin >> time;
    Bin >> rain;
    Bin >> no_log_out;
    Bin.Pop(day_time, sizeof(int) * 4);
    Bin.Pop(day_color, sizeof(uchar) * 12);

    CHECK_IN_BUFF_ERROR();

    GameTime.Reset(year, month, day, hour, minute, second, multiplier);
    Globals->SetYear(year);
    Globals->SetMonth(month);
    Globals->SetDay(day);
    Globals->SetHour(hour);
    Globals->SetMinute(minute);
    Globals->SetSecond(second);
    Globals->SetTimeMultiplier(multiplier);

    HexMngr.SetWeather(time, rain);
    SetDayTime(true);
    NoLogOut = no_log_out;
}

void FOClient::Net_OnLoadMap()
{
    WriteLog("Change map...\n");

    uint msg_len = 0;
    hash map_pid = 0;
    hash loc_pid = 0;
    uchar map_index_in_loc = 0;
    int map_time = 0;
    uchar map_rain = 0;
    hash hash_tiles = 0;
    hash hash_scen = 0;
    Bin >> msg_len;
    Bin >> map_pid;
    Bin >> loc_pid;
    Bin >> map_index_in_loc;
    Bin >> map_time;
    Bin >> map_rain;
    Bin >> hash_tiles;
    Bin >> hash_scen;
    if (map_pid != 0u) {
        NET_READ_PROPERTIES(Bin, TempPropertiesData)
        NET_READ_PROPERTIES(Bin, TempPropertiesDataExt)
    }

    CHECK_IN_BUFF_ERROR();

    if (CurMap != nullptr) {
        CurMap->IsDestroyed = true;
        CurMap->Release();
        CurMap = nullptr;
    }

    if (CurLocation != nullptr) {
        CurLocation->IsDestroyed = true;
        CurLocation->Release();
        CurLocation = nullptr;
    }

    if (map_pid != 0u) {
        CurLocation = new LocationView(0, ProtoMngr.GetProtoLocation(loc_pid));
        CurLocation->Props.RestoreData(TempPropertiesDataExt);
        CurMap = new MapView(0, ProtoMngr.GetProtoMap(map_pid));
        CurMap->Props.RestoreData(TempPropertiesData);
    }

    Settings.SpritesZoom = 1.0f;

    CurMapPid = map_pid;
    CurMapLocPid = loc_pid;
    CurMapIndexInLoc = map_index_in_loc;
    GameMapTexts.clear();
    HexMngr.UnloadMap();
    SndMngr.StopSounds();
    ShowMainScreen(SCREEN_WAIT, {});
    DeleteCritters();
    ResMngr.ReinitializeDynamicAtlas();

    // Global
    if (map_pid == 0u) {
        GmapNullParams();
        WriteLog("Global map loaded.\n");
    }
    // Local
    else {
        hash hash_tiles_cl = 0;
        hash hash_scen_cl = 0;
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
        LookBorders.clear();
        ShootBorders.clear();

        WriteLog("Local map loaded.\n");
    }

    Net_SendLoadMapOk();
}

void FOClient::Net_OnMap()
{
    uint msg_len = 0;
    hash map_pid = 0;
    ushort maxhx = 0;
    ushort maxhy = 0;
    bool send_tiles = 0;
    bool send_scenery = 0;
    Bin >> msg_len;
    Bin >> map_pid;
    Bin >> maxhx;
    Bin >> maxhy;
    Bin >> send_tiles;
    Bin >> send_scenery;

    CHECK_IN_BUFF_ERROR();

    WriteLog("Map {} received...\n", map_pid);

    const string map_name = _str("{}.map", map_pid);
    bool tiles = false;
    char* tiles_data = nullptr;
    uint tiles_len = 0;
    bool scen = false;
    char* scen_data = nullptr;
    uint scen_len = 0;

    if (send_tiles) {
        Bin >> tiles_len;
        if (tiles_len != 0u) {
            tiles_data = new char[tiles_len];
            Bin.Pop(tiles_data, tiles_len);
        }
        tiles = true;
    }

    if (send_scenery) {
        Bin >> scen_len;
        if (scen_len != 0u) {
            scen_data = new char[scen_len];
            Bin.Pop(scen_data, scen_len);
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
        // Todo: need attention!
        /*OutputFile output_file {};
        output_file.SetBEUInt(CLIENT_MAP_FORMAT_VER);
        output_file.SetBEUInt(map_pid);
        output_file.SetBEUShort(maxhx);
        output_file.SetBEUShort(maxhy);
        output_file.SetBEUInt(tiles_len);
        output_file.SetBEUInt(scen_len);
        if (tiles_len)
            output_file.SetData(tiles_data, tiles_len);
        if (scen_len)
            output_file.SetData(scen_data, scen_len);

        uint obuf_len = output_file.GetOutBufLen();
        uchar* buf = Compressor::Compress(output_file.GetOutBuf(), obuf_len);
        if (!buf)
        {
            WriteLog("Failed to compress data '{}', disconnect.\n", map_name);
            NetDisconnect();
            return;
        }

        Cache.SetRawData(map_name, buf, obuf_len);
        delete[] buf;*/
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
    uint msg_len = 0;
    uchar info_flags = 0;
    Bin >> msg_len;
    Bin >> info_flags;

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        GmapLoc.clear();
        ushort count_loc = 0;
        Bin >> count_loc;

        for (int i = 0; i < count_loc; i++) {
            GmapLocation loc {};
            Bin >> loc.LocId;
            Bin >> loc.LocPid;
            Bin >> loc.LocWx;
            Bin >> loc.LocWy;
            Bin >> loc.Radius;
            Bin >> loc.Color;
            Bin >> loc.Entrances;

            if (loc.LocId != 0u) {
                GmapLoc.push_back(loc);
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_LOCATION)) {
        GmapLocation loc;
        bool add = 0;
        Bin >> add;
        Bin >> loc.LocId;
        Bin >> loc.LocPid;
        Bin >> loc.LocWx;
        Bin >> loc.LocWy;
        Bin >> loc.Radius;
        Bin >> loc.Color;
        Bin >> loc.Entrances;

        const auto it = std::find_if(GmapLoc.begin(), GmapLoc.end(), [&loc](const GmapLocation& l) { return loc.LocId == l.LocId; });
        if (add) {
            if (it != GmapLoc.end()) {
                *it = loc;
            }
            else {
                GmapLoc.push_back(loc);
            }
        }
        else {
            if (it != GmapLoc.end()) {
                GmapLoc.erase(it);
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        Bin.Pop(GmapFog.GetData(), GM_ZONES_FOG_SIZE);
    }

    if (IsBitSet(info_flags, GM_INFO_FOG)) {
        ushort zx = 0;
        ushort zy = 0;
        uchar fog = 0;
        Bin >> zx;
        Bin >> zy;
        Bin >> fog;
        GmapFog.Set2Bit(zx, zy, fog);
    }

    if (IsBitSet(info_flags, GM_INFO_CRITTERS)) {
        DeleteCritters();
        // After wait AddCritters
    }

    CHECK_IN_BUFF_ERROR();
}

void FOClient::Net_OnSomeItems()
{
    uint msg_len = 0;
    auto param = 0;
    bool is_null = 0;
    uint items_count = 0;
    Bin >> msg_len;
    Bin >> param;
    Bin >> is_null;
    Bin >> items_count;

    vector<ItemView*> item_container;
    for (uint i = 0; i < items_count; i++) {
        uint item_id = 0;
        hash item_pid = 0;
        Bin >> item_id;
        Bin >> item_pid;
        NET_READ_PROPERTIES(Bin, TempPropertiesData)

        const ProtoItem* proto_item = ProtoMngr.GetProtoItem(item_pid);
        if ((item_id != 0u) && (proto_item != nullptr)) {
            ItemView* item = new ItemView(item_id, proto_item);
            item->Props.RestoreData(TempPropertiesData);
            item_container.push_back(item);
        }
    }

    CHECK_IN_BUFF_ERROR();

    ScriptSys.ReceiveItemsEvent(item_container, param);
}

void FOClient::Net_OnUpdateFilesList()
{
    uint msg_len = 0;
    bool outdated = 0;
    uint data_size = 0;
    vector<uchar> data;
    Bin >> msg_len;
    Bin >> outdated;
    Bin >> data_size;
    data.resize(data_size);
    if (data_size != 0u) {
        Bin.Pop(&data[0], static_cast<uint>(data.size()));
    }
    if (!outdated)
        NET_READ_PROPERTIES(Bin, GlovalVarsPropertiesData)

    CHECK_IN_BUFF_ERROR();

    File file(&data[0], static_cast<uint>(data.size()));

    Update->FileList.clear();
    Update->FilesWholeSize = 0;
    Update->ClientOutdated = outdated;

#if FO_WINDOWS
    /*bool have_exe = false;
    string exe_name;
    if (outdated)
        exe_name = _str(File::GetExePath()).extractFileName();*/
#endif

    for (uint file_index = 0;; file_index++) {
        short name_len = file.GetLEShort();
        if (name_len == -1) {
            break;
        }

        RUNTIME_ASSERT(name_len > 0);
        string name = string(reinterpret_cast<const char*>(file.GetCurBuf()), name_len);
        file.GoForward(name_len);
        uint size = file.GetLEUInt();
        uint hash = file.GetLEUInt();

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
        uint* cur_hash = reinterpret_cast<uint*>(Cache.GetRawData(_str("{}.hash", name), cur_hash_len));
        bool cached_hash_same = ((cur_hash != nullptr) && cur_hash_len == sizeof(hash) && *cur_hash == hash);
        if (name[0] != '$') {
            // Real file, human can disturb file consistency, make base recheck
            FileHeader file_header = FileMngr.ReadFileHeader(name);
            if (file_header && file_header.GetFsize() == size) {
                File file = FileMngr.ReadFile(name);
                if (cached_hash_same || (file && hash == Hashing::MurmurHash2(file.GetBuf(), file.GetFsize()))) {
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
        Update->FileList.push_back(update_file);
        Update->FilesWholeSize += size;
    }

#if FO_WINDOWS
    // if (Update->ClientOutdated && !have_exe)
    //    UpdateFilesAbort(STR_CLIENT_OUTDATED, "Client outdated!");
#else
    if (Update->ClientOutdated)
        UpdateFilesAbort(STR_CLIENT_OUTDATED, "Client outdated!");
#endif
}

void FOClient::Net_OnUpdateFileData()
{
    // Get portion
    uchar data[FILE_UPDATE_PORTION];
    Bin.Pop(data, sizeof(data));

    CHECK_IN_BUFF_ERROR();

    auto& update_file = Update->FileList.front();

    // Write data to temp file
    Update->TempFile->SetData(data, std::min(update_file.RemaningSize, static_cast<uint>(sizeof(data))));
    Update->TempFile->Save();

    // Get next portion or finalize data
    update_file.RemaningSize -= std::min(update_file.RemaningSize, static_cast<uint>(sizeof(data)));
    if (update_file.RemaningSize > 0) {
        // Request next portion
        Bout << NETMSG_GET_UPDATE_FILE_DATA;
    }
    else {
        // Finalize received data
        Update->TempFile->Save();
        Update->TempFile = nullptr;

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

        Update->FileList.erase(Update->FileList.begin());
        Update->FileDownloading = false;
    }
}

void FOClient::Net_OnAutomapsInfo()
{
    uint msg_len = 0;
    bool clear = 0;
    ushort locs_count = 0;
    Bin >> msg_len;
    Bin >> clear;

    if (clear) {
        Automaps.clear();
    }

    Bin >> locs_count;
    for (ushort i = 0; i < locs_count; i++) {
        uint loc_id = 0;
        hash loc_pid = 0;
        ushort maps_count = 0;
        Bin >> loc_id;
        Bin >> loc_pid;
        Bin >> maps_count;

        auto it = std::find_if(Automaps.begin(), Automaps.end(), [&loc_id](const Automap& m) { return loc_id == m.LocId; });

        // Delete from collection
        if (maps_count == 0u) {
            if (it != Automaps.end()) {
                Automaps.erase(it);
            }
        }
        // Add or modify
        else {
            Automap amap;
            amap.LocId = loc_id;
            amap.LocPid = loc_pid;
            amap.LocName = CurLang.Msg[TEXTMSG_LOCATIONS].GetStr(STR_LOC_NAME(loc_pid));

            for (ushort j = 0; j < maps_count; j++) {
                hash map_pid = 0;
                uchar map_index_in_loc = 0;
                Bin >> map_pid;
                Bin >> map_index_in_loc;

                amap.MapPids.push_back(map_pid);
                amap.MapNames.push_back(CurLang.Msg[TEXTMSG_LOCATIONS].GetStr(STR_LOC_MAP_NAME(loc_pid, map_index_in_loc)));
            }

            if (it != Automaps.end()) {
                *it = amap;
            }
            else {
                Automaps.push_back(amap);
            }
        }
    }

    CHECK_IN_BUFF_ERROR();
}

void FOClient::Net_OnViewMap()
{
    ushort hx = 0;
    ushort hy = 0;
    uint loc_id = 0;
    uint loc_ent = 0;
    Bin >> hx;
    Bin >> hy;
    Bin >> loc_id;
    Bin >> loc_ent;

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
    if (!HexMngr.IsMapLoaded()) {
        return;
    }

    static uint old_color = -1;
    uint color = GenericUtils::GetColorDay(HexMngr.GetMapDayTime(), HexMngr.GetMapDayColor(), HexMngr.GetMapTime(), nullptr);
    color = COLOR_GAME_RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);

    if (refresh) {
        old_color = -1;
    }
    if (old_color != color) {
        old_color = color;
        SetGameColor(old_color);
    }
}

void FOClient::WaitPing()
{
    Settings.WaitPing = true;
    Net_SendPing(PING_WAIT);
}

void FOClient::CrittersProcess()
{
    for (auto it = HexMngr.GetCritters().begin(); it != HexMngr.GetCritters().end();) {
        CritterView* cr = it->second;
        ++it;

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
            DeleteCritter(cr->GetId());
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

auto FOClient::IsCurInWindow() const -> bool
{
    return SprMngr.IsWindowFocused();
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

auto FOClient::FmtGameText(uint str_num, ...) -> string
{
    const string str = _str(CurLang.Msg[TEXTMSG_GAME].GetStr(str_num)).replace('\\', 'n', '\n');
    char res[MAX_FOTEXT];
    va_list list;
    va_start(list, str_num);
    vsprintf(res, str.c_str(), list);
    va_end(list);
    return res;
}

auto FOClient::AnimLoad(uint name_hash, AtlasType res_type) -> uint
{
    AnyFrames* anim = ResMngr.GetAnim(name_hash, res_type);
    if (anim == nullptr) {
        return 0;
    }

    IfaceAnim* ianim = new IfaceAnim {anim, res_type, GameTime.GameTick()};

    size_t index = 1;
    for (; index < Animations.size(); index++) {
        if (Animations[index] == nullptr) {
            break;
        }
    }

    if (index < Animations.size()) {
        Animations[index] = ianim;
    }
    else {
        Animations.push_back(ianim);
    }

    return static_cast<uint>(index);
}

auto FOClient::AnimLoad(const char* fname, AtlasType res_type) -> uint
{
    AnyFrames* anim = ResMngr.GetAnim(_str(fname).toHash(), res_type);
    if (anim == nullptr) {
        return 0;
    }

    IfaceAnim* ianim = new IfaceAnim {anim, res_type, GameTime.GameTick()};

    size_t index = 1;
    for (; index < Animations.size(); index++) {
        if (Animations[index] == nullptr) {
            break;
        }
    }

    if (index < Animations.size()) {
        Animations[index] = ianim;
    }
    else {
        Animations.push_back(ianim);
    }

    return static_cast<uint>(index);
}

auto FOClient::AnimGetCurSpr(uint anim_id) -> uint
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->Frames->Ind[Animations[anim_id]->CurSpr];
}

auto FOClient::AnimGetCurSprCnt(uint anim_id) -> uint
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->CurSpr;
}

auto FOClient::AnimGetSprCount(uint anim_id) -> uint
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->Frames->CntFrm;
}

auto FOClient::AnimGetFrames(uint anim_id) -> AnyFrames*
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return 0;
    }
    return Animations[anim_id]->Frames;
}

void FOClient::AnimRun(uint anim_id, uint flags)
{
    if (anim_id >= Animations.size() || (Animations[anim_id] == nullptr)) {
        return;
    }
    IfaceAnim* anim = Animations[anim_id];

    // Set flags
    anim->Flags = flags & 0xFFFF;
    flags >>= 16;

    // Set frm
    uchar cur_frm = flags & 0xFF;
    if (cur_frm > 0) {
        cur_frm--;
        if (cur_frm >= anim->Frames->CntFrm) {
            cur_frm = anim->Frames->CntFrm - 1;
        }
        anim->CurSpr = cur_frm;
    }
}

void FOClient::AnimProcess()
{
    const uint cur_tick = GameTime.GameTick();
    for (auto* anim : Animations) {
        if ((anim == nullptr) || (anim->Flags == 0u)) {
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
            uint end_spr = anim->Frames->CntFrm - 1;
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

void FOClient::OnSendGlobalValue(Entity* /*entity*/, Property* prop)
{
    if (prop->GetAccess() == Property::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Global, prop, Globals);
    }
    else {
        throw GenericException("Unable to send global modifiable property", prop->GetName());
    }
}

void FOClient::OnSendCritterValue(Entity* entity, Property* prop)
{
    CritterView* cr = dynamic_cast<CritterView*>(entity);
    if (cr->IsChosen()) {
        Net_SendProperty(NetProperty::Chosen, prop, cr);
    }
    else if (prop->GetAccess() == Property::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Critter, prop, cr);
    }
    else {
        throw GenericException("Unable to send critter modifiable property", prop->GetName());
    }
}

void FOClient::OnSetCritterModelName(Entity* entity, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    CritterView* cr = dynamic_cast<CritterView*>(entity);
    cr->RefreshModel();
    cr->Action(ACTION_REFRESH, 0, nullptr, false);
}

void FOClient::OnSendItemValue(Entity* entity, Property* prop)
{
    ItemView* item = dynamic_cast<ItemView*>(entity);
    if ((item->Id != 0u) && item->Id != uint(-1)) {
        if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER) {
            CritterView* cr = GetCritter(item->GetCritId());
            if ((cr != nullptr) && cr->IsChosen()) {
                Net_SendProperty(NetProperty::ChosenItem, prop, item);
            }
            else if ((cr != nullptr) && prop->GetAccess() == Property::PublicFullModifiable) {
                Net_SendProperty(NetProperty::CritterItem, prop, item);
            }
            else {
                throw GenericException("Unable to send item (a critter) modifiable property", prop->GetName());
            }
        }
        else if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
            if (prop->GetAccess() == Property::PublicFullModifiable) {
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

void FOClient::OnSetItemFlags(Entity* entity, Property* prop, void* /*cur_value*/, void* /*old_value*/)
{
    // IsColorize, IsBadItem, IsShootThru, IsLightThru, IsNoBlock

    ItemView* item = dynamic_cast<ItemView*>(entity);
    if (item->GetAccessory() == ITEM_ACCESSORY_HEX && HexMngr.IsMapLoaded()) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        bool rebuild_cache = false;
        if (prop == ItemView::PropertyIsColorize) {
            hex_item->RefreshAlpha();
        }
        else if (prop == ItemView::PropertyIsBadItem) {
            hex_item->SetSprite(nullptr);
        }
        else if (prop == ItemView::PropertyIsShootThru) {
            RebuildLookBorders = true, rebuild_cache = true;
        }
        else if (prop == ItemView::PropertyIsLightThru) {
            HexMngr.RebuildLight(), rebuild_cache = true;
        }
        else if (prop == ItemView::PropertyIsNoBlock) {
            rebuild_cache = true;
        }
        if (rebuild_cache) {
            HexMngr.GetField(hex_item->GetHexX(), hex_item->GetHexY()).ProcessCache();
        }
    }
}

void FOClient::OnSetItemSomeLight(Entity* /*entity*/, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    if (HexMngr.IsMapLoaded()) {
        HexMngr.RebuildLight();
    }
}

void FOClient::OnSetItemPicMap(Entity* entity, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    ItemView* item = dynamic_cast<ItemView*>(entity);

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->RefreshAnim();
    }
}

void FOClient::OnSetItemOffsetXY(Entity* entity, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    // OffsetX, OffsetY

    ItemView* item = dynamic_cast<ItemView*>(entity);

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX && HexMngr.IsMapLoaded()) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        hex_item->SetAnimOffs();
        HexMngr.ProcessHexBorders(hex_item);
    }
}

void FOClient::OnSetItemOpened(Entity* entity, Property* /*prop*/, void* cur_value, void* old_value)
{
    ItemView* item = dynamic_cast<ItemView*>(entity);
    const bool cur = *static_cast<bool*>(cur_value);
    const bool old = *static_cast<bool*>(old_value);

    if (item->GetIsCanOpen()) {
        ItemHexView* hex_item = dynamic_cast<ItemHexView*>(item);
        if (!old && cur) {
            hex_item->SetAnimFromStart();
        }
        if (old && !cur) {
            hex_item->SetAnimFromEnd();
        }
    }
}

void FOClient::OnSendMapValue(Entity* /*entity*/, Property* prop)
{
    if (prop->GetAccess() == Property::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Map, prop, Globals);
    }
    else {
        throw GenericException("Unable to send map modifiable property", prop->GetName());
    }
}

void FOClient::OnSendLocationValue(Entity* /*entity*/, Property* prop)
{
    if (prop->GetAccess() == Property::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Location, prop, Globals);
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
    if (RebuildLookBorders) {
        LookBordersPrepare();
        RebuildLookBorders = false;
    }

    // Map
    HexMngr.DrawMap();

    // Text on head
    for (auto& it : HexMngr.GetCritters()) {
        it.second->DrawTextOnHead();
    }

    // Texts on map
    const uint tick = GameTime.GameTick();
    for (auto it = GameMapTexts.begin(); it != GameMapTexts.end();) {
        MapText& mt = (*it);
        if (tick >= mt.StartTick + mt.Tick) {
            it = GameMapTexts.erase(it);
        }
        else {
            const uint dt = tick - mt.StartTick;
            const int procent = GenericUtils::Percent(mt.Tick, dt);
            const IRect r = mt.Pos.Interpolate(mt.EndPos, procent);
            Field& f = HexMngr.GetField(mt.HexX, mt.HexY);
            const int x = static_cast<int>((f.ScrX + (Settings.MapHexWidth / 2) + Settings.ScrOx) / Settings.SpritesZoom - 100.0f - static_cast<float>(mt.Pos.Left - r.Left));
            const int y = static_cast<int>((f.ScrY + (Settings.MapHexHeight / 2) - mt.Pos.Height() - (mt.Pos.Top - r.Top) + Settings.ScrOy) / Settings.SpritesZoom - 70.0f);

            uint color = mt.Color;
            if (mt.Fade) {
                color = (color ^ 0xFF000000) | ((0xFF * (100 - procent) / 100) << 24);
            }
            else if (mt.Tick > 500) {
                const uint hide = mt.Tick - 200;
                if (dt >= hide) {
                    const uint alpha = 0xFF * (100 - GenericUtils::Percent(mt.Tick - hide, dt - hide)) / 100;
                    color = (alpha << 24) | (color & 0xFFFFFF);
                }
            }

            SprMngr.DrawStr(IRect(x, y, x + 200, y + 70), mt.Text, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, FONT_DEFAULT);
            ++it;
        }
    }
}

void FOClient::AddMess(int mess_type, const string& msg)
{
    ScriptSys.MessageBoxEvent(msg, mess_type, false);
}

void FOClient::AddMess(int mess_type, const string& msg, bool script_call)
{
    ScriptSys.MessageBoxEvent(msg, mess_type, script_call);
}

void FOClient::FormatTags(string& text, CritterView* player, CritterView* npc, const string& lexems)
{
    if (text == "error") {
        text = "Text not found!";
        return;
    }

    vector<string> dialogs;
    int sex = 0;
    bool sex_tags = false;
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
                tag = (player != nullptr ? player->GetName() : "");
            }
            // Npc name
            else if (_str(tag).compareIgnoreCase("nname")) {
                tag = (npc != nullptr ? npc->GetName() : "");
            }
            // Sex
            else if (_str(tag).compareIgnoreCase("sex")) {
                sex = (player != nullptr ? player->GetGender() + 1 : 1);
                sex_tags = true;
                continue;
            }
            // Random
            else if (_str(tag).compareIgnoreCase("rnd")) {
                size_t first = text.find_first_of('|', i);
                size_t last = text.find_last_of('|', i);
                vector<string> rnd = _str(text.substr(first, last - first + 1)).split('|');
                text.erase(first, last - first + 1);
                if (!rnd.empty()) {
                    text.insert(first, rnd[GenericUtils::Random(0, static_cast<int>(rnd.size()) - 1)]);
                }
            }
            // Lexems
            else if (tag.length() > 4 && tag[0] == 'l' && tag[1] == 'e' && tag[2] == 'x' && tag[3] == ' ') {
                string lex = "$" + tag.substr(4);
                size_t pos = lexems.find(lex);
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
                    int msg_type = FOMsg::GetMsgType(msg_type_name);
                    if (msg_type < 0 || msg_type >= TEXTMSG_COUNT) {
                        tag = "<msg tag, unknown type>";
                    }
                    else if (CurLang.Msg[msg_type].Count(str_num) == 0u) {
                        tag = _str("<msg tag, string {} not found>", str_num);
                    }
                    else {
                        tag = CurLang.Msg[msg_type].GetStr(str_num);
                    }
                }
                else {
                    tag = "<msg tag parse fail>";
                }
            }
            // Script
            else if (tag.length() > 7 && tag[0] == 's' && tag[1] == 'c' && tag[2] == 'r' && tag[3] == 'i' && tag[4] == 'p' && tag[5] == 't' && tag[6] == ' ') {
                string func_name = _str(tag.substr(7)).substringUntil('$');
                if (!ScriptSys.CallFunc<string, string>(func_name, lexems, tag)) {
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

void FOClient::ShowMainScreen(int new_screen, map<string, int> params)
{
    while (GetActiveScreen(nullptr) != SCREEN_NONE) {
        HideScreen(SCREEN_NONE);
    }

    if (ScreenModeMain == new_screen) {
        return;
    }
    if (IsAutoLogin && new_screen == SCREEN_LOGIN) {
        return;
    }

    const int prev_main_screen = ScreenModeMain;
    if (ScreenModeMain != 0) {
        RunScreenScript(false, ScreenModeMain, {});
    }
    ScreenModeMain = new_screen;
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
            ScreenEffects.clear();
            WaitPic = ResMngr.GetRandomSplash();
        }
        break;
    default:
        break;
    }
}

auto FOClient::GetActiveScreen(vector<int>* screens) -> int
{
    vector<int> active_screens;
    ScriptSys.GetActiveScreensEvent(active_screens);

    if (screens != nullptr) {
        *screens = active_screens;
    }

    int active = (!active_screens.empty() ? active_screens.back() : SCREEN_NONE);
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

void FOClient::ShowScreen(int screen, map<string, int> params)
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

void FOClient::RunScreenScript(bool show, int screen, map<string, int> params)
{
    ScriptSys.ScreenChangeEvent(show, screen, std::move(params));
}

void FOClient::LmapPrepareMap()
{
    LmapPrepPix.clear();

    if (Chosen == nullptr) {
        return;
    }

    const int maxpixx = (LmapWMap[2] - LmapWMap[0]) / 2 / LmapZoom;
    const int maxpixy = (LmapWMap[3] - LmapWMap[1]) / 2 / LmapZoom;
    const int bx = Chosen->GetHexX() - maxpixx;
    const int by = Chosen->GetHexY() - maxpixy;
    const int ex = Chosen->GetHexX() + maxpixx;
    const int ey = Chosen->GetHexY() + maxpixy;

    const uint vis = Chosen->GetLookDistance();
    uint cur_color = 0;
    int pix_x = LmapWMap[2] - LmapWMap[0];
    int pix_y = 0;

    for (int i1 = bx; i1 < ex; i1++) {
        for (int i2 = by; i2 < ey; i2++) {
            pix_y += LmapZoom;
            if (i1 < 0 || i2 < 0 || i1 >= HexMngr.GetWidth() || i2 >= HexMngr.GetHeight()) {
                continue;
            }

            bool is_far = false;
            const uint dist = GeomHelper.DistGame(Chosen->GetHexX(), Chosen->GetHexY(), i1, i2);
            if (dist > vis) {
                is_far = true;
            }

            Field& f = HexMngr.GetField(i1, i2);
            if (f.Crit != nullptr) {
                cur_color = (f.Crit == Chosen ? 0xFF0000FF : 0xFFFF0000);
                LmapPrepPix.push_back({LmapWMap[0] + pix_x + (LmapZoom - 1), LmapWMap[1] + pix_y, cur_color});
                LmapPrepPix.push_back({LmapWMap[0] + pix_x, LmapWMap[1] + pix_y + ((LmapZoom - 1) / 2), cur_color});
            }
            else if (f.Flags.IsWall || f.Flags.IsScen) {
                if (f.Flags.ScrollBlock) {
                    continue;
                }
                if (!LmapSwitchHi && !f.Flags.IsWall) {
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
            LmapPrepPix.push_back({LmapWMap[0] + pix_x, LmapWMap[1] + pix_y, cur_color});
            LmapPrepPix.push_back({LmapWMap[0] + pix_x + (LmapZoom - 1), LmapWMap[1] + pix_y + ((LmapZoom - 1) / 2), cur_color});
        }
        pix_x -= LmapZoom;
        pix_y = 0;
    }

    LmapPrepareNextTick = GameTime.FrameTick() + MINIMAP_PREPARE_TICK;
}

void FOClient::GmapNullParams()
{
    GmapLoc.clear();
    GmapFog.Fill(0);
    DeleteCritters();
}

void FOClient::WaitDraw()
{
    SprMngr.DrawSpriteSize(WaitPic, 0, 0, Settings.ScreenWidth, Settings.ScreenHeight, true, true, 0);
    SprMngr.Flush();
}

void FOClient::OnItemInvChanged(ItemView* old_item, ItemView* item)
{
    ScriptSys.ItemInvChangedEvent(item, old_item);
    old_item->Release();
}
