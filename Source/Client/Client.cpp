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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DefaultSprites.h"
#include "GenericUtils.h"
#include "Log.h"
#include "ModelSprites.h"
#include "NetCommand.h"
#include "ParticleSprites.h"
#include "StringUtils.h"
#include "Version-Include.h"

FOClient::FOClient(GlobalSettings& settings, AppWindow* window, bool mapper_mode) :
#if !FO_SINGLEPLAYER
    FOEngineBase(settings, mapper_mode ? PropertiesRelationType::BothRelative : PropertiesRelationType::ClientRelative),
#else
    FOEngineBase(settings, PropertiesRelationType::BothRelative),
#endif
    EffectMngr(Settings, Resources),
    SprMngr(Settings, window, Resources, GameTime, EffectMngr, *this),
    ResMngr(Resources, SprMngr, *this),
    SndMngr(Settings, Resources),
    Keyb(Settings, SprMngr),
    Cache(strex(Settings.ResourcesDir).combinePath("Cache.fobin")),
    ClientDeferredCalls(this),
    _conn(Settings),
    _globalMapFog(GM_MAXZONEX, GM_MAXZONEY, nullptr)
{
    STACK_TRACE_ENTRY();

    Resources.AddDataSource(Settings.EmbeddedResources);
    Resources.AddDataSource(Settings.ResourcesDir, DataSourceType::DirRoot);

    Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("EngineData"));
    Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("Core"));
    Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("Texts"));
    Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("StaticMaps"));
    Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("ClientProtos"));
    if constexpr (FO_ANGELSCRIPT_SCRIPTING) {
        Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath("ClientAngelScript"));
    }

    for (const auto& entry : Settings.ClientResourceEntries) {
        Resources.AddDataSource(strex(Settings.ResourcesDir).combinePath(entry));
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

    EffectMngr.LoadDefaultEffects();

    // Init sprite subsystems
    SprMngr.RegisterSpriteFactory(std::make_unique<DefaultSpriteFactory>(SprMngr));
    SprMngr.RegisterSpriteFactory(std::make_unique<ParticleSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, *this));

#if FO_ENABLE_3D
    auto&& model_spr_factory = std::make_unique<ModelSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, *this, *this, *this);

    if (!Preload3dFiles.empty()) {
        WriteLog("Preload 3d files...");
        for (const auto& name : Preload3dFiles) {
            model_spr_factory->GetModelMngr()->PreloadModel(name);
        }
        WriteLog("Preload 3d files complete");
    }

    SprMngr.RegisterSpriteFactory(std::move(model_spr_factory));
#endif

    SprMngr.InitializeEgg(FOEngineBase::ToHashedString("TransparentEgg.png"), AtlasType::MapSprites);

    ResMngr.IndexFiles();

    GameTime.FrameAdvance();
    _fpsTime = GameTime.FrameTime();
    Settings.MousePos = App->Input.GetMousePosition();

    if (mapper_mode) {
        return;
    }

#if !FO_SINGLEPLAYER
    if (const auto restore_info = Resources.ReadFile("RestoreInfo.fobin")) {
        extern void Client_RegisterData(FOEngineBase*, const vector<uint8>&);
        Client_RegisterData(this, restore_info.GetData());
    }
    else {
        throw EngineDataNotFoundException(LINE_STR);
    }

    ScriptSys = new ClientScriptSystem(this);
    ScriptSys->InitSubsystems();
