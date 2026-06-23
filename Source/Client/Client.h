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

#include "3dStuff.h"
#include "Application.h"
#include "CacheStorage.h"
#include "ClientConnection.h"
#include "CritterHexView.h"
#include "CritterView.h"
#include "EffectManager.h"
#include "EngineBase.h"
#include "Entity.h"
#include "FontManager.h"
#include "Geometry.h"
#include "ImGuiStuff.h"
#include "ItemHexView.h"
#include "ItemView.h"
#include "LocationView.h"
#include "MapView.h"
#include "NetBuffer.h"
#include "PlayerView.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "ScriptSystem.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "TextPack.h"
#include "VideoClip.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ResourcesOutdatedException);

struct VideoPlaybackResources
{
    VideoPlaybackResources(VideoClip&& clip, unique_ptr<RenderTexture> tex) noexcept :
        Clip {std::move(clip)},
        Tex {std::move(tex)}
    {
    }

    VideoClip Clip;
    unique_ptr<RenderTexture> Tex;
};

///@ ExportRefType Client RefCounted Export = Stopped
class VideoPlayback : public RefCounted<VideoPlayback>
{
public:
    optional<VideoPlaybackResources> PlaybackResources {};
    bool Stopped {};
};

auto GetClientResources(GlobalSettings& settings) -> FileSystem;

class ClientEngine : public BaseEngine, public AnimationResolver
{
    friend class ClientScriptSystem;

public:
    explicit ClientEngine(ptr<GlobalSettings> settings, FileSystem&& resources, ptr<IAppWindow> window); // For client
    explicit ClientEngine(ptr<GlobalSettings> settings, FileSystem&& resources, ptr<IAppWindow> window, const MeatdataRegistrator& mapper_registrator); // For mapper
    ClientEngine(const ClientEngine&) = delete;
    ClientEngine(ClientEngine&&) noexcept = delete;
    auto operator=(const ClientEngine&) = delete;
    auto operator=(ClientEngine&&) noexcept = delete;
    ~ClientEngine() override;

    [[nodiscard]] auto GetEngine() -> ptr<ClientEngine> { return this; }

