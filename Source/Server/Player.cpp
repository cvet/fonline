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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

Player::Player(ServerEngine* engine, ident_t id, unique_ptr<ServerConnection> connection, const Properties* props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props, nullptr),
    PlayerProperties(GetInitRef()),
    _connection {std::move(connection)}
{
    FO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();
    SetEntityLock(&_ownedLock);
}

Player::~Player()
{
    FO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();

    if (!_engine->IsShutdownInProgress()) {
        FO_VERIFY_AND_CONTINUE(!_controlledCr, "Player still controls a critter during destruction", GetId(), _controlledCr ? _controlledCr->GetId() : ident_t {});
        FO_VERIFY_AND_CONTINUE(!_viewMap, "Player still has view map context during destruction", GetId());
        FO_VERIFY_AND_CONTINUE(!_viewMapTarget, "Player still has view map target during destruction", GetId(), _viewMapTarget ? _viewMapTarget->GetId() : ident_t {});
        FO_VERIFY_AND_CONTINUE(!_sendIgnoreEntity, "Player still has send-ignore entity during destruction", GetId(), _sendIgnoreEntity ? _sendIgnoreEntity->GetId() : ident_t {});
        FO_VERIFY_AND_CONTINUE(!_sendIgnoreProperty, "Player still has send-ignore property during destruction", GetId());
    }
}

void Player::SetName(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    _name = name;
}

void Player::SetControlledCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    _controlledCr = cr;
}

auto Player::GetSyncWidenEntity() noexcept -> ServerEntity*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();
    return _controlledCr.get();
}

void Player::DetachCritter()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (_controlledCr) {
        _controlledCr->DetachPlayer();
    }
}

void Player::SwapConnection(Player* other) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    ValidateEntityAccessStrong(other);
    FO_STRONG_ASSERT(other, "Player connection swap target is null");
    FO_STRONG_ASSERT(other != this, "Player connection swap target is the same player");

    std::swap(_connection, other->_connection);
}

void Player::SetIgnoreSendEntityProperty(const Entity* entity, const Property* prop) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    _sendIgnoreEntity = entity;
    _sendIgnoreProperty = prop;
}

void Player::SetViewMap(Map* map, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(!_controlledCr, "Controlled cr is already set");
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    if (_viewMapTarget != map) {
        if (_viewMapTarget) {
            _viewMapTarget->RemoveSpectatorPlayer(this);
        }

        _viewMapTarget = map;
        _viewMapTarget->AddSpectatorPlayer(this);
    }

    _viewMap = SafeAlloc::MakeUnique<ViewMapContext>(ViewMapContext {.MapId = map->GetId(), .Hex = hex});
}

void Player::ResetViewMap() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS_STRONG();
    if (_viewMapTarget) {
        _viewMapTarget->RemoveSpectatorPlayer(this);
        _viewMapTarget = nullptr;
    }

    _viewMap.reset();
}

void Player::Send_LoginSuccess()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<const uint8_t*>* global_vars_data = nullptr;
    vector<uint32_t>* global_vars_data_sizes = nullptr;
    _engine->StoreData(false, &global_vars_data, &global_vars_data_sizes);

    vector<const uint8_t*>* player_data = nullptr;
    vector<uint32_t>* player_data_sizes = nullptr;
    StoreData(true, &player_data, &player_data_sizes);

    auto out_buf = _connection->WriteMsg(NetMessage::LoginSuccess);

    out_buf->Write(GetId());
    out_buf->WritePropsData(global_vars_data, global_vars_data_sizes);
    out_buf->WritePropsData(player_data, player_data_sizes);
    SendInnerEntities(*out_buf, _engine.get(), false);
}

