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
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props),
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

auto Player::GetIp() const noexcept -> uint
{
    STACK_TRACE_ENTRY();

    return Connection->GetIp();
}

auto Player::GetHost() const noexcept -> string_view
{
    STACK_TRACE_ENTRY();

    return Connection->GetHost();
}

auto Player::GetPort() const noexcept -> uint16
{
    STACK_TRACE_ENTRY();

    return Connection->GetPort();
}

void Player::Send_LoginSuccess()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto encrypt_key = NetBuffer::GenerateEncryptKey();

    vector<const uint8*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    _engine->StoreData(false, &global_vars_data, &global_vars_data_sizes);

    vector<const uint8*>* player_data = nullptr;
    vector<uint>* player_data_sizes = nullptr;
    StoreData(true, &player_data, &player_data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_LOGIN_SUCCESS);
    Connection->OutBuf.Write(encrypt_key);
    Connection->OutBuf.Write(GetId());
    Connection->OutBuf.WritePropsData(global_vars_data, global_vars_data_sizes);
    Connection->OutBuf.WritePropsData(player_data, player_data_sizes);
    SendInnerEntities(_engine, false);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

    Connection->OutBuf.SetEncryptKey(encrypt_key);
}

void Player::Send_AddCritter(const Critter* cr)
{
    STACK_TRACE_ENTRY();

    const auto is_chosen = cr == GetControlledCritter();

    vector<const uint8*>* cr_data = nullptr;
    vector<uint>* cr_data_sizes = nullptr;
    cr->StoreData(is_chosen, &cr_data, &cr_data_sizes);

    const auto& inv_items = cr->GetConstInvItems();
    vector<const Item*> send_items;
    send_items.reserve(inv_items.size());

    for (const auto* item : inv_items) {
        if (item->CanSendItem(!is_chosen)) {
            send_items.emplace_back(item);
        }
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_CRITTER);

    Connection->OutBuf.Write(cr->GetId());
    Connection->OutBuf.Write(cr->GetProtoId());
    Connection->OutBuf.Write(cr->GetHexX());
    Connection->OutBuf.Write(cr->GetHexY());
    Connection->OutBuf.Write(cr->GetHexOffsX());
    Connection->OutBuf.Write(cr->GetHexOffsY());
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
    Connection->OutBuf.WritePropsData(cr_data, cr_data_sizes);

    SendInnerEntities(cr, is_chosen);

    Connection->OutBuf.Write(static_cast<uint>(send_items.size()));
    for (const auto* item : send_items) {
        SendItem(item, is_chosen, true, true);
    }

    Connection->OutBuf.Write(cr->GetIsAttached());
    Connection->OutBuf.Write(static_cast<uint16>(cr->AttachedCritters.size()));
    if (!cr->AttachedCritters.empty()) {
        for (const auto* attached_cr : cr->AttachedCritters) {
            Connection->OutBuf.Write(attached_cr->GetId());
        }
    }

    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);

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

    const Location* loc = nullptr;
    hstring pid_map;
    hstring pid_loc;
    uint8 map_index_in_loc = 0;

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
    Connection->OutBuf.StartMsg(NETMSG_LOAD_MAP);
    Connection->OutBuf.Write(loc != nullptr ? loc->GetId() : ident_t {});
    Connection->OutBuf.Write(map != nullptr ? map->GetId() : ident_t {});
    Connection->OutBuf.Write(pid_loc);
    Connection->OutBuf.Write(pid_map);
    Connection->OutBuf.Write(map_index_in_loc);
    if (map != nullptr) {
        Connection->OutBuf.WritePropsData(map_data, map_data_sizes);
        Connection->OutBuf.WritePropsData(loc_data, loc_data_sizes);
        SendInnerEntities(loc, false);
        SendInnerEntities(map, false);
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
    case NetProperty::CustomEntity:
        additional_args = 1;
        break;
    default:
        break;
    }

    const auto& props = entity->GetProperties();

    props.ValidateForRawData(prop);

    const auto prop_raw_data = props.GetRawData(prop);
    const auto is_pod = prop->IsPlainData();

    CONNECTION_OUTPUT_BEGIN(Connection);

    if (is_pod) {
        Connection->OutBuf.StartMsg(NETMSG_POD_PROPERTY(prop_raw_data.size(), additional_args));
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
    case NetProperty::CustomEntity:
        Connection->OutBuf.Write(dynamic_cast<const CustomEntity*>(entity)->GetId());
        break;
    default:
        break;
    }

    Connection->OutBuf.Write(prop->GetRegIndex());
    Connection->OutBuf.Push(prop_raw_data);

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
        Connection->OutBuf.Write(time_duration_to_ms<uint>(_engine->GameTime.FrameTime() - from_cr->Moving.StartTime + from_cr->Moving.OffsetTime));
        Connection->OutBuf.Write(from_cr->Moving.Speed);
        Connection->OutBuf.Write(from_cr->Moving.StartHexX);
        Connection->OutBuf.Write(from_cr->Moving.StartHexY);
        Connection->OutBuf.Write(static_cast<uint16>(from_cr->Moving.Steps.size()));
        for (const auto step : from_cr->Moving.Steps) {
            Connection->OutBuf.Write(step);
        }
        Connection->OutBuf.Write(static_cast<uint16>(from_cr->Moving.ControlSteps.size()));
        for (const auto control_step : from_cr->Moving.ControlSteps) {
            Connection->OutBuf.Write(control_step);
        }
        Connection->OutBuf.Write(from_cr->Moving.EndOx);
        Connection->OutBuf.Write(from_cr->Moving.EndOy);
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
    else {
        CONNECTION_OUTPUT_BEGIN(Connection);
        Connection->OutBuf.StartMsg(NETMSG_CRITTER_POS);
        Connection->OutBuf.Write(from_cr->GetId());
        Connection->OutBuf.Write(from_cr->GetHexX());
        Connection->OutBuf.Write(from_cr->GetHexY());
        Connection->OutBuf.Write(from_cr->GetHexOffsX());
        Connection->OutBuf.Write(from_cr->GetHexOffsY());
        Connection->OutBuf.Write(from_cr->GetDirAngle());
        Connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(Connection);
    }
}

