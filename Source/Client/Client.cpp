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

#include "Client.h"
#include "DefaultSprites.h"
#include "ModelSprites.h"
#include "NetCommand.h"
#include "ParticleSprites.h"
#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

extern void Client_RegisterData(EngineData*, const vector<uint8>&);
extern void InitClientEngine(FOClient*);

#if FO_ANGELSCRIPT_SCRIPTING
extern void Init_AngelScript_ClientScriptSystem(BaseEngine*);
#endif

static void InitClientEngineData(EngineData* engine, const GlobalSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    FileSystem resources;
    resources.AddDataSource(strex(settings.ClientResources).combinePath("Core"));

    if (const auto restore_info = resources.ReadFile("EngineData.fobin")) {
        Client_RegisterData(engine, restore_info.GetData());
    }
    else {
        throw EngineDataNotFoundException(FO_LINE_STR);
    }
}

FOClient::FOClient(GlobalSettings& settings, AppWindow* window, const EngineDataRegistrator& mapper_registrator) :
    BaseEngine(
        settings,
        mapper_registrator ? PropertiesRelationType::BothRelative : PropertiesRelationType::ClientRelative, //
        mapper_registrator ? mapper_registrator : [&] { InitClientEngineData(this, settings); }),
    EffectMngr(Settings, Resources),
    SprMngr(Settings, window, Resources, GameTime, EffectMngr, Hashes),
    ResMngr(Resources, SprMngr, *this),
    SndMngr(Settings, Resources),
    Keyb(Settings, SprMngr),
    Cache(Settings.CacheResources),
    _conn(Settings)
{
    FO_STACK_TRACE_ENTRY();

    Resources.AddDataSource(Settings.EmbeddedResources);

    for (const auto& entry : Settings.ClientResourceEntries) {
        Resources.AddDataSource(strex(Settings.ClientResources).combinePath(entry));
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
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<DefaultSpriteFactory>(SprMngr));
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ParticleSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, Hashes));
#if FO_ENABLE_3D
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ModelSpriteFactory>(SprMngr, Settings, EffectMngr, GameTime, Hashes, *this, *this));
#endif

    SprMngr.InitializeEgg(Hashes.ToHashedString("TransparentEgg.png"), AtlasType::MapSprites);
    ResMngr.IndexFiles();

    Settings.MousePos = App->Input.GetMousePosition();

    ScriptSys.MapEngineEntityType<PlayerView>(PlayerView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<ItemView>(ItemView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<CritterView>(CritterView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<MapView>(MapView::ENTITY_TYPE_NAME);
    ScriptSys.MapEngineEntityType<LocationView>(LocationView::ENTITY_TYPE_NAME);

    if (mapper_registrator) {
        return;
    }

#if FO_ANGELSCRIPT_SCRIPTING
    Init_AngelScript_ClientScriptSystem(this);
#endif

    _curLang = LanguagePack {Settings.Language, *this};
    _curLang.LoadFromResources(Resources);

    ProtoMngr.LoadFromResources(Resources);

    // Modules initialization
    InitClientEngine(this);

    ScriptSys.InitModules();

    OnStart.Fire();

    // Connection handlers
    _conn.SetConnectHandler([this](ClientConnection::ConnectResult result) { Net_OnConnect(result); });
    _conn.SetDisconnectHandler([this] { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NetMessage::LoginSuccess, [this] { Net_OnLoginSuccess(); });
    _conn.AddMessageHandler(NetMessage::RegisterSuccess, [this] { Net_OnRegisterSuccess(); });
    _conn.AddMessageHandler(NetMessage::PlaceToGameComplete, [this] { Net_OnPlaceToGameComplete(); });
    _conn.AddMessageHandler(NetMessage::AddCritter, [this] { Net_OnAddCritter(); });
    _conn.AddMessageHandler(NetMessage::RemoveCritter, [this] { Net_OnRemoveCritter(); });
    _conn.AddMessageHandler(NetMessage::CritterAction, [this] { Net_OnCritterAction(); });
    _conn.AddMessageHandler(NetMessage::CritterMoveItem, [this] { Net_OnCritterMoveItem(); });
    _conn.AddMessageHandler(NetMessage::CritterAnimate, [this] { Net_OnCritterAnimate(); });
    _conn.AddMessageHandler(NetMessage::CritterSetAnims, [this] { Net_OnCritterSetAnims(); });
    _conn.AddMessageHandler(NetMessage::CritterTeleport, [this] { Net_OnCritterTeleport(); });
    _conn.AddMessageHandler(NetMessage::CritterMove, [this] { Net_OnCritterMove(); });
    _conn.AddMessageHandler(NetMessage::CritterMoveSpeed, [this] { Net_OnCritterMoveSpeed(); });
    _conn.AddMessageHandler(NetMessage::CritterDir, [this] { Net_OnCritterDir(); });
    _conn.AddMessageHandler(NetMessage::CritterPos, [this] { Net_OnCritterPos(); });
    _conn.AddMessageHandler(NetMessage::CritterAttachments, [this] { Net_OnCritterAttachments(); });
    _conn.AddMessageHandler(NetMessage::Property, [this] { Net_OnProperty(); });
    _conn.AddMessageHandler(NetMessage::InfoMessage, [this] { Net_OnInfoMessage(); });
    _conn.AddMessageHandler(NetMessage::ChosenAddItem, [this] { Net_OnChosenAddItem(); });
    _conn.AddMessageHandler(NetMessage::ChosenRemoveItem, [this] { Net_OnChosenRemoveItem(); });
    _conn.AddMessageHandler(NetMessage::TimeSync, [this] { Net_OnTimeSync(); });
    _conn.AddMessageHandler(NetMessage::ViewMap, [this] { Net_OnViewMap(); });
    _conn.AddMessageHandler(NetMessage::LoadMap, [this] { Net_OnLoadMap(); });
    _conn.AddMessageHandler(NetMessage::SomeItems, [this] { Net_OnSomeItems(); });
    _conn.AddMessageHandler(NetMessage::RemoteCall, [this] { Net_OnRemoteCall(); });
    _conn.AddMessageHandler(NetMessage::AddItemOnMap, [this] { Net_OnAddItemOnMap(); });
    _conn.AddMessageHandler(NetMessage::RemoveItemFromMap, [this] { Net_OnRemoveItemFromMap(); });
    _conn.AddMessageHandler(NetMessage::AnimateItem, [this] { Net_OnAnimateItem(); });
    _conn.AddMessageHandler(NetMessage::Effect, [this] { Net_OnEffect(); });
    _conn.AddMessageHandler(NetMessage::FlyEffect, [this] { Net_OnFlyEffect(); });
    _conn.AddMessageHandler(NetMessage::PlaySound, [this] { Net_OnPlaySound(); });
    _conn.AddMessageHandler(NetMessage::InitData, [this] { Net_OnInitData(); });
    _conn.AddMessageHandler(NetMessage::AddCustomEntity, [this] { Net_OnAddCustomEntity(); });
    _conn.AddMessageHandler(NetMessage::RemoveCustomEntity, [this] { Net_OnRemoveCustomEntity(); });

    // Properties that sending to clients
    {
        const auto set_send_callbacks = [](const auto* registrator, const PropertyPostSetCallback& callback) {
            for (size_t i = 0; i < registrator->GetPropertiesCount(); i++) {
                const auto* prop = registrator->GetPropertyByIndex(numeric_cast<int32>(i));

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
        const auto set_callback = [](const auto* registrator, int32 prop_index, PropertyPostSetCallback callback) {
            const auto* prop = registrator->GetPropertyByIndex(prop_index);
            prop->AddPostSetter(std::move(callback));
        };

        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LookDistance_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterLookDistance(entity, prop); });
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::ModelName_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterModelName(entity, prop); });
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::ContourColor_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterContourColor(entity, prop); });
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::HideSprite_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterHideSprite(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::Colorize_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemFlags(entity, prop); });
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::ColorizeColor_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemFlags(entity, prop); });
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
}

