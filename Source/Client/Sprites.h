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

#define SPRITES_POOL_GROW_SIZE (10000)

class RenderEffect;
class MapSprite;
class SpriteManager;
class Sprites;
class Sprite;
using SpriteVec = vector<Sprite*>;

class Sprite
{
public:
    Sprites* Root;
    int DrawOrderType;
    uint DrawOrderPos;
    uint TreeIndex;
    uint SprId;
    uint* PSprId;
    int HexX, HexY;
    int ScrX, ScrY;
    int *PScrX, *PScrY;
    short *OffsX, *OffsY;
    int CutType;
    Sprite *Parent, *Child;
    float CutX, CutW, CutTexL, CutTexR;
    uchar* Alpha;
    uchar* Light;
    uchar* LightRight;
    uchar* LightLeft;
    int EggType;
    int ContourType;
    uint ContourColor;
    uint Color;
    uint FlashMask;
    RenderEffect** DrawEffect;
    bool* ValidCallback;
    bool Valid;
    MapSprite* MapSpr;
    Sprite** ExtraChainRoot;
    Sprite* ExtraChainParent;
    Sprite* ExtraChainChild;
    Sprite** ChainRoot;
    Sprite** ChainLast;
    Sprite* ChainParent;
    Sprite* ChainChild;
    int CutOyL, CutOyR;

    Sprite();
    void Unvalidate();
    Sprite* GetIntersected(int ox, int oy);

    void SetEgg(int egg);
    void SetContour(int contour);
    void SetContour(int contour, uint color);
    void SetColor(uint color);
    void SetAlpha(uchar* alpha);
    void SetFlash(uint mask);
    void SetLight(int corner, uchar* light, int maxhx, int maxhy);
    void SetFixedAlpha(uchar alpha);
};

class Sprites
{
    friend class Sprite;

public:
    static void GrowPool();
    Sprites(HexSettings& sett, SpriteManager& spr_mngr);
    ~Sprites();
    Sprite* RootSprite();
    Sprite& AddSprite(int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr,
        short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback);
    Sprite& InsertSprite(int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr,
        short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback);
    void Unvalidate();
    void SortByMapPos();
    uint Size();
    void Clear();

private:
    static SpriteVec spritesPool;
    HexSettings& settings;
    SpriteManager& sprMngr;
    Sprite* rootSprite;
    Sprite* lastSprite;
    uint spriteCount;
    SpriteVec unvalidatedSprites;

    Sprite& PutSprite(Sprite* child, int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id,
        uint* id_ptr, short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback);
};

class MapSprite : public NonCopyable
{
public:
    void AddRef() const { ++RefCount; }
    void Release() const
    {
        if (--RefCount == 0)
            delete this;
    }
    static MapSprite* Factory() { return new MapSprite(); }

    mutable int RefCount {1};
    bool Valid {};
    uint SprId {};
    ushort HexX {};
    ushort HexY {};
    hash ProtoId {};
    int FrameIndex {};
    int OffsX {};
    int OffsY {};
    bool IsFlat {};
    bool NoLight {};
    int DrawOrder {};
    int DrawOrderHyOffset {};
    int Corner {};
    bool DisableEgg {};
    uint Color {};
    uint ContourColor {};
    bool IsTweakOffs {};
    short TweakOffsX {};
    short TweakOffsY {};
    bool IsTweakAlpha {};
    uchar TweakAlpha {};
};
