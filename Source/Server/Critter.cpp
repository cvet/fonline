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

Critter::Critter(ServerEngine* engine, ident_t id, const ProtoCritter* proto, const Properties* props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties(), &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();
    SetEntityLock(&_ownedLock);
}

Critter::~Critter()
{
    FO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();

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

    FO_NO_VALIDATE_ENTITY_ACCESS();
    return _player ? _player->GetName() : _proto->GetName();
}

auto Critter::GetSyncWidenEntity() noexcept -> ServerEntity*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();
    return _player.get();
}

auto Critter::GetOfflineTime() const -> timespan
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    return GetControlledByPlayer() && _player == nullptr ? _engine->GameTime.GetFrameTime() - _playerDetachTime : timespan::zero;
}

auto Critter::IsAlive() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return GetCondition() == CritterCondition::Alive;
}

auto Critter::IsDead() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return GetCondition() == CritterCondition::Dead;
}

auto Critter::IsKnockout() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return GetCondition() == CritterCondition::Knockout;
}

auto Critter::CheckFind(CritterFindType find_type) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();

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

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(!GetControlledByPlayer(), "Controlled by player is already set");

    refcount_ptr<Map> map;

    if (GetMapId()) {
        map = GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map.get());
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

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    FO_VERIFY_AND_THROW(!_player, "Player is already set");

    refcount_ptr<Map> map;

    if (GetMapId()) {
        map = GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map.get());
    }

    SetControlledByPlayer(false);

    if (map) {
        map->RefreshCritterPlayerState(this);
    }
}

void Critter::AttachPlayer(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    FO_VERIFY_AND_THROW(player, "Missing player instance");
    FO_VERIFY_AND_THROW(!player->GetControlledCritterId(), "Player already controls a critter");
    FO_VERIFY_AND_THROW(!_player, "Player is already set");
    FO_VERIFY_AND_THROW(!player->GetViewMap(), "Player still has an active view map");

    _player = player;

    _player->SetControlledCritterId(GetId());
    _player->SetLastControlledCritterId(GetId());
    _player->SetControlledCritter(this);
}

void Critter::DetachPlayer()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(GetControlledByPlayer(), "Critter is not controlled by a player");
    FO_VERIFY_AND_THROW(_player, "Missing required player");
    FO_VERIFY_AND_THROW(_player->GetControlledCritterId() == GetId(), "Player controlled critter id does not match this critter");

    _player->SetControlledCritterId({});
    _player->SetControlledCritter(nullptr);

    _player = nullptr;
    _playerDetachTime = _engine->GameTime.GetFrameTime();
}

void Critter::SetMoving(refcount_ptr<MovingContext> moving)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_moving) {
        _moving->Complete(MovingState::Stopped);
        _lastMoving = _moving;
    }

    _moving = std::move(moving);
    _movingUid++;
    SetMovingSpeed(numeric_cast<int32_t>(_moving->GetSpeed()));
}

void Critter::StopMoving(MovingState reason)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (!_moving) {
        return;
    }

    _moving->Complete(reason);
    _lastMoving = _moving;
    _moving.reset();
    _movingUid++;
    SetMovingSpeed(0);
}

void Critter::AddAttachedCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vec_add_unique_value(_attachedCritters, cr);
}

void Critter::RemoveAttachedCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vec_remove_unique_value(_attachedCritters, cr);
}

