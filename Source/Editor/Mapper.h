#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "EffectManager.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "HexManager.h"
#include "Keyboard.h"
#include "MsgFiles.h"
#include "NetBuffer.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Script.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "Testing.h"

// Fonts
#define FONT_FO (0)
#define FONT_NUM (1)
#define FONT_BIG_NUM (2)
#define FONT_SAND_NUM (3)
#define FONT_SPECIAL (4)
#define FONT_THIN (6)
#define FONT_FAT (7)
#define FONT_BIG (8)
#ifndef FONT_DEFAULT
#define FONT_DEFAULT (5)
#endif

class CScriptDictionary;
class CScriptDict;
class CScriptArray;

class FOMapper
{
public:
    static FOMapper* Self;
    MapperSettings& Settings;
    GeometryHelper GeomHelper;
    FileManager FileMngr;
    FileManager ServerFileMngr;
    CacheStorage Cache;
    Keyboard Keyb;
    ProtoManager ProtoMngr;
    EffectManager EffectMngr;
    SpriteManager SprMngr;
    HexManager HexMngr;
    ResourceManager ResMngr;
    ConfigFile IfaceIni;
    static string ServerWritePath;
    static string ClientWritePath;
    PropertyVec ShowProps;

    FOMapper(MapperSettings& sett);
    ~FOMapper();
    int InitIface();
    bool IfaceLoadRect(Rect& comp, const char* name);
    void MainLoop();
    void RefreshTiles(int tab);
    uint GetProtoItemCurSprId(ProtoItem* proto_item);

    void ParseKeyboard();
    void ParseMouse();

#define DRAW_CR_INFO_MAX (2)
    int DrawCrExtInfo;

    // Game color
    void ChangeGameTime();

    // MSG File
    LanguagePack CurLang;

    // Map text
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

    // Animations
    struct IfaceAnim
    {
        AnyFrames* Frames;
        ushort Flags;
        uint CurSpr;
        uint LastTick;
        AtlasType ResType;

        IfaceAnim(AnyFrames* frm, AtlasType res_type) :
            Frames(frm), Flags(0), CurSpr(0), LastTick(Timer::FastTick()), ResType(res_type)
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
    void AnimFree(AtlasType res_type);

    // Cursor
    int CurMode;
#define CUR_MODE_DEFAULT (0)
#define CUR_MODE_MOVE_SELECTION (1)
#define CUR_MODE_PLACE_OBJECT (2)

    AnyFrames *CurPDef, *CurPHand;

    void CurDraw();
    void CurRMouseUp();
    void CurMMouseDown();

    bool IsCurInRect(Rect& rect, int ax, int ay);
    bool IsCurInRect(Rect& rect);
    bool IsCurInRectNoTransp(uint spr_id, Rect& rect, int ax, int ay);
    bool IsCurInInterface();
    bool GetCurHex(ushort& hx, ushort& hy, bool ignore_interface);

    int IntMode;
#define INT_MODE_CUSTOM0 (0)
#define INT_MODE_CUSTOM1 (1)
#define INT_MODE_CUSTOM2 (2)
#define INT_MODE_CUSTOM3 (3)
#define INT_MODE_CUSTOM4 (4)
#define INT_MODE_CUSTOM5 (5)
#define INT_MODE_CUSTOM6 (6)
#define INT_MODE_CUSTOM7 (7)
#define INT_MODE_CUSTOM8 (8)
#define INT_MODE_CUSTOM9 (9)
#define INT_MODE_ITEM (10)
#define INT_MODE_TILE (11)
#define INT_MODE_CRIT (12)
#define INT_MODE_FAST (13)
#define INT_MODE_IGNORE (14)
#define INT_MODE_INCONT (15)
#define INT_MODE_MESS (16)
#define INT_MODE_LIST (17)
#define INT_MODE_COUNT (18)
#define TAB_COUNT (15)

    int IntHold;
#define INT_NONE (0)
#define INT_BUTTON (1)
#define INT_MAIN (2)
#define INT_SELECT (3)
#define INT_OBJECT (4)
#define INT_SUB_TAB (5)

