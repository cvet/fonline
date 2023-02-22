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

#include "Player.h"
#include "Critter.h"
#include "CritterManager.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "Server.h"
#include "Settings.h"

Player::Player(FOServer* engine, ident_t id, ClientConnection* connection) :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME)),
    PlayerProperties(GetInitRef()),
    Connection {connection},
    _talkNextTick {_engine->GameTime.GameTick() + PROCESS_TALK_TICK}
{
    STACK_TRACE_ENTRY();

    _name = "(Unlogined)";
}

Player::~Player()
{
    STACK_TRACE_ENTRY();

    delete Connection;
}

void Player::SetName(string_view name)
{
    STACK_TRACE_ENTRY();

    _name = name;
}

void Player::SetOwnedCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_ownedCr);
    _ownedCr = cr;
}

auto Player::GetIp() const -> uint
{
    STACK_TRACE_ENTRY();

    return Connection->GetIp();
}

auto Player::GetHost() const -> string_view
{
    STACK_TRACE_ENTRY();

    return Connection->GetHost();
}

auto Player::GetPort() const -> uint16
{
    STACK_TRACE_ENTRY();

    return Connection->GetPort();
}

void Player::Send_AddCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    vector<uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto is_chosen = cr == GetOwnedCritter();
    cr->StoreData(is_chosen, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->OutBuf.StartMsg(NETMSG_ADD_CRITTER);
    Connection->OutBuf << cr->GetId();
    Connection->OutBuf << cr->GetHexX();
    Connection->OutBuf << cr->GetHexY();
    Connection->OutBuf << cr->GetHexOffsX();
    Connection->OutBuf << cr->GetHexOffsY();
    Connection->OutBuf << cr->GetDirAngle();
    Connection->OutBuf << cr->GetCond();
    Connection->OutBuf << cr->GetAnim1Alive();
    Connection->OutBuf << cr->GetAnim1Knockout();
    Connection->OutBuf << cr->GetAnim1Dead();
    Connection->OutBuf << cr->GetAnim2Alive();
    Connection->OutBuf << cr->GetAnim2Knockout();
    Connection->OutBuf << cr->GetAnim2Dead();
    Connection->OutBuf << cr->IsOwnedByPlayer();
    Connection->OutBuf << (cr->IsOwnedByPlayer() && cr->GetOwner() == nullptr);
    Connection->OutBuf << is_chosen;
    Connection->OutBuf << cr->GetProtoId();
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();

    CONNECTION_OUTPUT_END(Connection);

    if (!is_chosen) {
        Send_MoveItem(cr, nullptr, ACTION_REFRESH, 0);
    }

    if (cr->IsMoving()) {
        Send_Move(cr);
    }
}

