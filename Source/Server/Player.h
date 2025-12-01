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

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "Geometry.h"
#include "ServerConnection.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE();

class Item;
class Critter;
class Map;
class Location;
class MapManager;

class Player final : public ServerEntity, public PlayerProperties
{
public:
    Player() = delete;
    Player(FOServer* engine, ident_t id, unique_ptr<ServerConnection> connection, const Properties* props = nullptr) noexcept;
    Player(const Player&) = delete;
    Player(Player&&) noexcept = delete;
    auto operator=(const Player&) = delete;
    auto operator=(Player&&) noexcept = delete;
    ~Player() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _name; }
    [[nodiscard]] auto GetControlledCritter() noexcept -> Critter* { return _controlledCr.get(); }
    [[nodiscard]] auto GetControlledCritter() const noexcept -> const Critter* { return _controlledCr.get(); }
    [[nodiscard]] auto GetConnection() noexcept -> ServerConnection* { return _connection.get(); }
    [[nodiscard]] auto GetConnection() const noexcept -> const ServerConnection* { return _connection.get(); }

    void SetName(string_view name);
    void SetControlledCritter(Critter* cr);
    void SwapConnection(Player* other) noexcept;
    void SetIgnoreSendEntityProperty(const Entity* entity, const Property* prop) noexcept;

    void Send_LoginSuccess();
    void Send_Moving(const Critter* from_cr);
    void Send_MovingSpeed(const Critter* from_cr);
    void Send_Dir(const Critter* from_cr);
    void Send_AddCritter(const Critter* cr);
    void Send_RemoveCritter(const Critter* cr);
    void Send_LoadMap(const Map* map);
    void Send_Property(NetProperty type, const Property* prop, const Entity* entity);
    void Send_AddItemOnMap(const Item* item);
    void Send_RemoveItemFromMap(const Item* item);
    void Send_ChosenAddItem(const Item* item);
    void Send_ChosenRemoveItem(const Item* item);
    void Send_Teleport(const Critter* cr, mpos to_hex);
    void Send_TimeSync();
    void Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text = "");
    void Send_Action(const Critter* from_cr, CritterAction action, int32 action_data, const Item* context_item);
    void Send_MoveItem(const Critter* from_cr, const Item* moved_item, CritterAction action, CritterItemSlot prev_slot);
    void Send_ViewMap();
    void Send_PlaceToGameComplete();
    void Send_SomeItems(const vector<Item*>& items, bool owned, bool with_inner_entities, const any_t& context_param);
    void Send_Attachments(const Critter* from_cr);
    void Send_AddCustomEntity(CustomEntity* entity, bool owned);
    void Send_RemoveCustomEntity(ident_t id);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGetAccess, int32 /*arg1*/, string& /*arg2*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnAllowCommand, string /*arg1*/, uint8 /*arg2*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLogout);

private:
    void SendItem(NetOutBuffer& out_buf, const Item* item, bool owned, bool with_slot, bool with_inner_entities);
    void SendInnerEntities(NetOutBuffer& out_buf, const Entity* holder, bool owned);
    void SendCritterMoving(NetOutBuffer& out_buf, const Critter* cr);

    unique_ptr<ServerConnection> _connection;
    string _name {"(Unlogined)"};
    raw_ptr<Critter> _controlledCr {}; // Todo: allow attach many critters to sigle player
    raw_ptr<const Entity> _sendIgnoreEntity {};
    raw_ptr<const Property> _sendIgnoreProperty {};
};

FO_END_NAMESPACE();
