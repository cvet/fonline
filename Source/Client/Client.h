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

// Todo: fix soft scroll if critter teleports

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "CacheStorage.h"
#include "ClientConnection.h"
#include "CritterHexView.h"
#include "CritterView.h"
#include "EffectManager.h"
#include "EngineBase.h"
#include "Entity.h"
#include "Geometry.h"
#include "ItemHexView.h"
#include "ItemView.h"
#include "Keyboard.h"
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

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(EngineDataNotFoundException);
FO_DECLARE_EXCEPTION(ResourcesOutdatedException);

///@ ExportRefType Client
struct VideoPlayback
{
    FO_SCRIPTABLE_OBJECT_BEGIN();

    bool Stopped {};

    FO_SCRIPTABLE_OBJECT_END();

    unique_ptr<VideoClip> Clip {};
    unique_ptr<RenderTexture> Tex {};
};
static_assert(std::is_standard_layout_v<VideoPlayback>);

///@ ExportEnum
enum class EffectType : uint32
{
    None = 0,
    GenericSprite = 0x00000001,
    CritterSprite = 0x00000002,
    TileSprite = 0x00000004,
    RoofSprite = 0x00000008,
    RainSprite = 0x00000010,
    SkinnedMesh = 0x00000400,
    Interface = 0x00001000,
    Contour = 0x00002000,
    Font = 0x00010000,
    Primitive = 0x00100000,
    Light = 0x00200000,
    Fog = 0x00400000,
    FlushRenderTarget = 0x01000000,
    FlushPrimitive = 0x04000000,
    FlushMap = 0x08000000,
    FlushLight = 0x10000000,
    FlushFog = 0x20000000,
    Offscreen = 0x40000000,
};

// Connection reason
constexpr int32 INIT_NET_REASON_NONE = 0;
constexpr int32 INIT_NET_REASON_LOGIN = 1;
constexpr int32 INIT_NET_REASON_REG = 2;
constexpr int32 INIT_NET_REASON_LOAD = 3;
constexpr int32 INIT_NET_REASON_CUSTOM = 4;

class FOClient : public BaseEngine, public AnimationResolver
{
    friend class ClientScriptSystem;

public:
    explicit FOClient(GlobalSettings& settings, AppWindow* window); // For client
    explicit FOClient(GlobalSettings& settings, AppWindow* window, FileSystem&& resources, const EngineDataRegistrator& mapper_registrator); // For mapper
    FOClient(const FOClient&) = delete;
    FOClient(FOClient&&) noexcept = delete;
    auto operator=(const FOClient&) = delete;
    auto operator=(FOClient&&) noexcept = delete;
    ~FOClient() override;

    [[nodiscard]] auto GetEngine() -> FOClient* { return this; }

