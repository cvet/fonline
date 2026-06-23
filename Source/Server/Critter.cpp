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

#include "Critter.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

extern auto CheckItemVisibilityHook(const ServerEngine*, const Map*, const Critter*, const Item*) -> bool;

Critter::Critter(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME).as_ptr(), props ? props : nptr<const Properties> {&proto->GetProperties()}, &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(*GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    SetEntityLock(&_ownedLock);
}

Critter::~Critter()
{
    FO_STACK_TRACE_ENTRY();

    if (!_engine->IsShutdownInProgress()) {
        FO_VERIFY_AND_CONTINUE(!_player, "Server critter still has player during destruction", GetId(), _player ? _player->GetId() : ident_t {});
        FO_VERIFY_AND_CONTINUE(!GetMapId(), "Server critter still has map during destruction", GetId(), GetMapId());
        FO_VERIFY_AND_CONTINUE(!GetIsAttached(), "Server critter is still attached during destruction", GetId(), GetAttachMaster());
        FO_VERIFY_AND_CONTINUE(_invItems.empty(), "Server critter has inventory items during destruction", GetId(), _invItems.size());
        FO_VERIFY_AND_CONTINUE(_attachedCritters.empty(), "Server critter has attached critters during destruction", GetId(), _attachedCritters.size());
        FO_VERIFY_AND_CONTINUE(!_globalMapGroup, "Server critter still has global map group during destruction", GetId());
        FO_VERIFY_AND_CONTINUE(_visibleCrWhoSeeMe.empty(), "Server critter has reverse visible critters during destruction", GetId(), _visibleCrWhoSeeMe.size());
        FO_VERIFY_AND_CONTINUE(_visibleCr.empty(), "Server critter has visible critters during destruction", GetId(), _visibleCr.size());
        FO_VERIFY_AND_CONTINUE(_visibleCrWhoSeeMeMap.empty(), "Server critter has reverse visible critter map entries during destruction", GetId(), _visibleCrWhoSeeMeMap.size());
        FO_VERIFY_AND_CONTINUE(_visibleCrMap.empty(), "Server critter has visible critter map entries during destruction", GetId(), _visibleCrMap.size());
        FO_VERIFY_AND_CONTINUE(_visibleCrModes.empty(), "Server critter has visible critter modes during destruction", GetId(), _visibleCrModes.size());
        FO_VERIFY_AND_CONTINUE(_visibleCrGroup1.empty(), "Server critter has visible critter group1 entries during destruction", GetId(), _visibleCrGroup1.size());
        FO_VERIFY_AND_CONTINUE(_visibleCrGroup2.empty(), "Server critter has visible critter group2 entries during destruction", GetId(), _visibleCrGroup2.size());
        FO_VERIFY_AND_CONTINUE(_visibleCrGroup3.empty(), "Server critter has visible critter group3 entries during destruction", GetId(), _visibleCrGroup3.size());
        FO_VERIFY_AND_CONTINUE(_visibleItems.empty(), "Server critter has visible items during destruction", GetId(), _visibleItems.size());
        FO_VERIFY_AND_CONTINUE(_lockMapTransfers == 0, "Server critter has locked map transfers during destruction", GetId(), _lockMapTransfers);
    }
}

auto Critter::GetName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        return player->GetName();
    }

    return _proto->GetName();
}

auto Critter::GetSyncWidenEntity() noexcept -> nptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _player.as_nptr();
}

auto Critter::GetSyncWidenEntity() const noexcept -> nptr<const ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _player.as_nptr();
}

auto Critter::GetOfflineTime() const -> timespan
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetControlledByPlayer() && !_player ? _engine->GameTime.GetFrameTime() - _playerDetachTime : timespan::zero;
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

    FO_VERIFY_AND_THROW(!GetControlledByPlayer(), "Controlled by player is already set");

    SetControlledByPlayer(true);
    _playerDetachTime = _engine->GameTime.GetFrameTime();

    if (GetMapId()) {
        auto map_holder = require_refcount_ptr(GetParent<Map>());
        auto map = map_holder.as_ptr();
        ptr<Critter> self = this;
        map->RefreshCritterPlayerState(self);
    }
}

void Critter::UnmarkIsForPlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    FO_VERIFY_AND_THROW(!_player, "Player is already set");

    SetControlledByPlayer(false);

    if (GetMapId()) {
        auto map_holder = require_refcount_ptr(GetParent<Map>());
        auto map = map_holder.as_ptr();
        ptr<Critter> self = this;
        map->RefreshCritterPlayerState(self);
    }
}

