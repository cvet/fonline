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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ItemView.h"
#include "MapView.h"
#include "ResourceManager.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

CritterHexView::CritterHexView(MapView* map, ident_t id, const ProtoCritter* proto, const Properties* props) :
    CritterView(map->GetEngine(), id, proto, props),
    HexView(map)
{
    FO_STACK_TRACE_ENTRY();
}

void CritterHexView::Init()
{
    FO_STACK_TRACE_ENTRY();

#if FO_ENABLE_3D
    RefreshModel();
#endif

    AnimateStay();
    RefreshOffs();

    _fidgetTime = _engine->GameTime.GetFrameTime() + std::chrono::milliseconds {GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2u)};

    DrawEffect = _engine->EffectMngr.Effects.Critter;
}

void CritterHexView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    CritterView::OnDestroySelf();

    Spr = nullptr;

#if FO_ENABLE_3D
    _modelSpr = nullptr;
    _model = nullptr;
#endif
}

void CritterHexView::SetupSprite(MapSprite* mspr)
{
    FO_STACK_TRACE_ENTRY();

    HexView::SetupSprite(mspr);

    mspr->SetLight(CornerType::EastWest, _map->GetLightData(), _map->GetSize());

    if (!mspr->IsHidden() && GetHideSprite()) {
        mspr->SetHidden(true);
    }
}

auto CritterHexView::GetCurAnim() -> CritterAnim*
{
    FO_STACK_TRACE_ENTRY();

    return IsAnim() ? &_animSequence.front() : nullptr;
}

ItemView* CritterHexView::AddRawInvItem(ItemView* item)
{
    FO_STACK_TRACE_ENTRY();

    CritterView::AddRawInvItem(item);

    if (item->GetCritterSlot() != CritterItemSlot::Inventory && !IsAnim()) {
        AnimateStay();
    }

    return item;
}

void CritterHexView::DeleteInvItem(ItemView* item, bool animate)
{
    FO_STACK_TRACE_ENTRY();

    CritterView::DeleteInvItem(item, animate);

    if (animate && !IsAnim()) {
        AnimateStay();
    }
}

void CritterHexView::ClearMove()
{
    FO_STACK_TRACE_ENTRY();

    Moving.Steps = {};
    Moving.ControlSteps = {};
    Moving.StartTime = {};
    Moving.OffsetTime = {};
    Moving.Speed = {};
    Moving.StartHex = {};
    Moving.EndHex = {};
    Moving.WholeTime = {};
    Moving.WholeDist = {};
    Moving.StartHexOffset = {};
    Moving.EndHexOffset = {};
}

void CritterHexView::MoveAttachedCritters()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto hex = GetHex();
    const auto hex_offset = GetHexOffset();

    for (const auto cr_id : AttachedCritters) {
        if (auto* cr = _map->GetCritter(cr_id); cr != nullptr) {
            if (cr->GetHex() != hex) {
                _map->MoveCritter(cr, hex, false);
            }

            if (cr->GetHexOffset() != hex_offset) {
                cr->SetHexOffset(hex_offset);
                cr->RefreshOffs();
            }
        }
    }
}

void CritterHexView::Action(CritterAction action, int32 action_data, Entity* context_item, bool local_call /* = true */)
{
    FO_STACK_TRACE_ENTRY();

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
            _resetTime = _engine->GameTime.GetFrameTime() + _model->GetAnimDuration();
            _needReset = true;
        }
        else
