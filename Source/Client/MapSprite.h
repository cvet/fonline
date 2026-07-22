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

#include "Geometry.h"

FO_BEGIN_NAMESPACE

constexpr auto SPRITES_POOL_GROW_SIZE = 2000;

class RenderEffect;
class Sprite;
class MapSpriteList;

///@ ExportEnum
enum class DrawOrderType : uint8_t
{
    // Flat sprites pre-light
    Tile = 0,
    Tile1 = 1,
    Tile2 = 2,
    Tile3 = 3,
    Tile4 = 4,
    FlatItemPreLight = 6,
    HexGrid = 8,
    // Light primitives
    PreLight = 9,
    Light = 10,
    AfterLight = 11,
    // Flat sprites post-light
    DeadCritter = 13,
    FlatItemAfterLight = 16,
    FlatEnd = 18,
    // Normal sprites
    NormalBegin = 19,
    Item = 22,
    Critter = 25,
    Particles = 28,
    NormalEnd = 31,
    // Roof sprites
    Roof = 33,
    Roof1 = 34,
    Roof2 = 35,
    Roof3 = 36,
    Roof4 = 37,
    RoofParticles = 38,
    // Count: 40
    Last = 39,
};

///@ ExportEnum
enum class EggAppearenceType : uint8_t
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
    [[nodiscard]] auto GetSortValue() const noexcept -> uint32_t { return _index; }
    [[nodiscard]] auto GetDrawRect() const noexcept -> irect32;
    [[nodiscard]] auto GetDrawRootPos() const noexcept -> ipos32;
    [[nodiscard]] auto GetMapRootOffset() const noexcept -> ipos32;
    [[nodiscard]] auto GetSpriteRootOffset() const noexcept -> ipos32;
    [[nodiscard]] auto GetViewRect() const noexcept -> irect32;
    [[nodiscard]] auto IsHidden() const noexcept -> bool { return _hidden; }
    [[nodiscard]] auto GetSprite() const noexcept -> nptr<const Sprite> { return _pSpr ? *_pSpr : _spr; }
    [[nodiscard]] auto GetHex() const noexcept -> mpos { return _hex; }
    [[nodiscard]] auto GetHexOffset() const noexcept -> ipos32 { return _hexOffset; }
    [[nodiscard]] auto GetPHexOffset() const noexcept -> nptr<const ipos32> { return _pHexOffset; }
    [[nodiscard]] auto GetPSprOffset() const noexcept -> nptr<const ipos32> { return _pSprOffset; }
    [[nodiscard]] auto GetRootOffset() const noexcept -> ipos32 { return _pRootOffset ? *_pRootOffset : ipos32 {}; }
    [[nodiscard]] auto GetAlpha() const noexcept -> nptr<const uint8_t> { return _alpha; }
    [[nodiscard]] auto GetLight() const noexcept -> nptr<const ucolor> { return _light; }
    [[nodiscard]] auto GetLightRight() const noexcept -> nptr<const ucolor> { return _lightRight; }
    [[nodiscard]] auto GetLightLeft() const noexcept -> nptr<const ucolor> { return _lightLeft; }
    [[nodiscard]] auto GetEggAppearence() const noexcept -> EggAppearenceType { return _eggAppearence; }
    [[nodiscard]] auto GetColor() const noexcept -> ucolor { return _color; }
    [[nodiscard]] auto GetElevation() const noexcept -> int16_t { return _elevation; }
    [[nodiscard]] auto GetDrawEffect() const noexcept -> nptr<RenderEffect> { return _drawEffect ? *_drawEffect : nullptr; }
    [[nodiscard]] auto GetAngle() const noexcept -> int16_t { return _angle; }
    [[nodiscard]] auto GetMapProjected() const noexcept -> bool { return _mapProjected; }

    void Invalidate() noexcept;
    void SetEggAppearence(EggAppearenceType egg_appearence) noexcept;
    void SetColor(ucolor color) noexcept;
    void SetAlpha(nptr<const uint8_t> alpha) noexcept;
    void SetFixedAlpha(uint8_t alpha) noexcept;
    void SetLight(CornerType corner, ptr<const ucolor> light, msize size) noexcept;
    void SetHidden(bool hidden) noexcept;
    void SetElevation(int16_t elevation) noexcept;
    void SetAngle(int16_t angle) noexcept;
    void SetMapProjected(bool map_projected) noexcept;
    void CreateExtraChain(ptr<MapSprite*> mspr);
    void AddToExtraChain(ptr<MapSprite> mspr);

