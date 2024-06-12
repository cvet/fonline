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

#include "Critter.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"

Critter::Critter(FOServer* engine, ident_t id, const ProtoCritter* proto, const Properties* props) :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(GetInitRef())
{
    STACK_TRACE_ENTRY();

    _name = _str("{}_{}", proto->GetName(), id);
}

Critter::~Critter()
{
    STACK_TRACE_ENTRY();

    if (_player != nullptr) {
        _player->Release();
    }
}

auto Critter::GetOfflineTime() const -> time_duration
{
    STACK_TRACE_ENTRY();

    return GetIsControlledByPlayer() && _player == nullptr ? _engine->GameTime.FrameTime() - _playerDetachTime : time_duration {};
}

auto Critter::IsAlive() const -> bool
{
    STACK_TRACE_ENTRY();

    return GetCondition() == CritterCondition::Alive;
}

auto Critter::IsDead() const -> bool
{
    STACK_TRACE_ENTRY();

    return GetCondition() == CritterCondition::Dead;
}

auto Critter::IsKnockout() const -> bool
{
    STACK_TRACE_ENTRY();

    return GetCondition() == CritterCondition::Knockout;
}

auto Critter::CheckFind(CritterFindType find_type) const -> bool
{
    STACK_TRACE_ENTRY();

    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (IsEnumSet(find_type, CritterFindType::Players) && !GetIsControlledByPlayer()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Npc) && GetIsControlledByPlayer()) {
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

void Critter::MarkIsForPlayer()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!GetIsControlledByPlayer());

    SetIsControlledByPlayer(true);
    _playerDetachTime = _engine->GameTime.FrameTime();
}

void Critter::AttachPlayer(Player* player)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(GetIsControlledByPlayer());
    RUNTIME_ASSERT(player);
    RUNTIME_ASSERT(!player->GetControlledCritterId());
    RUNTIME_ASSERT(!_player);

    _player = player;
    _player->AddRef();

    _player->SetControlledCritterId(GetId());
    _player->SetLastControlledCritterId(GetId());
    _player->SetControlledCritter(this);

    _name = _player->GetName();
}

void Critter::DetachPlayer()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(GetIsControlledByPlayer());
    RUNTIME_ASSERT(_player);
    RUNTIME_ASSERT(_player->GetControlledCritterId() == GetId());

    _player->SetControlledCritterId({});
    _player->SetControlledCritter(nullptr);

    _player->Release();
    _player = nullptr;

    _playerDetachTime = _engine->GameTime.FrameTime();
}

void Critter::ClearMove()
{
    STACK_TRACE_ENTRY();

    Moving.Uid++;
    Moving.Steps = {};
    Moving.ControlSteps = {};
    Moving.StartTime = {};
    Moving.Speed = {};
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

void Critter::AttachToCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr != this);
    RUNTIME_ASSERT(cr->GetMapId() == GetMapId());
    RUNTIME_ASSERT(!cr->GetIsAttached());
    RUNTIME_ASSERT(!GetIsAttached());
    RUNTIME_ASSERT(AttachedCritters.empty());

    if (IsMoving()) {
        ClearMove();
        SendAndBroadcast_Moving();
    }

    const auto it = std::find(cr->AttachedCritters.begin(), cr->AttachedCritters.end(), this);
    RUNTIME_ASSERT(it == cr->AttachedCritters.end());
    cr->AttachedCritters.emplace_back(this);

    SetIsAttached(true);
    SetAttachMaster(cr->GetId());

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::DetachFromCritter()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(GetIsAttached());
    RUNTIME_ASSERT(GetAttachMaster());

    auto* cr = _engine->CrMngr.GetCritter(GetAttachMaster());
    RUNTIME_ASSERT(cr);

    const auto it = std::find(cr->AttachedCritters.begin(), cr->AttachedCritters.end(), this);
    RUNTIME_ASSERT(it != cr->AttachedCritters.end());
    cr->AttachedCritters.erase(it);

    SetIsAttached(false);
    SetAttachMaster({});

    SendAndBroadcast_Attachments();
    cr->SendAndBroadcast_Attachments();
}

