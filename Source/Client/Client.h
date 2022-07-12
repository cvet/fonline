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

// Todo: fix soft scroll if critter teleports

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "CacheStorage.h"
#include "CritterHexView.h"
#include "CritterView.h"
#include "EffectManager.h"
#include "EngineBase.h"
#include "Entity.h"
#include "ItemHexView.h"
#include "ItemView.h"
#include "Keyboard.h"
#include "LocationView.h"
#include "MapView.h"
#include "MsgFiles.h"
#include "NetBuffer.h"
#include "PlayerView.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "ScriptSystem.h"
#include "ServerConnection.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "StringUtils.h"
#include "Timer.h"
#include "TwoBitMask.h"

DECLARE_EXCEPTION(ResourcesOutdatedException);

// Screens
constexpr auto SCREEN_NONE = 0;
constexpr auto SCREEN_LOGIN = 1; // Primary screens
constexpr auto SCREEN_GAME = 2;
constexpr auto SCREEN_GLOBAL_MAP = 3;
constexpr auto SCREEN_WAIT = 4;
constexpr auto SCREEN_DIALOG = 6; // Secondary screens
constexpr auto SCREEN_TOWN_VIEW = 9;

// Connection reason
constexpr auto INIT_NET_REASON_NONE = 0;
constexpr auto INIT_NET_REASON_LOGIN = 1;
constexpr auto INIT_NET_REASON_REG = 2;
constexpr auto INIT_NET_REASON_LOAD = 3;
constexpr auto INIT_NET_REASON_CUSTOM = 4;

class FOClient : virtual public FOEngineBase, public AnimationResolver
{
    friend class ClientScriptSystem;

public:
#if !FO_SINGLEPLAYER
    FOClient(GlobalSettings& settings, AppWindow* window, const vector<uchar>& restore_info_bin);
#endif

    FOClient(const FOClient&) = delete;
    FOClient(FOClient&&) noexcept = delete;
    auto operator=(const FOClient&) = delete;
    auto operator=(FOClient&&) noexcept = delete;
    ~FOClient() override;

    [[nodiscard]] auto GetEngine() -> FOClient* { return this; }

    [[nodiscard]] auto ResolveCritterAnimation(hstring arg1, uint arg2, uint arg3, uint& arg4, uint& arg5, int& arg6, int& arg7, string& arg8) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationSubstitute(hstring arg1, uint arg2, uint arg3, hstring& arg4, uint& arg5, uint& arg6) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationFallout(hstring arg1, uint& arg2, uint& arg3, uint& arg4, uint& arg5, uint& arg6) -> bool override;

    [[nodiscard]] auto IsConnecting() const -> bool;
    [[nodiscard]] auto IsConnected() const -> bool;
    [[nodiscard]] auto GetChosen() -> CritterView*;
    [[nodiscard]] auto GetMapChosen() -> CritterHexView*;
    [[nodiscard]] auto CustomCall(string_view command, string_view separator) -> string;
    [[nodiscard]] auto GetCurLang() const -> const LanguagePack& { return _curLang; }
    [[nodiscard]] auto GetWorldmapFog() const -> const TwoBitMask& { return _worldmapFog; }
    [[nodiscard]] auto GetLangPack() const -> const LanguagePack& { return _curLang; }

    void MainLoop();
    void AddMess(uchar mess_type, string_view msg);
    void FormatTags(string& text, CritterView* cr, CritterView* npc, string_view lexems);
    void ScreenFadeIn() { ScreenFade(1000, COLOR_RGBA(0, 0, 0, 0), COLOR_RGBA(255, 0, 0, 0), false); }
    void ScreenFadeOut() { ScreenFade(1000, COLOR_RGBA(255, 0, 0, 0), COLOR_RGBA(0, 0, 0, 0), false); }
    void ScreenFade(uint time, uint from_color, uint to_color, bool push_back);
    void ScreenQuake(int noise, uint time);
    void ProcessInputEvent(const InputEvent& event);
    void RebuildLookBorders() { _rebuildLookBordersRequest = true; }

