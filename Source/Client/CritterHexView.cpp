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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Movement.h"
#include "ResourceManager.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

CritterHexView::CritterHexView(ptr<MapView> map, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props) :
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

    RefreshView(true);
    RefreshOffs();

    SetDrawEffect(_engine->EffectMngr.Effects.Critter);
}

void CritterHexView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    CritterView::OnDestroySelf();

    _spr = nullptr;

#if FO_ENABLE_3D
    if (_model) {
        auto model = _model.as_ptr();
        model->ClearAnimationCallbacks();
        model->SetAnimInitCallback({});
    }

    _modelSpr.reset();
    _model.reset();
#endif
}

void CritterHexView::SetupSprite(ptr<MapSprite> mspr)
{
    FO_STACK_TRACE_ENTRY();

    HexView::SetupSprite(mspr);

    mspr->SetElevation(GetElevation());
    mspr->SetLight(CornerType::EastWest, _map->GetLightData(), _map->GetSize());

    if (!mspr->IsHidden() && GetHideSprite()) {
        mspr->SetHidden(true);
    }
}

void CritterHexView::SetMoving(refcount_ptr<MovingContext> moving)
{
    FO_STACK_TRACE_ENTRY();

    if (_moving) {
        auto moving_ptr = _moving.as_ptr();
        moving_ptr->Complete(MovingState::Stopped);
    }

    _moving = std::move(moving);
    _walkAnchorAnim = nullptr;
    _walkAnchorDisp = {};
}

void CritterHexView::StopMoving()
{
    FO_STACK_TRACE_ENTRY();

    if (_moving) {
        auto moving = _moving.as_ptr();
        moving->Complete(MovingState::Stopped);
    }

    _moving.reset();
    _walkAnchorAnim.reset();
    _walkAnchorDisp = {};

    // Cash any accumulated sub-hex offset into the hex position. Rapid stop/start (key mashing)
    // re-creates the move every tap inheriting the current HexOffset, but the time-based step index
    // restarts at 0, so the critter's real forward progress piles up as an ever-growing HexOffset
    // while the hex stays put — stranding the resting sprite hexes from its logical hex. Snap to the
    // hex the sprite has actually reached, keeping only the sub-hex remainder. Because
    // GetHexPos(hex) + offset is invariant under this re-split, the sprite does not visually jump; the
    // logical hex just catches up to where the sprite already is.
    const auto offset = GetHexOffset();

    if (offset.x != 0 || offset.y != 0) {
        const auto base_pos = GeometryHelper::GetHexPos(GetHex());
        ipos32 residual {};
        const auto raw_hex = GeometryHelper::GetHexPosCoord(ipos32 {base_pos.x + offset.x, base_pos.y + offset.y}, &residual);
        const auto map_size = _map->GetSize();

        if (map_size.is_valid_pos(raw_hex)) {
            const auto snapped_hex = map_size.from_raw_pos(raw_hex);

            if (snapped_hex != GetHex()) {
                ptr<CritterHexView> self = this;
                _map->MoveCritter(self, snapped_hex, false);
            }

            SetHexOffset(ipos16 {numeric_cast<int16_t>(residual.x), numeric_cast<int16_t>(residual.y)});
            RefreshOffs();
        }
    }
}

