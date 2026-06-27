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

#include "Common.h"

#include "Geometry.h"
#include "Movement.h"
#include "ScriptSystem.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

template<typename TParent, typename TEntity>
static auto RequireParent(ptr<TEntity> entity, string_view error_message) -> refcount_ptr<TParent>
{
    FO_STACK_TRACE_ENTRY();

    auto parent = entity->template GetParent<TParent>();

    if (!parent) {
        throw ScriptException(error_message);
    }

    return std::move(parent).take_not_null();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetupScript(ptr<Critter> self, ScriptFunc<void, Critter*, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }

    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetupScriptEx(ptr<Critter> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsMoving(ptr<Critter> self)
{
    return self->IsMoving();
}

///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Server_Critter_GetMovingContext(ptr<Critter> self)
{
    auto moving = self->GetMovingContext();
    return moving.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Server_Critter_GetMovingUid(ptr<Critter> self)
{
    return self->GetMovingUid();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Player> Server_Critter_GetPlayer(ptr<Critter> self)
{
    auto player = self->GetPlayer();
    return player.get_no_const();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Map> Server_Critter_GetMap(ptr<Critter> self)
{
    auto nullable_map = self->GetParent<Map>();

    return ReleaseNullableScriptOwnership(std::move(nullable_map));
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToHex(ptr<Critter> self, mpos hex)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    auto nullable_map = self->GetParent<Map>();
    FO_VERIFY_AND_THROW(nullable_map, "Missing map instance");
    auto map_holder = std::move(nullable_map).take_not_null();
    auto map = map_holder.as_ptr();

    ValidateEntityAccess(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    if (hex != self->GetHex()) {
        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, self->GetDir(), 2);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToHex(ptr<Critter> self, mpos hex, mdir dir)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    auto nullable_map = self->GetParent<Map>();
    FO_VERIFY_AND_THROW(nullable_map, "Missing map instance");
    auto map_holder = std::move(nullable_map).take_not_null();
    auto map = map_holder.as_ptr();

    ValidateEntityAccess(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    if (hex != self->GetHex()) {
        if (self->GetDir() != dir) {
            self->ChangeDir(dir);
        }

        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, self->GetDir(), 2);
    }
    else if (self->GetDir() != dir) {
        self->ChangeDir(dir);
        self->Send_Dir(self);
        self->Broadcast_Dir();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(ptr<Critter> self, ptr<Map> map, mpos hex)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid target hex arg", hex, map->GetSize());
    }

    self->GetEngine()->MapMngr.TransferToMap(self, map, hex, self->GetDir(), 2);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(ptr<Critter> self, ptr<Map> map, mpos hex, mdir dir, bool preciseHex = false)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid target hex arg", hex, map->GetSize());
    }

    if (preciseHex) {
        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, dir, std::nullopt);
    }
    else {
        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, dir, 2);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobal(ptr<Critter> self)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    if (!self->GetMapId()) {
        return;
    }

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalWithGroup(ptr<Critter> self, readonly_vector<Critter*> group)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});

    for (nptr<Critter> nullable_cr : group) {
        if (!nullable_cr) {
            continue;
        }

        auto group_cr = nullable_cr.as_ptr();
        if (!group_cr->IsDestroyed() && !self->IsDestroyed() && !self->GetMapId()) {
            self->GetEngine()->MapMngr.TransferToGlobal(group_cr, self->GetId());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalGroup(ptr<Critter> self, ptr<Critter> globalCr)
{
    ptr<Critter> global_cr_ptr = globalCr;

    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }
    if (global_cr_ptr->GetMapId()) {
        throw ScriptException("Global critter is not on global map");
    }
    if (self == global_cr_ptr) {
        throw ScriptException("Cannot join own group");
    }

    MapManager& map_mngr = self->GetEngine()->MapMngr;

    if (self->GetMapId()) {
        map_mngr.TransferToGlobal(self, global_cr_ptr->GetId());
        return;
    }

    if (self->IsSameGlobalMapGroup(global_cr_ptr)) {
        return;
    }

    map_mngr.RemoveCritterFromMap(self, nullptr);

    if (self->IsDestroyed()) {
        return;
    }

    map_mngr.AddCritterToMap(self, nullptr, {}, mdir {}, global_cr_ptr->GetId());

    if (self->IsDestroyed()) {
        return;
    }

    for (ptr<Critter> member : self->GetGlobalMapGroup()) {
        if (!(member == self)) {
            self->Send_AddCritter(member);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsAlive(ptr<Critter> self)
{
    return self->IsAlive();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsKnockout(ptr<Critter> self)
{
    return self->IsKnockout();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsDead(ptr<Critter> self)
{
    return self->IsDead();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RefreshView(ptr<Critter> self)
{
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
    self->GetEngine()->MapMngr.ProcessVisibleItems(self);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetDir(ptr<Critter> self, mdir dir)
{
    if (dir == self->GetDir()) {
        return;
    }

    self->ChangeDir(dir);
    self->Send_Dir(self);
    self->Broadcast_Dir();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Critter> Server_Critter_GetCritter(ptr<Critter> self, ident_t id, CritterSeeType seeType)
{
    auto cr = self->GetCritter(id, seeType);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetCritters(ptr<Critter> self, CritterSeeType seeType, CritterFindType findType)
{
    vector<ptr<Critter>> critters = self->GetCritters(seeType, findType);

    if (self->GetMapId()) {
        std::ranges::stable_sort(critters, [hex = self->GetHex()](ptr<const Critter> cr1, ptr<const Critter> cr2) {
            const int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
            const int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
            return dist1 < dist2;
        });
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(ptr<Critter> self, ptr<Critter> cr)
{
    if (self == cr) {
        return true;
    }

    return self->IsSeeCritter(cr->GetId());
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSeenBy(ptr<Critter> self, ptr<Critter> cr)
{
    if (self == cr) {
        return true;
    }

    return cr->IsSeeCritter(self->GetId());
}

///@ ExportMethod
FO_SCRIPT_API CritterVisibilityMode Server_Critter_GetVisibilityMode(ptr<Critter> self, ptr<Critter> cr)
{
    if (self == cr) {
        return CritterVisibilityMode::Full;
    }

    return self->GetVisibleCritterMode(cr->GetId());
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(ptr<Critter> self, ptr<Item> item)
{
    return self->CheckVisibleItem(item->GetId());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Critter_CountItem(ptr<Critter> self, hstring protoId)
{
    return self->CountInvItemByPid(protoId);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Critter_CountItem(ptr<Critter> self, ptr<ProtoItem> proto)
{
    return self->CountInvItemByPid(proto->GetProtoId());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(ptr<Critter> self, hstring pid)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }

    const auto count = self->CountInvItemByPid(pid);

    if (count == 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, pid, count);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(ptr<Critter> self, ptr<ProtoItem> proto)
{
    const auto count = self->CountInvItemByPid(proto->GetProtoId());

    if (count == 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, proto->GetProtoId(), count);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(ptr<Critter> self, hstring pid, int32_t count)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }

    if (count <= 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, pid, count);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(ptr<Critter> self, ptr<ProtoItem> proto, int32_t count)
{
    if (count <= 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, proto->GetProtoId(), count);
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Critter_AddItem(ptr<Critter> self, hstring pid, int32_t count)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a critter that is being destroyed", self->GetId());
    }
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }
    if (!self->GetEngine()->GetProtoItem(pid)) {
        throw ScriptException("Invalid proto", pid);
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    auto item = self->GetEngine()->ItemMngr.AddItemCritter(self, pid, count);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Critter_AddItem(ptr<Critter> self, ptr<ProtoItem> proto, int32_t count)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a critter that is being destroyed", self->GetId());
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    auto item = self->GetEngine()->ItemMngr.AddItemCritter(self, proto->GetProtoId(), count);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    auto item = self->GetInvItem(itemId);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, hstring protoId)
{
    auto item = self->GetEngine()->CrMngr.GetItemByPidInvPriority(self, protoId);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, ptr<ProtoItem> proto)
{
    auto item = self->GetEngine()->CrMngr.GetItemByPidInvPriority(self, proto->GetProtoId());
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    for (ptr<Item> item : self->GetInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item.get_no_const();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(ptr<Critter> self)
{
    vector<ptr<Item>> items = self->GetInvItems();
    return MakeScriptHandleVector<Item>(items);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(ptr<Critter> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    vector<ptr<Item>> items = self->GetInvItems();

    vector<ptr<Item>> result;
    result.reserve(items.size());

    for (ptr<Item> item : items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.push_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(ptr<Critter> self, hstring protoId)
{
    vector<ptr<Item>> items = self->GetInvItems();

    vector<ptr<Item>> result;
    result.reserve(items.size());

    for (ptr<Item> item : items) {
        if (item->GetProtoId() == protoId) {
            result.push_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(ptr<Critter> self, ptr<ProtoItem> proto)
{
    vector<ptr<Item>> items = self->GetInvItems();

    vector<ptr<Item>> result;
    result.reserve(items.size());

    for (ptr<Item> item : items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            result.push_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeItemSlot(ptr<Critter> self, ident_t itemId, CritterItemSlot slot)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto self_holder = self.hold_ref();
    ignore_unused(self_holder);

    auto nullable_item = self->GetInvItem(itemId);

    if (!nullable_item) {
        throw ScriptException("Item not found");
    }

    auto item = nullable_item.as_ptr();
    refcount_ptr item_holder = item.hold_ref();
    ignore_unused(item_holder);

    // To slot arg is equal of current item slot
    if (item->GetCritterSlot() == slot) {
        return;
    }

    if (static_cast<size_t>(slot) >= self->GetEngine()->Settings->CritterSlotEnabled.size() || !self->GetEngine()->Settings->CritterSlotEnabled[static_cast<size_t>(slot)]) {
        throw ScriptException("Slot is not allowed");
    }

    const auto is_multi_item_allowed = static_cast<size_t>(slot) < self->GetEngine()->Settings->CritterSlotMultiItem.size() && self->GetEngine()->Settings->CritterSlotMultiItem[static_cast<size_t>(slot)];

    if (is_multi_item_allowed) {
        const auto from_slot = item->GetCritterSlot();

        item->SetCritterSlot(slot);

        self->SendAndBroadcast_MoveItem(item, CritterAction::MoveItem, from_slot);

        ValidateEntityAccess(self);
        ValidateEntityAccess(item);
        self->GetEngine()->OnCritterItemMoved.Fire(self.get(), item.get(), from_slot);
    }
    else {
        auto nullable_item_swap = self->GetInvItemBySlot(slot);
        const auto from_slot = item->GetCritterSlot();

        item->SetCritterSlot(slot);

        if (nullable_item_swap) {
            auto item_swap = nullable_item_swap.as_ptr();
            item_swap->SetCritterSlot(from_slot);
        }

        self->SendAndBroadcast_MoveItem(item, CritterAction::MoveItem, from_slot);

        if (nullable_item_swap) {
            auto item_swap = nullable_item_swap.as_ptr();
            refcount_ptr item_swap_holder = item_swap.hold_ref();
            ignore_unused(item_swap_holder);

            ValidateEntityAccess(self);
            ValidateEntityAccess(item_swap);
            self->GetEngine()->OnCritterItemMoved.Fire(self.get(), item_swap.get(), slot);
        }

        // ValidateEntityAccess and event dispatch tolerate destroyed entity arguments.
        ValidateEntityAccess(self);
        ValidateEntityAccess(item);
        self->GetEngine()->OnCritterItemMoved.Fire(self.get(), item.get(), from_slot);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetCondition(ptr<Critter> self, CritterCondition cond, CritterActionAnim actionAnim, nptr<AbstractItem> contextItem)
{
    const auto prev_cond = self->GetCondition();

    if (prev_cond == cond) {
        return;
    }

    refcount_nptr<Map> nullable_map {};

    if (self->GetMapId()) {
        nullable_map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(nullable_map, "Missing map instance");
        auto map = nullable_map.as_ptr();

        ValidateEntityAccess(map);

        map->RemoveCritterFromField(self);
    }

    self->SetCondition(cond);
    const auto context_item = contextItem.dyn_cast<const Item>();

    if (nullable_map) {
        auto map = nullable_map.as_ptr();
        map->AddCritterToField(self);
    }

    if (cond == CritterCondition::Dead) {
        self->SendAndBroadcast_Action(CritterAction::Dead, static_cast<int32_t>(actionAnim), context_item);
    }
    else if (cond == CritterCondition::Knockout) {
        self->SendAndBroadcast_Action(CritterAction::Knockout, static_cast<int32_t>(actionAnim), context_item);
    }
    else if (cond == CritterCondition::Alive) {
        if (prev_cond == CritterCondition::Knockout) {
            self->SendAndBroadcast_Action(CritterAction::StandUp, static_cast<int32_t>(actionAnim), context_item);
        }
        else {
            self->SendAndBroadcast_Action(CritterAction::Respawn, static_cast<int32_t>(actionAnim), context_item);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Action(ptr<Critter> self, CritterAction action, int32_t actionData, nptr<AbstractItem> contextItem)
{
    self->SendAndBroadcast_Action(action, actionData, contextItem.dyn_cast<const Item>());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SendItems(ptr<Critter> self, readonly_vector<Item*> items, bool owned = false, bool withInnerEntities = false, any_t contextParam = any_t {})
{
    vector<ptr<const Item>> send_items;
    send_items.reserve(items.size());

    for (nptr<const Item> item : items) {
        if (item) {
            send_items.emplace_back(item.as_ptr());
        }
    }

    self->Send_SomeItems(send_items, owned, withInnerEntities, contextParam);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Disconnect(ptr<Critter> self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    if (nptr<Player> nullable_player = self->GetPlayer()) {
        auto player = nullable_player.as_ptr();
        player->GetConnection()->GracefulDisconnect();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_MakeControllable(ptr<Critter> self, bool controllable)
{
    if (controllable) {
        if (self->GetControlledByPlayer()) {
            return;
        }
    }
    else {
        if (!self->GetControlledByPlayer()) {
            return;
        }

        if (self->GetPlayer()) {
            self->DetachPlayer();
        }
    }

    if (self->GetMapId()) {
        auto map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");

        auto sync_ctx = self->GetEngine()->GetCurrentSyncContext();
        FO_VERIFY_AND_THROW(sync_ctx, "Missing script execution context");
        sync_ctx->EnsureEntitySynced(map.get());
    }

    if (controllable) {
        self->MarkIsForPlayer();
    }
    else {
        self->UnmarkIsForPlayer();
    }
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsOnline(ptr<Critter> self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    return !!self->GetPlayer();
}

static auto StartCritterMoveToHex(ptr<Critter> self, mpos hex, int32_t cut, ipos16 end_hex_offset, int32_t speed, ScriptFunc<bool, Critter*, Item*> gag_callback_func) -> refcount_ptr<MovingContext>
{
    FO_STACK_TRACE_ENTRY();

    auto engine_ptr = self->GetEngine();
    auto map_holder = RequireParent<Map>(self, "Critter is not on map");
    auto map_ptr = map_holder.as_ptr();

    ValidateEntityAccess(map_ptr);

    if (!map_ptr->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid target hex arg", hex, map_ptr->GetSize());
    }
    if (speed < 0 || speed > std::numeric_limits<uint16_t>::max()) {
        throw ScriptException("Speed arg out of range", speed);
    }

    self->StopMoving();

    if (speed == 0) {
        refcount_ptr<MovingContext> failed_moving = SafeAlloc::MakeRefCounted<MovingContext>(map_ptr->GetSize(), numeric_cast<uint16_t>(speed), vector<mdir> {}, vector<uint16_t> {}, nanotime {}, timespan {}, self->GetHex(), self->GetHexOffset(), self->GetHexOffset());
        failed_moving->Complete(MovingState::CantMove);
        return failed_moving;
    }

    function<bool(ptr<const Item>)> gag_callback;

    if (gag_callback_func) {
        gag_callback = [gag_cb = SafeAlloc::MakeShared<ScriptFunc<bool, Critter*, Item*>>(std::move(gag_callback_func)), self](ptr<const Item> gag) mutable { return gag_cb->Call(self.get(), const_cast<Item*>(std::addressof(*gag))) && gag_cb->GetResult(); };
    }

    const int16_t clamped_ox = std::clamp(end_hex_offset.x, numeric_cast<int16_t>(-GameSettings::MAP_HEX_WIDTH / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_WIDTH / 2));
    const int16_t clamped_oy = std::clamp(end_hex_offset.y, numeric_cast<int16_t>(-GameSettings::MAP_HEX_HEIGHT / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_HEIGHT / 2));
    const ipos16 clamped_offset = {clamped_ox, clamped_oy};
    ptr<const Map> map_const = map_ptr;
    const auto find_path = engine_ptr->MapMngr.FindPath(map_const, self, self->GetHex(), hex, self->GetMultihex(), cut, clamped_offset, std::move(gag_callback));

    if (find_path.Result != FindPathOutput::ResultType::Ok) {
        auto state = MovingState::GenericError;

        switch (find_path.Result) {
        case FindPathOutput::ResultType::AlreadyHere:
            state = MovingState::Success;
            break;
        case FindPathOutput::ResultType::TooFar:
            state = MovingState::HexTooFar;
            break;
        case FindPathOutput::ResultType::HexBusy:
            state = MovingState::HexBusy;
            break;
        case FindPathOutput::ResultType::NoWay:
            state = MovingState::Deadlock;
            break;
        case FindPathOutput::ResultType::TraceFailed:
            state = MovingState::TraceFailed;
            break;
        default:
            break;
        }

        refcount_ptr<MovingContext> failed_moving = SafeAlloc::MakeRefCounted<MovingContext>(map_ptr->GetSize(), numeric_cast<uint16_t>(speed), vector<mdir> {}, vector<uint16_t> {}, nanotime {}, timespan {}, self->GetHex(), self->GetHexOffset(), self->GetHexOffset());

        if (state == MovingState::HexBusy) {
            failed_moving->SetBlockHexes(self->GetHex(), hex);
        }

        failed_moving->Complete(state);
        return failed_moving;
    }

    refcount_ptr<MovingContext> moving = SafeAlloc::MakeRefCounted<MovingContext>(map_ptr->GetSize(), numeric_cast<uint16_t>(speed), find_path.Steps, find_path.ControlSteps, engine_ptr->GameTime.GetFrameTime(), timespan {}, self->GetHex(), self->GetHexOffset(), find_path.EndHexOffset);
    engine_ptr->StartCritterMoving(self, moving, nullptr);
    return moving;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API ptr<MovingContext> Server_Critter_MoveToHex(ptr<Critter> self, mpos hex, int32_t cut, int32_t speed, ScriptFunc<bool, Critter*, Item*> gagCallabck)
{
    auto moving = StartCritterMoveToHex(self, hex, cut, ipos16 {}, speed, std::move(gagCallabck));

    return ReleaseScriptOwnership(std::move(moving));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API ptr<MovingContext> Server_Critter_MoveToHex(ptr<Critter> self, mpos hex, int32_t cut, ipos16 endHexOffset, int32_t speed, ScriptFunc<bool, Critter*, Item*> gagCallabck)
{
    auto moving = StartCritterMoveToHex(self, hex, cut, endHexOffset, speed, std::move(gagCallabck));

    return ReleaseScriptOwnership(std::move(moving));
}

///@ ExportMethod
FO_SCRIPT_API MovingState Server_Critter_GetMovingState(ptr<Critter> self)
{
    return self->GetMovingState();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_StopMoving(ptr<Critter> self)
{
    self->GetEngine()->StopCritterMoving(self, MovingState::Stopped);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeMovingSpeed(ptr<Critter> self, int32_t speed)
{
    if (speed < 0 || speed > std::numeric_limits<uint16_t>::max()) {
        throw ScriptException("Speed arg out of range", speed);
    }

    self->GetEngine()->ChangeCritterMovingSpeed(self, numeric_cast<uint16_t>(speed));
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_AttachToCritter(ptr<Critter> self, ptr<Critter> cr)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot attach a critter that is being destroyed", self->GetId());
    }
    if (cr->IsDestroying()) {
        throw ScriptException("Cannot attach to a critter that is being destroyed", cr->GetId());
    }
    if (cr == self) {
        throw ScriptException("Critter can't attach to itself");
    }
    if (cr->GetMapId() != self->GetMapId()) {
        throw ScriptException("Different maps with attached critter");
    }
    if (cr->GetIsAttached()) {
        throw ScriptException("Can't attach to attached critter");
    }
    if (self->HasAttachedCritters()) {
        throw ScriptException("Can't attach with attachments");
    }

    if (self->GetIsAttached()) {
        if (self->GetAttachMaster() == cr->GetId()) {
            return;
        }

        // Auto detach from previous critter
        self->DetachFromCritter();
    }

    self->AttachToCritter(cr);
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
    cr->MoveAttachedCritters();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DetachFromCritter(ptr<Critter> self)
{
    if (!self->GetIsAttached()) {
        return;
    }

    self->DetachFromCritter();
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DetachAllCritters(ptr<Critter> self)
{
    for (ptr<Critter> cr : copy_hold_ref(self->GetAttachedCritters())) {
        cr->DetachFromCritter();
    }

    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetAttachedCritters(ptr<Critter> self)
{
    span<ptr<Critter>> attached_critters = self->GetAttachedCritters();
    return MakeScriptHandleVector<Critter>(attached_critters);
}

///@ ExportMethod
FO_SCRIPT_API timespan Server_Critter_GetPlayerOfflineTime(ptr<Critter> self)
{
    return self->GetOfflineTime();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RefreshVisibility(ptr<Critter> self)
{
    MapManager& map_mngr = self->GetEngine()->MapMngr;
    map_mngr.ProcessVisibleCritters(self);
    map_mngr.ProcessVisibleItems(self);
}

FO_END_NAMESPACE
