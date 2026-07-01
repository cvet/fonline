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

#include "Critter.h"
#include "DataBase.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "Player.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EntityManagerException);

class EntityManager final
{
public:
    EntityManager() = delete;
    explicit EntityManager(ptr<ServerEngine> engine);
    EntityManager(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = delete;
    auto operator=(const EntityManager&) = delete;
    auto operator=(EntityManager&&) noexcept = delete;
    ~EntityManager() = default;

    [[nodiscard]] auto GetEntity(ident_t id) const noexcept -> refcount_nptr<const ServerEntity>;
    [[nodiscard]] auto GetEntity(ident_t id) noexcept -> refcount_nptr<ServerEntity>;
    [[nodiscard]] auto GetEntities() noexcept -> vector<refcount_ptr<ServerEntity>>;
    [[nodiscard]] auto GetEntitiesCount() const noexcept -> size_t;
    [[nodiscard]] auto GetPlayer(ident_t id) const noexcept -> refcount_nptr<const Player>;
    [[nodiscard]] auto GetPlayer(ident_t id) noexcept -> refcount_nptr<Player>;
    [[nodiscard]] auto GetPlayers() noexcept -> vector<refcount_ptr<Player>>;
    [[nodiscard]] auto GetPlayersCount() const noexcept -> size_t;
    [[nodiscard]] auto GetLocation(ident_t id) const noexcept -> refcount_nptr<const Location>;
    [[nodiscard]] auto GetLocation(ident_t id) noexcept -> refcount_nptr<Location>;
    [[nodiscard]] auto GetLocations() noexcept -> vector<refcount_ptr<Location>>;
    [[nodiscard]] auto GetLocationsCount() const noexcept -> size_t;
    [[nodiscard]] auto GetMap(ident_t id) const noexcept -> refcount_nptr<const Map>;
    [[nodiscard]] auto GetMap(ident_t id) noexcept -> refcount_nptr<Map>;
    [[nodiscard]] auto GetMaps() noexcept -> vector<refcount_ptr<Map>>;
    [[nodiscard]] auto GetMapsCount() const noexcept -> size_t;
    [[nodiscard]] auto GetCritter(ident_t id) const noexcept -> refcount_nptr<const Critter>;
    [[nodiscard]] auto GetCritter(ident_t id) noexcept -> refcount_nptr<Critter>;
    [[nodiscard]] auto GetCritters() noexcept -> vector<refcount_ptr<Critter>>;
    [[nodiscard]] auto GetCrittersCount() const noexcept -> size_t;
    [[nodiscard]] auto GetItem(ident_t id) const noexcept -> refcount_nptr<const Item>;
    [[nodiscard]] auto GetItem(ident_t id) noexcept -> refcount_nptr<Item>;
    [[nodiscard]] auto GetItems() noexcept -> vector<refcount_ptr<Item>>;
    [[nodiscard]] auto GetItemsCount() const noexcept -> size_t;

    template<typename T>
    [[nodiscard]] auto Get(ident_t id) noexcept -> refcount_nptr<T>
    {
        static_assert(std::is_base_of_v<ServerEntity, T>);
        shared_lock lock {_registryLock};
        const auto it = _allEntities.find(id);

        if (it == _allEntities.end()) {
            return nullptr;
        }

        return it->second.template dyn_cast<T>();
    }

    template<typename T>
    [[nodiscard]] auto Get(ident_t id) const noexcept -> refcount_nptr<const T>
    {
        static_assert(std::is_base_of_v<ServerEntity, T>);
        shared_lock lock {_registryLock};
        const auto it = _allEntities.find(id);

        if (it == _allEntities.end()) {
            return nullptr;
        }

        return it->second.template dyn_cast<const T>();
    }

    void LoadEntities();
    auto LoadLocation(ident_t loc_id, bool& is_error) noexcept -> refcount_nptr<Location>;
    auto LoadMap(ident_t map_id, bool& is_error) noexcept -> refcount_nptr<Map>;
    auto LoadCritter(ident_t cr_id, bool& is_error) noexcept -> refcount_nptr<Critter>;
    auto LoadItem(ident_t item_id, bool& is_error) noexcept -> refcount_nptr<Item>;

    void CallInit(ptr<Location> loc, bool first_time);
    void CallInit(ptr<Map> map, bool first_time);
    void CallInit(ptr<Critter> cr, bool first_time);
    void CallInit(ptr<Item> item, bool first_time);

    void RegisterPlayer(ptr<Player> player, ident_t id, bool persistent = true);
    void UnregisterPlayer(ptr<Player> player);
    void RegisterLocation(ptr<Location> loc);
    void UnregisterLocation(ptr<Location> loc);
    void RegisterMap(ptr<Map> map);
    void UnregisterMap(ptr<Map> map);
    void RegisterCritter(ptr<Critter> cr);
    void UnregisterCritter(ptr<Critter> cr);
    void RegisterItem(ptr<Item> item);
    void UnregisterItem(ptr<Item> item, bool delete_from_db);
    void RegisterCustomEntity(ptr<CustomEntity> custom_entity);
    void UnregisterCustomEntity(ptr<CustomEntity> custom_entity, bool delete_from_db);
    void MakePersistent(ptr<ServerEntity> entity, bool persistent, bool explicitly_requested = false);

    auto CreateCustomInnerEntity(ptr<Entity> holder, hstring entry, hstring pid) -> ptr<CustomEntity>;
    auto CreateCustomEntity(hstring type_name, hstring pid) -> ptr<CustomEntity>;
    auto LoadCustomEntity(hstring type_name, ident_t id, bool& is_error) noexcept -> refcount_nptr<CustomEntity>;
    auto GetCustomEntity(hstring type_name, ident_t id) -> refcount_nptr<CustomEntity>;
    void DestroyCustomEntity(ptr<CustomEntity> entity);
    void ForEachCustomEntityView(ptr<CustomEntity> entity, const function<void(ptr<Player> player, bool owner)>& callback);

    void DestroyEntity(ptr<Entity> entity);
    void DestroyInnerEntities(ptr<Entity> holder);
    void DestroyAllEntities();
    void FlushExactEntityId();

private:
    void MakePersistentRecursive(ptr<ServerEntity> entity, unordered_set<ptr<ServerEntity>>& processed);
    void MakeNonPersistentRecursive(ptr<ServerEntity> entity, unordered_set<ptr<ServerEntity>>& processed);
    void EnsureCascadeNodeSynced(ptr<ServerEntity> entity);
    void ForEachPersistentChildEntity(ptr<ServerEntity> entity, const function<void(ptr<ServerEntity> child)>& callback) const;

    void LoadInnerEntities(ptr<Entity> holder, bool& is_error) noexcept;
    void LoadInnerEntitiesEntry(ptr<Entity> holder, hstring entry, bool& is_error) noexcept;
    auto LoadEntityDoc(hstring type_name, hstring collection_name, ident_t id, bool expect_proto, bool& is_error) const noexcept -> tuple<AnyData::Document, hstring>;
    auto StoreEntityDoc(ptr<ServerEntity> entity) -> AnyData::Document;

    void RegisterEntity(ptr<ServerEntity> entity) FO_TSA_REQUIRES(_registryLock);
    void UnregisterEntity(ptr<ServerEntity> entity, bool delete_from_db) FO_TSA_REQUIRES(_registryLock);

    ptr<ServerEngine> _engine;

    mutable shared_mutex _registryLock {};
    unordered_map<ident_t, ptr<Player>> _allPlayers FO_TSA_GUARDED_BY(_registryLock) {};
    unordered_map<ident_t, ptr<Location>> _allLocations FO_TSA_GUARDED_BY(_registryLock) {};
    unordered_map<ident_t, ptr<Map>> _allMaps FO_TSA_GUARDED_BY(_registryLock) {};
    unordered_map<ident_t, ptr<Critter>> _allCritters FO_TSA_GUARDED_BY(_registryLock) {};
    unordered_map<ident_t, ptr<Item>> _allItems FO_TSA_GUARDED_BY(_registryLock) {};
    unordered_map<hstring, unordered_map<ident_t, ptr<CustomEntity>>> _allCustomEntities FO_TSA_GUARDED_BY(_registryLock) {};
    unordered_map<ident_t, refcount_ptr<ServerEntity>> _allEntities FO_TSA_GUARDED_BY(_registryLock) {};

    int64_t _lastEntityId {};
    int64_t _persistedEntityId {};

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
    const hstring _protoMigrationRuleName {};
    const hstring _removeMigrationReplacement {};
};

FO_END_NAMESPACE
