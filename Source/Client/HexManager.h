//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: move HexManager to MapView?

#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "ClientScripting.h"
#include "CritterView.h"
#include "EffectManager.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "ItemHexView.h"
#include "ItemView.h"
#include "MapLoader.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "SpriteManager.h"
#include "Sprites.h"
#include "Timer.h"

static constexpr int MAX_FIND_PATH = 600;

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

    void AddItem(ItemHexView* item, ItemHexView* block_lines_item);
    void EraseItem(ItemHexView* item, ItemHexView* block_lines_item);
    auto AddTile(AnyFrames* anim, short ox, short oy, uchar layer, bool is_roof) -> Tile&;
    void EraseTile(uint index, bool is_roof);
    auto GetTilesCount(bool is_roof) -> uint;
    auto GetTile(uint index, bool is_roof) -> Tile&;
    void AddDeadCrit(CritterView* cr);
    void EraseDeadCrit(CritterView* cr);
    void ProcessCache();
    void AddSpriteToChain(Sprite* spr);
    void UnvalidateSpriteChain() const;

    bool IsView {};
    Sprite* SpriteChain {};
    CritterView* Crit {};
    CritterViewVec* DeadCrits {};
    int ScrX {};
    int ScrY {};
    AnyFrames* SimplyTile[2] {};
    TileVec* Tiles[2] {};
    ItemHexViewVec* Items {};
    ItemHexViewVec* BlockLinesItems {};
    short RoofNum {};
    FlagsType Flags {};
    uchar Corner {};
};

class HexManager final
{
public:
    HexManager() = delete;
    HexManager(bool mapper_mode, HexSettings& settings, ProtoManager& proto_mngr, SpriteManager& spr_mngr, EffectManager& effect_mngr, ResourceManager& res_mngr, ClientScriptSystem& script_sys, GameTimer& game_time);
    HexManager(const HexManager&) = delete;
    HexManager(HexManager&&) noexcept = delete;
    auto operator=(const HexManager&) = delete;
    auto operator=(HexManager&&) noexcept = delete;
    ~HexManager();

    void ResizeField(ushort w, ushort h);
    [[nodiscard]] auto GetField(ushort hx, ushort hy) -> Field& { return _hexField[hy * _maxHexX + hx]; }
    [[nodiscard]] auto IsHexToDraw(ushort hx, ushort hy) const -> bool { return _hexField[hy * _maxHexX + hx].IsView; }
    [[nodiscard]] auto GetHexTrack(ushort hx, ushort hy) -> char& { return _hexTrack[hy * _maxHexX + hx]; }
    [[nodiscard]] auto GetWidth() const -> ushort { return _maxHexX; }
    [[nodiscard]] auto GetHeight() const -> ushort { return _maxHexY; }
    void ClearHexTrack();
    void SwitchShowTrack();
    [[nodiscard]] auto IsShowTrack() const -> bool { return _isShowTrack; }

    auto FindPath(CritterView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, UCharVec& steps, int cut) -> bool;
    auto CutPath(CritterView* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, int cut) -> bool;
    auto TraceBullet(ushort hx, ushort hy, ushort tx, ushort ty, uint dist, float angle, CritterView* find_cr, bool find_cr_safe, CritterViewVec* critters, int find_type, UShortPair* pre_block, UShortPair* block, UShortPairVec* steps, bool check_passed) -> bool;
    void FindSetCenter(int cx, int cy);

    [[nodiscard]] auto IsMapLoaded() const -> bool { return _curPidMap != 0; }
    auto LoadMap(CacheStorage& cache, hash map_pid) -> bool;
    void UnloadMap();
    void GetMapHash(CacheStorage& cache, hash map_pid, hash& hash_tiles, hash& hash_scen) const;
    void GenerateItem(uint id, hash proto_id, Properties& props);
    [[nodiscard]] auto GetDayTime() -> int;
    [[nodiscard]] auto GetMapTime() -> int;
    [[nodiscard]] auto GetMapDayTime() -> int*;
    [[nodiscard]] auto GetMapDayColor() -> uchar*;
    void OnResolutionChanged();

    void ReloadSprites();

