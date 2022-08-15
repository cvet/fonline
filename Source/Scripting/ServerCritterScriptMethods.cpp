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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GeometryHelper.h"
#include "Server.h"
#include "StringUtils.h"
#include "TwoBitMask.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetupScript(Critter* self, InitFunc<Critter*> initFunc)
{
    ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true);
    self->SetInitScript(initFunc);
}

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetupScriptEx(Critter* self, hstring initFunc)
{
    ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true);
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
[[maybe_unused]] void Server_Critter_TransitToHex(Critter* self, ushort hx, ushort hy, uchar dir)
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
        if (dir < self->GetEngine()->Settings.MapDirCount && self->GetDir() != dir) {
            self->ChangeDir(dir);
        }
        if (!self->GetEngine()->MapMngr.Transit(self, map, hx, hy, self->GetDir(), 2, 0, true)) {
            throw ScriptException("Transit fail");
        }
    }
    else if (dir < self->GetEngine()->Settings.MapDirCount && self->GetDir() != dir) {
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
[[maybe_unused]] void Server_Critter_TransitToMap(Critter* self, Map* map, ushort hx, ushort hy, uchar dir)
{
    if (self->LockMapTransfers > 0) {
        throw ScriptException("Transfers locked");
    }
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }

    auto dir_ = dir;
    if (dir_ >= self->GetEngine()->Settings.MapDirCount) {
        dir_ = 0;
    }

    if (!self->GetEngine()->MapMngr.Transit(self, map, hx, hy, dir_, 2, 0, true)) {
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

    if (self->GetMapId() != 0u && !self->GetEngine()->MapMngr.TransitToGlobal(self, 0, true)) {
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
    if (self->GetMapId() == 0u) {
        throw ScriptException("Critter already on global");
    }

    if (!self->GetEngine()->MapMngr.TransitToGlobal(self, 0, true)) {
        throw ScriptException("Transit to global failed");
    }

    for (auto* cr_ : group) {
        if (cr_ != nullptr && !cr_->IsDestroyed()) {
            self->GetEngine()->MapMngr.TransitToGlobal(cr_, self->GetId(), true);
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
    if (self->GetMapId() == 0u) {
        throw ScriptException("Critter already on global");
    }
    if (leader == nullptr) {
        throw ScriptException("Leader arg not found");
    }

    if (leader->GlobalMapGroup == nullptr) {
        throw ScriptException("Leader is not on global map");
    }

    if (!self->GetEngine()->MapMngr.TransitToGlobal(self, leader->GetId(), true)) {
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
[[maybe_unused]] void Server_Critter_ViewMap(Critter* self, Map* map, uint look, ushort hx, ushort hy, uchar dir)
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
    if (dir_ >= self->GetEngine()->Settings.MapDirCount) {
        dir_ = self->GetDir();
    }

    auto look_ = look;
    if (look_ == 0) {
        look_ = self->GetLookDistance();
    }

    self->ViewMapId = map->GetId();
    self->ViewMapPid = map->GetProtoId();
    self->ViewMapLook = static_cast<ushort>(look_);
    self->ViewMapHx = hx;
    self->ViewMapHy = hy;
    self->ViewMapDir = dir_;
    self->ViewMapLocId = 0;
    self->ViewMapLocEnt = 0;
    self->Send_LoadMap(map);
}

///# ...
///# param howSay ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_Say(Critter* self, uchar howSay, string_view text)
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
    else if (self->GetMapId() != 0u) {
        self->SendAndBroadcast_Text(self->VisCr, text, howSay, false);
    }
}

///# ...
///# param howSay ...
///# param textMsg ...
///# param numStr ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SayMsg(Critter* self, uchar howSay, ushort textMsg, uint numStr)
{
    if (self->IsNpc() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_TextMsg(self, numStr, howSay, textMsg);
    }
    else if (self->GetMapId() != 0u) {
        self->SendAndBroadcast_Msg(self->VisCr, numStr, howSay, textMsg);
    }
}

///# ...
///# param howSay ...
///# param textMsg ...
///# param numStr ...
///# param lexems ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SayMsg(Critter* self, uchar howSay, ushort textMsg, uint numStr, string_view lexems)
{
    if (self->IsNpc() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_TextMsgLex(self, numStr, howSay, textMsg, lexems);
    }
    else if (self->GetMapId() != 0u) {
        self->SendAndBroadcast_MsgLex(self->VisCr, numStr, howSay, textMsg, lexems);
    }
}

///# ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetDir(Critter* self, uchar dir)
{
    if (dir >= self->GetEngine()->Settings.MapDirCount) {
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
[[maybe_unused]] void Server_Critter_SetDirAngle(Critter* self, short dir_angle)
{
    const auto normalized_dir_angle = self->GetEngine()->Geometry.NormalizeAngle(dir_angle);

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
    std::sort(critters.begin(), critters.end(), [self, hx, hy](Critter* cr1, Critter* cr2) {
        const auto dist1 = self->GetEngine()->Geometry.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = self->GetEngine()->Geometry.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
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
    std::sort(result.begin(), result.end(), [self, hx, hy](Critter* cr1, Critter* cr2) {
        const auto dist1 = self->GetEngine()->Geometry.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = self->GetEngine()->Geometry.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
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

    const auto& critters = (self->GetMapId() ? self->VisCrSelf : *self->GlobalMapGroup);
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

    const auto& critters = (self->GetMapId() ? self->VisCr : *self->GlobalMapGroup);
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
    return self->CountItemPid(protoId);
}

///# ...
///# param pid ...
///# param count ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_DeleteItem(Critter* self, hstring pid, uint count)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }

    auto count_ = count;
    if (count_ == 0) {
        count_ = self->CountItemPid(pid);
    }

    return self->GetEngine()->ItemMngr.SubItemCritter(self, pid, count_, nullptr);
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
    if (!self->GetEngine()->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid proto", pid);
    }

    if (count == 0u) {
        return nullptr;
    }

    return self->GetEngine()->ItemMngr.AddItemCritter(self, pid, count);
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItem(Critter* self, uint itemId)
{
    if (itemId == 0u) {
        return nullptr;
    }

    return self->GetItem(itemId, false);
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
    for (auto* item : self->GetInventory()) {
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

    for (auto* item : self->GetInventory()) {
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
    return self->GetInventory();
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItems(Critter* self, ItemComponent component)
{
    vector<Item*> items;
    items.reserve(self->GetInventory().size());

    for (auto* item : self->GetInventory()) {
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
    items.reserve(self->GetInventory().size());

    for (auto* item : self->GetInventory()) {
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
    items.reserve(self->GetInventory().size());

    for (auto* item : self->GetInventory()) {
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
[[maybe_unused]] void Server_Critter_ChangeItemSlot(Critter* self, uint itemId, uchar slot)
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = self->GetItem(itemId, self->IsOwnedByPlayer());
    if (item == nullptr) {
        throw ScriptException("Item not found");
    }

    // To slot arg is equal of current item slot
    if (item->GetCritterSlot() == slot) {
        return;
    }

    if (slot >= self->GetEngine()->Settings.CritterSlotEnabled.size() || !self->GetEngine()->Settings.CritterSlotEnabled[slot]) {
        throw ScriptException("Slot is not allowed");
    }

    if (!self->GetEngine()->OnCritterCheckMoveItem.Fire(self, item, slot)) {
        throw ScriptException("Can't move item");
    }

    auto* item_swap = slot != 0 ? self->GetItemSlot(slot) : nullptr;
    const auto from_slot = item->GetCritterSlot();

    item->SetCritterSlot(slot);
    if (item_swap != nullptr) {
        item_swap->SetCritterSlot(from_slot);
    }

    self->SendAndBroadcast_MoveItem(item, ACTION_MOVE_ITEM, from_slot);

    if (item_swap != nullptr) {
        self->GetEngine()->OnCritterMoveItem.Fire(self, item_swap, slot);
    }
    self->GetEngine()->OnCritterMoveItem.Fire(self, item, from_slot);
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
///# param actionExt ...
///# param item ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_Action(Critter* self, int action, int actionExt, AbstractItem* item)
{
    self->SendAndBroadcast_Action(action, actionExt, dynamic_cast<Item*>(item));
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# param item ...
///# param clearSequence ...
///# param delayPlay ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Server_Critter_Animate(Critter* self, uint anim1, uint anim2, AbstractItem* item, bool clearSequence, bool delayPlay)
{
    self->SendAndBroadcast_Animate(anim1, anim2, dynamic_cast<Item*>(item), clearSequence, delayPlay);
}

///# ...
///# param cond ...
///# param anim1 ...
///# param anim2 ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetConditionAnims(Critter* self, CritterCondition cond, uint anim1, uint anim2)
{
    if (cond == CritterCondition::Alive) {
        self->SetAnim1Alive(anim1);
        self->SetAnim2Alive(anim2);
    }
    else if (cond == CritterCondition::Knockout) {
        self->SetAnim1Knockout(anim1);
        self->SetAnim2Knockout(anim2);
    }
    else if (cond == CritterCondition::Dead) {
        self->SetAnim1Dead(anim1);
        self->SetAnim2Dead(anim2);
    }

    self->SendAndBroadcast_SetAnims(cond, anim1, anim2);
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
[[maybe_unused]] bool Server_Critter_IsKnownLocation(Critter* self, uint locId)
{
    if (locId == 0u) {
        throw ScriptException("Invalid location id");
    }

    return self->GetEngine()->MapMngr.CheckKnownLoc(self, locId);
}

///# ...
///# param locId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetKnownLocation(Critter* self, uint locId)
{
    if (locId == 0u) {
        throw ScriptException("Invalid location id");
    }

    if (self->GetEngine()->MapMngr.CheckKnownLoc(self, locId)) {
        return;
    }

    auto* loc = self->GetEngine()->MapMngr.GetLocation(locId);
    if (loc == nullptr) {
        throw ScriptException("Location not found");
    }

    self->GetEngine()->MapMngr.AddKnownLoc(self, loc->GetId());

    if (loc->IsNonEmptyAutomaps()) {
        self->Send_AutomapsInfo(nullptr, loc);
    }

    if (self->GetMapId() == 0u) {
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
        if (self->GetMapId() == 0u) {
            self->Send_GlobalMapFog(zx, zy, GM_FOG_HALF);
        }
    }
}

///# ...
///# param locId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_UnsetKnownLocation(Critter* self, uint locId)
{
    if (locId == 0u) {
        throw ScriptException("Invalid location id");
    }

    if (!self->GetEngine()->MapMngr.CheckKnownLoc(self, locId)) {
        return;
    }

    self->GetEngine()->MapMngr.EraseKnownLoc(self, locId);

    if (self->GetMapId() == 0u) {
        if (auto* loc = self->GetEngine()->MapMngr.GetLocation(locId); loc != nullptr) {
            self->Send_GlobalLocation(loc, false);
        }
    }
}

///# ...
///# param zoneX ...
///# param zoneY ...
///# param fog ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetFog(Critter* self, ushort zoneX, ushort zoneY, int fog)
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
            self->Send_GlobalMapFog(zoneX, zoneY, fog);
        }
    }
}

///# ...
///# param zoneX ...
///# param zoneY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Server_Critter_GetFog(Critter* self, ushort zoneX, ushort zoneY)
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

    if (auto* owner = self->GetOwner(); owner != nullptr) {
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
///# param isRun ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_MoveToCritter(Critter* self, Critter* target, uint cut, bool isRun)
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
    self->TargetMoving.IsRun = isRun;
}

///# ...
///# param hx ...
///# param hy ...
///# param cut ...
///# param isRun ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_MoveToHex(Critter* self, ushort hx, ushort hy, uint cut, bool isRun)
{
    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::InProgress;
    self->TargetMoving.HexX = hx;
    self->TargetMoving.HexY = hy;
    self->TargetMoving.Cut = cut;
    self->TargetMoving.IsRun = isRun;
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
[[maybe_unused]] void Server_Critter_ResetMovingState(Critter* self, uint& gagId)
{
    gagId = self->TargetMoving.GagEntityId;

    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::Success;

    self->ClearMove();
}