FOClient::~FOClient()
{
    FO_STACK_TRACE_ENTRY();

    safe_call([] { App->Render.SetRenderTarget(nullptr); });

    safe_call([this] {
        _conn.SetConnectHandler(nullptr);
        _conn.SetDisconnectHandler(nullptr);
        _conn.Disconnect();
    });

    _chosen = nullptr;

    if (_curMap) {
        safe_call([this] { _curMap->DestroySelf(); });
        _curMap = nullptr;
    }

    if (_curLocation) {
        safe_call([this] { _curLocation->DestroySelf(); });
        _curLocation = nullptr;
    }

    if (_curPlayer) {
        safe_call([this] { _curPlayer->DestroySelf(); });
        _curPlayer = nullptr;
    }

    safe_call([this] {
        for (auto& cr : _globalMapCritters) {
            cr->DestroySelf();
        }
    });

    _globalMapCritters.clear();

    safe_call([this] { DestroyInnerEntities(); });
}

auto FOClient::ResolveCritterAnimation(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32& pass, uint32& flags, int32& ox, int32& oy, string& anim_name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return OnCritterAnimation.Fire(model_name, state_anim, action_anim, pass, flags, ox, oy, anim_name);
}

auto FOClient::ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return OnCritterAnimationSubstitute.Fire(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim);
}

auto FOClient::ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32& f_state_anim, int32& f_action_anim, int32& f_state_anim_ex, int32& f_action_anim_ex, uint32& flags) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return OnCritterAnimationFallout.Fire(model_name, state_anim, action_anim, f_state_anim, f_action_anim, f_state_anim_ex, f_action_anim_ex, flags);
}

auto FOClient::IsConnecting() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _conn.IsConnecting();
}

auto FOClient::IsConnected() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _conn.IsConnected();
}

auto FOClient::GetChosen() noexcept -> CritterView*
{
    FO_STACK_TRACE_ENTRY();

    if (_chosen && _chosen->IsDestroyed()) {
        _chosen = nullptr;
    }

    return _chosen.get();
}

auto FOClient::GetMapChosen() noexcept -> CritterHexView*
{
    FO_STACK_TRACE_ENTRY();

    return dynamic_cast<CritterHexView*>(GetChosen());
}

auto FOClient::GetGlobalMapCritter(ident_t cr_id) -> CritterView*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = std::ranges::find_if(_globalMapCritters, [cr_id](auto&& cr) { return cr->GetId() == cr_id; });
    return it != _globalMapCritters.end() ? it->get() : nullptr;
}

void FOClient::MainLoop()
{
    FO_STACK_TRACE_ENTRY();

    FrameAdvance();

#if FO_TRACY
    TracyPlot("Client FPS", numeric_cast<int64>(GameTime.GetFramesPerSecond()));
#endif

    // Network
    if (_initNetReason != INIT_NET_REASON_NONE && !_conn.IsConnecting() && !_conn.IsConnected()) {
        OnConnecting.Fire();
        _conn.Connect();
    }

    _conn.Process();
    ProcessInputEvents();
    ScriptSys.Process();
    TimeEventMngr.ProcessTimeEvents();
    OnLoop.Fire();

    if (_curMap != nullptr) {
        _curMap->Process();
    }

    App->MainWindow.GrabInput(_curMap != nullptr && _curMap->IsScrollEnabled());

    // Render
    EffectMngr.UpdateEffects(GameTime);

    {
        SprMngr.BeginScene();

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

        SprMngr.EndScene();
    }
}

