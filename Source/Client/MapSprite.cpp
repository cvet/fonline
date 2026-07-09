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

#include "MapSprite.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

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
        _owner->Invalidate(make_ptr(this));
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
        _extraChainParent->_extraChainChild = _extraChainChild;
    }
    if (_extraChainChild) [[likely]] {
        _extraChainChild->_extraChainParent = _extraChainParent;
    }
    if (_extraChainRoot && _extraChainChild) [[likely]] {
        _extraChainChild->_extraChainRoot = _extraChainRoot;
    }

    _extraChainRoot = nullptr;
    _extraChainParent = nullptr;
    _extraChainChild = nullptr;
}

auto MapSprite::GetDrawRect() const noexcept -> irect32
{
    FO_NO_STACK_TRACE_ENTRY();

    auto spr = GetSprite();
    FO_VERIFY_AND_RETURN_VALUE(spr, irect32(), "Map sprite has no sprite while computing draw rect", _hex, _drawOrder, _index);

    const isize32 spr_size = spr->GetSize();
    const ipos32 root_pos = GetDrawRootPos();
    const ipos32 root_offset = GetSpriteRootOffset();

    return {ipos32(root_pos.x - root_offset.x, root_pos.y - root_offset.y), spr_size};
}

auto MapSprite::GetDrawRootPos() const noexcept -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    ipos32 pos = _hexOffset + *_pHexOffset;

    if (_elevation != 0) {
        const float32_t elevation = numeric_cast<float32_t>(_elevation);
        const float32_t map_elevation_y = std::cos(GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT) * elevation;
        pos.y -= iround<int32_t>(map_elevation_y);
    }

    if (_pSprOffset) {
        pos += *_pSprOffset;
    }

    return pos;
}

auto MapSprite::GetMapRootOffset() const noexcept -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    ipos32 offset = _hexOffset;

    if (_pSprOffset) {
        offset += *_pSprOffset;
    }
    if (_pRootOffset) {
        offset -= *_pRootOffset;
    }

    return offset;
}

auto MapSprite::GetSpriteRootOffset() const noexcept -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    auto spr = GetSprite();
    FO_VERIFY_AND_RETURN_VALUE(spr, ipos32(), "Map sprite has no sprite while computing sprite root offset", _hex, _drawOrder, _index);

    const ipos32 spr_offset = spr->GetOffset();
    const isize32 spr_size = spr->GetSize();

    return {spr_size.width / 2 - spr_offset.x, spr_size.height - spr_offset.y};
}

auto MapSprite::GetViewRect() const noexcept -> irect32
{
    FO_NO_STACK_TRACE_ENTRY();

    auto spr = GetSprite();
    FO_VERIFY_AND_RETURN_VALUE(spr, irect32(), "Map sprite has no sprite while computing view rect", _hex, _drawOrder, _index);

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

void MapSprite::SetColor(ucolor color) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _color = color;
}

void MapSprite::SetAlpha(nptr<const uint8_t> alpha) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _alpha = alpha;
}

void MapSprite::SetFixedAlpha(uint8_t alpha) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _color.comp.a = alpha;
    _alpha = &_color.comp.a;
}

void MapSprite::SetLight(CornerType corner, ptr<const ucolor> light, msize size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_hex.x >= 1 && _hex.x < size.width - 1 && _hex.y >= 1 && _hex.y < size.height - 1) [[likely]] {
        const size_t width = numeric_cast<size_t>(size.width);
        const size_t height = numeric_cast<size_t>(size.height);
        const size_t light_count = width * height;
        const auto lights = make_span(light, light_count);
        const auto light_at = [&lights](size_t pos) noexcept -> ptr<const ucolor> {
            FO_STRONG_ASSERT(pos < lights.size(), "Light index is out of range");
            return &lights[pos];
        };
        const size_t light_index = numeric_cast<size_t>(_hex.y) * width + numeric_cast<size_t>(_hex.x);
        _light = light_at(light_index);

        switch (corner) {
        case CornerType::EastWest:
        case CornerType::East:
            _lightRight = light_at(light_index - 1);
            _lightLeft = light_at(light_index + 1);
            break;
        case CornerType::NorthSouth:
        case CornerType::West:
            _lightRight = light_at(light_index + width);
            _lightLeft = light_at(light_index - width);
            break;
        case CornerType::South:
            _lightRight = light_at(light_index - 1);
            _lightLeft = light_at(light_index - width);
            break;
        case CornerType::North:
            _lightRight = light_at(light_index + width);
            _lightLeft = light_at(light_index + 1);
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

void MapSprite::SetElevation(int16_t elevation) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _elevation = elevation;
}

void MapSprite::SetAngle(int16_t angle) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _angle = angle;
}

void MapSprite::SetMapProjected(bool map_projected) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _mapProjected = map_projected;
}

void MapSprite::CreateExtraChain(ptr<MapSprite*> mspr)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_extraChainRoot, "Extra chain root is already set");
    _extraChainRoot = mspr;
}

void MapSprite::AddToExtraChain(ptr<MapSprite> mspr)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_extraChainRoot, "Extra chain root is null");
    ptr<MapSprite> last_spr = this;

    while (last_spr->_extraChainChild) {
        last_spr = last_spr->_extraChainChild.as_ptr();
    }

    last_spr->_extraChainChild = mspr;
    mspr->_extraChainParent = last_spr;
}

void MapSpriteList::GrowPool() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _spritesPool.reserve(_spritesPool.size() + SPRITES_POOL_GROW_SIZE);

    for (int32_t i = 0; i < SPRITES_POOL_GROW_SIZE; i++) {
        _spritesPool.emplace_back(SafeAlloc::MakeUnique<MapSprite>());
    }
}

