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

#include "HexView.h"
#include "Client.h"
#include "EngineBase.h"
#include "MapSprite.h"
#include "MapView.h"

FO_BEGIN_NAMESPACE

HexView::HexView(MapView* map) :
    _map {map}
{
    FO_STACK_TRACE_ENTRY();
}

auto HexView::AddSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos32* phex_offset) -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_mapSprValid, "Map spr valid is already set");

    const auto hex_offset = ipos32 {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    auto* mspr = list.AddSprite(draw_order, hex, hex_offset, phex_offset, nullptr, _spr.get_pp(), &_sprOffset, &_rootOffset, &_curAlpha, _drawEffect.get_pp(), &_mapSprValid);

    _mapSpr = mspr;
    SetupSprite(mspr);

    FO_VERIFY_AND_THROW(_mapSpr == mspr, "Hex sprite setup changed the primary map sprite pointer", draw_order, hex, static_cast<const void*>(mspr), static_cast<const void*>(_mapSpr.get()));
    FO_VERIFY_AND_THROW(_mapSprValid, "Map sprite cache is invalid");
    return _mapSpr.get();
}

auto HexView::AddExtraSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos32* phex_offset) -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    make_if_not_exists(_extraMapSpr);
    _extraMapSpr->remove_if([](auto&& entry) { return !entry.second; });
    auto& entry = _extraMapSpr->emplace_back();

    const auto hex_offset = ipos32 {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    entry.first = list.AddSprite(draw_order, hex, hex_offset, phex_offset, nullptr, _spr.get_pp(), &_sprOffset, &_rootOffset, &_curAlpha, _drawEffect.get_pp(), &entry.second);

    SetupSprite(entry.first.get());
    return entry.first.get();
}

void HexView::SetupSprite(MapSprite* mspr)
{
    FO_STACK_TRACE_ENTRY();

    mspr->SetHidden(_mapSprHidden || IsFullyTransparent());
}

void HexView::Finish()
{
    FO_STACK_TRACE_ENTRY();

    _targetAlpha = 0;
    StartFade(_curAlpha);

    _finishing = true;
    _finishingTime = _fadingTime;
}

auto HexView::IsFinished() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _finishing && _map->GetEngine()->GameTime.GetFrameTime() >= _finishingTime;
}

void HexView::ProcessFading()
{
    FO_STACK_TRACE_ENTRY();

    EvaluateCurAlpha();
}

void HexView::FadeUp()
{
    FO_STACK_TRACE_ENTRY();

    StartFade(0);
}

void HexView::InheritAlphaFrom(const HexView* prev)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prev, "Missing required prev");

    _curAlpha = prev->_curAlpha;

    if (_curAlpha != _targetAlpha) {
        StartFade(_curAlpha);
    }
    else {
        _fading = false;
        _fadingTime = {};
    }
}

void HexView::StartFade(uint8_t from_alpha)
{
    FO_STACK_TRACE_ENTRY();

    const auto time = _map->GetEngine()->GameTime.GetFrameTime();

    _fadingTime = time + std::chrono::milliseconds {_map->GetEngine()->Settings.FadingDuration};
    _fadeFromAlpha = from_alpha;
    _fading = true;

    EvaluateCurAlpha();
}

void HexView::EvaluateCurAlpha()
{
    FO_STACK_TRACE_ENTRY();

    if (_fading) {
        const auto time = _map->GetEngine()->GameTime.GetFrameTime();
        const int32_t fading_duration = _map->GetEngine()->Settings.FadingDuration;
        const int32_t fading_remaining = time < _fadingTime ? (_fadingTime - time).to_ms<int32_t>() : 0;
        const float32_t t = fading_duration == 0 ? 1.0f : 1.0f - numeric_cast<float32_t>(fading_remaining) / numeric_cast<float32_t>(fading_duration);

        if (t >= 1.0f) {
            _fading = false;
            _curAlpha = _targetAlpha;
        }
        else {
            _curAlpha = lerp(_fadeFromAlpha, _targetAlpha, t);
        }
    }
    else {
        _curAlpha = _targetAlpha;
    }
}

void HexView::SetTargetAlpha(uint8_t alpha)
{
    FO_STACK_TRACE_ENTRY();

    if (_targetAlpha == alpha) {
        return;
    }

    const auto from_alpha = _curAlpha;
    _targetAlpha = alpha;
    StartFade(from_alpha);
}

void HexView::SetDefaultAlpha(uint8_t alpha)
{
    FO_STACK_TRACE_ENTRY();

    if (_defaultAlpha == _targetAlpha) {
        _defaultAlpha = alpha;
        SetTargetAlpha(alpha);
    }
    else {
        _defaultAlpha = alpha;
    }
}

void HexView::RestoreAlpha()
{
    FO_STACK_TRACE_ENTRY();

    SetTargetAlpha(_defaultAlpha);
}

void HexView::RefreshSprite()
{
    FO_STACK_TRACE_ENTRY();

    if (_mapSprValid) {
        SetupSprite(_mapSpr.get());
    }

    if (_extraMapSpr) {
        for (auto&& [mspr, valid] : *_extraMapSpr) {
            if (valid) {
                SetupSprite(mspr.get());
            }
        }
    }
}

auto HexView::GetMapSprite() const -> const MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapSprValid, "Map sprite cache is invalid");

    return _mapSpr.get();
}

auto HexView::GetMapSprite() -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_mapSprValid, "Map sprite cache is invalid");

    return _mapSpr.get();
}

void HexView::InvalidateSprite()
{
    FO_STACK_TRACE_ENTRY();

    if (_mapSprValid) {
        FO_VERIFY_AND_THROW(_mapSpr, "Missing required map spr");

        _mapSpr->Invalidate();
        _mapSpr.reset();

        FO_VERIFY_AND_THROW(!_mapSprValid, "Map spr valid is already set");
    }

    if (_extraMapSpr) {
        for (auto&& [mspr, valid] : *_extraMapSpr) {
            if (valid) {
                mspr->Invalidate();
            }
        }

        _extraMapSpr.reset();
    }
}

void HexView::SetSpriteVisiblity(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    const auto hidden = !enabled;

    if (_mapSprHidden == hidden) {
        return;
    }

    _mapSprHidden = hidden;
    RefreshSprite();
}

FO_END_NAMESPACE
