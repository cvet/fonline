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

#pragma once

#include "Common.h"

#include "Settings.h"

constexpr auto SPRITES_POOL_GROW_SIZE = 10000;

class RenderEffect;
class MapSprite;
class SpriteManager;
class Sprites;
class Sprite;
using SpriteVec = vector<Sprite*>;

class Sprite
{
public:
    Sprite() = default;
    Sprite(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept = delete;
    auto operator=(const Sprite&) = delete;
    auto operator=(Sprite&&) noexcept = delete;
    ~Sprite() = default;

    [[nodiscard]] auto GetIntersected(int ox, int oy) -> Sprite*;

    void Unvalidate();
    void SetEgg(int egg);
    void SetContour(int contour);
    void SetContour(int contour, uint color);
    void SetColor(uint color);
    void SetAlpha(uchar* alpha);
    void SetFlash(uint mask);
    void SetLight(int corner, uchar* light, int maxhx, int maxhy);
    void SetFixedAlpha(uchar alpha);

    // Todo:: incapsulate all sprite data
    Sprites* Root {};
    int DrawOrderType {};
    uint DrawOrderPos {};
    uint TreeIndex {};
    uint SprId {};
    uint* PSprId {};
    int HexX {};
    int HexY {};
    int ScrX {};
    int ScrY {};
    int* PScrX {};
    int* PScrY {};
    short* OffsX {};
    short* OffsY {};
    int CutType {};
    Sprite* Parent {};
    Sprite* Child {};
    float CutX {};
    float CutW {};
    float CutTexL {};
    float CutTexR {};
    uchar* Alpha {};
    uchar* Light {};
    uchar* LightRight {};
    uchar* LightLeft {};
    int EggType {};
    int ContourType {};
    uint ContourColor {};
    uint Color {};
    uint FlashMask {};
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
    int CutOyL {};
    int CutOyR {};
};

class Sprites final
{
    friend class Sprite;

public:
    Sprites() = delete;
    Sprites(HexSettings& settings, SpriteManager& spr_mngr) : _settings {settings}, _sprMngr(spr_mngr) { }
    Sprites(const Sprites&) = delete;
    Sprites(Sprites&&) noexcept = delete;
    auto operator=(const Sprites&) = delete;
    auto operator=(Sprites&&) noexcept = delete;
    ~Sprites() = default;

    static void GrowPool();

    [[nodiscard]] auto RootSprite() -> Sprite*;
    [[nodiscard]] auto Size() const -> uint;

    [[nodiscard]] auto AddSprite(int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback) -> Sprite&;
    [[nodiscard]] auto InsertSprite(int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback) -> Sprite&;

    void Unvalidate();
    void SortByMapPos();
    void Clear();

private:
    [[nodiscard]] auto PutSprite(Sprite* child, int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback) -> Sprite&;

    static SpriteVec _spritesPool;
    HexSettings& _settings;
    SpriteManager& _sprMngr;
    Sprite* _rootSprite {};
    Sprite* _lastSprite {};
    uint _spriteCount {};
    SpriteVec _unvalidatedSprites {};
    bool _nonConstHelper {};
};
