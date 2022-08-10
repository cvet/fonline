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

#include "CritterHexView.h"
#include "Client.h"
#include "EffectManager.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"
#include "ItemView.h"
#include "MapView.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"

CritterHexView::CritterHexView(MapView* map, uint id, const ProtoCritter* proto) : CritterView(map->GetEngine(), id, proto), _map {map}
{
    _tickFidget = _engine->GameTime.GameTick() + GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2u);
    DrawEffect = _engine->EffectMngr.Effects.Critter;
    auto layers = GetModelLayers();
    layers.resize(LAYERS3D_COUNT);
    SetModelLayers(layers);
}

CritterHexView::~CritterHexView()
{
#if FO_ENABLE_3D
    if (_model != nullptr) {
        _engine->SprMngr.FreeModel(_model);
    }
#endif
}

void CritterHexView::Init()
{
    CritterView::Init();

#if FO_ENABLE_3D
    RefreshModel();
#endif

    RefreshSpeed();
    AnimateStay();
    RefreshOffs();
    SetFade(true);
}

void CritterHexView::Finish()
{
    CritterView::Finish();

    SetFade(false);
    _finishingTime = FadingTick;
}

auto CritterHexView::IsFinishing() const -> bool
{
    return _finishingTime != 0;
}

auto CritterHexView::IsFinished() const -> bool
{
    return _finishingTime != 0u && _engine->GameTime.GameTick() > _finishingTime;
}

void CritterHexView::SetFade(bool fade_up)
{
    const auto tick = _engine->GameTime.GameTick();
    FadingTick = tick + FADING_PERIOD - (FadingTick > tick ? FadingTick - tick : 0);
    _fadeUp = fade_up;
    _fadingEnable = true;
}

auto CritterHexView::GetFadeAlpha() -> uchar
{
    const auto tick = _engine->GameTime.GameTick();
    const auto fading_proc = 100u - GenericUtils::Percent(FADING_PERIOD, FadingTick > tick ? FadingTick - tick : 0u);
    if (fading_proc == 100u) {
        _fadingEnable = false;
    }

    const auto alpha = (_fadeUp ? fading_proc * 255u / 100u : (100u - fading_proc) * 255u / 100u);
    return static_cast<uchar>(alpha);
}

auto CritterHexView::AddItem(uint id, const ProtoItem* proto, uchar slot, const vector<vector<uchar>>& properties_data) -> ItemView*
{
    auto* item = CritterView::AddItem(id, proto, slot, properties_data);

    if (item->GetCritterSlot() != 0u && !IsAnim()) {
        AnimateStay();
    }

    return item;
}

void CritterHexView::DeleteItem(ItemView* item, bool animate)
{
    CritterView::DeleteItem(item, animate);

    if (animate && !IsAnim()) {
        AnimateStay();
    }
}

auto CritterHexView::GetAttackDist() -> uint
{
    uint dist = 0;
    _engine->OnCritterGetAttackDistantion.Fire(this, nullptr, 0, dist);
    return dist;
}

