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
#include "Server.h"

FO_BEGIN_NAMESPACE

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetupScript(Critter* self, ScriptFunc<void, Critter*, bool> initFunc)
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
FO_SCRIPT_API void Server_Critter_SetupScriptEx(Critter* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsMoving(Critter* self)
{
    return self->IsMoving();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE MovingContext* Server_Critter_GetMovingContext(Critter* self)
{
    return self->GetMovingContext();
}

///@ ExportMethod
FO_SCRIPT_API uint32_t Server_Critter_GetMovingUid(Critter* self)
{
    return self->GetMovingUid();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Player* Server_Critter_GetPlayer(Critter* self)
{
    return self->GetPlayer();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Map* Server_Critter_GetMap(Critter* self)
{
    auto map = self->GetParent<Map>();
    return map.release_ownership();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToHex(Critter* self, mpos hex)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    auto map = self->GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    ValidateEntityAccess(map.get());

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    if (hex != self->GetHex()) {
        self->GetEngine()->MapMngr.TransferToMap(self, map.get(), hex, self->GetDir(), 2);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToHex(Critter* self, mpos hex, mdir dir)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    auto map = self->GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    ValidateEntityAccess(map.get());

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    if (hex != self->GetHex()) {
        if (self->GetDir() != dir) {
            self->ChangeDir(dir);
        }

        self->GetEngine()->MapMngr.TransferToMap(self, map.get(), hex, self->GetDir(), 2);
    }
    else if (self->GetDir() != dir) {
        self->ChangeDir(dir);
        self->Send_Dir(self);
        self->Broadcast_Dir();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(Critter* self, Map* map, mpos hex)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(map);
    ValidateEntityAccess(self->GetParentRaw().get());

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid target hex arg", hex, map->GetSize());
    }

    self->GetEngine()->MapMngr.TransferToMap(self, map, hex, self->GetDir(), 2);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(Critter* self, Map* map, mpos hex, mdir dir, bool preciseHex = false)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(map);
    ValidateEntityAccess(self->GetParentRaw().get());

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
FO_SCRIPT_API void Server_Critter_TransferToGlobal(Critter* self)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    if (!self->GetMapId()) {
        return;
    }

    ValidateEntityAccess(self->GetParentRaw().get());

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalWithGroup(Critter* self, readonly_vector<Critter*> group)
{
    if (self->IsMapTransfersLocked()) {
        throw ScriptException("Transfers locked");
    }

    ValidateEntityAccess(self->GetParentRaw().get());

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});

    for (auto* cr : group) {
        if (cr != nullptr && !cr->IsDestroyed() && !self->IsDestroyed() && !self->GetMapId()) {
            ValidateEntityAccess(cr);
            ValidateEntityAccess(cr->GetParentRaw().get());
            self->GetEngine()->MapMngr.TransferToGlobal(cr, self->GetId());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalGroup(Critter* self, Critter* globalCr)
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

    auto& map_mngr = self->GetEngine()->MapMngr;

    if (self->GetMapId()) {
        ValidateEntityAccess(self->GetParentRaw().get());
        map_mngr.TransferToGlobal(self, globalCr->GetId());
        return;
    }

    const auto& self_group = self->GetRawGlobalMapGroup();
    const auto& target_group = globalCr->GetRawGlobalMapGroup();

    if (self_group && target_group && self_group.get() == target_group.get()) {
        return;
    }

    ValidateEntityAccess(self->GetParentRaw().get());

    map_mngr.RemoveCritterFromMap(self, nullptr);

    if (self->IsDestroyed()) {
        return;
    }

    map_mngr.AddCritterToMap(self, nullptr, {}, mdir {}, globalCr->GetId());

    if (self->IsDestroyed()) {
        return;
    }

    for (const auto& member : self->GetGlobalMapGroup()) {
        if (member.get() != self) {
            self->Send_AddCritter(member.get());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsAlive(Critter* self)
{
    return self->IsAlive();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsKnockout(Critter* self)
{
    return self->IsKnockout();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsDead(Critter* self)
{
    return self->IsDead();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RefreshView(Critter* self)
{
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
    self->GetEngine()->MapMngr.ProcessVisibleItems(self);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetDir(Critter* self, mdir dir)
{
    if (dir == self->GetDir()) {
        return;
    }

    self->ChangeDir(dir);
    self->Send_Dir(self);
    self->Broadcast_Dir();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Critter* Server_Critter_GetCritter(Critter* self, ident_t id, CritterSeeType seeType)
{
    return self->GetCritter(id, seeType);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetCritters(Critter* self, CritterSeeType seeType, CritterFindType findType)
{
    if (self->GetMapId()) {
        auto map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map.get());

        auto critters = self->GetCritters(seeType, findType);

        std::ranges::stable_sort(critters, [hex = self->GetHex()](Critter* cr1, Critter* cr2) {
            const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
            const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
            return dist1 < dist2;
        });

        return critters;
    }

    return self->GetCritters(seeType, findType);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(Critter* self, Critter* cr)
{
    if (self == cr) {
        return true;
    }

    ValidateEntityAccess(cr);

    return self->IsSeeCritter(cr->GetId());
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSeenBy(Critter* self, Critter* cr)
{
    if (self == cr) {
        return true;
    }

    ValidateEntityAccess(cr);

    return cr->IsSeeCritter(self->GetId());
}

///@ ExportMethod
FO_SCRIPT_API CritterVisibilityMode Server_Critter_GetVisibilityMode(Critter* self, Critter* cr)
{
    if (self == cr) {
        return CritterVisibilityMode::Full;
    }

    ValidateEntityAccess(cr);

    return self->GetVisibleCritterMode(cr->GetId());
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(Critter* self, Item* item)
{
    ValidateEntityAccess(item);

    return self->CheckVisibleItem(item->GetId());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Critter_CountItem(Critter* self, hstring protoId)
{
    return self->CountInvItemByPid(protoId);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Critter_CountItem(Critter* self, ProtoItem* proto)
{
    return self->CountInvItemByPid(proto->GetProtoId());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(Critter* self, hstring pid)
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
FO_SCRIPT_API void Server_Critter_DestroyItem(Critter* self, ProtoItem* proto)
{
    const auto count = self->CountInvItemByPid(proto->GetProtoId());

    if (count == 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, proto->GetProtoId(), count);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(Critter* self, hstring pid, int32_t count)
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
FO_SCRIPT_API void Server_Critter_DestroyItem(Critter* self, ProtoItem* proto, int32_t count)
{
    if (count <= 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, proto->GetProtoId(), count);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_AddItem(Critter* self, hstring pid, int32_t count)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a critter that is being destroyed", self->GetId());
    }
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }
    if (self->GetEngine()->GetProtoItem(pid) == nullptr) {
        throw ScriptException("Invalid proto", pid);
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    return self->GetEngine()->ItemMngr.AddItemCritter(self, pid, count);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_AddItem(Critter* self, ProtoItem* proto, int32_t count)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a critter that is being destroyed", self->GetId());
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    return self->GetEngine()->ItemMngr.AddItemCritter(self, proto->GetProtoId(), count);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Critter_GetItem(Critter* self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    return self->GetInvItem(itemId);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Critter_GetItem(Critter* self, hstring protoId)
{
    return self->GetItemByPidInvPriority(protoId);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Critter_GetItem(Critter* self, ProtoItem* proto)
{
    return self->GetItemByPidInvPriority(proto->GetProtoId());
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Critter_GetItem(Critter* self, ItemProperty property, int32_t propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    for (auto& item : self->GetInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self)
{
    return vec_transform(self->GetInvItems(), [](auto&& item) -> Item* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, ItemProperty property, int32_t propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    auto items = self->GetInvItems();

    vector<Item*> result;
    result.reserve(items.size());

    for (auto& item : items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.push_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, hstring protoId)
{
    auto items = self->GetInvItems();

    vector<Item*> result;
    result.reserve(items.size());

    for (auto& item : items) {
        if (item->GetProtoId() == protoId) {
            result.push_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, ProtoItem* proto)
{
    if (proto == nullptr) {
        throw ScriptException("Item proto arg is null");
    }

    auto items = self->GetInvItems();

    vector<Item*> result;
    result.reserve(items.size());

    for (auto& item : items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            result.push_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeItemSlot(Critter* self, ident_t itemId, CritterItemSlot slot)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    refcount_ptr self_holder = self;
    ignore_unused(self_holder);

    auto* item = self->GetInvItem(itemId);

    if (item == nullptr) {
        throw ScriptException("Item not found");
    }

    refcount_ptr item_holder = item;
    ignore_unused(item_holder);

    // To slot arg is equal of current item slot
    if (item->GetCritterSlot() == slot) {
        return;
    }

    if (static_cast<size_t>(slot) >= self->GetEngine()->Settings.CritterSlotEnabled.size() || !self->GetEngine()->Settings.CritterSlotEnabled[static_cast<size_t>(slot)]) {
        throw ScriptException("Slot is not allowed");
    }

    const auto is_multi_item_allowed = static_cast<size_t>(slot) < self->GetEngine()->Settings.CritterSlotMultiItem.size() && self->GetEngine()->Settings.CritterSlotMultiItem[static_cast<size_t>(slot)];

    if (is_multi_item_allowed) {
        const auto from_slot = item->GetCritterSlot();

        item->SetCritterSlot(slot);

        self->SendAndBroadcast_MoveItem(item, CritterAction::MoveItem, from_slot);

        ValidateEntityAccess(self);
        ValidateEntityAccess(item);
        self->GetEngine()->OnCritterItemMoved.Fire(self, item, from_slot);
    }
    else {
        auto* item_swap = self->GetInvItemBySlot(slot);
        const auto from_slot = item->GetCritterSlot();

        item->SetCritterSlot(slot);

        if (item_swap != nullptr) {
            item_swap->SetCritterSlot(from_slot);
        }

        self->SendAndBroadcast_MoveItem(item, CritterAction::MoveItem, from_slot);

        if (item_swap != nullptr) {
            refcount_ptr item_swap_holder = item_swap;
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

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetCondition(Critter* self, CritterCondition cond, CritterActionAnim actionAnim, FO_NULLABLE AbstractItem* contextItem)
{
    ValidateEntityAccess(contextItem);

    const auto prev_cond = self->GetCondition();

    if (prev_cond == cond) {
        return;
    }

    refcount_ptr<Map> map;

    if (self->GetMapId()) {
        map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");

        ValidateEntityAccess(map.get());

        map->RemoveCritterFromField(self);
    }

    self->SetCondition(cond);

    if (map) {
        map->AddCritterToField(self);
    }

    if (cond == CritterCondition::Dead) {
        self->SendAndBroadcast_Action(CritterAction::Dead, static_cast<int32_t>(actionAnim), dynamic_cast<Item*>(contextItem));
    }
    else if (cond == CritterCondition::Knockout) {
        self->SendAndBroadcast_Action(CritterAction::Knockout, static_cast<int32_t>(actionAnim), dynamic_cast<Item*>(contextItem));
    }
    else if (cond == CritterCondition::Alive) {
        if (prev_cond == CritterCondition::Knockout) {
            self->SendAndBroadcast_Action(CritterAction::StandUp, static_cast<int32_t>(actionAnim), dynamic_cast<Item*>(contextItem));
        }
        else {
            self->SendAndBroadcast_Action(CritterAction::Respawn, static_cast<int32_t>(actionAnim), dynamic_cast<Item*>(contextItem));
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Action(Critter* self, CritterAction action, int32_t actionData, FO_NULLABLE AbstractItem* contextItem)
{
    ValidateEntityAccess(contextItem);

    self->SendAndBroadcast_Action(action, actionData, dynamic_cast<Item*>(contextItem));
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SendItems(Critter* self, readonly_vector<Item*> items, bool owned = false, bool withInnerEntities = false, any_t contextParam = any_t {})
{
    for (auto* item : items) {
        ValidateEntityAccess(item);
    }

    self->Send_SomeItems(items, owned, withInnerEntities, contextParam);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Disconnect(Critter* self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    if (auto* player = self->GetPlayer(); player != nullptr) {
        player->GetConnection()->GracefulDisconnect();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_MakeControllable(Critter* self, bool controllable)
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

        if (self->GetPlayer() != nullptr) {
            self->DetachPlayer();
        }
    }

    if (self->GetMapId()) {
        auto map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        EnsureEntitySynced(map.get());
    }

    if (controllable) {
        self->MarkIsForPlayer();
    }
    else {
        self->UnmarkIsForPlayer();
    }
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsOnline(Critter* self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    return self->GetPlayer() != nullptr;
}

static auto StartCritterMoveToHex(Critter* self, mpos hex, int32_t cut, ipos16 end_hex_offset, int32_t speed, ScriptFunc<bool, Critter*, Item*> gag_callback_func) -> MovingContext*
{
    auto* engine = self->GetEngine();
    auto map = self->GetParent<Map>();

    if (!map) {
        throw ScriptException("Critter is not on map");
    }

    ValidateEntityAccess(map.get());

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
        return failed_moving.release_ownership();
    }

    function<bool(const Item*)> gag_callback;

    if (gag_callback_func) {
        gag_callback = [gag_cb = SafeAlloc::MakeShared<ScriptFunc<bool, Critter*, Item*>>(std::move(gag_callback_func)), self](const Item* gag) mutable {
            auto* gag_non_const = const_cast<Item*>(gag);
            return gag_cb->Call(self, gag_non_const) && gag_cb->GetResult();
        };
    }

    const int16_t clamped_ox = std::clamp(end_hex_offset.x, numeric_cast<int16_t>(-GameSettings::MAP_HEX_WIDTH / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_WIDTH / 2));
    const int16_t clamped_oy = std::clamp(end_hex_offset.y, numeric_cast<int16_t>(-GameSettings::MAP_HEX_HEIGHT / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_HEIGHT / 2));
    const ipos16 clamped_offset = {clamped_ox, clamped_oy};
    const auto find_path = engine->MapMngr.FindPath(map.get(), self, self->GetHex(), hex, self->GetMultihex(), cut, clamped_offset, std::move(gag_callback));

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
        return failed_moving.release_ownership();
    }

    auto moving = SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), numeric_cast<uint16_t>(speed), find_path.Steps, find_path.ControlSteps, engine->GameTime.GetFrameTime(), timespan {}, self->GetHex(), self->GetHexOffset(), find_path.EndHexOffset);
    engine->StartCritterMoving(self, moving, nullptr);
    return moving.release_ownership();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API MovingContext* Server_Critter_MoveToHex(Critter* self, mpos hex, int32_t cut, int32_t speed, ScriptFunc<bool, Critter*, Item*> gagCallabck)
{
    return StartCritterMoveToHex(self, hex, cut, ipos16 {}, speed, std::move(gagCallabck));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API MovingContext* Server_Critter_MoveToHex(Critter* self, mpos hex, int32_t cut, ipos16 endHexOffset, int32_t speed, ScriptFunc<bool, Critter*, Item*> gagCallabck)
{
    return StartCritterMoveToHex(self, hex, cut, endHexOffset, speed, std::move(gagCallabck));
}

///@ ExportMethod
FO_SCRIPT_API MovingState Server_Critter_GetMovingState(Critter* self)
{
    return self->GetMovingState();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_StopMoving(Critter* self)
{
    self->GetEngine()->StopCritterMoving(self, MovingState::Stopped);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeMovingSpeed(Critter* self, int32_t speed)
{
    if (speed < 0 || speed > std::numeric_limits<uint16_t>::max()) {
        throw ScriptException("Speed arg out of range", speed);
    }

    self->GetEngine()->ChangeCritterMovingSpeed(self, numeric_cast<uint16_t>(speed));
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_AttachToCritter(Critter* self, Critter* cr)
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

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DetachFromCritter(Critter* self)
{
    if (!self->GetIsAttached()) {
        return;
    }

    self->DetachFromCritter();
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DetachAllCritters(Critter* self)
{
    for (auto* cr : copy_hold_ref(self->GetAttachedCritters())) {
        cr->DetachFromCritter();
    }

    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetAttachedCritters(Critter* self)
{
    return vec_transform(self->GetAttachedCritters(), [](auto&& cr) -> Critter* { return cr.get(); });
}

///@ ExportMethod
FO_SCRIPT_API timespan Server_Critter_GetPlayerOfflineTime(Critter* self)
{
    return self->GetOfflineTime();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RefreshVisibility(Critter* self)
{
    auto& mapMngr = self->GetEngine()->MapMngr;
    mapMngr.ProcessVisibleCritters(self);
    mapMngr.ProcessVisibleItems(self);
}

FO_END_NAMESPACE
