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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

///@ ExportMethod ExcludeInSingleplayer
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
FO_SCRIPT_API void Server_Critter_TransitToHex(Critter* self, mpos hex, uint8 dir)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }

    auto* map = self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
    RUNTIME_ASSERT(map);

    if (!map->GetSize().IsValidPos(hex)) {
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

        self->GetEngine()->MapMngr.TransitToMap(self, map, hex, self->GetDir(), 2);
    }
    else if (self->GetDir() != corrected_dir) {
        self->ChangeDir(corrected_dir);
        self->Send_Dir(self);
        self->Broadcast_Dir();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransitToMap(Critter* self, Map* map, mpos hex, uint8 dir)
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

    self->GetEngine()->MapMngr.TransitToMap(self, map, hex, corrected_dir, 2);

    const auto* loc = map->GetLocation();
    const auto wpos1 = ipos {self->GetWorldPos().x, self->GetWorldPos().y};
    const auto wpos2 = ipos {loc->GetWorldPos().x, loc->GetWorldPos().y};

    if (loc != nullptr && GenericUtils::DistSqrt(wpos1, wpos2) > loc->GetRadius()) {
        self->SetWorldPos(loc->GetWorldPos());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransitToMap(Critter* self, Map* map, mpos hex, uint8 dir, bool force_hex)
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
        self->GetEngine()->MapMngr.TransitToMap(self, map, hex, corrected_dir, std::nullopt);
    }
    else {
        self->GetEngine()->MapMngr.TransitToMap(self, map, hex, corrected_dir, 2);
    }

    const auto* loc = map->GetLocation();
    const auto loc_pos = loc->GetWorldPos();
    const auto cr_pos = self->GetWorldPos();

    if (GenericUtils::DistSqrt(ipos {loc_pos.x, loc_pos.y}, ipos {cr_pos.x, cr_pos.y}) > loc->GetRadius()) {
        self->SetWorldPos(loc->GetWorldPos());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransitToGlobal(Critter* self)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }

    if (!self->GetMapId()) {
        return;
    }

    self->GetEngine()->MapMngr.TransitToGlobal(self, {});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransitToGlobalWithGroup(Critter* self, const vector<Critter*>& group)
{
    if (self->LockMapTransfers != 0) {
        throw ScriptException("Transfers locked");
    }

    self->GetEngine()->MapMngr.TransitToGlobal(self, {});

    for (auto* cr : group) {
        if (cr != nullptr && !cr->IsDestroyed() && !self->IsDestroyed() && !self->GetMapId()) {
            self->GetEngine()->MapMngr.TransitToGlobal(cr, self->GetId());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_TransitToGlobalGroup(Critter* self, Critter* globalCr)
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

    self->GetEngine()->MapMngr.TransitToGlobal(self, globalCr->GetId());
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
FO_SCRIPT_API void Server_Critter_ViewMap(Critter* self, Map* map, uint look, mpos hex, uint8 dir)
{
    if (map == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!map->GetSize().IsValidPos(hex)) {
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
    self->ViewMapLook = static_cast<uint16>(look_);
    self->ViewMapHex = hex;
    self->ViewMapDir = dir_;
    self->ViewMapLocId = ident_t {};
    self->ViewMapLocEnt = 0;
    self->Send_LoadMap(map);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Say(Critter* self, uint8 howSay, string_view text)
{
    if (howSay != SAY_FLASH_WINDOW && text.empty()) {
        return;
    }
    if (!self->GetControlledByPlayer() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_Text(self, howSay != SAY_FLASH_WINDOW ? text : " ", howSay);
    }
    else if (self->GetMapId()) {
        self->SendAndBroadcast_Text(self->VisCr, text, howSay, false);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SayMsg(Critter* self, uint8 howSay, TextPackName textPack, uint numStr)
{
    if (!self->GetControlledByPlayer() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_TextMsg(self, howSay, textPack, numStr);
    }
    else if (self->GetMapId()) {
        self->SendAndBroadcast_Msg(self->VisCr, howSay, textPack, numStr);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SayMsg(Critter* self, uint8 howSay, TextPackName textPack, uint numStr, string_view lexems)
{
    if (!self->GetControlledByPlayer() && !self->IsAlive()) {
        return;
    }

    if (howSay >= SAY_NETMSG) {
        self->Send_TextMsgLex(self, howSay, textPack, numStr, lexems);
    }
    else if (self->GetMapId()) {
        self->SendAndBroadcast_MsgLex(self->VisCr, howSay, textPack, numStr, lexems);
    }
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
FO_SCRIPT_API vector<Critter*> Server_Critter_GetCritters(Critter* self, bool lookOnMe, CritterFindType findType)
{
    vector<Critter*> critters;

    for (auto* cr : lookOnMe ? self->VisCr : self->VisCrSelf) {
        if (cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    std::sort(critters.begin(), critters.end(), [hex = self->GetHex()](Critter* cr1, Critter* cr2) {
        const auto dist1 = GeometryHelper::DistGame(hex, cr1->GetHex());
        const auto dist2 = GeometryHelper::DistGame(hex, cr2->GetHex());

        return dist1 < dist2;
    });

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetTalkingCritters(Critter* self)
{
    vector<Critter*> result;

    for (auto* cr : self->VisCr) {
        if (cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == self->GetId()) {
            result.push_back(cr);
        }
    }

    std::sort(result.begin(), result.end(), [hex = self->GetHex()](Critter* cr1, Critter* cr2) {
        const auto dist1 = GeometryHelper::DistGame(hex, cr1->GetHex());
        const auto dist2 = GeometryHelper::DistGame(hex, cr2->GetHex());

        return dist1 < dist2;
    });

    return result;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Critter_GetTalkingCrittersCount(Critter* self)
{
    uint result = 0;

    for (const auto* cr : self->VisCr) {
        if (cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == self->GetId()) {
            result++;
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetGlobalMapGroupCritters(Critter* self)
{
    if (self->GetMapId()) {
        throw ScriptException("Critter is not on global map");
    }

    RUNTIME_ASSERT(self->GlobalMapGroup);

    return *self->GlobalMapGroup;
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

    const auto& critters = self->GetMapId() ? self->VisCrSelf : *self->GlobalMapGroup;

    return std::find(critters.begin(), critters.end(), cr) != critters.end();
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

    const auto& critters = self->GetMapId() ? self->VisCr : *self->GlobalMapGroup;
    return std::find(critters.begin(), critters.end(), cr) != critters.end();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsSee(Critter* self, Item* item)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }

    return self->CountIdVisItem(item->GetId());
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Critter_CountItem(Critter* self, hstring protoId)
{
    return self->CountInvItemPid(protoId);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(Critter* self, hstring pid)
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

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_DestroyItem(Critter* self, hstring pid, uint count)
{
    if (!pid) {
        throw ScriptException("Proto id arg is zero");
    }

    if (count == 0) {
        return;
    }

    self->GetEngine()->ItemMngr.SubItemCritter(self, pid, count);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_AddItem(Critter* self, hstring pid, uint count)
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
    for (auto* item : self->GetInvItems()) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item;
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Critter_GetItem(Critter* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    for (auto* item : self->GetInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self)
{
    return self->GetInvItems();
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, ItemComponent component)
{
    vector<Item*> items;
    items.reserve(self->GetInvItems().size());

    for (auto* item : self->GetInvItems()) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            items.push_back(item);
        }
    }

    return items;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    vector<Item*> items;
    items.reserve(self->GetInvItems().size());

    for (auto* item : self->GetInvItems()) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.push_back(item);
        }
    }

    return items;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Critter_GetItems(Critter* self, hstring protoId)
{
    vector<Item*> items;
    items.reserve(self->GetInvItems().size());

    for (auto* item : self->GetInvItems()) {
        if (item->GetProtoId() == protoId) {
            items.push_back(item);
        }
    }

    return items;
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
        auto* item_swap = self->GetInvItemSlot(slot);
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
        RUNTIME_ASSERT(map);

        map->RemoveCritterFromField(self);
    }

    self->SetCondition(cond);

    if (map != nullptr) {
        map->AddCritterToField(self);
    }

    if (cond == CritterCondition::Dead) {
        self->SendAndBroadcast_Action(CritterAction::Dead, static_cast<int>(actionAnim), dynamic_cast<Item*>(contextItem));
    }
    else if (cond == CritterCondition::Knockout) {
        self->SendAndBroadcast_Action(CritterAction::Knockout, static_cast<int>(actionAnim), dynamic_cast<Item*>(contextItem));
    }
    else if (cond == CritterCondition::Alive) {
        if (prev_cond == CritterCondition::Knockout) {
            self->SendAndBroadcast_Action(CritterAction::StandUp, static_cast<int>(actionAnim), dynamic_cast<Item*>(contextItem));
        }
        else {
            self->SendAndBroadcast_Action(CritterAction::Respawn, static_cast<int>(actionAnim), dynamic_cast<Item*>(contextItem));
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_CloseDialog(Critter* self)
{
    if (self->IsTalking()) {
        self->GetEngine()->CrMngr.CloseTalk(self);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Action(Critter* self, CritterAction action, int actionData, AbstractItem* contextItem)
{
    self->SendAndBroadcast_Action(action, actionData, dynamic_cast<Item*>(contextItem));
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API void Server_Critter_Animate(Critter* self, CritterStateAnim stateAnim, CritterActionAnim actionAnim, AbstractItem* contextItem, bool clearSequence, bool delayPlay)
{
    self->SendAndBroadcast_Animate(stateAnim, actionAnim, dynamic_cast<Item*>(contextItem), clearSequence, delayPlay);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetConditionAnims(Critter* self, CritterCondition cond, CritterStateAnim stateAnim, CritterActionAnim actionAnim)
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

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_PlaySound(Critter* self, string_view soundName, bool sendSelf)
{
    if (sendSelf) {
        self->Send_PlaySound(self->GetId(), soundName);
    }

    for (auto it = self->VisCr.begin(), end = self->VisCr.end(); it != end; ++it) {
        auto* cr_ = *it;
        cr_->Send_PlaySound(self->GetId(), soundName);
    }
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Critter_IsKnownLocation(Critter* self, ident_t locId)
{
    if (!locId) {
        throw ScriptException("Invalid location id");
    }

    return self->GetEngine()->MapMngr.CheckKnownLoc(self, locId);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetKnownLocation(Critter* self, ident_t locId)
{
    if (!locId) {
        throw ScriptException("Invalid location id");
    }

    if (self->GetEngine()->MapMngr.CheckKnownLoc(self, locId)) {
        return;
    }

    const auto* loc = self->GetEngine()->EntityMngr.GetLocation(locId);

    if (loc == nullptr) {
        throw ScriptException("Location not found");
    }

    self->GetEngine()->MapMngr.AddKnownLoc(self, loc->GetId());

    if (!self->GetMapId()) {
        self->Send_GlobalLocation(loc, true);
    }

    const auto zx = loc->GetWorldPos().x / self->GetEngine()->Settings.GlobalMapZoneLength;
    const auto zy = loc->GetWorldPos().y / self->GetEngine()->Settings.GlobalMapZoneLength;

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

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RemoveKnownLocation(Critter* self, ident_t locId)
{
    if (!locId) {
        throw ScriptException("Invalid location id");
    }

    if (!self->GetEngine()->MapMngr.CheckKnownLoc(self, locId)) {
        return;
    }

    self->GetEngine()->MapMngr.RemoveKnownLoc(self, locId);

    if (!self->GetMapId()) {
        if (const auto* loc = self->GetEngine()->EntityMngr.GetLocation(locId); loc != nullptr) {
            self->Send_GlobalLocation(loc, false);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_SetFog(Critter* self, uint16 zoneX, uint16 zoneY, int fog)
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

///@ ExportMethod
FO_SCRIPT_API int Server_Critter_GetFog(Critter* self, uint16 zoneX, uint16 zoneY)
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

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API void Server_Critter_Disconnect(Critter* self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    if (const auto* player = self->GetPlayer(); player != nullptr) {
        player->Connection->GracefulDisconnect();
    }
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API bool Server_Critter_IsOnline(Critter* self)
{
    if (!self->GetControlledByPlayer()) {
        throw ScriptException("Critter is not player");
    }

    return self->GetPlayer() != nullptr;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_MoveToCritter(Critter* self, Critter* target, uint cut, uint speed)
{
    if (target == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::InProgress;
    self->TargetMoving.TargId = target->GetId();
    self->TargetMoving.TargHex = target->GetHex();
    self->TargetMoving.Cut = cut;
    self->TargetMoving.Speed = static_cast<uint16>(speed);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_MoveToHex(Critter* self, mpos hex, uint cut, uint speed)
{
    self->TargetMoving = {};
    self->TargetMoving.State = MovingState::InProgress;
    self->TargetMoving.TargHex = hex;
    self->TargetMoving.Cut = cut;
    self->TargetMoving.Speed = static_cast<uint16>(speed);
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
    self->TargetMoving.State = MovingState::Success;

    if (self->IsMoving()) {
        self->ClearMove();
        self->SendAndBroadcast_Moving();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeMovingSpeed(Critter* self, int speed)
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
    for (auto* cr : copy(self->AttachedCritters)) {
        cr->DetachFromCritter();
    }

    self->GetEngine()->MapMngr.ProcessVisibleCritters(self);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Critter_GetAttachedCritters(Critter* self)
{
    return self->AttachedCritters;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_AddTimeEvent(Critter* self, ScriptFunc<uint, Critter*, any_t, uint*> func, tick_t duration, any_t identifier)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    self->AddTimeEvent(func.GetName(), 0, duration, identifier);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_AddTimeEvent(Critter* self, ScriptFunc<uint, Critter*, any_t, uint*> func, tick_t duration, any_t identifier, uint rate)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    self->AddTimeEvent(func.GetName(), rate, duration, identifier);
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Critter_GetTimeEvents(Critter* self, any_t identifier)
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

///@ ExportMethod
FO_SCRIPT_API uint Server_Critter_GetTimeEvents(Critter* self, any_t identifier, vector<uint>& indexes, vector<tick_t>& durations, vector<uint>& rates)
{
    auto&& te_identifiers = self->GetTE_Identifier();
    auto&& te_fire_times = self->GetTE_FireTime();
    auto&& te_rates = self->GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    const auto server_time = self->GetEngine()->GameTime.GetServerTime();

    uint count = 0;

    for (size_t i = 0; i < te_identifiers.size(); i++) {
        if (te_identifiers[i] == identifier) {
            indexes.push_back(static_cast<uint>(i));
            durations.emplace_back(te_fire_times[i].underlying_value() > server_time.underlying_value() ? te_fire_times[i].underlying_value() - server_time.underlying_value() : 0);
            rates.push_back(te_rates[i]);
            count++;
        }
    }

    return count;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Critter_GetTimeEvents(Critter* self, const vector<any_t>& findIdentifiers, vector<any_t>& identifiers, vector<uint>& indexes, vector<tick_t>& durations, vector<uint>& rates)
{
    auto&& te_identifiers = self->GetTE_Identifier();
    auto&& te_fire_times = self->GetTE_FireTime();
    auto&& te_rates = self->GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    const auto server_time = self->GetEngine()->GameTime.GetServerTime();

    uint count = 0;

    for (const auto& identifier : findIdentifiers) {
        for (size_t i = 0; i < te_identifiers.size(); i++) {
            if (te_identifiers[i] == identifier) {
                identifiers.push_back(te_identifiers[i]);
                indexes.push_back(static_cast<uint>(i));
                durations.emplace_back(te_fire_times[i].underlying_value() > server_time.underlying_value() ? te_fire_times[i].underlying_value() - server_time.underlying_value() : 0);
                rates.push_back(te_rates[i]);
                count++;
            }
        }
    }

    return count;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_ChangeTimeEvent(Critter* self, uint index, tick_t newDuration, uint newRate)
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

    self->RemoveTimeEvent(index);
    self->AddTimeEvent(te_func_names[index], newRate, newDuration, te_identifiers[index]);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RemoveTimeEvent(Critter* self, uint index)
{
    auto&& te_identifiers = self->GetTE_Identifier();

    if (index >= te_identifiers.size()) {
        throw ScriptException("Index arg is greater than maximum time events");
    }

    self->RemoveTimeEvent(index);
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Critter_RemoveTimeEvents(Critter* self, any_t identifier)
{
    auto&& te_identifiers = self->GetTE_Identifier();

    uint count = 0;
    size_t index = 0;

    for (const auto& te_identifier : te_identifiers) {
        if (te_identifier == identifier) {
            self->RemoveTimeEvent(index);
            count++;
        }
        else {
            index++;
        }
    }

    return count;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Critter_RemoveTimeEvents(Critter* self, const vector<any_t>& identifiers)
{
    uint count = 0;

    for (const auto& identifier : identifiers) {
        auto&& te_identifiers = self->GetTE_Identifier();
        size_t index = 0;

        for (const auto& te_identifier : te_identifiers) {
            if (te_identifier == identifier) {
                self->RemoveTimeEvent(index);
                count++;
            }
            else {
                index++;
            }
        }
    }

    return count;
}

///@ ExportMethod
FO_SCRIPT_API tick_t Server_Critter_GetPlayerOfflineTime(Critter* self)
{
    return tick_t {time_duration_to_ms<tick_t::underlying_type>(self->GetOfflineTime())};
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_RefreshDialogTime(Critter* self)
{
    self->Talk.StartTime = self->GetEngine()->GameTime.GameplayTime();
    self->Talk.TalkTime = std::chrono::milliseconds {self->Talk.Barter ? self->GetEngine()->Settings.DlgBarterMaxTime : self->GetEngine()->Settings.DlgTalkMaxTime};
}
