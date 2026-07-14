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

// SyncScope: requires self; init callback runs under the same cover and must widen before touching other entities.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetupScript(ptr<Critter> self, ScriptFunc<void, ptr<Critter>, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }

    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

// SyncScope: requires self; init callback runs under the same cover and must widen before touching other entities.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetupScriptEx(ptr<Critter> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

// SyncScope: requires self; reads current movement flag only.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsMoving(ptr<Critter> self)
{
    return self->IsMoving();
}

// SyncScope: requires self; returned movement context is a snapshot handle for self's active move.
///@ ExportMethod
FO_SCRIPT_API nptr<MovingContext> Server_Critter_GetMovingContext(ptr<Critter> self)
{
    auto moving = self->GetMovingContext();
    return moving;
}

// SyncScope: requires self; reads current movement uid only.
///@ ExportMethod
FO_SCRIPT_API uint32_t Server_Critter_GetMovingUid(ptr<Critter> self)
{
    return self->GetMovingUid();
}

// SyncScope: requires self; returns the auto-widen partner Player when one is attached.
///@ ExportMethod
FO_SCRIPT_API nptr<Player> Server_Critter_GetPlayer(ptr<Critter> self)
{
    auto player = self->GetPlayer();
    return player;
}

// SyncScope: requires self; returns the current parent map handle, but does not cover it for later reads.
///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Map> Server_Critter_GetMap(ptr<Critter> self)
{
    auto map = self->GetParent<Map>();

    return map ? map.take_not_null().release_ownership() : nullptr;
}

// SyncScope: requires self + current map; transfer keeps self covered and mutates current-map placement.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToHex(ptr<Critter> self, mpos hex)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    auto map = self->GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    ValidateEntityAccess(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    if (hex != self->GetHex()) {
        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, self->GetDir(), 2);
    }
}

// SyncScope: requires self + current map; transfer keeps self covered and mutates current-map placement/dir.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToHex(ptr<Critter> self, mpos hex, mdir dir)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    auto map = self->GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");

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

// SyncScope: requires self + source map if mapped + destination map + destination location; scripts should use Sync::LockForTransferToMap(self, map).
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(ptr<Critter> self, ptr<Map> map, mpos hex)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(map);
    ValidateEntityAccess(self->GetParentRaw());

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid target hex arg", hex, map->GetSize());
    }

    self->GetEngine()->MapMngr.TransferToMap(self, map, hex, self->GetDir(), 2);
}

// SyncScope: requires self + source map if mapped + destination map + destination location; scripts should use Sync::LockForTransferToMap(self, map).
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(ptr<Critter> self, ptr<Map> map, mpos hex, mdir dir, bool preciseHex = false)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(map);
    ValidateEntityAccess(self->GetParentRaw());

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

// SyncScope: requires self + current source map when self is mapped; mutates self to global-map state.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobal(ptr<Critter> self)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    if (!self->GetMapId()) {
        return;
    }

    ValidateEntityAccess(self->GetParentRaw());

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});
}

// SyncScope: requires self + current source map and every group critter + its current source map before transfer.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalWithGroup(ptr<Critter> self, readonly_vector<nptr<Critter>> group)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(self->GetParentRaw());

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});

    for (auto cr : group) {
        if (!cr) {
            continue;
        }

        if (!cr->IsDestroyed() && !self->IsDestroyed() && !self->GetMapId()) {
            ValidateEntityAccess(cr);
            ValidateEntityAccess(cr->GetParentRaw());
            self->GetEngine()->MapMngr.TransferToGlobal(cr, self->GetId());
        }
    }
}