void Critter::MoveAttachedCritters()
{
    STACK_TRACE_ENTRY();

    if (!GetMapId()) {
        return;
    }

    // Sync position
    auto* map = _engine->MapMngr.GetMap(GetMapId());
    RUNTIME_ASSERT(map);

    vector<tuple<Critter*, uint16, uint16, RefCountHolder<Critter>>> moved_critters;

    const auto new_hx = GetHexX();
    const auto new_hy = GetHexY();
    const auto new_hex_ox = GetHexOffsX();
    const auto new_hex_oy = GetHexOffsY();

    for (auto* cr : AttachedCritters) {
        RUNTIME_ASSERT(!cr->IsDestroyed());
        RUNTIME_ASSERT(cr->GetIsAttached());
        RUNTIME_ASSERT(cr->GetAttachMaster() == GetId());
        RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

        cr->SetHexOffsX(new_hex_ox);
        cr->SetHexOffsY(new_hex_oy);

        const auto hx = cr->GetHexX();
        const auto hy = cr->GetHexY();

        if (hx != new_hx || hy != new_hy) {
            map->RemoveCritterFromField(cr);
            cr->SetHexX(new_hx);
            cr->SetHexY(new_hy);
            map->AddCritterToField(cr);

            moved_critters.emplace_back(cr, hx, hy, RefCountHolder {cr});
        }
    }

    // Callbacks time
    auto this_ref_holder = RefCountHolder(this);
    auto map_ref_holder = RefCountHolder(map);
    const auto dir = GeometryHelper::AngleToDir(GetDirAngle());

    for (auto&& [cr, prev_hx, prev_hy, cr_ref_holder] : moved_critters) {
        const auto is_cr_valid = [cr = cr, map] {
            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return false;
            }
            if (cr->GetMapId() != map->GetId()) {
                return false;
            }
            return true;
        };

        if (!is_cr_valid()) {
            continue;
        }

        _engine->VerifyTrigger(map, cr, prev_hx, prev_hy, new_hx, new_hy, dir);

        if (!is_cr_valid()) {
            continue;
        }

        _engine->MapMngr.ProcessVisibleCritters(cr);

        if (!is_cr_valid()) {
            continue;
        }

        _engine->MapMngr.ProcessVisibleItems(cr);
    }
}

void Critter::ClearVisible()
{
    STACK_TRACE_ENTRY();

    for (auto* cr : VisCr) {
        auto it_ = std::find(cr->VisCrSelf.begin(), cr->VisCrSelf.end(), this);
        if (it_ != cr->VisCrSelf.end()) {
            cr->VisCrSelf.erase(it_);
            cr->VisCrSelfMap.erase(GetId());
        }
        cr->Send_RemoveCritter(this);
    }
    VisCr.clear();
    VisCrMap.clear();

    for (auto* cr : VisCrSelf) {
        auto it_ = std::find(cr->VisCr.begin(), cr->VisCr.end(), this);
        if (it_ != cr->VisCr.end()) {
            cr->VisCr.erase(it_);
            cr->VisCrMap.erase(GetId());
        }
    }
    VisCrSelf.clear();
    VisCrSelfMap.clear();

    VisCr1.clear();
    VisCr2.clear();
    VisCr3.clear();

    VisItem.clear();
}

auto Critter::GetCrSelf(ident_t cr_id) -> Critter*
{
    STACK_TRACE_ENTRY();

    const auto it = VisCrSelfMap.find(cr_id);
    return it != VisCrSelfMap.end() ? it->second : nullptr;
}

auto Critter::GetCrFromVisCr(CritterFindType find_type, bool vis_cr_self) -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

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

auto Critter::GetGlobalMapCritter(ident_t cr_id) const -> Critter*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(GlobalMapGroup);
    const auto it = std::find_if(GlobalMapGroup->begin(), GlobalMapGroup->end(), [&cr_id](const Critter* other) { return other->GetId() == cr_id; });
    return it != GlobalMapGroup->end() ? *it : nullptr;
}

