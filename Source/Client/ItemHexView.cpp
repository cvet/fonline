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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ItemHexView.h"
#include "Client.h"
#include "EffectManager.h"
#include "GenericUtils.h"
#include "MapSprite.h"
#include "MapView.h"
#include "Timer.h"

ItemHexView::ItemHexView(MapView* map, ident_t id, const ProtoItem* proto, const Properties* props) :
    ItemView(map->GetEngine(), id, proto, props),
    HexView(map)
{
    STACK_TRACE_ENTRY();
}

void ItemHexView::Init()
{
    STACK_TRACE_ENTRY();

    if (GetIsTile()) {
        DrawEffect = GetIsRoofTile() ? _engine->EffectMngr.Effects.Roof : _engine->EffectMngr.Effects.Tile;
    }
    else {
        DrawEffect = _engine->EffectMngr.Effects.Generic;
    }

    RefreshAnim();
    RefreshAlpha();
}

void ItemHexView::SetupSprite(MapSprite* mspr)
{
    STACK_TRACE_ENTRY();

    HexView::SetupSprite(mspr);

    mspr->SetColor(GetIsColorize() ? GetLightColor() : ucolor::clear);
    mspr->SetEggAppearence(GetEggType());

    if (GetIsBadItem()) {
        mspr->SetContour(ContourType::Red);
    }

    if (!GetIsNoLightInfluence()) {
        mspr->SetLight(GetCorner(), _map->GetLightData(), _map->GetWidth(), _map->GetHeight());
    }

    if (!mspr->IsHidden() && GetHideSprite()) {
        mspr->SetHidden(true);
    }
}

void ItemHexView::Process()
{
    STACK_TRACE_ENTRY();

    if (IsFading()) {
        ProcessFading();
    }

    if (_isEffect && !_isDynamicEffect && !IsFinishing() && !_anim->IsPlaying()) {
        Finish();
    }

    if (_isDynamicEffect && !IsFinishing()) {
        const auto dt = time_duration_to_ms<float>(_engine->GameTime.GameplayTime() - _effUpdateLastTime);
        if (dt > 0.0f) {
            auto speed = GetFlyEffectSpeed();
            if (speed == 0.0f) {
                speed = 1.0f;
            }

            _effCurX += _effSx * dt * speed;
            _effCurY += _effSy * dt * speed;

            RefreshOffs();

            _effUpdateLastTime = _engine->GameTime.GameplayTime();

            if (GenericUtils::DistSqrt(iround(_effCurX), iround(_effCurY), _effStartX, _effStartY) >= _effDist) {
                Finish();
            }
        }

        const auto dist = GenericUtils::DistSqrt(iround(_effCurX), iround(_effCurY), _effStartX, _effStartY);
        const auto proc = GenericUtils::Percent(_effDist, dist);
        auto&& [step_hx, step_hy] = _effSteps[_effSteps.size() * std::min(proc, 99u) / 100];

        if (GetHexX() != step_hx || GetHexY() != step_hy) {
            const auto hx = GetHexX();
            const auto hy = GetHexY();

            const auto [x, y] = _engine->Geometry.GetHexInterval(hx, hy, step_hx, step_hy);
            _effCurX -= static_cast<float>(x);
            _effCurY -= static_cast<float>(y);

            RefreshOffs();

            _map->MoveItem(this, step_hx, step_hy);
        }
    }
}

void ItemHexView::SetEffect(uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    _isEffect = true;
    _isDynamicEffect = false;

    const auto from_hx = GetHexX();
    const auto from_hy = GetHexY();

    if (from_hx != to_hx || from_hy != to_hy) {
        _isDynamicEffect = true;
        _effSteps.emplace_back(from_hx, from_hy);
        _map->TraceBullet(from_hx, from_hy, to_hx, to_hy, 0, 0.0f, nullptr, CritterFindType::Any, nullptr, nullptr, &_effSteps, false);
        auto [x, y] = _engine->Geometry.GetHexInterval(from_hx, from_hy, to_hx, to_hy);
        y += GenericUtils::Random(5, 25); // Center of body
        std::tie(_effSx, _effSy) = GenericUtils::GetStepsCoords(0, 0, x, y);
        _effDist = GenericUtils::DistSqrt(0, 0, x, y);
    }

    _effStartX = ScrX;
    _effStartY = ScrY;
    _effCurX = static_cast<float>(ScrX);
    _effCurY = static_cast<float>(ScrY);
    _effDir = GeometryHelper::GetFarDir(from_hx, from_hy, to_hx, to_hy);
    _effUpdateLastTime = _engine->GameTime.GameplayTime();
}

void ItemHexView::RefreshAlpha()
{
    STACK_TRACE_ENTRY();

    SetMaxAlpha(GetIsColorize() ? GetLightColor().comp.a : 255);
}

void ItemHexView::RefreshAnim()
{
    STACK_TRACE_ENTRY();

    const auto pic_name = GetPicMap();

    if (pic_name) {
        _anim = _engine->SprMngr.LoadSprite(pic_name, AtlasType::MapSprites);
    }
    else {
        _anim = nullptr;
    }

    if (_anim == nullptr) {
        _anim = _engine->ResMngr.GetItemDefaultSpr();
    }

    if (_isEffect) {
        _anim->SetDir(static_cast<uint8>(_effDir));
    }

    _anim->UseGameplayTimer();

    if (GetIsCanOpen()) {
        _anim->Stop();

        if (GetOpened()) {
            _anim->SetTime(1.0f);
        }
        else {
            _anim->SetTime(0.0f);
        }
    }
    else if (_isEffect && !_isDynamicEffect) {
        _anim->Play(hstring(), false, false);
    }
    else {
        _anim->PlayDefault();
    }

    Spr = _anim.get();

    RefreshOffs();
}

auto ItemHexView::GetEggType() const noexcept -> EggAppearenceType
{
    NO_STACK_TRACE_ENTRY();

    if (GetDisableEgg() || GetIsFlat()) {
        return EggAppearenceType::None;
    }

    switch (GetCorner()) {
    case CornerType::South:
        return EggAppearenceType::ByXOrY;
    case CornerType::North:
        return EggAppearenceType::ByXAndY;
    case CornerType::EastWest:
    case CornerType::West:
        return EggAppearenceType::ByY;
    case CornerType::East:
    case CornerType::NorthSouth:
        return EggAppearenceType::ByX;
    }

    return EggAppearenceType::None;
}

void ItemHexView::RefreshOffs()
{
    STACK_TRACE_ENTRY();

    ScrX = GetOffsetX();
    ScrY = GetOffsetY();

    if (GetIsTile()) {
        if (GetIsRoofTile()) {
            ScrX += _engine->Settings.MapRoofOffsX;
            ScrY += _engine->Settings.MapRoofOffsY;
        }
        else {
            ScrX += _engine->Settings.MapTileOffsX;
            ScrY += _engine->Settings.MapTileOffsY;
        }
    }

    if (_isDynamicEffect) {
        ScrX += iround(_effCurX);
        ScrY += iround(_effCurY);
    }
}