// Todo: restore 2D sprite moving
/*void CritterHexView::Move(uchar dir)
{
    if (dir >= _engine->Settings.MapDirCount || GetIsNoRotate()) {
        dir = 0;
    }

    SetDir(dir);

    const auto time_move = 1000 * 32 / (IsRunning ? GetRunSpeed() : GetWalkSpeed());

    TickStart(time_move);
    _animStartTick = _engine->GameTime.GameTick();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        auto anim1 = GetAnim1();
        uint anim2 = IsRunning ? ANIM2_RUN : ANIM2_WALK;
        if (GetIsHide()) {
            anim2 = IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK;
        }

        if (const auto model_anim = _model->EvaluateAnimation()) {
            anim1 = std::get<0>(*model_anim);
            anim2 = std::get<1>(*model_anim);
        }

        _model->SetDir(dir);

        ClearAnim();
        _animSequence.push_back({nullptr, time_move, 0, 0, true, dir + 1, anim1, anim2, nullptr});
        NextAnim(false);

        const auto [ox, oy] = GetWalkHexOffsets(dir);
        ChangeOffs(ox, oy, true);

        return;
    }
#endif

    if (_str(GetModelName()).startsWith("art/critters/")) {
        const auto anim1 = IsRunning ? ANIM1_UNARMED : GetAnim1();
        const uint anim2 = IsRunning ? ANIM2_RUN : ANIM2_WALK;
        auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, dir);
        if (anim == nullptr) {
            anim = _engine->ResMngr.CritterDefaultAnim;
        }

        uint step;
        uint beg_spr;
        uint end_spr;

        _curSpr = _lastEndSpr;

        if (!IsRunning) {
            const auto s0 = 4u;
            const auto s1 = 8u;

            if (_curSpr == s0 - 1u) {
                beg_spr = s0;
                end_spr = s1 - 1;
                step = 2;
            }
            else {
                beg_spr = 0;
                end_spr = s0 - 1;
                step = 1;
            }
        }
        else {
            switch (_curSpr) {
            default:
            case 0:
                beg_spr = 0;
                end_spr = 1;
                step = 1;
                break;
            case 1:
                beg_spr = 2;
                end_spr = 3;
                step = 2;
                break;
            case 3:
                beg_spr = 4;
                end_spr = 6;
                step = 3;
                break;
            case 6:
                beg_spr = 7;
                end_spr = anim->CntFrm - 1;
                step = 4;
                break;
            }
        }

        ClearAnim();
        _animSequence.push_back({anim, time_move, beg_spr, end_spr, true, 0, anim1, anim2, nullptr});
        NextAnim(false);

        for (uint i = 0; i < step; i++) {
            auto [ox, oy] = GetWalkHexOffsets(dir);
            ChangeOffs(ox, oy, true);
        }
    }
    else {
        const auto anim1 = GetAnim1();
        uint anim2 = IsRunning ? ANIM2_RUN : ANIM2_WALK;
        if (GetIsHide()) {
            anim2 = IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK;
        }

        auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, dir);
        if (anim == nullptr) {
            anim = _engine->ResMngr.CritterDefaultAnim;
        }

        auto m1 = 0;
        if (m1 <= 0) {
            m1 = 5;
        }
        auto m2 = 0;
        if (m2 <= 0) {
            m2 = 2;
        }

        const auto beg_spr = _lastEndSpr + 1;
        const auto end_spr = beg_spr + (IsRunning ? m2 : m1);

        ClearAnim();
        _animSequence.push_back({anim, time_move, beg_spr, end_spr, true, dir + 1, anim1, anim2, nullptr});
        NextAnim(false);

        const auto [ox, oy] = GetWalkHexOffsets(dir);
        ChangeOffs(ox, oy, true);
    }
}*/

void CritterHexView::RefreshSpeed()
{
    NON_CONST_METHOD_HINT();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        auto scale_factor = static_cast<float>(GetScaleFactor());
        if (scale_factor == 0.0f) {
            scale_factor = 1.0f;
        }
        else {
            scale_factor /= 1000.0f;
        }

        _model->SetMoveSpeed(static_cast<float>(GetWalkSpeed()) / 80.0f / scale_factor, static_cast<float>(GetRunSpeed()) / 160.0f / scale_factor);
    }
#endif
}

void CritterHexView::RefreshCombatMode()
{
#if FO_ENABLE_3D
    const auto is_combat = GetTimeoutBattle() > _engine->GameTime.GetFullSecond();

    if (is_combat != _model->IsCombatMode()) {
        if (_engine->Settings.Anim2CombatIdle != 0u && _animSequence.empty() && GetCond() == CritterCondition::Alive && GetAnim2Alive() == 0u && !IsMoving()) {
            if (_engine->Settings.Anim2CombatBegin != 0u && is_combat && _model->GetAnim2() != _engine->Settings.Anim2CombatIdle) {
                Animate(0, _engine->Settings.Anim2CombatBegin, nullptr);
            }
            else if (_engine->Settings.Anim2CombatEnd != 0u && !is_combat && _model->GetAnim2() == _engine->Settings.Anim2CombatIdle) {
                Animate(0, _engine->Settings.Anim2CombatEnd, nullptr);
            }
        }

        _model->SetCombatMode(is_combat);
    }
#endif
}

void CritterHexView::ClearMove()
{
    Moving.Steps = {};
    Moving.ControlSteps = {};
    Moving.StartTick = {};
    Moving.OffsetTick = {};
    Moving.StartHexX = {};
    Moving.StartHexY = {};
    Moving.EndHexX = {};
    Moving.EndHexY = {};
    Moving.WholeTime = {};
    Moving.WholeDist = {};
    Moving.StartOx = {};
    Moving.StartOy = {};
    Moving.EndOx = {};
    Moving.EndOy = {};
}

