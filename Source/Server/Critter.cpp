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

#include "Critter.h"
#include "CritterManager.h"
#include "Item.h"
#include "ItemManager.h"
#include "Location.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "ProtoManager.h"
#include "Settings.h"
#include "StringUtils.h"

#define FO_API_CRITTER_IMPL 1
#include "ScriptApi.h"

PROPERTIES_IMPL(Critter, "Critter", true);
#define FO_API_CRITTER_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(Critter, access, type, name, __VA_ARGS__);
#include "ScriptApi.h"

Critter::Critter(uint id, EntityType type, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time) : Entity(id, type, PropertiesRegistrator, proto), _settings {settings}, _geomHelper(_settings), _scriptSys {script_sys}, _gameTime {game_time}
{
}

auto Critter::IsFree() const -> bool
{
    return _gameTime.GameTick() - _startBreakTime >= _breakTime;
}

auto Critter::IsBusy() const -> bool
{
    return !IsFree();
}

void Critter::SetBreakTime(uint ms)
{
    _breakTime = ms;
    _startBreakTime = _gameTime.GameTick();
}

void Critter::SetBreakTimeDelta(uint ms)
{
    auto dt = _gameTime.GameTick() - _startBreakTime;
    if (dt > _breakTime) {
        dt -= _breakTime;
    }
    else {
        dt = 0;
    }
    if (dt > ms) {
        dt = 0;
    }
    SetBreakTime(ms - dt);
}

void Critter::SetWait(uint ms)
{
    _waitEndTick = _gameTime.GameTick() + ms;
}

auto Critter::IsWait() const -> bool
{
    return _gameTime.GameTick() < _waitEndTick;
}

auto Critter::GetAttackDist(Item* weap, uchar use) -> uint
{
    uint dist = 1;
    _scriptSys.CritterGetAttackDistantionEvent(this, weap, use, dist);
    return dist;
}

auto Critter::IsAlive() const -> bool
{
    return GetCond() == COND_ALIVE;
}

auto Critter::IsDead() const -> bool
{
    return GetCond() == COND_DEAD;
}

auto Critter::IsKnockout() const -> bool
{
    return GetCond() == COND_KNOCKOUT;
}

auto Critter::CheckFind(uchar find_type) const -> bool
{
    if (IsNpc()) {
        if (IsBitSet(find_type, FIND_ONLY_PLAYERS)) {
            return false;
        }
    }
    else {
        if (IsBitSet(find_type, FIND_ONLY_NPC)) {
            return false;
        }
    }
    return (IsBitSet(find_type, FIND_LIFE) && IsAlive()) || (IsBitSet(find_type, FIND_KO) && IsKnockout()) || (IsBitSet(find_type, FIND_DEAD) && IsDead());
}

void Critter::ClearVisible()
{
    for (auto* cr : VisCr) {
        auto it_ = std::find(cr->VisCrSelf.begin(), cr->VisCrSelf.end(), this);
        if (it_ != cr->VisCrSelf.end()) {
            cr->VisCrSelf.erase(it_);
            cr->VisCrSelfMap.erase(this->GetId());
        }
        cr->Send_RemoveCritter(this);
    }
    VisCr.clear();
    VisCrMap.clear();

    for (auto* cr : VisCrSelf) {
        auto it_ = std::find(cr->VisCr.begin(), cr->VisCr.end(), this);
        if (it_ != cr->VisCr.end()) {
            cr->VisCr.erase(it_);
            cr->VisCrMap.erase(this->GetId());
        }
    }
    VisCrSelf.clear();
    VisCrSelfMap.clear();

    VisCr1.clear();
    VisCr2.clear();
    VisCr3.clear();

    VisItem.clear();
}

auto Critter::GetCrSelf(uint crid) -> Critter*
{
    const auto it = VisCrSelfMap.find(crid);
    return it != VisCrSelfMap.end() ? it->second : nullptr;
}

auto Critter::GetCrFromVisCr(uchar find_type, bool vis_cr_self) -> vector<Critter*>
{
    auto& vis_cr = vis_cr_self ? VisCrSelf : VisCr;

    vector<Critter*> critters;
    for (auto* cr : vis_cr) {
        if (cr->CheckFind(find_type)) {
            critters.push_back(cr);
        }
    }
    return critters;
}

auto Critter::GetGlobalMapCritter(uint cr_id) const -> Critter*
{
    RUNTIME_ASSERT(GlobalMapGroup);
    const auto it = std::find_if(GlobalMapGroup->begin(), GlobalMapGroup->end(), [&cr_id](Critter* other) { return other->Id == cr_id; });
    return it != GlobalMapGroup->end() ? *it : nullptr;
}

auto Critter::AddCrIntoVisVec(Critter* add_cr) -> bool
{
    if (VisCrMap.count(add_cr->GetId()) != 0u) {
        return false;
    }

    VisCr.push_back(add_cr);
    VisCrMap.insert(std::make_pair(add_cr->GetId(), add_cr));

    add_cr->VisCrSelf.push_back(this);
    add_cr->VisCrSelfMap.insert(std::make_pair(this->GetId(), this));
    return true;
}

auto Critter::DelCrFromVisVec(Critter* del_cr) -> bool
{
    const auto it_map = VisCrMap.find(del_cr->GetId());
    if (it_map == VisCrMap.end()) {
        return false;
    }

    VisCrMap.erase(it_map);
    VisCr.erase(std::find(VisCr.begin(), VisCr.end(), del_cr));

    const auto it = std::find(del_cr->VisCrSelf.begin(), del_cr->VisCrSelf.end(), this);
    if (it != del_cr->VisCrSelf.end()) {
        del_cr->VisCrSelf.erase(it);
        del_cr->VisCrSelfMap.erase(this->GetId());
    }
    return true;
}

auto Critter::AddCrIntoVisSet1(uint crid) -> bool
{
    return VisCr1.insert(crid).second;
}

auto Critter::AddCrIntoVisSet2(uint crid) -> bool
{
    return VisCr2.insert(crid).second;
}

auto Critter::AddCrIntoVisSet3(uint crid) -> bool
{
    return VisCr3.insert(crid).second;
}

auto Critter::DelCrFromVisSet1(uint crid) -> bool
{
    return VisCr1.erase(crid) != 0;
}

auto Critter::DelCrFromVisSet2(uint crid) -> bool
{
    return VisCr2.erase(crid) != 0;
}

auto Critter::DelCrFromVisSet3(uint crid) -> bool
{
    return VisCr3.erase(crid) != 0;
}