void Critter::AttachPlayer(ptr<Player> player)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    FO_VERIFY_AND_THROW(!player->GetControlledCritterId(), "Player already controls a critter");
    FO_VERIFY_AND_THROW(!_player, "Player is already set");
    FO_VERIFY_AND_THROW(!player->GetViewMap(), "Player still has an active view map");

    _player = player.hold_ref();

    player->SetControlledCritterId(GetId());
    player->SetLastControlledCritterId(GetId());
    ptr<Critter> self = this;
    player->SetControlledCritter(self);
}

void Critter::DetachPlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    FO_VERIFY_AND_THROW(_player, "Missing required player");
    auto player = _player.as_ptr();
    FO_VERIFY_AND_THROW(player->GetControlledCritterId() == GetId(), "Player controlled critter id does not match this critter");

    player->SetControlledCritterId({});
    player->SetControlledCritter(nullptr);

    _player.reset();
    _playerDetachTime = _engine->GameTime.GetFrameTime();
}

void Critter::SetMoving(refcount_ptr<MovingContext> moving)
{
    FO_STACK_TRACE_ENTRY();

    if (_moving) {
        auto current_moving = _moving.as_ptr();
        current_moving->Complete(MovingState::Stopped);
        _lastMoving = _moving;
    }

    auto moving_ptr = moving.as_ptr();
    _moving = std::move(moving);
    _movingUid++;
    SetMovingSpeed(numeric_cast<int32_t>(moving_ptr->GetSpeed()));
}

void Critter::StopMoving(MovingState reason)
{
    FO_STACK_TRACE_ENTRY();

    if (!_moving) {
        return;
    }

    auto moving = _moving.as_ptr();
    moving->Complete(reason);
    _lastMoving = _moving;
    _moving.reset();
    _movingUid++;
    SetMovingSpeed(0);
}

void Critter::AddAttachedCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    vec_add_unique_value(_attachedCritters, cr);
}

void Critter::RemoveAttachedCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    vec_remove_unique_value(_attachedCritters, cr);
}

