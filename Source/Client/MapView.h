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
#include "Entity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ItemHexView.h"
#include "MapSprite.h"
#include "ScriptSystem.h"
#include "SpriteManager.h"
#include "TwoDimensionalGrid.h"

DECLARE_EXCEPTION(MapViewLoadException);

class FOClient;
class ItemHexView;
class CritterHexView;

struct FindPathResult
{
    vector<uint8> DirSteps {};
    vector<uint16> ControlSteps {};
};

///@ ExportRefType Client
struct SpritePattern
{
    SCRIPTABLE_OBJECT_BEGIN();

    bool Finished {};
    string SprName {};
    uint SprCount {};
    ipos EveryHex {1, 1};
    bool InteractWithRoof {};
    bool CheckTileProperty {};
    ItemProperty TileProperty {};
    int ExpectedTilePropertyValue {};

    void Finish() { NON_CONST_METHOD_HINT_ONELINE() FinishCallback ? FinishCallback() : (void)FinishCallback; }

    SCRIPTABLE_OBJECT_END();

    std::function<void()> FinishCallback {};
    vector<shared_ptr<Sprite>> Sprites {};
};
// Todo: fix static_assert(std::is_standard_layout_v<SpritePattern>);

class MapView final : public ClientEntity, public EntityWithProto, public MapProperties
{
public:
    struct LightSource
    {
        ident_t Id {};
        mpos Hex {};
        ucolor Color {};
        uint Distance {};
        uint8 Flags {};
        int Intensity {};
        const ipos* Offset {};
        bool Applied {};
        bool NeedReapply {};
        uint StartIntensity {};
        uint TargetIntensity {};
        uint CurIntensity {};
        ucolor CenterColor {};
        uint Capacity {};
        ipos LastOffset {};
        vector<tuple<mpos, uint8, bool>> FanHexes {}; // Hex, Alpha, UseOffsets
        vector<mpos> MarkedHexes {};
        time_point Time {};
        bool Finishing {};
    };

    struct ViewField
    {
        ipos RawHex {};
        ipos Offset {};
        fpos Offsetf {};
    };

    struct FieldFlags
    {
        bool ScrollBlock {};
        bool HasWall {};
        bool HasTransparentWall {};
        bool HasScenery {};
        bool MoveBlocked {};
        bool ShootBlocked {};
        bool LightBlocked {};
    };

    struct Field
    {
        bool IsView {};
        ipos Offset {};
        MapSprite* SpriteChain {};
        vector<CritterHexView*> Critters {};
        vector<CritterHexView*> MultihexCritters {};
        vector<ItemHexView*> Items {};
        vector<ItemHexView*> BlockLineItems {};
        vector<ItemHexView*> GroundTiles {};
        vector<ItemHexView*> RoofTiles {};
        int16 RoofNum {};
        CornerType Corner {};
        FieldFlags Flags {};
        unordered_map<LightSource*, ucolor> LightSources {};
    };

    struct AutoScrollInfo
    {
        bool Active {};
        bool CanStop {};
        fpos Offset {};
        fpos OffsetStep {};
        float Speed {};
        ident_t HardLockedCritter {};
        ident_t SoftLockedCritter {};
        mpos CritterLastHex {};
    };

    MapView(FOClient* engine, ident_t id, const ProtoMap* proto, const Properties* props = nullptr);
    MapView(const MapView&) = delete;
    MapView(MapView&&) noexcept = delete;
    auto operator=(const MapView&) = delete;
    auto operator=(MapView&&) noexcept = delete;
    ~MapView() override;

