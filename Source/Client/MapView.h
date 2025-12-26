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

#include "ClientEntity.h"
#include "CritterHexView.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "Geometry.h"
#include "ItemHexView.h"
#include "MapSprite.h"
#include "ScriptSystem.h"
#include "SpriteManager.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(MapViewLoadException);

class FOClient;
class ItemHexView;
class CritterHexView;

// Light flags
static constexpr uint8 LIGHT_GLOBAL = 0x40;
static constexpr uint8 LIGHT_INVERSE = 0x80;
static constexpr uint32 LIGHT_DISABLE_DIR_MASK = 0x3F;

///@ ExportRefType Client
struct SpritePattern
{
    FO_SCRIPTABLE_OBJECT_BEGIN();

    bool Finished {};
    ipos32 EveryHex {1, 1};
    bool InteractWithRoof {};
    bool CheckTileProperty {};
    ItemProperty TileProperty {};
    int32 ExpectedTilePropertyValue {};

    void Finish();

    FO_SCRIPTABLE_OBJECT_END();

    unique_ptr<vector<shared_ptr<Sprite>>> Sprites {};
};
static_assert(std::is_standard_layout_v<SpritePattern>);

class MapView final : public ClientEntity, public EntityWithProto, public MapProperties
{
public:
    struct LightSource
    {
        ident_t Id {};
        mpos Hex {};
        ucolor Color {};
        int32 Distance {};
        uint8 Flags {};
        int32 Intensity {};
        raw_ptr<const ipos32> Offset {};
        bool Applied {};
        bool NeedReapply {};
        int32 StartIntensity {};
        int32 TargetIntensity {};
        int32 CurIntensity {};
        ucolor CenterColor {};
        int32 Capacity {};
        vector<tuple<mpos, uint8, bool>> FanHexes {}; // Hex, Alpha, UseOffsets
        vector<mpos> MarkedHexes {};
        nanotime Time {};
        bool Finishing {};
    };

    struct ViewField
    {
        ipos32 RawHex {};
        ipos32 Offset {};
    };

    struct Field
    {
        ipos32 Offset {};
        raw_ptr<MapSprite> SpriteChain {};
        vector<raw_ptr<CritterHexView>> Critters {};
        vector<raw_ptr<CritterHexView>> OriginCritters {};
        vector<raw_ptr<ItemHexView>> Items {};
        vector<raw_ptr<ItemHexView>> OriginItems {};
        vector<pair<raw_ptr<ItemHexView>, bool>> MultihexItems {}; // true if drawable
        unordered_map<LightSource*, ucolor> LightSources {};
        raw_ptr<ItemHexView> GroundTile {};
        int32 RoofNum {};
        CornerType Corner {};
        bool IsView {};
        bool ScrollBlock {};
        bool HasWall {};
        bool HasTransparentWall {};
        bool HasScenery {};
        bool MoveBlocked {};
        bool ShootBlocked {};
        bool LightBlocked {};
        bool HasRoof {};
    };

    struct FindPathResult
    {
        vector<uint8> DirSteps {};
        vector<uint16> ControlSteps {};
    };

    MapView(FOClient* engine, ident_t id, const ProtoMap* proto, const Properties* props = nullptr);
    MapView(const MapView&) = delete;
    MapView(MapView&&) noexcept = delete;
    auto operator=(const MapView&) = delete;
    auto operator=(MapView&&) noexcept = delete;
    ~MapView() override;

    [[nodiscard]] auto IsMapperMode() const noexcept -> bool { return _mapperMode; }
    [[nodiscard]] auto IsShowTrack() const noexcept -> bool { return _isShowTrack; }
    [[nodiscard]] auto GetScreenSize() const noexcept -> isize32 { return _screenSize; }
    [[nodiscard]] auto GetField(mpos hex) noexcept -> const Field& { return _hexField->GetCellForReading(hex); }
    [[nodiscard]] auto IsHexToDraw(mpos hex) const noexcept -> bool { return _hexField->GetCellForReading(hex).IsView; }
    [[nodiscard]] auto GetHexTrack(mpos hex) noexcept -> int8& { return _hexTrack[static_cast<size_t>(hex.y) * _mapSize.width + hex.x]; }
    [[nodiscard]] auto GetLightData() noexcept -> ucolor* { return _hexLight.data(); }
    [[nodiscard]] auto IsManualScrolling() const noexcept -> bool;
    [[nodiscard]] auto GetHexContentSize(mpos hex) -> isize32;
    [[nodiscard]] auto GenTempEntityId() -> ident_t;