void Critter::AttachToCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
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

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(GetIsAttached(), "Missing required is attached");
    FO_VERIFY_AND_THROW(GetAttachMaster(), "Missing required attach master");

    auto cr = _engine->EntityMngr.GetCritter(GetAttachMaster());
    FO_VERIFY_AND_THROW(cr, "Missing critter instance");

    cr->RemoveAttachedCritter(this);
    SetIsAttached(false);
    SetAttachMaster({});

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::MoveAttachedCritters()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (!GetMapId()) {
        return;
    }

    // Sync position
    auto map = GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    vector<tuple<Critter*, mpos, refcount_ptr<Critter>>> moved_critters;

    const auto new_hex = GetHex();
    const auto new_hex_offset = GetHexOffset();

    auto attached = GetAttachedCritters();

    for (auto& cr : attached) {
        FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
        FO_VERIFY_AND_THROW(cr->GetIsAttached(), "Critter is not attached");
        FO_VERIFY_AND_THROW(cr->GetAttachMaster() == GetId(), "Attached critter has a different master");
        FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");

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
    const auto dir = GetDir();

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

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(GetMapId(), "Entity has no map id");

    while (!_visibleCrWhoSeeMe.empty()) {
        auto* cr = _visibleCrWhoSeeMe.front().get();
        FO_VERIFY_AND_THROW(cr, "Missing critter instance");
        const auto del_ok = RemoveVisibleCritter(cr);
        FO_STRONG_ASSERT(del_ok, "Missing required del ok");
        cr->Send_RemoveCritter(this);
    }
    while (!_visibleCr.empty()) {
        auto* cr = _visibleCr.front().get();
        FO_VERIFY_AND_THROW(cr, "Missing critter instance");
        const auto del_ok2 = cr->RemoveVisibleCritter(this);
        FO_STRONG_ASSERT(del_ok2, "Missing required del ok2");
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

    FO_VALIDATE_ENTITY_ACCESS();

    if (!GetMapId()) {
        FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
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

    FO_VALIDATE_ENTITY_ACCESS();

    if (!GetMapId()) {
        FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
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

    FO_VALIDATE_ENTITY_ACCESS();

    if (!GetMapId()) {
        FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");
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

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(!GetMapId(), "Map id is already set");
    FO_VERIFY_AND_THROW(_globalMapGroup, "Critter has no global map group");

    return *_globalMapGroup;
}

auto Critter::GetVisibleCritterMode(ident_t cr_id) const noexcept -> CritterVisibilityMode
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    const auto it = _visibleCrModes.find(cr_id);
    return it != _visibleCrModes.end() ? it->second : CritterVisibilityMode::None;
}

void Critter::SetVisibleCritterMode(ident_t cr_id, CritterVisibilityMode mode) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();

    if (mode == CritterVisibilityMode::None) {
        _visibleCrModes.erase(cr_id);
    }
    else {
        _visibleCrModes[cr_id] = mode;
    }
}

auto Critter::AddVisibleCritter(Critter* cr, CritterVisibilityMode mode) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(cr, "Missing critter instance");
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

auto Critter::RemoveVisibleCritter(Critter* cr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(cr, "Missing critter instance");
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

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleCrGroup1.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisGroup2(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleCrGroup2.emplace(cr_id).second;
}

auto Critter::AddCrIntoVisGroup3(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleCrGroup3.emplace(cr_id).second;
}

auto Critter::RemoveCrFromVisGroup1(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleCrGroup1.erase(cr_id) != 0;
}

auto Critter::RemoveCrFromVisGroup2(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleCrGroup2.erase(cr_id) != 0;
}

auto Critter::RemoveCrFromVisGroup3(ident_t cr_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleCrGroup3.erase(cr_id) != 0;
}

auto Critter::AddVisibleItem(ident_t item_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleItems.emplace(item_id).second;
}

auto Critter::RemoveVisibleItem(ident_t item_id) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleItems.erase(item_id) != 0;
}

auto Critter::CheckVisibleItem(ident_t item_id) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return _visibleItems.count(item_id) != 0;
}

auto Critter::CanSeeItemOnMap(const Item* item) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(item, "Missing item instance");

    if (IsDestroyed() || item->IsDestroyed()) {
        return false;
    }
    if (item->GetOwnership() != ItemOwnership::MapHex) {
        return false;
    }
    if (!GetMapId() || item->GetMapId() != GetMapId()) {
        return false;
    }

    const auto map = GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    return CheckItemVisibilityHook(_engine.get(), map.get(), this, item);
}

void Critter::ChangeDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    SetDir(dir);
}

void Critter::SetItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(item, "Missing item instance");

    vec_add_unique_value(_invItems, item);
    item->SetParent(this);
}

void Critter::RemoveItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(item, "Missing item instance");

    vec_remove_unique_value(_invItems, item);
    item->SetParent(nullptr);
}

auto Critter::GetInvItem(ident_t item_id) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    FO_NON_CONST_METHOD_HINT();

    for (auto& item : _invItems) {
        if (item->GetId() == item_id) {
            return item.get();
        }
    }

    return nullptr;
}

auto Critter::GetInvItems() noexcept -> vector<raw_ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    FO_NON_CONST_METHOD_HINT();

    return _invItems;
}

auto Critter::GetInvItems() const noexcept -> vector<raw_ptr<const Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    vector<raw_ptr<const Item>> result;
    result.reserve(_invItems.size());

    for (const auto& item : _invItems) {
        result.emplace_back(item.get());
    }

    return result;
}

auto Critter::HasItems() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    return !_invItems.empty();
}

auto Critter::GetInvItemByPid(hstring item_pid) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
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

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    FO_NON_CONST_METHOD_HINT();

    const auto it = std::ranges::find_if(_invItems, [&](auto&& item) noexcept -> bool { return item->GetCritterSlot() == slot; });

    if (it == _invItems.end()) {
        return nullptr;
    }

    return it->get();
}

