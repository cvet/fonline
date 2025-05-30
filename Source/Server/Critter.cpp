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

#include "Critter.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

Critter::Critter(FOServer* engine, ident_t id, const ProtoCritter* proto, const Properties* props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(GetInitRef())
{
    FO_STACK_TRACE_ENTRY();
}

Critter::~Critter()
{
    FO_STACK_TRACE_ENTRY();
}

auto Critter::GetOfflineTime() const -> timespan
{
    FO_STACK_TRACE_ENTRY();

    return GetControlledByPlayer() && _player == nullptr ? _engine->GameTime.GetFrameTime() - _playerDetachTime : timespan::zero;
}

auto Critter::IsAlive() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetCondition() == CritterCondition::Alive;
}

auto Critter::IsDead() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetCondition() == CritterCondition::Dead;
}

auto Critter::IsKnockout() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetCondition() == CritterCondition::Knockout;
}

auto Critter::CheckFind(CritterFindType find_type) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (IsEnumSet(find_type, CritterFindType::Players) && !GetControlledByPlayer()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Npc) && GetControlledByPlayer()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::NonDead) && IsDead()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Dead) && !IsDead()) {
        return false;
    }

    return true;
}

void Critter::MarkIsForPlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!GetControlledByPlayer());

    SetControlledByPlayer(true);
    _playerDetachTime = _engine->GameTime.GetFrameTime();
}

void Critter::AttachPlayer(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(GetControlledByPlayer());
    FO_RUNTIME_ASSERT(player);
    FO_RUNTIME_ASSERT(!player->GetControlledCritterId());
    FO_RUNTIME_ASSERT(!_player);

    _player = player;

    _player->SetControlledCritterId(GetId());
    _player->SetLastControlledCritterId(GetId());
    _player->SetControlledCritter(this);
}

void Critter::DetachPlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(GetControlledByPlayer());
    FO_RUNTIME_ASSERT(_player);
    FO_RUNTIME_ASSERT(_player->GetControlledCritterId() == GetId());

    _player->SetControlledCritterId({});
    _player->SetControlledCritter(nullptr);

    _player = nullptr;
    _playerDetachTime = _engine->GameTime.GetFrameTime();
}

void Critter::ClearMove()
{
    FO_STACK_TRACE_ENTRY();

    Moving.Uid++;
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

    SetMovingSpeed(0);
}

void Critter::AttachToCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr != this);
    FO_RUNTIME_ASSERT(cr->GetMapId() == GetMapId());
    FO_RUNTIME_ASSERT(!cr->GetIsAttached());
    FO_RUNTIME_ASSERT(!GetIsAttached());
    FO_RUNTIME_ASSERT(AttachedCritters.empty());

    if (IsMoving()) {
        ClearMove();
        SendAndBroadcast_Moving();
    }

    const auto it = std::ranges::find(cr->AttachedCritters, this);
    FO_RUNTIME_ASSERT(it == cr->AttachedCritters.end());
    cr->AttachedCritters.emplace_back(this);

    SetIsAttached(true);
    SetAttachMaster(cr->GetId());

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::DetachFromCritter()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(GetIsAttached());
    FO_RUNTIME_ASSERT(GetAttachMaster());

    auto* cr = _engine->EntityMngr.GetCritter(GetAttachMaster());
    FO_RUNTIME_ASSERT(cr);

    const auto it = std::ranges::find(cr->AttachedCritters, this);
    FO_RUNTIME_ASSERT(it != cr->AttachedCritters.end());
    cr->AttachedCritters.erase(it);

    SetIsAttached(false);
    SetAttachMaster({});

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::MoveAttachedCritters()
{
    FO_STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        return;
    }

    // Sync position
    auto* map = _engine->EntityMngr.GetMap(GetMapId());
    FO_RUNTIME_ASSERT(map);

    vector<tuple<Critter*, mpos, refcount_ptr<Critter>>> moved_critters;

    const auto new_hex = GetHex();
    const auto new_hex_offset = GetHexOffset();

    for (auto* cr : AttachedCritters) {
        FO_RUNTIME_ASSERT(!cr->IsDestroyed());
        FO_RUNTIME_ASSERT(cr->GetIsAttached());
        FO_RUNTIME_ASSERT(cr->GetAttachMaster() == GetId());
        FO_RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

        cr->SetHexOffset(new_hex_offset);

        const auto hex = cr->GetHex();

        if (hex != new_hex) {
            map->RemoveCritterFromField(cr);
            cr->SetHex(new_hex);
            map->AddCritterToField(cr);

            moved_critters.emplace_back(cr, hex, cr);
        }
    }

    // Callbacks time
    refcount_ptr this_ref_holder = this;
    refcount_ptr map_ref_holder = map;
    const auto dir = GeometryHelper::AngleToDir(GetDirAngle());

    for (const auto& moved_critter : moved_critters) {
        Critter* cr = std::get<0>(moved_critter);
        const mpos prev_hex = std::get<1>(moved_critter);

        const auto is_cr_valid = [cr, map] {
            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return false;
            }
            if (cr->GetMapId() != map->GetId()) {
                return false;
            }
            return true;
        };

        if (!is_cr_valid()) {
            continue;
        }

        _engine->VerifyTrigger(map, cr, prev_hex, new_hex, dir);

        if (!is_cr_valid()) {
            continue;
        }

        _engine->MapMngr.ProcessVisibleCritters(cr);

        if (!is_cr_valid()) {
            continue;
        }

        _engine->MapMngr.ProcessVisibleItems(cr);
    }
}