auto Critter::AddIdVisItem(uint item_id) -> bool
{
    return VisItem.insert(item_id).second;
}

auto Critter::DelIdVisItem(uint item_id) -> bool
{
    return VisItem.erase(item_id) != 0;
}

auto Critter::CountIdVisItem(uint item_id) const -> bool
{
    return VisItem.count(item_id) != 0;
}

void Critter::SetItem(Item* item)
{
    RUNTIME_ASSERT(item);

    _invItems.push_back(item);

    if (item->GetAccessory() != ITEM_ACCESSORY_CRITTER) {
        item->SetCritSlot(0);
    }
    item->SetAccessory(ITEM_ACCESSORY_CRITTER);
    item->SetCritId(Id);
}

auto Critter::GetItem(uint item_id, bool skip_hide) -> Item*
{
    if (item_id == 0u) {
        return nullptr;
    }

    for (auto* item : _invItems) {
        if (item->GetId() == item_id) {
            if (skip_hide && item->GetIsHidden()) {
                return nullptr;
            }
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetItemByPid(hash item_pid) -> Item*
{
    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetItemByPidSlot(hash item_pid, int slot) -> Item*
{
    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid && item->GetCritSlot() == slot) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetItemSlot(int slot) -> Item*
{
    for (auto* item : _invItems) {
        if (item->GetCritSlot() == slot) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetItemsSlot(int slot) -> vector<Item*>
{
    vector<Item*> items;
    items.reserve(_invItems.size());

    for (auto* item : _invItems) {
        if (slot < 0 || item->GetCritSlot() == slot) {
            items.push_back(item);
        }
    }
    return items;
}

auto Critter::CountItemPid(hash pid) -> uint
{
    uint res = 0;
    for (auto* item : _invItems) {
        if (item->GetProtoId() == pid) {
            res += item->GetCount();
        }
    }
    return res;
}

auto Critter::CountItems() -> uint
{
    uint count = 0;
    for (auto* item : _invItems) {
        count += item->GetCount();
    }
    return count;
}

auto Critter::GetInventory() -> vector<Item*>&
{
    return _invItems;
}

auto Critter::IsHaveGeckItem() -> bool
{
    for (auto* item : _invItems) {
        if (item->GetIsGeck()) {
            return true;
        }
    }
    return false;
}

auto Critter::SetScript(const string& /*func*/, bool /*first_time*/) -> bool
{
    /*hash func_num = 0;
    if (func)
    {
        func_num = scriptSys.BindScriptFuncNumByFunc(func);
        if (!func_num)
        {
            WriteLog("Script bind fail, critter '{}'.\n", GetName());
            return false;
        }
        SetScriptId(func_num);
    }
    else
    {
        func_num = GetScriptId();
    }

    if (func_num)
    {
        scriptSys.PrepareScriptFuncContext(func_num, GetName());
        scriptSys.SetArgEntity(this);
        scriptSys.SetArgBool(first_time);
        scriptSys.RunPrepared();
    }*/
    return true;
}

void Critter::Send_Property(NetProperty::Type type, Property* prop, Entity* entity)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Property(type, prop, entity);
    }
}
void Critter::Send_Move(Critter* from_cr, uint move_params)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Move(from_cr, move_params);
    }
}
void Critter::Send_Dir(Critter* from_cr)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Dir(from_cr);
    }
}
void Critter::Send_AddCritter(Critter* cr)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_AddCritter(cr);
    }
}
void Critter::Send_RemoveCritter(Critter* cr)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_RemoveCritter(cr);
    }
}
void Critter::Send_LoadMap(Map* map, MapManager& map_mngr)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_LoadMap(map, map_mngr);
    }
}
void Critter::Send_XY(Critter* cr)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_XY(cr);
    }
}
void Critter::Send_AddItemOnMap(Item* item)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_AddItemOnMap(item);
    }
}
void Critter::Send_EraseItemFromMap(Item* item)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_EraseItemFromMap(item);
    }
}
void Critter::Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_AnimateItem(item, from_frm, to_frm);
    }
}
void Critter::Send_AddItem(Item* item)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_AddItem(item);
    }
}
void Critter::Send_EraseItem(Item* item)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_EraseItem(item);
    }
}
void Critter::Send_GlobalInfo(uchar flags, MapManager& map_mngr)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_GlobalInfo(flags, map_mngr);
    }
}
void Critter::Send_GlobalLocation(Location* loc, bool add)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_GlobalLocation(loc, add);
    }
}
void Critter::Send_GlobalMapFog(ushort zx, ushort zy, uchar fog)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_GlobalMapFog(zx, zy, fog);
    }
}
void Critter::Send_AllProperties()
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_AllProperties();
    }
}
void Critter::Send_CustomCommand(Critter* cr, ushort cmd, int val)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_CustomCommand(cr, cmd, val);
    }
}
void Critter::Send_Talk()
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Talk();
    }
}
void Critter::Send_GameInfo(Map* map)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_GameInfo(map);
    }
}
void Critter::Send_Text(Critter* from_cr, const string& text, uchar how_say)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Text(from_cr, text, how_say);
    }
}
void Critter::Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_TextEx(from_id, text, how_say, unsafe_text);
    }
}
void Critter::Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_TextMsg(from_cr, str_num, how_say, num_msg);
    }
}
void Critter::Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_TextMsg(from_id, str_num, how_say, num_msg);
    }
}
void Critter::Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_TextMsgLex(from_cr, num_str, how_say, num_msg, lexems);
    }
}
void Critter::Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_TextMsgLex(from_id, num_str, how_say, num_msg, lexems);
    }
}
void Critter::Send_Action(Critter* from_cr, int action, int action_ext, Item* item)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Action(from_cr, action, action_ext, item);
    }
}
void Critter::Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_MoveItem(from_cr, item, action, prev_slot);
    }
}
void Critter::Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Animate(from_cr, anim1, anim2, item, clear_sequence, delay_play);
    }
}
void Critter::Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_SetAnims(from_cr, cond, anim1, anim2);
    }
}
void Critter::Send_CombatResult(uint* combat_res, uint len)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_CombatResult(combat_res, len);
    }
}
void Critter::Send_AutomapsInfo(void* locs_vec, Location* loc)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_AutomapsInfo(locs_vec, loc);
    }
}
void Critter::Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_Effect(eff_pid, hx, hy, radius);
    }
}
void Critter::Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_FlyEffect(eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy);
    }
}
void Critter::Send_PlaySound(uint crid_synchronize, const string& sound_name)
{
    if (IsPlayer()) {
        dynamic_cast<Client*>(this)->Send_PlaySound(crid_synchronize, sound_name);
    }
}

