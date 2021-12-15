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
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "Server.h"
#include "Settings.h"

Critter::Critter(FOServer* engine, uint id, Player* owner, const ProtoCritter* proto) : ServerEntity(engine, id, engine->GetPropertyRegistrator("Critter"), proto), _player {owner}
{
    if (_player != nullptr) {
        _player->AddRef();
    }

    SetBit(Flags, _player == nullptr ? FCRIT_NPC : FCRIT_PLAYER);
}

Critter::~Critter()
{
    if (_player) {
        _player->Release();
    }
}

auto Critter::GetOfflineTime() const -> uint
{
    return _playerDetached ? _engine->GameTime.FrameTick() - _playerDetachTick : 0u;
}

auto Critter::GetAttackDist(Item* weap, uchar use) -> uint
{
    uint dist = 1;
    _engine->CritterGetAttackDistantionEvent.Raise(this, weap, use, dist);
    return dist;
}

auto Critter::IsAlive() const -> bool
{
    return GetCond() == CritterCondition::Alive;
}

auto Critter::IsDead() const -> bool
{
    return GetCond() == CritterCondition::Dead;
}

auto Critter::IsKnockout() const -> bool
{
    return GetCond() == CritterCondition::Knockout;
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

void Critter::DetachPlayer()
{
    RUNTIME_ASSERT(_player != nullptr);
    RUNTIME_ASSERT(!_playerDetached);

    _player->Release();
    _player = nullptr;
    _playerDetached = true;
    _playerDetachTick = _engine->GameTime.FrameTick();
}

void Critter::AttachPlayer(Player* owner)
{
    RUNTIME_ASSERT(_player == nullptr);
    RUNTIME_ASSERT(_playerDetached);

    _playerDetached = false;
    _player = owner;
    _player->AddRef();
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
    auto& vis_cr = (vis_cr_self ? VisCrSelf : VisCr);

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
    const auto it = std::find_if(GlobalMapGroup->begin(), GlobalMapGroup->end(), [&cr_id](Critter* other) { return other->GetId() == cr_id; });
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
    item->SetCritId(GetId());
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

auto Critter::SetScript(string_view /*func*/, bool /*first_time*/) -> bool
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

void Critter::Broadcast_Property(NetProperty::Type type, Property* prop, ServerEntity* entity)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Move(uint move_params)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Move(this, move_params);
    }
}

void Critter::Broadcast_Position()
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Position(this);
    }
}

void Critter::Broadcast_Action(int action, int action_ext, Item* item)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Action(this, action, action_ext, item);
    }
}

void Critter::Broadcast_Dir()
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Dir(this);
    }
}

void Critter::Broadcast_CustomCommand(ushort num_param, int val)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_CustomCommand(this, num_param, val);
    }
}

void Critter::SendAndBroadcast_Action(int action, int action_ext, Item* item)
{
    Send_Action(this, action, action_ext, item);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Action(this, action, action_ext, item);
    }
}

void Critter::SendAndBroadcast_MoveItem(Item* item, uchar action, uchar prev_slot)
{
    Send_MoveItem(this, item, action, prev_slot);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_MoveItem(this, item, action, prev_slot);
    }
}

void Critter::SendAndBroadcast_Animate(uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play)
{
    Send_Animate(this, anim1, anim2, item, clear_sequence, delay_play);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Animate(this, anim1, anim2, item, clear_sequence, delay_play);
    }
}

void Critter::SendAndBroadcast_SetAnims(CritterCondition cond, uint anim1, uint anim2)
{
    Send_SetAnims(this, cond, anim1, anim2);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_SetAnims(this, cond, anim1, anim2);
    }
}

void Critter::SendAndBroadcast_Text(const vector<Critter*>& to_cr, string_view text, uchar how_say, bool unsafe_text)
{
    if (text.empty()) {
        return;
    }

    const auto from_id = GetId();

    Send_TextEx(from_id, text, how_say, unsafe_text);

    if (to_cr.empty()) {
        return;
    }

    auto dist = -1;
    if (how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD) {
        dist = _engine->Settings.ShoutDist + GetMultihex();
    }
    else if (how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD) {
        dist = _engine->Settings.WhisperDist + GetMultihex();
    }

    for (auto* cr : to_cr) {
        if (cr == this) {
            continue;
        }

        if (dist == -1) {
            cr->Send_TextEx(from_id, text, how_say, unsafe_text);
        }
        else if (_engine->GeomHelper.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextEx(from_id, text, how_say, unsafe_text);
        }
    }
}

