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

Player::Player(ptr<ServerEngine> engine, ident_t id, unique_ptr<ServerConnection> connection, nptr<const Properties> props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props, nullptr),
    PlayerProperties(*GetInitRef()),
    _connection {std::move(connection)}
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    SetEntityLock(&_ownedLock);
}

Player::~Player()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (!_engine->IsShutdownInProgress()) {
        FO_VERIFY_AND_CONTINUE(!_controlledCr.load(std::memory_order_relaxed), "Player still controls a critter during destruction", GetId());
        FO_VERIFY_AND_CONTINUE(!_viewMap, "Player still has view map context during destruction", GetId());
        FO_VERIFY_AND_CONTINUE(!_viewMapTarget, "Player still has view map target during destruction", GetId());
        FO_VERIFY_AND_CONTINUE(!_sendIgnoreEntity.load(std::memory_order_relaxed), "Player still has send-ignore entity during destruction", GetId());
        FO_VERIFY_AND_CONTINUE(!_sendIgnoreProperty.load(std::memory_order_relaxed), "Player still has send-ignore property during destruction", GetId());
    }
}

auto Player::GetName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _name;
}

auto Player::GetControlledCritter() noexcept -> nptr<Critter>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _controlledCr.load(std::memory_order_acquire);
}

auto Player::GetControlledCritter() const noexcept -> nptr<const Critter>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _controlledCr.load(std::memory_order_acquire);
}

ptr<ServerConnection> Player::GetConnection() noexcept FO_TSA_NO_ANALYSIS
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _connection;
}

ptr<const ServerConnection> Player::GetConnection() const noexcept FO_TSA_NO_ANALYSIS
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _connection;
}

auto Player::GetViewMap() const noexcept -> nptr<const ViewMapContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _viewMap ? make_nptr(&*_viewMap) : nullptr;
}

auto Player::GetViewMapTarget() const noexcept -> nptr<const Map>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _viewMapTarget;
}

auto Player::GetViewMapTarget() noexcept -> nptr<Map>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _viewMapTarget;
}

void Player::SetName(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    _name = name;
}

void Player::SetControlledCritter(nptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    _controlledCr.store(cr.get(), std::memory_order_release);
}

auto Player::GetSyncWidenEntity() noexcept -> nptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<ServerEntity>(_controlledCr.load(std::memory_order_acquire));
}

auto Player::GetSyncWidenEntity() const noexcept -> nptr<const ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<const ServerEntity>(_controlledCr.load(std::memory_order_acquire));
}

void Player::DetachCritter()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    if (auto controlled_cr = _controlledCr.load(std::memory_order_acquire)) {
        controlled_cr->DetachPlayer();
    }
}

// FO_TSA_NO_ANALYSIS: swaps this->_connection (guarded, held below) with other->_connection (guarded by the
// other player's lock, which is deliberately not taken — see below); the cross-object guarded access cannot be
// expressed to TSA.
void Player::SwapConnection(ptr<Player> other) noexcept FO_TSA_NO_ANALYSIS
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(other);
    FO_STRONG_ASSERT(other != this, "Player connection swap target is the same player");

    // Exclude a concurrent lock-free send on this player while its connection pointer is swapped. `other` is the
    // freshly-connected, not-yet-in-world player (reconnect, Server.cpp): it is in no critter's visible set and is
    // no spectator, so it can never be a lock-free send target and needs no guard here.
    scoped_lock conn_lock {_connectionLock};

    std::swap(_connection, other->_connection);
}

void Player::SetIgnoreSendEntityProperty(nptr<const Entity> entity, nptr<const Property> prop) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    _sendIgnoreEntity.store(entity.get(), std::memory_order_release);
    _sendIgnoreProperty.store(prop.get(), std::memory_order_release);
}

void Player::SetViewMap(ptr<Map> map, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VERIFY_AND_THROW(!_controlledCr.load(std::memory_order_acquire), "Controlled cr is already set");

    if (_viewMapTarget != map) {
        if (_viewMapTarget) {
            _viewMapTarget->RemoveSpectatorPlayer(this);
        }

        _viewMapTarget = map;
        _viewMapTarget->AddSpectatorPlayer(this);
    }

    _viewMap.emplace(ViewMapContext {.MapId = map->GetId(), .Hex = hex});
}

