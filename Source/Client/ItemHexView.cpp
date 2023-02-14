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
#include "Log.h"
#include "MapView.h"
#include "Sprites.h"
#include "Timer.h"

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto) :
    ItemView(map->GetEngine(), id, proto),
    _map {map}
{
    STACK_TRACE_ENTRY();

    DrawEffect = _engine->EffectMngr.Effects.Generic;
}

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto, const Properties& props) :
    ItemHexView(map, id, proto)
{
    STACK_TRACE_ENTRY();

    SetProperties(props);

    AfterConstruction();
}

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto, const vector<vector<uchar>>* props_data) :
    ItemHexView(map, id, proto)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(props_data);
    RestoreData(*props_data);

    AfterConstruction();
}

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto, const vector<vector<uchar>>* props_data, ushort hx, ushort hy) :
    ItemHexView(map, id, proto)
{
    STACK_TRACE_ENTRY();

    if (props_data != nullptr) {
        RestoreData(*props_data);
    }

    SetHexX(hx);
    SetHexY(hy);

    AfterConstruction();
}

void ItemHexView::AfterConstruction()
{
    STACK_TRACE_ENTRY();

    SetOwnership(ItemOwnership::MapHex);

    RefreshAnim();
    RefreshAlpha();

    if (GetIsShowAnim()) {
        _isShowAnim = true;
        _animNextTick = _engine->GameTime.GameTick() + GenericUtils::Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);
    }

    SetFade(true);
}

void ItemHexView::Finish()
{
    STACK_TRACE_ENTRY();

    SetFade(false);

    _finishing = true;
    _finishingTime = _fadingEndTick;

    if (_isEffect) {
        _finishingTime = _engine->GameTime.GameTick();
    }
}

auto ItemHexView::IsFinished() const -> bool
{
    STACK_TRACE_ENTRY();

    return _finishing && _engine->GameTime.GameTick() >= _finishingTime;
}

void ItemHexView::StopFinishing()
{
    STACK_TRACE_ENTRY();

    _finishing = false;
    SetFade(true);
    RefreshAnim();
}

void ItemHexView::Process()
{
    STACK_TRACE_ENTRY();

    // Animation
    if (_begFrm != _endFrm) {
        const auto anim_proc = GenericUtils::Percent(_anim->Ticks, _engine->GameTime.GameTick() - _animTick);
        if (anim_proc >= 100) {
            _begFrm = _animEndFrm;
            SetSpr(_endFrm);
            _animNextTick = _engine->GameTime.GameTick() + GetAnimWaitBase() * 10 + GenericUtils::Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);
        }
        else {
            const auto cur_spr = lerp(_begFrm, _endFrm, static_cast<float>(anim_proc) / 100.0f);
            RUNTIME_ASSERT((cur_spr >= _begFrm && cur_spr <= _endFrm) || (cur_spr >= _endFrm && cur_spr <= _begFrm));
            if (_curFrm != cur_spr) {
                SetSpr(cur_spr);
            }
        }
    }
    else if (_isEffect && !_finishing) {
        if (_isDynamicEffect) {
            SetAnimFromStart();
        }
        else {
            Finish();
        }
    }
    else if (_isShowAnim) {
        if (_engine->GameTime.GameTick() >= _animNextTick) {
            _isShowAnim = false;
            SetStayAnim();
        }
    }

    // Effect
    if (_isDynamicEffect && !_finishing) {
        const auto dt = static_cast<float>(_engine->GameTime.GameTick() - _effLastTick);
        if (dt > 0) {
            auto speed = GetFlyEffectSpeed();
            if (speed == 0.0f) {
                speed = 1.0f;
            }

            _effCurX += _effSx * dt * speed;
            _effCurY += _effSy * dt * speed;

            RefreshOffs();

            _effLastTick = _engine->GameTime.GameTick();

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

    // Fading
    if (_fading) {
        uint fading_proc;

        if (const auto tick = _engine->GameTime.GameTick(); tick < _fadingEndTick) {
            fading_proc = 100u - GenericUtils::Percent(_engine->Settings.FadingDuration, _fadingEndTick - tick);
        }
        else {
            fading_proc = 100u;
        }

        if (fading_proc == 100u) {
            _fading = false;
        }

        Alpha = static_cast<uchar>(_fadeUp ? fading_proc * 255u / 100u : (100u - fading_proc) * 255u / 100u);
        if (Alpha > _maxAlpha) {
            Alpha = _maxAlpha;
        }
    }
}

void ItemHexView::SetEffect(ushort to_hx, ushort to_hy)
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
    _effDir = _engine->Geometry.GetFarDir(from_hx, from_hy, to_hx, to_hy);
    _effLastTick = _engine->GameTime.GameTick();

    // Check off fade
    _fading = false;
    Alpha = _maxAlpha;

    RefreshAnim();
}

