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

#include "Client.h"
#include "ClientConnection.h"
#include "ClientScripting.h"
#include "GenericUtils.h"
#include "Log.h"
#include "NetCommand.h"
#include "StringUtils.h"
#include "Version-Include.h"

FOClient::FOClient(GlobalSettings& settings, AppWindow* window, bool mapper_mode) :
#if !FO_SINGLEPLAYER
    FOEngineBase(settings, PropertiesRelationType::ClientRelative),
#else
    FOEngineBase(settings, PropertiesRelationType::BothRelative),
#endif
    ProtoMngr(this),
    EffectMngr(Settings, Resources),
    SprMngr(Settings, window, Resources, EffectMngr),
    ResMngr(Resources, SprMngr, *this),
    SndMngr(Settings, Resources),
    Keyb(Settings, SprMngr),
    Cache(_str(Settings.ResourcesDir).combinePath("Cache.fobin")),
    ClientDeferredCalls(this),
    _conn(Settings),
    _worldmapFog(GM_MAXZONEX, GM_MAXZONEY, nullptr)
{
    Resources.AddDataSource(Settings.EmbeddedResources);
    Resources.AddDataSource(Settings.ResourcesDir, DataSourceType::DirRoot);

    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("EngineData"));
    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("Core"));
    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("Maps"));
    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("Protos"));
    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("Texts"));
    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("AngelScript"));

    for (const auto& entry : Settings.ClientResourceEntries) {
        Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath(entry));
    }

#if FO_IOS
    Resources.AddDataSource("../../Documents");
#elif FO_ANDROID
    Resources.AddDataSource("@AndroidAssets");
    // AddDataSource(SDL_AndroidGetInternalStoragePath());
    // AddDataSource(SDL_AndroidGetExternalStoragePath());
#elif FO_WEB
    Resources.AddDataSource("PersistentData");
#endif

    ResMngr.IndexFiles();

    GameTime.FrameAdvance();
    _fpsTick = GameTime.FrameTick();

    std::tie(Settings.MouseX, Settings.MouseY) = App->Input.GetMousePosition();

    // Language Packs
    _curLang.LoadTexts(Resources, Settings.Language);

    // Init 3d subsystem
#if FO_ENABLE_3D
    SprMngr.Init3dSubsystem(GameTime, *this, *this);

    if (!Preload3dFiles.empty()) {
        WriteLog("Preload 3d files...");
        for (const auto& name : Preload3dFiles) {
            SprMngr.Preload3dModel(name);
        }
        WriteLog("Preload 3d files complete");
    }
#endif

    EffectMngr.LoadDefaultEffects();

    if (mapper_mode) {
        return;
    }

#if !FO_SINGLEPLAYER
    if (const auto restore_info = Resources.ReadFile("RestoreInfo.fobin")) {
        extern void Client_RegisterData(FOEngineBase*, const vector<uchar>&);
        Client_RegisterData(this, restore_info.GetData());
    }
    else {
        throw EngineDataNotFoundException(LINE_STR);
    }

    ScriptSys = new ClientScriptSystem(this);
    ScriptSys->InitSubsystems();
