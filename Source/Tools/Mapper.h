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
        vector<ptr<const ProtoItem>> ItemProtos {};
        vector<ptr<const ProtoCritter>> CritterProtos {};
        int32_t Index {};
        int32_t Scroll {};
    };

    struct EntityBuf
    {
        ident_t Id {};
        mpos Hex {};
        mdir Dir {};
        bool IsCritter {};
        bool IsItem {};
        CritterItemSlot Slot {CritterItemSlot::Inventory};
        any_t StackId {};
        nptr<const ProtoEntity> Proto {};
        optional<Properties> Props {};
        vector<unique_ptr<EntityBuf>> Children {};
        EntityBuf() = default;
        EntityBuf(const EntityBuf& other);
        auto operator=(const EntityBuf& other) -> EntityBuf&;
        EntityBuf(EntityBuf&&) noexcept = default;
        auto operator=(EntityBuf&&) noexcept -> EntityBuf& = default;
        [[nodiscard]] auto GetProps() const noexcept -> nptr<const Properties> { return Props ? nptr<const Properties> {&*Props} : nullptr; }
    };

    struct UndoOp
    {
        string Label {};
        bool IsSnapshot {};
        std::function<bool(ptr<MapperEngine>, ptr<ptr<MapView>>)> Undo {};
        std::function<bool(ptr<MapperEngine>, ptr<ptr<MapView>>)> Redo {};

        UndoOp() = default;
        UndoOp(string label, std::function<bool(ptr<MapperEngine>, ptr<ptr<MapView>>)> undo, std::function<bool(ptr<MapperEngine>, ptr<ptr<MapView>>)> redo, bool is_snapshot = false);
    };

    struct UndoContext
    {
        vector<UndoOp> UndoStack {};
        vector<UndoOp> RedoStack {};
        int32_t CleanUndoDepth {-1};
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
        int32_t Type {};
        string Mess {};
        string Time {};
    };

    static constexpr auto FONT_FO = static_cast<FontType>(0);
    static constexpr auto FONT_NUM = static_cast<FontType>(1);
    static constexpr auto FONT_BIG_NUM = static_cast<FontType>(2);
    static constexpr auto FONT_SAND_NUM = static_cast<FontType>(3);
    static constexpr auto FONT_SPECIAL = static_cast<FontType>(4);
    static constexpr auto FONT_OLD_DEFAULT = static_cast<FontType>(5);
    static constexpr auto FONT_THIN = static_cast<FontType>(6);
    static constexpr auto FONT_FAT = static_cast<FontType>(7);
    static constexpr auto FONT_BIG = static_cast<FontType>(8);
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

    explicit MapperEngine(ptr<GlobalSettings> settings, FileSystem&& resources, ptr<IAppWindow> window);

    MapperEngine(const MapperEngine&) = delete;
    MapperEngine(MapperEngine&&) noexcept = delete;
    auto operator=(const MapperEngine&) = delete;
    auto operator=(MapperEngine&&) noexcept = delete;
    ~MapperEngine() override = default;

    void Shutdown() override;

    void InitIface();
    auto GetPreviewSprite(hstring fname) -> nptr<Sprite>;
    void SetInputLocked(bool locked) noexcept;
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
    void ChangeZoom(float32_t new_zoom);
    void DrawStr(const irect32& rect, string_view str, ucolor color, TextFormat format);

    void CurDraw();
    void CurRMouseUp();
    void CurMMouseDown();
    void SetCurMode(int cur_mode);
    void ProcessRightMouseInertia();
    void CommitPendingSelectionMove();
    void ResetPendingSelectionMove();

    auto IsCurInRect(const irect32& rect, int32_t ax, int32_t ay) const -> bool;
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
    void SetActivePanelMode(int32_t mode);

    void SetMapperHexOverlayVisible(bool visible);
    void ToggleMapperHexOverlay();
    auto IsMapperHexOverlayVisible() const noexcept -> bool { return MapperHexOverlayVisible; }
    void ClearMapperTrackOverlay();
    void AddMapperTrackOverlayHex(mpos hex, int32_t kind);
    auto GetMapperTrackOverlayHexes() const noexcept -> const vector<mpos>& { return MapperTrackOverlayHexes; }
    auto GetMapperTrackOverlayKinds() const noexcept -> const vector<int32_t>& { return MapperTrackOverlayKinds; }
    void MarkBlockedHexes();

    auto GetActiveSubTab() -> nptr<SubTab>;
    auto GetActiveSubTab() const -> nptr<const SubTab>;
    auto GetActiveProtoIndex() const -> int32_t;
    void SetActiveProtoIndex(int32_t index);
    void RefreshActiveProtoLists();
    auto IsItemMode() const -> bool { return ActiveItemProtos && ActiveProtoScroll; }
    auto IsCritMode() const -> bool { return ActiveCritterProtos && ActiveProtoScroll; }

    void MoveEntity(ptr<ClientEntity> entity, mpos hex);
    void DeleteEntity(ptr<ClientEntity> entity);
    void SelectAdd(ptr<ClientEntity> entity, optional<mpos> hex = std::nullopt, bool skip_refresh = false);
    void SelectAll();
    void SelectRemove(ptr<ClientEntity> entity, bool skip_refresh = false);
    void SelectClear();
    auto SelectMove(bool hex_move, int32_t& offs_hx, int32_t& offs_hy, int32_t& offs_x, int32_t& offs_y) -> bool;
    void SelectDelete();

    auto CreateCritter(hstring pid, mpos hex) -> ptr<CritterView>;
    auto CreateItem(hstring pid, mpos hex, nptr<Entity> owner) -> ptr<ItemView>;
    auto CloneEntity(ptr<Entity> entity) -> nptr<Entity>;
    void CloneInnerItems(ptr<MapView> map, ptr<ItemView> to_item, ptr<const ItemView> from_item);

    auto MergeItemsToMultihexMeshes(ptr<MapView> map) -> size_t;
    auto TryMergeItemToMultihexMesh(ptr<MapView> map, ptr<ItemHexView> item, bool skip_selected) -> nptr<ItemHexView>;
    void MergeItemToMultihexMesh(ptr<MapView> map, ptr<ItemHexView> source_item, ptr<ItemHexView> target_item);
    void FindMultihexMeshForItemAroundHex(ptr<MapView> map, ptr<ItemHexView> item, mpos hex, bool merge_to_it, unordered_set<ptr<ItemHexView>>& result) const;
    auto CompareMultihexItemForMerge(ptr<const ItemHexView> source_item, ptr<const ItemHexView> target_item, bool allow_clean_merge) const -> bool;
    auto BreakItemsMultihexMeshes(ptr<MapView> map) -> size_t;
    auto TryBreakItemFromMultihexMesh(ptr<MapView> map, ptr<ItemHexView> item, mpos hex) -> nptr<ItemHexView>;

    void BufferCopy();
    void BufferCut();
    void BufferPaste();

    void DrawInspectorImGui();
    auto ApplyEntityPropertyText(ptr<Entity> entity, ptr<const Property> prop, string_view value_text) -> bool;
    void ApplyInspectorPropertyEdit(ptr<Entity> entity);
    void ResetInspectorPropertyEditState();
    auto CancelInspectorPropertyEdit() -> bool;
    void SelectInspectorPropertyLine(int32_t line);
    auto GetInspectorEntity() -> nptr<ClientEntity>;

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
    auto JumpHistoryToIndex(int32_t target_index) -> bool;
    void ParseCommand(string_view command);
    auto LoadMap(string_view map_name) -> nptr<MapView>;
    auto LoadMapFromText(string_view map_name, const string& map_text) -> nptr<MapView>;
    void ShowMap(ptr<MapView> map);
    auto IsMapDirty(nptr<MapView> nullable_map) const -> bool;
    void SetMapDirty(nptr<MapView> nullable_map, bool dirty = true);
    void SaveCurrentMap();
    void ResetCurrentMapChanges();
    void SaveMap(ptr<MapView> map, string_view custom_name);
    void SaveMapToDir(ptr<MapView> map, string_view sub_dir, string_view name);
    void UnloadMap(ptr<MapView> map, bool clear_undo = true);
    void ResizeMap(ptr<MapView> map, int32_t width, int32_t height);

    void AddMess(string_view message_text);
    void MessBoxDraw();

    auto GetEntityInnerItems(ptr<ClientEntity> entity) const -> vector<refcount_ptr<ItemView>>;
    void CaptureEntityBuf(EntityBuf& entity_buf, ptr<ClientEntity> entity) const;
    auto RestoreEntityBuf(const EntityBuf& entity_buf, nptr<Entity> owner = nullptr) -> nptr<ClientEntity>;
    void RestoreEntityBufChildren(const EntityBuf& entity_buf, ptr<ItemView> item);
    auto FindEntityById(ptr<MapView> map, ident_t id) -> nptr<ClientEntity>;
    auto GetUndoContext(nptr<MapView> nullable_map, bool create) -> nptr<UndoContext>;
    auto GetUndoContext(nptr<const MapView> nullable_map, bool create) const -> nptr<const UndoContext>;
    void ClearUndoContext(nptr<MapView> nullable_map);
    void RemapUndoContext(nptr<MapView> nullable_old_map, nptr<MapView> nullable_new_map);
    void PushUndoOp(nptr<MapView> map, UndoOp op);
    auto CaptureMapSnapshot(nptr<const MapView> nullable_map) const -> string;
    auto RestoreMapSnapshot(ptr<ptr<MapView>> map, string_view map_name, const string& map_text) -> bool;

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapperMessage, string& /*text*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnEditMapLoad, MapView* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnEditMapSave, MapView* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnInspectorProperties, Entity* /*entity*/, vector<int32_t>& /*properties*/);

    FileSystem MapsFileSys {};
    vector<refcount_ptr<MapView>> LoadedMaps {};
    unordered_set<ptr<MapView>> DirtyMaps {};
    unordered_map<ptr<MapView>, UndoContext> UndoContexts {};
    vector<nptr<const Property>> ShowProps {};
    bool PressedKeys[0x100] {};
    unordered_map<hstring, shared_ptr<Sprite>> PreviewSprites {};
    int32_t CurMode {};
    shared_ptr<Sprite> CurPDef {};
    shared_ptr<Sprite> CurPHand {};
    int32_t ActivePanelMode {};
    int32_t MouseHoldMode {};
    ipos32 MainPanelPos {};
    mpos SelectHex1 {};
    mpos SelectHex2 {};
    ipos32 SelectPos {};
    vector<mpos> MapperTrackOverlayHexes {};
    vector<int32_t> MapperTrackOverlayKinds {};
    bool MapperHexOverlayVisible {};
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
    bool InputLocked {};
    irect32 MainPanelWindowRect {};
    irect32 MainPanelContentRect {};
    map<string, SubTab> Tabs[TAB_COUNT] {};
    nptr<SubTab> ActiveSubTabs[TAB_COUNT] {};
    string PanelModeNames[INT_MODE_COUNT] {};
    nptr<vector<ptr<const ProtoItem>>> ActiveItemProtos {};
    nptr<vector<ptr<const ProtoCritter>>> ActiveCritterProtos {};
    mdir CritterDir {};
    nptr<int32_t> ActiveProtoScroll {};
    int32_t ProtoWidth {};
    int32_t ProtosOnScreen {};
    array<char, 128> WorkspaceFilterBuf {};
    array<char, 256> ContentMapNameBuf {};
    array<char, 128> ContentMapFilterBuf {};
    int32_t ContentResizeW {};
    int32_t ContentResizeH {};
    int32_t CritterAnimState {};
    int32_t CritterAnimAction {};
    array<char, 128> CritterAnimSequenceBuf {};
    array<char, 128> ScriptCallFuncBuf {};
    array<char, 256> ScriptCallArgsBuf {};
    array<char, 128> MapBrowserFilterBuf {};
    int32_t TabIndex[INT_MODE_COUNT] {};
    refcount_nptr<ItemView> InContItem {};
    bool PreviewRoofTiles {};
    int32_t TileLayer {};
    bool SelectItemsEnabled {};
    bool SelectSceneryEnabled {};
    bool SelectWallsEnabled {};
    bool SelectCrittersEnabled {};
    bool SelectTilesEnabled {};
    bool SelectRoofTilesEnabled {};
    ipos32 BufferRawHex {};
    vector<refcount_ptr<ClientEntity>> SelectedEntities {};
    unordered_set<ptr<ClientEntity>> SelectedEntitiesSet {};
    vector<EntityBuf> EntitiesBuffer {};
    ipos32 InspectorPos {24, 24};
    int32_t InspectorSelectedLine {};
    string InspectorSelectedLineInitialValue {};
    string InspectorSelectedLineValue {};
    string InspectorEditBuf {};
    int32_t InspectorEditLine {-1};
    int32_t InspectorPendingFocusLine {-1};
    int32_t InspectorPendingFocusArrayIndex {-1};
    int32_t InspectorPendingCaretResetLine {-1};
    int32_t InspectorPendingCaretResetArrayIndex {-1};
    int32_t InspectorPendingCaretResetFrames {};
    bool InspectorLastEditCellRectValid {};
    float InspectorLastEditCellMinX {};
    float InspectorLastEditCellMinY {};
    float InspectorLastEditCellMaxX {};
    float InspectorLastEditCellMaxY {};
    bool InspectorVisible {};
    bool InspectorApplyToAll {};
    nptr<ClientEntity> InspectorEntity {};
    float SelectionSmallOffsetX {};
    float SelectionSmallOffsetY {};
    bool ConsoleEdit {};
    string ConsoleStr {};
    array<char, 4096> ConsoleBuf {};
    bool ConsoleSyncFromState {true};
    vector<string> ConsoleHistory {};
    int32_t ConsoleHistoryCur {};
    vector<MessBoxMessage> MessBox {};
    string MessBoxCurText {};
    int32_t MessBoxScroll {};
    nptr<MapView> LastHistoryMap {};
    int32_t LastHistoryUndoCount {-1};
    bool UndoRedoInProgress {};
    size_t MaxUndoDepth {100};
    bool SpritesCanDraw {};
    uint8_t SelectAlpha {100};
    ucolor SelectContourColor {255, 215, 40};
};

FO_END_NAMESPACE