void CritterHexView::Action(int action, int action_ext, ItemView* item, bool local_call /* = true */)
{
    _engine->OnCritterAction.Fire(local_call, this, action, action_ext, item);

    switch (action) {
    case ACTION_KNOCKOUT:
        SetCond(CritterCondition::Knockout);
        SetAnim2Knockout(action_ext);
        break;
    case ACTION_STANDUP:
        SetCond(CritterCondition::Alive);
        break;
    case ACTION_DEAD: {
        SetCond(CritterCondition::Dead);
        SetAnim2Dead(action_ext);
        auto* anim = GetCurAnim();
        _needReset = true;
#if FO_ENABLE_3D
        if (_model != nullptr) {
            _resetTick = _engine->GameTime.GameTick() + _model->GetAnimDuration();
        }
        else {
            _resetTick = _engine->GameTime.GameTick() + (anim != nullptr && anim->Anim != nullptr ? anim->Anim->Ticks : 1000);
        }
#else
        _resetTick = _engine->GameTime.GameTick() + (anim != nullptr && anim->Anim != nullptr ? anim->Anim->Ticks : 1000);
#endif
    } break;
    case ACTION_CONNECT:
        SetPlayerOffline(false);
        break;
    case ACTION_DISCONNECT:
        SetPlayerOffline(true);
        break;
    case ACTION_RESPAWN:
        SetCond(CritterCondition::Alive);
        Alpha = 0;
        SetFade(true);
        AnimateStay();
        _needReset = true;
        _resetTick = _engine->GameTime.GameTick(); // Fast
        break;
    case ACTION_REFRESH:
#if FO_ENABLE_3D
        if (_model != nullptr) {
            _model->StartMeshGeneration();
        }
#endif
        break;
    default:
        break;
    }

    if (!IsAnim()) {
        AnimateStay();
    }
}

void CritterHexView::NextAnim(bool erase_front)
{
    if (_animSequence.empty()) {
        return;
    }
    if (erase_front) {
        _animSequence.begin()->ActiveItem->Release();
        _animSequence.erase(_animSequence.begin());
    }
    if (_animSequence.empty()) {
        return;
    }

    const auto& cr_anim = _animSequence[0];
    _animStartTick = _engine->GameTime.GameTick();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        SetAnimOffs(0, 0);

        _model->SetAnimation(cr_anim.IndAnim1, cr_anim.IndAnim2, GetLayers3dData(), ANIMATION_ONE_TIME | ANIMATION_NO_ROTATE);

        return;
    }
#endif

    ProcessAnim(false, cr_anim.IndAnim1, cr_anim.IndAnim2, cr_anim.ActiveItem);

    _lastEndSpr = cr_anim.EndFrm;
    _curSpr = cr_anim.BeginFrm;

    SprId = cr_anim.Anim->GetSprId(_curSpr);

    short ox = 0;
    short oy = 0;
    for (const auto i : xrange(_curSpr % cr_anim.Anim->CntFrm + 1u)) {
        ox = static_cast<short>(ox + cr_anim.Anim->NextX[i]);
        oy = static_cast<short>(oy + cr_anim.Anim->NextY[i]);
    }

    SetAnimOffs(ox, oy);
}

void CritterHexView::Animate(uint anim1, uint anim2, ItemView* item)
{
    const auto dir = GetDir();
    if (anim1 == 0u) {
        anim1 = GetAnim1();
    }
    if (item != nullptr) {
        item = item->CreateRefClone();
    }

#if FO_ENABLE_3D
    if (_model != nullptr) {
        if (_model->ResolveAnimation(anim1, anim2)) {
            _animSequence.push_back({nullptr, 0, 0, 0, anim1, anim2, item});
            if (_animSequence.size() == 1) {
                NextAnim(false);
            }
        }
        else {
            if (!IsAnim()) {
                AnimateStay();
            }
        }

        return;
    }
#endif

    auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, dir);
    if (anim == nullptr) {
        if (!IsAnim()) {
            AnimateStay();
        }
        return;
    }

    _animSequence.push_back({anim, anim->Ticks, 0, anim->CntFrm - 1, anim->Anim1, anim->Anim2, item});
    if (_animSequence.size() == 1) {
        NextAnim(false);
    }
}

void CritterHexView::AnimateStay()
{
    auto anim1 = GetAnim1();
    auto anim2 = GetAnim2();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        if (IsMoving()) {
            _model->SetMoving(true, Moving.IsRunning);

            anim2 = Moving.IsRunning ? ANIM2_RUN : ANIM2_WALK;

            if (GetIsHide()) {
                anim2 = Moving.IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK;
            }
        }
        else {
            _model->SetMoving(false, Moving.IsRunning);

            anim2 = GetAnim2();
        }

        const auto scale_factor = GetScaleFactor();
        if (scale_factor != 0) {
            const auto scale = static_cast<float>(scale_factor) / 1000.0f;
            _model->SetScale(scale, scale, scale);
        }

        if (!_model->ResolveAnimation(anim1, anim2)) {
            anim1 = ANIM1_UNARMED;
            anim2 = ANIM2_IDLE;
        }

        ProcessAnim(false, anim1, anim2, nullptr);

        if (GetCond() == CritterCondition::Alive || GetCond() == CritterCondition::Knockout) {
            _model->SetAnimation(anim1, anim2, GetLayers3dData(), 0);
        }
        else {
            _model->SetAnimation(anim1, anim2, GetLayers3dData(), ANIMATION_STAY | ANIMATION_PERIOD(100));
        }

        return;
    }