#endif
        {
            const auto* anim = GetCurAnim();
            const auto duration = std::chrono::milliseconds {anim != nullptr && anim->AnimFrames != nullptr && anim->AnimFrames->WholeTicks != 0 ? anim->AnimFrames->WholeTicks : 1000};
            _resetTime = _engine->GameTime.GetFrameTime() + duration;
            _needReset = true;
        }
    } break;
    case CritterAction::Respawn:
        SetCondition(CritterCondition::Alive);
        FadeUp();
        AnimateStay();
        _resetTime = _engine->GameTime.GetFrameTime(); // Fast
        _needReset = true;
        break;
    case CritterAction::Connect:
        SetIsPlayerOffline(false);
        break;
    case CritterAction::Disconnect:
        SetIsPlayerOffline(true);
        break;
    case CritterAction::Refresh:
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
    FO_STACK_TRACE_ENTRY();

    if (_animSequence.empty()) {
        return;
    }

    if (erase_front) {
        _animSequence.erase(_animSequence.begin());
    }

    if (_animSequence.empty()) {
        return;
    }

    _animStartTime = _engine->GameTime.GetFrameTime();

    auto& cr_anim = _animSequence.front();

    _engine->OnCritterAnimationProcess.Fire(false, this, cr_anim.StateAnim, cr_anim.ActionAnim, cr_anim.ContextItem.get());

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
    FO_STACK_TRACE_ENTRY();

    const auto dir = GetDir();

    if (state_anim == CritterStateAnim::None) {
        state_anim = GetStateAnim();
    }

    refcount_ptr<Entity> resolved_context_item;

    if (context_item != nullptr) {
        if (auto* item = dynamic_cast<ItemView*>(context_item); item != nullptr) {
            resolved_context_item = item->CreateRefClone();
        }
        else if (auto* proto = dynamic_cast<ProtoItem*>(context_item); proto != nullptr) {
            resolved_context_item = proto;
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }

#if FO_ENABLE_3D
    if (_model != nullptr) {
        if (_model->ResolveAnimation(state_anim, action_anim)) {
            _animSequence.push_back(CritterAnim {nullptr, timespan::zero, 0, 0, state_anim, action_anim, resolved_context_item.get()});

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
            const auto duration = std::chrono::milliseconds {frames->WholeTicks != 0 ? frames->WholeTicks : 100};
            _animSequence.push_back(CritterAnim {frames, duration, 0, frames->CntFrm - 1, frames->StateAnim, frames->ActionAnim, resolved_context_item.get()});

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
    FO_STACK_TRACE_ENTRY();

    ClearAnim();

    _animStartTime = _engine->GameTime.GetFrameTime();

    auto state_anim = GetStateAnim();
    auto action_anim = GetActionAnim();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        const auto scale_factor = GetScaleFactor();
        const auto scale = scale_factor != 0 ? numeric_cast<float32>(scale_factor) / 1000.0f : 1.0f;

        _model->SetScale(scale, scale, scale);

        if (IsMoving()) {
            _model->SetMoving(true, iround<int32>(numeric_cast<float32>(Moving.Speed) / scale));
            action_anim = _model->GetMovingAnim();
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
            if (Moving.Speed < _engine->Settings.RunAnimStartSpeed) {
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
            _stayAnim.AnimDuration = std::chrono::milliseconds {frames->WholeTicks != 0 ? frames->WholeTicks : 100};
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
    FO_STACK_TRACE_ENTRY();

    _animSequence.clear();
}

auto CritterHexView::IsNeedReset() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _needReset && _engine->GameTime.GetFrameTime() >= _resetTime;
}

void CritterHexView::ResetOk()
{
    FO_STACK_TRACE_ENTRY();

    _needReset = false;
}

auto CritterHexView::GetActionAnim() const noexcept -> CritterActionAnim
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (GetCondition()) {
    case CritterCondition::Alive:
#if FO_ENABLE_3D
        if (const auto fixed_anim = GetAliveActionAnim(); fixed_anim == CritterActionAnim::None) {
            if (_model != nullptr && _model->IsCombatMode() && _engine->Settings.CombatAnimIdle != CritterActionAnim::None) {
                return _engine->Settings.CombatAnimIdle;
            }
            else {
                return CritterActionAnim::Idle;
            }
        }
        else {
            return fixed_anim;
        }
#else
        if (const auto fixed_anim = GetAliveActionAnim(); fixed_anim == CritterActionAnim::None) {
            return CritterActionAnim::Idle;
        }
        else {
            return fixed_anim;
        }
#endif
    case CritterCondition::Knockout:
        if (const auto fixed_anim = GetKnockoutActionAnim(); fixed_anim == CritterActionAnim::None) {
            return CritterActionAnim::IdleProneFront;
        }
        else {
            return fixed_anim;
        }
    case CritterCondition::Dead:
        if (const auto fixed_anim = GetDeadActionAnim(); fixed_anim == CritterActionAnim::None) {
            return CritterActionAnim::DeadFront;
        }
        else {
            return fixed_anim;
        }
    }

    return CritterActionAnim::Idle;
}

auto CritterHexView::IsAnimAvailable(CritterStateAnim state_anim, CritterActionAnim action_anim) -> bool
{
    FO_STACK_TRACE_ENTRY();

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
auto CritterHexView::GetModelLayersData() const -> const int32*
{
    FO_STACK_TRACE_ENTRY();

    const auto prop_raw_data = GetProperties().GetRawData(GetPropertyModelLayers());
    FO_RUNTIME_ASSERT(prop_raw_data.size() == sizeof(int32) * MODEL_LAYERS_COUNT);
    return reinterpret_cast<const int32*>(prop_raw_data.data());
}

void CritterHexView::RefreshModel()
{
    FO_STACK_TRACE_ENTRY();

    auto animCallbacks = _model != nullptr ? std::move(_model->AnimationCallbacks) : vector<ModelAnimationCallback>();

    Spr = nullptr;

    _modelSpr = nullptr;
    _model = nullptr;

    const hstring model_name = GetModelName();
    const string ext = strex(model_name).getFileExtension();

    if (ext == "fo3d") {
        _modelSpr = dynamic_ptr_cast<ModelSprite>(_engine->SprMngr.LoadSprite(model_name, AtlasType::MapSprites));

        if (_modelSpr) {
            _modelSpr->PlayDefault();

            Spr = _modelSpr.get();

            _model = _modelSpr->GetModel();
            _model->AnimationCallbacks = std::move(animCallbacks);
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
    FO_STACK_TRACE_ENTRY();

    ChangeDirAngle(GeometryHelper::DirToAngle(dir));
}

void CritterHexView::ChangeDirAngle(int32 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    ChangeLookDirAngle(dir_angle);
    ChangeMoveDirAngle(dir_angle);
}

void CritterHexView::ChangeLookDirAngle(int32 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    const auto normalized_dir_angle = GeometryHelper::NormalizeAngle(numeric_cast<int16>(dir_angle));
    const auto dir = GeometryHelper::AngleToDir(normalized_dir_angle);

    if (normalized_dir_angle == GetDirAngle() && dir == GetDir()) {
        return;
    }

    SetDirAngle(normalized_dir_angle);
    SetDir(dir);

#if FO_ENABLE_3D
    if (_model != nullptr) {
        _model->SetLookDirAngle(normalized_dir_angle);
    }
#endif

    if (!IsAnim()) {
        AnimateStay();
    }
}

void CritterHexView::ChangeMoveDirAngle(int32 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

#if FO_ENABLE_3D
    if (_model != nullptr) {
        _model->SetMoveDirAngle(dir_angle, true);
    }
    else
#endif
    {
        ignore_unused(dir_angle);
    }
}

void CritterHexView::Process()
{
    FO_STACK_TRACE_ENTRY();

    if (IsFading()) {
        ProcessFading();
    }

    if (IsMoving()) {
        ProcessMoving();
    }

    // Extra offsets
    if (_offsExtNextTime && _engine->GameTime.GetFrameTime() >= _offsExtNextTime) {
        _offsExtNextTime = _engine->GameTime.GetFrameTime() + std::chrono::milliseconds {30};

        const auto dist = GenericUtils::DistSqrt({0, 0}, {iround<int32>(_offsExt.x), iround<int32>(_offsExt.y)});
        const auto dist_div = dist / 10;
        const auto mul = std::max(numeric_cast<float32>(dist_div), 1.0f);

        _offsExt.x += _offsExtSpeed.x * mul;
        _offsExt.y += _offsExtSpeed.y * mul;

        if (GenericUtils::DistSqrt({0, 0}, {iround<int32>(_offsExt.x), iround<int32>(_offsExt.y)}) > dist) {
            _offsExtNextTime = {};
            _offsExt = {};
        }

        RefreshOffs();
    }

    // Attachments
    if (!AttachedCritters.empty()) {
        MoveAttachedCritters();
    }

    // Animation
    const auto& cur_anim = !_animSequence.empty() ? _animSequence.front() : _stayAnim;
    const auto anim_proc = (_engine->GameTime.GetFrameTime() - _animStartTime).to_ms<int32>() * 100 / (cur_anim.AnimDuration ? cur_anim.AnimDuration.to_ms<int32>() : 100);

    // Change frames
#if FO_ENABLE_3D
    if (_model == nullptr)
#endif
    {
        const auto frm_proc = !_animSequence.empty() ? std::min(anim_proc, 99) : anim_proc % 100;
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
    if (_engine->GameTime.GetFrameTime() >= _fidgetTime) {
#if FO_ENABLE_3D
        const auto is_combat_mode = _model != nullptr && _model->IsCombatMode();
#else
        constexpr auto is_combat_mode = false;
#endif

        if (_animSequence.empty() && GetCondition() == CritterCondition::Alive && !IsMoving() && !is_combat_mode) {
            Action(CritterAction::Fidget, 0, nullptr, false);
        }

        _fidgetTime = _engine->GameTime.GetFrameTime() + std::chrono::milliseconds {GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2)};
    }

    // Combat mode
#if FO_ENABLE_3D
    if (_model != nullptr) {
        if (const auto is_combat = GetModelInCombatMode(); is_combat != _model->IsCombatMode()) {
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
}

void CritterHexView::ProcessMoving()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!Moving.Steps.empty());
    FO_RUNTIME_ASSERT(!Moving.ControlSteps.empty());
    FO_RUNTIME_ASSERT(Moving.WholeTime > 0.0f);
    FO_RUNTIME_ASSERT(Moving.WholeDist > 0.0f);

    if (!IsAlive()) {
        ClearMove();
        AnimateStay();
        return;
    }

    auto normalized_time = (_engine->GameTime.GetFrameTime() - Moving.StartTime + Moving.OffsetTime).to_ms<float32>() / Moving.WholeTime;
    normalized_time = std::clamp(normalized_time, 0.0f, 1.0f);

    const auto dist_pos = Moving.WholeDist * normalized_time;

    auto start_hex = Moving.StartHex;
    auto cur_dist = 0.0f;

    auto done = false;
    auto control_step_begin = 0;
    for (size_t i = 0; i < Moving.ControlSteps.size(); i++) {
        auto hex = start_hex;

        FO_RUNTIME_ASSERT(control_step_begin <= Moving.ControlSteps[i]);
        FO_RUNTIME_ASSERT(Moving.ControlSteps[i] <= Moving.Steps.size());
        for (auto j = control_step_begin; j < Moving.ControlSteps[i]; j++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(hex, Moving.Steps[j], _map->GetSize());
            FO_RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = _engine->Geometry.GetHexInterval(start_hex, hex);

        if (i == 0) {
            ox -= Moving.StartHexOffset.x;
            oy -= Moving.StartHexOffset.y;
        }
        if (i == Moving.ControlSteps.size() - 1) {
            ox += Moving.EndHexOffset.x;
            oy += Moving.EndHexOffset.y;
        }

        const auto proj_oy = numeric_cast<float32>(oy) * _engine->Geometry.GetYProj();
        auto dist = std::sqrt(numeric_cast<float32>(ox * ox) + proj_oy * proj_oy);
        dist = std::max(dist, 0.0001f);

        if ((normalized_time < 1.0f && dist_pos >= cur_dist && dist_pos <= cur_dist + dist) || (normalized_time == 1.0f && i == Moving.ControlSteps.size() - 1)) {
            auto normalized_dist = (dist_pos - cur_dist) / dist;
            normalized_dist = std::clamp(normalized_dist, 0.0f, 1.0f);
            if (normalized_time == 1.0f) {
                normalized_dist = 1.0f;
            }

            // Evaluate current hex
            const auto step_index_f = normalized_dist * numeric_cast<float32>(Moving.ControlSteps[i] - control_step_begin);
            const auto step_index = control_step_begin + iround<int32>(step_index_f);
            FO_RUNTIME_ASSERT(step_index >= control_step_begin);
            FO_RUNTIME_ASSERT(step_index <= numeric_cast<int32>(Moving.ControlSteps[i]));

            auto hex2 = start_hex;

            for (auto j2 = control_step_begin; j2 < step_index; j2++) {
                const auto move_ok = GeometryHelper::MoveHexByDir(hex2, Moving.Steps[j2], _map->GetSize());
                FO_RUNTIME_ASSERT(move_ok);
            }

            const auto prev_hex = GetHex();

            _map->MoveCritter(this, hex2, false);

            const auto cur_hex = GetHex();
            const auto moved = cur_hex != prev_hex;

            // Evaluate current position
            auto&& [cr_ox, cr_oy] = _engine->Geometry.GetHexInterval(start_hex, cur_hex);

            if (i == 0) {
                cr_ox -= Moving.StartHexOffset.x;
                cr_oy -= Moving.StartHexOffset.y;
            }

            const auto lerp = [](int32 a, int32 b, float32 t) { return numeric_cast<float32>(a) * (1.0f - t) + numeric_cast<float32>(b) * t; };

            auto mx = lerp(0, ox, normalized_dist);
            auto my = lerp(0, oy, normalized_dist);
            mx -= numeric_cast<float32>(cr_ox);
            my -= numeric_cast<float32>(cr_oy);

            const auto mxi = numeric_cast<int16>(iround<int32>(mx));
            const auto myi = numeric_cast<int16>(iround<int32>(my));
            const auto hex_offset = GetHexOffset();

            if (moved || hex_offset.x != mxi || hex_offset.y != myi) {
#if FO_ENABLE_3D
                if (_model != nullptr) {
                    ipos32 model_offset;

                    if (moved) {
                        model_offset = _engine->Geometry.GetHexInterval(prev_hex, cur_hex);
                    }

                    model_offset.x -= hex_offset.x - mxi;
                    model_offset.y -= hex_offset.y - myi;

                    _model->MoveModel(model_offset);
                }
#endif

                SetHexOffset(ipos16 {mxi, myi});
                RefreshOffs();
            }

            // Evaluate dir angle
            const auto dir_angle_f = _engine->Geometry.GetLineDirAngle(0, 0, ox, oy);
            const auto dir_angle = iround<int16>(dir_angle_f);

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

        FO_RUNTIME_ASSERT(i < Moving.ControlSteps.size() - 1);

        control_step_begin = Moving.ControlSteps[i];
        start_hex = hex;
        cur_dist += dist;
    }

    if (normalized_time == 1.0f && GetHex() == Moving.EndHex) {
        ClearMove();
        AnimateStay();
    }
}

auto CritterHexView::GetViewRect() const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(IsSpriteValid());

    return GetSprite()->GetViewRect();
}

void CritterHexView::SetAnimSpr(const SpriteSheet* anim, int32 frm_index)
{
    FO_STACK_TRACE_ENTRY();

    _curFrmIndex = frm_index % anim->CntFrm;

    Spr = anim->GetSpr(_curFrmIndex);

    _offsAnim = {};

    if (anim->ActionAnim == CritterActionAnim::Walk || anim->ActionAnim == CritterActionAnim::Run) {
        // ...
    }
    else {
        for (const auto i : iterate_range(_curFrmIndex + 1)) {
            _offsAnim.x += anim->SprOffset[i].x;
            _offsAnim.y += anim->SprOffset[i].y;
        }
    }

    RefreshOffs();
}

void CritterHexView::AddExtraOffs(ipos32 offset)
{
    FO_STACK_TRACE_ENTRY();

    _offsExt.x += numeric_cast<float32>(offset.x);
    _offsExt.y += numeric_cast<float32>(offset.y);

    _offsExtSpeed = GenericUtils::GetStepsCoords({}, {iround<int32>(_offsExt.x), iround<int32>(_offsExt.y)});
    _offsExtSpeed.x = -_offsExtSpeed.x;
    _offsExtSpeed.y = -_offsExtSpeed.y;

    _offsExtNextTime = _engine->GameTime.GetFrameTime() + std::chrono::milliseconds {30};

    RefreshOffs();
}

void CritterHexView::RefreshOffs()
{
    FO_STACK_TRACE_ENTRY();

    const auto hex_offset = GetHexOffset();

    SprOffset = ipos32 {hex_offset.x, hex_offset.y} + ipos32 {iround<int32>(_offsExt.x), iround<int32>(_offsExt.y)} + _offsAnim;

    if (IsSpriteValid() && GetIsChosen()) {
        _engine->SprMngr.SetEgg(GetHex(), GetSprite());
    }
}

auto CritterHexView::GetNameTextPos(ipos32& pos) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (IsSpriteValid()) {
        const auto rect = GetViewRect();
        const auto rect_half_width = rect.width / 2;
        const auto x = iround<int32>(numeric_cast<float32>(rect.x + rect_half_width + _engine->Settings.ScreenOffset.x) / _map->GetSpritesZoom());
        const auto y = iround<int32>(numeric_cast<float32>(rect.y + _engine->Settings.ScreenOffset.y) / _map->GetSpritesZoom()) + _engine->Settings.NameOffset + GetNameOffset();

        pos = {x, y};
        return true;
    }

    return false;
}

FO_END_NAMESPACE();