auto Critter::AddCrIntoVisVec(Critter* add_cr) -> bool
{
    STACK_TRACE_ENTRY();

    if (VisCrMap.count(add_cr->GetId()) != 0) {
        return false;
    }

    VisCr.push_back(add_cr);
    VisCrMap.insert(std::make_pair(add_cr->GetId(), add_cr));

    add_cr->VisCrSelf.push_back(this);
    add_cr->VisCrSelfMap.insert(std::make_pair(GetId(), this));
    return true;
}

auto Critter::DelCrFromVisVec(Critter* del_cr) -> bool
{
    STACK_TRACE_ENTRY();

    const auto it_map = VisCrMap.find(del_cr->GetId());
    if (it_map == VisCrMap.end()) {
        return false;
    }

    VisCrMap.erase(it_map);
    VisCr.erase(std::find(VisCr.begin(), VisCr.end(), del_cr));

    const auto it = std::find(del_cr->VisCrSelf.begin(), del_cr->VisCrSelf.end(), this);
    if (it != del_cr->VisCrSelf.end()) {
        del_cr->VisCrSelf.erase(it);
        del_cr->VisCrSelfMap.erase(GetId());
    }
    return true;
}

auto Critter::AddCrIntoVisSet1(ident_t cr_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisCr1.insert(cr_id).second;
}

auto Critter::AddCrIntoVisSet2(ident_t cr_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisCr2.insert(cr_id).second;
}

auto Critter::AddCrIntoVisSet3(ident_t cr_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisCr3.insert(cr_id).second;
}

auto Critter::DelCrFromVisSet1(ident_t cr_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisCr1.erase(cr_id) != 0;
}

auto Critter::DelCrFromVisSet2(ident_t cr_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisCr2.erase(cr_id) != 0;
}

auto Critter::DelCrFromVisSet3(ident_t cr_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisCr3.erase(cr_id) != 0;
}

auto Critter::AddIdVisItem(ident_t item_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisItem.insert(item_id).second;
}

auto Critter::DelIdVisItem(ident_t item_id) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisItem.erase(item_id) != 0;
}

auto Critter::CountIdVisItem(ident_t item_id) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    return VisItem.count(item_id) != 0;
}

void Critter::ChangeDir(uint8 dir)
{
    STACK_TRACE_ENTRY();

    const auto normalized_dir = static_cast<uint8>(dir % GameSettings::MAP_DIR_COUNT);

    if (normalized_dir == GetDir()) {
        return;
    }

    SetDirAngle(GeometryHelper::DirToAngle(normalized_dir));
    SetDir(normalized_dir);
}

void Critter::ChangeDirAngle(int dir_angle)
{
    STACK_TRACE_ENTRY();

    const auto normalized_dir_angle = GeometryHelper::NormalizeAngle(static_cast<int16>(dir_angle));

    if (normalized_dir_angle == GetDirAngle()) {
        return;
    }

    SetDirAngle(normalized_dir_angle);
    SetDir(GeometryHelper::AngleToDir(normalized_dir_angle));
}

void Critter::SetItem(Item* item)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item);

    _invItems.push_back(item);

    if (item->GetOwnership() != ItemOwnership::CritterInventory) {
        item->SetCritterSlot(CritterItemSlot::Inventory);
    }

    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
}

auto Critter::GetInvItem(ident_t item_id, bool skip_hidden) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!item_id) {
        return nullptr;
    }

    for (auto* item : _invItems) {
        if (item->GetId() == item_id) {
            if (skip_hidden && item->GetIsHidden()) {
                return nullptr;
            }
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetInvItemByPid(hstring item_pid) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetInvItemByPidSlot(hstring item_pid, CritterItemSlot slot) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid && item->GetCritterSlot() == slot) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetInvItemSlot(CritterItemSlot slot) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetCritterSlot() == slot) {
            return item;
        }
    }
    return nullptr;
}

