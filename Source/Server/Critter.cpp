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

extern auto CheckItemVisibilityHook(ptr<const ServerEngine>, ptr<const Map>, ptr<const Critter>, ptr<const Item>) -> bool;

Critter::Critter(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME).as_ptr(), props ? props : nptr<const Properties> {proto->GetProperties()}, proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(*GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    SetEntityLock(&_ownedLock);
}

Critter::~Critter()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (!_engine->IsShutdownInProgress()) {
        FO_VERIFY_AND_CONTINUE(!_player.load(std::memory_order_relaxed), "Server critter still has player during destruction", GetId());
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

auto Critter::GetRawGlobalMapGroup() -> shared_ptr<vector<ptr<Critter>>>&
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _globalMapGroup;
}

void Critter::LockMapTransfers() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    _lockMapTransfers++;
}

void Critter::UnlockMapTransfers() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // NOT NOT_DESTROYED: MapManager::RemoveCritterFromMap pairs LockMapTransfers() with a
    // scope_exit{ UnlockMapTransfers() } around the OnMapCritterOut / OnCritterDisappeared events, which may
    // destroy this critter. The scope_exit must still balance the counter on an already-destroyed (but still
    // ref-held) critter — the destructor asserts _lockMapTransfers == 0 — and decrementing a plain counter
    // on a destroyed-but-allocated object is safe.
    FO_VALIDATE_ENTITY(LOCKED);
    _lockMapTransfers--;
}

auto Critter::HasPlayer() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _player.load(std::memory_order_acquire) != nullptr;
}

auto Critter::GetPlayer() const noexcept -> nptr<const Player>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<const Player>(_player.load(std::memory_order_acquire));
}

auto Critter::GetPlayer() noexcept -> nptr<Player>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<Player>(_player.load(std::memory_order_acquire));
}

auto Critter::GetVisibleItems() const noexcept -> const unordered_set<ident_t>&
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleItems;
}

auto Critter::IsSeeItem(ident_t item_id) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleItems.contains(item_id);
}

auto Critter::IsMoving() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _moving != nullptr;
}

auto Critter::GetMovingUid() const noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _movingUid;
}

auto Critter::GetMoving() const noexcept -> nptr<const MovingContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _moving;
}

auto Critter::GetMoving() noexcept -> nptr<MovingContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _moving;
}

auto Critter::GetMovingContext() const noexcept -> nptr<const MovingContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _moving ? nptr<const MovingContext> {_moving} : nptr<const MovingContext> {_lastMoving};
}

auto Critter::GetMovingContext() noexcept -> nptr<MovingContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _moving ? nptr<MovingContext> {_moving} : nptr<MovingContext> {_lastMoving};
}

auto Critter::GetMovingState() const noexcept -> MovingState
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _moving ? MovingState::InProgress : (_lastMoving ? _lastMoving->GetCompleteReason() : MovingState::Success);
}

auto Critter::IsMapTransfersLocked() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _lockMapTransfers != 0;
}

auto Critter::HasAttachedCritters() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return !_attachedCritters.empty();
}

auto Critter::GetAttachedCritters() noexcept -> span<ptr<Critter>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _attachedCritters;
}

auto Critter::GetAttachedCritters() const noexcept -> const_span<ptr<Critter>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _attachedCritters;
}

auto Critter::GetName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (nptr<const Player> player = _player.load(std::memory_order_acquire)) {
        return player->GetName();
    }

    return _proto->GetName();
}

auto Critter::GetSyncWidenEntity() noexcept -> nptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<ServerEntity>(_player.load(std::memory_order_acquire));
}

auto Critter::GetSyncWidenEntity() const noexcept -> nptr<const ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<const ServerEntity>(_player.load(std::memory_order_acquire));
}

auto Critter::GetPlayerForSend() const noexcept -> refcount_nptr<Player>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<Player>(_player.load(std::memory_order_acquire)).try_hold_ref();
}

auto Critter::GetOfflineTime() const -> timespan
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    return GetControlledByPlayer() && _player.load(std::memory_order_acquire) == nullptr ? _engine->GameTime.GetFrameTime() - _playerDetachTime : timespan::zero;
}

