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

CritterHexView::CritterHexView(MapView* map, ident_t id, const ProtoCritter* proto, const Properties* props) :
    CritterView(map->GetEngine(), id, proto, props),
    HexView(map)
{
    STACK_TRACE_ENTRY();
}

void CritterHexView::Init()
{
    STACK_TRACE_ENTRY();

#if FO_ENABLE_3D
    RefreshModel();
#endif

    AnimateStay();
    RefreshOffs();

    _fidgetTime = _engine->GameTime.GameplayTime() + std::chrono::milliseconds {GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2u)};

    DrawEffect = _engine->EffectMngr.Effects.Critter;
}

void CritterHexView::MarkAsDestroyed()
{
    STACK_TRACE_ENTRY();

    CritterView::MarkAsDestroyed();

    Spr = nullptr;

#if FO_ENABLE_3D
    _modelSpr = nullptr;
    _model = nullptr;
#endif
}

void CritterHexView::SetupSprite(MapSprite* mspr)
{
    STACK_TRACE_ENTRY();

    HexView::SetupSprite(mspr);

    mspr->SetLight(CornerType::EastWest, _map->GetLightData(), _map->GetWidth(), _map->GetHeight());

    if (!mspr->IsHidden() && GetHideSprite()) {
        mspr->SetHidden(true);
    }
}

auto CritterHexView::GetCurAnim() -> CritterAnim*
{
    STACK_TRACE_ENTRY();

    return IsAnim() ? &_animSequence.front() : nullptr;
}

auto CritterHexView::AddInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const Properties* props) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = CritterView::AddInvItem(id, proto, slot, props);

    if (item->GetCritterSlot() != CritterItemSlot::Inventory && !IsAnim()) {
        AnimateStay();
    }

    return item;
}

auto CritterHexView::AddInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const vector<vector<uint8>>& props_data) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = CritterView::AddInvItem(id, proto, slot, props_data);

    if (item->GetCritterSlot() != CritterItemSlot::Inventory && !IsAnim()) {
        AnimateStay();
    }

    return item;
}

void CritterHexView::DeleteInvItem(ItemView* item, bool animate)
{
    STACK_TRACE_ENTRY();

    CritterView::DeleteInvItem(item, animate);

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
    Moving.StartTime = {};
    Moving.OffsetTime = {};
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

void CritterHexView::MoveAttachedCritters()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto hx = GetHexX();
    const auto hy = GetHexY();
    const auto hex_ox = GetHexOffsX();
    const auto hex_oy = GetHexOffsY();

    for (const auto cr_id : AttachedCritters) {
        if (auto* cr = _map->GetCritter(cr_id); cr != nullptr) {
            if (cr->GetHexX() != hx || cr->GetHexY() != hy) {
                _map->MoveCritter(cr, hx, hy, false);
            }

            if (cr->GetHexOffsX() != hex_ox || cr->GetHexOffsY() != hex_oy) {
                cr->SetHexOffsX(hex_ox);
                cr->SetHexOffsY(hex_oy);
                cr->RefreshOffs();
            }
        }
    }
}

