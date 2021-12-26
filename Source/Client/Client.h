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

// Todo: fix soft scroll if critter teleports

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "CacheStorage.h"
#include "CritterView.h"
#include "EffectManager.h"
#include "EngineBase.h"
#include "Entity.h"
#include "FileSystem.h"
#include "GeometryHelper.h"
#include "HexManager.h"
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
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "StringUtils.h"
#include "Timer.h"
#include "TwoBitMask.h"
#include "WinApi-Include.h"

#include "zlib.h"
#if !FO_WINDOWS
// Todo: add working in IPv6 networks
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define SD_RECEIVE SHUT_RD
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR
#endif

DECLARE_EXCEPTION(ClientRestartException);

// Screens
constexpr auto SCREEN_NONE = 0;
// Primary screens
constexpr auto SCREEN_LOGIN = 1;
constexpr auto SCREEN_GAME = 2;
constexpr auto SCREEN_GLOBAL_MAP = 3;
constexpr auto SCREEN_WAIT = 4;
// Secondary screens
constexpr auto SCREEN_DIALOG = 6;
constexpr auto SCREEN_TOWN_VIEW = 9;

// Proxy types
constexpr auto PROXY_SOCKS4 = 1;
constexpr auto PROXY_SOCKS5 = 2;
constexpr auto PROXY_HTTP = 3;

// _initNetReason
constexpr auto INIT_NET_REASON_NONE = 0;
constexpr auto INIT_NET_REASON_LOGIN = 1;
constexpr auto INIT_NET_REASON_REG = 2;
constexpr auto INIT_NET_REASON_LOAD = 3;
constexpr auto INIT_NET_REASON_CUSTOM = 4;

class FOClient : public FOEngineBase, public AnimationResolver
{
public:
    struct MapText
    {
        ushort HexX {};
        ushort HexY {};
        uint StartTick {};
        uint Tick {};
        string Text;
        uint Color {};
        bool Fade {};
        IRect Pos {};
        IRect EndPos {};
    };

    FOClient() = delete;
    explicit FOClient(GlobalSettings& settings, ScriptSystem* script_sys = nullptr);
    FOClient(const FOClient&) = delete;
    FOClient(FOClient&&) noexcept = delete;
    auto operator=(const FOClient&) = delete;
    auto operator=(FOClient&&) noexcept = delete;
    ~FOClient() override;

    [[nodiscard]] auto GetEngine() -> FOClient* { return this; }

    [[nodiscard]] auto ResolveCritterAnimation(hstring arg1, uint arg2, uint arg3, uint& arg4, uint& arg5, int& arg6, int& arg7, string& arg8) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationSubstitute(hstring arg1, uint arg2, uint arg3, hstring& arg4, uint& arg5, uint& arg6) -> bool override;
    [[nodiscard]] auto ResolveCritterAnimationFallout(hstring arg1, uint& arg2, uint& arg3, uint& arg4, uint& arg5, uint& arg6) -> bool override;

    [[nodiscard]] auto GetChosen() -> CritterView*;
    [[nodiscard]] auto CustomCall(string_view command, string_view separator) -> string;
    [[nodiscard]] auto GetCritter(uint crid) -> CritterView* { return HexMngr.GetCritter(crid); }
    [[nodiscard]] auto GetItem(uint item_id) -> ItemHexView* { return HexMngr.GetItemById(item_id); }
    [[nodiscard]] auto GetCurLang() const -> const LanguagePack& { return _curLang; }
    [[nodiscard]] auto GetWorldmapFog() const -> const TwoBitMask& { return _worldmapFog; }

    void MainLoop();
    void AddMess(uchar mess_type, string_view msg);
    void AddMess(uchar mess_type, string_view msg, bool script_call);
    void FormatTags(string& text, CritterView* cr, CritterView* npc, string_view lexems);
    void ScreenFadeIn() { ScreenFade(1000, COLOR_RGBA(0, 0, 0, 0), COLOR_RGBA(255, 0, 0, 0), false); }
    void ScreenFadeOut() { ScreenFade(1000, COLOR_RGBA(255, 0, 0, 0), COLOR_RGBA(0, 0, 0, 0), false); }
    void ScreenFade(uint time, uint from_color, uint to_color, bool push_back);
    void ScreenQuake(int noise, uint time);
    void NetDisconnect();
    void WaitPing();
    void ProcessInputEvent(const InputEvent& event);
    void RebuildLookBorders() { _rebuildLookBordersRequest = true; }

