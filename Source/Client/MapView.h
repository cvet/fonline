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

struct LightSource
{
    uint16 HexX {};
    uint16 HexY {};
    uint ColorRGB {};
    int Distance {};
    uint8 Flags {};
    int Intensity {};
    int* OffsX {};
    int* OffsY {};
    int LastOffsX {};
    int LastOffsY {};
};

struct FindPathResult
{
    vector<uint8> Steps {};
    vector<uint16> ControlSteps {};
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
    bool IsMultihex {};
};

struct Field
{
    bool IsView {};
    int ScrX {};
    int ScrY {};
    MapSprite* SpriteChain {};
    vector<CritterHexView*> Critters {};
    vector<ItemHexView*> Items {};
    vector<ItemHexView*> BlockLineItems {};
    vector<ItemHexView*> GroundTiles {};
    vector<ItemHexView*> RoofTiles {};
    int16 RoofNum {};
    CornerType Corner {};
    FieldFlags Flags {};
};

///@ ExportObject Client
struct ParticlePattern
{
    SCRIPTABLE_OBJECT_BEGIN();

    bool Finished {};
    string ParticleName {};
    uint ParticleCount {};
    uint16 EveryHexX {1};
    uint16 EveryHexY {1};
    bool InteractWithRoof {};
    bool CheckTileProperty {};
    ItemProperty TileProperty {};
    int ExpectedTilePropertyValue {};

    void Finish() { NON_CONST_METHOD_HINT_ONELINE() FinishCallback ? FinishCallback() : (void)FinishCallback; }

    SCRIPTABLE_OBJECT_END();

    std::function<void()> FinishCallback {};
    vector<unique_del_ptr<ParticleSprite>> Particles {};
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
    [[nodiscard]] auto GetField(uint16 hx, uint16 hy) -> const Field& { return _hexField[hy * _width + hx]; }
    [[nodiscard]] auto IsHexToDraw(uint16 hx, uint16 hy) const -> bool { return _hexField[hy * _width + hx].IsView; }
    [[nodiscard]] auto GetHexTrack(uint16 hx, uint16 hy) -> char& { NON_CONST_METHOD_HINT_ONELINE() return _hexTrack[hy * _width + hx]; }
    [[nodiscard]] auto GetLightHex(uint16 hx, uint16 hy) -> uint8* { NON_CONST_METHOD_HINT_ONELINE() return &_hexLight[hy * _width * 3 + hx * 3]; }
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

    void AddMapText(string_view str, uint16 hx, uint16 hy, uint color, time_duration show_time, bool fade, int ox, int oy);
    auto GetRectForText(uint16 hx, uint16 hy) -> IRect;

    auto FindPath(CritterHexView* cr, uint16 start_x, uint16 start_y, uint16& end_x, uint16& end_y, int cut) -> optional<FindPathResult>;
    auto CutPath(CritterHexView* cr, uint16 start_x, uint16 start_y, uint16& end_x, uint16& end_y, int cut) -> bool;
    auto TraceMoveWay(uint16& hx, uint16& hy, int& ox, int& oy, vector<uint8>& steps, int quad_dir) -> bool;
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
    void EvaluateFieldFlags(uint16 hx, uint16 hy);
    void EvaluateFieldFlags(Field& field);
    void Resize(uint16 width, uint16 height);

    auto Scroll() -> bool;
    void ScrollToHex(int hx, int hy, float speed, bool can_stop);
    void ScrollOffset(int ox, int oy, float speed, bool can_stop);

    void SwitchShowHex();

    // Critters
    auto AddReceivedCritter(ident_t id, hstring pid, uint16 hx, uint16 hy, int16 dir_angle, const vector<vector<uint8>>& data) -> CritterHexView*;
    auto AddMapperCritter(hstring pid, uint16 hx, uint16 hy, int16 dir_angle, const Properties* props) -> CritterHexView*;
    auto GetCritter(ident_t id) -> CritterHexView*;
    auto GetActiveCritter(uint16 hx, uint16 hy) -> CritterHexView*;
    auto GetCritters() -> const vector<CritterHexView*>&;
    auto GetCritters(uint16 hx, uint16 hy, CritterFindType find_type) -> vector<CritterHexView*>;
    void MoveCritter(CritterHexView* cr, uint16 hx, uint16 hy, bool smoothly);
    void DestroyCritter(CritterHexView* cr);

    void SetCritterContour(ident_t cr_id, ContourType contour);
    void SetCrittersContour(ContourType contour);
    void SetMultihex(uint16 hx, uint16 hy, uint multihex, bool set);

    // Items
    auto AddReceivedItem(ident_t id, hstring pid, uint16 hx, uint16 hy, const vector<vector<uint8>>& data) -> ItemHexView*;
    auto AddMapperItem(hstring pid, uint16 hx, uint16 hy, const Properties* props) -> ItemHexView*;
    auto AddMapperTile(hstring pid, uint16 hx, uint16 hy, uint8 layer, bool is_roof) -> ItemHexView*;
    auto GetItem(uint16 hx, uint16 hy, hstring pid) -> ItemHexView*;
    auto GetItem(uint16 hx, uint16 hy, ident_t id) -> ItemHexView*;
    auto GetItem(ident_t id) -> ItemHexView*;
    auto GetItems() -> const vector<ItemHexView*>&;
    auto GetItems(uint16 hx, uint16 hy) -> const vector<ItemHexView*>&;
    auto GetTile(uint16 hx, uint16 hy, bool is_roof, int layer) -> ItemHexView*;
    auto GetTiles(uint16 hx, uint16 hy, bool is_roof) -> const vector<ItemHexView*>&;
    void MoveItem(ItemHexView* item, uint16 hx, uint16 hy);
    void DestroyItem(ItemHexView* item);