void FOClient::ScreenFade(timespan time, ucolor from_color, ucolor to_color, bool push_back)
{
    FO_STACK_TRACE_ENTRY();

    if (!push_back || _screenFadingEffects.empty()) {
        _screenFadingEffects.push_back({GameTime.GetFrameTime(), time, from_color, to_color});
    }
    else {
        nanotime last_tick;

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
    FO_STACK_TRACE_ENTRY();

    SprMngr.Flush();

    vector<PrimitivePoint> full_screen_quad;
    SprMngr.PrepareSquare(full_screen_quad, IRect(0, 0, Settings.ScreenWidth, Settings.ScreenHeight), ucolor::clear);

    for (auto it = _screenFadingEffects.begin(); it != _screenFadingEffects.end();) {
        auto& screen_effect = *it;

        if (GameTime.GetFrameTime() >= screen_effect.BeginTime + screen_effect.Duration) {
            it = _screenFadingEffects.erase(it);
            continue;
        }

        if (GameTime.GetFrameTime() >= screen_effect.BeginTime) {
            const auto proc = GenericUtils::Percent(screen_effect.Duration.to_ms<int32>(), (GameTime.GetFrameTime() - screen_effect.BeginTime).to_ms<int32>()) + 1;
            int32 res[4];

            for (auto i = 0; i < 4; i++) {
                const int32 sc = (reinterpret_cast<uint8*>(&screen_effect.StartColor))[i];
                const int32 ec = (reinterpret_cast<uint8*>(&screen_effect.EndColor))[i];
                const auto dc = ec - sc;
                res[i] = sc + dc * numeric_cast<int32>(proc) / 100;
            }

            const auto color = ucolor {numeric_cast<uint8>(res[0]), numeric_cast<uint8>(res[1]), numeric_cast<uint8>(res[2]), numeric_cast<uint8>(res[3])};

            for (auto i = 0; i < 6; i++) {
                full_screen_quad[i].PointColor = color;
            }

            SprMngr.DrawPoints(full_screen_quad, RenderPrimitiveType::TriangleList);
        }

        ++it;
    }
}

void FOClient::ScreenQuake(int32 noise, timespan time)
{
    FO_STACK_TRACE_ENTRY();

    Settings.ScreenOffset.x -= iround<int32>(_quakeScreenOffsX);
    Settings.ScreenOffset.y -= iround<int32>(_quakeScreenOffsY);

    _quakeScreenOffsX = numeric_cast<float32>(GenericUtils::Random(0, 1) != 0 ? noise : -noise);
    _quakeScreenOffsY = numeric_cast<float32>(GenericUtils::Random(0, 1) != 0 ? noise : -noise);
    _quakeScreenOffsStep = std::fabs(_quakeScreenOffsX) / (time.to_ms<float32>() / 30.0f);

    Settings.ScreenOffset.x += iround<int32>(_quakeScreenOffsX);
    Settings.ScreenOffset.y += iround<int32>(_quakeScreenOffsY);

    _quakeScreenOffsNextTime = GameTime.GetFrameTime() + std::chrono::milliseconds {30};
}

void FOClient::ProcessScreenEffectQuake()
{
    FO_STACK_TRACE_ENTRY();

    if ((_quakeScreenOffsX != 0.0f || _quakeScreenOffsY != 0.0f) && GameTime.GetFrameTime() >= _quakeScreenOffsNextTime) {
        Settings.ScreenOffset.x -= iround<int32>(_quakeScreenOffsX);
        Settings.ScreenOffset.y -= iround<int32>(_quakeScreenOffsY);

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

        Settings.ScreenOffset.x += iround<int32>(_quakeScreenOffsX);
        Settings.ScreenOffset.y += iround<int32>(_quakeScreenOffsY);

        _quakeScreenOffsNextTime = GameTime.GetFrameTime() + std::chrono::milliseconds {30};
    }
}

void FOClient::ProcessInputEvents()
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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

void FOClient::Net_OnConnect(ClientConnection::ConnectResult result)
{
    FO_STACK_TRACE_ENTRY();

    if (result == ClientConnection::ConnectResult::Success) {
        // After connect things
        if (_initNetReason == INIT_NET_REASON_LOGIN) {
            Net_SendLogIn();
        }
        else if (_initNetReason == INIT_NET_REASON_REG) {
            Net_SendCreatePlayer();
        }
        else if (_initNetReason == INIT_NET_REASON_LOAD) {
            // Net_SendSaveLoad( false, SaveLoadFileName.c_str(), nullptr );
        }
        else if (_initNetReason != INIT_NET_REASON_CUSTOM) {
            FO_UNREACHABLE_PLACE();
        }

        FO_RUNTIME_ASSERT(!_curPlayer);
        _curPlayer = SafeAlloc::MakeRefCounted<PlayerView>(this, ident_t {});

        OnConnected.Fire();
    }
    else if (result == ClientConnection::ConnectResult::Outdated) {
        throw ResourcesOutdatedException("Binary outdated");
    }
    else {
        OnConnectingFailed.Fire();
    }
}

void FOClient::Net_OnDisconnect()
{
    FO_STACK_TRACE_ENTRY();

    UnloadMap();

    if (_curPlayer) {
        _curPlayer->DestroySelf();
        _curPlayer = nullptr;
    }

    DestroyInnerEntities();

    _initNetReason = INIT_NET_REASON_NONE;
    OnDisconnected.Fire();
}

void FOClient::Net_SendLogIn()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    WriteLog("Player login");

    _conn.OutBuf.StartMsg(NetMessage::Login);
    _conn.OutBuf.Write(_loginName);
    _conn.OutBuf.Write(_loginPassword);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendCreatePlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    WriteLog("Player registration");

    _conn.OutBuf.StartMsg(NetMessage::Register);
    _conn.OutBuf.Write(_loginName);
    _conn.OutBuf.Write(_loginPassword);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendDir(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _conn.OutBuf.StartMsg(NetMessage::SendCritterDir);
    _conn.OutBuf.Write(_curMap->GetId());
    _conn.OutBuf.Write(cr->GetId());
    _conn.OutBuf.Write(cr->GetDirAngle());
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendMove(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    FO_RUNTIME_ASSERT(!cr->Moving.Steps.empty());

    if (cr->Moving.Steps.size() > 500) {
        BreakIntoDebugger();
        cr->ClearMove();
        return;
    }

    _conn.OutBuf.StartMsg(NetMessage::SendCritterMove);
    _conn.OutBuf.Write(_curMap->GetId());
    _conn.OutBuf.Write(cr->GetId());
    _conn.OutBuf.Write(cr->Moving.Speed);
    _conn.OutBuf.Write(cr->Moving.StartHex);
    _conn.OutBuf.Write(numeric_cast<uint16>(cr->Moving.Steps.size()));
    for (const auto step : cr->Moving.Steps) {
        _conn.OutBuf.Write(step);
    }
    _conn.OutBuf.Write(numeric_cast<uint16>(cr->Moving.ControlSteps.size()));
    for (const auto control_step : cr->Moving.ControlSteps) {
        _conn.OutBuf.Write(control_step);
    }
    _conn.OutBuf.Write(cr->Moving.EndHexOffset);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendStopMove(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _conn.OutBuf.StartMsg(NetMessage::SendStopCritterMove);
    _conn.OutBuf.Write(_curMap->GetId());
    _conn.OutBuf.Write(cr->GetId());
    _conn.OutBuf.Write(cr->GetHex());
    _conn.OutBuf.Write(cr->GetHexOffset());
    _conn.OutBuf.Write(cr->GetDirAngle());
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_SendProperty(NetProperty type, const Property* prop, Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
    FO_RUNTIME_ASSERT(entity);

    const auto& props = entity->GetProperties();
    props.ValidateForRawData(prop);
    const auto prop_raw_data = props.GetRawData(prop);

    _conn.OutBuf.StartMsg(NetMessage::SendProperty);
    _conn.OutBuf.Write(numeric_cast<uint32>(prop_raw_data.size()));
    _conn.OutBuf.Write(type);

    switch (type) {
    case NetProperty::CritterItem: {
        auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        FO_RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(dynamic_cast<ItemView*>(client_entity)->GetCritterId());
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    case NetProperty::Critter: {
        const auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        FO_RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    case NetProperty::MapItem: {
        const auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        FO_RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    case NetProperty::ChosenItem: {
        const auto* client_entity = dynamic_cast<ClientEntity*>(entity);
        FO_RUNTIME_ASSERT(client_entity);
        _conn.OutBuf.Write(client_entity->GetId());
    } break;
    default:
        break;
    }

    _conn.OutBuf.Write(prop->GetRegIndex());
    _conn.OutBuf.Push(prop_raw_data);
    _conn.OutBuf.EndMsg();
}

void FOClient::Net_OnInitData()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size = _conn.InBuf.Read<uint32>();

    vector<uint8> data;
    data.resize(data_size);
    _conn.InBuf.Pop(data.data(), data_size);

    _conn.InBuf.ReadPropsData(_globalsPropertiesData);
    const auto time = _conn.InBuf.Read<synctime>();

    RestoreData(_globalsPropertiesData);
    GameTime.SetSynchronizedTime(time);
    SetSynchronizedTime(time);

    if (!data.empty()) {
        FileSystem resources;

        resources.AddDataSource(Settings.ClientResources, DataSourceType::DirRoot);

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

            ignore_unused(hash);

            // Check size
            if (auto file = resources.ReadFileHeader(fname)) {
                if (file.GetSize() == size) {
                    continue;
                }
            }

            throw ResourcesOutdatedException("Resource pack outdated", fname);
        }

        reader.VerifyEnd();
    }
}

void FOClient::Net_OnRegisterSuccess()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Registration success");

    OnRegistrationSuccess.Fire();
}

void FOClient::Net_OnLoginSuccess()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Authentication success");

    const auto player_id = _conn.InBuf.Read<ident_t>();
    _conn.InBuf.ReadPropsData(_globalsPropertiesData);
    _conn.InBuf.ReadPropsData(_playerPropertiesData);

    RestoreData(_globalsPropertiesData);

    FO_RUNTIME_ASSERT(_curPlayer);
    FO_RUNTIME_ASSERT(!_curPlayer->GetId());
    _curPlayer->SetId(player_id, true);
    _curPlayer->RestoreData(_playerPropertiesData);

    FO_RUNTIME_ASSERT(!HasInnerEntities());
    ReceiveCustomEntities(this);

    OnLoginSuccess.Fire();
}

void FOClient::Net_OnAddCritter()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto pid = _conn.InBuf.Read<hstring>(Hashes);
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

    refcount_ptr<CritterView> cr;
    CritterHexView* hex_cr;

    if (_curMap != nullptr) {
        hex_cr = _curMap->AddReceivedCritter(cr_id, pid, hex, dir_angle, _tempPropertiesData);
        FO_RUNTIME_ASSERT(hex_cr);

        if (_mapLoaded) {
            hex_cr->FadeUp();
        }

        cr = hex_cr;
    }
    else {
        const auto* proto = ProtoMngr.GetProtoCritter(pid);
        const auto it = std::ranges::find_if(_globalMapCritters, [cr_id](auto&& cr2) { return cr2->GetId() == cr_id; });

        if (it != _globalMapCritters.end()) {
            BreakIntoDebugger();
            (*it)->MarkAsDestroyed();
            _globalMapCritters.erase(it);
        }

        cr = SafeAlloc::MakeRefCounted<CritterView>(this, cr_id, proto);
        cr->RestoreData(_tempPropertiesData);
        _globalMapCritters.emplace_back(cr);

        hex_cr = nullptr;
    }

    ReceiveCustomEntities(cr.get());

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
    const auto items_count = _conn.InBuf.Read<uint32>();

    for (uint32 i = 0; i < items_count; i++) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(Hashes);
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

    if (_curMap != nullptr) {
        if (is_attached) {
            for (auto& map_cr : _curMap->GetCritters()) {
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

    // Initial moving
    const auto is_moving = _conn.InBuf.Read<bool>();

    if (is_moving) {
        ReceiveCritterMoving(hex_cr);
    }

    if (hex_offset != ipos16 {} && hex_cr != nullptr) {
        hex_cr->RefreshOffs();
    }

    if (is_chosen) {
        _chosen = cr;
    }

    OnCritterIn.Fire(cr.get());

    if (hex_cr != nullptr) {
#if FO_ENABLE_3D
        if (hex_cr->IsModel()) {
            hex_cr->GetModel()->PrewarmParticles();
            hex_cr->GetModel()->StartMeshGeneration();
        }
#endif

        if (items_count != 0) {
            _curMap->UpdateCritterLightSource(hex_cr);
        }

        if (is_chosen) {
            _curMap->RebuildFog();
        }

        if (!hex_cr->IsAnim()) {
            hex_cr->AnimateStay();
        }
    }
}

void FOClient::Net_OnRemoveCritter()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();

    if (_curMap != nullptr) {
        auto* cr = _curMap->GetCritter(cr_id);

        if (cr == nullptr) {
            BreakIntoDebugger();
            return;
        }

        cr->Finish();

        OnCritterOut.Fire(cr);

        if (_chosen == cr) {
            _chosen = nullptr;
        }
    }
    else {
        const auto it = std::ranges::find_if(_globalMapCritters, [cr_id](auto&& cr) { return cr->GetId() == cr_id; });

        if (it == _globalMapCritters.end()) {
            BreakIntoDebugger();
            return;
        }

        auto cr = copy(*it);

        OnCritterOut.Fire(cr.get());
        _globalMapCritters.erase(it);

        if (_chosen == cr) {
            _chosen = nullptr;
        }

        cr->DestroySelf();
    }
}

void FOClient::Net_OnInfoMessage()
{
    FO_STACK_TRACE_ENTRY();

    const auto info_message = _conn.InBuf.Read<EngineInfoMessage>();
    const auto extra_text = _conn.InBuf.Read<string>();

    OnInfoMessage.Fire(info_message, extra_text);
}

void FOClient::Net_OnCritterDir()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto dir_angle = _conn.InBuf.Read<int16>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = _curMap->GetCritter(cr_id);

    if (cr != nullptr) {
        cr->ChangeLookDirAngle(dir_angle);
    }
}

void FOClient::Net_OnCritterMove()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    auto* cr = _curMap->GetCritter(cr_id);

    ReceiveCritterMoving(cr);

    if (cr != nullptr) {
        cr->AnimateStay();
    }
}

void FOClient::Net_OnCritterMoveSpeed()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto speed = _conn.InBuf.Read<uint16>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = _curMap->GetCritter(cr_id);

    if (cr == nullptr) {
        return;
    }
    if (!cr->IsMoving()) {
        return;
    }
    if (speed == cr->Moving.Speed) {
        return;
    }

    const auto diff = numeric_cast<float32>(speed) / numeric_cast<float32>(cr->Moving.Speed);
    const auto elapsed_time = (GameTime.GetFrameTime() - cr->Moving.StartTime + cr->Moving.OffsetTime).to_ms<float32>();

    cr->Moving.WholeTime /= diff;
    cr->Moving.StartTime = GameTime.GetFrameTime();
    cr->Moving.OffsetTime = std::chrono::milliseconds {iround<int32>(elapsed_time / diff)};
    cr->Moving.Speed = speed;
    cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);

    cr->AnimateStay();
}