void Critter::AttachToCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Cannot attach an already destroyed critter", GetId());
    FO_VERIFY_AND_THROW(!IsDestroying(), "Cannot attach a critter that is being destroyed", GetId());
    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Cannot attach to an already destroyed critter", cr->GetId());
    FO_VERIFY_AND_THROW(!cr->IsDestroying(), "Cannot attach to a critter that is being destroyed", cr->GetId());
    FO_VERIFY_AND_THROW(cr != this, "Critter visibility cannot target itself");
    FO_VERIFY_AND_THROW(cr->GetMapId() == GetMapId(), "Critter belongs to a different map");
    FO_VERIFY_AND_THROW(!cr->GetIsAttached(), "Critter is already attached");
    FO_VERIFY_AND_THROW(!GetIsAttached(), "Is attached is already set");
    FO_VERIFY_AND_THROW(!HasAttachedCritters(), "Has attached critters is already set");

    if (IsMoving()) {
        StopMoving(MovingState::Stopped);
    }

    ptr<Critter> self = this;
    cr->AddAttachedCritter(self);
    SetIsAttached(true);
    SetAttachMaster(cr->GetId());

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::DetachFromCritter()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(GetIsAttached(), "Critter is not attached");
    FO_VERIFY_AND_THROW(GetAttachMaster(), "Missing required attach master");

    auto nullable_cr = _engine->EntityMngr.GetCritter(GetAttachMaster());
    auto cr_holder = std::move(nullable_cr).take_not_null();
    auto cr = cr_holder.as_ptr();

    ptr<Critter> self = this;
    cr->RemoveAttachedCritter(self);
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
    auto map_holder = require_refcount_ptr(GetParent<Map>());
    auto map = map_holder.as_ptr();

    vector<pair<refcount_ptr<Critter>, mpos>> moved_critters;

    const auto new_hex = GetHex();
    const auto new_hex_offset = GetHexOffset();

    auto attached = GetAttachedCritters();

    for (ptr<Critter> cr : attached) {
        FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
        FO_VERIFY_AND_THROW(cr->GetIsAttached(), "Critter is not attached");
        FO_VERIFY_AND_THROW(cr->GetAttachMaster() == GetId(), "Attached critter has a different master");
        FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");

        cr->SetHexOffset(new_hex_offset);

        const auto hex = cr->GetHex();

        if (hex != new_hex) {
            map->RemoveCritterFromField(cr);
            cr->SetHex(new_hex);
            map->AddCritterToField(cr);

            moved_critters.emplace_back(cr.hold_ref(), hex);
        }
    }

    // Callbacks time
    ptr<Critter> self = this;
    auto this_ref_holder = self.hold_ref();
    auto map_ref_holder = map.as_ptr().hold_ref();
    const auto dir = GetDir();

    for (auto& [cr_ref_holder, prev_hex] : moved_critters) {
        auto cr = cr_ref_holder.as_ptr();

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

        map->VerifyTrigger(cr, prev_hex, new_hex, dir);

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

    FO_VERIFY_AND_THROW(GetMapId(), "Entity has no map id");

    while (!_visibleCrWhoSeeMe.empty()) {
        ptr<Critter> cr = _visibleCrWhoSeeMe.front();
        const auto del_ok = RemoveVisibleCritter(cr);
        FO_STRONG_ASSERT(del_ok, "Failed to remove visible critter");
        ptr<const Critter> self = this;
        cr->Send_RemoveCritter(self);
    }
    while (!_visibleCr.empty()) {
        ptr<Critter> cr = _visibleCr.front();
        ptr<Critter> self = this;
        const auto del_ok2 = cr->RemoveVisibleCritter(self);
        FO_STRONG_ASSERT(del_ok2, "Failed to remove self from visible critter");
    }

    _visibleCrGroup1.clear();
    _visibleCrGroup2.clear();
    _visibleCrGroup3.clear();

    _visibleItems.clear();

    FO_STRONG_ASSERT(_visibleCrWhoSeeMe.empty(), "Visible cr who see me must be empty before this operation");
    FO_STRONG_ASSERT(_visibleCrWhoSeeMeMap.empty(), "Visible cr who see me map must be empty before this operation");
    FO_STRONG_ASSERT(_visibleCr.empty(), "Visible cr must be empty before this operation");
    FO_STRONG_ASSERT(_visibleCrMap.empty(), "Visible cr map must be empty before this operation");
    FO_STRONG_ASSERT(_visibleCrModes.empty(), "Visible cr modes must be empty before this operation");
    FO_STRONG_ASSERT(_visibleCrGroup1.empty(), "Visible cr group1 must be empty before this operation");
    FO_STRONG_ASSERT(_visibleCrGroup2.empty(), "Visible cr group2 must be empty before this operation");
    FO_STRONG_ASSERT(_visibleCrGroup3.empty(), "Visible cr group3 must be empty before this operation");
    FO_STRONG_ASSERT(_visibleItems.empty(), "Visible items must be empty before this operation");
}

auto Critter::IsSeeCritter(ident_t cr_id) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
        const auto it = std::ranges::find_if(*_globalMapGroup, [&cr_id](ptr<Critter> other) { return other->GetId() == cr_id; });
        return it != _globalMapGroup->end() && cr_id != GetId();
    }

    if (_visibleCrMap.count(cr_id) != 0) {
        return true;
    }

    return false;
}

auto Critter::GetCritter(ident_t cr_id, CritterSeeType see_type) -> nptr<Critter>
{
    FO_STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
        const auto it = std::ranges::find_if(*_globalMapGroup, [&cr_id](ptr<Critter> other) { return other->GetId() == cr_id; });
        if (it != _globalMapGroup->end() && cr_id != GetId()) {
            return it->as_nptr();
        }

        return nullptr;
    }

    if (see_type == CritterSeeType::WhoSeeMe || see_type == CritterSeeType::Any) {
        const auto it = _visibleCrWhoSeeMeMap.find(cr_id);

        if (it != _visibleCrWhoSeeMeMap.end()) {
            return it->second.as_nptr();
        }
    }

    if (see_type == CritterSeeType::WhoISee || see_type == CritterSeeType::Any) {
        const auto it = _visibleCrMap.find(cr_id);

        if (it != _visibleCrMap.end()) {
            return it->second.as_nptr();
        }
    }

    return nullptr;
}

