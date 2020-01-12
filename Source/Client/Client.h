#pragma once

#include "3dStuff.h"
#include "Common.h"
#include "EffectManager.h"
#include "Entity.h"
#include "HexManager.h"
#include "Keyboard.h"
#include "MsgFiles.h"
#include "NetBuffer.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Script.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "Testing.h"
#include "WinApi_Include.h"

#include "theora/theoradec.h"
#include "zlib.h"

// Network
#ifndef FO_WINDOWS
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

class FOClient
{
public:
    static FOClient* Self;
    FOClient();
    ~FOClient();
    bool Reset();
    void ProcessAutoLogin();
    void Restart();
    void UpdateBinary();
    void TryExit();
    bool IsCurInWindow();
    void FlashGameWindow();
    void MainLoop();
    void NetDisconnect();
    void DrawIface();

    int InitCalls;
    bool DoRestart;
    Keyboard Keyb;
    ProtoManager ProtoMngr;
    EffectManager EffectMngr;
    SpriteManager SprMngr;
    HexManager HexMngr;
    ResourceManager ResMngr;
    SoundManager SndMngr;
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
    EffectVec OffscreenEffects;
    RenderTargetVec OffscreenSurfaces;
    RenderTargetVec ActiveOffscreenSurfaces;
    RenderTargetVec PreDirtyOffscreenSurfaces;
    RenderTargetVec DirtyOffscreenSurfaces;

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
    void ParseKeyboard();
    void ParseMouse();

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
    void* UpdateFileTemp;

    void UpdateFilesStart();
    void UpdateFilesLoop();
    void UpdateFilesAddText(uint num_str, const string& num_str_str);
    void UpdateFilesAbort(uint num_str, const string& num_str_str);

