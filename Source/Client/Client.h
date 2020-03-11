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
#include "Entity.h"
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
#include "ScriptSystem.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "Testing.h"
#include "Timer.h"
#include "WinApi_Include.h"

#include "theora/theoradec.h"
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
#define SCREEN__DIALOG (6)
#define SCREEN__TOWN_VIEW (9)

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

class CScriptDictionary;
class CScriptDict;
class CScriptArray;

class FOClient :
    public NonMovable // Todo: rename FOClient to just Client (after reworking server Client to ClientConnection)
{
public:
    static FOClient* Self; // Todo: remove client Self singleton

    FOClient(ClientSettings& sett);
    ~FOClient();
    void ProcessAutoLogin();
    void TryExit();
    bool IsCurInWindow();
    void FlashGameWindow();
    void MainLoop();
    void NetDisconnect();
    void DrawIface();

    ClientSettings Settings;
    GeometryHelper GeomHelper;
    int InitCalls;
    FileManager FileMngr;
    ScriptSystem ScriptSys;
    Keyboard Keyb;
    ProtoManager ProtoMngr;
    EffectManager EffectMngr;
    SpriteManager SprMngr;
    HexManager HexMngr;
    ResourceManager ResMngr;
    SoundManager SndMngr;
    CacheStorage Cache;
    GameTimer GameTime;

    hash CurMapPid;
    hash CurMapLocPid;
    uint CurMapIndexInLoc;
    StrVec Preload3dFiles;
    int WindowResolutionDiffX;
    int WindowResolutionDiffY;
    string LoginName;
    string LoginPassword;
    bool CanDrawInScripts;
    bool IsAutoLogin;

    // Offscreen drawing
    vector<RenderEffect*> OffscreenEffects;
    vector<RenderTarget*> OffscreenSurfaces;
    vector<RenderTarget*> ActiveOffscreenSurfaces;
    vector<RenderTarget*> PreDirtyOffscreenSurfaces;
    vector<RenderTarget*> DirtyOffscreenSurfaces;

    // Screen
    int ScreenModeMain;
    void ShowMainScreen(int new_screen, CScriptDictionary* params = nullptr);
    int GetMainScreen() { return ScreenModeMain; }
    bool IsMainScreen(int check_screen) { return check_screen == ScreenModeMain; }
    void ShowScreen(int screen, CScriptDictionary* params = nullptr);
    void HideScreen(int screen);
    int GetActiveScreen(IntVec** screens = nullptr);
    bool IsScreenPresent(int screen);
    void RunScreenScript(bool show, int screen, CScriptDictionary* params);

    // Input
    void ProcessInputEvents();
    void ProcessInputEvent(const InputEvent& event);

    // Update files
    struct UpdateFile
    {
        uint Index;
        string Name;
        uint Size;
        uint RemaningSize;
        uint Hash;
    };
    typedef vector<UpdateFile> UpdateFileVec;

    bool UpdateFilesInProgress;
    bool UpdateFilesClientOutdated;
    bool UpdateFilesCacheChanged;
    bool UpdateFilesFilesChanged;
    bool UpdateFilesConnection;
    uint UpdateFilesConnectTimeout;
    uint UpdateFilesTick;
    bool UpdateFilesAborted;
    bool UpdateFilesFontLoaded;
    string UpdateFilesText;
    string UpdateFilesProgress;
    UpdateFileVec* UpdateFilesList;
    uint UpdateFilesWholeSize;
    bool UpdateFileDownloading;
    unique_ptr<OutputFile> UpdateFileTemp;

    void UpdateFilesStart();
    void UpdateFilesLoop();
    void UpdateFilesAddText(uint num_str, const string& num_str_str);
    void UpdateFilesAbort(uint num_str, const string& num_str_str);

    // Network
    UCharVec ComBuf;
    NetBuffer Bin;
    NetBuffer Bout;
    z_stream ZStream;
    bool ZStreamOk;
    uint BytesReceive, BytesRealReceive, BytesSend;
    sockaddr_in SockAddr, ProxyAddr;
    SOCKET Sock;
    fd_set SockSet;
    ItemView* SomeItem;
    bool IsConnecting;
    bool IsConnected;
    bool InitNetBegin;
    int InitNetReason;
    bool InitialItemsSend;
    UCharVecVec GlovalVarsPropertiesData;
    UCharVecVec TempPropertiesData;
    UCharVecVec TempPropertiesDataExt;
    UCharVec TempPropertyData;

    bool CheckSocketStatus(bool for_write);
    bool NetConnect(const string& host, ushort port);
    bool FillSockAddr(sockaddr_in& saddr, const string& host, ushort port);
    void ParseSocket();
    int NetInput(bool unpack);
    bool NetOutput();
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

    uint PingTick, PingCallTick;

    // MSG File
    LanguagePack CurLang;

    string FmtGameText(uint str_num, ...);

    // Properties callbacks
    static void OnSendGlobalValue(Entity* entity, Property* prop);
    static void OnSendCritterValue(Entity* entity, Property* prop);
    static void OnSetCritterModelName(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSendItemValue(Entity* entity, Property* prop);
    static void OnSetItemFlags(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemSomeLight(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemPicMap(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemOffsetXY(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSendMapValue(Entity* entity, Property* prop);
    static void OnSendLocationValue(Entity* entity, Property* prop);

    /************************************************************************/
    /* Video                                                                */
    /************************************************************************/
    struct ShowVideo
    {
        string FileName {};
        string SoundName {};
        bool CanStop {};
    };

    struct VideoContext
    {
        struct StreamStates
        {
            static constexpr uint COUNT = 10;

            ogg_stream_state Streams[COUNT] {};
            bool StreamsState[COUNT] {};
            int MainIndex {};
        };

        th_dec_ctx* Context {};
        th_info VideoInfo {};
        th_comment Comment {};
        th_setup_info* SetupInfo {};
        th_ycbcr_buffer ColorBuffer {};
        ogg_sync_state SyncState {};
        ogg_packet Packet {};
        StreamStates SS {};
        File RawData {};
        RenderTarget* RT {};
        uint* TextureData {};
        uint CurFrame {};
        double StartTime {};
        double AverageRenderTime {};
    };

    int VideoDecodePacket();
    void RenderVideo();
    bool IsVideoPlayed() { return !ShowVideos.empty(); }
    bool IsCanStopVideo() { return ShowVideos.size() && ShowVideos[0].CanStop; }
    void AddVideo(const char* video_name, bool can_stop, bool clear_sequence);
    void PlayVideo();
    void NextVideo();
    void StopVideo();

    vector<ShowVideo> ShowVideos {};
    int MusicVolumeRestore {};
    VideoContext* CurVideo {};

    /************************************************************************/
    /* Animation                                                            */
    /************************************************************************/
    struct IfaceAnim
    {
        AnyFrames* Frames;
        ushort Flags;
        uint CurSpr;
        uint LastTick;
        AtlasType ResType;

        IfaceAnim(AnyFrames* frm, AtlasType res_type) :
            Frames(frm), Flags(0), CurSpr(0), LastTick(Timer::GameTick()), ResType(res_type)
        {
        }
    };
    typedef vector<IfaceAnim*> IfaceAnimVec;

#define ANIMRUN_TO_END (0x0001)
#define ANIMRUN_FROM_END (0x0002)
#define ANIMRUN_CYCLE (0x0004)
#define ANIMRUN_STOP (0x0008)
#define ANIMRUN_SET_FRM(frm) ((uint(uchar((frm) + 1))) << 16)

    IfaceAnimVec Animations;

    uint AnimLoad(uint name_hash, AtlasType res_type);
    uint AnimLoad(const char* fname, AtlasType res_type);
    uint AnimGetCurSpr(uint anim_id);
    uint AnimGetCurSprCnt(uint anim_id);
    uint AnimGetSprCount(uint anim_id);
    AnyFrames* AnimGetFrames(uint anim_id);
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
        ScreenEffect(uint begin_tick, uint time, uint col, uint end_col) :
            BeginTick(begin_tick), Time(time), StartColor(col), EndColor(end_col)
        {
        }
    };
    typedef vector<ScreenEffect> ScreenEffectVec;

    // Fading
    ScreenEffectVec ScreenEffects;

    // Quake
    int ScreenOffsX, ScreenOffsY;
    float ScreenOffsXf, ScreenOffsYf, ScreenOffsStep;
    uint ScreenOffsNextTick;

    void ScreenFadeIn() { ScreenFade(1000, COLOR_RGBA(0, 0, 0, 0), COLOR_RGBA(255, 0, 0, 0), false); }
    void ScreenFadeOut() { ScreenFade(1000, COLOR_RGBA(255, 0, 0, 0), COLOR_RGBA(0, 0, 0, 0), false); }
    void ScreenFade(uint time, uint from_color, uint to_color, bool push_back);
    void ScreenQuake(int noise, uint time);
    void ProcessScreenEffectFading();
    void ProcessScreenEffectQuake();

    /************************************************************************/
    /* Scripting                                                            */
    /************************************************************************/
    bool ReloadScripts();
    void OnItemInvChanged(ItemView* old_item, ItemView* new_item);

    /************************************************************************/
    /* Game                                                                 */
    /************************************************************************/
    struct MapText
    {
        ushort HexX, HexY;
        uint StartTick, Tick;
        string Text;
        uint Color;
        bool Fade;
        Rect Pos;
        Rect EndPos;
        bool operator==(const MapText& r) { return HexX == r.HexX && HexY == r.HexY; }
    };
    typedef vector<MapText> MapTextVec;

    MapTextVec GameMapTexts;
    uint GameMouseStay;

    void GameDraw();

    /************************************************************************/
    /* Dialog                                                               */
    /************************************************************************/
    uchar DlgIsNpc;
    uint DlgNpcId;

    void FormatTags(string& text, CritterView* player, CritterView* npc, const string& lexems);

    /************************************************************************/
    /* Mini-map                                                             */
    /************************************************************************/
#define MINIMAP_PREPARE_TICK (1000)

    PointVec LmapPrepPix;
    Rect LmapWMap;
    int LmapZoom;
    bool LmapSwitchHi;
    uint LmapPrepareNextTick;

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
        bool operator==(const uint& _right) { return (this->LocId == _right); }
    };
    typedef vector<GmapLocation> GmapLocationVec;
    GmapLocationVec GmapLoc;
    GmapLocation GmapTownLoc;

    void GmapNullParams();

    /************************************************************************/
    /* PipBoy                                                               */
    /************************************************************************/
    // HoloInfo
    uint HoloInfo[MAX_HOLO_INFO];

    // Automaps
    struct Automap
    {
        uint LocId;
        hash LocPid;
        string LocName;
        HashVec MapPids;
        StrVec MapNames;
        uint CurMap;

        Automap() : LocId(0), LocPid(0), CurMap(0) {}
        bool operator==(const uint id) const { return LocId == id; }
    };
    typedef vector<Automap> AutomapVec;

    AutomapVec Automaps;

    /************************************************************************/
    /* PickUp                                                               */
    /************************************************************************/
    uchar PupTransferType;
    uint PupContId;
    hash PupContPid;

    /************************************************************************/
    /* Wait                                                                 */
    /************************************************************************/
    AnyFrames* WaitPic;

    void WaitDraw();

    /************************************************************************/
    /* Generic                                                              */
    /************************************************************************/
    uint DaySumRGB;

    void SetDayTime(bool refresh);
    void SetGameColor(uint color);

    CritterView* Chosen;

    void AddCritter(CritterView* cr);
    CritterView* GetCritter(uint crid) { return HexMngr.GetCritter(crid); }
    ItemHexView* GetItem(uint item_id) { return HexMngr.GetItemById(item_id); }
    void DeleteCritters();
    void DeleteCritter(uint remid);

    bool NoLogOut;
    bool RebuildLookBorders;
    bool DrawLookBorders, DrawShootBorders;
    PointVec LookBorders, ShootBorders;

    void LookBordersPrepare();

    /************************************************************************/
    /* MessBox                                                              */
    /************************************************************************/
#define FOMB_GAME (0)
#define FOMB_TALK (1)
    void AddMess(int mess_type, const string& msg, bool script_call = false);
};