void Player::Send_RemoveCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_REMOVE_CRITTER);
    Connection->OutBuf << cr->GetId();
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_LoadMap(Map* map)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_ownedCr);

    const Location* loc = nullptr;
    hstring pid_map;
    hstring pid_loc;
    uint8 map_index_in_loc = 0;

    if (map == nullptr) {
        map = _engine->MapMngr.GetMap(_ownedCr->GetMapId());
    }

    if (map != nullptr) {
        loc = map->GetLocation();
        pid_map = map->GetProtoId();
        pid_loc = loc->GetProtoId();
        map_index_in_loc = static_cast<uint8>(loc->GetMapIndex(pid_map));
    }

    vector<uint8*>* map_data = nullptr;
    vector<uint>* map_data_sizes = nullptr;
    vector<uint8*>* loc_data = nullptr;
    vector<uint>* loc_data_sizes = nullptr;
    if (map != nullptr) {
        map->StoreData(false, &map_data, &map_data_sizes);
        loc->StoreData(false, &loc_data, &loc_data_sizes);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_LOADMAP);
    Connection->OutBuf << (loc != nullptr ? loc->GetId() : ident_t {});
    Connection->OutBuf << (map != nullptr ? map->GetId() : ident_t {});
    Connection->OutBuf << pid_loc;
    Connection->OutBuf << pid_map;
    Connection->OutBuf << map_index_in_loc;
    if (map != nullptr) {
        NET_WRITE_PROPERTIES(Connection->OutBuf, map_data, map_data_sizes);
        NET_WRITE_PROPERTIES(Connection->OutBuf, loc_data, loc_data_sizes);
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Property(NetProperty type, const Property* prop, Entity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(entity);

    if (SendIgnoreEntity == entity && SendIgnoreProperty == prop) {
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
    const void* data = entity->GetProperties().GetRawData(prop, data_size);

    CONNECTION_OUTPUT_BEGIN(Connection);

    const auto is_pod = prop->IsPlainData();
    if (is_pod) {
        Connection->OutBuf.StartMsg(NETMSG_POD_PROPERTY(data_size, additional_args));
    }
    else {
        Connection->OutBuf.StartMsg(NETMSG_COMPLEX_PROPERTY);
    }

    Connection->OutBuf << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        Connection->OutBuf << dynamic_cast<Item*>(entity)->GetCritterId();
        Connection->OutBuf << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    case NetProperty::Critter:
        Connection->OutBuf << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    case NetProperty::MapItem:
        Connection->OutBuf << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    case NetProperty::ChosenItem:
        Connection->OutBuf << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    default:
        break;
    }

    if (is_pod) {
        Connection->OutBuf << prop->GetRegIndex();
        Connection->OutBuf.Push(data, data_size);
    }
    else {
        Connection->OutBuf << prop->GetRegIndex();
        if (data_size != 0u) {
            Connection->OutBuf.Push(data, data_size);
        }
    }

    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Move(Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!from_cr->Moving.Steps.empty()) {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->OutBuf.StartMsg(NETMSG_CRITTER_MOVE);
        Connection->OutBuf << from_cr->GetId();
        Connection->OutBuf << static_cast<uint>(std::ceil(from_cr->Moving.WholeTime));
        Connection->OutBuf << _engine->GameTime.FrameTick() - from_cr->Moving.StartTick;
        Connection->OutBuf << from_cr->Moving.Speed;
        Connection->OutBuf << from_cr->Moving.StartHexX;
        Connection->OutBuf << from_cr->Moving.StartHexY;
        Connection->OutBuf << static_cast<uint16>(from_cr->Moving.Steps.size());
        for (auto step : from_cr->Moving.Steps) {
            Connection->OutBuf << step;
        }
        Connection->OutBuf << static_cast<uint16>(from_cr->Moving.ControlSteps.size());
        for (auto control_step : from_cr->Moving.ControlSteps) {
            Connection->OutBuf << control_step;
        }
        Connection->OutBuf << from_cr->Moving.EndOx;
        Connection->OutBuf << from_cr->Moving.EndOy;
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
    else {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->OutBuf.StartMsg(NETMSG_CRITTER_STOP_MOVE);
        Connection->OutBuf << from_cr->GetId();
        Connection->OutBuf << from_cr->GetHexX();
        Connection->OutBuf << from_cr->GetHexY();
        Connection->OutBuf << from_cr->GetHexOffsX();
        Connection->OutBuf << from_cr->GetHexOffsY();
        Connection->OutBuf << from_cr->GetDirAngle();
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
}

void Player::Send_Dir(Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_DIR);
    Connection->OutBuf << from_cr->GetId();
    Connection->OutBuf << from_cr->GetDirAngle();
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Action(Critter* from_cr, int action, int action_ext, Item* item)
{
    STACK_TRACE_ENTRY();

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_ACTION);
    Connection->OutBuf << from_cr->GetId();
    Connection->OutBuf << action;
    Connection->OutBuf << action_ext;
    Connection->OutBuf << (item != nullptr);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MoveItem(Critter* from_cr, Item* item, uint8 action, uint8 prev_slot)
{
    STACK_TRACE_ENTRY();

    const auto is_chosen = from_cr == GetOwnedCritter();

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    vector<const Item*> items;
    vector<vector<uint8*>*> items_data;
    vector<vector<uint>*> items_data_sizes;

    if (!is_chosen) {
        const auto& inv_items = from_cr->GetInventory();
        items.reserve(inv_items.size());
        for (const auto* item_ : inv_items) {
            const auto slot = item_->GetCritterSlot();
            if (slot < _engine->Settings.CritterSlotEnabled.size() && _engine->Settings.CritterSlotEnabled[slot] && //
                slot < _engine->Settings.CritterSlotSendData.size() && _engine->Settings.CritterSlotSendData[slot]) {
                items.push_back(item_);
            }
        }

        if (!items.empty()) {
            items_data.resize(items.size());
            items_data_sizes.resize(items.size());
            for (const auto i : xrange(items)) {
                items[i]->StoreData(false, &items_data[i], &items_data_sizes[i]);
            }
        }
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_MOVE_ITEM);
    Connection->OutBuf << from_cr->GetId();
    Connection->OutBuf << action;
    Connection->OutBuf << prev_slot;
    Connection->OutBuf << (item != nullptr);
    Connection->OutBuf << (item != nullptr ? item->GetCritterSlot() : uint8());
    Connection->OutBuf << static_cast<uint16>(items.size());
    for (const auto i : xrange(items)) {
        const auto* item_ = items[i];
        Connection->OutBuf << item_->GetCritterSlot();
        Connection->OutBuf << item_->GetId();
        Connection->OutBuf << item_->GetProtoId();
        NET_WRITE_PROPERTIES(Connection->OutBuf, items_data[i], items_data_sizes[i]);
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play)
{
    STACK_TRACE_ENTRY();

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_ANIMATE);
    Connection->OutBuf << from_cr->GetId();
    Connection->OutBuf << anim1;
    Connection->OutBuf << anim2;
    Connection->OutBuf << (item != nullptr);
    Connection->OutBuf << clear_sequence;
    Connection->OutBuf << delay_play;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_SetAnims(Critter* from_cr, CritterCondition cond, uint anim1, uint anim2)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_SET_ANIMS);
    Connection->OutBuf << from_cr->GetId();
    Connection->OutBuf << cond;
    Connection->OutBuf << anim1;
    Connection->OutBuf << anim2;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddItemOnMap(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto is_added = item->ViewPlaceOnMap;

    vector<uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    item->StoreData(false, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_ITEM_ON_MAP);
    Connection->OutBuf << item->GetId();
    Connection->OutBuf << item->GetProtoId();
    Connection->OutBuf << item->GetHexX();
    Connection->OutBuf << item->GetHexY();
    Connection->OutBuf << is_added;
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItemFromMap(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ERASE_ITEM_FROM_MAP);
    Connection->OutBuf << item->GetId();
    Connection->OutBuf << item->ViewPlaceOnMap;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AnimateItem(Item* item, uint8 from_frm, uint8 to_frm)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ANIMATE_ITEM);
    Connection->OutBuf << item->GetId();
    Connection->OutBuf << from_frm;
    Connection->OutBuf << to_frm;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddItem(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (item->GetIsHidden()) {
        return;
    }

    vector<uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    item->StoreData(true, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_ITEM);
    Connection->OutBuf << item->GetId();
    Connection->OutBuf << item->GetProtoId();
    Connection->OutBuf << item->GetCritterSlot();
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItem(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_REMOVE_ITEM);
    Connection->OutBuf << item->GetId();
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_GlobalInfo(uint8 info_flags)
{
    STACK_TRACE_ENTRY();

#define SEND_LOCATION_SIZE (sizeof(uint) + sizeof(hstring::hash_t) + sizeof(uint16) * 2 + sizeof(uint16) + sizeof(uint) + sizeof(uint8))

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_ownedCr);

    const auto known_locs = _ownedCr->GetKnownLocations();

    auto loc_count = static_cast<uint16>(known_locs.size());

    // Parse message
    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->OutBuf.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->OutBuf << info_flags;

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        Connection->OutBuf << loc_count;

        constexpr char empty_loc[SEND_LOCATION_SIZE] = {0, 0, 0, 0};
        for (uint16 i = 0; i < loc_count;) {
            auto loc_id = known_locs[i];
            const auto* loc = _engine->MapMngr.GetLocation(loc_id);
            if (loc != nullptr && !loc->GetToGarbage()) {
                i++;
                Connection->OutBuf << loc_id;
                Connection->OutBuf << loc->GetProtoId();
                Connection->OutBuf << loc->GetWorldX();
                Connection->OutBuf << loc->GetWorldY();
                Connection->OutBuf << loc->GetRadius();
                Connection->OutBuf << loc->GetColor();
                uint8 count = 0;
                if (loc->IsNonEmptyMapEntrances()) {
                    count = static_cast<uint8>(loc->GetMapEntrances().size() / 2);
                }
                Connection->OutBuf << count;
            }
            else {
                loc_count--;
                _engine->MapMngr.EraseKnownLoc(_ownedCr, loc_id);
                Connection->OutBuf.Push(empty_loc, sizeof(empty_loc));
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        auto gmap_fog = _ownedCr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }
        Connection->OutBuf.Push(gmap_fog.data(), GM_ZONES_FOG_SIZE);
    }

    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

    if (IsBitSet(info_flags, GM_INFO_CRITTERS)) {
        _engine->MapMngr.ProcessVisibleCritters(_ownedCr);
    }
}

void Player::Send_GlobalLocation(Location* loc, bool add)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    constexpr auto info_flags = GM_INFO_LOCATION;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->OutBuf << info_flags;
    Connection->OutBuf << add;
    Connection->OutBuf << loc->GetId();
    Connection->OutBuf << loc->GetProtoId();
    Connection->OutBuf << loc->GetWorldX();
    Connection->OutBuf << loc->GetWorldY();
    Connection->OutBuf << loc->GetRadius();
    Connection->OutBuf << loc->GetColor();
    uint8 count = 0;
    if (loc->IsNonEmptyMapEntrances()) {
        count = static_cast<uint8>(loc->GetMapEntrances().size() / 2);
    }
    Connection->OutBuf << count;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

#undef SEND_LOCATION_SIZE
}

void Player::Send_GlobalMapFog(uint16 zx, uint16 zy, uint8 fog)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    constexpr auto info_flags = GM_INFO_FOG;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->OutBuf << info_flags;
    Connection->OutBuf << zx;
    Connection->OutBuf << zy;
    Connection->OutBuf << fog;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Position(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_POS);
    Connection->OutBuf << cr->GetId();
    Connection->OutBuf << cr->GetHexX();
    Connection->OutBuf << cr->GetHexY();
    Connection->OutBuf << cr->GetHexOffsX();
    Connection->OutBuf << cr->GetHexOffsY();
    Connection->OutBuf << cr->GetDirAngle();
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AllProperties()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    StoreData(true, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ALL_PROPERTIES);
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Teleport(Critter* cr, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_TELEPORT);
    Connection->OutBuf << cr->GetId();
    Connection->OutBuf << to_hx;
    Connection->OutBuf << to_hy;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Talk()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_ownedCr);

    const auto close = _ownedCr->Talk.Type == TalkType::None;
    const auto is_npc = _ownedCr->Talk.Type == TalkType::Critter;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_TALK_NPC);

    if (close) {
        Connection->OutBuf << is_npc;
        Connection->OutBuf << _ownedCr->Talk.CritterId;
        Connection->OutBuf << _ownedCr->Talk.DialogPackId;
        Connection->OutBuf << static_cast<uint8>(0);
    }
    else {
        const auto all_answers = static_cast<uint8>(_ownedCr->Talk.CurDialog.Answers.size());

        auto talk_time = _ownedCr->Talk.TalkTime;
        if (talk_time != 0u) {
            const auto diff = _engine->GameTime.GameTick() - _ownedCr->Talk.StartTick;
            talk_time = diff < talk_time ? talk_time - diff : 1;
        }

        Connection->OutBuf << is_npc;
        Connection->OutBuf << _ownedCr->Talk.CritterId;
        Connection->OutBuf << _ownedCr->Talk.DialogPackId;
        Connection->OutBuf << all_answers;
        Connection->OutBuf << static_cast<uint16>(_ownedCr->Talk.Lexems.length()); // Lexems length
        if (_ownedCr->Talk.Lexems.length() != 0u) {
            Connection->OutBuf.Push(_ownedCr->Talk.Lexems.c_str(), _ownedCr->Talk.Lexems.length()); // Lexems string
        }
        Connection->OutBuf << _ownedCr->Talk.CurDialog.TextId; // Main text_id
        for (auto& answer : _ownedCr->Talk.CurDialog.Answers) {
            Connection->OutBuf << answer.TextId; // Answers text_id
        }
        Connection->OutBuf << talk_time; // Talk time
    }

    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TimeSync()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_TIME_SYNC);
    Connection->OutBuf << _engine->GetYear();
    Connection->OutBuf << _engine->GetMonth();
    Connection->OutBuf << _engine->GetDay();
    Connection->OutBuf << _engine->GetHour();
    Connection->OutBuf << _engine->GetMinute();
    Connection->OutBuf << _engine->GetSecond();
    Connection->OutBuf << _engine->GetTimeMultiplier();
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Text(Critter* from_cr, string_view text, uint8 how_say)
{
    STACK_TRACE_ENTRY();

    if (text.empty()) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};
    Send_TextEx(from_id, text, how_say, false);
}

