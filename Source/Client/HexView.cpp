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

#include "HexView.h"
#include "Client.h"
#include "EngineBase.h"
#include "MapSprite.h"
#include "MapView.h"

FO_BEGIN_NAMESPACE();

HexView::HexView(MapView* map) :
    _map {map}
{
    FO_STACK_TRACE_ENTRY();
}

auto HexView::AddSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos32* phex_offset) -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_mapSprValid);

    const auto hex_offset = ipos32 {_map->GetEngine()->Settings.MapHexWidth / 2, _map->GetEngine()->Settings.MapHexHeight / 2};
    auto* mspr = list.AddSprite(draw_order, hex, hex_offset, phex_offset, nullptr, _spr.get_pp(), &_sprOffset, &_curAlpha, _drawEffect.get_pp(), &_mapSprValid);

    _mapSpr = mspr;
    SetupSprite(mspr);

    FO_RUNTIME_ASSERT(_mapSpr == mspr);
    FO_RUNTIME_ASSERT(_mapSprValid);
    return _mapSpr.get();
}

auto HexView::AddExtraSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos32* phex_offset) -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    make_if_not_exists(_extraMapSpr);
    _extraMapSpr->remove_if([](auto&& entry) { return !entry.second; });
    auto& entry = _extraMapSpr->emplace_back();

    const auto hex_offset = ipos32 {_map->GetEngine()->Settings.MapHexWidth / 2, _map->GetEngine()->Settings.MapHexHeight / 2};
    entry.first = list.AddSprite(draw_order, hex, hex_offset, phex_offset, nullptr, _spr.get_pp(), &_sprOffset, &_curAlpha, _drawEffect.get_pp(), &entry.second);

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

    SetFade(false);

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

    SetFade(true);
}

void HexView::SetFade(bool fade_up)
{
    FO_STACK_TRACE_ENTRY();

    const auto time = _map->GetEngine()->GameTime.GetFrameTime();

    _fadingTime = time + std::chrono::milliseconds {_map->GetEngine()->Settings.FadingDuration} - (_fadingTime > time ? _fadingTime - time : timespan::zero);
    _fadeUp = fade_up;
    _fading = true;

    EvaluateCurAlpha();
}

void HexView::EvaluateCurAlpha()
{
    FO_STACK_TRACE_ENTRY();

    if (_fading) {
        const auto time = _map->GetEngine()->GameTime.GetFrameTime();
        const auto fading_proc = 100 - GenericUtils::Percent(_map->GetEngine()->Settings.FadingDuration, time < _fadingTime ? (_fadingTime - time).to_ms<int32>() : 0);

        if (fading_proc == 100) {
            _fading = false;
        }

        _curAlpha = numeric_cast<uint8>(_fadeUp ? fading_proc * _targetAlpha / 100 : (100 - fading_proc) * _targetAlpha / 100);
    }
    else {
        _curAlpha = _targetAlpha;
    }
}

void HexView::SetTargetAlpha(uint8 alpha)
{
    FO_STACK_TRACE_ENTRY();

    _targetAlpha = alpha;
    EvaluateCurAlpha();
}

void HexView::SetDefaultAlpha(uint8 alpha)
{
    FO_STACK_TRACE_ENTRY();

    if (_defaultAlpha == _targetAlpha) {
        _defaultAlpha = alpha;
        _targetAlpha = alpha;
        EvaluateCurAlpha();
    }
    else {
        _defaultAlpha = alpha;
    }
}

void HexView::RestoreAlpha()
{
    FO_STACK_TRACE_ENTRY();

    _targetAlpha = _defaultAlpha;
    EvaluateCurAlpha();
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

    FO_RUNTIME_ASSERT(_mapSprValid);

    return _mapSpr.get();
}

auto HexView::GetMapSprite() -> MapSprite*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_mapSprValid);

    return _mapSpr.get();
}

void HexView::InvalidateSprite()
{
    FO_STACK_TRACE_ENTRY();

    if (_mapSprValid) {
        FO_RUNTIME_ASSERT(_mapSpr);

        _mapSpr->Invalidate();
        _mapSpr.reset();

        FO_RUNTIME_ASSERT(!_mapSprValid);
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

FO_END_NAMESPACE();