auto Critter::CountInvItemByPid(hstring pid) const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    int32_t count = 0;

    for (const auto& item : _invItems) {
        if (item->GetProtoId() == pid) {
            count += item->GetCount();
        }
    }

    return count;
}

auto Critter::GetMapSpectators() -> ref_hold_vector<Player*>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto map = GetParent<Map>();

    if (map && map->HasSpectatorPlayers()) {
        return copy_hold_ref(map->GetSpectatorPlayers());
    }

    return ref_hold_vector<Player*>(0);
}

void Critter::Broadcast_Property(NetProperty type, const Property* prop, const ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Property(type, prop, entity);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Action(CritterAction action, int32_t action_data, const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Action(this, action, action_data, item);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_Action(this, action, action_data, item);
    }
}

void Critter::Broadcast_Dir()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Dir(this);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_Dir(this);
    }
}

void Critter::Broadcast_Teleport(mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_NON_CONST_METHOD_HINT();

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Teleport(this, to_hex);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_Teleport(this, to_hex);
    }
}

void Critter::SendAndBroadcast(const Player* ignore_player, const function<void(Critter*)>& cr_callback, const function<void(Player*)>& spectator_callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (ignore_player == nullptr || ignore_player != GetPlayer()) {
        cr_callback(this);
    }

    for (auto& cr : _visibleCrWhoSeeMe) {
        if (ignore_player == nullptr || ignore_player != cr->GetPlayer()) {
            cr_callback(cr.get());
        }
    }

    if (spectator_callback) {
        for (auto* player : GetMapSpectators()) {
            if (player != ignore_player) {
                spectator_callback(player);
            }
        }
    }
}

void Critter::SendAndBroadcast_Moving()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    Send_Moving(this);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Moving(this);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_Moving(this);
    }
}

void Critter::SendAndBroadcast_Action(CritterAction action, int32_t action_data, const Item* context_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    Send_Action(this, action, action_data, context_item);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Action(this, action, action_data, context_item);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_Action(this, action, action_data, context_item);
    }
}

void Critter::SendAndBroadcast_MoveItem(const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    Send_MoveItem(this, item, action, prev_slot);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_MoveItem(this, item, action, prev_slot);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_MoveItem(this, item, action, prev_slot);
    }
}

void Critter::SendAndBroadcast_Attachments()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    Send_Attachments(this);

    for (auto& cr : _visibleCrWhoSeeMe) {
        cr->Send_Attachments(this);
    }

    for (auto* player : GetMapSpectators()) {
        player->Send_Attachments(this);
    }
}

void Critter::Send_Property(NetProperty type, const Property* prop, const ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_Property(type, prop, entity);
    }
}

void Critter::Send_Moving(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_Moving(from_cr);
    }
}

void Critter::Send_MovingSpeed(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_MovingSpeed(from_cr);
    }
}

void Critter::Send_Dir(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_Dir(from_cr);
    }
}

void Critter::Send_AddCritter(const Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_AddCritter(cr);
    }
}

void Critter::Send_RemoveCritter(const Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_RemoveCritter(cr);
    }
}

void Critter::Send_CritterVisibilityMode(const Critter* cr, CritterVisibilityMode mode)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_CritterVisibilityMode(cr, mode);
    }
}

void Critter::Send_LoadMap(const Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    if (_player) {
        _player->Send_LoadMap(map);
    }
}

void Critter::Send_AddItemOnMap(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_AddItemOnMap(item);
    }
}

void Critter::Send_RemoveItemFromMap(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_RemoveItemFromMap(item);
    }
}

void Critter::Send_ChosenAddItem(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_ChosenAddItem(item);
    }
}

void Critter::Send_ChosenRemoveItem(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_ChosenRemoveItem(item);
    }
}

void Critter::Send_Teleport(const Critter* cr, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_Teleport(cr, to_hex);
    }
}

void Critter::Send_TimeSync()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_TimeSync();
    }
}

void Critter::Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_InfoMessage(info_message, extra_text);
    }
}

void Critter::Send_Action(const Critter* from_cr, CritterAction action, int32_t action_data, const Item* context_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_Action(from_cr, action, action_data, context_item);
    }
}

void Critter::Send_MoveItem(const Critter* from_cr, const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_MoveItem(from_cr, item, action, prev_slot);
    }
}

void Critter::Send_PlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_PlaceToGameComplete();
    }
}

void Critter::Send_SomeItems(const_span<Item*> items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_SomeItems(items, owned, with_inner_entities, context_param);
    }
}

void Critter::Send_Attachments(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_player) {
        _player->Send_Attachments(from_cr);
    }
}

FO_END_NAMESPACE
