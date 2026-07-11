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

#include "Client.h"
#include "AngelScriptScripting.h"
#include "DefaultSprites.h"
#include "MetadataRegistration.h"
#include "ModelSprites.h"
#include "Movement.h"
#include "ParticleSprites.h"

FO_BEGIN_NAMESPACE

extern void ClientInitHook(ptr<ClientEngine>);

auto GetClientResources(GlobalSettings& settings) -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    FileSystem resources;
    resources.AddPacksSource(IsPackaged() ? settings.ClientResources : settings.BakeOutput, settings.ClientResourceEntries);
    return resources;
}

ClientEngine::ClientEngine(ptr<GlobalSettings> settings, FileSystem&& resources, ptr<IAppWindow> window) :
    BaseEngine(settings, std::move(resources), [&] { RegisterClientMetadata(this, &resources); }),
    EffectMngr(Settings, make_ptr(&Resources), window->GetRender()),
    SprMngr(Settings, window, make_ptr(&Resources), make_ptr(&GameTime), make_ptr(&EffectMngr), make_ptr(&Hashes)),
    FontMngr(make_ptr(&SprMngr)),
    ResMngr(Settings, make_ptr(&Resources), make_ptr(&SprMngr), make_ptr(this)),
    SndMngr(Settings, make_ptr(&Resources), window->GetAudio()),
    Cache(fs_make_writable_path(settings->UserWritablePath, settings->CacheResources)),
    _conn(Settings)
{
    FO_STACK_TRACE_ENTRY();

    // Headless test clients still execute the normal draw pipeline, so dummy mode
    // must synthesize the full default effect set instead of the updater-only subset.
    EffectMngr.LoadDefaultEffects();

    // Init sprite subsystems
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<DefaultSpriteFactory>(&SprMngr));
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ParticleSpriteFactory>(&SprMngr, Settings, &EffectMngr, &GameTime, &Hashes));
#if FO_ENABLE_3D
    SprMngr.RegisterSpriteFactory(SafeAlloc::MakeUnique<ModelSpriteFactory>(&SprMngr, Settings, &EffectMngr, &GameTime, &Hashes, this, this));
#endif

    ResMngr.IndexFiles();

    MapEngineType<PlayerView>(EngineMetadata::GetBaseType(PlayerView::ENTITY_TYPE_NAME));
    MapEngineType<ItemView>(EngineMetadata::GetBaseType(ItemView::ENTITY_TYPE_NAME));
    MapEngineType<ItemHexView>(EngineMetadata::GetBaseType(ItemView::ENTITY_TYPE_NAME));
    MapEngineType<CritterView>(EngineMetadata::GetBaseType(CritterView::ENTITY_TYPE_NAME));
    MapEngineType<CritterHexView>(EngineMetadata::GetBaseType(CritterView::ENTITY_TYPE_NAME));
    MapEngineType<MapView>(EngineMetadata::GetBaseType(MapView::ENTITY_TYPE_NAME));
    MapEngineType<LocationView>(EngineMetadata::GetBaseType(LocationView::ENTITY_TYPE_NAME));

    MapScriptTypes(this);
#if FO_ANGELSCRIPT_SCRIPTING
    InitAngelScriptScripting(this, *settings, Resources);
#endif

    Hashes.SetResolveHashFailureHandler([this](hstring::hash_t hash) FO_DEFERRED { HandleUnresolvedHash(hash); });

    _curLang = TextPack {&Hashes};
    _curLang.LoadFromResources(Resources, Settings->Language);

    // Modules initialization
    ClientInitHook(this);

    InitModules();

    OnStart.Fire();

    // Connection handlers
    _conn.SetConnectHandler([this](ClientConnection::ConnectResult result) FO_DEFERRED { Net_OnConnect(result); });
    _conn.SetDisconnectHandler([this]() FO_DEFERRED { Net_OnDisconnect(); });
    _conn.AddMessageHandler(NetMessage::LoginSuccess, [this]() FO_DEFERRED { Net_OnLoginSuccess(); });
    _conn.AddMessageHandler(NetMessage::PlaceToGameComplete, [this]() FO_DEFERRED { Net_OnPlaceToGameComplete(); });
    _conn.AddMessageHandler(NetMessage::AddCritter, [this]() FO_DEFERRED { Net_OnAddCritter(); });
    _conn.AddMessageHandler(NetMessage::RemoveCritter, [this]() FO_DEFERRED { Net_OnRemoveCritter(); });
    _conn.AddMessageHandler(NetMessage::CritterVisibilityMode, [this]() FO_DEFERRED { Net_OnCritterVisibilityMode(); });
    _conn.AddMessageHandler(NetMessage::CritterAction, [this]() FO_DEFERRED { Net_OnCritterAction(); });
    _conn.AddMessageHandler(NetMessage::CritterMoveItem, [this]() FO_DEFERRED { Net_OnCritterMoveItem(); });
    _conn.AddMessageHandler(NetMessage::CritterTeleport, [this]() FO_DEFERRED { Net_OnCritterTeleport(); });
    _conn.AddMessageHandler(NetMessage::CritterMove, [this]() FO_DEFERRED { Net_OnCritterMove(); });
    _conn.AddMessageHandler(NetMessage::CritterMoveSpeed, [this]() FO_DEFERRED { Net_OnCritterMoveSpeed(); });
    _conn.AddMessageHandler(NetMessage::CritterDir, [this]() FO_DEFERRED { Net_OnCritterDir(); });
    _conn.AddMessageHandler(NetMessage::CritterPos, [this]() FO_DEFERRED { Net_OnCritterPos(); });
    _conn.AddMessageHandler(NetMessage::CritterAttachments, [this]() FO_DEFERRED { Net_OnCritterAttachments(); });
    _conn.AddMessageHandler(NetMessage::Property, [this]() FO_DEFERRED { Net_OnProperty(); });
    _conn.AddMessageHandler(NetMessage::InfoMessage, [this]() FO_DEFERRED { Net_OnInfoMessage(); });
    _conn.AddMessageHandler(NetMessage::ChosenAddItem, [this]() FO_DEFERRED { Net_OnChosenAddItem(); });
    _conn.AddMessageHandler(NetMessage::ChosenRemoveItem, [this]() FO_DEFERRED { Net_OnChosenRemoveItem(); });
    _conn.AddMessageHandler(NetMessage::TimeSync, [this]() FO_DEFERRED { Net_OnTimeSync(); });
    _conn.AddMessageHandler(NetMessage::ViewMap, [this]() FO_DEFERRED { Net_OnViewMap(); });
    _conn.AddMessageHandler(NetMessage::LoadMap, [this]() FO_DEFERRED { Net_OnLoadMap(); });
    _conn.AddMessageHandler(NetMessage::SomeItems, [this]() FO_DEFERRED { Net_OnSomeItems(); });
    _conn.AddMessageHandler(NetMessage::RemoteCall, [this]() FO_DEFERRED { Net_OnRemoteCall(); });
    _conn.AddMessageHandler(NetMessage::AddItemOnMap, [this]() FO_DEFERRED { Net_OnAddItemOnMap(); });
    _conn.AddMessageHandler(NetMessage::RemoveItemFromMap, [this]() FO_DEFERRED { Net_OnRemoveItemFromMap(); });
    _conn.AddMessageHandler(NetMessage::InitData, [this]() FO_DEFERRED { Net_OnInitData(); });
    _conn.AddMessageHandler(NetMessage::AddCustomEntity, [this]() FO_DEFERRED { Net_OnAddCustomEntity(); });
    _conn.AddMessageHandler(NetMessage::RemoveCustomEntity, [this]() FO_DEFERRED { Net_OnRemoveCustomEntity(); });
    _conn.AddMessageHandler(NetMessage::HashList, [this]() FO_DEFERRED { Net_OnHashList(); });

    // Properties that sending to clients
    {
        const auto wrap_post_setter = [this](void (ClientEngine::*callback)(ptr<Entity>, ptr<const Property>)) -> PropertyPostSetCallback {
            return [this, callback](nptr<Entity> entity, ptr<const Property> prop) FO_DEFERRED {
                FO_VERIFY_AND_THROW(entity, "Property owner entity is null");
                (this->*callback)(entity, prop);
            };
        };

        const auto set_send_callbacks = [](nptr<const PropertyRegistrator> registrator, const PropertyPostSetCallback& callback) {
            FO_VERIFY_AND_THROW(registrator, "Missing property registrator");

            for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
                auto prop = registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
                FO_VERIFY_AND_THROW(prop, "Missing property by index");

                if (!prop->IsModifiableByClient() && !prop->IsModifiableByAnyClient()) {
                    continue;
                }

                prop->AddPostSetter(callback);
            }
        };

        set_send_callbacks(GetPropertyRegistrator(GameProperties::ENTITY_TYPE_NAME), wrap_post_setter(&ClientEngine::OnSendGlobalValue));
        set_send_callbacks(GetPropertyRegistrator(PlayerProperties::ENTITY_TYPE_NAME), wrap_post_setter(&ClientEngine::OnSendPlayerValue));
        set_send_callbacks(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), wrap_post_setter(&ClientEngine::OnSendItemValue));
        set_send_callbacks(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), wrap_post_setter(&ClientEngine::OnSendCritterValue));
        set_send_callbacks(GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME), wrap_post_setter(&ClientEngine::OnSendMapValue));
        set_send_callbacks(GetPropertyRegistrator(LocationProperties::ENTITY_TYPE_NAME), wrap_post_setter(&ClientEngine::OnSendLocationValue));
    }

    // Properties with custom behaviours
    {
        const auto wrap_post_setter = [this](void (ClientEngine::*callback)(ptr<Entity>, ptr<const Property>)) -> PropertyPostSetCallback {
            return [this, callback](nptr<Entity> entity, ptr<const Property> prop) FO_DEFERRED {
                FO_VERIFY_AND_THROW(entity, "Property owner entity is null");
                (this->*callback)(entity, prop);
            };
        };

        const auto set_callback = [](nptr<const PropertyRegistrator> registrator, int32_t prop_index, PropertyPostSetCallback callback) {
            FO_VERIFY_AND_THROW(registrator, "Missing property registrator");

            auto prop = registrator->GetPropertyByIndex(prop_index);
            FO_VERIFY_AND_THROW(prop, "Missing property by index");
            prop->AddPostSetter(std::move(callback));
        };

        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LookDistance_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterLookDistance));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::ModelName_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterModelName));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::HideSprite_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterHideSprite));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::Elevation_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterElevation));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LightSource_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterLight));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LightIntensity_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterLight));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LightDistance_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterLight));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LightFlags_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterLight));
        set_callback(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), CritterView::LightColor_RegIndex, wrap_post_setter(&ClientEngine::OnSetCritterLight));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::Colorize_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemFlags));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::ColorizeColor_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemFlags));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::ShootThru_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemFlags));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightThru_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemFlags));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::NoBlock_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemFlags));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightSource_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemSomeLight));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightIntensity_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemSomeLight));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightDistance_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemSomeLight));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightFlags_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemSomeLight));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::LightColor_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemSomeLight));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::PicMap_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemPicMap));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::Offset_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemOffsetCoords));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::HideSprite_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemHideSprite));
        set_callback(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), ItemView::Elevation_RegIndex, wrap_post_setter(&ClientEngine::OnSetItemElevation));
    }

    _eventUnsubscriber += (*window->GetOnScreenSizeChanged()) += [this]() FO_DEFERRED { OnScreenSizeChanged.Fire(); };
}

ClientEngine::ClientEngine(ptr<GlobalSettings> settings, FileSystem&& resources, ptr<IAppWindow> window, const MeatdataRegistrator& mapper_registrator) :
    BaseEngine(settings, std::move(resources), mapper_registrator),
    EffectMngr(Settings, make_ptr(&Resources), window->GetRender()),
    SprMngr(Settings, window, make_ptr(&Resources), make_ptr(&GameTime), make_ptr(&EffectMngr), make_ptr(&Hashes)),
    FontMngr(make_ptr(&SprMngr)),
    ResMngr(Settings, make_ptr(&Resources), make_ptr(&SprMngr), make_ptr(this)),
    SndMngr(Settings, make_ptr(&Resources), window->GetAudio()),
    Cache(fs_make_writable_path(settings->UserWritablePath, settings->CacheResources)),
    _conn(Settings)
{
    FO_STACK_TRACE_ENTRY();
}