// SyncScope: requires self + globalCr; if self is mapped also requires self's current source map.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalGroup(ptr<Critter> self, ptr<Critter> globalCr)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(globalCr);

    if (globalCr->GetMapId()) {
        throw ScriptException("Global critter is not on global map");
    }
    if (self == globalCr) {
        throw ScriptException("Cannot join own group");
    }

    MapManager& map_mngr = self->GetEngine()->MapMngr;

    if (self->GetMapId()) {
        ValidateEntityAccess(self->GetParentRaw());
        map_mngr.TransferToGlobal(self, globalCr->GetId());
        return;
    }

    const auto& self_group = self->GetRawGlobalMapGroup();
    const auto& target_group = globalCr->GetRawGlobalMapGroup();

    if (self_group && target_group && self_group == target_group) {
        return;
    }

    ValidateEntityAccess(self->GetParentRaw());

    map_mngr.RemoveCritterFromMap(self, nullptr);

    if (self->IsDestroyed()) {
        return;
    }

    map_mngr.AddCritterToMap(self, nullptr, {}, mdir {}, globalCr->GetId());

    if (self->IsDestroyed()) {
        return;
    }

    for (auto member : self->GetGlobalMapGroup()) {
        if (!(member == self)) {
            self->Send_AddCritter(member);
        }
    }
}

// SyncScope: requires self; reads current condition state only.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsAlive(ptr<Critter> self)
{
    return self->IsAlive();
}

// SyncScope: requires self; reads current condition state only.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsKnockout(ptr<Critter> self)
{
    return self->IsKnockout();
}

// SyncScope: requires self; reads current condition state only.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsDead(ptr<Critter> self)
{
    return self->IsDead();
}

// SyncScope: requires self + current map when mapped; recomputes visible critters/items.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RefreshView(ptr<Critter> self)
{
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
    self->GetEngine()->MapMngr.ProcessVisibleItems(self);
}

// SyncScope: requires self; mutates direction and broadcasts it.
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

// SyncScope: requires self; observer lookup may need self's map covered when self is mapped.
///@ ExportMethod
FO_SCRIPT_API nptr<Critter> Server_Critter_GetCritter(ptr<Critter> self, ident_t id, CritterSeeType seeType)
{
    auto cr = self->GetCritter(id, seeType);
    return cr;
}

// SyncScope: requires self and current map when mapped; returned critters are covered only while that cover remains.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<Critter>> Server_Critter_GetCritters(ptr<Critter> self, CritterSeeType seeType, CritterFindType findType)
{
    if (self->GetMapId()) {
        auto map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map);

        vector<ptr<Critter>> critters = self->GetCritters(seeType, findType);

        std::ranges::stable_sort(critters, [hex = self->GetHex()](ptr<const Critter> cr1, ptr<const Critter> cr2) {
            const int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
            const int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
            return dist1 < dist2;
        });

        return critters;
    }

    return self->GetCritters(seeType, findType);
}

// SyncScope: requires self + cr; checks self's visibility cache for a covered critter.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(ptr<Critter> self, ptr<Critter> cr)
{
    if (self == cr) {
        return true;
    }

    ValidateEntityAccess(cr);

    return self->IsSeeCritter(cr->GetId());
}

// SyncScope: requires self + cr; checks cr's visibility cache for this covered critter.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSeenBy(ptr<Critter> self, ptr<Critter> cr)
{
    if (self == cr) {
        return true;
    }

    ValidateEntityAccess(cr);

    return cr->IsSeeCritter(self->GetId());
}

// SyncScope: requires self + cr; reads self's visibility-mode cache for a covered critter.
///@ ExportMethod
FO_SCRIPT_API CritterVisibilityMode Server_Critter_GetVisibilityMode(ptr<Critter> self, ptr<Critter> cr)
{
    if (self == cr) {
        return CritterVisibilityMode::Full;
    }

    ValidateEntityAccess(cr);

    return self->GetVisibleCritterMode(cr->GetId());
}

// SyncScope: requires self + item; checks self's visible-item cache for a covered item.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(ptr<Critter> self, ptr<Item> item)
{
    ValidateEntityAccess(item);

    return self->CheckVisibleItem(item->GetId());
}

// SyncScope: requires self; counts matching inventory items under self's inventory cover.
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Critter_CountItem(ptr<Critter> self, hstring protoId)
{
    return self->CountInvItemByPid(protoId);
}

// SyncScope: requires self; counts matching inventory items under self's inventory cover.
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Critter_CountItem(ptr<Critter> self, ptr<ProtoItem> proto)
{
    return self->CountInvItemByPid(proto->GetProtoId());
}

// SyncScope: requires self; destroys matching inventory items under self's holder cover.
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

// SyncScope: requires self; destroys matching inventory items under self's holder cover.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(ptr<Critter> self, ptr<ProtoItem> proto)
{
    const auto count = self->CountInvItemByPid(proto->GetProtoId());

    if (count == 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, proto->GetProtoId(), count);
}