auto Critter::GetInvItemsSlot(CritterItemSlot slot) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<Item*> items;
    items.reserve(_invItems.size());

    for (auto* item : _invItems) {
        if (item->GetCritterSlot() == slot) {
            items.push_back(item);
        }
    }
    return items;
}

auto Critter::CountInvItemPid(hstring pid) const -> uint
{
    STACK_TRACE_ENTRY();

    uint res = 0;
    for (const auto* item : _invItems) {
        if (item->GetProtoId() == pid) {
            res += item->GetCount();
        }
    }
    return res;
}

auto Critter::CountInvItems() const -> uint
{
    STACK_TRACE_ENTRY();

    uint count = 0;
    for (const auto* item : _invItems) {
        count += item->GetCount();
    }
    return count;
}

auto Critter::IsHaveGeckItem() const -> bool
{
    STACK_TRACE_ENTRY();

    for (const auto* item : _invItems) {
        if (item->GetIsGeck()) {
            return true;
        }
    }
    return false;
}

void Critter::Broadcast_Property(NetProperty type, const Property* prop, const ServerEntity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Property(type, prop, entity);
    }
}

void Critter::Broadcast_Action(CritterAction action, int action_data, const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Action(this, action, action_data, item);
    }
}

void Critter::Broadcast_Dir()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Dir(this);
    }
}

void Critter::Broadcast_Teleport(uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Teleport(this, to_hx, to_hy);
    }
}

void Critter::SendAndBroadcast(const Player* ignore_player, const std::function<void(Critter*)>& callback)
{
    STACK_TRACE_ENTRY();

    if (ignore_player == nullptr || ignore_player != GetPlayer()) {
        callback(this);
    }

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        if (ignore_player == nullptr || ignore_player != cr->GetPlayer()) {
            callback(cr);
        }
    }
}

void Critter::SendAndBroadcast_Moving()
{
    STACK_TRACE_ENTRY();

    Send_Moving(this);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Moving(this);
    }
}

void Critter::SendAndBroadcast_Action(CritterAction action, int action_data, const Item* context_item)
{
    STACK_TRACE_ENTRY();

    Send_Action(this, action, action_data, context_item);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Action(this, action, action_data, context_item);
    }
}

void Critter::SendAndBroadcast_MoveItem(const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    STACK_TRACE_ENTRY();

    Send_MoveItem(this, item, action, prev_slot);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_MoveItem(this, item, action, prev_slot);
    }
}

void Critter::SendAndBroadcast_Animate(CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play)
{
    STACK_TRACE_ENTRY();

    Send_Animate(this, state_anim, action_anim, context_item, clear_sequence, delay_play);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Animate(this, state_anim, action_anim, context_item, clear_sequence, delay_play);
    }
}

void Critter::SendAndBroadcast_SetAnims(CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim)
{
    STACK_TRACE_ENTRY();

    Send_SetAnims(this, cond, state_anim, action_anim);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_SetAnims(this, cond, state_anim, action_anim);
    }
}

void Critter::SendAndBroadcast_Text(const vector<Critter*>& to_cr, string_view text, uint8 how_say, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

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
        else if (GeometryHelper::CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextEx(from_id, text, how_say, unsafe_text);
        }
    }
}

void Critter::SendAndBroadcast_Msg(const vector<Critter*>& to_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    Send_TextMsg(this, how_say, text_pack, str_num);

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
            cr->Send_TextMsg(this, how_say, text_pack, str_num);
        }
        else if (GeometryHelper::CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextMsg(this, how_say, text_pack, str_num);
        }
    }
}

void Critter::SendAndBroadcast_MsgLex(const vector<Critter*>& to_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    Send_TextMsgLex(this, how_say, text_pack, str_num, lexems);

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
            cr->Send_TextMsgLex(this, how_say, text_pack, str_num, lexems);
        }
        else if (GeometryHelper::CheckDist(GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex())) {
            cr->Send_TextMsgLex(this, how_say, text_pack, str_num, lexems);
        }
    }
}