#endif

    _curLang = LanguagePack {Settings.Language, *this};
    _curLang.LoadTexts(Resources);

    ProtoMngr.LoadFromResources();

    // Modules initialization
    extern void InitClientEngine(FOClient * client);
    InitClientEngine(this);

    ScriptSys->InitModules();

    OnStart.Fire();

    // Connection handlers
    _conn.AddConnectHandler([this](bool success) { Net_OnConnect(success); });
    _conn.AddDisconnectHandler([this] { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NETMSG_WRONG_NET_PROTO, [this] { Net_OnWrongNetProto(); });
    _conn.AddMessageHandler(NETMSG_LOGIN_SUCCESS, [this] { Net_OnLoginSuccess(); });
    _conn.AddMessageHandler(NETMSG_REGISTER_SUCCESS, [this] { Net_OnRegisterSuccess(); });
    _conn.AddMessageHandler(NETMSG_PLACE_TO_GAME_COMPLETE, [this] { Net_OnPlaceToGameComplete(); });
    _conn.AddMessageHandler(NETMSG_ADD_CRITTER, [this] { Net_OnAddCritter(); });
    _conn.AddMessageHandler(NETMSG_REMOVE_CRITTER, [this] { Net_OnRemoveCritter(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_ACTION, [this] { Net_OnCritterAction(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_MOVE_ITEM, [this] { Net_OnCritterMoveItem(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_ANIMATE, [this] { Net_OnCritterAnimate(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_SET_ANIMS, [this] { Net_OnCritterSetAnims(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_TELEPORT, [this] { Net_OnCritterTeleport(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_MOVE, [this] { Net_OnCritterMove(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_MOVE_SPEED, [this] { Net_OnCritterMoveSpeed(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_DIR, [this] { Net_OnCritterDir(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_POS, [this] { Net_OnCritterPos(); });
    _conn.AddMessageHandler(NETMSG_CRITTER_ATTACHMENTS, [this] { Net_OnCritterAttachments(); });
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
    _conn.AddMessageHandler(NETMSG_CHOSEN_ADD_ITEM, [this] { Net_OnChosenAddItem(); });
    _conn.AddMessageHandler(NETMSG_CHOSEN_REMOVE_ITEM, [this] { Net_OnChosenRemoveItem(); });
    _conn.AddMessageHandler(NETMSG_TALK_NPC, [this] { Net_OnChosenTalk(); });
    _conn.AddMessageHandler(NETMSG_TIME_SYNC, [this] { Net_OnTimeSync(); });
    _conn.AddMessageHandler(NETMSG_VIEW_MAP, [this] { Net_OnViewMap(); });
    _conn.AddMessageHandler(NETMSG_LOAD_MAP, [this] { Net_OnLoadMap(); });
    _conn.AddMessageHandler(NETMSG_GLOBAL_INFO, [this] { Net_OnGlobalInfo(); });
    _conn.AddMessageHandler(NETMSG_GLOBAL_LOCATION, [this] { Net_OnGlobalLocation(); });
    _conn.AddMessageHandler(NETMSG_GLOBAL_FOG, [this] { Net_OnGlobalFog(); });
    _conn.AddMessageHandler(NETMSG_SOME_ITEMS, [this] { Net_OnSomeItems(); });
    _conn.AddMessageHandler(NETMSG_REMOTE_CALL, [this] { Net_OnRemoteCall(); });
    _conn.AddMessageHandler(NETMSG_ADD_ITEM_ON_MAP, [this] { Net_OnAddItemOnMap(); });
    _conn.AddMessageHandler(NETMSG_REMOVE_ITEM_FROM_MAP, [this] { Net_OnRemoveItemFromMap(); });
    _conn.AddMessageHandler(NETMSG_ANIMATE_ITEM, [this] { Net_OnAnimateItem(); });
    _conn.AddMessageHandler(NETMSG_EFFECT, [this] { Net_OnEffect(); });
    _conn.AddMessageHandler(NETMSG_FLY_EFFECT, [this] { Net_OnFlyEffect(); });
    _conn.AddMessageHandler(NETMSG_PLAY_SOUND, [this] { Net_OnPlaySound(); });
    _conn.AddMessageHandler(NETMSG_UPDATE_FILES_LIST, [this] { Net_OnUpdateFilesResponse(); });
    _conn.AddMessageHandler(NETMSG_ADD_CUSTOM_ENTITY, [this] { Net_OnAddCustomEntity(); });
    _conn.AddMessageHandler(NETMSG_REMOVE_CUSTOM_ENTITY, [this] { Net_OnRemoveCustomEntity(); });

    // Properties that sending to clients
    {
        const auto set_send_callbacks = [](const auto* registrator, const PropertyPostSetCallback& callback) {
            for (size_t i = 0; i < registrator->GetPropertiesCount(); i++) {
                const auto* prop = registrator->GetPropertyByIndex(static_cast<int>(i));

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

        set_send_callbacks(GetPropertyRegistrator(GameProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendGlobalValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(PlayerProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendPlayerValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendItemValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendCritterValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendMapValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(LocationProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendLocationValue(entity, prop); });
    }

    // Properties with custom behaviours
    {
        const auto set_callback = [](const auto* registrator, int prop_index, PropertyPostSetCallback callback) {
            const auto* prop = registrator->GetPropertyByIndex(prop_index);
            prop->AddPostSetter(std::move(callback));
        };

        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LookDistance_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterLookDistance(entity, prop); });
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::ModelName_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterModelName(entity, prop); });
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::ContourColor_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterContourColor(entity, prop); });
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::HideSprite_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterHideSprite(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::Colorize_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemFlags(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::BadItem_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemFlags(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::ShootThru_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemFlags(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightThru_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemFlags(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::NoBlock_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemFlags(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightSource_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemSomeLight(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightIntensity_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemSomeLight(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightDistance_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemSomeLight(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightFlags_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemSomeLight(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightColor_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemSomeLight(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::PicMap_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemPicMap(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::Offset_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemOffsetCoords(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::Opened_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemOpened(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::HideSprite_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemHideSprite(entity, prop); });
    }

    _eventUnsubscriber += window->OnScreenSizeChanged += [this] { OnScreenSizeChanged.Fire(); };

    ScreenFadeOut();

    // Auto login
    TryAutoLogin();
}

FOClient::~FOClient()
{
    STACK_TRACE_ENTRY();

    safe_call([] { App->Render.SetRenderTarget(nullptr); });
    safe_call([this] { _conn.Disconnect(); });

    if (_chosen != nullptr) {
        _chosen->Release();
        _chosen = nullptr;
    }

    if (CurMap != nullptr) {
        safe_call([this] { CurMap->DestroySelf(); });
        CurMap = nullptr;
    }

    if (_curPlayer != nullptr) {
        safe_call([this] { _curPlayer->DestroySelf(); });
        _curPlayer = nullptr;
    }

    if (_curLocation != nullptr) {
        safe_call([this] { _curLocation->DestroySelf(); });
        _curLocation = nullptr;
    }

    safe_call([this] { DestroyInnerEntities(); });

    delete ScriptSys;
    ScriptSys = nullptr;
}

auto FOClient::ResolveCritterAnimation(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint& pass, uint& flags, int& ox, int& oy, string& anim_name) -> bool
{
    STACK_TRACE_ENTRY();

    return OnCritterAnimation.Fire(model_name, state_anim, action_anim, pass, flags, ox, oy, anim_name);
}

auto FOClient::ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool
{
    STACK_TRACE_ENTRY();

    return OnCritterAnimationSubstitute.Fire(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim);
}

auto FOClient::ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint& f_state_anim, uint& f_action_anim, uint& f_state_anim_ex, uint& f_action_anim_ex, uint& flags) -> bool
{
    STACK_TRACE_ENTRY();

    return OnCritterAnimationFallout.Fire(model_name, state_anim, action_anim, f_state_anim, f_action_anim, f_state_anim_ex, f_action_anim_ex, flags);
}

auto FOClient::IsConnecting() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _conn.IsConnecting();
}

auto FOClient::IsConnected() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _conn.IsConnected();
}

auto FOClient::GetChosen() noexcept -> CritterView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_chosen != nullptr && _chosen->IsDestroyed()) {
        _chosen->Release();
        _chosen = nullptr;
    }

    return _chosen;
}

auto FOClient::GetMapChosen() noexcept -> CritterHexView*
{
    STACK_TRACE_ENTRY();

    return dynamic_cast<CritterHexView*>(GetChosen());
}

auto FOClient::GetGlobalMapCritter(ident_t cr_id) -> CritterView*
{
    STACK_TRACE_ENTRY();

    const auto it = std::find_if(_globalMapCritters.begin(), _globalMapCritters.end(), [cr_id](const auto* cr) { return cr->GetId() == cr_id; });
    return it != _globalMapCritters.end() ? *it : nullptr;
}

void FOClient::TryAutoLogin()
{
    STACK_TRACE_ENTRY();

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

    const auto auto_login_args = strex(auto_login).split(' ');
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
    STACK_TRACE_ENTRY();

    const auto time_changed = GameTime.FrameAdvance();

    // FPS counter
    if (GameTime.FrameTime() - _fpsTime >= std::chrono::milliseconds {1000}) {
        Settings.FPS = _fpsCounter;
        _fpsCounter = 0;
        _fpsTime = GameTime.FrameTime();
    }
    else {
        _fpsCounter++;
    }

#if FO_TRACY
    TracyPlot("Client FPS", static_cast<int64>(Settings.FPS));
#endif

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

    // Game time
    if (time_changed) {
        const auto st = GameTime.ServerToDateTime(GameTime.GetServerTime());

        SetYear(st.Year);
        SetMonth(st.Month);
        SetDay(st.Day);
        SetHour(st.Hour);
        SetMinute(st.Minute);
        SetSecond(st.Second);
    }

    // Script subsystems update
    ScriptSys->Process();

    ClientDeferredCalls.ProcessDeferredCalls();

#if !FO_SINGLEPLAYER
    // Script loop
    OnLoop.Fire();
#endif

    if (CurMap != nullptr) {
        CurMap->Process();
    }

    App->MainWindow.GrabInput(CurMap != nullptr && CurMap->IsScrollEnabled());

    // Render
    EffectMngr.UpdateEffects(GameTime);

    {
        SprMngr.BeginScene({0, 0, 0});
        auto end_scene = ScopeCallback([this]() noexcept { safe_call([this] { SprMngr.EndScene(); }); });

        // Quake effect
        ProcessScreenEffectQuake();

        // Make dirty offscreen surfaces
        if (!PreDirtyOffscreenSurfaces.empty()) {
            DirtyOffscreenSurfaces.insert(DirtyOffscreenSurfaces.end(), PreDirtyOffscreenSurfaces.begin(), PreDirtyOffscreenSurfaces.end());
            PreDirtyOffscreenSurfaces.clear();
        }

        CanDrawInScripts = true;
        OnRenderIface.Fire();
        CanDrawInScripts = false;

        ProcessVideo();
        ProcessScreenEffectFading();
    }
}

void FOClient::ScreenFade(time_duration time, ucolor from_color, ucolor to_color, bool push_back)
{
    STACK_TRACE_ENTRY();

    if (!push_back || _screenFadingEffects.empty()) {
        _screenFadingEffects.push_back({GameTime.FrameTime(), time, from_color, to_color});
    }
    else {
        time_point last_tick;
        for (const auto& e : _screenFadingEffects) {
            if (e.BeginTime + e.Duration > last_tick) {
                last_tick = e.BeginTime + e.Duration;
            }
        }
        _screenFadingEffects.push_back({last_tick, time, from_color, to_color});
    }
}

void FOClient::ProcessScreenEffectFading()
{
    STACK_TRACE_ENTRY();

    SprMngr.Flush();

    vector<PrimitivePoint> full_screen_quad;
    SprMngr.PrepareSquare(full_screen_quad, IRect(0, 0, Settings.ScreenWidth, Settings.ScreenHeight), ucolor::clear);

    for (auto it = _screenFadingEffects.begin(); it != _screenFadingEffects.end();) {
        auto& screen_effect = *it;

        if (GameTime.FrameTime() >= screen_effect.BeginTime + screen_effect.Duration) {
            it = _screenFadingEffects.erase(it);
            continue;
        }

        if (GameTime.FrameTime() >= screen_effect.BeginTime) {
            const auto proc = GenericUtils::Percent(time_duration_to_ms<uint>(screen_effect.Duration), time_duration_to_ms<uint>(GameTime.FrameTime() - screen_effect.BeginTime)) + 1;
            int res[4];

            for (auto i = 0; i < 4; i++) {
                const int sc = (reinterpret_cast<uint8*>(&screen_effect.StartColor))[i];
                const int ec = (reinterpret_cast<uint8*>(&screen_effect.EndColor))[i];
                const auto dc = ec - sc;
                res[i] = sc + dc * static_cast<int>(proc) / 100;
            }

            const auto color = ucolor {static_cast<uint8>(res[0]), static_cast<uint8>(res[1]), static_cast<uint8>(res[2]), static_cast<uint8>(res[3])};
            for (auto i = 0; i < 6; i++) {
                full_screen_quad[i].PointColor = color;
            }

            SprMngr.DrawPoints(full_screen_quad, RenderPrimitiveType::TriangleList);
        }

        ++it;
    }
}

void FOClient::ScreenQuake(int noise, time_duration time)
{
    STACK_TRACE_ENTRY();

    Settings.ScreenOffset.x -= iround(_quakeScreenOffsX);
    Settings.ScreenOffset.y -= iround(_quakeScreenOffsY);

    _quakeScreenOffsX = static_cast<float>(GenericUtils::Random(0, 1) != 0 ? noise : -noise);
    _quakeScreenOffsY = static_cast<float>(GenericUtils::Random(0, 1) != 0 ? noise : -noise);
    _quakeScreenOffsStep = std::fabs(_quakeScreenOffsX) / (time_duration_to_ms<float>(time) / 30.0f);

    Settings.ScreenOffset.x += iround(_quakeScreenOffsX);
    Settings.ScreenOffset.y += iround(_quakeScreenOffsY);

    _quakeScreenOffsNextTime = GameTime.GameplayTime() + std::chrono::milliseconds {30};
}

void FOClient::ProcessScreenEffectQuake()
{
    STACK_TRACE_ENTRY();

    if ((_quakeScreenOffsX != 0.0f || _quakeScreenOffsY != 0.0f) && GameTime.GameplayTime() >= _quakeScreenOffsNextTime) {
        Settings.ScreenOffset.x -= iround(_quakeScreenOffsX);
        Settings.ScreenOffset.y -= iround(_quakeScreenOffsY);

        if (_quakeScreenOffsX < 0.0f) {
            _quakeScreenOffsX += _quakeScreenOffsStep;
        }
        else if (_quakeScreenOffsX > 0.0f) {
            _quakeScreenOffsX -= _quakeScreenOffsStep;
        }

        if (_quakeScreenOffsY < 0.0f) {
            _quakeScreenOffsY += _quakeScreenOffsStep;
        }
        else if (_quakeScreenOffsY > 0.0f) {
            _quakeScreenOffsY -= _quakeScreenOffsStep;
        }

        _quakeScreenOffsX = -_quakeScreenOffsX;
        _quakeScreenOffsY = -_quakeScreenOffsY;

        Settings.ScreenOffset.x += iround(_quakeScreenOffsX);
        Settings.ScreenOffset.y += iround(_quakeScreenOffsY);

        _quakeScreenOffsNextTime = GameTime.GameplayTime() + std::chrono::milliseconds {30};
    }
}

void FOClient::ProcessInputEvents()
{
    STACK_TRACE_ENTRY();

    if (SprMngr.IsWindowFocused()) {
        InputEvent ev;
        while (App->Input.PollEvent(ev)) {
            ProcessInputEvent(ev);
        }
    }
    else {
        Settings.MousePos = App->Input.GetMousePosition();

        Keyb.Lost();
        OnInputLost.Fire();
    }
}

void FOClient::ProcessInputEvent(const InputEvent& ev)
{
    STACK_TRACE_ENTRY();

    if (_video && !_video->IsStopped() && _videoCanInterrupt) {
        if (ev.Type == InputEvent::EventType::KeyDownEvent || ev.Type == InputEvent::EventType::MouseDownEvent) {
            _video->Stop();
        }
    }

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
        Settings.MousePos = {ev.MouseMove.MouseX, ev.MouseMove.MouseY};

        OnMouseMove.Fire({ev.MouseMove.DeltaX, ev.MouseMove.DeltaY});
    }
    else if (ev.Type == InputEvent::EventType::MouseDownEvent) {
        OnMouseDown.Fire(ev.MouseDown.Button);
    }
    else if (ev.Type == InputEvent::EventType::MouseUpEvent) {
        OnMouseUp.Fire(ev.MouseUp.Button);
    }
    else if (ev.Type == InputEvent::EventType::MouseWheelEvent) {
        if (ev.MouseWheel.Delta > 0) {
            OnMouseDown.Fire(MouseButton::WheelUp);
            OnMouseUp.Fire(MouseButton::WheelUp);
        }
        else {
            OnMouseDown.Fire(MouseButton::WheelDown);
            OnMouseUp.Fire(MouseButton::WheelDown);
        }
    }
}

void FOClient::Net_OnConnect(bool success)
{
    STACK_TRACE_ENTRY();

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
            UNREACHABLE_PLACE();
        }

        RUNTIME_ASSERT(!_curPlayer);
        _curPlayer = new PlayerView(this, ident_t {});
    }
    else {
        ShowMainScreen(SCREEN_LOGIN, {});
        AddMessage(FOMB_GAME, _curLang.GetTextPack(TextPackName::Game).GetStr(STR_NET_CONN_FAIL));
    }
}

void FOClient::Net_OnDisconnect()
{
    STACK_TRACE_ENTRY();

    if (CurMap != nullptr) {
        CurMap->DestroySelf();
        CurMap = nullptr;

        CleanupSpriteCache();
    }

    if (_curPlayer != nullptr) {
        _curPlayer->DestroySelf();
        _curPlayer = nullptr;
    }

    DestroyInnerEntities();

    TryAutoLogin();
}

void FOClient::Net_SendLogIn()
{
    STACK_TRACE_ENTRY();

    WriteLog("Player login");

    _conn.OutBuf.StartMsg(NETMSG_LOGIN);
    _conn.OutBuf.Write(_loginName);
    _conn.OutBuf.Write(_loginPassword);
    _conn.OutBuf.EndMsg();

    AddMessage(FOMB_GAME, _curLang.GetTextPack(TextPackName::Game).GetStr(STR_NET_CONN_SUCCESS));
}

void FOClient::Net_SendCreatePlayer()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    WriteLog("Player registration");

    _conn.OutBuf.StartMsg(NETMSG_REGISTER);
    _conn.OutBuf.Write(_loginName);
    _conn.OutBuf.Write(_loginPassword);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendText(string_view send_str, uint8 how_say)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _conn.OutBuf.StartMsg(NETMSG_SEND_TEXT);
    _conn.OutBuf.Write(how_say);
    _conn.OutBuf.Write(send_str);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendDir(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _conn.OutBuf.StartMsg(NETMSG_DIR);
    _conn.OutBuf.Write(CurMap->GetId());
    _conn.OutBuf.Write(cr->GetId());
    _conn.OutBuf.Write(cr->GetDirAngle());
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendMove(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(!cr->Moving.Steps.empty());

    if (cr->Moving.Steps.size() > 500) {
        cr->ClearMove();
        return;
    }

    _conn.OutBuf.StartMsg(NETMSG_SEND_MOVE);
    _conn.OutBuf.Write(CurMap->GetId());
    _conn.OutBuf.Write(cr->GetId());
    _conn.OutBuf.Write(cr->Moving.Speed);
    _conn.OutBuf.Write(cr->Moving.StartHex);
    _conn.OutBuf.Write(static_cast<uint16>(cr->Moving.Steps.size()));
    for (const auto step : cr->Moving.Steps) {
        _conn.OutBuf.Write(step);
    }
    _conn.OutBuf.Write(static_cast<uint16>(cr->Moving.ControlSteps.size()));
    for (const auto control_step : cr->Moving.ControlSteps) {
        _conn.OutBuf.Write(control_step);
    }
    _conn.OutBuf.Write(cr->Moving.EndHexOffset);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendStopMove(CritterHexView* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _conn.OutBuf.StartMsg(NETMSG_SEND_STOP_MOVE);
    _conn.OutBuf.Write(CurMap->GetId());
    _conn.OutBuf.Write(cr->GetId());
    _conn.OutBuf.Write(cr->GetHex());
    _conn.OutBuf.Write(cr->GetHexOffset());
    _conn.OutBuf.Write(cr->GetDirAngle());
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendProperty(NetProperty type, const Property* prop, Entity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();
    RUNTIME_ASSERT(entity);

    const auto& props = entity->GetProperties();

    props.ValidateForRawData(prop);

    const auto prop_raw_data = props.GetRawData(prop);
    const auto is_pod = prop->IsPlainData();

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

    if (is_pod) {
        _conn.OutBuf.StartMsg(NETMSG_SEND_POD_PROPERTY(prop_raw_data.size(), additional_args));
    }
    else {
        _conn.OutBuf.StartMsg(NETMSG_SEND_COMPLEX_PROPERTY);
    }

    _conn.OutBuf.Write(static_cast<char>(type));

    switch (type) {
    case NetProperty::CritterItem: {
        auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(dynamic_cast<ItemView*>(client_entity)->GetCritterId());
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    case NetProperty::Critter: {
        const auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    case NetProperty::MapItem: {
        const auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    case NetProperty::ChosenItem: {
        const auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    default:
        break;
    }

    _conn.OutBuf.Write(prop->GetRegIndex());
    _conn.OutBuf.Push(prop_raw_data);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendTalk(ident_t cr_id, hstring dlg_pack_id, uint8 answer)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _conn.OutBuf.StartMsg(NETMSG_SEND_TALK_NPC);
    _conn.OutBuf.Write(cr_id);
    _conn.OutBuf.Write(dlg_pack_id);
    _conn.OutBuf.Write(answer);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_OnUpdateFilesResponse()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto outdated = _conn.InBuf.Read<bool>();
    const auto data_size = _conn.InBuf.Read<uint>();

    vector<uint8> data;
    data.resize(data_size);
    _conn.InBuf.Pop(data.data(), data_size);

    if (!outdated) {
        _conn.InBuf.ReadPropsData(_globalsPropertiesData);
    }

    if (outdated) {
        throw ResourcesOutdatedException("Binary outdated");
    }

    if (!data.empty()) {
        FileSystem resources;

        resources.AddDataSource(Settings.ResourcesDir, DataSourceType::DirRoot);

        auto reader = DataReader(data);

        for (uint file_index = 0;; file_index++) {
            const auto name_len = reader.Read<int16>();
            if (name_len == -1) {
                break;
            }

            RUNTIME_ASSERT(name_len > 0);
            const auto fname = string(reader.ReadPtr<char>(name_len), name_len);
            const auto size = reader.Read<uint>();
            const auto hash = reader.Read<uint>();

            UNUSED_VARIABLE(hash);

            // Check hash
            if (auto file = resources.ReadFileHeader(fname)) {
                if (file.GetSize() == size) {
                    continue;
                }
            }

            throw ResourcesOutdatedException("Resource pack outdated", fname);
        }

        reader.VerifyEnd();
    }

    RestoreData(_globalsPropertiesData);
}

void FOClient::Net_OnWrongNetProto()
{
    STACK_TRACE_ENTRY();

    AddMessage(FOMB_GAME, _curLang.GetTextPack(TextPackName::Game).GetStr(STR_CLIENT_OUTDATED));
}

void FOClient::Net_OnRegisterSuccess()
{
    STACK_TRACE_ENTRY();

    WriteLog("Registration success");

    OnRegistrationSuccess.Fire();
}

void FOClient::Net_OnLoginSuccess()
{
    STACK_TRACE_ENTRY();

    WriteLog("Authentication success");

    AddMessage(FOMB_GAME, _curLang.GetTextPack(TextPackName::Game).GetStr(STR_NET_LOGINOK));

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto encrypt_key = _conn.InBuf.Read<uint>();
    const auto player_id = _conn.InBuf.Read<ident_t>();
    _conn.InBuf.ReadPropsData(_globalsPropertiesData);
    _conn.InBuf.ReadPropsData(_playerPropertiesData);

    RestoreData(_globalsPropertiesData);

    RUNTIME_ASSERT(_curPlayer);
    RUNTIME_ASSERT(!_curPlayer->GetId());
    _curPlayer->SetId(player_id, true);
    _curPlayer->RestoreData(_playerPropertiesData);

    RUNTIME_ASSERT(!HasInnerEntities());
    ReceiveCustomEntities(this);

    _conn.InBuf.SetEncryptKey(encrypt_key);

    OnLoginSuccess.Fire();
}

void FOClient::Net_OnAddCritter()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto pid = _conn.InBuf.Read<hstring>(*this);
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto hex_offset = _conn.InBuf.Read<ipos16>();
    const auto dir_angle = _conn.InBuf.Read<int16>();
    const auto cond = _conn.InBuf.Read<CritterCondition>();
    const auto alive_state_anim = _conn.InBuf.Read<CritterStateAnim>();
    const auto knockout_state_anim = _conn.InBuf.Read<CritterStateAnim>();
    const auto dead_state_anim = _conn.InBuf.Read<CritterStateAnim>();
    const auto alive_action_anim = _conn.InBuf.Read<CritterActionAnim>();
    const auto knockout_action_anim = _conn.InBuf.Read<CritterActionAnim>();
    const auto dead_action_anim = _conn.InBuf.Read<CritterActionAnim>();
    const auto is_controlled_by_player = _conn.InBuf.Read<bool>();
    const auto is_player_offline = _conn.InBuf.Read<bool>();
    const auto is_chosen = _conn.InBuf.Read<bool>();
    _conn.InBuf.ReadPropsData(_tempPropertiesData);

    CritterView* cr;
    CritterHexView* hex_cr;

    if (CurMap != nullptr) {
        hex_cr = CurMap->AddReceivedCritter(cr_id, pid, hex, dir_angle, _tempPropertiesData);
        RUNTIME_ASSERT(hex_cr);

        if (_screenModeMain != SCREEN_WAIT) {
            hex_cr->FadeUp();
        }

        cr = hex_cr;
    }
    else {
        const auto* proto = ProtoMngr.GetProtoCritter(pid);
        const auto it = std::find_if(_globalMapCritters.begin(), _globalMapCritters.end(), [cr_id](const auto* cr2) { return cr2->GetId() == cr_id; });

        if (it != _globalMapCritters.end()) {
            BreakIntoDebugger();
            (*it)->MarkAsDestroyed();
            (*it)->Release();
            _globalMapCritters.erase(it);
        }

        cr = new CritterView(this, cr_id, proto);
        cr->RestoreData(_tempPropertiesData);
        _globalMapCritters.emplace_back(cr);

        hex_cr = nullptr;
    }

    ReceiveCustomEntities(cr);

    cr->SetHexOffset(hex_offset);
    cr->SetCondition(cond);
    cr->SetAliveStateAnim(alive_state_anim);
    cr->SetKnockoutStateAnim(knockout_state_anim);
    cr->SetDeadStateAnim(dead_state_anim);
    cr->SetAliveActionAnim(alive_action_anim);
    cr->SetKnockoutActionAnim(knockout_action_anim);
    cr->SetDeadActionAnim(dead_action_anim);
    cr->SetControlledByPlayer(is_controlled_by_player);
    cr->SetIsChosen(is_chosen);
    cr->SetIsPlayerOffline(is_player_offline);

    // Initial items
    const auto items_count = _conn.InBuf.Read<uint>();

    for (uint i = 0; i < items_count; i++) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(*this);
        const auto item_slot = _conn.InBuf.Read<CritterItemSlot>();
        _conn.InBuf.ReadPropsData(_tempPropertiesData);

        const auto* proto = ProtoMngr.GetProtoItem(item_pid);
        auto* item = cr->AddReceivedInvItem(item_id, proto, item_slot, _tempPropertiesData);

        ReceiveCustomEntities(item);
    }

    // Initial attachment
    const auto is_attached = _conn.InBuf.Read<bool>();

    const auto attached_critters_count = _conn.InBuf.Read<uint16>();
    vector<ident_t> attached_critters;
    attached_critters.resize(attached_critters_count);
    for (uint16 i = 0; i < attached_critters_count; i++) {
        attached_critters[i] = _conn.InBuf.Read<ident_t>();
    }

    cr->SetIsAttached(is_attached);
    cr->AttachedCritters = std::move(attached_critters);

    if (CurMap != nullptr) {
        if (is_attached) {
            for (auto* map_cr : CurMap->GetCritters()) {
                if (!map_cr->AttachedCritters.empty() && std::find(map_cr->AttachedCritters.begin(), map_cr->AttachedCritters.end(), cr_id) != map_cr->AttachedCritters.end()) {
                    map_cr->MoveAttachedCritters();
                    break;
                }
            }
        }

        if (!hex_cr->AttachedCritters.empty()) {
            hex_cr->MoveAttachedCritters();
        }
    }

    if (hex_offset != ipos16 {} && hex_cr != nullptr) {
        hex_cr->RefreshOffs();
    }

    if (is_chosen) {
        _chosen = cr;
        _chosen->AddRef();
    }

    OnCritterIn.Fire(cr);

    if (hex_cr != nullptr) {
#if FO_ENABLE_3D
        if (hex_cr->IsModel()) {
            hex_cr->GetModel()->PrewarmParticles();
            hex_cr->GetModel()->StartMeshGeneration();
        }
#endif

        if (items_count != 0) {
            CurMap->UpdateCritterLightSource(hex_cr);
        }

        if (is_chosen) {
            CurMap->RebuildFog();
        }

        if (!hex_cr->IsAnim()) {
            hex_cr->AnimateStay();
        }
    }
}

void FOClient::Net_OnRemoveCritter()
{
    STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();

    if (CurMap != nullptr) {
        auto* cr = CurMap->GetCritter(cr_id);
        if (cr == nullptr) {
            BreakIntoDebugger();
            return;
        }

        cr->Finish();

        OnCritterOut.Fire(cr);

        if (cr == _chosen) {
            _chosen->Release();
            _chosen = nullptr;
        }
    }
    else {
        const auto it = std::find_if(_globalMapCritters.begin(), _globalMapCritters.end(), [cr_id](const auto* cr) { return cr->GetId() == cr_id; });
        if (it == _globalMapCritters.end()) {
            BreakIntoDebugger();
            return;
        }

        auto* cr = *it;

        OnCritterOut.Fire(cr);
        _globalMapCritters.erase(it);

        if (cr == _chosen) {
            _chosen->Release();
            _chosen = nullptr;
        }

        cr->DestroySelf();
    }
}

void FOClient::Net_OnText()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto how_say = _conn.InBuf.Read<uint8>();
    const auto text = _conn.InBuf.Read<string>();
    const auto unsafe_text = _conn.InBuf.Read<bool>();

    if (how_say == SAY_FLASH_WINDOW) {
        FlashGameWindow();
        return;
    }

    string str = text;

    if (unsafe_text) {
        Keyb.RemoveInvalidChars(str, KIF_NO_SPEC_SYMBOLS);
    }

    if (!str.empty()) {
        OnInMessage.Fire(string(str), how_say, cr_id);
    }
}

void FOClient::Net_OnTextMsg(bool with_lexems)
{
    STACK_TRACE_ENTRY();

    if (with_lexems) {
        [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    }

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto how_say = _conn.InBuf.Read<uint8>();
    const auto text_pack = _conn.InBuf.Read<TextPackName>();
    const auto str_num = _conn.InBuf.Read<TextPackKey>();
    const auto lexems = with_lexems ? _conn.InBuf.Read<string>() : string();

    if (how_say == SAY_FLASH_WINDOW) {
        FlashGameWindow();
        return;
    }

    if (const auto& msg = _curLang.GetTextPack(text_pack); msg.GetStrCount(str_num) != 0) {
        string str = copy(msg.GetStr(str_num));
        FormatTags(str, GetChosen(), CurMap != nullptr ? CurMap->GetCritter(cr_id) : nullptr, lexems);

        if (!str.empty()) {
            OnInMessage.Fire(str, how_say, cr_id);
        }
    }
}

void FOClient::OnMapText(string_view str, mpos hex, ucolor color)
{
    STACK_TRACE_ENTRY();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    uint show_time = 0;
    auto processed_str = strex(str).str();

    OnMapMessage.Fire(processed_str, hex, color, show_time);

    CurMap->AddMapText(processed_str, hex, color, std::chrono::milliseconds {show_time}, false, {0, 0});

    FlashGameWindow();
}

void FOClient::Net_OnMapText()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto color = _conn.InBuf.Read<ucolor>();
    const auto text = _conn.InBuf.Read<string>();
    const auto unsafe_text = _conn.InBuf.Read<bool>();

    string str = text;

    if (unsafe_text) {
        Keyb.RemoveInvalidChars(str, KIF_NO_SPEC_SYMBOLS);
    }

    OnMapText(str, hex, color);
}

void FOClient::Net_OnMapTextMsg()
{
    STACK_TRACE_ENTRY();

    const auto hex = _conn.InBuf.Read<mpos>();
    const auto color = _conn.InBuf.Read<ucolor>();
    const auto text_pack = _conn.InBuf.Read<TextPackName>();
    const auto str_num = _conn.InBuf.Read<TextPackKey>();

    auto str = copy(_curLang.GetTextPack(text_pack).GetStr(str_num));

    FormatTags(str, GetChosen(), nullptr, "");
    OnMapText(str, hex, color);
}

void FOClient::Net_OnMapTextMsgLex()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto color = _conn.InBuf.Read<ucolor>();
    const auto text_pack = _conn.InBuf.Read<TextPackName>();
    const auto str_num = _conn.InBuf.Read<TextPackKey>();
    const auto lexems = _conn.InBuf.Read<string>();

    auto str = copy(_curLang.GetTextPack(text_pack).GetStr(str_num));

    FormatTags(str, GetChosen(), nullptr, lexems);
    OnMapText(str, hex, color);
}

void FOClient::Net_OnCritterDir()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto dir_angle = _conn.InBuf.Read<int16>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);
    if (cr != nullptr) {
        cr->ChangeLookDirAngle(dir_angle);
    }
}

void FOClient::Net_OnCritterMove()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto whole_time = _conn.InBuf.Read<uint>();
    const auto offset_time = _conn.InBuf.Read<uint>();
    const auto speed = _conn.InBuf.Read<uint16>();
    const auto start_hex = _conn.InBuf.Read<mpos>();

    const auto steps_count = _conn.InBuf.Read<uint16>();
    vector<uint8> steps;
    steps.resize(steps_count);
    for (auto i = 0; i < steps_count; i++) {
        steps[i] = _conn.InBuf.Read<uint8>();
    }

    const auto control_steps_count = _conn.InBuf.Read<uint16>();
    vector<uint16> control_steps;
    control_steps.resize(control_steps_count);
    for (auto i = 0; i < control_steps_count; i++) {
        control_steps[i] = _conn.InBuf.Read<uint16>();
    }

    const auto end_hex_offset = _conn.InBuf.Read<ipos16>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);
    if (cr == nullptr) {
        return;
    }

    cr->ClearMove();

    cr->Moving.Speed = speed;
    cr->Moving.StartTime = GameTime.FrameTime();
    cr->Moving.OffsetTime = std::chrono::milliseconds {offset_time};
    cr->Moving.WholeTime = static_cast<float>(whole_time);
    cr->Moving.Steps = steps;
    cr->Moving.ControlSteps = control_steps;
    cr->Moving.StartHex = start_hex;
    cr->Moving.StartHexOffset = cr->GetHexOffset();
    cr->Moving.EndHexOffset = end_hex_offset;

    if (offset_time == 0 && start_hex != cr->GetHex()) {
        const auto cr_offset = Geometry.GetHexInterval(start_hex, cr->GetHex());
        cr->Moving.StartHexOffset = {static_cast<int16>(cr->Moving.StartHexOffset.x + cr_offset.x), static_cast<int16>(cr->Moving.StartHexOffset.y + cr_offset.y)};
    }

    cr->Moving.WholeDist = 0.0f;

    mpos next_start_hex = start_hex;
    uint16 control_step_begin = 0;

    for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
        auto hex = next_start_hex;

        RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(hex, cr->Moving.Steps[j], CurMap->GetSize());
            RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = Geometry.GetHexInterval(next_start_hex, hex);

        if (i == 0) {
            ox -= cr->Moving.StartHexOffset.x;
            oy -= cr->Moving.StartHexOffset.y;
        }
        if (i == cr->Moving.ControlSteps.size() - 1) {
            ox += cr->Moving.EndHexOffset.x;
            oy += cr->Moving.EndHexOffset.y;
        }

        const auto proj_oy = static_cast<float>(oy) * Geometry.GetYProj();
        const auto dist = std::sqrt(static_cast<float>(ox * ox) + proj_oy * proj_oy);

        cr->Moving.WholeDist += dist;

        control_step_begin = cr->Moving.ControlSteps[i];
        next_start_hex = hex;

        cr->Moving.EndHex = hex;
    }

    cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);
    cr->Moving.WholeDist = std::max(cr->Moving.WholeDist, 0.0001f);

    RUNTIME_ASSERT(!cr->Moving.Steps.empty());
    RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
    RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
    RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

    cr->AnimateStay();
}

void FOClient::Net_OnCritterMoveSpeed()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto speed = _conn.InBuf.Read<uint16>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);

    if (cr == nullptr) {
        return;
    }
    if (!cr->IsMoving()) {
        return;
    }
    if (speed == cr->Moving.Speed) {
        return;
    }

    const auto diff = static_cast<float>(speed) / static_cast<float>(cr->Moving.Speed);
    const auto elapsed_time = time_duration_to_ms<float>(GameTime.FrameTime() - cr->Moving.StartTime + cr->Moving.OffsetTime);

    cr->Moving.WholeTime /= diff;
    cr->Moving.StartTime = GameTime.FrameTime();
    cr->Moving.OffsetTime = std::chrono::milliseconds {iround(elapsed_time / diff)};
    cr->Moving.Speed = speed;
    cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);
}

void FOClient::Net_OnCritterAction()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto action = _conn.InBuf.Read<CritterAction>();
    const auto action_ext = _conn.InBuf.Read<int>();
    const auto is_context_item = _conn.InBuf.Read<bool>();

    ItemView* context_item = nullptr;

    auto context_item_release = ScopeCallback([&context_item]() noexcept {
        if (context_item != nullptr) {
            context_item->Release();
        }
    });

    if (is_context_item) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(*this);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);

        const auto* proto = ProtoMngr.GetProtoItem(item_pid);
        context_item = new ItemView(this, item_id, proto);
        context_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(context_item);
    }

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);

    if (cr == nullptr) {
        BreakIntoDebugger();
        return;
    }

    cr->Action(action, action_ext, context_item, false);
}

void FOClient::Net_OnCritterMoveItem()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto action = _conn.InBuf.Read<CritterAction>();
    const auto prev_slot = _conn.InBuf.Read<CritterItemSlot>();
    const auto cur_slot = _conn.InBuf.Read<CritterItemSlot>();
    const auto is_moved_item = _conn.InBuf.Read<bool>();

    ItemView* moved_item = nullptr;

    auto moved_item_release = ScopeCallback([&moved_item]() noexcept {
        if (moved_item != nullptr) {
            moved_item->Release();
        }
    });

    if (is_moved_item) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(*this);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);

        const auto* proto = ProtoMngr.GetProtoItem(item_pid);
        moved_item = new ItemView(this, item_id, proto);
        moved_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(moved_item);
    }

    CritterView* cr;

    if (CurMap != nullptr) {
        cr = CurMap->GetCritter(cr_id);
    }
    else {
        cr = GetGlobalMapCritter(cr_id);
    }

    if (cr == nullptr) {
        BreakIntoDebugger();

        // Skip rest data
        const auto items_count = _conn.InBuf.Read<uint>();

        for (uint i = 0; i < items_count; i++) {
            (void)_conn.InBuf.Read<ident_t>();
            (void)_conn.InBuf.Read<hstring>(*this);
            (void)_conn.InBuf.Read<CritterItemSlot>();
            _conn.InBuf.ReadPropsData(_tempPropertiesData);
            ReceiveCustomEntities(nullptr);
        }

        return;
    }

    // Todo: rework critters inventory updating
    const auto items_count = _conn.InBuf.Read<uint>();

    if (items_count != 0) {
        RUNTIME_ASSERT(!cr->GetIsChosen());

        cr->DeleteAllInvItems();

        for (uint i = 0; i < items_count; i++) {
            const auto item_id = _conn.InBuf.Read<ident_t>();
            const auto item_pid = _conn.InBuf.Read<hstring>(*this);
            const auto item_slot = _conn.InBuf.Read<CritterItemSlot>();
            _conn.InBuf.ReadPropsData(_tempPropertiesData);

            const auto* proto = ProtoMngr.GetProtoItem(item_pid);
            auto* item = cr->AddReceivedInvItem(item_id, proto, item_slot, _tempPropertiesData);

            ReceiveCustomEntities(item);
        }
    }

    if (auto* hex_cr = dynamic_cast<CritterHexView*>(cr); hex_cr != nullptr) {
        hex_cr->Action(action, static_cast<int>(prev_slot), moved_item, false);
    }

    if (moved_item != nullptr && cur_slot != prev_slot && cr->GetIsChosen()) {
        if (auto* item = cr->GetInvItem(moved_item->GetId()); item != nullptr) {
            item->SetCritterSlot(cur_slot);
            moved_item->SetCritterSlot(prev_slot);

            OnItemInvChanged.Fire(item, moved_item);
        }
    }

    if (const auto* hex_cr = dynamic_cast<CritterHexView*>(cr); hex_cr != nullptr) {
        CurMap->UpdateCritterLightSource(hex_cr);
    }
}

void FOClient::Net_OnCritterAnimate()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto state_anim = _conn.InBuf.Read<CritterStateAnim>();
    const auto action_anim = _conn.InBuf.Read<CritterActionAnim>();
    const auto clear_sequence = _conn.InBuf.Read<bool>();
    const auto delay_play = _conn.InBuf.Read<bool>();
    const auto is_context_item = _conn.InBuf.Read<bool>();

    ItemView* context_item = nullptr;

    auto context_item_release = ScopeCallback([&context_item]() noexcept {
        if (context_item != nullptr) {
            context_item->Release();
        }
    });

    if (is_context_item) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(*this);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);

        const auto* proto = ProtoMngr.GetProtoItem(item_pid);
        context_item = new ItemView(this, item_id, proto);
        context_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(context_item);
    }

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);
    if (cr == nullptr) {
        return;
    }

    if (clear_sequence) {
        cr->ClearAnim();
    }
    if (delay_play || !cr->IsAnim()) {
        cr->Animate(state_anim, action_anim, context_item);
    }
}

void FOClient::Net_OnCritterSetAnims()
{
    STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto cond = _conn.InBuf.Read<CritterCondition>();
    const auto state_anim = _conn.InBuf.Read<CritterStateAnim>();
    const auto action_anim = _conn.InBuf.Read<CritterActionAnim>();

    CritterView* cr;
    CritterHexView* hex_cr;

    if (CurMap != nullptr) {
        hex_cr = CurMap->GetCritter(cr_id);
        cr = hex_cr;
    }
    else {
        cr = GetGlobalMapCritter(cr_id);
        hex_cr = nullptr;
    }

    if (cr != nullptr) {
        if (cond == CritterCondition::Alive) {
            cr->SetAliveStateAnim(state_anim);
            cr->SetAliveActionAnim(action_anim);
        }
        if (cond == CritterCondition::Knockout) {
            cr->SetKnockoutStateAnim(state_anim);
            cr->SetKnockoutActionAnim(action_anim);
        }
        if (cond == CritterCondition::Dead) {
            cr->SetDeadStateAnim(state_anim);
            cr->SetDeadActionAnim(action_anim);
        }
    }

    if (hex_cr != nullptr && !hex_cr->IsAnim()) {
        hex_cr->AnimateStay();
    }
}

void FOClient::Net_OnCritterTeleport()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto to_hex = _conn.InBuf.Read<mpos>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = CurMap->GetCritter(cr_id);
    if (cr == nullptr) {
        return;
    }

    CurMap->MoveCritter(cr, to_hex, false);

    if (cr->GetIsChosen()) {
        if (CurMap->AutoScroll.HardLockedCritter == cr->GetId() || CurMap->AutoScroll.SoftLockedCritter == cr->GetId()) {
            CurMap->AutoScroll.CritterLastHex = cr->GetHex();
        }

        CurMap->ScrollToHex(cr->GetHex(), 0.1f, false);
    }
}

void FOClient::Net_OnCritterPos()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto hex_offset = _conn.InBuf.Read<ipos16>();
    const auto dir_angle = _conn.InBuf.Read<int16>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    RUNTIME_ASSERT(CurMap->GetSize().IsValidPos(hex));

    auto* cr = CurMap->GetCritter(cr_id);
    if (cr == nullptr) {
        return;
    }

    cr->ClearMove();

    cr->ChangeLookDirAngle(dir_angle);
    cr->ChangeMoveDirAngle(dir_angle);

    if (cr->GetHex() != hex) {
        CurMap->MoveCritter(cr, hex, true);

        if (cr->GetIsChosen()) {
            CurMap->RebuildFog();
        }
    }

    const auto cr_hex_offset = cr->GetHexOffset();

    if (cr_hex_offset != hex_offset) {
        cr->AddExtraOffs({cr_hex_offset.x - hex_offset.x, cr_hex_offset.y - hex_offset.y});
        cr->SetHexOffset(hex_offset);
        cr->RefreshOffs();
    }
}

void FOClient::Net_OnCritterAttachments()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto is_attached = _conn.InBuf.Read<bool>();
    const auto attach_master = _conn.InBuf.Read<ident_t>();

    const auto attached_critters_count = _conn.InBuf.Read<uint16>();
    vector<ident_t> attached_critters;
    attached_critters.resize(attached_critters_count);
    for (uint16 i = 0; i < attached_critters_count; i++) {
        attached_critters[i] = _conn.InBuf.Read<ident_t>();
    }

    if (CurMap != nullptr) {
        auto* cr = CurMap->GetCritter(cr_id);
        if (cr == nullptr) {
            BreakIntoDebugger();
            return;
        }

        cr->SetAttachMaster(attach_master);

        if (cr->GetIsAttached() != is_attached) {
            cr->SetIsAttached(is_attached);

            if (is_attached) {
                for (auto* map_cr : CurMap->GetCritters()) {
                    if (!map_cr->AttachedCritters.empty() && std::find(map_cr->AttachedCritters.begin(), map_cr->AttachedCritters.end(), cr_id) != map_cr->AttachedCritters.end()) {
                        map_cr->MoveAttachedCritters();
                        break;
                    }
                }
            }
        }

        cr->AttachedCritters = std::move(attached_critters);

        if (!cr->AttachedCritters.empty()) {
            cr->MoveAttachedCritters();
        }
    }
    else {
        auto* cr = GetGlobalMapCritter(cr_id);
        if (cr == nullptr) {
            BreakIntoDebugger();
            return;
        }

        cr->SetIsAttached(is_attached);
        cr->SetAttachMaster(attach_master);
        cr->AttachedCritters = std::move(attached_critters);
    }
}