// SyncScope: requires self; destroys matching inventory item count under self's holder cover.
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

// SyncScope: requires self; destroys matching inventory item count under self's holder cover.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(ptr<Critter> self, ptr<ProtoItem> proto, int32_t count)
{
    if (count <= 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, proto->GetProtoId(), count);
}

// SyncScope: requires self; creates and attaches a new inventory item under self's cover.
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
    return item;
}

// SyncScope: requires self; creates and attaches a new inventory item under self's cover.
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
    return item;
}

// SyncScope: requires self; returned inventory item is covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    auto item = self->GetInvItem(itemId);
    return item;
}

// SyncScope: requires self; returned inventory item is covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, hstring protoId)
{
    return self->GetItemByPidInvPriority(protoId);
}

// SyncScope: requires self; returned inventory item is covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, ptr<ProtoItem> proto)
{
    return self->GetItemByPidInvPriority(proto->GetProtoId());
}

// SyncScope: requires self; returned inventory item is covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Critter_GetItem(ptr<Critter> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    for (auto item : self->GetInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

// SyncScope: requires self; returned inventory items are covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<Item>> Server_Critter_GetItems(ptr<Critter> self)
{
    vector<ptr<Item>> items = self->GetInvItems();
    return items;
}

// SyncScope: requires self; returned inventory items are covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<Item>> Server_Critter_GetItems(ptr<Critter> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    vector<ptr<Item>> items = self->GetInvItems();

    vector<ptr<Item>> result;
    result.reserve(items.size());

    for (auto item : items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.push_back(item);
        }
    }

    return result;
}

// SyncScope: requires self; returned inventory items are covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<Item>> Server_Critter_GetItems(ptr<Critter> self, hstring protoId)
{
    vector<ptr<Item>> items = self->GetInvItems();

    vector<ptr<Item>> result;
    result.reserve(items.size());

    for (auto item : items) {
        if (item->GetProtoId() == protoId) {
            result.push_back(item);
        }
    }

    return result;
}

// SyncScope: requires self; returned inventory items are covered by self while the cover remains.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<Item>> Server_Critter_GetItems(ptr<Critter> self, ptr<ProtoItem> proto)
{
    vector<ptr<Item>> items = self->GetInvItems();

    vector<ptr<Item>> result;
    result.reserve(items.size());

    for (auto item : items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            result.push_back(item);
        }
    }

    return result;
}

// SyncScope: requires self + inventory item; mutates item slot and fires movement/equipment events.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeItemSlot(ptr<Critter> self, ident_t itemId, CritterItemSlot slot)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto self_holder = self.hold_ref();
    ignore_unused(self_holder);

    auto item = self->GetInvItem(itemId);

    if (!item) {
        throw ScriptException("Item not found");
    }

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
        self->GetEngine()->OnCritterItemMoved.Fire(self, item, from_slot);
    }
    else {
        auto item_swap = self->GetInvItemBySlot(slot);
        const auto from_slot = item->GetCritterSlot();

        item->SetCritterSlot(slot);

        if (item_swap) {
            item_swap->SetCritterSlot(from_slot);
        }

        self->SendAndBroadcast_MoveItem(item, CritterAction::MoveItem, from_slot);

        if (item_swap) {
            refcount_ptr item_swap_holder = item_swap.hold_ref();
            ignore_unused(item_swap_holder);

            ValidateEntityAccess(self);
            ValidateEntityAccess(item_swap);
            self->GetEngine()->OnCritterItemMoved.Fire(self, item_swap, slot);
        }

        // ValidateEntityAccess and event dispatch tolerate destroyed entity arguments.
        ValidateEntityAccess(self);
        ValidateEntityAccess(item);
        self->GetEngine()->OnCritterItemMoved.Fire(self, item, from_slot);
    }
}