void Critter::SendA_Property(NetProperty::Type type, Property* prop, Entity* entity)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_Property(type, prop, entity);
        }
    }
}

void Critter::SendA_Move(uint move_params)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_Move(this, move_params);
        }
    }
}

void Critter::SendA_XY()
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_XY(this);
        }
    }
}

void Critter::SendA_Action(int action, int action_ext, Item* item)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_Action(this, action, action_ext, item);
        }
    }
}

void Critter::SendAA_Action(int action, int action_ext, Item* item)
{
    if (IsPlayer()) {
        Send_Action(this, action, action_ext, item);
    }

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_Action(this, action, action_ext, item);
        }
    }
}

void Critter::SendAA_MoveItem(Item* item, uchar action, uchar prev_slot)
{
    if (IsPlayer()) {
        Send_MoveItem(this, item, action, prev_slot);
    }

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_MoveItem(this, item, action, prev_slot);
        }
    }
}

void Critter::SendAA_Animate(uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play)
{
    if (IsPlayer()) {
        Send_Animate(this, anim1, anim2, item, clear_sequence, delay_play);
    }

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_Animate(this, anim1, anim2, item, clear_sequence, delay_play);
        }
    }
}

void Critter::SendAA_SetAnims(int cond, uint anim1, uint anim2)
{
    if (IsPlayer()) {
        Send_SetAnims(this, cond, anim1, anim2);
    }

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_SetAnims(this, cond, anim1, anim2);
        }
    }
}

void Critter::SendAA_Text(const vector<Critter*>& to_cr, const string& text, uchar how_say, bool unsafe_text)
{
    if (text.empty()) {
        return;
    }

    const auto from_id = GetId();

    if (IsPlayer()) {
        Send_TextEx(from_id, text, how_say, unsafe_text);
    }

    if (to_cr.empty()) {
        return;
    }

    auto dist = -1;
    if (how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD) {
        dist = _settings.ShoutDist + GetMultihex();
    }
    else if (how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD) {
        dist = _settings.WhisperDist + GetMultihex();
    }

    for (auto* cr : to_cr) {
        if (cr == this || !cr->IsPlayer()) {
            continue;
        }

        if (dist == -1) {
            cr->Send_TextEx(from_id, text, how_say, unsafe_text);
        }
        else if (_geomHelper.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextEx(from_id, text, how_say, unsafe_text);
        }
    }
}

void Critter::SendAA_Msg(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg)
{
    if (IsPlayer()) {
        Send_TextMsg(this, num_str, how_say, num_msg);
    }

    if (to_cr.empty()) {
        return;
    }

    auto dist = -1;
    if (how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD) {
        dist = _settings.ShoutDist + GetMultihex();
    }
    else if (how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD) {
        dist = _settings.WhisperDist + GetMultihex();
    }

    for (auto* cr : to_cr) {
        if (cr == this) {
            continue;
        }
        if (!cr->IsPlayer()) {
            continue;
        }

        // SYNC_LOCK(cr);

        if (dist == -1) {
            cr->Send_TextMsg(this, num_str, how_say, num_msg);
        }
        else if (_geomHelper.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextMsg(this, num_str, how_say, num_msg);
        }
    }
}

void Critter::SendAA_MsgLex(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems)
{
    if (IsPlayer()) {
        Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);
    }

    if (to_cr.empty()) {
        return;
    }

    auto dist = -1;
    if (how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD) {
        dist = _settings.ShoutDist + GetMultihex();
    }
    else if (how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD) {
        dist = _settings.WhisperDist + GetMultihex();
    }

    for (auto* cr : to_cr) {
        if (cr == this) {
            continue;
        }
        if (!cr->IsPlayer()) {
            continue;
        }

        // SYNC_LOCK(cr);

        if (dist == -1) {
            cr->Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);
        }
        else if (_geomHelper.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);
        }
    }
}

void Critter::SendA_Dir()
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_Dir(this);
        }
    }
}

void Critter::SendA_CustomCommand(ushort num_param, int val)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (cr->IsPlayer()) {
            cr->Send_CustomCommand(this, num_param, val);
        }
    }
}

void Critter::Send_AddAllItems()
{
    if (!IsPlayer()) {
        return;
    }

    auto* cl = dynamic_cast<Client*>(this);
    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_CLEAR_ITEMS;
    CLIENT_OUTPUT_END(cl);

    for (auto* item : _invItems) {
        Send_AddItem(item);
    }

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_ALL_ITEMS_SEND;
    CLIENT_OUTPUT_END(cl);
}

void Critter::Send_AllAutomapsInfo(MapManager& map_mngr)
{
    if (!IsPlayer()) {
        return;
    }

    vector<Location*> locs;
    for (auto loc_id : GetKnownLocations()) {
        auto* loc = map_mngr.GetLocation(loc_id);
        if (loc != nullptr && loc->IsNonEmptyAutomaps()) {
            locs.push_back(loc);
        }
    }

    Send_AutomapsInfo(&locs, nullptr);
}

void Critter::SendMessage(int num, int val, int to, MapManager& map_mngr)
{
    switch (to) {
    case MESSAGE_TO_VISIBLE_ME: {
        auto critters = VisCr;
        for (auto* cr : critters) {
            _scriptSys.CritterMessageEvent(cr, this, num, val);
        }
    } break;
    case MESSAGE_TO_IAM_VISIBLE: {
        auto critters = VisCrSelf;
        for (auto* cr : critters) {
            _scriptSys.CritterMessageEvent(cr, this, num, val);
        }
    } break;
    case MESSAGE_TO_ALL_ON_MAP: {
        auto* map = map_mngr.GetMap(GetMapId());
        if (map == nullptr) {
            break;
        }

        auto critters = map->GetCritters();
        for (auto* cr : critters) {
            _scriptSys.CritterMessageEvent(cr, this, num, val);
        }
    } break;
    default:
        break;
    }
}

auto Critter::IsTransferTimeouts(bool send) -> bool
{
    if (GetTimeoutTransfer() > _gameTime.GetFullSecond()) {
        if (send) {
            Send_TextMsg(this, STR_TIMEOUT_TRANSFER_WAIT, SAY_NETMSG, TEXTMSG_GAME);
        }
        return true;
    }
    if (GetTimeoutBattle() > _gameTime.GetFullSecond()) {
        if (send) {
            Send_TextMsg(this, STR_TIMEOUT_BATTLE_WAIT, SAY_NETMSG, TEXTMSG_GAME);
        }
        return true;
    }
    return false;
}