    auto AnimLoad(hstring name, AtlasType res_type) -> uint;
    auto AnimGetCurSpr(uint anim_id) -> uint;
    auto AnimGetCurSprCnt(uint anim_id) -> uint;
    auto AnimGetSprCount(uint anim_id) -> uint;
    auto AnimGetFrames(uint anim_id) -> AnyFrames*;
    void AnimRun(uint anim_id, uint flags);

    void ShowMainScreen(int new_screen, map<string, int> params);
    auto GetMainScreen() const -> int { return _screenModeMain; }
    auto IsMainScreen(int check_screen) const -> bool { return check_screen == _screenModeMain; }
    void ShowScreen(int screen, map<string, int> params);
    void HideScreen(int screen);
    auto GetActiveScreen(vector<int>* screens) -> int;
    auto IsScreenPresent(int screen) -> bool;
    void RunScreenScript(bool show, int screen, map<string, int> params);

    ///@ ExportEvent
    ScriptEvent<> StartEvent {};
    ///@ ExportEvent
    ScriptEvent<> FinishEvent {};
    ///@ ExportEvent
    ScriptEvent<string /*login*/, string /*password*/> AutoLoginEvent {};
    ///@ ExportEvent
    ScriptEvent<> LoopEvent {};
    ///@ ExportEvent
    ScriptEvent<vector<int>& /*screens*/> GetActiveScreensEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*show*/, int /*screen*/, map<string, int> /*data*/> ScreenChangeEvent {};
    ///@ ExportEvent
    ScriptEvent<int /*offsetX*/, int /*offsetY*/> ScreenScrollEvent {};
    ///@ ExportEvent
    ScriptEvent<> RenderIfaceEvent {};
    ///@ ExportEvent
    ScriptEvent<> RenderMapEvent {};
    ///@ ExportEvent
    ScriptEvent<MouseButton /*button*/> MouseDownEvent {};
    ///@ ExportEvent
    ScriptEvent<MouseButton /*button*/> MouseUpEvent {};
    ///@ ExportEvent
    ScriptEvent<int /*offsetX*/, int /*offsetY*/> MouseMoveEvent {};
    ///@ ExportEvent
    ScriptEvent<KeyCode /*key*/, string /*text*/> KeyDownEvent {};
    ///@ ExportEvent
    ScriptEvent<KeyCode /*key*/> KeyUpEvent {};
    ///@ ExportEvent
    ScriptEvent<> InputLostEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/> CritterInEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/> CritterOutEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemMapInEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/, ItemView* /*oldItem*/> ItemMapChangedEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemMapOutEvent {};
    ///@ ExportEvent
    ScriptEvent<> ItemInvAllInEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemInvInEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/, ItemView* /*oldItem*/> ItemInvChangedEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemInvOutEvent {};
    ///@ ExportEvent
    ScriptEvent<> MapLoadEvent {};
    ///@ ExportEvent
    ScriptEvent<> MapUnloadEvent {};
    ///@ ExportEvent
    ScriptEvent<vector<ItemView*> /*items*/, int /*param*/> ReceiveItemsEvent {};
    ///@ ExportEvent
    ScriptEvent<string& /*text*/, ushort& /*hexX*/, ushort& /*hexY*/, uint& /*color*/, uint& /*delay*/> MapMessageEvent {};
    ///@ ExportEvent
    ScriptEvent<string /*text*/, int& /*sayType*/, uint& /*critterId*/, uint& /*delay*/> InMessageEvent {};
    ///@ ExportEvent
    ScriptEvent<string& /*text*/, int& /*sayType*/> OutMessageEvent {};
    ///@ ExportEvent
    ScriptEvent<string /*text*/, uchar /*type*/, bool /*scriptCall*/> MessageBoxEvent {};
    ///@ ExportEvent
    ScriptEvent<vector<uint> /*result*/> CombatResultEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/> ItemCheckMoveEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*localCall*/, CritterView* /*critter*/, int /*action*/, int /*actionExt*/, ItemView* /*actionItem*/> CritterActionEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*arg1*/, CritterView* /*arg2*/, uint /*arg3*/, uint /*arg4*/, ItemView* /*arg5*/> Animation2dProcessEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*arg1*/, CritterView* /*arg2*/, uint /*arg3*/, uint /*arg4*/, ItemView* /*arg5*/> Animation3dProcessEvent {};
    ///@ ExportEvent
    ScriptEvent<hstring /*arg1*/, uint /*arg2*/, uint /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, int& /*arg6*/, int& /*arg7*/, string& /*arg8*/> CritterAnimationEvent {};
    ///@ ExportEvent
    ScriptEvent<hstring /*arg1*/, uint /*arg2*/, uint /*arg3*/, hstring& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/> CritterAnimationSubstituteEvent {};
    ///@ ExportEvent
    ScriptEvent<hstring /*arg1*/, uint& /*arg2*/, uint& /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/> CritterAnimationFalloutEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/, ItemView* /*item*/, char /*toSlot*/> CritterCheckMoveItemEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/, ItemView* /*item*/, char /*itemMode*/, uint& /*dist*/> CritterGetAttackDistantionEvent {};

