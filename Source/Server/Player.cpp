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

#include "Player.h"
#include "Critter.h"
#include "CritterManager.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "Server.h"
#include "Settings.h"

Player::Player(FOServer* engine, ident_t id, ClientConnection* connection, const Properties* props) :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), props),
    PlayerProperties(GetInitRef()),
    Connection {connection},
    _talkNextTime {_engine->GameTime.GameplayTime() + std::chrono::milliseconds {PROCESS_TALK_TIME}}
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

void Player::SetControlledCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    _controlledCr = cr;
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

void Player::Send_AddCritter(const Critter* cr)
{
    STACK_TRACE_ENTRY();

    vector<const uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    const auto is_chosen = cr == GetControlledCritter();
    cr->StoreData(is_chosen, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_CRITTER);
    Connection->OutBuf.Write(cr->GetId());
    Connection->OutBuf.Write(cr->GetProtoId());
    Connection->OutBuf.Write(cr->GetMapHex());
    Connection->OutBuf.Write(cr->GetMapHexOffset());
    Connection->OutBuf.Write(cr->GetDirAngle());
    Connection->OutBuf.Write(cr->GetCondition());
    Connection->OutBuf.Write(cr->GetAliveStateAnim());
    Connection->OutBuf.Write(cr->GetKnockoutStateAnim());
    Connection->OutBuf.Write(cr->GetDeadStateAnim());
    Connection->OutBuf.Write(cr->GetAliveActionAnim());
    Connection->OutBuf.Write(cr->GetKnockoutActionAnim());
    Connection->OutBuf.Write(cr->GetDeadActionAnim());
    Connection->OutBuf.Write(cr->GetIsControlledByPlayer());
    Connection->OutBuf.Write(cr->GetIsControlledByPlayer() && cr->GetPlayer() == nullptr);
    Connection->OutBuf.Write(is_chosen);
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

    if (!is_chosen) {
        Send_MoveItem(cr, nullptr, CritterAction::Refresh, CritterItemSlot::Inventory);
    }

    if (cr->GetIsAttached() || !cr->AttachedCritters.empty()) {
        Send_Attachments(cr);
    }

    if (cr->IsMoving()) {
        Send_Moving(cr);
    }
}