void Critter::AddCrTimeEvent(hash /*func_num*/, uint /*rate*/, uint duration, int /*identifier*/) const
{
    if (duration != 0u) {
        duration += _gameTime.GetFullSecond();
    }

    /*CScriptArray* te_next_time = GetTE_NextTime();
    CScriptArray* te_func_num = GetTE_FuncNum();
    CScriptArray* te_rate = GetTE_Rate();
    CScriptArray* te_identifier = GetTE_Identifier();
    RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
    RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
    RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());

    uint i = 0;
    for (uint j = te_func_num->GetSize(); i < j; i++)
        if (duration < *(uint*)te_next_time->At(i))
            break;

    te_next_time->InsertAt(i, &duration);
    te_func_num->InsertAt(i, &func_num);
    te_rate->InsertAt(i, &rate);
    te_identifier->InsertAt(i, &identifier);

    SetTE_NextTime(te_next_time);
    SetTE_FuncNum(te_func_num);
    SetTE_Rate(te_rate);
    SetTE_Identifier(te_identifier);

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();*/
}

void Critter::EraseCrTimeEvent(int index)
{
    /*CScriptArray* te_next_time = GetTE_NextTime();
    CScriptArray* te_func_num = GetTE_FuncNum();
    CScriptArray* te_rate = GetTE_Rate();
    CScriptArray* te_identifier = GetTE_Identifier();
    RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
    RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
    RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());

    if (index < (int)te_next_time->GetSize())
    {
        te_next_time->RemoveAt(index);
        te_func_num->RemoveAt(index);
        te_rate->RemoveAt(index);
        te_identifier->RemoveAt(index);

        SetTE_NextTime(te_next_time);
        SetTE_FuncNum(te_func_num);
        SetTE_Rate(te_rate);
        SetTE_Identifier(te_identifier);
    }

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();*/
}

void Critter::ContinueTimeEvents(int offs_time)
{
    /*CScriptArray* te_next_time = GetTE_NextTime();
    if (te_next_time->GetSize() > 0)
    {
        for (uint i = 0, j = te_next_time->GetSize(); i < j; i++)
            *(uint*)te_next_time->At(i) += offs_time;

        SetTE_NextTime(te_next_time);
    }
    te_next_time->Release();*/
}

Client::Client(NetConnection* conn, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time) : Critter(0, EntityType::Client, proto, settings, script_sys, game_time)
{
    Connection = conn;
    CritterIsNpc = false;
    SetBit(Flags, FCRIT_PLAYER);
    _pingNextTick = _gameTime.FrameTick() + PING_CLIENT_LIFE_TIME;
    _talkNextTick = _gameTime.GameTick() + PROCESS_TALK_TICK;
    LastActivityTime = _gameTime.FrameTick();
}

Client::~Client()
{
    delete Connection;
}

auto Client::GetIp() const -> uint
{
    return Connection->Ip;
}

auto Client::GetIpStr() const -> const char*
{
    return Connection->Host.c_str();
}

auto Client::GetPort() const -> ushort
{
    return Connection->Port;
}

auto Client::IsOnline() const -> bool
{
    return !Connection->IsDisconnected;
}

auto Client::IsOffline() const -> bool
{
    return Connection->IsDisconnected;
}

void Client::Disconnect() const
{
    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_DISCONNECT;
    CLIENT_OUTPUT_END(this);
}

void Client::RemoveFromGame()
{
    CanBeRemoved = true;
}

auto Client::GetOfflineTime() const -> uint
{
    return static_cast<uint>(Timer::RealtimeTick()) - Connection->DisconnectTick;
}

auto Client::IsToPing() const -> bool
{
    return State == ClientState::Playing && _gameTime.FrameTick() >= _pingNextTick && !(GetTimeoutTransfer() > _gameTime.GetFullSecond());
}

void Client::PingClient()
{
    if (!_pingOk) {
        Connection->Disconnect();
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_PING;
    Connection->Bout << static_cast<uchar> PING_CLIENT;
    CLIENT_OUTPUT_END(this);

    _pingNextTick = _gameTime.FrameTick() + PING_CLIENT_LIFE_TIME;
    _pingOk = false;
}

void Client::PingOk(uint next_ping)
{
    _pingOk = true;
    _pingNextTick = _gameTime.FrameTick() + next_ping;
}

void Client::Send_AddCritter(Critter* cr)
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    const auto is_npc = cr->IsNpc();
    auto msg = is_npc ? NETMSG_ADD_NPC : NETMSG_ADD_PLAYER;
    uint msg_len = sizeof(msg) + sizeof(msg_len) + sizeof(uint) + sizeof(ushort) * 2 + sizeof(uchar) + sizeof(int) + sizeof(uint) * 6 + sizeof(uint) + (is_npc ? sizeof(hash) : UTF8_BUF_SIZE(MAX_NAME));

    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = cr->Props.StoreData(IsBitSet(cr->Flags, FCRIT_CHOSEN), &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CLIENT_OUTPUT_BEGIN(this);

    Connection->Bout << msg;
    Connection->Bout << msg_len;
    Connection->Bout << cr->GetId();
    Connection->Bout << cr->GetHexX();
    Connection->Bout << cr->GetHexY();
    Connection->Bout << cr->GetDir();
    Connection->Bout << cr->GetCond();
    Connection->Bout << cr->GetAnim1Life();
    Connection->Bout << cr->GetAnim1Knockout();
    Connection->Bout << cr->GetAnim1Dead();
    Connection->Bout << cr->GetAnim2Life();
    Connection->Bout << cr->GetAnim2Knockout();
    Connection->Bout << cr->GetAnim2Dead();
    Connection->Bout << cr->Flags;

    if (is_npc) {
        const auto* npc = dynamic_cast<Npc*>(cr);
        Connection->Bout << npc->GetProtoId();
    }
    else {
        const auto* cl = dynamic_cast<Client*>(cr);
        Connection->Bout.Push(cl->Name.c_str(), UTF8_BUF_SIZE(MAX_NAME));
    }

    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes)

    CLIENT_OUTPUT_END(this);

    if (cr != this) {
        Send_MoveItem(cr, nullptr, ACTION_REFRESH, 0);
    }
}

void Client::Send_RemoveCritter(Critter* cr) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_REMOVE_CRITTER;
    Connection->Bout << cr->GetId();
    CLIENT_OUTPUT_END(this);
}

