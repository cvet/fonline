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

Player::Player(FOServer* engine, uint id, string_view name, ClientConnection* connection, const ProtoCritter* proto) : ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), proto), PlayerProperties(GetInitRef()), Connection {connection}, _name {name}, _talkNextTick {_engine->GameTime.GameTick() + PROCESS_TALK_TICK}
{
}

Player::~Player()
{
    Connection->HardDisconnect();
    delete Connection;
}

auto Player::GetName() const -> string_view
{
    return _name;
}

auto Player::GetIp() const -> uint
{
    return Connection->GetIp();
}

auto Player::GetHost() const -> string_view
{
    return Connection->GetHost();
}

auto Player::GetPort() const -> ushort
{
    return Connection->GetPort();
}

void Player::Send_AddCritter(Critter* cr)
{
    if (IsSendDisabled()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(ushort) * 2 + sizeof(short) * 3 + sizeof(CritterCondition) + sizeof(uint) * 6 + sizeof(bool) * 3 + sizeof(hstring::hash_t);

    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto is_chosen = cr == GetOwnedCritter();
    const auto whole_data_size = cr->StoreData(is_chosen, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->Bout << NETMSG_ADD_CRITTER;
    Connection->Bout << msg_len;
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

    CONNECTION_OUTPUT_END(Connection);

    if (cr != _ownedCr) {
        Send_MoveItem(cr, nullptr, ACTION_REFRESH, 0);
    }

    if (cr->IsMoving()) {
        Send_Move(cr);
    }
}

void Player::Send_RemoveCritter(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_REMOVE_CRITTER;
    Connection->Bout << cr->GetId();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_LoadMap(Map* map, MapManager& map_mngr)
{
    if (IsSendDisabled()) {
        return;
    }

    Location* loc = nullptr;
    hstring pid_map;
    hstring pid_loc;
    uchar map_index_in_loc = 0;
    uint hash_tiles = 0;
    uint hash_scen = 0;

    if (map == nullptr) {
        RUNTIME_ASSERT(_ownedCr);
        map = map_mngr.GetMap(_ownedCr->GetMapId());
    }

    if (map != nullptr) {
        loc = map->GetLocation();
        pid_map = map->GetProtoId();
        pid_loc = loc->GetProtoId();
        map_index_in_loc = static_cast<uchar>(loc->GetMapIndex(pid_map));
        hash_tiles = map->GetStaticMap()->HashTiles;
        hash_scen = map->GetStaticMap()->HashScen;
    }

    uint msg_len = sizeof(uint) + sizeof(uint) + sizeof(hstring::hash_t) * 2 + sizeof(uchar) + sizeof(int) + sizeof(uchar) + sizeof(hstring::hash_t) * 2;
    vector<uchar*>* map_data = nullptr;
    vector<uint>* map_data_sizes = nullptr;
    vector<uchar*>* loc_data = nullptr;
    vector<uint>* loc_data_sizes = nullptr;
    if (map != nullptr) {
        const auto map_whole_data_size = map->StoreData(false, &map_data, &map_data_sizes);
        const auto loc_whole_data_size = loc->StoreData(false, &loc_data, &loc_data_sizes);
        msg_len += sizeof(ushort) + map_whole_data_size + sizeof(ushort) + loc_whole_data_size;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->Bout << NETMSG_LOADMAP;
    Connection->Bout << msg_len;
    Connection->Bout << (loc != nullptr ? loc->GetId() : 0u);
    Connection->Bout << (map != nullptr ? map->GetId() : 0u);
    Connection->Bout << pid_loc;
    Connection->Bout << pid_map;
    Connection->Bout << map_index_in_loc;
    Connection->Bout << hash_tiles;
    Connection->Bout << hash_scen;
    if (map != nullptr) {
        NET_WRITE_PROPERTIES(Connection->Bout, map_data, map_data_sizes);
        NET_WRITE_PROPERTIES(Connection->Bout, loc_data, loc_data_sizes);
    }

    CONNECTION_OUTPUT_END(Connection);

    IsTransferring = true;
}

void Player::Send_Property(NetProperty type, const Property* prop, Entity* entity)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(entity);

    if (IsSendDisabled()) {
        return;
    }
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
        Connection->Bout << NETMSG_POD_PROPERTY(data_size, additional_args);
    }
    else {
        const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(char) + additional_args * sizeof(uint) + sizeof(ushort) + data_size;

        Connection->Bout << NETMSG_COMPLEX_PROPERTY;
        Connection->Bout << msg_len;
    }

    Connection->Bout << static_cast<char>(type);

    switch (type) {
    case NetProperty::CritterItem:
        Connection->Bout << dynamic_cast<Item*>(entity)->GetCritId();
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

    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Move(Critter* from_cr)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    if (!from_cr->Moving.Steps.empty()) {
        const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(uint) * 2 + sizeof(bool) + sizeof(ushort) * 2 + //
            static_cast<uint>(sizeof(uchar) * from_cr->Moving.Steps.size()) + static_cast<uint>(sizeof(ushort) * from_cr->Moving.ControlSteps.size()) + sizeof(char) * 2;

        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->Bout << NETMSG_CRITTER_MOVE;
        Connection->Bout << msg_len;
        Connection->Bout << from_cr->GetId();
        Connection->Bout << static_cast<uint>(std::ceil(from_cr->Moving.WholeTime));
        Connection->Bout << _engine->GameTime.FrameTick() - from_cr->Moving.StartTick;
        Connection->Bout << from_cr->Moving.IsRunning;
        Connection->Bout << from_cr->Moving.StartHexX;
        Connection->Bout << from_cr->Moving.StartHexY;
        Connection->Bout << static_cast<ushort>(from_cr->Moving.Steps.size());
        for (auto step : from_cr->Moving.Steps) {
            Connection->Bout << step;
        }
        Connection->Bout << static_cast<ushort>(from_cr->Moving.ControlSteps.size());
        for (auto control_step : from_cr->Moving.ControlSteps) {
            Connection->Bout << control_step;
        }
        Connection->Bout << from_cr->Moving.EndOx;
        Connection->Bout << from_cr->Moving.EndOy;
        CONNECTION_OUTPUT_END(Connection);
    }
    else {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->Bout << NETMSG_CRITTER_STOP_MOVE;
        Connection->Bout << from_cr->GetId();
        Connection->Bout << from_cr->GetHexX();
        Connection->Bout << from_cr->GetHexY();
        Connection->Bout << from_cr->GetHexOffsX();
        Connection->Bout << from_cr->GetHexOffsY();
        Connection->Bout << from_cr->GetDirAngle();
        CONNECTION_OUTPUT_END(Connection);
    }
}

void Player::Send_Dir(Critter* from_cr)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_DIR;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << from_cr->GetDirAngle();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Action(Critter* from_cr, int action, int action_ext, Item* item)
{
    if (IsSendDisabled()) {
        return;
    }

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_ACTION;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << action_ext;
    Connection->Bout << (item != nullptr);
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot)
{
    if (IsSendDisabled()) {
        return;
    }

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(action) + sizeof(prev_slot) + sizeof(bool);

    const auto& inv_items = from_cr->GetInventory();
    vector<const Item*> items;
    items.reserve(inv_items.size());
    for (const auto* item_ : inv_items) {
        const auto slot = item_->GetCritSlot();
        if (slot < _engine->Settings.CritterSlotEnabled.size() && _engine->Settings.CritterSlotEnabled[slot] && slot < _engine->Settings.CritterSlotSendData.size() && _engine->Settings.CritterSlotSendData[slot]) {
            items.push_back(item_);
        }
    }

    msg_len += sizeof(ushort);
    vector<vector<uchar*>*> items_data(items.size());
    vector<vector<uint>*> items_data_sizes(items.size());
    for (const auto i : xrange(items)) {
        const auto whole_data_size = items[i]->StoreData(false, &items_data[i], &items_data_sizes[i]);
        msg_len += sizeof(uchar) + sizeof(uint) + sizeof(hstring::hash_t) + sizeof(ushort) + whole_data_size;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_MOVE_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << prev_slot;
    Connection->Bout << (item != nullptr);
    Connection->Bout << static_cast<ushort>(items.size());
    for (const auto i : xrange(items)) {
        const auto* item_ = items[i];
        Connection->Bout << item_->GetCritSlot();
        Connection->Bout << item_->GetId();
        Connection->Bout << item_->GetProtoId();
        NET_WRITE_PROPERTIES(Connection->Bout, items_data[i], items_data_sizes[i]);
    }
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play)
{
    if (IsSendDisabled()) {
        return;
    }

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_ANIMATE;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << anim1;
    Connection->Bout << anim2;
    Connection->Bout << (item != nullptr);
    Connection->Bout << clear_sequence;
    Connection->Bout << delay_play;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_SetAnims(Critter* from_cr, CritterCondition cond, uint anim1, uint anim2)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_SET_ANIMS;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << cond;
    Connection->Bout << anim1;
    Connection->Bout << anim2;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddItemOnMap(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    const auto is_added = item->ViewPlaceOnMap;
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(hstring::hash_t) + sizeof(ushort) * 2 + sizeof(bool);

    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = item->StoreData(false, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_ADD_ITEM_ON_MAP;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetHexX();
    Connection->Bout << item->GetHexY();
    Connection->Bout << is_added;
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItemFromMap(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_ERASE_ITEM_FROM_MAP;
    Connection->Bout << item->GetId();
    Connection->Bout << item->ViewPlaceOnMap;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_ANIMATE_ITEM;
    Connection->Bout << item->GetId();
    Connection->Bout << from_frm;
    Connection->Bout << to_frm;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddItem(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }
    if (item->GetIsHidden()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(hstring::hash_t) + sizeof(uchar);
    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = item->StoreData(true, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_ADD_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetCritSlot();
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItem(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_REMOVE_ITEM;
    Connection->Bout << item->GetId();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_GlobalInfo(uchar info_flags, MapManager& map_mngr)
{
#define SEND_LOCATION_SIZE (sizeof(uint) + sizeof(hstring::hash_t) + sizeof(ushort) * 2 + sizeof(ushort) + sizeof(uint) + sizeof(uchar))

    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    RUNTIME_ASSERT(_ownedCr);

    if (_ownedCr->LockMapTransfers != 0) {
        return;
    }

    const auto known_locs = _ownedCr->GetKnownLocations();

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
    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        Connection->Bout << loc_count;

        constexpr char empty_loc[SEND_LOCATION_SIZE] = {0, 0, 0, 0};
        for (ushort i = 0; i < loc_count;) {
            auto loc_id = known_locs[i];
            const auto* loc = map_mngr.GetLocation(loc_id);
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
                map_mngr.EraseKnownLoc(_ownedCr, loc_id);
                Connection->Bout.Push(empty_loc, sizeof(empty_loc));
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        auto gmap_fog = _ownedCr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }
        Connection->Bout.Push(&gmap_fog[0], GM_ZONES_FOG_SIZE);
    }

    CONNECTION_OUTPUT_END(Connection);

    if (IsBitSet(info_flags, GM_INFO_CRITTERS)) {
        map_mngr.ProcessVisibleCritters(_ownedCr);
    }
}

void Player::Send_GlobalLocation(Location* loc, bool add)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    constexpr auto info_flags = GM_INFO_LOCATION;
    constexpr uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(info_flags) + sizeof(add) + SEND_LOCATION_SIZE;

    CONNECTION_OUTPUT_BEGIN(Connection);
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
    CONNECTION_OUTPUT_END(Connection);

#undef SEND_LOCATION_SIZE
}

void Player::Send_GlobalMapFog(ushort zx, ushort zy, uchar fog)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    constexpr auto info_flags = GM_INFO_FOG;
    constexpr uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(info_flags) + sizeof(zx) + sizeof(zy) + sizeof(fog);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;
    Connection->Bout << zx;
    Connection->Bout << zy;
    Connection->Bout << fog;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Position(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_POS;
    Connection->Bout << cr->GetId();
    Connection->Bout << cr->GetHexX();
    Connection->Bout << cr->GetHexY();
    Connection->Bout << cr->GetHexOffsX();
    Connection->Bout << cr->GetHexOffsY();
    Connection->Bout << cr->GetDirAngle();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AllProperties()
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(uint);

    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = StoreData(true, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_ALL_PROPERTIES;
    Connection->Bout << msg_len;
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Teleport(Critter* cr, ushort to_hx, ushort to_hy)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_TELEPORT;
    Connection->Bout << cr->GetId();
    Connection->Bout << to_hx;
    Connection->Bout << to_hy;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Talk()
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    RUNTIME_ASSERT(_ownedCr);

    const auto close = _ownedCr->_talk.Type == TalkType::None;
    const auto is_npc = static_cast<uchar>(_ownedCr->_talk.Type == TalkType::Critter);
    const auto talk_id = (is_npc != 0u ? _ownedCr->_talk.CritterId : _ownedCr->_talk.DialogPackId.as_uint());
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(is_npc) + sizeof(talk_id) + sizeof(uchar);

    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->Bout << NETMSG_TALK_NPC;

    if (close) {
        Connection->Bout << msg_len;
        Connection->Bout << is_npc;
        Connection->Bout << talk_id;
        Connection->Bout << static_cast<uchar>(0);
    }
    else {
        const auto all_answers = static_cast<uchar>(_ownedCr->_talk.CurDialog.Answers.size());
        msg_len += sizeof(uint) + sizeof(uint) * all_answers + sizeof(uint) + sizeof(ushort) + static_cast<uint>(_ownedCr->_talk.Lexems.length());

        auto talk_time = _ownedCr->_talk.TalkTime;
        if (talk_time != 0u) {
            const auto diff = _engine->GameTime.GameTick() - _ownedCr->_talk.StartTick;
            talk_time = diff < talk_time ? talk_time - diff : 1;
        }

        Connection->Bout << msg_len;
        Connection->Bout << is_npc;
        Connection->Bout << talk_id;
        Connection->Bout << all_answers;
        Connection->Bout << static_cast<ushort>(_ownedCr->_talk.Lexems.length()); // Lexems length
        if (_ownedCr->_talk.Lexems.length() != 0u) {
            Connection->Bout.Push(_ownedCr->_talk.Lexems.c_str(), static_cast<uint>(_ownedCr->_talk.Lexems.length())); // Lexems string
        }
        Connection->Bout << _ownedCr->_talk.CurDialog.TextId; // Main text_id
        for (auto& answer : _ownedCr->_talk.CurDialog.Answers) {
            Connection->Bout << answer.TextId; // Answers text_id
        }
        Connection->Bout << talk_time; // Talk time
    }

    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_GameInfo(Map* map)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
    }

    /*int day_time[4];
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

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_GAME_INFO;
    Connection->Bout << Globals->GetYearStart();
    Connection->Bout << Globals->GetYear();
    Connection->Bout << Globals->GetMonth();
    Connection->Bout << Globals->GetDay();
    Connection->Bout << Globals->GetHour();
    Connection->Bout << Globals->GetMinute();
    Connection->Bout << Globals->GetSecond();
    Connection->Bout << Globals->GetTimeMultiplier();
    Connection->Bout << no_log_out;
    Connection->Bout.Push(day_time, sizeof(day_time));
    Connection->Bout.Push(day_color, sizeof(day_color));
    CONNECTION_OUTPUT_END(Connection);*/
}

void Player::Send_Text(Critter* from_cr, string_view text, uchar how_say)
{
    if (IsSendDisabled()) {
        return;
    }
    if (text.empty()) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : 0;
    Send_TextEx(from_id, text, how_say, false);
}

void Player::Send_TextEx(uint from_id, string_view text, uchar how_say, bool unsafe_text)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(from_id) + sizeof(how_say) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(text.length()) + sizeof(unsafe_text);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CRITTER_TEXT;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsg(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }

    const auto from_id = (from_cr != nullptr ? from_cr->GetId() : 0u);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_MSG;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsg(uint from_id, uint num_str, uchar how_say, ushort num_msg)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_MSG;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, string_view lexems)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }

    const auto from_id = (from_cr != nullptr ? from_cr->GetId() : 0u);
    const uint msg_len = NETMSG_MSG_SIZE + sizeof(msg_len) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(lexems.length());

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, string_view lexems)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }
    if (num_str == 0u) {
        return;
    }

    const uint msg_len = NETMSG_MSG_SIZE + sizeof(msg_len) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(lexems.length());

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapText(ushort hx, ushort hy, uint color, string_view text, bool unsafe_text)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(hx) + sizeof(hy) + sizeof(color) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(text.length()) + sizeof(unsafe_text);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_MAP_TEXT;
    Connection->Bout << msg_len;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_MAP_TEXT_MSG;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, string_view lexems)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(ushort) * 2 + sizeof(uint) + sizeof(ushort) + sizeof(uint) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(lexems.length());

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_MAP_TEXT_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AutomapsInfo(void* locs_vec, Location* loc)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
    }

    /*if (locs_vec)
    {
        LocationVec* locs = (LocationVec*)locs_vec;
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(bool) + sizeof(ushort);
        for (uint i = 0, j = (uint)locs->size(); i < j; i++)
        {
            Location* loc_ = (*locs)[i];
            msg_len += sizeof(uint) + sizeof(hstring::hash_t) + sizeof(ushort);
            if (loc_->IsNonEmptyAutomaps())
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                msg_len += (sizeof(hstring::hash_t) + sizeof(uchar)) * (uint)automaps->GetSize();
                automaps->Release();
            }
        }

        CONNECTION_OUTPUT_BEGIN(Connection);
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
                    hstring pid = *(hash*)automaps->At(k);
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
        CONNECTION_OUTPUT_END(Connection);
    }

    if (loc)
    {
        uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(bool) + sizeof(ushort) + sizeof(uint) + sizeof(hash) +
            sizeof(ushort);
        CScriptArray* automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr);
        if (automaps)
            msg_len += (sizeof(hash) + sizeof(uchar)) * (uint)automaps->GetSize();

        CONNECTION_OUTPUT_BEGIN(Connection);
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
        CONNECTION_OUTPUT_END(Connection);

        if (automaps)
            automaps->Release();
    }*/
}

void Player::Send_Effect(hstring eff_pid, ushort hx, ushort hy, ushort radius)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_EFFECT;
    Connection->Bout << eff_pid;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << radius;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_FlyEffect(hstring eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_FLY_EFFECT;
    Connection->Bout << eff_pid;
    Connection->Bout << from_crid;
    Connection->Bout << to_crid;
    Connection->Bout << from_hx;
    Connection->Bout << from_hy;
    Connection->Bout << to_hx;
    Connection->Bout << to_hy;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_PlaySound(uint crid_synchronize, string_view sound_name)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    const uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(crid_synchronize) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(sound_name.length());

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_PLAY_SOUND;
    Connection->Bout << msg_len;
    Connection->Bout << crid_synchronize;
    Connection->Bout << sound_name;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_ViewMap()
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    RUNTIME_ASSERT(_ownedCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_VIEW_MAP;
    Connection->Bout << _ownedCr->ViewMapHx;
    Connection->Bout << _ownedCr->ViewMapHy;
    Connection->Bout << _ownedCr->ViewMapLocId;
    Connection->Bout << _ownedCr->ViewMapLocEnt;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_SomeItem(Item* item)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) + sizeof(hstring::hash_t) + sizeof(uchar);
    vector<uchar*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto whole_data_size = item->StoreData(false, &data, &data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_SOME_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    NET_WRITE_PROPERTIES(Connection->Bout, data, data_sizes);
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_CustomMessage(uint msg)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << msg;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddAllItems()
{
    if (IsSendDisabled()) {
        return;
    }

    RUNTIME_ASSERT(_ownedCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_CLEAR_ITEMS;
    CONNECTION_OUTPUT_END(Connection);

    for (auto* item : _ownedCr->_invItems) {
        Send_AddItem(item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_ALL_ITEMS_SEND;
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AllAutomapsInfo(MapManager& map_mngr)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    RUNTIME_ASSERT(_ownedCr);

    vector<Location*> locs;
    for (const auto loc_id : _ownedCr->GetKnownLocations()) {
        auto* loc = map_mngr.GetLocation(loc_id);
        if (loc != nullptr && loc->IsNonEmptyAutomaps()) {
            locs.push_back(loc);
        }
    }

    Send_AutomapsInfo(&locs, nullptr);
}

void Player::Send_SomeItems(const vector<Item*>* items, int param)
{
    NON_CONST_METHOD_HINT();

    if (IsSendDisabled()) {
        return;
    }

    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(param) + sizeof(bool) + sizeof(uint);

    vector<vector<uchar*>*> items_data(items != nullptr ? items->size() : 0);
    vector<vector<uint>*> items_data_sizes(items != nullptr ? items->size() : 0);
    for (size_t i = 0, j = items_data.size(); i < j; i++) {
        const auto whole_data_size = items->at(i)->StoreData(false, &items_data[i], &items_data_sizes[i]);
        msg_len += sizeof(uint) + sizeof(hstring::hash_t) + sizeof(ushort) + whole_data_size;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->Bout << NETMSG_SOME_ITEMS;
    Connection->Bout << msg_len;
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
    CONNECTION_OUTPUT_END(Connection);
}
