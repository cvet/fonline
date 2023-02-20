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

    Connection->Bout.StartMsg(NETMSG_ADD_CRITTER);
    Connection->Bout << cr->GetId();
    Connection->Bout << cr->GetHexX();
    Connection->Bout << cr->GetHexY();
    Connection->Bout << cr->GetHexOffsX();
    Connection->Bout << cr->GetHexOffsY();
    Connection->Bout << cr->GetDirAngle();
    Connection->Bout << cr->GetCond();
    Connection->Bout << cr->GetAnim1Alive();
    Connection->Bout << cr->GetAnim1Knockout();
    Connection->Bout << cr->GetAnim1Dead();
    Connection->Bout << cr->GetAnim2Alive();
    Connection->Bout << cr->GetAnim2Knockout();
    Connection->Bout << cr->GetAnim2Dead();
    Connection->Bout << cr->IsOwnedByPlayer();
    Connection->Bout << (cr->IsOwnedByPlayer() && cr->GetOwner() == nullptr);
    Connection->Bout << is_chosen;
    Connection->Bout << cr->GetProtoId();
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    Connection->Bout.EndMsg();

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
    Connection->Bout.StartMsg(NETMSG_REMOVE_CRITTER);
    Connection->Bout << cr->GetId();
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_LOADMAP);
    Connection->Bout << (loc != nullptr ? loc->GetId() : ident_t {});
    Connection->Bout << (map != nullptr ? map->GetId() : ident_t {});
    Connection->Bout << pid_loc;
    Connection->Bout << pid_map;
    Connection->Bout << map_index_in_loc;
    if (map != nullptr) {
        NET_WRITE_PROPERTIES(Connection->Bout, map_data, map_data_sizes);
        NET_WRITE_PROPERTIES(Connection->Bout, loc_data, loc_data_sizes);
    }
    Connection->Bout.EndMsg();
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
        Connection->Bout.StartMsg(NETMSG_POD_PROPERTY(data_size, additional_args));
    }
    else {
        Connection->Bout.StartMsg(NETMSG_COMPLEX_PROPERTY);
    }

    Connection->Bout << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        Connection->Bout << dynamic_cast<Item*>(entity)->GetCritterId();
        Connection->Bout << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    case NetProperty::Critter:
        Connection->Bout << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    case NetProperty::MapItem:
        Connection->Bout << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    case NetProperty::ChosenItem:
        Connection->Bout << dynamic_cast<ServerEntity*>(entity)->GetId();
        break;
    default:
        break;
    }

    if (is_pod) {
        Connection->Bout << prop->GetRegIndex();
        Connection->Bout.Push(data, data_size);
    }
    else {
        Connection->Bout << prop->GetRegIndex();
        if (data_size != 0u) {
            Connection->Bout.Push(data, data_size);
        }
    }

    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Move(Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!from_cr->Moving.Steps.empty()) {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->Bout.StartMsg(NETMSG_CRITTER_MOVE);
        Connection->Bout << from_cr->GetId();
        Connection->Bout << static_cast<uint>(std::ceil(from_cr->Moving.WholeTime));
        Connection->Bout << _engine->GameTime.FrameTick() - from_cr->Moving.StartTick;
        Connection->Bout << from_cr->Moving.Speed;
        Connection->Bout << from_cr->Moving.StartHexX;
        Connection->Bout << from_cr->Moving.StartHexY;
        Connection->Bout << static_cast<uint16>(from_cr->Moving.Steps.size());
        for (auto step : from_cr->Moving.Steps) {
            Connection->Bout << step;
        }
        Connection->Bout << static_cast<uint16>(from_cr->Moving.ControlSteps.size());
        for (auto control_step : from_cr->Moving.ControlSteps) {
            Connection->Bout << control_step;
        }
        Connection->Bout << from_cr->Moving.EndOx;
        Connection->Bout << from_cr->Moving.EndOy;
        Connection->Bout.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
    else {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->Bout.StartMsg(NETMSG_CRITTER_STOP_MOVE);
        Connection->Bout << from_cr->GetId();
        Connection->Bout << from_cr->GetHexX();
        Connection->Bout << from_cr->GetHexY();
        Connection->Bout << from_cr->GetHexOffsX();
        Connection->Bout << from_cr->GetHexOffsY();
        Connection->Bout << from_cr->GetDirAngle();
        Connection->Bout.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
}

void Player::Send_Dir(Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_CRITTER_DIR);
    Connection->Bout << from_cr->GetId();
    Connection->Bout << from_cr->GetDirAngle();
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Action(Critter* from_cr, int action, int action_ext, Item* item)
{
    STACK_TRACE_ENTRY();

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_CRITTER_ACTION);
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << action_ext;
    Connection->Bout << (item != nullptr);
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_CRITTER_MOVE_ITEM);
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << prev_slot;
    Connection->Bout << (item != nullptr);
    Connection->Bout << (item != nullptr ? item->GetCritterSlot() : uint8());
    Connection->Bout << static_cast<uint16>(items.size());
    for (const auto i : xrange(items)) {
        const auto* item_ = items[i];
        Connection->Bout << item_->GetCritterSlot();
        Connection->Bout << item_->GetId();
        Connection->Bout << item_->GetProtoId();
        NET_WRITE_PROPERTIES(Connection->Bout, items_data[i], items_data_sizes[i]);
    }
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play)
{
    STACK_TRACE_ENTRY();

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_CRITTER_ANIMATE);
    Connection->Bout << from_cr->GetId();
    Connection->Bout << anim1;
    Connection->Bout << anim2;
    Connection->Bout << (item != nullptr);
    Connection->Bout << clear_sequence;
    Connection->Bout << delay_play;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_SetAnims(Critter* from_cr, CritterCondition cond, uint anim1, uint anim2)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_CRITTER_SET_ANIMS);
    Connection->Bout << from_cr->GetId();
    Connection->Bout << cond;
    Connection->Bout << anim1;
    Connection->Bout << anim2;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_ADD_ITEM_ON_MAP);
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetHexX();
    Connection->Bout << item->GetHexY();
    Connection->Bout << is_added;
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItemFromMap(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_ERASE_ITEM_FROM_MAP);
    Connection->Bout << item->GetId();
    Connection->Bout << item->ViewPlaceOnMap;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AnimateItem(Item* item, uint8 from_frm, uint8 to_frm)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_ANIMATE_ITEM);
    Connection->Bout << item->GetId();
    Connection->Bout << from_frm;
    Connection->Bout << to_frm;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_ADD_ITEM);
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetCritterSlot();
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItem(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_REMOVE_ITEM);
    Connection->Bout << item->GetId();
    Connection->Bout.EndMsg();
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

    Connection->Bout.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->Bout << info_flags;

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        Connection->Bout << loc_count;

        constexpr char empty_loc[SEND_LOCATION_SIZE] = {0, 0, 0, 0};
        for (uint16 i = 0; i < loc_count;) {
            auto loc_id = known_locs[i];
            const auto* loc = _engine->MapMngr.GetLocation(loc_id);
            if (loc != nullptr && !loc->GetToGarbage()) {
                i++;
                Connection->Bout << loc_id;
                Connection->Bout << loc->GetProtoId();
                Connection->Bout << loc->GetWorldX();
                Connection->Bout << loc->GetWorldY();
                Connection->Bout << loc->GetRadius();
                Connection->Bout << loc->GetColor();
                uint8 count = 0;
                if (loc->IsNonEmptyMapEntrances()) {
                    count = static_cast<uint8>(loc->GetMapEntrances().size() / 2);
                }
                Connection->Bout << count;
            }
            else {
                loc_count--;
                _engine->MapMngr.EraseKnownLoc(_ownedCr, loc_id);
                Connection->Bout.Push(empty_loc, sizeof(empty_loc));
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        auto gmap_fog = _ownedCr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }
        Connection->Bout.Push(gmap_fog.data(), GM_ZONES_FOG_SIZE);
    }

    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->Bout << info_flags;
    Connection->Bout << add;
    Connection->Bout << loc->GetId();
    Connection->Bout << loc->GetProtoId();
    Connection->Bout << loc->GetWorldX();
    Connection->Bout << loc->GetWorldY();
    Connection->Bout << loc->GetRadius();
    Connection->Bout << loc->GetColor();
    uint8 count = 0;
    if (loc->IsNonEmptyMapEntrances()) {
        count = static_cast<uint8>(loc->GetMapEntrances().size() / 2);
    }
    Connection->Bout << count;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

