//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Server_Critter_IsPlayer(Critter* self)
{
    return self->IsPlayer();
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
///@ ExportMethod
[[maybe_unused]] Map* Server_Critter_GetMap(Critter* self)
{
    return self->GetEngine()->MapMngr.GetMap(self->GetMapId());
}

///# ...
///# param direction ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_MoveToDir(Critter* self, uchar direction)
{
    auto* map = self->GetEngine()->MapMngr.GetMap(self->GetMapId());
    if (!map) {
        throw ScriptException("Critter is on global");
    }
    if (direction >= self->GetEngine()->Settings.MapDirCount) {
        throw ScriptException("Invalid direction arg");
    }

    /*auto hx = self->GetHexX();
    auto hy = self->GetHexY();
    if (self->GetEngine()->GeomHelper.MoveHexByDir(hx, hy, direction, map->GetWidth(), map->GetHeight())) {
        const auto move_flags = static_cast<ushort>(direction | BIN16(00000000, 00111000));
        const auto move = self->GetEngine()->Act_Move(self, hx, hy, move_flags);
        if (move) {
            self->Send_Move(self, move_flags);
        }

        return move;
    }*/
    return false;
}

///# ...
///# param hx ...
///# param hy ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToHex(Critter* self, ushort hx, ushort hy, uchar dir)
{
    if (self->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }
    auto* map = self->GetEngine()->MapMngr.GetMap(self->GetMapId());
    if (!map) {
        throw ScriptException("Critter is on global");
    }
    if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    if (hx != self->GetHexX() || hy != self->GetHexY()) {
        if (dir < self->GetEngine()->Settings.MapDirCount && self->GetDir() != dir) {
            self->SetDir(dir);
        }
        if (!self->GetEngine()->MapMngr.Transit(self, map, hx, hy, self->GetDir(), 2, 0, true)) {
            throw ScriptException("Transit fail");
        }
    }
    else if (dir < self->GetEngine()->Settings.MapDirCount && self->GetDir() != dir) {
        self->SetDir(dir);
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
[[maybe_unused]] void Server_Critter_TransitToMapHex(Critter* self, Map* map, ushort hx, ushort hy, uchar dir)
{
    if (self->LockMapTransfers) {
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

    // Todo: need attention!
    auto* loc = map->GetLocation();
    if (loc && GenericUtils::DistSqrt(self->GetWorldX(), self->GetWorldY(), loc->GetWorldX(), loc->GetWorldY()) > loc->GetRadius()) {
        self->SetWorldX(loc->GetWorldX());
        self->SetWorldY(loc->GetWorldY());
    }
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToGlobal(Critter* self)
{
    if (self->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }

    if (self->GetMapId() && !self->GetEngine()->MapMngr.TransitToGlobal(self, 0, true)) {
        throw ScriptException("Transit to global failed");
    }
}

///# ...
///# param group ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToGlobalWithGroup(Critter* self, const vector<Critter*>& group)
{
    if (self->LockMapTransfers) {
        throw ScriptException("Transfers locked");
    }
    if (!self->GetMapId()) {
        throw ScriptException("Critter already on global");
    }

    if (!self->GetEngine()->MapMngr.TransitToGlobal(self, 0, true)) {
        throw ScriptException("Transit to global failed");
    }

    for (auto* cr_ : group) {
        if (cr_ != nullptr && !cr_->IsDestroyed) {
            self->GetEngine()->MapMngr.TransitToGlobal(cr_, self->GetId(), true);
        }
    }
}

///# ...
///# param leader ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_TransitToGlobalGroup(Critter* self, Critter* leader)
{
    if (self->LockMapTransfers) {
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

    if (!self->IsPlayer()) {
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
    self->Send_LoadMap(map, self->GetEngine()->MapMngr);
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
    else if (self->GetMapId()) {
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
        return; // throw ScriptException("Npc is not life";
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
[[maybe_unused]] void Server_Critter_SayMsgLex(Critter* self, uchar howSay, ushort textMsg, uint numStr, string_view lexems)
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
[[maybe_unused]] void Server_Critter_SetDir(Critter* self, uchar dir)
{
    if (dir >= self->GetEngine()->Settings.MapDirCount) {
        throw ScriptException("Invalid direction arg");
    }

    // Direction already set
    if (self->GetDir() == dir) {
        return;
    }

    self->SetDir(dir);

    if (self->GetMapId()) {
        self->Send_Dir(self);
        self->Broadcast_Dir();
    }
}

///# ...
///# param lookOnMe ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Critter_GetCritters(Critter* self, bool lookOnMe, uchar findType)
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
        const auto dist1 = self->GetEngine()->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = self->GetEngine()->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
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
        const auto dist1 = self->GetEngine()->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY());
        const auto dist2 = self->GetEngine()->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
        return dist1 < dist2;
    });

    return result;
}

///# ...
///# param cr ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsSeeCr(Critter* self, Critter* cr)
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
[[maybe_unused]] bool Server_Critter_IsSeenByCr(Critter* self, Critter* cr)
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
[[maybe_unused]] bool Server_Critter_IsSeeItem(Critter* self, Item* item)
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
[[maybe_unused]] uint Server_Critter_CountItem(Critter* self, hash protoId)
{
    return self->CountItemPid(protoId);
}

///# ...
///# param pid ...
///# param count ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_DeleteItem(Critter* self, hash pid, uint count)
{
    if (pid == 0u) {
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
[[maybe_unused]] Item* Server_Critter_AddItem(Critter* self, hash pid, uint count)
{
    if (pid == 0u) {
        throw ScriptException("Proto id arg is zero");
    }
    if (!self->GetEngine()->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid proto", _str().parseHash(pid));
    }

    return self->GetEngine()->ItemMngr.AddItemCritter(self, pid, count > 0 ? count : 1);
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItem(Critter* self, uint itemId)
{
    if (itemId == 0u) {
        return static_cast<Item*>(nullptr);
    }

    return self->GetItem(itemId, false);
}

///# ...
///# param predicate ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItemByPredicate(Critter* self, const std::function<bool(Item*)>& predicate)
{
    auto inv_items_copy = self->GetInventory(); // Make copy cuz predicate call can change inventory
    for (auto* item : inv_items_copy) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
            return item;
        }
    }

    return static_cast<Item*>(nullptr);
}

///# ...
///# param slot ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItemBySlot(Critter* self, int slot)
{
    return self->GetItemSlot(slot);
}

///# ...
///# param protoId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Critter_GetItemByPid(Critter* self, hash protoId)
{
    return self->GetEngine()->CrMngr.GetItemByPidInvPriority(self, protoId);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItems(Critter* self)
{
    return self->GetInventory();
}

///# ...
///# param slot ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItemsBySlot(Critter* self, int slot)
{
    return self->GetItemsSlot(slot);
}

///# ...
///# param predicate ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Critter_GetItemsByPredicate(Critter* self, const std::function<bool(Item*)>& predicate)
{
    auto inv_items = self->GetInventory();
    vector<Item*> items;
    items.reserve(inv_items.size());
    for (auto* item : inv_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
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

    auto* item = self->GetItem(itemId, self->IsPlayer());
    if (!item) {
        throw ScriptException("Item not found");
    }

    // To slot arg is equal of current item slot
    if (item->GetCritSlot() == slot) {
        return;
    }

    if (slot >= self->GetEngine()->Settings.CritterSlotEnabled.size() || !self->GetEngine()->Settings.CritterSlotEnabled[slot]) {
        throw ScriptException("Slot is not allowed");
    }

    if (!self->GetEngine()->CritterCheckMoveItemEvent.Raise(self, item, slot)) {
        throw ScriptException("Can't move item");
    }

    auto* item_swap = (slot ? self->GetItemSlot(slot) : nullptr);
    const auto from_slot = item->GetCritSlot();

    item->SetCritSlot(slot);
    if (item_swap) {
        item_swap->SetCritSlot(from_slot);
    }

    self->SendAndBroadcast_MoveItem(item, ACTION_MOVE_ITEM, from_slot);

    if (item_swap) {
        self->GetEngine()->CritterMoveItemEvent.Raise(self, item_swap, slot);
    }
    self->GetEngine()->CritterMoveItemEvent.Raise(self, item, from_slot);
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
[[maybe_unused]] void Server_Critter_Action(Critter* self, int action, int actionExt, Item* item)
{
    self->SendAndBroadcast_Action(action, actionExt, item);
}

///# ...
///# param anim1 ...
///# param anim2 ...
///# param item ...
///# param clearSequence ...
///# param delayPlay ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Server_Critter_Animate(Critter* self, uint anim1, uint anim2, Item* item, bool clearSequence, bool delayPlay)
{
    self->SendAndBroadcast_Animate(anim1, anim2, item, clearSequence, delayPlay);
}

///# ...
///# param cond ...
///# param anim1 ...
///# param anim2 ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetConditionAnims(Critter* self, CritterCondition cond, uint anim1, uint anim2)
{
    if (cond == CritterCondition::Alive) {
        self->SetAnim1Life(anim1);
        self->SetAnim2Life(anim2);
    }
    if (cond == CritterCondition::Knockout) {
        self->SetAnim1Knockout(anim1);
        self->SetAnim2Knockout(anim2);
    }
    if (cond == CritterCondition::Dead) {
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
///# param byId ...
///# param locNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Critter_IsKnownLocation(Critter* self, bool byId, uint locNum)
{
    if (byId) {
        return self->GetEngine()->MapMngr.CheckKnownLocById(self, locNum);
    }
    return self->GetEngine()->MapMngr.CheckKnownLocByPid(self, locNum);
}

///# ...
///# param byId ...
///# param locNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetKnownLocation(Critter* self, bool byId, uint locNum)
{
    auto* loc = (byId ? self->GetEngine()->MapMngr.GetLocation(locNum) : self->GetEngine()->MapMngr.GetLocationByPid(locNum, 0));
    if (!loc) {
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
            self->Send_GlobalMapFog(zx, zy, GM_FOG_HALF);
        }
    }
}

///# ...
///# param byId ...
///# param locNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_UnsetKnownLocation(Critter* self, bool byId, uint locNum)
{
    auto* loc = (byId ? self->GetEngine()->MapMngr.GetLocation(locNum) : self->GetEngine()->MapMngr.GetLocationByPid(locNum, 0));
    if (!loc) {
        throw ScriptException("Location not found");
    }
    if (!self->GetEngine()->MapMngr.CheckKnownLocById(self, loc->GetId())) {
        throw ScriptException("Player is not know this location");
    }

    self->GetEngine()->MapMngr.EraseKnownLoc(self, loc->GetId());

    if (!self->GetMapId()) {
        self->Send_GlobalLocation(loc, false);
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
///# param param ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ShowItems(Critter* self, const vector<Item*>& items, int param)
{
    // Critter is not player
    if (!self->IsPlayer()) {
        return;
    }

    self->Send_SomeItems(&items, param);
}

///# ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Server_Critter_Disconnect(Critter* self)
{
    if (!self->IsPlayer()) {
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
    if (!self->IsPlayer()) {
        throw ScriptException("Critter is not player");
    }

    return self->GetOwner() != nullptr;
}

///# ...
///# param funcName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_SetScript(Critter* self, string_view funcName)
{
    if (!funcName.empty()) {
        if (!self->SetScript(funcName, true)) {
            throw ScriptException("Script function not found");
        }
    }
    else {
        self->SetScriptId(0);
    }
}

///# ...
///# param func ...
///# param duration ...
///# param identifier ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_AddTimeEvent(Critter* self, const std::function<void(Critter*)>& func, uint duration, int identifier)
{
    /*hash func_num = self->GetEngine()->ScriptSys.BindScriptFuncNumByFunc(func);
    if (!func_num)
        throw ScriptException("Function not found");

    self->AddCrTimeEvent(func_num, 0, duration, identifier);*/
}

///# ...
///# param func ...
///# param duration ...
///# param identifier ...
///# param rate ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_AddTimeEventWithRate(Critter* self, const std::function<void(Critter*)>& func, uint duration, int identifier, uint rate)
{
    /*hash func_num = self->GetEngine()->ScriptSys.BindScriptFuncNumByFunc(func);
    if (!func_num)
        throw ScriptException("Function not found");

    self->AddCrTimeEvent(func_num, rate, duration, identifier);*/
}

///# ...
///# param identifier ...
///# param indexes ...
///# param durations ...
///# param rates ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_GetTimeEvents(Critter* self, int identifier, vector<int>& indexes, vector<int>& durations, vector<int>& rates)
{
    /*CScriptArray* te_identifier = self->GetTE_Identifier();
    UIntVec te_vec;
    for (uint i = 0, j = te_identifier->GetSize(); i < j; i++)
    {
        if (*(int*)te_identifier->At(i) == identifier)
            te_vec.push_back(i);
    }
    te_identifier->Release();

    uint size = (uint)te_vec.size();
    if (!size || (!indexes && !durations && !rates))
        return size;

    CScriptArray* te_next_time = nullptr;
    CScriptArray* te_rate = nullptr;

    uint indexes_size = 0, durations_size = 0, rates_size = 0;
    if (indexes)
    {
        indexes_size = indexes->GetSize();
        indexes->Resize(indexes_size + size);
    }
    if (durations)
    {
        te_next_time = self->GetTE_NextTime();
        RUNTIME_ASSERT(te_next_time->GetSize() == te_identifier->GetSize());
        durations_size = durations->GetSize();
        durations->Resize(durations_size + size);
    }
    if (rates)
    {
        te_rate = self->GetTE_Rate();
        RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());
        rates_size = rates->GetSize();
        rates->Resize(rates_size + size);
    }

    for (uint i = 0; i < size; i++)
    {
        if (indexes)
        {
            *(uint*)indexes->At(indexes_size + i) = te_vec[i];
        }
        if (durations)
        {
            uint next_time = *(uint*)te_next_time->At(te_vec[i]);
            *(uint*)durations->At(durations_size + i) =
                (next_time > self->GetEngine()->Settings.FullSecond ? next_time - self->GetEngine()->Settings.FullSecond : 0);
        }
        if (rates)
        {
            *(uint*)rates->At(rates_size + i) = *(uint*)te_rate->At(te_vec[i]);
        }
    }

    if (te_next_time)
        te_next_time->Release();
    if (te_rate)
        te_rate->Release();

    return size;*/
    return 0;
}

///# ...
///# param findIdentifiers ...
///# param identifiers ...
///# param indexes ...
///# param durations ...
///# param rates ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_GetTimeEventsExt(Critter* self, const vector<int>& findIdentifiers, const vector<int>& identifiers, vector<int>& indexes, vector<int>& durations, vector<int>& rates)
{
    /*IntVec find_vec;
    self->GetEngine()->ScriptSys.AssignScriptArrayInVector(find_vec, findIdentifiers);

    CScriptArray* te_identifier = self->GetTE_Identifier();
    UIntVec te_vec;
    for (uint i = 0, j = te_identifier->GetSize(); i < j; i++)
    {
        if (std::find(find_vec.begin(), find_vec.end(), *(int*)te_identifier->At(i)) != find_vec.end())
            te_vec.push_back(i);
    }

    uint size = (uint)te_vec.size();
    if (!size || (!identifiers && !indexes && !durations && !rates))
    {
        te_identifier->Release();
        return size;
    }

    CScriptArray* te_next_time = nullptr;
    CScriptArray* te_rate = nullptr;

    uint identifiers_size = 0, indexes_size = 0, durations_size = 0, rates_size = 0;
    if (identifiers)
    {
        identifiers_size = identifiers->GetSize();
        identifiers->Resize(identifiers_size + size);
    }
    if (indexes)
    {
        indexes_size = indexes->GetSize();
        indexes->Resize(indexes_size + size);
    }
    if (durations)
    {
        te_next_time = self->GetTE_NextTime();
        RUNTIME_ASSERT(te_next_time->GetSize() == te_identifier->GetSize());
        durations_size = durations->GetSize();
        durations->Resize(durations_size + size);
    }
    if (rates)
    {
        te_rate = self->GetTE_Rate();
        RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());
        rates_size = rates->GetSize();
        rates->Resize(rates_size + size);
    }

    for (uint i = 0; i < size; i++)
    {
        if (identifiers)
        {
            *(int*)identifiers->At(identifiers_size + i) = *(uint*)te_identifier->At(te_vec[i]);
        }
        if (indexes)
        {
            *(uint*)indexes->At(indexes_size + i) = te_vec[i];
        }
        if (durations)
        {
            uint next_time = *(uint*)te_next_time->At(te_vec[i]);
            *(uint*)durations->At(durations_size + i) =
                (next_time > self->GetEngine()->Settings.FullSecond ? next_time - self->GetEngine()->Settings.FullSecond : 0);
        }
        if (rates)
        {
            *(uint*)rates->At(rates_size + i) = *(uint*)te_rate->At(te_vec[i]);
        }
    }

    te_identifier->Release();
    if (te_next_time)
        te_next_time->Release();
    if (te_rate)
        te_rate->Release();

    return size;*/
    return 0;
}

///# ...
///# param index ...
///# param newDuration ...
///# param newRate ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ChangeTimeEvent(Critter* self, uint index, uint newDuration, uint newRate)
{
    /*CScriptArray* te_func_num = self->GetTE_FuncNum();
    CScriptArray* te_identifier = self->GetTE_Identifier();
    RUNTIME_ASSERT(te_func_num->GetSize() == te_identifier->GetSize());
    if (index >= te_func_num->GetSize())
    {
        te_func_num->Release();
        te_identifier->Release();
        throw ScriptException("Index arg is greater than maximum time events");
    }

    hash func_num = *(hash*)te_func_num->At(index);
    int identifier = *(int*)te_identifier->At(index);
    te_func_num->Release();
    te_identifier->Release();

    self->EraseCrTimeEvent(index);
    self->AddCrTimeEvent(func_num, newRate, newDuration, identifier);*/
}

///# ...
///# param index ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_EraseTimeEvent(Critter* self, uint index)
{
    /*CScriptArray* te_func_num = self->GetTE_FuncNum();
    uint size = te_func_num->GetSize();
    te_func_num->Release();
    if (index >= size)
        throw ScriptException("Index arg is greater than maximum time events");

    self->EraseCrTimeEvent(index);*/
}

///# ...
///# param identifier ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_EraseTimeEvents(Critter* self, int identifier)
{
    /*CScriptArray* te_next_time = self->GetTE_NextTime();
    CScriptArray* te_func_num = self->GetTE_FuncNum();
    CScriptArray* te_rate = self->GetTE_Rate();
    CScriptArray* te_identifier = self->GetTE_Identifier();
    RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
    RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
    RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());

    uint result = 0;
    for (uint i = 0; i < te_identifier->GetSize();)
    {
        if (identifier == *(int*)te_identifier->At(i))
        {
            result++;
            te_next_time->RemoveAt(i);
            te_func_num->RemoveAt(i);
            te_rate->RemoveAt(i);
            te_identifier->RemoveAt(i);
        }
        else
        {
            i++;
        }
    }

    self->SetTE_NextTime(te_next_time);
    self->SetTE_FuncNum(te_func_num);
    self->SetTE_Rate(te_rate);
    self->SetTE_Identifier(te_identifier);

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    return result;*/
    return 0;
}

///# ...
///# param identifiers ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Critter_EraseTimeEventsExt(Critter* self, const vector<int>& identifiers)
{
    /*IntVec identifiers_;
    self->GetEngine()->ScriptSys.AssignScriptArrayInVector(identifiers_, identifiers);

    CScriptArray* te_next_time = self->GetTE_NextTime();
    CScriptArray* te_func_num = self->GetTE_FuncNum();
    CScriptArray* te_rate = self->GetTE_Rate();
    CScriptArray* te_identifier = self->GetTE_Identifier();
    RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
    RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
    RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());

    uint result = 0;
    for (uint i = 0; i < te_func_num->GetSize();)
    {
        if (std::find(identifiers_.begin(), identifiers_.end(), *(int*)te_identifier->At(i)) != identifiers_.end())
        {
            result++;
            te_next_time->RemoveAt(i);
            te_func_num->RemoveAt(i);
            te_rate->RemoveAt(i);
            te_identifier->RemoveAt(i);
        }
        else
        {
            i++;
        }
    }

    self->SetTE_NextTime(te_next_time);
    self->SetTE_FuncNum(te_func_num);
    self->SetTE_Rate(te_rate);
    self->SetTE_Identifier(te_identifier);

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();

    return result;*/
    return 0;
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

    self->Moving = Critter::MovingData();
    self->Moving.TargId = target->GetId();
    self->Moving.HexX = target->GetHexX();
    self->Moving.HexY = target->GetHexY();
    self->Moving.Cut = cut;
    self->Moving.IsRun = isRun;
}

///# ...
///# param hx ...
///# param hy ...
///# param cut ...
///# param isRun ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_MoveToHex(Critter* self, ushort hx, ushort hy, uint cut, bool isRun)
{
    self->Moving = Critter::MovingData();
    self->Moving.HexX = hx;
    self->Moving.HexY = hy;
    self->Moving.Cut = cut;
    self->Moving.IsRun = isRun;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Server_Critter_GetMovingState(Critter* self)
{
    return self->Moving.State;
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Server_Critter_ResetMovingState(Critter* self)
{
    self->Moving = Critter::MovingData();
    self->Moving.State = 1;
}