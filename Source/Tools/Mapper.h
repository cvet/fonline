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

#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "Client.h"
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
#include "PlayerView.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "Timer.h"

class FOMapper final : public FOClient
{
public:
    struct IfaceAnim
    {
        AnyFrames* Frames {};
        AtlasType ResType {};
        uint LastTick {};
        ushort Flags {};
        uint CurSpr {};
    };

    struct SubTab
    {
        vector<const ProtoItem*> ItemProtos {};
        vector<const ProtoCritter*> NpcProtos {};
        vector<string> TileNames {};
        vector<hash> TileHashes {};
        int Index {};
        int Scroll {};
    };

    struct TileTab
    {
        vector<string> TileDirs {};
        vector<bool> TileSubDirs {};
    };

    struct SelMapTile
    {
        ushort HexX {};
        ushort HexY {};
        bool IsRoof {};
    };

    struct EntityBuf
    {
        ushort HexX {};
        ushort HexY {};
        bool IsCritter {};
        bool IsItem {};
        const ProtoEntity* Proto {};
        Properties* Props {};
        vector<EntityBuf*> Children {};
    };

    struct TileBuf
    {
        hash Name {};
        ushort HexX {};
        ushort HexY {};
        short OffsX {};
        short OffsY {};
        uchar Layer {};
        bool IsRoof {};
    };

    struct MessBoxMessage
    {
        int Type {};
        string Mess {};
        string Time {};
    };

    static constexpr auto FONT_FO = 0;
    static constexpr auto FONT_NUM = 1;
    static constexpr auto FONT_BIG_NUM = 2;
    static constexpr auto FONT_SAND_NUM = 3;
    static constexpr auto FONT_SPECIAL = 4;
    static constexpr auto FONT_DEFAULT = 5;
    static constexpr auto FONT_THIN = 6;
    static constexpr auto FONT_FAT = 7;
    static constexpr auto FONT_BIG = 8;
    static constexpr auto DRAW_CR_INFO_MAX = 2;
    static constexpr auto CUR_MODE_DEFAULT = 0;
    static constexpr auto CUR_MODE_MOVE_SELECTION = 1;
    static constexpr auto CUR_MODE_PLACE_OBJECT = 2;
    static constexpr auto INT_MODE_CUSTOM0 = 0;
    static constexpr auto INT_MODE_CUSTOM1 = 1;
    static constexpr auto INT_MODE_CUSTOM2 = 2;
    static constexpr auto INT_MODE_CUSTOM3 = 3;
    static constexpr auto INT_MODE_CUSTOM4 = 4;
    static constexpr auto INT_MODE_CUSTOM5 = 5;
    static constexpr auto INT_MODE_CUSTOM6 = 6;
    static constexpr auto INT_MODE_CUSTOM7 = 7;
    static constexpr auto INT_MODE_CUSTOM8 = 8;
    static constexpr auto INT_MODE_CUSTOM9 = 9;
    static constexpr auto INT_MODE_ITEM = 10;
    static constexpr auto INT_MODE_TILE = 11;
    static constexpr auto INT_MODE_CRIT = 12;
    static constexpr auto INT_MODE_FAST = 13;
    static constexpr auto INT_MODE_IGNORE = 14;
    static constexpr auto INT_MODE_INCONT = 15;
    static constexpr auto INT_MODE_MESS = 16;
    static constexpr auto INT_MODE_LIST = 17;
    static constexpr auto INT_MODE_COUNT = 18;
    static constexpr auto TAB_COUNT = 15;
    static constexpr auto INT_NONE = 0;
    static constexpr auto INT_BUTTON = 1;
    static constexpr auto INT_MAIN = 2;
    static constexpr auto INT_SELECT = 3;
    static constexpr auto INT_OBJECT = 4;
    static constexpr auto INT_SUB_TAB = 5;
    static constexpr auto SELECT_TYPE_OLD = 0;
    static constexpr auto SELECT_TYPE_NEW = 1;
    static constexpr auto DEFAULT_SUB_TAB = "000 - all";
    static constexpr auto DRAW_NEXT_HEIGHT = 12;
    static constexpr auto CONSOLE_KEY_TICK = 500;
    static constexpr auto CONSOLE_MAX_ACCELERATE = 460;

    FOMapper() = delete;
    explicit FOMapper(GlobalSettings& settings);
    FOMapper(const FOMapper&) = delete;
    FOMapper(FOMapper&&) noexcept = delete;
    auto operator=(const FOMapper&) = delete;
    auto operator=(FOMapper&&) noexcept = delete;
    ~FOMapper() = default;

