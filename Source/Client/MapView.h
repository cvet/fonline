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

#include "ClientEntity.h"
#include "CritterHexView.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "FogOfWar.h"
#include "Geometry.h"
#include "ItemHexView.h"
#include "MapSprite.h"
#include "PathFinding.h"
#include "ScriptSystem.h"
#include "SpriteManager.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(MapViewLoadException);

class ClientEngine;
class ItemHexView;
class CritterHexView;
class CritterView;

enum class LightFlag : uint16_t
{
    None = 0,
    Global = 0x01,
    Inverse = 0x02,
    StopDir0 = 0x04,
    StopDir1 = 0x08,
    StopDir2 = 0x10,
    StopDir3 = 0x20,
    StopDir4 = 0x40,
    StopDir5 = 0x80,
    StopDir6 = 0x100,
    StopDir7 = 0x200,
};

///@ ExportRefType Client RefCounted Export = Finished, EveryHex, InteractWithRoof, CheckTileProperty, TileProperty, ExpectedTilePropertyValue, Finish
class SpritePattern : public RefCounted<SpritePattern>
{
public:
    void Finish();

    bool Finished {};
    ipos32 EveryHex {1, 1};
    bool InteractWithRoof {};
    bool CheckTileProperty {};
    ItemProperty TileProperty {};
    int32_t ExpectedTilePropertyValue {};
    vector<shared_ptr<Sprite>> Sprites {};
};

///@ ExportRefType Client RefCounted Export = Enabled, Distance, Radius, ExtraLength, TransitionDuration, OvalRoundness, EdgeNoise, Depth, ClearRadius, TintColor, OverlayColor, CenterColor, Traced, CheckShootBlocks, OriginHex, Disposed, Dispose
class FogLayer : public RefCounted<FogLayer>
{
public:
    void Dispose() noexcept;

    // Script-tunable configuration (exported as properties)
    bool Enabled {true};
    int32_t Distance {}; // Zone reach in hexes (traced overlay); 0 = derive from the look distance
    int32_t Radius {}; // Look radius in hexes; 0 = use the origin critter's look distance
    int32_t ExtraLength {1}; // Extra look length added to the derived radius
    int32_t TransitionDuration {150}; // Grow/move/shrink morph duration in ms
    float32_t OvalRoundness {1.0f}; // 0 = raw hexagon, 1 = full smooth oval
    float32_t EdgeNoise {0.15f}; // Drifting-mist rim ripple strength
    float32_t Depth {0.15f}; // Extra darkening with distance past the edge
    float32_t ClearRadius {0.85f}; // Fraction kept crisp before the edge fade (lower = wider border)
    ucolor TintColor {10, 13, 20, 255}; // Cold dim tint of the unseen area (rgb used; a ignored)
    ucolor OverlayColor {}; // Traced-overlay (zone) color
    ucolor CenterColor {}; // Traced-overlay center color
    bool Traced {}; // Positive traced overlay (attack/observation zone) vs inverse look fog
    bool CheckShootBlocks {true};
    mpos OriginHex {}; // Origin for a hex-anchored fog (ignored when following a critter)
    bool Disposed {}; // Script-readable status: true once Dispose() ran or the owning map was destroyed (recreate then)

    // Engine-internal (not exported to script)
    DrawOrderType DrawOrder {};
    bool FollowCritter {};
    ident_t OriginCritterId {};
    ipos32 OriginWorldPos {}; // Origin's absolute world-pixel pos (camera-independent); anchors edge noise to the world so it flows as the fog moves
    nptr<RenderEffect> CustomFogEffect {}; // Rasterizes the fog points; null = engine default
    nptr<RenderEffect> CustomFlushEffect {}; // Flushes the fog to the map; null = engine default
    FogShape Shape {};
};