void CritterHexView::MoveAttachedCritters()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto hex = GetHex();
    const auto hex_offset = GetHexOffset();

    for (const auto cr_id : _attachedCritters) {
        if (nptr<CritterHexView> nullable_cr = _map->GetCritter(cr_id)) {
            auto cr = nullable_cr.as_ptr();

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

void CritterHexView::Action(CritterAction action, int32_t action_data, nptr<Entity> context_item, bool local_call /* = true */)
{
    FO_STACK_TRACE_ENTRY();

    _engine->OnCritterAction.Fire(local_call, this, action, action_data, context_item);

    switch (action) {
    case CritterAction::Knockout:
        SetCondition(CritterCondition::Knockout);
        break;
    case CritterAction::StandUp:
        SetCondition(CritterCondition::Alive);
        break;
    case CritterAction::Dead: {
        SetCondition(CritterCondition::Dead);

#if FO_ENABLE_3D
        if (_model) {
            auto model = _model.as_ptr();
            _resetTime = _engine->GameTime.GetFrameTime() + model->GetAnimDuration();
            _needReset = true;
        }
        else
#endif
        {
            _resetTime = _engine->GameTime.GetFrameTime() + (_curAnim.has_value() ? _curAnim->FramesDuration : timespan());
            _needReset = true;
        }
    } break;
    case CritterAction::Respawn:
        SetCondition(CritterCondition::Alive);
        FadeUp();
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

    RefreshView();
}

void CritterHexView::NextAnim()
{
    FO_STACK_TRACE_ENTRY();

    _curAnim.reset();

    if (_animSequence.empty()) {
        return;
    }

    _animStartTime = _engine->GameTime.GetFrameTime();

    _curAnim = std::move(_animSequence.front());
    _animSequence.erase(_animSequence.begin());

    _engine->OnCritterAnimationInit.Fire(this, _curAnim->StateAnim, _curAnim->ActionAnim, _curAnim->GetContextItem());

#if FO_ENABLE_3D
    if (_model) {
        auto model = _model.as_ptr();

        if (!model->ResolveAnimation(_curAnim->StateAnim, _curAnim->ActionAnim)) {
            NextAnim();
            return;
        }
    }
    else
#endif
    {
        auto nullable_frames = _engine->ResMngr.GetCritterAnimFrames(GetModelName(), _curAnim->StateAnim, _curAnim->ActionAnim, GetDir());

        if (!nullable_frames) {
            NextAnim();
            return;
        }

        auto frames = nullable_frames.as_ptr();

        _curAnim->Frames = frames;
        _curAnim->FrameIndex = 0;
        _curAnim->FramesDuration = timespan(std::chrono::milliseconds(frames->GetWholeTicks() != 0 ? frames->GetWholeTicks() : 100));
    }

    _engine->OnCritterAnimationProcess.Fire(this, _curAnim->StateAnim, _curAnim->ActionAnim, _curAnim->GetContextItem(), false);

#if FO_ENABLE_3D
    if (_model) {
        constexpr auto anim_flags = CombineEnum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoRotate);
        auto model = _model.as_ptr();
        model->PlayAnim(_curAnim->StateAnim, _curAnim->ActionAnim, GetModelLayersData(), 0.0f, anim_flags);
    }
    else
#endif
    {
        SetAnimSpr(_curAnim->Frames.as_ptr(), _curAnim->FrameIndex);
    }
}

void CritterHexView::AppendAnim(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<Entity> context_item)
{
    FO_STACK_TRACE_ENTRY();

    refcount_nptr<Entity> resolved_context_item {};

    if (context_item) {
        if (nptr<ItemView> nullable_item = context_item.dyn_cast<ItemView>(); nullable_item) {
            auto item = nullable_item.as_ptr();
            resolved_context_item = item->CreateRefClone();
        }
        else if (nptr<ProtoItem> nullable_proto = context_item.dyn_cast<ProtoItem>(); nullable_proto) {
            auto proto = nullable_proto.as_ptr();
            resolved_context_item = proto.hold_ref();
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }

    _animSequence.emplace_back(CritterAnim {.StateAnim = state_anim, .ActionAnim = action_anim, .ContextItem = std::move(resolved_context_item)});

    if (!_curAnim.has_value()) {
        NextAnim();
    }
}

void CritterHexView::StopAnim()
{
    FO_STACK_TRACE_ENTRY();

    _animSequence.clear();
    _curAnim.reset();
}

void CritterHexView::RefreshView(bool no_smooth)
{
    FO_STACK_TRACE_ENTRY();

#if FO_ENABLE_3D
    if (_model) {
        auto model = _model.as_ptr();
        const auto scale_factor = GetScaleFactor();
        const auto scale = scale_factor != 0 ? numeric_cast<float32_t>(scale_factor) / 1000.0f : 1.0f;
        model->SetScale(scale, scale, scale);

        int32_t moving_speed = 0;

        if (IsMoving()) {
            auto nullable_moving = GetMoving();
            FO_VERIFY_AND_THROW(nullable_moving, "Critter movement state is missing");
            auto moving = nullable_moving.as_ptr();
            moving_speed = iround<int32_t>(numeric_cast<float32_t>(moving->GetSpeed()) / scale);
        }

        model->UpdatePose(GetCondition() == CritterCondition::Alive, IsMoving(), moving_speed);
    }
#endif

    if (!_curAnim.has_value()) {
        _animStartTime = _engine->GameTime.GetFrameTime();

        auto state_anim = CritterStateAnim::None;
        auto action_anim = CritterActionAnim::None;

        _engine->OnCritterAnimationInit.Fire(this, state_anim, action_anim, nullptr);

#if FO_ENABLE_3D
        if (_model) {
            auto model = _model.as_ptr();

            if (IsMoving() && GetCondition() == CritterCondition::Alive) {
                action_anim = model->GetMovingAnim();
            }

            if (!model->ResolveAnimation(state_anim, action_anim)) {
                state_anim = CritterStateAnim::Unarmed;
                action_anim = CritterActionAnim::Idle;
            }

            _engine->OnCritterAnimationProcess.Fire(this, state_anim, action_anim, nullptr, true);

            auto anim_flags = no_smooth ? ModelAnimFlags::NoSmooth : ModelAnimFlags::None;

            if (GetCondition() == CritterCondition::Alive) {
                model->PlayAnim(state_anim, action_anim, GetModelLayersData(), 0.0f, anim_flags);
            }
            else {
                anim_flags = CombineEnum(anim_flags, ModelAnimFlags::Freeze);
                const float32_t frozen_time = GetCondition() == CritterCondition::Dead ? 1.0f : 0.0f;
                model->PlayAnim(state_anim, action_anim, GetModelLayersData(), frozen_time, anim_flags);
            }
        }
        else
#endif
        {
            ignore_unused(no_smooth);

            if (IsMoving() && GetCondition() == CritterCondition::Alive) {
                auto nullable_moving = GetMoving();
                FO_VERIFY_AND_THROW(nullable_moving, "Critter movement state is missing");
                auto moving = nullable_moving.as_ptr();

                if (moving->GetSpeed() < numeric_cast<uint16_t>(_engine->Settings->RunAnimStartSpeed)) {
                    action_anim = CritterActionAnim::Walk;
                }
                else {
                    action_anim = CritterActionAnim::Run;
                }
            }

            auto nullable_frames = _engine->ResMngr.GetCritterAnimFrames(GetModelName(), state_anim, action_anim, GetDir());

            if (!nullable_frames) {
                nullable_frames = _engine->ResMngr.GetCritterDummyFrames();
            }

            FO_VERIFY_AND_THROW(nullable_frames, "Critter animation frames are missing");
            auto frames = nullable_frames.as_ptr();

            _idle2dAnim.StateAnim = state_anim;
            _idle2dAnim.ActionAnim = action_anim;

            if (_idle2dAnim.Frames != frames) {
                _engine->OnCritterAnimationProcess.Fire(this, state_anim, action_anim, nullptr, true);

                _idle2dAnim.Frames = frames;
                _idle2dAnim.FrameIndex = 0;
                _idle2dAnim.FramesDuration = timespan(std::chrono::milliseconds(frames->GetWholeTicks() != 0 ? frames->GetWholeTicks() : 100));

                if (GetCondition() == CritterCondition::Dead) {
                    _idle2dAnim.FrameIndex = _idle2dAnim.Frames->GetFramesCount() - 1;
                }
            }

            SetAnimSpr(_idle2dAnim.Frames.as_ptr(), _idle2dAnim.FrameIndex);
        }
    }
}

auto CritterHexView::IsAnimAvailable(CritterStateAnim state_anim, CritterActionAnim action_anim) -> bool
{
    FO_STACK_TRACE_ENTRY();

#if FO_ENABLE_3D
    if (_model) {
        auto model = _model.as_ptr();
        return model->HasAnimation(state_anim, action_anim);
    }
#endif

    return !!_engine->ResMngr.GetCritterAnimFrames(GetModelName(), state_anim, action_anim, GetDir());
}

#if FO_ENABLE_3D
auto CritterHexView::GetModelLayersData() const -> ptr<const int32_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto prop_raw_data = GetProperties().GetRawData(GetPropertyModelLayers());
    FO_VERIFY_AND_THROW(prop_raw_data.size() == sizeof(int32_t) * MODEL_LAYERS_COUNT, "Model layer property raw data size does not match layer count", prop_raw_data.size(), MODEL_LAYERS_COUNT, sizeof(int32_t));
    ptr<const uint8_t> data = prop_raw_data.data();
    return data.reinterpret_as<int32_t>();
}

void CritterHexView::RefreshModel()
{
    FO_STACK_TRACE_ENTRY();

    vector<ModelAnimationCallback> animCallbacks {};

    if (_model) {
        auto model = _model.as_ptr();
        animCallbacks = model->TakeAnimationCallbacks();
    }

    _spr = nullptr;

    _modelSpr.reset();
    _model.reset();

    const hstring model_name = GetModelName();
    const string ext = strex(model_name).get_file_extension();

    if (ext == "fo3d") {
        _modelSpr = dynamic_ptr_cast<ModelSprite>(_engine->SprMngr.LoadSprite(model_name, AtlasType::MapSprites));

        if (_modelSpr) {
            _modelSpr->PlayDefault();

            _spr = _modelSpr.as_nptr();

            nptr<ModelInstance> nullable_model = _modelSpr->GetModel();
            FO_VERIFY_AND_THROW(nullable_model, "Model sprite is missing its model instance");
            auto model = nullable_model.as_ptr();

            _model = nullable_model;
            model->SetAnimationCallbacks(std::move(animCallbacks));
            model->SetAnimInitCallback([this](CritterStateAnim& state_anim, CritterActionAnim& action_anim) FO_DEFERRED {
                // Callback from 3d
                _engine->OnCritterAnimationInit.Fire(this, state_anim, action_anim, nullptr);
            });

            model->SetLookDir(GetDir());
            model->SetMoveDir(GetDir(), false);

            if (_map->IsMapperMode()) {
                model->PlayAnim(CritterStateAnim::Unarmed, CritterActionAnim::Idle, GetModelLayersData(), 0.0f, ModelAnimFlags::None);
                model->PrewarmParticles();
                model->StartMeshGeneration();
            }
        }
        else {
            BreakIntoDebugger();

            _spr = _engine->ResMngr.GetCritterDummyFrames();
        }
    }
}
#endif

void CritterHexView::ChangeDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    ChangeLookDir(dir);
    ChangeMoveDir(dir);
}

void CritterHexView::ChangeLookDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    if (dir == GetDir()) {
        return;
    }

    SetDir(dir);

#if FO_ENABLE_3D
    if (_model) {
        auto model = _model.as_ptr();

        if (!model->HasBodyRotation()) {
            model->SetMoveDir(dir, true);
        }

        model->SetLookDir(dir);
    }
#endif

    RefreshView();
}

