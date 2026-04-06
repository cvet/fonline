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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Geometry.h"
#include "ImGuiStuff.h"
#include "ItemHexView.h"
#include "ItemView.h"
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

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(MapperException);

class MapperEngine final : public ClientEngine
{
    friend class MapperScriptSystem;

public:
    struct SubTab
    {
        vector<raw_ptr<const ProtoItem>> ItemProtos {};
        vector<raw_ptr<const ProtoCritter>> CritterProtos {};
        int32 Index {};
        int32 Scroll {};
    };

    struct EntityBuf
    {
        ident_t Id {};
        mpos Hex {};
        int16 DirAngle {};
        bool IsCritter {};
        bool IsItem {};
        CritterItemSlot Slot {CritterItemSlot::Inventory};
        any_t StackId {};
        raw_ptr<const ProtoEntity> Proto {};
        unique_ptr<Properties> Props {};
        vector<unique_ptr<EntityBuf>> Children {};
        EntityBuf() = default;
        EntityBuf(const EntityBuf& other);
        auto operator=(const EntityBuf& other) -> EntityBuf&;
        EntityBuf(EntityBuf&&) noexcept = default;
        auto operator=(EntityBuf&&) noexcept -> EntityBuf& = default;
    };

    struct UndoOp
    {
        string Label {};
        bool IsSnapshot {};
        std::function<bool(MapperEngine&, raw_ptr<MapView>&)> Undo {};
        std::function<bool(MapperEngine&, raw_ptr<MapView>&)> Redo {};

        UndoOp() = default;
        UndoOp(string label, std::function<bool(MapperEngine&, raw_ptr<MapView>&)> undo, std::function<bool(MapperEngine&, raw_ptr<MapView>&)> redo, bool is_snapshot = false);
    };

    struct UndoContext
    {
        vector<UndoOp> UndoStack {};
        vector<UndoOp> RedoStack {};
        int32 CleanUndoDepth {-1};
    };

    struct MoveCommandEntry
    {
        ident_t EntityId {};
        bool HasHex {};
        mpos OldHex {};
        mpos NewHex {};
        bool HasOffset {};
        ipos16 OldOffset {};
        ipos16 NewOffset {};
        vector<mpos> OldMultihexMesh {};
        vector<mpos> NewMultihexMesh {};
    };

    struct MessBoxMessage
    {
        int32 Type {};
        string Mess {};
        string Time {};
    };

    // Todo: move mapper constants to enums
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
    static constexpr auto INT_SELECT = 3;
    static constexpr auto INT_MOVE_SELECTION = 4;
    static constexpr auto INT_PAN = 5;
    static constexpr auto DEFAULT_SUB_TAB = "000 - all";
    static constexpr auto DRAW_NEXT_HEIGHT = 12;
    static constexpr auto CONSOLE_KEY_TICK = 500;
    static constexpr auto CONSOLE_MAX_ACCELERATE = 460;

    explicit MapperEngine(GlobalSettings& settings, FileSystem&& resources, AppWindow& window);

    MapperEngine(const MapperEngine&) = delete;
    MapperEngine(MapperEngine&&) noexcept = delete;
    auto operator=(const MapperEngine&) = delete;
    auto operator=(MapperEngine&&) noexcept = delete;
    ~MapperEngine() override = default;

    void InitIface();
    auto GetPreviewSprite(hstring fname) -> Sprite*;
    void MapperMainLoop();
    auto BeginMapperFrameInput() -> bool;
    void ProcessMapperInputEvent(const InputEvent& ev);
    void DrawMapperFrame();
    void HandleMapperKeyboardEvent(const InputEvent& ev);
    void HandlePrimaryMapperHotkeys(KeyCode dikdw, bool block_hotkeys);
    void HandleShiftMapperHotkeys(KeyCode dikdw, bool block_hotkeys);
    void HandleCtrlMapperHotkeys(KeyCode dikdw, bool block_hotkeys);
    void UpdateArrowScrollKeys(KeyCode dikdw, KeyCode dikup);
    void HandleMapperConsoleKeyDown(KeyCode dikdw, string_view key_text);
    void ChangeZoom(float32 new_zoom);
    void DrawStr(const irect32& rect, string_view str, uint32 flags, ucolor color, int32 num_font);