void ItemHexView::SetFade(bool fade_up)
{
    STACK_TRACE_ENTRY();

    const auto tick = _engine->GameTime.GameTick();
    _fadingEndTick = tick + _engine->Settings.FadingDuration - (_fadingEndTick > tick ? _fadingEndTick - tick : 0);
    _fadeUp = fade_up;
    _fading = true;
}

void ItemHexView::SkipFade()
{
    STACK_TRACE_ENTRY();

    if (_fading) {
        _fading = false;
        Alpha = _fadeUp ? _maxAlpha : 0;
    }
}

void ItemHexView::RefreshAnim()
{
    STACK_TRACE_ENTRY();

    _anim = nullptr;

    const auto pic_name = GetPicMap();
    if (pic_name) {
        _anim = _engine->ResMngr.GetItemAnim(pic_name);
    }
    if (pic_name && _anim == nullptr) {
        WriteLog("PicMap for item '{}' not found", GetName());
    }

    if (_anim != nullptr && _isEffect) {
        _anim = _anim->GetDir(_effDir);
    }
    if (_anim == nullptr) {
        _anim = _engine->ResMngr.ItemHexDefaultAnim;
    }

    SetStayAnim();
    _animBegFrm = _begFrm;
    _animEndFrm = _endFrm;

    if (GetIsCanOpen()) {
        if (GetOpened()) {
            SetSpr(_animEndFrm);
            _begFrm = _curFrm;
            _endFrm = _curFrm;
        }
        else {
            SetSpr(_animBegFrm);
            _begFrm = _curFrm;
            _endFrm = _curFrm;
        }
    }
}

void ItemHexView::SetSprite(Sprite* spr)
{
    STACK_TRACE_ENTRY();

    if (spr != nullptr) {
        SprDraw = spr;
    }

    if (SprDrawValid) {
        SprDraw->SetColor(IsColorize() ? GetColor() : 0);
        SprDraw->SetEggAppearence(GetEggType());
        if (GetIsBadItem()) {
            SprDraw->SetContour(ContourType::Red);
        }
    }
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
    default:
        return EggAppearenceType::ByX;
    }
}

void ItemHexView::SetAnimFromEnd()
{
    STACK_TRACE_ENTRY();

    _begFrm = _animEndFrm;
    _endFrm = _animBegFrm;
    SetSpr(_begFrm);
    _animTick = _engine->GameTime.GameTick();
}

void ItemHexView::SetAnimFromStart()
{
    STACK_TRACE_ENTRY();

    _begFrm = _animBegFrm;
    _endFrm = _animEndFrm;
    SetSpr(_begFrm);
    _animTick = _engine->GameTime.GameTick();
}

void ItemHexView::SetAnim(uint beg, uint end)
{
    STACK_TRACE_ENTRY();

    if (beg > _anim->CntFrm - 1) {
        beg = _anim->CntFrm - 1;
    }
    if (end > _anim->CntFrm - 1) {
        end = _anim->CntFrm - 1;
    }

    _begFrm = beg;
    _endFrm = end;
    SetSpr(_begFrm);
    _animTick = _engine->GameTime.GameTick();
}

void ItemHexView::SetSpr(uint num_spr)
{
    STACK_TRACE_ENTRY();

    _curFrm = num_spr;
    SprId = _anim->GetSprId(_curFrm);

    RefreshOffs();
}

void ItemHexView::RefreshOffs()
{
    STACK_TRACE_ENTRY();

    ScrX = GetOffsetX();
    ScrY = GetOffsetY();

    for (const auto i : xrange(_curFrm + 1u)) {
        ScrX += _anim->NextX[i];
        ScrY += _anim->NextY[i];
    }

    if (_isDynamicEffect) {
        ScrX += iround(_effCurX);
        ScrY += iround(_effCurY);
    }
}

void ItemHexView::SetStayAnim()
{
    STACK_TRACE_ENTRY();

    if (GetIsShowAnimExt()) {
        SetAnim(GetAnimStay0(), GetAnimStay1());
    }
    else {
        SetAnim(0, _anim->CntFrm - 1);
    }
}

void ItemHexView::SetShowAnim()
{
    STACK_TRACE_ENTRY();

    if (GetIsShowAnimExt()) {
        SetAnim(GetAnimShow0(), GetAnimShow1());
    }
    else {
        SetAnim(0, _anim->CntFrm - 1);
    }
}

void ItemHexView::SetHideAnim()
{
    STACK_TRACE_ENTRY();

    if (GetIsShowAnimExt()) {
        SetAnim(GetAnimHide0(), GetAnimHide1());
        _animBegFrm = GetAnimHide1();
        _animEndFrm = GetAnimHide1();
    }
    else {
        SetAnim(0, _anim->CntFrm - 1);
        _animBegFrm = _anim->CntFrm - 1;
        _animEndFrm = _anim->CntFrm - 1;
    }
}