void Client::Send_LoadMap(Map* map, MapManager& map_mngr)
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    Location* loc = nullptr;
    hash pid_map = 0;
    hash pid_loc = 0;
    uchar map_index_in_loc = 0;
    auto map_time = -1;
    uchar map_rain = 0;
    hash hash_tiles = 0;
    hash hash_scen = 0;

    if (map == nullptr) {
        map = map_mngr.GetMap(GetMapId());
    }

    if (map != nullptr) {
        loc = map->GetLocation();
        pid_map = map->GetProtoId();
        pid_loc = loc->GetProtoId();
        map_index_in_loc = static_cast<uchar>(loc->GetMapIndex(pid_map));
        map_time = map->GetCurDayTime();
        map_rain = map->GetRainCapacity();
        hash_tiles = map->GetStaticMap()->HashTiles;
        hash_scen = map->GetStaticMap()->HashScen;
    }

    uint msg_len = sizeof(uint) + sizeof(uint) + sizeof(hash) * 2 + sizeof(uchar) + sizeof(int) + sizeof(uchar) + sizeof(hash) * 2;
    vector<uchar*>* map_data = nullptr;
    vector<uint>* map_data_sizes = nullptr;
    vector<uchar*>* loc_data = nullptr;
    vector<uint>* loc_data_sizes = nullptr;
    if (map != nullptr) {
        const auto map_whole_data_size = map->Props.StoreData(false, &map_data, &map_data_sizes);
        const auto loc_whole_data_size = loc->Props.StoreData(false, &loc_data, &loc_data_sizes);
        msg_len += sizeof(ushort) + map_whole_data_size + sizeof(ushort) + loc_whole_data_size;
    }

    CLIENT_OUTPUT_BEGIN(this);

    Connection->Bout << NETMSG_LOADMAP;
    Connection->Bout << msg_len;
    Connection->Bout << pid_map;
    Connection->Bout << pid_loc;
    Connection->Bout << map_index_in_loc;
    Connection->Bout << map_time;
    Connection->Bout << map_rain;
    Connection->Bout << hash_tiles;
    Connection->Bout << hash_scen;
    if (map != nullptr) {
        NET_WRITE_PROPERTIES(Connection->Bout, map_data, map_data_sizes)
        NET_WRITE_PROPERTIES(Connection->Bout, loc_data, loc_data_sizes)
    }

    CLIENT_OUTPUT_END(this);

    State = ClientState::Transferring;
}

void Client::Send_Property(NetProperty::Type type, Property* prop, Entity* entity)
{
    RUNTIME_ASSERT(entity);

    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint additional_args = 0;
    switch (type) {
    case NetProperty::Critter:
        additional_args = 1;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        break;
    case NetProperty::CritterItem:
        additional_args = 2;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        break;
    default:
        break;
    }

    uint data_size = 0;
    void* data = entity->Props.GetRawData(prop, data_size);

    CLIENT_OUTPUT_BEGIN(this);

    const auto is_pod = prop->IsPOD();
    if (is_pod) {
        Connection->Bout << NETMSG_POD_PROPERTY(data_size, additional_args);
    }
    else {
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(char) + additional_args * sizeof(uint) + sizeof(ushort) + data_size;

        Connection->Bout << NETMSG_COMPLEX_PROPERTY;
        Connection->Bout << msg_len;
    }

    Connection->Bout << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        Connection->Bout << dynamic_cast<Item*>(entity)->GetCritId();
        Connection->Bout << entity->Id;
        break;
    case NetProperty::Critter:
        Connection->Bout << entity->Id;
        break;
    case NetProperty::MapItem:
        Connection->Bout << entity->Id;
        break;
    case NetProperty::ChosenItem:
        Connection->Bout << entity->Id;
        break;
    default:
        break;
    }

    if (is_pod) {
        Connection->Bout << static_cast<ushort>(prop->GetRegIndex());
        Connection->Bout.Push(data, data_size);
    }
    else {
        Connection->Bout << static_cast<ushort>(prop->GetRegIndex());
        if (data_size != 0u) {
            Connection->Bout.Push(data, data_size);
        }
    }

    CLIENT_OUTPUT_END(this);
}

void Client::Send_Move(Critter* from_cr, uint move_params) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_MOVE;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << move_params;
    Connection->Bout << from_cr->GetHexX();
    Connection->Bout << from_cr->GetHexY();
    CLIENT_OUTPUT_END(this);
}

void Client::Send_Dir(Critter* from_cr) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_DIR;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << from_cr->GetDir();
    CLIENT_OUTPUT_END(this);
}

void Client::Send_Action(Critter* from_cr, int action, int action_ext, Item* item) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    Send_XY(from_cr);

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_ACTION;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << action_ext;
    Connection->Bout << (item != nullptr);
    CLIENT_OUTPUT_END(this);
}

void Client::Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(action) + sizeof(prev_slot) + sizeof(bool);

    auto& inv = from_cr->GetInventory();
    vector<Item*> items;
    items.reserve(inv.size());
    for (auto* item_ : inv) {
        const auto slot = item_->GetCritSlot();
        if (slot < _settings.CritterSlotEnabled.size() && _settings.CritterSlotEnabled[slot] && slot < _settings.CritterSlotSendData.size() && _settings.CritterSlotSendData[slot]) {
            items.push_back(item_);
        }
    }

    msg_len += sizeof(ushort);
    vector<vector<uchar*>*> items_data(items.size());
    vector<vector<uint>*> items_data_sizes(items.size());
    for (const auto i : xrange(items)) {
        const auto whole_data_size = items[i]->Props.StoreData(false, &items_data[i], &items_data_sizes[i]);
        msg_len += sizeof(uchar) + sizeof(uint) + sizeof(hash) + sizeof(ushort) + whole_data_size;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_MOVE_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << prev_slot;
    Connection->Bout << (item != nullptr);
    Connection->Bout << static_cast<ushort>(items.size());
    for (const auto i : xrange(items)) {
        auto* item_ = items[i];
        Connection->Bout << item_->GetCritSlot();
        Connection->Bout << item_->GetId();
        Connection->Bout << item_->GetProtoId();
        NET_WRITE_PROPERTIES(Connection->Bout, items_data[i], items_data_sizes[i])
    }
    CLIENT_OUTPUT_END(this);
}

