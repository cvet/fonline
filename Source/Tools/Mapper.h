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

#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "Client.h"
#include "CritterHexView.h"
#include "CritterView.h"
#include "EffectManager.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "ItemHexView.h"
#include "ItemView.h"
#include "Keyboard.h"
#include "LocationView.h"
#include "MapLoader.h"
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
#include "Timer.h"

DECLARE_EXCEPTION(MapperException);

class FOMapper final : virtual public FOEngineBase, public FOClient
{
    friend class MapperScriptSystem;

public:
    struct SubTab
    {
        vector<const ProtoItem*> ItemProtos {};
        vector<const ProtoCritter*> NpcProtos {};
        vector<hstring> TileNames {};
        uint Index {};
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
        hstring Name {};
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
    static constexpr auto FONT_THIN = 6;
    static constexpr auto FONT_FAT = 7;
    static constexpr auto FONT_BIG = 8;
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

    FOMapper(GlobalSettings& settings, AppWindow* window);

    FOMapper(const FOMapper&) = delete;
    FOMapper(FOMapper&&) noexcept = delete;
    auto operator=(const FOMapper&) = delete;
    auto operator=(FOMapper&&) noexcept = delete;
    ~FOMapper() override = default;

    void InitIface();
    auto IfaceLoadRect(IRect& comp, string_view name) const -> bool;
    void MapperMainLoop();
    void RefreshTiles(int tab);
    auto GetProtoItemCurSprId(const ProtoItem* proto_item) -> uint;
    void ChangeGameTime();
    void ProcessMapperInput();

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

    auto GetTabIndex() const -> uint;
    void SetTabIndex(uint index);
    void RefreshCurProtos();
    auto IsObjectMode() const -> bool { return CurItemProtos != nullptr && CurProtoScroll != nullptr; }
    auto IsTileMode() const -> bool { return CurTileNames != nullptr && CurProtoScroll != nullptr; }
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

    auto AddCritter(hstring pid, ushort hx, ushort hy) -> CritterView*;
    auto AddItem(hstring pid, ushort hx, ushort hy, Entity* owner) -> ItemView*;
    void AddTile(hstring name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof);
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
    auto LoadMap(string_view map_name) -> MapView*;
    void ShowMap(MapView* map);
    void SaveMap(MapView* map, string_view custom_name);
    void UnloadMap(MapView* map);
    void ResizeMap(MapView* map, ushort width, ushort height);

    void AddMess(string_view message_text);
    void MessBoxDraw();

    void DrawIfaceLayer(uint layer);

    auto GetEntityInnerItems(ClientEntity* entity) -> vector<ItemView*>;

    ///@ ExportEvent
    ENTITY_EVENT(OnConsoleMessage, string& /*text*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnEditMapLoad, MapView* /*map*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnEditMapSave, MapView* /*map*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnInspectorProperties, Entity* /*entity*/, vector<int>& /*properties*/);

    FileSystem ContentFileSys {};
    vector<MapView*> LoadedMaps {};
    unique_ptr<ConfigFile> IfaceIni {};
    vector<const Property*> ShowProps {};
    bool PressedKeys[0x100] {};
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
    ushort SelectHexX1 {};
    ushort SelectHexY1 {};
    ushort SelectHexX2 {};
    ushort SelectHexY2 {};
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
    vector<hstring>* CurTileNames {};
    vector<const ProtoCritter*>* CurNpcProtos {};
    uchar NpcDir {};
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