    [[nodiscard]] auto ResolveCritterAnimationFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32& pass, uint32& flags, int32& ox, int32& oy, string& anim_name) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32& f_state_anim, int32& f_action_anim, int32& f_state_anim_ex, int32& f_action_anim_ex, uint32& flags) -> bool override;

    [[nodiscard]] auto IsConnecting() const noexcept -> bool;
    [[nodiscard]] auto IsConnected() const noexcept -> bool;
    [[nodiscard]] auto GetConnection() noexcept -> ClientConnection& { return _conn; }
    [[nodiscard]] auto GetChosen() noexcept -> CritterView*;
    [[nodiscard]] auto GetMapChosen() noexcept -> CritterHexView*;
    [[nodiscard]] auto GetGlobalMapCritter(ident_t cr_id) -> CritterView*;
    [[nodiscard]] auto GetGlobalMapCritters() const noexcept -> const vector<refcount_ptr<CritterView>>& { return _globalMapCritters; }
    [[nodiscard]] auto GetGlobalMapCritters() noexcept -> vector<refcount_ptr<CritterView>>& { return _globalMapCritters; }
    [[nodiscard]] auto GetCurLang() const noexcept -> const LanguagePack& { return _curLang; }
    [[nodiscard]] auto IsVideoPlaying() const noexcept -> bool { return !!_video || !_videoQueue.empty(); }
    [[nodiscard]] auto GetCurPlayer() noexcept -> PlayerView* { return _curPlayer.get(); }
    [[nodiscard]] auto GetCurLocation() noexcept -> LocationView* { return _curLocation.get(); }
    [[nodiscard]] auto GetCurMap() noexcept -> MapView* { return _curMap.get(); }

    void MainLoop();
    void ChangeLanguage(string_view lang_name);
    void ScreenFade(timespan time, ucolor from_color, ucolor to_color, bool push_back);
    void ScreenQuake(int32 noise, timespan time);
    void ProcessInputEvent(const InputEvent& ev);

    auto AnimLoad(hstring name, AtlasType atlas_type) -> uint32;
    void AnimFree(uint32 anim_id);
    auto AnimGetSpr(uint32 anim_id) -> Sprite*;

    void Connect(string_view login, string_view password, int32 reason);
    void Disconnect();
    void CritterMoveTo(CritterHexView* cr, variant<tuple<mpos, ipos16>, int32> pos_or_dir, int32 speed);
    void CritterLookTo(CritterHexView* cr, variant<uint8, int16> dir_or_angle);
    void PlayVideo(string_view video_name, bool can_interrupt, bool enqueue);

    auto GetEntity(ident_t id) -> ClientEntity*;
    void RegisterEntity(ClientEntity* entity);
    void UnregisterEntity(ClientEntity* entity);

    auto CustomCall(string_view command, string_view separator) -> string;

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
    FO_ENTITY_EVENT(OnRegistrationSuccess);
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
    FO_ENTITY_EVENT(OnRenderMap);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMouseDown, MouseButton /*button*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMouseUp, MouseButton /*button*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMouseMove, ipos32 /*offsetPos*/);
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
    FO_ENTITY_EVENT(OnItemMapIn, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemMapChanged, ItemView* /*item*/, ItemView* /*oldItem*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemMapOut, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInvIn, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInvChanged, ItemView* /*item*/, ItemView* /*oldItem*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInvOut, ItemView* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCustomEntityIn, ClientEntity* /*entity*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCustomEntityOut, ClientEntity* /*entity*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapLoad);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapLoaded);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapUnload);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnReceiveItems, vector<ItemView*> /*items*/, any_t /*contextParam*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAction, bool /*localCall*/, CritterView* /*cr*/, CritterAction /*action*/, int32 /*actionData*/, AbstractItem* /*contextItem*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationInit, CritterView* /*cr*/, CritterStateAnim& /*stateAnim*/, CritterActionAnim& /*actionAnim*/, AbstractItem* /*contextItem*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationProcess, CritterView* /*cr*/, CritterStateAnim /*stateAnim*/, CritterActionAnim /*actionAnim*/, AbstractItem* /*contextItem*/, bool /*refreshAnim*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationFrames, hstring /*modelName*/, CritterStateAnim /*stateAnim*/, CritterActionAnim /*actionAnim*/, int32& /*pass*/, uint32& /*flags*/, int32& /*ox*/, int32& /*oy*/, string& /*animName*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationSubstitute, hstring /*baseModelName*/, CritterStateAnim /*baseStateAnim*/, CritterActionAnim /*baseActionAnim*/, hstring& /*modelName*/, CritterStateAnim& /*stateAnim*/, CritterActionAnim& /*actionAnim*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAnimationFallout, hstring /*modelName*/, CritterStateAnim /*stateAnim*/, CritterActionAnim /*actionAnim*/, int32& /*fStateAnim*/, int32& /*fActionAnim*/, int32& /*fStateAnimEx*/, int32& /*fActionAnimEx*/, uint32& /*flags*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnScreenSizeChanged);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapView, mpos /*hex*/);

    EffectManager EffectMngr;
    SpriteManager SprMngr;
    ResourceManager ResMngr;
    SoundManager SndMngr;
    Keyboard Keyb;
    CacheStorage Cache;

    bool CanDrawInScripts {};

    vector<raw_ptr<RenderEffect>> OffscreenEffects {};
    vector<raw_ptr<RenderTarget>> OffscreenSurfaces {};
    vector<raw_ptr<RenderTarget>> ActiveOffscreenSurfaces {};
    vector<raw_ptr<RenderTarget>> PreDirtyOffscreenSurfaces {};
    vector<raw_ptr<RenderTarget>> DirtyOffscreenSurfaces {};

#if FO_ENABLE_3D
    vector<shared_ptr<ModelSprite>> DrawCritterModel {};
    vector<hstring> DrawCritterModelCrType {};
    vector<bool> DrawCritterModelFailedToLoad {};
    int32 DrawCritterModelLayers[MODEL_LAYERS_COUNT] {};
#endif