void Client::Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (clear_sequence) {
        Send_XY(from_cr);
    }
    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_ANIMATE;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << anim1;
    Connection->Bout << anim2;
    Connection->Bout << (item != nullptr);
    Connection->Bout << clear_sequence;
    Connection->Bout << delay_play;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_SET_ANIMS;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << cond;
    Connection->Bout << anim1;
    Connection->Bout << anim2;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_AddItemOnMap(Item* item) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    const auto is_added = item->ViewPlaceOnMap;
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(hash) + sizeof(ushort) * 2 + sizeof(bool);

    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = item->Props.StoreData(false, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_ADD_ITEM_ON_MAP;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetHexX();
    Connection->Bout << item->GetHexY();
    Connection->Bout << is_added;
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes)
    CLIENT_OUTPUT_END(this);
}

void Client::Send_EraseItemFromMap(Item* item) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_ERASE_ITEM_FROM_MAP;
    Connection->Bout << item->GetId();
    Connection->Bout << item->ViewPlaceOnMap;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_ANIMATE_ITEM;
    Connection->Bout << item->GetId();
    Connection->Bout << from_frm;
    Connection->Bout << to_frm;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_AddItem(Item* item) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (item->GetIsHidden()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(hash) + sizeof(uchar);
    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = item->Props.StoreData(true, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_ADD_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetCritSlot();
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes)
    CLIENT_OUTPUT_END(this);
}

void Client::Send_EraseItem(Item* item) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_REMOVE_ITEM;
    Connection->Bout << item->GetId();
    CLIENT_OUTPUT_END(this);
}

void Client::Send_SomeItems(const vector<Item*>* items, int param) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(param) + sizeof(bool) + sizeof(uint);

    vector<vector<uchar*>*> items_data(items != nullptr ? items->size() : 0);
    vector<vector<uint>*> items_data_sizes(items != nullptr ? items->size() : 0);
    for (size_t i = 0, j = items_data.size(); i < j; i++) {
        const auto whole_data_size = items->at(i)->Props.StoreData(false, &items_data[i], &items_data_sizes[i]);
        msg_len += sizeof(uint) + sizeof(hash) + sizeof(ushort) + whole_data_size;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_SOME_ITEMS;
    Connection->Bout << msg_len;
    Connection->Bout << param;
    Connection->Bout << (items == nullptr);
    Connection->Bout << static_cast<uint>(items != nullptr ? items->size() : 0);
    for (size_t i = 0, j = items_data.size(); i < j; i++) {
        const auto* item = items->at(i);
        Connection->Bout << item->GetId();
        Connection->Bout << item->GetProtoId();
        NET_WRITE_PROPERTIES(Connection->Bout, items_data[i], items_data_sizes[i])
    }
    CLIENT_OUTPUT_END(this);
}

#define SEND_LOCATION_SIZE (sizeof(uint) + sizeof(hash) + sizeof(ushort) * 2 + sizeof(ushort) + sizeof(uint) + sizeof(uchar))
void Client::Send_GlobalInfo(uchar info_flags, MapManager& map_mngr)
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (LockMapTransfers != 0) {
        return;
    }

    auto known_locs = GetKnownLocations();

    // Calculate length of message
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(info_flags);

    auto loc_count = static_cast<ushort>(known_locs.size());
    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        msg_len += sizeof(loc_count) + SEND_LOCATION_SIZE * loc_count;
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        msg_len += GM_ZONES_FOG_SIZE;
    }

    // Parse message
    CLIENT_OUTPUT_BEGIN(this);

    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        Connection->Bout << loc_count;

        char empty_loc[SEND_LOCATION_SIZE] = {0, 0, 0, 0};
        for (ushort i = 0; i < loc_count;) {
            auto loc_id = known_locs[i];
            auto* loc = map_mngr.GetLocation(loc_id);
            if (loc != nullptr && !loc->GetToGarbage()) {
                i++;
                Connection->Bout << loc_id;
                Connection->Bout << loc->GetProtoId();
                Connection->Bout << loc->GetWorldX();
                Connection->Bout << loc->GetWorldY();
                Connection->Bout << loc->GetRadius();
                Connection->Bout << loc->GetColor();
                uchar count = 0;
                if (loc->IsNonEmptyMapEntrances()) {
                    count = static_cast<uchar>(loc->GetMapEntrances().size() / 2);
                }
                Connection->Bout << count;
            }
            else {
                loc_count--;
                map_mngr.EraseKnownLoc(this, loc_id);
                Connection->Bout.Push(empty_loc, sizeof(empty_loc));
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        auto gmap_fog = GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }
        Connection->Bout.Push(&gmap_fog[0], GM_ZONES_FOG_SIZE);
    }

    CLIENT_OUTPUT_END(this);

    if (IsBitSet(info_flags, GM_INFO_CRITTERS)) {
        map_mngr.ProcessVisibleCritters(this);
    }
}

void Client::Send_GlobalLocation(Location* loc, bool add) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    auto info_flags = GM_INFO_LOCATION;
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(info_flags) + sizeof(add) + SEND_LOCATION_SIZE;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;
    Connection->Bout << add;
    Connection->Bout << loc->GetId();
    Connection->Bout << loc->GetProtoId();
    Connection->Bout << loc->GetWorldX();
    Connection->Bout << loc->GetWorldY();
    Connection->Bout << loc->GetRadius();
    Connection->Bout << loc->GetColor();
    uchar count = 0;
    if (loc->IsNonEmptyMapEntrances()) {
        count = static_cast<uchar>(loc->GetMapEntrances().size() / 2);
    }
    Connection->Bout << count;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_GlobalMapFog(ushort zx, ushort zy, uchar fog) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    auto info_flags = GM_INFO_FOG;
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(info_flags) + sizeof(zx) + sizeof(zy) + sizeof(fog);

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;
    Connection->Bout << zx;
    Connection->Bout << zy;
    Connection->Bout << fog;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_XY(Critter* cr) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_XY;
    Connection->Bout << cr->GetId();
    Connection->Bout << cr->GetHexX();
    Connection->Bout << cr->GetHexY();
    Connection->Bout << cr->GetDir();
    CLIENT_OUTPUT_END(this);
}

void Client::Send_AllProperties()
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(uint);

    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = Props.StoreData(true, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_ALL_PROPERTIES;
    Connection->Bout << msg_len;
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes)
    CLIENT_OUTPUT_END(this);
}