void Critter::ClearVisible()
{
    FO_STACK_TRACE_ENTRY();

    for (auto* cr : VisCr) {
        auto it_ = std::ranges::find(cr->VisCrSelf, this);
        if (it_ != cr->VisCrSelf.end()) {
            cr->VisCrSelf.erase(it_);
            cr->VisCrSelfMap.erase(GetId());
        }
        cr->Send_RemoveCritter(this);
    }

    VisCr.clear();
    VisCrMap.clear();

    for (auto* cr : VisCrSelf) {
        auto it_ = std::ranges::find(cr->VisCr, this);
        if (it_ != cr->VisCr.end()) {
            cr->VisCr.erase(it_);
            cr->VisCrMap.erase(GetId());
        }
    }

    VisCrSelf.clear();
    VisCrSelfMap.clear();

    VisCr1.clear();
    VisCr2.clear();
    VisCr3.clear();

    VisItem.clear();
}

auto Critter::GetCrSelf(ident_t cr_id) -> Critter*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = VisCrSelfMap.find(cr_id);

    return it != VisCrSelfMap.end() ? it->second : nullptr;
}

auto Critter::GetCrFromVisCr(CritterFindType find_type, bool vis_cr_self) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto& vis_cr = vis_cr_self ? VisCrSelf : VisCr;

    vector<Critter*> critters;
    critters.reserve(vis_cr.size());

    for (auto* cr : vis_cr) {
        if (cr->CheckFind(find_type)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

auto Critter::GetGlobalMapCritter(ident_t cr_id) const -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(GlobalMapGroup);
    const auto it = std::find_if(GlobalMapGroup->begin(), GlobalMapGroup->end(), [&cr_id](const Critter* other) { return other->GetId() == cr_id; });

    return it != GlobalMapGroup->end() ? *it : nullptr;
}

auto Critter::AddCrIntoVisVec(Critter* add_cr) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto inserted = VisCrMap.emplace(add_cr->GetId(), add_cr).second;

    if (!inserted) {
        return false;
    }

    const auto inserted2 = add_cr->VisCrSelfMap.emplace(GetId(), this).second;
    FO_RUNTIME_ASSERT(inserted2);

    VisCr.emplace_back(add_cr);
    add_cr->VisCrSelf.emplace_back(this);

    return true;
}

auto Critter::DelCrFromVisVec(Critter* del_cr) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it_map = VisCrMap.find(del_cr->GetId());

    if (it_map == VisCrMap.end()) {
        return false;
    }

    VisCrMap.erase(it_map);

    const auto it_map2 = del_cr->VisCrSelfMap.find(GetId());
    FO_RUNTIME_ASSERT(it_map2 != del_cr->VisCrSelfMap.end());
    del_cr->VisCrSelfMap.erase(it_map2);

    const auto it = std::ranges::find(VisCr, del_cr);
    FO_RUNTIME_ASSERT(it != VisCr.end());
    VisCr.erase(it);

    const auto it2 = std::ranges::find(del_cr->VisCrSelf, this);
    FO_RUNTIME_ASSERT(it2 != del_cr->VisCrSelf.end());
    del_cr->VisCrSelf.erase(it2);

    return true;
}