void FOClient::Net_OnChosenAddItem()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto item_id = _conn.InBuf.Read<ident_t>();
    const auto item_pid = _conn.InBuf.Read<hstring>(*this);
    const auto item_slot = _conn.InBuf.Read<CritterItemSlot>();
    _conn.InBuf.ReadPropsData(_tempPropertiesData);

    auto* chosen = GetChosen();

    if (chosen == nullptr) {
        WriteLog("Chosen is not created on add item");
        BreakIntoDebugger();

        // Skip rest data
        ReceiveCustomEntities(nullptr);

        return;
    }

    if (auto* prev_item = chosen->GetInvItem(item_id); prev_item != nullptr) {
        chosen->DeleteInvItem(prev_item, false);
    }

    const auto* proto = ProtoMngr.GetProtoItem(item_pid);
    auto* item = chosen->AddReceivedInvItem(item_id, proto, item_slot, _tempPropertiesData);

    ReceiveCustomEntities(item);

    if (CurMap != nullptr) {
        CurMap->RebuildFog();

        if (const auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr) {
            CurMap->UpdateCritterLightSource(hex_chosen);
        }
    }

    OnItemInvIn.Fire(item);
}

void FOClient::Net_OnChosenRemoveItem()
{
    STACK_TRACE_ENTRY();

    const auto item_id = _conn.InBuf.Read<ident_t>();

    auto* chosen = GetChosen();

    if (chosen == nullptr) {
        WriteLog("Chosen is not created in remove item");
        BreakIntoDebugger();
        return;
    }

    auto* item = chosen->GetInvItem(item_id);

    if (item == nullptr) {
        // Valid case, item may be removed locally
        return;
    }

    auto* item_clone = item->CreateRefClone();
    auto item_clone_release = ScopeCallback([item_clone]() noexcept { item_clone->Release(); });

    chosen->DeleteInvItem(item, true);

    OnItemInvOut.Fire(item_clone);

    if (CurMap != nullptr) {
        if (const auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr) {
            CurMap->UpdateCritterLightSource(hex_chosen);
        }
    }
}