auto Critter::IsAlive() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return GetCondition() == CritterCondition::Alive;
}

auto Critter::IsDead() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return GetCondition() == CritterCondition::Dead;
}

auto Critter::IsKnockout() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return GetCondition() == CritterCondition::Knockout;
}

auto Critter::CheckFind(CritterFindType find_type) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

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

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VERIFY_AND_THROW(!GetControlledByPlayer(), "Controlled by player is already set");

    refcount_nptr<Map> map;

    if (GetMapId()) {
        map = GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map);
    }

    SetControlledByPlayer(true);
    _playerDetachTime = _engine->GameTime.GetFrameTime();

    if (map) {
        map->RefreshCritterPlayerState(this);
    }
}

void Critter::UnmarkIsForPlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    FO_VERIFY_AND_THROW(!_player.load(std::memory_order_acquire), "Player is already set");

    refcount_nptr<Map> map;

    if (GetMapId()) {
        map = GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map);
    }

    SetControlledByPlayer(false);

    if (map) {
        map->RefreshCritterPlayerState(this);
    }
}

void Critter::AttachPlayer(ptr<Player> player)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    ValidateEntityAccess(player);
    FO_VERIFY_AND_THROW(!player->GetControlledCritterId(), "Player already controls a critter");
    FO_VERIFY_AND_THROW(!_player.load(std::memory_order_acquire), "Player is already set");
    FO_VERIFY_AND_THROW(!player->GetViewMap(), "Player still has an active view map");

    // Owning publication (mirrors ServerEntity::SetParent): AddRef the new owner, then publish atomically so a
    // concurrent lock-free GetPlayerForSend reader sees either the old or the new pointer, never a torn value.
    player->AddRef();
    _player.store(player.get(), std::memory_order_release);

    player->SetControlledCritterId(GetId());
    player->SetLastControlledCritterId(GetId());
    player->SetControlledCritter(this);
}

void Critter::DetachPlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    nptr<Player> player = _player.load(std::memory_order_acquire);
    FO_VERIFY_AND_THROW(player, "Missing required player");
    ValidateEntityAccess(player);
    FO_VERIFY_AND_THROW(player->GetControlledCritterId() == GetId(), "Player controlled critter id does not match this critter");

    player->SetControlledCritterId({});
    player->SetControlledCritter(nullptr);

    _player.store(nullptr, std::memory_order_release);
    player->Release();
    _playerDetachTime = _engine->GameTime.GetFrameTime();
}

void Critter::SetMoving(refcount_ptr<MovingContext> moving)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

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

    // NOT NOT_DESTROYING: MapManager::Transfer/RemoveCritterFromMap stop a critter's movement while it is being
    // destroyed (TransferToGlobal of a destroying critter is part of teardown), so this cleanup is legitimately
    // reached during IsDestroying.
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

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

    // Adds a child into the holder's owned _attachedCritters list (torn down during destruction), so the
    // holder must not be mid-destruction. The only caller (AttachToCritter) already guards both ends.
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    vec_add_unique_value(_attachedCritters, cr);
}

void Critter::RemoveAttachedCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    vec_remove_unique_value(_attachedCritters, cr);
}

