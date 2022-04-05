//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "CritterView.h"
#include "Client.h"
#include "EffectManager.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"
#include "ItemView.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"

CritterView::CritterView(FOClient* engine, uint id, const ProtoCritter* proto, bool mapper_mode) : ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), proto), CritterProperties(GetInitRef()), _mapperMode {mapper_mode}
{
    _tickFidget = _engine->GameTime.GameTick() + GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2u);
    DrawEffect = _engine->EffectMngr.Effects.Critter;
    auto layers = GetModelLayers();
    layers.resize(LAYERS3D_COUNT);
    SetModelLayers(layers);
}

CritterView::~CritterView()
{
    if (_model != nullptr) {
        _engine->SprMngr.FreeModel(_model);
    }
    if (_modelStay != nullptr) {
        _engine->SprMngr.FreeModel(_modelStay);
    }
}

void CritterView::Init()
{
    RefreshModel();
    AnimateStay();

    const auto* si = _engine->SprMngr.GetSpriteInfo(SprId);
    if (si != nullptr) {
        _textRect = IRect {0, 0, si->Width, si->Height};
    }

    SetFade(true);
}

void CritterView::Finish()
{
    SetFade(false);
    _finishingTime = FadingTick;
}

auto CritterView::IsFinishing() const -> bool
{
    return _finishingTime != 0;
}

auto CritterView::IsFinish() const -> bool
{
    return _finishingTime != 0u && _engine->GameTime.GameTick() > _finishingTime;
}

void CritterView::SetFade(bool fade_up)
{
    const auto tick = _engine->GameTime.GameTick();
    FadingTick = tick + FADING_PERIOD - (FadingTick > tick ? FadingTick - tick : 0);
    _fadeUp = fade_up;
    _fadingEnable = true;
}

auto CritterView::GetFadeAlpha() -> uchar
{
    const auto tick = _engine->GameTime.GameTick();
    const auto fading_proc = 100u - GenericUtils::Percent(FADING_PERIOD, FadingTick > tick ? FadingTick - tick : 0u);
    if (fading_proc == 100u) {
        _fadingEnable = false;
    }

    const auto alpha = (_fadeUp ? fading_proc * 255u / 100u : (100u - fading_proc) * 255u / 100u);
    return static_cast<uchar>(alpha);
}

void CritterView::AddItem(ItemView* item)
{
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritId(GetId());

    InvItems.push_back(item);

    std::sort(InvItems.begin(), InvItems.end(), [](ItemView* l, ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });

    if (item->GetCritSlot() != 0u && !IsAnim()) {
        AnimateStay();
    }
}

void CritterView::DeleteItem(ItemView* item, bool animate)
{
    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetCritId(0);
    item->SetCritSlot(0);

    const auto it = std::find(InvItems.begin(), InvItems.end(), item);
    RUNTIME_ASSERT(it != InvItems.end());
    InvItems.erase(it);

    item->MarkAsDestroyed();
    _engine->ScriptSys->RemoveEntity(item);
    item->Release();

    if (animate && !IsAnim()) {
        AnimateStay();
    }
}

void CritterView::DeleteAllItems()
{
    while (!InvItems.empty()) {
        DeleteItem(*InvItems.begin(), false);
    }
}

auto CritterView::GetItem(uint item_id) -> ItemView*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : InvItems) {
        if (item->GetId() == item_id) {
            return item;
        }
    }
    return nullptr;
}

auto CritterView::GetItemByPid(hstring item_pid) -> ItemView*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : InvItems) {
        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }
    return nullptr;
}

auto CritterView::CountItemPid(hstring item_pid) const -> uint
{
    uint result = 0;
    for (const auto* item : InvItems) {
        if (item->GetProtoId() == item_pid) {
            result += item->GetCount();
        }
    }
    return result;
}

auto CritterView::IsCombatMode() const -> bool
{
    return GetTimeoutBattle() > _engine->GameTime.GetFullSecond();
}

auto CritterView::CheckFind(CritterFindType find_type) const -> bool
{
    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (IsEnumSet(find_type, CritterFindType::Players) && IsNpc()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Npc) && !IsNpc()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Alive) && IsDead()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Dead) && !IsDead()) {
        return false;
    }
    return true;
}

auto CritterView::GetAttackDist() -> uint
{
    uint dist = 0;
    _engine->OnCritterGetAttackDistantion.Fire(this, nullptr, 0, dist);
    return dist;
}