ClientEngine::~ClientEngine()
{
    FO_STACK_TRACE_ENTRY();
}

void ClientEngine::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    Disconnect();

    OnFinish.Fire();

    UnsubscribeAllEvents();
    ClearAllTimeEvents();

    TimeEventMngr.ClearTimeEvents();

    _conn.SetConnectHandler(nullptr);
    _conn.SetDisconnectHandler(nullptr);
    _conn.Disconnect();

    SprMngr.GetRender().SetRenderTarget(nullptr);

    _chosen.reset();

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        map->DestroySelf();
        _curMap.reset();
    }

    if (_curLocation) {
        auto location = GetCurLocation();
        FO_VERIFY_AND_THROW(location, "Location is null");
        location->DestroySelf();
        _curLocation.reset();
    }

    if (_curPlayer) {
        auto player = GetCurPlayer();
        FO_VERIFY_AND_THROW(player, "Player is null");
        player->DestroySelf();
        _curPlayer.reset();
    }

    for (size_t i = 0; i < _globalMapCritters.size(); i++) {
        _globalMapCritters[i]->DestroySelf();
    }

    _globalMapCritters.clear();

    DestroyInnerEntities();

    ShutdownBackends();

    FO_VERIFY_AND_THROW(GetRefCount() == 1, "Client engine still has external references after shutdown", GetRefCount());
}

void ClientEngine::ScheduleDelayedCallback(timespan delay, function<void()> body)
{
    FO_STACK_TRACE_ENTRY();

    const auto fire_time = GameTime.GetFrameTime() + delay;
    const auto pos = std::ranges::lower_bound(_scheduledCallbacks, fire_time, {}, &ScheduledCallback::FireTime);
    _scheduledCallbacks.insert(pos, ScheduledCallback {.FireTime = fire_time, .Body = std::move(body)});
}

void ClientEngine::ProcessScheduledCallbacks()
{
    FO_STACK_TRACE_ENTRY();

    // Execute only callbacks that were due when this pass began.
    const auto now = GameTime.GetFrameTime();
    const auto due_end = std::ranges::upper_bound(_scheduledCallbacks, now, {}, &ScheduledCallback::FireTime);
    const size_t due_count = numeric_cast<size_t>(std::distance(_scheduledCallbacks.begin(), due_end));
    vector<function<void()>> callbacks;
    callbacks.reserve(due_count);

    for (auto it = _scheduledCallbacks.begin(); it != due_end; ++it) {
        callbacks.emplace_back(std::move(it->Body));
    }

    _scheduledCallbacks.erase(_scheduledCallbacks.begin(), due_end);

    for (auto& body : callbacks) {
        try {
            body();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }
}

auto ClientEngine::ResolveCritterAnimationFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32_t& pass, uint32_t& flags, int32_t& ox, int32_t& oy, string& anim_name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return OnCritterAnimationFrames.Fire(model_name, state_anim, action_anim, pass, flags, ox, oy, anim_name) == EventResult::ContinueChain;
}

auto ClientEngine::ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return OnCritterAnimationSubstitute.Fire(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim) == EventResult::ContinueChain;
}

auto ClientEngine::ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32_t& f_state_anim, int32_t& f_action_anim, int32_t& f_state_anim_ex, int32_t& f_action_anim_ex, uint32_t& flags) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return OnCritterAnimationFallout.Fire(model_name, state_anim, action_anim, f_state_anim, f_action_anim, f_state_anim_ex, f_action_anim_ex, flags) == EventResult::ContinueChain;
}

auto ClientEngine::IsConnecting() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _conn.IsConnecting();
}

auto ClientEngine::IsConnected() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _conn.IsConnected();
}

auto ClientEngine::GetChosen() noexcept -> nptr<CritterView>
{
    FO_STACK_TRACE_ENTRY();

    if (_chosen) {
        if (_chosen->IsDestroyed()) {
            _chosen.reset();
        }
    }

    return _chosen;
}

auto ClientEngine::GetMapChosen() noexcept -> nptr<CritterHexView>
{
    FO_STACK_TRACE_ENTRY();

    auto chosen = GetChosen();

    if (!chosen) {
        return nullptr;
    }

    return chosen.dyn_cast<CritterHexView>();
}

auto ClientEngine::GetGlobalMapCritter(ident_t cr_id) -> nptr<CritterView>
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _globalMapCritters.size(); i++) {
        auto cr = _globalMapCritters[i].as_ptr();

        if (cr->GetId() == cr_id) {
            return cr;
        }
    }

    return nullptr;
}

void ClientEngine::MainLoop()
{
    FO_STACK_TRACE_ENTRY();

    FrameAdvance();

#if FO_TRACY
    TracyPlot("Client FPS", numeric_cast<int64_t>(GameTime.GetFramesPerSecond()));
#endif

    // Network
    if (_connectionRequest && !_conn.IsConnecting() && !_conn.IsConnected()) {
        OnConnecting.Fire();
        _conn.Connect();
    }

    _conn.Process();
    ProcessInputEvents();
    ProcessScheduledCallbacks();
    TimeEventMngr.ProcessTimeEvents();
    OnLoop.Fire();

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        map->Process();
    }

    bool manual_scrolling = false;

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        manual_scrolling = map->IsManualScrolling();
    }

    SprMngr.GetWindow()->GrabInput(manual_scrolling);

    // Render
    EffectMngr.UpdateEffects(GameTime);
    FontMngr.FrameUpdate();

    {
        SprMngr.BeginScene();

        // Make dirty offscreen surfaces
        if (!PreDirtyOffscreenSurfaces.empty()) {
            DirtyOffscreenSurfaces.insert(DirtyOffscreenSurfaces.end(), PreDirtyOffscreenSurfaces.begin(), PreDirtyOffscreenSurfaces.end());
            PreDirtyOffscreenSurfaces.clear();
        }

        CanDrawInScripts = true;
        const auto restore_draw_scope = scope_exit([this]() noexcept { CanDrawInScripts = false; });

        OnRenderIface.Fire();

        ProcessVideo();

        SprMngr.EndScene();
    }
}

void ClientEngine::ProcessInputEvents()
{
    FO_STACK_TRACE_ENTRY();

    auto input = SprMngr.GetInput();

    if (SprMngr.IsWindowFocused()) {
        InputEvent ev;
        while (input->PollEvent(ev)) {
            ProcessInputEvent(ev);
        }
    }
    else {
        MousePos = input->GetMousePosition();

        OnInputLost.Fire();
    }

    if (HasForcedMousePos) {
        MousePos = ForcedMousePos;
    }
}

void ClientEngine::ProcessInputEvent(const InputEvent& ev)
{
    FO_STACK_TRACE_ENTRY();

    if (_video && !_video->Clip.IsStopped() && _videoCanInterrupt) {
        if (ev.Type == InputEvent::EventType::KeyDownEvent || ev.Type == InputEvent::EventType::MouseDownEvent || ev.Type == InputEvent::EventType::TouchDownEvent || ev.Type == InputEvent::EventType::TouchMoveEvent || ev.Type == InputEvent::EventType::TouchUpEvent || ev.Type == InputEvent::EventType::TouchTapEvent || ev.Type == InputEvent::EventType::TouchDoubleTapEvent || ev.Type == InputEvent::EventType::TouchScrollEvent || ev.Type == InputEvent::EventType::TouchZoomEvent) {
            _video->Clip.Stop();
        }
    }

    if (ev.Type == InputEvent::EventType::KeyDownEvent) {
        const auto key_code = ev.KeyDown.Code;
        const auto key_text = ev.KeyDown.Text;

        OnKeyDown.Fire(key_code, key_text);
    }
    else if (ev.Type == InputEvent::EventType::KeyUpEvent) {
        const auto key_code = ev.KeyUp.Code;

        OnKeyUp.Fire(key_code);
    }
    else if (ev.Type == InputEvent::EventType::MouseMoveEvent) {
        MousePos = {ev.MouseMove.MouseX, ev.MouseMove.MouseY};

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
    else if (ev.Type == InputEvent::EventType::TouchDownEvent) {
        MousePos = {ev.TouchDown.TouchX, ev.TouchDown.TouchY};

        OnTouchDown.Fire(ev.TouchDown.FingerId, MousePos);
    }
    else if (ev.Type == InputEvent::EventType::TouchMoveEvent) {
        MousePos = {ev.TouchMove.TouchX, ev.TouchMove.TouchY};

        OnTouchMove.Fire(ev.TouchMove.FingerId, MousePos, {ev.TouchMove.DeltaX, ev.TouchMove.DeltaY});
    }
    else if (ev.Type == InputEvent::EventType::TouchUpEvent) {
        MousePos = {ev.TouchUp.TouchX, ev.TouchUp.TouchY};

        OnTouchUp.Fire(ev.TouchUp.FingerId, MousePos);
    }
    else if (ev.Type == InputEvent::EventType::TouchTapEvent) {
        MousePos = {ev.TouchTap.TouchX, ev.TouchTap.TouchY};

        OnTouchTap.Fire(MousePos);
    }
    else if (ev.Type == InputEvent::EventType::TouchDoubleTapEvent) {
        MousePos = {ev.TouchDoubleTap.TouchX, ev.TouchDoubleTap.TouchY};

        OnTouchDoubleTap.Fire(MousePos);
    }
    else if (ev.Type == InputEvent::EventType::TouchScrollEvent) {
        MousePos = {ev.TouchScroll.TouchX, ev.TouchScroll.TouchY};

        OnTouchScroll.Fire(MousePos, {ev.TouchScroll.DeltaX, ev.TouchScroll.DeltaY});
    }
    else if (ev.Type == InputEvent::EventType::TouchZoomEvent) {
        MousePos = {ev.TouchZoom.TouchX, ev.TouchZoom.TouchY};

        OnTouchZoom.Fire(MousePos, ev.TouchZoom.Factor);
    }
}

void ClientEngine::Net_OnConnect(ClientConnection::ConnectResult result)
{
    FO_STACK_TRACE_ENTRY();

    if (result == ClientConnection::ConnectResult::Success) {
        FO_VERIFY_AND_THROW(!_curPlayer, "Cur player is already set");
        _curPlayer = SafeAlloc::MakeRefCounted<PlayerView>(this, ident_t {});
        OnConnected.Fire();
    }
    else if (result == ClientConnection::ConnectResult::CompatibilityOutdated) {
        throw ResourcesOutdatedException("Binary outdated");
    }
    else if (result == ClientConnection::ConnectResult::UpdaterOutdated) {
        throw ResourcesOutdatedException("Updater outdated");
    }
    else {
        OnConnectingFailed.Fire();
    }
}

void ClientEngine::Net_OnDisconnect()
{
    FO_STACK_TRACE_ENTRY();

    UnloadMap();

    if (_curPlayer) {
        auto player = GetCurPlayer();
        FO_VERIFY_AND_THROW(player, "Player is null");
        player->DestroySelf();
        _curPlayer.reset();
    }

    DestroyInnerEntities();

    _connectionRequest = false;
    OnDisconnected.Fire();
}

void ClientEngine::HandleOutboundRemoteCall(hstring name, ptr<Entity> caller, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(caller);

    _conn.OutBuf->StartMsg(NetMessage::RemoteCall);
    _conn.OutBuf->Write<hstring>(name);
    _conn.OutBuf->Write<int32_t>(numeric_cast<int32_t>(data.size()));
    _conn.OutBuf->Push(data);
    _conn.OutBuf->EndMsg();
}

void ClientEngine::HandleUnresolvedHash(hstring::hash_t hash)
{
    FO_STACK_TRACE_ENTRY();

    if (!_conn.IsConnected() || _conn.OutBuf->IsMsgStarted()) {
        return;
    }

    _conn.OutBuf->StartMsg(NetMessage::UnresolvedHash);
    _conn.OutBuf->Write(hash);
    _conn.OutBuf->EndMsg();

    _conn.FlushPendingData();
}

void ClientEngine::Net_SendDir(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_curMap, "No current map");
    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");

    _conn.OutBuf->StartMsg(NetMessage::SendCritterDir);
    _conn.OutBuf->Write(map->GetId());
    _conn.OutBuf->Write(cr->GetId());
    _conn.OutBuf->Write(cr->GetDir());
    _conn.OutBuf->EndMsg();
}