void Critter::AttachToCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
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

    cr->AddAttachedCritter(this);
    SetIsAttached(true);
    SetAttachMaster(cr->GetId());

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::DetachFromCritter()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(GetIsAttached(), "Missing required is attached");
    FO_VERIFY_AND_THROW(GetAttachMaster(), "Missing required attach master");

    auto nullable_cr = _engine->EntityMngr.GetCritter(GetAttachMaster());
    auto cr = std::move(nullable_cr).take_not_null();

    cr->RemoveAttachedCritter(this);
    SetIsAttached(false);
    SetAttachMaster({});

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::MoveAttachedCritters()
{
    FO_STACK_TRACE_ENTRY();

    // NOT NOT_DESTROYING: reached from MapManager::Transfer on a destroying critter (its IsDestroyed-only guard
    // lets an IsDestroying critter through during the transfer/destroy cascade).
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (!GetMapId()) {
        return;
    }

    // Sync position
    auto map = require_refcount_ptr(GetParent<Map>());

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
    auto this_ref_holder = refcount_ptr<Critter>::from_add_ref(this);
    auto map_ref_holder = map;
    const auto dir = GetDir();

    for (auto& [cr, prev_hex] : moved_critters) {
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

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(GetMapId(), "Entity has no map id");

    while (!_visibleCrWhoSeeMe.empty()) {
        ptr<Critter> cr = _visibleCrWhoSeeMe.front();
        const auto del_ok = RemoveVisibleCritter(cr);
        FO_STRONG_ASSERT(del_ok, "Failed to remove visible critter");
        cr->Send_RemoveCritter(this);
    }
    while (!_visibleCr.empty()) {
        ptr<Critter> cr = _visibleCr.front();
        const auto del_ok2 = cr->RemoveVisibleCritter(this);
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

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

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

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

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

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (!GetMapId()) {
        FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
        vector<ptr<Critter>> critters = copy(*_globalMapGroup);
        vec_remove_unique_value(critters, this);
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

auto Critter::GetGlobalMapGroup() -> span<ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    // NOT NOT_DESTROYING: the DestroyCritter -> TransferToGlobal cascade unhooks a destroying critter onto the
    // global map and reads its group here (MapManager::Transfer), so this runs while IsDestroying.
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(!GetMapId(), "Map id is already set");
    FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");

    return *_globalMapGroup;
}

auto Critter::GetVisibleCritterMode(ident_t cr_id) const noexcept -> CritterVisibilityMode
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    const auto it = _visibleCrModes.find(cr_id);
    return it != _visibleCrModes.end() ? it->second : CritterVisibilityMode::None;
}

void Critter::SetVisibleCritterMode(ident_t cr_id, CritterVisibilityMode mode) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

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

    // Adds links into this critter's owned visibility structures (_visibleCrWhoSeeMe*) and into the target's
    // (_visibleCrMap/_visibleCrModes/_visibleCr) — both torn down during destruction, so neither end may be
    // mid-destruction. The live ProcessCritterLook path already returns before this on a destroying entity.
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
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

    const auto inserted2 = cr->_visibleCrMap.emplace(GetId(), this).second;
    FO_STRONG_ASSERT(inserted2, "Visible critter graph asymmetry on add", GetId(), cr->GetId());

    cr->_visibleCrModes[GetId()] = mode;
    _visibleCrWhoSeeMe.emplace_back(cr);
    cr->_visibleCr.emplace_back(this);

    return true;
}

auto Critter::RemoveVisibleCritter(ptr<Critter> cr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
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

    const auto it2 = std::ranges::find(cr->_visibleCr, this);
    FO_STRONG_ASSERT(it2 != cr->_visibleCr.end(), "Lookup failed in critter visible cr");
    cr->_visibleCr.erase(it2);

    return true;
}

auto Critter::AddCrIntoVisGroup1(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleCrGroup1.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisGroup2(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleCrGroup2.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisGroup3(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleCrGroup3.emplace(cr_id).second;
}

auto Critter::RemoveCrFromVisGroup1(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleCrGroup1.erase(cr_id) != 0;
}

auto Critter::RemoveCrFromVisGroup2(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleCrGroup2.erase(cr_id) != 0;
}

auto Critter::RemoveCrFromVisGroup3(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleCrGroup3.erase(cr_id) != 0;
}

auto Critter::AddVisibleItem(ident_t item_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleItems.emplace(item_id).second;
}

auto Critter::RemoveVisibleItem(ident_t item_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleItems.erase(item_id) != 0;
}

auto Critter::CheckVisibleItem(ident_t item_id) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _visibleItems.count(item_id) != 0;
}

auto Critter::CanSeeItemOnMap(ptr<const Item> item) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    // NOT NOT_DESTROYED/NOT_DESTROYING: this visibility query gracefully returns false when this critter or
    // the item is destroyed (the check below). It is invoked from per-critter/item loops that may have fired
    // entity-destroying events, so it must tolerate a destroyed self rather than assert/throw before that check.
    FO_VALIDATE_ENTITY(LOCKED);

    if (IsDestroyed() || item->IsDestroyed()) {
        return false;
    }
    if (item->GetOwnership() != ItemOwnership::MapHex) {
        return false;
    }
    if (!GetMapId() || item->GetMapId() != GetMapId()) {
        return false;
    }

    auto map = require_refcount_ptr(GetParent<Map>());

    return CheckItemVisibilityHook(_engine, map.as_ptr(), this, item);
}

void Critter::ChangeDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    // NOT NOT_DESTROYING: MapManager::AddCritterToMap sets a critter's facing during the transfer/destroy
    // cascade (past an IsDestroyed-only guard), so this can run while the critter is IsDestroying.
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    SetDir(dir);
}

