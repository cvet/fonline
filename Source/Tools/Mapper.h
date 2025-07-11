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

#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "Client.h"
#include "ConfigFile.h"
#include "CritterHexView.h"
#include "CritterView.h"
#include "EffectManager.h"
#include "Entity.h"
#include "Geometry.h"
#include "ItemHexView.h"
#include "ItemView.h"
#include "Keyboard.h"
#include "LocationView.h"
#include "MapLoader.h"
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

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(MapperException);

class FOMapper final : public FOClient
{
    friend class MapperScriptSystem;

public:
    struct SubTab
    {
        vector<const ProtoItem*> ItemProtos {};
        vector<const ProtoCritter*> NpcProtos {};
        int32 Index {};
        int32 Scroll {};
    };

    struct EntityBuf
    {
        mpos Hex {};
        bool IsCritter {};
        bool IsItem {};
        const ProtoEntity* Proto {};
        Properties* Props {};
        vector<EntityBuf*> Children {};
    };

    struct MessBoxMessage
    {
        int32 Type {};
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
    auto IfaceLoadRect(irect32& rect, string_view name) const -> bool;
    auto GetIfaceSpr(hstring fname) -> Sprite*;
    void MapperMainLoop();
    void ProcessMapperInput();
    void DrawStr(const irect32& rect, string_view str, uint32 flags, ucolor color, int32 num_font);

    void CurDraw();
    void CurRMouseUp();
    void CurMMouseDown();

    auto IsCurInRect(const irect32& rect, int32 ax, int32 ay) const -> bool;
    auto IsCurInRect(const irect32& rect) const -> bool;
    auto IsCurInRectNoTransp(const Sprite* spr, const irect32& rect, int32 ax, int32 ay) const -> bool;
    auto IsCurInInterface() const -> bool;
    auto GetCurHex(mpos& hex, bool ignore_interface) -> bool;

    void IntDraw();
    void IntLMouseDown();
    void IntLMouseUp();
    void IntMouseMove();
    void IntSetMode(int32 mode);

    auto GetTabIndex() const -> int32;
    void SetTabIndex(int32 index);
    void RefreshCurProtos();
    auto IsItemMode() const -> bool { return CurItemProtos != nullptr && CurProtoScroll != nullptr; }
    auto IsCritMode() const -> bool { return CurNpcProtos != nullptr && CurProtoScroll != nullptr; }

    void MoveEntity(ClientEntity* entity, mpos hex);
    void DeleteEntity(ClientEntity* entity);
    void SelectClear();
    void SelectAddItem(ItemHexView* item);
    void SelectAddCrit(CritterView* npc);
    void SelectAddTile(mpos hex, bool is_roof);
    void SelectAdd(ClientEntity* entity);
    void SelectErase(ClientEntity* entity);
    void SelectAll();
    auto SelectMove(bool hex_move, int32& offs_hx, int32& offs_hy, int32& offs_x, int32& offs_y) -> bool;
    void SelectDelete();

    auto CreateCritter(hstring pid, mpos hex) -> CritterView*;
    auto CreateItem(hstring pid, mpos hex, Entity* owner) -> ItemView*;
    auto CloneEntity(Entity* entity) -> Entity*;
    void CloneInnerItems(ItemView* to_item, const ItemView* from_item);

    void BufferCopy();
    void BufferCut();
    void BufferPaste();

    void ObjDraw();
    void DrawLine(string_view name, string_view type_name, string_view text, bool is_const, irect32& r);
    void ObjKeyDown(KeyCode dik, string_view dik_text);
    void ObjKeyDownApply(Entity* entity);
    void SelectEntityProp(int32 line);
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
    void ResizeMap(MapView* map, int32 width, int32 height);

    void AddMess(string_view message_text);
    void MessBoxDraw();

    void DrawIfaceLayer(int32 layer);

    auto GetEntityInnerItems(ClientEntity* entity) const -> vector<refcount_ptr<ItemView>>;

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapperMessage, string& /*text*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnEditMapLoad, MapView* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnEditMapSave, MapView* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnInspectorProperties, Entity* /*entity*/, vector<int32>& /*properties*/);