#endif

    auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, GetDir());
    if (anim == nullptr) {
        anim = _engine->ResMngr.CritterDefaultAnim;
    }

    if (_stayAnim.Anim != anim) {
        ProcessAnim(true, anim1, anim2, nullptr);

        _stayAnim.Anim = anim;
        _stayAnim.AnimTick = anim->Ticks;
        _stayAnim.BeginFrm = 0;
        _stayAnim.EndFrm = anim->CntFrm - 1;
        if (GetCond() == CritterCondition::Dead) {
            _stayAnim.BeginFrm = _stayAnim.EndFrm;
        }
        _curSpr = _stayAnim.BeginFrm;
    }

    SprId = anim->GetSprId(_curSpr);

    int ox = 0;
    int oy = 0;
    for (const auto i : xrange(_curSpr % anim->CntFrm + 1u)) {
        ox += anim->NextX[i];
        oy += anim->NextY[i];
    }

    SetAnimOffs(ox, oy);
}

auto CritterHexView::IsWalkAnim() const -> bool
{
    if (!_animSequence.empty()) {
        const auto anim2 = _animSequence.front().IndAnim2;
        return anim2 == ANIM2_WALK || anim2 == ANIM2_RUN || anim2 == ANIM2_LIMP || anim2 == ANIM2_PANIC_RUN;
    }
    return false;
}

void CritterHexView::ClearAnim()
{
    for (const auto& anim : _animSequence) {
        anim.ActiveItem->Release();
    }
    _animSequence.clear();
}

auto CritterHexView::IsHaveLightSources() const -> bool
{
    for (const auto* item : _items) {
        if (item->GetIsLight()) {
            return true;
        }
    }
    return false;
}

auto CritterHexView::IsNeedReset() const -> bool
{
    return _needReset && _engine->GameTime.GameTick() >= _resetTick;
}

void CritterHexView::ResetOk()
{
    _needReset = false;
}

auto CritterHexView::GetAnim1() const -> uint
{
    switch (GetCond()) {
    case CritterCondition::Alive:
        return GetAnim1Alive() != 0u ? GetAnim1Alive() : ANIM1_UNARMED;
    case CritterCondition::Knockout:
        return GetAnim1Knockout() != 0u ? GetAnim1Knockout() : ANIM1_UNARMED;
    case CritterCondition::Dead:
        return GetAnim1Dead() != 0u ? GetAnim1Dead() : ANIM1_UNARMED;
    }
    return ANIM1_UNARMED;
}

auto CritterHexView::GetAnim2() const -> uint
{
    switch (GetCond()) {
    case CritterCondition::Alive:
#if FO_ENABLE_3D
        return GetAnim2Alive() != 0u ? GetAnim2Alive() : (_model != nullptr && _model->IsCombatMode() && _engine->Settings.Anim2CombatIdle != 0u ? _engine->Settings.Anim2CombatIdle : ANIM2_IDLE);
#else
        return GetAnim2Alive() != 0u ? GetAnim2Alive() : ANIM2_IDLE;
#endif
    case CritterCondition::Knockout:
        return GetAnim2Knockout() != 0u ? GetAnim2Knockout() : ANIM2_IDLE_PRONE_FRONT;
    case CritterCondition::Dead:
        return GetAnim2Dead() != 0u ? GetAnim2Dead() : ANIM2_DEAD_FRONT;
    }
    return ANIM2_IDLE;
}

void CritterHexView::ProcessAnim(bool animate_stay, uint anim1, uint anim2, ItemView* item)
{
    _engine->OnCritterAnimationProcess.Fire(animate_stay, this, anim1, anim2, item);
}

auto CritterHexView::GetLayers3dData() -> int*
{
    const auto layers = GetModelLayers();
    RUNTIME_ASSERT(layers.size() == LAYERS3D_COUNT);
    std::memcpy(_modelLayers, layers.data(), sizeof(_modelLayers));
    return _modelLayers;
}

auto CritterHexView::IsAnimAvailable(uint anim1, uint anim2) const -> bool
{
    if (anim1 == 0u) {
        anim1 = GetAnim1();
    }

#if FO_ENABLE_3D
    if (_model != nullptr) {
        return _model->HasAnimation(anim1, anim2);
    }
#endif

    return _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, GetDir()) != nullptr;
}

