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

#include "MapSprite.h"
#include "SpriteManager.h"

void MapSprite::Invalidate()
{
    STACK_TRACE_ENTRY();

    if (!Valid) {
        return;
    }
    Valid = false;

    if (ValidCallback != nullptr) {
        *ValidCallback = false;
        ValidCallback = nullptr;
    }

    if (ExtraChainRoot != nullptr) {
        *ExtraChainRoot = ExtraChainChild;
    }
    if (ExtraChainParent != nullptr) {
        ExtraChainParent->ExtraChainChild = ExtraChainChild;
    }
    if (ExtraChainChild != nullptr) {
        ExtraChainChild->ExtraChainParent = ExtraChainParent;
    }
    if (ExtraChainRoot != nullptr && ExtraChainChild != nullptr) {
        ExtraChainChild->ExtraChainRoot = ExtraChainRoot;
    }
    ExtraChainRoot = nullptr;
    ExtraChainParent = nullptr;
    ExtraChainChild = nullptr;

    if (MapSpr != nullptr) {
        // Todo: MapSprite releasing
        // MapSpr->Release();
        MapSpr = nullptr;
    }

    Root->_invalidatedSprites.push_back(this);

    if (ChainRoot != nullptr) {
        *ChainRoot = ChainChild;
    }
    if (ChainLast != nullptr) {
        *ChainLast = ChainParent;
    }
    if (ChainParent != nullptr) {
        ChainParent->ChainChild = ChainChild;
    }
    if (ChainChild != nullptr) {
        ChainChild->ChainParent = ChainParent;
    }
    if (ChainRoot != nullptr && ChainChild != nullptr) {
        ChainChild->ChainRoot = ChainRoot;
    }
    if (ChainLast != nullptr && ChainParent != nullptr) {
        ChainParent->ChainLast = ChainLast;
    }
    ChainRoot = nullptr;
    ChainLast = nullptr;
    ChainParent = nullptr;
    ChainChild = nullptr;

    Root = nullptr;
}

auto MapSprite::CheckHit(int ox, int oy, bool check_transparent) const -> bool
{
    STACK_TRACE_ENTRY();

    if (ox < 0 || oy < 0) {
        return false;
    }

    return !check_transparent || Root->_sprMngr.IsPixNoTransp(PSprId != nullptr ? *PSprId : SprId, ox, oy, true);
}

void MapSprite::SetEggAppearence(EggAppearenceType egg_appearence)
{
    STACK_TRACE_ENTRY();

    EggAppearence = egg_appearence;
}

void MapSprite::SetContour(ContourType contour)
{
    STACK_TRACE_ENTRY();

    Contour = contour;
}

void MapSprite::SetContour(ContourType contour, uint color)
{
    STACK_TRACE_ENTRY();

    Contour = contour;
    ContourColor = color;
}

void MapSprite::SetColor(uint color)
{
    STACK_TRACE_ENTRY();

    Color = color;
}

void MapSprite::SetAlpha(const uint8* alpha)
{
    STACK_TRACE_ENTRY();

    Alpha = alpha;
}

void MapSprite::SetFixedAlpha(uint8 alpha)
{
    STACK_TRACE_ENTRY();

    auto* color_alpha = reinterpret_cast<uint8*>(&Color) + 3;
    *color_alpha = alpha;
    Alpha = color_alpha;
}

void MapSprite::SetLight(CornerType corner, const uint8* light, uint16 maxhx, uint16 maxhy)
{
    STACK_TRACE_ENTRY();

    if (HexX >= 1 && HexX < maxhx - 1 && HexY >= 1 && HexY < maxhy - 1) {
        Light = &light[HexY * maxhx * 3 + HexX * 3];

        switch (corner) {
        case CornerType::EastWest:
        case CornerType::East:
            LightRight = Light - 3;
            LightLeft = Light + 3;
            break;
        case CornerType::NorthSouth:
        case CornerType::West:
            LightRight = Light + maxhx * 3;
            LightLeft = Light - maxhx * 3;
            break;
        case CornerType::South:
            LightRight = Light - 3;
            LightLeft = Light - maxhx * 3;
            break;
        case CornerType::North:
            LightRight = Light + maxhx * 3;
            LightLeft = Light + 3;
            break;
        }
    }
    else {
        Light = nullptr;
        LightRight = nullptr;
        LightLeft = nullptr;
    }
}

