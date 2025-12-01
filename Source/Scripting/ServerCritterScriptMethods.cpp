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

#include "Common.h"

#include "Geometry.h"
#include "Server.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetupScript(Critter* self, InitFunc<Critter*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetupScriptEx(Critter* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsMoving(Critter* self)
{
    return self->IsMoving() || self->TargetMoving.State == MovingState::InProgress;
}

///@ ExportMethod
FO_SCRIPT_API Player* Server_Critter_GetPlayer(Critter* self)
{
    return self->GetPlayer();
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Critter_GetMap(Critter* self)
{
    return self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToHex(Critter* self, mpos hex, uint8 dir)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }

    auto* map = self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
    FO_RUNTIME_ASSERT(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    auto corrected_dir = dir;
    if (corrected_dir >= GameSettings::MAP_DIR_COUNT) {
        corrected_dir = self->GetDir();
    }

    if (hex != self->GetHex()) {
        if (self->GetDir() != corrected_dir) {
            self->ChangeDir(corrected_dir);
        }

        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, self->GetDir(), 2);
    }
    else if (self->GetDir() != corrected_dir) {
        self->ChangeDir(corrected_dir);
        self->Send_Dir(self);
        self->Broadcast_Dir();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(Critter* self, Map* map, mpos hex, uint8 dir)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }

    auto corrected_dir = dir;

    if (corrected_dir >= GameSettings::MAP_DIR_COUNT) {
        corrected_dir = self->GetDir();
    }

    self->GetEngine()->MapMngr.TransferToMap(self, map, hex, corrected_dir, 2);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToMap(Critter* self, Map* map, mpos hex, uint8 dir, bool force_hex)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }

    auto corrected_dir = dir;

    if (corrected_dir >= GameSettings::MAP_DIR_COUNT) {
        corrected_dir = self->GetDir();
    }

    if (force_hex) {
        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, corrected_dir, std::nullopt);
    }
    else {
        self->GetEngine()->MapMngr.TransferToMap(self, map, hex, corrected_dir, 2);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobal(Critter* self)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }

    if (!self->GetMapId()) {
        return;
    }

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalWithGroup(Critter* self, const vector<Critter*>& group)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }

    self->GetEngine()->MapMngr.TransferToGlobal(self, {});

    for (auto* cr : group) {
        if (cr != nullptr && !cr->IsDestroyed() && !self->IsDestroyed() && !self->GetMapId()) {
            self->GetEngine()->MapMngr.TransferToGlobal(cr, self->GetId());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransferToGlobalGroup(Critter* self, Critter* globalCr)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }
    if (!self->GetMapId()) {
        throw ScriptException("Critter already on global");
    }
    if (globalCr == nullptr) {
        throw ScriptException("Global critter arg is null");
    }
    if (globalCr->GetMapId()) {
        throw ScriptException("Global critter is not on global map");
    }

    self->GetEngine()->MapMngr.TransferToGlobal(self, globalCr->GetId());
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
FO_SCRIPT_API void Server_Critter_ViewMap(Critter* self, Map* map, int32 look, mpos hex, uint8 dir)
{
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    if (!self->GetControlledByPlayer()) {
        return;
    }

    auto dir_ = dir;

    if (dir_ >= GameSettings::MAP_DIR_COUNT) {
        dir_ = self->GetDir();
    }

    auto look_ = look;

    if (look_ == 0) {
        look_ = self->GetLookDistance();
    }

    self->ViewMapId = map->GetId();
    self->ViewMapPid = map->GetProtoId();
    self->ViewMapLook = numeric_cast<uint16>(look_);
    self->ViewMapHex = hex;
    self->ViewMapDir = dir_;
    self->ViewMapLocId = ident_t {};
    self->ViewMapLocEnt = 0;
    self->Send_LoadMap(map);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetDir(Critter* self, uint8 dir)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid direction arg");
    }

    if (self->GetDir() == dir) {
        return;
    }

    self->ChangeDir(dir);
    self->Send_Dir(self);
    self->Broadcast_Dir();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetDirAngle(Critter* self, int16 dir_angle)
{
    const auto normalized_dir_angle = GeometryHelper::NormalizeAngle(dir_angle);

    if (self->GetDirAngle() == normalized_dir_angle) {
        return;
    }

    self->ChangeDirAngle(normalized_dir_angle);
    self->Send_Dir(self);
    self->Broadcast_Dir();
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Critter_GetCritter(Critter* self, ident_t id, CritterSeeType seeType)
{
    return self->GetCritter(id, seeType);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetCritters(Critter* self, CritterSeeType seeType, CritterFindType findType)
{
    auto critters = self->GetCritters(seeType, findType);

    if (self->GetMapId()) {
        std::ranges::stable_sort(critters, [hex = self->GetHex()](Critter* cr1, Critter* cr2) {
            const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
            const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
            return dist1 < dist2;
        });
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(Critter* self, Critter* cr)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (self == cr) {
        return true;
    }

    return self->IsSeeCritter(cr->GetId());
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSeenBy(Critter* self, Critter* cr)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (self == cr) {
        return true;
    }

    return cr->IsSeeCritter(self->GetId());
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(Critter* self, Item* item)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    return self->CheckVisibleItem(item->GetId());
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Critter_CountItem(Critter* self, hstring protoId)
{
    return self->CountInvItemByPid(protoId);
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
FO_SCRIPT_API void Server_Critter_DestroyItem(Critter* self, hstring pid, int32 count)
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
FO_SCRIPT_API Item* Server_Critter_AddItem(Critter* self, hstring pid, int32 count)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }
    if (self->GetEngine()->ProtoMngr.GetProtoItem(pid) == nullptr) {
        throw ScriptException("Invalid proto", pid);
    }

    if (count <= 0) {
        return nullptr;
    }

    return self->GetEngine()->ItemMngr.AddItemCritter(self, pid, count);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_GetItem(Critter* self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    return self->GetInvItem(itemId);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_GetItem(Critter* self, hstring protoId)
{
    return self->GetEngine()->CrMngr.GetItemByPidInvPriority(self, protoId);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_GetItem(Critter* self, ItemComponent component)
{
    for (auto& item : self->GetInvItems()) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_GetItem(Critter* self, ItemProperty property, int32 propertyValue)
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
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, ItemComponent component)
{
    const auto items = self->GetInvItems();

    vector<Item*> result;
    result.reserve(items.size());

    for (auto& item : items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.push_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto items = self->GetInvItems();

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
    const auto items = self->GetInvItems();

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
FO_SCRIPT_API void Server_Critter_ChangeItemSlot(Critter* self, ident_t itemId, CritterItemSlot slot)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = self->GetInvItem(itemId);

    if (item == nullptr) {
        throw ScriptException("Item not found");
    }

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
            self->GetEngine()->OnCritterItemMoved.Fire(self, item_swap, slot);
        }

        self->GetEngine()->OnCritterItemMoved.Fire(self, item, from_slot);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetCondition(Critter* self, CritterCondition cond, CritterActionAnim actionAnim, AbstractItem* contextItem)
{
    const auto prev_cond = self->GetCondition();

    if (prev_cond == cond) {
        return;
    }

    Map* map = nullptr;

    if (self->GetMapId()) {
        map = self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
        FO_RUNTIME_ASSERT(map);

        map->RemoveCritterFromField(self);
    }

    self->SetCondition(cond);

    if (map != nullptr) {
        map->AddCritterToField(self);
    }

    if (cond == CritterCondition::Dead) {
        self->SendAndBroadcast_Action(CritterAction::Dead, static_cast<int32>(actionAnim), dynamic_cast<Item*>(contextItem));
    }
    else if (cond == CritterCondition::Knockout) {
        self->SendAndBroadcast_Action(CritterAction::Knockout, static_cast<int32>(actionAnim), dynamic_cast<Item*>(contextItem));
    }
    else if (cond == CritterCondition::Alive) {
        if (prev_cond == CritterCondition::Knockout) {
            self->SendAndBroadcast_Action(CritterAction::StandUp, static_cast<int32>(actionAnim), dynamic_cast<Item*>(contextItem));
        }
        else {
            self->SendAndBroadcast_Action(CritterAction::Respawn, static_cast<int32>(actionAnim), dynamic_cast<Item*>(contextItem));
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Action(Critter* self, CritterAction action, int32 actionData, AbstractItem* contextItem)
{
    self->SendAndBroadcast_Action(action, actionData, dynamic_cast<Item*>(contextItem));
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SendItems(Critter* self, const vector<Item*>& items)
{
    self->Send_SomeItems(items, false, false, {});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SendItems(Critter* self, const vector<Item*>& items, bool owned, bool withInnerEntities, any_t contextParam)
{
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
FO_SCRIPT_API bool Server_Critter_IsOnline(Critter* self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    return self->GetPlayer() != nullptr;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_MoveToCritter(Critter* self, Critter* target, int32 cut, int32 speed)
{
    if (target == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::InProgress;
    self->TargetMoving.TargId = target->GetId();
    self->TargetMoving.TargHex = target->GetHex();
    self->TargetMoving.Cut = cut;
    self->TargetMoving.Speed = numeric_cast<uint16>(speed);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_MoveToHex(Critter* self, mpos hex, int32 cut, int32 speed)
{
    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::InProgress;
    self->TargetMoving.TargHex = hex;
    self->TargetMoving.Cut = cut;
    self->TargetMoving.Speed = numeric_cast<uint16>(speed);
}

///@ ExportMethod
FO_SCRIPT_API MovingState Server_Critter_GetMovingState(Critter* self)
{
    return self->TargetMoving.State;
}

///@ ExportMethod
FO_SCRIPT_API MovingState Server_Critter_GetMovingState(Critter* self, ident_t& gagId)
{
    gagId = self->TargetMoving.GagEntityId;

    return self->TargetMoving.State;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_StopMoving(Critter* self)
{
    self->TargetMoving = {};

    if (self->IsMoving()) {
        self->ClearMove();
        self->SendAndBroadcast_Moving();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeMovingSpeed(Critter* self, int32 speed)
{
    self->GetEngine()->ChangeCritterMovingSpeed(self, numeric_cast<uint16>(speed));
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_AttachToCritter(Critter* self, Critter* cr)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
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
    if (!self->AttachedCritters.empty()) {
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
    for (auto& cr : copy_hold_ref(self->AttachedCritters)) {
        cr->DetachFromCritter();
    }

    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetAttachedCritters(Critter* self)
{
    return vec_transform(self->AttachedCritters, [](auto&& cr) -> Critter* { return cr.get(); });
}

///@ ExportMethod
FO_SCRIPT_API timespan Server_Critter_GetPlayerOfflineTime(Critter* self)
{
    return self->GetOfflineTime();
}

FO_END_NAMESPACE();
