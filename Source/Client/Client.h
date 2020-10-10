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
#include "ClientScripting.h"
#include "CritterView.h"
#include "EffectManager.h"
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
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "Timer.h"
#include "TwoBitMask.h"
#include "WinApi-Include.h"

#include "zlib.h"
#ifndef FO_WINDOWS
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

// Fonts
#ifndef FONT_DEFAULT
#define FONT_DEFAULT (0)
#endif

// Screens
#define SCREEN_NONE (0)
// Primary screens
#define SCREEN_LOGIN (1)
#define SCREEN_GAME (2)
#define SCREEN_GLOBAL_MAP (3)
#define SCREEN_WAIT (4)
// Secondary screens
#define SCREEN_DIALOG (6)
#define SCREEN_TOWN_VIEW (9)

// Proxy types
#define PROXY_SOCKS4 (1)
#define PROXY_SOCKS5 (2)
#define PROXY_HTTP (3)

// InitNetReason
#define INIT_NET_REASON_NONE (0)
#define INIT_NET_REASON_LOGIN (1)
#define INIT_NET_REASON_REG (2)
#define INIT_NET_REASON_LOAD (3)
#define INIT_NET_REASON_CUSTOM (4)

class FOClient final // Todo: rename FOClient to just Client (after reworking server Client to ClientConnection)
{
public:
    FOClient() = delete;
    explicit FOClient(GlobalSettings& settings);
    FOClient(const FOClient&) = delete;
    FOClient(FOClient&&) noexcept = delete;
    auto operator=(const FOClient&) = delete;
    auto operator=(FOClient&&) noexcept = delete;
    ~FOClient();

    void ProcessAutoLogin();
    void TryExit();
    auto IsCurInWindow() const -> bool;
    void FlashGameWindow();
    void MainLoop();
    void NetDisconnect();
    void DrawIface();

    int InitCalls {};
    ClientSettings& Settings;
    GeometryHelper GeomHelper;
    FileManager FileMngr;
    ClientScriptSystem ScriptSys;
    GameTimer GameTime;
    ProtoManager ProtoMngr;
    EffectManager EffectMngr;
    SpriteManager SprMngr;
    ResourceManager ResMngr;
    HexManager HexMngr;
    SoundManager SndMngr;
    Keyboard Keyb;
    CacheStorage Cache;
    GlobalVars* Globals {};
    hash CurMapPid {};
    hash CurMapLocPid {};
    uint CurMapIndexInLoc {};
    StrVec Preload3dFiles {};
    int WindowResolutionDiffX {};
    int WindowResolutionDiffY {};
    string LoginName {};
    string LoginPassword {};
    bool CanDrawInScripts {};
    bool IsAutoLogin {};
    MapView* CurMap {};
    LocationView* CurLocation {};
    uint FpsTick {};
    uint FpsCounter {};

    // Offscreen drawing
    vector<RenderEffect*> OffscreenEffects;
    vector<RenderTarget*> OffscreenSurfaces;
    vector<RenderTarget*> ActiveOffscreenSurfaces;
    vector<RenderTarget*> PreDirtyOffscreenSurfaces;
    vector<RenderTarget*> DirtyOffscreenSurfaces;

    // Screen
    int ScreenModeMain {};
    void ShowMainScreen(int new_screen, StrIntMap params);
    auto GetMainScreen() const -> int { return ScreenModeMain; }
    auto IsMainScreen(int check_screen) const -> bool { return check_screen == ScreenModeMain; }
    void ShowScreen(int screen, StrIntMap params);
    void HideScreen(int screen);
    auto GetActiveScreen(IntVec* screens) -> int;
    auto IsScreenPresent(int screen) -> bool;
    void RunScreenScript(bool show, int screen, StrIntMap params);

    // Input
    void ProcessInputEvents();
    void ProcessInputEvent(const InputEvent& event);

    // Update files
    struct UpdateFile
    {
        uint Index {};
        string Name;
        uint Size {};
        uint RemaningSize {};
        uint Hash {};
    };
    typedef vector<UpdateFile> UpdateFileVec;

    bool UpdateFilesInProgress {};
    bool UpdateFilesClientOutdated {};
    bool UpdateFilesCacheChanged {};
    bool UpdateFilesFilesChanged {};
    bool UpdateFilesConnection {};
    uint UpdateFilesConnectTimeout {};
    uint UpdateFilesTick {};
    bool UpdateFilesAborted {};
    bool UpdateFilesFontLoaded {};
    string UpdateFilesText;
    string UpdateFilesProgress;
    UpdateFileVec* UpdateFilesList {};
    uint UpdateFilesWholeSize {};
    bool UpdateFileDownloading {};
    unique_ptr<OutputFile> UpdateFileTemp;

