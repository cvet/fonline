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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

Player::Player(FOServer* engine, ident_t id, unique_ptr<ServerConnection> connection, const Properties* props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props),
    PlayerProperties(GetInitRef()),
    _connection {std::move(connection)},
    _talkNextTime {_engine->GameTime.GameplayTime() + std::chrono::milliseconds {PROCESS_TALK_TIME}}
{
    STACK_TRACE_ENTRY();
}

Player::~Player()
{
    STACK_TRACE_ENTRY();
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

void Player::SwapConnection(Player* other) noexcept
{
    STACK_TRACE_ENTRY();

    STRONG_ASSERT(other);
    STRONG_ASSERT(other != this);

    std::swap(_connection, other->_connection);
}

void Player::Send_LoginSuccess()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<const uint8*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    _engine->StoreData(false, &global_vars_data, &global_vars_data_sizes);

    vector<const uint8*>* player_data = nullptr;
    vector<uint>* player_data_sizes = nullptr;
    StoreData(true, &player_data, &player_data_sizes);

    auto out_buf = _connection->WriteMsg(NetMessage::LoginSuccess);

    out_buf->Write(GetId());
    out_buf->WritePropsData(global_vars_data, global_vars_data_sizes);
    out_buf->WritePropsData(player_data, player_data_sizes);
    SendInnerEntities(*out_buf, _engine, false);
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

    auto out_buf = _connection->WriteMsg(NetMessage::AddCritter);

    out_buf->Write(cr->GetId());
    out_buf->Write(cr->GetProtoId());
    out_buf->Write(cr->GetHex());
    out_buf->Write(cr->GetHexOffset());
    out_buf->Write(cr->GetDirAngle());
    out_buf->Write(cr->GetCondition());
    out_buf->Write(cr->GetAliveStateAnim());
    out_buf->Write(cr->GetKnockoutStateAnim());
    out_buf->Write(cr->GetDeadStateAnim());
    out_buf->Write(cr->GetAliveActionAnim());
    out_buf->Write(cr->GetKnockoutActionAnim());
    out_buf->Write(cr->GetDeadActionAnim());
    out_buf->Write(cr->GetControlledByPlayer());
    out_buf->Write(cr->GetControlledByPlayer() && cr->GetPlayer() == nullptr);
    out_buf->Write(is_chosen);
    out_buf->WritePropsData(cr_data, cr_data_sizes);

    SendInnerEntities(*out_buf, cr, is_chosen);

    out_buf->Write(static_cast<uint>(send_items.size()));

    for (const auto* item : send_items) {
        SendItem(*out_buf, item, is_chosen, true, true);
    }

    out_buf->Write(cr->GetIsAttached());
    out_buf->Write(static_cast<uint16>(cr->AttachedCritters.size()));

    if (!cr->AttachedCritters.empty()) {
        for (const auto* attached_cr : cr->AttachedCritters) {
            out_buf->Write(attached_cr->GetId());
        }
    }

    if (cr->IsMoving()) {
        out_buf->Write(true);
        SendCritterMoving(*out_buf, cr);
    }
    else {
        out_buf->Write(false);
    }
}

void Player::Send_RemoveCritter(const Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::RemoveCritter);

    out_buf->Write(cr->GetId());
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

    auto out_buf = _connection->WriteMsg(NetMessage::LoadMap);

    out_buf->Write(loc != nullptr ? loc->GetId() : ident_t {});
    out_buf->Write(map != nullptr ? map->GetId() : ident_t {});
    out_buf->Write(pid_loc);
    out_buf->Write(pid_map);
    out_buf->Write(map_index_in_loc);

    if (map != nullptr) {
        out_buf->WritePropsData(map_data, map_data_sizes);
        out_buf->WritePropsData(loc_data, loc_data_sizes);
        SendInnerEntities(*out_buf, loc, false);
        SendInnerEntities(*out_buf, map, false);
    }
}