void Critter::SetItem(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    // Adds a child item into the critter's owned inventory (_invItems) and parents it to this critter; the
    // inventory is torn down during destruction, so a critter mid-destruction must never gain new inventory.
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    vec_add_unique_value(_invItems, item);
    item->SetParent(this);
}

void Critter::RemoveItem(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    vec_remove_unique_value(_invItems, item);
    item->SetParent(nullptr);
}

auto Critter::GetInvItem(ident_t item_id) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

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

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    return _invItems;
}

auto Critter::GetInvItems() const noexcept -> vector<ptr<const Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
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

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return !_invItems.empty();
}

auto Critter::GetInvItemByPid(hstring item_pid) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    for (ptr<Item> item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item.as_nptr();
        }
    }

    return nullptr;
}

auto Critter::GetItemByPidInvPriority(hstring item_pid) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    auto nullable_proto = _engine->GetProtoItem(item_pid);
    FO_VERIFY_AND_THROW(nullable_proto, "Item proto not found", item_pid);
    auto proto = nullable_proto.as_ptr();

    if (proto->GetStackable()) {
        for (auto& item : _invItems) {
            if (item->GetProtoId() == item_pid) {
                return item.get();
            }
        }
    }
    else {
        // Non-stackable: prefer an item actually in the Inventory slot over one equipped elsewhere.
        nptr<Item> another_slot;

        for (auto& item : _invItems) {
            if (item->GetProtoId() == item_pid) {
                if (item->GetCritterSlot() == CritterItemSlot::Inventory) {
                    return item.get();
                }

                another_slot = item.get();
            }
        }

        return another_slot;
    }

    return nullptr;
}

auto Critter::GetInvItemBySlot(CritterItemSlot slot) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    const auto it = std::ranges::find_if(_invItems, [&](ptr<Item> item) noexcept -> bool { return item->GetCritterSlot() == slot; });

    if (it == _invItems.end()) {
        return nullptr;
    }

    return it->as_nptr();
}

auto Critter::CountInvItemByPid(hstring pid) const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    int32_t count = 0;

    for (ptr<const Item> item : _invItems) {
        if (item->GetProtoId() == pid) {
            count += item->GetCount();
        }
    }

    return count;
}

auto Critter::GetMapSpectators() -> vector<refcount_ptr<Player>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    auto map = GetParent<Map>();

    if (map) {
        return map->GetSpectatorPlayersForSend();
    }

    return {};
}

auto Critter::GetBroadcastRecipients(nptr<const Player> ignore_player) -> vector<refcount_ptr<Player>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    auto spectators = GetMapSpectators();

    vector<refcount_ptr<Player>> recipients;
    recipients.reserve(_visibleCrWhoSeeMe.size() + spectators.size());

    for (ptr<Critter> cr : _visibleCrWhoSeeMe) {
        if (refcount_nptr<Player> player = cr->GetPlayerForSend(); player && player != ignore_player) {
            recipients.emplace_back(std::move(player).take_not_null());
        }
    }

    for (refcount_ptr<Player> player : spectators) {
        if (player != ignore_player) {
            recipients.emplace_back(std::move(player));
        }
    }

    return recipients;
}

