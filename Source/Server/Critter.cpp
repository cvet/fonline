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

    for (auto& cr : AttachedCritters) {
        FO_RUNTIME_ASSERT(!cr->IsDestroyed());
        FO_RUNTIME_ASSERT(cr->GetIsAttached());
        FO_RUNTIME_ASSERT(cr->GetAttachMaster() == GetId());
        FO_RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

        cr->SetHexOffset(new_hex_offset);

        const auto hex = cr->GetHex();

        if (hex != new_hex) {
            map->RemoveCritterFromField(cr.get());
            cr->SetHex(new_hex);
            map->AddCritterToField(cr.get());

            moved_critters.emplace_back(cr.get(), hex, cr.get());
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

void Critter::ClearVisibleEnitites()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(GetMapId());

    while (!_visibleCrWhoSeeMe.empty()) {
        auto* cr = _visibleCrWhoSeeMe.front().get();
        const auto del_ok = RemoveVisibleCritter(cr);
        FO_RUNTIME_ASSERT(del_ok);
        cr->Send_RemoveCritter(this);
    }
    while (!_visibleCr.empty()) {
        auto* cr = _visibleCr.front().get();
        const auto del_ok2 = cr->RemoveVisibleCritter(this);
        FO_RUNTIME_ASSERT(del_ok2);
    }

    _visibleCrGroup1.clear();
    _visibleCrGroup2.clear();
    _visibleCrGroup3.clear();

    _visibleItems.clear();

    FO_RUNTIME_ASSERT(_visibleCrWhoSeeMe.empty());
    FO_RUNTIME_ASSERT(_visibleCrWhoSeeMeMap.empty());
    FO_RUNTIME_ASSERT(_visibleCr.empty());
    FO_RUNTIME_ASSERT(_visibleCrMap.empty());
    FO_RUNTIME_ASSERT(_visibleCrGroup1.empty());
    FO_RUNTIME_ASSERT(_visibleCrGroup2.empty());
    FO_RUNTIME_ASSERT(_visibleCrGroup3.empty());
    FO_RUNTIME_ASSERT(_visibleItems.empty());
}

auto Critter::IsSeeCritter(ident_t cr_id) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        FO_RUNTIME_ASSERT(_globalMapGroup);
        const auto it = std::ranges::find_if(*_globalMapGroup, [&cr_id](auto&& other) { return other->GetId() == cr_id; });
        return it != _globalMapGroup->end() && cr_id != GetId();
    }

    if (_visibleCrMap.count(cr_id) != 0) {
        return true;
    }

    return false;
}

auto Critter::GetCritter(ident_t cr_id, CritterSeeType see_type) -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        FO_RUNTIME_ASSERT(_globalMapGroup);
        const auto it = std::ranges::find_if(*_globalMapGroup, [&cr_id](auto&& other) { return other->GetId() == cr_id; });
        return it != _globalMapGroup->end() && cr_id != GetId() ? it->get() : nullptr;
    }

    if (see_type == CritterSeeType::WhoSeeMe || see_type == CritterSeeType::Any) {
        const auto it = _visibleCrWhoSeeMeMap.find(cr_id);

        if (it != _visibleCrWhoSeeMeMap.end()) {
            return it->second.get();
        }
    }

    if (see_type == CritterSeeType::WhoISee || see_type == CritterSeeType::Any) {
        const auto it = _visibleCrMap.find(cr_id);

        if (it != _visibleCrMap.end()) {
            return it->second.get();
        }
    }

    return nullptr;
}

auto Critter::GetCritters(CritterSeeType see_type, CritterFindType find_type) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        FO_RUNTIME_ASSERT(_globalMapGroup);
        auto critters = copy(*_globalMapGroup);
        vec_remove_unique_value(critters, this);
        return vec_transform(critters, [](auto&& cr) -> Critter* { return cr.get(); });
    }

    if (see_type == CritterSeeType::WhoSeeMe && find_type == CritterFindType::Any) {
        return vec_transform(_visibleCrWhoSeeMe, [](auto&& cr) -> Critter* { return cr.get(); });
    }
    if (see_type == CritterSeeType::WhoISee && find_type == CritterFindType::Any) {
        return vec_transform(_visibleCr, [](auto&& cr) -> Critter* { return cr.get(); });
    }

    vector<Critter*> critters;

    if (see_type == CritterSeeType::Any) {
        critters.reserve(_visibleCrWhoSeeMe.size() + _visibleCr.size());
    }
    else if (see_type == CritterSeeType::WhoSeeMe) {
        critters.reserve(_visibleCrWhoSeeMe.size());
    }
    else if (see_type == CritterSeeType::WhoISee) {
        critters.reserve(_visibleCr.size());
    }

    if (see_type == CritterSeeType::WhoSeeMe || see_type == CritterSeeType::Any) {
        for (auto& cr : _visibleCrWhoSeeMe) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr.get());
            }
        }
    }
    if (see_type == CritterSeeType::WhoISee || see_type == CritterSeeType::Any) {
        for (auto& cr : _visibleCr) {
            if (cr->CheckFind(find_type)) {
                if (see_type == CritterSeeType::Any && std::ranges::find(critters, cr) != critters.end()) {
                    continue;
                }

                critters.emplace_back(cr.get());
            }
        }
    }

    return critters;
}

