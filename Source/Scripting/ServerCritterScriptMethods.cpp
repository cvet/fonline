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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "GenericUtils.h"
#include "Server.h"
#include "StringUtils.h"
#include "TwoBitMask.h"

// ReSharper disable CppInconsistentNaming
// ReSharper disable CppClangTidyPerformanceUnnecessaryValueParam

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetupScript(Critter* self, InitFunc<Critter*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetupScriptEx(Critter* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsMoving(Critter* self)
{
    return self->IsMoving() || self->TargetMoving.State == MovingState::InProgress;
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Server_Critter_IsOwnedByPlayer(Critter* self)
{
    return self->IsOwnedByPlayer();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsNpc(Critter* self)
{
    return self->IsNpc();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] Player* Server_Critter_GetOwner(Critter* self)
{
    return self->GetOwner();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Critter_GetMap(Critter* self)
{
    return self->GetEngine()->MapMngr.GetMap(self->GetMapId());
}

///# ...
///# param hx ...
///# param hy ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToHex(Critter* self, uint16 hx, uint16 hy, uint8 dir)
{
    if (self->LockMapTransfers > 0) {
        throw ScriptException("Transfers locked");
    }

    auto* map = self->GetEngine()->MapMngr.GetMap(self->GetMapId());
    if (map == nullptr) {
        throw ScriptException("Critter is on global");
    }
    if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    if (hx != self->GetHexX() || hy != self->GetHexY()) {
        if (dir < GameSettings::MAP_DIR_COUNT && self->GetDir() != dir) {
            self->ChangeDir(dir);
        }
        if (!self->GetEngine()->MapMngr.Transit(self, map, hx, hy, self->GetDir(), 2, ident_t {})) {
            throw ScriptException("Transit fail");
        }
    }
    else if (dir < GameSettings::MAP_DIR_COUNT && self->GetDir() != dir) {
        self->ChangeDir(dir);
        self->Send_Dir(self);
        self->Broadcast_Dir();
    }
}

///# ...
///# param map ...
///# param hx ...
///# param hy ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToMap(Critter* self, Map* map, uint16 hx, uint16 hy, uint8 dir)
{
    if (self->LockMapTransfers > 0) {
        throw ScriptException("Transfers locked");
    }
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }

    auto dir_ = dir;
    if (dir_ >= GameSettings::MAP_DIR_COUNT) {
        dir_ = 0;
    }

    if (!self->GetEngine()->MapMngr.Transit(self, map, hx, hy, dir_, 2, ident_t {})) {
        throw ScriptException("Transit to map hex fail");
    }

    const auto* loc = map->GetLocation();
    if (loc != nullptr && GenericUtils::DistSqrt(self->GetWorldX(), self->GetWorldY(), loc->GetWorldX(), loc->GetWorldY()) > loc->GetRadius()) {
        self->SetWorldX(loc->GetWorldX());
        self->SetWorldY(loc->GetWorldY());
    }
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToGlobal(Critter* self)
{
    if (self->LockMapTransfers > 0) {
        throw ScriptException("Transfers locked");
    }

    if (self->GetMapId() && !self->GetEngine()->MapMngr.TransitToGlobal(self, ident_t {})) {
        throw ScriptException("Transit to global failed");
    }
}

///# ...
///# param group ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToGlobalWithGroup(Critter* self, const vector<Critter*>& group)
{
    if (self->LockMapTransfers > 0) {
        throw ScriptException("Transfers locked");
    }
    if (!self->GetMapId()) {
        throw ScriptException("Critter already on global");
    }

    if (!self->GetEngine()->MapMngr.TransitToGlobal(self, ident_t {})) {
        throw ScriptException("Transit to global failed");
    }

    for (auto* cr_ : group) {
        if (cr_ != nullptr && !cr_->IsDestroyed()) {
            self->GetEngine()->MapMngr.TransitToGlobal(cr_, self->GetId());
        }
    }
}

///# ...
///# param leader ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToGlobalGroup(Critter* self, Critter* leader)
{
    if (self->LockMapTransfers > 0) {
        throw ScriptException("Transfers locked");
    }
    if (!self->GetMapId()) {
        throw ScriptException("Critter already on global");
    }
    if (leader == nullptr) {
        throw ScriptException("Leader arg not found");
    }

    if (leader->GlobalMapGroup == nullptr) {
        throw ScriptException("Leader is not on global map");
    }

    if (!self->GetEngine()->MapMngr.TransitToGlobal(self, leader->GetId())) {
        throw ScriptException("Transit fail");
    }
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsAlive(Critter* self)
{
    return self->IsAlive();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsKnockout(Critter* self)
{
    return self->IsKnockout();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsDead(Critter* self)
{
    return self->IsDead();
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_RefreshView(Critter* self)
{
    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
    self->GetEngine()->MapMngr.ProcessVisibleItems(self);
}

///# ...
///# param map ...
///# param look ...
///# param hx ...
///# param hy ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ViewMap(Critter* self, Map* map, uint look, uint16 hx, uint16 hy, uint8 dir)
{
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    if (!self->IsOwnedByPlayer()) {
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
    self->ViewMapLook = static_cast<uint16>(look_);
    self->ViewMapHx = hx;
    self->ViewMapHy = hy;
    self->ViewMapDir = dir_;
    self->ViewMapLocId = ident_t {};
    self->ViewMapLocEnt = 0;
    self->Send_LoadMap(map);
}

///# ...
///# param howSay ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_Say(Critter* self, uint8 howSay, string_view text)
{
    if (howSay != SAY_FLASH_WINDOW && text.empty()) {
        return;
    }
    if (self->IsNpc() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_Text(self, howSay != SAY_FLASH_WINDOW ? text : " ", howSay);
    }
    else if (self->GetMapId()) {
        self->SendAndBroadcast_Text(self->VisCr, text, howSay, false);
    }
}

///# ...
///# param howSay ...
///# param textMsg ...
///# param numStr ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SayMsg(Critter* self, uint8 howSay, uint16 textMsg, uint numStr)
{
    if (self->IsNpc() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_TextMsg(self, numStr, howSay, textMsg);
    }
    else if (self->GetMapId()) {
        self->SendAndBroadcast_Msg(self->VisCr, numStr, howSay, textMsg);
    }
}

///# ...
///# param howSay ...
///# param textMsg ...
///# param numStr ...
///# param lexems ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SayMsg(Critter* self, uint8 howSay, uint16 textMsg, uint numStr, string_view lexems)
{
    if (self->IsNpc() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_TextMsgLex(self, numStr, howSay, textMsg, lexems);
    }
    else if (self->GetMapId()) {
        self->SendAndBroadcast_MsgLex(self->VisCr, numStr, howSay, textMsg, lexems);
    }
}

///# ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetDir(Critter* self, uint8 dir)
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

///# ...
///# param dir_angle ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetDirAngle(Critter* self, int16 dir_angle)
{
    const auto normalized_dir_angle = GeometryHelper::NormalizeAngle(dir_angle);

    if (self->GetDirAngle() == normalized_dir_angle) {
        return;
    }

    self->ChangeDirAngle(normalized_dir_angle);
    self->Send_Dir(self);
    self->Broadcast_Dir();
}

///# ...
///# param lookOnMe ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Critter_GetCritters(Critter* self, bool lookOnMe, CritterFindType findType)
{
    vector<Critter*> critters;

    for (auto* cr : lookOnMe ? self->VisCr : self->VisCrSelf) {
        if (cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    int hx = self->GetHexX();
    int hy = self->GetHexY();
    std::sort(critters.begin(), critters.end(), [hx, hy](Critter* cr1, Critter* cr2) {
        const auto dist1 = GeometryHelper::DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = GeometryHelper::DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
        return dist1 < dist2;
    });

    return critters;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Critter_GetTalkingCritters(Critter* self)
{
    if (!self->IsNpc()) {
        throw ScriptException("Critter is not npc");
    }

    vector<Critter*> result;

    for (auto* cr : self->VisCr) {
        if (cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == self->GetId()) {
            result.push_back(cr);
        }
    }

    int hx = self->GetHexX();
    int hy = self->GetHexY();
    std::sort(result.begin(), result.end(), [hx, hy](Critter* cr1, Critter* cr2) {
        const auto dist1 = GeometryHelper::DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = GeometryHelper::DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
        return dist1 < dist2;
    });

    return result;
}

///# ...
///# param cr ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsSee(Critter* self, Critter* cr)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (self == cr) {
        return true;
    }

    const auto& critters = self->GetMapId() ? self->VisCrSelf : *self->GlobalMapGroup;
    return std::find(critters.begin(), critters.end(), cr) != critters.end();
}

///# ...
///# param cr ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsSeenBy(Critter* self, Critter* cr)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (self == cr) {
        return true;
    }

    const auto& critters = self->GetMapId() ? self->VisCr : *self->GlobalMapGroup;
    return std::find(critters.begin(), critters.end(), cr) != critters.end();
}

///# ...
///# param item ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsSee(Critter* self, Item* item)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    return self->CountIdVisItem(item->GetId());
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_CountItem(Critter* self, hstring protoId)
{
    return self->CountInvItemPid(protoId);
}

///# ...
///# param pid ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_DeleteItem(Critter* self, hstring pid)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }

    const auto count = self->CountInvItemPid(pid);
    if (count == 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, pid, count);
}

///# ...
///# param pid ...
///# param count ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_DeleteItem(Critter* self, hstring pid, uint count)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }

    if (count == 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, pid, count);
}

///# ...
///# param pid ...
///# param count ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_AddItem(Critter* self, hstring pid, uint count)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }
    if (self->GetEngine()->ProtoMngr.GetProtoItem(pid) == nullptr) {
        throw ScriptException("Invalid proto", pid);
    }

    if (count == 0) {
        return nullptr;
    }

    return self->GetEngine()->ItemMngr.AddItemCritter(self, pid, count);
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItem(Critter* self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    return self->GetInvItem(itemId, false);
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItem(Critter* self, hstring protoId)
{
    return self->GetEngine()->CrMngr.GetItemByPidInvPriority(self, protoId);
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItem(Critter* self, ItemComponent component)
{
    for (auto* item : self->GetRawInvItems()) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItem(Critter* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    for (auto* item : self->GetRawInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItems(Critter* self)
{
    return self->GetRawInvItems();
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItems(Critter* self, ItemComponent component)
{
    vector<Item*> items;
    items.reserve(self->GetRawInvItems().size());

    for (auto* item : self->GetRawInvItems()) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItems(Critter* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    vector<Item*> items;
    items.reserve(self->GetRawInvItems().size());

    for (auto* item : self->GetRawInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItems(Critter* self, hstring protoId)
{
    vector<Item*> items;
    items.reserve(self->GetRawInvItems().size());

    for (auto* item : self->GetRawInvItems()) {
        if (item->GetProtoId() == protoId) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param itemId ...
///# param slot ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ChangeItemSlot(Critter* self, ident_t itemId, CritterItemSlot slot)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = self->GetInvItem(itemId, self->IsOwnedByPlayer());
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

    if (!self->GetEngine()->OnCritterCheckMoveItem.Fire(self, item, slot)) {
        throw ScriptException("Can't move item");
    }

    const auto is_multi_item_allowed = slot == CritterItemSlot::Inventory || (static_cast<size_t>(slot) < self->GetEngine()->Settings.CritterSlotMultiItem.size() && self->GetEngine()->Settings.CritterSlotMultiItem[static_cast<size_t>(slot)]);

    if (is_multi_item_allowed) {
        const auto from_slot = item->GetCritterSlot();

        item->SetCritterSlot(slot);

        self->SendAndBroadcast_MoveItem(item, CritterAction::MoveItem, from_slot);

        self->GetEngine()->OnCritterMoveItem.Fire(self, item, from_slot);
    }
    else {
        auto* item_swap = self->GetInvItemSlot(slot);
        const auto from_slot = item->GetCritterSlot();

        item->SetCritterSlot(slot);
        if (item_swap != nullptr) {
            item_swap->SetCritterSlot(from_slot);
        }

        self->SendAndBroadcast_MoveItem(item, CritterAction::MoveItem, from_slot);

        if (item_swap != nullptr) {
            self->GetEngine()->OnCritterMoveItem.Fire(self, item_swap, slot);
        }

        self->GetEngine()->OnCritterMoveItem.Fire(self, item, from_slot);
    }
}

///# ...
///# param cond ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetCondition(Critter* self, CritterCondition cond)
{
    const auto prev_cond = self->GetCond();
    if (prev_cond == cond) {
        return;
    }

    self->SetCond(cond);

    if (self->GetMapId()) {
        auto* map = self->GetEngine()->MapMngr.GetMap(self->GetMapId());
        RUNTIME_ASSERT(map);

        const auto hx = self->GetHexX();
        const auto hy = self->GetHexY();
        const auto multihex = self->GetMultihex();
        const auto is_dead = (cond == CritterCondition::Dead);

        map->UnsetFlagCritter(hx, hy, multihex, !is_dead);
        map->SetFlagCritter(hx, hy, multihex, is_dead);
    }
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_CloseDialog(Critter* self)
{
    if (self->IsTalking()) {
        self->GetEngine()->CrMngr.CloseTalk(self);
    }
}

///# ...
///# param action ...
///# param actionData ...
///# param contextItem ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_Action(Critter* self, CritterAction action, int actionData, AbstractItem* contextItem)
{
    self->SendAndBroadcast_Action(action, actionData, dynamic_cast<Item*>(contextItem));
}

///# ...
///# param stateAnim ...
///# param actionAnim ...
///# param contextItem ...
///# param clearSequence ...
///# param delayPlay ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Server_Critter_Animate(Critter* self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, AbstractItem* contextItem, bool clearSequence, bool delayPlay)
{
    self->SendAndBroadcast_Animate(stateAnim, actionAnim, dynamic_cast<Item*>(contextItem), clearSequence, delayPlay);
}

///# ...
///# param cond ...
///# param stateAnim ...
///# param actionAnim ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetConditionAnims(Critter* self, CritterCondition cond, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
{
    if (cond == CritterCondition::Alive) {
        self->SetAliveStateAnim(stateAnim);
        self->SetAliveActionAnim(actionAnim);
    }
    else if (cond == CritterCondition::Knockout) {
        self->SetKnockoutStateAnim(stateAnim);
        self->SetKnockoutActionAnim(actionAnim);
    }
    else if (cond == CritterCondition::Dead) {
        self->SetDeadStateAnim(stateAnim);
        self->SetDeadActionAnim(actionAnim);
    }

    self->SendAndBroadcast_SetAnims(cond, stateAnim, actionAnim);
}

///# ...
///# param soundName ...
///# param sendSelf ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_PlaySound(Critter* self, string_view soundName, bool sendSelf)
{
    if (sendSelf) {
        self->Send_PlaySound(self->GetId(), soundName);
    }

    for (auto it = self->VisCr.begin(), end = self->VisCr.end(); it != end; ++it) {
        auto* cr_ = *it;
        cr_->Send_PlaySound(self->GetId(), soundName);
    }
}

///# ...
///# param locId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsKnownLocation(Critter* self, ident_t locId)
{
    if (!locId) {
        throw ScriptException("Invalid location id");
    }

    return self->GetEngine()->MapMngr.CheckKnownLoc(self, locId);
}

///# ...
///# param locId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetKnownLocation(Critter* self, ident_t locId)
{
    if (!locId) {
        throw ScriptException("Invalid location id");
    }

    if (self->GetEngine()->MapMngr.CheckKnownLoc(self, locId)) {
        return;
    }

    const auto* loc = self->GetEngine()->MapMngr.GetLocation(locId);
    if (loc == nullptr) {
        throw ScriptException("Location not found");
    }

    self->GetEngine()->MapMngr.AddKnownLoc(self, loc->GetId());

    if (loc->IsNonEmptyAutomaps()) {
        self->Send_AutomapsInfo(nullptr, loc);
    }

    if (!self->GetMapId()) {
        self->Send_GlobalLocation(loc, true);
    }

    const auto zx = loc->GetWorldX() / self->GetEngine()->Settings.GlobalMapZoneLength;
    const auto zy = loc->GetWorldY() / self->GetEngine()->Settings.GlobalMapZoneLength;

    auto gmap_fog = self->GetGlobalMapFog();

    if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
        gmap_fog.resize(GM_ZONES_FOG_SIZE);
        self->SetGlobalMapFog(gmap_fog);
    }

    auto gmap_mask = TwoBitMask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());

    if (gmap_mask.Get2Bit(zx, zy) == GM_FOG_FULL) {
        gmap_mask.Set2Bit(zx, zy, GM_FOG_HALF);
        self->SetGlobalMapFog(gmap_fog);
        if (!self->GetMapId()) {
            self->Send_GlobalMapFog(static_cast<uint16>(zx), static_cast<uint16>(zy), GM_FOG_HALF);
        }
    }
}

///# ...
///# param locId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_UnsetKnownLocation(Critter* self, ident_t locId)
{
    if (!locId) {
        throw ScriptException("Invalid location id");
    }

    if (!self->GetEngine()->MapMngr.CheckKnownLoc(self, locId)) {
        return;
    }

    self->GetEngine()->MapMngr.EraseKnownLoc(self, locId);

    if (!self->GetMapId()) {
        if (const auto* loc = self->GetEngine()->MapMngr.GetLocation(locId); loc != nullptr) {
            self->Send_GlobalLocation(loc, false);
        }
    }
}

///# ...
///# param zoneX ...
///# param zoneY ...
///# param fog ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetFog(Critter* self, uint16 zoneX, uint16 zoneY, int fog)
{
    if (fog < GM_FOG_FULL || fog > GM_FOG_NONE) {
        throw ScriptException("Invalid fog arg");
    }
    if (zoneX >= self->GetEngine()->Settings.GlobalMapWidth || zoneY >= self->GetEngine()->Settings.GlobalMapHeight) {
        return;
    }

    auto gmap_fog = self->GetGlobalMapFog();

    if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
        gmap_fog.resize(GM_ZONES_FOG_SIZE);
        self->SetGlobalMapFog(gmap_fog);
    }

    auto gmap_mask = TwoBitMask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());

    if (gmap_mask.Get2Bit(zoneX, zoneY) != fog) {
        gmap_mask.Set2Bit(zoneX, zoneY, fog);
        self->SetGlobalMapFog(gmap_fog);
        if (!self->GetMapId()) {
            self->Send_GlobalMapFog(zoneX, zoneY, static_cast<uint8>(fog));
        }
    }
}

///# ...
///# param zoneX ...
///# param zoneY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Server_Critter_GetFog(Critter* self, uint16 zoneX, uint16 zoneY)
{
    if (zoneX >= self->GetEngine()->Settings.GlobalMapWidth || zoneY >= self->GetEngine()->Settings.GlobalMapHeight) {
        return GM_FOG_FULL;
    }

    auto gmap_fog = self->GetGlobalMapFog();

    if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
        gmap_fog.resize(GM_ZONES_FOG_SIZE);
        self->SetGlobalMapFog(gmap_fog);
    }

    const auto gmap_mask = TwoBitMask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());
    return gmap_mask.Get2Bit(zoneX, zoneY);
}

///# ...
///# param items ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SendItems(Critter* self, const vector<Item*>& items)
{
    self->Send_SomeItems(&items, 0);
}

///# ...
///# param items ...
///# param param ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SendItems(Critter* self, const vector<Item*>& items, int param)
{
    self->Send_SomeItems(&items, param);
}

///# ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Server_Critter_Disconnect(Critter* self)
{
    if (!self->IsOwnedByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    if (const auto* owner = self->GetOwner(); owner != nullptr) {
        owner->Connection->GracefulDisconnect();
    }
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Server_Critter_IsOnline(Critter* self)
{
    if (!self->IsOwnedByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    return self->GetOwner() != nullptr;
}

///# ...
///# param target ...
///# param cut ...
///# param speed ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_MoveToCritter(Critter* self, Critter* target, uint cut, uint speed)
{
    if (target == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::InProgress;
    self->TargetMoving.TargId = target->GetId();
    self->TargetMoving.HexX = target->GetHexX();
    self->TargetMoving.HexY = target->GetHexY();
    self->TargetMoving.Cut = cut;
    self->TargetMoving.Speed = static_cast<uint16>(speed);
}

///# ...
///# param hx ...
///# param hy ...
///# param cut ...
///# param speed ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_MoveToHex(Critter* self, uint16 hx, uint16 hy, uint cut, uint speed)
{
    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::InProgress;
    self->TargetMoving.HexX = hx;
    self->TargetMoving.HexY = hy;
    self->TargetMoving.Cut = cut;
    self->TargetMoving.Speed = static_cast<uint16>(speed);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] MovingState Server_Critter_GetMovingState(Critter* self)
{
    return self->TargetMoving.State;
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ResetMovingState(Critter* self)
{
    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::Success;

    self->ClearMove();
}

///# ...
///# param gagId ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ResetMovingState(Critter* self, ident_t& gagId)
{
    gagId = self->TargetMoving.GagEntityId;

    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::Success;

    self->ClearMove();
}

///# ...
///# param func ...
///# param duration ...
///# param identifier ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_AddTimeEvent(Critter* self, ScriptFunc<uint, Critter*, any_t, uint*> func, tick_t duration, any_t identifier)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    self->AddTimeEvent(func.GetName(), 0, duration, identifier);
}

///# ...
///# param func ...
///# param duration ...
///# param identifier ...
///# param rate ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_AddTimeEvent(Critter* self, ScriptFunc<uint, Critter*, any_t, uint*> func, tick_t duration, any_t identifier, uint rate)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    self->AddTimeEvent(func.GetName(), rate, duration, identifier);
}

///# ...
///# param identifier ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_GetTimeEvents(Critter* self, any_t identifier)
{
    auto&& te_identifiers = self->GetTE_Identifier();

    uint count = 0;

    for (const auto& te_identifier : te_identifiers) {
        if (te_identifier == identifier) {
            count++;
        }
    }

    return count;
}

///# ...
///# param identifier ...
///# param indexes ...
///# param durations ...
///# param rates ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_GetTimeEvents(Critter* self, any_t identifier, vector<uint>& indexes, vector<tick_t>& durations, vector<uint>& rates)
{
    auto&& te_identifiers = self->GetTE_Identifier();
    auto&& te_fire_times = self->GetTE_FireTime();
    auto&& te_rates = self->GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    const auto full_second = self->GetEngine()->GameTime.GetFullSecond();

    uint count = 0;

    for (size_t i = 0; i < te_identifiers.size(); i++) {
        if (te_identifiers[i] == identifier) {
            indexes.push_back(static_cast<uint>(i));
            durations.emplace_back(te_fire_times[i].underlying_value() > full_second.underlying_value() ? te_fire_times[i].underlying_value() - full_second.underlying_value() : 0);
            rates.push_back(te_rates[i]);
            count++;
        }
    }

    return count;
}

///# ...
///# param findIdentifiers ...
///# param identifiers ...
///# param indexes ...
///# param durations ...
///# param rates ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_GetTimeEvents(Critter* self, const vector<any_t>& findIdentifiers, vector<any_t>& identifiers, vector<uint>& indexes, vector<tick_t>& durations, vector<uint>& rates)
{
    auto&& te_identifiers = self->GetTE_Identifier();
    auto&& te_fire_times = self->GetTE_FireTime();
    auto&& te_rates = self->GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    const auto full_second = self->GetEngine()->GameTime.GetFullSecond();

    uint count = 0;

    for (const auto& identifier : findIdentifiers) {
        for (size_t i = 0; i < te_identifiers.size(); i++) {
            if (te_identifiers[i] == identifier) {
                identifiers.push_back(te_identifiers[i]);
                indexes.push_back(static_cast<uint>(i));
                durations.emplace_back(te_fire_times[i].underlying_value() > full_second.underlying_value() ? te_fire_times[i].underlying_value() - full_second.underlying_value() : 0);
                rates.push_back(te_rates[i]);
                count++;
            }
        }
    }

    return count;
}

///# ...
///# param index ...
///# param newDuration ...
///# param newRate ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ChangeTimeEvent(Critter* self, uint index, tick_t newDuration, uint newRate)
{
    auto&& te_identifiers = self->GetTE_Identifier();
    auto&& te_fire_times = self->GetTE_FireTime();
    auto&& te_func_names = self->GetTE_FuncName();
    auto&& te_rates = self->GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_func_names.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    if (index >= te_identifiers.size()) {
        throw ScriptException("Index arg is greater than maximum time events");
    }

    self->EraseTimeEvent(index);
    self->AddTimeEvent(te_func_names[index], newRate, newDuration, te_identifiers[index]);
}

///# ...
///# param index ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_EraseTimeEvent(Critter* self, uint index)
{
    auto&& te_identifiers = self->GetTE_Identifier();

    if (index >= te_identifiers.size()) {
        throw ScriptException("Index arg is greater than maximum time events");
    }

    self->EraseTimeEvent(index);
}

///# ...
///# param identifier ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_EraseTimeEvents(Critter* self, any_t identifier)
{
    auto&& te_identifiers = self->GetTE_Identifier();

    uint count = 0;
    size_t index = 0;

    for (const auto& te_identifier : te_identifiers) {
        if (te_identifier == identifier) {
            self->EraseTimeEvent(index);
            count++;
        }
        else {
            index++;
        }
    }

    return count;
}

///# ...
///# param identifiers ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_EraseTimeEvents(Critter* self, const vector<any_t>& identifiers)
{
    uint count = 0;

    for (const auto& identifier : identifiers) {
        auto&& te_identifiers = self->GetTE_Identifier();
        size_t index = 0;

        for (const auto& te_identifier : te_identifiers) {
            if (te_identifier == identifier) {
                self->EraseTimeEvent(index);
                count++;
            }
            else {
                index++;
            }
        }
    }

    return count;
}
