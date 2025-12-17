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

#include "MapSprite.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

MapSprite::~MapSprite()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (IsValid()) [[unlikely]] {
        Reset();
    }
}

void MapSprite::Invalidate() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_owner) [[likely]] {
        _owner->Invalidate(this);
    }
}

void MapSprite::Reset() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _owner.reset();

    if (_validCallback) [[likely]] {
        *_validCallback = false;
        _validCallback = nullptr;
    }

    if (_extraChainRoot) [[likely]] {
        *_extraChainRoot = _extraChainChild.get();
    }
    if (_extraChainParent) [[likely]] {
        _extraChainParent->_extraChainChild = _extraChainChild.get();
    }
    if (_extraChainChild) [[likely]] {
        _extraChainChild->_extraChainParent = _extraChainParent.get();
    }
    if (_extraChainRoot && _extraChainChild) [[likely]] {
        _extraChainChild->_extraChainRoot = _extraChainRoot.get();
    }

    _extraChainRoot = nullptr;
    _extraChainParent = nullptr;
    _extraChainChild = nullptr;
}

auto MapSprite::GetDrawRect() const noexcept -> irect32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* spr = GetSprite();
    FO_RUNTIME_VERIFY(spr, irect32());

    const ipos32 spr_offset = spr->GetOffset();
    const isize32 spr_size = spr->GetSize();
    int32 x = _hexOffset.x + _pHexOffset->x - spr_size.width / 2 + spr_offset.x;
    int32 y = _hexOffset.y + _pHexOffset->y - spr_size.height + spr_offset.y;

    if (_pSprOffset) {
        x += _pSprOffset->x;
        y += _pSprOffset->y;
    }

    return {ipos32(x, y), spr_size};
}

auto MapSprite::GetViewRect() const noexcept -> irect32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* spr = GetSprite();
    FO_RUNTIME_VERIFY(spr, irect32());

    auto rect = GetDrawRect();

    if (const auto view_rect = spr->GetViewSize(); view_rect.has_value()) [[likely]] {
        const auto view_ox = view_rect->x;
        const auto view_oy = view_rect->y;
        const auto view_width = view_rect->width;
        const auto view_height = view_rect->height;

        rect.x = rect.x + rect.width / 2 - view_width / 2 + view_ox;
        rect.y = rect.y + rect.height - view_height + view_oy;
        rect.width = view_width;
        rect.height = view_height;
    }

    return rect;
}

void MapSprite::SetEggAppearence(EggAppearenceType egg_appearence) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _eggAppearence = egg_appearence;
}

void MapSprite::SetContour(ContourType contour) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _contour = contour;
}

void MapSprite::SetContour(ContourType contour, ucolor color) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _contour = contour;
    _contourColor = color;
}

void MapSprite::SetColor(ucolor color) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _color = color;
}

void MapSprite::SetAlpha(const uint8* alpha) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _alpha = alpha;
}

void MapSprite::SetFixedAlpha(uint8 alpha) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* color_alpha = reinterpret_cast<uint8*>(&_color) + 3;
    *color_alpha = alpha;
    _alpha = color_alpha;
}

void MapSprite::SetLight(CornerType corner, const ucolor* light, msize size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_hex.x >= 1 && _hex.x < size.width - 1 && _hex.y >= 1 && _hex.y < size.height - 1) [[likely]] {
        _light = &light[_hex.y * size.width + _hex.x];

        switch (corner) {
        case CornerType::EastWest:
        case CornerType::East:
            _lightRight = _light.get() - 1;
            _lightLeft = _light.get() + 1;
            break;
        case CornerType::NorthSouth:
        case CornerType::West:
            _lightRight = _light.get() + static_cast<size_t>(size.width);
            _lightLeft = _light.get() - static_cast<size_t>(size.width);
            break;
        case CornerType::South:
            _lightRight = _light.get() - 1;
            _lightLeft = _light.get() - static_cast<size_t>(size.width);
            break;
        case CornerType::North:
            _lightRight = _light.get() + static_cast<size_t>(size.width);
            _lightLeft = _light.get() + 1;
            break;
        }
    }
    else {
        _light = nullptr;
        _lightRight = nullptr;
        _lightLeft = nullptr;
    }
}

void MapSprite::SetHidden(bool hidden) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _hidden = hidden;
}

void MapSprite::CreateExtraChain(MapSprite** mspr)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_extraChainRoot);
    _extraChainRoot = mspr;
}

void MapSprite::AddToExtraChain(MapSprite* mspr)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_extraChainRoot);
    auto* last_spr = this;

    while (last_spr->_extraChainChild) {
        last_spr = last_spr->_extraChainChild.get();
    }

    last_spr->_extraChainChild = mspr;
    mspr->_extraChainParent = last_spr;
}

