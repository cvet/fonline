// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Critter.h"
#include "Geometry.h"
#include "Item.h"
#include "Map.h"

FO_BEGIN_NAMESPACE

class ServerEngine;
class ProtoManager;
class EntityManager;
class MapManager;
class CritterManager;

FO_DECLARE_EXCEPTION(ItemManagerException);

class ItemManager final
{
public:
    ItemManager() = delete;
    explicit ItemManager(ptr<ServerEngine> engine);
    ItemManager(const ItemManager&) = delete;
    ItemManager(ItemManager&&) noexcept = delete;
    auto operator=(const ItemManager&) = delete;
    auto operator=(ItemManager&&) noexcept = delete;
    ~ItemManager() = default;

    auto CreateItem(hstring pid, nptr<const Properties> props) -> ptr<Item>;
    auto CreateItemOnHex(ptr<Map> map, mpos hex, hstring pid, nptr<const Properties> props) -> ptr<Item>;
    auto CloneItem(ptr<Item> source) -> ptr<Item>;
    void DestroyItem(ptr<Item> item);
    auto MoveItem(ptr<Item> item, ptr<Critter> to_cr) -> nptr<Item>;
    auto MoveItem(ptr<Item> item, ptr<Map> to_map, mpos to_hex) -> nptr<Item>;
    auto MoveItem(ptr<Item> item, ptr<Item> to_cont, const any_t& stack_id) -> nptr<Item>;

private:
    auto GetItemHolder(ptr<Item> item) -> ptr<Entity>;
    void RemoveItemHolder(ptr<Item> item, ptr<Entity> holder);
    auto GetMoveSourceHolder(ptr<Item> item) -> nptr<Entity>;

    ptr<ServerEngine> _engine;
};

FO_END_NAMESPACE