    void UpdateFilesStart();
    void UpdateFilesLoop();
    void UpdateFilesAddText(uint num_str, const string& num_str_str);
    void UpdateFilesAbort(uint num_str, const string& num_str_str);

    // Network
    UCharVec ComBuf;
    NetBuffer Bin;
    NetBuffer Bout;
    z_stream ZStream {};
    bool ZStreamOk {};
    uint BytesReceive {}, BytesRealReceive {}, BytesSend {};
    sockaddr_in SockAddr {}, ProxyAddr {};
    SOCKET Sock {};
    fd_set SockSet {};
    ItemView* SomeItem {};
    bool IsConnecting {};
    bool IsConnected {};
    bool InitNetBegin {};
    int InitNetReason;
    bool InitialItemsSend {};
    UCharVecVec GlovalVarsPropertiesData;
    UCharVecVec TempPropertiesData;
    UCharVecVec TempPropertiesDataExt;
    UCharVec TempPropertyData;

    auto CheckSocketStatus(bool for_write) -> bool;
    auto NetConnect(const string& host, ushort port) -> bool;
    auto FillSockAddr(sockaddr_in& saddr, const string& host, ushort port) -> bool;
    void ParseSocket();
    auto NetInput(bool unpack) -> int;
    auto NetOutput() -> bool;
    void NetProcess();

    void Net_SendUpdate();
    void Net_SendLogIn();
    void Net_SendCreatePlayer();
    void Net_SendProperty(NetProperty::Type type, Property* prop, Entity* entity);
    void Net_SendTalk(uchar is_npc, uint id_to_talk, uchar answer);
    void Net_SendGetGameInfo();
    void Net_SendGiveMap(bool automap, hash map_pid, uint loc_id, hash tiles_hash, hash scen_hash);
    void Net_SendLoadMapOk();
    void Net_SendText(const char* send_str, uchar how_say);
    void Net_SendDir();
    void Net_SendMove(UCharVec steps);
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

    void OnText(const string& str, uint crid, int how_say);
    void OnMapText(const string& str, ushort hx, ushort hy, uint color);
    void CrittersProcess();
    void WaitPing();

    uint PingTick {}, PingCallTick {};

    // MSG File
    LanguagePack CurLang;

    auto FmtGameText(uint str_num, ...) -> string;