#undef SEND_LOCATION_SIZE
}

void Player::Send_GlobalMapFog(uint16 zx, uint16 zy, uint8 fog)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    constexpr auto info_flags = GM_INFO_FOG;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->Bout << info_flags;
    Connection->Bout << zx;
    Connection->Bout << zy;
    Connection->Bout << fog;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Position(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_CRITTER_POS);
    Connection->Bout << cr->GetId();
    Connection->Bout << cr->GetHexX();
    Connection->Bout << cr->GetHexY();
    Connection->Bout << cr->GetHexOffsX();
    Connection->Bout << cr->GetHexOffsY();
    Connection->Bout << cr->GetDirAngle();
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_ALL_PROPERTIES);
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Teleport(Critter* cr, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_CRITTER_TELEPORT);
    Connection->Bout << cr->GetId();
    Connection->Bout << to_hx;
    Connection->Bout << to_hy;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_TALK_NPC);

    if (close) {
        Connection->Bout << is_npc;
        Connection->Bout << _ownedCr->Talk.CritterId;
        Connection->Bout << _ownedCr->Talk.DialogPackId;
        Connection->Bout << static_cast<uint8>(0);
    }
    else {
        const auto all_answers = static_cast<uint8>(_ownedCr->Talk.CurDialog.Answers.size());

        auto talk_time = _ownedCr->Talk.TalkTime;
        if (talk_time != 0u) {
            const auto diff = _engine->GameTime.GameTick() - _ownedCr->Talk.StartTick;
            talk_time = diff < talk_time ? talk_time - diff : 1;
        }

        Connection->Bout << is_npc;
        Connection->Bout << _ownedCr->Talk.CritterId;
        Connection->Bout << _ownedCr->Talk.DialogPackId;
        Connection->Bout << all_answers;
        Connection->Bout << static_cast<uint16>(_ownedCr->Talk.Lexems.length()); // Lexems length
        if (_ownedCr->Talk.Lexems.length() != 0u) {
            Connection->Bout.Push(_ownedCr->Talk.Lexems.c_str(), _ownedCr->Talk.Lexems.length()); // Lexems string
        }
        Connection->Bout << _ownedCr->Talk.CurDialog.TextId; // Main text_id
        for (auto& answer : _ownedCr->Talk.CurDialog.Answers) {
            Connection->Bout << answer.TextId; // Answers text_id
        }
        Connection->Bout << talk_time; // Talk time
    }

    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TimeSync()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_TIME_SYNC);
    Connection->Bout << _engine->GetYear();
    Connection->Bout << _engine->GetMonth();
    Connection->Bout << _engine->GetDay();
    Connection->Bout << _engine->GetHour();
    Connection->Bout << _engine->GetMinute();
    Connection->Bout << _engine->GetSecond();
    Connection->Bout << _engine->GetTimeMultiplier();
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_CRITTER_TEXT);
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_MSG);
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_MSG);
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_MSG_LEX);
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_MSG_LEX);
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapText(uint16 hx, uint16 hy, uint color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_MAP_TEXT);
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsg(uint16 hx, uint16 hy, uint color, uint16 num_msg, uint num_str)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_MAP_TEXT_MSG);
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsgLex(uint16 hx, uint16 hy, uint color, uint16 num_msg, uint num_str, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_MAP_TEXT_MSG_LEX);
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems;
    Connection->Bout.EndMsg();
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
        Connection->Bout.StartMsg(NETMSG_AUTOMAPS_INFO);
        Connection->Bout << (bool)true; // Clear list
        Connection->Bout << (uint16)locs->size();
        for (uint i = 0, j = (uint)locs->size(); i < j; i++)
        {
            Location* loc_ = (*locs)[i];
            Connection->Bout << loc_->GetId();
            Connection->Bout << loc_->GetProtoId();
            if (loc_->IsNonEmptyAutomaps())
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                Connection->Bout << (uint16)automaps->GetSize();
                for (uint k = 0, l = (uint)automaps->GetSize(); k < l; k++)
                {
                    hstring pid = *(hash*)automaps->At(k);
                    Connection->Bout << pid;
                    Connection->Bout << (uint8)loc_->GetMapIndex(pid);
                }
                automaps->Release();
            }
            else
            {
                Connection->Bout << (uint16)0;
            }
        }
        Connection->Bout.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }

    if (loc)
    {
        CScriptArray* automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr);

        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->Bout.StartMsg(NETMSG_AUTOMAPS_INFO);
        Connection->Bout << (bool)false; // Append this information
        Connection->Bout << (uint16)1;
        Connection->Bout << loc->GetId();
        Connection->Bout << loc->GetProtoId();
        Connection->Bout << (uint16)(automaps ? automaps->GetSize() : 0);
        if (automaps)
        {
            for (uint i = 0, j = (uint)automaps->GetSize(); i < j; i++)
            {
                hash pid = *(hash*)automaps->At(i);
                Connection->Bout << pid;
                Connection->Bout << (uint8)loc->GetMapIndex(pid);
            }
        }
        Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_EFFECT);
    Connection->Bout << eff_pid;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << radius;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_FLY_EFFECT);
    Connection->Bout << eff_pid;
    Connection->Bout << from_cr_id;
    Connection->Bout << to_cr_id;
    Connection->Bout << from_hx;
    Connection->Bout << from_hy;
    Connection->Bout << to_hx;
    Connection->Bout << to_hy;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_PLAY_SOUND);
    Connection->Bout << cr_id_synchronize;
    Connection->Bout << sound_name;
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_ViewMap()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_ownedCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_VIEW_MAP);
    Connection->Bout << _ownedCr->ViewMapHx;
    Connection->Bout << _ownedCr->ViewMapHy;
    Connection->Bout << _ownedCr->ViewMapLocId;
    Connection->Bout << _ownedCr->ViewMapLocEnt;
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_SOME_ITEM);
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_PlaceToGameComplete()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_PLACE_TO_GAME_COMPLETE);
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddAllItems()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_ownedCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_CLEAR_ITEMS);
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

    for (auto* item : _ownedCr->_invItems) {
        Send_AddItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout.StartMsg(NETMSG_ALL_ITEMS_SEND);
    Connection->Bout.EndMsg();
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
    Connection->Bout.StartMsg(NETMSG_SOME_ITEMS);
    Connection->Bout << param;
    Connection->Bout << (items == nullptr);
    Connection->Bout << static_cast<uint>(items != nullptr ? items->size() : 0);
    if (items != nullptr) {
        for (size_t i = 0, j = items_data.size(); i < j; i++) {
            const auto* item = items->at(i);
            Connection->Bout << item->GetId();
            Connection->Bout << item->GetProtoId();
            NET_WRITE_PROPERTIES(Connection->Bout, items_data[i], items_data_sizes[i]);
        }
    }
    Connection->Bout.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}