void CritterView::DrawStay(IRect r)
{
    if (_engine->GameTime.FrameTick() - _staySprTick > 500) {
        _staySprDir++;
        if (_staySprDir >= _engine->Settings.MapDirCount) {
            _staySprDir = 0;
        }
        _staySprTick = _engine->GameTime.FrameTick();
    }

    const auto dir = (!IsAlive() ? GetDir() : _staySprDir);
    const auto anim1 = GetAnim1();
    const auto anim2 = GetAnim2();

    if (_model == nullptr) {
        auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, dir);
        if (anim != nullptr) {
            const auto spr_id = (IsAlive() ? anim->Ind[0] : anim->Ind[anim->CntFrm - 1]);
            _engine->SprMngr.DrawSpriteSize(spr_id, r.Left, r.Top, r.Width(), r.Height(), false, true, 0);
        }
    }
    else if (_modelStay != nullptr) {
        _modelStay->SetDir(dir);
        _modelStay->SetAnimation(anim1, anim2, GetLayers3dData(), ANIMATION_STAY | ANIMATION_PERIOD(100) | ANIMATION_NO_SMOOTH);
        _engine->SprMngr.Draw3d(r.CenterX(), r.Bottom, _modelStay, COLOR_IFACE);
    }
}

auto CritterView::IsLastHexes() const -> bool
{
    return !_lastHexes.empty();
}

void CritterView::FixLastHexes()
{
    if (IsLastHexes() && std::get<0>(_lastHexes.back()) == GetHexX() && std::get<1>(_lastHexes.back()) == GetHexY()) {
        return;
    }

    _lastHexes.emplace_back(GetHexX(), GetHexY());
}

auto CritterView::PopLastHex() -> tuple<ushort, ushort>
{
    const auto [hx, hy] = _lastHexes.back();
    _lastHexes.pop_back();
    return {hx, hy};
}

void CritterView::Move(uchar dir)
{
    if (dir >= _engine->Settings.MapDirCount || GetIsNoRotate()) {
        dir = 0;
    }

    SetDir(dir);

    const auto time_move = IsRunning ? GetRunTime() : GetWalkTime();

    TickStart(time_move);
    _animStartTick = _engine->GameTime.GameTick();

    if (_model == nullptr) {
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
    }
    else {
        auto anim1 = GetAnim1();
        uint anim2 = IsRunning ? ANIM2_RUN : ANIM2_WALK;
        if (GetIsHide()) {
            anim2 = IsRunning ? ANIM2_SNEAK_RUN : ANIM2_SNEAK_WALK;
        }

        if (auto model_anim = _model->EvaluateAnimation()) {
            anim1 = std::get<0>(*model_anim);
            anim2 = std::get<1>(*model_anim);
        }

        _model->SetDir(dir);

        ClearAnim();
        _animSequence.push_back({nullptr, time_move, 0, 0, true, dir + 1, anim1, anim2, nullptr});
        NextAnim(false);

        const auto [ox, oy] = GetWalkHexOffsets(dir);
        ChangeOffs(ox, oy, true);
    }
}

void CritterView::Action(int action, int action_ext, ItemView* item, bool local_call /* = true */)
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
        _resetTick = _engine->GameTime.GameTick() + (anim != nullptr && anim->Anim != nullptr ? anim->Anim->Ticks : 1000);
    } break;
    case ACTION_CONNECT:
        UnsetBit(Flags, FCRIT_DISCONNECT);
        break;
    case ACTION_DISCONNECT:
        SetBit(Flags, FCRIT_DISCONNECT);
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
        if (_model != nullptr) {
            _model->StartMeshGeneration();
        }
        break;
    default:
        break;
    }

    if (!IsAnim()) {
        AnimateStay();
    }
}