    // Properties callbacks
    void OnSendGlobalValue(Entity* entity, Property* prop);
    void OnSendCritterValue(Entity* entity, Property* prop);
    static void OnSetCritterModelName(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSendItemValue(Entity* entity, Property* prop);
    void OnSetItemFlags(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemSomeLight(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemPicMap(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemOffsetXY(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSendMapValue(Entity* entity, Property* prop);
    void OnSendLocationValue(Entity* entity, Property* prop);

    /************************************************************************/
    /* Animation                                                            */
    /************************************************************************/
    struct IfaceAnim
    {
        AnyFrames* Frames {};
        AtlasType ResType {};
        uint LastTick {};
        ushort Flags {};
        uint CurSpr {};
    };

    vector<IfaceAnim*> Animations;

    auto AnimLoad(uint name_hash, AtlasType res_type) -> uint;
    auto AnimLoad(const char* fname, AtlasType res_type) -> uint;
    auto AnimGetCurSpr(uint anim_id) -> uint;
    auto AnimGetCurSprCnt(uint anim_id) -> uint;
    auto AnimGetSprCount(uint anim_id) -> uint;
    auto AnimGetFrames(uint anim_id) -> AnyFrames*;
    void AnimRun(uint anim_id, uint flags);
    void AnimProcess();

    /************************************************************************/
    /* Screen effects                                                       */
    /************************************************************************/
    struct ScreenEffect
    {
        uint BeginTick;
        uint Time;
        uint StartColor;
        uint EndColor;
        ScreenEffect(uint begin_tick, uint time, uint col, uint end_col) : BeginTick(begin_tick), Time(time), StartColor(col), EndColor(end_col) { }
    };

    using ScreenEffectVec = vector<ScreenEffect>;

    // Fading
    ScreenEffectVec ScreenEffects;

    // Quake
    int ScreenOffsX {}, ScreenOffsY {};
    float ScreenOffsXf {}, ScreenOffsYf {}, ScreenOffsStep {};
    uint ScreenOffsNextTick {};

    void ScreenFadeIn() { ScreenFade(1000, COLOR_RGBA(0, 0, 0, 0), COLOR_RGBA(255, 0, 0, 0), false); }
    void ScreenFadeOut() { ScreenFade(1000, COLOR_RGBA(255, 0, 0, 0), COLOR_RGBA(0, 0, 0, 0), false); }
    void ScreenFade(uint time, uint from_color, uint to_color, bool push_back);
    void ScreenQuake(int noise, uint time);
    void ProcessScreenEffectFading();
    void ProcessScreenEffectQuake();

    void OnItemInvChanged(ItemView* old_item, ItemView* new_item);

    /************************************************************************/
    /* Game                                                                 */
    /************************************************************************/
    struct MapText
    {
        ushort HexX {}, HexY {};
        uint StartTick {}, Tick {};
        string Text;
        uint Color {};
        bool Fade {};
        Rect Pos;
        Rect EndPos;
        auto operator==(const MapText& r) const -> bool { return HexX == r.HexX && HexY == r.HexY; }
    };

    using MapTextVec = vector<MapText>;

    MapTextVec GameMapTexts;
    uint GameMouseStay {};

    void GameDraw();

    /************************************************************************/
    /* Dialog                                                               */
    /************************************************************************/
    uchar DlgIsNpc {};
    uint DlgNpcId {};

    void FormatTags(string& text, CritterView* player, CritterView* npc, const string& lexems);

    /************************************************************************/
    /* Mini-map                                                             */
    /************************************************************************/
#define MINIMAP_PREPARE_TICK (1000)

    PointVec LmapPrepPix;
    Rect LmapWMap;
    int LmapZoom;
    bool LmapSwitchHi {};
    uint LmapPrepareNextTick {};

    void LmapPrepareMap();

    /************************************************************************/
    /* Global map                                                           */
    /************************************************************************/
    // Mask
    TwoBitMask GmapFog;
    PointVec GmapFogPix;

    // Locations
    struct GmapLocation
    {
        uint LocId;
        hash LocPid;
        ushort LocWx;
        ushort LocWy;
        ushort Radius;
        uint Color;
        uchar Entrances;
        auto operator==(const uint& other) const -> bool { return this->LocId == other; }
    };

    using GmapLocationVec = vector<GmapLocation>;
    GmapLocationVec GmapLoc;
    GmapLocation GmapTownLoc {};

    void GmapNullParams();

    /************************************************************************/
    /* PipBoy                                                               */
    /************************************************************************/
    // HoloInfo
    uint HoloInfo[MAX_HOLO_INFO] {};

    // Automaps
    struct Automap
    {
        uint LocId {};
        hash LocPid {};
        string LocName {};
        HashVec MapPids {};
        StrVec MapNames {};
        uint CurMap {};

        auto operator==(const uint id) const -> bool { return LocId == id; }
    };

    using AutomapVec = vector<Automap>;

    AutomapVec Automaps;

    /************************************************************************/
    /* PickUp                                                               */
    /************************************************************************/
    uchar PupTransferType {};
    uint PupContId {};
    hash PupContPid {};

    /************************************************************************/
    /* Wait                                                                 */
    /************************************************************************/
    AnyFrames* WaitPic;

    void WaitDraw();

    /************************************************************************/
    /* Generic                                                              */
    /************************************************************************/
    uint DaySumRGB {};

    void SetDayTime(bool refresh);
    void SetGameColor(uint color);

    CritterView* Chosen {};

    void AddCritter(CritterView* cr);
    auto GetCritter(uint crid) -> CritterView* { return HexMngr.GetCritter(crid); }
    auto GetItem(uint item_id) -> ItemHexView* { return HexMngr.GetItemById(item_id); }
    void DeleteCritters();
    void DeleteCritter(uint remid);

    bool NoLogOut {};
    bool RebuildLookBorders {};
    bool DrawLookBorders, DrawShootBorders {};
    PointVec LookBorders, ShootBorders;

    void LookBordersPrepare();

    /************************************************************************/
    /* MessBox                                                              */
    /************************************************************************/
#define FOMB_GAME (0)
#define FOMB_TALK (1)
    void AddMess(int mess_type, const string& msg);
    void AddMess(int mess_type, const string& msg, bool script_call);
};