void Player::Send_RemoveCritter(const Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_REMOVE_CRITTER);
    Connection->OutBuf.Write(cr->GetId());
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_LoadMap(const Map* map)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    const Location* loc = nullptr;
    hstring pid_map;
    hstring pid_loc;
    uint8 map_index_in_loc = 0;

    if (map == nullptr) {
        map = _engine->MapMngr.GetMap(_controlledCr->GetMapId());
    }

    if (map != nullptr) {
        loc = map->GetLocation();
        pid_map = map->GetProtoId();
        pid_loc = loc->GetProtoId();
        map_index_in_loc = static_cast<uint8>(loc->GetMapIndex(pid_map));
    }

    vector<const uint8*>* map_data = nullptr;
    vector<uint>* map_data_sizes = nullptr;
    vector<const uint8*>* loc_data = nullptr;
    vector<uint>* loc_data_sizes = nullptr;
    if (map != nullptr) {
        map->StoreData(false, &map_data, &map_data_sizes);
        loc->StoreData(false, &loc_data, &loc_data_sizes);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_LOADMAP);
    Connection->OutBuf.Write(loc != nullptr ? loc->GetId() : ident_t {});
    Connection->OutBuf.Write(map != nullptr ? map->GetId() : ident_t {});
    Connection->OutBuf.Write(pid_loc);
    Connection->OutBuf.Write(pid_map);
    Connection->OutBuf.Write(map_index_in_loc);
    if (map != nullptr) {
        NET_WRITE_PROPERTIES(Connection->OutBuf, map_data, map_data_sizes);
        NET_WRITE_PROPERTIES(Connection->OutBuf, loc_data, loc_data_sizes);
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Property(NetProperty type, const Property* prop, const Entity* entity)
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

    Connection->OutBuf.Write(type);

    switch (type) {
    case NetProperty::CritterItem:
        Connection->OutBuf.Write(dynamic_cast<const Item*>(entity)->GetCritterId());
        Connection->OutBuf.Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    case NetProperty::Critter:
        Connection->OutBuf.Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    case NetProperty::MapItem:
        Connection->OutBuf.Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    case NetProperty::ChosenItem:
        Connection->OutBuf.Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    default:
        break;
    }

    if (is_pod) {
        Connection->OutBuf.Write(prop->GetRegIndex());
        Connection->OutBuf.Push(data, data_size);
    }
    else {
        Connection->OutBuf.Write(prop->GetRegIndex());
        if (data_size != 0) {
            Connection->OutBuf.Push(data, data_size);
        }
    }

    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Moving(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!from_cr->Moving.Steps.empty()) {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->OutBuf.StartMsg(NETMSG_CRITTER_MOVE);
        Connection->OutBuf.Write(from_cr->GetId());
        Connection->OutBuf.Write(static_cast<uint>(std::ceil(from_cr->Moving.WholeTime)));
        Connection->OutBuf.Write(time_duration_to_ms<uint>(_engine->GameTime.FrameTime() - from_cr->Moving.StartTime));
        Connection->OutBuf.Write(from_cr->Moving.Speed);
        Connection->OutBuf.Write(from_cr->Moving.StartHex);
        Connection->OutBuf.Write(static_cast<uint16>(from_cr->Moving.Steps.size()));
        for (const auto step : from_cr->Moving.Steps) {
            Connection->OutBuf.Write(step);
        }
        Connection->OutBuf.Write(static_cast<uint16>(from_cr->Moving.ControlSteps.size()));
        for (const auto control_step : from_cr->Moving.ControlSteps) {
            Connection->OutBuf.Write(control_step);
        }
        Connection->OutBuf.Write(from_cr->Moving.EndHexOffset);
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
    else {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->OutBuf.StartMsg(NETMSG_CRITTER_POS);
        Connection->OutBuf.Write(from_cr->GetId());
        Connection->OutBuf.Write(from_cr->GetMapHex());
        Connection->OutBuf.Write(from_cr->GetMapHexOffset());
        Connection->OutBuf.Write(from_cr->GetDirAngle());
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
}

void Player::Send_Dir(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_DIR);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(from_cr->GetDirAngle());
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Action(const Critter* from_cr, CritterAction action, int action_data, const Item* context_item)
{
    STACK_TRACE_ENTRY();

    if (context_item != nullptr) {
        Send_SomeItem(context_item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_ACTION);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(action);
    Connection->OutBuf.Write(action_data);
    Connection->OutBuf.Write(context_item != nullptr);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MoveItem(const Critter* from_cr, const Item* item, CritterAction action, CritterItemSlot prev_slot)
{
    STACK_TRACE_ENTRY();

    const auto is_chosen = from_cr == GetControlledCritter();

    if (item != nullptr) {
        Send_SomeItem(item);
    }

    vector<const Item*> items;
    vector<vector<const uint8*>*> items_data;
    vector<vector<uint>*> items_data_sizes;

    if (!is_chosen) {
        const auto& inv_items = from_cr->GetConstRawInvItems();
        items.reserve(inv_items.size());
        for (const auto* item_ : inv_items) {
            const auto slot = item_->GetCritterSlot();
            if (static_cast<size_t>(slot) < _engine->Settings.CritterSlotEnabled.size() && _engine->Settings.CritterSlotEnabled[static_cast<size_t>(slot)] && //
                static_cast<size_t>(slot) < _engine->Settings.CritterSlotSendData.size() && _engine->Settings.CritterSlotSendData[static_cast<size_t>(slot)]) {
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
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(action);
    Connection->OutBuf.Write(prev_slot);
    Connection->OutBuf.Write(item != nullptr);
    Connection->OutBuf.Write(item != nullptr ? item->GetCritterSlot() : CritterItemSlot::Inventory);
    Connection->OutBuf.Write(static_cast<uint16>(items.size()));
    for (const auto i : xrange(items)) {
        const auto* item_ = items[i];
        Connection->OutBuf.Write(item_->GetCritterSlot());
        Connection->OutBuf.Write(item_->GetId());
        Connection->OutBuf.Write(item_->GetProtoId());
        NET_WRITE_PROPERTIES(Connection->OutBuf, items_data[i], items_data_sizes[i]);
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Animate(const Critter* from_cr, CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play)
{
    STACK_TRACE_ENTRY();

    if (context_item != nullptr) {
        Send_SomeItem(context_item);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_ANIMATE);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(state_anim);
    Connection->OutBuf.Write(action_anim);
    Connection->OutBuf.Write(context_item != nullptr);
    Connection->OutBuf.Write(clear_sequence);
    Connection->OutBuf.Write(delay_play);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_SetAnims(const Critter* from_cr, CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_SET_ANIMS);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(cond);
    Connection->OutBuf.Write(state_anim);
    Connection->OutBuf.Write(action_anim);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddItemOnMap(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<const uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    item->StoreData(false, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_ITEM_ON_MAP);
    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.Write(item->GetProtoId());
    Connection->OutBuf.Write(item->GetMapHex());
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItemFromMap(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ERASE_ITEM_FROM_MAP);
    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AnimateItem(const Item* item, hstring anim_name, bool looped, bool reversed)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ANIMATE_ITEM);
    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.Write(anim_name);
    Connection->OutBuf.Write(looped);
    Connection->OutBuf.Write(reversed);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AddItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (item->GetIsHidden()) {
        return;
    }

    vector<const uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    item->StoreData(true, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_ITEM);
    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.Write(item->GetProtoId());
    Connection->OutBuf.Write(item->GetCritterSlot());
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_EraseItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_REMOVE_ITEM);
    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_GlobalInfo(uint8 info_flags)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    const auto known_locs = _controlledCr->GetKnownLocations();

    auto loc_count = static_cast<uint16>(known_locs.size());

    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->OutBuf.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->OutBuf.Write(info_flags);

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        Connection->OutBuf.Write(loc_count);

        for (uint16 i = 0; i < loc_count;) {
            const auto loc_id = known_locs[i];
            const auto* loc = _engine->MapMngr.GetLocation(loc_id);
            if (loc != nullptr && !loc->GetToGarbage()) {
                i++;
                Connection->OutBuf.Write(loc_id);
                Connection->OutBuf.Write(loc->GetProtoId());
                Connection->OutBuf.Write(loc->GetWorldPos());
                Connection->OutBuf.Write(loc->GetRadius());
                Connection->OutBuf.Write(loc->GetColor());
                uint8 count = 0;
                if (loc->IsNonEmptyMapEntrances()) {
                    count = static_cast<uint8>(loc->GetMapEntrances().size() / 2);
                }
                Connection->OutBuf.Write(count);
            }
            else {
                loc_count--;
                _engine->MapMngr.EraseKnownLoc(_controlledCr, loc_id);

                constexpr size_t send_location_size = sizeof(uint) + sizeof(hstring::hash_t) + sizeof(uint16) * 2 + sizeof(uint16) + sizeof(uint) + sizeof(uint8);
                constexpr char empty_loc[send_location_size] = {0};
                Connection->OutBuf.Push(empty_loc, sizeof(empty_loc));
            }
        }
    }

    if (IsBitSet(info_flags, GM_INFO_ZONES_FOG)) {
        auto gmap_fog = _controlledCr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }
        Connection->OutBuf.Push(gmap_fog.data(), GM_ZONES_FOG_SIZE);
    }

    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

    if (IsBitSet(info_flags, GM_INFO_CRITTERS)) {
        _engine->MapMngr.ProcessVisibleCritters(_controlledCr);
    }
}

void Player::Send_GlobalLocation(const Location* loc, bool add)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    constexpr auto info_flags = GM_INFO_LOCATION;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->OutBuf.Write(info_flags);
    Connection->OutBuf.Write(add);
    Connection->OutBuf.Write(loc->GetId());
    Connection->OutBuf.Write(loc->GetProtoId());
    Connection->OutBuf.Write(loc->GetWorldPos());
    Connection->OutBuf.Write(loc->GetRadius());
    Connection->OutBuf.Write(loc->GetColor());
    uint8 count = 0;
    if (loc->IsNonEmptyMapEntrances()) {
        count = static_cast<uint8>(loc->GetMapEntrances().size() / 2);
    }
    Connection->OutBuf.Write(count);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_GlobalMapFog(uint16 zx, uint16 zy, uint8 fog)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    constexpr auto info_flags = GM_INFO_FOG;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->OutBuf.Write(info_flags);
    Connection->OutBuf.Write(zx);
    Connection->OutBuf.Write(zy);
    Connection->OutBuf.Write(fog);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AllProperties()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<const uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    StoreData(true, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ALL_PROPERTIES);
    NET_WRITE_PROPERTIES(Connection->OutBuf, data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Teleport(const Critter* cr, mpos to_hex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_TELEPORT);
    Connection->OutBuf.Write(cr->GetId());
    Connection->OutBuf.Write(to_hex);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Talk()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    const auto close = _controlledCr->Talk.Type == TalkType::None;
    const auto is_npc = _controlledCr->Talk.Type == TalkType::Critter;

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_TALK_NPC);

    if (close) {
        Connection->OutBuf.Write(is_npc);
        Connection->OutBuf.Write(_controlledCr->Talk.CritterId);
        Connection->OutBuf.Write(_controlledCr->Talk.DialogPackId);
        Connection->OutBuf.Write(static_cast<uint8>(0));
    }
    else {
        const auto all_answers = static_cast<uint8>(_controlledCr->Talk.CurDialog.Answers.size());

        auto talk_time = _controlledCr->Talk.TalkTime;
        if (talk_time != time_duration {}) {
            const auto diff = _engine->GameTime.GameplayTime() - _controlledCr->Talk.StartTime;
            talk_time = diff < talk_time ? talk_time - diff : std::chrono::milliseconds {1};
        }

        Connection->OutBuf.Write(is_npc);
        Connection->OutBuf.Write(_controlledCr->Talk.CritterId);
        Connection->OutBuf.Write(_controlledCr->Talk.DialogPackId);
        Connection->OutBuf.Write(all_answers);
        Connection->OutBuf.Write(static_cast<uint16>(_controlledCr->Talk.Lexems.length()));
        if (_controlledCr->Talk.Lexems.length() != 0) {
            Connection->OutBuf.Push(_controlledCr->Talk.Lexems.c_str(), _controlledCr->Talk.Lexems.length());
        }
        Connection->OutBuf.Write(_controlledCr->Talk.CurDialog.TextId);
        for (const auto& answer : _controlledCr->Talk.CurDialog.Answers) {
            Connection->OutBuf.Write(answer.TextId);
        }
        Connection->OutBuf.Write(time_duration_to_ms<uint>(talk_time));
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
    Connection->OutBuf.Write(_engine->GetYear());
    Connection->OutBuf.Write(_engine->GetMonth());
    Connection->OutBuf.Write(_engine->GetDay());
    Connection->OutBuf.Write(_engine->GetHour());
    Connection->OutBuf.Write(_engine->GetMinute());
    Connection->OutBuf.Write(_engine->GetSecond());
    Connection->OutBuf.Write(_engine->GetTimeMultiplier());
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Text(const Critter* from_cr, string_view text, uint8 how_say)
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
    Connection->OutBuf.Write(from_id);
    Connection->OutBuf.Write(how_say);
    Connection->OutBuf.Write(text);
    Connection->OutBuf.Write(unsafe_text);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsg(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG);
    Connection->OutBuf.Write(from_id);
    Connection->OutBuf.Write(how_say);
    Connection->OutBuf.Write(text_pack);
    Connection->OutBuf.Write(str_num);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsg(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG);
    Connection->OutBuf.Write(from_id);
    Connection->OutBuf.Write(how_say);
    Connection->OutBuf.Write(text_pack);
    Connection->OutBuf.Write(str_num);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsgLex(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG_LEX);
    Connection->OutBuf.Write(from_id);
    Connection->OutBuf.Write(how_say);
    Connection->OutBuf.Write(text_pack);
    Connection->OutBuf.Write(str_num);
    Connection->OutBuf.Write(lexems);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_TextMsgLex(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MSG_LEX);
    Connection->OutBuf.Write(from_id);
    Connection->OutBuf.Write(how_say);
    Connection->OutBuf.Write(text_pack);
    Connection->OutBuf.Write(str_num);
    Connection->OutBuf.Write(lexems);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapText(mpos hex, ucolor color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT);
    Connection->OutBuf.Write(hex);
    Connection->OutBuf.Write(color);
    Connection->OutBuf.Write(text);
    Connection->OutBuf.Write(unsafe_text);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsg(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT_MSG);
    Connection->OutBuf.Write(hex);
    Connection->OutBuf.Write(color);
    Connection->OutBuf.Write(text_pack);
    Connection->OutBuf.Write(str_num);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsgLex(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT_MSG_LEX);
    Connection->OutBuf.Write(hex);
    Connection->OutBuf.Write(color);
    Connection->OutBuf.Write(text_pack);
    Connection->OutBuf.Write(str_num);
    Connection->OutBuf.Write(lexems);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_AutomapsInfo(const void* locs_vec, const Location* loc)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // Todo: restore automaps
    UNUSED_VARIABLE(locs_vec);
    UNUSED_VARIABLE(loc);
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
        Connection->OutBuf.Write((bool)true; // Clear list
        Connection->OutBuf.Write((uint16)locs->size();
        for (uint i = 0, j = (uint)locs->size(); i < j; i++)
        {
            Location* loc_ = (*locs)[i];
            Connection->OutBuf.Write(loc_->GetId();
            Connection->OutBuf.Write(loc_->GetProtoId();
            if (loc_->IsNonEmptyAutomaps())
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                Connection->OutBuf.Write((uint16)automaps->GetSize();
                for (uint k = 0, l = (uint)automaps->GetSize(); k < l; k++)
                {
                    hstring pid = *(hash*)automaps->At(k);
                    Connection->OutBuf.Write(pid;
                    Connection->OutBuf.Write((uint8)loc_->GetMapIndex(pid);
                }
                automaps->Release();
            }
            else
            {
                Connection->OutBuf.Write((uint16)0;
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
        Connection->OutBuf.Write((bool)false; // Append this information
        Connection->OutBuf.Write((uint16)1;
        Connection->OutBuf.Write(loc->GetId();
        Connection->OutBuf.Write(loc->GetProtoId();
        Connection->OutBuf.Write((uint16)(automaps ? automaps->GetSize() : 0);
        if (automaps)
        {
            for (uint i = 0, j = (uint)automaps->GetSize(); i < j; i++)
            {
                hash pid = *(hash*)automaps->At(i);
                Connection->OutBuf.Write(pid;
                Connection->OutBuf.Write((uint8)loc->GetMapIndex(pid);
            }
        }
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);

        if (automaps)
            automaps->Release();
    }*/
}

void Player::Send_Effect(hstring eff_pid, mpos hex, uint16 radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_EFFECT);
    Connection->OutBuf.Write(eff_pid);
    Connection->OutBuf.Write(hex);
    Connection->OutBuf.Write(radius);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, mpos from_hex, mpos to_hex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_FLY_EFFECT);
    Connection->OutBuf.Write(eff_pid);
    Connection->OutBuf.Write(from_cr_id);
    Connection->OutBuf.Write(to_cr_id);
    Connection->OutBuf.Write(from_hex);
    Connection->OutBuf.Write(to_hex);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_PLAY_SOUND);
    Connection->OutBuf.Write(cr_id_synchronize);
    Connection->OutBuf.Write(sound_name);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_ViewMap()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_VIEW_MAP);
    Connection->OutBuf.Write(_controlledCr->ViewMapHex);
    Connection->OutBuf.Write(_controlledCr->ViewMapLocId);
    Connection->OutBuf.Write(_controlledCr->ViewMapLocEnt);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_SomeItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<const uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;
    item->StoreData(false, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_SOME_ITEM);
    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.Write(item->GetProtoId());
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

    RUNTIME_ASSERT(_controlledCr);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CLEAR_ITEMS);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

    for (const auto* item : _controlledCr->_invItems) {
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

    RUNTIME_ASSERT(_controlledCr);

    vector<Location*> locs;
    for (const auto loc_id : _controlledCr->GetKnownLocations()) {
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

    vector<vector<const uint8*>*> items_data(items != nullptr ? items->size() : 0);
    vector<vector<uint>*> items_data_sizes(items != nullptr ? items->size() : 0);
    for (size_t i = 0; i < items_data.size(); i++) {
        items->at(i)->StoreData(false, &items_data[i], &items_data_sizes[i]);
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_SOME_ITEMS);
    Connection->OutBuf.Write(param);
    Connection->OutBuf.Write(items == nullptr);
    Connection->OutBuf.Write(static_cast<uint>(items != nullptr ? items->size() : 0));
    if (items != nullptr) {
        for (size_t i = 0; i < items_data.size(); i++) {
            const auto* item = items->at(i);
            Connection->OutBuf.Write(item->GetId());
            Connection->OutBuf.Write(item->GetProtoId());
            NET_WRITE_PROPERTIES(Connection->OutBuf, items_data[i], items_data_sizes[i]);
        }
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Attachments(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_ATTACHMENTS);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(from_cr->GetIsAttached());
    Connection->OutBuf.Write(from_cr->GetAttachMaster());
    Connection->OutBuf.Write(static_cast<uint16>(from_cr->AttachedCritters.size()));
    for (const auto* attached_cr : from_cr->AttachedCritters) {
        Connection->OutBuf.Write(attached_cr->GetId());
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}