void Player::ResetViewMap() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (_viewMapTarget) {
        _viewMapTarget->RemoveSpectatorPlayer(this);
        _viewMapTarget = nullptr;
    }

    _viewMap.reset();
}

void Player::Send_LoginSuccess()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    const auto player_data = StoreData(true);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::LoginSuccess);

    out_buf->Write(GetId());

    {
        _engine->LockForPropertyAccess();
        auto unlock_global_vars = scope_exit([this]() noexcept { _engine->UnlockForPropertyAccess(); });

        const auto global_vars_data = _engine->StoreData(false);
        out_buf->WritePropsData(*global_vars_data.Data, *global_vars_data.Sizes);
    }

    out_buf->WritePropsData(*player_data.Data, *player_data.Sizes);
    SendInnerEntities(*out_buf, _engine, false);
}

void Player::Send_AddCritter(ptr<const Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    auto controlled_cr = GetControlledCritter();
    nptr<const Critter> cr_lookup = cr;
    const bool is_chosen = controlled_cr == cr_lookup;

    const auto cr_data = cr->StoreData(is_chosen);

    const auto inv_items = cr->GetInvItems();
    vector<ptr<const Item>> send_items;
    send_items.reserve(inv_items.size());

    for (ptr<const Item> item : inv_items) {
        if (item->CanSendItem(!is_chosen)) {
            send_items.emplace_back(item);
        }
    }

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::AddCritter);

    out_buf->Write(cr->GetId());
    out_buf->Write(cr->GetProtoId());
    out_buf->Write(cr->GetHex());
    out_buf->Write(cr->GetHexOffset());
    out_buf->Write(cr->GetDir());
    out_buf->Write(cr->GetCondition());
    out_buf->Write(cr->GetControlledByPlayer());
    out_buf->Write(cr->GetControlledByPlayer() && !cr->GetPlayer());
    out_buf->Write(is_chosen);
    out_buf->WritePropsData(*cr_data.Data, *cr_data.Sizes);

    SendInnerEntities(*out_buf, cr, is_chosen);

    out_buf->Write(numeric_cast<uint32_t>(send_items.size()));

    for (ptr<const Item> item : send_items) {
        SendItem(*out_buf, item, is_chosen, true, true);
    }

    {
        auto receiver_cr = GetControlledCritter();
        CritterVisibilityMode vis_mode = CritterVisibilityMode::Full;

        if (receiver_cr) {
            vis_mode = receiver_cr->GetVisibleCritterMode(cr->GetId());
        }

        out_buf->Write(vis_mode != CritterVisibilityMode::None ? vis_mode : CritterVisibilityMode::Full);
    }

    const auto cr_attached = cr->GetAttachedCritters();

    out_buf->Write(cr->GetIsAttached());
    out_buf->Write(numeric_cast<uint16_t>(cr_attached.size()));

    for (ptr<const Critter> attached_cr : cr_attached) {
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

void Player::Send_RemoveCritter(ptr<const Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::RemoveCritter);

    out_buf->Write(cr->GetId());
}

void Player::Send_CritterVisibilityMode(ptr<const Critter> cr, CritterVisibilityMode mode)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::CritterVisibilityMode);

    out_buf->Write(cr->GetId());
    out_buf->Write(mode);
}

void Player::Send_LoadMap(nptr<const Map> map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(map);

    nptr<const Location> loc;
    hstring pid_map;
    hstring pid_loc;
    ident_t loc_id {};
    ident_t map_id {};
    int32_t map_index_in_loc = 0;

    if (map) {
        loc = map->GetLocation();
        FO_VERIFY_AND_THROW(loc, "Map has no owning location");

        FO_VALIDATE_ENTITY_ACCESS_VALUE(loc);
        pid_map = map->GetProtoId();
        pid_loc = loc->GetProtoId();
        loc_id = loc->GetId();
        map_id = map->GetId();
        map_index_in_loc = numeric_cast<int32_t>(loc->GetMapIndex(pid_map));
    }

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::LoadMap);

    out_buf->Write(loc_id);
    out_buf->Write(map_id);
    out_buf->Write(pid_loc);
    out_buf->Write(pid_map);
    out_buf->Write(map_index_in_loc);

    if (map) {
        const auto map_data = map->StoreData(false);
        const auto loc_data = loc->StoreData(false);
        out_buf->WritePropsData(*map_data.Data, *map_data.Sizes);
        out_buf->WritePropsData(*loc_data.Data, *loc_data.Sizes);
        SendInnerEntities(*out_buf, loc, false);
        SendInnerEntities(*out_buf, map, false);
    }
}