void Player::Send_Property(NetProperty type, const Property* prop, const Entity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(entity);
    RUNTIME_ASSERT(prop);

    if (SendIgnoreEntity == entity && SendIgnoreProperty == prop) {
        return;
    }

    const auto& props = entity->GetProperties();
    props.ValidateForRawData(prop);
    const auto prop_raw_data = props.GetRawData(prop);

    auto out_buf = _connection->WriteMsg(NetMessage::Property);

    out_buf->Write(static_cast<uint>(prop_raw_data.size()));
    out_buf->Write(type);

    switch (type) {
    case NetProperty::CritterItem:
        out_buf->Write(dynamic_cast<const Item*>(entity)->GetCritterId());
        out_buf->Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    case NetProperty::Critter:
        out_buf->Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    case NetProperty::MapItem:
        out_buf->Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    case NetProperty::ChosenItem:
        out_buf->Write(dynamic_cast<const ServerEntity*>(entity)->GetId());
        break;
    case NetProperty::CustomEntity:
        out_buf->Write(dynamic_cast<const CustomEntity*>(entity)->GetId());
        break;
    default:
        break;
    }

    out_buf->Write(prop->GetRegIndex());
    out_buf->Push(prop_raw_data);
}

void Player::Send_Moving(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!from_cr->Moving.Steps.empty()) {
        auto out_buf = _connection->WriteMsg(NetMessage::CritterMove);

        out_buf->Write(from_cr->GetId());
        SendCritterMoving(*out_buf, from_cr);
    }
    else {
        auto out_buf = _connection->WriteMsg(NetMessage::CritterPos);

        out_buf->Write(from_cr->GetId());
        out_buf->Write(from_cr->GetHex());
        out_buf->Write(from_cr->GetHexOffset());
        out_buf->Write(from_cr->GetDirAngle());
    }
}

void Player::Send_MovingSpeed(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::CritterMoveSpeed);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(from_cr->Moving.Speed);
}

void Player::Send_Dir(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::CritterDir);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(from_cr->GetDirAngle());
}

void Player::Send_Action(const Critter* from_cr, CritterAction action, int action_data, const Item* context_item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto is_chosen = from_cr == GetControlledCritter();

    auto out_buf = _connection->WriteMsg(NetMessage::CritterAction);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(action);
    out_buf->Write(action_data);
    out_buf->Write(context_item != nullptr);

    if (context_item != nullptr) {
        SendItem(*out_buf, context_item, is_chosen, false, false);
    }
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

    auto out_buf = _connection->WriteMsg(NetMessage::CritterMoveItem);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(action);
    out_buf->Write(prev_slot);
    out_buf->Write(moved_item != nullptr ? moved_item->GetCritterSlot() : CritterItemSlot::Inventory);
    out_buf->Write(moved_item != nullptr);

    if (moved_item != nullptr) {
        SendItem(*out_buf, moved_item, is_chosen, false, false);
    }

    out_buf->Write(static_cast<uint>(send_items.size()));

    for (const auto* item : send_items) {
        SendItem(*out_buf, item, false, true, true);
    }
}

void Player::Send_Animate(const Critter* from_cr, CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto is_chosen = from_cr == GetControlledCritter();

    auto out_buf = _connection->WriteMsg(NetMessage::CritterAnimate);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(state_anim);
    out_buf->Write(action_anim);
    out_buf->Write(clear_sequence);
    out_buf->Write(delay_play);
    out_buf->Write(context_item != nullptr);

    if (context_item != nullptr) {
        SendItem(*out_buf, context_item, is_chosen, false, false);
    }
}

void Player::Send_SetAnims(const Critter* from_cr, CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::CritterSetAnims);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(cond);
    out_buf->Write(state_anim);
    out_buf->Write(action_anim);
}

void Player::Send_AddItemOnMap(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::AddItemOnMap);

    out_buf->Write(item->GetHex());
    SendItem(*out_buf, item, false, false, true);
}

void Player::Send_RemoveItemFromMap(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::RemoveItemFromMap);

    out_buf->Write(item->GetId());
}