void FOClient::Net_OnCritterAction()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto action = _conn.InBuf.Read<CritterAction>();
    const auto action_ext = _conn.InBuf.Read<int32>();
    const auto is_context_item = _conn.InBuf.Read<bool>();

    refcount_ptr<ItemView> context_item;

    if (is_context_item) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(Hashes);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);

        const auto* proto = ProtoMngr.GetProtoItem(item_pid);
        context_item = SafeAlloc::MakeRefCounted<ItemView>(this, item_id, proto);
        context_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(context_item.get());
    }

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = _curMap->GetCritter(cr_id);

    if (cr == nullptr) {
        BreakIntoDebugger();
        return;
    }

    cr->Action(action, action_ext, context_item.get(), false);

    if (context_item) {
        context_item->MarkAsDestroyed();
    }
}

void FOClient::Net_OnCritterMoveItem()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto action = _conn.InBuf.Read<CritterAction>();
    const auto prev_slot = _conn.InBuf.Read<CritterItemSlot>();
    const auto cur_slot = _conn.InBuf.Read<CritterItemSlot>();
    const auto is_moved_item = _conn.InBuf.Read<bool>();

    refcount_ptr<ItemView> moved_item;

    if (is_moved_item) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(Hashes);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);

        const auto* proto = ProtoMngr.GetProtoItem(item_pid);
        moved_item = SafeAlloc::MakeRefCounted<ItemView>(this, item_id, proto);
        moved_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(moved_item.get());
    }

    CritterView* cr;

    if (_curMap != nullptr) {
        cr = _curMap->GetCritter(cr_id);
    }
    else {
        cr = GetGlobalMapCritter(cr_id);
    }

    if (cr == nullptr) {
        BreakIntoDebugger();

        // Skip rest data
        const auto items_count = _conn.InBuf.Read<uint32>();

        for (uint32 i = 0; i < items_count; i++) {
            (void)_conn.InBuf.Read<ident_t>();
            (void)_conn.InBuf.Read<hstring>(Hashes);
            (void)_conn.InBuf.Read<CritterItemSlot>();
            _conn.InBuf.ReadPropsData(_tempPropertiesData);
            ReceiveCustomEntities(nullptr);
        }

        return;
    }

    // Todo: rework critters inventory updating
    const auto items_count = _conn.InBuf.Read<uint32>();

    if (items_count != 0) {
        FO_RUNTIME_ASSERT(!cr->GetIsChosen());

        cr->DeleteAllInvItems();

        for (uint32 i = 0; i < items_count; i++) {
            const auto item_id = _conn.InBuf.Read<ident_t>();
            const auto item_pid = _conn.InBuf.Read<hstring>(Hashes);
            const auto item_slot = _conn.InBuf.Read<CritterItemSlot>();
            _conn.InBuf.ReadPropsData(_tempPropertiesData);

            const auto* proto = ProtoMngr.GetProtoItem(item_pid);
            auto* item = cr->AddReceivedInvItem(item_id, proto, item_slot, _tempPropertiesData);

            ReceiveCustomEntities(item);
        }
    }

    if (auto* hex_cr = dynamic_cast<CritterHexView*>(cr); hex_cr != nullptr) {
        hex_cr->Action(action, static_cast<int32>(prev_slot), moved_item.get(), false);
    }

    if (moved_item && cur_slot != prev_slot && cr->GetIsChosen()) {
        if (auto* item = cr->GetInvItem(moved_item->GetId()); item != nullptr) {
            item->SetCritterSlot(cur_slot);
            moved_item->SetCritterSlot(prev_slot);

            OnItemInvChanged.Fire(item, moved_item.get());
        }
    }

    if (const auto* hex_cr = dynamic_cast<CritterHexView*>(cr); hex_cr != nullptr) {
        _curMap->UpdateCritterLightSource(hex_cr);
    }
}

void FOClient::Net_OnCritterAnimate()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto state_anim = _conn.InBuf.Read<CritterStateAnim>();
    const auto action_anim = _conn.InBuf.Read<CritterActionAnim>();
    const auto clear_sequence = _conn.InBuf.Read<bool>();
    const auto delay_play = _conn.InBuf.Read<bool>();
    const auto is_context_item = _conn.InBuf.Read<bool>();

    refcount_ptr<ItemView> context_item;

    if (is_context_item) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto item_pid = _conn.InBuf.Read<hstring>(Hashes);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);

        const auto* proto = ProtoMngr.GetProtoItem(item_pid);
        context_item = SafeAlloc::MakeRefCounted<ItemView>(this, item_id, proto);
        context_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(context_item.get());
    }

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = _curMap->GetCritter(cr_id);

    if (cr == nullptr) {
        return;
    }

    if (clear_sequence) {
        cr->ClearAnim();
    }
    if (delay_play || !cr->IsAnim()) {
        cr->Animate(state_anim, action_anim, context_item.get());
    }

    if (context_item) {
        context_item->MarkAsDestroyed();
    }
}