void CritterView::NextAnim(bool erase_front)
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

    auto& cr_anim = _animSequence[0];
    _animStartTick = _engine->GameTime.GameTick();

    ProcessAnim(false, _model == nullptr, cr_anim.IndAnim1, cr_anim.IndAnim2, cr_anim.ActiveItem);

    if (_model == nullptr) {
        _lastEndSpr = cr_anim.EndFrm;
        _curSpr = cr_anim.BeginFrm;

        SprId = cr_anim.Anim->GetSprId(_curSpr);

        short ox = 0;
        short oy = 0;
        for (const auto i : xrange(_curSpr % cr_anim.Anim->CntFrm + 1u)) {
            ox = static_cast<short>(ox + cr_anim.Anim->NextX[i]);
            oy = static_cast<short>(oy + cr_anim.Anim->NextY[i]);
        }

        SetOffs(ox, oy, cr_anim.MoveText);
    }
    else {
        SetOffs(0, 0, cr_anim.MoveText);

        _model->SetAnimation(cr_anim.IndAnim1, cr_anim.IndAnim2, GetLayers3dData(), (cr_anim.DirOffs != 0 ? 0 : ANIMATION_ONE_TIME) | (IsCombatMode() ? ANIMATION_COMBAT : 0));
    }
}

void CritterView::Animate(uint anim1, uint anim2, ItemView* item)
{
    const auto dir = GetDir();
    if (anim1 == 0u) {
        anim1 = GetAnim1();
    }
    if (item != nullptr) {
        item = item->Clone();
    }

    if (_model == nullptr) {
        auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, dir);
        if (anim == nullptr) {
            if (!IsAnim()) {
                AnimateStay();
            }
            return;
        }

        // Todo: migrate critter on head text moving in scripts
        const auto move_text = true;
        //			(Cond==COND_DEAD || Cond==COND_KNOCKOUT ||
        //			(anim2!=ANIM2_SHOW_WEAPON && anim2!=ANIM2_HIDE_WEAPON && anim2!=ANIM2_PREPARE_WEAPON &&
        // anim2!=ANIM2_TURNOFF_WEAPON && 			anim2!=ANIM2_DODGE_FRONT && anim2!=ANIM2_DODGE_BACK &&
        // anim2!=ANIM2_USE
        // &&
        // anim2!=ANIM2_PICKUP && 			anim2!=ANIM2_DAMAGE_FRONT && anim2!=ANIM2_DAMAGE_BACK && anim2!=ANIM2_IDLE
        // && anim2!=ANIM2_IDLE_COMBAT));

        _animSequence.push_back({anim, anim->Ticks, 0, anim->CntFrm - 1, move_text, 0, anim->Anim1, anim->Anim2, item});
    }
    else {
        if (auto model_anim = _model->EvaluateAnimation()) {
            anim1 = std::get<0>(*model_anim);
            anim2 = std::get<1>(*model_anim);
        }
        else {
            if (!IsAnim()) {
                AnimateStay();
            }
            return;
        }

        _animSequence.push_back({nullptr, 0, 0, 0, true, 0, anim1, anim2, item});
    }

    if (_animSequence.size() == 1) {
        NextAnim(false);
    }
}

void CritterView::AnimateStay()
{
    auto anim1 = GetAnim1();
    auto anim2 = GetAnim2();

    if (_model == nullptr) {
        auto* anim = _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, GetDir());
        if (anim == nullptr) {
            anim = _engine->ResMngr.CritterDefaultAnim;
        }

        if (_stayAnim.Anim != anim) {
            ProcessAnim(true, true, anim1, anim2, nullptr);

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

        SetOffs(0, 0, true);

        short ox = 0;
        short oy = 0;
        for (const auto i : xrange(_curSpr % anim->CntFrm + 1u)) {
            ox = static_cast<short>(ox + anim->NextX[i]);
            oy = static_cast<short>(oy + anim->NextY[i]);
        }

        ChangeOffs(ox, oy, false);
    }
    else {
        _model->SetDir(GetDir());

        const auto scale_factor = GetScaleFactor();
        if (scale_factor != 0) {
            const auto scale = static_cast<float>(scale_factor) / 1000.0f;
            _model->SetScale(scale, scale, scale);
        }

        if (auto model_anim = _model->EvaluateAnimation()) {
            anim1 = std::get<0>(*model_anim);
            anim2 = std::get<1>(*model_anim);
        }
        else {
            anim1 = ANIM1_UNARMED;
            anim2 = ANIM2_IDLE;
        }

        ProcessAnim(true, false, anim1, anim2, nullptr);
        SetOffs(0, 0, true);

        if (GetCond() == CritterCondition::Alive || GetCond() == CritterCondition::Knockout) {
            _model->SetAnimation(anim1, anim2, GetLayers3dData(), IsCombatMode() ? ANIMATION_COMBAT : 0);
        }
        else {
            _model->SetAnimation(anim1, anim2, GetLayers3dData(), ANIMATION_STAY | ANIMATION_PERIOD(100) | (IsCombatMode() ? ANIMATION_COMBAT : 0));
        }
    }
}