auto Critter::GetCritters(CritterSeeType see_type, CritterFindType find_type) -> vector<ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
        vector<ptr<Critter>> critters = copy(*_globalMapGroup);
        ptr<Critter> self = this;
        vec_remove_unique_value(critters, self);
        return critters;
    }

    if (see_type == CritterSeeType::WhoSeeMe && find_type == CritterFindType::Any) {
        return _visibleCrWhoSeeMe;
    }
    if (see_type == CritterSeeType::WhoISee && find_type == CritterFindType::Any) {
        return _visibleCr;
    }

    vector<ptr<Critter>> critters;

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
        for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr);
            }
        }
    }
    if (see_type == CritterSeeType::WhoISee || see_type == CritterSeeType::Any) {
        for (ptr<Critter> cr : _visibleCr) {
            if (cr->CheckFind(find_type)) {
                if (see_type == CritterSeeType::Any && std::ranges::find(critters, cr) != critters.end()) {
                    continue;
                }

                critters.emplace_back(cr);
            }
        }
    }

    return critters;
}

auto Critter::HasGlobalMapGroup() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !!_globalMapGroup;
}

auto Critter::IsSameGlobalMapGroup(ptr<const Critter> other) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _globalMapGroup && other->_globalMapGroup && _globalMapGroup == other->_globalMapGroup;
}

auto Critter::GetGlobalMapGroup() -> span<ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!GetMapId(), "Map id is already set");
    FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");

    return *_globalMapGroup;
}

void Critter::CreateGlobalMapGroup()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!GetMapId(), "Critter is not on the global map");
    FO_VERIFY_AND_THROW(!_globalMapGroup, "Critter already has a global map group");

    _globalMapGroup = SafeAlloc::MakeShared<vector<ptr<Critter>>>();
}

void Critter::JoinGlobalMapGroup(ptr<Critter> group_owner)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!GetMapId(), "Critter is not on the global map");
    FO_VERIFY_AND_THROW(!group_owner->GetMapId(), "Group owner is not on the global map");
    FO_VERIFY_AND_THROW(!_globalMapGroup, "Critter already has a global map group");
    FO_VERIFY_AND_THROW(group_owner->_globalMapGroup, "Group owner has no global map group");

    _globalMapGroup = group_owner->_globalMapGroup;
}

void Critter::AddToGlobalMapGroup(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!GetMapId(), "Critter is not on the global map");
    FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
    vec_add_unique_value(*_globalMapGroup, cr);
}

void Critter::RemoveFromGlobalMapGroup(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
    vec_remove_unique_value(*_globalMapGroup, cr);
}

void Critter::ResetGlobalMapGroup() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _globalMapGroup.reset();
}

auto Critter::GetVisibleCritterMode(ident_t cr_id) const noexcept -> CritterVisibilityMode
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _visibleCrModes.find(cr_id);
    return it != _visibleCrModes.end() ? it->second : CritterVisibilityMode::None;
}

void Critter::SetVisibleCritterMode(ident_t cr_id, CritterVisibilityMode mode) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (mode == CritterVisibilityMode::None) {
        _visibleCrModes.erase(cr_id);
    }
    else {
        _visibleCrModes[cr_id] = mode;
    }
}

auto Critter::AddVisibleCritter(ptr<Critter> cr, CritterVisibilityMode mode) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(GetMapId(), "Entity has no map id");
    FO_VERIFY_AND_THROW(cr != this, "Critter visibility cannot target itself");
    FO_VERIFY_AND_THROW(cr->GetId() != GetId(), "Critter visibility target has the same id as source");
    FO_VERIFY_AND_THROW(cr->GetMapId() == GetMapId(), "Critter belongs to a different map");
    FO_VERIFY_AND_THROW(mode != CritterVisibilityMode::None, "Critter visibility mode is not set");

    ValidateEntityAccess(cr);

    const auto inserted = _visibleCrWhoSeeMeMap.emplace(cr->GetId(), cr).second;

    if (!inserted) {
        return false;
    }

    ptr<Critter> self = this;
    const auto inserted2 = cr->_visibleCrMap.emplace(GetId(), self).second;
    FO_STRONG_ASSERT(inserted2, "Visible critter graph asymmetry on add", GetId(), cr->GetId());

    cr->_visibleCrModes[GetId()] = mode;
    _visibleCrWhoSeeMe.emplace_back(cr);
    cr->_visibleCr.emplace_back(self);

    return true;
}