void FOClient::Net_OnCritterSetAnims()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto cond = _conn.InBuf.Read<CritterCondition>();
    const auto state_anim = _conn.InBuf.Read<CritterStateAnim>();
    const auto action_anim = _conn.InBuf.Read<CritterActionAnim>();

    CritterView* cr;
    CritterHexView* hex_cr;

    if (_curMap != nullptr) {
        hex_cr = _curMap->GetCritter(cr_id);
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
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto to_hex = _conn.InBuf.Read<mpos>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* cr = _curMap->GetCritter(cr_id);
    if (cr == nullptr) {
        return;
    }

    _curMap->MoveCritter(cr, to_hex, false);

    if (cr->GetIsChosen()) {
        if (_curMap->AutoScroll.HardLockedCritter == cr->GetId() || _curMap->AutoScroll.SoftLockedCritter == cr->GetId()) {
            _curMap->AutoScroll.CritterLastHex = cr->GetHex();
        }

        _curMap->ScrollToHex(cr->GetHex(), 0.1f, false);
    }
}

void FOClient::Net_OnCritterPos()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto hex_offset = _conn.InBuf.Read<ipos16>();
    const auto dir_angle = _conn.InBuf.Read<int16>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    FO_RUNTIME_ASSERT(_curMap->GetSize().IsValidPos(hex));

    auto* cr = _curMap->GetCritter(cr_id);

    if (cr == nullptr) {
        return;
    }

    cr->ClearMove();

    cr->ChangeLookDirAngle(dir_angle);
    cr->ChangeMoveDirAngle(dir_angle);

    if (cr->GetHex() != hex) {
        _curMap->MoveCritter(cr, hex, true);

        if (cr->GetIsChosen()) {
            _curMap->RebuildFog();
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
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf.Read<ident_t>();
    const auto is_attached = _conn.InBuf.Read<bool>();
    const auto attach_master = _conn.InBuf.Read<ident_t>();

    const auto attached_critters_count = _conn.InBuf.Read<uint16>();
    vector<ident_t> attached_critters;
    attached_critters.resize(attached_critters_count);
    for (uint16 i = 0; i < attached_critters_count; i++) {
        attached_critters[i] = _conn.InBuf.Read<ident_t>();
    }

    if (_curMap != nullptr) {
        auto* cr = _curMap->GetCritter(cr_id);
        if (cr == nullptr) {
            BreakIntoDebugger();
            return;
        }

        cr->SetAttachMaster(attach_master);

        if (cr->GetIsAttached() != is_attached) {
            cr->SetIsAttached(is_attached);

            if (is_attached) {
                for (auto& map_cr : _curMap->GetCritters()) {
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
    FO_STACK_TRACE_ENTRY();

    const auto item_id = _conn.InBuf.Read<ident_t>();
    const auto item_pid = _conn.InBuf.Read<hstring>(Hashes);
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

    if (_curMap != nullptr) {
        _curMap->RebuildFog();

        if (const auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr) {
            _curMap->UpdateCritterLightSource(hex_chosen);
        }
    }

    OnItemInvIn.Fire(item);
}

void FOClient::Net_OnChosenRemoveItem()
{
    FO_STACK_TRACE_ENTRY();

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

    auto item_clone = item->CreateRefClone();

    chosen->DeleteInvItem(item, true);

    OnItemInvOut.Fire(item_clone.get());

    if (_curMap != nullptr) {
        if (const auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr) {
            _curMap->UpdateCritterLightSource(hex_chosen);
        }
    }
}

void FOClient::Net_OnAddItemOnMap()
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = _conn.InBuf.Read<mpos>();
    const auto item_id = _conn.InBuf.Read<ident_t>();
    const auto item_pid = _conn.InBuf.Read<hstring>(Hashes);
    _conn.InBuf.ReadPropsData(_tempPropertiesData);

    if (_curMap == nullptr) {
        BreakIntoDebugger();

        // Skip rest data
        ReceiveCustomEntities(nullptr);

        return;
    }

    auto* item = _curMap->AddReceivedItem(item_id, item_pid, hex, _tempPropertiesData);
    FO_RUNTIME_ASSERT(item);

    ReceiveCustomEntities(item);

    if (_mapLoaded) {
        item->FadeUp();
    }

    OnItemMapIn.Fire(item);
}

void FOClient::Net_OnRemoveItemFromMap()
{
    FO_STACK_TRACE_ENTRY();

    const auto item_id = _conn.InBuf.Read<ident_t>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* item = _curMap->GetItem(item_id);
    if (item != nullptr) {
        OnItemMapOut.Fire(item);

        // Refresh borders
        if (!item->GetShootThru()) {
            _curMap->RebuildFog();
        }

        item->Finish();
    }
}

void FOClient::Net_OnAnimateItem()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto item_id = _conn.InBuf.Read<ident_t>();
    const auto anim_name = _conn.InBuf.Read<hstring>(Hashes);
    const auto looped = _conn.InBuf.Read<bool>();
    const auto reversed = _conn.InBuf.Read<bool>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    auto* item = _curMap->GetItem(item_id);

    if (item != nullptr) {
        item->GetAnim()->Play(anim_name, looped, reversed);
    }
}

void FOClient::Net_OnEffect()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto eff_pid = _conn.InBuf.Read<hstring>(Hashes);
    const auto hex = _conn.InBuf.Read<mpos>();
    const auto radius = _conn.InBuf.Read<uint16>();
    FO_RUNTIME_ASSERT(radius < MAX_HEX_OFFSET);

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    _curMap->RunEffectItem(eff_pid, hex, hex);

    const auto [sx, sy] = Geometry.GetHexOffsets(hex);
    const auto count = GenericUtils::NumericalNumber(radius) * GameSettings::MAP_DIR_COUNT;

    for (int32 i = 0; i < count; i++) {
        const auto ex = numeric_cast<int16>(hex.x) + sx[i];
        const auto ey = numeric_cast<int16>(hex.y) + sy[i];

        if (_curMap->GetSize().IsValidPos(ipos {ex, ey})) {
            _curMap->RunEffectItem(eff_pid, {numeric_cast<uint16>(ex), numeric_cast<uint16>(ey)}, {numeric_cast<uint16>(ex), numeric_cast<uint16>(ey)});
        }
    }
}

void FOClient::Net_OnFlyEffect()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto eff_pid = _conn.InBuf.Read<hstring>(Hashes);
    const auto eff_cr1_id = _conn.InBuf.Read<ident_t>();
    const auto eff_cr2_id = _conn.InBuf.Read<ident_t>();

    auto eff_cr1_hex = _conn.InBuf.Read<mpos>();
    auto eff_cr2_hex = _conn.InBuf.Read<mpos>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    const auto* cr1 = _curMap->GetCritter(eff_cr1_id);
    if (cr1 != nullptr) {
        eff_cr1_hex = cr1->GetHex();
    }

    const auto* cr2 = _curMap->GetCritter(eff_cr2_id);
    if (cr2 != nullptr) {
        eff_cr2_hex = cr2->GetHex();
    }

    _curMap->RunEffectItem(eff_pid, eff_cr1_hex, eff_cr2_hex);
}

void FOClient::Net_OnPlaySound()
{
    FO_STACK_TRACE_ENTRY();

    // Todo: synchronize effects showing (for example shot and kill)
    [[maybe_unused]] const auto synchronize_cr_id = _conn.InBuf.Read<ident_t>();
    const auto sound_name = _conn.InBuf.Read<string>();

    SndMngr.PlaySound(ResMngr.GetSoundNames(), sound_name);
}

void FOClient::Net_OnPlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    _mapLoaded = true;

    auto* chosen = GetChosen();

    if (_curMap != nullptr && chosen != nullptr) {
        _curMap->FindSetCenter(chosen->GetHex());

        if (auto* hex_chosen = dynamic_cast<CritterHexView*>(chosen); hex_chosen != nullptr) {
            hex_chosen->AnimateStay();
            _curMap->UpdateCritterLightSource(hex_chosen);
        }
    }

    OnMapLoaded.Fire();

    WriteLog("Map loaded");
}

