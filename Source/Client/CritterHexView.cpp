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
#include "ItemView.h"
#include "MapView.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"

CritterHexView::CritterHexView(MapView* map, uint id, const ProtoCritter* proto) : CritterView(map->GetEngine(), id, proto), _map {map}
{
    STACK_TRACE_ENTRY();

    _tickFidget = _engine->GameTime.GameTick() + GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2u);
    DrawEffect = _engine->EffectMngr.Effects.Critter;
    auto layers = GetModelLayers();
    layers.resize(LAYERS3D_COUNT);
    SetModelLayers(layers);
}

void CritterHexView::Init()
{
    STACK_TRACE_ENTRY();

    CritterView::Init();

#if FO_ENABLE_3D
    RefreshModel();
#endif

    AnimateStay();
    RefreshOffs();
    SetFade(true);
}

void CritterHexView::Finish()
{
    STACK_TRACE_ENTRY();

    CritterView::Finish();

    SetFade(false);
    _finishingTime = FadingTick;
}

auto CritterHexView::IsFinished() const -> bool
{
    STACK_TRACE_ENTRY();

    return _finishingTime != 0 && _engine->GameTime.GameTick() > _finishingTime;
}

void CritterHexView::SetFade(bool fade_up)
{
    STACK_TRACE_ENTRY();

    const auto tick = _engine->GameTime.GameTick();
    FadingTick = tick + _engine->Settings.FadingDuration - (FadingTick > tick ? FadingTick - tick : 0);
    _fadeUp = fade_up;
    _fadingEnabled = true;
}

auto CritterHexView::GetFadeAlpha() -> uchar
{
    STACK_TRACE_ENTRY();

    const auto tick = _engine->GameTime.GameTick();
    const auto fading_proc = 100u - GenericUtils::Percent(_engine->Settings.FadingDuration, FadingTick > tick ? FadingTick - tick : 0u);
    if (fading_proc == 100u) {
        _fadingEnabled = false;
    }

    const auto alpha = (_fadeUp ? fading_proc * 255u / 100u : (100u - fading_proc) * 255u / 100u);
    return static_cast<uchar>(alpha);
}

auto CritterHexView::GetCurAnim() -> CritterAnim*
{
    STACK_TRACE_ENTRY();

    return IsAnim() ? &_animSequence.front() : nullptr;
}

auto CritterHexView::AddItem(uint id, const ProtoItem* proto, uchar slot, const vector<vector<uchar>>& properties_data) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = CritterView::AddItem(id, proto, slot, properties_data);

    if (item->GetCritterSlot() != 0u && !IsAnim()) {
        AnimateStay();
    }

    return item;
}

void CritterHexView::DeleteItem(ItemView* item, bool animate)
{
    STACK_TRACE_ENTRY();

    CritterView::DeleteItem(item, animate);

    if (animate && !IsAnim()) {
        AnimateStay();
    }
}

auto CritterHexView::GetAttackDist() -> uint
{
    STACK_TRACE_ENTRY();

    uint dist = 0;
    _engine->OnCritterGetAttackDistantion.Fire(this, nullptr, 0, dist);
    return dist;
}

void CritterHexView::ClearMove()
{
    STACK_TRACE_ENTRY();

    Moving.Steps = {};
    Moving.ControlSteps = {};
    Moving.StartTick = {};
    Moving.OffsetTick = {};
    Moving.Speed = {};
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

void CritterHexView::Action(int action, int action_ext, Entity* context_item, bool local_call /* = true */)
{
    STACK_TRACE_ENTRY();

    _engine->OnCritterAction.Fire(local_call, this, action, action_ext, context_item);

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
    STACK_TRACE_ENTRY();

    if (_animSequence.empty()) {
        return;
    }

    if (erase_front) {
        if (_animSequence.begin()->ContextItem != nullptr) {
            _animSequence.begin()->ContextItem->Release();
        }
        _animSequence.erase(_animSequence.begin());
    }

    if (_animSequence.empty()) {
        return;
    }

    _animStartTick = _engine->GameTime.GameTick();

    const auto& cr_anim = _animSequence.front();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        _model->SetAnimation(cr_anim.IndAnim1, cr_anim.IndAnim2, GetModelLayersData(), ANIMATION_ONE_TIME | ANIMATION_NO_ROTATE);
    }
    else
#endif
    {
        _engine->OnCritterAnimationProcess.Fire(false, this, cr_anim.IndAnim1, cr_anim.IndAnim2, cr_anim.ContextItem);

        SetAnimSpr(cr_anim.Anim, cr_anim.BeginFrm);
    }
}