void Player::Send_MovingSpeed(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_MOVE_SPEED);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(from_cr->Moving.Speed);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
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

    NON_CONST_METHOD_HINT();

    const auto is_chosen = from_cr == GetControlledCritter();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_ACTION);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(action);
    Connection->OutBuf.Write(action_data);
    Connection->OutBuf.Write(context_item != nullptr);
    if (context_item != nullptr) {
        SendItem(context_item, is_chosen, false, false);
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MoveItem(const Critter* from_cr, const Item* moved_item, CritterAction action, CritterItemSlot prev_slot)
{
    STACK_TRACE_ENTRY();

    const auto is_chosen = from_cr == GetControlledCritter();

    vector<const Item*> send_items;

    if (!is_chosen) {
        const auto& inv_items = from_cr->GetConstInvItems();
        send_items.reserve(inv_items.size());

        for (const auto* item : inv_items) {
            if (item->CanSendItem(true)) {
                send_items.emplace_back(item);
            }
        }
    }

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_MOVE_ITEM);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(action);
    Connection->OutBuf.Write(prev_slot);
    Connection->OutBuf.Write(moved_item != nullptr ? moved_item->GetCritterSlot() : CritterItemSlot::Inventory);
    Connection->OutBuf.Write(moved_item != nullptr);
    if (moved_item != nullptr) {
        SendItem(moved_item, is_chosen, false, false);
    }
    Connection->OutBuf.Write(static_cast<uint>(send_items.size()));
    for (const auto* item : send_items) {
        SendItem(item, false, true, true);
    }
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_Animate(const Critter* from_cr, CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto is_chosen = from_cr == GetControlledCritter();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_ANIMATE);
    Connection->OutBuf.Write(from_cr->GetId());
    Connection->OutBuf.Write(state_anim);
    Connection->OutBuf.Write(action_anim);
    Connection->OutBuf.Write(clear_sequence);
    Connection->OutBuf.Write(delay_play);
    Connection->OutBuf.Write(context_item != nullptr);
    if (context_item != nullptr) {
        SendItem(context_item, is_chosen, false, false);
    }
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

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_ITEM_ON_MAP);
    Connection->OutBuf.Write(item->GetHexX());
    Connection->OutBuf.Write(item->GetHexY());
    SendItem(item, false, false, true);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_RemoveItemFromMap(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_REMOVE_ITEM_FROM_MAP);
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