// SyncScope: requires self + current map when mapped + context item when non-null; mutates condition and map fields.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetCondition(ptr<Critter> self, CritterCondition cond, CritterActionAnim actionAnim, nptr<AbstractItem> contextItem)
{
    ValidateEntityAccess(contextItem);

    const auto prev_cond = self->GetCondition();

    if (prev_cond == cond) {
        return;
    }

    refcount_nptr<Map> map {};

    if (self->GetMapId()) {
        map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");

        ValidateEntityAccess(map);

        map->RemoveCritterFromField(self);
    }

    self->SetCondition(cond);
    const auto context_item = contextItem.dyn_cast<const Item>();

    if (map) {
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

// SyncScope: requires self + context item when non-null; sends/broadcasts action only.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Action(ptr<Critter> self, CritterAction action, int32_t actionData, nptr<AbstractItem> contextItem)
{
    ValidateEntityAccess(contextItem);

    self->SendAndBroadcast_Action(action, actionData, contextItem.dyn_cast<const Item>());
}

// SyncScope: requires self + every non-null item in items; sends a network payload without changing cover.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SendItems(ptr<Critter> self, readonly_vector<nptr<Item>> items, bool owned = false, bool withInnerEntities = false, any_t contextParam = any_t {})
{
    vector<ptr<const Item>> send_items;
    send_items.reserve(items.size());

    for (nptr<const Item> item : items) {
        ValidateEntityAccess(item);

        if (item) {
            send_items.emplace_back(item);
        }
    }

    self->Send_SomeItems(send_items, owned, withInnerEntities, contextParam);
}

// SyncScope: requires self + its auto-widened Player link; closes that player's connection.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Disconnect(ptr<Critter> self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    if (auto player = self->GetPlayer()) {
        player->GetConnection()->GracefulDisconnect();
    }
}

// SyncScope: requires self + current map; changes controllable/player state and may self-sync map.
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
        ValidateEntityAccess(map);
    }

    if (controllable) {
        self->MarkIsForPlayer();
    }
    else {
        self->UnmarkIsForPlayer();
    }
}

// SyncScope: requires self + its auto-widened Player link; reads online state only.
///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsOnline(ptr<Critter> self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    return !!self->GetPlayer();
}

static auto StartCritterMoveToHex(ptr<Critter> self, mpos hex, int32_t cut, ipos16 end_hex_offset, int32_t speed, ScriptFunc<bool, ptr<Critter>, ptr<Item>> gag_callback_func) -> refcount_ptr<MovingContext>
{
    FO_STACK_TRACE_ENTRY();

    auto engine = self->GetEngine();
    auto map = RequireParent<Map>(self, "Critter is not on map");

    ValidateEntityAccess(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid target hex arg", hex, map->GetSize());
    }
    if (speed < 0 || speed > std::numeric_limits<uint16_t>::max()) {
        throw ScriptException("Speed arg out of range", speed);
    }

    self->StopMoving();

    if (speed == 0) {
        auto failed_moving = SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), numeric_cast<uint16_t>(speed), vector<mdir> {}, vector<uint16_t> {}, nanotime {}, timespan {}, self->GetHex(), self->GetHexOffset(), self->GetHexOffset());
        failed_moving->Complete(MovingState::CantMove);
        return failed_moving;
    }

    function<bool(ptr<const Item>)> gag_callback;

    if (gag_callback_func) {
        gag_callback = [gag_cb = SafeAlloc::MakeShared<ScriptFunc<bool, ptr<Critter>, ptr<Item>>>(std::move(gag_callback_func)), self](ptr<const Item> gag) mutable { return gag_cb->Call(self, make_ptr(const_cast<Item*>(std::addressof(*gag)))) && gag_cb->GetResult(); };
    }

    const int16_t clamped_ox = std::clamp(end_hex_offset.x, numeric_cast<int16_t>(-GameSettings::MAP_HEX_WIDTH / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_WIDTH / 2));
    const int16_t clamped_oy = std::clamp(end_hex_offset.y, numeric_cast<int16_t>(-GameSettings::MAP_HEX_HEIGHT / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_HEIGHT / 2));
    const ipos16 clamped_offset = {clamped_ox, clamped_oy};
    const auto find_path = engine->MapMngr.FindPath(map, self, self->GetHex(), hex, self->GetMultihex(), cut, clamped_offset, std::move(gag_callback));

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

        auto failed_moving = SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), numeric_cast<uint16_t>(speed), vector<mdir> {}, vector<uint16_t> {}, nanotime {}, timespan {}, self->GetHex(), self->GetHexOffset(), self->GetHexOffset());

        if (state == MovingState::HexBusy) {
            failed_moving->SetBlockHexes(self->GetHex(), hex);
        }

        failed_moving->Complete(state);
        return failed_moving;
    }

    auto moving = SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), numeric_cast<uint16_t>(speed), find_path.Steps, find_path.ControlSteps, engine->GameTime.GetFrameTime(), timespan {}, self->GetHex(), self->GetHexOffset(), find_path.EndHexOffset);
    engine->StartCritterMoving(self, moving, nullptr);
    return moving;
}