void Critter::SendAndBroadcast_Attachments()
{
    STACK_TRACE_ENTRY();

    Send_Attachments(this);

    if (VisCr.empty()) {
        return;
    }

    for (auto* cr : VisCr) {
        cr->Send_Attachments(this);
    }
}

auto Critter::IsTalking() const -> bool
{
    STACK_TRACE_ENTRY();

    return Talk.Type != TalkType::None;
}

auto Critter::GetTalkingCritters() const -> uint
{
    STACK_TRACE_ENTRY();

    uint talkers = 0;

    for (const auto* cr : VisCr) {
        if (cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == GetId()) {
            talkers++;
        }
    }

    return talkers;
}

auto Critter::GetBarterCritters() const -> uint
{
    STACK_TRACE_ENTRY();

    auto barter = 0;

    for (const auto* cr : VisCr) {
        if (cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == GetId() && cr->Talk.Barter) {
            barter++;
        }
    }

    return barter;
}

auto Critter::IsFreeToTalk() const -> bool
{
    STACK_TRACE_ENTRY();

    auto max_talkers = GetMaxTalkers();
    if (max_talkers == 0) {
        max_talkers = _engine->Settings.NpcMaxTalkers;
    }

    return GetTalkingCritters() < max_talkers;
}

void Critter::Send_Property(NetProperty type, const Property* prop, const ServerEntity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Property(type, prop, entity);
    }
}

void Critter::Send_Moving(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Moving(from_cr);
    }
}

void Critter::Send_Dir(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Dir(from_cr);
    }
}

void Critter::Send_AddCritter(const Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddCritter(cr);
    }
}

void Critter::Send_RemoveCritter(const Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_RemoveCritter(cr);
    }
}

void Critter::Send_LoadMap(const Map* map)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_LoadMap(map);
    }
}

void Critter::Send_AddItemOnMap(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddItemOnMap(item);
    }
}

void Critter::Send_EraseItemFromMap(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_EraseItemFromMap(item);
    }
}

void Critter::Send_AnimateItem(const Item* item, hstring anim_name, bool looped, bool reversed)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AnimateItem(item, anim_name, looped, reversed);
    }
}

void Critter::Send_AddItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddItem(item);
    }
}

void Critter::Send_EraseItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_EraseItem(item);
    }
}

void Critter::Send_GlobalInfo()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GlobalInfo();
    }
}

void Critter::Send_GlobalLocation(const Location* loc, bool add)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GlobalLocation(loc, add);
    }
}

void Critter::Send_GlobalMapFog(uint16 zx, uint16 zy, uint8 fog)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_GlobalMapFog(zx, zy, fog);
    }
}

void Critter::Send_Teleport(const Critter* cr, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Teleport(cr, to_hx, to_hy);
    }
}

void Critter::Send_AllProperties()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AllProperties();
    }
}

void Critter::Send_Talk()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Talk();
    }
}

void Critter::Send_TimeSync()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TimeSync();
    }
}

void Critter::Send_Text(const Critter* from_cr, string_view text, uint8 how_say)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Text(from_cr, text, how_say);
    }
}

void Critter::Send_TextEx(ident_t from_id, string_view text, uint8 how_say, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextEx(from_id, text, how_say, unsafe_text);
    }
}

void Critter::Send_TextMsg(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsg(from_cr, how_say, text_pack, str_num);
    }
}

void Critter::Send_TextMsg(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsg(from_id, how_say, text_pack, str_num);
    }
}

void Critter::Send_TextMsgLex(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsgLex(from_cr, how_say, text_pack, str_num, lexems);
    }
}

void Critter::Send_TextMsgLex(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_TextMsgLex(from_id, how_say, text_pack, str_num, lexems);
    }
}

void Critter::Send_Action(const Critter* from_cr, CritterAction action, int action_data, const Item* context_item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Action(from_cr, action, action_data, context_item);
    }
}