    auto InitIface() -> int;
    auto IfaceLoadRect(IRect& comp, string_view name) -> bool;
    void MainLoop();
    void RefreshTiles(int tab);
    auto GetProtoItemCurSprId(const ProtoItem* proto_item) -> uint;
    void ChangeGameTime();
    void ProcessInputEvents();
    void ProcessInputEvent(const InputEvent& event);

    auto AnimLoad(uint name_hash, AtlasType res_type) -> uint;
    auto AnimLoad(string_view fname, AtlasType res_type) -> uint;
    auto AnimGetCurSpr(uint anim_id) -> uint;
    auto AnimGetCurSprCnt(uint anim_id) -> uint;
    auto AnimGetSprCount(uint anim_id) -> uint;
    auto AnimGetFrames(uint anim_id) -> AnyFrames*;
    void AnimRun(uint anim_id, uint flags);
    void AnimProcess();
    void AnimFree(AtlasType res_type);

    void CurDraw();
    void CurRMouseUp();
    void CurMMouseDown();

    auto IsCurInRect(const IRect& rect, int ax, int ay) const -> bool;
    auto IsCurInRect(const IRect& rect) const -> bool;
    auto IsCurInRectNoTransp(uint spr_id, const IRect& rect, int ax, int ay) const -> bool;
    auto IsCurInInterface() const -> bool;
    auto GetCurHex(ushort& hx, ushort& hy, bool ignore_interface) -> bool;

    void IntDraw();
    void IntLMouseDown();
    void IntLMouseUp();
    void IntMouseMove();
    void IntSetMode(int mode);

    auto GetTabIndex() -> uint;
    void SetTabIndex(uint index);
    void RefreshCurProtos();
    auto IsObjectMode() const -> bool { return CurItemProtos != nullptr && CurProtoScroll != nullptr; }
    auto IsTileMode() const -> bool { return CurTileHashes != nullptr && CurTileNames != nullptr && CurProtoScroll != nullptr; }
    auto IsCritMode() const -> bool { return CurNpcProtos != nullptr && CurProtoScroll != nullptr; }

    void MoveEntity(ClientEntity* entity, ushort hx, ushort hy);
    void DeleteEntity(ClientEntity* entity);
    void SelectClear();
    void SelectAddItem(ItemHexView* item);
    void SelectAddCrit(CritterView* npc);
    void SelectAddTile(ushort hx, ushort hy, bool is_roof);
    void SelectAdd(ClientEntity* entity);
    void SelectErase(ClientEntity* entity);
    void SelectAll();
    auto SelectMove(bool hex_move, int& offs_hx, int& offs_hy, int& offs_x, int& offs_y) -> bool;
    void SelectDelete();

    auto AddCritter(hash pid, ushort hx, ushort hy) -> CritterView*;
    auto AddItem(hash pid, ushort hx, ushort hy, Entity* owner) -> ItemView*;
    void AddTile(hash name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof);
    auto CloneEntity(Entity* entity) -> Entity*;

    void BufferCopy();
    void BufferCut();
    void BufferPaste(int hx, int hy);

    void ObjDraw();
    void DrawLine(string_view name, string_view type_name, string_view text, bool is_const, IRect& r);
    void ObjKeyDown(KeyCode dik, string_view dik_text);
    void ObjKeyDownApply(Entity* entity);
    void SelectEntityProp(int line);
    auto GetInspectorEntity() -> ClientEntity*;

    void ConsoleDraw();
    void ConsoleKeyDown(KeyCode dik, string_view dik_text);
    void ConsoleKeyUp(KeyCode dik);
    void ConsoleProcess();
    void ParseCommand(string_view command);

    void MessBoxGenerate();
    void AddMess(string_view message_text);
    void MessBoxDraw();
    auto SaveLogFile() -> bool;

    void RunStartScript();
    void RunMapLoadScript(MapView* map);
    void RunMapSaveScript(MapView* map);
    void DrawIfaceLayer(uint layer);

