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
        mspr->SetLight(GetCorner(), _map->GetLightHex({}), _map->GetSize());
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

            _effCurOffset.x += _effStepOffset.x * dt * speed;
            _effCurOffset.y += _effStepOffset.y * dt * speed;

            RefreshOffs();

            _effUpdateLastTime = _engine->GameTime.GameplayTime();

            if (GenericUtils::DistSqrt({iround(_effCurOffset.x), iround(_effCurOffset.y)}, _effStartOffset) >= _effDist) {
                Finish();
            }
        }

        const auto dist = GenericUtils::DistSqrt({iround(_effCurOffset.x), iround(_effCurOffset.y)}, _effStartOffset);
        const auto proc = GenericUtils::Percent(_effDist, dist);
        const auto step_hex = _effSteps[_effSteps.size() * std::min(proc, 99u) / 100];

        if (const auto hex = GetMapHex(); hex != step_hex) {
            const auto [x, y] = _engine->Geometry.GetHexInterval(hex, step_hex);

            _effCurOffset.x -= static_cast<float>(x);
            _effCurOffset.y -= static_cast<float>(y);

            RefreshOffs();

            _map->MoveItem(this, step_hex);
        }
    }
}

void ItemHexView::SetEffect(mpos to_hex)
{
    STACK_TRACE_ENTRY();

    _isEffect = true;
    _isDynamicEffect = false;

    const auto cur_hex = GetMapHex();

    if (cur_hex != to_hex) {
        _isDynamicEffect = true;

        _effSteps.emplace_back(cur_hex);
        _map->TraceBullet(cur_hex, to_hex, 0, 0.0f, nullptr, CritterFindType::Any, nullptr, nullptr, &_effSteps, false);

        auto pos_offset = _engine->Geometry.GetHexInterval(cur_hex, to_hex);
        pos_offset.y += GenericUtils::Random(5, 25); // Center of body

        _effStepOffset = GenericUtils::GetStepsCoords({}, pos_offset);
        _effDist = GenericUtils::DistSqrt({}, pos_offset);
    }

    _effStartOffset = SprOffset;
    _effCurOffset = {static_cast<float>(SprOffset.x), static_cast<float>(SprOffset.y)};
    _effDir = GeometryHelper::GetFarDir(cur_hex, to_hex);
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
        _anim->SetDir(_effDir);
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

    const auto draw_offset = GetDrawOffset();

    SprOffset = ipos {draw_offset.x, draw_offset.y};

    if (GetIsTile()) {
        if (GetIsRoofTile()) {
            SprOffset.x += _engine->Settings.MapRoofOffsX;
            SprOffset.y += _engine->Settings.MapRoofOffsY;
        }
        else {
            SprOffset.x += _engine->Settings.MapTileOffsX;
            SprOffset.y += _engine->Settings.MapTileOffsY;
        }
    }

    if (_isDynamicEffect) {
        SprOffset.x += iround(_effCurOffset.x);
        SprOffset.y += iround(_effCurOffset.y);
    }
}
