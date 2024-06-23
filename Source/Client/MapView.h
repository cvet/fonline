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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "MapLoader.h"
#include "MapSprite.h"
#include "ScriptSystem.h"
#include "SpriteManager.h"
#include "TwoDimensionalGrid.h"

DECLARE_EXCEPTION(MapViewLoadException);

static constexpr auto MAX_FIND_PATH = 600;

class FOClient;
class ItemHexView;
class CritterHexView;

struct ViewField
{
    int HexX {};
    int HexY {};
    int ScrX {};
    int ScrY {};
    float ScrXf {};
    float ScrYf {};
};

struct FindPathResult
{
    vector<uint8> Steps {};
    vector<uint16> ControlSteps {};
};

struct LightSource
{
    ident_t Id {};
    uint16 HexX {};
    uint16 HexY {};
    ucolor Color {};
    uint Distance {};
    uint8 Flags {};
    int Intensity {};
    const int* OffsX {};
    const int* OffsY {};
    bool Applied {};
    bool NeedReapply {};
    uint StartIntensity {};
    uint TargetIntensity {};
    uint CurIntensity {};
    ucolor CenterColor {};
    uint Capacity {};
    int LastOffsX {};
    int LastOffsY {};
    vector<tuple<uint16, uint16, uint8, bool>> FanHexes {}; // HexX, HexY, Alpha, UseOffsets
    vector<tuple<uint16, uint16>> MarkedHexes {}; // HexX, HexY, Color
    time_point Time {};
    bool Finishing {};
};

struct FieldFlags
{
    bool ScrollBlock {};
    bool IsWall {};
    bool IsWallTransp {};
    bool IsScen {};
    bool IsMoveBlocked {};
    bool IsShootBlocked {};
    bool IsLightBlocked {};
};

struct Field
{
    bool IsView {};
    int ScrX {};
    int ScrY {};
    int16 RoofNum {};
    CornerType Corner {};
    FieldFlags Flags {};
    MapSprite* SpriteChain {};
    small_vector<CritterHexView*, 3> Critters {};
    small_vector<CritterHexView*, 3> MultihexCritters {};
    small_vector<ItemHexView*, 3> Items {};
    small_vector<ItemHexView*, 3> BlockLineItems {};
    small_vector<ItemHexView*, 3> GroundTiles {};
    small_vector<ItemHexView*, 3> RoofTiles {};
    unordered_map<LightSource*, ucolor> LightSources {};
};

///@ ExportObject Client
struct SpritePattern
{
    SCRIPTABLE_OBJECT_BEGIN();

    bool Finished {};
    string SprName {};
    uint SprCount {};
    uint16 EveryHexX {1};
    uint16 EveryHexY {1};
    bool InteractWithRoof {};
    bool CheckTileProperty {};
    ItemProperty TileProperty {};
    int ExpectedTilePropertyValue {};

    void Finish() { NON_CONST_METHOD_HINT_ONELINE() FinishCallback ? FinishCallback() : (void)FinishCallback; }

    SCRIPTABLE_OBJECT_END();

    std::function<void()> FinishCallback {};
    vector<shared_ptr<Sprite>> Sprites {};
};

class MapView final : public ClientEntity, public EntityWithProto, public MapProperties
{
public:
    struct AutoScrollInfo
    {
        bool Active {};
        bool CanStop {};
        float OffsX {};
        float OffsY {};
        float OffsXStep {};
        float OffsYStep {};
        float Speed {};
        ident_t HardLockedCritter {};
        ident_t SoftLockedCritter {};
        uint16 CritterLastHexX {};
        uint16 CritterLastHexY {};
    };

    MapView(FOClient* engine, ident_t id, const ProtoMap* proto, const Properties* props = nullptr);
    MapView(const MapView&) = delete;
    MapView(MapView&&) noexcept = delete;
    auto operator=(const MapView&) = delete;
    auto operator=(MapView&&) noexcept = delete;
    ~MapView() override;

    [[nodiscard]] auto IsMapperMode() const -> bool { return _mapperMode; }
    [[nodiscard]] auto IsShowTrack() const -> bool { return _isShowTrack; }
    [[nodiscard]] auto GetField(uint16 hx, uint16 hy) -> const Field& { NON_CONST_METHOD_HINT_ONELINE() return _hexField.GetCellForReading(hx, hy); }
    [[nodiscard]] auto IsHexToDraw(uint16 hx, uint16 hy) const -> bool { return _hexField.GetCellForReading(hx, hy).IsView; }
    [[nodiscard]] auto GetHexTrack(uint16 hx, uint16 hy) -> char& { return _hexTrack[static_cast<size_t>(hy) * _width + hx]; }
    [[nodiscard]] auto GetLightData() -> ucolor* { return _hexLight.data(); }
    [[nodiscard]] auto GetGlobalDayTime() const -> int;
    [[nodiscard]] auto GetMapDayTime() const -> int;
    [[nodiscard]] auto GetDrawList() -> MapSpriteList&;
    [[nodiscard]] auto IsScrollEnabled() const -> bool;

