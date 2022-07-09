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

#pragma once

#include "Common.h"

#include "Critter.h"
#include "DataBase.h"
#include "Entity.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "Player.h"

DECLARE_EXCEPTION(EntitiesLoadException);

class ProtoManager;

class EntityManager final
{
public:
    using LocationFabric = std::function<Location*(uint, const ProtoLocation*)>;
    using MapFabric = std::function<Map*(uint, const ProtoMap*)>;
    using NpcFabric = std::function<Critter*(uint, const ProtoCritter*)>;
    using ItemFabric = std::function<Item*(uint, const ProtoItem*)>;

    static constexpr auto ENTITIES_FINALIZATION_FUSE_VALUE = 10000;

    EntityManager() = delete;
    explicit EntityManager(FOServer* engine);
    EntityManager(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = delete;
    auto operator=(const EntityManager&) = delete;
    auto operator=(EntityManager&&) noexcept = delete;
    ~EntityManager() = default;

    [[nodiscard]] auto FindCritterItems(uint cr_id) -> vector<Item*>;
    [[nodiscard]] auto GetEntity(uint id) -> ServerEntity*;
    [[nodiscard]] auto GetEntities() -> vector<ServerEntity*>;
    [[nodiscard]] auto GetPlayer(uint id) -> Player*;
    [[nodiscard]] auto GetPlayers() -> vector<Player*>;
    [[nodiscard]] auto GetPlayers() const -> vector<const Player*>;
    [[nodiscard]] auto GetItem(uint id) -> Item*;
    [[nodiscard]] auto GetItems() -> vector<Item*>;
    [[nodiscard]] auto GetCritter(uint id) -> Critter*;
    [[nodiscard]] auto GetCritters() -> vector<Critter*>;
    [[nodiscard]] auto GetMap(uint id) -> Map*;
    [[nodiscard]] auto GetMapByPid(hstring pid, uint skip_count) -> Map*;
    [[nodiscard]] auto GetMaps() -> vector<Map*>;
    [[nodiscard]] auto GetLocation(uint id) -> Location*;
    [[nodiscard]] auto GetLocationByPid(hstring pid, uint skip_count) -> Location*;
    [[nodiscard]] auto GetLocations() -> vector<Location*>;
    [[nodiscard]] auto GetCustomEntity(string_view entity_class_name, uint id) -> ServerEntity*;

    void LoadEntities(const LocationFabric& loc_fabric, const MapFabric& map_fabric, const NpcFabric& npc_fabric, const ItemFabric& item_fabric);
    void InitAfterLoad();
    void RegisterEntity(ServerEntity* entity);
    void UnregisterEntity(ServerEntity* entity);
    void FinalizeEntities();
    auto CreateCustomEntity(string_view entity_class_name) -> ServerEntity*;
    void DeleteCustomEntity(string_view entity_class_name, uint id);

private:
    FOServer* _engine;
    unordered_map<uint, ServerEntity*> _allEntities {}; // Todo: optimize entities lookup
    bool _nonConstHelper {};
};