void CritterHexView::Animate(uint anim1, uint anim2, Entity* context_item)
{
    STACK_TRACE_ENTRY();

    const auto dir = GetDir();
    if (anim1 == 0u) {
        anim1 = GetAnim1();
    }

    Entity* fixed_context_item = nullptr;

    if (context_item != nullptr) {
        if (const auto* item = dynamic_cast<ItemView*>(context_item); item != nullptr) {
            fixed_context_item = item->CreateRefClone();
        }
        else if (auto* proto = dynamic_cast<ProtoItem*>(context_item); proto != nullptr) {
            fixed_context_item = proto;
            proto->AddRef();
        }
        else {
            throw UnreachablePlaceException("Invalid context item");
        }
    }

#if FO_ENABLE_3D
    if (_model != nullptr) {
        if (_model->ResolveAnimation(anim1, anim2)) {
            _animSequence.push_back(CritterAnim {nullptr, 0, 0, 0, anim1, anim2, fixed_context_item});
            if (_animSequence.size() == 1) {
                NextAnim(false);
            }
        }
        else {
            if (!IsAnim()) {
                AnimateStay();
            }
        }
    }
    else
#endif
    {
        auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, dir);
        if (anim != nullptr) {
            _animSequence.push_back(CritterAnim {anim, anim->Ticks, 0, anim->CntFrm - 1, anim->Anim1, anim->Anim2, fixed_context_item});
            if (_animSequence.size() == 1) {
                NextAnim(false);
            }
        }
        else {
            if (!IsAnim()) {
                AnimateStay();
            }
        }
    }
}

void CritterHexView::AnimateStay()
{
    STACK_TRACE_ENTRY();

    ClearAnim();

    _animStartTick = _engine->GameTime.GameTick();

    auto anim1 = GetAnim1();
    auto anim2 = GetAnim2();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        const auto scale_factor = GetScaleFactor();
        const auto scale = scale_factor != 0 ? static_cast<float>(scale_factor) / 1000.0f : 1.0f;

        _model->SetScale(scale, scale, scale);

        if (IsMoving()) {
            _model->SetMoving(true, static_cast<uint>(static_cast<float>(Moving.Speed) / scale));

            anim2 = _model->GetMovingAnim2();
        }
        else {
            _model->SetMoving(false);
        }

        if (!_model->ResolveAnimation(anim1, anim2)) {
            anim1 = ANIM1_UNARMED;
            anim2 = ANIM2_IDLE;
        }

        _engine->OnCritterAnimationProcess.Fire(true, this, anim1, anim2, nullptr);

        if (GetCond() == CritterCondition::Alive || GetCond() == CritterCondition::Knockout) {
            _model->SetAnimation(anim1, anim2, GetModelLayersData(), 0);
        }
        else {
            _model->SetAnimation(anim1, anim2, GetModelLayersData(), ANIMATION_STAY | ANIMATION_PERIOD(100));
        }
    }
    else