    [[nodiscard]] auto IsMapperMode() const noexcept -> bool { return _mapperMode; }
    [[nodiscard]] auto IsShowTrack() const noexcept -> bool { return _isShowTrack; }
    [[nodiscard]] auto GetField(mpos pos) noexcept -> const Field& { NON_CONST_METHOD_HINT_ONELINE() return _hexField->GetCellForReading(pos); }
    [[nodiscard]] auto IsHexToDraw(mpos pos) const noexcept -> bool { return _hexField->GetCellForReading(pos).IsView; }
    [[nodiscard]] auto GetHexTrack(mpos pos) noexcept -> char& { return _hexTrack[static_cast<size_t>(pos.y) * _mapSize.width + pos.x]; }
    [[nodiscard]] auto GetLightData() noexcept -> ucolor* { return _hexLight.data(); }
    [[nodiscard]] auto GetGlobalDayTime() const noexcept -> int;
    [[nodiscard]] auto GetMapDayTime() const noexcept -> int;
    [[nodiscard]] auto GetDrawList() noexcept -> MapSpriteList&;
    [[nodiscard]] auto IsScrollEnabled() const noexcept -> bool;

    void EnableMapperMode();
    void LoadFromFile(string_view map_name, const string& str);
    void LoadStaticData();
    void Process();

    void DrawMap();
    void DrawMapTexts();

    void AddMapText(string_view str, mpos hex, ucolor color, time_duration show_time, bool fade, ipos offset);
    auto GetRectForText(mpos hex) -> IRect;