void FOClient::Net_OnProperty()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size = _conn.InBuf.Read<uint32>();
    const auto type = _conn.InBuf.Read<NetProperty>();

    ident_t cr_id;
    ident_t item_id;
    ident_t entity_id;

    switch (type) {
    case NetProperty::CritterItem:
        cr_id = _conn.InBuf.Read<ident_t>();
        item_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::Critter:
        cr_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::MapItem:
        item_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::ChosenItem:
        item_id = _conn.InBuf.Read<ident_t>();
        break;
    case NetProperty::CustomEntity:
        entity_id = _conn.InBuf.Read<ident_t>();
        break;
    default:
        break;
    }

    const auto property_index = _conn.InBuf.Read<uint16>();

    PropertyRawData prop_data;
    _conn.InBuf.Pop(prop_data.Alloc(data_size), data_size);

    Entity* entity = nullptr;

    switch (type) {
    case NetProperty::Game:
        entity = this;
        break;
    case NetProperty::Player:
        entity = _curPlayer.get();
        break;
    case NetProperty::Critter:
        entity = _curMap != nullptr ? _curMap->GetCritter(cr_id) : nullptr;
        break;
    case NetProperty::Chosen:
        entity = GetChosen();
        break;
    case NetProperty::MapItem:
        entity = _curMap->GetItem(item_id);
        break;
    case NetProperty::CritterItem:
        if (_curMap != nullptr) {
            if (auto* cr = _curMap->GetCritter(cr_id); cr != nullptr) {
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
        entity = _curMap.get();
        break;
    case NetProperty::Location:
        entity = _curLocation.get();
        break;
    case NetProperty::CustomEntity:
        entity = GetEntity(entity_id);
        break;
    default:
        FO_UNREACHABLE_PLACE();
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

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

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

void FOClient::Net_OnTimeSync()
{
    FO_STACK_TRACE_ENTRY();

    const auto time = _conn.InBuf.Read<synctime>();

    GameTime.SetSynchronizedTime(time);
    SetSynchronizedTime(time);
}

void FOClient::Net_OnLoadMap()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Change map");

    const auto loc_id = _conn.InBuf.Read<ident_t>();
    const auto map_id = _conn.InBuf.Read<ident_t>();
    const auto loc_pid = _conn.InBuf.Read<hstring>(Hashes);
    const auto map_pid = _conn.InBuf.Read<hstring>(Hashes);
    const auto map_index_in_loc = _conn.InBuf.Read<int32>();

    if (map_pid) {
        _conn.InBuf.ReadPropsData(_tempPropertiesData);
        _conn.InBuf.ReadPropsData(_tempPropertiesDataExt);
    }

    UnloadMap();

    _curMapLocPid = loc_pid;
    _curMapIndexInLoc = map_index_in_loc;

    if (map_pid) {
        const auto* loc_proto = ProtoMngr.GetProtoLocation(loc_pid);
        _curLocation = SafeAlloc::MakeRefCounted<LocationView>(this, loc_id, loc_proto);
        _curLocation->RestoreData(_tempPropertiesDataExt);

        const auto* map_proto = ProtoMngr.GetProtoMap(map_pid);
        _curMap = SafeAlloc::MakeRefCounted<MapView>(this, map_id, map_proto);
        _curMap->RestoreData(_tempPropertiesData);
        _curMap->LoadStaticData();

        ReceiveCustomEntities(_curLocation.get());
        ReceiveCustomEntities(_curMap.get());

        WriteLog("Start load map");
    }
    else {
        WriteLog("Start load global map");
    }

    OnMapLoad.Fire();
}

void FOClient::Net_OnSomeItems()
{
    FO_STACK_TRACE_ENTRY();

    const auto context_param = any_t {_conn.InBuf.Read<string>()};
    const auto items_count = _conn.InBuf.Read<uint32>();

    vector<refcount_ptr<ItemView>> items;
    items.reserve(items_count);

    for (uint32 i = 0; i < items_count; i++) {
        const auto item_id = _conn.InBuf.Read<ident_t>();
        const auto pid = _conn.InBuf.Read<hstring>(Hashes);
        _conn.InBuf.ReadPropsData(_tempPropertiesData);
        FO_RUNTIME_ASSERT(item_id);

        const auto* proto = ProtoMngr.GetProtoItem(pid);
        auto item = SafeAlloc::MakeRefCounted<ItemView>(this, item_id, proto);
        item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(item.get());

        items.emplace_back(item);
    }

    const auto items2 = vec_transform(items, [](auto&& item) -> ItemView* { return item.get(); });
    OnReceiveItems.Fire(items2, context_param);
}

void FOClient::Net_OnViewMap()
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = _conn.InBuf.Read<mpos>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    OnMapView.Fire(hex);
}

void FOClient::Net_OnRemoteCall()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto rpc_num = _conn.InBuf.Read<uint32>();

    ScriptSys.HandleRemoteCall(rpc_num, _curPlayer.get());
}

void FOClient::Net_OnAddCustomEntity()
{
    FO_STACK_TRACE_ENTRY();

    const auto holder_id = _conn.InBuf.Read<ident_t>();
    const auto holder_entry = _conn.InBuf.Read<hstring>(Hashes);
    const auto id = _conn.InBuf.Read<ident_t>();
    const auto pid = _conn.InBuf.Read<hstring>(Hashes);
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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    const auto entries_count = _conn.InBuf.Read<uint16>();

    if (entries_count == 0) {
        return;
    }

    for (uint16 i = 0; i < entries_count; i++) {
        const auto entry = _conn.InBuf.Read<hstring>(Hashes);
        const auto entities_count = _conn.InBuf.Read<uint32>();

        for (uint32 j = 0; j < entities_count; j++) {
            const auto id = _conn.InBuf.Read<ident_t>();
            const auto pid = _conn.InBuf.Read<hstring>(Hashes);
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
    FO_STACK_TRACE_ENTRY();

    const auto type_name = std::get<0>(GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.at(entry));

    FO_RUNTIME_ASSERT(IsValidEntityType(type_name));

    const bool has_protos = GetEntityTypeInfo(type_name).HasProtos;
    const ProtoEntity* proto = nullptr;

    if (pid) {
        FO_RUNTIME_ASSERT(has_protos);
        proto = ProtoMngr.GetProtoEntity(type_name, pid);
        FO_RUNTIME_ASSERT(proto);
    }
    else {
        FO_RUNTIME_ASSERT(!has_protos);
    }

    const auto* registrator = GetPropertyRegistrator(type_name);

    refcount_ptr<CustomEntityView> entity;

    if (proto != nullptr) {
        entity = SafeAlloc::MakeRefCounted<CustomEntityWithProtoView>(this, id, registrator, nullptr, proto);
    }
    else {
        entity = SafeAlloc::MakeRefCounted<CustomEntityView>(this, id, registrator, nullptr);
    }

    entity->RestoreData(data);

    if (const auto* holder_with_id = dynamic_cast<ClientEntity*>(holder); holder_with_id != nullptr) {
        entity->SetCustomHolderId(holder_with_id->GetId());
    }
    else {
        FO_RUNTIME_ASSERT(holder == this);
    }

    entity->SetCustomHolderEntry(entry);

    holder->AddInnerEntity(entry, entity.get());

    return entity.get();
}

void FOClient::ReceiveCritterMoving(CritterHexView* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto whole_time = _conn.InBuf.Read<uint32>();
    const auto offset_time = _conn.InBuf.Read<uint32>();
    const auto speed = _conn.InBuf.Read<uint16>();
    const auto start_hex = _conn.InBuf.Read<mpos>();

    const auto steps_count = _conn.InBuf.Read<uint16>();
    vector<uint8> steps;
    steps.resize(steps_count);
    for (uint16 i = 0; i < steps_count; i++) {
        steps[i] = _conn.InBuf.Read<uint8>();
    }

    const auto control_steps_count = _conn.InBuf.Read<uint16>();
    vector<uint16> control_steps;
    control_steps.resize(control_steps_count);
    for (uint16 i = 0; i < control_steps_count; i++) {
        control_steps[i] = _conn.InBuf.Read<uint16>();
    }

    const auto end_hex_offset = _conn.InBuf.Read<ipos16>();

    if (_curMap == nullptr) {
        BreakIntoDebugger();
        return;
    }

    if (cr == nullptr) {
        BreakIntoDebugger();
        return;
    }

    cr->ClearMove();

    cr->Moving.Speed = speed;
    cr->Moving.StartTime = GameTime.GetFrameTime();
    cr->Moving.OffsetTime = std::chrono::milliseconds {offset_time};
    cr->Moving.WholeTime = numeric_cast<float32>(whole_time);
    cr->Moving.Steps = steps;
    cr->Moving.ControlSteps = control_steps;
    cr->Moving.StartHex = start_hex;
    cr->Moving.StartHexOffset = cr->GetHexOffset();
    cr->Moving.EndHexOffset = end_hex_offset;

    if (offset_time == 0 && start_hex != cr->GetHex()) {
        const auto cr_offset = Geometry.GetHexInterval(start_hex, cr->GetHex());
        cr->Moving.StartHexOffset = {numeric_cast<int16>(cr->Moving.StartHexOffset.x + cr_offset.x), numeric_cast<int16>(cr->Moving.StartHexOffset.y + cr_offset.y)};
    }

    cr->Moving.WholeDist = 0.0f;

    mpos next_start_hex = start_hex;
    uint16 control_step_begin = 0;

    for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
        auto hex = next_start_hex;

        FO_RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        FO_RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(hex, cr->Moving.Steps[j], _curMap->GetSize());
            FO_RUNTIME_ASSERT(move_ok);
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

        const auto proj_oy = numeric_cast<float32>(oy) * Geometry.GetYProj();
        const auto dist = std::sqrt(numeric_cast<float32>(ox * ox) + proj_oy * proj_oy);

        cr->Moving.WholeDist += dist;

        control_step_begin = cr->Moving.ControlSteps[i];
        next_start_hex = hex;

        cr->Moving.EndHex = hex;
    }

    cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);
    cr->Moving.WholeDist = std::max(cr->Moving.WholeDist, 0.0001f);

    FO_RUNTIME_ASSERT(!cr->Moving.Steps.empty());
    FO_RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
    FO_RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
    FO_RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);
}

