// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Critter.h"
#include "Geometry.h"
#include "Item.h"
#include "Map.h"
#include "Player.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

class ServerEngine;
class ProtoManager;
class EntityManager;
class MapManager;
class ItemManager;

FO_DECLARE_EXCEPTION(CritterManagerException);

class CritterManager final
{
public:
    CritterManager() = delete;
    explicit CritterManager(ptr<ServerEngine> engine);
    CritterManager(const CritterManager&) = delete;
    CritterManager(CritterManager&&) noexcept = delete;
    auto operator=(const CritterManager&) = delete;
    auto operator=(CritterManager&&) noexcept = delete;
    ~CritterManager() = default;

    [[nodiscard]] auto GetNonPlayerCritters() -> vector<refcount_ptr<Critter>>;
    [[nodiscard]] auto GetPlayerCritters() -> vector<refcount_ptr<Critter>>;

    auto CreateCritterOnMap(hstring proto_id, nptr<const Properties> props, ptr<Map> map, mpos hex, mdir dir) -> ptr<Critter>;
    void DestroyCritter(ptr<Critter> cr);
    void DestroyInventory(ptr<Critter> cr);
    auto AddItemToCritter(ptr<Critter> cr, ptr<Item> item, bool send) -> ptr<Item>;
    void RemoveItemFromCritter(ptr<Critter> cr, ptr<Item> item, bool send);

private:
    ptr<ServerEngine> _engine;
};

FO_END_NAMESPACE