    void ChangeZoom(int zoom); // <0 in, >0 out, 0 normalize
    void GetScreenHexes(int& sx, int& sy) const;
    void GetHexCurrentPosition(ushort hx, ushort hy, int& x, int& y) const;
    auto ProcessHexBorders(ItemHexView* item) -> bool;

    void RebuildMap(int rx, int ry);
    void RebuildMapOffset(int ox, int oy);
    void DrawMap();
    void SetFog(PointVec& look_points, PointVec& shoot_points, short* offs_x, short* offs_y);
    [[nodiscard]] auto GetDrawTree() -> Sprites& { return _mainTree; }
    void RefreshMap() { RebuildMap(_screenHexX, _screenHexY); }

private:
    [[nodiscard]] auto GetViewWidth() const -> int;
    [[nodiscard]] auto GetViewHeight() const -> int;

    void PrepareFogToDraw();
    void InitView(int cx, int cy);
    void ResizeView();
    [[nodiscard]] auto IsVisible(uint spr_id, int ox, int oy) -> bool;
    auto ProcessHexBorders(uint spr_id, int ox, int oy, bool resize_map) -> bool;

    HexSettings& _settings;
    GeometryHelper _geomHelper;
    ProtoManager& _protoMngr;
    SpriteManager& _sprMngr;
    EffectManager& _effectMngr;
    ResourceManager& _resMngr;
    ClientScriptSystem& _scriptSys;
    GameTimer& _gameTime;

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

    hash _curPidMap {};
    int _curMapTime {-1};
    int _dayTime[4] {};
    uchar _dayColor[12] {};
    hash _curHashTiles {};
    hash _curHashScen {};

    bool _mapperMode {};
    RenderTarget* _rtMap {};
    RenderTarget* _rtLight {};
    RenderTarget* _rtFog {};
    uint _rtScreenOx {};
    uint _rtScreenOy {};
    Sprites _mainTree;
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
    PointVec _fogLookPoints {};
    PointVec _fogShootPoints {};

    // Scroll
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

    auto Scroll() -> bool;
    void ScrollToHex(int hx, int hy, float speed, bool can_stop);
    void ScrollOffset(int ox, int oy, float speed, bool can_stop);

    AutoScrollInfo AutoScroll {};

    // Weather
    void SwitchShowHex();
    void SwitchShowRain();
    void SetWeather(int time, uchar rain);

private:
    auto ScrollCheckPos(int (&positions)[4], int dir1, int dir2) -> bool;
    auto ScrollCheck(int xmod, int ymod) -> bool;

    // Critters
public:
    void SetCritter(CritterView* cr);
    auto TransitCritter(CritterView* cr, int hx, int hy, bool animate, bool force) -> bool;
    auto GetCritter(uint crid) -> CritterView*;
    auto GetChosen() -> CritterView*;
    void AddCritter(CritterView* cr);
    void RemoveCritter(CritterView* cr);
    void DeleteCritter(uint crid);
    void DeleteCritters();
    void GetCritters(ushort hx, ushort hy, CritterViewVec& crits, int find_type);
    auto GetCritters() -> CritterViewMap& { return _allCritters; }
    void SetCritterContour(uint crid, int contour);
    void SetCrittersContour(int contour);
    void SetMultihex(ushort hx, ushort hy, uint multihex, bool set);

private:
    CritterViewMap _allCritters {};
    uint _chosenId {};
    uint _critterContourCrId {};
    int _critterContour {};
    int _crittersContour {};

    // Items
public:
    auto AddItem(uint id, hash pid, ushort hx, ushort hy, bool is_added, UCharVecVec* data) -> uint;
    void FinishItem(uint id, bool is_deleted);
    void DeleteItem(ItemHexView* item, bool destroy_item, ItemHexViewVec::iterator* it_hex_items);
    void PushItem(ItemHexView* item);
    auto GetItem(ushort hx, ushort hy, hash pid) -> ItemHexView*;
    auto GetItemById(ushort hx, ushort hy, uint id) -> ItemHexView*;
    auto GetItemById(uint id) -> ItemHexView*;
    void GetItems(ushort hx, ushort hy, ItemHexViewVec& items);
    auto GetItems() -> ItemHexViewVec& { return _hexItems; }
    auto GetRectForText(ushort hx, ushort hy) -> Rect;
    void ProcessItems();
    void SkipItemsFade();

private:
    void AddFieldItem(ushort hx, ushort hy, ItemHexView* item);
    void EraseFieldItem(ushort hx, ushort hy, ItemHexView* item);