auto FOClient::GetEntity(ident_t id) -> ClientEntity*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _allEntities.find(id);

    return it != _allEntities.end() ? it->second : nullptr;
}

void FOClient::RegisterEntity(ClientEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(entity);
    FO_RUNTIME_ASSERT(entity->GetId());

    _allEntities[entity->GetId()] = entity;
}

void FOClient::UnregisterEntity(ClientEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(entity);
    FO_RUNTIME_ASSERT(entity->GetId());

    _allEntities.erase(entity->GetId());
}

auto FOClient::AnimLoad(hstring name, AtlasType atlas_type) -> uint32
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _ifaceAnimationsCache.find(name); it != _ifaceAnimationsCache.end()) {
        auto& iface_anim = it->second;

        iface_anim->Anim->PlayDefault();

        _ifaceAnimations[++_ifaceAnimCounter] = std::move(iface_anim);
        _ifaceAnimationsCache.erase(it);

        return _ifaceAnimCounter;
    }

    auto anim = SprMngr.LoadSprite(name, atlas_type);

    if (!anim) {
        BreakIntoDebugger();
        return 0;
    }

    anim->PlayDefault();

    auto iface_anim = SafeAlloc::MakeUnique<IfaceAnim>();

    iface_anim->Name = name;
    iface_anim->Anim = anim;

    _ifaceAnimations[++_ifaceAnimCounter] = std::move(iface_anim);

    return _ifaceAnimCounter;
}

void FOClient::AnimFree(uint32 anim_id)
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _ifaceAnimations.find(anim_id); it != _ifaceAnimations.end()) {
        auto& iface_anim = it->second;

        iface_anim->Anim->Stop();

        _ifaceAnimationsCache.emplace(iface_anim->Name, std::move(iface_anim));
        _ifaceAnimations.erase(it);
    }
}

auto FOClient::AnimGetSpr(uint32 anim_id) -> Sprite*
{
    FO_STACK_TRACE_ENTRY();

    if (anim_id == 0) {
        return nullptr;
    }

    if (const auto it = _ifaceAnimations.find(anim_id); it != _ifaceAnimations.end()) {
        return it->second->Anim.get();
    }

    return nullptr;
}

void FOClient::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(entity == this);

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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(entity == _curPlayer.get());

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (!_curPlayer->GetId()) {
        throw ScriptException("Can't modify player public/protected property on unlogined player");
    }

    Net_SendProperty(NetProperty::Player, prop, _curPlayer.get());
}

void FOClient::OnSendCritterValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (auto* item = dynamic_cast<ItemView*>(entity); item != nullptr && !item->GetStatic() && item->GetId()) {
        if (item->GetOwnership() == ItemOwnership::CritterInventory) {
            const auto* cr = _curMap->GetCritter(item->GetCritterId());
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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(entity == _curMap.get());

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Map, prop, _curMap.get());
    }
    else {
        throw GenericException("Unable to send map modifiable property", prop->GetName());
    }
}

void FOClient::OnSendLocationValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(entity == _curLocation.get());

    if (entity == _sendIgnoreEntity && prop == _sendIgnoreProperty) {
        return;
    }

    if (prop->GetAccess() == Property::AccessType::PublicFullModifiable) {
        Net_SendProperty(NetProperty::Location, prop, _curLocation.get());
    }
    else {
        throw GenericException("Unable to send location modifiable property", prop->GetName());
    }
}

void FOClient::OnSetCritterLookDistance(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr && cr->GetIsChosen()) {
        cr->GetMap()->RefreshMap();
        cr->GetMap()->RebuildFog();
    }
}

void FOClient::OnSetCritterModelName(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

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
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr && cr->IsSpriteValid()) {
        cr->GetSprite()->SetContour(cr->GetSprite()->Contour, cr->GetContourColor());
    }
}

void FOClient::OnSetCritterHideSprite(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

    if (auto* cr = dynamic_cast<CritterHexView*>(entity); cr != nullptr) {
        cr->SetSpriteVisiblity(!cr->GetHideSprite());
    }
}

void FOClient::OnSetItemFlags(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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
            _curMap->RebuildFog();
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
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    // IsLight, LightIntensity, LightDistance, LightFlags, LightColor

    ignore_unused(entity);
    ignore_unused(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->GetMap()->UpdateItemLightSource(item);
    }
}

void FOClient::OnSetItemPicMap(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->RefreshAnim();
    }
}

void FOClient::OnSetItemOffsetCoords(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->RefreshOffs();
        item->GetMap()->MeasureMapBorders(item);
    }
}

void FOClient::OnSetItemOpened(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

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
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(prop);

    if (auto* item = dynamic_cast<ItemHexView*>(entity); item != nullptr) {
        item->SetSpriteVisiblity(!item->GetHideSprite());
    }
}

void FOClient::ChangeLanguage(string_view lang_name)
{
    FO_STACK_TRACE_ENTRY();

    auto lang_pack = LanguagePack {lang_name, *this};
    lang_pack.LoadFromResources(Resources);

    _curLang = std::move(lang_pack);
    Settings.Language = lang_name;
}

void FOClient::UnloadMap()
{
    FO_STACK_TRACE_ENTRY();

    OnMapUnload.Fire();

    if (_curMap) {
        _curMap->DestroySelf();
        _curMap = nullptr;

        CleanupSpriteCache();
    }

    if (_curLocation) {
        _curLocation->DestroySelf();
        _curLocation = nullptr;
    }

    for (auto& cr : _globalMapCritters) {
        cr->DestroySelf();
    }

    _globalMapCritters.clear();

    SndMngr.StopSounds();

    _mapLoaded = false;
}

