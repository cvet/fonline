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

#include "Sprites.h"
#include "SpriteManager.h"

void Sprite::Unvalidate()
{
    if (!Valid) {
        return;
    }
    Valid = false;

    if (ValidCallback != nullptr) {
        *ValidCallback = false;
        ValidCallback = nullptr;
    }

    if (Parent != nullptr) {
        Parent->Child = nullptr;
        Parent->Unvalidate();
    }
    if (Child != nullptr) {
        Child->Parent = nullptr;
        Child->Unvalidate();
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

    Root->_unvalidatedSprites.push_back(this);

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

auto Sprite::GetIntersected(int ox, int oy) -> Sprite*
{
    if (ox < 0 || oy < 0) {
        return nullptr;
    }

    return Root->_sprMngr.IsPixNoTransp(PSprId != nullptr ? *PSprId : SprId, ox, oy, true) ? this : nullptr;
}

void Sprite::SetEggAppearence(EggAppearenceType egg_appearence)
{
    if (!Valid) {
        return;
    }

    Valid = false;
    EggAppearence = egg_appearence;
    if (Parent != nullptr) {
        Parent->SetEggAppearence(egg_appearence);
    }
    if (Child != nullptr) {
        Child->SetEggAppearence(egg_appearence);
    }
    Valid = true;
}

void Sprite::SetContour(ContourType contour)
{
    if (!Valid) {
        return;
    }

    Valid = false;
    Contour = contour;
    if (Parent != nullptr) {
        Parent->SetContour(contour);
    }
    if (Child != nullptr) {
        Child->SetContour(contour);
    }
    Valid = true;
}

void Sprite::SetContour(ContourType contour, uint color)
{
    if (!Valid) {
        return;
    }

    Valid = false;
    Contour = contour;
    ContourColor = color;
    if (Parent != nullptr) {
        Parent->SetContour(contour, color);
    }
    if (Child != nullptr) {
        Child->SetContour(contour, color);
    }
    Valid = true;
}

void Sprite::SetColor(uint color)
{
    if (!Valid) {
        return;
    }

    Valid = false;
    Color = color;
    if (Parent != nullptr) {
        Parent->SetColor(color);
    }
    if (Child != nullptr) {
        Child->SetColor(color);
    }
    Valid = true;
}

void Sprite::SetAlpha(uchar* alpha)
{
    if (!Valid) {
        return;
    }

    Valid = false;
    Alpha = alpha;
    if (Parent != nullptr) {
        Parent->SetAlpha(alpha);
    }
    if (Child != nullptr) {
        Child->SetAlpha(alpha);
    }
    Valid = true;
}

void Sprite::SetLight(CornerType corner, uchar* light, ushort maxhx, ushort maxhy)
{
    if (!Valid) {
        return;
    }

    Valid = false;

    if (HexX >= 1 && HexX < maxhx - 1 && HexY >= 1 && HexY < maxhy - 1) {
        Light = &light[HexY * maxhx * 3 + HexX * 3];

        switch (corner) {
        default:
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

    if (Parent != nullptr) {
        Parent->SetLight(corner, light, maxhx, maxhy);
    }
    if (Child != nullptr) {
        Child->SetLight(corner, light, maxhx, maxhy);
    }

    Valid = true;
}

void Sprite::SetFixedAlpha(uchar alpha)
{
    if (!Valid) {
        return;
    }

    Valid = false;

    Alpha = reinterpret_cast<uchar*>(&Color) + 3;
    *Alpha = alpha;

    if (Parent != nullptr) {
        Parent->SetFixedAlpha(alpha);
    }
    if (Child != nullptr) {
        Child->SetFixedAlpha(alpha);
    }

    Valid = true;
}

void Sprites::GrowPool()
{
    NON_CONST_METHOD_HINT();

    _spritesPool.reserve(_spritesPool.size() + SPRITES_POOL_GROW_SIZE);

    for (uint i = 0; i < SPRITES_POOL_GROW_SIZE; i++) {
        _spritesPool.push_back(new Sprite());
    }
}

auto Sprites::RootSprite() -> Sprite*
{
    NON_CONST_METHOD_HINT();

    return _rootSprite;
}

auto Sprites::PutSprite(Sprite* child, DrawOrderType draw_order, ushort hx, ushort hy, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback) -> Sprite&
{
    _spriteCount++;

    Sprite* spr;
    if (!_unvalidatedSprites.empty()) {
        spr = _unvalidatedSprites.back();
        _unvalidatedSprites.pop_back();
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
    spr->Parent = nullptr;
    spr->Child = nullptr;

    // Draw order
    spr->DrawOrder = draw_order;

    if (draw_order < DrawOrderType::Normal) {
        spr->DrawOrderPos = spr->HexY * MAXHEX_MAX + spr->HexX + MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(draw_order);
    }
    else {
        spr->DrawOrderPos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(DrawOrderType::Normal) + spr->HexY * static_cast<int>(DrawOrderType::Normal) * MAXHEX_MAX + spr->HexX * static_cast<int>(DrawOrderType::Normal) + (static_cast<int>(draw_order) - static_cast<int>(DrawOrderType::Normal));
    }

    return *spr;
}

auto Sprites::AddSprite(DrawOrderType draw_order, ushort hx, ushort hy, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback) -> Sprite&
{
    return PutSprite(nullptr, draw_order, hx, hy, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback);
}

auto Sprites::InsertSprite(DrawOrderType draw_order, ushort hx, ushort hy, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, RenderEffect** effect, bool* callback) -> Sprite&
{
    // Find place
    uint pos;
    if (draw_order < DrawOrderType::Normal) {
        pos = hy * MAXHEX_MAX + hx + MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(draw_order);
    }
    else {
        pos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(DrawOrderType::Normal) + hy * static_cast<int>(DrawOrderType::Normal) * MAXHEX_MAX + hx * static_cast<int>(DrawOrderType::Normal) + (static_cast<int>(draw_order) - static_cast<int>(DrawOrderType::Normal));
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

void Sprites::Unvalidate()
{
    while (_rootSprite != nullptr) {
        _rootSprite->Unvalidate();
    }
    _spriteCount = 0;
}

void Sprites::SortByMapPos()
{
    if (_rootSprite == nullptr) {
        return;
    }

    vector<Sprite*> sprites;
    sprites.reserve(_spriteCount);
    auto* spr = _rootSprite;
    while (spr != nullptr) {
        sprites.push_back(spr);
        spr = spr->ChainChild;
    }

    auto& spr_infos = _sprMngr.GetSpritesInfo();
    std::sort(sprites.begin(), sprites.end(), [&spr_infos](Sprite* spr1, Sprite* spr2) {
        const auto* si1 = spr_infos[spr1->PSprId != nullptr ? *spr1->PSprId : spr1->SprId];
        const auto* si2 = spr_infos[spr2->PSprId != nullptr ? *spr2->PSprId : spr2->SprId];
        return si1 != nullptr && si2 != nullptr && si1->Atlas != nullptr && si2->Atlas != nullptr && si1->Atlas->MainTex < si2->Atlas->MainTex;
    });

    std::sort(sprites.begin(), sprites.end(), [](Sprite* spr1, Sprite* spr2) {
        if (spr1->DrawOrderPos == spr2->DrawOrderPos) {
            return spr1->TreeIndex < spr2->TreeIndex;
        }
        return spr1->DrawOrderPos < spr2->DrawOrderPos;
    });

    for (auto* sprite : sprites) {
        sprite->ChainParent = nullptr;
        sprite->ChainChild = nullptr;
        sprite->ChainRoot = nullptr;
        sprite->ChainLast = nullptr;
    }

    for (size_t i = 1; i < sprites.size(); i++) {
        sprites[i - 1]->ChainChild = sprites[i];
        sprites[i]->ChainParent = sprites[i - 1];
    }

    _rootSprite = sprites.front();
    _lastSprite = sprites.back();
    _rootSprite->ChainRoot = &_rootSprite;
    _lastSprite->ChainLast = &_lastSprite;
}

auto Sprites::Size() const -> uint
{
    return _spriteCount;
}

void Sprites::Clear()
{
    Unvalidate();

    for (auto* spr : _unvalidatedSprites) {
        _spritesPool.push_back(spr);
    }
    _unvalidatedSprites.clear();
}