    void CurDraw();
    void CurRMouseUp();
    void CurMMouseDown();
    void SetCurMode(int cur_mode);
    void ProcessRightMouseInertia();
    void CommitPendingSelectionMove();
    void ResetPendingSelectionMove();

    auto IsCurInRect(const irect32& rect, int32 ax, int32 ay) const -> bool;
    auto IsCurInRect(const irect32& rect) const -> bool;
    auto IsCurInInterface() const -> bool;
    auto GetCurHex(mpos& hex, bool ignore_interface) -> bool;

    void DrawMainPanelImGui();
    void DrawWorkspaceWindowImGui();
    void DrawContentWindowImGui();
    void DrawCritterAnimationsWindowImGui();
    void DrawScriptCallWindowImGui();
    void DrawMapListWindowImGui();
    void DrawMapWindowImGui();
    void DrawHistoryWindowImGui();
    void DrawSettingsWindowImGui();
    void HandleLeftMouseDown();
    auto HandleMapLeftMouseDown() -> bool;
    void HandleLeftMouseUp();
    void HandleSelectionMouseDrag();
    void SetActivePanelMode(int32 mode);

    auto GetActiveSubTab() -> SubTab*;
    auto GetActiveSubTab() const -> const SubTab*;
    auto GetActiveProtoIndex() const -> int32;
    void SetActiveProtoIndex(int32 index);
    void RefreshActiveProtoLists();
    auto IsItemMode() const -> bool { return ActiveItemProtos != nullptr && ActiveProtoScroll != nullptr; }
    auto IsCritMode() const -> bool { return ActiveCritterProtos != nullptr && ActiveProtoScroll != nullptr; }

    void MoveEntity(ClientEntity* entity, mpos hex);
    void DeleteEntity(ClientEntity* entity);
    void SelectAdd(ClientEntity* entity, optional<mpos> hex = std::nullopt, bool skip_refresh = false);
    void SelectAll();
    void SelectRemove(ClientEntity* entity, bool skip_refresh = false);
    void SelectClear();
    auto SelectMove(bool hex_move, int32& offs_hx, int32& offs_hy, int32& offs_x, int32& offs_y) -> bool;
    void SelectDelete();

    auto CreateCritter(hstring pid, mpos hex) -> CritterView*;
    auto CreateItem(hstring pid, mpos hex, Entity* owner) -> ItemView*;
    auto CloneEntity(Entity* entity) -> Entity*;
    void CloneInnerItems(ItemView* to_item, const ItemView* from_item);

    auto MergeItemsToMultihexMeshes(MapView* map) -> size_t;
    auto TryMergeItemToMultihexMesh(MapView* map, ItemHexView* item, bool skip_selected) -> ItemHexView*;
    void MergeItemToMultihexMesh(MapView* map, ItemHexView* source_item, ItemHexView* target_item);
    void FindMultihexMeshForItemAroundHex(MapView* map, ItemHexView* item, mpos hex, bool merge_to_it, unordered_set<ItemHexView*>& result) const;
    auto CompareMultihexItemForMerge(const ItemHexView* source_item, const ItemHexView* target_item, bool allow_clean_merge) const -> bool;
    auto BreakItemsMultihexMeshes(MapView* map) -> size_t;
    auto TryBreakItemFromMultihexMesh(MapView* map, ItemHexView* item, mpos hex) -> ItemHexView*;

    void BufferCopy();
    void BufferCut();
    void BufferPaste();