void Critter::Send_MoveItem(const Critter* from_cr, const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MoveItem(from_cr, item, action, prev_slot);
    }
}

void Critter::Send_Animate(const Critter* from_cr, CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Animate(from_cr, state_anim, action_anim, context_item, clear_sequence, delay_play);
    }
}

void Critter::Send_SetAnims(const Critter* from_cr, CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_SetAnims(from_cr, cond, state_anim, action_anim);
    }
}

void Critter::Send_AutomapsInfo(const void* locs_vec, const Location* loc)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AutomapsInfo(locs_vec, loc);
    }
}

void Critter::Send_Effect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Effect(eff_pid, hx, hy, radius);
    }
}

void Critter::Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_FlyEffect(eff_pid, from_cr_id, to_cr_id, from_hx, from_hy, to_hx, to_hy);
    }
}

void Critter::Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_PlaySound(cr_id_synchronize, sound_name);
    }
}

void Critter::Send_MapText(uint16 hx, uint16 hy, ucolor color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MapText(hx, hy, color, text, unsafe_text);
    }
}

void Critter::Send_MapTextMsg(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MapTextMsg(hx, hy, color, text_pack, str_num);
    }
}

void Critter::Send_MapTextMsgLex(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_MapTextMsgLex(hx, hy, color, text_pack, str_num, lexems);
    }
}

void Critter::Send_ViewMap()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_ViewMap();
    }
}

void Critter::Send_PlaceToGameComplete()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_PlaceToGameComplete();
    }
}

void Critter::Send_AddAllItems()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AddAllItems();
    }
}

void Critter::Send_AllAutomapsInfo()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_AllAutomapsInfo();
    }
}

void Critter::Send_SomeItems(const vector<Item*>* items, int param)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_SomeItems(items, param);
    }
}

void Critter::Send_Attachments(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_player != nullptr) {
        _player->Send_Attachments(from_cr);
    }
}

void Critter::AddTimeEvent(hstring func_name, uint rate, tick_t duration, const any_t& identifier)
{
    STACK_TRACE_ENTRY();

    auto te_identifiers = GetTE_Identifier();
    auto te_fire_times = GetTE_FireTime();
    auto te_func_names = GetTE_FuncName();
    auto te_rates = GetTE_Rate();
    RUNTIME_ASSERT(te_identifiers.size() == te_fire_times.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_func_names.size());
    RUNTIME_ASSERT(te_identifiers.size() == te_rates.size());

    const auto fire_time = tick_t {_engine->GameTime.GetFullSecond().underlying_value() + duration.underlying_value()};

    size_t index = 0;

    for ([[maybe_unused]] const auto& te_identifier : te_identifiers) {
        if (fire_time.underlying_value() < te_fire_times[index].underlying_value()) {
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
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

    // Fast checking
    uint data_size = 0;
    const auto* data = GetProperties().GetRawData(GetPropertyTE_FireTime(), data_size);
    if (data_size == 0) {
        return;
    }

    const auto& near_fire_time = *reinterpret_cast<const tick_t*>(data);
    const auto full_second = _engine->GameTime.GetFullSecond();
    if (full_second.underlying_value() < near_fire_time.underlying_value()) {
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

    if (auto func = _engine->ScriptSys->FindFunc<uint, Critter*, any_t, uint*>(func_name)) {
        if (func(this, identifier, &rate)) {
            if (const auto next_call_duration = func.GetResult(); next_call_duration > 0) {
                AddTimeEvent(func_name, rate, tick_t {next_call_duration}, identifier);
            }
        }
    }
    else if (auto func2 = _engine->ScriptSys->FindFunc<tick_t, Critter*, any_t, uint*>(func_name)) {
        if (func2(this, identifier, &rate)) {
            if (const auto next_call_duration = func2.GetResult(); next_call_duration.underlying_value() > 0) {
                AddTimeEvent(func_name, rate, next_call_duration, identifier);
            }
        }
    }
    else {
        throw ScriptException("Time event func not found", func_name);
    }
}
