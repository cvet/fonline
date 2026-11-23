// FOnline Engine
// SPDX-License-Identifier: MIT

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
    [[nodiscard]] auto IsMapSpriteVisibleAtHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetVisibleMapSpriteAtHex(mpos hex) const noexcept -> nptr<const MapSprite>;
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

    auto AddSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, int8_t sub_layer, nptr<const ipos32> phex_offset) -> ptr<MapSprite>;
    auto AddExtraSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, int8_t sub_layer, nptr<const ipos32> phex_offset) -> ptr<MapSprite>;
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
    optional<ipos32> _sprOffsetOverride {};

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
