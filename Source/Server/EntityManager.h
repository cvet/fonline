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

class EntityManager final
{
public:
    static constexpr auto ENTITIES_FINALIZATION_FUSE_VALUE = 10000;

    EntityManager() = delete;
    explicit EntityManager(FOServer* engine);
    EntityManager(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = delete;
    auto operator=(const EntityManager&) = delete;
    auto operator=(EntityManager&&) noexcept = delete;
    ~EntityManager() = default;

    [[nodiscard]] auto GetPlayer(ident_t id) -> Player*;
    [[nodiscard]] auto GetPlayers() -> const unordered_map<ident_t, Player*>&;
    [[nodiscard]] auto GetLocation(ident_t id) -> Location*;
    [[nodiscard]] auto GetLocationByPid(hstring pid, uint skip_count) -> Location*;
    [[nodiscard]] auto GetLocations() -> const unordered_map<ident_t, Location*>&;
    [[nodiscard]] auto GetMap(ident_t id) -> Map*;
    [[nodiscard]] auto GetMapByPid(hstring pid, uint skip_count) -> Map*;
    [[nodiscard]] auto GetMaps() -> const unordered_map<ident_t, Map*>&;
    [[nodiscard]] auto GetCritter(ident_t id) -> Critter*;
    [[nodiscard]] auto GetCritters() -> const unordered_map<ident_t, Critter*>&;
    [[nodiscard]] auto GetItem(ident_t id) -> Item*;
    [[nodiscard]] auto GetItems() -> const unordered_map<ident_t, Item*>&;

    void LoadEntities();
    void FinalizeEntities();

    auto LoadLocation(ident_t loc_id, bool& is_error) -> Location*;
    auto LoadMap(ident_t map_id, bool& is_error) -> Map*;
    auto LoadCritter(ident_t cr_id, Player* owner, bool& is_error) -> Critter*;
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

    auto GetCustomEntity(string_view entity_class_name, ident_t id) -> ServerEntity*;
    auto CreateCustomEntity(string_view entity_class_name) -> ServerEntity*;
    void DeleteCustomEntity(string_view entity_class_name, ident_t id);

private:
    auto LoadEntityDoc(string_view collection_name, ident_t id, bool& is_error) const -> tuple<AnyData::Document, hstring>;

    void RegisterEntityEx(ServerEntity* entity);
    void UnregisterEntityEx(ServerEntity* entity, bool delete_from_db);

    FOServer* _engine;
    unordered_map<ident_t, Player*> _allPlayers {};
    unordered_map<ident_t, Location*> _allLocations {};
    unordered_map<ident_t, Map*> _allMaps {};
    unordered_map<ident_t, Critter*> _allCritters {};
    unordered_map<ident_t, Item*> _allItems {};
    unordered_map<string, unordered_map<ident_t, ServerEntity*>> _allCustomEntities {};
    bool _nonConstHelper {};
};