    [[nodiscard]] auto ResolveCritterAnimationFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32_t& pass, uint32_t& flags, int32_t& ox, int32_t& oy, string& anim_name) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32_t& f_state_anim, int32_t& f_action_anim, int32_t& f_state_anim_ex, int32_t& f_action_anim_ex, uint32_t& flags) -> bool override;

    [[nodiscard]] auto IsConnecting() const noexcept -> bool;
    [[nodiscard]] auto IsConnected() const noexcept -> bool;
    [[nodiscard]] auto GetConnection() noexcept -> ptr<ClientConnection> { return &_conn; }
    [[nodiscard]] auto GetChosen() noexcept -> nptr<CritterView>;
    [[nodiscard]] auto GetMapChosen() noexcept -> nptr<CritterHexView>;
    [[nodiscard]] auto GetGlobalMapCritter(ident_t cr_id) -> nptr<CritterView>;
    [[nodiscard]] auto GetGlobalMapCritters() const noexcept -> const_span<refcount_ptr<CritterView>> { return _globalMapCritters; }
    [[nodiscard]] auto GetGlobalMapCritters() noexcept -> span<refcount_ptr<CritterView>> { return _globalMapCritters; }
    [[nodiscard]] auto GetCurLang() const noexcept -> const TextPack& { return _curLang; }
    [[nodiscard]] auto GetLangPack(string_view lang_name) -> const TextPack&;
    [[nodiscard]] auto IsVideoPlaying() const noexcept -> bool { return !!_video || !_videoQueue.empty(); }
    [[nodiscard]] auto GetCurPlayer() noexcept -> nptr<PlayerView> { return _curPlayer.as_nptr(); }
    [[nodiscard]] auto GetCurPlayerPtr() noexcept -> ptr<PlayerView>
    {
        FO_STRONG_ASSERT(_curPlayer, "No current player");

        return _curPlayer.as_ptr();
    }
    [[nodiscard]] auto GetCurLocation() noexcept -> nptr<LocationView> { return _curLocation.as_nptr(); }
    [[nodiscard]] auto GetCurLocationPtr() noexcept -> ptr<LocationView>
    {
        FO_STRONG_ASSERT(_curLocation, "No current location");

        return _curLocation.as_ptr();
    }
    [[nodiscard]] auto GetCurMap() noexcept -> nptr<MapView> { return _curMap.as_nptr(); }
    [[nodiscard]] auto GetCurMap() const noexcept -> nptr<const MapView> { return _curMap.as_nptr(); }

    [[nodiscard]] auto GetCurMapPtr() noexcept -> ptr<MapView>
    {
        FO_STRONG_ASSERT(_curMap, "No current map");

        return _curMap.as_ptr();
    }
    [[nodiscard]] auto GetCurMapPtr() const noexcept -> ptr<const MapView>
    {
        FO_STRONG_ASSERT(_curMap, "No current map");

        return _curMap.as_ptr();
    }
    void Shutdown() override;

    void ScheduleDelayedCallback(timespan delay, function<void()> body) override;
    void ProcessScheduledCallbacks();

    void MainLoop();
    void ChangeLanguage(string_view lang_name);
    void ProcessInputEvent(const InputEvent& ev);
    void SetEffect(EffectType effectType, int64_t effectSubtype, string_view effectPath);
    void SetEffectScriptValue(EffectType effectType, int64_t effectSubtype, int32_t valueIndex, float32_t value);
    void SetEffectScriptValues(EffectType effectType, int64_t effectSubtype, int32_t valueStartIndex, const_span<float32_t> values, int32_t valuesOffset = 0, int32_t valuesCount = -1);
    void ClearEffectScriptValues(EffectType effectType, int64_t effectSubtype);
    auto GetOffscreenEffect(int32_t effectSubtype) -> ptr<RenderEffect>;

    auto AnimLoad(hstring name, AtlasType atlas_type) -> uint32_t;
    void AnimFree(uint32_t anim_id);
    auto AnimGetSpr(uint32_t anim_id) -> nptr<Sprite>;

    void Connect();
    void Disconnect();

    void CritterMoveTo(ptr<CritterHexView> cr, variant<tuple<mpos, ipos16, int32_t>, mdir> pos_or_dir, int32_t speed);
    void CritterLookTo(ptr<CritterHexView> cr, mdir dir);
    void PlayVideo(string_view video_name, bool can_interrupt, bool enqueue);

    auto GetEntity(ident_t id) -> nptr<ClientEntity>;
    void RegisterEntity(ptr<ClientEntity> entity);
    void UnregisterEntity(ptr<ClientEntity> entity);

    void DrawMiniMap(int32_t zoom, int32_t x, int32_t y, int32_t w, int32_t h);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnStart);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnConnecting);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnConnectingFailed);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnConnected);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnDisconnected);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLoginSuccess);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnInfoMessage, EngineInfoMessage /*infoMessage*/, string /*extraText*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLoop);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnScreenScroll);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderIface);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_Rebuild, MapView* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_BeforeTiles, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_AfterTiles, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_BeforeFlatSprites, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_AfterFlatSprites, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_BeforeLighting, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_AfterLighting, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_BeforeSprites, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_AfterSprites, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_BeforeFog, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_AfterFog, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_AfterSpritesAndFog, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_BeforeFlushMap, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnRenderMap_AfterFlushMap, MapView* /*map*/, irect32 /*drawArea*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMouseDown, MouseButton /*button*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMouseUp, MouseButton /*button*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMouseMove, ipos32 /*offsetPos*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnTouchTap, ipos32 /*screenPos*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnTouchDoubleTap, ipos32 /*screenPos*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnTouchScroll, ipos32 /*screenPos*/, ipos32 /*offsetPos*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnTouchZoom, ipos32 /*screenPos*/, float32_t /*factor*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnKeyDown, KeyCode /*key*/, string /*text*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnKeyUp, KeyCode /*key*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnInputLost);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterIn, CritterView* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterOut, CritterView* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterVisibilityModeChanged, CritterView* /*cr*/, CritterVisibilityMode /*mode*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemMapIn, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemMapOut, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInvIn, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInvOut, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCustomEntityIn, ClientEntity* /*entity*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCustomEntityOut, ClientEntity* /*entity*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPreLoadMap, hstring /*locPid*/, hstring /*mapPid*/, isize32& /*screenSize*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapLoad);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapLoaded);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapUnload);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnReceiveItems, vector<ItemView*> /*items*/, any_t /*contextParam*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAction, bool /*localCall*/, CritterView* /*cr*/, CritterAction /*action*/, int32_t /*actionData*/, AbstractItem* /*contextItem*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationInit, CritterView* /*cr*/, CritterStateAnim& /*stateAnim*/, CritterActionAnim& /*actionAnim*/, AbstractItem* /*contextItem*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationProcess, CritterView* /*cr*/, CritterStateAnim /*stateAnim*/, CritterActionAnim /*actionAnim*/, AbstractItem* /*contextItem*/, bool /*refreshAnim*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationFrames, hstring /*modelName*/, CritterStateAnim /*stateAnim*/, CritterActionAnim /*actionAnim*/, int32_t& /*pass*/, uint32_t& /*flags*/, int32_t& /*ox*/, int32_t& /*oy*/, string& /*animName*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationSubstitute, hstring /*baseModelName*/, CritterStateAnim /*baseStateAnim*/, CritterActionAnim /*baseActionAnim*/, hstring& /*modelName*/, CritterStateAnim& /*stateAnim*/, CritterActionAnim& /*actionAnim*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationFallout, hstring /*modelName*/, CritterStateAnim /*stateAnim*/, CritterActionAnim /*actionAnim*/, int32_t& /*fStateAnim*/, int32_t& /*fActionAnim*/, int32_t& /*fStateAnimEx*/, int32_t& /*fActionAnimEx*/, uint32_t& /*flags*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnScreenSizeChanged);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapView, mpos /*hex*/);

    EffectManager EffectMngr;
    SpriteManager SprMngr;
    FontManager FontMngr;
    ResourceManager ResMngr;
    SoundManager SndMngr;
    CacheStorage Cache;

    ipos32 MousePos {};
    ipos32 ForcedMousePos {};
    bool HasForcedMousePos {};

    bool CanDrawInScripts {};

    vector<nptr<RenderEffect>> OffscreenEffects {};
    vector<ptr<RenderTarget>> OffscreenSurfaces {};
    vector<ptr<RenderTarget>> ActiveOffscreenSurfaces {};
    vector<ptr<RenderTarget>> PreDirtyOffscreenSurfaces {};
    vector<ptr<RenderTarget>> DirtyOffscreenSurfaces {};

