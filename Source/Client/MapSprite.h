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

#include "Geometry.h"

FO_BEGIN_NAMESPACE();

constexpr auto SPRITES_POOL_GROW_SIZE = 2000;

class RenderEffect;
class Sprite;
class MapSpriteList;

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

class MapSprite final
{
    friend class MapSpriteList;

public:
    MapSprite() noexcept = default;
    MapSprite(const MapSprite&) = delete;
    MapSprite(MapSprite&&) noexcept = delete;
    auto operator=(const MapSprite&) = delete;
    auto operator=(MapSprite&&) noexcept = delete;
    ~MapSprite();

    [[nodiscard]] auto IsValid() const noexcept -> bool { return !!_owner; }
    [[nodiscard]] auto GetDrawOrder() const noexcept -> DrawOrderType { return _drawOrder; }
    [[nodiscard]] auto GetSortValue() const noexcept -> uint32 { return _index; }
    [[nodiscard]] auto GetDrawRect() const noexcept -> irect32;
    [[nodiscard]] auto GetViewRect() const noexcept -> irect32;
    [[nodiscard]] auto IsHidden() const noexcept -> bool { return _hidden; }
    [[nodiscard]] auto GetSprite() const noexcept -> const Sprite* { return _pSpr ? *_pSpr.get() : _spr.get(); }
    [[nodiscard]] auto GetHex() const noexcept -> mpos { return _hex; }
    [[nodiscard]] auto GetHexOffset() const noexcept -> ipos32 { return _hexOffset; }
    [[nodiscard]] auto GetPHexOffset() const noexcept -> const ipos32* { return _pHexOffset.get(); }
    [[nodiscard]] auto GetPSprOffset() const noexcept -> const ipos32* { return _pSprOffset.get(); }
    [[nodiscard]] auto GetAlpha() const noexcept -> const uint8* { return _alpha.get(); }
    [[nodiscard]] auto GetLight() const noexcept -> const ucolor* { return _light.get(); }
    [[nodiscard]] auto GetLightRight() const noexcept -> const ucolor* { return _lightRight.get(); }
    [[nodiscard]] auto GetLightLeft() const noexcept -> const ucolor* { return _lightLeft.get(); }
    [[nodiscard]] auto GetEggAppearence() const noexcept -> EggAppearenceType { return _eggAppearence; }
    [[nodiscard]] auto GetContour() const noexcept -> ContourType { return _contour; }
    [[nodiscard]] auto GetContourColor() const noexcept -> ucolor { return _contourColor; }
    [[nodiscard]] auto GetColor() const noexcept -> ucolor { return _color; }
    [[nodiscard]] auto GetDrawEffect() const noexcept -> RenderEffect** { return _drawEffect.get(); }

    void Invalidate() noexcept;
    void SetEggAppearence(EggAppearenceType egg_appearence) noexcept;
    void SetContour(ContourType contour) noexcept;
    void SetContour(ContourType contour, ucolor color) noexcept;
    void SetColor(ucolor color) noexcept;
    void SetAlpha(const uint8* alpha) noexcept;
    void SetFixedAlpha(uint8 alpha) noexcept;
    void SetLight(CornerType corner, const ucolor* light, msize size) noexcept;
    void SetHidden(bool hidden) noexcept;
    void CreateExtraChain(MapSprite** mspr);
    void AddToExtraChain(MapSprite* mspr);

private:
    void Reset() noexcept;

    raw_ptr<MapSpriteList> _owner {};
    uint32 _drawOrderPos {};
    uint32 _globalPos {};
    uint32 _index {};
    DrawOrderType _drawOrder {};
    bool _hidden {};
    raw_ptr<bool> _validCallback {};
    raw_ptr<const Sprite> _spr {};
    raw_ptr<const Sprite*> _pSpr {};
    mpos _hex {};
    ipos32 _hexOffset {};
    raw_ptr<const ipos32> _pHexOffset {};
    raw_ptr<const ipos32> _pSprOffset {};
    raw_ptr<const uint8> _alpha {};
    raw_ptr<const ucolor> _light {};
    raw_ptr<const ucolor> _lightRight {};
    raw_ptr<const ucolor> _lightLeft {};
    EggAppearenceType _eggAppearence {};
    ContourType _contour {};
    ucolor _contourColor {};
    ucolor _color {};
    mutable raw_ptr<RenderEffect*> _drawEffect {};
    raw_ptr<MapSprite*> _extraChainRoot {};
    raw_ptr<MapSprite> _extraChainParent {};
    raw_ptr<MapSprite> _extraChainChild {};
};

class MapSpriteList final
{
    friend class MapSprite;

public:
    MapSpriteList() noexcept = default;
    MapSpriteList(const MapSpriteList&) = delete;
    MapSpriteList(MapSpriteList&&) noexcept = delete;
    auto operator=(const MapSpriteList&) = delete;
    auto operator=(MapSpriteList&&) noexcept = delete;
    ~MapSpriteList() = default;

    [[nodiscard]] auto HasActiveSprites() const noexcept { return !_activeSprites.empty(); }
    [[nodiscard]] auto GetActiveSprites() noexcept -> const vector<unique_ptr<MapSprite>>& { return _activeSprites; }

    auto AddSprite(DrawOrderType draw_order, mpos hex, ipos32 hex_offset, const ipos32* phex_offset, const Sprite* spr, const Sprite** pspr, const ipos32* spr_offset, const uint8* alpha, RenderEffect** effect, bool* callback) noexcept -> MapSprite*;
    void InvalidateAll() noexcept;
    void SortIfNeeded() noexcept;

private:
    void GrowPool() noexcept;
    void Invalidate(MapSprite* mspr) noexcept;

    vector<unique_ptr<MapSprite>> _activeSprites {};
    vector<unique_ptr<MapSprite>> _spritesPool {};
    uint32 _globalCounter {};
    bool _needSort {};
};

///@ ExportRefType Client HasFactory
struct MapSpriteHolder
{
    MapSpriteHolder() = default;
    MapSpriteHolder(const MapSpriteHolder&) = delete;
    MapSpriteHolder(MapSpriteHolder&&) noexcept = delete;
    auto operator=(const MapSpriteHolder&) = delete;
    auto operator=(MapSpriteHolder&&) noexcept = delete;
    ~MapSpriteHolder();

    FO_SCRIPTABLE_OBJECT_BEGIN();

    bool Valid {};
    uint32 SprId {};
    mpos Hex {};
    hstring ProtoId {};
    ipos32 Offset {};
    bool IsFlat {};
    bool NoLight {};
    DrawOrderType DrawOrder {};
    int32 DrawOrderHyOffset {};
    CornerType Corner {};
    bool DisableEgg {};
    ucolor Color {};
    ucolor ContourColor {};
    bool IsTweakOffs {};
    ipos32 TweakOffset {};
    bool IsTweakAlpha {};
    uint8 TweakAlpha {};

    void StopDraw();

    FO_SCRIPTABLE_OBJECT_END();

    raw_ptr<MapSprite> MSpr {};
};
static_assert(std::is_standard_layout_v<MapSpriteHolder>);

FO_END_NAMESPACE();
