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
#include "ClientEntity.h"
#include "CritterHexView.h"
#include "EffectManager.h"
#include "Entity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "GeometryHelper.h"
#include "ItemHexView.h"
#include "MapLoader.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "SpriteManager.h"
#include "Sprites.h"
#include "Timer.h"

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
    ushort HexX {};
    ushort HexY {};
    uint ColorRGB {};
    uchar Distance {};
    uchar Flags {};
    int Intensity {};
    short* OffsX {};
    short* OffsY {};
    short LastOffsX {};
    short LastOffsY {};
};

class Field final
{
public:
    struct Tile
    {
        AnyFrames* Anim {};
        short OffsX {};
        short OffsY {};
        uchar Layer {};
    };
    using TileVec = vector<Tile>;

    struct FlagsType
    {
        bool ScrollBlock : 1;
        bool IsWall : 1;
        bool IsWallTransp : 1;
        bool IsScen : 1;
        bool IsNotPassed : 1;
        bool IsNotRaked : 1;
        bool IsNoLight : 1;
        bool IsMultihex : 1;
    };

    Field() = default;
    Field(const Field&) = delete;
    Field(Field&&) noexcept = delete;
    auto operator=(const Field&) = delete;
    auto operator=(Field&&) noexcept = delete;
    ~Field();

    [[nodiscard]] auto GetTilesCount(bool is_roof) -> uint;
    [[nodiscard]] auto GetTile(uint index, bool is_roof) -> Tile&;

    auto AddTile(AnyFrames* anim, short ox, short oy, uchar layer, bool is_roof) -> Tile&;
    void AddItem(ItemHexView* item, ItemHexView* block_lines_item);
    void EraseItem(ItemHexView* item, ItemHexView* block_lines_item);
    void EraseTile(uint index, bool is_roof);
    void AddDeadCrit(CritterHexView* cr);
    void EraseDeadCrit(CritterHexView* cr);
    void ProcessCache();
    void AddSpriteToChain(Sprite* spr);
    void UnvalidateSpriteChain() const;

    bool IsView {};
    Sprite* SpriteChain {};
    CritterHexView* Crit {};
    vector<CritterHexView*>* DeadCrits {};
    int ScrX {};
    int ScrY {};
    AnyFrames* SimplyTile[2] {};
    vector<Tile>* Tiles[2] {};
    vector<ItemHexView*>* Items {};
    vector<ItemHexView*>* BlockLinesItems {};
    short RoofNum {};
    FlagsType Flags {};
    CornerType Corner {};
};