void Player::Send_AnimateItem(const Item* item, hstring anim_name, bool looped, bool reversed)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::AnimateItem);

    out_buf->Write(item->GetId());
    out_buf->Write(anim_name);
    out_buf->Write(looped);
    out_buf->Write(reversed);
}

void Player::Send_ChosenAddItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::ChosenAddItem);

    SendItem(*out_buf, item, true, true, true);
}

void Player::Send_ChosenRemoveItem(const Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::ChosenRemoveItem);

    out_buf->Write(item->GetId());
}

void Player::Send_Teleport(const Critter* cr, mpos to_hex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::CritterTeleport);

    out_buf->Write(cr->GetId());
    out_buf->Write(to_hex);
}

void Player::Send_Talk()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    const auto close = _controlledCr->Talk.Type == TalkType::None;
    const auto is_npc = _controlledCr->Talk.Type == TalkType::Critter;

    auto out_buf = _connection->WriteMsg(NetMessage::TalkNpc);

    if (close) {
        out_buf->Write(is_npc);
        out_buf->Write(_controlledCr->Talk.CritterId);
        out_buf->Write(_controlledCr->Talk.DialogPackId);
        out_buf->Write(static_cast<uint8>(0));
    }
    else {
        const auto all_answers = static_cast<uint8>(_controlledCr->Talk.CurDialog.Answers.size());

        auto talk_time = _controlledCr->Talk.TalkTime;
        if (talk_time != time_duration {}) {
            const auto diff = _engine->GameTime.GameplayTime() - _controlledCr->Talk.StartTime;
            talk_time = diff < talk_time ? talk_time - diff : std::chrono::milliseconds {1};
        }

        out_buf->Write(is_npc);
        out_buf->Write(_controlledCr->Talk.CritterId);
        out_buf->Write(_controlledCr->Talk.DialogPackId);
        out_buf->Write(all_answers);
        out_buf->Write(static_cast<uint16>(_controlledCr->Talk.Lexems.length()));
        if (!_controlledCr->Talk.Lexems.empty()) {
            out_buf->Push(_controlledCr->Talk.Lexems.c_str(), _controlledCr->Talk.Lexems.length());
        }
        out_buf->Write(_controlledCr->Talk.CurDialog.TextId);
        for (const auto& answer : _controlledCr->Talk.CurDialog.Answers) {
            out_buf->Write(answer.TextId);
        }
        out_buf->Write(time_duration_to_ms<uint>(talk_time));
    }
}

void Player::Send_TimeSync()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::TimeSync);

    out_buf->Write(_engine->GetYear());
    out_buf->Write(_engine->GetMonth());
    out_buf->Write(_engine->GetDay());
    out_buf->Write(_engine->GetHour());
    out_buf->Write(_engine->GetMinute());
    out_buf->Write(_engine->GetSecond());
    out_buf->Write(_engine->GetTimeMultiplier());
}

void Player::Send_Text(const Critter* from_cr, string_view text, uint8 how_say)
{
    STACK_TRACE_ENTRY();

    if (text.empty()) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};

    auto out_buf = _connection->WriteMsg(NetMessage::Text);

    out_buf->Write(from_id);
    out_buf->Write(how_say);
    out_buf->Write(text);
    out_buf->Write(false);
}

void Player::Send_TextEx(ident_t from_id, string_view text, uint8 how_say, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::Text);

    out_buf->Write(from_id);
    out_buf->Write(how_say);
    out_buf->Write(text);
    out_buf->Write(unsafe_text);
}

void Player::Send_TextMsg(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};

    auto out_buf = _connection->WriteMsg(NetMessage::TextMsg);

    out_buf->Write(from_id);
    out_buf->Write(how_say);
    out_buf->Write(text_pack);
    out_buf->Write(str_num);
}