    void DrawInspectorImGui();
    auto ApplyEntityPropertyText(Entity* entity, const Property* prop, string_view value_text) -> bool;
    void ApplyInspectorPropertyEdit(Entity* entity);
    void ResetInspectorPropertyEditState();
    auto CancelInspectorPropertyEdit() -> bool;
    void SelectInspectorPropertyLine(int32 line);
    auto GetInspectorEntity() -> ClientEntity*;

    void DrawConsoleImGui();
    void ConsoleSubmitCommand();
    void ResetImGuiSettings();
    auto IsImGuiMouseCaptured() const -> bool;
    auto IsImGuiTextInputActive() const -> bool;
    auto CanUndo() const -> bool;
    auto CanRedo() const -> bool;
    auto GetUndoLabel() const -> string;
    auto GetRedoLabel() const -> string;
    auto ExecuteUndo() -> bool;
    auto ExecuteRedo() -> bool;
    auto JumpHistoryToIndex(int32 target_index) -> bool;
    void ParseCommand(string_view command);
    auto LoadMap(string_view map_name) -> MapView*;
    auto LoadMapFromText(string_view map_name, const string& map_text) -> MapView*;
    void ShowMap(MapView* map);
    auto IsMapDirty(const MapView* map) const -> bool;
    void SetMapDirty(MapView* map, bool dirty = true);
    void SaveCurrentMap();
    void ResetCurrentMapChanges();
    void SaveMap(MapView* map, string_view custom_name);
    void UnloadMap(MapView* map, bool clear_undo = true);
    void ResizeMap(MapView* map, int32 width, int32 height);

    void AddMess(string_view message_text);
    void MessBoxDraw();

    void DrawIfaceLayer(int32 layer);