void Player::Send_ChosenAddItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CHOSEN_ADD_ITEM);
    SendItem(item, true, true, true);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_ChosenRemoveItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CHOSEN_REMOVE_ITEM);
    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_GlobalInfo()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    constexpr uint8 info_flags = GM_INFO_LOCATIONS | GM_INFO_ZONES_FOG;

    const auto known_locs = _controlledCr->GetKnownLocations();

    auto loc_count = static_cast<uint16>(known_locs.size());

    CONNECTION_OUTPUT_BEGIN(Connection);

    Connection->OutBuf.StartMsg(NETMSG_GLOBAL_INFO);
    Connection->OutBuf.Write(info_flags);

    if (IsBitSet(info_flags, GM_INFO_LOCATIONS)) {
        Connection->OutBuf.Write(loc_count);

        for (uint16 i = 0; i < loc_count;) {
            const auto loc_id = known_locs[i];
            const auto* loc = _engine->EntityMngr.GetLocation(loc_id);
            if (loc != nullptr && !loc->GetToGarbage()) {
                i++;
                Connection->OutBuf.Write(loc_id);
                Connection->OutBuf.Write(loc->GetProtoId());
                Connection->OutBuf.Write(loc->GetWorldX());
                Connection->OutBuf.Write(loc->GetWorldY());
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
                _engine->MapMngr.RemoveKnownLoc(_controlledCr, loc_id);

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
    Connection->OutBuf.Write(loc->GetWorldX());
    Connection->OutBuf.Write(loc->GetWorldY());
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

void Player::Send_Teleport(const Critter* cr, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_CRITTER_TELEPORT);
    Connection->OutBuf.Write(cr->GetId());
    Connection->OutBuf.Write(to_hx);
    Connection->OutBuf.Write(to_hy);
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

void Player::Send_MapText(uint16 hx, uint16 hy, ucolor color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT);
    Connection->OutBuf.Write(hx);
    Connection->OutBuf.Write(hy);
    Connection->OutBuf.Write(color);
    Connection->OutBuf.Write(text);
    Connection->OutBuf.Write(unsafe_text);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsg(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT_MSG);
    Connection->OutBuf.Write(hx);
    Connection->OutBuf.Write(hy);
    Connection->OutBuf.Write(color);
    Connection->OutBuf.Write(text_pack);
    Connection->OutBuf.Write(str_num);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_MapTextMsgLex(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_MAP_TEXT_MSG_LEX);
    Connection->OutBuf.Write(hx);
    Connection->OutBuf.Write(hy);
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

void Player::Send_Effect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_EFFECT);
    Connection->OutBuf.Write(eff_pid);
    Connection->OutBuf.Write(hx);
    Connection->OutBuf.Write(hy);
    Connection->OutBuf.Write(radius);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_FLY_EFFECT);
    Connection->OutBuf.Write(eff_pid);
    Connection->OutBuf.Write(from_cr_id);
    Connection->OutBuf.Write(to_cr_id);
    Connection->OutBuf.Write(from_hx);
    Connection->OutBuf.Write(from_hy);
    Connection->OutBuf.Write(to_hx);
    Connection->OutBuf.Write(to_hy);
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
    Connection->OutBuf.Write(_controlledCr->ViewMapHx);
    Connection->OutBuf.Write(_controlledCr->ViewMapHy);
    Connection->OutBuf.Write(_controlledCr->ViewMapLocId);
    Connection->OutBuf.Write(_controlledCr->ViewMapLocEnt);
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

void Player::Send_AllAutomapsInfo()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    vector<Location*> locs;

    for (const auto loc_id : _controlledCr->GetKnownLocations()) {
        auto* loc = _engine->EntityMngr.GetLocation(loc_id);
        if (loc != nullptr && loc->IsNonEmptyAutomaps()) {
            locs.push_back(loc);
        }
    }

    Send_AutomapsInfo(&locs, nullptr);
}