protected:
    // Todo: make IfaceAnim scriptable object
    struct IfaceAnim
    {
        hstring Name {};
        shared_ptr<Sprite> Anim {};
    };

    struct ScreenFadingData
    {
        nanotime BeginTime {};
        timespan Duration {};
        ucolor StartColor {};
        ucolor EndColor {};
    };

    void CleanupSpriteCache();
    void DestroyInnerEntities();

    void ProcessInputEvents();
    void ProcessScreenEffectFading(); // Todo: move screen fading to scripts
    void ProcessScreenEffectQuake(); // Todo: move screen quake effect to scripts
    void ProcessVideo();

    void UnloadMap();
    void LmapPrepareMap();

    void Net_SendLogIn();
    void Net_SendCreatePlayer();
    void Net_SendProperty(NetProperty type, const Property* prop, const Entity* entity);
    void Net_SendDir(CritterHexView* cr);
    void Net_SendMove(CritterHexView* cr);
    void Net_SendStopMove(CritterHexView* cr);

    void Net_OnConnect(ClientConnection::ConnectResult result);
    void Net_OnDisconnect();
    void Net_OnInitData();
    void Net_OnRegisterSuccess();
    void Net_OnLoginSuccess();
    void Net_OnAddCritter();
    void Net_OnRemoveCritter();
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

    void ReceiveCustomEntities(Entity* holder);
    auto CreateCustomEntityView(Entity* holder, hstring entry, ident_t id, hstring pid, const vector<vector<uint8>>& data) -> CustomEntityView*;
    void ReceiveCritterMoving(CritterHexView* cr);

    void OnSendGlobalValue(Entity* entity, const Property* prop);
    void OnSendPlayerValue(Entity* entity, const Property* prop);
    void OnSendCritterValue(Entity* entity, const Property* prop);
    void OnSendItemValue(Entity* entity, const Property* prop);
    void OnSendMapValue(Entity* entity, const Property* prop);
    void OnSendLocationValue(Entity* entity, const Property* prop);

    void OnSetCritterLookDistance(Entity* entity, const Property* prop);
    void OnSetCritterModelName(Entity* entity, const Property* prop);
    void OnSetCritterContourColor(Entity* entity, const Property* prop);
    void OnSetCritterHideSprite(Entity* entity, const Property* prop);
    void OnSetItemFlags(Entity* entity, const Property* prop);
    void OnSetItemSomeLight(Entity* entity, const Property* prop);
    void OnSetItemPicMap(Entity* entity, const Property* prop);
    void OnSetItemOffsetCoords(Entity* entity, const Property* prop);
    void OnSetItemHideSprite(Entity* entity, const Property* prop);

    ClientConnection _conn;
    EventUnsubscriber _eventUnsubscriber {};
    LanguagePack _curLang {};

    string _loginName {};
    string _loginPassword {};

    unordered_map<ident_t, raw_ptr<ClientEntity>> _allEntities {};
    vector<refcount_ptr<CritterView>> _globalMapCritters {};
    refcount_ptr<PlayerView> _curPlayer {};
    refcount_ptr<LocationView> _curLocation {};
    refcount_ptr<MapView> _curMap {};
    refcount_ptr<CritterView> _chosen {};

    hstring _curMapLocPid {};
    int32 _curMapIndexInLoc {};
    bool _mapLoaded {};

    int32 _initNetReason {INIT_NET_REASON_NONE};

    nullable_raw_ptr<const Entity> _sendIgnoreEntity {};
    nullable_raw_ptr<const Property> _sendIgnoreProperty {};

    vector<vector<uint8>> _globalsPropertiesData {};
    vector<vector<uint8>> _playerPropertiesData {};
    vector<vector<uint8>> _tempPropertiesData {};
    vector<vector<uint8>> _tempPropertiesDataExt {};
    vector<vector<uint8>> _tempPropertiesDataCustomEntity {};

    uint32 _ifaceAnimCounter {};
    unordered_map<uint32, unique_ptr<IfaceAnim>> _ifaceAnimations {};
    unordered_multimap<hstring, unique_ptr<IfaceAnim>> _ifaceAnimationsCache {};

    vector<ScreenFadingData> _screenFadingEffects {};

    float32 _quakeScreenOffsX {};
    float32 _quakeScreenOffsY {};
    float32 _quakeScreenOffsStep {};
    nanotime _quakeScreenOffsNextTime {};

    vector<PrimitivePoint> _lmapPrepPix {};
    irect32 _lmapWMap {};
    int32 _lmapZoom {2};
    bool _lmapSwitchHi {};
    nanotime _lmapPrepareNextTime {};

    unique_ptr<VideoClip> _video {};
    unique_ptr<RenderTexture> _videoTex {};
    bool _videoCanInterrupt {};
    vector<tuple<string, bool>> _videoQueue {};
};

FO_END_NAMESPACE();
