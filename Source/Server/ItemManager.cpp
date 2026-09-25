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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "ItemManager.h"
#include "CritterManager.h"
#include "EntityManager.h"
#include "EntitySync.h"
#include "MapManager.h"
#include "ProtoManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

ItemManager::ItemManager(ptr<ServerEngine> engine) :
    _engine {engine}
{
}

auto ItemManager::GetItemHolder(ptr<Item> item) -> ptr<Entity>
{
    ValidateEntityAccess(item);

    if (item->GetOwnership() == ItemOwnership::Nowhere) {
        throw ItemManagerException("Item does not have a holder", item->GetId(), item->GetProtoId());
    }

    auto holder = item->GetParent();
    FO_VERIFY_AND_THROW(holder, "Missing required holder");
    ValidateEntityAccess(holder);
    return holder;
}

void ItemManager::RemoveItemHolder(ptr<Item> item, ptr<Entity> holder)
{
    ValidateEntityAccess(holder.dyn_cast<ServerEntity>());
    EnsureEntitySynced(item);

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        if (auto cr = holder.dyn_cast<Critter>()) {
            _engine->CrMngr.RemoveItemFromCritter(cr, item, true);
        }
        else {
            throw ItemManagerException("Item owner (critter inventory) not found");
        }
    } break;
    case ItemOwnership::MapHex: {
        if (auto map = holder.dyn_cast<Map>()) {
            map->RemoveItem(item->GetId());
        }
        else {
            throw ItemManagerException("Item owner (map) not found");
        }
    } break;
    case ItemOwnership::ItemContainer: {
        if (auto cont = holder.dyn_cast<Item>()) {
            cont->RemoveItemFromContainer(item);
        }
        else {
            throw ItemManagerException("Item owner (container) not found");
        }
    } break;
    default:
        break;
    }
}

auto ItemManager::GetMoveSourceHolder(ptr<Item> item) -> nptr<Entity>
{
    FO_VERIFY_AND_THROW(!item->IsDestroyed() && !item->IsDestroying(), "Cannot move an item that is being destroyed", item->GetId());

    // A detached item has no holder to leave, so moving it only places it
    if (item->GetOwnership() == ItemOwnership::Nowhere) {
        return nullptr;
    }

    return GetItemHolder(item);
}

auto ItemManager::CreateItem(hstring pid, nptr<const Properties> props) -> ptr<Item>
{
    FO_TRACE_ZONE(Entity);

    auto proto = _engine->GetProtoItem(pid);

    if (!proto) {
        throw ItemManagerException("Item proto not found", pid);
    }

    auto item = safe_alloc::make_refcounted<Item>(_engine, ident_t {}, proto, props);
    _engine->EntityMngr.RegisterItem(item);

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::Nowhere);

    // Reset ownership properties
    if (props) {
        item->SetMapId({});
        item->SetHex({});
        item->SetCritterId({});
        item->SetCritterSlot({});
        item->SetContainerId({});
        item->SetContainerStack({});
        item->SetInnerItemIds({});
    }

    _engine->EntityMngr.CallInit(item, true);

    if (item->IsDestroyed()) {
        throw ItemManagerException("Item destroyed during init", pid);
    }

    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::Nowhere, "Item is already owned by another holder");

    return item;
}

auto ItemManager::CreateItemOnHex(ptr<Map> map, mpos hex, hstring pid, nptr<const Properties> props) -> ptr<Item>
{
    FO_TRACE_ZONE(Entity);

    auto map_holder = map.hold_ref();
    ignore_unused(map_holder);
    ValidateEntityAccess(map);

    auto item = CreateItem(pid, props);
    auto item_holder = item.hold_ref();
    ignore_unused(item_holder);

    map->AddItem(item, hex, nullptr);

    if (item->IsDestroyed() || map->IsDestroyed()) {
        throw ItemManagerException("Map item add event destroyed the committed entity", pid);
    }
    if (item->GetOwnership() != ItemOwnership::MapHex || item->GetMapId() != map->GetId() || item->GetHex() != hex || map->GetItem(item->GetId()) != item) {
        throw ItemManagerException("Map item add event moved the committed item", pid);
    }

    return item;
}

