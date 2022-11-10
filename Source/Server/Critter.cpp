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

#include "Critter.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "Server.h"
#include "Settings.h"

Critter::Critter(FOServer* engine, uint id, Player* owner, const ProtoCritter* proto) : ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME)), EntityWithProto(this, proto), CritterProperties(GetInitRef()), _player {owner}
{
    if (_player != nullptr) {
        _player->AddRef();
    }
}

Critter::~Critter()
{
    if (_player != nullptr) {
        _player->Release();
    }
}

auto Critter::GetStorageName() const -> string_view
{
    return IsOwnedByPlayer() ? "PlayerCritter" : GetClassName();
}

auto Critter::GetOfflineTime() const -> uint
{
    return _playerDetached ? _engine->GameTime.FrameTick() - _playerDetachTick : 0u;
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

auto Critter::CheckFind(CritterFindType find_type) const -> bool
{
    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (IsEnumSet(find_type, CritterFindType::Players) && IsNpc()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Npc) && !IsNpc()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Alive) && IsDead()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Dead) && !IsDead()) {
        return false;
    }
    return true;
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

void Critter::ClearMove()
{
    Moving.Uid++;
    Moving.Steps = {};
    Moving.ControlSteps = {};
    Moving.StartTick = {};
    Moving.StartHexX = {};
    Moving.StartHexY = {};
    Moving.EndHexX = {};
    Moving.EndHexY = {};
    Moving.WholeTime = {};
    Moving.WholeDist = {};
    Moving.StartOx = {};
    Moving.StartOy = {};
    Moving.EndOx = {};
    Moving.EndOy = {};
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

auto Critter::GetCrFromVisCr(CritterFindType find_type, bool vis_cr_self) -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    const auto& vis_cr = vis_cr_self ? VisCrSelf : VisCr;

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

void Critter::ChangeDir(uchar dir)
{
    const auto normalized_dir = static_cast<uchar>(dir % GameSettings::MAP_DIR_COUNT);

    if (normalized_dir == GetDir()) {
        return;
    }

    SetDirAngle(_engine->Geometry.DirToAngle(normalized_dir));
    SetDir(normalized_dir);
}

void Critter::ChangeDirAngle(int dir_angle)
{
    const auto normalized_dir_angle = _engine->Geometry.NormalizeAngle(static_cast<short>(dir_angle));

    if (normalized_dir_angle == GetDirAngle()) {
        return;
    }

    SetDirAngle(normalized_dir_angle);
    SetDir(_engine->Geometry.AngleToDir(normalized_dir_angle));
}

void Critter::SetItem(Item* item)
{
    RUNTIME_ASSERT(item);

    _invItems.push_back(item);

    if (item->GetOwnership() != ItemOwnership::CritterInventory) {
        item->SetCritterSlot(0);
    }

    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
}

auto Critter::GetItem(uint item_id, bool skip_hide) -> Item*
{
    NON_CONST_METHOD_HINT();

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

auto Critter::GetItemByPid(hstring item_pid) -> Item*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetItemByPidSlot(hstring item_pid, int slot) -> Item*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid && item->GetCritterSlot() == slot) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetItemSlot(int slot) -> Item*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetCritterSlot() == slot) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetItemsSlot(int slot) -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    vector<Item*> items;
    items.reserve(_invItems.size());

    for (auto* item : _invItems) {
        if (slot < 0 || item->GetCritterSlot() == slot) {
            items.push_back(item);
        }
    }
    return items;
}

auto Critter::CountItemPid(hstring pid) const -> uint
{
    uint res = 0;
    for (const auto* item : _invItems) {
        if (item->GetProtoId() == pid) {
            res += item->GetCount();
        }
    }
    return res;
}

auto Critter::CountItems() const -> uint
{
    uint count = 0;
    for (const auto* item : _invItems) {
        count += item->GetCount();
    }
    return count;
}

auto Critter::GetInventory() -> vector<Item*>&
{
    return _invItems;
}

auto Critter::IsHaveGeckItem() const -> bool
{
    for (const auto* item : _invItems) {
        if (item->GetIsGeck()) {
            return true;
        }
    }
    return false;
}

void Critter::Broadcast_Property(NetProperty type, const Property* prop, ServerEntity* entity)
{
    NON_CONST_METHOD_HINT();

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Move()
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Move(this);
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

void Critter::Broadcast_Teleport(ushort to_hx, ushort to_hy)
{
    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Teleport(this, to_hx, to_hy);
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

    auto dist = static_cast<uint>(-1);

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

        if (dist == static_cast<uint>(-1)) {
            cr->Send_TextEx(from_id, text, how_say, unsafe_text);
        }
        else if (_engine->Geometry.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
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

    auto dist = static_cast<uint>(-1);

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

        if (dist == static_cast<uint>(-1)) {
            cr->Send_TextMsg(this, num_str, how_say, num_msg);
        }
        else if (_engine->Geometry.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
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

    auto dist = static_cast<uint>(-1);

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

        if (dist == static_cast<uint>(-1)) {
            cr->Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);
        }
        else if (_engine->Geometry.CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextMsgLex(this, num_str, how_say, num_msg, lexems);
        }
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
    for (const auto* cr : VisCr) {
        if (cr->_talk.Type == TalkType::Critter && cr->_talk.CritterId == GetId()) {
            talk++;
        }
    }
    return talk;
}

auto Critter::IsTalkedPlayers() const -> bool
{
    for (const auto* cr : VisCr) {
        if (cr->_talk.Type == TalkType::Critter && cr->_talk.CritterId == GetId()) {
            return true;
        }
    }
    return false;
}

auto Critter::GetBarterPlayers() const -> uint
{
    auto barter = 0u;
    for (const auto* cr : VisCr) {
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

void Critter::Send_Property(NetProperty type, const Property* prop, ServerEntity* entity)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Property(type, prop, entity);
    }
}

void Critter::Send_Move(Critter* from_cr)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Move(from_cr);
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

void Critter::Send_LoadMap(Map* map)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_LoadMap(map);
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

void Critter::Send_GlobalInfo(uchar flags)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GlobalInfo(flags);
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

void Critter::Send_Teleport(Critter* cr, ushort to_hx, ushort to_hy)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Teleport(cr, to_hx, to_hy);
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

void Critter::Send_TimeSync()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TimeSync();
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

void Critter::Send_AutomapsInfo(void* locs_vec, Location* loc)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AutomapsInfo(locs_vec, loc);
    }
}