void CritterHexView::ChangeMoveDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

#if FO_ENABLE_3D
    if (_model) {
        auto model = _model.as_ptr();
        model->SetMoveDir(dir, true);
    }
    else
#endif
    {
        ignore_unused(dir);
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

        const auto dist = _offsExt.dist();
        const auto dist_div = iround<int32_t>(dist) / 10;
        const auto mul = std::max(numeric_cast<float32_t>(dist_div), 1.0f);

        _offsExt.x += _offsExtSpeed.x * mul;
        _offsExt.y += _offsExtSpeed.y * mul;

        if (_offsExt.dist() > dist) {
            _offsExtNextTime = {};
            _offsExt = {};
        }

        RefreshOffs();
    }

    // Attachments
    if (HasAttachedCritters()) {
        MoveAttachedCritters();
    }

    // Animation
#if FO_ENABLE_3D
    if (!_model)
#endif
    {
        auto& cur_anim = _curAnim.has_value() ? _curAnim.value() : _idle2dAnim;

        const auto action = cur_anim.Frames->GetActionAnim();
        const bool root_motion_drive = !_curAnim.has_value() && IsMoving() && (action == CritterActionAnim::Walk || action == CritterActionAnim::Run);

        if (root_motion_drive && cur_anim.Frames != _walkAnchorAnim) {
            const auto pos = EvaluateMovementDisplacement();
            auto new_anim = cur_anim.Frames.as_ptr();

            const auto sum_total = [](ptr<const SpriteSheet> sheet) -> ipos32 {
                ipos32 sum {};

                for (const auto i : iterate_range(sheet->GetFramesCount())) {
                    sum.x += sheet->GetSprOffset()[i].x;
                    sum.y += sheet->GetSprOffset()[i].y;
                }

                return sum;
            };

            ipos32 new_anchor_disp = pos;

            if (_walkAnchorAnim) {
                const auto old_total = sum_total(_walkAnchorAnim.as_ptr());
                const int64_t old_total_dot_total = numeric_cast<int64_t>(old_total.x) * old_total.x + numeric_cast<int64_t>(old_total.y) * old_total.y;

                if (old_total_dot_total > 0) {
                    const int32_t old_rel_x = pos.x - _walkAnchorDisp.x;
                    const int32_t old_rel_y = pos.y - _walkAnchorDisp.y;
                    const int64_t old_rel_dot_total = numeric_cast<int64_t>(old_rel_x) * old_total.x + numeric_cast<int64_t>(old_rel_y) * old_total.y;

                    int64_t old_cycle_proj = old_rel_dot_total % old_total_dot_total;

                    if (old_cycle_proj < 0) {
                        old_cycle_proj += old_total_dot_total;
                    }

                    const auto new_total = sum_total(new_anim);

                    const auto div_round = [](int64_t num, int64_t den) -> int32_t {
                        FO_VERIFY_AND_THROW(den > 0, "Rounding division denominator must be positive");
                        const int64_t half = den / 2;
                        return numeric_cast<int32_t>(num >= 0 ? (num + half) / den : -((-num + half) / den));
                    };

                    new_anchor_disp = ipos32 {
                        pos.x - div_round(old_cycle_proj * new_total.x, old_total_dot_total),
                        pos.y - div_round(old_cycle_proj * new_total.y, old_total_dot_total),
                    };
                }
            }

            _walkAnchorAnim = new_anim;
            _walkAnchorDisp = new_anchor_disp;
        }

        int32_t frm_index;

        if (root_motion_drive) {
            frm_index = EvaluateMovementFrameIndex(cur_anim.Frames.as_ptr());
        }
        else {
            const auto anim_proc = (_engine->GameTime.GetFrameTime() - _animStartTime).to_ms<int32_t>() * 100 / cur_anim.FramesDuration.to_ms<int32_t>();
            const auto frm_proc = _curAnim.has_value() ? std::min(anim_proc, 100) : anim_proc % 100;
            frm_index = lerp(0, cur_anim.Frames->GetFramesCount() - 1, numeric_cast<float32_t>(frm_proc) / 100.0f);
        }

        if (root_motion_drive) {
            cur_anim.FrameIndex = frm_index;
            SetAnimSpr(cur_anim.Frames.as_ptr(), cur_anim.FrameIndex);
        }
        else if (frm_index != cur_anim.FrameIndex) {
            cur_anim.FrameIndex = frm_index;
            SetAnimSpr(cur_anim.Frames.as_ptr(), cur_anim.FrameIndex);
        }
    }

    if (_curAnim.has_value()) {
        bool anim_complete = false;

#if FO_ENABLE_3D
        if (_model) {
            auto model = _model.as_ptr();
            anim_complete = !model->IsAnimationPlaying();
        }
        else
#endif
        {
            anim_complete = _engine->GameTime.GetFrameTime() - _animStartTime >= _curAnim->FramesDuration;
        }

        if (anim_complete) {
            NextAnim();
        }

        RefreshView();
    }

    // Reset critter view
    if (_needReset && _engine->GameTime.GetFrameTime() >= _resetTime) {
        ptr<CritterHexView> self = this;
        _map->ReapplyCritterView(self);
        _needReset = false;
    }
}