auto Critter::RemoveVisibleCritter(ptr<Critter> cr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(GetMapId(), "Entity has no map id");
    FO_VERIFY_AND_THROW(cr != this, "Critter visibility cannot target itself");
    FO_VERIFY_AND_THROW(cr->GetId() != GetId(), "Critter visibility target has the same id as source");
    FO_VERIFY_AND_THROW(cr->GetMapId() == GetMapId(), "Critter belongs to a different map");

    ValidateEntityAccess(cr);

    const auto it_map = _visibleCrWhoSeeMeMap.find(cr->GetId());

    if (it_map == _visibleCrWhoSeeMeMap.end()) {
        return false;
    }

    _visibleCrWhoSeeMeMap.erase(it_map);

    const auto it_map2 = cr->_visibleCrMap.find(GetId());
    FO_STRONG_ASSERT(it_map2 != cr->_visibleCrMap.end(), "Lookup failed in critter visible cr map");
    cr->_visibleCrMap.erase(it_map2);

    cr->_visibleCrModes.erase(GetId());

    const auto it = std::ranges::find(_visibleCrWhoSeeMe, cr);
    FO_STRONG_ASSERT(it != _visibleCrWhoSeeMe.end(), "Lookup failed in visible cr who see me");
    _visibleCrWhoSeeMe.erase(it);

    ptr<Critter> self = this;
    const auto it2 = std::ranges::find(cr->_visibleCr, self);
    FO_STRONG_ASSERT(it2 != cr->_visibleCr.end(), "Lookup failed in critter visible cr");
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

auto Critter::CanSeeItemOnMap(ptr<const Item> item) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (IsDestroyed() || item->IsDestroyed()) {
        return false;
    }
    if (item->GetOwnership() != ItemOwnership::MapHex) {
        return false;
    }
    if (!GetMapId() || item->GetMapId() != GetMapId()) {
        return false;
    }

    auto map_holder = require_refcount_ptr(GetParent<Map>());
    auto map = map_holder.as_ptr();

    return CheckItemVisibilityHook(_engine.get(), map.get(), this, item.get());
}

void Critter::ChangeDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    SetDir(dir);
}

void Critter::SetItem(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    vec_add_unique_value(_invItems, item);
    ptr<Critter> cr = this;
    item->SetParent(cr);
}

void Critter::RemoveItem(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    vec_remove_unique_value(_invItems, item);
    item->SetParent(nullptr);
}

auto Critter::GetInvItem(ident_t item_id) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (ptr<Item> item : _invItems) {
        if (item->GetId() == item_id) {
            return item.as_nptr();
        }
    }

    return nullptr;
}

auto Critter::GetInvItems() noexcept -> vector<ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return _invItems;
}

auto Critter::GetInvItems() const noexcept -> vector<ptr<const Item>>
{
    FO_STACK_TRACE_ENTRY();

    vector<ptr<const Item>> result;
    result.reserve(_invItems.size());

    for (ptr<const Item> item : _invItems) {
        result.emplace_back(item);
    }

    return result;
}

auto Critter::HasItems() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_invItems.empty();
}

auto Critter::GetInvItemByPid(hstring item_pid) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (ptr<Item> item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item.as_nptr();
        }
    }

    return nullptr;
}

auto Critter::GetInvItemBySlot(CritterItemSlot slot) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto it = std::ranges::find_if(_invItems, [&](ptr<Item> item) noexcept -> bool { return item->GetCritterSlot() == slot; });

    if (it == _invItems.end()) {
        return nullptr;
    }

    return it->as_nptr();
}

auto Critter::CountInvItemByPid(hstring pid) const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    int32_t count = 0;

    for (ptr<const Item> item : _invItems) {
        if (item->GetProtoId() == pid) {
            count += item->GetCount();
        }
    }

    return count;
}

auto Critter::GetMapSpectators() -> ref_hold_vector<ptr<Player>>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto nullable_map = GetParent<Map>();

    if (nullable_map) {
        auto map = nullable_map.as_ptr();

        if (map->HasSpectatorPlayers()) {
            return copy_hold_ref(map->GetSpectatorPlayers());
        }
    }

    return ref_hold_vector<ptr<Player>>(0);
}

void Critter::Broadcast_Property(NetProperty type, ptr<const Property> prop, ptr<const ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_Property(type, prop, entity);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Action(CritterAction action, int32_t action_data, nptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ptr<const Critter> self = this;

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_Action(self, action, action_data, item);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_Action(self, action, action_data, item);
    }
}

void Critter::Broadcast_Dir()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ptr<const Critter> self = this;

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_Dir(self);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_Dir(self);
    }
}