void Player::Send_Property(NetProperty type, ptr<const Property> prop, ptr<const Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(entity);

    if (entity == _sendIgnoreEntity.load(std::memory_order_acquire) && prop == _sendIgnoreProperty.load(std::memory_order_acquire)) {
        return;
    }

    const auto props = entity->GetProperties();
    props->ValidateForRawData(prop);
    const auto prop_raw_data = props->GetRawData(prop);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::Property);

    out_buf->Write(numeric_cast<uint32_t>(prop_raw_data.size()));
    out_buf->Write(type);

    switch (type) {
    case NetProperty::CritterItem: {
        const auto item = entity.dyn_cast<Item>();
        const auto server_entity = entity.dyn_cast<ServerEntity>();
        FO_VERIFY_AND_THROW(item, "Missing item instance");
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(item->GetCritterId());
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::Critter: {
        const auto server_entity = entity.dyn_cast<ServerEntity>();
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::MapItem: {
        const auto server_entity = entity.dyn_cast<ServerEntity>();
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::ChosenItem: {
        const auto server_entity = entity.dyn_cast<ServerEntity>();
        FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
        out_buf->Write(server_entity->GetId());
    } break;
    case NetProperty::CustomEntity: {
        const auto custom_entity = entity.dyn_cast<CustomEntity>();
        FO_VERIFY_AND_THROW(custom_entity, "Missing custom entity instance");
        out_buf->Write(custom_entity->GetId());
    } break;
    default:
        break;
    }

    out_buf->Write(prop->GetRegIndex());
    out_buf->Push(prop_raw_data);
}

void Player::Send_Moving(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    scoped_lock conn_lock {_connectionLock};

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

void Player::Send_MovingSpeed(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::CritterMoveSpeed);

    out_buf->Write(from_cr->GetId());

    auto moving = from_cr->GetMoving();
    FO_VERIFY_AND_THROW(moving, "Critter has no active movement state");
    out_buf->Write(moving->GetSpeed());
}

void Player::Send_Dir(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::CritterDir);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(from_cr->GetDir());
}

void Player::Send_Action(ptr<const Critter> from_cr, CritterAction action, int32_t action_data, nptr<const Item> context_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    auto controlled_cr = GetControlledCritter();
    nptr<const Critter> source_cr = from_cr;
    const bool is_chosen = controlled_cr == source_cr;

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::CritterAction);

    out_buf->Write(from_cr->GetId());
    out_buf->Write(action);
    out_buf->Write(action_data);
    out_buf->Write(!!context_item);

    if (context_item) {
        SendItem(*out_buf, context_item, is_chosen, false, false);
    }
}

void Player::Send_MoveItem(ptr<const Critter> from_cr, nptr<const Item> moved_item, CritterAction action, CritterItemSlot prev_slot)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    auto controlled_cr = GetControlledCritter();
    nptr<const Critter> source_cr = from_cr;
    const bool is_chosen = controlled_cr == source_cr;

    vector<ptr<const Item>> send_items;

    if (!is_chosen) {
        const auto inv_items = from_cr->GetInvItems();
        send_items.reserve(inv_items.size());

        for (ptr<const Item> item : inv_items) {
            if (item->CanSendItem(true)) {
                send_items.emplace_back(item);
            }
        }
    }

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::CritterMoveItem);

    CritterItemSlot moved_item_slot = CritterItemSlot::Inventory;

    if (moved_item) {
        moved_item_slot = moved_item->GetCritterSlot();
    }

    out_buf->Write(from_cr->GetId());
    out_buf->Write(action);
    out_buf->Write(prev_slot);
    out_buf->Write(moved_item_slot);
    out_buf->Write(!!moved_item);

    if (moved_item) {
        SendItem(*out_buf, moved_item, is_chosen, false, false);
    }

    out_buf->Write(numeric_cast<uint32_t>(send_items.size()));

    for (ptr<const Item> item : send_items) {
        SendItem(*out_buf, item, false, true, true);
    }
}