    // Network
    uchar* ComBuf;
    uint ComLen;
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
    bool NetConnect(const char* host, ushort port);
    bool FillSockAddr(sockaddr_in& saddr, const char* host, ushort port);
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
        string FileName;
        string SoundName;
        bool CanStop;
    };
    typedef vector<ShowVideo> ShowVideoVec;

    struct VideoContext
    {
        th_dec_ctx* Context;
        th_info VideoInfo;
        th_comment Comment;
        th_setup_info* SetupInfo;
        th_ycbcr_buffer ColorBuffer;
        ogg_sync_state SyncState;
        ogg_packet Packet;
        struct StreamStates
        {
            static const uint COUNT = 10;
            ogg_stream_state Streams[COUNT];
            bool StreamsState[COUNT];
            int MainIndex;
        } SS;
        File RawData;
        RenderTarget* RT;
        uchar* TextureData;
        uint CurFrame;
        double StartTime;
        double AverageRenderTime;
    };

    ShowVideoVec ShowVideos;
    int MusicVolumeRestore;
    VideoContext* CurVideo;

    int VideoDecodePacket();
    void RenderVideo();
    bool IsVideoPlayed() { return !ShowVideos.empty(); }
    bool IsCanStopVideo() { return ShowVideos.size() && ShowVideos[0].CanStop; }
    void AddVideo(const char* video_name, bool can_stop, bool clear_sequence);
    void PlayVideo();
    void NextVideo();
    void StopVideo();

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
    // Mirror
    Texture* ScreenMirrorTexture;
    int ScreenMirrorX, ScreenMirrorY;
    uint ScreenMirrorEndTick;
    bool ScreenMirrorStart;

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

    struct SScriptFunc
    {
        static bool Crit_IsChosen(CritterView* cr);
        static bool Crit_IsPlayer(CritterView* cr);
        static bool Crit_IsNpc(CritterView* cr);
        static bool Crit_IsOffline(CritterView* cr);
        static bool Crit_IsLife(CritterView* cr);
        static bool Crit_IsKnockout(CritterView* cr);
        static bool Crit_IsDead(CritterView* cr);
        static bool Crit_IsFree(CritterView* cr);
        static bool Crit_IsBusy(CritterView* cr);

        static bool Crit_IsAnim3d(CritterView* cr);
        static bool Crit_IsAnimAviable(CritterView* cr, uint anim1, uint anim2);
        static bool Crit_IsAnimPlaying(CritterView* cr);
        static uint Crit_GetAnim1(CritterView* cr);
        static void Crit_Animate(CritterView* cr, uint anim1, uint anim2);
        static void Crit_AnimateEx(CritterView* cr, uint anim1, uint anim2, ItemView* item);
        static void Crit_ClearAnim(CritterView* cr);
        static void Crit_Wait(CritterView* cr, uint ms);
        static uint Crit_CountItem(CritterView* cr, hash proto_id);
        static ItemView* Crit_GetItem(CritterView* cr, uint item_id);
        static ItemView* Crit_GetItemPredicate(CritterView* cr, asIScriptFunction* predicate);
        static ItemView* Crit_GetItemBySlot(CritterView* cr, uchar slot);
        static ItemView* Crit_GetItemByPid(CritterView* cr, hash proto_id);
        static CScriptArray* Crit_GetItems(CritterView* cr);
        static CScriptArray* Crit_GetItemsBySlot(CritterView* cr, uchar slot);
        static CScriptArray* Crit_GetItemsPredicate(CritterView* cr, asIScriptFunction* predicate);
        static void Crit_SetVisible(CritterView* cr, bool visible);
        static bool Crit_GetVisible(CritterView* cr);
        static void Crit_set_ContourColor(CritterView* cr, uint value);
        static uint Crit_get_ContourColor(CritterView* cr);
        static void Crit_GetNameTextInfo(
            CritterView* cr, bool& name_visible, int& x, int& y, int& w, int& h, int& lines);
        static void Crit_AddAnimationCallback(
            CritterView* cr, uint anim1, uint anim2, float normalized_time, asIScriptFunction* animation_callback);
        static bool Crit_GetBonePosition(CritterView* cr, hash bone_name, int& bone_x, int& bone_y);

        static ItemView* Item_Clone(ItemView* item, uint count);
        static bool Item_GetMapPosition(ItemView* item, ushort& hx, ushort& hy);
        static void Item_Animate(ItemView* item, uint from_frame, uint to_frame);
        static CScriptArray* Item_GetItems(ItemView* cont, uint stack_id);

        static string Global_CustomCall(string command, string separator);
        static CritterView* Global_GetChosen();
        static ItemView* Global_GetItem(uint item_id);
        static CScriptArray* Global_GetMapAllItems();
        static CScriptArray* Global_GetMapHexItems(ushort hx, ushort hy);
        static uint Global_GetCrittersDistantion(CritterView* cr1, CritterView* cr2);
        static CritterView* Global_GetCritter(uint critter_id);
        static CScriptArray* Global_GetCritters(ushort hx, ushort hy, uint radius, int find_type);
        static CScriptArray* Global_GetCrittersByPids(hash pid, int find_type);
        static CScriptArray* Global_GetCrittersInPath(
            ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type);
        static CScriptArray* Global_GetCrittersInPathBlock(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy,
            float angle, uint dist, int find_type, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx,
            ushort& block_hy);
        static void Global_GetHexInPath(
            ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist);
        static CScriptArray* Global_GetPathHex(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut);
        static CScriptArray* Global_GetPathCr(CritterView* cr, ushort to_hx, ushort to_hy, uint cut);
        static uint Global_GetPathLengthHex(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut);
        static uint Global_GetPathLengthCr(CritterView* cr, ushort to_hx, ushort to_hy, uint cut);
        static void Global_FlushScreen(uint from_color, uint to_color, uint ms);
        static void Global_QuakeScreen(uint noise, uint ms);
        static bool Global_PlaySound(string sound_name);
        static bool Global_PlayMusic(string music_name, uint repeat_time);
        static void Global_PlayVideo(string video_name, bool can_stop);

        static hash Global_GetCurrentMapPid();
        static void Global_Message(string msg);
        static void Global_MessageType(string msg, int type);
        static void Global_MessageMsg(int text_msg, uint str_num);
        static void Global_MessageMsgType(int text_msg, uint str_num, int type);
        static void Global_MapMessage(
            string text, ushort hx, ushort hy, uint ms, uint color, bool fade, int ox, int oy);
        static string Global_GetMsgStr(int text_msg, uint str_num);
        static string Global_GetMsgStrSkip(int text_msg, uint str_num, uint skip_count);
        static uint Global_GetMsgStrNumUpper(int text_msg, uint str_num);
        static uint Global_GetMsgStrNumLower(int text_msg, uint str_num);
        static uint Global_GetMsgStrCount(int text_msg, uint str_num);
        static bool Global_IsMsgStr(int text_msg, uint str_num);
        static string Global_ReplaceTextStr(string text, string replace, string str);
        static string Global_ReplaceTextInt(string text, string replace, int i);
        static string Global_FormatTags(string text, string lexems);
        static void Global_MoveScreenToHex(ushort hx, ushort hy, uint speed, bool can_stop);
        static void Global_MoveScreenOffset(int ox, int oy, uint speed, bool can_stop);
        static void Global_LockScreenScroll(CritterView* cr, bool soft_lock, bool unlock_if_same);
        static int Global_GetFog(ushort zone_x, ushort zone_y);
        static uint Global_GetDayTime(uint day_part);
        static void Global_GetDayColor(uint day_part, uchar& r, uchar& g, uchar& b);

        static uint Global_GetFullSecond(
            ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second);
        static void Global_GetGameTime(uint full_second, ushort& year, ushort& month, ushort& day, ushort& day_of_week,
            ushort& hour, ushort& minute, ushort& second);
        static void Global_GetTime(ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour,
            ushort& minute, ushort& second, ushort& milliseconds);
        static void Global_SetPropertyGetCallback(asIScriptGeneric* gen);
        static void Global_AddPropertySetCallback(asIScriptGeneric* gen);
        static void Global_AllowSlot(uchar index, bool enable_send);
        static bool Global_LoadDataFile(string dat_name);

        static uint Global_LoadSprite(string spr_name);
        static uint Global_LoadSpriteHash(uint name_hash);
        static int Global_GetSpriteWidth(uint spr_id, int frame_index);
        static int Global_GetSpriteHeight(uint spr_id, int frame_index);
        static uint Global_GetSpriteCount(uint spr_id);
        static uint Global_GetSpriteTicks(uint spr_id);
        static uint Global_GetPixelColor(uint spr_id, int frame_index, int x, int y);
        static void Global_GetTextInfo(string text, int w, int h, int font, int flags, int& tw, int& th, int& lines);
        static void Global_DrawSprite(uint spr_id, int frame_index, int x, int y, uint color, bool offs);
        static void Global_DrawSpriteSize(
            uint spr_id, int frame_index, int x, int y, int w, int h, bool zoom, uint color, bool offs);
        static void Global_DrawSpritePattern(
            uint spr_id, int frame_index, int x, int y, int w, int h, int spr_width, int spr_height, uint color);
        static void Global_DrawText(string text, int x, int y, int w, int h, uint color, int font, int flags);
        static void Global_DrawPrimitive(int primitive_type, CScriptArray* data);
        static void Global_DrawMapSprite(MapSprite* map_spr);
        static void Global_DrawCritter2d(hash model_name, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b,
            bool scratch, bool center, uint color);
        static void Global_DrawCritter3d(uint instance, hash model_name, uint anim1, uint anim2, CScriptArray* layers,
            CScriptArray* position, uint color);
        static void Global_PushDrawScissor(int x, int y, int w, int h);
        static void Global_PopDrawScissor();
        static void Global_ActivateOffscreenSurface(bool force_clear);
        static void Global_PresentOffscreenSurface(int effect_subtype);
        static void Global_PresentOffscreenSurfaceExt(int effect_subtype, int x, int y, int w, int h);
        static void Global_PresentOffscreenSurfaceExt2(
            int effect_subtype, int from_x, int from_y, int from_w, int from_h, int to_x, int to_y, int to_w, int to_h);

        static void Global_ShowScreen(int screen, CScriptDictionary* params);
        static void Global_HideScreen(int screen);
        static bool Global_GetHexPos(ushort hx, ushort hy, int& x, int& y);
        static bool Global_GetMonitorHex(int x, int y, ushort& hx, ushort& hy);
        static ItemView* Global_GetMonitorItem(int x, int y);
        static CritterView* Global_GetMonitorCritter(int x, int y);
        static Entity* Global_GetMonitorEntity(int x, int y);
        static ushort Global_GetMapWidth();
        static ushort Global_GetMapHeight();
        static bool Global_IsMapHexPassed(ushort hx, ushort hy);
        static bool Global_IsMapHexRaked(ushort hx, ushort hy);
        static void Global_MoveHexByDir(ushort& hx, ushort& hy, uchar dir, uint steps);
        static hash Global_GetTileName(ushort hx, ushort hy, bool roof, int layer);
        static void Global_Preload3dFiles(CScriptArray* fnames);
        static void Global_WaitPing();
        static bool Global_LoadFont(int font, string font_fname);
        static void Global_SetDefaultFont(int font, uint color);
        static bool Global_SetEffect(int effect_type, int effect_subtype, string effect_name, string effect_defines);
        static void Global_RefreshMap(bool only_tiles, bool only_roof, bool only_light);
        static void Global_MouseClick(int x, int y, int button);
        static void Global_KeyboardPress(uchar key1, uchar key2, string key1_text, string key2_text);
        static void Global_SetRainAnimation(string fall_anim_name, string drop_anim_name);
        static void Global_ChangeZoom(float target_zoom);
        static bool Global_SaveScreenshot(string file_path);
        static bool Global_SaveText(string file_path, string text);
        static void Global_SetCacheData(string name, const CScriptArray* data);
        static void Global_SetCacheDataSize(string name, const CScriptArray* data, uint data_size);
        static CScriptArray* Global_GetCacheData(string name);
        static void Global_SetCacheDataStr(string name, string str);
        static string Global_GetCacheDataStr(string name);
        static bool Global_IsCacheData(string name);
        static void Global_EraseCacheData(string name);
        static void Global_SetUserConfig(CScriptArray* key_values);

        static MapView* ClientCurMap;
        static LocationView* ClientCurLocation;
    } ScriptFunc;

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
    TwoBitMask* GmapFog;
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