void Client::Send_CustomCommand(Critter* cr, ushort cmd, int val) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CUSTOM_COMMAND;
    Connection->Bout << cr->GetId();
    Connection->Bout << cmd;
    Connection->Bout << val;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_Talk()
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    const auto close = Talk.Type == TalkType::None;
    auto is_npc = static_cast<uchar>(Talk.Type == TalkType::Npc);
    auto talk_id = is_npc != 0u ? Talk.TalkNpc : Talk.DialogPackId;
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(is_npc) + sizeof(talk_id) + sizeof(uchar);

    CLIENT_OUTPUT_BEGIN(this);

    Connection->Bout << NETMSG_TALK_NPC;

    if (close) {
        Connection->Bout << msg_len;
        Connection->Bout << is_npc;
        Connection->Bout << talk_id;
        Connection->Bout << static_cast<uchar>(0);
    }
    else {
        const auto all_answers = static_cast<uchar>(Talk.CurDialog.Answers.size());
        msg_len += sizeof(uint) + sizeof(uint) * all_answers + sizeof(uint) + sizeof(ushort) + static_cast<uint>(Talk.Lexems.length());

        auto talk_time = Talk.TalkTime;
        if (talk_time != 0u) {
            const auto diff = _gameTime.GameTick() - Talk.StartTick;
            talk_time = diff < talk_time ? talk_time - diff : 1;
        }

        Connection->Bout << msg_len;
        Connection->Bout << is_npc;
        Connection->Bout << talk_id;
        Connection->Bout << all_answers;
        Connection->Bout << static_cast<ushort>(Talk.Lexems.length()); // Lexems length
        if (Talk.Lexems.length() != 0u) {
            Connection->Bout.Push(Talk.Lexems.c_str(), static_cast<uint>(Talk.Lexems.length())); // Lexems string
        }
        Connection->Bout << Talk.CurDialog.TextId; // Main text_id
        for (auto& answer : Talk.CurDialog.Answers) {
            Connection->Bout << answer.TextId; // Answers text_id
        }
        Connection->Bout << talk_time; // Talk time
    }

    CLIENT_OUTPUT_END(this);
}

void Client::Send_GameInfo(Map* /*map*/) const
{
    if (IsSendDisabled() || IsOffline()) {
    }

    /*int time = (map ? map->GetCurDayTime() : -1);
    uchar rain = (map ? map->GetRainCapacity() : 0);
    bool no_log_out = (map ? map->GetIsNoLogOut() : true);

    int day_time[4];
    uchar day_color[12];
    CScriptArray* day_time_arr = (map ? map->GetDayTime() : nullptr);
    CScriptArray* day_color_arr = (map ? map->GetDayColor() : nullptr);
    RUNTIME_ASSERT(!day_time_arr || day_time_arr->GetSize() == 0 || day_time_arr->GetSize() == 4);
    RUNTIME_ASSERT(!day_color_arr || day_color_arr->GetSize() == 0 || day_color_arr->GetSize() == 12);
    if (day_time_arr && day_time_arr->GetSize() > 0)
        std::memcpy(day_time, day_time_arr->At(0), sizeof(day_time));
    else
        memzero(day_time, sizeof(day_time));
    if (day_color_arr && day_color_arr->GetSize() > 0)
        std::memcpy(day_color, day_color_arr->At(0), sizeof(day_color));
    else
        memzero(day_color, sizeof(day_color));
    if (day_time_arr)
        day_time_arr->Release();
    if (day_color_arr)
        day_color_arr->Release();

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_GAME_INFO;
    Connection->Bout << Globals->GetYearStart();
    Connection->Bout << Globals->GetYear();
    Connection->Bout << Globals->GetMonth();
    Connection->Bout << Globals->GetDay();
    Connection->Bout << Globals->GetHour();
    Connection->Bout << Globals->GetMinute();
    Connection->Bout << Globals->GetSecond();
    Connection->Bout << Globals->GetTimeMultiplier();
    Connection->Bout << time;
    Connection->Bout << rain;
    Connection->Bout << no_log_out;
    Connection->Bout.Push(day_time, sizeof(day_time));
    Connection->Bout.Push(day_color, sizeof(day_color));
    CLIENT_OUTPUT_END(this);*/
}

void Client::Send_Text(Critter* from_cr, const string& text, uchar how_say) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (text.empty()) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : 0;
    Send_TextEx(from_id, text, how_say, false);
}

void Client::Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(from_id) + sizeof(how_say) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(text.length()) + sizeof(unsafe_text);

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_CRITTER_TEXT;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_TextMsg(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }
    const auto from_id = from_cr != nullptr ? from_cr->GetId() : 0;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_MSG;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_TextMsg(uint from_id, uint num_str, uchar how_say, ushort num_msg) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_MSG;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }

    auto lex_len = static_cast<ushort>(strlen(lexems));
    if (lex_len == 0u || lex_len > MAX_DLG_LEXEMS_TEXT) {
        Send_TextMsg(from_cr, num_str, how_say, num_msg);
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : 0;
    const uint msg_len = NETMSG_MSG_SIZE + sizeof(lex_len) + lex_len;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lex_len;
    Connection->Bout.Push(lexems, lex_len);
    CLIENT_OUTPUT_END(this);
}

void Client::Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }

    auto lex_len = static_cast<ushort>(strlen(lexems));
    if (lex_len == 0u || lex_len > MAX_DLG_LEXEMS_TEXT) {
        Send_TextMsg(from_id, num_str, how_say, num_msg);
        return;
    }

    const uint msg_len = NETMSG_MSG_SIZE + sizeof(lex_len) + lex_len;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lex_len;
    Connection->Bout.Push(lexems, lex_len);
    CLIENT_OUTPUT_END(this);
}

void Client::Send_MapText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(hx) + sizeof(hy) + sizeof(color) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(text.length()) + sizeof(unsafe_text);

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_MAP_TEXT;
    Connection->Bout << msg_len;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_MAP_TEXT_MSG;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(ushort) * 2 + sizeof(uint) + sizeof(ushort) + sizeof(uint) + sizeof(lexems_len) + lexems_len;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_MAP_TEXT_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems_len;
    if (lexems_len != 0u) {
        Connection->Bout.Push(lexems, lexems_len);
    }
    CLIENT_OUTPUT_END(this);
}

void Client::Send_CombatResult(uint* combat_res, uint len) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }
    if (combat_res == nullptr || len > _settings.FloodSize / sizeof(uint)) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(len) + len * sizeof(uint);

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_COMBAT_RESULTS;
    Connection->Bout << msg_len;
    Connection->Bout << len;
    if (len != 0u) {
        Connection->Bout.Push(combat_res, len * sizeof(uint));
    }
    CLIENT_OUTPUT_END(this);
}