void Critter::Broadcast_Teleport(mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ptr<const Critter> self = this;

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_Teleport(self, to_hex);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_Teleport(self, to_hex);
    }
}

void Critter::SendAndBroadcast(nptr<const Player> ignore_player, const function<void(ptr<Critter>)>& cr_callback, const function<void(ptr<Player>)>& spectator_callback)
{
    FO_STACK_TRACE_ENTRY();

    auto self_player = GetPlayer();

    if (!ignore_player || !(ignore_player == self_player)) {
        ptr<Critter> self = this;
        cr_callback(self);
    }

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        auto cr_player = cr->GetPlayer();

        if (!ignore_player || !(ignore_player == cr_player)) {
            cr_callback(cr);
        }
    }

    if (spectator_callback) {
        for (ptr<Player> player : GetMapSpectators()) {
            nptr<const Player> spectator = player;

            if (!(spectator == ignore_player)) {
                spectator_callback(player);
            }
        }
    }
}

void Critter::SendAndBroadcast_Moving()
{
    FO_STACK_TRACE_ENTRY();

    ptr<const Critter> self = this;

    Send_Moving(self);

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_Moving(self);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_Moving(self);
    }
}

void Critter::SendAndBroadcast_Action(CritterAction action, int32_t action_data, nptr<const Item> context_item)
{
    FO_STACK_TRACE_ENTRY();

    ptr<const Critter> self = this;

    Send_Action(self, action, action_data, context_item);

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_Action(self, action, action_data, context_item);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_Action(self, action, action_data, context_item);
    }
}

void Critter::SendAndBroadcast_MoveItem(nptr<const Item> item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    ptr<const Critter> self = this;

    Send_MoveItem(self, item, action, prev_slot);

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_MoveItem(self, item, action, prev_slot);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_MoveItem(self, item, action, prev_slot);
    }
}

void Critter::SendAndBroadcast_Attachments()
{
    FO_STACK_TRACE_ENTRY();

    ptr<const Critter> self = this;

    Send_Attachments(self);

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        cr->Send_Attachments(self);
    }

    for (ptr<Player> player : GetMapSpectators()) {
        player->Send_Attachments(self);
    }
}

void Critter::Send_Property(NetProperty type, ptr<const Property> prop, ptr<const ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_Property(type, prop, entity);
    }
}

void Critter::Send_Moving(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_Moving(from_cr);
    }
}

void Critter::Send_MovingSpeed(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_MovingSpeed(from_cr);
    }
}

void Critter::Send_Dir(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_Dir(from_cr);
    }
}

void Critter::Send_AddCritter(ptr<const Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_AddCritter(cr);
    }
}

void Critter::Send_RemoveCritter(ptr<const Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_RemoveCritter(cr);
    }
}

void Critter::Send_CritterVisibilityMode(ptr<const Critter> cr, CritterVisibilityMode mode)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_CritterVisibilityMode(cr, mode);
    }
}

void Critter::Send_LoadMap(nptr<const Map> map)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_LoadMap(map);
    }
}

void Critter::Send_AddItemOnMap(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_AddItemOnMap(item);
    }
}

void Critter::Send_RemoveItemFromMap(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_RemoveItemFromMap(item);
    }
}

void Critter::Send_ChosenAddItem(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_ChosenAddItem(item);
    }
}

void Critter::Send_ChosenRemoveItem(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_ChosenRemoveItem(item);
    }
}

void Critter::Send_Teleport(ptr<const Critter> cr, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_Teleport(cr, to_hex);
    }
}

void Critter::Send_TimeSync()
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_TimeSync();
    }
}

void Critter::Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_InfoMessage(info_message, extra_text);
    }
}

void Critter::Send_Action(ptr<const Critter> from_cr, CritterAction action, int32_t action_data, nptr<const Item> context_item)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_Action(from_cr, action, action_data, context_item);
    }
}

void Critter::Send_MoveItem(ptr<const Critter> from_cr, nptr<const Item> item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_MoveItem(from_cr, item, action, prev_slot);
    }
}

void Critter::Send_PlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_PlaceToGameComplete();
    }
}

void Critter::Send_SomeItems(const_span<ptr<const Item>> items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_SomeItems(items, owned, with_inner_entities, context_param);
    }
}

void Critter::Send_Attachments(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    if (_player) {
        auto player = _player.as_ptr();
        player->Send_Attachments(from_cr);
    }
}

FO_END_NAMESPACE