auto Critter::GetGlobalMapGroup() -> span<raw_ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!GetMapId());
    FO_RUNTIME_ASSERT(_globalMapGroup);

    return *_globalMapGroup;
}

auto Critter::AddVisibleCritter(Critter* cr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(GetMapId());
    FO_RUNTIME_ASSERT(cr != this);
    FO_RUNTIME_ASSERT(cr->GetId() != GetId());
    FO_RUNTIME_ASSERT(cr->GetMapId() == GetMapId());

    const auto inserted = _visibleCrWhoSeeMeMap.emplace(cr->GetId(), cr).second;

    if (!inserted) {
        return false;
    }

    const auto inserted2 = cr->_visibleCrMap.emplace(GetId(), this).second;
    FO_RUNTIME_ASSERT(inserted2);

    _visibleCrWhoSeeMe.emplace_back(cr);
    cr->_visibleCr.emplace_back(this);

    return true;
}

auto Critter::RemoveVisibleCritter(Critter* cr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(GetMapId());
    FO_RUNTIME_ASSERT(cr != this);
    FO_RUNTIME_ASSERT(cr->GetId() != GetId());
    FO_RUNTIME_ASSERT(cr->GetMapId() == GetMapId());

    const auto it_map = _visibleCrWhoSeeMeMap.find(cr->GetId());

    if (it_map == _visibleCrWhoSeeMeMap.end()) {
        return false;
    }

    _visibleCrWhoSeeMeMap.erase(it_map);

    const auto it_map2 = cr->_visibleCrMap.find(GetId());
    FO_RUNTIME_ASSERT(it_map2 != cr->_visibleCrMap.end());
    cr->_visibleCrMap.erase(it_map2);

    const auto it = std::ranges::find(_visibleCrWhoSeeMe, cr);
    FO_RUNTIME_ASSERT(it != _visibleCrWhoSeeMe.end());
    _visibleCrWhoSeeMe.erase(it);

    const auto it2 = std::ranges::find(cr->_visibleCr, this);
    FO_RUNTIME_ASSERT(it2 != cr->_visibleCr.end());
    cr->_visibleCr.erase(it2);

    return true;
}

auto Critter::AddCrIntoVisGroup1(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleCrGroup1.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisGroup2(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleCrGroup2.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisGroup3(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleCrGroup3.emplace(cr_id).second;
}

auto Critter::RemoveCrFromVisGroup1(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleCrGroup1.erase(cr_id) != 0;
}

auto Critter::RemoveCrFromVisGroup2(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleCrGroup2.erase(cr_id) != 0;
}

auto Critter::RemoveCrFromVisGroup3(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleCrGroup3.erase(cr_id) != 0;
}

auto Critter::AddVisibleItem(ident_t item_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleItems.emplace(item_id).second;
}

auto Critter::RemoveVisibleItem(ident_t item_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleItems.erase(item_id) != 0;
}

auto Critter::CheckVisibleItem(ident_t item_id) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _visibleItems.count(item_id) != 0;
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

    for (auto& item : _invItems) {
        if (item->GetId() == item_id) {
            return item.get();
        }
    }

    return nullptr;
}

auto Critter::GetInvItemByPid(hstring item_pid) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item.get();
        }
    }

    return nullptr;
}

auto Critter::GetInvItemBySlot(CritterItemSlot slot) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto it = std::ranges::find_if(_invItems, [&](auto&& item) noexcept -> bool { return item->GetCritterSlot() == slot; });

    if (it == _invItems.end()) {
        return nullptr;
    }

    return it->get();
}

auto Critter::CountInvItemByPid(hstring pid) const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    int32 count = 0;

    for (const auto& item : _invItems) {
        if (item->GetProtoId() == pid) {
            count += item->GetCount();
        }
    }

    return count;
}

void Critter::Broadcast_Property(NetProperty type, const Property* prop, const ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Action(CritterAction action, int32 action_data, const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Action(this, action, action_data, item);
    }
}

void Critter::Broadcast_Dir()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Dir(this);
    }
}

void Critter::Broadcast_Teleport(mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Teleport(this, to_hex);
    }
}

void Critter::SendAndBroadcast(const Player* ignore_player, const function<void(Critter*)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    if (ignore_player == nullptr || ignore_player != GetPlayer()) {
        callback(this);
    }

    for (auto& cr : _visibleCrWhoSeeMe) {
        if (ignore_player == nullptr || ignore_player != cr->GetPlayer()) {
            callback(cr.get());
        }
    }
}

void Critter::SendAndBroadcast_Moving()
{
    FO_STACK_TRACE_ENTRY();

    Send_Moving(this);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Moving(this);
    }
}

void Critter::SendAndBroadcast_Action(CritterAction action, int32 action_data, const Item* context_item)
{
    FO_STACK_TRACE_ENTRY();

    Send_Action(this, action, action_data, context_item);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Action(this, action, action_data, context_item);
    }
}

void Critter::SendAndBroadcast_MoveItem(const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    Send_MoveItem(this, item, action, prev_slot);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_MoveItem(this, item, action, prev_slot);
    }
}

void Critter::SendAndBroadcast_Attachments()
{
    FO_STACK_TRACE_ENTRY();

    Send_Attachments(this);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Attachments(this);
    }
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