void CritterHexView::SynchronizeMoving()
{
    FO_STACK_TRACE_ENTRY();

    if (IsMoving()) {
        ProcessMoving();
    }
}

void CritterHexView::NormalizeHexOffset()
{
    FO_STACK_TRACE_ENTRY();

    mpos hex = GetHex();
    ipos16 hex_offset = GetHexOffset();

    if (!GeometryHelper::NormalizeHexOffset(hex, hex_offset, GetMap()->GetSize())) {
        return;
    }

    if (GetHex() != hex) {
        ptr<CritterHexView> self = this;
        GetMap()->MoveCritter(self, hex, false);
    }

    if (GetHexOffset() != hex_offset) {
        SetHexOffset(hex_offset);
        RefreshOffs();
    }
}

void CritterHexView::ProcessMoving()
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_moving = GetMoving();
    FO_VERIFY_AND_THROW(nullable_moving, "Missing active movement state");
    auto moving = nullable_moving.as_ptr();
    moving->ValidateRuntimeState();

    moving->UpdateCurrentTime(_engine->GameTime.GetFrameTime());
    const auto progress = moving->EvaluateProgress();
    const auto prev_hex = GetHex();

    ptr<CritterHexView> self = this;
    _map->MoveCritter(self, progress.Hex, false);

    const auto cur_hex = GetHex();
    const auto moved = cur_hex != prev_hex;
    const auto hex_offset = GetHexOffset();

    if (moved || hex_offset != progress.HexOffset) {
#if FO_ENABLE_3D
        if (_model) {
            auto model = _model.as_ptr();
            ipos32 model_offset;

            if (moved) {
                model_offset = GeometryHelper::GetHexOffset(prev_hex, cur_hex);
            }

            model_offset.x -= hex_offset.x - progress.HexOffset.x;
            model_offset.y -= hex_offset.y - progress.HexOffset.y;

            model->MoveModel(model_offset);
        }
#endif

        SetHexOffset(progress.HexOffset);
        RefreshOffs();
    }