    auto GetEntityInnerItems(ClientEntity* entity) const -> vector<refcount_ptr<ItemView>>;
    void CaptureEntityBuf(EntityBuf& entity_buf, ClientEntity* entity) const;
    auto RestoreEntityBuf(const EntityBuf& entity_buf, Entity* owner = nullptr) -> ClientEntity*;
    void RestoreEntityBufChildren(const EntityBuf& entity_buf, ItemView* item);
    auto FindEntityById(raw_ptr<MapView> map, ident_t id) -> ClientEntity*;
    auto GetUndoContext(MapView* map, bool create) -> UndoContext*;
    auto GetUndoContext(const MapView* map, bool create) const -> const UndoContext*;
    void ClearUndoContext(MapView* map);
    void RemapUndoContext(MapView* old_map, MapView* new_map);
    void PushUndoOp(MapView* map, UndoOp op);
    auto CaptureMapSnapshot(MapView* map) const -> string;
    auto RestoreMapSnapshot(raw_ptr<MapView>& map, string_view map_name, const string& map_text) -> bool;

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
    unordered_set<raw_ptr<MapView>> DirtyMaps {};
    unordered_map<raw_ptr<MapView>, UndoContext> UndoContexts {};
    vector<raw_ptr<const Property>> ShowProps {};
    bool PressedKeys[0x100] {};
    unordered_map<hstring, shared_ptr<Sprite>> PreviewSprites {};
    int32 CurMode {};
    shared_ptr<Sprite> CurPDef {};
    shared_ptr<Sprite> CurPHand {};
    int32 ActivePanelMode {};
    int32 MouseHoldMode {};
    ipos32 MainPanelPos {};
    mpos SelectHex1 {};
    mpos SelectHex2 {};
    ipos32 SelectPos {};
    bool RightMouseDragged {};
    fpos32 RightMouseInertia {};
    fpos32 RightMouseVelocityAccum {};
    nanotime RightMouseVelocityTime {};
    vector<MoveCommandEntry> PendingSelectionMoveEntries {};
    bool SelectAxialGrid {true};
    bool SelectEntireEntity {};
    bool WorkspaceWindowVisible {};
    bool ContentWindowVisible {};
    bool CritterAnimationsWindowVisible {};
    bool ScriptCallWindowVisible {};
    bool MapListWindowVisible {};
    bool MapWindowVisible {};
    bool HistoryWindowVisible {};
    bool SettingsWindowVisible {};
    bool ResetImGuiSettingsRequested {};
    irect32 MainPanelWindowRect {};
    irect32 MainPanelContentRect {};
    map<string, SubTab> Tabs[TAB_COUNT] {};
    raw_ptr<SubTab> ActiveSubTabs[TAB_COUNT] {};
    string PanelModeNames[INT_MODE_COUNT] {};
    raw_ptr<vector<raw_ptr<const ProtoItem>>> ActiveItemProtos {};
    raw_ptr<vector<raw_ptr<const ProtoCritter>>> ActiveCritterProtos {};
    uint8 CritterDir {};
    raw_ptr<int32> ActiveProtoScroll {};
    int32 ProtoWidth {};
    int32 ProtosOnScreen {};
    array<char, 128> WorkspaceFilterBuf {};
    array<char, 256> ContentMapNameBuf {};
    array<char, 128> ContentMapFilterBuf {};
    int32 ContentResizeW {};
    int32 ContentResizeH {};
    int32 CritterAnimState {};
    int32 CritterAnimAction {};
    array<char, 128> CritterAnimSequenceBuf {};
    array<char, 128> ScriptCallFuncBuf {};
    array<char, 256> ScriptCallArgsBuf {};
    array<char, 128> MapBrowserFilterBuf {};
    int32 TabIndex[INT_MODE_COUNT] {};
    refcount_ptr<ItemView> InContItem {};
    bool PreviewRoofTiles {};
    int32 TileLayer {};
    bool SelectItemsEnabled {};
    bool SelectSceneryEnabled {};
    bool SelectWallsEnabled {};
    bool SelectCrittersEnabled {};
    bool SelectTilesEnabled {};
    bool SelectRoofTilesEnabled {};
    ipos32 BufferRawHex {};
    vector<refcount_ptr<ClientEntity>> SelectedEntities {};
    unordered_set<raw_ptr<ClientEntity>> SelectedEntitiesSet {};
    vector<EntityBuf> EntitiesBuffer {};
    ipos32 InspectorPos {24, 24};
    int32 InspectorSelectedLine {};
    string InspectorSelectedLineInitialValue {};
    string InspectorSelectedLineValue {};
    string InspectorEditBuf {};
    int32 InspectorEditLine {-1};
    int32 InspectorPendingFocusLine {-1};
    int32 InspectorPendingFocusArrayIndex {-1};
    int32 InspectorPendingCaretResetLine {-1};
    int32 InspectorPendingCaretResetArrayIndex {-1};
    int32 InspectorPendingCaretResetFrames {};
    bool InspectorLastEditCellRectValid {};
    float InspectorLastEditCellMinX {};
    float InspectorLastEditCellMinY {};
    float InspectorLastEditCellMaxX {};
    float InspectorLastEditCellMaxY {};
    bool InspectorVisible {};
    bool InspectorApplyToAll {};
    raw_ptr<ClientEntity> InspectorEntity {};
    float SelectionSmallOffsetX {};
    float SelectionSmallOffsetY {};
    bool ConsoleEdit {};
    string ConsoleStr {};
    array<char, 4096> ConsoleBuf {};
    bool ConsoleSyncFromState {true};
    vector<string> ConsoleHistory {};
    int32 ConsoleHistoryCur {};
    vector<MessBoxMessage> MessBox {};
    string MessBoxCurText {};
    int32 MessBoxScroll {};
    raw_ptr<MapView> LastHistoryMap {};
    int32 LastHistoryUndoCount {-1};
    bool UndoRedoInProgress {};
    size_t MaxUndoDepth {100};
    bool SpritesCanDraw {};
    uint8 SelectAlpha {100};
};

FO_END_NAMESPACE