    auto FindPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int cut) -> optional<FindPathResult>;
    auto CutPath(CritterHexView* cr, mpos start_hex, mpos& target_hex, int cut) -> bool;
    auto TraceMoveWay(mpos& start_hex, ipos16& hex_offset, vector<uint8>& dir_steps, int quad_dir) const -> bool;
    void TraceBullet(mpos start_hex, mpos target_hex, uint dist, float angle, vector<CritterHexView*>* critters, CritterFindType find_type, mpos* pre_block_hex, mpos* block_hex, vector<mpos>* hex_steps, bool check_shoot_blocks);

    void ClearHexTrack();
    void SwitchShowTrack();

    void ChangeZoom(int zoom); // < 0 in, > 0 out, 0 normalize

    auto GetScreenRawHex() const -> ipos;
    auto GetHexCurrentPosition(mpos hex) const -> ipos;

    void FindSetCenter(mpos hex);
    void RebuildMap(ipos screen_raw_hex);
    void RebuildMapOffset(ipos hex_offset);
    void RefreshMap() { RebuildMap(_screenRawHex); }
    void RebuildFog() { _rebuildFog = true; }
    void SetShootBorders(bool enabled, uint dist);
    auto MeasureMapBorders(const Sprite* spr, ipos offset) -> bool;
    auto MeasureMapBorders(const ItemHexView* item) -> bool;
    void RecacheHexFlags(mpos hex);
    void Resize(msize size);

    auto Scroll() -> bool;
    void ScrollToHex(mpos hex, float speed, bool can_stop);
    void ScrollOffset(ipos offset, float speed, bool can_stop);

    void SwitchShowHex();

    // Critters
    auto AddReceivedCritter(ident_t id, hstring pid, mpos hex, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*;
    auto AddMapperCritter(hstring pid, mpos hex, int16 dir_angle, const Properties* props) -> CritterHexView*;
    auto GetCritter(ident_t id) -> CritterHexView*;
    auto GetNonDeadCritter(mpos hex) -> CritterHexView*;
    auto GetCritters() -> const vector<CritterHexView*>&;
    auto GetCritters(mpos hex, CritterFindType find_type) -> vector<CritterHexView*>;
    void MoveCritter(CritterHexView* cr, mpos to_hex, bool smoothly);
    void DestroyCritter(CritterHexView* cr);

    void SetCritterContour(ident_t cr_id, ContourType contour);
    void SetCrittersContour(ContourType contour);

    // Items
    auto AddReceivedItem(ident_t id, hstring pid, mpos hex, const vector<vector<uint8>>& data) -> ItemHexView*;
    auto AddMapperItem(hstring pid, mpos hex, const Properties* props) -> ItemHexView*;
    auto AddMapperTile(hstring pid, mpos hex, uint8 layer, bool is_roof) -> ItemHexView*;
    auto GetItem(mpos hex, hstring pid) -> ItemHexView*;
    auto GetItem(mpos hex, ident_t id) -> ItemHexView*;
    auto GetItem(ident_t id) -> ItemHexView*;
    auto GetItems() -> const vector<ItemHexView*>&;
    auto GetItems(mpos hex) -> const vector<ItemHexView*>&;
    auto GetTile(mpos hex, bool is_roof, int layer) -> ItemHexView*;
    auto GetTiles(mpos hex, bool is_roof) -> const vector<ItemHexView*>&;
    void MoveItem(ItemHexView* item, mpos hex);
    void DestroyItem(ItemHexView* item);

    auto GetHexAtScreenPos(ipos pos, mpos& hex, ipos* hex_offset) const -> bool;
    auto GetItemAtScreenPos(ipos pos, bool& item_egg, int extra_range, bool check_transparent) -> ItemHexView*; // With transparent egg
    auto GetCritterAtScreenPos(ipos pos, bool ignore_dead_and_chosen, int extra_range, bool check_transparent) -> CritterHexView*;
    auto GetEntityAtScreenPos(ipos pos, int extra_range, bool check_transparent) -> ClientEntity*;

    void UpdateCritterLightSource(const CritterHexView* cr);
    void UpdateItemLightSource(const ItemHexView* item);
    void UpdateHexLightSources(mpos hex);

    void SetSkipRoof(mpos hex);
    void MarkRoofNum(ipos raw_hex, int16 num);

    void RunEffectItem(hstring eff_pid, mpos from_hex, mpos to_hex);

    auto RunSpritePattern(string_view name, uint count) -> SpritePattern*;

    void SetCursorPos(CritterHexView* cr, ipos pos, bool show_steps, bool refresh);
    void DrawCursor(const Sprite* spr);
    void DrawCursor(string_view text);

    [[nodiscard]] auto IsFastPid(hstring pid) const -> bool;
    [[nodiscard]] auto IsIgnorePid(hstring pid) const -> bool;
    [[nodiscard]] auto GetHexesRect(mpos from_hex, mpos to_hex) const -> vector<mpos>;

    void AddFastPid(hstring pid);
    void ClearFastPids();
    void AddIgnorePid(hstring pid);
    void SwitchIgnorePid(hstring pid);
    void ClearIgnorePids();
    void MarkBlockedHexes();

    auto GetTempEntityId() const -> ident_t;

    auto ValidateForSave() const -> vector<string>;
    auto SaveToText() const -> string;

    AutoScrollInfo AutoScroll {};

private:
    struct MapText
    {
        mpos Hex {};
        time_point StartTime {};
        time_duration Duration {};
        string Text {};
        ucolor Color {};
        bool Fade {};
        IRect Pos {};
        IRect EndPos {};
    };

    [[nodiscard]] auto IsVisible(const Sprite* spr, ipos offset) const -> bool;
    [[nodiscard]] auto GetViewSize() const -> isize;
    [[nodiscard]] auto ScrollCheckPos(int (&view_fields_to_check)[4], uint8 dir1, optional<uint8> dir2) const -> bool;
    [[nodiscard]] auto ScrollCheck(int xmod, int ymod) const -> bool;

    void OnDestroySelf() override;

    auto AddCritterInternal(CritterHexView* cr) -> CritterHexView*;
    void AddCritterToField(CritterHexView* cr);
    void RemoveCritterFromField(CritterHexView* cr);
    void SetMultihexCritter(CritterHexView* cr, bool set);

    auto AddItemInternal(ItemHexView* item) -> ItemHexView*;
    void AddItemToField(ItemHexView* item);
    void RemoveItemFromField(ItemHexView* item);

    void RecacheHexFlags(Field& field);

    void PrepareFogToDraw();
    void InitView(ipos screen_raw_hex);
    void ResizeView();

    void AddSpriteToChain(Field& field, MapSprite* mspr);
    void InvalidateSpriteChain(Field& field);

    // Lighting
    void ProcessLighting();
    void UpdateLightSource(ident_t id, mpos hex, ucolor color, uint distance, uint8 flags, int intensity, const ipos* offset);
    void FinishLightSource(ident_t id);
    void CleanLightSourceOffsets(ident_t id);
    void ApplyLightFan(LightSource* ls);
    void CleanLightFan(LightSource* ls);
    void TraceLightLine(LightSource* ls, mpos from_hex, mpos& to_hex, uint distance, uint intensity);
    void MarkLightStep(LightSource* ls, mpos from_hex, mpos to_hex, uint intensity);
    void MarkLightEnd(LightSource* ls, mpos from_hex, mpos to_hex, uint intensity);
    void MarkLightEndNeighbor(LightSource* ls, mpos hex, bool north_south, uint intensity);
    void MarkLight(LightSource* ls, mpos hex, uint intensity);
    void CalculateHexLight(mpos hex, const Field& field);
    void LightFanToPrimitves(const LightSource* ls, vector<PrimitivePoint>& points, vector<PrimitivePoint>& soft_points) const;

    void OnScreenSizeChanged();

    EventUnsubscriber _eventUnsubscriber {};

    bool _mapperMode {};
    bool _mapLoading {};
    msize _mapSize {};

    vector<CritterHexView*> _critters {};
    unordered_map<ident_t, CritterHexView*> _crittersMap {};
    vector<ItemHexView*> _allItems {};
    vector<ItemHexView*> _staticItems {};
    vector<ItemHexView*> _dynamicItems {};
    vector<ItemHexView*> _nonTileItems {};
    vector<ItemHexView*> _processingItems {};
    unordered_map<ident_t, ItemHexView*> _itemsMap {};

    vector<MapText> _mapTexts {};

    unique_ptr<StaticTwoDimensionalGrid<Field, mpos, msize>> _hexField {};
    vector<int16> _findPathGrid {};

    MapSpriteList _mapSprites;

    time_point _scrollLastTime {};

    shared_ptr<Sprite> _picTrack1 {};
    shared_ptr<Sprite> _picTrack2 {};
    shared_ptr<Sprite> _picHexMask {};
    vector<ucolor> _picHexMaskData {};
    shared_ptr<Sprite> _picHex[3] {};
    bool _isShowTrack {};
    bool _isShowHex {};

    RenderTarget* _rtMap {};
    RenderTarget* _rtLight {};
    RenderTarget* _rtFog {};
    int _rtScreenOx {};
    int _rtScreenOy {};

    ipos _screenRawHex {};
    int _hTop {};
    int _hBottom {};
    int _wLeft {};
    int _wRight {};
    int _wVisible {};
    int _hVisible {};
    vector<ViewField> _viewField {};

    const ipos* _fogOffset {};
    ipos _fogLastOffset {};
    bool _fogForceRerender {};
    bool _rebuildFog {};
    bool _drawLookBorders {true};
    bool _drawShootBorders {};
    uint _shootBordersDist {};
    vector<PrimitivePoint> _fogLookPoints {};
    vector<PrimitivePoint> _fogShootPoints {};

    ident_t _critterContourCrId {};
    ContourType _critterContour {};
    ContourType _crittersContour {};

    vector<ucolor> _hexLight {};
    vector<ucolor> _hexTargetLight {};
    time_point _hexLightTime {};

    int _prevMapDayTime {};
    int _prevGlobalDayTime {};
    ucolor _prevMapDayColor {};
    ucolor _prevGlobalDayColor {};
    ucolor _mapDayColor {};
    ucolor _globalDayColor {};
    int _mapDayLightCapacity {};
    int _globalDayLightCapacity {};

    unordered_map<ident_t, unique_ptr<LightSource>> _lightSources {};
    unordered_map<LightSource*, size_t> _visibleLightSources {};
    vector<vector<PrimitivePoint>> _lightPoints {};
    vector<vector<PrimitivePoint>> _lightSoftPoints {};
    size_t _globalLights {};
    bool _needReapplyLights {};
    bool _needRebuildLightPrimitives {};

    int _roofSkip {};

    int _drawCursorX {};
    shared_ptr<Sprite> _cursorPrePic {};
    shared_ptr<Sprite> _cursorPostPic {};
    shared_ptr<Sprite> _cursorXPic {};
    int _cursorX {};
    int _cursorY {};
    int _lastCurX {};
    mpos _lastCurPos {};

    unordered_set<hstring> _fastPids {};
    unordered_set<hstring> _ignorePids {};
    vector<char> _hexTrack {};

    vector<unique_release_ptr<SpritePattern>> _spritePatterns {};
};
