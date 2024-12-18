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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

class EntityManager final
{
public:
    EntityManager() = delete;
    explicit EntityManager(FOServer* engine);
    EntityManager(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = delete;
    auto operator=(const EntityManager&) = delete;
    auto operator=(EntityManager&&) noexcept = delete;
    ~EntityManager() = default;

    [[nodiscard]] auto GetEntity(ident_t id) noexcept -> ServerEntity*;
    [[nodiscard]] auto GetEntities() noexcept -> const unordered_map<ident_t, ServerEntity*>& { return _allEntities; }
    [[nodiscard]] auto GetEntitiesCount() const noexcept -> size_t { return _allEntities.size(); }
    [[nodiscard]] auto GetPlayer(ident_t id) noexcept -> Player*;
    [[nodiscard]] auto GetPlayers() noexcept -> const unordered_map<ident_t, Player*>& { return _allPlayers; }
    [[nodiscard]] auto GetPlayersCount() const noexcept -> size_t { return _allPlayers.size(); }
    [[nodiscard]] auto GetLocation(ident_t id) noexcept -> Location*;
    [[nodiscard]] auto GetLocations() noexcept -> const unordered_map<ident_t, Location*>& { return _allLocations; }
    [[nodiscard]] auto GetLocationsCount() const noexcept -> size_t { return _allLocations.size(); }
    [[nodiscard]] auto GetMap(ident_t id) noexcept -> Map*;
    [[nodiscard]] auto GetMaps() noexcept -> const unordered_map<ident_t, Map*>& { return _allMaps; }
    [[nodiscard]] auto GetMapsCount() const noexcept -> size_t { return _allMaps.size(); }
    [[nodiscard]] auto GetCritter(ident_t id) noexcept -> Critter*;
    [[nodiscard]] auto GetCritters() noexcept -> const unordered_map<ident_t, Critter*>& { return _allCritters; }
    [[nodiscard]] auto GetCrittersCount() const noexcept -> size_t { return _allCritters.size(); }
    [[nodiscard]] auto GetItem(ident_t id) noexcept -> Item*;
    [[nodiscard]] auto GetItems() noexcept -> const unordered_map<ident_t, Item*>& { return _allItems; }
    [[nodiscard]] auto GetItemsCount() const noexcept -> size_t { return _allEntities.size(); }

    template<typename T>
    [[nodiscard]] auto Get(ident_t id) noexcept -> T*
    {
        static_assert(std::is_base_of_v<ServerEntity, T>);
        const auto it = _allEntities.find(id);
        return it != _allEntities.end() ? dynamic_cast<T*>(it->second) : nullptr;
    }

    template<typename T>
    [[nodiscard]] auto Get(ident_t id) const noexcept -> const T*
    {
        static_assert(std::is_base_of_v<ServerEntity, T>);
        const auto it = _allEntities.find(id);
        return it != _allEntities.end() ? dynamic_cast<const T*>(it->second) : nullptr;
    }

    void LoadEntities();
    auto LoadLocation(ident_t loc_id, bool& is_error) -> Location*;
    auto LoadMap(ident_t map_id, bool& is_error) -> Map*;
    auto LoadCritter(ident_t cr_id, bool& is_error) -> Critter*;
    auto LoadItem(ident_t item_id, bool& is_error) -> Item*;

    void CallInit(Location* loc, bool first_time);
    void CallInit(Map* map, bool first_time);
    void CallInit(Critter* cr, bool first_time);
    void CallInit(Item* item, bool first_time);

    void RegisterEntity(Player* entity, ident_t id);
    void UnregisterEntity(Player* entity);
    void RegisterEntity(Location* entity);
    void UnregisterEntity(Location* entity);
    void RegisterEntity(Map* entity);
    void UnregisterEntity(Map* entity);
    void RegisterEntity(Critter* entity);
    void UnregisterEntity(Critter* entity);
    void RegisterEntity(Item* entity);
    void UnregisterEntity(Item* entity, bool delete_from_db);
    void RegisterEntity(CustomEntity* entity);
    void UnregisterEntity(CustomEntity* entity, bool delete_from_db);

    auto CreateCustomInnerEntity(Entity* holder, hstring entry, hstring pid) -> CustomEntity*;
    auto CreateCustomEntity(hstring type_name, hstring pid) -> CustomEntity*;
    auto LoadCustomEntity(ident_t id, bool& is_error) -> CustomEntity*;
    auto GetCustomEntity(hstring type_name, ident_t id) -> CustomEntity*;
    void DestroyCustomEntity(CustomEntity* entity);
    void ForEachCustomEntityView(CustomEntity* entity, const std::function<void(Player* player, bool owner)>& callback);

    void DestroyEntity(Entity* entity);
    void DestroyInnerEntities(Entity* holder);
    void DestroyAllEntities();

private:
    void LoadInnerEntities(Entity* holder, bool& is_error);
    auto LoadEntityDoc(hstring type_name, hstring collection_name, ident_t id, bool expect_proto, bool& is_error) const -> tuple<AnyData::Document, hstring>;

    void RegisterEntityEx(ServerEntity* entity);
    void UnregisterEntityEx(ServerEntity* entity, bool delete_from_db);

    FOServer* _engine;

    unordered_map<ident_t, Player*> _allPlayers {};
    unordered_map<ident_t, Location*> _allLocations {};
    unordered_map<ident_t, Map*> _allMaps {};
    unordered_map<ident_t, Critter*> _allCritters {};
    unordered_map<ident_t, Item*> _allItems {};
    unordered_map<hstring, unordered_map<ident_t, CustomEntity*>> _allCustomEntities {};
    unordered_map<ident_t, ServerEntity*> _allEntities {};

    const hstring _entityTypeMapCollection {};
    const hstring _playerTypeName {};
    const hstring _locationTypeName {};
    const hstring _mapTypeName {};
    const hstring _critterTypeName {};
    const hstring _itemTypeName {};
    const hstring _playerCollectionName {};
    const hstring _locationCollectionName {};
    const hstring _mapCollectionName {};
    const hstring _critterCollectionName {};
    const hstring _itemCollectionName {};
    const hstring _removeMigrationRuleName {};

    bool _nonConstHelper {};
};