    AnyFrames *IntMainPic, *IntPTab, *IntPSelect, *IntPShow;
    int IntX, IntY;
    int IntVectX, IntVectY;
    ushort SelectHX1, SelectHY1, SelectHX2, SelectHY2;
    int SelectX, SelectY;

#define SELECT_TYPE_OLD (0)
#define SELECT_TYPE_NEW (1)
    int SelectType;

    bool IntVisible, IntFix;

    Rect IntWMain;
    Rect IntWWork, IntWHint;

    Rect IntBCust[10], IntBItem, IntBTile, IntBCrit, IntBFast, IntBIgnore, IntBInCont, IntBMess, IntBList, IntBScrBack,
        IntBScrBackFst, IntBScrFront, IntBScrFrontFst;

    Rect IntBShowItem, IntBShowScen, IntBShowWall, IntBShowCrit, IntBShowTile, IntBShowRoof, IntBShowFast;

    void IntDraw();
    void IntLMouseDown();
    void IntLMouseUp();
    void IntMouseMove();
    void IntSetMode(int mode);

    // Maps
    MapViewVec LoadedMaps;
    MapView* ActiveMap;

// Tabs
#define DEFAULT_SUB_TAB "000 - all"
    struct SubTab
    {
        ProtoItemVec ItemProtos;
        ProtoCritterVec NpcProtos;
        StrVec TileNames;
        HashVec TileHashes;
        int Index, Scroll;
        SubTab() : Index(0), Scroll(0) {}
    };
    typedef map<string, SubTab> SubTabMap;

    struct TileTab
    {
        StrVec TileDirs;
        BoolVec TileSubDirs;
    };

    SubTabMap Tabs[TAB_COUNT];
    SubTab* TabsActive[TAB_COUNT];
    TileTab TabsTiles[TAB_COUNT];
    string TabsName[INT_MODE_COUNT];
    int TabsScroll[INT_MODE_COUNT];

    bool SubTabsActive;
    int SubTabsActiveTab;
    AnyFrames* SubTabsPic;
    Rect SubTabsRect;
    int SubTabsX, SubTabsY;

    // Prototypes
    ProtoItemVec* CurItemProtos;
    HashVec* CurTileHashes;
    StrVec* CurTileNames;
    ProtoCritterVec* CurNpcProtos;
    int NpcDir;
    int* CurProtoScroll;
    uint ProtoWidth;
    uint ProtosOnScreen;
    uint TabIndex[INT_MODE_COUNT];
    int InContScroll;
    int ListScroll;
    ItemView* InContItem;
    bool DrawRoof;
    int TileLayer;

    uint GetTabIndex();
    void SetTabIndex(uint index);
    void RefreshCurProtos();
    bool IsObjectMode() { return CurItemProtos && CurProtoScroll; }
    bool IsTileMode() { return CurTileHashes && CurTileNames && CurProtoScroll; }
    bool IsCritMode() { return CurNpcProtos && CurProtoScroll; }

    // Select
    Rect IntBSelectItem, IntBSelectScen, IntBSelectWall, IntBSelectCrit, IntBSelectTile, IntBSelectRoof;
    bool IsSelectItem, IsSelectScen, IsSelectWall, IsSelectCrit, IsSelectTile, IsSelectRoof;
    EntityVec SelectedEntities;

    // Select Tile, Roof
    struct SelMapTile
    {
        ushort HexX, HexY;
        bool IsRoof;

        SelMapTile(ushort hx, ushort hy, bool is_roof) : HexX(hx), HexY(hy), IsRoof(is_roof) {}
    };
    typedef vector<SelMapTile> SelMapTileVec;
    SelMapTileVec SelectedTile;

    // Select methods
    void MoveEntity(Entity* entity, ushort hx, ushort hy);
    void DeleteEntity(Entity* entity);
    void SelectClear();
    void SelectAddItem(ItemHexView* item);
    void SelectAddCrit(CritterView* npc);
    void SelectAddTile(ushort hx, ushort hy, bool is_roof);
    void SelectAdd(Entity* entity);
    void SelectErase(Entity* entity);
    void SelectAll();
    bool SelectMove(bool hex_move, int& offs_hx, int& offs_hy, int& offs_x, int& offs_y);
    void SelectDelete();