void ItemManager::DestroyItem(ptr<Item> item)
{
    FO_TRACE_ZONE(Entity);

    auto item_holder = item.hold_ref();
    ignore_unused(item_holder);

    EnsureEntitySynced(item);

    // Redundant calls
    if (item->IsDestroying() || item->IsDestroyed()) {
        return;
    }

    item->MarkAsDestroying();

    // Finish events
    _engine->OnItemFinish.Fire(item);
    FO_VERIFY_AND_THROW(!item->IsDestroyed(), "Item is already destroyed");

    if (item->GetOwnership() != ItemOwnership::Nowhere) {
        auto holder = item->GetParent();
        FO_VERIFY_AND_THROW(holder, "Missing required holder");
        ValidateEntityAccess(holder);
    }

    // Tear off from environment
    for (size_t prev_deps = std::numeric_limits<size_t>::max(); item->GetOwnership() != ItemOwnership::Nowhere || item->HasInnerItems() || item->HasInnerEntities();) {
        try {
            if (item->GetOwnership() != ItemOwnership::Nowhere) {
                RemoveItemHolder(item, GetItemHolder(item));
            }

            while (item->HasInnerItems()) {
                ptr<Item> inner = item->GetAllInnerItems().front();
                // Inner item is covered through the captured container's chain; the recursive
                // DestroyItem takes the inner item's own lock itself as its teardown capture
                ValidateEntityAccess(inner);
                DestroyItem(inner);
            }

            if (item->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(item);
            }
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }

        // Each teardown pass must strictly reduce the item's remaining dependencies; a non-converging
        // loop is corruption, so terminate rather than leave a half-destroyed "undead" item
        size_t remaining_deps = (item->GetOwnership() != ItemOwnership::Nowhere ? 1 : 0) + (item->HasInnerItems() ? item->GetAllInnerItems().size() : 0) + item->GetInnerEntitiesCount();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Item destruction made no progress", item->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }

    _engine->TimeEventMngr.CancelAllForEntity(item);
    item->SetParent(nullptr);
    item->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterItem(item, true);
}

auto ItemManager::CloneItem(ptr<Item> source) -> ptr<Item>
{
    EnsureEntitySynced(source);
    auto source_holder = source.hold_ref();
    ignore_unused(source_holder);

    FO_VERIFY_AND_THROW(!source->IsDestroyed() && !source->IsDestroying(), "Cannot clone an item that is being destroyed", source->GetId());
    FO_VERIFY_AND_THROW(!source->HasInnerItems(), "Cannot clone a container with contents", source->GetId());

    return CreateItem(source->GetProtoId(), source->GetProperties());
}

auto ItemManager::MoveItem(ptr<Item> item, ptr<Critter> to_cr) -> nptr<Item>
{
    FO_TRACE_ZONE(Entity);

    EnsureEntitySynced(item);
    ValidateEntityAccess(to_cr);
    auto item_holder = item.hold_ref();
    auto to_cr_holder = to_cr.hold_ref();
    ignore_unused(item_holder);
    ignore_unused(to_cr_holder);

    if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritterId() == to_cr->GetId()) {
        return item;
    }

    auto holder = GetMoveSourceHolder(item);
    auto holder_holder = holder.try_hold_ref();
    ignore_unused(holder_holder);

    if (holder) {
        RemoveItemHolder(item, holder);
    }

    // Removal events re-enter scripts, and an item they have already placed somewhere else counts as moved
    if (item->IsDestroyed() || item->GetOwnership() != ItemOwnership::Nowhere || to_cr->IsDestroyed() || to_cr->IsDestroying()) {
        return nullptr;
    }

    return _engine->CrMngr.AddItemToCritter(to_cr, item, true);
}

auto ItemManager::MoveItem(ptr<Item> item, ptr<Map> to_map, mpos to_hex) -> nptr<Item>
{
    FO_TRACE_ZONE(Entity);

    EnsureEntitySynced(item);
    ValidateEntityAccess(to_map);
    auto item_holder = item.hold_ref();
    auto to_map_holder = to_map.hold_ref();
    ignore_unused(item_holder);
    ignore_unused(to_map_holder);

    if (item->GetOwnership() == ItemOwnership::MapHex && item->GetMapId() == to_map->GetId() && item->GetHex() == to_hex) {
        return item;
    }

    auto holder = GetMoveSourceHolder(item);
    auto holder_holder = holder.try_hold_ref();
    ignore_unused(holder_holder);
    auto dropper = holder.dyn_cast<Critter>();
    auto dropper_holder = dropper.try_hold_ref();
    ignore_unused(dropper_holder);

    if (holder) {
        RemoveItemHolder(item, holder);
    }

    // Removal events re-enter scripts, and an item they have already placed somewhere else counts as moved
    if (item->IsDestroyed() || item->GetOwnership() != ItemOwnership::Nowhere || to_map->IsDestroyed() || to_map->IsDestroying()) {
        return nullptr;
    }

    to_map->AddItem(item, to_hex, dropper);

    if (item->IsDestroyed() || to_map->IsDestroyed()) {
        return nullptr;
    }
    if (item->GetOwnership() != ItemOwnership::MapHex || item->GetMapId() != to_map->GetId() || to_map->GetItem(item->GetId()) != item) {
        return nullptr;
    }

    return item;
}

auto ItemManager::MoveItem(ptr<Item> item, ptr<Item> to_cont, const any_t& stack_id) -> nptr<Item>
{
    FO_TRACE_ZONE(Entity);

    EnsureEntitySynced(item);
    ValidateEntityAccess(to_cont);
    auto item_holder = item.hold_ref();
    auto to_cont_holder = to_cont.hold_ref();
    ignore_unused(item_holder);
    ignore_unused(to_cont_holder);

    if (item->GetOwnership() == ItemOwnership::ItemContainer && item->GetContainerId() == to_cont->GetId() && item->GetContainerStack() == stack_id) {
        return item;
    }

    auto holder = GetMoveSourceHolder(item);
    auto holder_holder = holder.try_hold_ref();
    ignore_unused(holder_holder);

    if (holder) {
        RemoveItemHolder(item, holder);
    }

    // Removal events re-enter scripts, and an item they have already placed somewhere else counts as moved
    if (item->IsDestroyed() || item->GetOwnership() != ItemOwnership::Nowhere || to_cont->IsDestroyed() || to_cont->IsDestroying()) {
        return nullptr;
    }

    return to_cont->AddItemToContainer(item, stack_id);
}

FO_END_NAMESPACE
