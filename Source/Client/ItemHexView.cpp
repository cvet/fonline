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

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto) : ItemView(map->GetEngine(), id, proto), _map {map}
{
    DrawEffect = _engine->EffectMngr.Effects.Generic;
}

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto, const Properties& props) : ItemHexView(map, id, proto)
{
    SetProperties(props);

    AfterConstruction();
}

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto, const vector<vector<uchar>>* props_data) : ItemHexView(map, id, proto)
{
    RUNTIME_ASSERT(props_data);
    RestoreData(*props_data);

    AfterConstruction();
}

ItemHexView::ItemHexView(MapView* map, uint id, const ProtoItem* proto, const vector<vector<uchar>>* props_data, ushort hx, ushort hy, int* hex_scr_x, int* hex_scr_y) : ItemHexView(map, id, proto)
{
    if (props_data != nullptr) {
        RestoreData(*props_data);
    }

    SetHexX(hx);
    SetHexY(hy);
    HexScrX = hex_scr_x;
    HexScrY = hex_scr_y;

    AfterConstruction();
}

void ItemHexView::AfterConstruction()
{
    SetOwnership(ItemOwnership::MapHex);

    RefreshAnim();
    RefreshAlpha();

    if (GetIsShowAnim()) {
        _isAnimated = true;
    }

    _animNextTick = _engine->GameTime.GameTick() + GenericUtils::Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);

    SetFade(true);
}

void ItemHexView::Finish()
{
    SetFade(false);

    _finishing = true;
    _finishingTime = _fadingTick;

    if (_isEffect) {
        _finishingTime = _engine->GameTime.GameTick();
    }
}

auto ItemHexView::IsFinishing() const -> bool
{
    return _finishing;
}

auto ItemHexView::IsFinished() const -> bool
{
    return _finishing && _engine->GameTime.GameTick() >= _finishingTime;
}

void ItemHexView::StopFinishing()
{
    _finishing = false;
    SetFade(true);
    RefreshAnim();
}

void ItemHexView::Process()
{
    // Animation
    if (_begSpr != _endSpr) {
        const auto anim_proc = GenericUtils::Percent(Anim->Ticks, _engine->GameTime.GameTick() - _animTick);
        if (anim_proc >= 100) {
            _begSpr = _animEndSpr;
            SetSpr(_endSpr);
            _animNextTick = _engine->GameTime.GameTick() + GetAnimWaitBase() * 10 + GenericUtils::Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);
        }
        else {
            const auto cur_spr = _begSpr + (_endSpr - _begSpr + (_begSpr < _endSpr ? 1 : -1)) * anim_proc / 100;
            if (_curSpr != cur_spr) {
                SetSpr(cur_spr);
            }
        }
    }
    else if (_isEffect && !IsFinishing()) {
        if (IsDynamicEffect()) {
            SetAnimFromStart();
        }
        else {
            Finish();
        }
    }
    else if (IsAnimated() && _engine->GameTime.GameTick() - _animTick >= Anim->Ticks) {
        if (_engine->GameTime.GameTick() >= _animNextTick) {
            SetStayAnim();
        }
    }

    // Effect
    if (IsDynamicEffect() && !IsFinishing()) {
        const auto dt = static_cast<float>(_engine->GameTime.GameTick() - _effLastTick);
        if (dt > 0) {
            auto speed = GetFlyEffectSpeed();
            if (speed == 0.0f) {
                speed = 1.0f;
            }

            _effCurX += _effSx * dt * speed;
            _effCurY += _effSy * dt * speed;

            SetAnimOffs();

            _effLastTick = _engine->GameTime.GameTick();

            if (GenericUtils::DistSqrt(iround(_effCurX), iround(_effCurY), _effStartX, _effStartY) >= _effDist) {
                Finish();
            }
        }

        auto&& [step_hx, step_hy] = GetEffectStep();
        if (GetHexX() != step_hx || GetHexY() != step_hy) {
            const auto hx = GetHexX();
            const auto hy = GetHexY();

            const auto [x, y] = _engine->Geometry.GetHexInterval(hx, hy, step_hx, step_hy);
            _effCurX -= static_cast<float>(x);
            _effCurY -= static_cast<float>(y);

            SetAnimOffs();

            _map->MoveItem(this, step_hx, step_hy);
        }
    }

    // Fading
    if (_fading) {
        auto fading_proc = 100u - GenericUtils::Percent(FADING_PERIOD, _fadingTick - _engine->GameTime.GameTick());
        fading_proc = std::clamp(fading_proc, 0u, 100u);
        if (fading_proc >= 100u) {
            fading_proc = 100u;
            _fading = false;
        }

        Alpha = static_cast<uchar>((_fadeUp ? fading_proc * 255u / 100u : (100u - fading_proc) * 255u / 100u));
        if (Alpha > _maxAlpha) {
            Alpha = _maxAlpha;
        }
    }
}

void ItemHexView::SetEffect(float sx, float sy, uint dist, int dir)
{
    // Init effect
    _effSx = sx;
    _effSy = sy;
    _effDist = dist;
    _effStartX = ScrX;
    _effStartY = ScrY;
    _effCurX = static_cast<float>(ScrX);
    _effCurY = static_cast<float>(ScrY);
    _effDir = dir;
    _effLastTick = _engine->GameTime.GameTick();
    _isEffect = true;

    // Check off fade
    _fading = false;
    Alpha = _maxAlpha;

    // Refresh effect animation dir
    RefreshAnim();
}