void Player::Send_AddItemOnMap(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::AddItemOnMap);

    out_buf->Write(item->GetHex());
    SendItem(*out_buf, item, false, false, true);
}

void Player::Send_RemoveItemFromMap(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::RemoveItemFromMap);

    out_buf->Write(item->GetId());
}

void Player::Send_ChosenAddItem(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::ChosenAddItem);

    SendItem(*out_buf, item, true, true, true);
}

void Player::Send_ChosenRemoveItem(ptr<const Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::ChosenRemoveItem);

    out_buf->Write(item->GetId());
}

void Player::Send_Teleport(ptr<const Critter> cr, mpos to_hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::CritterTeleport);

    out_buf->Write(cr->GetId());
    out_buf->Write(to_hex);
}

void Player::Send_TimeSync()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::TimeSync);

    out_buf->Write(_engine->GameTime.GetSynchronizedTime());
}

void Player::Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::InfoMessage);

    out_buf->Write(info_message);
    out_buf->Write(extra_text);
}

void Player::Send_HashList(const_span<string> hash_strings)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    if (_connection->IsHardDisconnected() || !_connection->IsHandshakeComplete()) {
        return;
    }

    auto out_buf = _connection->WriteMsg(NetMessage::HashList);

    out_buf->Write(numeric_cast<uint32_t>(hash_strings.size()));

    for (const auto& hash_string : hash_strings) {
        out_buf->Write(hash_string);
    }
}

void Player::Send_RemoteCall(hstring rpc_name, const_span<uint8_t> rpc_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::RemoteCall);

    out_buf->Write<hstring>(rpc_name);
    out_buf->Write<int32_t>(numeric_cast<int32_t>(rpc_data.size()));
    out_buf->Push(rpc_data);
}

void Player::Send_Ping(bool answer)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::Ping);

    out_buf->Write(answer);
}

void Player::Send_HandshakeAnswer(bool compatibility_outdated, bool updater_outdated, uint32_t out_encrypt_key)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    // The out-buffer encrypt key must be installed on the same connection the answer was written to, so
    // both steps happen under one connection-lock hold.
    scoped_lock conn_lock {_connectionLock};

    {
        auto out_buf = _connection->WriteMsg(NetMessage::HandshakeAnswer);

        out_buf->Write(compatibility_outdated);
        out_buf->Write(updater_outdated);
        out_buf->Write(out_encrypt_key);
    }

    {
        auto out_buf = _connection->WriteBuf();

        out_buf->SetEncryptKey(out_encrypt_key);
    }
}

void Player::Send_InitData(const_span<uint8_t> update_desc)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::InitData);

    out_buf->Write(numeric_cast<uint32_t>(update_desc.size()));
    out_buf->Push(update_desc);

    {
        _engine->LockForPropertyAccess();
        auto unlock_global_vars = scope_exit([this]() noexcept { _engine->UnlockForPropertyAccess(); });

        const auto global_vars_data = _engine->StoreData(false);
        out_buf->WritePropsData(*global_vars_data.Data, *global_vars_data.Sizes);
    }

    out_buf->Write(_engine->GameTime.GetSynchronizedTime());
}

void Player::Send_UpdateFileData(const_span<uint8_t> update_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::UpdateFileData);

    out_buf->Write(numeric_cast<int32_t>(update_data.size()));

    if (!update_data.empty()) {
        out_buf->Push(update_data);
    }
}

void Player::Send_ViewMap()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VERIFY_AND_THROW(_viewMap, "Player has no visible map");
    auto view_map = make_ptr(&*_viewMap);

    scoped_lock conn_lock {_connectionLock};
    auto out_buf = _connection->WriteMsg(NetMessage::ViewMap);

    out_buf->Write(view_map->Hex);
}

void Player::Send_PlaceToGameComplete()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    _connection->WriteMsg(NetMessage::PlaceToGameComplete);
}

void Player::Send_SomeItems(const_span<ptr<const Item>> items, bool owned, bool with_inner_entities, const any_t& context_param)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::SomeItems);

    out_buf->Write(string(context_param));
    out_buf->Write(numeric_cast<uint32_t>(items.size()));

    for (ptr<const Item> item : items) {
        SendItem(*out_buf, item, owned, false, with_inner_entities);
    }
}