void FOClient::Net_OnAddItemOnMap()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto item_id = _conn.InBuf.Read<ident_t>();
    const auto item_pid = _conn.InBuf.Read<hstring>(*this);
    _conn.InBuf.ReadPropsData(_tempPropertiesData);

    if (CurMap == nullptr) {
        BreakIntoDebugger();

        // Skip rest data
        ReceiveCustomEntities(nullptr);

        return;
    }

    auto* item = CurMap->AddReceivedItem(item_id, item_pid, hex, _tempPropertiesData);
    RUNTIME_ASSERT(item);

    ReceiveCustomEntities(item);

    if (_screenModeMain != SCREEN_WAIT) {
        item->FadeUp();
    }

    OnItemMapIn.Fire(item);
}

void FOClient::Net_OnRemoveItemFromMap()
{
    STACK_TRACE_ENTRY();

    const auto item_id = _conn.InBuf.Read<ident_t>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* item = CurMap->GetItem(item_id);
    if (item != nullptr) {
        OnItemMapOut.Fire(item);

        // Refresh borders
        if (!item->GetShootThru()) {
            CurMap->RebuildFog();
        }

        item->Finish();
    }
}

void FOClient::Net_OnAnimateItem()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto item_id = _conn.InBuf.Read<ident_t>();
    const auto anim_name = _conn.InBuf.Read<hstring>(*this);
    const auto looped = _conn.InBuf.Read<bool>();
    const auto reversed = _conn.InBuf.Read<bool>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* item = CurMap->GetItem(item_id);

    if (item != nullptr) {
        item->GetAnim()->Play(anim_name, looped, reversed);
    }
}