#if FO_ENABLE_3D
void CritterHexView::RefreshModel()
{
    // Release previous
    if (_model != nullptr) {
        _engine->SprMngr.FreeModel(_model);
        _model = nullptr;
    }

    // Load new
    const string ext = _str(GetModelName()).getFileExtension();
    if (ext == "fo3d") {
        _engine->SprMngr.PushAtlasType(AtlasType::Dynamic);

        auto* model = _engine->SprMngr.LoadModel(GetModelName(), true);
        if (model != nullptr) {
            _model = model;

            _model->SetLookDirAngle(GetDirAngle());
            _model->SetMoveDirAngle(GetDirAngle(), false);

            SprId = _model->SprId;

            _model->SetAnimation(ANIM1_UNARMED, ANIM2_IDLE, GetLayers3dData(), 0);

            if (_map->IsMapperMode()) {
                _model->StartMeshGeneration();
            }
        }
        else {
            BreakIntoDebugger();
        }

        _engine->SprMngr.PopAtlasType();
    }
}
#endif

void CritterHexView::ChangeDir(uchar dir)
{
    ChangeDirAngle(_engine->Geometry.DirToAngle(dir));
}

void CritterHexView::ChangeDirAngle(int dir_angle)
{
    ChangeLookDirAngle(dir_angle);
    ChangeMoveDirAngle(dir_angle);
}

void CritterHexView::ChangeLookDirAngle(int dir_angle)
{
    const auto normalized_dir_angle = _engine->Geometry.NormalizeAngle(static_cast<short>(dir_angle));

    if (normalized_dir_angle == GetDirAngle()) {
        return;
    }

    SetDirAngle(normalized_dir_angle);
    SetDir(_engine->Geometry.AngleToDir(normalized_dir_angle));

#if FO_ENABLE_3D
    if (_model != nullptr) {
        _model->SetLookDirAngle(normalized_dir_angle);
    }
#endif

    if (!IsAnim()) {
        AnimateStay();
    }
}

void CritterHexView::ChangeMoveDirAngle(int dir_angle)
{
    NON_CONST_METHOD_HINT();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        _model->SetMoveDirAngle(dir_angle, true);
    }
#endif
}

void CritterHexView::Process()
{
    // Fading
    if (_fadingEnable) {
        Alpha = GetFadeAlpha();
    }

    // Extra offsets
    if (_offsExtNextTick != 0u && _engine->GameTime.GameTick() >= _offsExtNextTick) {
        _offsExtNextTick = _engine->GameTime.GameTick() + 30;

        const auto dist = GenericUtils::DistSqrt(0, 0, _oxExtI, _oyExtI);
        const auto dist_div = dist / 10u;
        auto mul = static_cast<float>(dist_div);
        if (mul < 1.0f) {
            mul = 1.0f;
        }

        _oxExtF += _oxExtSpeed * mul;
        _oyExtF += _oyExtSpeed * mul;
        _oxExtI = static_cast<short>(_oxExtF);
        _oyExtI = static_cast<short>(_oyExtF);

        if (GenericUtils::DistSqrt(0, 0, _oxExtI, _oyExtI) > dist) {
            _offsExtNextTick = 0;
            _oxExtI = 0;
            _oyExtI = 0;
        }

        RefreshOffs();
    }

    // Animation
    const auto& cr_anim = !_animSequence.empty() ? _animSequence.front() : _stayAnim;
    auto anim_proc = (_engine->GameTime.GameTick() - _animStartTick) * 100u / (cr_anim.AnimTick != 0u ? cr_anim.AnimTick : 1u);
    if (anim_proc >= 100u) {
        if (!_animSequence.empty()) {
            anim_proc = 100u;
        }
        else {
            anim_proc %= 100u;
        }
    }

    // Change frames
#if FO_ENABLE_3D
    const auto is_no_model = _model == nullptr;
#else
    constexpr auto is_no_model = true;
#endif
    if (is_no_model && anim_proc < 100u) {
        const auto cur_spr = cr_anim.BeginFrm + (cr_anim.EndFrm - cr_anim.BeginFrm + 1u) * anim_proc / 100u;
        if (cur_spr != _curSpr) {
            // Change frame
            const auto old_spr = _curSpr;
            _curSpr = cur_spr;
            SprId = cr_anim.Anim->GetSprId(_curSpr);

            // Change offsets
            short ox = 0;
            short oy = 0;

            for (uint i = 0, j = old_spr % cr_anim.Anim->CntFrm; i <= j; i++) {
                ox = static_cast<short>(ox - cr_anim.Anim->NextX[i]);
                oy = static_cast<short>(oy - cr_anim.Anim->NextY[i]);
            }
            for (uint i = 0, j = cur_spr % cr_anim.Anim->CntFrm; i <= j; i++) {
                ox = static_cast<short>(ox + cr_anim.Anim->NextX[i]);
                oy = static_cast<short>(oy + cr_anim.Anim->NextY[i]);
            }

            SetAnimOffs(ox, oy);
        }
    }

    if (!_animSequence.empty()) {
        // Move offsets
        if (false) {
            // auto&& [ox, oy] = GetWalkHexOffsets(static_cast<uchar>(cr_anim.DirOffs - 1));
            // SetOffs(static_cast<short>(ox - ox * anim_proc / 100), static_cast<short>(oy - oy * anim_proc / 100), true);

            if (anim_proc >= 100u) {
                NextAnim(true);
            }
        }
        else {
#if FO_ENABLE_3D
            if (_model != nullptr && !_model->IsAnimationPlaying()) {
                NextAnim(true);
            }
            else if (_model == nullptr && anim_proc >= 100u) {
                NextAnim(true);
            }
#else
            if (anim_proc >= 100u) {
                NextAnim(true);
            }
#endif
        }

        if (_animSequence.empty()) {
            AnimateStay();
        }
    }

    // Fidget animation
    if (_engine->GameTime.GameTick() >= _tickFidget) {
#if FO_ENABLE_3D
        const auto is_combat_mode = _model != nullptr && _model->IsCombatMode();
#else
        constexpr auto is_combat_mode = false;
#endif

        if (_animSequence.empty() && GetCond() == CritterCondition::Alive && !IsMoving() && is_combat_mode) {
            Action(ACTION_FIDGET, 0, nullptr, false);
        }

        _tickFidget = _engine->GameTime.GameTick() + GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2);
    }

    RefreshCombatMode();
}

