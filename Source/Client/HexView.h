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

#pragma once

#include "Common.h"

#include "Geometry.h"
#include "MapSprite.h"

FO_BEGIN_NAMESPACE

class MapView;

class HexView
{
public:
    using ExtraMapSpriteList = list<pair<nptr<MapSprite>, bool>>;

    HexView() = delete;
    explicit HexView(ptr<MapView> map);
    HexView(const HexView&) = delete;
    HexView(HexView&&) noexcept = delete;
    auto operator=(const HexView&) = delete;
    auto operator=(HexView&&) noexcept = delete;
    virtual ~HexView() = default;

    [[nodiscard]] auto GetMap() noexcept -> ptr<MapView> { return _map; }
    [[nodiscard]] auto GetMap() const noexcept -> ptr<const MapView> { return _map; }
    [[nodiscard]] auto IsMapSpriteValid() const noexcept -> bool { return _mapSprValid; }
    [[nodiscard]] auto IsMapSpriteVisible() const noexcept -> bool { return _mapSprValid && !_mapSpr->IsHidden(); }
    [[nodiscard]] auto GetMapSprite() const -> ptr<const MapSprite>;
    [[nodiscard]] auto GetMapSprite() -> ptr<MapSprite>;
    [[nodiscard]] auto HasExtraMapSprites() const noexcept -> bool { return !!_extraMapSpr; }
    [[nodiscard]] auto GetExtraMapSprites() const noexcept -> nptr<const ExtraMapSpriteList> { return _extraMapSpr ? make_nptr(&*_extraMapSpr) : nullptr; }
    [[nodiscard]] auto GetSprite() const noexcept -> nptr<const Sprite> { return _spr; }
    [[nodiscard]] auto GetSpriteOffset() const noexcept -> ipos32 { return _sprOffset; }
    [[nodiscard]] auto GetSpriteOffsetPtr() const noexcept -> ptr<const ipos32> { return &_sprOffset; }
    [[nodiscard]] auto GetDrawEffect() -> nptr<RenderEffect> { return _drawEffect; }
    [[nodiscard]] auto GetCurAlpha() const noexcept -> uint8_t { return _curAlpha; }
    [[nodiscard]] auto IsTransparent() const noexcept -> bool { return _targetAlpha < 0xFF; }
    [[nodiscard]] auto IsFullyTransparent() const noexcept -> bool { return _curAlpha == 0 && !_fading; }
    [[nodiscard]] auto IsFading() const noexcept -> bool { return _fading; }
    [[nodiscard]] auto IsFinishing() const noexcept -> bool { return _finishing; }
    [[nodiscard]] auto IsFinished() const noexcept -> bool;

    auto AddSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, nptr<const ipos32> phex_offset) -> ptr<MapSprite>;
    auto AddExtraSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, nptr<const ipos32> phex_offset) -> ptr<MapSprite>;
    void Finish();
    void InheritAlphaFrom(ptr<const HexView> prev);
    void FadeUp();
    void SetTargetAlpha(uint8_t alpha);
    void SetDefaultAlpha(uint8_t alpha);
    void RestoreAlpha();
    void RefreshSprite();
    void InvalidateSprite();
    void SetSpriteVisiblity(bool enabled);
    void SetDrawEffect(nptr<RenderEffect> effect) { _drawEffect = effect; }

protected:
    ptr<MapView> _map;
    nptr<const Sprite> _spr {};
    ipos32 _sprOffset {};
    ipos32 _rootOffset {};

    virtual void SetupSprite(ptr<MapSprite> mspr);
    void ProcessFading();

private:
    void StartFade(uint8_t from_alpha);
    void EvaluateCurAlpha();

    nptr<MapSprite> _mapSpr {};
    bool _mapSprValid {};
    bool _mapSprHidden {};

    optional<ExtraMapSpriteList> _extraMapSpr {};

    nptr<RenderEffect> _drawEffect {};

    uint8_t _curAlpha {0xFF};
    uint8_t _defaultAlpha {0xFF};
    uint8_t _targetAlpha {0xFF};

    bool _fading {};
    nanotime _fadingTime {};
    uint8_t _fadeFromAlpha {0xFF};

    bool _finishing {};
    nanotime _finishingTime {};
};

FO_END_NAMESPACE