auto Critter::AddCrIntoVisSet1(ident_t cr_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisCr1.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisSet2(ident_t cr_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisCr2.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisSet3(ident_t cr_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisCr3.emplace(cr_id).second;
}

auto Critter::DelCrFromVisSet1(ident_t cr_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisCr1.erase(cr_id) != 0;
}

auto Critter::DelCrFromVisSet2(ident_t cr_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisCr2.erase(cr_id) != 0;
}

auto Critter::DelCrFromVisSet3(ident_t cr_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisCr3.erase(cr_id) != 0;
}

auto Critter::AddIdVisItem(ident_t item_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisItem.emplace(item_id).second;
}

auto Critter::DelIdVisItem(ident_t item_id) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisItem.erase(item_id) != 0;
}

auto Critter::CountIdVisItem(ident_t item_id) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return VisItem.count(item_id) != 0;
}

void Critter::ChangeDir(uint8 dir)
{
    FO_STACK_TRACE_ENTRY();

    const auto normalized_dir = numeric_cast<uint8>(dir % GameSettings::MAP_DIR_COUNT);
    const auto dir_angle = GeometryHelper::DirToAngle(normalized_dir);

    SetDirAngle(dir_angle);
    SetDir(normalized_dir);
}

void Critter::ChangeDirAngle(int32 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    const auto normalized_dir_angle = GeometryHelper::NormalizeAngle(numeric_cast<int16>(dir_angle));
    const auto dir = GeometryHelper::AngleToDir(normalized_dir_angle);

    SetDirAngle(normalized_dir_angle);
    SetDir(dir);
}

void Critter::SetItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);

    vec_add_unique_value(_invItems, item);
}

void Critter::RemoveItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);

    vec_remove_unique_value(_invItems, item);
}

auto Critter::GetInvItem(ident_t item_id) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetId() == item_id) {
            return item;
        }
    }

    return nullptr;
}

auto Critter::GetInvItemByPid(hstring item_pid) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }

    return nullptr;
}

auto Critter::GetInvItemByPidSlot(hstring item_pid, CritterItemSlot slot) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid && item->GetCritterSlot() == slot) {
            return item;
        }
    }

    return nullptr;
}

auto Critter::GetInvItemSlot(CritterItemSlot slot) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return vec_filter_first(_invItems, [&](const Item* item) noexcept { return item->GetCritterSlot() == slot; });
}

auto Critter::GetInvItemsSlot(CritterItemSlot slot) -> vector<Item*>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return vec_filter(_invItems, [&](const Item* item) { return item->GetCritterSlot() == slot; });
}

auto Critter::CountInvItemPid(hstring pid) const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    int32 count = 0;

    for (const auto* item : _invItems) {
        if (item->GetProtoId() == pid) {
            count += item->GetCount();
        }
    }

    return count;
}

auto Critter::CountInvItems() const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    int32 count = 0;

    for (const auto* item : _invItems) {
        count += item->GetCount();
    }

    return count;
}

void Critter::Broadcast_Property(NetProperty type, const Property* prop, const ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto* cr : VisCr) {
        cr->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Action(CritterAction action, int32 action_data, const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto* cr : VisCr) {
        cr->Send_Action(this, action, action_data, item);
    }
}

void Critter::Broadcast_Dir()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto* cr : VisCr) {
        cr->Send_Dir(this);
    }
}

void Critter::Broadcast_Teleport(mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto* cr : VisCr) {
        cr->Send_Teleport(this, to_hex);
    }
}

void Critter::SendAndBroadcast(const Player* ignore_player, const std::function<void(Critter*)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    if (ignore_player == nullptr || ignore_player != GetPlayer()) {
        callback(this);
    }

    for (auto* cr : VisCr) {
        if (ignore_player == nullptr || ignore_player != cr->GetPlayer()) {
            callback(cr);
        }
    }
}

void Critter::SendAndBroadcast_Moving()
{
    FO_STACK_TRACE_ENTRY();

    Send_Moving(this);

    for (auto* cr : VisCr) {
        cr->Send_Moving(this);
    }
}

void Critter::SendAndBroadcast_Action(CritterAction action, int32 action_data, const Item* context_item)
{
    FO_STACK_TRACE_ENTRY();

    Send_Action(this, action, action_data, context_item);

    for (auto* cr : VisCr) {
        cr->Send_Action(this, action, action_data, context_item);
    }
}

void Critter::SendAndBroadcast_MoveItem(const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    Send_MoveItem(this, item, action, prev_slot);

    for (auto* cr : VisCr) {
        cr->Send_MoveItem(this, item, action, prev_slot);
    }
}

