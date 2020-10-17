//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Critter.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "Item.h"
#include "Map.h"
#include "ServerScripting.h"
#include "Settings.h"
#include "Timer.h"

class ProtoManager;
class EntityManager;
class MapManager;
class ItemManager;

class CritterManager final
{
public:
    CritterManager() = delete;
    CritterManager(ServerSettings& settings, ProtoManager& proto_mngr, EntityManager& entity_mngr, MapManager& map_mngr, ItemManager& item_mngr, ServerScriptSystem& script_sys, GameTimer& game_time);
    CritterManager(const CritterManager&) = delete;
    CritterManager(CritterManager&&) noexcept = delete;
    auto operator=(const CritterManager&) = delete;
    auto operator=(CritterManager&&) noexcept = delete;
    ~CritterManager() = default;

    [[nodiscard]] auto GetCritters() -> vector<Critter*>;
    [[nodiscard]] auto GetNpcs() -> vector<Npc*>;
    [[nodiscard]] auto GetClients(bool on_global_map_only) -> vector<Client*>;
    [[nodiscard]] auto GetGlobalMapCritters(ushort wx, ushort wy, uint radius, uchar find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritter(uint cr_id) -> Critter*;
    [[nodiscard]] auto GetCritter(uint cr_id) const -> const Critter*;
    [[nodiscard]] auto GetPlayer(uint cr_id) -> Client*;
    [[nodiscard]] auto GetPlayer(const char* name) -> Client*;
    [[nodiscard]] auto GetNpc(uint cr_id) -> Npc*;
    [[nodiscard]] auto GetItemByPidInvPriority(Critter* cr, hash item_pid) -> Item*;
    [[nodiscard]] auto PlayersInGame() const -> uint;
    [[nodiscard]] auto NpcInGame() const -> uint;
    [[nodiscard]] auto CrittersInGame() const -> uint;

    [[nodiscard]] auto CreateNpc(hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy) -> Npc*;

    void LinkCritters();
    void InitAfterLoad();
    void DeleteNpc(Critter* cr);
    void DeleteInventory(Critter* cr);
    void AddItemToCritter(Critter* cr, Item*& item, bool send);
    void EraseItemFromCritter(Critter* cr, Item* item, bool send);
    void ProcessTalk(Client* cl, bool force);
    void CloseTalk(Client* cl);

private:
    ServerSettings& _settings;
    GeometryHelper _geomHelper;
    ProtoManager& _protoMngr;
    EntityManager& _entityMngr;
    MapManager& _mapMngr;
    ItemManager& _itemMngr;
    ServerScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    int _dummy {};
};