class MapView final : public ClientEntity, public EntityWithProto, public MapProperties
{
public:
    struct LightSource
    {
        ident_t Id {};
        mpos Hex {};
        ucolor Color {};
        int32_t Distance {};
        LightFlag Flags {};
        int32_t Intensity {};
        nptr<const ipos32> Offset {};
        bool Applied {};
        bool NeedReapply {};
        int32_t StartIntensity {};
        int32_t TargetIntensity {};
        int32_t CurIntensity {};
        ucolor CenterColor {};
        int32_t Capacity {};
        vector<tuple<mpos, uint8_t, bool>> FanHexes {}; // Hex, Alpha, UseOffsets
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
        nptr<MapSprite> SpriteChain {};
        vector<ptr<CritterHexView>> Critters {};
        vector<ptr<CritterHexView>> OriginCritters {};
        vector<ptr<ItemHexView>> Items {};
        vector<ptr<ItemHexView>> OriginItems {};
        vector<pair<ptr<ItemHexView>, bool>> MultihexItems {}; // true if drawable
        unordered_map<ptr<LightSource>, ucolor> LightSources {};
        nptr<ItemHexView> GroundTile {};
        int32_t RoofNum {};
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
        vector<mdir> DirSteps {};
        vector<uint16_t> ControlSteps {};
        ipos16 EndHexOffset {};
    };