    void MarkAsDestroyed() override;
    void EnableMapperMode();
    void LoadFromFile(string_view map_name, const string& str);
    void LoadStaticData();
    void Process();

    void DrawMap();
    void DrawMapTexts();

    void AddMapText(string_view str, uint16 hx, uint16 hy, ucolor color, time_duration show_time, bool fade, int ox, int oy);
    auto GetRectForText(uint16 hx, uint16 hy) -> IRect;

    auto FindPath(CritterHexView* cr, uint16 start_x, uint16 start_y, uint16& end_x, uint16& end_y, int cut) -> optional<FindPathResult>;
    auto CutPath(CritterHexView* cr, uint16 start_x, uint16 start_y, uint16& end_x, uint16& end_y, int cut) -> bool;
    auto TraceMoveWay(uint16& hx, uint16& hy, int& ox, int& oy, vector<uint8>& steps, int quad_dir) const -> bool;
    auto TraceBullet(uint16 hx, uint16 hy, uint16 tx, uint16 ty, uint dist, float angle, vector<CritterHexView*>* critters, CritterFindType find_type, pair<uint16, uint16>* pre_block, pair<uint16, uint16>* block, vector<pair<uint16, uint16>>* steps, bool check_passed) -> bool;

    void ClearHexTrack();
    void SwitchShowTrack();

    void ChangeZoom(int zoom); // < 0 in, > 0 out, 0 normalize

    auto GetScreenHexes() const -> tuple<int, int>;
    void GetHexCurrentPosition(uint16 hx, uint16 hy, int& x, int& y) const;

    void FindSetCenter(int cx, int cy);
    void RebuildMap(int screen_hx, int screen_hy);
    void RebuildMapOffset(int ox, int oy);
    void RefreshMap() { RebuildMap(_screenHexX, _screenHexY); }
    void RebuildFog() { _rebuildFog = true; }
    void SetShootBorders(bool enabled);
    auto MeasureMapBorders(const Sprite* spr, int ox, int oy) -> bool;
    auto MeasureMapBorders(const ItemHexView* item) -> bool;
    void RecacheHexFlags(uint16 hx, uint16 hy);
    void Resize(uint16 width, uint16 height);

    auto Scroll() -> bool;
    void ScrollToHex(int hx, int hy, float speed, bool can_stop);
    void ScrollOffset(int ox, int oy, float speed, bool can_stop);

    void SwitchShowHex();

    // Critters
    auto AddReceivedCritter(ident_t id, hstring pid, uint16 hx, uint16 hy, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*;
    auto AddMapperCritter(hstring pid, uint16 hx, uint16 hy, int16 dir_angle, const Properties* props) -> CritterHexView*;
    auto GetCritter(ident_t id) -> CritterHexView*;
    auto GetNonDeadCritter(uint16 hx, uint16 hy) -> CritterHexView*;
    auto GetCritters() -> const vector<CritterHexView*>&;
    auto GetCritters(uint16 hx, uint16 hy, CritterFindType find_type) -> vector<CritterHexView*>;
    void MoveCritter(CritterHexView* cr, uint16 hx, uint16 hy, bool smoothly);
    void DestroyCritter(CritterHexView* cr);

    void SetCritterContour(ident_t cr_id, ContourType contour);
    void SetCrittersContour(ContourType contour);

    // Items
    auto AddReceivedItem(ident_t id, hstring pid, uint16 hx, uint16 hy, const vector<vector<uint8>>& data) -> ItemHexView*;
    auto AddMapperItem(hstring pid, uint16 hx, uint16 hy, const Properties* props) -> ItemHexView*;
    auto AddMapperTile(hstring pid, uint16 hx, uint16 hy, uint8 layer, bool is_roof) -> ItemHexView*;
    auto GetItem(uint16 hx, uint16 hy, hstring pid) -> ItemHexView*;
    auto GetItem(uint16 hx, uint16 hy, ident_t id) -> ItemHexView*;
    auto GetItem(ident_t id) -> ItemHexView*;
    auto GetItems() -> const vector<ItemHexView*>&;
    auto GetItems(uint16 hx, uint16 hy) -> vector<ItemHexView*>;
    auto GetTile(uint16 hx, uint16 hy, bool is_roof, int layer) -> ItemHexView*;
    auto GetTiles(uint16 hx, uint16 hy, bool is_roof) -> vector<ItemHexView*>;
    void MoveItem(ItemHexView* item, uint16 hx, uint16 hy);
    void DestroyItem(ItemHexView* item);

    auto GetHexAtScreenPos(int x, int y, uint16& hx, uint16& hy, int* hex_ox, int* hex_oy) const -> bool;
    auto GetItemAtScreenPos(int x, int y, bool& item_egg, int extra_range, bool check_transparent) -> ItemHexView*; // With transparent egg
    auto GetCritterAtScreenPos(int x, int y, bool ignore_dead_and_chosen, int extra_range, bool check_transparent) -> CritterHexView*;
    auto GetEntityAtScreenPos(int x, int y, int extra_range, bool check_transparent) -> ClientEntity*;

    void UpdateCritterLightSource(const CritterHexView* cr);
    void UpdateItemLightSource(const ItemHexView* item);
    void UpdateHexLightSources(uint16 hx, uint16 hy);

    void SetSkipRoof(uint16 hx, uint16 hy);
    void MarkRoofNum(int hxi, int hyi, int16 num);

    void RunEffectItem(hstring eff_pid, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy);

    auto RunSpritePattern(string_view name, uint count) -> SpritePattern*;

    void SetCursorPos(CritterHexView* cr, int x, int y, bool show_steps, bool refresh);
    void DrawCursor(const Sprite* spr);
    void DrawCursor(string_view text);

    [[nodiscard]] auto IsFastPid(hstring pid) const -> bool;
    [[nodiscard]] auto IsIgnorePid(hstring pid) const -> bool;
    [[nodiscard]] auto GetHexesRect(const IRect& rect) const -> vector<pair<uint16, uint16>>;

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
        uint16 HexX {};
        uint16 HexY {};
        time_point StartTime {};
        time_duration Duration {};
        string Text {};
        ucolor Color {};
        bool Fade {};
        IRect Pos {};
        IRect EndPos {};
    };