    auto AnimLoad(hstring name, AtlasType res_type) -> uint;
    void AnimFree(uint anim_id);
    auto AnimGetCurSpr(uint anim_id) const -> uint;
    auto AnimGetCurSprCnt(uint anim_id) const -> uint;
    auto AnimGetSprCount(uint anim_id) const -> uint;
    auto AnimGetFrames(uint anim_id) -> AnyFrames*;
    void AnimRun(uint anim_id, uint flags);

    void ShowMainScreen(int new_screen, map<string, string> params);
    auto GetMainScreen() const -> int { return _screenModeMain; }
    auto IsMainScreen(int check_screen) const -> bool { return check_screen == _screenModeMain; }
    void ShowScreen(int screen, map<string, string> params);
    void HideScreen(int screen);
    auto GetActiveScreen(vector<int>* screens) -> int;
    auto IsScreenPresent(int screen) -> bool;
    void RunScreenScript(bool show, int screen, map<string, string> params);

    ///@ ExportEvent
    ENTITY_EVENT(OnStart);
    ///@ ExportEvent
    ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    ENTITY_EVENT(OnAutoLogin, string /*login*/, string /*password*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoop);
    ///@ ExportEvent
    ENTITY_EVENT(OnGetActiveScreens, vector<int>& /*screens*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnScreenChange, bool /*show*/, int /*screen*/, map<string, string> /*data*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnScreenScroll, int /*offsetX*/, int /*offsetY*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnRenderIface);
    ///@ ExportEvent
    ENTITY_EVENT(OnRenderMap);
    ///@ ExportEvent
    ENTITY_EVENT(OnMouseDown, MouseButton /*button*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMouseUp, MouseButton /*button*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMouseMove, int /*offsetX*/, int /*offsetY*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnKeyDown, KeyCode /*key*/, string /*text*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnKeyUp, KeyCode /*key*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnInputLost);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterIn, CritterView* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterOut, CritterView* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemMapIn, ItemView* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemMapChanged, ItemView* /*item*/, ItemView* /*oldItem*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemMapOut, ItemView* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemInvAllIn);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemInvIn, ItemView* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemInvChanged, ItemView* /*item*/, ItemView* /*oldItem*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemInvOut, ItemView* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapLoad);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapUnload);
    ///@ ExportEvent
    ENTITY_EVENT(OnReceiveItems, vector<ItemView*> /*items*/, int /*param*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapMessage, string& /*text*/, ushort& /*hexX*/, ushort& /*hexY*/, uint& /*color*/, uint& /*delay*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnInMessage, string /*text*/, int& /*sayType*/, uint& /*crId*/, uint& /*delay*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnOutMessage, string& /*text*/, int& /*sayType*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMessageBox, int /*type*/, string /*text*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemCheckMove, ItemView* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAction, bool /*localCall*/, CritterView* /*cr*/, int /*action*/, int /*actionExt*/, AbstractItem* /*actionItem*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAnimationProcess, bool /*animateStay*/, CritterView* /*cr*/, uint /*anim1*/, uint /*anim2*/, AbstractItem* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAnimation, hstring /*arg1*/, uint /*arg2*/, uint /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, int& /*arg6*/, int& /*arg7*/, string& /*arg8*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAnimationSubstitute, hstring /*arg1*/, uint /*arg2*/, uint /*arg3*/, hstring& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAnimationFallout, hstring /*arg1*/, uint& /*arg2*/, uint& /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterCheckMoveItem, CritterView* /*cr*/, ItemView* /*item*/, uchar /*toSlot*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterGetAttackDistantion, CritterView* /*cr*/, AbstractItem* /*item*/, uchar /*itemMode*/, uint& /*dist*/);

    GameTimer GameTime;
    ProtoManager ProtoMngr;
    EffectManager EffectMngr;
    SpriteManager SprMngr;
    ResourceManager ResMngr;
    SoundManager SndMngr;
    Keyboard Keyb;
    CacheStorage Cache;

    MapView* CurMap {};
    hstring CurMapPid {};
    bool CanDrawInScripts {};
    vector<string> Preload3dFiles {};

