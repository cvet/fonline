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

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "EntitySync.h"
#include "Geometry.h"
#include "Movement.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE

class Player;
class MapManager;
class Item;
class Map;
class Location;

class Critter final : public ServerEntity, public EntityWithProto, public CritterProperties
{
public:
    Critter() = delete;
    Critter(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props = nullptr) noexcept;
    Critter(const Critter&) = delete;
    Critter(Critter&&) noexcept = delete;
    auto operator=(const Critter&) = delete;
    auto operator=(Critter&&) noexcept = delete;
    ~Critter() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override;
    [[nodiscard]] auto HasPlayer() const noexcept -> bool
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return _player.load(std::memory_order_acquire) != nullptr;
    }
    [[nodiscard]] auto GetPlayer() const noexcept -> nptr<const Player>
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return nptr<const Player>(_player.load(std::memory_order_acquire));
    }
    [[nodiscard]] auto GetPlayer() noexcept -> nptr<Player>
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return nptr<Player>(_player.load(std::memory_order_acquire));
    }
    // Lock-free, refcount-pinned resolution of the controlling player for sync-free broadcast fan-out.
    // Mirrors ServerEntity::GetParentRaw: no entity-access validation, and the returned handle keeps the
    // player alive for the whole send. Use only on the snapshot/dispatch path, never as a script accessor.
    [[nodiscard]] auto GetPlayerForSend() const noexcept -> refcount_nptr<Player>;
    [[nodiscard]] auto GetSyncWidenEntity() noexcept -> nptr<ServerEntity> override;
    [[nodiscard]] auto GetSyncWidenEntity() const noexcept -> nptr<const ServerEntity> override;
    [[nodiscard]] auto GetOfflineTime() const -> timespan;
    [[nodiscard]] auto IsAlive() const noexcept -> bool;
    [[nodiscard]] auto IsDead() const noexcept -> bool;
    [[nodiscard]] auto IsKnockout() const noexcept -> bool;
    [[nodiscard]] auto CheckFind(CritterFindType find_type) const noexcept -> bool;
    [[nodiscard]] auto HasItems() const noexcept -> bool;
    [[nodiscard]] auto GetInvItem(ident_t item_id) noexcept -> nptr<Item>;
    [[nodiscard]] auto GetInvItems() noexcept -> vector<ptr<Item>>;
    [[nodiscard]] auto GetInvItems() const noexcept -> vector<ptr<const Item>>;
    [[nodiscard]] auto GetInvItemByPid(hstring item_pid) noexcept -> nptr<Item>;
    [[nodiscard]] auto GetInvItemBySlot(CritterItemSlot slot) noexcept -> nptr<Item>;
    [[nodiscard]] auto CountInvItemByPid(hstring pid) const noexcept -> int32_t;
    [[nodiscard]] auto GetVisibleItems() const noexcept -> const unordered_set<ident_t>&
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _visibleItems;
    }
    [[nodiscard]] auto IsSeeItem(ident_t item_id) const noexcept -> bool
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _visibleItems.contains(item_id);
    }
    [[nodiscard]] auto IsSeeCritter(ident_t cr_id) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id, CritterSeeType see_type) -> nptr<Critter>;
    [[nodiscard]] auto GetCritters(CritterSeeType see_type, CritterFindType find_type) -> vector<ptr<Critter>>;
    [[nodiscard]] auto HasGlobalMapGroup() const noexcept -> bool;
    [[nodiscard]] auto IsSameGlobalMapGroup(ptr<const Critter> other) const noexcept -> bool;
    [[nodiscard]] auto GetGlobalMapGroup() -> span<ptr<Critter>>;
    void CreateGlobalMapGroup();
    void JoinGlobalMapGroup(ptr<Critter> group_owner);
    void AddToGlobalMapGroup(ptr<Critter> cr);
    void RemoveFromGlobalMapGroup(ptr<Critter> cr);
    void ResetGlobalMapGroup() noexcept;
    [[nodiscard]] auto IsMoving() const noexcept -> bool
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return !!_moving;
    }
    [[nodiscard]] auto GetMovingUid() const noexcept -> uint32_t
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _movingUid;
    }
    [[nodiscard]] auto GetMoving() const noexcept -> nptr<const MovingContext>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _moving.as_nptr();
    }
    [[nodiscard]] auto GetMoving() noexcept -> nptr<MovingContext>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _moving.as_nptr();
    }
    [[nodiscard]] auto GetMovingContext() const noexcept -> nptr<const MovingContext>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        if (_moving) {
            return _moving.as_nptr();
        }
        return _lastMoving.as_nptr();
    }
    [[nodiscard]] auto GetMovingContext() noexcept -> nptr<MovingContext>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        if (_moving) {
            return _moving.as_nptr();
        }
        return _lastMoving.as_nptr();
    }
    [[nodiscard]] auto GetMovingState() const noexcept -> MovingState
    {
        FO_VALIDATE_ENTITY_ACCESS();
        if (_moving) {
            return MovingState::InProgress;
        }
        if (!_lastMoving) {
            return MovingState::Success;
        }

        auto last_moving = _lastMoving.as_ptr();
        return last_moving->GetCompleteReason();
    }
    [[nodiscard]] auto IsMapTransfersLocked() const noexcept -> bool
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _lockMapTransfers != 0;
    }
    [[nodiscard]] auto HasAttachedCritters() const noexcept -> bool
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return !_attachedCritters.empty();
    }
    [[nodiscard]] auto GetAttachedCritters() noexcept -> span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _attachedCritters;
    }
    [[nodiscard]] auto GetAttachedCritters() const noexcept -> const_span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _attachedCritters;
    }

    auto AddVisibleCritter(ptr<Critter> cr, CritterVisibilityMode mode) -> bool;
    auto GetVisibleCritterMode(ident_t cr_id) const noexcept -> CritterVisibilityMode;
    void SetVisibleCritterMode(ident_t cr_id, CritterVisibilityMode mode) noexcept;
    auto RemoveVisibleCritter(ptr<Critter> cr) -> bool;
    auto AddCrIntoVisGroup1(ident_t cr_id) noexcept -> bool;
    auto AddCrIntoVisGroup2(ident_t cr_id) noexcept -> bool;
    auto AddCrIntoVisGroup3(ident_t cr_id) noexcept -> bool;
    auto RemoveCrFromVisGroup1(ident_t cr_id) noexcept -> bool;
    auto RemoveCrFromVisGroup2(ident_t cr_id) noexcept -> bool;
    auto RemoveCrFromVisGroup3(ident_t cr_id) noexcept -> bool;
    auto AddVisibleItem(ident_t item_id) noexcept -> bool;
    auto RemoveVisibleItem(ident_t item_id) noexcept -> bool;
    auto CheckVisibleItem(ident_t item_id) const noexcept -> bool;
    auto CanSeeItemOnMap(ptr<const Item> item) const -> bool;

    void MarkIsForPlayer();
    void UnmarkIsForPlayer();
    void SetMoving(refcount_ptr<MovingContext> moving);
    void StopMoving(MovingState reason = MovingState::Stopped);
    void AttachPlayer(ptr<Player> player);
    void DetachPlayer();
    void AttachToCritter(ptr<Critter> cr);
    void DetachFromCritter();
    void MoveAttachedCritters();
    void AddAttachedCritter(ptr<Critter> cr);
    void RemoveAttachedCritter(ptr<Critter> cr);
    void ClearVisibleEnitites();
    void SetItem(ptr<Item> item);
    void RemoveItem(ptr<Item> item);
    void ChangeDir(mdir dir);
    void LockMapTransfers() noexcept
    {
        FO_VALIDATE_ENTITY_ACCESS();
        _lockMapTransfers++;
    }
    void UnlockMapTransfers() noexcept
    {
        FO_VALIDATE_ENTITY_ACCESS();
        _lockMapTransfers--;
    }

    void Broadcast_Property(NetProperty type, ptr<const Property> prop, ptr<const ServerEntity> entity);
    void Broadcast_Action(CritterAction action, int32_t action_data, nptr<const Item> item);
    void Broadcast_Dir();
    void Broadcast_Teleport(mpos to_hex);

    void SendAndBroadcast(nptr<const Player> ignore_player, const function<void(ptr<Player>)>& player_callback);
    void SendAndBroadcast_Moving();
    void SendAndBroadcast_Action(CritterAction action, int32_t action_data, nptr<const Item> context_item);
    void SendAndBroadcast_MoveItem(nptr<const Item> item, CritterAction action, CritterItemSlot prev_slot);
    void SendAndBroadcast_Attachments();

    void Send_Property(NetProperty type, ptr<const Property> prop, ptr<const ServerEntity> entity);
    void Send_Moving(ptr<const Critter> from_cr);
    void Send_MovingSpeed(ptr<const Critter> from_cr);
    void Send_Dir(ptr<const Critter> from_cr);
    void Send_AddCritter(ptr<const Critter> cr);
    void Send_RemoveCritter(ptr<const Critter> cr);
    void Send_CritterVisibilityMode(ptr<const Critter> cr, CritterVisibilityMode mode);
    void Send_LoadMap(nptr<const Map> map);
    void Send_AddItemOnMap(ptr<const Item> item);
    void Send_RemoveItemFromMap(ptr<const Item> item);
    void Send_ChosenAddItem(ptr<const Item> item);
    void Send_ChosenRemoveItem(ptr<const Item> item);
    void Send_Teleport(ptr<const Critter> cr, mpos to_hex);
    void Send_TimeSync();
    void Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text = "");
    void Send_Action(ptr<const Critter> from_cr, CritterAction action, int32_t action_data, nptr<const Item> context_item);
    void Send_MoveItem(ptr<const Critter> from_cr, nptr<const Item> item, CritterAction action, CritterItemSlot prev_slot);
    void Send_PlaceToGameComplete();
    void Send_SomeItems(const_span<ptr<const Item>> items, bool owned, bool with_inner_entities, const any_t& context_param);
    void Send_Attachments(ptr<const Critter> from_cr);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppeared, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppearedDist1, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppearedDist2, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppearedDist3, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappeared, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappearedDist1, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappearedDist2, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappearedDist3, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterVisibilityModeChanged, Critter* /*targetCr*/, CritterVisibilityMode /*mode*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemOnMapAppeared, Item* /*item*/, Critter* /*dropper*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemOnMapDisappeared, Item* /*item*/, Critter* /*picker*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemOnMapChanged, Item* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnTalk, Critter* /*playerCr*/, bool /*begin*/, int32_t /*talkers*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnBarter, Critter* /*playerCr*/, bool /*begin*/, int32_t /*barterCount*/);

