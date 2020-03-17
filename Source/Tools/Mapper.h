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

// Todo: add standalone Mapper application

#pragma once

#include "Common.h"

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
#include "MapLoader.h"
#include "MapView.h"
#include "MapperScripting.h"
#include "MsgFiles.h"
#include "NetBuffer.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
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

class FOMapper : public NonMovable // Todo: rename FOMapper to just Mapper
{
public:
    MapperSettings& Settings;
    GeometryHelper GeomHelper;
    FileManager FileMngr;
    FileManager ServerFileMngr;
    MapperScriptSystem ScriptSys;
    CacheStorage Cache;
    Keyboard Keyb;
    ProtoManager ProtoMngr;
    EffectManager EffectMngr;
    SpriteManager SprMngr;
    HexManager HexMngr;
    ResourceManager ResMngr;
    ConfigFile IfaceIni;
    string ServerWritePath {};
    string ClientWritePath {};
    PropertyVec ShowProps {};
    MapView* ClientCurMap {};
    LocationView* ClientCurLocation {};

    FOMapper(GlobalSettings& sett);
    int InitIface();
    bool IfaceLoadRect(Rect& comp, const char* name);
    void MainLoop();
    void RefreshTiles(int tab);
    uint GetProtoItemCurSprId(ProtoItem* proto_item);

    void ProcessInputEvents();
    void ProcessInputEvent(const InputEvent& event);

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
        IfaceAnim(AnyFrames* frm, AtlasType res_type);

        AnyFrames* Frames {};
        ushort Flags {};
        uint CurSpr {};
        uint LastTick {};
        AtlasType ResType {};
    };
    using IfaceAnimVec = vector<IfaceAnim*>;

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
    void ObjKeyDown(KeyCode dik, const char* dik_text);
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
    KeyCode ConsoleLastKey;
    string ConsoleLastKeyText;
    uint ConsoleKeyTick;
    int ConsoleAccelerate;

    void ConsoleDraw();
    void ConsoleKeyDown(KeyCode dik, const char* dik_text);
    void ConsoleKeyUp(KeyCode dik);
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
    bool SpritesCanDraw;
    void RunStartScript();
    void RunMapLoadScript(MapView* map);
    void RunMapSaveScript(MapView* map);
    void DrawIfaceLayer(uint layer);

    void OnSetItemFlags(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemSomeLight(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemPicMap(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemOffsetXY(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value);
};
