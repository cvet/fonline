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

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "EntitySync.h"
#include "Geometry.h"
#include "ServerConnection.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE

class Item;
class Critter;
class Map;
class Location;
class MapManager;

struct ViewMapContext
{
    ident_t MapId {};
    mpos Hex {};
};

class Player final : public ServerEntity, public PlayerProperties
{
public:
    Player() = delete;
    Player(ptr<ServerEngine> engine, ident_t id, unique_ptr<ServerConnection> connection, nptr<const Properties> props = nullptr) noexcept;
    Player(const Player&) = delete;
    Player(Player&&) noexcept = delete;
    auto operator=(const Player&) = delete;
    auto operator=(Player&&) noexcept = delete;
    ~Player() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _name; }
    [[nodiscard]] auto GetControlledCritter() noexcept -> nptr<Critter> { return _controlledCr; }
    [[nodiscard]] auto GetControlledCritter() const noexcept -> nptr<const Critter> { return _controlledCr; }
    [[nodiscard]] auto GetSyncWidenEntity() noexcept -> nptr<ServerEntity> override;
    [[nodiscard]] auto GetSyncWidenEntity() const noexcept -> nptr<const ServerEntity> override;
    [[nodiscard]] auto GetConnection() noexcept -> ptr<ServerConnection>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _connection.as_ptr();
    }
    [[nodiscard]] auto GetConnection() const noexcept -> ptr<const ServerConnection>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _connection.as_ptr();
    }
    [[nodiscard]] auto GetViewMap() const noexcept -> nptr<const ViewMapContext> { return _viewMap ? nptr<const ViewMapContext> {&*_viewMap} : nullptr; }
    [[nodiscard]] auto GetViewMapTarget() const noexcept -> nptr<const Map> { return _viewMapTarget; }
    [[nodiscard]] auto GetViewMapTarget() noexcept -> nptr<Map> { return _viewMapTarget; }

    void SetName(string_view name);
    void SetControlledCritter(nptr<Critter> cr);
    void DetachCritter();
    void SwapConnection(ptr<Player> other) noexcept;
    void SetIgnoreSendEntityProperty(nptr<const Entity> entity, nptr<const Property> prop) noexcept;
    void SetViewMap(ptr<Map> map, mpos hex);
    void ResetViewMap() noexcept;

    void Send_LoginSuccess();
    void Send_Moving(ptr<const Critter> from_cr);
    void Send_MovingSpeed(ptr<const Critter> from_cr);
    void Send_Dir(ptr<const Critter> from_cr);
    void Send_AddCritter(ptr<const Critter> cr);
    void Send_RemoveCritter(ptr<const Critter> cr);
    void Send_CritterVisibilityMode(ptr<const Critter> cr, CritterVisibilityMode mode);
    void Send_LoadMap(nptr<const Map> nullable_map);
    void Send_Property(NetProperty type, ptr<const Property> prop, ptr<const Entity> entity);
    void Send_AddItemOnMap(ptr<const Item> item);
    void Send_RemoveItemFromMap(ptr<const Item> item);
    void Send_ChosenAddItem(ptr<const Item> item);
    void Send_ChosenRemoveItem(ptr<const Item> item);
    void Send_Teleport(ptr<const Critter> cr, mpos to_hex);
    void Send_TimeSync();
    void Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text = "");
    void Send_Action(ptr<const Critter> from_cr, CritterAction action, int32_t action_data, nptr<const Item> nullable_context_item);
    void Send_MoveItem(ptr<const Critter> from_cr, nptr<const Item> nullable_moved_item, CritterAction action, CritterItemSlot prev_slot);
    void Send_ViewMap();
    void Send_PlaceToGameComplete();
    void Send_SomeItems(const_span<ptr<const Item>> items, bool owned, bool with_inner_entities, const any_t& context_param);
    void Send_Attachments(ptr<const Critter> from_cr);
    void Send_AddCustomEntity(ptr<CustomEntity> entity, bool owned);
    void Send_RemoveCustomEntity(ident_t id);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGetAccess, int32_t /*arg1*/, string& /*arg2*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnAllowCommand, string /*arg1*/, uint8_t /*arg2*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLogout);

private:
    void SendItem(NetOutBuffer& out_buf, ptr<const Item> item, bool owned, bool with_slot, bool with_inner_entities);
    void SendInnerEntities(NetOutBuffer& out_buf, ptr<const Entity> holder, bool owned);
    void SendCritterMoving(NetOutBuffer& out_buf, ptr<const Critter> cr);

    unique_ptr<ServerConnection> _connection;
    string _name {"(Unlogined)"};
    nptr<Critter> _controlledCr {};
    nptr<const Entity> _sendIgnoreEntity {};
    nptr<const Property> _sendIgnoreProperty {};
    optional<ViewMapContext> _viewMap {};
    nptr<Map> _viewMapTarget {};
    EntityLock _ownedLock {};
};

FO_END_NAMESPACE