void ClientEngine::Net_SendMove(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cr->IsMoving(), "Critter is not moving");
    FO_VERIFY_AND_THROW(_curMap, "No current map");

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto moving = cr->GetMoving();
    FO_VERIFY_AND_THROW(moving, "Missing active movement state");

    if (std::cmp_greater(moving->GetSteps().size(), Settings->MaxPathFindLength)) {
        BreakIntoDebugger();
        cr->StopMoving();
        return;
    }

    _conn.OutBuf->StartMsg(NetMessage::SendCritterMove);
    _conn.OutBuf->Write(map->GetId());
    _conn.OutBuf->Write(cr->GetId());
    _conn.OutBuf->Write(moving->GetSpeed());
    _conn.OutBuf->Write(moving->GetStartHex());
    _conn.OutBuf->Write(numeric_cast<uint16_t>(moving->GetSteps().size()));
    for (const auto step : moving->GetSteps()) {
        _conn.OutBuf->Write(step.hex());
    }
    _conn.OutBuf->Write(numeric_cast<uint16_t>(moving->GetControlSteps().size()));
    for (const auto control_step : moving->GetControlSteps()) {
        _conn.OutBuf->Write(control_step);
    }
    _conn.OutBuf->Write(moving->GetEndHexOffset());
    _conn.OutBuf->EndMsg();
}

void ClientEngine::Net_SendStopMove(ptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_curMap, "No current map");
    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");

    _conn.OutBuf->StartMsg(NetMessage::SendStopCritterMove);
    _conn.OutBuf->Write(map->GetId());
    _conn.OutBuf->Write(cr->GetId());
    _conn.OutBuf->Write(cr->GetHex());
    _conn.OutBuf->Write(cr->GetHexOffset());
    _conn.OutBuf->Write(cr->GetDir());
    _conn.OutBuf->EndMsg();
}

void ClientEngine::Net_SendProperty(NetProperty type, ptr<const Property> prop, ptr<const Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    const auto props = entity->GetProperties();
    props->ValidateForRawData(prop);
    const auto prop_raw_data = props->GetRawData(prop);

    _conn.OutBuf->StartMsg(NetMessage::SendProperty);
    _conn.OutBuf->Write(numeric_cast<uint32_t>(prop_raw_data.size()));
    _conn.OutBuf->Write(type);

    switch (type) {
    case NetProperty::CritterItem: {
        auto client_entity = entity.dyn_cast<const ClientEntity>();
        FO_VERIFY_AND_THROW(client_entity, "Missing client entity instance");
        auto item = client_entity.dyn_cast<const ItemView>();
        FO_VERIFY_AND_THROW(item, "Missing item instance");
        _conn.OutBuf->Write(item->GetCritterId());
        _conn.OutBuf->Write(client_entity->GetId());
    } break;
    case NetProperty::Critter: {
        auto client_entity = entity.dyn_cast<const ClientEntity>();
        FO_VERIFY_AND_THROW(client_entity, "Missing client entity instance");
        _conn.OutBuf->Write(client_entity->GetId());
    } break;
    case NetProperty::MapItem: {
        auto client_entity = entity.dyn_cast<const ClientEntity>();
        FO_VERIFY_AND_THROW(client_entity, "Missing client entity instance");
        _conn.OutBuf->Write(client_entity->GetId());
    } break;
    case NetProperty::ChosenItem: {
        auto client_entity = entity.dyn_cast<const ClientEntity>();
        FO_VERIFY_AND_THROW(client_entity, "Missing client entity instance");
        _conn.OutBuf->Write(client_entity->GetId());
    } break;
    default:
        break;
    }

    _conn.OutBuf->Write(prop->GetRegIndex());
    _conn.OutBuf->Push(prop_raw_data);
    _conn.OutBuf->EndMsg();
}

void ClientEngine::Net_OnInitData()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size = _conn.InBuf->Read<uint32_t>();

    vector<uint8_t> data;
    data.resize(data_size);

    _conn.InBuf->Pop(data.data(), data_size);

    _conn.InBuf->ReadPropsData(_globalsPropertiesData);
    const auto time = _conn.InBuf->Read<synctime>();

    RestoreData(_globalsPropertiesData);
    GameTime.SetSynchronizedTime(time);
    SetSynchronizedTime(time);

    if (!data.empty()) {
        FileSystem resources;
        resources.AddDirSource(Settings->ClientResources, false, true, true);

        if (!Settings->UserWritablePath.empty()) {
            // Installed client: self-update resource patches live in the per-user writable dir; layer
            // it on top so the up-to-date file wins the size/hash check below.
            resources.AddDirSource(fs_make_writable_path(Settings->UserWritablePath, Settings->ClientResources), false, true, true);
        }

        auto reader = DataReader(data);

        while (true) {
            const auto name_len = reader.Read<int16_t>();

            if (name_len == -1) {
                break;
            }

            FO_VERIFY_AND_THROW(name_len > 0, "Name len must be positive", name_len);
            const size_t fname_size = numeric_cast<size_t>(name_len);
            string fname;
            fname.resize(fname_size);
            reader.ReadStringBytes(fname);
            const auto size = reader.Read<uint64_t>();
            const auto hash = reader.Read<uint64_t>();
            const auto target = reader.Read<UpdateFileTarget>();
            const auto data_index = reader.Read<uint32_t>();

            ignore_unused(hash);
            ignore_unused(data_index);

            if (target != UpdateFileTarget::ClientResources) {
                continue;
            }

            // Check size
            if (auto file = resources.ReadFileHeader(fname)) {
                if (file.GetSize() == size) {
                    continue;
                }
            }

            if (IsPackaged()) {
                throw ResourcesOutdatedException("Resource pack outdated", fname);
            }
        }

        reader.VerifyEnd();
    }
}

void ClientEngine::Net_OnHashList()
{
    FO_STACK_TRACE_ENTRY();

    const auto count = _conn.InBuf->Read<uint32_t>();

    for (uint32_t i = 0; i < count; i++) {
        const auto str = _conn.InBuf->Read<string>();

        // Learn the string locally so the matching hash now resolves; same hash function as the server
        Hashes.ToHashedString(str);
    }

    if (count != 0) {
        WriteLog("Learned {} previously unresolved hash(es) from server", count);
    }
}

void ClientEngine::Net_OnLoginSuccess()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Authentication success");

    const auto player_id = _conn.InBuf->Read<ident_t>();
    _conn.InBuf->ReadPropsData(_globalsPropertiesData);
    _conn.InBuf->ReadPropsData(_playerPropertiesData);

    RestoreData(_globalsPropertiesData);

    FO_VERIFY_AND_THROW(_curPlayer, "Current player is null");
    auto player = GetCurPlayer();
    FO_VERIFY_AND_THROW(player, "Player is null");
    FO_VERIFY_AND_THROW(!player->GetId(), "Current player already has an assigned id");
    player->SetId(player_id, true);
    player->RestoreData(_playerPropertiesData);

    FO_VERIFY_AND_THROW(!HasInnerEntities(), "Has inner entities is already set");
    ReceiveCustomEntities(this);

    OnLoginSuccess.Fire();
}

void ClientEngine::Net_OnAddCritter()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto pid = _conn.InBuf->Read<hstring>(Hashes);
    const auto hex = _conn.InBuf->Read<mpos>();
    const auto hex_offset = _conn.InBuf->Read<ipos16>();
    const auto dir = _conn.InBuf->Read<mdir>();
    const auto cond = _conn.InBuf->Read<CritterCondition>();
    const auto is_controlled_by_player = _conn.InBuf->Read<bool>();
    const auto is_player_offline = _conn.InBuf->Read<bool>();
    const auto is_chosen = _conn.InBuf->Read<bool>();
    _conn.InBuf->ReadPropsData(_tempPropertiesData);

    nptr<CritterHexView> hex_cr;
    refcount_ptr<CritterView> cr = [&]() -> refcount_ptr<CritterView> {
        if (_curMap) {
            auto map = GetCurMap();
            FO_VERIFY_AND_THROW(map, "Map is null");
            auto added_cr = map->AddReceivedCritter(cr_id, pid, hex, dir, _tempPropertiesData, _mapLoaded);
            hex_cr = added_cr;

            return added_cr.hold_ref();
        }

        auto proto = GetProtoCritter(pid);
        FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

        size_t erase_index = _globalMapCritters.size();

        for (size_t i = 0; i < _globalMapCritters.size(); i++) {
            if (_globalMapCritters[i]->GetId() == cr_id) {
                erase_index = i;
                break;
            }
        }

        if (erase_index != _globalMapCritters.size()) {
            BreakIntoDebugger();
            _globalMapCritters[erase_index]->MarkAsDestroyed();
            _globalMapCritters.erase(_globalMapCritters.begin() + numeric_cast<ptrdiff_t>(erase_index));
        }

        auto global_cr = SafeAlloc::MakeRefCounted<CritterView>(this, cr_id, proto.as_ptr());
        global_cr->RestoreData(_tempPropertiesData);
        _globalMapCritters.emplace_back(global_cr);

        return global_cr;
    }();

    ReceiveCustomEntities(cr);

    cr->SetHexOffset(hex_offset);
    cr->SetCondition(cond);
    cr->SetControlledByPlayer(is_controlled_by_player);
    cr->SetIsChosen(is_chosen);
    cr->SetIsPlayerOffline(is_player_offline);

    // Initial items
    const auto items_count = _conn.InBuf->Read<uint32_t>();

    for (uint32_t i = 0; i < items_count; i++) {
        const auto item_id = _conn.InBuf->Read<ident_t>();
        const auto item_pid = _conn.InBuf->Read<hstring>(Hashes);
        const auto item_slot = _conn.InBuf->Read<CritterItemSlot>();
        _conn.InBuf->ReadPropsData(_tempPropertiesData);

        auto proto = GetProtoItem(item_pid);
        FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

        auto item = cr->AddReceivedInvItem(item_id, proto.as_ptr(), item_slot, _tempPropertiesData);

        ReceiveCustomEntities(item);
    }

    // Visibility mode (sent regardless of context; default Full for global map)
    const auto vis_mode = _conn.InBuf->Read<CritterVisibilityMode>();
    cr->SetVisibilityMode(vis_mode);

    // Initial attachment
    const auto is_attached = _conn.InBuf->Read<bool>();

    const auto attached_critters_count = _conn.InBuf->Read<uint16_t>();
    vector<ident_t> attached_critters;
    attached_critters.resize(attached_critters_count);

    for (uint16_t i = 0; i < attached_critters_count; i++) {
        attached_critters[i] = _conn.InBuf->Read<ident_t>();
    }

    cr->SetIsAttached(is_attached);
    cr->SetAttachedCritters(std::move(attached_critters));

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");

        if (is_attached) {
            span<refcount_ptr<CritterHexView>> map_critters = map->GetCritters();

            for (size_t i = 0; i < map_critters.size(); i++) {
                auto map_cr = map_critters[i].as_ptr();

                if (map_cr->IsAttachedCritter(cr_id)) {
                    map_cr->MoveAttachedCritters();
                    break;
                }
            }
        }

        FO_VERIFY_AND_THROW(hex_cr, "Attached-critter hex critter must be resolved on the current map");

        if (hex_cr->HasAttachedCritters()) {
            hex_cr->MoveAttachedCritters();
        }
    }

    // Initial moving
    const auto is_moving = _conn.InBuf->Read<bool>();

    if (is_moving) {
        ReceiveCritterMoving(hex_cr);
    }

    if (hex_offset != ipos16 {} && hex_cr) {
        hex_cr->RefreshOffs();
    }

    if (is_chosen) {
        _chosen = cr;
    }

    if (hex_cr) {
        FO_VERIFY_AND_THROW(_curMap, "No current map");
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");

#if FO_ENABLE_3D
        if (hex_cr->IsModel()) {
            auto model = hex_cr->GetModel();
            FO_VERIFY_AND_THROW(model, "Critter model is null");
            model->PrewarmParticles();
            model->StartMeshGeneration();
        }
#endif

        if (is_chosen) {
            map->RebuildFog();
        }

        hex_cr->RefreshView(true);
        hex_cr->RefreshOffs();

        if (items_count != 0 && !is_chosen) {
            map->UpdateCritterLightSource(hex_cr);
        }
    }

    OnCritterIn.Fire(cr);
}