void Player::Send_AddCritter(const Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto is_chosen = cr == GetControlledCritter();

    vector<const uint8_t*>* cr_data = nullptr;
    vector<uint32_t>* cr_data_sizes = nullptr;
    cr->StoreData(is_chosen, &cr_data, &cr_data_sizes);

    const auto inv_items = cr->GetInvItems();
    vector<const Item*> send_items;
    send_items.reserve(inv_items.size());

    for (const auto& item : inv_items) {
        if (item->CanSendItem(!is_chosen)) {
            send_items.emplace_back(item.get());
        }
    }

    auto out_buf = _connection->WriteMsg(NetMessage::AddCritter);

    out_buf->Write(cr->GetId());
    out_buf->Write(cr->GetProtoId());
    out_buf->Write(cr->GetHex());
    out_buf->Write(cr->GetHexOffset());
    out_buf->Write(cr->GetDir());
    out_buf->Write(cr->GetCondition());
    out_buf->Write(cr->GetControlledByPlayer());
    out_buf->Write(cr->GetControlledByPlayer() && cr->GetPlayer() == nullptr);
    out_buf->Write(is_chosen);
    out_buf->WritePropsData(cr_data, cr_data_sizes);

    SendInnerEntities(*out_buf, cr, is_chosen);

    out_buf->Write(numeric_cast<uint32_t>(send_items.size()));

    for (const auto* item : send_items) {
        SendItem(*out_buf, item, is_chosen, true, true);
    }

    {
        const auto* receiver_cr = GetControlledCritter();
        const auto vis_mode = receiver_cr != nullptr ? receiver_cr->GetVisibleCritterMode(cr->GetId()) : CritterVisibilityMode::Full;
        out_buf->Write(vis_mode != CritterVisibilityMode::None ? vis_mode : CritterVisibilityMode::Full);
    }

    const auto cr_attached = cr->GetAttachedCritters();

    out_buf->Write(cr->GetIsAttached());
    out_buf->Write(numeric_cast<uint16_t>(cr_attached.size()));

    for (const auto& attached_cr : cr_attached) {
        out_buf->Write(attached_cr->GetId());
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
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::RemoveCritter);

    out_buf->Write(cr->GetId());
}

void Player::Send_CritterVisibilityMode(const Critter* cr, CritterVisibilityMode mode)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::CritterVisibilityMode);

    out_buf->Write(cr->GetId());
    out_buf->Write(mode);
}

void Player::Send_LoadMap(const Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const Location* loc = nullptr;
    hstring pid_map;
    hstring pid_loc;
    int32_t map_index_in_loc = 0;

    if (map != nullptr) {
        loc = map->GetLocation();
        pid_map = map->GetProtoId();
        pid_loc = loc->GetProtoId();
        map_index_in_loc = numeric_cast<int32_t>(loc->GetMapIndex(pid_map));
    }

    vector<const uint8_t*>* map_data = nullptr;
    vector<uint32_t>* map_data_sizes = nullptr;
    vector<const uint8_t*>* loc_data = nullptr;
    vector<uint32_t>* loc_data_sizes = nullptr;

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
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(entity, "Missing entity instance");
    FO_VERIFY_AND_THROW(prop, "Missing property instance");

    if (_sendIgnoreEntity == entity && _sendIgnoreProperty == prop) {
        return;
    }

    const auto& props = entity->GetProperties();
    props.ValidateForRawData(prop);
    const auto prop_raw_data = props.GetRawData(prop);

    auto out_buf = _connection->WriteMsg(NetMessage::Property);

    out_buf->Write(numeric_cast<uint32_t>(prop_raw_data.size()));
    out_buf->Write(type);

    switch (type) {
    case NetProperty::CritterItem: {
        const auto* item = dynamic_cast<const Item*>(entity);
        const auto* server_entity = dynamic_cast<const ServerEntity*>(entity);
        FO_VERIFY_AND_THROW(item, "Missing item instance");
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(item->GetCritterId());
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::Critter: {
        const auto* server_entity = dynamic_cast<const ServerEntity*>(entity);
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::MapItem: {
        const auto* server_entity = dynamic_cast<const ServerEntity*>(entity);
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::ChosenItem: {
        const auto* server_entity = dynamic_cast<const ServerEntity*>(entity);
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::CustomEntity: {
        const auto* custom_entity = dynamic_cast<const CustomEntity*>(entity);
        FO_VERIFY_AND_THROW(custom_entity, "Missing custom entity instance");
        out_buf->Write(custom_entity->GetId());
    } break;
    default:
        break;
    }

    out_buf->Write(prop->GetRegIndex());
    out_buf->Push(prop_raw_data);
}

void Player::Send_Moving(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    if (from_cr->IsMoving()) {
        auto out_buf = _connection->WriteMsg(NetMessage::CritterMove);

        out_buf->Write(from_cr->GetId());
        SendCritterMoving(*out_buf, from_cr);
    }
    else {
        auto out_buf = _connection->WriteMsg(NetMessage::CritterPos);

        out_buf->Write(from_cr->GetId());
        out_buf->Write(from_cr->GetHex());
        out_buf->Write(from_cr->GetHexOffset());
        out_buf->Write(from_cr->GetDir());
    }
}

void Player::Send_MovingSpeed(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::CritterMoveSpeed);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(from_cr->GetMoving()->GetSpeed());
}

void Player::Send_Dir(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::CritterDir);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(from_cr->GetDir());
}

void Player::Send_Action(const Critter* from_cr, CritterAction action, int32_t action_data, const Item* context_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
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
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto is_chosen = from_cr == GetControlledCritter();

    vector<const Item*> send_items;

    if (!is_chosen) {
        const auto inv_items = from_cr->GetInvItems();
        send_items.reserve(inv_items.size());

        for (const auto& item : inv_items) {
            if (item->CanSendItem(true)) {
                send_items.emplace_back(item.get());
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

    out_buf->Write(numeric_cast<uint32_t>(send_items.size()));

    for (const auto* item : send_items) {
        SendItem(*out_buf, item, false, true, true);
    }
}

void Player::Send_AddItemOnMap(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::AddItemOnMap);

    out_buf->Write(item->GetHex());
    SendItem(*out_buf, item, false, false, true);
}

void Player::Send_RemoveItemFromMap(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::RemoveItemFromMap);

    out_buf->Write(item->GetId());
}

void Player::Send_ChosenAddItem(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::ChosenAddItem);

    SendItem(*out_buf, item, true, true, true);
}

void Player::Send_ChosenRemoveItem(const Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::ChosenRemoveItem);

    out_buf->Write(item->GetId());
}

void Player::Send_Teleport(const Critter* cr, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::CritterTeleport);

    out_buf->Write(cr->GetId());
    out_buf->Write(to_hex);
}