    // Entites creation
    CritterView* AddCritter(hash pid, ushort hx, ushort hy);
    ItemView* AddItem(hash pid, ushort hx, ushort hy, Entity* owner);
    void AddTile(hash name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof);
    Entity* CloneEntity(Entity* entity);

    // Buffer
    struct EntityBuf
    {
        ushort HexX;
        ushort HexY;
        EntityType Type;
        ProtoEntity* Proto;
        Properties* Props;
        vector<EntityBuf*> Children;
    };
    struct TileBuf
    {
        hash Name;
        ushort HexX, HexY;
        short OffsX, OffsY;
        uchar Layer;
        bool IsRoof;
    };
    typedef vector<TileBuf> TileBufVec;
    typedef vector<EntityBuf> EntityBufVec;

    EntityBufVec EntitiesBuffer;
    TileBufVec TilesBuffer;

    void BufferCopy();
    void BufferCut();
    void BufferPaste(int hx, int hy);

// Object
#define DRAW_NEXT_HEIGHT (12)

    AnyFrames *ObjWMainPic, *ObjPBToAllDn;
    Rect ObjWMain, ObjWWork, ObjBToAll;
    int ObjX, ObjY;
    int ItemVectX, ItemVectY;
    int ObjCurLine;
    bool ObjCurLineIsConst;
    string ObjCurLineInitValue;
    string ObjCurLineValue;
    bool ObjVisible, ObjFix;
    bool ObjToAll;
    Entity* InspectorEntity;

    void ObjDraw();
    void DrawLine(const string& name, const string& type_name, const string& text, bool is_const, Rect& r);
    void ObjKeyDown(uchar dik, const char* dik_text);
    void ObjKeyDownApply(Entity* entity);
    void SelectEntityProp(int line);
    Entity* GetInspectorEntity();

    // Console
    AnyFrames* ConsolePic;
    int ConsolePicX, ConsolePicY, ConsoleTextX, ConsoleTextY;
    bool ConsoleEdit;
    string ConsoleStr;
    uint ConsoleCur;

    vector<string> ConsoleHistory;
    int ConsoleHistoryCur;

#define CONSOLE_KEY_TICK (500)
#define CONSOLE_MAX_ACCELERATE (460)
    int ConsoleLastKey;
    string ConsoleLastKeyText;
    uint ConsoleKeyTick;
    int ConsoleAccelerate;

    void ConsoleDraw();
    void ConsoleKeyDown(uchar dik, const char* dik_text);
    void ConsoleKeyUp(uchar dik);
    void ConsoleProcess();
    void ParseCommand(const string& command);

    // Mess box
    struct MessBoxMessage
    {
        int Type;
        string Mess;
        string Time;

        MessBoxMessage(int type, const char* mess, const char* time) : Type(type), Mess(mess), Time(time) {}
    };
    typedef vector<MessBoxMessage> MessBoxMessageVec;

    MessBoxMessageVec MessBox;
    string MessBoxCurText;
    int MessBoxScroll;

    void MessBoxGenerate();
    void AddMess(const char* message_text);
    void MessBoxDraw();
    bool SaveLogFile();

    // Scripts
    static bool SpritesCanDraw;
    bool InitScriptSystem();
    void FinishScriptSystem();
    void RunStartScript();
    void RunMapLoadScript(MapView* map);
    void RunMapSaveScript(MapView* map);
    void DrawIfaceLayer(uint layer);