void Player::Send_TextEx(ident_t from_id, string_view text, uint8 how_say, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_TEXT);
    Connection->OutBuf << from_id;
    Connection->OutBuf << how_say;
    Connection->OutBuf << text;
    Connection->OutBuf << unsafe_text;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsg(Critter* from_cr, uint num_str, uint8 how_say, uint16 num_msg)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (num_str == 0u) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG);
    Connection->OutBuf << from_id;
    Connection->OutBuf << how_say;
    Connection->OutBuf << num_msg;
    Connection->OutBuf << num_str;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsg(ident_t from_id, uint num_str, uint8 how_say, uint16 num_msg)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (num_str == 0u) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG);
    Connection->OutBuf << from_id;
    Connection->OutBuf << how_say;
    Connection->OutBuf << num_msg;
    Connection->OutBuf << num_str;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsgLex(Critter* from_cr, uint num_str, uint8 how_say, uint16 num_msg, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (num_str == 0u) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG_LEX);
    Connection->OutBuf << from_id;
    Connection->OutBuf << how_say;
    Connection->OutBuf << num_msg;
    Connection->OutBuf << num_str;
    Connection->OutBuf << lexems;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsgLex(ident_t from_id, uint num_str, uint8 how_say, uint16 num_msg, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (num_str == 0u) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG_LEX);
    Connection->OutBuf << from_id;
    Connection->OutBuf << how_say;
    Connection->OutBuf << num_msg;
    Connection->OutBuf << num_str;
    Connection->OutBuf << lexems;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapText(uint16 hx, uint16 hy, uint color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT);
    Connection->OutBuf << hx;
    Connection->OutBuf << hy;
    Connection->OutBuf << color;
    Connection->OutBuf << text;
    Connection->OutBuf << unsafe_text;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsg(uint16 hx, uint16 hy, uint color, uint16 num_msg, uint num_str)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT_MSG);
    Connection->OutBuf << hx;
    Connection->OutBuf << hy;
    Connection->OutBuf << color;
    Connection->OutBuf << num_msg;
    Connection->OutBuf << num_str;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsgLex(uint16 hx, uint16 hy, uint color, uint16 num_msg, uint num_str, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT_MSG_LEX);
    Connection->OutBuf << hx;
    Connection->OutBuf << hy;
    Connection->OutBuf << color;
    Connection->OutBuf << num_msg;
    Connection->OutBuf << num_str;
    Connection->OutBuf << lexems;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AutomapsInfo(void* locs_vec, Location* loc)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // Todo: restore automaps
    /*if (locs_vec)
    {
        LocationVec* locs = (LocationVec*)locs_vec;
        for (uint i = 0, j = (uint)locs->size(); i < j; i++)
        {
            Location* loc_ = (*locs)[i];
            if (loc_->IsNonEmptyAutomaps())
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                automaps->Release();
            }
        }

        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->OutBuf.StartMsg(NETMSG_AUTOMAPS_INFO);
        Connection->OutBuf << (bool)true; // Clear list
        Connection->OutBuf << (uint16)locs->size();
        for (uint i = 0, j = (uint)locs->size(); i < j; i++)
        {
            Location* loc_ = (*locs)[i];
            Connection->OutBuf << loc_->GetId();
            Connection->OutBuf << loc_->GetProtoId();
            if (loc_->IsNonEmptyAutomaps())
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                Connection->OutBuf << (uint16)automaps->GetSize();
                for (uint k = 0, l = (uint)automaps->GetSize(); k < l; k++)
                {
                    hstring pid = *(hash*)automaps->At(k);
                    Connection->OutBuf << pid;
                    Connection->OutBuf << (uint8)loc_->GetMapIndex(pid);
                }
                automaps->Release();
            }
            else
            {
                Connection->OutBuf << (uint16)0;
            }
        }
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }

    if (loc)
    {
        CScriptArray* automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr);

        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->OutBuf.StartMsg(NETMSG_AUTOMAPS_INFO);
        Connection->OutBuf << (bool)false; // Append this information
        Connection->OutBuf << (uint16)1;
        Connection->OutBuf << loc->GetId();
        Connection->OutBuf << loc->GetProtoId();
        Connection->OutBuf << (uint16)(automaps ? automaps->GetSize() : 0);
        if (automaps)
        {
            for (uint i = 0, j = (uint)automaps->GetSize(); i < j; i++)
            {
                hash pid = *(hash*)automaps->At(i);
                Connection->OutBuf << pid;
                Connection->OutBuf << (uint8)loc->GetMapIndex(pid);
            }
        }
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);

        if (automaps)
            automaps->Release();
    }*/
}