void ClientEngine::Net_OnRemoveCritter()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        auto cr = map->GetCritter(cr_id);

        if (!cr) {
            BreakIntoDebugger();
            return;
        }

        cr->Finish();

        OnCritterOut.Fire(cr);

        if (GetChosen() == cr) {
            _chosen.reset();
        }
    }
    else {
        size_t erase_index = _globalMapCritters.size();

        for (size_t i = 0; i < _globalMapCritters.size(); i++) {
            if (_globalMapCritters[i]->GetId() == cr_id) {
                erase_index = i;
                break;
            }
        }

        if (erase_index == _globalMapCritters.size()) {
            BreakIntoDebugger();
            return;
        }

        auto cr = copy(_globalMapCritters[erase_index]);

        OnCritterOut.Fire(cr);
        _globalMapCritters.erase(_globalMapCritters.begin() + numeric_cast<ptrdiff_t>(erase_index));

        if (GetChosen().as_ptr() == cr) {
            _chosen.reset();
        }

        cr->DestroySelf();
    }
}

void ClientEngine::Net_OnCritterVisibilityMode()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto mode = _conn.InBuf->Read<CritterVisibilityMode>();

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");

        if (auto cr = map->GetCritter(cr_id)) {
            cr->SetVisibilityMode(mode);
            OnCritterVisibilityModeChanged.Fire(cr, mode);
        }
        else {
            BreakIntoDebugger();
        }
    }
}

void ClientEngine::Net_OnInfoMessage()
{
    FO_STACK_TRACE_ENTRY();

    const auto info_message = _conn.InBuf->Read<EngineInfoMessage>();
    const auto extra_text = _conn.InBuf->Read<string>();

    OnInfoMessage.Fire(info_message, extra_text);
}

void ClientEngine::Net_OnCritterDir()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto dir = _conn.InBuf->Read<mdir>();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto cr = map->GetCritter(cr_id);

    if (cr) {
        if (cr->GetControlledByPlayer()) {
            cr->ChangeLookDir(dir);
        }
        else {
            cr->ChangeDir(dir);
        }
    }
}

void ClientEngine::Net_OnCritterMove()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    nptr<CritterHexView> cr;

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        cr = map->GetCritter(cr_id);
    }

    ReceiveCritterMoving(cr);

    if (cr) {
        cr->RefreshView();
    }
}

void ClientEngine::Net_OnCritterMoveSpeed()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto speed = _conn.InBuf->Read<uint16_t>();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto cr = map->GetCritter(cr_id);

    if (!cr) {
        return;
    }

    if (!cr->IsMoving()) {
        return;
    }

    auto moving = cr->GetMoving();
    FO_VERIFY_AND_THROW(moving, "Missing active movement state");

    if (speed == moving->GetSpeed()) {
        return;
    }

    moving->ChangeSpeed(speed, GameTime.GetFrameTime());

    cr->RefreshView();
}

void ClientEngine::Net_OnCritterAction()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto action = _conn.InBuf->Read<CritterAction>();
    const auto action_ext = _conn.InBuf->Read<int32_t>();
    const auto is_context_item = _conn.InBuf->Read<bool>();

    refcount_nptr<ItemView> context_item {};

    if (is_context_item) {
        const auto item_id = _conn.InBuf->Read<ident_t>();
        const auto item_pid = _conn.InBuf->Read<hstring>(Hashes);
        _conn.InBuf->ReadPropsData(_tempPropertiesData);

        auto proto = GetProtoItem(item_pid);
        FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

        context_item = SafeAlloc::MakeRefCounted<ItemView>(this, item_id, proto);
        context_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(context_item);
    }

    if (!_curMap) {
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto cr = map->GetCritter(cr_id);

    if (!cr) {
        BreakIntoDebugger();
        return;
    }

    cr->Action(action, action_ext, context_item, false);

    if (context_item) {
        context_item->MarkAsDestroyed();
    }
}

void ClientEngine::Net_OnCritterMoveItem()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto action = _conn.InBuf->Read<CritterAction>();
    const auto prev_slot = _conn.InBuf->Read<CritterItemSlot>();
    const auto cur_slot = _conn.InBuf->Read<CritterItemSlot>();
    const auto is_moved_item = _conn.InBuf->Read<bool>();

    refcount_nptr<ItemView> moved_item {};

    if (is_moved_item) {
        const auto item_id = _conn.InBuf->Read<ident_t>();
        const auto item_pid = _conn.InBuf->Read<hstring>(Hashes);
        _conn.InBuf->ReadPropsData(_tempPropertiesData);

        auto proto = GetProtoItem(item_pid);
        FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

        moved_item = SafeAlloc::MakeRefCounted<ItemView>(this, item_id, proto);
        moved_item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(moved_item);
    }

    nptr<CritterView> cr;

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        cr = map->GetCritter(cr_id);
    }
    else {
        cr = GetGlobalMapCritter(cr_id);
    }

    if (!cr) {
        BreakIntoDebugger();

        // Skip rest data
        const auto items_count = _conn.InBuf->Read<uint32_t>();

        for (uint32_t i = 0; i < items_count; i++) {
            (void)_conn.InBuf->Read<ident_t>();
            (void)_conn.InBuf->Read<hstring>(Hashes);
            (void)_conn.InBuf->Read<CritterItemSlot>();
            _conn.InBuf->ReadPropsData(_tempPropertiesData);
            ReceiveCustomEntities(nullptr);
        }

        return;
    }

    const auto items_count = _conn.InBuf->Read<uint32_t>();

    if (items_count != 0) {
        FO_VERIFY_AND_THROW(!cr->GetIsChosen(), "Critter is already marked as chosen");

        cr->DeleteAllInvItems();

        for (uint32_t i = 0; i < items_count; i++) {
            const auto item_id = _conn.InBuf->Read<ident_t>();
            const auto item_pid = _conn.InBuf->Read<hstring>(Hashes);
            const auto item_slot = _conn.InBuf->Read<CritterItemSlot>();
            _conn.InBuf->ReadPropsData(_tempPropertiesData);

            auto proto = GetProtoItem(item_pid);
            FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

            auto item = cr->AddReceivedInvItem(item_id, proto.as_ptr(), item_slot, _tempPropertiesData);

            ReceiveCustomEntities(item);
        }
    }

    if (auto hex_cr = cr.dyn_cast<CritterHexView>()) {
        hex_cr->RefreshView();
        hex_cr->Action(action, static_cast<int32_t>(prev_slot), moved_item, false);
    }

    if (moved_item && cur_slot != prev_slot && cr->GetIsChosen()) {
        if (auto item = cr->GetInvItem(moved_item->GetId())) {
            item->SetCritterSlot(cur_slot);
            moved_item->SetCritterSlot(prev_slot);
        }
    }

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");

        if (auto hex_cr = cr.dyn_cast<CritterHexView>()) {
            map->UpdateCritterLightSource(hex_cr);
        }
    }
}

void ClientEngine::Net_OnCritterTeleport()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto to_hex = _conn.InBuf->Read<mpos>();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto cr = map->GetCritter(cr_id);

    if (!cr) {
        return;
    }

    map->MoveCritter(cr, to_hex, false);

    if (cr->GetIsChosen()) {
        map->ScrollToHex(cr->GetHex(), cr->GetHexOffset(), 100, false);
    }
}

void ClientEngine::Net_OnCritterPos()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto hex = _conn.InBuf->Read<mpos>();
    const auto hex_offset = _conn.InBuf->Read<ipos16>();
    const auto dir = _conn.InBuf->Read<mdir>();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Received critter movement target is outside the current client map bounds", cr_id, hex, map->GetId(), map->GetSize());

    auto cr = map->GetCritter(cr_id);

    if (!cr) {
        return;
    }

    cr->StopMoving();

    cr->ChangeLookDir(dir);
    cr->ChangeMoveDir(dir);

    if (cr->GetHex() != hex) {
        map->MoveCritter(cr, hex, true);

        if (cr->GetIsChosen()) {
            map->RebuildFog();
        }
    }

    const auto cr_hex_offset = cr->GetHexOffset();

    if (cr_hex_offset != hex_offset) {
        cr->AddExtraOffs({cr_hex_offset.x - hex_offset.x, cr_hex_offset.y - hex_offset.y});
        cr->SetHexOffset(hex_offset);
        cr->RefreshOffs();
    }

    cr->RefreshView();
}

void ClientEngine::Net_OnCritterAttachments()
{
    FO_STACK_TRACE_ENTRY();

    const auto cr_id = _conn.InBuf->Read<ident_t>();
    const auto is_attached = _conn.InBuf->Read<bool>();
    const auto attach_master = _conn.InBuf->Read<ident_t>();

    const auto attached_critters_count = _conn.InBuf->Read<uint16_t>();
    vector<ident_t> attached_critters;
    attached_critters.resize(attached_critters_count);

    for (uint16_t i = 0; i < attached_critters_count; i++) {
        attached_critters[i] = _conn.InBuf->Read<ident_t>();
    }

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        auto cr = map->GetCritter(cr_id);

        if (!cr) {
            BreakIntoDebugger();
            return;
        }

        cr->SetAttachMaster(attach_master);

        if (cr->GetIsAttached() != is_attached) {
            cr->SetIsAttached(is_attached);

            if (is_attached) {
                span<refcount_ptr<CritterHexView>> map_critters = map->GetCritters();

                for (size_t i = 0; i < map_critters.size(); i++) {
                    auto map_cr = map_critters[i].as_ptr();

                    if (map_cr->IsAttachedCritter(cr_id)) {
                        map_cr->MoveAttachedCritters();
                        break;
                    }
                }
            }
        }

        cr->SetAttachedCritters(std::move(attached_critters));

        if (cr->HasAttachedCritters()) {
            cr->MoveAttachedCritters();
        }
    }
    else {
        auto cr = GetGlobalMapCritter(cr_id);

        if (!cr) {
            BreakIntoDebugger();
            return;
        }

        cr->SetIsAttached(is_attached);
        cr->SetAttachMaster(attach_master);
        cr->SetAttachedCritters(std::move(attached_critters));
    }
}

void ClientEngine::Net_OnChosenAddItem()
{
    FO_STACK_TRACE_ENTRY();

    const auto item_id = _conn.InBuf->Read<ident_t>();
    const auto item_pid = _conn.InBuf->Read<hstring>(Hashes);
    const auto item_slot = _conn.InBuf->Read<CritterItemSlot>();
    _conn.InBuf->ReadPropsData(_tempPropertiesData);

    auto chosen = GetChosen();

    if (!chosen) {
        WriteLog("Chosen is not created on add item");
        BreakIntoDebugger();

        // Skip rest data
        ReceiveCustomEntities(nullptr);

        return;
    }

    if (auto prev_item = chosen->GetInvItem(item_id)) {
        chosen->DeleteInvItem(prev_item);
    }

    auto proto = GetProtoItem(item_pid);
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    auto item = chosen->AddReceivedInvItem(item_id, proto.as_ptr(), item_slot, _tempPropertiesData);

    ReceiveCustomEntities(item);

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        map->RebuildFog();

        if (auto hex_chosen = chosen.dyn_cast<CritterHexView>()) {
            hex_chosen->RefreshView();
            map->UpdateCritterLightSource(hex_chosen);
        }
    }

    OnItemInvIn.Fire(item);
}

void ClientEngine::Net_OnChosenRemoveItem()
{
    FO_STACK_TRACE_ENTRY();

    const auto item_id = _conn.InBuf->Read<ident_t>();

    auto chosen = GetChosen();

    if (!chosen) {
        WriteLog("Chosen is not created in remove item");
        BreakIntoDebugger();
        return;
    }

    auto item = chosen->GetInvItem(item_id);

    if (!item) {
        // Valid case, item may be removed locally
        return;
    }

    auto item_clone = item->CreateRefClone();
    chosen->DeleteInvItem(item);

    OnItemInvOut.Fire(item_clone);

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");

        if (auto hex_chosen = chosen.dyn_cast<CritterHexView>()) {
            hex_chosen->RefreshView();
            map->UpdateCritterLightSource(hex_chosen);
        }
    }
}