void FOClient::Net_OnEffect()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto eff_pid = _conn.InBuf.Read<hstring>(*this);
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto radius = _conn.InBuf.Read<uint16>();
    RUNTIME_ASSERT(radius < MAX_HEX_OFFSET);

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    CurMap->RunEffectItem(eff_pid, hex, hex);

    const auto [sx, sy] = Geometry.GetHexOffsets(hex);
    const auto count = GenericUtils::NumericalNumber(radius) * GameSettings::MAP_DIR_COUNT;

    for (uint i = 0; i < count; i++) {
        const auto ex = static_cast<int16>(hex.x) + sx[i];
        const auto ey = static_cast<int16>(hex.y) + sy[i];

        if (CurMap->GetSize().IsValidPos(ipos {ex, ey})) {
            CurMap->RunEffectItem(eff_pid, {static_cast<uint16>(ex), static_cast<uint16>(ey)}, {static_cast<uint16>(ex), static_cast<uint16>(ey)});
        }
    }
}

void FOClient::Net_OnFlyEffect()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto eff_pid = _conn.InBuf.Read<hstring>(*this);
    const auto eff_cr1_id = _conn.InBuf.Read<ident_t>();
    const auto eff_cr2_id = _conn.InBuf.Read<ident_t>();

    auto eff_cr1_hex = _conn.InBuf.Read<mpos>();
    auto eff_cr2_hex = _conn.InBuf.Read<mpos>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    const auto* cr1 = CurMap->GetCritter(eff_cr1_id);
    if (cr1 != nullptr) {
        eff_cr1_hex = cr1->GetHex();
    }

    const auto* cr2 = CurMap->GetCritter(eff_cr2_id);
    if (cr2 != nullptr) {
        eff_cr2_hex = cr2->GetHex();
    }

    CurMap->RunEffectItem(eff_pid, eff_cr1_hex, eff_cr2_hex);
}

void FOClient::Net_OnPlaySound()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    // Todo: synchronize effects showing (for example shot and kill)
    [[maybe_unused]] const auto synchronize_cr_id = _conn.InBuf.Read<uint>();
    const auto sound_name = _conn.InBuf.Read<string>();

    SndMngr.PlaySound(ResMngr.GetSoundNames(), sound_name);
}

void FOClient::Net_OnPlaceToGameComplete()
{
    STACK_TRACE_ENTRY();

    auto* chosen = GetChosen();
    if (chosen == nullptr) {
        BreakIntoDebugger();
        WriteLog("Chosen is not created in end place to game");
        return;
    }

    int target_screen;

    if (CurMap != nullptr) {
        CurMap->FindSetCenter(chosen->GetHex());

        if (auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr) {
            hex_chosen->AnimateStay();
            CurMap->UpdateCritterLightSource(hex_chosen);
        }

        target_screen = SCREEN_GAME;
    }
    else {
        target_screen = SCREEN_GLOBAL_MAP;
    }

    if (target_screen != GetMainScreen()) {
        FlashGameWindow();
        ScreenFadeOut();
        ShowMainScreen(target_screen, {});
    }

    WriteLog("Entering to game complete");
}

void FOClient::Net_OnProperty(uint data_size)
{
    STACK_TRACE_ENTRY();

    uint msg_len;

    if (data_size == 0) {
        msg_len = _conn.InBuf.Read<uint>();
    }
    else {
        msg_len = 0;
    }

    const auto type = _conn.InBuf.Read<NetProperty>();

    ident_t cr_id;
    ident_t item_id;
    ident_t entity_id;

    uint additional_args = 0;

    switch (type) {
    case NetProperty::CritterItem:
        additional_args = 2;
        cr_id = _conn.InBuf.Read<ident_t>();
        item_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::Critter:
        additional_args = 1;
        cr_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        item_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        item_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::CustomEntity:
        additional_args = 1;
        entity_id = _conn.InBuf.Read<ident_t>();
        break;
    default:
        break;
    }

    const auto property_index = _conn.InBuf.Read<uint16>();

    PropertyRawData prop_data;

    if (data_size != 0) {
        _conn.InBuf.Pop(prop_data.Alloc(data_size), data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(uint16);

        if (len != 0) {
            _conn.InBuf.Pop(prop_data.Alloc(len), len);
        }
    }

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
                entity = cr->GetInvItem(item_id);
            }
        }
        break;
    case NetProperty::ChosenItem:
        if (auto* chosen = GetChosen(); chosen != nullptr) {
            entity = chosen->GetInvItem(item_id);
        }
        break;
    case NetProperty::Map:
        entity = CurMap;
        break;
    case NetProperty::Location:
        entity = _curLocation;
        break;
    case NetProperty::CustomEntity:
        entity = GetEntity(entity_id);
        break;
    default:
        UNREACHABLE_PLACE();
    }

    if (entity == nullptr) {
        BreakIntoDebugger();
        return;
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(property_index);

    if (prop == nullptr) {
        BreakIntoDebugger();
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
        auto* item = dynamic_cast<ItemView*>(entity);

        OnItemMapChanged.Fire(item, item);
    }

    if (type == NetProperty::ChosenItem) {
        auto* item = dynamic_cast<ItemView*>(entity);

        OnItemInvChanged.Fire(item, item);
    }
}

void FOClient::Net_OnChosenTalk()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto is_npc = _conn.InBuf.Read<bool>();
    const auto talk_cr_id = _conn.InBuf.Read<ident_t>();
    const auto talk_dlg_id = _conn.InBuf.Read<hstring>(*this);
    const auto count_answ = _conn.InBuf.Read<uint8>();

    if (count_answ == 0) {
        if (IsScreenPresent(SCREEN_DIALOG)) {
            HideScreen(SCREEN_DIALOG);
        }

        return;
    }

    const auto lexems = _conn.InBuf.Read<string>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* npc = is_npc ? CurMap->GetCritter(talk_cr_id) : nullptr;

    const auto text_id = _conn.InBuf.Read<uint>();

    vector<uint> answers_texts;
    for (auto i = 0; i < count_answ; i++) {
        const auto answ_text_id = _conn.InBuf.Read<uint>();
        answers_texts.push_back(answ_text_id);
    }

    auto str = copy(_curLang.GetTextPack(TextPackName::Dialogs).GetStr(text_id));

    FormatTags(str, GetChosen(), npc, lexems);

    const auto text_to_script = str;
    string answers_to_script;

    for (const auto answers_text : answers_texts) {
        str = copy(_curLang.GetTextPack(TextPackName::Dialogs).GetStr(answers_text));
        FormatTags(str, GetChosen(), npc, lexems);
        answers_to_script += string(!answers_to_script.empty() ? "\n" : "") + str;
    }

    const auto talk_time = _conn.InBuf.Read<uint>();

    map<string, any_t> params;
    params["TalkerIsNpc"] = any_t {strex("{}", is_npc ? 1 : 0)};
    params["TalkerId"] = any_t {strex("{}", is_npc ? talk_cr_id.underlying_value() : talk_dlg_id.as_uint())};
    params["Text"] = any_t {strex("{}", text_to_script)};
    params["Answers"] = any_t {strex("{}", answers_to_script)};
    params["TalkTime"] = any_t {strex("{}", talk_time)};
    ShowScreen(SCREEN_DIALOG, params);
}