void CritterHexView::ProcessMoving()
{
    RUNTIME_ASSERT(!Moving.Steps.empty());
    RUNTIME_ASSERT(!Moving.ControlSteps.empty());
    RUNTIME_ASSERT(Moving.WholeTime > 0.0f);
    RUNTIME_ASSERT(Moving.WholeDist > 0.0f);

    if (!IsAlive()) {
        ClearMove();
        AnimateStay();
        return;
    }

    auto normalized_time = static_cast<float>(_engine->GameTime.FrameTick() - Moving.StartTick + Moving.OffsetTick) / Moving.WholeTime;
    normalized_time = std::clamp(normalized_time, 0.0f, 1.0f);

    const auto dist_pos = Moving.WholeDist * normalized_time;

    auto start_hx = Moving.StartHexX;
    auto start_hy = Moving.StartHexY;
    auto cur_dist = 0.0f;

    auto done = false;
    auto control_step_begin = 0;
    for (size_t i = 0; i < Moving.ControlSteps.size(); i++) {
        auto hx = start_hx;
        auto hy = start_hy;

        RUNTIME_ASSERT(control_step_begin <= Moving.ControlSteps[i]);
        RUNTIME_ASSERT(Moving.ControlSteps[i] <= Moving.Steps.size());
        for (auto j = control_step_begin; j < Moving.ControlSteps[i]; j++) {
            const auto move_ok = _engine->Geometry.MoveHexByDir(hx, hy, Moving.Steps[j], _map->GetWidth(), _map->GetHeight());
            RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = _engine->Geometry.GetHexInterval(start_hx, start_hy, hx, hy);

        if (i == 0) {
            ox -= Moving.StartOx;
            oy -= Moving.StartOy;
        }
        if (i == Moving.ControlSteps.size() - 1) {
            ox += Moving.EndOx;
            oy += Moving.EndOy;
        }

        const auto proj_oy = static_cast<float>(oy) * _engine->Geometry.GetYProj();
        auto dist = std::sqrt(static_cast<float>(ox * ox) + proj_oy * proj_oy);
        if (dist < 0.0001f) {
            dist = 0.0001f;
        }

        if ((normalized_time < 1.0f && dist_pos >= cur_dist && dist_pos <= cur_dist + dist) || (normalized_time == 1.0f && i == Moving.ControlSteps.size() - 1)) {
            auto normalized_dist = (dist_pos - cur_dist) / dist;
            normalized_dist = std::clamp(normalized_dist, 0.0f, 1.0f);
            if (normalized_time == 1.0f) {
                normalized_dist = 1.0f;
            }

            // Evaluate current hex
            const auto step_index_f = std::round(normalized_dist * static_cast<float>(Moving.ControlSteps[i] - control_step_begin));
            const auto step_index = control_step_begin + static_cast<int>(step_index_f);
            RUNTIME_ASSERT(step_index >= control_step_begin);
            RUNTIME_ASSERT(step_index <= Moving.ControlSteps[i]);

            auto hx2 = start_hx;
            auto hy2 = start_hy;

            for (auto j2 = control_step_begin; j2 < step_index; j2++) {
                const auto move_ok = _engine->Geometry.MoveHexByDir(hx2, hy2, Moving.Steps[j2], _map->GetWidth(), _map->GetHeight());
                RUNTIME_ASSERT(move_ok);
            }

            const auto old_hx = GetHexX();
            const auto old_hy = GetHexY();

            _map->TransitCritter(this, hx2, hy2, false);

            const auto cur_hx = GetHexX();
            const auto cur_hy = GetHexY();
            const auto moved = (cur_hx != old_hx || cur_hy != old_hy);

            if (moved) {
                ResetOk();

                if (IsChosen()) {
                    // RebuildLookBorders = true;
                }
            }

            // Evaluate current position
            const auto cr_hx = GetHexX();
            const auto cr_hy = GetHexY();

            auto&& [cr_ox, cr_oy] = _engine->Geometry.GetHexInterval(start_hx, start_hy, cr_hx, cr_hy);

            if (i == 0) {
                cr_ox -= Moving.StartOx;
                cr_oy -= Moving.StartOy;
            }

            const auto lerp = [](int a, int b, float t) { return static_cast<float>(a) * (1.0f - t) + static_cast<float>(b) * t; };

            auto mx = lerp(0, ox, normalized_dist);
            auto my = lerp(0, oy, normalized_dist);
            mx -= static_cast<float>(cr_ox);
            my -= static_cast<float>(cr_oy);

            const auto mxi = static_cast<short>(std::round(mx));
            const auto myi = static_cast<short>(std::round(my));
            if (moved || GetHexOffsX() != mxi || GetHexOffsY() != myi) {
#if FO_ENABLE_3D
                if (_model != nullptr) {
                    int model_ox = 0;
                    int model_oy = 0;
                    if (moved) {
                        std::tie(model_ox, model_oy) = _engine->Geometry.GetHexInterval(old_hx, old_hy, cur_hx, cur_hy);
                    }
                    model_ox -= GetHexOffsX() - mxi;
                    model_oy -= GetHexOffsY() - myi;
                    _model->MoveModel(model_ox, model_oy);
                }
#endif

                SetHexOffsX(mxi);
                SetHexOffsY(myi);
                RefreshOffs();
            }

            // Evaluate dir angle
            const auto dir_angle = _engine->Geometry.GetLineDirAngle(0, 0, ox, oy);
            ChangeMoveDirAngle(static_cast<int>(dir_angle));

            done = true;
        }

        if (done) {
            break;
        }

        RUNTIME_ASSERT(i < Moving.ControlSteps.size() - 1);

        control_step_begin = Moving.ControlSteps[i];
        start_hx = hx;
        start_hy = hy;
        cur_dist += dist;
    }

    if (normalized_time == 1.0f && GetHexX() == Moving.EndHexX && GetHexY() == Moving.EndHexY) {
        ClearMove();
        AnimateStay();
    }
}

void CritterHexView::SetSprRect()
{
    if (SprDrawValid) {
        const auto old = DRect;

        DRect = _engine->SprMngr.GetDrawRect(SprDraw);

        _textRect.Left += DRect.Left - old.Left;
        _textRect.Right += DRect.Left - old.Left;
        _textRect.Top += DRect.Top - old.Top;
        _textRect.Bottom += DRect.Top - old.Top;

        if (IsChosen()) {
            _engine->SprMngr.SetEgg(GetHexX(), GetHexY(), SprDraw);
        }
    }
}

auto CritterHexView::GetTextRect() const -> IRect
{
    if (SprDrawValid) {
        return _textRect;
    }
    return {};
}

void CritterHexView::SetAnimOffs(int ox, int oy)
{
    _oxAnim = ox;
    _oyAnim = oy;

    RefreshOffs();
}

void CritterHexView::AddExtraOffs(int ext_ox, int ext_oy)
{
    _oxExtI += ext_ox;
    _oyExtI += ext_oy;
    _oxExtF = static_cast<float>(_oxExtI);
    _oyExtF = static_cast<float>(_oyExtI);

    std::tie(_oxExtSpeed, _oyExtSpeed) = GenericUtils::GetStepsCoords(0, 0, _oxExtI, _oyExtI);
    _oxExtSpeed = -_oxExtSpeed;
    _oyExtSpeed = -_oyExtSpeed;
    _offsExtNextTick = _engine->GameTime.GameTick() + 30;

    RefreshOffs();
}

void CritterHexView::RefreshOffs()
{
    SprOx = static_cast<short>(GetHexOffsX() + _oxExtI + _oxAnim);
    SprOy = static_cast<short>(GetHexOffsY() + _oyExtI + _oyAnim);

    if (SprDrawValid) {
        DRect = _engine->SprMngr.GetDrawRect(SprDraw);

        _textRect = DRect;

#if FO_ENABLE_3D
        if (_model != nullptr) {
            // Todo: expose text on head offset to some settings
            // _textRect.Top += _engine->SprMngr.GetSpriteInfo(SprId)->Height / 6;
            _textRect.Top = _textRect.Bottom - 100;
        }
#endif

        if (IsChosen()) {
            _engine->SprMngr.SetEgg(GetHexX(), GetHexY(), SprDraw);
        }
    }
}

auto CritterHexView::GetWalkHexOffsets(uchar dir) const -> tuple<short, short>
{
    auto hx = 1;
    auto hy = 1;
    _engine->Geometry.MoveHexByDirUnsafe(hx, hy, dir);
    return _engine->Geometry.GetHexInterval(hx, hy, 1, 1);
}

void CritterHexView::SetText(string_view str, uint color, uint text_delay)
{
    _tickStartText = _engine->GameTime.GameTick();
    _strTextOnHead = str;
    _tickTextDelay = text_delay;
    _textOnHeadColor = color;
}

void CritterHexView::GetNameTextPos(int& x, int& y) const
{
    const auto tr = GetTextRect();
    const auto tr_half_width = tr.Width() / 2;
    x = static_cast<int>(static_cast<float>(tr.Left + tr_half_width + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom - 100.0f);
    y = static_cast<int>(static_cast<float>(tr.Top + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom - 70.0f) + GetNameOffset();
}

void CritterHexView::GetNameTextInfo(bool& name_visible, int& x, int& y, int& w, int& h, int& lines) const
{
    name_visible = false;

    string str;
    if (_strTextOnHead.empty()) {
        if (IsOwnedByPlayer() && !_engine->Settings.ShowPlayerNames) {
            return;
        }
        if (IsNpc() && !_engine->Settings.ShowNpcNames) {
            return;
        }

        name_visible = true;

        str = _nameOnHead;

        if (_engine->Settings.ShowCritId) {
            str += _str("  {}", GetId());
        }
        if (_ownedByPlayer && _isPlayerOffline) {
            str += _engine->Settings.PlayerOffAppendix;
        }
    }
    else {
        str = _strTextOnHead;
    }

    GetNameTextPos(x, y);

    if (_engine->SprMngr.GetTextInfo(200, 70, str, -1, FT_CENTERX | FT_BOTTOM | FT_BORDERED, w, h, lines)) {
        x += 100 - w / 2;
        y += 70 - h;
    }
}

void CritterHexView::DrawTextOnHead()
{
    if (_strTextOnHead.empty()) {
        if (IsOwnedByPlayer() && !_engine->Settings.ShowPlayerNames) {
            return;
        }
        if (IsNpc() && !_engine->Settings.ShowNpcNames) {
            return;
        }
    }

    if (SprDrawValid) {
        int x = 0;
        int y = 0;
        GetNameTextPos(x, y);
        const auto r = IRect(x, y, x + 200, y + 70);

        string str;
        uint color;
        if (_strTextOnHead.empty()) {
            str = _nameOnHead;

            if (_engine->Settings.ShowCritId) {
                str += _str(" ({})", GetId());
            }
            if (_ownedByPlayer && _isPlayerOffline) {
                str += _engine->Settings.PlayerOffAppendix;
            }

            color = GetNameColor();
            if (color == 0u) {
                color = COLOR_CRITTER_NAME;
            }
        }
        else {
            str = _strTextOnHead;
            color = _textOnHeadColor;

            if (_tickTextDelay > 500) {
                const auto dt = _engine->GameTime.GameTick() - _tickStartText;
                const auto hide = _tickTextDelay - 200;

                if (dt >= hide) {
                    const auto alpha = 0xFF * (100 - GenericUtils::Percent(_tickTextDelay - hide, dt - hide)) / 100;
                    color = (alpha << 24) | (color & 0xFFFFFF);
                }
            }
        }

        if (_fadingEnable) {
            const uint alpha = GetFadeAlpha();
            _engine->SprMngr.DrawStr(r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, (alpha << 24) | (color & 0xFFFFFF), 0);
        }
        else if (!IsFinishing()) {
            _engine->SprMngr.DrawStr(r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, 0);
        }
    }

    if (_engine->GameTime.GameTick() - _tickStartText >= _tickTextDelay && !_strTextOnHead.empty()) {
        _strTextOnHead = "";
    }
}