#if FO_ENABLE_3D
    vector<shared_ptr<ModelSprite>> DrawCritterModel {};
    vector<hstring> DrawCritterModelCrType {};
    vector<bool> DrawCritterModelFailedToLoad {};
    int32_t DrawCritterModelLayers[MODEL_LAYERS_COUNT] {};
#endif

protected:
    struct IfaceAnim
    {
        hstring Name {};
        shared_ptr<Sprite> Anim {};
    };

    void CleanupSpriteCache();
    void DestroyInnerEntities();

    void ProcessInputEvents();
    void ProcessVideo();

    void UnloadMap();
    void LmapPrepareMap();
    auto ResolveEffectScriptValueTarget(EffectType effectType, int64_t effectSubtype) -> nptr<RenderEffect>;
    auto ResolveRequiredEffectScriptValueTarget(EffectType effectType, int64_t effectSubtype) -> ptr<RenderEffect>;

    void HandleOutboundRemoteCall(hstring name, ptr<Entity> caller, const_span<uint8_t> data) override;
    void HandleUnresolvedHash(hstring::hash_t hash);

    void Net_SendProperty(NetProperty type, ptr<const Property> prop, ptr<const Entity> entity);
    void Net_SendDir(ptr<CritterHexView> cr);
    void Net_SendMove(ptr<CritterHexView> cr);
    void Net_SendStopMove(ptr<CritterHexView> cr);

    void Net_OnConnect(ClientConnection::ConnectResult result);
    void Net_OnDisconnect();
    void Net_OnInitData();
    void Net_OnHashList();
    void Net_OnLoginSuccess();
    void Net_OnAddCritter();
    void Net_OnRemoveCritter();
    void Net_OnCritterVisibilityMode();
    void Net_OnInfoMessage();
    void Net_OnAddItemOnMap();
    void Net_OnRemoveItemFromMap();
    void Net_OnPlaceToGameComplete();
    void Net_OnProperty();
    void Net_OnCritterDir();
    void Net_OnCritterMove();
    void Net_OnCritterMoveSpeed();
    void Net_OnCritterAction();
    void Net_OnCritterMoveItem();
    void Net_OnCritterTeleport();
    void Net_OnCritterPos();
    void Net_OnCritterAttachments();
    void Net_OnChosenAddItem();
    void Net_OnChosenRemoveItem();
    void Net_OnTimeSync();
    void Net_OnLoadMap();
    void Net_OnSomeItems();
    void Net_OnViewMap();
    void Net_OnRemoteCall();
    void Net_OnAddCustomEntity();
    void Net_OnRemoveCustomEntity();

    void ReceiveCustomEntities(nptr<Entity> nullable_holder);
    auto CreateCustomEntityView(ptr<Entity> holder, hstring entry, ident_t id, hstring pid, const vector<vector<uint8_t>>& data) -> ptr<CustomEntityView>;
    void ReceiveCritterMoving(nptr<CritterHexView> nullable_cr);

    void OnSendGlobalValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendPlayerValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendCritterValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendItemValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendMapValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendLocationValue(ptr<Entity> entity, ptr<const Property> prop);

    void OnSetCritterLookDistance(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetCritterModelName(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetCritterHideSprite(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetCritterElevation(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetCritterLight(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemFlags(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemSomeLight(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemPicMap(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemOffsetCoords(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemHideSprite(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemElevation(ptr<Entity> entity, ptr<const Property> prop);

    ClientConnection _conn;
    bool _connectionRequest {};
    EventUnsubscriber _eventUnsubscriber {};
    TextPack _curLang {ptr<HashResolver> {&Hashes}};
    vector<pair<string, TextPack>> _langPackCache {};

    unordered_map<ident_t, ptr<ClientEntity>> _allEntities {};
    vector<refcount_ptr<CritterView>> _globalMapCritters {};
    refcount_nptr<PlayerView> _curPlayer {};
    refcount_nptr<LocationView> _curLocation {};
    refcount_nptr<MapView> _curMap {};
    refcount_nptr<CritterView> _chosen {};

    hstring _curMapLocPid {};
    int32_t _curMapIndexInLoc {};
    bool _mapLoaded {};

    nptr<const Entity> _sendIgnoreEntity {};
    nptr<const Property> _sendIgnoreProperty {};

    vector<vector<uint8_t>> _globalsPropertiesData {};
    vector<vector<uint8_t>> _playerPropertiesData {};
    vector<vector<uint8_t>> _tempPropertiesData {};
    vector<vector<uint8_t>> _tempPropertiesDataExt {};
    vector<vector<uint8_t>> _tempPropertiesDataCustomEntity {};
    vector<uint8_t> _remoteCallData {};

    uint32_t _ifaceAnimCounter {};
    unordered_map<uint32_t, unique_ptr<IfaceAnim>> _ifaceAnimations {};
    unordered_multimap<hstring, unique_ptr<IfaceAnim>> _ifaceAnimationsCache {};

    vector<PrimitivePoint> _lmapPrepPix {};
    irect32 _lmapWMap {};
    int32_t _lmapZoom {2};
    bool _lmapSwitchHi {};
    nanotime _lmapPrepareNextTime {};

    struct ActiveVideoPlayback
    {
        ActiveVideoPlayback(VideoClip&& clip, unique_ptr<RenderTexture> tex) noexcept :
            Clip {std::move(clip)},
            Tex {std::move(tex)}
        {
        }

        VideoClip Clip;
        unique_ptr<RenderTexture> Tex;
    };

    optional<ActiveVideoPlayback> _video {};
    bool _videoCanInterrupt {};
    vector<tuple<string, bool>> _videoQueue {};

    // Sorted ascending by `FireTime`. Per-frame dispatch in `MainLoop` only needs to peek
    // the front and pop entries whose deadline passed; nothing scanned every frame.
    struct ScheduledCallback
    {
        nanotime FireTime {};
        function<void()> Body {};
    };
    vector<ScheduledCallback> _scheduledCallbacks {};
};

FO_END_NAMESPACE