MapSpriteList::MapSpriteList(SpriteManager& spr_mngr) :
    _sprMngr {spr_mngr}
{
    STACK_TRACE_ENTRY();
}

MapSpriteList::~MapSpriteList()
{
    STACK_TRACE_ENTRY();

    Invalidate();

    for (const auto* spr : _invalidatedSprites) {
        delete spr;
    }
    for (const auto* spr : _spritesPool) {
        delete spr;
    }
}

void MapSpriteList::GrowPool()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _spritesPool.reserve(_spritesPool.size() + SPRITES_POOL_GROW_SIZE);

    for (uint i = 0; i < SPRITES_POOL_GROW_SIZE; i++) {
        _spritesPool.push_back(new MapSprite());
    }
}

auto MapSpriteList::RootSprite() -> MapSprite*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _rootSprite;
}

auto MapSpriteList::PutSprite(MapSprite* child, DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, const int* sx, const int* sy, uint id, const uint* id_ptr, const int* ox, const int* oy, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&
{
    STACK_TRACE_ENTRY();

    _spriteCount++;

    MapSprite* spr;
    if (!_invalidatedSprites.empty()) {
        spr = _invalidatedSprites.back();
        _invalidatedSprites.pop_back();
    }
    else {
        if (_spritesPool.empty()) {
            GrowPool();
        }

        spr = _spritesPool.back();
        _spritesPool.pop_back();
    }

    spr->Root = this;

    if (child == nullptr) {
        if (_lastSprite == nullptr) {
            _rootSprite = spr;
            _lastSprite = spr;
            spr->ChainRoot = &_rootSprite;
            spr->ChainLast = &_lastSprite;
            spr->ChainParent = nullptr;
            spr->ChainChild = nullptr;
            spr->TreeIndex = 0;
        }
        else {
            spr->ChainParent = _lastSprite;
            spr->ChainChild = nullptr;
            _lastSprite->ChainChild = spr;
            _lastSprite->ChainLast = nullptr;
            spr->ChainLast = &_lastSprite;
            spr->TreeIndex = _lastSprite->TreeIndex + 1;
            _lastSprite = spr;
        }
    }
    else {
        spr->ChainChild = child;
        spr->ChainParent = child->ChainParent;
        child->ChainParent = spr;
        if (spr->ChainParent != nullptr) {
            spr->ChainParent->ChainChild = spr;
        }

        // Recalculate indices
        auto index = spr->ChainParent != nullptr ? spr->ChainParent->TreeIndex + 1 : 0;
        auto* spr_ = spr;
        while (spr_ != nullptr) {
            spr_->TreeIndex = index;
            spr_ = spr_->ChainChild;
            index++;
        }

        if (spr->ChainParent == nullptr) {
            RUNTIME_ASSERT(child->ChainRoot);
            _rootSprite = spr;
            spr->ChainRoot = &_rootSprite;
            child->ChainRoot = nullptr;
        }
    }

    spr->HexX = hx;
    spr->HexY = hy;
    spr->ScrX = x;
    spr->ScrY = y;
    spr->PScrX = sx;
    spr->PScrY = sy;
    spr->SprId = id;
    spr->PSprId = id_ptr;
    spr->OffsX = ox;
    spr->OffsY = oy;
    spr->Alpha = alpha;
    spr->Light = nullptr;
    spr->LightRight = nullptr;
    spr->LightLeft = nullptr;
    spr->Valid = true;
    spr->ValidCallback = callback;
    if (callback != nullptr) {
        *callback = true;
    }
    spr->EggAppearence = EggAppearenceType::None;
    spr->Contour = ContourType::None;
    spr->ContourColor = 0u;
    spr->Color = 0u;
    spr->DrawEffect = effect;

    // Draw order
    spr->DrawOrder = draw_order;

    if (draw_order < DrawOrderType::NormalBegin || draw_order > DrawOrderType::NormalEnd) {
        spr->DrawOrderPos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(draw_order) + spr->HexY * MAXHEX_MAX + spr->HexX;
    }
    else {
        spr->DrawOrderPos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(DrawOrderType::NormalBegin) + spr->HexY * static_cast<int>(DrawOrderType::NormalBegin) * MAXHEX_MAX + spr->HexX * static_cast<int>(DrawOrderType::NormalBegin) + (static_cast<int>(draw_order) - static_cast<int>(DrawOrderType::NormalBegin));
    }

    return *spr;
}

auto MapSpriteList::AddSprite(DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, const int* sx, const int* sy, uint id, const uint* id_ptr, const int* ox, const int* oy, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&
{
    STACK_TRACE_ENTRY();

    return PutSprite(nullptr, draw_order, hx, hy, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback);
}

auto MapSpriteList::InsertSprite(DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, const int* sx, const int* sy, uint id, const uint* id_ptr, const int* ox, const int* oy, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&
{
    STACK_TRACE_ENTRY();

    // Find place
    uint pos;
    if (draw_order < DrawOrderType::NormalBegin || draw_order > DrawOrderType::NormalEnd) {
        pos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(draw_order) + hy * MAXHEX_MAX + hx;
    }
    else {
        pos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(DrawOrderType::NormalBegin) + hy * static_cast<int>(DrawOrderType::NormalBegin) * MAXHEX_MAX + hx * static_cast<int>(DrawOrderType::NormalBegin) + (static_cast<int>(draw_order) - static_cast<int>(DrawOrderType::NormalBegin));
    }

    auto* parent = _rootSprite;
    while (parent != nullptr) {
        if (!parent->Valid) {
            continue;
        }
        if (pos < parent->DrawOrderPos) {
            break;
        }
        parent = parent->ChainChild;
    }

    return PutSprite(parent, draw_order, hx, hy, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback);
}

void MapSpriteList::Invalidate()
{
    STACK_TRACE_ENTRY();

    while (_rootSprite != nullptr) {
        _rootSprite->Invalidate();
    }

    _lastSprite = nullptr;
    _spriteCount = 0;
}

void MapSpriteList::Sort()
{
    STACK_TRACE_ENTRY();

    if (_rootSprite == nullptr) {
        return;
    }

    _sortSprites.reserve(_spriteCount);

    auto* spr = _rootSprite;
    while (spr != nullptr) {
        _sortSprites.emplace_back(spr);
        spr = spr->ChainChild;
    }

    std::sort(_sortSprites.begin(), _sortSprites.end(), [](const MapSprite* spr1, const MapSprite* spr2) {
        if (spr1->DrawOrderPos == spr2->DrawOrderPos) {
            return spr1->TreeIndex < spr2->TreeIndex;
        }
        return spr1->DrawOrderPos < spr2->DrawOrderPos;
    });

    _rootSprite = _sortSprites.front();
    _lastSprite = _sortSprites.back();

    for (size_t i = 1; i < _sortSprites.size(); i++) {
        _sortSprites[i]->TreeIndex = i;
        _sortSprites[i - 1]->ChainChild = _sortSprites[i];
        _sortSprites[i]->ChainParent = _sortSprites[i - 1];
        _sortSprites[i]->ChainRoot = nullptr;
        _sortSprites[i]->ChainLast = nullptr;
    }

    _rootSprite->TreeIndex = 0;
    _rootSprite->ChainParent = nullptr;
    _rootSprite->ChainRoot = &_rootSprite;
    _rootSprite->ChainLast = nullptr;

    _lastSprite->ChainChild = nullptr;
    _lastSprite->ChainLast = &_lastSprite;

    _sortSprites.clear();
}