void Player::Send_SomeItems(const vector<Item*>& items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_SOME_ITEMS);
    Connection->OutBuf.Write(string(context_param));
    Connection->OutBuf.Write(static_cast<uint>(items.size()));
    for (const auto* item : items) {
        SendItem(item, owned, false, with_inner_entities);
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

void Player::Send_AddCustomEntity(CustomEntity* entity, bool owned)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<const uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;

    entity->StoreData(owned, &data, &data_sizes);

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_ADD_CUSTOM_ENTITY);
    Connection->OutBuf.Write(entity->GetCustomHolderId());
    Connection->OutBuf.Write(entity->GetCustomHolderEntry());
    Connection->OutBuf.Write(entity->GetId());
    if (const auto* entity_with_proto = dynamic_cast<CustomEntityWithProto*>(entity); entity_with_proto != nullptr) {
        RUNTIME_ASSERT(_engine->GetEntityTypeInfo(entity->GetTypeName()).HasProtos);
        Connection->OutBuf.Write(entity_with_proto->GetProtoId());
    }
    else {
        RUNTIME_ASSERT(!_engine->GetEntityTypeInfo(entity->GetTypeName()).HasProtos);
        Connection->OutBuf.Write(hstring());
    }
    Connection->OutBuf.WritePropsData(data, data_sizes);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::Send_RemoveCustomEntity(ident_t id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    CONNECTION_OUTPUT_BEGIN(Connection);
    Connection->OutBuf.StartMsg(NETMSG_REMOVE_CUSTOM_ENTITY);
    Connection->OutBuf.Write(id);
    Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(Connection);
}

void Player::SendItem(const Item* item, bool owned, bool with_slot, bool with_inner_entities)
{
    STACK_TRACE_ENTRY();

    Connection->OutBuf.Write(item->GetId());
    Connection->OutBuf.Write(item->GetProtoId());

    if (with_slot) {
        Connection->OutBuf.Write(item->GetCritterSlot());
    }

    vector<const uint8*>* item_data = nullptr;
    vector<uint>* item_data_sizes = nullptr;
    item->StoreData(owned, &item_data, &item_data_sizes);
    Connection->OutBuf.WritePropsData(item_data, item_data_sizes);

    if (with_inner_entities) {
        SendInnerEntities(item, false);
    }
    else {
        Connection->OutBuf.Write(static_cast<uint16>(0));
    }
}

void Player::SendInnerEntities(const Entity* holder, bool owned)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!holder->HasInnerEntities()) {
        Connection->OutBuf.Write(static_cast<uint16>(0));
        return;
    }

    const auto& entries_entities = holder->GetRawInnerEntities();

    Connection->OutBuf.Write(static_cast<uint16>(entries_entities.size()));

    for (auto&& [entry, entities] : entries_entities) {
        Connection->OutBuf.Write(entry);

        const auto entry_access = std::get<1>(_engine->GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.at(entry));

        if (entry_access == EntityHolderEntryAccess::Private || (entry_access == EntityHolderEntryAccess::Protected && !owned)) {
            Connection->OutBuf.Write(static_cast<uint>(0));
            continue;
        }

        Connection->OutBuf.Write(static_cast<uint>(entities.size()));

        for (const auto* entity : entities) {
            const auto* custom_entity = dynamic_cast<const CustomEntity*>(entity);
            RUNTIME_ASSERT(custom_entity);

            vector<const uint8*>* data = nullptr;
            vector<uint>* data_sizes = nullptr;

            custom_entity->StoreData(owned, &data, &data_sizes);

            Connection->OutBuf.Write(custom_entity->GetId());

            if (const auto* custom_entity_with_proto = dynamic_cast<const CustomEntityWithProto*>(custom_entity); custom_entity_with_proto != nullptr) {
                Connection->OutBuf.Write(custom_entity_with_proto->GetProtoId());
            }
            else {
                Connection->OutBuf.Write(hstring());
            }

            Connection->OutBuf.WritePropsData(data, data_sizes);

            SendInnerEntities(entity, owned);
        }
    }
}