void ClientEngine::Net_OnAddItemOnMap()
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = _conn.InBuf->Read<mpos>();
    const auto item_id = _conn.InBuf->Read<ident_t>();
    const auto item_pid = _conn.InBuf->Read<hstring>(Hashes);
    _conn.InBuf->ReadPropsData(_tempPropertiesData);

    if (!_curMap) {
        BreakIntoDebugger();

        // Skip rest data
        ReceiveCustomEntities(nullptr);

        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto item = map->AddReceivedItem(item_id, item_pid, hex, _tempPropertiesData, _mapLoaded);

    ReceiveCustomEntities(item);

    OnItemMapIn.Fire(item);
}

void ClientEngine::Net_OnRemoveItemFromMap()
{
    FO_STACK_TRACE_ENTRY();

    const auto item_id = _conn.InBuf->Read<ident_t>();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto item = map->GetItem(item_id);

    if (item) {
        OnItemMapOut.Fire(item);

        // Refresh borders
        if (!item->GetShootThru()) {
            map->RebuildFog();
        }

        item->Finish();
    }
}

void ClientEngine::Net_OnPlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    _mapLoaded = true;

    auto chosen = GetChosen();

    if (_curMap && chosen) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");

        map->InstantScrollTo(chosen->GetHex());

        if (auto hex_chosen = chosen.dyn_cast<CritterHexView>()) {
            hex_chosen->RefreshView();
            hex_chosen->RefreshOffs();
            map->UpdateCritterLightSource(hex_chosen);
        }
    }

    OnMapLoaded.Fire();

    WriteLog("Map loaded");
}

void ClientEngine::Net_OnProperty()
{
    FO_STACK_TRACE_ENTRY();

    const auto data_size = _conn.InBuf->Read<uint32_t>();
    const auto type = _conn.InBuf->Read<NetProperty>();

    ident_t cr_id;
    ident_t item_id;
    ident_t entity_id;

    switch (type) {
    case NetProperty::CritterItem:
        cr_id = _conn.InBuf->Read<ident_t>();
        item_id = _conn.InBuf->Read<ident_t>();
        break;
    case NetProperty::Critter:
        cr_id = _conn.InBuf->Read<ident_t>();
        break;
    case NetProperty::MapItem:
        item_id = _conn.InBuf->Read<ident_t>();
        break;
    case NetProperty::ChosenItem:
        item_id = _conn.InBuf->Read<ident_t>();
        break;
    case NetProperty::CustomEntity:
        entity_id = _conn.InBuf->Read<ident_t>();
        break;
    default:
        break;
    }

    const auto property_index = _conn.InBuf->Read<uint16_t>();

    PropertyRawData prop_data;
    _conn.InBuf->Pop(prop_data.Alloc(data_size), data_size);

    nptr<Entity> entity {};

    switch (type) {
    case NetProperty::Game:
        entity = this;
        break;
    case NetProperty::Player:
        entity = GetCurPlayer();
        break;
    case NetProperty::Critter:
        if (_curMap) {
            auto map = GetCurMap();
            FO_VERIFY_AND_THROW(map, "Map is null");
            entity = map->GetCritter(cr_id);
        }
        break;
    case NetProperty::Chosen:
        entity = GetChosen();
        break;
    case NetProperty::MapItem:
        if (_curMap) {
            auto map = GetCurMap();
            FO_VERIFY_AND_THROW(map, "Map is null");
            entity = map->GetItem(item_id);
        }
        break;
    case NetProperty::CritterItem:
        if (_curMap) {
            auto map = GetCurMap();
            FO_VERIFY_AND_THROW(map, "Map is null");

            if (auto cr = map->GetCritter(cr_id)) {
                entity = cr->GetInvItem(item_id);
            }
        }
        break;
    case NetProperty::ChosenItem:
        if (auto chosen = GetChosen()) {
            entity = chosen->GetInvItem(item_id);
        }
        break;
    case NetProperty::Map:
        entity = GetCurMap();
        break;
    case NetProperty::Location:
        entity = GetCurLocation();
        break;
    case NetProperty::CustomEntity:
        entity = GetEntity(entity_id);
        break;
    default:
        FO_UNREACHABLE_PLACE();
    }

    if (!entity) {
        BreakIntoDebugger();
        return;
    }

    auto prop = entity->GetProperties()->GetRegistrator()->GetPropertyByIndex(property_index);

    if (!prop) {
        BreakIntoDebugger();
        return;
    }

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    {
        _sendIgnoreEntity = entity;
        _sendIgnoreProperty = prop;

        auto revert_send_ignore = scope_exit([this]() noexcept {
            _sendIgnoreEntity = nullptr;
            _sendIgnoreProperty = nullptr;
        });

        entity->SetValueFromData(prop, prop_data);
    }
}

void ClientEngine::Net_OnTimeSync()
{
    FO_STACK_TRACE_ENTRY();

    const auto time = _conn.InBuf->Read<synctime>();

    GameTime.SetSynchronizedTimeMonotonic(time);
    SetSynchronizedTime(GameTime.GetSynchronizedTime());
}

void ClientEngine::Net_OnLoadMap()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Change map");

    const auto loc_id = _conn.InBuf->Read<ident_t>();
    const auto map_id = _conn.InBuf->Read<ident_t>();
    const auto loc_pid = _conn.InBuf->Read<hstring>(Hashes);
    const auto map_pid = _conn.InBuf->Read<hstring>(Hashes);
    const auto map_index_in_loc = _conn.InBuf->Read<int32_t>();

    if (map_pid) {
        _conn.InBuf->ReadPropsData(_tempPropertiesData);
        _conn.InBuf->ReadPropsData(_tempPropertiesDataExt);
    }

    UnloadMap();

    _curMapLocPid = loc_pid;
    _curMapIndexInLoc = map_index_in_loc;

    if (map_pid) {
        auto loc_proto = GetProtoLocation(loc_pid);
        FO_VERIFY_AND_THROW(loc_proto, "Missing required location prototype");
        auto map_proto = GetProtoMap(map_pid);
        FO_VERIFY_AND_THROW(map_proto, "Missing required map prototype");

        isize32 screen_size = {Settings->ScreenWidth, Settings->ScreenHeight};
        OnPreLoadMap.Fire(loc_pid, map_pid, screen_size);

        _curLocation = SafeAlloc::MakeRefCounted<LocationView>(this, loc_id, loc_proto);
        auto location = GetCurLocation();
        FO_VERIFY_AND_THROW(location, "Location is null");
        location->RestoreData(_tempPropertiesDataExt);

        _curMap = SafeAlloc::MakeRefCounted<MapView>(this, map_id, map_proto, screen_size);
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        map->RestoreData(_tempPropertiesData);
        map->LoadStaticData();

        ReceiveCustomEntities(location);
        ReceiveCustomEntities(map);

        WriteLog("Start load map");
    }
    else {
        WriteLog("Start load global map");
    }

    OnMapLoad.Fire();
}

void ClientEngine::Net_OnSomeItems()
{
    FO_STACK_TRACE_ENTRY();

    const auto context_param = any_t(_conn.InBuf->Read<string>());
    const auto items_count = _conn.InBuf->Read<uint32_t>();

    vector<refcount_ptr<ItemView>> items;
    items.reserve(items_count);

    for (uint32_t i = 0; i < items_count; i++) {
        const auto item_id = _conn.InBuf->Read<ident_t>();
        const auto pid = _conn.InBuf->Read<hstring>(Hashes);
        _conn.InBuf->ReadPropsData(_tempPropertiesData);
        FO_VERIFY_AND_THROW(item_id, "Item id is empty");

        auto proto = GetProtoItem(pid);
        FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

        auto item = SafeAlloc::MakeRefCounted<ItemView>(this, item_id, proto.as_ptr());
        item->RestoreData(_tempPropertiesData);

        ReceiveCustomEntities(item);

        items.emplace_back(item);
    }

    const auto items2 = vec_transform(items, [](auto&& item) -> ptr<ItemView> { return item; });
    OnReceiveItems.Fire(items2, context_param);
}

void ClientEngine::Net_OnViewMap()
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = _conn.InBuf->Read<mpos>();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    OnMapView.Fire(hex);
}

void ClientEngine::Net_OnRemoteCall()
{
    FO_STACK_TRACE_ENTRY();

    const auto remote_call_name = _conn.InBuf->Read<hstring>(Hashes);
    const auto data_size = _conn.InBuf->Read<int32_t>();

    _remoteCallData.resize(data_size);

    _conn.InBuf->Pop(_remoteCallData.data(), data_size);

    HandleInboundRemoteCall(remote_call_name, nullptr, _remoteCallData);
}

void ClientEngine::Net_OnAddCustomEntity()
{
    FO_STACK_TRACE_ENTRY();

    const auto holder_id = _conn.InBuf->Read<ident_t>();
    const auto holder_entry = _conn.InBuf->Read<hstring>(Hashes);
    const auto id = _conn.InBuf->Read<ident_t>();
    const auto pid = _conn.InBuf->Read<hstring>(Hashes);
    _conn.InBuf->ReadPropsData(_tempPropertiesDataCustomEntity);

    nptr<Entity> holder;

    if (holder_id) {
        holder = GetEntity(holder_id);

        if (!holder) {
            BreakIntoDebugger();
            return;
        }
    }
    else {
        holder = this;
    }

    auto entity = CreateCustomEntityView(holder.as_ptr(), holder_entry, id, pid, _tempPropertiesDataCustomEntity);

    OnCustomEntityIn.Fire(entity);
}

void ClientEngine::Net_OnRemoveCustomEntity()
{
    FO_STACK_TRACE_ENTRY();

    const auto id = _conn.InBuf->Read<ident_t>();

    auto entity = GetEntity(id);

    if (!entity) {
        BreakIntoDebugger();
        return;
    }

    auto entity_ref_holder = entity.hold_ref();
    auto custom_entity = entity.dyn_cast<CustomEntityView>();

    if (!custom_entity) {
        BreakIntoDebugger();
        return;
    }

    OnCustomEntityOut.Fire(custom_entity);

    nptr<Entity> holder;

    if (custom_entity->GetCustomHolderId()) {
        holder = GetEntity(custom_entity->GetCustomHolderId());
    }
    else {
        holder = this;
    }

    if (holder) {
        holder->RemoveInnerEntity(custom_entity->GetCustomHolderEntry(), custom_entity);
    }
    else {
        BreakIntoDebugger();
    }

    custom_entity->DestroySelf();
}

void ClientEngine::ReceiveCustomEntities(nptr<Entity> holder)
{
    FO_STACK_TRACE_ENTRY();

    const auto entries_count = _conn.InBuf->Read<uint16_t>();

    if (entries_count == 0) {
        return;
    }

    for (uint16_t i = 0; i < entries_count; i++) {
        const auto entry = _conn.InBuf->Read<hstring>(Hashes);
        const auto entities_count = _conn.InBuf->Read<uint32_t>();

        for (uint32_t j = 0; j < entities_count; j++) {
            const auto id = _conn.InBuf->Read<ident_t>();
            const auto pid = _conn.InBuf->Read<hstring>(Hashes);
            _conn.InBuf->ReadPropsData(_tempPropertiesDataCustomEntity);

            if (holder) {
                auto entity = CreateCustomEntityView(holder.as_ptr(), entry, id, pid, _tempPropertiesDataCustomEntity);

                ReceiveCustomEntities(entity);
            }
            else {
                ReceiveCustomEntities(nullptr);
            }
        }
    }
}