    ClientSettings& Settings;
    GeometryHelper GeomHelper;
    FileManager FileMngr;
    ScriptSystem* ScriptSys;
    GameTimer GameTime;

    EffectManager EffectMngr;
    SpriteManager SprMngr;
    ResourceManager ResMngr;
    HexManager HexMngr;
    SoundManager SndMngr;
    Keyboard Keyb;
    CacheStorage Cache;

    unique_ptr<ProtoManager> ProtoMngr {};
    hstring CurMapPid {};
    vector<MapText> GameMapTexts {};
    bool CanDrawInScripts {};
    vector<string> Preload3dFiles {};

    vector<RenderEffect*> OffscreenEffects {};
    vector<RenderTarget*> OffscreenSurfaces {};
    vector<RenderTarget*> ActiveOffscreenSurfaces {};
    vector<RenderTarget*> PreDirtyOffscreenSurfaces {};
    vector<RenderTarget*> DirtyOffscreenSurfaces {};

    vector<ModelInstance*> DrawCritterModel {};
    vector<hstring> DrawCritterModelCrType {};
    vector<bool> DrawCritterModelFailedToLoad {};
    int DrawCritterModelLayers[LAYERS3D_COUNT] {};

private:
    struct ClientUpdate
    {
        struct UpdateFile
        {
            uint Index {};
            string Name;
            uint Size {};
            uint RemaningSize {};
            uint Hash {};
        };

        bool ClientOutdated {};
        bool CacheChanged {};
        bool FilesChanged {};
        bool Connecting {};
        uint ConnectionTimeout {};
        uint Duration {};
        bool Aborted {};
        bool FontLoaded {};
        string Messages;
        string Progress;
        bool FileListReceived {};
        vector<UpdateFile> FileList {};
        uint FilesWholeSize {};
        bool FileDownloading {};
        unique_ptr<OutputFile> TempFile {};
    };

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

    static constexpr auto FONT_DEFAULT = 5;
    static constexpr auto MINIMAP_PREPARE_TICK = 1000u;

    void RegisterData(const vector<uchar>& restore_info_bin);

    void ProcessAutoLogin();
    void CrittersProcess();
    void ProcessInputEvents();
    void TryExit();
    void FlashGameWindow();
    void DrawIface();
    void GameDraw();
    void WaitDraw();

    void SetDayTime(bool refresh);
    void SetGameColor(uint color);
    void LookBordersPrepare();
    void LmapPrepareMap();
    void GmapNullParams();

    void UpdateFilesLoop();
    void UpdateFilesAddText(uint num_str, string_view num_str_str);
    void UpdateFilesAbort(uint num_str, string_view num_str_str);

    auto CheckSocketStatus(bool for_write) -> bool;
    auto NetConnect(string_view host, ushort port) -> bool;
    auto FillSockAddr(sockaddr_in& saddr, string_view host, ushort port) -> bool;
    void ProcessSocket();
    auto NetInput(bool unpack) -> int;
    auto NetOutput() -> bool;
    void NetProcess();

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
    void Net_SendMove(vector<uchar> steps);
    void Net_SendPing(uchar ping);
    void Net_SendRefereshMe();