    void OnSetItemFlags(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemSomeLight(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemPicMap(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemOffsetXY(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value);

    MapperSettings& SettingsExt;
    FileManager ServerFileMngr;
    MapperScriptSystem ScriptSysExt;

    ConfigFile IfaceIni;
    string ServerWritePath {};
    string ClientWritePath {};
    PropertyVec ShowProps {};
    MapView* ClientCurMap {};
    LocationView* ClientCurLocation {};
    int DrawCrExtInfo {};
    LanguagePack CurLang {};
    vector<IfaceAnim*> Animations {};
    int CurMode {};
    AnyFrames* CurPDef {};
    AnyFrames* CurPHand {};
    int IntMode {};
    int IntHold {};
    AnyFrames* IntMainPic {};
    AnyFrames* IntPTab {};
    AnyFrames* IntPSelect {};
    AnyFrames* IntPShow {};
    int IntX {};
    int IntY {};
    int IntVectX {};
    int IntVectY {};
    ushort SelectHX1 {};
    ushort SelectHY1 {};
    ushort SelectHX2 {};
    ushort SelectHY2 {};
    int SelectX {};
    int SelectY {};
    int SelectType {};
    bool IntVisible {};
    bool IntFix {};
    IRect IntWMain {};
    IRect IntWWork {};
    IRect IntWHint {};
    IRect IntBCust[10] {};
    IRect IntBItem {};
    IRect IntBTile {};
    IRect IntBCrit {};
    IRect IntBFast {};
    IRect IntBIgnore {};
    IRect IntBInCont {};
    IRect IntBMess {};
    IRect IntBList {};
    IRect IntBScrBack {};
    IRect IntBScrBackFst {};
    IRect IntBScrFront {};
    IRect IntBScrFrontFst {};
    IRect IntBShowItem {};
    IRect IntBShowScen {};
    IRect IntBShowWall {};
    IRect IntBShowCrit {};
    IRect IntBShowTile {};
    IRect IntBShowRoof {};
    IRect IntBShowFast {};
    vector<MapView*> LoadedMaps {};
    MapView* ActiveMap {};
    map<string, SubTab> Tabs[TAB_COUNT] {};
    SubTab* TabsActive[TAB_COUNT] {};
    TileTab TabsTiles[TAB_COUNT] {};
    string TabsName[INT_MODE_COUNT] {};
    int TabsScroll[INT_MODE_COUNT] {};
    bool SubTabsActive {};
    int SubTabsActiveTab {};
    AnyFrames* SubTabsPic {};
    IRect SubTabsRect {};
    int SubTabsX {};
    int SubTabsY {};
    vector<const ProtoItem*>* CurItemProtos {};
    vector<hash>* CurTileHashes {};
    vector<string>* CurTileNames {};
    vector<const ProtoCritter*>* CurNpcProtos {};
    int NpcDir {};
    int* CurProtoScroll {};
    uint ProtoWidth {};
    uint ProtosOnScreen {};
    uint TabIndex[INT_MODE_COUNT] {};
    int InContScroll {};
    int ListScroll {};
    ItemView* InContItem {};
    bool DrawRoof {};
    int TileLayer {};
    IRect IntBSelectItem {};
    IRect IntBSelectScen {};
    IRect IntBSelectWall {};
    IRect IntBSelectCrit {};
    IRect IntBSelectTile {};
    IRect IntBSelectRoof {};
    bool IsSelectItem {};
    bool IsSelectScen {};
    bool IsSelectWall {};
    bool IsSelectCrit {};
    bool IsSelectTile {};
    bool IsSelectRoof {};
    vector<ClientEntity*> SelectedEntities {};
    vector<SelMapTile> SelectedTile {};
    vector<EntityBuf> EntitiesBuffer {};
    vector<TileBuf> TilesBuffer {};
    AnyFrames* ObjWMainPic {};
    AnyFrames* ObjPbToAllDn {};
    IRect ObjWMain {};
    IRect ObjWWork {};
    IRect ObjBToAll {};
    int ObjX {};
    int ObjY {};
    int ItemVectX {};
    int ItemVectY {};
    int ObjCurLine {};
    bool ObjCurLineIsConst {};
    string ObjCurLineInitValue {};
    string ObjCurLineValue {};
    bool ObjVisible {};
    bool ObjFix {};
    bool ObjToAll {};
    ClientEntity* InspectorEntity {};
    AnyFrames* ConsolePic {};
    int ConsolePicX {};
    int ConsolePicY {};
    int ConsoleTextX {};
    int ConsoleTextY {};
    bool ConsoleEdit {};
    string ConsoleStr {};
    uint ConsoleCur {};
    vector<string> ConsoleHistory {};
    int ConsoleHistoryCur {};
    KeyCode ConsoleLastKey {};
    string ConsoleLastKeyText {};
    uint ConsoleKeyTick {};
    int ConsoleAccelerate {};
    vector<MessBoxMessage> MessBox {};
    string MessBoxCurText {};
    int MessBoxScroll {};
    bool SpritesCanDraw {};
};