auto ClientEngine::CreateCustomEntityView(ptr<Entity> holder, hstring entry, ident_t id, hstring pid, const vector<vector<uint8_t>>& data) -> ptr<CustomEntityView>
{
    FO_STACK_TRACE_ENTRY();

    const auto type_name = GetEntityType(holder->GetTypeName()).HolderEntries.at(entry).TargetType;

    FO_VERIFY_AND_THROW(IsValidEntityType(type_name), "Invalid entity type name");

    const bool has_protos = GetEntityType(type_name).HasProtos;
    nptr<const ProtoEntity> proto;

    if (pid) {
        FO_VERIFY_AND_THROW(has_protos, "Pid provided for an entity type without protos");
        proto = GetProtoEntity(type_name, pid);
        FO_VERIFY_AND_THROW(proto, "Missing prototype instance");
    }
    else {
        FO_VERIFY_AND_THROW(!has_protos, "Has protos is already set");
    }

    auto registrator = GetPropertyRegistrator(type_name);
    FO_VERIFY_AND_THROW(registrator, "Missing property registrator");

    refcount_ptr<CustomEntityView> entity = [&]() -> refcount_ptr<CustomEntityView> {
        if (proto) {
            return SafeAlloc::MakeRefCounted<CustomEntityWithProtoView>(this, id, registrator.as_ptr(), proto.as_ptr());
        }

        return SafeAlloc::MakeRefCounted<CustomEntityView>(this, id, registrator.as_ptr(), nullptr, nullptr);
    }();

    entity->RestoreData(data);

    if (auto holder_with_id = holder.dyn_cast<ClientEntity>()) {
        entity->SetCustomHolderId(holder_with_id->GetId());
    }
    else {
        FO_VERIFY_AND_THROW(holder == this, "Custom client entity holder must be a client entity or the client engine", id, entry, holder->GetTypeName(), holder->GetId());
    }

    entity->SetCustomHolderEntry(entry);
    holder->AddInnerEntity(entry, entity);
    return entity;
}

void ClientEngine::ReceiveCritterMoving(nptr<CritterHexView> cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto whole_time = _conn.InBuf->Read<uint32_t>();
    const auto offset_time = _conn.InBuf->Read<uint32_t>();
    const auto speed = _conn.InBuf->Read<uint16_t>();
    const auto start_hex = _conn.InBuf->Read<mpos>();

    const auto steps_count = _conn.InBuf->Read<uint16_t>();
    vector<mdir> steps;
    steps.resize(steps_count);

    for (uint16_t i = 0; i < steps_count; i++) {
        steps[i] = mdir(_conn.InBuf->Read<hdir>());
    }

    const auto control_steps_count = _conn.InBuf->Read<uint16_t>();
    vector<uint16_t> control_steps;
    control_steps.resize(control_steps_count);

    for (uint16_t i = 0; i < control_steps_count; i++) {
        control_steps[i] = _conn.InBuf->Read<uint16_t>();
    }

    const auto end_hex_offset = _conn.InBuf->Read<ipos16>();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    if (!cr) {
        BreakIntoDebugger();
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");

    cr->StopMoving();

    ipos16 start_hex_offset = cr->GetHexOffset();

    if (offset_time == 0 && start_hex != cr->GetHex()) {
        const auto cr_offset = GeometryHelper::GetHexOffset(start_hex, cr->GetHex());
        start_hex_offset = {numeric_cast<int16_t>(start_hex_offset.x + cr_offset.x), numeric_cast<int16_t>(start_hex_offset.y + cr_offset.y)};
    }

    cr->SetMoving(SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), speed, std::move(steps), std::move(control_steps), GameTime.GetFrameTime(), std::chrono::milliseconds {offset_time}, start_hex, start_hex_offset, end_hex_offset, numeric_cast<float32_t>(whole_time)));
    auto moving = cr->GetMoving();
    FO_VERIFY_AND_THROW(moving, "Missing active movement state");
    moving->ValidateRuntimeState();
}

auto ClientEngine::GetEntity(ident_t id) -> nptr<ClientEntity>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _allEntities.find(id);

    if (it == _allEntities.end()) {
        return nullptr;
    }

    return it->second;
}

void ClientEngine::RegisterEntity(ptr<ClientEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity->GetId(), "Entity has no assigned id");

    _allEntities.insert_or_assign(entity->GetId(), entity);
}

void ClientEngine::UnregisterEntity(ptr<ClientEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity->GetId(), "Entity has no assigned id");

    _allEntities.erase(entity->GetId());
}

auto ClientEngine::AnimLoad(hstring name, AtlasType atlas_type) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _ifaceAnimationsCache.find(name); it != _ifaceAnimationsCache.end()) {
        auto& cached_anim = it->second;

        cached_anim->Anim->PlayDefault();

        _ifaceAnimations.emplace(++_ifaceAnimCounter, std::move(cached_anim));
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

    _ifaceAnimations.emplace(++_ifaceAnimCounter, std::move(iface_anim));

    return _ifaceAnimCounter;
}

void ClientEngine::AnimFree(uint32_t anim_id)
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _ifaceAnimations.find(anim_id); it != _ifaceAnimations.end()) {
        auto& iface_anim = it->second;

        iface_anim->Anim->Stop();

        _ifaceAnimationsCache.emplace(iface_anim->Name, std::move(iface_anim));
        _ifaceAnimations.erase(it);
    }
}

auto ClientEngine::AnimGetSpr(uint32_t anim_id) -> nptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    if (anim_id == 0) {
        return nullptr;
    }

    if (const auto it = _ifaceAnimations.find(anim_id); it != _ifaceAnimations.end()) {
        return it->second->Anim;
    }

    return nullptr;
}

void ClientEngine::OnSendGlobalValue(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity == this, "Global property sender received an entity different from the client engine", string_view {prop->GetName()}, entity->GetTypeName(), entity->GetId());

    if (_sendIgnoreEntity == entity && _sendIgnoreProperty == prop) {
        return;
    }

    if (prop->IsModifiableByAnyClient()) {
        Net_SendProperty(NetProperty::Game, prop, this);
    }
    else {
        throw GenericException("Unable to send global modifiable property", prop->GetName());
    }
}

void ClientEngine::OnSendPlayerValue(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const Entity> cur_player = GetCurPlayer();
    FO_VERIFY_AND_THROW(cur_player == entity, "Player property sender received an entity different from the current player", prop->GetName(), entity->GetTypeName(), entity->GetId(), _curPlayer ? _curPlayer->GetId() : ident_t {});

    if (_sendIgnoreEntity == entity && _sendIgnoreProperty == prop) {
        return;
    }

    FO_VERIFY_AND_THROW(_curPlayer, "No current player");
    auto player = GetCurPlayer();
    FO_VERIFY_AND_THROW(player, "Player is null");

    if (!player->GetId()) {
        throw ScriptException("Can't modify player public/protected property on unlogined player");
    }

    Net_SendProperty(NetProperty::Player, prop, player);
}

void ClientEngine::OnSendCritterValue(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    if (_sendIgnoreEntity == entity && _sendIgnoreProperty == prop) {
        return;
    }

    auto cr = entity.dyn_cast<CritterView>();
    FO_VERIFY_AND_THROW(cr, "Missing critter instance");

    if (cr->GetIsChosen()) {
        Net_SendProperty(NetProperty::Chosen, prop, cr);
    }
    else if (prop->IsModifiableByAnyClient()) {
        Net_SendProperty(NetProperty::Critter, prop, cr);
    }
    else {
        throw GenericException("Unable to send critter modifiable property", prop->GetName());
    }
}

void ClientEngine::OnSendItemValue(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    if (_sendIgnoreEntity == entity && _sendIgnoreProperty == prop) {
        return;
    }

    auto item = entity.dyn_cast<const ItemView>();

    if (item) {
        if (item->GetStatic() || !item->GetId()) {
            return;
        }

        if (item->GetOwnership() == ItemOwnership::CritterInventory) {
            nptr<const CritterView> cr;

            if (_curMap) {
                auto map = GetCurMap();
                FO_VERIFY_AND_THROW(map, "Map is null");
                cr = map->GetCritter(item->GetCritterId());
            }
            else {
                cr = GetGlobalMapCritter(item->GetCritterId());
            }

            if (cr) {
                if (cr->GetIsChosen()) {
                    Net_SendProperty(NetProperty::ChosenItem, prop, item);
                    return;
                }

                if (prop->IsModifiableByAnyClient()) {
                    Net_SendProperty(NetProperty::CritterItem, prop, item);
                    return;
                }
            }

            throw GenericException("Unable to send item (critter) modifiable property", prop->GetName());
        }
        else if (item->GetOwnership() == ItemOwnership::MapHex) {
            if (prop->IsModifiableByAnyClient()) {
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

void ClientEngine::OnSendMapValue(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const Entity> cur_map_entity = GetCurMap();
    FO_VERIFY_AND_THROW(cur_map_entity == entity, "Map property sender received an entity different from the current map", prop->GetName(), entity->GetTypeName(), entity->GetId(), _curMap ? _curMap->GetId() : ident_t {});

    if (_sendIgnoreEntity == entity && _sendIgnoreProperty == prop) {
        return;
    }

    auto cur_map = GetCurMap();
    FO_VERIFY_AND_THROW(cur_map, "Current map is null");

    if (prop->IsModifiableByAnyClient()) {
        Net_SendProperty(NetProperty::Map, prop, cur_map);
    }
    else {
        throw GenericException("Unable to send map modifiable property", prop->GetName());
    }
}

void ClientEngine::OnSendLocationValue(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const Entity> cur_location_entity = GetCurLocation();
    FO_VERIFY_AND_THROW(cur_location_entity == entity, "Location property sender received an entity different from the current location", prop->GetName(), entity->GetTypeName(), entity->GetId(), _curLocation ? _curLocation->GetId() : ident_t {});

    if (_sendIgnoreEntity == entity && _sendIgnoreProperty == prop) {
        return;
    }

    auto cur_location = GetCurLocation();
    FO_VERIFY_AND_THROW(cur_location, "Current location is null");

    if (prop->IsModifiableByAnyClient()) {
        Net_SendProperty(NetProperty::Location, prop, cur_location);
    }
    else {
        throw GenericException("Unable to send location modifiable property", prop->GetName());
    }
}

void ClientEngine::OnSetCritterLookDistance(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto cr = entity.dyn_cast<CritterHexView>()) {
        if (cr->GetIsChosen()) {
            auto map = cr->GetMap();
            map->RebuildFog();
        }
    }
}

void ClientEngine::OnSetCritterModelName(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto cr = entity.dyn_cast<CritterHexView>()) {
#if FO_ENABLE_3D
        cr->RefreshModel();

        if (cr->IsModel()) {
            auto model = cr->GetModel();
            FO_VERIFY_AND_THROW(model, "Critter model is null");
            model->StartMeshGeneration();
        }
#endif

        cr->Action(CritterAction::Refresh, 0, nullptr, false);
    }
}

void ClientEngine::OnSetCritterHideSprite(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto cr = entity.dyn_cast<CritterHexView>()) {
        cr->SetSpriteVisiblity(!cr->GetHideSprite());
    }
}

void ClientEngine::OnSetCritterElevation(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto cr = entity.dyn_cast<CritterHexView>()) {
        cr->RefreshSprite();
    }
}

void ClientEngine::OnSetItemFlags(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    // Colorize, ColorizeColor, ShootThru, LightThru, NoBlock

    if (auto item = entity.dyn_cast<ItemHexView>()) {
        bool rebuild_cache = false;

        auto colorize_prop = item->GetPropertyColorize();
        auto colorize_color_prop = item->GetPropertyColorizeColor();
        auto shoot_thru_prop = item->GetPropertyShootThru();
        auto light_thru_prop = item->GetPropertyLightThru();
        auto no_block_prop = item->GetPropertyNoBlock();

        if (prop == colorize_prop || prop == colorize_color_prop) {
            item->RefreshAlpha();
            item->RefreshSprite();
        }
        else if (prop == shoot_thru_prop) {
            auto map = item->GetMap();
            map->RebuildFog();
            rebuild_cache = true;
        }
        else if (prop == light_thru_prop) {
            auto map = item->GetMap();
            map->UpdateHexLightSources(item->GetHex());
            rebuild_cache = true;
        }
        else if (prop == no_block_prop) {
            rebuild_cache = true;
        }

        if (rebuild_cache) {
            auto map = item->GetMap();
            map->RecacheHexFlags(item->GetHex());
        }
    }
}

