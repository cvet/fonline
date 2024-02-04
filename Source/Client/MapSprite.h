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

#include "Entity.h"

constexpr auto SPRITES_POOL_GROW_SIZE = 10000;

class RenderEffect;
class SpriteManager;
class MapSpriteList;
class MapSprite;
class Sprite;

///@ ExportEnum
enum class DrawOrderType : uint8
{
    Tile = 0,
    Tile1 = 1,
    Tile2 = 2,
    Tile3 = 3,
    Tile4 = 4,
    HexGrid = 5,
    FlatScenery = 8,
    Ligth = 9,
    DeadCritter = 10,
    FlatItem = 13,
    Track = 16,

    NormalBegin = 20,
    Scenery = 23,
    Item = 26,
    Critter = 29,
    Particles = 30,
    NormalEnd = 32,

    Roof = 33,
    Roof1 = 34,
    Roof2 = 35,
    Roof3 = 36,
    Roof4 = 37,
    RoofParticles = 38,
    Last = 39,
};

///@ ExportEnum
enum class ContourType : uint8
{
    None,
    Red,
    Yellow,
    Custom,
};

///@ ExportEnum
enum class EggAppearenceType : uint8
{
    None,
    Always,
    ByX,
    ByY,
    ByXAndY,
    ByXOrY,
};

///@ ExportRefType Client HasFactory
struct MapSpriteData
{
    SCRIPTABLE_OBJECT_BEGIN();

    bool Valid {};
    uint SprId {};
    mpos Hex {};
    hstring ProtoId {};
    ipos Offset {};
    bool IsFlat {};
    bool NoLight {};
    DrawOrderType DrawOrder {};
    int DrawOrderHyOffset {};
    CornerType Corner {};
    bool DisableEgg {};
    ucolor Color {};
    ucolor ContourColor {};
    bool IsTweakOffs {};
    ipos TweakOffset {};
    bool IsTweakAlpha {};
    uint8 TweakAlpha {};

    SCRIPTABLE_OBJECT_END();
};
static_assert(std::is_standard_layout_v<MapSpriteData>);

class MapSprite final
{
public:
    MapSprite() = default;
    MapSprite(const MapSprite&) = delete;
    MapSprite(MapSprite&&) noexcept = delete;
    auto operator=(const MapSprite&) = delete;
    auto operator=(MapSprite&&) noexcept = delete;
    ~MapSprite() = default;

    [[nodiscard]] auto GetDrawRect() const -> IRect;
    [[nodiscard]] auto GetViewRect() const -> IRect;
    [[nodiscard]] auto CheckHit(ipos pos, bool check_transparent) const -> bool;

    void Invalidate();
    void SetEggAppearence(EggAppearenceType egg_appearence);
    void SetContour(ContourType contour);
    void SetContour(ContourType contour, ucolor color);
    void SetColor(ucolor color);
    void SetAlpha(const uint8* alpha);
    void SetFixedAlpha(uint8 alpha);
    void SetLight(CornerType corner, const uint8* light, msize size);

    // Todo:: incapsulate all sprite data
    MapSpriteList* Root {};
    DrawOrderType DrawOrder {};
    uint DrawOrderPos {};
    size_t TreeIndex {};
    const Sprite* Spr {};
    const Sprite* const* PSpr {};
    mpos Hex {};
    ipos HexOffset {};
    const ipos* PHexOffset {};
    const ipos* SprOffset {};
    const uint8* Alpha {};
    const uint8* Light {};
    const uint8* LightRight {};
    const uint8* LightLeft {};
    EggAppearenceType EggAppearence {};
    ContourType Contour {};
    ucolor ContourColor {};
    ucolor Color {};
    RenderEffect** DrawEffect {};
    bool* ValidCallback {};
    bool Valid {};
    MapSpriteData* MapSpr {};

    MapSprite** ExtraChainRoot {};
    MapSprite* ExtraChainParent {};
    MapSprite* ExtraChainChild {};
    MapSprite** ChainRoot {};
    MapSprite** ChainLast {};
    MapSprite* ChainParent {};
    MapSprite* ChainChild {};
};

class MapSpriteList final
{
    friend class MapSprite;

public:
    MapSpriteList() = delete;
    explicit MapSpriteList(SpriteManager& spr_mngr);
    MapSpriteList(const MapSpriteList&) = delete;
    MapSpriteList(MapSpriteList&&) noexcept = delete;
    auto operator=(const MapSpriteList&) = delete;
    auto operator=(MapSpriteList&&) noexcept = delete;
    ~MapSpriteList();

    [[nodiscard]] auto RootSprite() -> MapSprite*;

    auto AddSprite(DrawOrderType draw_order, mpos hex, ipos hex_offset, const ipos* phex_offset, const Sprite* spr, const Sprite* const* pspr, const ipos* spr_offset, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&;
    auto InsertSprite(DrawOrderType draw_order, mpos hex, ipos hex_offset, const ipos* phex_offset, const Sprite* spr, const Sprite* const* pspr, const ipos* spr_offset, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&;
    void Invalidate();
    void Sort();

private:
    auto PutSprite(MapSprite* child, DrawOrderType draw_order, mpos hex, ipos hex_offset, const ipos* phex_offset, const Sprite* spr, const Sprite* const* pspr, const ipos* spr_offset, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&;
    void GrowPool();

    SpriteManager& _sprMngr;
    vector<MapSprite*> _spritesPool {};
    MapSprite* _rootSprite {};
    MapSprite* _lastSprite {};
    uint _spriteCount {};
    vector<MapSprite*> _sortSprites {};
    vector<MapSprite*> _invalidatedSprites {};
    bool _nonConstHelper {};
};