auto CritterView::IsWalkAnim() const -> bool
{
    if (!_animSequence.empty()) {
        const int anim2 = _animSequence[0].IndAnim2;
        return anim2 == ANIM2_WALK || anim2 == ANIM2_RUN || anim2 == ANIM2_LIMP || anim2 == ANIM2_PANIC_RUN;
    }
    return false;
}

void CritterView::ClearAnim()
{
    for (auto& i : _animSequence) {
        i.ActiveItem->Release();
    }
    _animSequence.clear();
}

auto CritterView::IsHaveLightSources() const -> bool
{
    for (const auto* item : InvItems) {
        if (item->GetIsLight()) {
            return true;
        }
    }
    return false;
}

auto CritterView::IsNeedReset() const -> bool
{
    return _needReset && _engine->GameTime.GameTick() >= _resetTick;
}

void CritterView::ResetOk()
{
    _needReset = false;
}

void CritterView::TickStart(uint ms)
{
    _tickCount = ms;
    _startTick = _engine->GameTime.GameTick();
}

void CritterView::TickNull()
{
    _tickCount = 0;
}

auto CritterView::IsFree() const -> bool
{
    return _engine->GameTime.GameTick() - _startTick >= _tickCount;
}

auto CritterView::GetAnim1() const -> uint
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

auto CritterView::GetAnim2() const -> uint
{
    switch (GetCond()) {
    case CritterCondition::Alive:
        return GetAnim2Alive() != 0u ? GetAnim2Alive() : IsCombatMode() && _engine->Settings.Anim2CombatIdle != 0u ? _engine->Settings.Anim2CombatIdle : ANIM2_IDLE;
    case CritterCondition::Knockout:
        return GetAnim2Knockout() != 0u ? GetAnim2Knockout() : ANIM2_IDLE_PRONE_FRONT;
    case CritterCondition::Dead:
        return GetAnim2Dead() != 0u ? GetAnim2Dead() : ANIM2_DEAD_FRONT;
    }
    return ANIM2_IDLE;
}

void CritterView::ProcessAnim(bool animate_stay, bool is2d, uint anim1, uint anim2, ItemView* item)
{
    if (is2d) {
        _engine->OnAnimation2dProcess.Fire(animate_stay, this, anim1, anim2, item);
    }
    else {
        _engine->OnAnimation3dProcess.Fire(animate_stay, this, anim1, anim2, item);
    }
}

auto CritterView::GetLayers3dData() -> int*
{
    auto layers = GetModelLayers();
    RUNTIME_ASSERT(layers.size() == LAYERS3D_COUNT);
    std::memcpy(_modelLayers, &layers[0], sizeof(_modelLayers));
    return _modelLayers;
}

auto CritterView::IsAnimAvailable(uint anim1, uint anim2) const -> bool
{
    if (anim1 == 0u) {
        anim1 = GetAnim1();
    }
    if (_model != nullptr) {
        return _model->HasAnimation(anim1, anim2);
    }
    return _engine->ResMngr.GetCritterAnim(GetModelName(), anim1, anim2, GetDir()) != nullptr;
}

void CritterView::RefreshModel()
{
    // Release previous
    if (_model != nullptr) {
        _engine->SprMngr.FreeModel(_model);
        _model = nullptr;
    }

    if (_modelStay != nullptr) {
        _engine->SprMngr.FreeModel(_modelStay);
        _modelStay = nullptr;
    }

    // Check 3d availability
    const string ext = _str(GetModelName()).getFileExtension();
    if (ext != "fo3d") {
        return;
    }

    // Try load
    _engine->SprMngr.PushAtlasType(AtlasType::Dynamic);

    auto* model = _engine->SprMngr.LoadModel(GetModelName(), true);
    if (model != nullptr) {
        _model = model;
        _modelStay = _engine->SprMngr.LoadModel(GetModelName(), false);

        _model->SetDir(GetDir());
        SprId = _model->SprId;

        _model->SetAnimation(ANIM1_UNARMED, ANIM2_IDLE, GetLayers3dData(), 0);

        // Start mesh generation for Mapper
        if (_mapperMode) {
            _model->StartMeshGeneration();
            _modelStay->StartMeshGeneration();
        }
    }

    _engine->SprMngr.PopAtlasType();
}