    auto GetHexAtScreenPos(int x, int y, uint16& hx, uint16& hy, int* hex_ox, int* hex_oy) const -> bool;
    auto GetItemAtScreenPos(int x, int y, bool& item_egg, int extra_range, bool check_transparent) -> ItemHexView*; // With transparent egg
    auto GetCritterAtScreenPos(int x, int y, bool ignore_dead_and_chosen, int extra_range, bool check_transparent) -> CritterHexView*;
    auto GetEntityAtScreenPos(int x, int y, int extra_range, bool check_transparent) -> ClientEntity*;

    void RebuildLight() { _requestRebuildLight = _requestRenderLight = true; }

    void SetSkipRoof(uint16 hx, uint16 hy);
    void MarkRoofNum(int hxi, int hyi, int16 num);

    void RunEffectItem(hstring eff_pid, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy);

    auto RunParticlePattern(string_view name, uint count) -> ParticlePattern*;

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
        uint Color {};
        bool Fade {};
        IRect Pos {};
        IRect EndPos {};
    };

    [[nodiscard]] auto FieldAt(uint16 hx, uint16 hy) -> Field& { NON_CONST_METHOD_HINT_ONELINE() return _hexField[hy * _width + hx]; }
    [[nodiscard]] auto IsVisible(const Sprite* spr, int ox, int oy) const -> bool;
    [[nodiscard]] auto GetViewWidth() const -> int;
    [[nodiscard]] auto GetViewHeight() const -> int;
    [[nodiscard]] auto ScrollCheckPos(int (&positions)[4], int dir1, int dir2) -> bool;
    [[nodiscard]] auto ScrollCheck(int xmod, int ymod) -> bool;

    auto AddCritterInternal(CritterHexView* cr) -> CritterHexView*;
    void AddCritterToField(CritterHexView* cr);
    void RemoveCritterFromField(CritterHexView* cr);

    auto AddItemInternal(ItemHexView* item) -> ItemHexView*;
    void AddItemToField(ItemHexView* item);
    void RemoveItemFromField(ItemHexView* item);

    void PrepareFogToDraw();
    void InitView(int screen_hx, int screen_hy);
    void ResizeView();

    void AddSpriteToChain(Field& field, MapSprite* mspr);
    void InvalidateSpriteChain(Field& field);

    // Lighting
    void PrepareLightToDraw();
    void MarkLight(uint16 hx, uint16 hy, uint inten);
    void MarkLightEndNeighbor(uint16 hx, uint16 hy, bool north_south, uint inten);
    void MarkLightEnd(uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint inten);
    void MarkLightStep(uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint inten);
    void TraceLight(uint16 from_hx, uint16 from_hy, uint16& hx, uint16& hy, int dist, uint inten);
    void ParseLightTriangleFan(const LightSource& ls);
    void RealRebuildLight();
    void CollectLightSources();

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

    vector<Field> _hexField {};
    vector<int16> _findPathGrid {};

    MapSpriteList _mapSprites;

    time_point _scrollLastTime {};

    unique_ptr<SpriteSheet> _picTrack1 {};
    unique_ptr<SpriteSheet> _picTrack2 {};
    unique_ptr<SpriteSheet> _picHexMask {};
    vector<uint> _picHexMaskData {};
    unique_ptr<SpriteSheet> _picHex[3] {};
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

    bool _requestRebuildLight {};
    bool _requestRenderLight {};
    vector<uint8> _hexLight {};

    int _prevMapDayTime {};
    int _prevGlobalDayTime {};
    uint _prevMapDayColor {};
    uint _prevGlobalDayColor {};
    uint _mapDayColor {};
    uint _globalDayColor {};
    int _mapDayLightCapacity {};
    int _globalDayLightCapacity {};

    size_t _lightPointsCount {};
    vector<vector<PrimitivePoint>> _lightPoints {};
    vector<PrimitivePoint> _lightSoftPoints {};
    vector<LightSource> _lightSources {};
    vector<LightSource> _staticLightSources {};
    int _lightCapacity {};
    int _lightMinHx {};
    int _lightMaxHx {};
    int _lightMinHy {};
    int _lightMaxHy {};
    int _lightProcentR {};
    int _lightProcentG {};
    int _lightProcentB {};
    bool _hasGlobalLights {};

    int _roofSkip {};

    int _drawCursorX {};
    unique_ptr<SpriteSheet> _cursorPrePic {};
    unique_ptr<SpriteSheet> _cursorPostPic {};
    unique_ptr<SpriteSheet> _cursorXPic {};
    int _cursorX {};
    int _cursorY {};
    int _lastCurX {};
    uint16 _lastCurHx {};
    uint16 _lastCurHy {};

    set<hstring> _fastPids {};
    set<hstring> _ignorePids {};
    vector<char> _hexTrack {};

    vector<unique_release_ptr<ParticlePattern>> _particlePatterns {};
};