#endif
    {
        if (IsMoving()) {
            const auto walk_speed = _engine->Settings.AnimWalkSpeed;
            const auto run_speed = _engine->Settings.AnimRunSpeed;
            RUNTIME_ASSERT(run_speed >= walk_speed);

            if (Moving.Speed < walk_speed + (run_speed - walk_speed) / 2) {
                anim2 = ANIM2_WALK;
            }
            else {
                anim2 = ANIM2_RUN;
            }
        }

        auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, GetDir());
        if (anim == nullptr) {
            anim = _engine->ResMngr.CritterDefaultAnim;
        }

        if (_stayAnim.Anim != anim) {
            _engine->OnCritterAnimationProcess.Fire(true, this, anim1, anim2, nullptr);

            _stayAnim.Anim = anim;
            _stayAnim.AnimTick = anim->Ticks;
            _stayAnim.BeginFrm = 0;
            _stayAnim.EndFrm = anim->CntFrm - 1;

            if (GetCond() == CritterCondition::Dead) {
                _stayAnim.BeginFrm = _stayAnim.EndFrm;
            }
        }

        SetAnimSpr(anim, _stayAnim.BeginFrm);
    }
}

void CritterHexView::ClearAnim()
{
    STACK_TRACE_ENTRY();

    for (const auto& anim : _animSequence) {
        if (anim.ContextItem != nullptr) {
            anim.ContextItem->Release();
        }
    }
    _animSequence.clear();
}

auto CritterHexView::IsHaveLightSources() const -> bool
{
    STACK_TRACE_ENTRY();

    for (const auto* item : _items) {
        if (item->GetIsLight()) {
            return true;
        }
    }
    return false;
}

auto CritterHexView::IsNeedReset() const -> bool
{
    STACK_TRACE_ENTRY();

    return _needReset && _engine->GameTime.GameTick() >= _resetTick;
}

void CritterHexView::ResetOk()
{
    STACK_TRACE_ENTRY();

    _needReset = false;
}