    MapView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoMap> proto, isize32 screen_size, nptr<const Properties> props = nullptr);
    MapView(const MapView&) = delete;
    MapView(MapView&&) noexcept = delete;
    auto operator=(const MapView&) = delete;
    auto operator=(MapView&&) noexcept = delete;
    ~MapView() override;

    [[nodiscard]] auto IsMapperMode() const noexcept -> bool { return _mapperMode; }
    [[nodiscard]] auto IsShowMapperOverlay() const noexcept -> bool { return _isShowMapperOverlay; }
    [[nodiscard]] auto IsShowMapperHiddenSprites() const noexcept -> bool { return _isShowMapperHiddenSprites; }
    [[nodiscard]] auto IsScrollCheck() const noexcept -> bool { return _scrollCheckEnabled; }
    [[nodiscard]] auto GetScreenSize() const noexcept -> isize32 { return _screenSize; }
    [[nodiscard]] auto GetField(mpos hex) noexcept -> const Field& { return _hexField->GetCellForReading(hex); }
    [[nodiscard]] auto IsHexToDraw(mpos hex) const noexcept -> bool { return _hexField->GetCellForReading(hex).IsView; }
    [[nodiscard]] auto GetLightData() noexcept -> ptr<ucolor> { return _hexLight.data(); }
    [[nodiscard]] auto IsManualScrolling() const noexcept -> bool;
    [[nodiscard]] auto IsAutoScrolling() const noexcept -> bool { return _autoScrollActive; }
    [[nodiscard]] auto GetHexContentSize(mpos hex) -> isize32;
    [[nodiscard]] auto GenTempEntityId() -> ident_t;

    void EnableMapperMode();
    void SetScrollCheck(bool enabled);
    void LoadFromFile(string_view map_name, const string& str);
    void LoadStaticData();
    void Process();

    void DrawMap();
    auto DrawEntitySprite(ptr<ClientEntity> entity, ptr<RenderEffect> effect, ucolor color, int32_t padding) -> bool;

    auto FindPath(nptr<CritterHexView> nullable_cr, mpos start_hex, mpos& target_hex, int32_t cut, ipos16 target_hex_offset = {}) -> optional<FindPathResult>;
    auto CutPath(nptr<CritterHexView> cr, mpos start_hex, mpos& target_hex, int32_t cut) -> bool;
    auto TraceMoveWay(mpos& start_hex, ipos16& hex_offset, vector<mdir>& dir_steps, mdir dir, int32_t multihex) const -> bool;
    void TraceBullet(mpos start_hex, mpos target_hex, int32_t dist, float32_t angle, nptr<vector<ptr<CritterHexView>>> nullable_critters, CritterFindType find_type, nptr<mpos> pre_block_hex, nptr<mpos> block_hex, nptr<vector<mpos>> hex_steps, bool check_shoot_blocks);

    void SetShowMapperOverlay(bool show);
    void SetShowMapperHiddenSprites(bool show);

    void SetScreenSize(isize32 size);
    auto ScreenToMapPos(ipos32 screen_pos) const -> ipos32;
    auto MapToScreenPos(ipos32 map_pos) const -> ipos32;
    auto GetScreenRawHex() const -> ipos32;
    auto GetCenterRawHex() const -> ipos32;
    auto ConvertToScreenRawHex(ipos32 center_raw_hex) const -> ipos32;
    auto GetHexMapPos(mpos hex) const -> ipos32;
    auto IsOutsideArea(mpos hex) const -> bool;

    void RebuildMap();
    void RebuildFog();
    auto AddFog(nptr<CritterView> cr, DrawOrderType draw_order, nptr<RenderEffect> custom_flush_effect) -> ptr<FogLayer>;
    auto AddFog(mpos hex, DrawOrderType draw_order, nptr<RenderEffect> custom_flush_effect) -> ptr<FogLayer>;
    auto MeasureMapBorders(ptr<const Sprite> spr, ipos32 offset) -> bool;
    auto MeasureMapBorders(ptr<const ItemHexView> item) -> bool;
    void RecacheHexFlags(mpos hex);
    void Resize(msize size);

    void ChangeZoom(float32_t new_zoom, fpos32 anchor);
    void InstantZoom(float32_t new_zoom, fpos32 anchor);

    auto GetScrollOffset() const noexcept -> fpos32 { return _scrollOffset; }
    void SetDayColors(ucolor map_color, int32_t map_light_capacity, ucolor global_color, int32_t global_light_capacity);
    void ScrollToHex(mpos hex, ipos16 hex_offset, int32_t speed, bool can_stop);
    void ApplyScrollOffset(ipos32 offset, int32_t speed, bool can_stop);
    void SetExtraScrollOffset(fpos32 offset);
    void InstantScroll(fpos32 scroll);
    void InstantScrollTo(mpos center_hex);

    // Critters
    auto AddReceivedCritter(ident_t id, hstring pid, mpos hex, mdir dir, const vector<vector<uint8_t>>& data, bool fade_in) -> ptr<CritterHexView>;
    auto AddMapperCritter(hstring pid, mpos hex, mdir dir, nptr<const Properties> props, ident_t id = {}) -> ptr<CritterHexView>;
    auto GetCritter(ident_t id) -> nptr<CritterHexView>;
    auto GetNonDeadCritter(mpos hex) -> nptr<CritterHexView>;
    auto GetCritters() -> span<refcount_ptr<CritterHexView>> { return _critters; }
    auto GetCritters() const -> const_span<refcount_ptr<CritterHexView>> { return _critters; }
    auto GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<ptr<CritterHexView>>;
    auto GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<ptr<const CritterHexView>>;
    auto GetCrittersInRadius(mpos hex, int32_t radius, CritterFindType find_type) -> vector<ptr<CritterHexView>>;
    void MoveCritter(ptr<CritterHexView> cr, mpos to_hex, bool smoothly);
    void ReapplyCritterView(ptr<CritterHexView> cr);
    void DestroyCritter(ptr<CritterHexView> cr);

    // Items
    auto AddReceivedItem(ident_t id, hstring pid, mpos hex, const vector<vector<uint8_t>>& data, bool fade_in) -> ptr<ItemHexView>;
    auto AddMapperItem(hstring pid, mpos hex, nptr<const Properties> props, ident_t id = {}) -> ptr<ItemHexView>;
    auto AddMapperTile(hstring pid, mpos hex, uint8_t layer, bool is_roof) -> ptr<ItemHexView>;
    auto AddLocalItem(hstring pid, mpos hex) -> ptr<ItemHexView>;
    auto GetItem(ident_t id) -> nptr<ItemHexView>;
    auto GetItemOnHex(mpos hex) -> nptr<ItemHexView>;
    auto GetItemOnHex(mpos hex, hstring pid) -> nptr<ItemHexView>;
    auto GetItems() -> span<refcount_ptr<ItemHexView>> { return _items; }
    auto GetItems() const -> const_span<refcount_ptr<ItemHexView>> { return _items; }
    auto GetItemsOnHex(mpos hex) -> span<ptr<ItemHexView>>;
    auto GetItemsOnHex(mpos hex) const -> const_span<ptr<ItemHexView>>;
    void RefreshItem(ptr<ItemHexView> item, bool deferred = false);
    void DefferedRefreshItems();
    void MoveItem(ptr<ItemHexView> item, mpos hex);
    void DestroyItem(ptr<ItemHexView> item);
    void DestroyItems(const_span<ptr<ItemHexView>> items);

    auto GetHexAtScreen(ipos32 screen_pos, mpos& hex, nptr<ipos32> hex_offset) const -> bool;
    auto GetItemAtScreen(ipos32 screen_pos, bool& item_egg, int32_t extra_range, bool check_transparent) -> pair<nptr<ItemHexView>, nptr<const MapSprite>>; // With transparent egg
    auto GetCritterAtScreen(ipos32 screen_pos, bool ignore_dead_and_chosen, int32_t extra_range, bool check_transparent) -> pair<nptr<CritterHexView>, nptr<const MapSprite>>;
    auto GetEntityAtScreen(ipos32 screen_pos, int32_t extra_range, bool check_transparent) -> pair<nptr<ClientEntity>, nptr<const MapSprite>>;

    void UpdateCritterLightSource(ptr<const CritterHexView> cr);
    void UpdateItemLightSource(ptr<const ItemHexView> item);
    void UpdateHexLightSources(mpos hex);

    void SetHiddenRoof(mpos hex);
    void SetTransparentEgg(TransparentEggSlot slot, mpos hex, ipos32 hex_offset, isize32 egg_size, bool apply_size_ext = false);
    void ClearTransparentEgg(TransparentEggSlot slot);

    auto AddMapSprite(ptr<const Sprite> spr, mpos hex, DrawOrderType draw_order, int32_t draw_order_hy_offset, ipos32 offset, nptr<const ipos32> poffset, nptr<const uint8_t> palpha, nptr<bool> callback) -> ptr<MapSprite>;

    auto RunSpritePattern(string_view name, size_t count) -> nptr<SpritePattern>;

    auto IsFastPid(hstring pid) const -> bool;
    auto IsIgnorePid(hstring pid) const -> bool;
    void AddFastPid(hstring pid);
    void ClearFastPids();
    void AddIgnorePid(hstring pid);
    void SwitchIgnorePid(hstring pid);
    void ClearIgnorePids();

    void SetHeaderExtraFields(map<string, string> fields);
    auto SaveToText() const -> string;