void ClientEngine::OnSetItemSomeLight(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    // LightSource, LightIntensity, LightDistance, LightFlags, LightColor.

    ignore_unused(prop);

    if (auto hex_item = entity.dyn_cast<ItemHexView>()) {
        auto map = hex_item->GetMap();
        map->UpdateItemLightSource(hex_item);
    }
}

void ClientEngine::OnSetCritterLight(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    // Re-apply the critter's light fan after a single bundled write to Critter.Light.

    ignore_unused(prop);

    if (auto hex_cr = entity.dyn_cast<CritterHexView>()) {
        auto map = hex_cr->GetMap();
        map->UpdateCritterLightSource(hex_cr);
    }
}

void ClientEngine::OnSetItemPicMap(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto item = entity.dyn_cast<ItemHexView>()) {
        item->RefreshAnim();
    }
}

void ClientEngine::OnSetItemOffsetCoords(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto item = entity.dyn_cast<ItemHexView>()) {
        item->RefreshOffs();
        auto map = item->GetMap();
        map->MeasureMapBorders(item);
    }
}

void ClientEngine::OnSetItemHideSprite(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto item = entity.dyn_cast<ItemHexView>()) {
        item->SetSpriteVisiblity(!item->GetHideSprite());
    }
}

void ClientEngine::OnSetItemElevation(ptr<Entity> entity, ptr<const Property> prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    if (auto item = entity.dyn_cast<ItemHexView>()) {
        item->RefreshSprite();
        auto map = item->GetMap();
        map->MeasureMapBorders(item);
    }
}

void ClientEngine::ChangeLanguage(string_view lang_name)
{
    FO_STACK_TRACE_ENTRY();

    auto lang_pack = TextPack {&Hashes};
    lang_pack.LoadFromResources(Resources, lang_name);

    _curLang = std::move(lang_pack);
    Settings->Language = lang_name;
}

auto ClientEngine::GetLangPack(string_view lang_name) -> const TextPack&
{
    FO_STACK_TRACE_ENTRY();

    if (lang_name.empty() || lang_name == Settings->Language) {
        return _curLang;
    }

    for (auto&& [cached_lang_name, cached_pack] : _langPackCache) {
        if (cached_lang_name == lang_name) {
            return cached_pack;
        }
    }

    TextPack lang_pack {&Hashes};
    lang_pack.LoadFromResources(Resources, lang_name);
    auto& [cached_lang_name, cached_pack] = _langPackCache.emplace_back(string {lang_name}, std::move(lang_pack));
    return cached_pack;
}

void ClientEngine::UnloadMap()
{
    FO_STACK_TRACE_ENTRY();

    OnMapUnload.Fire();

    Settings->ScrollMouseRight = false;
    Settings->ScrollMouseLeft = false;
    Settings->ScrollMouseDown = false;
    Settings->ScrollMouseUp = false;

    if (_curMap) {
        auto map = GetCurMap();
        FO_VERIFY_AND_THROW(map, "Map is null");
        map->DestroySelf();
        _curMap.reset();

        CleanupSpriteCache();
    }

    if (_curLocation) {
        auto location = GetCurLocation();
        FO_VERIFY_AND_THROW(location, "Location is null");
        location->DestroySelf();
        _curLocation.reset();
    }

    for (size_t i = 0; i < _globalMapCritters.size(); i++) {
        _globalMapCritters[i]->DestroySelf();
    }

    _globalMapCritters.clear();

    SndMngr.StopSounds();

    _mapLoaded = false;
}

void ClientEngine::LmapPrepareMap()
{
    FO_STACK_TRACE_ENTRY();

    _lmapPrepPix.clear();

    if (!_curMap) {
        BreakIntoDebugger();
        return;
    }

    auto map = GetCurMap();
    FO_VERIFY_AND_THROW(map, "Map is null");
    auto chosen = GetMapChosen();

    if (!chosen) {
        return;
    }

    const auto hex = chosen->GetHex();
    const auto maxpixx = _lmapWMap.width / 2 / _lmapZoom;
    const auto maxpixy = _lmapWMap.height / 2 / _lmapZoom;
    const auto bx = hex.x - maxpixx;
    const auto by = hex.y - maxpixy;
    const auto ex = hex.x + maxpixx;
    const auto ey = hex.y + maxpixy;

    const auto vis = chosen->GetLookDistance();
    auto pix_x = _lmapWMap.width;
    auto pix_y = 0;

    for (auto i1 = bx; i1 < ex; i1++) {
        for (auto i2 = by; i2 < ey; i2++) {
            pix_y += _lmapZoom;

            if (!map->GetSize().is_valid_pos(i1, i2)) {
                continue;
            }

            const auto hex2 = map->GetSize().from_raw_pos(i1, i2);

            bool is_far = false;
            const auto dist = GeometryHelper::GetDistance(hex.x, hex.y, i1, i2);

            if (dist > vis) {
                is_far = true;
            }

            const auto& field = map->GetField(hex2);
            ucolor cur_color;

            if (auto cr = map->GetNonDeadCritter(hex2)) {
                cur_color = cr == chosen ? ucolor {0, 0, 255} : ucolor {255, 0, 0};
                _lmapPrepPix.emplace_back(PrimitivePoint {.PointPos = {_lmapWMap.x + pix_x + (_lmapZoom - 1), _lmapWMap.y + pix_y}, .PointColor = cur_color});
                _lmapPrepPix.emplace_back(PrimitivePoint {.PointPos = {_lmapWMap.x + pix_x, _lmapWMap.y + pix_y + (_lmapZoom - 1) / 2}, .PointColor = cur_color});
            }
            else if (field.HasWall || field.HasScenery) {
                if (!_lmapSwitchHi && !field.HasWall) {
                    continue;
                }

                cur_color = ucolor {(field.HasWall ? ucolor {0, 255, 0, 255} : ucolor {0, 255, 0, 127})};
            }
            else {
                continue;
            }

            if (is_far) {
                cur_color.comp.a = 0x22;
            }

            _lmapPrepPix.emplace_back(PrimitivePoint {.PointPos = {_lmapWMap.x + pix_x, _lmapWMap.y + pix_y}, .PointColor = cur_color});
            _lmapPrepPix.emplace_back(PrimitivePoint {.PointPos = {_lmapWMap.x + pix_x + (_lmapZoom - 1), _lmapWMap.y + pix_y + (_lmapZoom - 1) / 2}, .PointColor = cur_color});
        }

        pix_x -= _lmapZoom;
        pix_y = 0;
    }

    _lmapPrepareNextTime = GameTime.GetFrameTime() + std::chrono::milliseconds {1000};
}

void ClientEngine::CleanupSpriteCache()
{
    FO_STACK_TRACE_ENTRY();

    ResMngr.CleanupCritterFrames();
    SprMngr.CleanupSpriteCache();
}

void ClientEngine::DestroyInnerEntities()
{
    FO_STACK_TRACE_ENTRY();

    if (HasInnerEntities()) {
        auto inner_entities = GetInnerEntities();
        FO_VERIFY_AND_THROW(inner_entities, "Inner entities collection is null");

        for (auto& entities : *inner_entities | std::views::values) {
            for (auto& entity : entities) {
                auto custom_entity = require_refcount_ptr(entity.dyn_cast<CustomEntityView>());
                custom_entity->DestroySelf();
            }
        }

        ClearInnerEntities();
    }
}

void ClientEngine::DrawMiniMap(int32_t zoom, int32_t x, int32_t y, int32_t w, int32_t h)
{
    FO_STACK_TRACE_ENTRY();

    if (zoom != _lmapZoom || x != _lmapWMap.x || y != _lmapWMap.y || w != _lmapWMap.width || h != _lmapWMap.height) {
        _lmapZoom = zoom;
        _lmapWMap.x = x;
        _lmapWMap.y = y;
        _lmapWMap.width = w;
        _lmapWMap.height = h;
        LmapPrepareMap();
    }
    else if (GameTime.GetFrameTime() >= _lmapPrepareNextTime) {
        LmapPrepareMap();
    }

    SprMngr.DrawPoints(_lmapPrepPix, RenderPrimitiveType::LineList);
}

void ClientEngine::Connect()
{
    FO_STACK_TRACE_ENTRY();

    _connectionRequest = true;
}

void ClientEngine::Disconnect()
{
    FO_STACK_TRACE_ENTRY();

    _connectionRequest = false;
    _conn.Disconnect();
}

void ClientEngine::CritterMoveTo(ptr<CritterHexView> cr, variant<tuple<mpos, ipos16, int32_t>, mdir> pos_or_dir, int32_t speed)
{
    FO_STACK_TRACE_ENTRY();

    if (cr->GetIsAttached()) {
        return;
    }

    if (cr->IsMoving()) {
        cr->SynchronizeMoving();
    }

    const auto prev_moving = cr->IsMoving();

    cr->StopMoving();
    cr->NormalizeHexOffset();
    auto map = cr->GetMap();

    bool try_move = false;
    mpos hex;
    ipos16 end_hex_offset;
    vector<mdir> steps;
    vector<uint16_t> control_steps;

    if (speed != 0) {
        if (pos_or_dir.index() == 0) {
            hex = std::get<0>(std::get<0>(pos_or_dir));
            const auto target_hex_offset = std::get<1>(std::get<0>(pos_or_dir));
            const auto cut = std::get<2>(std::get<0>(pos_or_dir));
            const auto find_path = map->FindPath(cr, cr->GetHex(), hex, cut, target_hex_offset);

            if (find_path && !find_path->DirSteps.empty()) {
                steps = find_path->DirSteps;
                control_steps = find_path->ControlSteps;
                end_hex_offset = find_path->EndHexOffset;
                try_move = true;
            }
        }
        else if (pos_or_dir.index() == 1) {
            const auto dir = std::get<1>(pos_or_dir);

            hex = cr->GetHex();
            end_hex_offset = cr->GetHexOffset();
            vector<mdir> raw_steps;

            if (map->TraceMoveWay(hex, end_hex_offset, raw_steps, dir, cr->GetMultihex())) {
                steps.insert(steps.end(), raw_steps.begin(), raw_steps.end());
                control_steps.push_back(numeric_cast<uint16_t>(steps.size()));
                try_move = true;
            }
        }
    }

    if (try_move) {
        cr->SetMoving(SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), numeric_cast<uint16_t>(speed), std::move(steps), std::move(control_steps), GameTime.GetFrameTime(), timespan {}, cr->GetHex(), cr->GetHexOffset(), end_hex_offset));
        auto moving = cr->GetMoving();
        FO_VERIFY_AND_THROW(moving, "Missing active movement state");
        moving->ValidateRuntimeState();
        cr->RefreshView();
        Net_SendMove(cr);
    }

    if (prev_moving && !cr->IsMoving()) {
        cr->StopMoving();
        cr->RefreshView();
        Net_SendStopMove(cr);
    }
}

void ClientEngine::CritterLookTo(ptr<CritterHexView> cr, mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    const auto prev_dir = cr->GetDir();

    cr->ChangeLookDir(dir);

    if (cr->GetDir() != prev_dir) {
        Net_SendDir(cr);
    }
}

void ClientEngine::PlayVideo(string_view video_name, bool can_interrupt, bool enqueue)
{
    FO_STACK_TRACE_ENTRY();

    if (_video && enqueue) {
        _videoQueue.emplace_back(string(video_name), can_interrupt);
        return;
    }

    _video.reset();
    _videoQueue.clear();
    _videoCanInterrupt = can_interrupt;

    if (video_name.empty()) {
        return;
    }

    const auto names = strex(video_name).split('|');
    FO_VERIFY_AND_THROW(!names.empty(), "Video playback request produced no resource candidates after splitting the video name", video_name, can_interrupt);
    const auto file = Resources.ReadFile(names.front());

    if (!file) {
        return;
    }

    VideoClip video_clip {file.GetData()};
    auto video_tex = SprMngr.GetRender().CreateTexture(video_clip.GetSize(), true, false);
    _video.emplace(std::move(video_clip), std::move(video_tex));

    if (names.size() > 1) {
        SndMngr.StopMusic();

        if (!names[1].empty()) {
            SndMngr.PlayMusic(names[1], timespan::zero);
        }
    }
}