// SyncScope: requires self + current map; starts movement and returns the new movement context.
///@ ExportMethod PassOwnership
FO_SCRIPT_API ptr<MovingContext> Server_Critter_MoveToHex(ptr<Critter> self, mpos hex, int32_t cut, int32_t speed, ScriptFunc<bool, ptr<Critter>, ptr<Item>> gagCallabck)
{
    auto moving = StartCritterMoveToHex(self, hex, cut, ipos16 {}, speed, std::move(gagCallabck));

    return moving.release_ownership();
}

// SyncScope: requires self + current map; starts movement and returns the new movement context.
///@ ExportMethod PassOwnership
FO_SCRIPT_API ptr<MovingContext> Server_Critter_MoveToHex(ptr<Critter> self, mpos hex, int32_t cut, ipos16 endHexOffset, int32_t speed, ScriptFunc<bool, ptr<Critter>, ptr<Item>> gagCallabck)
{
    auto moving = StartCritterMoveToHex(self, hex, cut, endHexOffset, speed, std::move(gagCallabck));

    return moving.release_ownership();
}

// SyncScope: requires self; reads current movement state only.
///@ ExportMethod
FO_SCRIPT_API MovingState Server_Critter_GetMovingState(ptr<Critter> self)
{
    return self->GetMovingState();
}

// SyncScope: requires self; stops current movement.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_StopMoving(ptr<Critter> self)
{
    self->GetEngine()->StopCritterMoving(self, MovingState::Stopped);
}

// SyncScope: requires self; changes current movement speed.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeMovingSpeed(ptr<Critter> self, int32_t speed)
{
    if (speed < 0 || speed > std::numeric_limits<uint16_t>::max()) {
        throw ScriptException("Speed arg out of range", speed);
    }

    self->GetEngine()->ChangeCritterMovingSpeed(self, numeric_cast<uint16_t>(speed));
}

// SyncScope: requires self + cr, and both must be on the same covered map.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_AttachToCritter(ptr<Critter> self, ptr<Critter> cr)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot attach a critter that is being destroyed", self->GetId());
    }
    if (cr->IsDestroying()) {
        throw ScriptException("Cannot attach to a critter that is being destroyed", cr->GetId());
    }

    ValidateEntityAccess(cr);

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

// SyncScope: requires self + current map; detaches self and refreshes visibility.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DetachFromCritter(ptr<Critter> self)
{
    if (!self->GetIsAttached()) {
        return;
    }

    self->DetachFromCritter();
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

// SyncScope: requires self and attached critters; detaches each attached critter and refreshes visibility.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DetachAllCritters(ptr<Critter> self)
{
    for (auto cr : copy_hold_ref(self->GetAttachedCritters())) {
        cr->DetachFromCritter();
    }

    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

// SyncScope: requires self; returned attached critters need their own cover before mutation.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<Critter>> Server_Critter_GetAttachedCritters(ptr<Critter> self)
{
    span<ptr<Critter>> attached_critters = self->GetAttachedCritters();
    return vector<ptr<Critter>>(attached_critters.begin(), attached_critters.end());
}

// SyncScope: requires self; reads offline state for this critter's player link only.
///@ ExportMethod
FO_SCRIPT_API timespan Server_Critter_GetPlayerOfflineTime(ptr<Critter> self)
{
    return self->GetOfflineTime();
}

// SyncScope: requires self + current map when mapped; recomputes visible critters/items.
///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RefreshVisibility(ptr<Critter> self)
{
    MapManager& map_mngr = self->GetEngine()->MapMngr;
    map_mngr.ProcessVisibleCritters(self);
    map_mngr.ProcessVisibleItems(self);
}

FO_END_NAMESPACE