void Critter::SendAndBroadcast_Msg(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg)
{
    Send_TextMsg(this, num_str, how_say, num_msg);

    if (to_cr.empty()) {
        return;
    }

    auto dist = -1;
    if (how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD) {
        dist = _engine->Settings.ShoutDist + GetMultihex();
    }
    else if (how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD) {
        dist = _engine->Settings.WhisperDist + GetMultihex();
    }

    for (auto* cr : to_cr) {
        if (cr == this) {
            continue;
        }

        if (dist == -1) {
            cr->Send_TextMsg(this, num_str, how_say, num_msg);
        }
        else if (_engine->GeomHelper.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextMsg(this, num_str, how_say, num_msg);
        }
    }
}

void Critter::SendAndBroadcast_MsgLex(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg, string_view lexems)
{
    Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);

    if (to_cr.empty()) {
        return;
    }

    auto dist = -1;
    if (how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD) {
        dist = _engine->Settings.ShoutDist + GetMultihex();
    }
    else if (how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD) {
        dist = _engine->Settings.WhisperDist + GetMultihex();
    }

    for (auto* cr : to_cr) {
        if (cr == this) {
            continue;
        }

        if (dist == -1) {
            cr->Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);
        }
        else if (_engine->GeomHelper.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);
        }
    }
}

void Critter::SendMessage(int num, int val, int to, MapManager& map_mngr)
{
    switch (to) {
    case MESSAGE_TO_VISIBLE_ME: {
        auto critters = VisCr;
        for (auto* cr : critters) {
            _engine->CritterMessageEvent.Raise(cr, this, num, val);
        }
    } break;
    case MESSAGE_TO_IAM_VISIBLE: {
        auto critters = VisCrSelf;
        for (auto* cr : critters) {
            _engine->CritterMessageEvent.Raise(cr, this, num, val);
        }
    } break;
    case MESSAGE_TO_ALL_ON_MAP: {
        auto* map = map_mngr.GetMap(GetMapId());
        if (map == nullptr) {
            break;
        }

        auto critters = map->GetCritters();
        for (auto* cr : critters) {
            _engine->CritterMessageEvent.Raise(cr, this, num, val);
        }
    } break;
    default:
        break;
    }
}

auto Critter::IsTransferTimeouts(bool send) -> bool
{
    if (GetTimeoutTransfer() > _engine->GameTime.GetFullSecond()) {
        if (send) {
            Send_TextMsg(this, STR_TIMEOUT_TRANSFER_WAIT, SAY_NETMSG, TEXTMSG_GAME);
        }
        return true;
    }
    if (GetTimeoutBattle() > _engine->GameTime.GetFullSecond()) {
        if (send) {
            Send_TextMsg(this, STR_TIMEOUT_BATTLE_WAIT, SAY_NETMSG, TEXTMSG_GAME);
        }
        return true;
    }
    return false;
}

auto Critter::IsTalking() const -> bool
{
    return _talk.Type != TalkType::None;
}

auto Critter::GetTalkedPlayers() const -> uint
{
    auto talk = 0u;
    for (auto* cr : VisCr) {
        if (cr->_talk.Type == TalkType::Critter && cr->_talk.CritterId == GetId()) {
            talk++;
        }
    }
    return talk;
}

auto Critter::IsTalkedPlayers() const -> bool
{
    for (auto* cr : VisCr) {
        if (cr->_talk.Type == TalkType::Critter && cr->_talk.CritterId == GetId()) {
            return true;
        }
    }
    return false;
}

auto Critter::GetBarterPlayers() const -> uint
{
    auto barter = 0u;
    for (auto* cr : VisCr) {
        if (cr->_talk.Type == TalkType::Critter && cr->_talk.CritterId == GetId() && cr->_talk.Barter) {
            barter++;
        }
    }
    return barter;
}

auto Critter::IsFreeToTalk() const -> bool
{
    auto max_talkers = GetMaxTalkers();
    if (max_talkers == 0u) {
        max_talkers = _engine->Settings.NpcMaxTalkers;
    }

    return GetTalkedPlayers() < max_talkers;
}

void Critter::AddCrTimeEvent(hash /*func_num*/, uint /*rate*/, uint duration, int /*identifier*/) const
{
    // if (duration != 0u) {
    //    duration += _gameTime.GetFullSecond();
    //}

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

void Critter::Send_Property(NetProperty::Type type, const Property* prop, ServerEntity* entity)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Property(type, prop, entity);
    }
}