void Client::Send_AutomapsInfo(void* /*locs_vec*/, Location* /*loc*/) const
{
    if (IsSendDisabled() || IsOffline()) {
    }

    /*if (locs_vec)
    {
        LocationVec* locs = (LocationVec*)locs_vec;
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(bool) + sizeof(ushort);
        for (uint i = 0, j = (uint)locs->size(); i < j; i++)
        {
            Location* loc_ = (*locs)[i];
            msg_len += sizeof(uint) + sizeof(hash) + sizeof(ushort);
            if (loc_->IsNonEmptyAutomaps())
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                msg_len += (sizeof(hash) + sizeof(uchar)) * (uint)automaps->GetSize();
                automaps->Release();
            }
        }

        CLIENT_OUTPUT_BEGIN(this);
        Connection->Bout << NETMSG_AUTOMAPS_INFO;
        Connection->Bout << msg_len;
        Connection->Bout << (bool)true; // Clear list
        Connection->Bout << (ushort)locs->size();
        for (uint i = 0, j = (uint)locs->size(); i < j; i++)
        {
            Location* loc_ = (*locs)[i];
            Connection->Bout << loc_->GetId();
            Connection->Bout << loc_->GetProtoId();
            if (loc_->IsNonEmptyAutomaps())
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                Connection->Bout << (ushort)automaps->GetSize();
                for (uint k = 0, l = (uint)automaps->GetSize(); k < l; k++)
                {
                    hash pid = *(hash*)automaps->At(k);
                    Connection->Bout << pid;
                    Connection->Bout << (uchar)loc_->GetMapIndex(pid);
                }
                automaps->Release();
            }
            else
            {
                Connection->Bout << (ushort)0;
            }
        }
        CLIENT_OUTPUT_END(this);
    }

    if (loc)
    {
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(bool) + sizeof(ushort) + sizeof(uint) + sizeof(hash) +
            sizeof(ushort);
        CScriptArray* automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr);
        if (automaps)
            msg_len += (sizeof(hash) + sizeof(uchar)) * (uint)automaps->GetSize();

        CLIENT_OUTPUT_BEGIN(this);
        Connection->Bout << NETMSG_AUTOMAPS_INFO;
        Connection->Bout << msg_len;
        Connection->Bout << (bool)false; // Append this information
        Connection->Bout << (ushort)1;
        Connection->Bout << loc->GetId();
        Connection->Bout << loc->GetProtoId();
        Connection->Bout << (ushort)(automaps ? automaps->GetSize() : 0);
        if (automaps)
        {
            for (uint i = 0, j = (uint)automaps->GetSize(); i < j; i++)
            {
                hash pid = *(hash*)automaps->At(i);
                Connection->Bout << pid;
                Connection->Bout << (uchar)loc->GetMapIndex(pid);
            }
        }
        CLIENT_OUTPUT_END(this);

        if (automaps)
            automaps->Release();
    }*/
}

void Client::Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_EFFECT;
    Connection->Bout << eff_pid;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << radius;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_FLY_EFFECT;
    Connection->Bout << eff_pid;
    Connection->Bout << from_crid;
    Connection->Bout << to_crid;
    Connection->Bout << from_hx;
    Connection->Bout << from_hy;
    Connection->Bout << to_hx;
    Connection->Bout << to_hy;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_PlaySound(uint crid_synchronize, const string& sound_name) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(crid_synchronize) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(sound_name.length());

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_PLAY_SOUND;
    Connection->Bout << msg_len;
    Connection->Bout << crid_synchronize;
    Connection->Bout << sound_name;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_ViewMap() const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_VIEW_MAP;
    Connection->Bout << ViewMapHx;
    Connection->Bout << ViewMapHy;
    Connection->Bout << ViewMapLocId;
    Connection->Bout << ViewMapLocEnt;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_SomeItem(Item* item) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(hash) + sizeof(uchar);
    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = item->Props.StoreData(false, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << NETMSG_SOME_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes)
    CLIENT_OUTPUT_END(this);
}

void Client::Send_CustomMessage(uint msg) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << msg;
    CLIENT_OUTPUT_END(this);
}

void Client::Send_CustomMessage(uint msg, uchar* data, uint data_size) const
{
    if (IsSendDisabled() || IsOffline()) {
        return;
    }

    uint msg_len = sizeof(msg) + sizeof(msg_len) + data_size;

    CLIENT_OUTPUT_BEGIN(this);
    Connection->Bout << msg;
    Connection->Bout << msg_len;
    if (data_size != 0u) {
        Connection->Bout.Push(data, data_size);
    }
    CLIENT_OUTPUT_END(this);
}

auto Client::IsTalking() const -> bool
{
    return Talk.Type != TalkType::None;
}

Npc::Npc(uint id, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time) : Critter(id, EntityType::Npc, proto, settings, script_sys, game_time)
{
    CritterIsNpc = true;
    SetBit(Flags, FCRIT_NPC);
}

auto Npc::GetTalkedPlayers() const -> uint
{
    uint talk = 0;
    for (auto* cr : VisCr) {
        if (!cr->IsPlayer()) {
            continue;
        }
        auto* cl = dynamic_cast<Client*>(cr);
        if (cl->Talk.Type == TalkType::Npc && cl->Talk.TalkNpc == GetId()) {
            talk++;
        }
    }
    return talk;
}

auto Npc::IsTalkedPlayers() const -> bool
{
    for (auto* cr : VisCr) {
        if (!cr->IsPlayer()) {
            continue;
        }
        auto* cl = dynamic_cast<Client*>(cr);
        if (cl->Talk.Type == TalkType::Npc && cl->Talk.TalkNpc == GetId()) {
            return true;
        }
    }
    return false;
}

auto Npc::GetBarterPlayers() const -> uint
{
    uint barter = 0;
    for (auto* cr : VisCr) {
        if (!cr->IsPlayer()) {
            continue;
        }
        auto* cl = dynamic_cast<Client*>(cr);
        if (cl->Talk.Type == TalkType::Npc && cl->Talk.TalkNpc == GetId() && cl->Talk.Barter) {
            barter++;
        }
    }
    return barter;
}

auto Npc::IsFreeToTalk() const -> bool
{
    auto max_talkers = GetMaxTalkers();
    if (max_talkers == 0u) {
        max_talkers = _settings.NpcMaxTalkers;
    }

    return GetTalkedPlayers() < max_talkers;
}