#if FO_ENABLE_3D
    if (_model) {
        if (GetControlledByPlayer()) {
            ChangeMoveDir(progress.Dir);
        }
        else {
            ChangeDir(progress.Dir);
        }
    }
    else
#endif
    {
        ChangeDir(progress.Dir);
    }

    if (progress.Completed && GetHex() == moving->GetEndHex()) {
        StopMoving();
        RefreshView();
    }
}

auto CritterHexView::GetViewRect() const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsMapSpriteValid(), "Critter map sprite is not valid");

    return GetMapSprite()->GetViewRect();
}

void CritterHexView::SetAnimSpr(ptr<const SpriteSheet> anim, int32_t frm_index)
{
    FO_STACK_TRACE_ENTRY();

    const auto cur_index = frm_index % anim->GetFramesCount();

    _spr = anim->GetSpr(cur_index);

    _offsAnim = {};

    const auto action = anim->GetActionAnim();

    if (action == CritterActionAnim::Walk || action == CritterActionAnim::Run) {
        if (IsMoving()) {
            // Snap the sprite to its frame's intended root-motion position: while a frame is shown
            // the sprite stays put, then jumps by Delta accum on every frame transition. _offsAnim
            // = (cycle_start - disp) + accum[i] cancels the engine's linear hex_offset within the
            // frame; sprite = start_hex + cycle_start + accum[i] is therefore constant per frame.
            const_span<ipos32> spr_offset = anim->GetSprOffset();
            const auto frames_count = anim->GetFramesCount();

            ipos32 total {};

            for (const auto i : iterate_range(frames_count)) {
                total.x += spr_offset[i].x;
                total.y += spr_offset[i].y;
            }

            ipos32 accum {};

            for (const auto i : iterate_range(cur_index + 1)) {
                accum.x += spr_offset[i].x;
                accum.y += spr_offset[i].y;
            }

            const int64_t total_dot_total = numeric_cast<int64_t>(total.x) * total.x + numeric_cast<int64_t>(total.y) * total.y;

            if (total_dot_total > 0) {
                const auto pos = EvaluateMovementDisplacement();
                const int32_t rel_x = pos.x - _walkAnchorDisp.x;
                const int32_t rel_y = pos.y - _walkAnchorDisp.y;
                const int64_t rel_dot_total = numeric_cast<int64_t>(rel_x) * total.x + numeric_cast<int64_t>(rel_y) * total.y;

                // Floor division so negative projections still wrap into [0, total_dot_total).
                int64_t cycle_number;

                if (rel_dot_total >= 0) {
                    cycle_number = rel_dot_total / total_dot_total;
                }
                else {
                    cycle_number = -((-rel_dot_total + total_dot_total - 1) / total_dot_total);
                }

                const int32_t cycle_start_x = _walkAnchorDisp.x + numeric_cast<int32_t>(cycle_number * total.x);
                const int32_t cycle_start_y = _walkAnchorDisp.y + numeric_cast<int32_t>(cycle_number * total.y);

                _offsAnim.x = cycle_start_x - pos.x + accum.x;
                _offsAnim.y = cycle_start_y - pos.y + accum.y;
            }
        }
    }
    else {
        for (const auto i : iterate_range(cur_index + 1)) {
            _offsAnim.x += anim->GetSprOffset()[i].x;
            _offsAnim.y += anim->GetSprOffset()[i].y;
        }
    }

    RefreshOffs();
}

