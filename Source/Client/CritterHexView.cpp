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

    RefreshView(true);
    RefreshOffs();

    SetDrawEffect(_engine->EffectMngr.Effects.Critter.get());
}

void CritterHexView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    CritterView::OnDestroySelf();

    _spr = nullptr;

#if FO_ENABLE_3D
    if (_model) {
        _model->AnimationCallbacks.clear();
        _model->SetAnimInitCallback({});
    }

    _modelSpr.reset();
    _model.reset();
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

void CritterHexView::SetMoving(refcount_ptr<MovingContext> moving)
{
    FO_STACK_TRACE_ENTRY();

    if (_moving != nullptr) {
        _moving->Complete(MovingState::Stopped);
    }

    _moving = std::move(moving);
}

void CritterHexView::StopMoving()
{
    FO_STACK_TRACE_ENTRY();

    if (_moving != nullptr) {
        _moving->Complete(MovingState::Stopped);
    }

    _moving = nullptr;
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
        StopMoving();
        break;
    case CritterAction::StandUp:
        SetCondition(CritterCondition::Alive);
        break;
    case CritterAction::Dead: {
        SetCondition(CritterCondition::Dead);
        StopMoving();

#if FO_ENABLE_3D
        if (_model) {
            _resetTime = _engine->GameTime.GetFrameTime() + _model->GetAnimDuration();
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

    _engine->OnCritterAnimationInit.Fire(this, _curAnim->StateAnim, _curAnim->ActionAnim, _curAnim->ContextItem.get());

#if FO_ENABLE_3D
    if (_model) {
        if (!_model->ResolveAnimation(_curAnim->StateAnim, _curAnim->ActionAnim)) {
            NextAnim();
            return;
        }
    }
    else
#endif
    {
        const auto* frames = _engine->ResMngr.GetCritterAnimFrames(GetModelName(), _curAnim->StateAnim, _curAnim->ActionAnim, GetDir());

        if (frames == nullptr) {
            NextAnim();
            return;
        }

        _curAnim->Frames = frames;
        _curAnim->FrameIndex = 0;
        _curAnim->FramesDuration = timespan(std::chrono::milliseconds(frames->GetWholeTicks() != 0 ? frames->GetWholeTicks() : 100));
    }

    _engine->OnCritterAnimationProcess.Fire(this, _curAnim->StateAnim, _curAnim->ActionAnim, _curAnim->ContextItem.get(), false);

#if FO_ENABLE_3D
    if (_model) {
        constexpr auto anim_flags = CombineEnum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoRotate);
        _model->PlayAnim(_curAnim->StateAnim, _curAnim->ActionAnim, GetModelLayersData(), 0.0f, anim_flags);
    }
    else
#endif
    {
        SetAnimSpr(_curAnim->Frames.get(), _curAnim->FrameIndex);
    }
}

void CritterHexView::AppendAnim(CritterStateAnim state_anim, CritterActionAnim action_anim, Entity* context_item)
{
    FO_STACK_TRACE_ENTRY();

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

    _animSequence.emplace_back(CritterAnim {.StateAnim = state_anim, .ActionAnim = action_anim, .ContextItem = resolved_context_item});

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
        const auto scale_factor = GetScaleFactor();
        const auto scale = scale_factor != 0 ? numeric_cast<float32>(scale_factor) / 1000.0f : 1.0f;
        _model->SetScale(scale, scale, scale);

        const auto moving_speed = IsMoving() ? iround<int32>(numeric_cast<float32>(GetMoving()->GetSpeed()) / scale) : 0;
        _model->UpdatePose(GetCondition() == CritterCondition::Alive, IsMoving(), moving_speed);
    }
#endif

    if (!_curAnim.has_value()) {
        _animStartTime = _engine->GameTime.GetFrameTime();

        auto state_anim = CritterStateAnim::None;
        auto action_anim = CritterActionAnim::None;

        _engine->OnCritterAnimationInit.Fire(this, state_anim, action_anim, nullptr);

#if FO_ENABLE_3D
        if (_model) {
            if (IsMoving() && GetCondition() == CritterCondition::Alive) {
                action_anim = _model->GetMovingAnim();
            }

            if (!_model->ResolveAnimation(state_anim, action_anim)) {
                state_anim = CritterStateAnim::Unarmed;
                action_anim = CritterActionAnim::Idle;
            }

            _engine->OnCritterAnimationProcess.Fire(this, state_anim, action_anim, nullptr, true);

            auto anim_flags = no_smooth ? ModelAnimFlags::NoSmooth : ModelAnimFlags::None;

            if (GetCondition() == CritterCondition::Alive || GetCondition() == CritterCondition::Knockout) {
                _model->PlayAnim(state_anim, action_anim, GetModelLayersData(), 0.0f, anim_flags);
            }
            else {
                anim_flags = CombineEnum(anim_flags, ModelAnimFlags::Freeze);
                _model->PlayAnim(state_anim, action_anim, GetModelLayersData(), 1.0f, anim_flags);
            }
        }
        else
#endif
        {
            ignore_unused(no_smooth);

            if (IsMoving()) {
                if (GetMoving()->GetSpeed() < numeric_cast<uint16>(_engine->Settings.RunAnimStartSpeed)) {
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

            SetAnimSpr(_idle2dAnim.Frames.get(), _idle2dAnim.FrameIndex);
        }
    }
}

auto CritterHexView::IsAnimAvailable(CritterStateAnim state_anim, CritterActionAnim action_anim) -> bool
{
    FO_STACK_TRACE_ENTRY();

#if FO_ENABLE_3D
    if (_model) {
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

    auto animCallbacks = _model ? std::move(_model->AnimationCallbacks) : vector<ModelAnimationCallback>();

    _spr = nullptr;

    _modelSpr.reset();
    _model.reset();

    const hstring model_name = GetModelName();
    const string ext = strex(model_name).get_file_extension();

    if (ext == "fo3d") {
        _modelSpr = dynamic_ptr_cast<ModelSprite>(_engine->SprMngr.LoadSprite(model_name, AtlasType::MapSprites));

        if (_modelSpr) {
            _modelSpr->PlayDefault();

            _spr = _modelSpr.get();

            _model = _modelSpr->GetModel();
            _model->AnimationCallbacks = std::move(animCallbacks);
            _model->SetAnimInitCallback([this](CritterStateAnim& state_anim, CritterActionAnim& action_anim) FO_DEFERRED {
                // Callback from 3d
                _engine->OnCritterAnimationInit.Fire(this, state_anim, action_anim, nullptr);
            });

            _model->SetLookDirAngle(GetDirAngle());
            _model->SetMoveDirAngle(GetDirAngle(), false);

            if (_map->IsMapperMode()) {
                _model->PlayAnim(CritterStateAnim::Unarmed, CritterActionAnim::Idle, GetModelLayersData(), 0.0f, ModelAnimFlags::None);
                _model->PrewarmParticles();
                _model->StartMeshGeneration();
            }
        }
        else {
            BreakIntoDebugger();

            _spr = _engine->ResMngr.GetCritterDummyFrames();
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
    if (_model) {
        if (!_model->HasBodyRotation()) {
            _model->SetMoveDirAngle(normalized_dir_angle, true);
        }

        _model->SetLookDirAngle(normalized_dir_angle);
    }
#endif

    RefreshView();
}

void CritterHexView::ChangeMoveDirAngle(int32 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

#if FO_ENABLE_3D
    if (_model) {
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

        const auto dist = _offsExt.dist();
        const auto dist_div = iround<int32>(dist) / 10;
        const auto mul = std::max(numeric_cast<float32>(dist_div), 1.0f);

        _offsExt.x += _offsExtSpeed.x * mul;
        _offsExt.y += _offsExtSpeed.y * mul;

        if (_offsExt.dist() > dist) {
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
#if FO_ENABLE_3D
    if (!_model)
#endif
    {
        auto& cur_anim = _curAnim.has_value() ? _curAnim.value() : _idle2dAnim;

        const auto anim_proc = (_engine->GameTime.GetFrameTime() - _animStartTime).to_ms<int32>() * 100 / cur_anim.FramesDuration.to_ms<int32>();
        const auto frm_proc = _curAnim.has_value() ? std::min(anim_proc, 100) : anim_proc % 100;
        const auto frm_index = lerp(0, cur_anim.Frames->GetFramesCount() - 1, numeric_cast<float32>(frm_proc) / 100.0f);

        if (frm_index != cur_anim.FrameIndex) {
            cur_anim.FrameIndex = frm_index;
            SetAnimSpr(cur_anim.Frames.get(), cur_anim.FrameIndex);
        }
    }

    if (_curAnim.has_value()) {
        bool anim_complete = false;

#if FO_ENABLE_3D
        if (_model) {
            anim_complete = !_model->IsAnimationPlaying();
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
        _map->ReapplyCritterView(this);
        _needReset = false;
    }
}

void CritterHexView::ProcessMoving()
{
    FO_STACK_TRACE_ENTRY();

    auto* moving = GetMoving();
    FO_RUNTIME_ASSERT(moving);
    moving->ValidateRuntimeState();

    if (!IsAlive()) {
        StopMoving();
        RefreshView();
        return;
    }

    moving->UpdateCurrentTime(_engine->GameTime.GetFrameTime());
    const auto progress = moving->EvaluateProgress();
    const auto prev_hex = GetHex();

    _map->MoveCritter(this, progress.Hex, false);

    const auto cur_hex = GetHex();
    const auto moved = cur_hex != prev_hex;
    const auto hex_offset = GetHexOffset();

    if (moved || hex_offset != progress.HexOffset) {
#if FO_ENABLE_3D
        if (_model) {
            ipos32 model_offset;

            if (moved) {
                model_offset = GeometryHelper::GetHexOffset(prev_hex, cur_hex);
            }

            model_offset.x -= hex_offset.x - progress.HexOffset.x;
            model_offset.y -= hex_offset.y - progress.HexOffset.y;

            _model->MoveModel(model_offset);
        }
#endif

        SetHexOffset(progress.HexOffset);
        RefreshOffs();
    }

#if FO_ENABLE_3D
    if (_model) {
        if (GetControlledByPlayer()) {
            ChangeMoveDirAngle(progress.DirAngle);
        }
        else {
            ChangeDirAngle(progress.DirAngle);
        }
    }
    else
#endif
    {
        ChangeDir(GeometryHelper::AngleToDir(progress.DirAngle));
    }

    if (progress.Completed && GetHex() == moving->GetEndHex()) {
        StopMoving();
        RefreshView();
    }
}

auto CritterHexView::GetViewRect() const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(IsMapSpriteValid());

    return GetMapSprite()->GetViewRect();
}

void CritterHexView::SetAnimSpr(const SpriteSheet* anim, int32 frm_index)
{
    FO_STACK_TRACE_ENTRY();

    const auto cur_index = frm_index % anim->GetFramesCount();

    _spr = anim->GetSpr(cur_index);

    _offsAnim = {};

    if (anim->GetActionAnim() == CritterActionAnim::Walk || anim->GetActionAnim() == CritterActionAnim::Run) {
        // ...
    }
    else {
        for (const auto i : iterate_range(cur_index + 1)) {
            _offsAnim.x += anim->GetSprOffset()[i].x;
            _offsAnim.y += anim->GetSprOffset()[i].y;
        }
    }

    RefreshOffs();
}

void CritterHexView::AddExtraOffs(ipos32 offset)
{
    FO_STACK_TRACE_ENTRY();

    _offsExt.x += numeric_cast<float32>(offset.x);
    _offsExt.y += numeric_cast<float32>(offset.y);

    _offsExtSpeed = GeometryHelper::GetStepsCoords({}, {iround<int32>(_offsExt.x), iround<int32>(_offsExt.y)});
    _offsExtSpeed.x = -_offsExtSpeed.x;
    _offsExtSpeed.y = -_offsExtSpeed.y;

    _offsExtNextTime = _engine->GameTime.GetFrameTime() + std::chrono::milliseconds {30};

    RefreshOffs();
}

void CritterHexView::RefreshOffs()
{
    FO_STACK_TRACE_ENTRY();

    const auto hex_offset = GetHexOffset();
    _sprOffset = ipos32(hex_offset) + _offsExt.round<int32>() + _offsAnim;
}

auto CritterHexView::GetNameTextPos(ipos32& pos) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (IsMapSpriteValid()) {
        const irect32 rect = GetViewRect();
        pos = _map->MapToScreenPos({rect.x + rect.width / 2, rect.y});
        pos.y += _engine->Settings.NameOffset + GetNameOffset();
        return true;
    }

    return false;
}

FO_END_NAMESPACE