auto ItemHexView::GetEffectStep() const -> pair<ushort, ushort>
{
    auto dist = GenericUtils::DistSqrt(iround(_effCurX), iround(_effCurY), _effStartX, _effStartY);
    if (dist > _effDist) {
        dist = _effDist;
    }

    auto proc = GenericUtils::Percent(_effDist, dist);
    if (proc > 99) {
        proc = 99;
    }

    return EffSteps[EffSteps.size() * proc / 100];
}

void ItemHexView::SetFade(bool fade_up)
{
    const auto tick = _engine->GameTime.GameTick();
    _fadingTick = tick + FADING_PERIOD - (_fadingTick > tick ? _fadingTick - tick : 0);
    _fadeUp = fade_up;
    _fading = true;
}

void ItemHexView::SkipFade()
{
    if (_fading) {
        _fading = false;
        Alpha = _fadeUp ? _maxAlpha : 0;
    }
}

void ItemHexView::RefreshAnim()
{
    Anim = nullptr;

    const auto pic_name = GetPicMap();
    if (pic_name) {
        Anim = _engine->ResMngr.GetItemAnim(pic_name);
    }
    if (pic_name && Anim == nullptr) {
        WriteLog("PicMap for item '{}' not found", GetName());
    }

    if (Anim != nullptr && _isEffect) {
        Anim = Anim->GetDir(_effDir);
    }
    if (Anim == nullptr) {
        Anim = _engine->ResMngr.ItemHexDefaultAnim;
    }

    SetStayAnim();
    _animBegSpr = _begSpr;
    _animEndSpr = _endSpr;

    if (GetIsCanOpen()) {
        if (GetOpened()) {
            SetSprEnd();
        }
        else {
            SetSprStart();
        }
    }
}

void ItemHexView::SetSprite(Sprite* spr)
{
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

void ItemHexView::StartAnimate()
{
    SetStayAnim();
    _animNextTick = _engine->GameTime.GameTick() + GetAnimWaitBase() * 10 + GenericUtils::Random(GetAnimWaitRndMin() * 10, GetAnimWaitRndMax() * 10);
    _isAnimated = true;
}

void ItemHexView::StopAnimate()
{
    SetSpr(_animBegSpr);
    _begSpr = _animBegSpr;
    _endSpr = _animBegSpr;
    _isAnimated = false;
}

void ItemHexView::SetAnimFromEnd()
{
    _begSpr = _animEndSpr;
    _endSpr = _animBegSpr;
    SetSpr(_begSpr);
    _animTick = _engine->GameTime.GameTick();
}

void ItemHexView::SetAnimFromStart()
{
    _begSpr = _animBegSpr;
    _endSpr = _animEndSpr;
    SetSpr(_begSpr);
    _animTick = _engine->GameTime.GameTick();
}

void ItemHexView::SetAnim(uint beg, uint end)
{
    if (beg > Anim->CntFrm - 1) {
        beg = Anim->CntFrm - 1;
    }
    if (end > Anim->CntFrm - 1) {
        end = Anim->CntFrm - 1;
    }

    _begSpr = beg;
    _endSpr = end;
    SetSpr(_begSpr);
    _animTick = _engine->GameTime.GameTick();
}

void ItemHexView::SetSprStart()
{
    SetSpr(_animBegSpr);
    _begSpr = _curSpr;
    _endSpr = _curSpr;
}

void ItemHexView::SetSprEnd()
{
    SetSpr(_animEndSpr);
    _begSpr = _curSpr;
    _endSpr = _curSpr;
}

void ItemHexView::SetSpr(uint num_spr)
{
    _curSpr = num_spr;
    SprId = Anim->GetSprId(_curSpr);
    SetAnimOffs();
}

void ItemHexView::SetAnimOffs()
{
    ScrX = GetOffsetX();
    ScrY = GetOffsetY();

    for (const auto i : xrange(_curSpr + 1u)) {
        ScrX += Anim->NextX[i];
        ScrY += Anim->NextY[i];
    }

    if (IsDynamicEffect()) {
        ScrX += iround(_effCurX);
        ScrY += iround(_effCurY);
    }
}

void ItemHexView::SetStayAnim()
{
    if (GetIsShowAnimExt()) {
        SetAnim(GetAnimStay0(), GetAnimStay1());
    }
    else {
        SetAnim(0, Anim->CntFrm - 1);
    }
}

void ItemHexView::SetShowAnim()
{
    if (GetIsShowAnimExt()) {
        SetAnim(GetAnimShow0(), GetAnimShow1());
    }
    else {
        SetAnim(0, Anim->CntFrm - 1);
    }
}

void ItemHexView::SetHideAnim()
{
    if (GetIsShowAnimExt()) {
        SetAnim(GetAnimHide0(), GetAnimHide1());
        _animBegSpr = GetAnimHide1();
        _animEndSpr = GetAnimHide1();
    }
    else {
        SetAnim(0, Anim->CntFrm - 1);
        _animBegSpr = Anim->CntFrm - 1;
        _animEndSpr = Anim->CntFrm - 1;
    }
}