void CritterHexView::Action(CritterAction action, int action_data, Entity* context_item, bool local_call /* = true */)
{
    STACK_TRACE_ENTRY();

    _engine->OnCritterAction.Fire(local_call, this, action, action_data, context_item);

    switch (action) {
    case CritterAction::Knockout:
        SetCondition(CritterCondition::Knockout);
        ClearMove();
        break;
    case CritterAction::StandUp:
        SetCondition(CritterCondition::Alive);
        break;
    case CritterAction::Dead: {
        SetCondition(CritterCondition::Dead);
        ClearMove();

#if FO_ENABLE_3D
        if (_model != nullptr) {
            _resetTime = _engine->GameTime.GameplayTime() + _model->GetAnimDuration();
            _needReset = true;
        }
        else
#endif
        {
            const auto* anim = GetCurAnim();
            _resetTime = _engine->GameTime.GameplayTime() + std::chrono::milliseconds {anim != nullptr && anim->AnimFrames != nullptr ? anim->AnimFrames->WholeTicks : 1000};
            _needReset = true;
        }
    } break;
    case CritterAction::Respawn:
        SetCondition(CritterCondition::Alive);
        FadeUp();
        AnimateStay();
        _resetTime = _engine->GameTime.GameplayTime(); // Fast
        _needReset = true;
        break;
    case CritterAction::Connect:
        SetIsPlayerOffline(false);
        break;
    case CritterAction::Disconnect:
        SetIsPlayerOffline(true);
        break;
    case CritterAction::Refresh:
#if FO_ENABLE_3D
        if (_model != nullptr) {
            _model->PrewarmParticles();
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

    _animStartTime = _engine->GameTime.GameplayTime();

    const auto& cr_anim = _animSequence.front();

    _engine->OnCritterAnimationProcess.Fire(false, this, cr_anim.StateAnim, cr_anim.ActionAnim, cr_anim.ContextItem);

#if FO_ENABLE_3D
    if (_model != nullptr) {
        _model->SetAnimation(cr_anim.StateAnim, cr_anim.ActionAnim, GetModelLayersData(), ANIMATION_ONE_TIME | ANIMATION_NO_ROTATE);
    }
    else
#endif
    {
        SetAnimSpr(cr_anim.AnimFrames, cr_anim.BeginFrm);
    }
}

void CritterHexView::Animate(CritterStateAnim state_anim, CritterActionAnim action_anim, Entity* context_item)
{
    STACK_TRACE_ENTRY();

    const auto dir = GetDir();
    if (state_anim == CritterStateAnim::None) {
        state_anim = GetStateAnim();
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
        if (_model->ResolveAnimation(state_anim, action_anim)) {
            _animSequence.push_back(CritterAnim {nullptr, time_duration {}, 0, 0, state_anim, action_anim, fixed_context_item});
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
        const auto* frames = _engine->ResMngr.GetCritterAnimFrames(GetModelName(), state_anim, action_anim, dir);

        if (frames != nullptr) {
            _animSequence.push_back(CritterAnim {frames, std::chrono::milliseconds {frames->WholeTicks}, 0, frames->CntFrm - 1, frames->StateAnim, frames->ActionAnim, fixed_context_item});
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

    _animStartTime = _engine->GameTime.GameplayTime();

    auto state_anim = GetStateAnim();
    auto action_anim = GetActionAnim();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        const auto scale_factor = GetScaleFactor();
        const auto scale = scale_factor != 0 ? static_cast<float>(scale_factor) / 1000.0f : 1.0f;

        _model->SetScale(scale, scale, scale);

        if (IsMoving()) {
            _model->SetMoving(true, static_cast<uint>(static_cast<float>(Moving.Speed) / scale));

            action_anim = _model->GetMovingAnim2();
        }
        else {
            _model->SetMoving(false);
        }

        if (!_model->ResolveAnimation(state_anim, action_anim)) {
            state_anim = CritterStateAnim::Unarmed;
            action_anim = CritterActionAnim::Idle;
        }

        _engine->OnCritterAnimationProcess.Fire(true, this, state_anim, action_anim, nullptr);

        if (GetCondition() == CritterCondition::Alive || GetCondition() == CritterCondition::Knockout) {
            _model->SetAnimation(state_anim, action_anim, GetModelLayersData(), 0);
        }
        else {
            _model->SetAnimation(state_anim, action_anim, GetModelLayersData(), ANIMATION_STAY | ANIMATION_PERIOD(100));
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
                action_anim = CritterActionAnim::Walk;
            }
            else {
                action_anim = CritterActionAnim::Run;
            }
        }

        const auto* frames = _engine->ResMngr.GetCritterAnimFrames(GetModelName(), state_anim, action_anim, GetDir());
        if (frames == nullptr) {
            frames = _engine->ResMngr.GetCritterDummyFrames();
        }

        if (_stayAnim.AnimFrames != frames) {
            _engine->OnCritterAnimationProcess.Fire(true, this, state_anim, action_anim, nullptr);

            _stayAnim.AnimFrames = frames;
            _stayAnim.AnimDuration = std::chrono::milliseconds {frames->WholeTicks};
            _stayAnim.BeginFrm = 0;
            _stayAnim.EndFrm = frames->CntFrm - 1;

            if (GetCondition() == CritterCondition::Dead) {
                _stayAnim.BeginFrm = _stayAnim.EndFrm;
            }
        }

        SetAnimSpr(frames, _stayAnim.BeginFrm);
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

auto CritterHexView::IsNeedReset() const -> bool
{
    STACK_TRACE_ENTRY();

    return _needReset && _engine->GameTime.GameplayTime() >= _resetTime;
}

void CritterHexView::ResetOk()
{
    STACK_TRACE_ENTRY();

    _needReset = false;
}

auto CritterHexView::GetActionAnim() const -> CritterActionAnim
{
    STACK_TRACE_ENTRY();

    switch (GetCondition()) {
    case CritterCondition::Alive:
#if FO_ENABLE_3D
        return GetAliveActionAnim() != CritterActionAnim::None ? GetAliveActionAnim() : (_model != nullptr && _model->IsCombatMode() && _engine->Settings.CombatAnimIdle != CritterActionAnim::None ? _engine->Settings.CombatAnimIdle : CritterActionAnim::Idle);
#else
        return GetAliveActionAnim() != CritterActionAnim::None ? GetAliveActionAnim() : CritterActionAnim::Idle;
#endif
    case CritterCondition::Knockout:
        return GetKnockoutActionAnim() != CritterActionAnim::None ? GetKnockoutActionAnim() : CritterActionAnim::IdleProneFront;
    case CritterCondition::Dead:
        return GetDeadActionAnim() != CritterActionAnim::None ? GetDeadActionAnim() : CritterActionAnim::DeadFront;
    }

    return CritterActionAnim::Idle;
}

auto CritterHexView::IsAnimAvailable(CritterStateAnim state_anim, CritterActionAnim action_anim) const -> bool
{
    STACK_TRACE_ENTRY();

    if (state_anim == CritterStateAnim::None) {
        state_anim = GetStateAnim();
    }

#if FO_ENABLE_3D
    if (_model != nullptr) {
        return _model->HasAnimation(state_anim, action_anim);
    }
#endif

    return _engine->ResMngr.GetCritterAnimFrames(GetModelName(), state_anim, action_anim, GetDir()) != nullptr;
}

#if FO_ENABLE_3D
auto CritterHexView::GetModelLayersData() const -> const int*
{
    STACK_TRACE_ENTRY();

    uint data_size;
    const auto* data = GetProperties().GetRawData(GetPropertyModelLayers(), data_size);
    RUNTIME_ASSERT(data_size == sizeof(int) * MODEL_LAYERS_COUNT);
    return reinterpret_cast<const int*>(data);
}

void CritterHexView::RefreshModel()
{
    STACK_TRACE_ENTRY();

    Spr = nullptr;

    _modelSpr = nullptr;
    _model = nullptr;

    const auto model_name = GetModelName();
    const auto ext = _str(model_name).getFileExtension();

    if (ext == "fo3d") {
        _modelSpr = dynamic_pointer_cast<ModelSprite>(_engine->SprMngr.LoadSprite(model_name, AtlasType::MapSprites));

        if (_modelSpr) {
            _modelSpr->UseGameplayTimer();
            _modelSpr->PlayDefault();

            Spr = _modelSpr.get();

            _model = _modelSpr->GetModel();

            _model->SetLookDirAngle(GetDirAngle());
            _model->SetMoveDirAngle(GetDirAngle(), false);

            _model->SetAnimation(CritterStateAnim::Unarmed, CritterActionAnim::Idle, GetModelLayersData(), 0);
            _model->PrewarmParticles();

            if (_map->IsMapperMode()) {
                _model->StartMeshGeneration();
            }
        }
        else {
            BreakIntoDebugger();

            Spr = _engine->ResMngr.GetCritterDummyFrames();
        }
    }
}
#endif

void CritterHexView::ChangeDir(uint8 dir)
{
    STACK_TRACE_ENTRY();

    ChangeDirAngle(GeometryHelper::DirToAngle(dir));
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

    const auto normalized_dir_angle = GeometryHelper::NormalizeAngle(static_cast<int16>(dir_angle));

    if (normalized_dir_angle == GetDirAngle()) {
        return;
    }

    SetDirAngle(normalized_dir_angle);
    SetDir(GeometryHelper::AngleToDir(normalized_dir_angle));

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
    else
#endif
    {
        UNUSED_VARIABLE(dir_angle);
    }
}

void CritterHexView::Process()
{
    STACK_TRACE_ENTRY();

    if (IsFading()) {
        ProcessFading();
    }

    if (IsMoving()) {
        ProcessMoving();
    }

    // Extra offsets
    if (_offsExtNextTime != time_point {} && _engine->GameTime.GameplayTime() >= _offsExtNextTime) {
        _offsExtNextTime = _engine->GameTime.GameplayTime() + std::chrono::milliseconds {30};

        const auto dist = GenericUtils::DistSqrt(0, 0, iround(_oxExt), iround(_oyExt));
        const auto dist_div = dist / 10u;
        auto mul = static_cast<float>(dist_div);
        if (mul < 1.0f) {
            mul = 1.0f;
        }

        _oxExt += _oxExtSpeed * mul;
        _oyExt += _oyExtSpeed * mul;

        if (GenericUtils::DistSqrt(0, 0, iround(_oxExt), iround(_oyExt)) > dist) {
            _offsExtNextTime = time_point {};
            _oxExt = 0.0f;
            _oyExt = 0.0f;
        }

        RefreshOffs();
    }

    // Attachments
    if (!AttachedCritters.empty()) {
        MoveAttachedCritters();
    }

    // Animation
    const auto& cur_anim = !_animSequence.empty() ? _animSequence.front() : _stayAnim;
    const auto anim_proc = time_duration_to_ms<uint>(_engine->GameTime.GameplayTime() - _animStartTime) * 100 / (cur_anim.AnimDuration != time_duration {} ? time_duration_to_ms<uint>(cur_anim.AnimDuration) : 100);

    // Change frames
#if FO_ENABLE_3D
    if (_model == nullptr)
#endif
    {
        const auto frm_proc = !_animSequence.empty() ? std::min(anim_proc, 99u) : anim_proc % 100;
        const auto frm_index = cur_anim.BeginFrm + (cur_anim.EndFrm - cur_anim.BeginFrm + 1) * frm_proc / 100;
        if (frm_index != _curFrmIndex) {
            SetAnimSpr(cur_anim.AnimFrames, frm_index);
        }
    }

    if (!_animSequence.empty()) {
#if FO_ENABLE_3D
        if (_model != nullptr && !_model->IsAnimationPlaying()) {
            NextAnim(true);
        }
        else if (_model == nullptr && anim_proc >= 100) {
            NextAnim(true);
        }
#else
        if (anim_proc >= 100) {
            NextAnim(true);
        }
#endif

        if (_animSequence.empty()) {
            AnimateStay();
        }
    }

    // Fidget animation
    // Todo: fidget animation to scripts
    if (_engine->GameTime.GameplayTime() >= _fidgetTime) {
#if FO_ENABLE_3D
        const auto is_combat_mode = _model != nullptr && _model->IsCombatMode();
#else
        constexpr auto is_combat_mode = false;
#endif

        if (_animSequence.empty() && GetCondition() == CritterCondition::Alive && !IsMoving() && !is_combat_mode) {
            Action(CritterAction::Fidget, 0, nullptr, false);
        }

        _fidgetTime = _engine->GameTime.GameplayTime() + std::chrono::milliseconds {GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2)};
    }

    // Combat mode
#if FO_ENABLE_3D
    if (_model != nullptr) {
        if (const auto is_combat = GetIsModelInCombatMode(); is_combat != _model->IsCombatMode()) {
            if (_engine->Settings.CombatAnimIdle != CritterActionAnim::None && _animSequence.empty() && GetCondition() == CritterCondition::Alive && GetAliveActionAnim() == CritterActionAnim::None && !IsMoving()) {
                if (_engine->Settings.CombatAnimBegin != CritterActionAnim::None && is_combat && _model->GetActionAnim() != _engine->Settings.CombatAnimIdle) {
                    Animate(CritterStateAnim::None, _engine->Settings.CombatAnimBegin, nullptr);
                }
                else if (_engine->Settings.CombatAnimEnd != CritterActionAnim::None && !is_combat && _model->GetActionAnim() == _engine->Settings.CombatAnimIdle) {
                    Animate(CritterStateAnim::None, _engine->Settings.CombatAnimEnd, nullptr);
                }
            }

            _model->SetCombatMode(is_combat);
        }
    }
#endif

    if (!_strTextOnHead.empty() && _engine->GameTime.GameplayTime() - _startTextTime >= _textShowDuration) {
        _strTextOnHead = "";
    }
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

    auto normalized_time = time_duration_to_ms<float>(_engine->GameTime.FrameTime() - Moving.StartTime + Moving.OffsetTime) / Moving.WholeTime;
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
            const auto move_ok = GeometryHelper::MoveHexByDir(hx, hy, Moving.Steps[j], _map->GetWidth(), _map->GetHeight());
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
                const auto move_ok = GeometryHelper::MoveHexByDir(hx2, hy2, Moving.Steps[j2], _map->GetWidth(), _map->GetHeight());
                RUNTIME_ASSERT(move_ok);
            }

            const auto old_hx = GetHexX();
            const auto old_hy = GetHexY();

            _map->MoveCritter(this, hx2, hy2, false);

            const auto cur_hx = GetHexX();
            const auto cur_hy = GetHexY();
            const auto moved = cur_hx != old_hx || cur_hy != old_hy;

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

            const auto mxi = static_cast<int16>(iround(mx));
            const auto myi = static_cast<int16>(iround(my));

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
            const auto dir_angle_f = _engine->Geometry.GetLineDirAngle(0, 0, ox, oy);
            const auto dir_angle = static_cast<int16>(round(dir_angle_f));

#if FO_ENABLE_3D
            if (_model != nullptr) {
                ChangeMoveDirAngle(dir_angle);
            }
            else
#endif
            {
                ChangeDir(GeometryHelper::AngleToDir(dir_angle));
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

    RUNTIME_ASSERT(IsSpriteValid());

    return GetSprite()->GetViewRect();
}

void CritterHexView::SetAnimSpr(const SpriteSheet* anim, uint frm_index)
{
    STACK_TRACE_ENTRY();

    _curFrmIndex = frm_index;

    Spr = anim->GetSpr(_curFrmIndex);

    _oxAnim = 0;
    _oyAnim = 0;

    if (anim->ActionAnim == CritterActionAnim::Walk || anim->ActionAnim == CritterActionAnim::Run) {
        // ...
    }
    else {
        for (const auto i : xrange(_curFrmIndex + 1u)) {
            _oxAnim += anim->SprOffset[i].X;
            _oyAnim += anim->SprOffset[i].Y;
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

    _offsExtNextTime = _engine->GameTime.GameplayTime() + std::chrono::milliseconds {30};

    RefreshOffs();
}

void CritterHexView::RefreshOffs()
{
    STACK_TRACE_ENTRY();

    ScrX = GetHexOffsX() + iround(_oxExt) + _oxAnim;
    ScrY = GetHexOffsY() + iround(_oyExt) + _oyAnim;

    if (IsSpriteValid() && GetIsChosen()) {
        _engine->SprMngr.SetEgg(GetHexX(), GetHexY(), GetSprite());
    }
}

void CritterHexView::SetText(string_view str, ucolor color, time_duration text_delay)
{
    STACK_TRACE_ENTRY();

    _startTextTime = _engine->GameTime.GameplayTime();
    _strTextOnHead = str;
    _textShowDuration = text_delay;
    _textOnHeadColor = color;
}

void CritterHexView::GetNameTextPos(int& x, int& y) const
{
    STACK_TRACE_ENTRY();

    if (IsSpriteValid()) {
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

auto CritterHexView::IsNameVisible() const -> bool
{
    if (!_engine->Settings.ShowCritterName) {
        return false;
    }
    if (!_engine->Settings.ShowPlayerName && GetIsControlledByPlayer()) {
        return false;
    }
    if (!_engine->Settings.ShowNpcName && !GetIsControlledByPlayer()) {
        return false;
    }
    if (!_engine->Settings.ShowDeadNpcName && !GetIsControlledByPlayer() && IsDead()) {
        return false;
    }

    return true;
}

void CritterHexView::GetNameTextInfo(bool& name_visible, int& x, int& y, int& w, int& h, int& lines) const
{
    STACK_TRACE_ENTRY();

    name_visible = false;

    string str;
    if (_strTextOnHead.empty()) {
        name_visible = IsNameVisible();

        str = _name;

        if (GetIsControlledByPlayer() && GetIsPlayerOffline()) {
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

    NON_CONST_METHOD_HINT();

    if (!IsSpriteValid()) {
        return;
    }

    string str;
    ucolor color;

    if (_strTextOnHead.empty()) {
        if (!IsNameVisible()) {
            return;
        }

        str = _name;

        if (GetIsControlledByPlayer() && GetIsPlayerOffline()) {
            str += _engine->Settings.PlayerOffAppendix;
        }

        color = GetNameColor();

        if (color == ucolor::clear) {
            color = _textOnHeadColor;
        }
    }
    else {
        if (!_engine->Settings.ShowCritterHeadText) {
            return;
        }

        str = _strTextOnHead;
        color = _textOnHeadColor;
    }

    int x = 0;
    int y = 0;
    GetNameTextPos(x, y);
    const auto r = IRect(x - 100, y - 200, x + 100, y);

    _engine->SprMngr.DrawStr(r, str, FT_CENTERX | FT_BOTTOM | FT_BORDERED, ucolor {color, Alpha}, -1);
}