    static void OnSetItemFlags(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemSomeLight(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemPicMap(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemOffsetXY(Entity* entity, Property* prop, void* cur_value, void* old_value);
    static void OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value);

    struct SScriptFunc
    {
        static ItemView* Item_AddChild(ItemView* item, hash pid);
        static ItemView* Crit_AddChild(CritterView* cr, hash pid);
        static CScriptArray* Item_GetChildren(ItemView* item);
        static CScriptArray* Crit_GetChildren(CritterView* cr);

        static ItemView* Global_AddItem(hash pid, ushort hx, ushort hy);
        static CritterView* Global_AddCritter(hash pid, ushort hx, ushort hy);
        static ItemView* Global_GetItemByHex(ushort hx, ushort hy);
        static CScriptArray* Global_GetItemsByHex(ushort hx, ushort hy);
        static CritterView* Global_GetCritterByHex(ushort hx, ushort hy, int find_type);
        static CScriptArray* Global_GetCrittersByHex(ushort hx, ushort hy, int find_type);
        static void Global_MoveEntity(Entity* entity, ushort hx, ushort hy);
        static void Global_DeleteEntity(Entity* entity);
        static void Global_DeleteEntities(CScriptArray* entities);
        static void Global_SelectEntity(Entity* entity, bool set);
        static void Global_SelectEntities(CScriptArray* entities, bool set);
        static Entity* Global_GetSelectedEntity();
        static CScriptArray* Global_GetSelectedEntities();

        static uint Global_GetTilesCount(ushort hx, ushort hy, bool roof);
        static void Global_DeleteTile(ushort hx, ushort hy, bool roof, int layer);
        static hash Global_GetTileHash(ushort hx, ushort hy, bool roof, int layer);
        static void Global_AddTileHash(ushort hx, ushort hy, int ox, int oy, int layer, bool roof, hash pic_hash);
        static string Global_GetTileName(ushort hx, ushort hy, bool roof, int layer);
        static void Global_AddTileName(ushort hx, ushort hy, int ox, int oy, int layer, bool roof, string pic_name);

        static void Global_AllowSlot(uchar index, bool enable_send);
        static void Global_SetPropertyGetCallback(asIScriptGeneric* gen);
        static MapView* Global_LoadMap(string file_name);
        static void Global_UnloadMap(MapView* pmap);
        static bool Global_SaveMap(MapView* pmap, string custom_name);
        static bool Global_ShowMap(MapView* pmap);
        static CScriptArray* Global_GetLoadedMaps(int& index);
        static CScriptArray* Global_GetMapFileNames(string dir);
        static void Global_ResizeMap(ushort width, ushort height);

        static uint Global_TabGetTileDirs(int tab, CScriptArray* dir_names, CScriptArray* include_subdirs);
        static uint Global_TabGetItemPids(int tab, string sub_tab, CScriptArray* item_pids);
        static uint Global_TabGetCritterPids(int tab, string sub_tab, CScriptArray* critter_pids);
        static void Global_TabSetTileDirs(int tab, CScriptArray* dir_names, CScriptArray* include_subdirs);
        static void Global_TabSetItemPids(int tab, string sub_tab, CScriptArray* item_pids);
        static void Global_TabSetCritterPids(int tab, string sub_tab, CScriptArray* critter_pids);
        static void Global_TabDelete(int tab);
        static void Global_TabSelect(int tab, string sub_tab, bool show);
        static void Global_TabSetName(int tab, string tab_name);

        static void Global_MoveScreenToHex(ushort hx, ushort hy, uint speed, bool can_stop);
        static void Global_MoveScreenOffset(int ox, int oy, uint speed, bool can_stop);
        static void Global_MoveHexByDir(ushort& hx, ushort& hy, uchar dir, uint steps);
        static string Global_GetIfaceIniStr(string key);
        static void Global_AddDataSource(string dat_name);
        static bool Global_LoadFont(int font, string font_fname);
        static void Global_SetDefaultFont(int font, uint color);
        static void Global_MouseClick(int x, int y, int button);
        static void Global_KeyboardPress(uchar key1, uchar key2, string key1_text, string key2_text);
        static void Global_SetRainAnimation(string fall_anim_name, string drop_anim_name);
        static void Global_ChangeZoom(float target_zoom);

        static void Global_Message(string msg);
        static void Global_MessageMsg(int text_msg, uint str_num);
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

        static void Global_GetHexInPath(
            ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist);
        static uint Global_GetPathLengthHex(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut);
        static bool Global_GetHexPos(ushort hx, ushort hy, int& x, int& y);
        static bool Global_GetMonitorHex(int x, int y, ushort& hx, ushort& hy, bool ignore_interface);
        static Entity* Global_GetMonitorObject(int x, int y, bool ignore_interface);

        static uint Global_LoadSprite(string spr_name);
        static uint Global_LoadSpriteHash(uint name_hash);
        static int Global_GetSpriteWidth(uint spr_id, int spr_index);
        static int Global_GetSpriteHeight(uint spr_id, int spr_index);
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

        static MapView* ClientCurMap;
        static LocationView* ClientCurLocation;
    };
};