void Critter::Broadcast_Property(NetProperty type, ptr<const Property> prop, ptr<const ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    // NOT NOT_DESTROYING: a property change can fire during the critter's own teardown drain (the final
    // state push to viewers), so this broadcast is legitimately reached while IsDestroying. Both callees
    // (GetBroadcastRecipients, Player::Send_Property) are teardown-safe.
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Action(CritterAction action, int32_t action_data, nptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_Action(this, action, action_data, item);
    }
}

void Critter::Broadcast_Dir()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_Dir(this);
    }
}

void Critter::Broadcast_Teleport(mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_Teleport(this, to_hex);
    }
}

void Critter::SendAndBroadcast(nptr<const Player> ignore_player, const function<void(ptr<Player>)>& player_callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    if (nptr<Player> self_player = _player.load(std::memory_order_acquire); self_player && self_player != ignore_player) {
        player_callback(self_player.as_ptr());
    }

    for (refcount_ptr<Player> player : GetBroadcastRecipients(ignore_player)) {
        player_callback(player);
    }
}

void Critter::SendAndBroadcast_Moving()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    Send_Moving(this);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_Moving(this);
    }
}

void Critter::SendAndBroadcast_Action(CritterAction action, int32_t action_data, nptr<const Item> context_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    Send_Action(this, action, action_data, context_item);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_Action(this, action, action_data, context_item);
    }
}

void Critter::SendAndBroadcast_MoveItem(nptr<const Item> item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    Send_MoveItem(this, item, action, prev_slot);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_MoveItem(this, item, action, prev_slot);
    }
}

void Critter::SendAndBroadcast_Attachments()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    Send_Attachments(this);

    for (refcount_ptr<Player> player : GetBroadcastRecipients()) {
        player->Send_Attachments(this);
    }
}

void Critter::Send_Property(NetProperty type, ptr<const Property> prop, ptr<const ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(entity);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_Property(type, prop, entity);
    }
}

void Critter::Send_Moving(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_Moving(from_cr);
    }
}

void Critter::Send_MovingSpeed(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_MovingSpeed(from_cr);
    }
}

void Critter::Send_Dir(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_Dir(from_cr);
    }
}

void Critter::Send_AddCritter(ptr<const Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_AddCritter(cr);
    }
}

void Critter::Send_RemoveCritter(ptr<const Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_RemoveCritter(cr);
    }
}

void Critter::Send_CritterVisibilityMode(ptr<const Critter> cr, CritterVisibilityMode mode)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_CritterVisibilityMode(cr, mode);
    }
}

void Critter::Send_LoadMap(nptr<const Map> map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(map);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_LoadMap(map);
    }
}

void Critter::Send_AddItemOnMap(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_AddItemOnMap(item);
    }
}

void Critter::Send_RemoveItemFromMap(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_RemoveItemFromMap(item);
    }
}

void Critter::Send_ChosenAddItem(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_ChosenAddItem(item);
    }
}

void Critter::Send_ChosenRemoveItem(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_ChosenRemoveItem(item);
    }
}

void Critter::Send_Teleport(ptr<const Critter> cr, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_Teleport(cr, to_hex);
    }
}

void Critter::Send_TimeSync()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_TimeSync();
    }
}

void Critter::Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_InfoMessage(info_message, extra_text);
    }
}

void Critter::Send_Action(ptr<const Critter> from_cr, CritterAction action, int32_t action_data, nptr<const Item> context_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_Action(from_cr, action, action_data, context_item);
    }
}

void Critter::Send_MoveItem(ptr<const Critter> from_cr, nptr<const Item> item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_MoveItem(from_cr, item, action, prev_slot);
    }
}

void Critter::Send_PlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_PlaceToGameComplete();
    }
}

void Critter::Send_SomeItems(const_span<ptr<const Item>> items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_SomeItems(items, owned, with_inner_entities, context_param);
    }
}

void Critter::Send_Attachments(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    if (nptr<Player> player = _player.load(std::memory_order_acquire)) {
        player->Send_Attachments(from_cr);
    }
}

FO_END_NAMESPACE