void Player::Send_Attachments(ptr<const Critter> from_cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(from_cr);

    scoped_lock conn_lock {_connectionLock};

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

void Player::Send_AddCustomEntity(ptr<CustomEntity> entity, bool owned)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(entity);

    const auto data = entity->StoreData(owned);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::AddCustomEntity);

    out_buf->Write(entity->GetCustomHolderId());
    out_buf->Write(entity->GetCustomHolderEntry());
    out_buf->Write(entity->GetId());

    const auto entity_with_proto = entity.dyn_cast<CustomEntityWithProto>();
    if (entity_with_proto) {
        FO_VERIFY_AND_THROW(_engine->GetEntityType(entity->GetTypeName()).HasProtos, "Entity type has no prototypes but a proto entity was requested");
        out_buf->Write(entity_with_proto->GetProtoId());
    }
    else {
        FO_VERIFY_AND_THROW(!_engine->GetEntityType(entity->GetTypeName()).HasProtos, "Custom entity type requires a proto id but the entity is not proto-backed", entity->GetTypeName(), entity->GetId());
        out_buf->Write(hstring());
    }

    out_buf->WritePropsData(*data.Data, *data.Sizes);
}

void Player::Send_RemoveCustomEntity(ident_t id)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    scoped_lock conn_lock {_connectionLock};

    auto out_buf = _connection->WriteMsg(NetMessage::RemoveCustomEntity);

    out_buf->Write(id);
}

void Player::SendItem(NetOutBuffer& out_buf, ptr<const Item> item, bool owned, bool with_slot, bool with_inner_entities)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(item);

    out_buf.Write(item->GetId());
    out_buf.Write(item->GetProtoId());

    if (with_slot) {
        out_buf.Write(item->GetCritterSlot());
    }

    const auto item_data = item->StoreData(owned);
    out_buf.WritePropsData(*item_data.Data, *item_data.Sizes);

    if (with_inner_entities) {
        SendInnerEntities(out_buf, item, false);
    }
    else {
        out_buf.Write(numeric_cast<uint16_t>(0));
    }
}

void Player::SendInnerEntities(NetOutBuffer& out_buf, ptr<const Entity> holder, bool owned)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(holder);

    if (!holder->HasInnerEntities()) {
        out_buf.Write(numeric_cast<uint16_t>(0));
        return;
    }

    auto entries_entities = holder->GetInnerEntities();
    FO_VERIFY_AND_THROW(entries_entities, "Entry entities collection is null");
    uint16_t entries_count = 0;

    for (const auto& entry : *entries_entities | std::views::keys) {
        const auto entry_sync = _engine->GetEntityType(holder->GetTypeName()).HolderEntries.at(entry).Sync;

        if (entry_sync == EntityHolderEntrySync::PublicSync || (entry_sync == EntityHolderEntrySync::OwnerSync && owned)) {
            entries_count++;
        }
    }

    out_buf.Write(entries_count);

    if (entries_count == 0) {
        return;
    }

    for (auto&& [entry, entities] : *entries_entities) {
        const auto entry_sync = _engine->GetEntityType(holder->GetTypeName()).HolderEntries.at(entry).Sync;

        if (entry_sync == EntityHolderEntrySync::NoSync || (entry_sync == EntityHolderEntrySync::OwnerSync && !owned)) {
            continue;
        }

        out_buf.Write(entry);
        out_buf.Write(numeric_cast<uint32_t>(entities.size()));

        for (const auto& entity : entities) {
            auto custom_entity = require_refcount_ptr(entity.dyn_cast<CustomEntity>());

            const auto data = custom_entity->StoreData(owned);

            out_buf.Write(custom_entity->GetId());

            const auto custom_entity_with_proto = (nptr<const CustomEntity> {custom_entity}).dyn_cast<CustomEntityWithProto>();
            if (custom_entity_with_proto) {
                out_buf.Write(custom_entity_with_proto->GetProtoId());
            }
            else {
                out_buf.Write(hstring());
            }

            out_buf.WritePropsData(*data.Data, *data.Sizes);

            SendInnerEntities(out_buf, entity, owned);
        }
    }
}

void Player::SendCritterMoving(NetOutBuffer& out_buf, ptr<const Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    FO_VALIDATE_ENTITY_ACCESS_VALUE(cr);

    FO_VERIFY_AND_THROW(cr->IsMoving(), "Critter is not moving");

    auto moving = cr->GetMoving();
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