void FOClient::LmapPrepareMap()
{
    FO_STACK_TRACE_ENTRY();

    _lmapPrepPix.clear();

    if (_curMap == nullptr) {
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

            if (i1 < 0 || i2 < 0 || i1 >= numeric_cast<int32>(_curMap->GetSize().width) || i2 >= numeric_cast<int32>(_curMap->GetSize().height)) {
                continue;
            }

            bool is_far = false;
            const auto dist = GeometryHelper::DistGame(hex.x, hex.y, i1, i2);

            if (dist > vis) {
                is_far = true;
            }

            const auto& field = _curMap->GetField({numeric_cast<uint16>(i1), numeric_cast<uint16>(i2)});
            ucolor cur_color;

            if (const auto* cr = _curMap->GetNonDeadCritter({numeric_cast<uint16>(i1), numeric_cast<uint16>(i2)}); cr != nullptr) {
                cur_color = cr == chosen ? ucolor {0, 0, 255} : ucolor {255, 0, 0};
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

    _lmapPrepareNextTime = GameTime.GetFrameTime() + std::chrono::milliseconds {1000};
}

void FOClient::CleanupSpriteCache()
{
    FO_STACK_TRACE_ENTRY();

    ResMngr.CleanupCritterFrames();
    SprMngr.CleanupSpriteCache();
}

void FOClient::DestroyInnerEntities()
{
    FO_STACK_TRACE_ENTRY();

    if (HasInnerEntities()) {
        for (auto&& [entry, entities] : GetInnerEntities()) {
            for (auto& entity : entities) {
                auto* custom_entity = dynamic_cast<CustomEntityView*>(entity.get());
                FO_RUNTIME_ASSERT(custom_entity);

                custom_entity->DestroySelf();
            }
        }

        ClearInnerEntities();
    }
}

auto FOClient::CustomCall(string_view command, string_view separator) -> string
{
    FO_STACK_TRACE_ENTRY();

    // Parse command
    vector<string> args;
    const auto command_str = string(command);
    istringstream ss(command_str);

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
        if (_curMap != nullptr) {
            _curMap->SwitchShowTrack();
        }
    }
    else if (cmd == "SwitchShowHex") {
        if (_curMap != nullptr) {
            _curMap->SwitchShowHex();
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
        //    _curMap->RebuildFog();
        // }
    }
    else if (cmd == "BytesSend") {
        return strex("{}", _conn.GetBytesSend());
    }
    else if (cmd == "BytesReceive") {
        return strex("{}", _conn.GetBytesReceived());
    }
    else if (cmd == "SetResolution" && args.size() >= 3) {
        const int32 w = strex(args[1]).toInt();
        const int32 h = strex(args[2]).toInt();

        SprMngr.SetScreenSize({w, h});
        SprMngr.SetWindowSize({w, h});
    }
    else if (cmd == "RefreshAlwaysOnTop") {
        SprMngr.SetAlwaysOnTop(Settings.AlwaysOnTop);
    }
    else if (cmd == "DrawMiniMap" && args.size() >= 6) {
        const int32 zoom = strex(args[1]).toInt();
        const int32 x = strex(args[2]).toInt();
        const int32 y = strex(args[3]).toInt();
        const int32 x2 = x + strex(args[4]).toInt();
        const int32 y2 = y + strex(args[5]).toInt();

        if (zoom != _lmapZoom || x != _lmapWMap[0] || y != _lmapWMap[1] || x2 != _lmapWMap[2] || y2 != _lmapWMap[3]) {
            _lmapZoom = zoom;
            _lmapWMap[0] = x;
            _lmapWMap[1] = y;
            _lmapWMap[2] = x2;
            _lmapWMap[3] = y2;
            LmapPrepareMap();
        }
        else if (GameTime.GetFrameTime() >= _lmapPrepareNextTime) {
            LmapPrepareMap();
        }

        SprMngr.DrawPoints(_lmapPrepPix, RenderPrimitiveType::LineList);
    }
    else if (cmd == "SkipRoof" && args.size() == 3) {
        if (_curMap != nullptr) {
            const auto hx = strex(args[1]).toUInt();
            const auto hy = strex(args[2]).toUInt();
            _curMap->SetSkipRoof({numeric_cast<uint16>(hx), numeric_cast<uint16>(hy)});
        }
    }
    else {
        throw ScriptException("Invalid custom call command", cmd, args.size());
    }

    return "";
}

void FOClient::Connect(string_view login, string_view password, int32 reason)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(reason == INIT_NET_REASON_LOGIN || reason == INIT_NET_REASON_REG || reason == INIT_NET_REASON_CUSTOM);

    if (reason == _initNetReason) {
        return;
    }

    _loginName = login;
    _loginPassword = password;
    _initNetReason = reason;

    if (_conn.IsConnected()) {
        if (_initNetReason == INIT_NET_REASON_LOGIN) {
            Net_SendLogIn();
        }
        else if (_initNetReason == INIT_NET_REASON_REG) {
            Net_SendCreatePlayer();
        }
    }
}

void FOClient::Disconnect()
{
    FO_STACK_TRACE_ENTRY();

    _initNetReason = INIT_NET_REASON_NONE;
    _conn.Disconnect();
}

void FOClient::CritterMoveTo(CritterHexView* cr, variant<tuple<mpos, ipos16>, int32> pos_or_dir, int32 speed)
{
    FO_STACK_TRACE_ENTRY();

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
                    control_steps.push_back(numeric_cast<uint16>(steps.size()));
                    try_move = true;
                }
            }
        }
    }

    if (try_move) {
        cr->Moving.Steps = steps;
        cr->Moving.ControlSteps = control_steps;
        cr->Moving.StartTime = GameTime.GetFrameTime();
        cr->Moving.Speed = numeric_cast<uint16>(speed);
        cr->Moving.StartHex = cr->GetHex();
        cr->Moving.EndHex = hex;
        cr->Moving.StartHexOffset = cr->GetHexOffset();
        cr->Moving.EndHexOffset = hex_offset;

        cr->Moving.WholeTime = {};
        cr->Moving.WholeDist = {};

        FO_RUNTIME_ASSERT(cr->Moving.Speed > 0);
        const auto base_move_speed = numeric_cast<float32>(cr->Moving.Speed);

        auto next_start_hex = cr->Moving.StartHex;
        uint16 control_step_begin = 0;

        for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
            auto hex2 = next_start_hex;

            FO_RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
            FO_RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
            for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
                const auto move_ok = GeometryHelper::MoveHexByDir(hex2, cr->Moving.Steps[j], cr->GetMap()->GetSize());
                FO_RUNTIME_ASSERT(move_ok);
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

            const auto proj_oy = numeric_cast<float32>(offset2.y) * Geometry.GetYProj();
            const auto dist = std::sqrt(numeric_cast<float32>(offset2.x * offset2.x) + proj_oy * proj_oy);

            cr->Moving.WholeDist += dist;
            cr->Moving.WholeTime += dist / base_move_speed * 1000.0f;

            control_step_begin = cr->Moving.ControlSteps[i];
            next_start_hex = hex2;

            if (i == cr->Moving.ControlSteps.size() - 1) {
                FO_RUNTIME_ASSERT(hex2 == cr->Moving.EndHex);
            }
        }

        cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);
        cr->Moving.WholeDist = std::max(cr->Moving.WholeDist, 0.0001f);

        FO_RUNTIME_ASSERT(!cr->Moving.Steps.empty());
        FO_RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
        FO_RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
        FO_RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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
    FO_RUNTIME_ASSERT(!names.empty());
    const auto file = Resources.ReadFile(names.front());

    if (!file) {
        return;
    }

    _video = SafeAlloc::MakeUnique<VideoClip>(file.GetData());
    _videoTex = App->Render.CreateTexture(_video->GetSize(), true, false);

    if (names.size() > 1) {
        SndMngr.StopMusic();

        if (!names[1].empty()) {
            SndMngr.PlayMusic(names[1], timespan::zero);
        }
    }
}

void FOClient::ProcessVideo()
{
    FO_STACK_TRACE_ENTRY();

    if (_video) {
        _videoTex->UpdateTextureRegion({}, _videoTex->Size, _video->RenderFrame().data());
        SprMngr.DrawTexture(_videoTex.get(), false);

        if (_video->IsStopped()) {
            _video.reset();
            _videoTex.reset();
            SndMngr.StopMusic();
        }
    }

    if (!_video && !_videoQueue.empty()) {
        auto queue = copy(_videoQueue);

        PlayVideo(std::get<0>(queue.front()), std::get<1>(queue.front()), false);

        queue.erase(queue.begin());
        _videoQueue = queue;
    }
}

FO_END_NAMESPACE();