void Player::Send_TimeSync()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::TimeSync);

    out_buf->Write(_engine->GameTime.GetSynchronizedTime());
}

void Player::Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::InfoMessage);

    out_buf->Write(info_message);
    out_buf->Write(extra_text);
}

void Player::Send_ViewMap()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(_viewMap, "Player has no visible map");

    auto out_buf = _connection->WriteMsg(NetMessage::ViewMap);

    out_buf->Write(_viewMap->Hex);
}

void Player::Send_PlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    _connection->WriteMsg(NetMessage::PlaceToGameComplete);
}

void Player::Send_SomeItems(const_span<Item*> items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::SomeItems);

    out_buf->Write(string(context_param));
    out_buf->Write(numeric_cast<uint32_t>(items.size()));

    for (const auto* item : items) {
        SendItem(*out_buf, item, owned, false, with_inner_entities);
    }
}

void Player::Send_Attachments(const Critter* from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::CritterAttachments);

    out_buf->Write(from_cr->GetId());

    out_buf->Write(from_cr->GetIsAttached());
    out_buf->Write(from_cr->GetAttachMaster());

    const auto from_cr_attached = from_cr->GetAttachedCritters();
    out_buf->Write(numeric_cast<uint16_t>(from_cr_attached.size()));

    for (const auto& attached_cr : from_cr_attached) {
        out_buf->Write(attached_cr->GetId());
    }
}

void Player::Send_AddCustomEntity(CustomEntity* entity, bool owned)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<const uint8_t*>* data = nullptr;
    vector<uint32_t>* data_sizes = nullptr;

    entity->StoreData(owned, &data, &data_sizes);

    auto out_buf = _connection->WriteMsg(NetMessage::AddCustomEntity);

    out_buf->Write(entity->GetCustomHolderId());
    out_buf->Write(entity->GetCustomHolderEntry());
    out_buf->Write(entity->GetId());

    if (const auto* entity_with_proto = dynamic_cast<CustomEntityWithProto*>(entity); entity_with_proto != nullptr) {
        FO_VERIFY_AND_THROW(_engine->GetEntityType(entity->GetTypeName()).HasProtos, "Entity type has no prototypes but a proto entity was requested");
        out_buf->Write(entity_with_proto->GetProtoId());
    }
    else {
        FO_VERIFY_AND_THROW(!_engine->GetEntityType(entity->GetTypeName()).HasProtos, "Custom entity type requires a proto id but the entity is not proto-backed", entity->GetTypeName(), entity->GetId());
        out_buf->Write(hstring());
    }

    out_buf->WritePropsData(data, data_sizes);
}