void Critter::Send_Move(Critter* from_cr, uint move_params)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Move(from_cr, move_params);
    }
}

void Critter::Send_Dir(Critter* from_cr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Dir(from_cr);
    }
}

void Critter::Send_AddCritter(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddCritter(cr);
    }
}

void Critter::Send_RemoveCritter(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_RemoveCritter(cr);
    }
}

void Critter::Send_LoadMap(Map* map, MapManager& map_mngr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_LoadMap(map, map_mngr);
    }
}

void Critter::Send_Position(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Position(cr);
    }
}

void Critter::Send_AddItemOnMap(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddItemOnMap(item);
    }
}

void Critter::Send_EraseItemFromMap(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_EraseItemFromMap(item);
    }
}

void Critter::Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AnimateItem(item, from_frm, to_frm);
    }
}

void Critter::Send_AddItem(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddItem(item);
    }
}

void Critter::Send_EraseItem(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_EraseItem(item);
    }
}

void Critter::Send_GlobalInfo(uchar flags, MapManager& map_mngr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GlobalInfo(flags, map_mngr);
    }
}

void Critter::Send_GlobalLocation(Location* loc, bool add)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GlobalLocation(loc, add);
    }
}

void Critter::Send_GlobalMapFog(ushort zx, ushort zy, uchar fog)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GlobalMapFog(zx, zy, fog);
    }
}

void Critter::Send_CustomCommand(Critter* cr, ushort cmd, int val)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_CustomCommand(cr, cmd, val);
    }
}

void Critter::Send_AllProperties()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AllProperties();
    }
}

void Critter::Send_Talk()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Talk();
    }
}

void Critter::Send_GameInfo(Map* map)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GameInfo(map);
    }
}

void Critter::Send_Text(Critter* from_cr, string_view text, uchar how_say)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Text(from_cr, text, how_say);
    }
}

void Critter::Send_TextEx(uint from_id, string_view text, uchar how_say, bool unsafe_text)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextEx(from_id, text, how_say, unsafe_text);
    }
}

void Critter::Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsg(from_cr, str_num, how_say, num_msg);
    }
}

void Critter::Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsg(from_id, str_num, how_say, num_msg);
    }
}

void Critter::Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, string_view lexems)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsgLex(from_cr, num_str, how_say, num_msg, lexems);
    }
}

void Critter::Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, string_view lexems)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsgLex(from_id, num_str, how_say, num_msg, lexems);
    }
}

void Critter::Send_Action(Critter* from_cr, int action, int action_ext, Item* item)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Action(from_cr, action, action_ext, item);
    }
}

void Critter::Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MoveItem(from_cr, item, action, prev_slot);
    }
}

void Critter::Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Animate(from_cr, anim1, anim2, item, clear_sequence, delay_play);
    }
}

void Critter::Send_SetAnims(Critter* from_cr, CritterCondition cond, uint anim1, uint anim2)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_SetAnims(from_cr, cond, anim1, anim2);
    }
}

void Critter::Send_CombatResult(uint* combat_res, uint len)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_CombatResult(combat_res, len);
    }
}

void Critter::Send_AutomapsInfo(void* locs_vec, Location* loc)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AutomapsInfo(locs_vec, loc);
    }
}

void Critter::Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Effect(eff_pid, hx, hy, radius);
    }
}

void Critter::Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_FlyEffect(eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy);
    }
}

void Critter::Send_PlaySound(uint crid_synchronize, string_view sound_name)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_PlaySound(crid_synchronize, sound_name);
    }
}

void Critter::Send_MapText(ushort hx, ushort hy, uint color, string_view text, bool unsafe_text)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MapText(hx, hy, color, text, unsafe_text);
    }
}

void Critter::Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MapTextMsg(hx, hy, color, num_msg, num_str);
    }
}

void Critter::Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, string_view lexems)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MapTextMsgLex(hx, hy, color, num_msg, num_str, lexems);
    }
}

void Critter::Send_ViewMap()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_ViewMap();
    }
}

void Critter::Send_SomeItem(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_SomeItem(item);
    }
}

void Critter::Send_CustomMessage(uint msg)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_CustomMessage(msg);
    }
}

void Critter::Send_AddAllItems()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddAllItems();
    }
}

void Critter::Send_AllAutomapsInfo(MapManager& map_mngr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AllAutomapsInfo(map_mngr);
    }
}

void Critter::Send_SomeItems(const vector<Item*>* items, int param)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_SomeItems(items, param);
    }
}