void MapSpriteList::GrowPool() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _spritesPool.reserve(_spritesPool.size() + SPRITES_POOL_GROW_SIZE);

    for (int32 i = 0; i < SPRITES_POOL_GROW_SIZE; i++) {
        _spritesPool.emplace_back(SafeAlloc::MakeUnique<MapSprite>());
    }
}

auto MapSpriteList::AddSprite(DrawOrderType draw_order, mpos hex, ipos32 hex_offset, const ipos32* phex_offset, const Sprite* spr, const Sprite** pspr, const ipos32* spr_offset, const uint8* alpha, RenderEffect** effect, bool* callback) noexcept -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    if (_spritesPool.empty()) [[unlikely]] {
        GrowPool();
    }

    auto mspr = std::move(_spritesPool.back());
    _spritesPool.pop_back();

    mspr->_owner = this;
    mspr->_index = static_cast<uint32>(_activeSprites.size());
    mspr->_globalPos = ++_globalCounter;

    mspr->_drawOrder = draw_order;

    if (draw_order < DrawOrderType::NormalBegin || draw_order > DrawOrderType::NormalEnd) {
        mspr->_drawOrderPos = GameSettings::MAX_MAP_SIZE * GameSettings::MAX_MAP_SIZE * static_cast<int32>(draw_order) + //
            hex.y * GameSettings::MAX_MAP_SIZE + hex.x;
    }
    else {
        mspr->_drawOrderPos = GameSettings::MAX_MAP_SIZE * GameSettings::MAX_MAP_SIZE * static_cast<int32>(DrawOrderType::NormalBegin) + //
            hex.y * static_cast<int32>(DrawOrderType::NormalBegin) * GameSettings::MAX_MAP_SIZE + //
            hex.x * static_cast<int32>(DrawOrderType::NormalBegin) + //
            (static_cast<int32>(draw_order) - static_cast<int32>(DrawOrderType::NormalBegin));
    }

    mspr->_hex = hex;
    mspr->_hexOffset = hex_offset;
    mspr->_pHexOffset = phex_offset;
    mspr->_spr = spr;
    mspr->_pSpr = pspr;
    mspr->_pSprOffset = spr_offset;
    mspr->_alpha = alpha;
    mspr->_light = nullptr;
    mspr->_lightRight = nullptr;
    mspr->_lightLeft = nullptr;
    mspr->_eggAppearence = EggAppearenceType::None;
    mspr->_contour = ContourType::None;
    mspr->_contourColor = ucolor::clear;
    mspr->_color = ucolor::clear;
    mspr->_drawEffect = effect;
    mspr->_validCallback = callback;

    if (callback != nullptr) [[likely]] {
        *callback = true;
    }

    _activeSprites.emplace_back(std::move(mspr));
    _needSort = true;
    return _activeSprites.back().get();
}

void MapSpriteList::InvalidateAll() noexcept
{
    FO_STACK_TRACE_ENTRY();

    while (!_activeSprites.empty()) {
        Invalidate(_activeSprites.back().get());
    }

    _globalCounter = 0;
}

void MapSpriteList::Invalidate(MapSprite* mspr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(mspr->_owner);
    mspr->Reset();

    const auto index = mspr->_index;
    _spritesPool.emplace_back(std::move(_activeSprites[index]));

    if (index < static_cast<uint32>(_activeSprites.size() - 1)) [[likely]] {
        _activeSprites[index] = std::move(_activeSprites.back());
        _activeSprites[index]->_index = index;
        _needSort = true;
    }

    _activeSprites.pop_back();
}

void MapSpriteList::SortIfNeeded() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!_needSort) [[likely]] {
        return;
    }

    std::ranges::sort(_activeSprites, [](auto&& mspr1, auto&& mspr2) -> bool {
        if (mspr1->_drawOrderPos == mspr2->_drawOrderPos) [[unlikely]] {
            return mspr1->_globalPos < mspr2->_globalPos;
        }
        return mspr1->_drawOrderPos < mspr2->_drawOrderPos;
    });

    uint32 index = 0;

    for (auto& mspr : _activeSprites) {
        mspr->_index = index++;
    }

    _needSort = false;
}

MapSpriteHolder::~MapSpriteHolder()
{
    FO_STACK_TRACE_ENTRY();

    if (Valid) [[unlikely]] {
        MSpr->Invalidate();
    }
}

void MapSpriteHolder::StopDraw()
{
    FO_STACK_TRACE_ENTRY();

    if (Valid) [[likely]] {
        FO_RUNTIME_ASSERT(MSpr);
        MSpr->Invalidate();
        FO_RUNTIME_ASSERT(!Valid);
    }

    MSpr.reset();
}

FO_END_NAMESPACE();