auto CritterHexView::EvaluateMovementDisplacement() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsMoving(), "Critter is not currently moving");

    auto nullable_moving = GetMoving();
    FO_VERIFY_AND_THROW(nullable_moving, "Critter movement state is missing");
    auto moving = nullable_moving.as_ptr();

    const auto start_hex = moving->GetStartHex();
    const auto start_hex_offset = moving->GetStartHexOffset();
    const auto cur_hex = GetHex();
    const auto hex_offset = GetHexOffset();

    const auto cur_from_start = GeometryHelper::GetHexOffset(start_hex, cur_hex);

    return ipos32 {
        cur_from_start.x + numeric_cast<int32_t>(hex_offset.x) - numeric_cast<int32_t>(start_hex_offset.x),
        cur_from_start.y + numeric_cast<int32_t>(hex_offset.y) - numeric_cast<int32_t>(start_hex_offset.y),
    };
}

auto CritterHexView::EvaluateMovementFrameIndex(ptr<const SpriteSheet> anim) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsMoving(), "Critter is not currently moving");

    const auto frames_count = anim->GetFramesCount();
    FO_VERIFY_AND_THROW(frames_count > 0, "Movement animation has no frames", frames_count);

    const_span<ipos32> spr_offset = anim->GetSprOffset();

    ipos32 total {};

    for (const auto i : iterate_range(frames_count)) {
        total.x += spr_offset[i].x;
        total.y += spr_offset[i].y;
    }

    const int64_t total_dot_total = numeric_cast<int64_t>(total.x) * total.x + numeric_cast<int64_t>(total.y) * total.y;

    if (total_dot_total <= 0) {
        return 0;
    }

    // Anchor-relative projection so direction changes restart the cycle from the critter's current
    // position instead of measuring the entire past path against the new direction's T axis.
    const auto pos = EvaluateMovementDisplacement();
    const int32_t rel_x = pos.x - _walkAnchorDisp.x;
    const int32_t rel_y = pos.y - _walkAnchorDisp.y;
    const int64_t rel_dot_total = numeric_cast<int64_t>(rel_x) * total.x + numeric_cast<int64_t>(rel_y) * total.y;

    int64_t cycle_proj = rel_dot_total % total_dot_total;

    if (cycle_proj < 0) {
        cycle_proj += total_dot_total;
    }

    // Find the frame whose accumulated root-motion projection along T is closest to cycle_proj.
    ipos32 accum {};
    int32_t best_index = 0;
    int64_t best_diff = std::numeric_limits<int64_t>::max();

    for (const auto i : iterate_range(frames_count)) {
        accum.x += spr_offset[i].x;
        accum.y += spr_offset[i].y;

        const int64_t accum_dot_total = numeric_cast<int64_t>(accum.x) * total.x + numeric_cast<int64_t>(accum.y) * total.y;
        const int64_t diff = std::abs(accum_dot_total - cycle_proj);

        if (diff < best_diff) {
            best_diff = diff;
            best_index = i;
        }
    }

    return best_index;
}