void Player::Send_Effect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_EFFECT);
    Connection->OutBuf << eff_pid;
    Connection->OutBuf << hx;
    Connection->OutBuf << hy;
    Connection->OutBuf << radius;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_FLY_EFFECT);
    Connection->OutBuf << eff_pid;
    Connection->OutBuf << from_cr_id;
    Connection->OutBuf << to_cr_id;
    Connection->OutBuf << from_hx;
    Connection->OutBuf << from_hy;
    Connection->OutBuf << to_hx;
    Connection->OutBuf << to_hy;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_PLAY_SOUND);
    Connection->OutBuf << cr_id_synchronize;
    Connection->OutBuf << sound_name;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_ViewMap()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_ownedCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_VIEW_MAP);
    Connection->OutBuf << _ownedCr->ViewMapHx;
    Connection->OutBuf << _ownedCr->ViewMapHy;
    Connection->OutBuf << _ownedCr->ViewMapLocId;
    Connection->OutBuf << _ownedCr->ViewMapLocEnt;
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_SomeItem(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    item->StoreData(false, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_SOME_ITEM);
    Connection->OutBuf << item->GetId();
    Connection->OutBuf << item->GetProtoId();
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_PlaceToGameComplete()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_PLACE_TO_GAME_COMPLETE);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddAllItems()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_ownedCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CLEAR_ITEMS);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

    for (auto* item : _ownedCr->_invItems) {
        Send_AddItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ALL_ITEMS_SEND);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AllAutomapsInfo()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_ownedCr);

    vector<Location*> locs;
    for (const auto loc_id : _ownedCr->GetKnownLocations()) {
        auto* loc = _engine->MapMngr.GetLocation(loc_id);
        if (loc != nullptr && loc->IsNonEmptyAutomaps()) {
            locs.push_back(loc);
        }
    }

    Send_AutomapsInfo(&locs, nullptr);
}

void Player::Send_SomeItems(const vector<Item*>* items, int param)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<vector<uint8*>*> items_data(items != nullptr ? items->size() : 0);
    vector<vector<uint>*> items_data_sizes(items != nullptr ? items->size() : 0);
    for (size_t i = 0, j = items_data.size(); i < j; i++) {
        items->at(i)->StoreData(false, &items_data[i], &items_data_sizes[i]);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_SOME_ITEMS);
    Connection->OutBuf << param;
    Connection->OutBuf << (items == nullptr);
    Connection->OutBuf << static_cast<uint>(items != nullptr ? items->size() : 0);
    if (items != nullptr) {
        for (size_t i = 0, j = items_data.size(); i < j; i++) {
            const auto* item = items->at(i);
            Connection->OutBuf << item->GetId();
            Connection->OutBuf << item->GetProtoId();
            NET_WRITE_PROPERTIES(Connection->OutBuf, items_data[i], items_data_sizes[i]);
        }
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}
