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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

    mspr->SetColor(GetIsColorize() ? GetLightColor() & 0xFFFFFF : 0);
    mspr->SetEggAppearence(GetEggType());

    if (GetIsBadItem()) {
        mspr->SetContour(ContourType::Red);
    }

    if (!GetIsNoLightInfluence()) {
        mspr->SetLight(GetCorner(), _map->GetLightHex(0, 0), _map->GetWidth(), _map->GetHeight());
    }
}

void ItemHexView::Process()
{
    STACK_TRACE_ENTRY();

    if (IsFading()) {
        ProcessFading();
    }

    if (_isEffect && !_isDynamicEffect && !IsFinishing()) {
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

        auto dist = GenericUtils::DistSqrt(iround(_effCurX), iround(_effCurY), _effStartX, _effStartY);
        if (dist > _effDist) {
            dist = _effDist;
        }

        auto proc = GenericUtils::Percent(_effDist, dist);
        if (proc > 99) {
            proc = 99;
        }

        auto&& [step_hx, step_hy] = _effSteps[_effSteps.size() * proc / 100];

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

void ItemHexView::SetFlyEffect(uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    const auto from_hx = GetHexX();
    const auto from_hy = GetHexY();

    auto sx = 0.0f;
    auto sy = 0.0f;
    auto dist = 0u;

    if (from_hx != to_hx || from_hy != to_hy) {
        _effSteps.emplace_back(from_hx, from_hy);
        _map->TraceBullet(from_hx, from_hy, to_hx, to_hy, 0, 0.0f, nullptr, CritterFindType::Any, nullptr, nullptr, &_effSteps, false);
        auto [x, y] = _engine->Geometry.GetHexInterval(from_hx, from_hy, to_hx, to_hy);
        y += GenericUtils::Random(5, 25); // Center of body
        std::tie(sx, sy) = GenericUtils::GetStepsCoords(0, 0, x, y);
        dist = GenericUtils::DistSqrt(0, 0, x, y);
    }

    _isEffect = true;
    _effSx = sx;
    _effSy = sy;
    _isDynamicEffect = _effSx != 0.0f || _effSy != 0.0f;
    _effDist = dist;
    _effStartX = ScrX;
    _effStartY = ScrY;
    _effCurX = static_cast<float>(ScrX);
    _effCurY = static_cast<float>(ScrY);
    _effDir = GeometryHelper::GetFarDir(from_hx, from_hy, to_hx, to_hy);
    _effUpdateLastTime = _engine->GameTime.GameplayTime();

    RefreshAnim();
}

void ItemHexView::RefreshAlpha()
{
    STACK_TRACE_ENTRY();

    SetMaxAlpha(GetIsColorize() ? GetLightColor() >> 24 : 0xFF);
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
    else {
        _anim->PlayDefault();
    }

    Spr = _anim.get();

    RefreshOffs();
}

auto ItemHexView::GetEggType() const -> EggAppearenceType
{
    STACK_TRACE_ENTRY();

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