private:
    struct TransparentEggInfo
    {
        mpos Hex {};
        ipos32 HexOffset {};
        isize32 Size {};
        bool ApplySizeExt {};
        bool Valid {};
    };

    void ProcessScroll(float32_t dt);
    void ProcessZoom(float32_t dt);
    void RefreshMinZoom();

    auto AddCritterInternal(ptr<CritterHexView> cr) -> ptr<CritterHexView>;
    void AddCritterToField(ptr<CritterHexView> cr);
    void RemoveCritterFromField(ptr<CritterHexView> cr);
    void SetMultihexCritter(ptr<CritterHexView> cr, bool set);
    void DrawHexCritter(ptr<CritterHexView> cr, ptr<Field> field, mpos hex);

    auto AddItemInternal(ptr<ItemHexView> item) -> ptr<ItemHexView>;
    void AddItemToField(ptr<ItemHexView> item);
    void RemoveItemFromField(ptr<ItemHexView> item);
    void DrawHexItem(ptr<ItemHexView> item, ptr<Field> field, mpos hex, bool extra_draw);

    void RecacheHexFlags(ptr<Field> field);
    void RecacheScrollBlocks();

    void RebuildMapNow();
    void RebuildMapOffset(ipos32 axial_hex_offset);
    void ShowHexLines(int ox, int oy); // ox: -1 left 1 right oy: -1 top 1 bottom
    void HideHexLines(int ox, int oy);
    void ShowHex(const ViewField& vf);
    void HideHex(const ViewField& vf);

    auto GetViewSize() const -> isize32;
    void InitView();

    void AddSpriteToChain(ptr<Field> field, ptr<MapSprite> mspr);
    void InvalidateSpriteChain(ptr<Field> field);

    auto HasFogLayers() const noexcept -> bool;
    void PrepareFogToDraw();
    void DrawSpritesWithFog(const irect32& draw_area);
    void DrawFogSlot(const irect32& draw_area, DrawOrderType draw_order);

    void UpdateTransparentEgg(TransparentEggSlot slot);
    void UpdateTransparentEggs();

    void ProcessLighting();
    void UpdateLightSource(ident_t id, mpos hex, ucolor color, int32_t distance, LightFlag flags, int32_t intensity, nptr<const ipos32> offset);
    void FinishLightSource(ident_t id);
    void CleanLightSourceOffsets(ident_t id);
    void ApplyLightFan(ptr<LightSource> ls);
    void CleanLightFan(ptr<LightSource> ls);
    void TraceLightLine(ptr<LightSource> ls, mpos from_hex, mpos& to_hex, int32_t distance, int32_t raw_intensity);
    void MarkLightStep(ptr<LightSource> ls, mpos from_hex, mpos to_hex, int32_t raw_intensity);
    void MarkLightEnd(ptr<LightSource> ls, mpos from_hex, mpos to_hex, int32_t raw_intensity);
    void MarkLightEndNeighbor(ptr<LightSource> ls, mpos hex, bool north_south, int32_t raw_intensity);
    void MarkLight(ptr<LightSource> ls, mpos hex, int32_t raw_intensity);
    void CalculateHexLight(mpos hex, ptr<const Field> field);
    void LightFanToPrimitves(ptr<const LightSource> ls, vector<PrimitivePoint>& points) const;

    void OnDestroySelf() override;
    void OnScreenSizeChanged();

    EventUnsubscriber _eventUnsubscriber {};

    bool _mapperMode {};
    bool _mapLoading {};
    msize _mapSize {};
    float32_t _mapDepthHalf {};
    ident_t _workEntityId {};
    map<string, string> _headerExtraFields {};

    vector<refcount_ptr<CritterHexView>> _critters {};
    unordered_map<ident_t, ptr<CritterHexView>> _crittersMap {};
    vector<refcount_ptr<ItemHexView>> _items {};
    vector<ptr<ItemHexView>> _staticItems {};
    vector<ptr<ItemHexView>> _dynamicItems {};
    vector<ptr<ItemHexView>> _processingItems {};
    unordered_map<ident_t, ptr<ItemHexView>> _itemsMap {};
    unordered_set<refcount_ptr<ItemHexView>> _deferredRefreshItems {};

    optional<StaticTwoDimensionalGrid<Field, mpos, msize>> _hexField {};

    bool _rebuildMap {};
    MapSpriteList _mapSprites {};
    MapSpriteList _indoorMaskSprites {};

    fpos32 _zoomAnchor {};
    float32_t _minZoomScroll {};
    bool _scrollCheckEnabled {};
    irect32 _scrollArea {};
    fpos32 _scrollOffset {};
    fpos32 _extraScrollOffset {};
    timespan _scrollDtAccum {};

    bool _autoScrollActive {};
    bool _autoScrollCanStop {};
    fpos32 _autoScrollOffset {};
    int32_t _autoScrollSpeed {};

    bool _isShowMapperOverlay {true};
    bool _isShowMapperHiddenSprites {true};

    nptr<RenderTarget> _rtMap {};
    nptr<RenderTarget> _rtIndoorMask {}; // Currently-hidden-roof alpha mask; tells the weather shader where the player can see inside and should not be visually obstructed by snow/rain/dust
    nptr<RenderTarget> _rtLight {}; // Lighting and fog intermediate target
    optional<irect32> _currentRenderDrawArea {};

    isize32 _maxScroll {};
    isize32 _screenSize {};
    fsize32 _viewSize {};
    ipos32 _screenRawHex {}; // Left-top corner
    int32_t _hTop {};
    int32_t _hBottom {};
    int32_t _wLeft {};
    int32_t _wRight {};
    int32_t _wVisible {};
    int32_t _hVisible {};
    vector<ViewField> _viewField {};

    static constexpr size_t FOG_SLOT_COUNT = static_cast<size_t>(DrawOrderType::Last) + 1;
    array<vector<refcount_ptr<FogLayer>>, FOG_SLOT_COUNT> _fogs {};

    vector<ucolor> _hexLight {};
    vector<ucolor> _hexTargetLight {};
    nanotime _hexLightTime {};

    unordered_map<ident_t, unique_ptr<LightSource>> _lightSources {};
    unordered_map<ptr<LightSource>, size_t> _visibleLightSources {};
    vector<vector<PrimitivePoint>> _lightPoints {};
    size_t _globalLights {};
    bool _needReapplyLights {};
    bool _needRebuildLightPrimitives {};

    // Reused per-frame scratch buffers for Process() / ProcessLighting()
    vector<ptr<CritterHexView>> _critterToDeleteScratch {};
    vector<ptr<ItemHexView>> _itemToDeleteScratch {};
    vector<ptr<LightSource>> _reapplyLightSourcesScratch {};
    vector<ptr<LightSource>> _removeLightSourcesScratch {};

    int32_t _hiddenRoofNum {};

    array<TransparentEggInfo, SpriteManager::EGG_SLOT_COUNT> _transparentEggs {};

    unordered_set<hstring> _fastPids {};
    unordered_set<hstring> _ignorePids {};
    vector<refcount_ptr<SpritePattern>> _spritePatterns {};
};

FO_END_NAMESPACE