auto CritterHexView::GetAnim2() const -> uint
{
    STACK_TRACE_ENTRY();

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

auto CritterHexView::IsAnimAvailable(uint anim1, uint anim2) const -> bool
{
    STACK_TRACE_ENTRY();

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
auto CritterHexView::GetModelLayersData() const -> const int*
{
    STACK_TRACE_ENTRY();

    uint data_size;
    const auto* data = GetProperties().GetRawData(GetPropertyModelLayers(), data_size);
    RUNTIME_ASSERT(data_size == sizeof(int) * LAYERS3D_COUNT);
    return reinterpret_cast<const int*>(data);
}

void CritterHexView::RefreshModel()
{
    STACK_TRACE_ENTRY();

    _model = nullptr;

    const auto model_name = GetModelName();
    const auto ext = _str(model_name).getFileExtension();

    if (ext == "fo3d") {
        _engine->SprMngr.PushAtlasType(AtlasType::Dynamic);

        _model = _engine->SprMngr.LoadModel(model_name, true);

        if (_model) {
            _model->SetLookDirAngle(GetDirAngle());
            _model->SetMoveDirAngle(GetDirAngle(), false);

            SprId = _model->SprId;

            _model->SetAnimation(ANIM1_UNARMED, ANIM2_IDLE, GetModelLayersData(), 0);

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
    STACK_TRACE_ENTRY();

    ChangeDirAngle(_engine->Geometry.DirToAngle(dir));
}

void CritterHexView::ChangeDirAngle(int dir_angle)
{
    STACK_TRACE_ENTRY();

    ChangeLookDirAngle(dir_angle);
    ChangeMoveDirAngle(dir_angle);
}

void CritterHexView::ChangeLookDirAngle(int dir_angle)
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        _model->SetMoveDirAngle(dir_angle, true);
    }
#endif
}

void CritterHexView::Process()
{
    STACK_TRACE_ENTRY();

    if (IsMoving()) {
        ProcessMoving();
    }

    if (_fadingEnabled) {
        Alpha = GetFadeAlpha();
    }

    // Extra offsets
    if (_offsExtNextTick != 0u && _engine->GameTime.GameTick() >= _offsExtNextTick) {
        _offsExtNextTick = _engine->GameTime.GameTick() + 30;

        const auto dist = GenericUtils::DistSqrt(0, 0, iround(_oxExt), iround(_oyExt));
        const auto dist_div = dist / 10u;
        auto mul = static_cast<float>(dist_div);
        if (mul < 1.0f) {
            mul = 1.0f;
        }

        _oxExt += _oxExtSpeed * mul;
        _oyExt += _oyExtSpeed * mul;

        if (GenericUtils::DistSqrt(0, 0, iround(_oxExt), iround(_oyExt)) > dist) {
            _offsExtNextTick = 0;
            _oxExt = 0.0f;
            _oyExt = 0.0f;
        }

        RefreshOffs();
    }

    // Animation
    const auto& cur_anim = !_animSequence.empty() ? _animSequence.front() : _stayAnim;
    const auto anim_proc = (_engine->GameTime.GameTick() - _animStartTick) * 100 / (cur_anim.AnimTick != 0 ? cur_anim.AnimTick : 100);

    // Change frames
#if FO_ENABLE_3D
    if (_model == nullptr)
#endif
    {
        const auto frm_proc = !_animSequence.empty() ? std::min(anim_proc, 99u) : anim_proc % 100;
        const auto frm_index = cur_anim.BeginFrm + (cur_anim.EndFrm - cur_anim.BeginFrm + 1) * frm_proc / 100;
        if (frm_index != _curFrmIndex) {
            SetAnimSpr(cur_anim.Anim, frm_index);
        }
    }

    if (!_animSequence.empty()) {
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

        if (_animSequence.empty()) {
            AnimateStay();
        }
    }

    // Fidget animation
    // Todo: fidget animation to scripts
    if (_engine->GameTime.GameTick() >= _tickFidget) {
#if FO_ENABLE_3D
        const auto is_combat_mode = _model != nullptr && _model->IsCombatMode();
#else
        constexpr auto is_combat_mode = false;
#endif

        if (_animSequence.empty() && GetCond() == CritterCondition::Alive && !IsMoving() && !is_combat_mode) {
            Action(ACTION_FIDGET, 0, nullptr, false);
        }

        _tickFidget = _engine->GameTime.GameTick() + GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2);
    }

    // Combat mode
#if FO_ENABLE_3D
    if (_model != nullptr) {
        if (const auto is_combat = GetTimeoutBattle() > _engine->GameTime.GetFullSecond(); is_combat != _model->IsCombatMode()) {
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
    }
#endif
}

void CritterHexView::ProcessMoving()
{
    STACK_TRACE_ENTRY();

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

            _map->MoveCritter(this, hx2, hy2, false);

            const auto cur_hx = GetHexX();
            const auto cur_hy = GetHexY();
            const auto moved = (cur_hx != old_hx || cur_hy != old_hy);

            if (moved && IsChosen()) {
                _map->RebuildFog();
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

            const auto mxi = static_cast<short>(iround(mx));
            const auto myi = static_cast<short>(iround(my));
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

#if FO_ENABLE_3D
            if (_model != nullptr) {
                ChangeMoveDirAngle(static_cast<int>(dir_angle));
            }
            else
#endif
            {
                ChangeDir(_engine->Geometry.AngleToDir(static_cast<short>(dir_angle)));
            }

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

auto CritterHexView::GetViewRect() const -> IRect
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(SprDrawValid);

    return _engine->SprMngr.GetViewRect(SprDraw);
}

void CritterHexView::SetAnimSpr(const AnyFrames* anim, uint frm_index)
{
    STACK_TRACE_ENTRY();

    _curFrmIndex = frm_index;

    SprId = anim->GetSprId(_curFrmIndex);

    _oxAnim = 0;
    _oyAnim = 0;

    if (anim->Anim2 == ANIM2_WALK || anim->Anim2 == ANIM2_RUN) {
        // ...
    }
    else {
        for (const auto i : xrange(_curFrmIndex + 1u)) {
            _oxAnim += anim->NextX[i];
            _oyAnim += anim->NextY[i];
        }
    }

    RefreshOffs();
}

void CritterHexView::AddExtraOffs(int ext_ox, int ext_oy)
{
    STACK_TRACE_ENTRY();

    _oxExt += static_cast<float>(ext_ox);
    _oyExt += static_cast<float>(ext_oy);

    std::tie(_oxExtSpeed, _oyExtSpeed) = GenericUtils::GetStepsCoords(0, 0, iround(_oxExt), iround(_oyExt));
    _oxExtSpeed = -_oxExtSpeed;
    _oyExtSpeed = -_oyExtSpeed;

    _offsExtNextTick = _engine->GameTime.GameTick() + 30;

    RefreshOffs();
}

void CritterHexView::RefreshOffs()
{
    STACK_TRACE_ENTRY();

    SprOx = GetHexOffsX() + iround(_oxExt) + _oxAnim;
    SprOy = GetHexOffsY() + iround(_oyExt) + _oyAnim;

    if (SprDrawValid) {
        if (IsChosen()) {
            _engine->SprMngr.SetEgg(GetHexX(), GetHexY(), SprDraw);
        }
    }
}

void CritterHexView::SetText(string_view str, uint color, uint text_delay)
{
    STACK_TRACE_ENTRY();

    _tickStartText = _engine->GameTime.GameTick();
    _strTextOnHead = str;
    _tickTextDelay = text_delay;
    _textOnHeadColor = color;
}

void CritterHexView::GetNameTextPos(int& x, int& y) const
{
    STACK_TRACE_ENTRY();

    if (SprDrawValid) {
        const auto rect = GetViewRect();
        const auto rect_half_width = rect.Width() / 2;

        x = iround(static_cast<float>(rect.Left + rect_half_width + _engine->Settings.ScrOx) / _map->GetSpritesZoom());
        y = iround(static_cast<float>(rect.Top + _engine->Settings.ScrOy) / _map->GetSpritesZoom()) + _engine->Settings.NameOffset + GetNameOffset();
    }
    else {
        // Offscreen
        x = -1000;
        y = -1000;
    }
}

void CritterHexView::GetNameTextInfo(bool& name_visible, int& x, int& y, int& w, int& h, int& lines) const
{
    STACK_TRACE_ENTRY();

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

        str = _name;

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

    if (_engine->SprMngr.GetTextInfo(200, 200, str, -1, FT_CENTERX | FT_BOTTOM | FT_BORDERED, w, h, lines)) {
        x -= w / 2;
        y -= h;
    }
}

void CritterHexView::DrawTextOnHead()
{
    STACK_TRACE_ENTRY();

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
        const auto r = IRect(x - 100, y - 200, x + 100, y);

        string str;
        uint color;
        if (_strTextOnHead.empty()) {
            str = _name;

            if (_engine->Settings.ShowCritId) {
                str += _str(" ({} {} {})", GetId(), GetHexX(), GetHexY());
            }
            if (_ownedByPlayer && _isPlayerOffline) {
                str += _engine->Settings.PlayerOffAppendix;
            }

            color = GetNameColor();
            if (color == 0u) {
                color = _textOnHeadColor;
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

        if (_fadingEnabled) {
            const uint alpha = GetFadeAlpha();
            _engine->SprMngr.DrawStr(r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, (alpha << 24) | (color & 0xFFFFFF), -1);
        }
        else if (!IsFinishing()) {
            _engine->SprMngr.DrawStr(r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, color, -1);
        }
    }

    if (_engine->GameTime.GameTick() - _tickStartText >= _tickTextDelay && !_strTextOnHead.empty()) {
        _strTextOnHead = "";
    }
}