void Player::Send_TextMsg(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    auto out_buf = _connection->WriteMsg(NetMessage::TextMsg);

    out_buf->Write(from_id);
    out_buf->Write(how_say);
    out_buf->Write(text_pack);
    out_buf->Write(str_num);
}

void Player::Send_TextMsgLex(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    const auto from_id = from_cr != nullptr ? from_cr->GetId() : ident_t {};

    auto out_buf = _connection->WriteMsg(NetMessage::TextMsgLex);

    out_buf->Write(from_id);
    out_buf->Write(how_say);
    out_buf->Write(text_pack);
    out_buf->Write(str_num);
    out_buf->Write(lexems);
}

void Player::Send_TextMsgLex(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (str_num == 0) {
        return;
    }

    auto out_buf = _connection->WriteMsg(NetMessage::TextMsgLex);

    out_buf->Write(from_id);
    out_buf->Write(how_say);
    out_buf->Write(text_pack);
    out_buf->Write(str_num);
    out_buf->Write(lexems);
}

void Player::Send_MapText(mpos hex, ucolor color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::MapText);

    out_buf->Write(hex);
    out_buf->Write(color);
    out_buf->Write(text);
    out_buf->Write(unsafe_text);
}

void Player::Send_MapTextMsg(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::MapTextMsg);

    out_buf->Write(hex);
    out_buf->Write(color);
    out_buf->Write(text_pack);
    out_buf->Write(str_num);
}

void Player::Send_MapTextMsgLex(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::MapTextMsgLex);

    out_buf->Write(hex);
    out_buf->Write(color);
    out_buf->Write(text_pack);
    out_buf->Write(str_num);
    out_buf->Write(lexems);
}

void Player::Send_Effect(hstring eff_pid, mpos hex, uint16 radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::Effect);

    out_buf->Write(eff_pid);
    out_buf->Write(hex);
    out_buf->Write(radius);
}

void Player::Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, mpos from_hex, mpos to_hex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::FlyEffect);

    out_buf->Write(eff_pid);
    out_buf->Write(from_cr_id);
    out_buf->Write(to_cr_id);
    out_buf->Write(from_hex);
    out_buf->Write(to_hex);
}

void Player::Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::PlaySound);

    out_buf->Write(cr_id_synchronize);
    out_buf->Write(sound_name);
}

void Player::Send_ViewMap()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_controlledCr);

    auto out_buf = _connection->WriteMsg(NetMessage::ViewMap);

    out_buf->Write(_controlledCr->ViewMapHex);
    out_buf->Write(_controlledCr->ViewMapLocId);
    out_buf->Write(_controlledCr->ViewMapLocEnt);
}

void Player::Send_PlaceToGameComplete()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _connection->WriteMsg(NetMessage::PlaceToGameComplete);
}

void Player::Send_SomeItems(const vector<Item*>& items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::SomeItems);

    out_buf->Write(string(context_param));
    out_buf->Write(static_cast<uint>(items.size()));

    for (const auto* item : items) {
        SendItem(*out_buf, item, owned, false, with_inner_entities);
    }
}

void Player::Send_Attachments(const Critter* from_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::CritterAttachments);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(from_cr->GetIsAttached());
    out_buf->Write(from_cr->GetAttachMaster());
    out_buf->Write(static_cast<uint16>(from_cr->AttachedCritters.size()));

    for (const auto* attached_cr : from_cr->AttachedCritters) {
        out_buf->Write(attached_cr->GetId());
    }
}

void Player::Send_AddCustomEntity(CustomEntity* entity, bool owned)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<const uint8*>* data = nullptr;
    vector<uint>* data_sizes = nullptr;

    entity->StoreData(owned, &data, &data_sizes);

    auto out_buf = _connection->WriteMsg(NetMessage::AddCustomEntity);

    out_buf->Write(entity->GetCustomHolderId());
    out_buf->Write(entity->GetCustomHolderEntry());
    out_buf->Write(entity->GetId());

    if (const auto* entity_with_proto = dynamic_cast<CustomEntityWithProto*>(entity); entity_with_proto != nullptr) {
        RUNTIME_ASSERT(_engine->GetEntityTypeInfo(entity->GetTypeName()).HasProtos);
        out_buf->Write(entity_with_proto->GetProtoId());
    }
    else {
        RUNTIME_ASSERT(!_engine->GetEntityTypeInfo(entity->GetTypeName()).HasProtos);
        out_buf->Write(hstring());
    }

    out_buf->WritePropsData(data, data_sizes);
}