    void Net_OnWrongNetProto();
    void Net_OnLoginSuccess();
    void Net_OnAddCritter(bool is_npc);
    void Net_OnRemoveCritter();
    void Net_OnText();
    void Net_OnTextMsg(bool with_lexems);
    void Net_OnMapText();
    void Net_OnMapTextMsg();
    void Net_OnMapTextMsgLex();
    void Net_OnAddItemOnMap();
    void Net_OnEraseItemFromMap();
    void Net_OnAnimateItem();
    void Net_OnCombatResult();
    void Net_OnEffect();
    void Net_OnFlyEffect();
    void Net_OnPlaySound();
    void Net_OnPing();
    void Net_OnEndParseToGame();
    void Net_OnProperty(uint data_size);
    void Net_OnCritterDir();
    void Net_OnCritterMove();
    void Net_OnSomeItem();
    void Net_OnCritterAction();
    void Net_OnCritterMoveItem();
    void Net_OnCritterAnimate();
    void Net_OnCritterSetAnims();
    void Net_OnCustomCommand();
    void Net_OnCritterXY();
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
    void Net_OnUpdateFilesList();
    void Net_OnUpdateFileData();
    void Net_OnAutomapsInfo();
    void Net_OnViewMap();

    void OnText(string_view str, uint crid, int how_say);
    void OnMapText(string_view str, ushort hx, ushort hy, uint color);

    void OnSendGlobalValue(Entity* entity, const Property* prop);
    void OnSendPlayerValue(Entity* entity, const Property* prop);
    void OnSendCritterValue(Entity* entity, const Property* prop);
    void OnSetCritterModelName(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSendItemValue(Entity* entity, const Property* prop);
    void OnSetItemFlags(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemSomeLight(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemPicMap(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemOffsetXY(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemOpened(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSendMapValue(Entity* entity, const Property* prop);
    void OnSendLocationValue(Entity* entity, const Property* prop);

    void AnimProcess();

    void ProcessScreenEffectFading();
    void ProcessScreenEffectQuake();
    void OnItemInvChanged(ItemView* old_item, ItemView* new_item);

    void AddCritter(CritterView* cr);
    void DeleteCritters();
    void DeleteCritter(uint crid);

    int _initCalls {};
    vector<uchar> _restoreInfoBin {};
    hstring _curMapLocPid {};
    uint _curMapIndexInLoc {};
    int _windowResolutionDiffX {};
    int _windowResolutionDiffY {};
    string _loginName {};
    string _loginPassword {};
    bool _isAutoLogin {};
    PlayerView* _curPlayer {};
    MapView* _curMap {};
    LocationView* _curLocation {};
    uint _fpsTick {};
    uint _fpsCounter {};
    optional<ClientUpdate> _updateData {};
    int _screenModeMain {};
    vector<uchar> _incomeBuf {};
    NetInBuffer _netIn {};
    NetOutBuffer _netOut {};
    z_stream* _zStream {};
    uint _bytesReceive {};
    uint _bytesRealReceive {};
    uint _bytesSend {};
    sockaddr_in _sockAddr {};
    sockaddr_in _proxyAddr {};
    SOCKET _netSock {};
    fd_set _netSockSet {};
    ItemView* _someItem {};
    bool _isConnecting {};
    bool _isConnected {};
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
    bool _noLogOut {};
    bool _rebuildLookBordersRequest {};
    bool _drawLookBorders;
    bool _drawShootBorders {};
    PrimitivePoints _lookBorders {};
    PrimitivePoints _shootBorders {};
    AnyFrames* _waitPic {};
    uchar _pupTransferType {};
    uint _pupContId {};
    hstring _pupContPid {};
    uint _holoInfo[MAX_HOLO_INFO] {};
    vector<Automap> _automaps {};
    TwoBitMask _worldmapFog {};
    PrimitivePoints _worldmapFogPix {};
    vector<GmapLocation> _worldmapLoc {};
    GmapLocation _worldmapTownLoc {};
    PrimitivePoints _lmapPrepPix {};
    IRect _lmapWMap {};
    int _lmapZoom {};
    bool _lmapSwitchHi {};
    uint _lmapPrepareNextTick {};
    uchar _dlgIsNpc {};
    uint _dlgNpcId {};
    optional<uint> _prevDayTimeColor {};
};