auto MapSpriteList::AddSprite(DrawOrderType draw_order, mpos hex, ipos32 hex_offset, nptr<const ipos32> phex_offset, nptr<const Sprite> spr, nptr<const Sprite*> pspr, nptr<const ipos32> spr_offset, nptr<const ipos32> root_offset, nptr<const uint8_t> alpha, nptr<RenderEffect*> effect, nptr<bool> callback) noexcept -> ptr<MapSprite>
{
    FO_STACK_TRACE_ENTRY();

    if (_spritesPool.empty()) [[unlikely]] {
        GrowPool();
    }

    auto mspr = std::move(_spritesPool.back());
    _spritesPool.pop_back();

    mspr->_owner = this;
    mspr->_index = static_cast<uint32_t>(_activeSprites.size());
    mspr->_globalPos = ++_globalCounter;

    mspr->_drawOrder = draw_order;

    if (draw_order < DrawOrderType::NormalBegin || draw_order > DrawOrderType::NormalEnd) {
        mspr->_drawOrderPos = GameSettings::MAX_MAP_SIZE * GameSettings::MAX_MAP_SIZE * static_cast<int32_t>(draw_order) + //
            hex.y * GameSettings::MAX_MAP_SIZE + hex.x;
    }
    else {
        mspr->_drawOrderPos = GameSettings::MAX_MAP_SIZE * GameSettings::MAX_MAP_SIZE * static_cast<int32_t>(DrawOrderType::NormalBegin) + //
            hex.y * static_cast<int32_t>(DrawOrderType::NormalBegin) * GameSettings::MAX_MAP_SIZE + //
            hex.x * static_cast<int32_t>(DrawOrderType::NormalBegin) + //
            (static_cast<int32_t>(draw_order) - static_cast<int32_t>(DrawOrderType::NormalBegin));
    }

    mspr->_hex = hex;
    mspr->_hexOffset = hex_offset;
    mspr->_pHexOffset = phex_offset;
    mspr->_spr = spr;
    mspr->_pSpr = pspr;
    mspr->_pSprOffset = spr_offset;
    mspr->_pRootOffset = root_offset;
    mspr->_alpha = alpha;
    mspr->_light = nullptr;
    mspr->_lightRight = nullptr;
    mspr->_lightLeft = nullptr;
    mspr->_eggAppearence = EggAppearenceType::None;
    mspr->_color = ucolor::clear;
    mspr->_elevation = 0;
    mspr->_angle = 0;
    mspr->_mapProjected = false;
    mspr->_drawEffect = effect;
    mspr->_validCallback = callback;

    if (callback) [[likely]] {
        *callback = true;
    }

    _activeSprites.emplace_back(std::move(mspr));
    _needSort = true;

    if (!_orderBroken && _activeSprites.size() >= 2) {
        auto tail = _activeSprites.back().as_ptr();

        if (tail->_drawOrderPos < _activeSprites[_activeSprites.size() - 2]->_drawOrderPos) {
            _orderBroken = true;
        }
    }

    return _activeSprites.back();
}

void MapSpriteList::InvalidateAll() noexcept
{
    FO_STACK_TRACE_ENTRY();

    while (!_activeSprites.empty()) {
        Invalidate(_activeSprites.back());
    }

    _globalCounter = 0;
}

void MapSpriteList::Invalidate(ptr<MapSprite> mspr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(mspr->_owner, "Map sprite has no owner", mspr->_index);
    mspr->Reset();

    const auto index = mspr->_index;
    _spritesPool.emplace_back(std::move(_activeSprites[index]));
    _needSort = true;

    if (index < static_cast<uint32_t>(_activeSprites.size() - 1)) [[likely]] {
        _activeSprites[index] = std::move(_activeSprites.back());
        _activeSprites[index]->_index = index;
        _orderBroken = true;
    }

    _activeSprites.pop_back();
}

void MapSpriteList::SortIfNeeded() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!_needSort) [[likely]] {
        return;
    }

    if (_orderBroken) {
        std::ranges::sort(_activeSprites, [](auto&& mspr1, auto&& mspr2) -> bool {
            if (mspr1->_drawOrderPos == mspr2->_drawOrderPos) [[unlikely]] {
                return mspr1->_globalPos < mspr2->_globalPos;
            }

            return mspr1->_drawOrderPos < mspr2->_drawOrderPos;
        });
    }

    uint32_t index = 0;

    for (auto& mspr : _activeSprites) {
        mspr->_index = index++;
    }

    uint32_t pos = 0;

    for (uint32_t order = 0; order < DrawOrderRangeSize - 1; order++) {
        while (pos < _activeSprites.size() && static_cast<uint32_t>(_activeSprites[pos]->GetDrawOrder()) < order) {
            pos++;
        }

        _drawOrderRangeBegin[order] = pos;
    }

    _drawOrderRangeBegin[DrawOrderRangeSize - 1] = numeric_cast<uint32_t>(_activeSprites.size());

    _needSort = false;
    _orderBroken = false;
}

auto MapSpriteList::GetDrawOrderRange(DrawOrderType from, DrawOrderType to) const -> pair<uint32_t, uint32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_needSort, "Map sprite list must be sorted before querying a draw-order range");
    FO_VERIFY_AND_THROW(static_cast<uint32_t>(from) <= static_cast<uint32_t>(to), "Requested draw-order range has inverted boundaries", from, to);

    return {_drawOrderRangeBegin[static_cast<size_t>(from)], _drawOrderRangeBegin[static_cast<size_t>(to) + 1]};
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
        FO_VERIFY_AND_THROW(MSpr, "Map sprite holder has no sprite");
        MSpr->Invalidate();
        FO_VERIFY_AND_THROW(!Valid, "Map sprite holder must be invalidated after stopping draw");
    }

    MSpr.reset();
}

FO_END_NAMESPACE