void ClientEngine::ProcessVideo()
{
    FO_STACK_TRACE_ENTRY();

    if (_video) {
        _video->Tex->UpdateTextureRegion({}, _video->Tex->Size, _video->Clip.RenderFrame());
        SprMngr.DrawTexture(_video->Tex, false);

        if (_video->Clip.IsStopped()) {
            _video.reset();
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

void ClientEngine::SetEffect(EffectType effectType, int64_t effectSubtype, string_view effectPath)
{
    FO_STACK_TRACE_ENTRY();

    const auto reload_effect = [this, effectPath](ptr<RenderEffect> def_effect) -> ptr<RenderEffect> { return EffectMngr.ResolveEffect(def_effect, effectPath); };

    const uint32_t eff_type = static_cast<uint32_t>(effectType);

    if (((eff_type & static_cast<uint32_t>(EffectType::GenericSprite)) != 0) && effectSubtype != 0) {
        auto map = GetCurMap();

        if (map && effectSubtype >= 0 && effectSubtype <= std::numeric_limits<uint32_t>::max()) {
            auto item = map->GetItem(ident_t {numeric_cast<uint32_t>(effectSubtype)});

            if (item) {
                item->SetDrawEffect(reload_effect(EffectMngr.Effects.Generic));
            }
        }
    }
    if (((eff_type & static_cast<uint32_t>(EffectType::CritterSprite)) != 0) && effectSubtype != 0) {
        auto map = GetCurMap();

        if (map && effectSubtype >= 0 && effectSubtype <= std::numeric_limits<uint32_t>::max()) {
            auto cr = map->GetCritter(ident_t {numeric_cast<uint32_t>(effectSubtype)});

            if (cr) {
                cr->SetDrawEffect(reload_effect(EffectMngr.Effects.Critter));
            }
        }
    }

    if (((eff_type & static_cast<uint32_t>(EffectType::GenericSprite)) != 0) && effectSubtype == 0) {
        EffectMngr.Effects.Generic = EffectMngr.ResolveEffect(EffectMngr.Effects.GenericDefault, effectPath);
    }
    if (((eff_type & static_cast<uint32_t>(EffectType::CritterSprite)) != 0) && effectSubtype == 0) {
        EffectMngr.Effects.Critter = EffectMngr.ResolveEffect(EffectMngr.Effects.CritterDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::TileSprite)) != 0) {
        EffectMngr.Effects.Tile = EffectMngr.ResolveEffect(EffectMngr.Effects.TileDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::RoofSprite)) != 0) {
        EffectMngr.Effects.Roof = EffectMngr.ResolveEffect(EffectMngr.Effects.RoofDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::RainSprite)) != 0) {
        EffectMngr.Effects.Rain = EffectMngr.ResolveEffect(EffectMngr.Effects.RainDefault, effectPath);
    }

#if FO_ENABLE_3D
    if ((eff_type & static_cast<uint32_t>(EffectType::SkinnedMesh)) != 0) {
        EffectMngr.Effects.SkinnedModel = EffectMngr.ResolveEffect(EffectMngr.Effects.SkinnedModelDefault, effectPath);
    }
#endif

    if ((eff_type & static_cast<uint32_t>(EffectType::Interface)) != 0) {
        EffectMngr.Effects.Iface = EffectMngr.ResolveEffect(EffectMngr.Effects.IfaceDefault, effectPath);
    }

    if (((eff_type & static_cast<uint32_t>(EffectType::Font)) != 0) && effectSubtype == -1) {
        EffectMngr.Effects.Font = EffectMngr.ResolveEffect(EffectMngr.Effects.FontDefault, effectPath);
    }
    if (((eff_type & static_cast<uint32_t>(EffectType::Font)) != 0) && effectSubtype >= 0) {
        FontMngr.SetFontEffect(static_cast<FontType>(effectSubtype), reload_effect(EffectMngr.Effects.Font));
    }

    if ((eff_type & static_cast<uint32_t>(EffectType::Primitive)) != 0) {
        EffectMngr.Effects.Primitive = EffectMngr.ResolveEffect(EffectMngr.Effects.PrimitiveDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::Light)) != 0) {
        EffectMngr.Effects.Light = EffectMngr.ResolveEffect(EffectMngr.Effects.LightDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::Fog)) != 0) {
        EffectMngr.Effects.Fog = EffectMngr.ResolveEffect(EffectMngr.Effects.FogDefault, effectPath);
    }

    if ((eff_type & static_cast<uint32_t>(EffectType::FlushRenderTarget)) != 0) {
        EffectMngr.Effects.FlushRenderTarget = EffectMngr.ResolveEffect(EffectMngr.Effects.FlushRenderTargetDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::FlushPrimitive)) != 0) {
        EffectMngr.Effects.FlushPrimitive = EffectMngr.ResolveEffect(EffectMngr.Effects.FlushPrimitiveDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::FlushMap)) != 0) {
        EffectMngr.Effects.FlushMap = EffectMngr.ResolveEffect(EffectMngr.Effects.FlushMapDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::FlushLight)) != 0) {
        EffectMngr.Effects.FlushLight = EffectMngr.ResolveEffect(EffectMngr.Effects.FlushLightDefault, effectPath);
    }
    if ((eff_type & static_cast<uint32_t>(EffectType::FlushFog)) != 0) {
        EffectMngr.Effects.FlushFog = EffectMngr.ResolveEffect(EffectMngr.Effects.FlushFogDefault, effectPath);
    }

    if ((eff_type & static_cast<uint32_t>(EffectType::Offscreen)) != 0) {
        if (effectSubtype < 0) {
            throw ScriptException("Negative effect subtype");
        }

        OffscreenEffects.resize(std::max(OffscreenEffects.size(), numeric_cast<size_t>(effectSubtype) + 1));
        OffscreenEffects[numeric_cast<size_t>(effectSubtype)] = reload_effect(EffectMngr.Effects.GenericDefault);
    }
}

void ClientEngine::SetEffectScriptValue(EffectType effectType, int64_t effectSubtype, int32_t valueIndex, float32_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetEffectScriptValues(effectType, effectSubtype, valueIndex, const_span<float32_t> {&value, 1});
}

void ClientEngine::SetEffectScriptValues(EffectType effectType, int64_t effectSubtype, int32_t valueStartIndex, const_span<float32_t> values, int32_t valuesOffset, int32_t valuesCount)
{
    FO_STACK_TRACE_ENTRY();

    const int32_t values_size = numeric_cast<int32_t>(values.size());

    if (valuesOffset < 0 || valuesOffset > values_size) {
        throw ScriptException("Effect script values offset is out of range", valuesOffset, values_size);
    }

    int32_t actual_values_count = valuesCount;

    if (actual_values_count < 0) {
        actual_values_count = values_size - valuesOffset;
    }
    if (actual_values_count < 0 || actual_values_count > values_size - valuesOffset) {
        throw ScriptException("Effect script values count is out of range", actual_values_count, values_size, valuesOffset);
    }
    if (valueStartIndex < 0 || valueStartIndex > numeric_cast<int32_t>(EFFECT_SCRIPT_VALUES)) {
        throw ScriptException("Effect script value index is out of range", valueStartIndex);
    }
    if (actual_values_count > numeric_cast<int32_t>(EFFECT_SCRIPT_VALUES) - valueStartIndex) {
        throw ScriptException("Effect script value range is out of range", valueStartIndex, actual_values_count);
    }

    auto effect = ResolveRequiredEffectScriptValueTarget(effectType, effectSubtype);

    const_span<float32_t> selected_values;
    if (actual_values_count != 0) {
        selected_values = values.subspan(numeric_cast<size_t>(valuesOffset), numeric_cast<size_t>(actual_values_count));
    }

    EffectMngr.SetEffectScriptValues(effect, valueStartIndex, selected_values);
}

void ClientEngine::ClearEffectScriptValues(EffectType effectType, int64_t effectSubtype)
{
    FO_STACK_TRACE_ENTRY();

    auto effect = ResolveRequiredEffectScriptValueTarget(effectType, effectSubtype);

    EffectMngr.ClearEffectScriptValues(effect);
}

auto ClientEngine::GetOffscreenEffect(int32_t effectSubtype) -> ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    if (effectSubtype < 0 || effectSubtype >= numeric_cast<int32_t>(OffscreenEffects.size()) || !OffscreenEffects[numeric_cast<size_t>(effectSubtype)]) {
        throw ScriptException("Invalid effect subtype");
    }

    return OffscreenEffects[numeric_cast<size_t>(effectSubtype)];
}

auto ClientEngine::ResolveEffectScriptValueTarget(EffectType effectType, int64_t effectSubtype) -> nptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    switch (effectType) {
    case EffectType::GenericSprite:
        if (effectSubtype != 0) {
            if (effectSubtype < 0 || effectSubtype > std::numeric_limits<uint32_t>::max()) {
                throw ScriptException("Invalid generic sprite effect subtype", effectSubtype);
            }

            auto map = GetCurMap();
            if (!map) {
                throw ScriptException("Current map is not available");
            }

            auto item = map->GetItem(ident_t {numeric_cast<uint32_t>(effectSubtype)});
            if (!item) {
                throw ScriptException("Generic sprite effect target item not found", effectSubtype);
            }

            return item->GetDrawEffect();
        }

        return EffectMngr.Effects.Generic;

    case EffectType::CritterSprite:
        if (effectSubtype != 0) {
            if (effectSubtype < 0 || effectSubtype > std::numeric_limits<uint32_t>::max()) {
                throw ScriptException("Invalid critter sprite effect subtype", effectSubtype);
            }

            auto map = GetCurMap();
            if (!map) {
                throw ScriptException("Current map is not available");
            }

            auto cr = map->GetCritter(ident_t {numeric_cast<uint32_t>(effectSubtype)});
            if (!cr) {
                throw ScriptException("Critter sprite effect target critter not found", effectSubtype);
            }

            return cr->GetDrawEffect();
        }

        return EffectMngr.Effects.Critter;

    case EffectType::TileSprite:
        return EffectMngr.Effects.Tile;

    case EffectType::RoofSprite:
        return EffectMngr.Effects.Roof;

    case EffectType::RainSprite:
        return EffectMngr.Effects.Rain;

#if FO_ENABLE_3D
    case EffectType::SkinnedMesh:
        return EffectMngr.Effects.SkinnedModel;
#endif

    case EffectType::Interface:
        return EffectMngr.Effects.Iface;

    case EffectType::Font:
        if (effectSubtype != -1) {
            throw ScriptException("Per-font script values are not supported");
        }

        return EffectMngr.Effects.Font;

    case EffectType::Primitive:
        return EffectMngr.Effects.Primitive;

    case EffectType::Light:
        return EffectMngr.Effects.Light;

    case EffectType::Fog:
        return EffectMngr.Effects.Fog;

    case EffectType::FlushRenderTarget:
        return EffectMngr.Effects.FlushRenderTarget;

    case EffectType::FlushPrimitive:
        return EffectMngr.Effects.FlushPrimitive;

    case EffectType::FlushMap:
        return EffectMngr.Effects.FlushMap;

    case EffectType::FlushLight:
        return EffectMngr.Effects.FlushLight;

    case EffectType::FlushFog:
        return EffectMngr.Effects.FlushFog;

    case EffectType::Offscreen:
        if (effectSubtype < 0 || effectSubtype > std::numeric_limits<int32_t>::max()) {
            throw ScriptException("Invalid offscreen effect subtype", effectSubtype);
        }

        return GetOffscreenEffect(numeric_cast<int32_t>(effectSubtype));

    default:
        throw ScriptException("Unsupported effect script value target", static_cast<uint32_t>(effectType), effectSubtype);
    }
}

auto ClientEngine::ResolveRequiredEffectScriptValueTarget(EffectType effectType, int64_t effectSubtype) -> ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto effect = ResolveEffectScriptValueTarget(effectType, effectSubtype);

    if (!effect) {
        throw ScriptException("Effect script value target is not loaded", static_cast<uint32_t>(effectType), effectSubtype);
    }

    if (!effect->IsNeedScriptValueBuf()) {
        throw ScriptException("Effect does not declare ScriptValueBuf", static_cast<uint32_t>(effectType), effectSubtype);
    }

    return effect;
}

FO_END_NAMESPACE