void Player::Send_RemoveCustomEntity(ident_t id)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    auto out_buf = _connection->WriteMsg(NetMessage::RemoveCustomEntity);

    out_buf->Write(id);
}

void Player::SendItem(NetOutBuffer& out_buf, const Item* item, bool owned, bool with_slot, bool with_inner_entities)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    out_buf.Write(item->GetId());
    out_buf.Write(item->GetProtoId());

    if (with_slot) {
        out_buf.Write(item->GetCritterSlot());
    }

    vector<const uint8_t*>* item_data = nullptr;
    vector<uint32_t>* item_data_sizes = nullptr;
    item->StoreData(owned, &item_data, &item_data_sizes);
    out_buf.WritePropsData(item_data, item_data_sizes);

    if (with_inner_entities) {
        SendInnerEntities(out_buf, item, false);
    }
    else {
        out_buf.Write(numeric_cast<uint16_t>(0));
    }
}

void Player::SendInnerEntities(NetOutBuffer& out_buf, const Entity* holder, bool owned)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    if (!holder->HasInnerEntities()) {
        out_buf.Write(numeric_cast<uint16_t>(0));
        return;
    }

    const auto& entries_entities = holder->GetInnerEntities();
    uint16_t entries_count = 0;

    for (const auto& entry : entries_entities | std::views::keys) {
        const auto entry_sync = _engine->GetEntityType(holder->GetTypeName()).HolderEntries.at(entry).Sync;

        if (entry_sync == EntityHolderEntrySync::PublicSync || (entry_sync == EntityHolderEntrySync::OwnerSync && owned)) {
            entries_count++;
        }
    }

    out_buf.Write(entries_count);

    if (entries_count == 0) {
        return;
    }

    for (auto&& [entry, entities] : entries_entities) {
        const auto entry_sync = _engine->GetEntityType(holder->GetTypeName()).HolderEntries.at(entry).Sync;

        if (entry_sync == EntityHolderEntrySync::NoSync || (entry_sync == EntityHolderEntrySync::OwnerSync && !owned)) {
            continue;
        }

        out_buf.Write(entry);
        out_buf.Write(numeric_cast<uint32_t>(entities.size()));

        for (const auto& entity : entities) {
            const auto* custom_entity = dynamic_cast<const CustomEntity*>(entity.get());
            FO_VERIFY_AND_THROW(custom_entity, "Missing custom entity instance");

            vector<const uint8_t*>* data = nullptr;
            vector<uint32_t>* data_sizes = nullptr;

            custom_entity->StoreData(owned, &data, &data_sizes);

            out_buf.Write(custom_entity->GetId());

            if (const auto* custom_entity_with_proto = dynamic_cast<const CustomEntityWithProto*>(custom_entity); custom_entity_with_proto != nullptr) {
                out_buf.Write(custom_entity_with_proto->GetProtoId());
            }
            else {
                out_buf.Write(hstring());
            }

            out_buf.WritePropsData(data, data_sizes);

            SendInnerEntities(out_buf, entity.get(), owned);
        }
    }
}

void Player::SendCritterMoving(NetOutBuffer& out_buf, const Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_NON_CONST_METHOD_HINT();

    FO_VERIFY_AND_THROW(cr->IsMoving(), "Critter is not moving");

    auto* moving = cr->GetMoving();
    FO_VERIFY_AND_THROW(moving, "Missing active movement state");

    out_buf.Write(iround<int32_t>(std::ceil(moving->GetWholeTime())));
    out_buf.Write(iround<int32_t>(moving->GetRuntimeElapsedTime(_engine->GameTime.GetFrameTime())));
    out_buf.Write(moving->GetSpeed());
    out_buf.Write(moving->GetStartHex());
    out_buf.Write(numeric_cast<uint16_t>(moving->GetSteps().size()));

    for (const auto step : moving->GetSteps()) {
        out_buf.Write(step.hex());
    }

    out_buf.Write(numeric_cast<uint16_t>(moving->GetControlSteps().size()));

    for (const auto control_step : moving->GetControlSteps()) {
        out_buf.Write(control_step);
    }

    out_buf.Write(moving->GetEndHexOffset());
}

FO_END_NAMESPACE