    void EnableMapperMode();
    void LoadFromFile(string_view map_name, const string& str);
    void LoadStaticData();
    void Process();

    void DrawMap();

    auto FindPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int32 cut) -> optional<FindPathResult>;
    auto CutPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int32 cut) -> bool;
    auto TraceMoveWay(mpos& start_hex, ipos16& hex_offset, vector<uint8>& dir_steps, int32 quad_dir) const -> bool;
    void TraceBullet(mpos start_hex, mpos target_hex, int32 dist, float32 angle, vector<CritterHexView*>* critters, CritterFindType find_type, mpos* pre_block_hex, mpos* block_hex, vector<mpos>* hex_steps, bool check_shoot_blocks);

    void ClearHexTrack();
    void SwitchShowTrack();

    auto ScreenToMapPos(ipos32 screen_pos) const -> ipos32;
    auto MapToScreenPos(ipos32 map_pos) const -> ipos32;
    auto GetScreenRawHex() const -> ipos32;
    auto GetCenterRawHex() const -> ipos32;
    auto ConvertToScreenRawHex(ipos32 center_raw_hex) const -> ipos32;
    auto GetHexMapPos(mpos hex) const -> ipos32;
    auto IsOutsideArea(mpos hex) const -> bool;

    void RebuildMap() { _rebuildMap = true; }
    void RebuildFog() { _rebuildFog = true; }
    void SetShootBorders(bool enabled, int32 dist);
    auto MeasureMapBorders(const Sprite* spr, ipos32 offset) -> bool;
    auto MeasureMapBorders(const ItemHexView* item) -> bool;
    void RecacheHexFlags(mpos hex);
    void Resize(msize size);

    void ChangeZoom(float32 new_zoom, fpos32 anchor);
    void InstantZoom(float32 new_zoom, fpos32 anchor);

    void ScrollToHex(mpos hex, ipos16 hex_offset, int32 speed, bool can_stop);
    void ApplyScrollOffset(ipos32 offset, int32 speed, bool can_stop);
    void LockScreenScroll(CritterView* cr, int32 speed, bool soft_lock, bool unlock_if_same);
    void SetExtraScrollOffset(fpos32 offset);
    void InstantScroll(fpos32 scroll);
    void InstantScrollTo(mpos center_hex);

    // Critters
    auto AddReceivedCritter(ident_t id, hstring pid, mpos hex, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*;
    auto AddMapperCritter(hstring pid, mpos hex, int16 dir_angle, const Properties* props) -> CritterHexView*;
    auto GetCritter(ident_t id) -> CritterHexView*;
    auto GetNonDeadCritter(mpos hex) -> CritterHexView*;
    auto GetCritters() -> span<refcount_ptr<CritterHexView>> { return _critters; }
    auto GetCritters() const -> span<const refcount_ptr<CritterHexView>> { return _critters; }
    auto GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<CritterHexView*>;
    auto GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<const CritterHexView*>;
    void MoveCritter(CritterHexView* cr, mpos to_hex, bool smoothly);
    void ReapplyCritterView(CritterHexView* cr);
    void DestroyCritter(CritterHexView* cr);

    void SetCritterContour(ident_t cr_id, ContourType contour);
    void SetCrittersContour(ContourType contour);

    // Items
    auto AddReceivedItem(ident_t id, hstring pid, mpos hex, const vector<vector<uint8>>& data) -> ItemHexView*;
    auto AddMapperItem(hstring pid, mpos hex, const Properties* props) -> ItemHexView*;
    auto AddMapperTile(hstring pid, mpos hex, uint8 layer, bool is_roof) -> ItemHexView*;
    auto AddLocalItem(hstring pid, mpos hex) -> ItemHexView*;
    auto GetItem(ident_t id) -> ItemHexView*;
    auto GetItemOnHex(mpos hex) -> ItemHexView*;
    auto GetItemOnHex(mpos hex, hstring pid) -> ItemHexView*;
    auto GetItems() -> span<refcount_ptr<ItemHexView>> { return _items; }
    auto GetItems() const -> span<const refcount_ptr<ItemHexView>> { return _items; }
    auto GetItemsOnHex(mpos hex) -> span<raw_ptr<ItemHexView>>;
    auto GetItemsOnHex(mpos hex) const -> span<const raw_ptr<ItemHexView>>;
    void RefreshItem(ItemHexView* item, bool deferred = false);
    void DefferedRefreshItems();
    void MoveItem(ItemHexView* item, mpos hex);
    void DestroyItem(ItemHexView* item);

    auto GetHexAtScreen(ipos32 screen_pos, mpos& hex, ipos32* hex_offset) const -> bool;
    auto GetItemAtScreen(ipos32 screen_pos, bool& item_egg, int32 extra_range, bool check_transparent) -> pair<ItemHexView*, const MapSprite*>; // With transparent egg
    auto GetCritterAtScreen(ipos32 screen_pos, bool ignore_dead_and_chosen, int32 extra_range, bool check_transparent) -> pair<CritterHexView*, const MapSprite*>;
    auto GetEntityAtScreen(ipos32 screen_pos, int32 extra_range, bool check_transparent) -> pair<ClientEntity*, const MapSprite*>;

    void UpdateCritterLightSource(const CritterHexView* cr);
    void UpdateItemLightSource(const ItemHexView* item);
    void UpdateHexLightSources(mpos hex);

    void SwitchShowHex();
    void MarkBlockedHexes();
    void SetHiddenRoof(mpos hex);

    auto AddMapSprite(const Sprite* spr, mpos hex, DrawOrderType draw_order, int32 draw_order_hy_offset, ipos32 offset, const ipos32* poffset, const uint8* palpha, bool* callback) -> MapSprite*;

    auto RunSpritePattern(string_view name, size_t count) -> SpritePattern*;

    auto IsFastPid(hstring pid) const -> bool;
    auto IsIgnorePid(hstring pid) const -> bool;
    void AddFastPid(hstring pid);
    void ClearFastPids();
    void AddIgnorePid(hstring pid);
    void SwitchIgnorePid(hstring pid);
    void ClearIgnorePids();

    auto SaveToText() const -> string;

private:
    void ProcessScroll(float32 dt);
    void ProcessZoom(float32 dt);
    void RefreshMinZoom();

    auto AddCritterInternal(CritterHexView* cr) -> CritterHexView*;
    void AddCritterToField(CritterHexView* cr);
    void RemoveCritterFromField(CritterHexView* cr);
    void SetMultihexCritter(CritterHexView* cr, bool set);
    void DrawHexCritter(CritterHexView* cr, Field& field, mpos hex);

    auto AddItemInternal(ItemHexView* item) -> ItemHexView*;
    void AddItemToField(ItemHexView* item);
    void RemoveItemFromField(ItemHexView* item);
    void DrawHexItem(ItemHexView* item, Field& field, mpos hex, bool extra_draw);

    void RecacheHexFlags(Field& field);
    void RecacheScrollBlocks();

    void RebuildMapNow();
    void RebuildMapOffset(ipos32 axial_hex_offset);
    void ShowHexLines(int ox, int oy); // ox: -1 left 1 right oy: -1 top 1 bottom
    void HideHexLines(int ox, int oy);
    void ShowHex(const ViewField& vf);
    void HideHex(const ViewField& vf);

    auto GetViewSize() const -> isize32;
    void InitView();

    void AddSpriteToChain(Field& field, MapSprite* mspr);
    void InvalidateSpriteChain(Field& field);

    void PrepareFogToDraw();

    void ProcessLighting();
    void UpdateLightSource(ident_t id, mpos hex, ucolor color, int32 distance, uint8 flags, int32 intensity, const ipos32* offset);
    void FinishLightSource(ident_t id);
    void CleanLightSourceOffsets(ident_t id);
    void ApplyLightFan(LightSource* ls);
    void CleanLightFan(LightSource* ls);
    void TraceLightLine(LightSource* ls, mpos from_hex, mpos& to_hex, int32 distance, int32 intensity);
    void MarkLightStep(LightSource* ls, mpos from_hex, mpos to_hex, int32 intensity);
    void MarkLightEnd(LightSource* ls, mpos from_hex, mpos to_hex, int32 intensity);
    void MarkLightEndNeighbor(LightSource* ls, mpos hex, bool north_south, int32 intensity);
    void MarkLight(LightSource* ls, mpos hex, int32 intensity);
    void CalculateHexLight(mpos hex, const Field& field);
    void LightFanToPrimitves(const LightSource* ls, vector<PrimitivePoint>& points, vector<PrimitivePoint>& soft_points) const;

    void OnDestroySelf() override;
    void OnScreenSizeChanged();

    static auto GetColorDay(const vector<int32>& day_time, const vector<uint8>& colors, int32 game_time, int32* light) -> ucolor;

    EventUnsubscriber _eventUnsubscriber {};

    bool _mapperMode {};
    bool _mapLoading {};
    msize _mapSize {};
    ident_t _workEntityId {};

    vector<refcount_ptr<CritterHexView>> _critters {};
    unordered_map<ident_t, raw_ptr<CritterHexView>> _crittersMap {};
    vector<refcount_ptr<ItemHexView>> _items {};
    vector<raw_ptr<ItemHexView>> _staticItems {};
    vector<raw_ptr<ItemHexView>> _dynamicItems {};
    vector<raw_ptr<ItemHexView>> _processingItems {};
    unordered_map<ident_t, raw_ptr<ItemHexView>> _itemsMap {};
    unordered_set<refcount_ptr<ItemHexView>> _deferredRefreshItems {};

    unique_ptr<StaticTwoDimensionalGrid<Field, mpos, msize>> _hexField {};
    vector<int16> _findPathGrid {};

    bool _rebuildMap {};
    MapSpriteList _mapSprites {};

    fpos32 _zoomAnchor {};
    float32 _minZoomScroll {};
    irect32 _scrollArea {};
    fpos32 _scrollOffset {};
    fpos32 _extraScrollOffset {};
    timespan _scrollDtAccum {};

    bool _autoScrollActive {};
    bool _autoScrollCanStop {};
    fpos32 _autoScrollOffset {};
    int32 _autoScrollSpeed {};
    ident_t _autoScrollHardLockedCritter {};
    ident_t _autoScrollSoftLockedCritter {};
    mpos _autoScrollCritterLastHex {};
    ipos16 _autoScrollCritterLastHexOffset {};
    int32 _autoScrollLockSpeed {};

    shared_ptr<Sprite> _picTrack1 {};
    shared_ptr<Sprite> _picTrack2 {};
    shared_ptr<Sprite> _picHexMask {};
    vector<ucolor> _picHexMaskData {};
    shared_ptr<Sprite> _picHex[3] {};
    bool _isShowTrack {};
    bool _isShowHex {};

    raw_ptr<RenderTarget> _rtMap {};
    raw_ptr<RenderTarget> _rtLight {}; // Lighting and fog intermediate target

    isize32 _maxScroll {};
    isize32 _screenSize {};
    fsize32 _viewSize {};
    ipos32 _screenRawHex {}; // Left-top corner
    int32 _hTop {};
    int32 _hBottom {};
    int32 _wLeft {};
    int32 _wRight {};
    int32 _wVisible {};
    int32 _hVisible {};
    vector<ViewField> _viewField {};

    bool _rebuildFog {};
    bool _drawLookBorders {true};
    bool _drawShootBorders {};
    int32 _shootBordersDist {};
    vector<PrimitivePoint> _fogLookPoints {};
    vector<PrimitivePoint> _fogShootPoints {};

    ident_t _critterContourCrId {};
    ContourType _critterContour {};
    ContourType _crittersContour {};

    vector<ucolor> _hexLight {};
    vector<ucolor> _hexTargetLight {};
    nanotime _hexLightTime {};

    int32 _prevMapDayTime {-1};
    int32 _prevGlobalDayTime {-1};
    ucolor _prevMapDayColor {};
    ucolor _prevGlobalDayColor {};
    ucolor _mapDayColor {};
    ucolor _globalDayColor {};
    int32 _mapDayLightCapacity {};
    int32 _globalDayLightCapacity {};

    unordered_map<ident_t, unique_ptr<LightSource>> _lightSources {};
    unordered_map<LightSource*, size_t> _visibleLightSources {};
    vector<vector<PrimitivePoint>> _lightPoints {};
    vector<vector<PrimitivePoint>> _lightSoftPoints {};
    size_t _globalLights {};
    bool _needReapplyLights {};
    bool _needRebuildLightPrimitives {};

    int32 _hiddenRoofNum {};

    unordered_set<hstring> _fastPids {};
    unordered_set<hstring> _ignorePids {};
    vector<int8> _hexTrack {};

    vector<refcount_ptr<SpritePattern>> _spritePatterns {};
};

FO_END_NAMESPACE();