void Critter::SendAndBroadcast_Animate(CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play)
{
    FO_STACK_TRACE_ENTRY();

    Send_Animate(this, state_anim, action_anim, context_item, clear_sequence, delay_play);

    for (auto* cr : VisCr) {
        cr->Send_Animate(this, state_anim, action_anim, context_item, clear_sequence, delay_play);
    }
}

void Critter::SendAndBroadcast_SetAnims(CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim)
{
    FO_STACK_TRACE_ENTRY();

    Send_SetAnims(this, cond, state_anim, action_anim);

    for (auto* cr : VisCr) {
        cr->Send_SetAnims(this, cond, state_anim, action_anim);
    }
}

void Critter::SendAndBroadcast_Attachments()
{
    FO_STACK_TRACE_ENTRY();

    Send_Attachments(this);

    for (auto* cr : VisCr) {
        cr->Send_Attachments(this);
    }
}

auto Critter::IsTalking() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return Talk.Type != TalkType::None;
}

auto Critter::GetTalkingCritters() const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    int32 talkers = 0;

    for (const auto* cr : VisCr) {
        if (cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == GetId()) {
            talkers++;
        }
    }

    return talkers;
}

auto Critter::GetBarterCritters() const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    auto barter = 0;

    for (const auto* cr : VisCr) {
        if (cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == GetId() && cr->Talk.Barter) {
            barter++;
        }
    }

    return barter;
}

auto Critter::IsFreeToTalk() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto max_talkers = GetMaxTalkers();

    if (max_talkers == 0) {
        max_talkers = _engine->Settings.NpcMaxTalkers;
    }

    return GetTalkingCritters() < max_talkers;
}

void Critter::Send_Property(NetProperty type, const Property* prop, const ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Property(type, prop, entity);
    }
}

void Critter::Send_Moving(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Moving(from_cr);
    }
}

void Critter::Send_MovingSpeed(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_MovingSpeed(from_cr);
    }
}

void Critter::Send_Dir(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Dir(from_cr);
    }
}

void Critter::Send_AddCritter(const Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_AddCritter(cr);
    }
}

void Critter::Send_RemoveCritter(const Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_RemoveCritter(cr);
    }
}

void Critter::Send_LoadMap(const Map* map)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_LoadMap(map);
    }
}

void Critter::Send_AddItemOnMap(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_AddItemOnMap(item);
    }
}

void Critter::Send_RemoveItemFromMap(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_RemoveItemFromMap(item);
    }
}

void Critter::Send_AnimateItem(const Item* item, hstring anim_name, bool looped, bool reversed)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_AnimateItem(item, anim_name, looped, reversed);
    }
}

void Critter::Send_ChosenAddItem(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_ChosenAddItem(item);
    }
}

void Critter::Send_ChosenRemoveItem(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_ChosenRemoveItem(item);
    }
}

void Critter::Send_Teleport(const Critter* cr, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Teleport(cr, to_hex);
    }
}

void Critter::Send_Talk()
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Talk();
    }
}

void Critter::Send_TimeSync()
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_TimeSync();
    }
}

void Critter::Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_InfoMessage(info_message, extra_text);
    }
}

void Critter::Send_Action(const Critter* from_cr, CritterAction action, int32 action_data, const Item* context_item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Action(from_cr, action, action_data, context_item);
    }
}

void Critter::Send_MoveItem(const Critter* from_cr, const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_MoveItem(from_cr, item, action, prev_slot);
    }
}

void Critter::Send_Animate(const Critter* from_cr, CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Animate(from_cr, state_anim, action_anim, context_item, clear_sequence, delay_play);
    }
}

void Critter::Send_SetAnims(const Critter* from_cr, CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_SetAnims(from_cr, cond, state_anim, action_anim);
    }
}

void Critter::Send_Effect(hstring eff_pid, mpos hex, uint16 radius)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Effect(eff_pid, hex, radius);
    }
}

void Critter::Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, mpos from_hex, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_FlyEffect(eff_pid, from_cr_id, to_cr_id, from_hex, to_hex);
    }
}

void Critter::Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_PlaySound(cr_id_synchronize, sound_name);
    }
}

void Critter::Send_ViewMap()
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_ViewMap();
    }
}

void Critter::Send_PlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_PlaceToGameComplete();
    }
}

void Critter::Send_SomeItems(const vector<Item*>& items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_SomeItems(items, owned, with_inner_entities, context_param);
    }
}

void Critter::Send_Attachments(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        _player->Send_Attachments(from_cr);
    }
}

FO_END_NAMESPACE();