    vector<RenderEffect*> OffscreenEffects {};
    vector<RenderTarget*> OffscreenSurfaces {};
    vector<RenderTarget*> ActiveOffscreenSurfaces {};
    vector<RenderTarget*> PreDirtyOffscreenSurfaces {};
    vector<RenderTarget*> DirtyOffscreenSurfaces {};

#if FO_ENABLE_3D
    vector<ModelInstance*> DrawCritterModel {};
#endif
    vector<hstring> DrawCritterModelCrType {};
    vector<bool> DrawCritterModelFailedToLoad {};
    int DrawCritterModelLayers[LAYERS3D_COUNT] {};

protected:
    struct IfaceAnim
    {
        AnyFrames* Frames {};
        AtlasType ResType {};
        uint LastTick {};
        ushort Flags {};
        uint CurSpr {};
    };

    struct ScreenEffect
    {
        uint BeginTick {};
        uint Time {};
        uint StartColor {};
        uint EndColor {};
    };

    struct GmapLocation
    {
        uint LocId {};
        hstring LocPid {};
        ushort LocWx {};
        ushort LocWy {};
        ushort Radius {};
        uint Color {};
        uchar Entrances {};
    };

    struct Automap
    {
        uint LocId {};
        hstring LocPid {};
        string LocName {};
        vector<hstring> MapPids {};
        vector<string> MapNames {};
        uint CurMap {};
    };

    static constexpr auto MINIMAP_PREPARE_TICK = 1000u;

    FOClient(GlobalSettings& settings, AppWindow* window, PropertiesRelationType props_relation, const RegisterDataCallback& register_data_callback);

    void ProcessAutoLogin();
    void ProcessGlobalMap();
    void ProcessInputEvents();
    void TryExit();
    void FlashGameWindow();
    void DrawIface();
    void GameDraw();
    void WaitDraw();

    void SetDayTime(bool refresh);
    void LookBordersPrepare();
    void LmapPrepareMap();
    void GmapNullParams();

    void Net_SendUpdate();
    void Net_SendLogIn();
    void Net_SendCreatePlayer();
    void Net_SendProperty(NetProperty type, const Property* prop, Entity* entity);
    void Net_SendTalk(uchar is_npc, uint id_to_talk, uchar answer);
    void Net_SendGetGameInfo();
    void Net_SendGiveMap(bool automap, hstring map_pid, uint loc_id, uint tiles_hash, uint scen_hash);
    void Net_SendLoadMapOk();
    void Net_SendText(string_view send_str, uchar how_say);
    void Net_SendDir();
    void Net_SendMove();
    void Net_SendStopMove();
    void Net_SendPing(uchar ping);
    void Net_SendRefereshMe();

    void Net_OnConnect(bool success);
    void Net_OnDisconnect();
    void Net_OnUpdateFilesResponse();
    void Net_OnWrongNetProto();
    void Net_OnLoginSuccess();
    void Net_OnAddCritter();
    void Net_OnRemoveCritter();
    void Net_OnText();
    void Net_OnTextMsg(bool with_lexems);
    void Net_OnMapText();
    void Net_OnMapTextMsg();
    void Net_OnMapTextMsgLex();
    void Net_OnAddItemOnMap();
    void Net_OnEraseItemFromMap();
    void Net_OnAnimateItem();
    void Net_OnEffect();
    void Net_OnFlyEffect();
    void Net_OnPlaySound();
    void Net_OnEndParseToGame();
    void Net_OnProperty(uint data_size);
    void Net_OnCritterDir();
    void Net_OnCritterMove();
    void Net_OnCritterStopMove();
    void Net_OnSomeItem();
    void Net_OnCritterAction();
    void Net_OnCritterMoveItem();
    void Net_OnCritterAnimate();
    void Net_OnCritterSetAnims();
    void Net_OnCritterTeleport();
    void Net_OnCritterPos();
    void Net_OnAllProperties();
    void Net_OnChosenClearItems();
    void Net_OnChosenAddItem();
    void Net_OnChosenEraseItem();
    void Net_OnAllItemsSend();
    void Net_OnChosenTalk();
    void Net_OnGameInfo();
    void Net_OnLoadMap();
    void Net_OnMap();
    void Net_OnGlobalInfo();
    void Net_OnSomeItems();
    void Net_OnAutomapsInfo();
    void Net_OnViewMap();