void FOClient::Net_OnTimeSync()
{
    STACK_TRACE_ENTRY();

    const auto year = _conn.InBuf.Read<uint16>();
    const auto month = _conn.InBuf.Read<uint16>();
    const auto day = _conn.InBuf.Read<uint16>();
    const auto hour = _conn.InBuf.Read<uint16>();
    const auto minute = _conn.InBuf.Read<uint16>();
    const auto second = _conn.InBuf.Read<uint16>();
    const auto multiplier = _conn.InBuf.Read<uint16>();

    GameTime.SetServerTime(year, month, day, hour, minute, second, multiplier);

    SetYear(year);
    SetMonth(month);
    SetDay(day);
    SetHour(hour);
    SetMinute(minute);
    SetSecond(second);
    SetTimeMultiplier(multiplier);
}

void FOClient::Net_OnLoadMap()
{
    STACK_TRACE_ENTRY();

    WriteLog("Change map");

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto loc_id = _conn.InBuf.Read<ident_t>();
    const auto map_id = _conn.InBuf.Read<ident_t>();
    const auto loc_pid = _conn.InBuf.Read<hstring>(*this);
    const auto map_pid = _conn.InBuf.Read<hstring>(*this);
    const auto map_index_in_loc = _conn.InBuf.Read<uint8>();

    if (map_pid) {
        _conn.InBuf.ReadPropsData(_tempPropertiesData);
        _conn.InBuf.ReadPropsData(_tempPropertiesDataExt);
    }

    OnMapUnload.Fire();

    if (CurMap != nullptr) {
        CurMap->DestroySelf();
        CurMap = nullptr;

        CleanupSpriteCache();
    }

    if (_curLocation != nullptr) {
        _curLocation->DestroySelf();
        _curLocation = nullptr;
    }

    SndMngr.StopSounds();
    ShowMainScreen(SCREEN_WAIT, {});

    _curMapLocPid = loc_pid;
    _curMapIndexInLoc = map_index_in_loc;

    if (map_pid) {
        const auto* loc_proto = ProtoMngr.GetProtoLocation(loc_pid);
        _curLocation = new LocationView(this, loc_id, loc_proto);
        _curLocation->RestoreData(_tempPropertiesDataExt);

        const auto* map_proto = ProtoMngr.GetProtoMap(map_pid);
        CurMap = new MapView(this, map_id, map_proto);
        CurMap->RestoreData(_tempPropertiesData);
        CurMap->LoadStaticData();

        ReceiveCustomEntities(_curLocation);
        ReceiveCustomEntities(CurMap);

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
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();

    _globalMapLocs.clear();

    const auto count_loc = _conn.InBuf.Read<uint16>();

    for (auto i = 0; i < count_loc; i++) {
        GmapLocation loc;
        loc.LocId = _conn.InBuf.Read<ident_t>();
        loc.LocPid = _conn.InBuf.Read<hstring>(*this);
        loc.LocPos = _conn.InBuf.Read<upos16>();
        loc.Radius = _conn.InBuf.Read<uint16>();
        loc.Color = _conn.InBuf.Read<ucolor>();
        loc.Entrances = _conn.InBuf.Read<uint8>();
        _globalMapLocs.push_back(loc);
    }

    auto* fog_data = _globalMapFog.GetData();
    _conn.InBuf.Pop(fog_data, GM_ZONES_FOG_SIZE);
}

void FOClient::Net_OnGlobalLocation()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();

    const auto add = _conn.InBuf.Read<bool>();

    GmapLocation loc;
    loc.LocId = _conn.InBuf.Read<ident_t>();
    loc.LocPid = _conn.InBuf.Read<hstring>(*this);
    loc.LocPos = _conn.InBuf.Read<upos16>();
    loc.Radius = _conn.InBuf.Read<uint16>();
    loc.Color = _conn.InBuf.Read<ucolor>();
    loc.Entrances = _conn.InBuf.Read<uint8>();

    const auto it = std::find_if(_globalMapLocs.begin(), _globalMapLocs.end(), [&loc](const GmapLocation& l) { return loc.LocId == l.LocId; });
    if (add) {
        if (it != _globalMapLocs.end()) {
            *it = loc;
        }
        else {
            _globalMapLocs.push_back(loc);
        }
    }
    else {
        if (it != _globalMapLocs.end()) {
            _globalMapLocs.erase(it);
        }
    }
}

void FOClient::Net_OnGlobalFog()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();

    const auto zx = _conn.InBuf.Read<uint16>();
    const auto zy = _conn.InBuf.Read<uint16>();
    const auto fog = _conn.InBuf.Read<uint8>();

    _globalMapFog.Set2Bit(zx, zy, fog);
}

void FOClient::Net_OnSomeItems()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto context_param = any_t {_conn.InBuf.Read<string>()};
    const auto items_count = _conn.InBuf.Read<uint>();

    vector<ItemView*> items;
    items.reserve(items_count);

    for (uint i = 0; i < items_count; i++) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto pid = _conn.InBuf.Read<hstring>(*this);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);
        RUNTIME_ASSERT(item_id);

        const auto* proto = ProtoMngr.GetProtoItem(pid);
        auto* item = new ItemView(this, item_id, proto);
        item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(item);

        items.emplace_back(item);
    }

    OnReceiveItems.Fire(items, context_param);

    for (const auto* item : items) {
        item->Release();
    }
}

void FOClient::Net_OnViewMap()
{
    STACK_TRACE_ENTRY();

    const auto hex = _conn.InBuf.Read<mpos>();
    const auto loc_id = _conn.InBuf.Read<ident_t>();
    const auto loc_ent = _conn.InBuf.Read<uint>();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    CurMap->FindSetCenter(hex);
    ShowMainScreen(SCREEN_GAME, {});
    ScreenFadeOut();

    map<string, any_t> params;
    params["LocationId"] = any_t {strex("{}", loc_id)};
    params["LocationEntrance"] = any_t {strex("{}", loc_ent)};
    ShowScreen(SCREEN_TOWN_VIEW, params);
}

void FOClient::Net_OnRemoteCall()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto rpc_num = _conn.InBuf.Read<uint>();

    ScriptSys->HandleRemoteCall(rpc_num, _curPlayer);
}

void FOClient::Net_OnAddCustomEntity()
{
    STACK_TRACE_ENTRY();

    [[maybe_unused]] const auto msg_len = _conn.InBuf.Read<uint>();
    const auto holder_id = _conn.InBuf.Read<ident_t>();
    const auto holder_entry = _conn.InBuf.Read<hstring>(*this);
    const auto id = _conn.InBuf.Read<ident_t>();
    const auto pid = _conn.InBuf.Read<hstring>(*this);
    _conn.InBuf.ReadPropsData(_tempPropertiesDataCustomEntity);

    Entity* holder;

    if (holder_id) {
        holder = GetEntity(holder_id);

        if (holder == nullptr) {
            BreakIntoDebugger();
            return;
        }
    }
    else {
        holder = this;
    }

    auto* entity = CreateCustomEntityView(holder, holder_entry, id, pid, _tempPropertiesDataCustomEntity);

    OnCustomEntityIn.Fire(entity);
}

void FOClient::Net_OnRemoveCustomEntity()
{
    STACK_TRACE_ENTRY();

    const auto id = _conn.InBuf.Read<ident_t>();

    auto* entity = GetEntity(id);

    if (entity == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* custom_entity = dynamic_cast<CustomEntityView*>(entity);

    if (custom_entity == nullptr) {
        BreakIntoDebugger();
        return;
    }

    OnCustomEntityOut.Fire(custom_entity);

    Entity* holder;

    if (custom_entity->GetCustomHolderId()) {
        holder = GetEntity(custom_entity->GetCustomHolderId());
    }
    else {
        holder = this;
    }

    if (holder != nullptr) {
        holder->RemoveInnerEntity(custom_entity->GetCustomHolderEntry(), custom_entity);
    }
    else {
        BreakIntoDebugger();
    }

    custom_entity->DestroySelf();
}

void FOClient::ReceiveCustomEntities(Entity* holder)
{
    STACK_TRACE_ENTRY();

    const auto entries_count = _conn.InBuf.Read<uint16>();

    if (entries_count == 0) {
        return;
    }

    for (uint16 i = 0; i < entries_count; i++) {
        const auto entry = _conn.InBuf.Read<hstring>(*this);
        const auto entities_count = _conn.InBuf.Read<uint>();

        for (uint j = 0; j < entities_count; j++) {
            const auto id = _conn.InBuf.Read<ident_t>();
            const auto pid = _conn.InBuf.Read<hstring>(*this);
            _conn.InBuf.ReadPropsData(_tempPropertiesDataCustomEntity);

            if (holder != nullptr) {
                auto* entity = CreateCustomEntityView(holder, entry, id, pid, _tempPropertiesDataCustomEntity);

                ReceiveCustomEntities(entity);
            }
            else {
                ReceiveCustomEntities(nullptr);
            }
        }
    }
}

auto FOClient::CreateCustomEntityView(Entity* holder, hstring entry, ident_t id, hstring pid, const vector<vector<uint8>>& data) -> CustomEntityView*
{
    STACK_TRACE_ENTRY();

    const auto type_name = std::get<0>(GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.at(entry));

    RUNTIME_ASSERT(IsValidEntityType(type_name));

    const bool has_protos = GetEntityTypeInfo(type_name).HasProtos;
    const ProtoEntity* proto = nullptr;

    if (pid) {
        RUNTIME_ASSERT(has_protos);
        proto = ProtoMngr.GetProtoEntity(type_name, pid);
        RUNTIME_ASSERT(proto);
    }
    else {
        RUNTIME_ASSERT(!has_protos);
    }

    const auto* registrator = GetPropertyRegistrator(type_name);

    CustomEntityView* entity;

    if (proto != nullptr) {
        entity = new CustomEntityWithProtoView(this, id, registrator, nullptr, proto);
    }
    else {
        entity = new CustomEntityView(this, id, registrator, nullptr);
    }

    entity->RestoreData(data);

    if (const auto* holder_with_id = dynamic_cast<ClientEntity*>(holder); holder_with_id != nullptr) {
        entity->SetCustomHolderId(holder_with_id->GetId());
    }
    else {
        RUNTIME_ASSERT(holder == this);
    }

    entity->SetCustomHolderEntry(entry);

    holder->AddInnerEntity(entry, entity);

    return entity;
}

auto FOClient::GetEntity(ident_t id) -> ClientEntity*
{
    STACK_TRACE_ENTRY();

    const auto it = _allEntities.find(id);

    return it != _allEntities.end() ? it->second : nullptr;
}

void FOClient::RegisterEntity(ClientEntity* entity)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(entity);
    RUNTIME_ASSERT(entity->GetId());

    _allEntities[entity->GetId()] = entity;
}

void FOClient::UnregisterEntity(ClientEntity* entity)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(entity);
    RUNTIME_ASSERT(entity->GetId());

    _allEntities.erase(entity->GetId());
}

void FOClient::FlashGameWindow()
{
    STACK_TRACE_ENTRY();

    if (SprMngr.IsWindowFocused()) {
        return;
    }

    if (Settings.WinNotify) {
        SprMngr.BlinkWindow();
    }
}

auto FOClient::AnimLoad(hstring name, AtlasType atlas_type) -> uint
{
    STACK_TRACE_ENTRY();

    if (const auto it = _ifaceAnimationsCache.find(name); it != _ifaceAnimationsCache.end()) {
        auto&& iface_anim = it->second;

        iface_anim->Anim->PlayDefault();

        _ifaceAnimations[++_ifaceAnimIndex] = std::move(iface_anim);
        _ifaceAnimationsCache.erase(it);

        return _ifaceAnimIndex;
    }

    auto&& anim = SprMngr.LoadSprite(name, atlas_type);
    if (!anim) {
        BreakIntoDebugger();
        return 0;
    }

    anim->PlayDefault();

    auto&& iface_anim = std::make_unique<IfaceAnim>();

    iface_anim->Name = name;
    iface_anim->Anim = anim;

    _ifaceAnimations[++_ifaceAnimIndex] = std::move(iface_anim);

    return _ifaceAnimIndex;
}