void CritterView::ChangeDir(uchar dir, bool animate /* = true */)
{
    if (dir >= _engine->Settings.MapDirCount || GetIsNoRotate()) {
        dir = 0;
    }
    if (GetDir() == dir) {
        return;
    }

    SetDir(dir);

    if (_model != nullptr) {
        _model->SetDir(dir);
    }

    if (animate && !IsAnim()) {
        AnimateStay();
    }
}

void CritterView::Process()
{
    // Fading
    if (_fadingEnable) {
        Alpha = GetFadeAlpha();
    }

    // Extra offsets
    if (_offsExtNextTick != 0u && _engine->GameTime.GameTick() >= _offsExtNextTick) {
        _offsExtNextTick = _engine->GameTime.GameTick() + 30;

        SprOx = static_cast<short>(SprOx - _oxExtI);
        SprOy = static_cast<short>(SprOy - _oyExtI);

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

        if (GenericUtils::DistSqrt(0, 0, _oxExtI, _oyExtI) > dist) // End of work
        {
            _offsExtNextTick = 0;
            _oxExtI = 0;
            _oyExtI = 0;
        }

        SetOffs(SprOx, SprOy, true);
    }

    // Animation
    auto& cr_anim = (!_animSequence.empty() ? _animSequence[0] : _stayAnim);
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
    if (_model == nullptr && anim_proc < 100u) {
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

            ChangeOffs(ox, oy, cr_anim.MoveText);
        }
    }

    if (!_animSequence.empty()) {
        // Move offsets
        if (cr_anim.DirOffs != 0) {
            const auto [ox, oy] = GetWalkHexOffsets(static_cast<uchar>(cr_anim.DirOffs - 1));

            SetOffs(static_cast<short>(ox - ox * anim_proc / 100), static_cast<short>(oy - oy * anim_proc / 100), true);

            if (anim_proc >= 100u) {
                NextAnim(true);
                if (!MoveSteps.empty()) {
                    return;
                }
            }
        }
        else {
            if (_model == nullptr) {
                if (anim_proc >= 100u) {
                    NextAnim(true);
                }
            }
            else {
                if (!_model->IsAnimationPlaying()) {
                    NextAnim(true);
                }
            }
        }

        if (!MoveSteps.empty()) {
            return;
        }

        if (_animSequence.empty()) {
            AnimateStay();
        }
    }

    // Battle 3d mode
    // Todo: do same for 2d animations
    if (_model != nullptr && _engine->Settings.Anim2CombatIdle != 0u && _animSequence.empty() && GetCond() == CritterCondition::Alive && GetAnim2Alive() == 0u) {
        if (_engine->Settings.Anim2CombatBegin != 0u && IsCombatMode() && _model->GetAnim2() != static_cast<int>(_engine->Settings.Anim2CombatIdle)) {
            Animate(0, _engine->Settings.Anim2CombatBegin, nullptr);
        }
        else if (_engine->Settings.Anim2CombatEnd != 0u && !IsCombatMode() && _model->GetAnim2() == static_cast<int>(_engine->Settings.Anim2CombatIdle)) {
            Animate(0, _engine->Settings.Anim2CombatEnd, nullptr);
        }
    }

    // Fidget animation
    if (_engine->GameTime.GameTick() >= _tickFidget) {
        if (_animSequence.empty() && GetCond() == CritterCondition::Alive && IsFree() && MoveSteps.empty() && !IsCombatMode()) {
            Action(ACTION_FIDGET, 0, nullptr, false);
        }

        _tickFidget = _engine->GameTime.GameTick() + GenericUtils::Random(_engine->Settings.CritterFidgetTime, _engine->Settings.CritterFidgetTime * 2);
    }
}

void CritterView::ChangeOffs(short change_ox, short change_oy, bool move_text)
{
    SetOffs(static_cast<short>(SprOx - _oxExtI + change_ox), static_cast<short>(SprOy - _oyExtI + change_oy), move_text);
}

void CritterView::SetOffs(short set_ox, short set_oy, bool move_text)
{
    SprOx = static_cast<short>(set_ox + _oxExtI);
    SprOy = static_cast<short>(set_oy + _oyExtI);

    if (SprDrawValid) {
        DRect = _engine->SprMngr.GetDrawRect(SprDraw);

        if (move_text) {
            _textRect = DRect;
            if (_model != nullptr) {
                _textRect.Top += _engine->SprMngr.GetSpriteInfo(SprId)->Height / 6;
            }
        }

        if (IsChosen()) {
            _engine->SprMngr.SetEgg(GetHexX(), GetHexY(), SprDraw);
        }
    }
}