class MapView final : public ClientEntity, public MapProperties
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
        uint HardLockedCritter {};
        uint SoftLockedCritter {};
        ushort CritterLastHexX {};
        ushort CritterLastHexY {};
    };

    MapView(FOClient* engine, uint id, const ProtoMap* proto);
    MapView(const MapView&) = delete;
    MapView(MapView&&) noexcept = delete;
    auto operator=(const MapView&) = delete;
    auto operator=(MapView&&) noexcept = delete;
    ~MapView() override;

    [[nodiscard]] auto IsMapperMode() const -> bool { return _mapperMode; }
    [[nodiscard]] auto IsShowTrack() const -> bool { return _isShowTrack; }
    [[nodiscard]] auto GetField(ushort hx, ushort hy) -> Field& { NON_CONST_METHOD_HINT_ONELINE() return _hexField[hy * _maxHexX + hx]; }
    [[nodiscard]] auto IsHexToDraw(ushort hx, ushort hy) const -> bool { return _hexField[hy * _maxHexX + hx].IsView; }
    [[nodiscard]] auto GetHexTrack(ushort hx, ushort hy) -> char& { NON_CONST_METHOD_HINT_ONELINE() return _hexTrack[hy * _maxHexX + hx]; }
    [[nodiscard]] auto GetLightHex(ushort hx, ushort hy) -> uchar* { NON_CONST_METHOD_HINT_ONELINE() return &_hexLight[hy * _maxHexX * 3 + hx * 3]; }
    [[nodiscard]] auto GetDayTime() const -> int;
    [[nodiscard]] auto GetMapTime() const -> int;
    [[nodiscard]] auto GetMapDayTime() -> int*;
    [[nodiscard]] auto GetMapDayColor() -> uchar*;
    [[nodiscard]] auto GetDrawTree() -> Sprites& { return _mainTree; }

    void MarkAsDestroyed() override;
    void EnableMapperMode();

    void Process();
    void DrawMap();

    void AddTile(const MapTile& tile);

    void AddMapText(string_view str, ushort hx, ushort hy, uint color, uint show_time, bool fade, int ox, int oy);
    auto GetRectForText(ushort hx, ushort hy) -> IRect;

    auto FindPath(CritterHexView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, vector<uchar>& steps, int cut) -> bool;
    auto CutPath(CritterHexView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, int cut) -> bool;
    auto TraceBullet(ushort hx, ushort hy, ushort tx, ushort ty, uint dist, float angle, CritterHexView* find_cr, bool find_cr_safe, vector<CritterHexView*>* critters, CritterFindType find_type, pair<ushort, ushort>* pre_block, pair<ushort, ushort>* block, vector<pair<ushort, ushort>>* steps, bool check_passed) -> bool;

    void ClearHexTrack();
    void SwitchShowTrack();

    void ReloadSprites();
    void OnResolutionChanged();

    void ChangeZoom(int zoom); // < 0 in, > 0 out, 0 normalize

    void GetScreenHexes(int& sx, int& sy) const;
    void GetHexCurrentPosition(ushort hx, ushort hy, int& x, int& y) const;

    void FindSetCenter(int cx, int cy);
    void ProcessHexBorders(ItemHexView* item);
    void ResizeField(ushort w, ushort h);
    void RebuildMap(int rx, int ry);
    void RebuildMapOffset(int ox, int oy);
    void RefreshMap() { RebuildMap(_screenHexX, _screenHexY); }

    void SetFog(PrimitivePoints& look_points, PrimitivePoints& shoot_points, short* offs_x, short* offs_y);

    auto Scroll() -> bool;
    void ScrollToHex(int hx, int hy, float speed, bool can_stop);
    void ScrollOffset(int ox, int oy, float speed, bool can_stop);

    void SwitchShowHex();

    // Critters
    auto AddCritter(uint id, const ProtoCritter* proto, const map<string, string>& props_kv) -> CritterHexView*;
    auto AddCritter(uint id, const ProtoCritter* proto, ushort hx, ushort hy, const vector<vector<uchar>>& data) -> CritterHexView*;
    auto GetCritter(uint id) -> CritterHexView*;
    auto GetCritters() -> const vector<CritterHexView*>&;
    auto GetCritters(ushort hx, ushort hy, CritterFindType find_type) -> vector<CritterHexView*>;
    void MoveCritter(CritterHexView* cr, ushort hx, ushort hy);
    auto TransitCritter(CritterHexView* cr, ushort hx, ushort hy, bool animate, bool force) -> bool;
    void DestroyCritter(CritterHexView* cr);

    void SetCritterContour(uint crid, int contour);
    void SetCrittersContour(int contour);
    void SetMultihex(ushort hx, ushort hy, uint multihex, bool set);

    // Items
    auto AddItem(uint id, const ProtoItem* proto, const map<string, string>& props_kv) -> ItemHexView*;
    auto AddItem(uint id, hstring pid, ushort hx, ushort hy, bool is_added, vector<vector<uchar>>* data) -> ItemHexView*;
    auto GetItem(ushort hx, ushort hy, hstring pid) -> ItemHexView*;
    auto GetItem(ushort hx, ushort hy, uint id) -> ItemHexView*;
    auto GetItem(uint id) -> ItemHexView*;
    auto GetItems(ushort hx, ushort hy) -> vector<ItemHexView*>;
    auto GetItems() -> vector<ItemHexView*> { return _items; }
    void MoveItem(ItemHexView* item, ushort hx, ushort hy);
    void DestroyItem(ItemHexView* item);

    void SkipItemsFade();

    auto GetHexScreenPos(int x, int y, ushort& hx, ushort& hy) const -> bool;
    auto GetItemAtScreenPos(int x, int y, bool& item_egg) -> ItemHexView*; // With transparent egg
    auto GetCritterAtScreenPos(int x, int y, bool ignore_dead_and_chosen) -> CritterHexView*;
    auto GetEntityAtScreenPos(int x, int y) -> ClientEntity*;

    void ClearHexLight();
    void RebuildLight() { _requestRebuildLight = _requestRenderLight = true; }
    auto GetLights() -> vector<LightSource>& { return _lightSources; }

    void RebuildTiles();
    void RebuildRoof();
    void SetSkipRoof(ushort hx, ushort hy);
    void MarkRoofNum(int hxi, int hyi, short num);

    auto RunEffect(hstring eff_pid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) -> bool;

    void SetCursorPos(CritterHexView* cr, int x, int y, bool show_steps, bool refresh);
    void DrawCursor(uint spr_id);
    void DrawCursor(string_view text);

    [[nodiscard]] auto GetTiles(ushort hx, ushort hy, bool is_roof) -> vector<MapTile>&;
    [[nodiscard]] auto IsFastPid(hstring pid) const -> bool;
    [[nodiscard]] auto IsIgnorePid(hstring pid) const -> bool;
    [[nodiscard]] auto GetHexesRect(const IRect& rect) const -> vector<pair<ushort, ushort>>;

    void ClearSelTiles();
    void ParseSelTiles();
    void SetTile(hstring name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof, bool select);
    void EraseTile(ushort hx, ushort hy, uchar layer, bool is_roof, uint skip_index);
    void AddFastPid(hstring pid);
    void ClearFastPids();
    void AddIgnorePid(hstring pid);
    void SwitchIgnorePid(hstring pid);
    void ClearIgnorePids();
    void MarkPassedHexes();

    AutoScrollInfo AutoScroll {};
    uchar SelectAlpha {100};
    ProtoMap* CurProtoMap {};
    vector<vector<MapTile>> TilesField {};
    vector<vector<MapTile>> RoofsField {};