#endif

    ProtoMngr.LoadFromResources();

    ResMngr.ReinitializeDynamicAtlas();

    // Recreate static atlas
    SprMngr.AccumulateAtlasData();
    SprMngr.PushAtlasType(AtlasType::Static);

    // Modules initialization
    ScriptSys->InitModules();
    OnStart.Fire();

    // Flush atlas data
    SprMngr.PopAtlasType();
    SprMngr.FlushAccumulatedAtlasData();

    // Finish fonts
    SprMngr.BuildFonts();

    // Connection handlers
    _conn.AddConnectHandler([this](bool success) { Net_OnConnect(success); });
    _conn.AddDisconnectHandler([this] { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NETMSG_WRONG_NET_PROTO, [this] { Net_OnWrongNetProto(); });
    _conn.AddMessageHandler(NETMSG_LOGIN_SUCCESS, [this] { Net_OnLoginSuccess(); });
    _conn.AddMessageHandler(NETMSG_REGISTER_SUCCESS, [this] { Net_OnRegisterSuccess(); });
    _conn.AddMessageHandler(NETMSG_PLACE_TO_GAME_COMPLETE, [this] { Net_OnPlaceToGameComplete(); });
    _conn.AddMessageHandler(NETMSG_ADD_CRITTER, [this] { Net_OnAddCritter(); });
    _conn.AddMessageHandler(NETMSG_REMOVE_CRITTER, [this] { Net_OnRemoveCritter(); });
    _conn.AddMessageHandler(NETMSG_SOME_ITEM, [this] { Net_OnSomeItem(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_ACTION, [this] { Net_OnCritterAction(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_MOVE_ITEM, [this] { Net_OnCritterMoveItem(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_ANIMATE, [this] { Net_OnCritterAnimate(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_SET_ANIMS, [this] { Net_OnCritterSetAnims(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_TELEPORT, [this] { Net_OnCritterTeleport(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_MOVE, [this] { Net_OnCritterMove(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_STOP_MOVE, [this] { Net_OnCritterStopMove(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_DIR, [this] { Net_OnCritterDir(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_POS, [this] { Net_OnCritterPos(); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(1, 0), [this] { Net_OnProperty(1); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(1, 1), [this] { Net_OnProperty(1); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(1, 2), [this] { Net_OnProperty(1); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(2, 0), [this] { Net_OnProperty(2); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(2, 1), [this] { Net_OnProperty(2); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(2, 2), [this] { Net_OnProperty(2); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(4, 0), [this] { Net_OnProperty(4); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(4, 1), [this] { Net_OnProperty(4); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(4, 2), [this] { Net_OnProperty(4); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(8, 0), [this] { Net_OnProperty(8); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(8, 1), [this] { Net_OnProperty(8); });
    _conn.AddMessageHandler(NETMSG_POD_PROPERTY(8, 2), [this] { Net_OnProperty(8); });
    _conn.AddMessageHandler(NETMSG_COMPLEX_PROPERTY, [this] { Net_OnProperty(0); });
    _conn.AddMessageHandler(NETMSG_CRITTER_TEXT, [this] { Net_OnText(); });
    _conn.AddMessageHandler(NETMSG_MSG, [this] { Net_OnTextMsg(false); });
    _conn.AddMessageHandler(NETMSG_MSG_LEX, [this] { Net_OnTextMsg(true); });
    _conn.AddMessageHandler(NETMSG_MAP_TEXT, [this] { Net_OnMapText(); });
    _conn.AddMessageHandler(NETMSG_MAP_TEXT_MSG, [this] { Net_OnMapTextMsg(); });
    _conn.AddMessageHandler(NETMSG_MAP_TEXT_MSG_LEX, [this] { Net_OnMapTextMsgLex(); });
    _conn.AddMessageHandler(NETMSG_ALL_PROPERTIES, [this] { Net_OnAllProperties(); });
    _conn.AddMessageHandler(NETMSG_CLEAR_ITEMS, [this] { Net_OnChosenClearItems(); });
    _conn.AddMessageHandler(NETMSG_ADD_ITEM, [this] { Net_OnChosenAddItem(); });
    _conn.AddMessageHandler(NETMSG_REMOVE_ITEM, [this] { Net_OnChosenEraseItem(); });
    _conn.AddMessageHandler(NETMSG_ALL_ITEMS_SEND, [this] { Net_OnAllItemsSend(); });
    _conn.AddMessageHandler(NETMSG_TALK_NPC, [this] { Net_OnChosenTalk(); });
    _conn.AddMessageHandler(NETMSG_TIME_SYNC, [this] { Net_OnTimeSync(); });
    _conn.AddMessageHandler(NETMSG_AUTOMAPS_INFO, [this] { Net_OnAutomapsInfo(); });
    _conn.AddMessageHandler(NETMSG_VIEW_MAP, [this] { Net_OnViewMap(); });
    _conn.AddMessageHandler(NETMSG_LOADMAP, [this] { Net_OnLoadMap(); });
    _conn.AddMessageHandler(NETMSG_GLOBAL_INFO, [this] { Net_OnGlobalInfo(); });
    _conn.AddMessageHandler(NETMSG_SOME_ITEMS, [this] { Net_OnSomeItems(); });
    _conn.AddMessageHandler(NETMSG_RPC, [this] { Net_OnRemoteCall(); });
    _conn.AddMessageHandler(NETMSG_ADD_ITEM_ON_MAP, [this] { Net_OnAddItemOnMap(); });
    _conn.AddMessageHandler(NETMSG_ERASE_ITEM_FROM_MAP, [this] { Net_OnEraseItemFromMap(); });
    _conn.AddMessageHandler(NETMSG_ANIMATE_ITEM, [this] { Net_OnAnimateItem(); });
    _conn.AddMessageHandler(NETMSG_EFFECT, [this] { Net_OnEffect(); });
    _conn.AddMessageHandler(NETMSG_FLY_EFFECT, [this] { Net_OnFlyEffect(); });
    _conn.AddMessageHandler(NETMSG_PLAY_SOUND, [this] { Net_OnPlaySound(); });
    _conn.AddMessageHandler(NETMSG_UPDATE_FILES_LIST, [this] { Net_OnUpdateFilesResponse(); });

    // Properties that sending to clients
    {
        const auto set_send_callbacks = [](const auto* registrator, const PropertyPostSetCallback& callback) {
            const auto count = static_cast<int>(registrator->GetCount());
            for (auto i = 0; i < count; i++) {
                const auto* prop = registrator->GetByIndex(i);

                switch (prop->GetAccess()) {
                case Property::AccessType::PublicModifiable:
                    [[fallthrough]];
                case Property::AccessType::PublicFullModifiable:
                    [[fallthrough]];
                case Property::AccessType::ProtectedModifiable:
                    break;
                default:
                    continue;
                }

                prop->AddPostSetter(callback);
            }
        };

        set_send_callbacks(GetPropertyRegistrator(GameProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendGlobalValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(PlayerProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendPlayerValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendItemValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendCritterValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendMapValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendLocationValue(entity, prop); });
    }

    // Properties with custom behaviours
    {
        const auto set_callback = [](const auto* registrator, int prop_index, PropertySetCallback callback) {
            const auto* prop = registrator->GetByIndex(prop_index);
            prop->AddSetter(std::move(callback));
        };

        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME), CritterView::ModelName_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetCritterModelName(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME), CritterView::ContourColor_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetCritterContourColor(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::IsColorize_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemFlags(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::IsBadItem_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemFlags(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::IsShootThru_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemFlags(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::IsLightThru_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemFlags(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::IsNoBlock_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemFlags(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::IsLight_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemSomeLight(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::LightIntensity_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemSomeLight(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::LightDistance_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemSomeLight(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::LightFlags_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemSomeLight(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::LightColor_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemSomeLight(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::PicMap_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemPicMap(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::OffsetX_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemOffsetCoords(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::OffsetY_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemOffsetCoords(entity, prop, data.GetPtrAs<void>()); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), ItemView::Opened_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemOpened(entity, prop, data.GetPtrAs<void>()); });
    }

    ScreenFadeOut();

    // Auto login
    ProcessAutoLogin();
}

FOClient::~FOClient()
{
    delete ScriptSys;
}

void FOClient::Shutdown()
{
    App->Render.SetRenderTarget(nullptr);
    _conn.Disconnect();
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

auto FOClient::IsConnecting() const -> bool
{
    return _conn.IsConnecting();
}

auto FOClient::IsConnected() const -> bool
{
    return _conn.IsConnected();
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

    // Network
    if (_initNetReason != INIT_NET_REASON_NONE && !_conn.IsConnecting()) {
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

    // Script subsystems update
    ScriptSys->Process();

    ClientDeferredCalls.Process();

#if !FO_SINGLEPLAYER
    // Script loop
    OnLoop.Fire();
#endif

    if (IsMainScreen(SCREEN_GLOBAL_MAP)) {
        ProcessGlobalMap();
    }

    if (CurMap != nullptr) {
        CurMap->Process();
    }

    // Render
    EffectMngr.UpdateEffects(GameTime);

    SprMngr.BeginScene(COLOR_RGB(0, 0, 0));
    {
        // Quake effect
        ProcessScreenEffectQuake();

        // Render map
        if (GetMainScreen() == SCREEN_GAME && CurMap != nullptr) {
            if (Settings.ShowMoveCursor) {
                CurMap->SetCursorPos(GetMapChosen(), Settings.MouseX, Settings.MouseY, Keyb.CtrlDwn, false);
            }

            CurMap->DrawMap();
        }

        // Make dirty offscreen surfaces
        if (!PreDirtyOffscreenSurfaces.empty()) {
            DirtyOffscreenSurfaces.insert(DirtyOffscreenSurfaces.end(), PreDirtyOffscreenSurfaces.begin(), PreDirtyOffscreenSurfaces.end());
            PreDirtyOffscreenSurfaces.clear();
        }

        CanDrawInScripts = true;
        OnRenderIface.Fire();
        CanDrawInScripts = false;

        ProcessScreenEffectFading();
    }
    SprMngr.EndScene();
}

void FOClient::ScreenFade(uint time, uint from_color, uint to_color, bool push_back)
{
    if (!push_back || _screenEffects.empty()) {
        _screenEffects.push_back({GameTime.FrameTick(), time, from_color, to_color});
    }
    else {
        uint last_tick = 0;
        for (const auto& e : _screenEffects) {
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

    vector<PrimitivePoint> full_screen_quad;
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

            SprMngr.DrawPoints(full_screen_quad, RenderPrimitiveType::TriangleList);
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
    std::tie(Settings.MouseX, Settings.MouseY) = App->Input.GetMousePosition();

    if (SprMngr.IsWindowFocused()) {
        InputEvent ev;
        while (App->Input.PollEvent(ev)) {
            ProcessInputEvent(ev);
        }
    }
    else {
        Keyb.Lost();
        OnInputLost.Fire();
    }
}

void FOClient::ProcessInputEvent(const InputEvent& ev)
{
    if (ev.Type == InputEvent::EventType::KeyDownEvent) {
        const auto key_code = ev.KeyDown.Code;
        const auto key_text = ev.KeyDown.Text;

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
    else if (ev.Type == InputEvent::EventType::KeyUpEvent) {
        const auto key_code = ev.KeyUp.Code;

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
    else if (ev.Type == InputEvent::EventType::MouseMoveEvent) {
        const auto mouse_x = ev.MouseMove.MouseX;
        const auto mouse_y = ev.MouseMove.MouseY;
        const auto delta_x = ev.MouseMove.DeltaX;
        const auto delta_y = ev.MouseMove.DeltaY;

        Settings.MouseX = mouse_x;
        Settings.MouseY = mouse_y;

        OnMouseMove.Fire(delta_x, delta_y);
    }
    else if (ev.Type == InputEvent::EventType::MouseDownEvent) {
        OnMouseDown.Fire(ev.MouseDown.Button);
    }
    else if (ev.Type == InputEvent::EventType::MouseUpEvent) {
        OnMouseUp.Fire(ev.MouseUp.Button);
    }
    else if (ev.Type == InputEvent::EventType::MouseWheelEvent) {
        if (ev.MouseWheel.Delta < 0) {
            OnMouseDown.Fire(MouseButton::WheelUp);
        }
        else {
            OnMouseDown.Fire(MouseButton::WheelDown);
        }
    }
}

void FOClient::Net_OnConnect(bool success)
{
    if (success) {
        // Reason
        const auto reason = _initNetReason;
        _initNetReason = INIT_NET_REASON_NONE;

        Net_SendHandshake();

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

        RUNTIME_ASSERT(!_curPlayer);
        _curPlayer = new PlayerView(this, 0u);
    }
    else {
        ShowMainScreen(SCREEN_LOGIN, {});
        AddMessage(0, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_FAIL));
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

void FOClient::Net_SendHandshake()
{
    NON_CONST_METHOD_HINT();

    _conn.OutBuf << NETMSG_HANDSHAKE;
    _conn.OutBuf << static_cast<uint>(FO_COMPATIBILITY_VERSION);

    const auto encrypt_key = NetBuffer::GenerateEncryptKey();
    _conn.OutBuf << encrypt_key;
    _conn.OutBuf.SetEncryptKey(encrypt_key);
    _conn.InBuf.SetEncryptKey(encrypt_key);

    constexpr uchar padding[28] = {};
    _conn.OutBuf.Push(padding, sizeof(padding));
}

void FOClient::Net_SendLogIn()
{
    WriteLog("Player login");

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + NetBuffer::STRING_LEN_SIZE * 2u + static_cast<uint>(_loginName.length() + _loginPassword.length());

    _conn.OutBuf << NETMSG_LOGIN;
    _conn.OutBuf << msg_len;
    _conn.OutBuf << _loginName;
    _conn.OutBuf << _loginPassword;

    AddMessage(0, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_CONN_SUCCESS));
}

void FOClient::Net_SendCreatePlayer()
{
    NON_CONST_METHOD_HINT();

    WriteLog("Player registration");

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + NetBuffer::STRING_LEN_SIZE * 2u + static_cast<uint>(_loginName.length() + _loginPassword.length());

    _conn.OutBuf << NETMSG_REGISTER;
    _conn.OutBuf << msg_len;
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

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(how_say) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(str.length());

    _conn.OutBuf << NETMSG_SEND_TEXT;
    _conn.OutBuf << msg_len;
    _conn.OutBuf << how_say;
    _conn.OutBuf << str;
}

void FOClient::Net_SendDir(CritterHexView* cr)
{
    NON_CONST_METHOD_HINT();

    _conn.OutBuf << NETMSG_DIR;
    _conn.OutBuf << CurMap->GetId();
    _conn.OutBuf << cr->GetId();
    _conn.OutBuf << cr->GetDirAngle();
}

void FOClient::Net_SendMove(CritterHexView* cr)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(!cr->Moving.Steps.empty());

    if (cr->Moving.Steps.size() > 500) {
        cr->ClearMove();
        return;
    }

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) * 2 + sizeof(ushort) + sizeof(ushort) * 2 + //
        sizeof(ushort) + static_cast<uint>(sizeof(uchar) * cr->Moving.Steps.size()) + //
        sizeof(ushort) + static_cast<uint>(sizeof(ushort) * cr->Moving.ControlSteps.size()) + sizeof(short) * 2;

    _conn.OutBuf << NETMSG_SEND_MOVE;
    _conn.OutBuf << msg_len;
    _conn.OutBuf << CurMap->GetId();
    _conn.OutBuf << cr->GetId();
    _conn.OutBuf << cr->Moving.Speed;
    _conn.OutBuf << cr->Moving.StartHexX;
    _conn.OutBuf << cr->Moving.StartHexY;
    _conn.OutBuf << static_cast<ushort>(cr->Moving.Steps.size());
    for (auto step : cr->Moving.Steps) {
        _conn.OutBuf << step;
    }
    _conn.OutBuf << static_cast<ushort>(cr->Moving.ControlSteps.size());
    for (auto control_step : cr->Moving.ControlSteps) {
        _conn.OutBuf << control_step;
    }
    _conn.OutBuf << cr->Moving.EndOx;
    _conn.OutBuf << cr->Moving.EndOy;
}

void FOClient::Net_SendStopMove(CritterHexView* cr)
{
    NON_CONST_METHOD_HINT();

    _conn.OutBuf << NETMSG_SEND_STOP_MOVE;
    _conn.OutBuf << CurMap->GetId();
    _conn.OutBuf << cr->GetId();
    _conn.OutBuf << cr->GetHexX();
    _conn.OutBuf << cr->GetHexY();
    _conn.OutBuf << cr->GetHexOffsX();
    _conn.OutBuf << cr->GetHexOffsY();
    _conn.OutBuf << cr->GetDirAngle();
}

void FOClient::Net_SendProperty(NetProperty type, const Property* prop, Entity* entity)
{
    NON_CONST_METHOD_HINT();
    RUNTIME_ASSERT(entity);

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

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
        const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(char) + additional_args * sizeof(uint) + sizeof(ushort) + data_size;
        _conn.OutBuf << NETMSG_SEND_COMPLEX_PROPERTY;
        _conn.OutBuf << msg_len;
    }

    _conn.OutBuf << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        _conn.OutBuf << dynamic_cast<ItemView*>(client_entity)->GetCritterId();
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
    NON_CONST_METHOD_HINT();

    _conn.OutBuf << NETMSG_SEND_TALK_NPC;
    _conn.OutBuf << is_npc;
    _conn.OutBuf << id_to_talk;
    _conn.OutBuf << answer;
}

void FOClient::Net_SendPing(uchar ping)
{
    NON_CONST_METHOD_HINT();

    _conn.OutBuf << NETMSG_PING;
    _conn.OutBuf << ping;
}

void FOClient::Net_OnUpdateFilesResponse()
{
    uint msg_len;
    bool outdated;
    uint data_size;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> outdated;
    _conn.InBuf >> data_size;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    vector<uchar> data;
    data.resize(data_size);
    if (data_size != 0u) {
        _conn.InBuf.Pop(data.data(), data_size);
    }

    if (!outdated) {
        NET_READ_PROPERTIES(_conn.InBuf, _globalsPropertiesData);
    }

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    // RestoreData(_globalsPropertiesData);

    // Todo: run updater if resources changed

    /*if (outdated) {
        AddMessage(0, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_CLIENT_OUTDATED));
    }

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
        if (_resources.ReadFileHeader(fname)) {
            const auto file_hash = _resources.ReadFileText(_str("{}.hash", fname));
            if (file_hash.empty()) {
                // Hashing::MurmurHash2(file2.GetBuf(), file2.GetSize());
            }

            if (_str(file_hash).toUInt() == hash) {
                continue;
            }
        }
    }

    throw ResourcesOutdatedException("...");*/
}

void FOClient::Net_OnWrongNetProto()
{
    AddMessage(0, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_CLIENT_OUTDATED));
}

void FOClient::Net_OnRegisterSuccess()
{
    WriteLog("Registration success");

    OnRegistrationSuccess.Fire();
}

void FOClient::Net_OnLoginSuccess()
{
    WriteLog("Authentication success");

    AddMessage(0, _curLang.Msg[TEXTMSG_GAME].GetStr(STR_NET_LOGINOK));

    uint msg_len;
    uint encrypt_key;
    uint player_id;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> encrypt_key;
    _conn.InBuf >> player_id;

    NET_READ_PROPERTIES(_conn.InBuf, _globalsPropertiesData);
    NET_READ_PROPERTIES(_conn.InBuf, _playerPropertiesData);

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    _conn.InBuf.SetEncryptKey(encrypt_key);

    RestoreData(_globalsPropertiesData);

    RUNTIME_ASSERT(_curPlayer);
    RUNTIME_ASSERT(_curPlayer->GetId() == 0u);
    _curPlayer->SetId(player_id);
    _curPlayer->RestoreData(_playerPropertiesData);

    OnLoginSuccess.Fire();
}

void FOClient::Net_OnAddCritter()
{
    uint msg_len;
    _conn.InBuf >> msg_len;

    uint crid;
    ushort hx;
    ushort hy;
    short hex_ox;
    short hex_oy;
    short dir_angle;
    _conn.InBuf >> crid;
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> hex_ox;
    _conn.InBuf >> hex_oy;
    _conn.InBuf >> dir_angle;

    CritterCondition cond;
    uint anim1_alive;
    uint anim1_ko;
    uint anim1_dead;
    uint anim2_alive;
    uint anim2_ko;
    uint anim2_dead;
    _conn.InBuf >> cond;
    _conn.InBuf >> anim1_alive;
    _conn.InBuf >> anim1_ko;
    _conn.InBuf >> anim1_dead;
    _conn.InBuf >> anim2_alive;
    _conn.InBuf >> anim2_ko;
    _conn.InBuf >> anim2_dead;

    bool is_owned_by_player;
    bool is_player_offline;
    bool is_chosen;
    _conn.InBuf >> is_owned_by_player;
    _conn.InBuf >> is_player_offline;
    _conn.InBuf >> is_chosen;

    const hstring pid = _conn.InBuf.ReadHashedString(*this);
    const auto* proto = ProtoMngr.GetProtoCritter(pid);
    RUNTIME_ASSERT(proto);

    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->AddCritter(crid, proto, hx, hy, _tempPropertiesData);
    cr->SetHexOffsX(hex_ox);
    cr->SetHexOffsY(hex_oy);
    cr->ChangeDirAngle(dir_angle);
    cr->SetCond(cond);
    cr->SetAnim1Alive(anim1_alive);
    cr->SetAnim1Knockout(anim1_ko);
    cr->SetAnim1Dead(anim1_dead);
    cr->SetAnim2Alive(anim2_alive);
    cr->SetAnim2Knockout(anim2_ko);
    cr->SetAnim2Dead(anim2_dead);
    cr->SetPlayer(is_owned_by_player, is_chosen);
    if (is_owned_by_player) {
        cr->SetPlayerOffline(is_player_offline);
    }

    if (cr->IsChosen()) {
        _chosen = cr;
        _chosen->AddRef();
    }

    OnCritterIn.Fire(cr);

    if (cr->IsChosen()) {
        CurMap->RebuildFog();
    }
}

void FOClient::Net_OnRemoveCritter()
{
    uint remove_crid;
    _conn.InBuf >> remove_crid;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (how_say == SAY_FLASH_WINDOW) {
        FlashGameWindow();
        return;
    }

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value {}", msg_num);
        return;
    }

    if (const auto& msg = _curLang.Msg[msg_num]; msg.Count(num_str) != 0u) {
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

    const auto get_format = [this](uint str_num) -> string { return _str(_curLang.Msg[TEXTMSG_GAME].GetStr(str_num)).replace('\\', 'n', '\n').replace("%s", "{}").replace("%d", "{}"); };

    auto* cr = (how_say != SAY_RADIO ? (CurMap != nullptr ? CurMap->GetCritter(crid) : nullptr) : nullptr);

    // Critter text on head
    if (fstr_cr != 0u && cr != nullptr) {
        cr->SetText(_str(get_format(fstr_cr), fstr), COLOR_TEXT, text_delay);
    }

    // Message box text
    if (fstr_mb != 0u) {
        if (how_say == SAY_NETMSG) {
            AddMessage(0, _str(get_format(fstr_mb), fstr));
        }
        else if (how_say == SAY_RADIO) {
            ushort channel = 0u;
            if (auto* chosen = GetChosen(); chosen != nullptr) {
                const auto* radio = chosen->GetItem(crid);
                if (radio != nullptr) {
                    channel = radio->GetRadioChannel();
                }
            }
            AddMessage(0, _str(get_format(fstr_mb), channel, fstr));
        }
        else {
            const auto cr_name = (cr != nullptr ? cr->GetName() : "?");
            AddMessage(0, _str(get_format(fstr_mb), cr_name, fstr));
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight()) {
        WriteLog("Invalid coords, hx {}, hy {}, text '{}'", hx, hy, text);
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}", msg_num);
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (msg_num >= TEXTMSG_COUNT) {
        WriteLog("Msg num invalid value, num {}", msg_num);
        return;
    }

    auto str = _curLang.Msg[msg_num].GetStr(num_str);
    FormatTags(str, GetChosen(), nullptr, lexems);
    OnMapText(str, hx, hy, color);
}

void FOClient::Net_OnCritterDir()
{
    uint crid;
    short dir_angle;
    _conn.InBuf >> crid;
    _conn.InBuf >> dir_angle;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
    if (cr != nullptr) {
        cr->ChangeLookDirAngle(dir_angle);
    }
}

void FOClient::Net_OnCritterMove()
{
    uint msg_len;
    uint cr_id;
    uint whole_time;
    uint offset_time;
    ushort speed;
    ushort start_hx;
    ushort start_hy;
    ushort steps_count;
    vector<uchar> steps;
    ushort control_steps_count;
    vector<ushort> control_steps;
    short end_hex_ox;
    short end_hex_oy;

    _conn.InBuf >> msg_len;
    _conn.InBuf >> cr_id;
    _conn.InBuf >> whole_time;
    _conn.InBuf >> offset_time;
    _conn.InBuf >> speed;
    _conn.InBuf >> start_hx;
    _conn.InBuf >> start_hy;
    _conn.InBuf >> steps_count;
    steps.resize(steps_count);
    for (auto i = 0; i < steps_count; i++) {
        _conn.InBuf >> steps[i];
    }
    _conn.InBuf >> control_steps_count;
    control_steps.resize(control_steps_count);
    for (auto i = 0; i < control_steps_count; i++) {
        _conn.InBuf >> control_steps[i];
    }
    _conn.InBuf >> end_hex_ox;
    _conn.InBuf >> end_hex_oy;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);
    if (cr == nullptr) {
        return;
    }

    cr->ClearMove();

    cr->Moving.Speed = speed;
    cr->Moving.StartTick = GameTime.FrameTick();
    cr->Moving.OffsetTick = offset_time;
    cr->Moving.WholeTime = static_cast<float>(whole_time);

    cr->Moving.Steps = steps;
    cr->Moving.ControlSteps = control_steps;
    cr->Moving.StartHexX = start_hx;
    cr->Moving.StartHexY = start_hy;
    cr->Moving.StartOx = cr->GetHexOffsX();
    cr->Moving.StartOy = cr->GetHexOffsY();
    cr->Moving.EndOx = end_hex_ox;
    cr->Moving.EndOy = end_hex_oy;

    if (offset_time == 0 && (start_hx != cr->GetHexX() || start_hy != cr->GetHexY())) {
        auto&& [cr_ox, cr_oy] = Geometry.GetHexInterval(start_hx, start_hy, cr->GetHexX(), cr->GetHexY());
        cr->Moving.StartOx = static_cast<short>(cr->Moving.StartOx + cr_ox);
        cr->Moving.StartOy = static_cast<short>(cr->Moving.StartOy + cr_oy);
    }

    cr->Moving.WholeDist = 0.0f;

    auto control_step_begin = 0;
    for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
        auto hx = start_hx;
        auto hy = start_hy;

        RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = Geometry.MoveHexByDir(hx, hy, cr->Moving.Steps[j], CurMap->GetWidth(), CurMap->GetHeight());
            RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = Geometry.GetHexInterval(start_hx, start_hy, hx, hy);

        if (i == 0) {
            ox -= cr->Moving.StartOx;
            oy -= cr->Moving.StartOy;
        }
        if (i == cr->Moving.ControlSteps.size() - 1) {
            ox += cr->Moving.EndOx;
            oy += cr->Moving.EndOy;
        }

        const auto proj_oy = static_cast<float>(oy) * Geometry.GetYProj();
        const auto dist = std::sqrt(static_cast<float>(ox * ox) + proj_oy * proj_oy);

        cr->Moving.WholeDist += dist;

        control_step_begin = cr->Moving.ControlSteps[i];
        start_hx = hx;
        start_hy = hy;

        cr->Moving.EndHexX = hx;
        cr->Moving.EndHexY = hy;
    }

    if (cr->Moving.WholeTime < 0.0001f) {
        cr->Moving.WholeTime = 0.0001f;
    }
    if (cr->Moving.WholeDist < 0.0001f) {
        cr->Moving.WholeDist = 0.0001f;
    }

    RUNTIME_ASSERT(!cr->Moving.Steps.empty());
    RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
    RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
    RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

    cr->AnimateStay();
}

void FOClient::Net_OnCritterStopMove()
{
    uint cr_id;
    ushort start_hx;
    ushort start_hy;
    short hex_ox;
    short hex_oy;
    short dir_angle;

    _conn.InBuf >> cr_id;
    _conn.InBuf >> start_hx;
    _conn.InBuf >> start_hy;
    _conn.InBuf >> hex_ox;
    _conn.InBuf >> hex_oy;
    _conn.InBuf >> dir_angle;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);
    if (cr == nullptr) {
        return;
    }

    cr->ClearMove();
    cr->AnimateStay();
}

void FOClient::Net_OnSomeItem()
{
    uint msg_len;
    uint item_id;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> item_id;
    const hstring item_pid = _conn.InBuf.ReadHashedString(*this);

    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (!cr->IsChosen()) {
        int64 prev_hash_sum = 0;
        for (const auto* item : cr->GetItems()) {
            prev_hash_sum += item->LightGetHash();
        }

        cr->DeleteAllItems();

        for (ushort i = 0; i < slots_data_count; i++) {
            const auto* proto_item = ProtoMngr.GetProtoItem(slots_data_pid[i]);
            RUNTIME_ASSERT(proto_item != nullptr);
            cr->AddItem(slots_data_id[i], proto_item, slots_data_slot[i], slots_data_data[i]);
        }

        int64 hash_sum = 0;
        for (const auto* item : cr->GetItems()) {
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

void FOClient::Net_OnCritterTeleport()
{
    uint crid;
    ushort to_hx;
    ushort to_hy;
    _conn.InBuf >> crid;
    _conn.InBuf >> to_hx;
    _conn.InBuf >> to_hy;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    CurMap->MoveCritter(cr, to_hx, to_hy);

    if (cr->IsChosen()) {
        if (CurMap->AutoScroll.HardLockedCritter == cr->GetId() || CurMap->AutoScroll.SoftLockedCritter == cr->GetId()) {
            CurMap->AutoScroll.CritterLastHexX = cr->GetHexX();
            CurMap->AutoScroll.CritterLastHexY = cr->GetHexY();
        }

        CurMap->ScrollToHex(cr->GetHexX(), cr->GetHexY(), 0.1f, false);

        // Maybe changed some parameter influencing on look borders
        CurMap->RebuildFog();
    }
}

void FOClient::Net_OnCritterPos()
{
    uint crid;
    ushort hx;
    ushort hy;
    short hex_ox;
    short hex_oy;
    short dir_angle;
    _conn.InBuf >> crid;
    _conn.InBuf >> hx;
    _conn.InBuf >> hy;
    _conn.InBuf >> hex_ox;
    _conn.InBuf >> hex_oy;
    _conn.InBuf >> dir_angle;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* cr = CurMap->GetCritter(crid);
    if (cr == nullptr) {
        return;
    }

    if (hx >= CurMap->GetWidth() || hy >= CurMap->GetHeight()) {
        return;
    }

    cr->ClearMove();

    cr->ChangeLookDirAngle(dir_angle);
    cr->ChangeMoveDirAngle(dir_angle);

    if (cr->GetHexX() != hx || cr->GetHexY() != hy) {
        CurMap->TransitCritter(cr, hx, hy, true);

        if (cr->IsChosen()) {
            CurMap->RebuildFog();
        }
    }

    if (cr->GetHexX() != hx || cr->GetHexY() != hy) {
        cr->Moving.RealHexX = hx;
        cr->Moving.RealHexY = hy;
    }
    else {
        cr->Moving.RealHexX = 0;
        cr->Moving.RealHexY = 0;
    }

    if (cr->GetHexOffsX() != hex_ox || cr->GetHexOffsY() != hex_oy) {
        cr->AddExtraOffs(cr->GetHexOffsX() - hex_ox, cr->GetHexOffsY() - hex_oy);
        cr->SetHexOffsX(hex_ox);
        cr->SetHexOffsY(hex_oy);
        cr->RefreshOffs();
    }
}

void FOClient::Net_OnAllProperties()
{
    WriteLog("Chosen properties");

    uint msg_len;
    _conn.InBuf >> msg_len;

    NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen not created, skip");
        return;
    }

    chosen->RestoreData(_tempPropertiesData);

    // Animate
    if (auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr && !hex_chosen->IsAnim()) {
        hex_chosen->AnimateStay();
    }

    // Refresh borders
    CurMap->RebuildFog();
}

void FOClient::Net_OnChosenClearItems()
{
    _initialItemsSend = true;

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in clear items");
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in add item");
        return;
    }

    auto* prev_item = chosen->GetItem(item_id);
    uchar prev_slot = 0;
    uint prev_light_hash = 0;
    if (prev_item != nullptr) {
        prev_slot = prev_item->GetCritterSlot();
        prev_light_hash = prev_item->LightGetHash();
        chosen->DeleteItem(prev_item, false);
    }

    const auto* proto_item = ProtoMngr.GetProtoItem(pid);
    RUNTIME_ASSERT(proto_item);

    auto* item = chosen->AddItem(item_id, proto_item, slot, _tempPropertiesData);

    item->AddRef();

    CurMap->RebuildFog();

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in erase item");
        return;
    }

    auto* item = chosen->GetItem(item_id);
    if (item == nullptr) {
        WriteLog("Item not found, id {}", item_id);
        return;
    }

    auto* item_clone = item->CreateRefClone();

    const auto rebuild_light = (item->GetIsLight() && item->GetCritterSlot() != 0u);
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
        WriteLog("Chosen is not created in all items send");
        return;
    }

#if FO_ENABLE_3D
    if (auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr && hex_chosen->IsModel()) {
        hex_chosen->GetModel()->StartMeshGeneration();
    }
#endif

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* item = CurMap->AddItem(item_id, item_pid, item_hx, item_hy, is_added, &_tempPropertiesData);
    if (item != nullptr) {
        OnItemMapIn.Fire(item);

        // Refresh borders
        if (!item->GetIsShootThru()) {
            CurMap->RebuildFog();
        }
    }
}

void FOClient::Net_OnEraseItemFromMap()
{
    uint item_id;
    bool is_deleted;
    _conn.InBuf >> item_id;
    _conn.InBuf >> is_deleted;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (CurMap == nullptr) {
        return;
    }

    auto* item = CurMap->GetItem(item_id);
    if (item != nullptr) {
        OnItemMapOut.Fire(item);

        // Refresh borders
        if (!item->GetIsShootThru()) {
            CurMap->RebuildFog();
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    auto* item = CurMap->GetItem(item_id);
    if (item != nullptr) {
        item->SetAnim(from_frm, to_frm);
    }
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    // Base hex effect
    CurMap->RunEffect(eff_pid, hx, hy, hx, hy);

    // Radius hexes effect
    if (radius > MAX_HEX_OFFSET) {
        radius = MAX_HEX_OFFSET;
    }

    const auto [sx, sy] = Geometry.GetHexOffsets((hx % 2) != 0);
    const auto maxhx = CurMap->GetWidth();
    const auto maxhy = CurMap->GetHeight();
    const auto count = GenericUtils::NumericalNumber(radius) * GameSettings::MAP_DIR_COUNT;

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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
        WriteLog("Run effect '{}' failed", eff_pid);
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    SndMngr.PlaySound(ResMngr.GetSoundNames(), sound_name);
}

void FOClient::Net_OnPlaceToGameComplete()
{
    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        WriteLog("Chosen is not created in end parse to game");
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

    WriteLog("Entering to game complete");
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

    PropertyRawData prop_data;

    if (data_size != 0u) {
        _conn.InBuf.Pop(prop_data.Alloc(data_size), data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
        if (len != 0u) {
            _conn.InBuf.Pop(prop_data.Alloc(len), len);
        }
    }

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    RUNTIME_ASSERT(!prop->IsDisabled());
    RUNTIME_ASSERT(!prop->IsVirtual());

    {
        _sendIgnoreEntity = entity;
        _sendIgnoreProperty = prop;

        auto revert_send_ignore = ScopeCallback([this]() noexcept {
            _sendIgnoreEntity = nullptr;
            _sendIgnoreProperty = nullptr;
        });

        entity->SetValueFromData(prop, prop_data);
    }

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    auto str = _curLang.Msg[TEXTMSG_DLG].GetStr(text_id);
    FormatTags(str, GetChosen(), npc, lexems);
    auto text_to_script = str;
    vector<string> answers_to_script;
    for (const auto answers_text : answers_texts) {
        str = _curLang.Msg[TEXTMSG_DLG].GetStr(answers_text);
        FormatTags(str, GetChosen(), npc, lexems);
        answers_to_script.push_back(str);
    }

    _conn.InBuf >> talk_time;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

void FOClient::Net_OnTimeSync()
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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
    WriteLog("Change map..");

    uint msg_len;
    uint loc_id;
    uint map_id;
    uchar map_index_in_loc;

    _conn.InBuf >> msg_len;
    _conn.InBuf >> loc_id;
    _conn.InBuf >> map_id;
    const auto loc_pid = _conn.InBuf.ReadHashedString(*this);
    const auto map_pid = _conn.InBuf.ReadHashedString(*this);
    _conn.InBuf >> map_index_in_loc;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    if (map_pid) {
        NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesData);
        NET_READ_PROPERTIES(_conn.InBuf, _tempPropertiesDataExt);
    }

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    OnMapUnload.Fire();

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

    SndMngr.StopSounds();
    ShowMainScreen(SCREEN_WAIT, {});
    ResMngr.ReinitializeDynamicAtlas();

    Settings.SpritesZoom = 1.0f;
    CurMapPid = map_pid;
    _curMapLocPid = loc_pid;
    _curMapIndexInLoc = map_index_in_loc;

    if (map_pid) {
        SetDayTime(true);

        _curLocation = new LocationView(this, loc_id, ProtoMngr.GetProtoLocation(loc_pid));
        _curLocation->RestoreData(_tempPropertiesDataExt);

        CurMap = new MapView(this, map_id, ProtoMngr.GetProtoMap(map_pid));
        CurMap->RestoreData(_tempPropertiesData);
        CurMap->LoadStaticData();

        WriteLog("Local map loaded");
    }
    else {
        GmapNullParams();

        WriteLog("Global map loaded");
    }

    OnMapLoad.Fire();
}

void FOClient::Net_OnGlobalInfo()
{
    uint msg_len;
    uchar info_flags;
    _conn.InBuf >> msg_len;
    _conn.InBuf >> info_flags;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

    CHECK_SERVER_IN_BUF_ERROR(_conn);
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

    CHECK_SERVER_IN_BUF_ERROR(_conn);

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

void FOClient::Net_OnRemoteCall()
{
    uint msg_len;
    _conn.InBuf >> msg_len;
    uint rpc_num;
    _conn.InBuf >> rpc_num;

    CHECK_SERVER_IN_BUF_ERROR(_conn);

    ScriptSys->HandleRemoteCall(rpc_num, _curPlayer);
}

void FOClient::SetDayTime(bool refresh)
{
    if (refresh) {
        _prevDayTimeColor.reset();
    }

    if (CurMap == nullptr) {
        return;
    }

    const auto color = GenericUtils::GetColorDay(CurMap->GetMapDayTime(), CurMap->GetMapDayColor(), CurMap->GetMapTime(), nullptr);

    if (!_prevDayTimeColor.has_value() || _prevDayTimeColor != color) {
        _prevDayTimeColor = color;
        SprMngr.SetSpritesTreeColor(color);
        CurMap->RefreshMap();
    }
}

void FOClient::ProcessGlobalMap()
{
    // Todo: global map critters
    throw NotImplementedException(LINE_STR);
}

void FOClient::TryExit()
{
    const auto active = GetActiveScreen(nullptr);
    if (active != SCREEN_NONE) {
        HideScreen(SCREEN_NONE);
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

void FOClient::AnimFree(uint anim_id)
{
    RUNTIME_ASSERT(anim_id < _ifaceAnimations.size());

    if (_ifaceAnimations[anim_id]) {
        delete _ifaceAnimations[anim_id];
        _ifaceAnimations[anim_id] = nullptr;
    }
}

auto FOClient::AnimGetCurSpr(uint anim_id) const -> uint
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->Frames->Ind[_ifaceAnimations[anim_id]->CurSpr];
}

auto FOClient::AnimGetCurSprCnt(uint anim_id) const -> uint
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->CurSpr;
}

auto FOClient::AnimGetSprCount(uint anim_id) const -> uint
{
    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->Frames->CntFrm;
}

auto FOClient::AnimGetFrames(uint anim_id) -> AnyFrames*
{
    NON_CONST_METHOD_HINT();

    if (anim_id >= _ifaceAnimations.size() || (_ifaceAnimations[anim_id] == nullptr)) {
        return 0;
    }
    return _ifaceAnimations[anim_id]->Frames;
}

void FOClient::AnimRun(uint anim_id, uint flags)
{
    NON_CONST_METHOD_HINT();

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
    NON_CONST_METHOD_HINT();

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
    RUNTIME_ASSERT(entity == _curPlayer);

    if (_curPlayer->GetId() == 0u) {
        throw ScriptException("Can't modify player public/protected property on unlogined player");
    }

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

void FOClient::OnSendItemValue(Entity* entity, const Property* prop)
{
    if (auto* item = dynamic_cast<ItemView*>(entity); item != nullptr && item->GetId() != 0u) {
        if (item->GetOwnership() == ItemOwnership::CritterInventory) {
            const auto* cr = CurMap->GetCritter(item->GetCritterId());
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
    RUNTIME_ASSERT(entity == _curLocation);

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Location, prop, _curLocation);
    }
    else {
        throw GenericException("Unable to send location modifiable property", prop->GetName());
    }
}

void FOClient::OnSetCritterModelName(Entity* entity, const Property* prop, const void* new_value)
{
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(new_value);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
#if FO_ENABLE_3D
        cr->RefreshModel();
#endif
        cr->Action(ACTION_REFRESH, 0, nullptr, false);
    }
}

void FOClient::OnSetCritterContourColor(Entity* entity, const Property* prop, const void* new_value)
{
    UNUSED_VARIABLE(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr && cr->SprDrawValid) {
        cr->SprDraw->SetContour(cr->SprDraw->Contour, *static_cast<const uint*>(new_value));
    }
}

void FOClient::OnSetItemFlags(Entity* entity, const Property* prop, const void* new_value)
{
    // IsColorize, IsBadItem, IsShootThru, IsLightThru, IsNoBlock

    UNUSED_VARIABLE(new_value);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        auto rebuild_cache = false;
        if (prop == item->GetPropertyIsColorize()) {
            item->RefreshAlpha();
        }
        else if (prop == item->GetPropertyIsBadItem()) {
            item->SetSprite(nullptr);
        }
        else if (prop == item->GetPropertyIsShootThru()) {
            CurMap->RebuildFog();
            rebuild_cache = true;
        }
        else if (prop == item->GetPropertyIsLightThru()) {
            item->GetMap()->RebuildLight();
            rebuild_cache = true;
        }
        else if (prop == item->GetPropertyIsNoBlock()) {
            rebuild_cache = true;
        }
        if (rebuild_cache) {
            item->GetMap()->GetField(item->GetHexX(), item->GetHexY()).ProcessCache();
        }
    }
}

void FOClient::OnSetItemSomeLight(Entity* entity, const Property* prop, const void* new_value)
{
    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(new_value);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->GetMap()->RebuildLight();
    }
}

void FOClient::OnSetItemPicMap(Entity* entity, const Property* prop, const void* new_value)
{
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(new_value);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->RefreshAnim();
    }
}

void FOClient::OnSetItemOffsetCoords(Entity* entity, const Property* prop, const void* new_value)
{
    // OffsetX, OffsetY

    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(new_value);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->SetAnimOffs();
        item->GetMap()->ProcessHexBorders(item);
    }
}

void FOClient::OnSetItemOpened(Entity* entity, const Property* prop, const void* new_value)
{
    UNUSED_VARIABLE(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        const auto new_bool = *static_cast<const bool*>(new_value);

        if (item->GetIsCanOpen()) {
            if (new_bool) {
                item->SetAnimFromStart();
            }
            if (!new_bool) {
                item->SetAnimFromEnd();
            }
        }
    }
}

void FOClient::AddMessage(uchar mess_type, string_view msg)
{
    OnMessageBox.Fire(mess_type, string(msg));
}

// Todo: move targs formatting to scripts
void FOClient::FormatTags(string& text, CritterView* cr, CritterView* npc, string_view lexems)
{
    NON_CONST_METHOD_HINT();

    if (text == "error") {
        text = "Text not found!";
        return;
    }

    vector<string> dialogs;
    auto sex = 0;
    auto sex_tags = false;
    string tag;

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
                if (!ScriptSys->CallFunc<string, string>(ToHashedString(func_name), string(lexems), tag)) {
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
            const auto dist = Geometry.DistGame(chosen->GetHexX(), chosen->GetHexY(), i1, i2);
            if (dist > vis) {
                is_far = true;
            }

            auto& f = CurMap->GetField(static_cast<ushort>(i1), static_cast<ushort>(i2));
            uint cur_color;

            if (const auto* cr = f.GetActiveCritter(); cr != nullptr) {
                cur_color = (cr == chosen ? 0xFF0000FF : 0xFFFF0000);
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
    if (_waitPic != nullptr) {
        SprMngr.DrawSpriteSize(_waitPic->GetCurSprId(GameTime.GameTick()), 0, 0, Settings.ScreenWidth, Settings.ScreenHeight, true, true, 0);
        SprMngr.Flush();
    }
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
        args.emplace_back(command);
    }
    if (args.empty()) {
        throw ScriptException("Empty custom call command");
    }

    // Execute
    const auto cmd = args[0];
    if (cmd == "Login" && args.size() >= 3) {
        _loginName = args[1];
        _loginPassword = args[2];

        if (_conn.IsConnected()) {
            Net_SendLogIn();
        }
        else {
            RUNTIME_ASSERT(!_conn.IsConnecting());
            _initNetReason = INIT_NET_REASON_LOGIN;
        }
    }
    else if (cmd == "Register" && args.size() >= 3) {
        _loginName = args[1];
        _loginPassword = args[2];

        if (_conn.IsConnected()) {
            Net_SendCreatePlayer();
        }
        else {
            RUNTIME_ASSERT(!_conn.IsConnecting());
            _initNetReason = INIT_NET_REASON_REG;
        }
    }
    else if (cmd == "CustomConnect") {
        _loginName = "";
        _loginPassword = "";

        RUNTIME_ASSERT(_initNetReason == INIT_NET_REASON_NONE);
        RUNTIME_ASSERT(!_conn.IsConnected());
        RUNTIME_ASSERT(!_conn.IsConnecting());

        _initNetReason = INIT_NET_REASON_CUSTOM;
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
        if (!Settings.Fullscreen) {
            if (SprMngr.EnableFullscreen()) {
                Settings.Fullscreen = true;
            }
        }
        else {
            if (SprMngr.DisableFullscreen()) {
                Settings.Fullscreen = false;

                if (_windowResolutionDiffX != 0 || _windowResolutionDiffY != 0) {
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
        // _rebuildFog = true;
    }
    else if (cmd == "SwitchShootBorders") {
        // _drawShootBorders = !_drawShootBorders;
        // _rebuildFog = true;
    }
    else if (cmd == "GetShootBorders") {
        // return _drawShootBorders ? "true" : "false";
    }
    else if (cmd == "SetShootBorders" && args.size() >= 2) {
        // auto set = (args[1] == "true");
        // if (_drawShootBorders != set) {
        //    _drawShootBorders = set;
        //    CurMap->RebuildFog();
        // }
    }
    else if (cmd == "SetMousePos" && args.size() == 4) {
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
    }
    else if (cmd == "MouseMove" && args.size() == 2) {
        /*bool enabled = _str(args[1]).toBool();
        if (enabled)
            SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
        else
            SDL_EventState(SDL_MOUSEMOTION, SDL_DISABLE);*/
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
            _curLang.LoadTexts(Resources, args[1]);
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

        if (!Settings.Fullscreen) {
            const auto [x, y] = SprMngr.GetWindowPosition();
            SprMngr.SetWindowPosition(x - diff_w / 2, y - diff_h / 2);
        }
        else {
            _windowResolutionDiffX += diff_w / 2;
            _windowResolutionDiffY += diff_h / 2;
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

        SprMngr.DrawPoints(_lmapPrepPix, RenderPrimitiveType::LineList);
    }
    else if (cmd == "SetCrittersContour" && args.size() == 2) {
        auto countour_type = static_cast<ContourType>(_str(args[1]).toInt());
        CurMap->SetCrittersContour(countour_type);
    }
    else if (cmd == "SetCritterContour" && args.size() == 3) {
        auto countour_type = static_cast<ContourType>(_str(args[1]).toInt());
        uint cr_id = _str(args[2]).toInt();
        CurMap->SetCritterContour(cr_id, countour_type);
    }
    else if (cmd == "DrawWait") {
        WaitDraw();
    }
    else if (cmd == "MoveItem" && args.size() == 5) {
        auto item_count = _str(args[1]).toUInt();
        auto item_id = _str(args[2]).toUInt();
        auto item_swap_id = _str(args[3]).toUInt();
        auto to_slot = _str(args[4]).toInt();
        auto* item = GetChosen()->GetItem(item_id);
        auto* item_swap = (item_swap_id ? GetChosen()->GetItem(item_swap_id) : nullptr);
        auto* old_item = item->CreateRefClone();
        int from_slot = item->GetCritterSlot();

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
            item->SetCritterSlot(static_cast<uchar>(to_slot));
            if (item_swap) {
                item_swap->SetCritterSlot(static_cast<uchar>(from_slot));
            }

            GetMapChosen()->Action(ACTION_MOVE_ITEM, from_slot, item, true);
            if (item_swap) {
                GetMapChosen()->Action(ACTION_MOVE_ITEM_SWAP, to_slot, item_swap, true);
            }
        }

        // Light
        CurMap->RebuildFog();

        if (is_light && (to_slot == 0 || (from_slot == 0 && to_slot != -1))) {
            CurMap->RebuildLight();
        }

        // Notify scripts about item changing
        OnItemInvChanged.Fire(item, old_item);
        old_item->Release();
    }
    else if (cmd == "SkipRoof" && args.size() == 3) {
        auto hx = _str(args[1]).toUInt();
        auto hy = _str(args[2]).toUInt();
        CurMap->SetSkipRoof(static_cast<ushort>(hx), static_cast<ushort>(hy));
    }
    else if (cmd == "RebuildLookBorders") {
        CurMap->RebuildFog();
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
    else if (cmd == "Message" && args.size() >= 2) {
        MessageBox::ShowErrorMessage("Info", args[1], "");
    }
    else if (cmd == "Exit") {
        Settings.Quit = true;
    }
    else {
        throw ScriptException("Invalid custom call command", cmd, args.size());
    }

    return "";
}

void FOClient::CritterMoveTo(CritterHexView* cr, variant<tuple<ushort, ushort, int, int>, int> pos_or_dir, uint speed)
{
    const auto prev_moving = cr->IsMoving();

    cr->ClearMove();

    bool try_move = false;
    ushort hx = 0;
    ushort hy = 0;
    short ox = 0;
    short oy = 0;
    vector<uchar> steps;
    vector<ushort> control_steps;

    if (speed > 0) {
        if (pos_or_dir.index() == 0) {
            hx = std::get<0>(std::get<0>(pos_or_dir));
            hy = std::get<1>(std::get<0>(pos_or_dir));
            ox = std::get<2>(std::get<0>(pos_or_dir));
            oy = std::get<3>(std::get<0>(pos_or_dir));

            const auto find_path = cr->GetMap()->FindPath(cr, cr->GetHexX(), cr->GetHexY(), hx, hy, -1);
            if (find_path && !find_path->Steps.empty()) {
                steps = find_path->Steps;
                control_steps = find_path->ControlSteps;
                try_move = true;
            }
        }
        else if (pos_or_dir.index() == 1) {
            const auto quad_dir = std::get<1>(pos_or_dir);

            if (quad_dir != -1) {
                hx = cr->GetHexX();
                hy = cr->GetHexY();
                if (cr->GetMap()->TraceMoveWay(hx, hy, ox, oy, steps, quad_dir)) {
                    control_steps.push_back(static_cast<ushort>(steps.size()));
                    try_move = true;
                }
            }
        }
    }

    if (try_move) {
        cr->Moving.Steps = steps;
        cr->Moving.ControlSteps = control_steps;
        cr->Moving.StartTick = GameTime.FrameTick();
        cr->Moving.Speed = static_cast<ushort>(speed);
        cr->Moving.StartHexX = cr->GetHexX();
        cr->Moving.StartHexY = cr->GetHexY();
        cr->Moving.EndHexX = hx;
        cr->Moving.EndHexY = hy;
        cr->Moving.StartOx = cr->GetHexOffsX();
        cr->Moving.StartOy = cr->GetHexOffsY();
        cr->Moving.EndOx = ox;
        cr->Moving.EndOy = oy;

        cr->Moving.WholeTime = {};
        cr->Moving.WholeDist = {};

        RUNTIME_ASSERT(cr->Moving.Speed > 0);
        const auto base_move_speed = static_cast<float>(cr->Moving.Speed);

        auto start_hx = cr->Moving.StartHexX;
        auto start_hy = cr->Moving.StartHexY;

        auto control_step_begin = 0;
        for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
            auto hx2 = start_hx;
            auto hy2 = start_hy;

            RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
            RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
            for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
                const auto move_ok = Geometry.MoveHexByDir(hx2, hy2, cr->Moving.Steps[j], cr->GetMap()->GetWidth(), cr->GetMap()->GetHeight());
                RUNTIME_ASSERT(move_ok);
            }

            auto&& [ox2, oy2] = Geometry.GetHexInterval(start_hx, start_hy, hx2, hy2);

            if (i == 0) {
                ox2 -= cr->Moving.StartOx;
                oy2 -= cr->Moving.StartOy;
            }
            if (i == cr->Moving.ControlSteps.size() - 1) {
                ox2 += cr->Moving.EndOx;
                oy2 += cr->Moving.EndOy;
            }

            const auto proj_oy = static_cast<float>(oy2) * Geometry.GetYProj();
            const auto dist = std::sqrt(static_cast<float>(ox2 * ox2) + proj_oy * proj_oy);

            cr->Moving.WholeDist += dist;
            cr->Moving.WholeTime += dist / base_move_speed * 1000.0f;

            control_step_begin = cr->Moving.ControlSteps[i];
            start_hx = hx2;
            start_hy = hy2;

            if (i == cr->Moving.ControlSteps.size() - 1) {
                RUNTIME_ASSERT(hx2 == cr->Moving.EndHexX);
                RUNTIME_ASSERT(hy2 == cr->Moving.EndHexY);
            }
        }

        if (cr->Moving.WholeTime < 0.0001f) {
            cr->Moving.WholeTime = 0.0001f;
        }
        if (cr->Moving.WholeDist < 0.0001f) {
            cr->Moving.WholeDist = 0.0001f;
        }

        RUNTIME_ASSERT(!cr->Moving.Steps.empty());
        RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
        RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
        RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

        cr->AnimateStay();
        Net_SendMove(cr);
    }

    if (prev_moving && !cr->IsMoving()) {
        cr->AnimateStay();
        Net_SendStopMove(cr);
    }
}

void FOClient::CritterLookTo(CritterHexView* cr, variant<uchar, short> dir_or_angle)
{
    if (dir_or_angle.index() == 0) {
        cr->ChangeDir(std::get<0>(dir_or_angle));
    }
    else if (dir_or_angle.index() == 1) {
        cr->ChangeLookDirAngle(std::get<1>(dir_or_angle));
    }

    Net_SendDir(cr);
}