void CritterHexView::AddExtraOffs(ipos32 offset)
{
    FO_STACK_TRACE_ENTRY();

    _offsExt.x += numeric_cast<float32_t>(offset.x);
    _offsExt.y += numeric_cast<float32_t>(offset.y);

    _offsExtSpeed = GeometryHelper::GetStepsCoords({}, {iround<int32_t>(_offsExt.x), iround<int32_t>(_offsExt.y)});
    _offsExtSpeed.x = -_offsExtSpeed.x;
    _offsExtSpeed.y = -_offsExtSpeed.y;

    _offsExtNextTime = _engine->GameTime.GetFrameTime() + std::chrono::milliseconds {30};

    RefreshOffs();
}

void CritterHexView::RefreshOffs()
{
    FO_STACK_TRACE_ENTRY();

    const auto hex_offset = GetHexOffset();
    _sprOffset = ipos32(hex_offset) + _offsExt.round<int32_t>() + _offsAnim;
}

auto CritterHexView::GetNameTextPos(ipos32& pos) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (IsMapSpriteValid()) {
        const irect32 rect = GetViewRect();
        pos = _map->MapToScreenPos({rect.x + rect.width / 2, rect.y});
        pos.y += _engine->Settings->NameOffset + GetNameOffset();
        return true;
    }

    return false;
}

FO_END_NAMESPACE