private:
    void Reset() noexcept;

    nptr<MapSpriteList> _owner {};
    uint64_t _drawOrderPos {};
    uint32_t _globalPos {};
    uint32_t _index {};
    DrawOrderType _drawOrder {};
    bool _hidden {};
    nptr<bool> _validCallback {};
    nptr<const Sprite> _spr {};
    nptr<const Sprite*> _pSpr {};
    mpos _hex {};
    ipos32 _hexOffset {};
    nptr<const ipos32> _pHexOffset {};
    nptr<const ipos32> _pSprOffset {};
    // Static logical-root offset (item proto Offset): the bottom-center→trunk vector. Kept separate from
    // _pSprOffset (which still positions the bitmap) so the depth/sort anchor can use the logical root, not the
    // bitmap bottom-center. Null for sprites without one (critters, particles).
    nptr<const ipos32> _pRootOffset {};
    nptr<const uint8_t> _alpha {};
    nptr<const ucolor> _light {};
    nptr<const ucolor> _lightRight {};
    nptr<const ucolor> _lightLeft {};
    EggAppearenceType _eggAppearence {};
    ucolor _color {};
    int16_t _elevation {};
    int16_t _angle {};
    bool _mapProjected {};
    mutable nptr<RenderEffect*> _drawEffect {};
    nptr<MapSprite*> _extraChainRoot {};
    nptr<MapSprite> _extraChainParent {};
    nptr<MapSprite> _extraChainChild {};
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
    [[nodiscard]] auto GetActiveSprites() const noexcept -> const_span<unique_ptr<MapSprite>> { return _activeSprites; }
    [[nodiscard]] auto GetDrawOrderRange(DrawOrderType from, DrawOrderType to) const -> pair<uint32_t, uint32_t>;

    auto AddSprite(DrawOrderType draw_order, mpos hex, ipos32 hex_offset, nptr<const ipos32> phex_offset, nptr<const Sprite> spr, nptr<const Sprite*> pspr, nptr<const ipos32> spr_offset, nptr<const ipos32> root_offset, nptr<const uint8_t> alpha, nptr<RenderEffect*> effect, nptr<bool> callback) noexcept -> ptr<MapSprite>;
    void InvalidateAll() noexcept;
    void SortIfNeeded() noexcept;

    static auto MakeDrawOrderPos(DrawOrderType draw_order, mpos hex) noexcept -> uint64_t;

private:
    void GrowPool() noexcept;
    void Invalidate(ptr<MapSprite> mspr) noexcept;

    vector<unique_ptr<MapSprite>> _activeSprites {};
    vector<unique_ptr<MapSprite>> _spritesPool {};
    uint32_t _globalCounter {};
    bool _needSort {};
    bool _orderBroken {};
    static constexpr size_t DrawOrderRangeSize = static_cast<size_t>(DrawOrderType::Last) + 2;
    array<uint32_t, DrawOrderRangeSize> _drawOrderRangeBegin {};
};

///@ ExportRefType Client RefCounted HasFactory Export = Valid, SprId, Hex, ProtoId, Offset, IsFlat, NoLight, DrawOrder, DrawOrderHyOffset, Corner, DisableEgg, Color, IsTweakOffs, TweakOffset, IsTweakAlpha, TweakAlpha, Angle, MapProjected, StopDraw
class MapSpriteHolder : public RefCounted<MapSpriteHolder>
{
public:
    MapSpriteHolder() = default;
    MapSpriteHolder(const MapSpriteHolder&) = delete;
    MapSpriteHolder(MapSpriteHolder&&) noexcept = delete;
    auto operator=(const MapSpriteHolder&) = delete;
    auto operator=(MapSpriteHolder&&) noexcept = delete;
    ~MapSpriteHolder();

    void StopDraw();

    bool Valid {};
    uint32_t SprId {};
    mpos Hex {};
    hstring ProtoId {};
    ipos32 Offset {};
    bool IsFlat {};
    bool NoLight {};
    DrawOrderType DrawOrder {DrawOrderType::Item};
    int32_t DrawOrderHyOffset {};
    CornerType Corner {};
    bool DisableEgg {};
    ucolor Color {};
    bool IsTweakOffs {};
    ipos32 TweakOffset {};
    bool IsTweakAlpha {};
    uint8_t TweakAlpha {};
    int16_t Angle {};
    bool MapProjected {};
    nptr<MapSprite> MSpr {};
};

FO_END_NAMESPACE