void Critter::Send_Effect(hstring eff_pid, ushort hx, ushort hy, ushort radius)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Effect(eff_pid, hx, hy, radius);
    }
}

void Critter::Send_FlyEffect(hstring eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy)
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

void Critter::Send_PlaceToGameComplete()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_PlaceToGameComplete();
    }
}

void Critter::Send_AddAllItems()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddAllItems();
    }
}

void Critter::Send_AllAutomapsInfo()
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AllAutomapsInfo();
    }
}

void Critter::Send_SomeItems(const vector<Item*>* items, int param)
{
    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_SomeItems(items, param);
    }
}

void Critter::AddTimeEvent(hstring func_name, uint rate, uint duration, int identifier)
{
    auto te_identifiers = GetTE_Identifier();
    auto te_fire_times = GetTE_FireTime();
    auto te_func_names = GetTE_FuncName();
    auto te_rates = GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_func_names.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    const auto fire_time = duration + _engine->GameTime.GetFullSecond();

    size_t index = 0;

    for ([[maybe_unused]] const auto te_identifier : te_identifiers) {
        if (fire_time < te_fire_times[index]) {
            break;
        }

        index++;
    }

    te_identifiers.insert(te_identifiers.begin() + static_cast<decltype(te_identifiers)::difference_type>(index), identifier);
    te_fire_times.insert(te_fire_times.begin() + static_cast<decltype(te_fire_times)::difference_type>(index), fire_time);
    te_func_names.insert(te_func_names.begin() + static_cast<decltype(te_func_names)::difference_type>(index), func_name);
    te_rates.insert(te_rates.begin() + static_cast<decltype(te_rates)::difference_type>(index), rate);

    SetTE_FireTime(te_fire_times);
    SetTE_FuncName(te_func_names);
    SetTE_Rate(te_rates);
    SetTE_Identifier(te_identifiers);
}

void Critter::EraseTimeEvent(size_t index)
{
    auto te_identifiers = GetTE_Identifier();
    auto te_fire_times = GetTE_FireTime();
    auto te_func_names = GetTE_FuncName();
    auto te_rates = GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_func_names.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());
    RUNTIME_ASSERT(index < te_identifiers.size());

    te_identifiers.erase(te_identifiers.begin() + static_cast<decltype(te_identifiers)::difference_type>(index));
    te_fire_times.erase(te_fire_times.begin() + static_cast<decltype(te_fire_times)::difference_type>(index));
    te_func_names.erase(te_func_names.begin() + static_cast<decltype(te_func_names)::difference_type>(index));
    te_rates.erase(te_rates.begin() + static_cast<decltype(te_rates)::difference_type>(index));

    SetTE_FireTime(te_fire_times);
    SetTE_FuncName(te_func_names);
    SetTE_Rate(te_rates);
    SetTE_Identifier(te_identifiers);
}

void Critter::ProcessTimeEvents()
{
    // Fast checking
    uint data_size = 0;
    const auto* data = GetProperties().GetRawData(GetPropertyTE_FireTime(), data_size);
    if (data_size == 0) {
        return;
    }

    const auto& near_fire_time = *reinterpret_cast<const uint*>(data);
    const auto full_second = _engine->GameTime.GetFullSecond();
    if (full_second < near_fire_time) {
        return;
    }

    // One event per cycle
    auto te_identifiers = GetTE_Identifier();
    auto te_fire_times = GetTE_FireTime();
    auto te_func_names = GetTE_FuncName();
    auto te_rates = GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_func_names.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    const auto identifier = te_identifiers.front();
    const auto func_name = te_func_names.front();
    auto rate = te_rates.front();

    te_identifiers.erase(te_identifiers.begin());
    te_fire_times.erase(te_fire_times.begin());
    te_func_names.erase(te_func_names.begin());
    te_rates.erase(te_rates.begin());

    SetTE_Identifier(te_identifiers);
    SetTE_FireTime(te_fire_times);
    SetTE_FuncName(te_func_names);
    SetTE_Rate(te_rates);

    if (auto func = _engine->ScriptSys->FindFunc<uint, Critter*, int, uint*>(func_name)) {
        if (func(this, identifier, &rate)) {
            if (const auto next_call_duration = func.GetResult(); next_call_duration > 0) {
                AddTimeEvent(func_name, rate, next_call_duration, identifier);
            }
        }
    }
    else {
        throw ScriptException("Time event func not found", func_name);
    }
}
