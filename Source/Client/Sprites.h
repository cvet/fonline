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

constexpr auto SPRITES_POOL_GROW_SIZE = 10000;

class RenderEffect;
class SpriteManager;
class Sprites;
class Sprite;

///@ ExportEnum
enum class DrawOrderType : uint8
{
    Tile = 0,
    Tile1 = 1,
    Tile2 = 2,
    Tile3 = 3,
    TileEnd = 4,
    HexGrid = 5,
    FlatScenery = 8,
    Ligth = 9,
    DeadCritter = 10,
    FlatItem = 13,
    Track = 16,
    Normal = 20,
    Scenery = 23,
    Item = 26,
    Critter = 29,
    Rain = 32,
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

///@ ExportObject Client
struct MapSprite
{
    SCRIPTABLE_OBJECT();
    bool Valid {};
    uint SprId {};
    uint16 HexX {};
    uint16 HexY {};
    hstring ProtoId {};
    int FrameIndex {};
    int OffsX {};
    int OffsY {};
    bool IsFlat {};
    bool NoLight {};
    DrawOrderType DrawOrder {};
    int DrawOrderHyOffset {};
    CornerType Corner {};
    bool DisableEgg {};
    uint Color {};
    uint ContourColor {};
    bool IsTweakOffs {};
    int TweakOffsX {};
    int TweakOffsY {};
    bool IsTweakAlpha {};
    uint8 TweakAlpha {};
};

class Sprite
{
public:
    Sprite() = default;
    Sprite(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept = delete;
    auto operator=(const Sprite&) = delete;
    auto operator=(Sprite&&) noexcept = delete;
    ~Sprite() = default;

    [[nodiscard]] auto GetIntersected(int ox, int oy, bool check_transparent) -> Sprite*;

    void Unvalidate();
    void SetEggAppearence(EggAppearenceType egg_appearence);
    void SetContour(ContourType contour);
    void SetContour(ContourType contour, uint color);
    void SetColor(uint color);
    void SetAlpha(uint8* alpha);
    void SetLight(CornerType corner, uint8* light, uint16 maxhx, uint16 maxhy);
    void SetFixedAlpha(uint8 alpha);

    // Todo:: incapsulate all sprite data
    Sprites* Root {};
    DrawOrderType DrawOrder {};
    uint DrawOrderPos {};
    uint TreeIndex {};
    uint SprId {};
    uint* PSprId {};
    uint16 HexX {};
    uint16 HexY {};
    int ScrX {};
    int ScrY {};
    int* PScrX {};
    int* PScrY {};
    int* OffsX {};
    int* OffsY {};
    Sprite* Parent {};
    Sprite* Child {};
    uint8* Alpha {};
    uint8* Light {};
    uint8* LightRight {};
    uint8* LightLeft {};
    EggAppearenceType EggAppearence {};
    ContourType Contour {};
    uint ContourColor {};
    uint Color {};
    RenderEffect** DrawEffect {};
    bool* ValidCallback {};
    bool Valid {};
    MapSprite* MapSpr {};
    Sprite** ExtraChainRoot {};
    Sprite* ExtraChainParent {};
    Sprite* ExtraChainChild {};
    Sprite** ChainRoot {};
    Sprite** ChainLast {};
    Sprite* ChainParent {};
    Sprite* ChainChild {};
};

class Sprites final
{
    friend class Sprite;

public:
    Sprites() = delete;
    Sprites(SpriteManager& spr_mngr, vector<Sprite*>& pool) :
        _sprMngr {spr_mngr},
        _spritesPool {pool}
    {
    }
    Sprites(const Sprites&) = delete;
    Sprites(Sprites&&) noexcept = delete;
    auto operator=(const Sprites&) = delete;
    auto operator=(Sprites&&) noexcept = delete;
    ~Sprites() = default;

    [[nodiscard]] auto RootSprite() -> Sprite*;
    [[nodiscard]] auto Size() const -> uint;

    [[nodiscard]] auto AddSprite(DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, int* ox, int* oy, uint8* alpha, RenderEffect** effect, bool* callback) -> Sprite&;
    [[nodiscard]] auto InsertSprite(DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, int* ox, int* oy, uint8* alpha, RenderEffect** effect, bool* callback) -> Sprite&;

    void Unvalidate();
    void SortByMapPos();
    void Clear();

private:
    [[nodiscard]] auto PutSprite(Sprite* child, DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, int* ox, int* oy, uint8* alpha, RenderEffect** effect, bool* callback) -> Sprite&;

    void GrowPool();

    SpriteManager& _sprMngr;
    vector<Sprite*>& _spritesPool;
    Sprite* _rootSprite {};
    Sprite* _lastSprite {};
    uint _spriteCount {};
    vector<Sprite*> _unvalidatedSprites {};
    bool _nonConstHelper {};
};
