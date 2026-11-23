// FOnline Engine
// SPDX-License-Identifier: MIT

#include "HexView.h"

#include "Client.h"
#include "EngineBase.h"
#include "MapSprite.h"
#include "MapView.h"

FO_BEGIN_NAMESPACE

HexView::HexView(ptr<MapView> map) :
    _map {map}
{
}

auto HexView::AddSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, int8_t sub_layer, nptr<const ipos32> phex_offset) -> ptr<MapSprite>
{
    FO_VERIFY_AND_THROW(!_mapSprValid, "Map spr valid is already set");

    ipos32 hex_offset {ipos32 {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2}};
    auto mspr {list.AddSprite(draw_order, hex, hex_offset, phex_offset, nullptr, _spr.get_pp(), &_sprOffset, &_rootOffset, &_curAlpha, _drawEffect.get_pp(), &_mapSprValid, sub_layer)};

    _mapSpr = mspr;
    SetupSprite(mspr);

    FO_VERIFY_AND_THROW(_mapSpr == mspr, "Hex sprite setup changed the primary map sprite pointer", draw_order, hex);
    FO_VERIFY_AND_THROW(_mapSprValid, "Map sprite cache is invalid");

    return _mapSpr;
}

auto HexView::AddExtraSprite(MapSpriteList& list, DrawOrderType draw_order, mpos hex, int8_t sub_layer, nptr<const ipos32> phex_offset) -> ptr<MapSprite>
{
    if (!_extraMapSpr) {
        _extraMapSpr.emplace();
    }

    _extraMapSpr->remove_if([](auto&& entry) { return !entry.second; });
    auto& entry {_extraMapSpr->emplace_back()};

    ipos32 hex_offset {ipos32 {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2}};
    entry.first = list.AddSprite(draw_order, hex, hex_offset, phex_offset, nullptr, _spr.get_pp(), &_sprOffset, &_rootOffset, &_curAlpha, _drawEffect.get_pp(), &entry.second, sub_layer);

    auto map_spr {entry.first};
    FO_VERIFY_AND_THROW(map_spr, "Map sprite is null");
    SetupSprite(map_spr);
    return map_spr;
}

void HexView::SetupSprite(ptr<MapSprite> mspr)
{
    mspr->SetSpriteOffsetOverride(&_sprOffsetOverride);
    mspr->SetHidden(_mapSprHidden || IsFullyTransparent());
}

void HexView::Finish()
{
    _targetAlpha = 0;
    StartFade(_curAlpha);

    _finishing = true;
    _finishingTime = _fadingTime;
}

auto HexView::IsFinished() const noexcept -> bool
{
    return _finishing && _map->GetEngine()->GameTime.GetFrameTime() >= _finishingTime;
}

void HexView::ProcessFading()
{
    EvaluateCurAlpha();
}

void HexView::FadeUp()
{
    StartFade(0);
}

void HexView::InheritAlphaFrom(ptr<const HexView> prev)
{
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
    nanotime time {_map->GetEngine()->GameTime.GetFrameTime()};

    _fadingTime = time + std::chrono::milliseconds {_map->GetEngine()->Settings->View.FadingDuration};
    _fadeFromAlpha = from_alpha;
    _fading = true;

    EvaluateCurAlpha();
}

void HexView::EvaluateCurAlpha()
{
    if (_fading) {
        nanotime time {_map->GetEngine()->GameTime.GetFrameTime()};
        timespan fading_duration {std::chrono::milliseconds {_map->GetEngine()->Settings->View.FadingDuration}};
        timespan fading_remaining {time < _fadingTime ? _fadingTime - time : timespan::zero};
        float32_t t {fading_duration <= timespan::zero ? 1.0f : 1.0f - fading_remaining.div<float32_t>(fading_duration)};

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
    if (_targetAlpha == alpha) {
        return;
    }

    uint8_t from_alpha {_curAlpha};
    _targetAlpha = alpha;
    StartFade(from_alpha);
}

void HexView::SetDefaultAlpha(uint8_t alpha)
{
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
    SetTargetAlpha(_defaultAlpha);
}

void HexView::RefreshSprite()
{
    if (_mapSprValid) {
        SetupSprite(_mapSpr);
    }

    if (_extraMapSpr) {
        for (auto&& [mspr, valid] : *_extraMapSpr) {
            if (valid) {
                SetupSprite(mspr);
            }
        }
    }
}

auto HexView::IsMapSpriteVisibleAtHex(mpos hex) const noexcept -> bool
{
    return !!GetVisibleMapSpriteAtHex(hex);
}

auto HexView::GetVisibleMapSpriteAtHex(mpos hex) const noexcept -> nptr<const MapSprite>
{
    if (_mapSprValid && _mapSpr && _mapSpr->GetHex() == hex && !_mapSpr->IsHidden()) {
        return _mapSpr;
    }

    if (_extraMapSpr) {
        for (auto&& [mspr, valid] : *_extraMapSpr) {
            if (valid && mspr && mspr->GetHex() == hex && !mspr->IsHidden()) {
                return mspr;
            }
        }
    }

    return nullptr;
}

auto HexView::GetMapSprite() const -> ptr<const MapSprite>
{
    FO_VERIFY_AND_THROW(_mapSprValid, "Map sprite cache is invalid");

    return _mapSpr;
}

auto HexView::GetMapSprite() -> ptr<MapSprite>
{
    FO_VERIFY_AND_THROW(_mapSprValid, "Map sprite cache is invalid");

    return _mapSpr;
}

void HexView::InvalidateSprite()
{
    if (_mapSprValid) {
        FO_VERIFY_AND_THROW(_mapSpr, "Map sprite is null");

        _mapSpr->Invalidate();
        _mapSpr.reset();

        FO_VERIFY_AND_THROW(!_mapSprValid, "Map spr valid is already set");
    }

    if (_extraMapSpr) {
        for (auto&& [mspr, valid] : *_extraMapSpr) {
            if (valid) {
                auto map_spr {mspr};
                FO_VERIFY_AND_THROW(map_spr, "Map sprite is null");
                map_spr->Invalidate();
            }
        }

        _extraMapSpr.reset();
    }
}

void HexView::SetSpriteVisiblity(bool enabled)
{
    bool hidden {!enabled};

    if (_mapSprHidden == hidden) {
        return;
    }

    _mapSprHidden = hidden;
    RefreshSprite();
}

FO_END_NAMESPACE