    void OnText(string_view str, uint crid, int how_say);
    void OnMapText(string_view str, ushort hx, ushort hy, uint color);

    void OnSendGlobalValue(Entity* entity, const Property* prop, const void* new_value);
    void OnSendPlayerValue(Entity* entity, const Property* prop, const void* new_value);
    void OnSendCritterValue(Entity* entity, const Property* prop, const void* new_value);
    void OnSendItemValue(Entity* entity, const Property* prop, const void* new_value);
    void OnSendMapValue(Entity* entity, const Property* prop, const void* new_value);
    void OnSendLocationValue(Entity* entity, const Property* prop, const void* new_value);

    void OnSetCritterModelName(Entity* entity, const Property* prop, const void* new_value);
    void OnSetCritterSpeed(Entity* entity, const Property* prop, const void* new_value);
    void OnSetCritterContourColor(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemFlags(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemSomeLight(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemPicMap(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemOffsetCoords(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemOpened(Entity* entity, const Property* prop, const void* new_value);

    void AnimProcess();

    void ProcessScreenEffectFading();
    void ProcessScreenEffectQuake();

    ServerConnection _conn;
    hstring _curMapLocPid {};
    uint _curMapIndexInLoc {};
    int _windowResolutionDiffX {};
    int _windowResolutionDiffY {};
    string _loginName {};
    string _loginPassword {};
    bool _isAutoLogin {};
    PlayerView* _curPlayer {};
    LocationView* _curLocation {};
    uint _fpsTick {};
    uint _fpsCounter {};
    int _screenModeMain {SCREEN_WAIT};
    ItemView* _someItem {};
    bool _initNetBegin {};
    int _initNetReason {INIT_NET_REASON_NONE};
    bool _initialItemsSend {};
    vector<vector<uchar>> _globalsPropertiesData {};
    vector<vector<uchar>> _playerPropertiesData {};
    vector<vector<uchar>> _tempPropertiesData {};
    vector<vector<uchar>> _tempPropertiesDataExt {};
    vector<uchar> _tempPropertyData {};
    uint _pingTick {};
    uint _pingCallTick {};
    LanguagePack _curLang {};
    vector<IfaceAnim*> _ifaceAnimations {};
    vector<ScreenEffect> _screenEffects {};
    int _screenOffsX {};
    int _screenOffsY {};
    float _screenOffsXf {};
    float _screenOffsYf {};
    float _screenOffsStep {};
    uint _screenOffsNextTick {};
    uint _gameMouseStay {};
    uint _daySumRGB {};
    CritterView* _chosen {};
    bool _rebuildLookBordersRequest {};
    bool _drawLookBorders {true};
    bool _drawShootBorders {};
    vector<PrimitivePoint> _lookBorders {};
    vector<PrimitivePoint> _shootBorders {};
    AnyFrames* _waitPic {};
    uchar _pupTransferType {};
    uint _pupContId {};
    hstring _pupContPid {};
    uint _holoInfo[MAX_HOLO_INFO] {};
    vector<Automap> _automaps {};
    vector<CritterView*> _worldmapCritters {};
    TwoBitMask _worldmapFog {};
    vector<PrimitivePoint> _worldmapFogPix {};
    vector<GmapLocation> _worldmapLoc {};
    GmapLocation _worldmapTownLoc {};
    vector<PrimitivePoint> _lmapPrepPix {};
    IRect _lmapWMap {};
    int _lmapZoom {2};
    bool _lmapSwitchHi {};
    uint _lmapPrepareNextTick {};
    uchar _dlgIsNpc {};
    uint _dlgNpcId {};
    optional<uint> _prevDayTimeColor {};
    const Entity* _sendIgnoreEntity {};
    const Property* _sendIgnoreProperty {};
};