void FOClient::AnimFree(uint anim_id)
{
    STACK_TRACE_ENTRY();

    if (const auto it = _ifaceAnimations.find(anim_id); it != _ifaceAnimations.end()) {
        auto&& iface_anim = it->second;

        iface_anim->Anim->Stop();

        _ifaceAnimationsCache.emplace(iface_anim->Name, std::move(iface_anim));
        _ifaceAnimations.erase(it);
    }
}

auto FOClient::AnimGetSpr(uint anim_id) -> Sprite*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _ifaceAnimations.find(anim_id); it != _ifaceAnimations.end()) {
        return it->second->Anim.get();
    }
    return nullptr;
}

void FOClient::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(entity == this);

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Game, prop, this);
    }
    else {
        throw GenericException("Unable to send global modifiable property", prop->GetName());
    }
}

void FOClient::OnSendPlayerValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(entity == _curPlayer);

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (!_curPlayer->GetId()) {
        throw ScriptException("Can't modify player public/protected property on unlogined player");
    }

    Net_SendProperty(NetProperty::Player, prop, _curPlayer);
}

void FOClient::OnSendCritterValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    auto* cr = dynamic_cast<CritterView*>(entity);
    if (cr->GetIsChosen()) {
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
    STACK_TRACE_ENTRY();

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (auto* item = dynamic_cast<ItemView*>(entity); item != nullptr && !item->GetStatic() && item->GetId()) {
        if (item->GetOwnership() == ItemOwnership::CritterInventory) {
            const auto* cr = CurMap->GetCritter(item->GetCritterId());
            if (cr != nullptr && cr->GetIsChosen()) {
                Net_SendProperty(NetProperty::ChosenItem, prop, item);
            }
            else if (cr != nullptr && prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
                Net_SendProperty(NetProperty::CritterItem, prop, item);
            }
            else {
                throw GenericException("Unable to send item (critter) modifiable property", prop->GetName());
            }
        }
        else if (item->GetOwnership() == ItemOwnership::MapHex) {
            if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
                Net_SendProperty(NetProperty::MapItem, prop, item);
            }
            else {
                throw GenericException("Unable to send item (map) modifiable property", prop->GetName());
            }
        }
        else {
            throw GenericException("Unable to send item (container) modifiable property", prop->GetName());
        }
    }
}

void FOClient::OnSendMapValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(entity == CurMap);

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Map, prop, CurMap);
    }
    else {
        throw GenericException("Unable to send map modifiable property", prop->GetName());
    }
}

void FOClient::OnSendLocationValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(entity == _curLocation);

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Location, prop, _curLocation);
    }
    else {
        throw GenericException("Unable to send location modifiable property", prop->GetName());
    }
}

void FOClient::OnSetCritterLookDistance(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr && cr->GetIsChosen()) {
        cr->GetMap()->RefreshMap();
        cr->GetMap()->RebuildFog();
    }
}

void FOClient::OnSetCritterModelName(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
#if FO_ENABLE_3D
        cr->RefreshModel();

        if (cr->IsModel()) {
            cr->GetModel()->StartMeshGeneration();
        }
#endif

        cr->Action(CritterAction::Refresh, 0, nullptr, false);
    }
}

void FOClient::OnSetCritterContourColor(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr && cr->IsSpriteValid()) {
        cr->GetSprite()->SetContour(cr->GetSprite()->Contour, cr->GetContourColor());
    }
}

void FOClient::OnSetCritterHideSprite(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        cr->SetSpriteVisiblity(!cr->GetHideSprite());
    }
}

void FOClient::OnSetItemFlags(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // Colorize, ColorizeColor, BadItem, ShootThru, LightThru, NoBlock

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        auto rebuild_cache = false;

        if (prop == item->GetPropertyColorize() || prop == item->GetPropertyColorizeColor()) {
            item->RefreshAlpha();
            item->RefreshSprite();
        }
        else if (prop == item->GetPropertyBadItem()) {
            item->RefreshSprite();
        }
        else if (prop == item->GetPropertyShootThru()) {
            CurMap->RebuildFog();
            rebuild_cache = true;
        }
        else if (prop == item->GetPropertyLightThru()) {
            item->GetMap()->UpdateHexLightSources(item->GetHex());
            rebuild_cache = true;
        }
        else if (prop == item->GetPropertyNoBlock()) {
            rebuild_cache = true;
        }

        if (rebuild_cache) {
            item->GetMap()->RecacheHexFlags(item->GetHex());
        }
    }
}

void FOClient::OnSetItemSomeLight(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->GetMap()->UpdateItemLightSource(item);
    }
}

void FOClient::OnSetItemPicMap(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->RefreshAnim();
    }
}

void FOClient::OnSetItemOffsetCoords(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->RefreshOffs();
        item->GetMap()->MeasureMapBorders(item);
    }
}

void FOClient::OnSetItemOpened(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        if (item->GetCanOpen()) {
            if (item->GetOpened()) {
                item->GetAnim()->Play({}, false, false);
            }
            else {
                item->GetAnim()->Play({}, false, true);
            }
        }
    }
}

void FOClient::OnSetItemHideSprite(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->SetSpriteVisiblity(!item->GetHideSprite());
    }
}

void FOClient::ChangeLanguage(string_view lang_name)
{
    STACK_TRACE_ENTRY();

    auto lang_pack = LanguagePack {lang_name, *this};
    lang_pack.LoadTexts(Resources);

    _curLang = std::move(lang_pack);
    Settings.Language = lang_name;
}

void FOClient::ConsoleMessage(string_view msg)
{
    STACK_TRACE_ENTRY();

    auto str = string(msg);
    int how_say = SAY_NORM;
    const auto result = OnOutMessage.Fire(str, how_say);

    if (result && !str.empty()) {
        Net_SendText(str, static_cast<uint8>(how_say));
    }
}

void FOClient::AddMessage(int mess_type, string_view msg)
{
    STACK_TRACE_ENTRY();

    OnMessageBox.Fire(mess_type, string(msg));
}

// Todo: move targs formatting to scripts
void FOClient::FormatTags(string& text, CritterView* cr, CritterView* npc, string_view lexems)
{
    STACK_TRACE_ENTRY();

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
                tag = strex(text.substr(i + 1)).substringUntil('|');
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

            tag = strex(text.substr(i + 1)).substringUntil('@');
            text.erase(i, tag.length() + 2);
            if (tag.empty()) {
                break;
            }

            // Player name
            if (strex(tag).compareIgnoreCase("pname")) {
                tag = (cr != nullptr ? cr->GetName() : "");
            }
            // Npc name
            else if (strex(tag).compareIgnoreCase("nname")) {
                tag = (npc != nullptr ? npc->GetName() : "");
            }
            // Sex
            else if (strex(tag).compareIgnoreCase("sex")) {
                sex = (cr != nullptr && cr->GetSexTagFemale() ? 2 : 1);
                sex_tags = true;
                continue;
            }
            // Random
            else if (strex(tag).compareIgnoreCase("rnd")) {
                auto first = text.find_first_of('|', i);
                auto last = text.find_last_of('|', i);
                auto rnd = strex(text.substr(first, last - first + 1)).split('|');
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
                    tag = strex(lexems.substr(pos)).substringUntil('$').trim();
                }
                else {
                    tag = "<lexem not found>";
                }
            }
            // Text pack
            else if (tag.length() > 5 && tag[0] == 't' && tag[1] == 'e' && tag[2] == 'x' && tag[3] == 't' && tag[4] == ' ') {
                tag = tag.substr(5);
                tag = strex(tag).erase('(').erase(')');

                istringstream itag(tag);
                string pack_name_str;
                uint str_num = 0;

                if (itag >> pack_name_str >> str_num) {
                    bool failed = false;
                    const auto pack_name = _curLang.ResolveTextPackName(pack_name_str, &failed);
                    const auto& text_pack = _curLang.GetTextPack(pack_name);

                    if (failed) {
                        tag = "<text tag, invalid pack name>";
                    }
                    else if (text_pack.GetStrCount(str_num) == 0) {
                        tag = strex("<text tag, string {} not found>", str_num);
                    }
                    else {
                        tag = text_pack.GetStr(str_num);
                    }
                }
                else {
                    tag = "<text tag parse fail>";
                }
            }
            // Script
            else if (tag.length() > 7 && tag[0] == 's' && tag[1] == 'c' && tag[2] == 'r' && tag[3] == 'i' && tag[4] == 'p' && tag[5] == 't' && tag[6] == ' ') {
                string func_name = strex(tag.substr(7)).substringUntil('$');
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

void FOClient::ShowMainScreen(int new_screen, const map<string, any_t>& params)
{
    STACK_TRACE_ENTRY();

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
    RunScreenScript(true, new_screen, params);

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
            _screenFadingEffects.clear();
            _waitPic = ResMngr.GetRandomSplash();
        }
        break;
    default:
        break;
    }
}

auto FOClient::GetActiveScreen(vector<int>* screens) -> int
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

    vector<int> active_screens;
    GetActiveScreen(&active_screens);
    return std::find(active_screens.begin(), active_screens.end(), screen) != active_screens.end();
}

void FOClient::ShowScreen(int screen, const map<string, any_t>& params)
{
    STACK_TRACE_ENTRY();

    RunScreenScript(true, screen, params);
}

void FOClient::HideScreen(int screen)
{
    STACK_TRACE_ENTRY();

    if (screen == SCREEN_NONE) {
        screen = GetActiveScreen(nullptr);
    }
    if (screen == SCREEN_NONE) {
        return;
    }

    RunScreenScript(false, screen, {});
}

void FOClient::RunScreenScript(bool show, int screen, const map<string, any_t>& params)
{
    STACK_TRACE_ENTRY();

    OnScreenChange.Fire(show, screen, params);
}

