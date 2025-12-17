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

#pragma once

#include "Common.h"

#include "Geometry.h"
#include "MapSprite.h"

FO_BEGIN_NAMESPACE();

class MapView;

class HexView
{
public:
    HexView() = delete;
    explicit HexView(MapView* map);
    HexView(const HexView&) = delete;
    HexView(HexView&&) noexcept = delete;
    auto operator=(const HexView&) = delete;
    auto operator=(HexView&&) noexcept = delete;
    virtual ~HexView() = default;

    [[nodiscard]] auto GetMap() noexcept -> MapView* { return _map.get(); }
    [[nodiscard]] auto GetMap() const noexcept -> const MapView* { return _map.get(); }
    [[nodiscard]] auto IsMapSpriteValid() const noexcept -> bool { return _mapSprValid; }
    [[nodiscard]] auto IsMapSpriteVisible() const noexcept -> bool { return _mapSprValid && !_mapSpr->IsHidden(); }
    [[nodiscard]] auto GetMapSprite() const -> const MapSprite*;
    [[nodiscard]] auto GetMapSprite() -> MapSprite*;
    [[nodiscard]] auto HasExtraMapSprites() const noexcept -> bool { return !!_extraMapSpr; }
    [[nodiscard]] auto GetExtraMapSprites() const noexcept -> const list<pair<raw_ptr<MapSprite>, bool>>& { return *_extraMapSpr; }
    [[nodiscard]] auto GetSprite() const noexcept -> const Sprite* { return _spr.get(); }
    [[nodiscard]] auto GetSpriteOffset() const noexcept -> ipos32 { return _sprOffset; }
    [[nodiscard]] auto GetSpriteOffsetPtr() const noexcept -> const ipos32* { return &_sprOffset; }
    [[nodiscard]] auto GetDrawEffect() -> RenderEffect* { return _drawEffect.get(); }
    [[nodiscard]] auto GetCurAlpha() const noexcept -> uint8 { return _curAlpha; }
    [[nodiscard]] auto IsTransparent() const noexcept -> bool { return _targetAlpha < 0xFF; }
    [[nodiscard]] auto IsFullyTransparent() const noexcept -> bool { return _targetAlpha == 0; }
    [[nodiscard]] auto IsFading() const noexcept -> bool { return _fading; }
    [[nodiscard]] auto IsFinishing() const noexcept -> bool { return _finishing; }
    [[nodiscard]] auto IsFinished() const noexcept -> bool;

    auto AddSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos32* phex_offset) -> MapSprite*;
    auto AddExtraSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, const ipos32* phex_offset) -> MapSprite*;
    void Finish();
    auto StoreFading() -> tuple<bool, bool, nanotime> { return {_fading, _fadeUp, _fadingTime}; }
    void RestoreFading(const tuple<bool, bool, nanotime>& data) { std::tie(_fading, _fadeUp, _fadingTime) = data; }
    void FadeUp();
    void SetTargetAlpha(uint8 alpha);
    void SetDefaultAlpha(uint8 alpha);
    void RestoreAlpha();
    void RefreshSprite();
    void InvalidateSprite();
    void SetSpriteVisiblity(bool enabled);
    void SetDrawEffect(RenderEffect* effect) { _drawEffect = effect; }

protected:
    raw_ptr<MapView> _map;
    raw_ptr<const Sprite> _spr {};
    ipos32 _sprOffset {};

    virtual void SetupSprite(MapSprite* mspr);
    void ProcessFading();

private:
    void SetFade(bool fade_up);
    void EvaluateCurAlpha();

    raw_ptr<MapSprite> _mapSpr {};
    bool _mapSprValid {};
    bool _mapSprHidden {};

    unique_ptr<list<pair<raw_ptr<MapSprite>, bool>>> _extraMapSpr {};

    raw_ptr<RenderEffect> _drawEffect {};

    uint8 _curAlpha {0xFF};
    uint8 _defaultAlpha {0xFF};
    uint8 _targetAlpha {0xFF};

    bool _fading {};
    bool _fadeUp {};
    nanotime _fadingTime {};

    bool _finishing {};
    nanotime _finishingTime {};
};

FO_END_NAMESPACE();