    FileSystem MapsFileSys {};
    vector<refcount_ptr<MapView>> LoadedMaps {};
    unique_ptr<ConfigFile> IfaceIni {};
    vector<const Property*> ShowProps {};
    bool PressedKeys[0x100] {};
    unordered_map<hstring, shared_ptr<Sprite>> IfaceSpr {};
    int32 CurMode {};
    shared_ptr<Sprite> CurPDef {};
    shared_ptr<Sprite> CurPHand {};
    int32 IntMode {};
    int32 IntHold {};
    shared_ptr<Sprite> IntMainPic {};
    shared_ptr<Sprite> IntPTab {};
    shared_ptr<Sprite> IntPSelect {};
    shared_ptr<Sprite> IntPShow {};
    int32 IntX {};
    int32 IntY {};
    int32 IntVectX {};
    int32 IntVectY {};
    mpos SelectHex1 {};
    mpos SelectHex2 {};
    ipos32 SelectPos {};
    int32 SelectType {};
    bool IntVisible {};
    bool IntFix {};
    irect32 IntWMain {};
    irect32 IntWWork {};
    irect32 IntWHint {};
    irect32 IntBCust[10] {};
    irect32 IntBItem {};
    irect32 IntBTile {};
    irect32 IntBCrit {};
    irect32 IntBFast {};
    irect32 IntBIgnore {};
    irect32 IntBInCont {};
    irect32 IntBMess {};
    irect32 IntBList {};
    irect32 IntBScrBack {};
    irect32 IntBScrBackFst {};
    irect32 IntBScrFront {};
    irect32 IntBScrFrontFst {};
    irect32 IntBShowItem {};
    irect32 IntBShowScen {};
    irect32 IntBShowWall {};
    irect32 IntBShowCrit {};
    irect32 IntBShowTile {};
    irect32 IntBShowRoof {};
    irect32 IntBShowFast {};
    map<string, SubTab> Tabs[TAB_COUNT] {};
    SubTab* TabsActive[TAB_COUNT] {};
    string TabsName[INT_MODE_COUNT] {};
    int32 TabsScroll[INT_MODE_COUNT] {};
    bool SubTabsActive {};
    int32 SubTabsActiveTab {};
    shared_ptr<Sprite> SubTabsPic {};
    irect32 SubTabsRect {};
    int32 SubTabsX {};
    int32 SubTabsY {};
    vector<const ProtoItem*>* CurItemProtos {};
    vector<const ProtoCritter*>* CurNpcProtos {};
    uint8 NpcDir {};
    int32* CurProtoScroll {};
    int32 ProtoWidth {};
    int32 ProtosOnScreen {};
    int32 TabIndex[INT_MODE_COUNT] {};
    int32 InContScroll {};
    int32 ListScroll {};
    refcount_ptr<ItemView> InContItem {};
    bool DrawRoof {};
    int32 TileLayer {};
    irect32 IntBSelectItem {};
    irect32 IntBSelectScen {};
    irect32 IntBSelectWall {};
    irect32 IntBSelectCrit {};
    irect32 IntBSelectTile {};
    irect32 IntBSelectRoof {};
    bool IsSelectItem {};
    bool IsSelectScen {};
    bool IsSelectWall {};
    bool IsSelectCrit {};
    bool IsSelectTile {};
    bool IsSelectRoof {};
    ipos32 BufferRawHex {};
    vector<ClientEntity*> SelectedEntities {};
    vector<EntityBuf> EntitiesBuffer {};
    shared_ptr<Sprite> ObjWMainPic {};
    shared_ptr<Sprite> ObjPbToAllDn {};
    irect32 ObjWMain {};
    irect32 ObjWWork {};
    irect32 ObjBToAll {};
    int32 ObjX {};
    int32 ObjY {};
    int32 ItemVectX {};
    int32 ItemVectY {};
    int32 ObjCurLine {};
    bool ObjCurLineIsConst {};
    string ObjCurLineInitValue {};
    string ObjCurLineValue {};
    bool ObjVisible {};
    bool ObjFix {};
    bool ObjToAll {};
    ClientEntity* InspectorEntity {};
    shared_ptr<Sprite> ConsolePic {};
    int32 ConsolePicX {};
    int32 ConsolePicY {};
    int32 ConsoleTextX {};
    int32 ConsoleTextY {};
    bool ConsoleEdit {};
    string ConsoleStr {};
    int32 ConsoleCur {};
    vector<string> ConsoleHistory {};
    int32 ConsoleHistoryCur {};
    KeyCode ConsoleLastKey {};
    string ConsoleLastKeyText {};
    nanotime ConsoleKeyTime {};
    int32 ConsoleAccelerate {};
    vector<MessBoxMessage> MessBox {};
    string MessBoxCurText {};
    int32 MessBoxScroll {};
    bool SpritesCanDraw {};
    uint8 SelectAlpha {100};
};

FO_END_NAMESPACE();