void FOClient::LmapPrepareMap()
{
    STACK_TRACE_ENTRY();

    _lmapPrepPix.clear();

    if (CurMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    const auto* chosen = GetMapChosen();
    if (chosen == nullptr) {
        return;
    }

    const auto hex = chosen->GetHex();
    const auto maxpixx = (_lmapWMap[2] - _lmapWMap[0]) / 2 / _lmapZoom;
    const auto maxpixy = (_lmapWMap[3] - _lmapWMap[1]) / 2 / _lmapZoom;
    const auto bx = hex.x - maxpixx;
    const auto by = hex.y - maxpixy;
    const auto ex = hex.x + maxpixx;
    const auto ey = hex.y + maxpixy;

    const auto vis = chosen->GetLookDistance();
    auto pix_x = _lmapWMap[2] - _lmapWMap[0];
    auto pix_y = 0;

    for (auto i1 = bx; i1 < ex; i1++) {
        for (auto i2 = by; i2 < ey; i2++) {
            pix_y += _lmapZoom;

            if (i1 < 0 || i2 < 0 || i1 >= CurMap->GetSize().width || i2 >= CurMap->GetSize().height) {
                continue;
            }

            auto is_far = false;
            const auto dist = GeometryHelper::DistGame(hex.x, hex.y, i1, i2);
            if (dist > vis) {
                is_far = true;
            }

            const auto& field = CurMap->GetField({static_cast<uint16>(i1), static_cast<uint16>(i2)});
            ucolor cur_color;

            if (const auto* cr = CurMap->GetNonDeadCritter({static_cast<uint16>(i1), static_cast<uint16>(i2)}); cr != nullptr) {
                cur_color = (cr == chosen ? ucolor {0, 0, 255} : ucolor {255, 0, 0});
                _lmapPrepPix.emplace_back(PrimitivePoint {{_lmapWMap[0] + pix_x + (_lmapZoom - 1), _lmapWMap[1] + pix_y}, cur_color});
                _lmapPrepPix.emplace_back(PrimitivePoint {{_lmapWMap[0] + pix_x, _lmapWMap[1] + pix_y + (_lmapZoom - 1) / 2}, cur_color});
            }
            else if (field.Flags.HasWall || field.Flags.HasScenery) {
                if (field.Flags.ScrollBlock) {
                    continue;
                }
                if (!_lmapSwitchHi && !field.Flags.HasWall) {
                    continue;
                }
                cur_color = ucolor {(field.Flags.HasWall ? ucolor {0, 255, 0, 255} : ucolor {0, 255, 0, 127})};
            }
            else {
                continue;
            }

            if (is_far) {
                cur_color.comp.a = 0x22;
            }

            _lmapPrepPix.emplace_back(PrimitivePoint {{_lmapWMap[0] + pix_x, _lmapWMap[1] + pix_y}, cur_color});
            _lmapPrepPix.emplace_back(PrimitivePoint {{_lmapWMap[0] + pix_x + (_lmapZoom - 1), _lmapWMap[1] + pix_y + (_lmapZoom - 1) / 2}, cur_color});
        }

        pix_x -= _lmapZoom;
        pix_y = 0;
    }

    _lmapPrepareNextTime = GameTime.FrameTime() + std::chrono::milliseconds {1000};
}

void FOClient::GmapNullParams()
{
    STACK_TRACE_ENTRY();

    _globalMapLocs.clear();
    _globalMapFog.Fill(0);

    for (auto* cr : _globalMapCritters) {
        cr->DestroySelf();
    }

    _globalMapCritters.clear();
}

void FOClient::WaitDraw()
{
    STACK_TRACE_ENTRY();

    if (_waitPic) {
        SprMngr.DrawSpriteSize(_waitPic.get(), {0, 0}, {Settings.ScreenWidth, Settings.ScreenHeight}, true, true, COLOR_SPRITE);
        SprMngr.Flush();
    }
}

void FOClient::CleanupSpriteCache()
{
    STACK_TRACE_ENTRY();

    ResMngr.CleanupCritterFrames();
    SprMngr.CleanupSpriteCache();
}

void FOClient::DestroyInnerEntities()
{
    STACK_TRACE_ENTRY();

    if (HasInnerEntities()) {
        for (auto&& [entry, entities] : GetRawInnerEntities()) {
            for (auto* entity : entities) {
                auto* custom_entity = dynamic_cast<CustomEntityView*>(entity);
                RUNTIME_ASSERT(custom_entity);

                custom_entity->DestroySelf();
            }
        }

        ClearInnerEntities();
    }
}

auto FOClient::CustomCall(string_view command, string_view separator) -> string
{
    STACK_TRACE_ENTRY();

    // Parse command
    vector<string> args;
    const auto command_str = string(command);
    std::istringstream ss(command_str);

    if (!separator.empty()) {
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

    if (cmd == "DumpAtlases") {
        SprMngr.GetAtlasMngr().DumpAtlases();
    }
    else if (cmd == "SwitchShowTrack") {
        if (CurMap != nullptr) {
            CurMap->SwitchShowTrack();
        }
    }
    else if (cmd == "SwitchShowHex") {
        if (CurMap != nullptr) {
            CurMap->SwitchShowHex();
        }
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
    else if (cmd == "BytesSend") {
        return strex("{}", _conn.GetBytesSend());
    }
    else if (cmd == "BytesReceive") {
        return strex("{}", _conn.GetBytesReceived());
    }
    else if (cmd == "SetResolution" && args.size() >= 3) {
        const int w = strex(args[1]).toInt();
        const int h = strex(args[2]).toInt();

        SprMngr.SetScreenSize({w, h});
        SprMngr.SetWindowSize({w, h});
    }
    else if (cmd == "RefreshAlwaysOnTop") {
        SprMngr.SetAlwaysOnTop(Settings.AlwaysOnTop);
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
        const auto is_cr = strex(args[1]).toBool();
        const auto cr_id = is_cr ? ident_t {strex(args[2]).toUInt()} : ident_t {};
        const auto dlg_pack_id = is_cr ? hstring() : ResolveHash(strex(args[2]).toUInt());
        const auto answer_index = static_cast<uint8>(strex(args[3]).toUInt());

        Net_SendTalk(cr_id, dlg_pack_id, answer_index);
    }
    else if (cmd == "DrawMiniMap" && args.size() >= 6) {
        const int zoom = strex(args[1]).toInt();
        const int x = strex(args[2]).toInt();
        const int y = strex(args[3]).toInt();
        const int x2 = x + strex(args[4]).toInt();
        const int y2 = y + strex(args[5]).toInt();

        if (zoom != _lmapZoom || x != _lmapWMap[0] || y != _lmapWMap[1] || x2 != _lmapWMap[2] || y2 != _lmapWMap[3]) {
            _lmapZoom = zoom;
            _lmapWMap[0] = x;
            _lmapWMap[1] = y;
            _lmapWMap[2] = x2;
            _lmapWMap[3] = y2;
            LmapPrepareMap();
        }
        else if (GameTime.FrameTime() >= _lmapPrepareNextTime) {
            LmapPrepareMap();
        }

        SprMngr.DrawPoints(_lmapPrepPix, RenderPrimitiveType::LineList);
    }
    else if (cmd == "DrawWait") {
        WaitDraw();
    }
    else if (cmd == "SkipRoof" && args.size() == 3) {
        if (CurMap != nullptr) {
            const auto hx = strex(args[1]).toUInt();
            const auto hy = strex(args[2]).toUInt();
            CurMap->SetSkipRoof({static_cast<uint16>(hx), static_cast<uint16>(hy)});
        }
    }
    else {
        throw ScriptException("Invalid custom call command", cmd, args.size());
    }

    return "";
}

void FOClient::Connect(string_view login, string_view password, int reason)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(reason == INIT_NET_REASON_LOGIN || reason == INIT_NET_REASON_REG || reason == INIT_NET_REASON_CUSTOM);

    _loginName = login;
    _loginPassword = password;

    if (!_conn.IsConnected()) {
        _initNetReason = reason;
    }
    else {
        if (reason == INIT_NET_REASON_LOGIN) {
            Net_SendLogIn();
        }
        else if (reason == INIT_NET_REASON_REG) {
            Net_SendCreatePlayer();
        }
    }
}

void FOClient::Disconnect()
{
    STACK_TRACE_ENTRY();

    _conn.Disconnect();

    if (!_conn.IsConnected() && !IsMainScreen(SCREEN_LOGIN)) {
        ShowMainScreen(SCREEN_LOGIN, {});
    }
}

void FOClient::CritterMoveTo(CritterHexView* cr, variant<tuple<mpos, ipos16>, int> pos_or_dir, uint speed)
{
    STACK_TRACE_ENTRY();

    if (cr->GetIsAttached()) {
        return;
    }

    const auto prev_moving = cr->IsMoving();

    cr->ClearMove();

    bool try_move = false;
    mpos hex;
    ipos16 hex_offset;
    vector<uint8> steps;
    vector<uint16> control_steps;

    if (speed != 0 && cr->IsAlive()) {
        if (pos_or_dir.index() == 0) {
            hex = std::get<0>(std::get<0>(pos_or_dir));
            hex_offset = std::get<1>(std::get<0>(pos_or_dir));

            const auto find_path = cr->GetMap()->FindPath(cr, cr->GetHex(), hex, -1);

            if (find_path && !find_path->DirSteps.empty()) {
                steps = find_path->DirSteps;
                control_steps = find_path->ControlSteps;
                try_move = true;
            }
        }
        else if (pos_or_dir.index() == 1) {
            const auto quad_dir = std::get<1>(pos_or_dir);

            if (quad_dir != -1) {
                hex = cr->GetHex();

                if (cr->GetMap()->TraceMoveWay(hex, hex_offset, steps, quad_dir)) {
                    control_steps.push_back(static_cast<uint16>(steps.size()));
                    try_move = true;
                }
            }
        }
    }

    if (try_move) {
        cr->Moving.Steps = steps;
        cr->Moving.ControlSteps = control_steps;
        cr->Moving.StartTime = GameTime.FrameTime();
        cr->Moving.Speed = static_cast<uint16>(speed);
        cr->Moving.StartHex = cr->GetHex();
        cr->Moving.EndHex = hex;
        cr->Moving.StartHexOffset = cr->GetHexOffset();
        cr->Moving.EndHexOffset = hex_offset;

        cr->Moving.WholeTime = {};
        cr->Moving.WholeDist = {};

        RUNTIME_ASSERT(cr->Moving.Speed > 0);
        const auto base_move_speed = static_cast<float>(cr->Moving.Speed);

        auto next_start_hex = cr->Moving.StartHex;
        uint16 control_step_begin = 0;

        for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
            auto hex2 = next_start_hex;

            RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
            RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
            for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
                const auto move_ok = GeometryHelper::MoveHexByDir(hex2, cr->Moving.Steps[j], cr->GetMap()->GetSize());
                RUNTIME_ASSERT(move_ok);
            }

            auto offset2 = Geometry.GetHexInterval(next_start_hex, hex2);

            if (i == 0) {
                offset2.x -= cr->Moving.StartHexOffset.x;
                offset2.y -= cr->Moving.StartHexOffset.y;
            }
            if (i == cr->Moving.ControlSteps.size() - 1) {
                offset2.x += cr->Moving.EndHexOffset.x;
                offset2.y += cr->Moving.EndHexOffset.y;
            }

            const auto proj_oy = static_cast<float>(offset2.y) * Geometry.GetYProj();
            const auto dist = std::sqrt(static_cast<float>(offset2.x * offset2.x) + proj_oy * proj_oy);

            cr->Moving.WholeDist += dist;
            cr->Moving.WholeTime += dist / base_move_speed * 1000.0f;

            control_step_begin = cr->Moving.ControlSteps[i];
            next_start_hex = hex2;

            if (i == cr->Moving.ControlSteps.size() - 1) {
                RUNTIME_ASSERT(hex2 == cr->Moving.EndHex);
            }
        }

        cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);
        cr->Moving.WholeDist = std::max(cr->Moving.WholeDist, 0.0001f);

        RUNTIME_ASSERT(!cr->Moving.Steps.empty());
        RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
        RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
        RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

        cr->AnimateStay();
        Net_SendMove(cr);
    }

    if (prev_moving && !cr->IsMoving()) {
        cr->ClearMove();
        cr->AnimateStay();
        Net_SendStopMove(cr);
    }
}

void FOClient::CritterLookTo(CritterHexView* cr, variant<uint8, int16> dir_or_angle)
{
    STACK_TRACE_ENTRY();

    const auto prev_dir_angle = cr->GetDirAngle();

    if (dir_or_angle.index() == 0) {
        cr->ChangeDir(std::get<0>(dir_or_angle));
    }
    else if (dir_or_angle.index() == 1) {
        cr->ChangeLookDirAngle(std::get<1>(dir_or_angle));
    }

    if (cr->GetDirAngle() != prev_dir_angle) {
        Net_SendDir(cr);
    }
}

void FOClient::PlayVideo(string_view video_name, bool can_interrupt, bool enqueue)
{
    STACK_TRACE_ENTRY();

    if (_video && enqueue) {
        _videoQueue.emplace_back(string(video_name), can_interrupt);
        return;
    }

    _video.reset();
    _videoTex.reset();
    _videoQueue.clear();
    _videoCanInterrupt = can_interrupt;

    if (video_name.empty()) {
        return;
    }

    const auto names = strex(video_name).split('|');

    const auto file = Resources.ReadFile(names[0]);
    if (!file) {
        return;
    }

    _video = std::make_unique<VideoClip>(file.GetData());
    _videoTex = unique_ptr<RenderTexture> {App->Render.CreateTexture(_video->GetSize(), true, false)};

    if (names.size() > 1) {
        SndMngr.StopMusic();

        if (!names[1].empty()) {
            SndMngr.PlayMusic(names[1], std::chrono::milliseconds {0});
        }
    }
}

void FOClient::ProcessVideo()
{
    STACK_TRACE_ENTRY();

    if (_video) {
        _videoTex->UpdateTextureRegion({}, _videoTex->Size, _video->RenderFrame().data());
        SprMngr.DrawTexture(_videoTex.get(), false);

        if (_video->IsStopped()) {
            _video.reset();
            _videoTex.reset();
            SndMngr.StopMusic();
            ScreenFadeOut();
        }
    }

    if (!_video && !_videoQueue.empty()) {
        auto queue = copy(_videoQueue);

        PlayVideo(std::get<0>(queue.front()), std::get<1>(queue.front()), false);

        queue.erase(queue.begin());
        _videoQueue = queue;
    }
}