void CritterView::SetSprRect()
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

auto CritterView::GetTextRect() const -> IRect
{
    if (SprDrawValid) {
        return _textRect;
    }
    return IRect();
}

void CritterView::AddOffsExt(short ox, short oy)
{
    SprOx = static_cast<short>(SprOx - _oxExtI);
    SprOy = static_cast<short>(SprOy - _oyExtI);
    ox = static_cast<short>(ox + _oxExtI);
    oy = static_cast<short>(oy + _oyExtI);
    _oxExtI = ox;
    _oyExtI = oy;
    _oxExtF = static_cast<float>(ox);
    _oyExtF = static_cast<float>(oy);
    std::tie(_oxExtSpeed, _oyExtSpeed) = GenericUtils::GetStepsXY(0, 0, ox, oy);
    _oxExtSpeed = -_oxExtSpeed;
    _oyExtSpeed = -_oyExtSpeed;
    _offsExtNextTick = _engine->GameTime.GameTick() + 30;
    SetOffs(SprOx, SprOy, true);
}

auto CritterView::GetWalkHexOffsets(uchar dir) const -> tuple<short, short>
{
    auto hx = 1;
    auto hy = 1;
    _engine->GeomHelper.MoveHexByDirUnsafe(hx, hy, dir);
    return _engine->GeomHelper.GetHexInterval(hx, hy, 1, 1);
}

void CritterView::SetText(string_view str, uint color, uint text_delay)
{
    _tickStartText = _engine->GameTime.GameTick();
    _strTextOnHead = str;
    _tickTextDelay = text_delay;
    _textOnHeadColor = color;
}

void CritterView::GetNameTextInfo(bool& name_visible, int& x, int& y, int& w, int& h, int& lines)
{
    name_visible = false;

    string str;
    if (_strTextOnHead.empty()) {
        if (IsPlayer() && !_engine->Settings.ShowPlayerNames) {
            return;
        }
        if (IsNpc() && !_engine->Settings.ShowNpcNames) {
            return;
        }

        name_visible = true;

        str = _nameOnHead.empty() ? AlternateName : _nameOnHead;
        if (_engine->Settings.ShowCritId) {
            str += _str("  {}", GetId());
        }
        if (IsBitSet(Flags, FCRIT_DISCONNECT)) {
            str += _engine->Settings.PlayerOffAppendix;
        }
    }
    else {
        str = _strTextOnHead;
    }

    const auto tr = GetTextRect();
    const auto tr_half_width = tr.Width() / 2;
    x = static_cast<int>(static_cast<float>(tr.Left + tr_half_width + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom - 100.0f);
    y = static_cast<int>(static_cast<float>(tr.Top + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom - 70.0f);

    if (_engine->SprMngr.GetTextInfo(200, 70, str, -1, FT_CENTERX | FT_BOTTOM | FT_BORDERED, w, h, lines)) {
        x += 100 - w / 2;
        y += 70 - h;
    }
}

void CritterView::DrawTextOnHead()
{
    if (_strTextOnHead.empty()) {
        if (IsPlayer() && !_engine->Settings.ShowPlayerNames) {
            return;
        }
        if (IsNpc() && !_engine->Settings.ShowNpcNames) {
            return;
        }
    }

    if (SprDrawValid) {
        const auto tr = GetTextRect();
        const auto tr_half_width = tr.Width() / 2;
        const auto x = static_cast<int>(static_cast<float>(tr.Left + tr_half_width + _engine->Settings.ScrOx) / _engine->Settings.SpritesZoom - 100.0f);
        const auto y = static_cast<int>(static_cast<float>(tr.Top + _engine->Settings.ScrOy) / _engine->Settings.SpritesZoom - 70.0f);
        const IRect r(x, y, x + 200, y + 70);

        string str;
        uint color;
        if (_strTextOnHead.empty()) {
            str = _nameOnHead.empty() ? AlternateName : _nameOnHead;

            if (_engine->Settings.ShowCritId) {
                str += _str(" ({})", GetId());
            }
            if (IsBitSet(Flags, FCRIT_DISCONNECT)) {
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