void Player::Send_RemoveCustomEntity(ident_t id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto out_buf = _connection->WriteMsg(NetMessage::RemoveCustomEntity);

    out_buf->Write(id);
}

void Player::SendItem(NetOutBuffer& out_buf, const Item* item, bool owned, bool with_slot, bool with_inner_entities)
{
    STACK_TRACE_ENTRY();

    out_buf.Write(item->GetId());
    out_buf.Write(item->GetProtoId());

    if (with_slot) {
        out_buf.Write(item->GetCritterSlot());
    }

    vector<const uint8*>* item_data = nullptr;
    vector<uint>* item_data_sizes = nullptr;
    item->StoreData(owned, &item_data, &item_data_sizes);
    out_buf.WritePropsData(item_data, item_data_sizes);

    if (with_inner_entities) {
        SendInnerEntities(out_buf, item, false);
    }
    else {
        out_buf.Write(static_cast<uint16>(0));
    }
}

void Player::SendInnerEntities(NetOutBuffer& out_buf, const Entity* holder, bool owned)
{
    STACK_TRACE_ENTRY();

    if (!holder->HasInnerEntities()) {
        out_buf.Write(static_cast<uint16>(0));
        return;
    }

    const auto& entries_entities = holder->GetRawInnerEntities();

    out_buf.Write(static_cast<uint16>(entries_entities.size()));

    for (auto&& [entry, entities] : entries_entities) {
        out_buf.Write(entry);

        const auto entry_access = std::get<1>(_engine->GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.at(entry));

        if (entry_access == EntityHolderEntryAccess::Private || (entry_access == EntityHolderEntryAccess::Protected && !owned)) {
            out_buf.Write(static_cast<uint>(0));
            continue;
        }

        out_buf.Write(static_cast<uint>(entities.size()));

        for (const auto* entity : entities) {
            const auto* custom_entity = dynamic_cast<const CustomEntity*>(entity);
            RUNTIME_ASSERT(custom_entity);

            vector<const uint8*>* data = nullptr;
            vector<uint>* data_sizes = nullptr;

            custom_entity->StoreData(owned, &data, &data_sizes);

            out_buf.Write(custom_entity->GetId());

            if (const auto* custom_entity_with_proto = dynamic_cast<const CustomEntityWithProto*>(custom_entity); custom_entity_with_proto != nullptr) {
                out_buf.Write(custom_entity_with_proto->GetProtoId());
            }
            else {
                out_buf.Write(hstring());
            }

            out_buf.WritePropsData(data, data_sizes);

            SendInnerEntities(out_buf, entity, owned);
        }
    }
}

void Player::SendCritterMoving(NetOutBuffer& out_buf, const Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    out_buf.Write(static_cast<uint>(std::ceil(cr->Moving.WholeTime)));
    out_buf.Write(time_duration_to_ms<uint>(_engine->GameTime.FrameTime() - cr->Moving.StartTime + cr->Moving.OffsetTime));
    out_buf.Write(cr->Moving.Speed);
    out_buf.Write(cr->Moving.StartHex);
    out_buf.Write(static_cast<uint16>(cr->Moving.Steps.size()));

    for (const auto step : cr->Moving.Steps) {
        out_buf.Write(step);
    }

    out_buf.Write(static_cast<uint16>(cr->Moving.ControlSteps.size()));

    for (const auto control_step : cr->Moving.ControlSteps) {
        out_buf.Write(control_step);
    }

    out_buf.Write(cr->Moving.EndHexOffset);
}