    [[nodiscard]] auto IsVisible(const Sprite* spr, int ox, int oy) const -> bool;
    [[nodiscard]] auto GetViewWidth() const -> int;
    [[nodiscard]] auto GetViewHeight() const -> int;
    [[nodiscard]] auto ScrollCheckPos(int (&positions)[4], int dir1, int dir2) const -> bool;
    [[nodiscard]] auto ScrollCheck(int xmod, int ymod) const -> bool;

    auto AddCritterInternal(CritterHexView* cr) -> CritterHexView*;
    void AddCritterToField(CritterHexView* cr);
    void RemoveCritterFromField(CritterHexView* cr);
    void SetMultihexCritter(CritterHexView* cr, bool set);

    auto AddItemInternal(ItemHexView* item) -> ItemHexView*;
    void AddItemToField(ItemHexView* item);
    void RemoveItemFromField(ItemHexView* item);

    void RecacheHexFlags(Field& field);

    void PrepareFogToDraw();
    void InitView(int screen_hx, int screen_hy);
    void ResizeView();

    void AddSpriteToChain(Field& field, MapSprite* mspr);
    void InvalidateSpriteChain(Field& field);

    // Lighting
    void ProcessLighting();
    void UpdateLightSource(ident_t id, uint16 hx, uint16 hy, ucolor color, uint distance, uint8 flags, int intensity, const int* ox, const int* oy);
    void FinishLightSource(ident_t id);
    void CleanLightSourceOffsets(ident_t id);
    void ApplyLightFan(LightSource* ls);
    void CleanLightFan(LightSource* ls);
    void TraceLightLine(LightSource* ls, uint16 from_hx, uint16 from_hy, uint16& hx, uint16& hy, uint distance, uint intensity);
    void MarkLightStep(LightSource* ls, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint intensity);
    void MarkLightEnd(LightSource* ls, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint intensity);
    void MarkLightEndNeighbor(LightSource* ls, uint16 hx, uint16 hy, bool north_south, uint intensity);
    void MarkLight(LightSource* ls, uint16 hx, uint16 hy, uint intensity);
    void CalculateHexLight(uint16 hx, uint16 hy, const Field& field);
    void LightFanToPrimitves(const LightSource* ls, vector<PrimitivePoint>& points, vector<PrimitivePoint>& soft_points) const;

    void OnScreenSizeChanged();

    EventUnsubscriber _eventUnsubscriber {};

    bool _mapperMode {};
    bool _mapLoading {};
    uint16 _width {};
    uint16 _height {};

    vector<CritterHexView*> _critters {};
    unordered_map<ident_t, CritterHexView*> _crittersMap {};
    vector<ItemHexView*> _allItems {};
    vector<ItemHexView*> _staticItems {};
    vector<ItemHexView*> _dynamicItems {};
    vector<ItemHexView*> _nonTileItems {};
    vector<ItemHexView*> _processingItems {};
    unordered_map<ident_t, ItemHexView*> _itemsMap {};

    vector<MapText> _mapTexts {};

    TwoDimensionalGrid<Field, uint16, false> _hexField {};
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

    int _screenHexX {};
    int _screenHexY {};
    int _hTop {};
    int _hBottom {};
    int _wLeft {};
    int _wRight {};
    int _wVisible {};
    int _hVisible {};
    vector<ViewField> _viewField {};

    int* _fogOffsX {};
    int* _fogOffsY {};
    int _fogLastOffsX {};
    int _fogLastOffsY {};
    bool _fogForceRerender {};
    bool _rebuildFog {};
    bool _drawLookBorders {true};
    bool _drawShootBorders {};
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
    uint16 _lastCurHx {};
    uint16 _lastCurHy {};

    unordered_set<hstring> _fastPids {};
    unordered_set<hstring> _ignorePids {};
    vector<char> _hexTrack {};

    vector<unique_release_ptr<SpritePattern>> _spritePatterns {};
};