    ItemHexViewVec _hexItems {};

    // Light
public:
    void ClearHexLight();
    [[nodiscard]] auto GetLightHex(ushort hx, ushort hy) -> uchar* { return &_hexLight[hy * _maxHexX * 3 + hx * 3]; }
    void RebuildLight() { _requestRebuildLight = _requestRenderLight = true; }
    auto GetLights() -> vector<LightSource>& { return _lightSources; }

private:
    void PrepareLightToDraw();
    void MarkLight(ushort hx, ushort hy, uint inten);
    void MarkLightEndNeighbor(ushort hx, ushort hy, bool north_south, uint inten);
    void MarkLightEnd(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten);
    void MarkLightStep(ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten);
    void TraceLight(ushort from_hx, ushort from_hy, ushort& hx, ushort& hy, int dist, uint inten);
    void ParseLightTriangleFan(LightSource& ls);
    void RealRebuildLight();
    void CollectLightSources();

    bool _requestRebuildLight {};
    bool _requestRenderLight {};
    uchar* _hexLight {};
    uint _lightPointsCount {};
    vector<PointVec> _lightPoints {};
    PointVec _lightSoftPoints {};
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

public:
    void RebuildTiles();
    void RebuildRoof();
    void SetSkipRoof(int hx, int hy);
    void MarkRoofNum(int hx, int hy, int num);
    auto GetHexPixel(int x, int y, ushort& hx, ushort& hy) -> bool;
    auto GetItemPixel(int x, int y, bool& item_egg) -> ItemHexView*; // With transparent egg
    auto GetCritterPixel(int x, int y, bool ignore_dead_and_chosen) -> CritterView*;
    void GetSmthPixel(int x, int y, ItemHexView*& item, CritterView*& cr);
    auto RunEffect(hash eff_pid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) -> bool;
    void ProcessRain();
    void SetRainAnimation(const char* fall_anim_name, const char* drop_anim_name);
    void SetCursorPos(int x, int y, bool show_steps, bool refresh);
    void DrawCursor(uint spr_id);
    void DrawCursor(const char* text);

private:
    struct Drop
    {
        uint CurSprId {};
        short OffsX {};
        short OffsY {};
        short GroundOffsY {};
        short DropCnt {};
    };

    auto CheckTilesBorder(Field::Tile& tile, bool is_roof) -> bool;

    Sprites _tilesTree;
    Sprites _roofTree;
    int _roofSkip {};
    vector<Drop*> _rainData {};
    int _rainCapacity {};
    string _picRainFallName {};
    string _picRainDropName {};
    AnyFrames* _picRainFall {};
    AnyFrames* _picRainDrop {};
    Sprites _roofRainTree;
    uint _rainLastTick {};

    int _drawCursorX {};
    AnyFrames* _cursorPrePic {};
    AnyFrames* _cursorPostPic {};
    AnyFrames* _cursorXPic {};
    int _cursorX {};
    int _cursorY {};

    // Editor stuff
public:
    using TileVecVec = vector<MapTileVec>;

    auto SetProtoMap(ProtoMap& pmap) -> bool;
    void GetProtoMap(ProtoMap& pmap);
    auto GetTiles(ushort hx, ushort hy, bool is_roof) -> MapTileVec&;
    void ClearSelTiles();
    void ParseSelTiles();
    void SetTile(hash name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof, bool select);
    void EraseTile(ushort hx, ushort hy, uchar layer, bool is_roof, uint skip_index);
    void AddFastPid(hash pid);
    auto IsFastPid(hash pid) -> bool;
    void ClearFastPids();
    void AddIgnorePid(hash pid);
    void SwitchIgnorePid(hash pid);
    auto IsIgnorePid(hash pid) -> bool;
    void ClearIgnorePids();
    void GetHexesRect(const Rect& rect, UShortPairVec& hexes);
    void MarkPassedHexes();

    uchar SelectAlpha {100};
    ProtoMap* CurProtoMap {};
    TileVecVec TilesField {};
    TileVecVec RoofsField {};

private:
    HashSet _fastPids {};
    HashSet _ignorePids {};
};