private:
    struct MapText
    {
        ushort HexX {};
        ushort HexY {};
        uint StartTick {};
        uint Tick {};
        string Text;
        uint Color {};
        bool Fade {};
        IRect Pos {};
        IRect EndPos {};
    };

    [[nodiscard]] auto IsVisible(uint spr_id, int ox, int oy) const -> bool;
    [[nodiscard]] auto GetViewWidth() const -> int;
    [[nodiscard]] auto GetViewHeight() const -> int;
    [[nodiscard]] auto ScrollCheckPos(int (&positions)[4], int dir1, int dir2) -> bool;
    [[nodiscard]] auto ScrollCheck(int xmod, int ymod) -> bool;

    void ProcessItems();

    void AddCritter(CritterHexView* cr);
    void AddCritterToField(CritterHexView* cr);
    void RemoveCritterFromField(CritterHexView* cr);

    void AddItem(ItemHexView* item);
    void AddItemToField(ItemHexView* item);
    void RemoveItemFromField(ItemHexView* item);

    void DrawMapTexts();

    void PrepareFogToDraw();
    void InitView(int cx, int cy);
    void ResizeView();
    auto ProcessHexBorders(uint spr_id, int ox, int oy, bool resize_map) -> bool;
    auto ProcessTileBorder(Field::Tile& tile, bool is_roof) -> bool;

    // Lighting
    void PrepareLightToDraw();
    void MarkLight(ushort hx, ushort hy, uint inten);
    void MarkLightEndNeighbor(ushort hx, ushort hy, bool north_south, uint inten);
    void MarkLightEnd(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten);
    void MarkLightStep(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten);
    void TraceLight(ushort from_hx, ushort from_hy, ushort& hx, ushort& hy, int dist, uint inten);
    void ParseLightTriangleFan(LightSource& ls);
    void RealRebuildLight();
    void CollectLightSources();

    vector<CritterHexView*> _critters {};
    map<uint, CritterHexView*> _crittersMap {};
    vector<ItemHexView*> _items {};
    map<uint, ItemHexView*> _itemsMap {};
    vector<MapText> _mapTexts {};

    SpriteVec _spritesPool {};
    Sprites _mainTree;
    Sprites _tilesTree;
    Sprites _roofTree;
    ushort _maxHexX {};
    ushort _maxHexY {};
    Field* _hexField {};
    char* _hexTrack {};
    AnyFrames* _picTrack1 {};
    AnyFrames* _picTrack2 {};
    AnyFrames* _picHexMask {};
    bool _isShowTrack {};
    bool _isShowHex {};
    AnyFrames* _picHex[3] {};
    string _curDataPrefix {};
    short* _findPathGrid {};
    int _curMapTime {-1};
    int _dayTime[4] {};
    uchar _dayColor[12] {};
    bool _mapperMode {};
    RenderTarget* _rtMap {};
    RenderTarget* _rtLight {};
    RenderTarget* _rtFog {};
    uint _rtScreenOx {};
    uint _rtScreenOy {};
    ViewField* _viewField {};
    int _screenHexX {};
    int _screenHexY {};
    int _hTop {};
    int _hBottom {};
    int _wLeft {};
    int _wRight {};
    int _wVisible {};
    int _hVisible {};
    short* _fogOffsX {};
    short* _fogOffsY {};
    short _fogLastOffsX {};
    short _fogLastOffsY {};
    bool _fogForceRerender {};
    PrimitivePoints _fogLookPoints {};
    PrimitivePoints _fogShootPoints {};
    uint _critterContourCrId {};
    int _critterContour {};
    int _crittersContour {};

    bool _requestRebuildLight {};
    bool _requestRenderLight {};
    uchar* _hexLight {};
    uint _lightPointsCount {};
    vector<PrimitivePoints> _lightPoints {};
    PrimitivePoints _lightSoftPoints {};
    vector<LightSource> _lightSources {};
    vector<LightSource> _lightSourcesScen {};
    int _lightCapacity {};
    int _lightMinHx {};
    int _lightMaxHx {};
    int _lightMinHy {};
    int _lightMaxHy {};
    int _lightProcentR {};
    int _lightProcentG {};
    int _lightProcentB {};
    int _roofSkip {};
    int _drawCursorX {};
    AnyFrames* _cursorPrePic {};
    AnyFrames* _cursorPostPic {};
    AnyFrames* _cursorXPic {};
    int _cursorX {};
    int _cursorY {};
    set<hstring> _fastPids {};
    set<hstring> _ignorePids {};
};
