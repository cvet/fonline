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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"
#include "MapSprite.h"
#include "MapView.h"
#include "Timer.h"

HexView::HexView(MapView* map) :
    _map {map}
{
    STACK_TRACE_ENTRY();
}

auto HexView::AddSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos* hex_offset) -> MapSprite*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_mapSprValid);

    auto& mspr = list.AddSprite(draw_order, hex, {_map->GetEngine()->Settings.MapHexWidth / 2, _map->GetEngine()->Settings.MapHexHeight / 2}, //
        hex_offset, nullptr, &Spr, &SprOffset, &Alpha, &DrawEffect, &_mapSprValid);

    SetupSprite(&mspr);

    RUNTIME_ASSERT(_mapSpr == &mspr);
    RUNTIME_ASSERT(_mapSprValid);

    return _mapSpr;
}

auto HexView::InsertSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos* hex_offset) -> MapSprite*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_mapSprValid);

    auto& mspr = list.InsertSprite(draw_order, hex, {_map->GetEngine()->Settings.MapHexWidth / 2, _map->GetEngine()->Settings.MapHexHeight / 2}, //
        hex_offset, nullptr, &Spr, &SprOffset, &Alpha, &DrawEffect, &_mapSprValid);

    SetupSprite(&mspr);

    RUNTIME_ASSERT(_mapSpr == &mspr);
    RUNTIME_ASSERT(_mapSprValid);

    return _mapSpr;
}

void HexView::SetupSprite(MapSprite* mspr)
{
    STACK_TRACE_ENTRY();

    _mapSpr = mspr;
    _mapSpr->SetHidden(_mapSprHidden || IsFullyTransparent());
}

void HexView::Finish()
{
    STACK_TRACE_ENTRY();

    SetFade(false);

    _finishing = true;
    _finishingTime = _fadingTime;
}

auto HexView::IsFinished() const -> bool
{
    STACK_TRACE_ENTRY();

    return _finishing && _map->GetEngine()->GameTime.GameplayTime() >= _finishingTime;
}

void HexView::ProcessFading()
{
    STACK_TRACE_ENTRY();

    Alpha = EvaluateFadeAlpha();

    if (Alpha > _maxAlpha) {
        Alpha = _maxAlpha;
    }
}

void HexView::FadeUp()
{
    STACK_TRACE_ENTRY();

    SetFade(true);
}

void HexView::SetFade(bool fade_up)
{
    STACK_TRACE_ENTRY();

    const auto time = _map->GetEngine()->GameTime.GameplayTime();

    _fadingTime = time + std::chrono::milliseconds {_map->GetEngine()->Settings.FadingDuration} - (_fadingTime > time ? _fadingTime - time : time_duration {0});
    _fadeUp = fade_up;
    _fading = true;

    Alpha = EvaluateFadeAlpha();
}

auto HexView::EvaluateFadeAlpha() -> uint8
{
    STACK_TRACE_ENTRY();

    const auto time = _map->GetEngine()->GameTime.GameplayTime();
    const auto fading_proc = 100 - GenericUtils::Percent(_map->GetEngine()->Settings.FadingDuration, _fadingTime > time ? time_duration_to_ms<uint>(_fadingTime - time) : 0);

    if (fading_proc == 100) {
        _fading = false;
    }

    const auto alpha = _fadeUp ? fading_proc * 255 / 100 : (100 - fading_proc) * 255 / 100;
    return static_cast<uint8>(alpha);
}

void HexView::SetMaxAlpha(uint8 alpha)
{
    STACK_TRACE_ENTRY();

    _maxAlpha = alpha;
}

void HexView::RestoreAlpha()
{
    STACK_TRACE_ENTRY();

    Alpha = _maxAlpha;
}

void HexView::RefreshSprite()
{
    STACK_TRACE_ENTRY();

    if (_mapSprValid) {
        SetupSprite(_mapSpr);
    }
}

auto HexView::GetSprite() const -> const MapSprite*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapSprValid);

    return _mapSpr;
}

auto HexView::GetSprite() -> MapSprite*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapSprValid);

    return _mapSpr;
}

void HexView::InvalidateSprite()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_mapSpr);
    RUNTIME_ASSERT(_mapSprValid);

    _mapSpr->Invalidate();
    _mapSpr = nullptr;

    RUNTIME_ASSERT(!_mapSprValid);
}

void HexView::SetSpriteVisiblity(bool enabled)
{
    STACK_TRACE_ENTRY();

    const auto hidden = !enabled;

    if (_mapSprHidden == hidden) {
        return;
    }

    _mapSprHidden = hidden;

    if (_mapSpr != nullptr) {
        SetupSprite(_mapSpr);
    }
}