private:
    auto GetMapSpectators() -> vector<refcount_ptr<Player>>;
    auto GetBroadcastRecipients(nptr<const Player> ignore_player = nullptr) -> vector<refcount_ptr<Player>>;

    uint32_t _movingUid {};
    refcount_nptr<MovingContext> _moving {};
    refcount_nptr<MovingContext> _lastMoving {};
    std::atomic<Player*> _player {};
    nanotime _playerDetachTime {};
    vector<ptr<Critter>> _attachedCritters {};
    vector<ptr<Item>> _invItems {};
    vector<ptr<Critter>> _visibleCrWhoSeeMe {};
    vector<ptr<Critter>> _visibleCr {};
    unordered_map<ident_t, ptr<Critter>> _visibleCrWhoSeeMeMap {};
    unordered_map<ident_t, ptr<Critter>> _visibleCrMap {};
    unordered_map<ident_t, CritterVisibilityMode> _visibleCrModes {};
    unordered_set<ident_t> _visibleCrGroup1 {};
    unordered_set<ident_t> _visibleCrGroup2 {};
    unordered_set<ident_t> _visibleCrGroup3 {};
    unordered_set<ident_t> _visibleItems {};
    shared_ptr<vector<ptr<Critter>>> _globalMapGroup {};
    int32_t _lockMapTransfers {};
    EntityLock _ownedLock {};
};

FO_END_NAMESPACE
