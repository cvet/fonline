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

#include "ItemManager.h"
#include "CritterManager.h"
#include "EntityManager.h"
#include "EntitySync.h"
#include "MapManager.h"
#include "ProtoManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

ItemManager::ItemManager(ServerEngine* engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto ItemManager::GetItemHolder(Item* item) -> Entity*
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(item);

    if (item->GetOwnership() == ItemOwnership::Nowhere) {
        throw ItemManagerException("Item does not have a holder", item->GetId(), item->GetProtoId());
    }

    auto holder = item->GetParent();
    FO_VERIFY_AND_THROW(holder, "Missing required holder");
    ValidateEntityAccess(holder.get());

    return holder.get();
}

void ItemManager::RemoveItemHolder(Item* item, Entity* holder)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(item, "Missing item instance");
    FO_VERIFY_AND_THROW(holder, "Missing required holder");

    EnsureEntitySynced(dynamic_cast<ServerEntity*>(holder));
    EnsureEntitySynced(item);

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        if (auto* cr = dynamic_cast<Critter*>(holder); cr != nullptr) {
            _engine->CrMngr.RemoveItemFromCritter(cr, item, true);
        }
        else {
            throw ItemManagerException("Item owner (critter inventory) not found");
        }
    } break;
    case ItemOwnership::MapHex: {
        if (auto* map = dynamic_cast<Map*>(holder); map != nullptr) {
            map->RemoveItem(item->GetId());
        }
        else {
            throw ItemManagerException("Item owner (map) not found");
        }
    } break;
    case ItemOwnership::ItemContainer: {
        if (auto* cont = dynamic_cast<Item*>(holder); cont != nullptr) {
            cont->RemoveItemFromContainer(item);
        }
        else {
            throw ItemManagerException("Item owner (container) not found");
        }
    } break;
    default:
        break;
    }

    item->SetOwnership(ItemOwnership::Nowhere);
}

auto ItemManager::CreateItem(hstring pid, int32_t count, const Properties* props) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);

    const auto* proto = _engine->GetProtoItem(pid);

    if (proto == nullptr) {
        throw ItemManagerException("Item proto not found", pid);
    }

    auto item = SafeAlloc::MakeRefCounted<Item>(_engine.get(), ident_t {}, proto, props);
    _engine->EntityMngr.RegisterItem(item.get());

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::Nowhere);

    // Reset ownership properties
    if (props != nullptr) {
        item->SetMapId({});
        item->SetHex({});
        item->SetCritterId({});
        item->SetCritterSlot({});
        item->SetContainerId({});
        item->SetContainerStack({});
        item->SetInnerItemIds({});
    }

    if (count != 0 && item->GetStackable()) {
        item->SetCount(count);
    }
    else {
        item->SetCount(1);
    }

    _engine->EntityMngr.CallInit(item.get(), true);

    if (item->IsDestroyed()) {
        throw ItemManagerException("Item destroyed during init", pid);
    }

    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::Nowhere, "Item is already owned by another holder");

    return item.get();
}

auto ItemManager::CreateItemOnHex(Map* map, mpos hex, hstring pid, int32_t count, Properties* props) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(map, "Missing map instance");
    refcount_ptr map_holder = map;
    ignore_unused(map_holder);

    if (count <= 0) {
        throw ItemManagerException("Invalid items cound");
    }

    const auto* proto = _engine->GetProtoItem(pid);

    if (proto == nullptr) {
        throw ItemManagerException("Item proto not found", pid);
    }

    const auto add_item = [&]() -> Item* {
        auto* item = CreateItem(pid, proto->GetStackable() ? count : 1, props);
        refcount_ptr item_holder = item;
        ignore_unused(item_holder);

        map->AddItem(item, hex, nullptr);

        if (item->IsDestroyed() || map->IsDestroyed()) {
            throw ItemManagerException("Map item add event destroyed the committed entity", pid);
        }
        if (item->GetOwnership() != ItemOwnership::MapHex || item->GetMapId() != map->GetId() || item->GetHex() != hex || map->GetItem(item->GetId()) != item) {
            throw ItemManagerException("Map item add event moved the committed item", pid);
        }

        return item;
    };

    auto* item = add_item();

    // Non-stacked items
    if (item != nullptr && !proto->GetStackable() && count > 1) {
        const auto fixed_count = std::min(count, _engine->Settings.MaxAddUnstackableItems);

        for (int32_t i = 0; i < fixed_count; i++) {
            if (add_item() == nullptr) {
                break;
            }
        }
    }

    return item;
}

void ItemManager::DestroyItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    EnsureEntitySynced(item);

    // Redundant calls
    if (item->IsDestroying() || item->IsDestroyed()) {
        return;
    }

    item->MarkAsDestroying();

    // Finish events
    _engine->OnItemFinish.Fire(item);
    FO_VERIFY_AND_THROW(!item->IsDestroyed(), "Item is already destroyed");

    // Tear off from environment
    for (size_t prev_deps = std::numeric_limits<size_t>::max(); item->GetOwnership() != ItemOwnership::Nowhere || item->HasInnerItems() || item->HasInnerEntities();) {
        try {
            if (item->GetOwnership() != ItemOwnership::Nowhere) {
                RemoveItemHolder(item, GetItemHolder(item));
            }

            while (item->HasInnerItems()) {
                auto* inner = item->GetAllInnerItems().front();
                EnsureEntitySynced(inner);
                DestroyItem(inner);
            }

            if (item->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(item);
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }

        // Each teardown pass must strictly reduce the item's remaining dependencies; a non-converging
        // loop is corruption, so terminate rather than leave a half-destroyed "undead" item.
        const size_t remaining_deps = (item->GetOwnership() != ItemOwnership::Nowhere ? 1 : 0) + (item->HasInnerItems() ? item->GetAllInnerItems().size() : 0) + item->GetInnerEntitiesCount();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Item destruction made no progress", item->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }

    _engine->TimeEventMngr.CancelAllForEntity(item);
    item->SetParent(nullptr);
    item->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterItem(item, true);
}

auto ItemManager::SplitItem(Item* item, int32_t count) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(item);

    FO_VERIFY_AND_THROW(item->GetStackable(), "Item is not stackable");
    FO_VERIFY_AND_THROW(count > 0, "Count must be positive", count);
    FO_VERIFY_AND_THROW(count < item->GetCount(), "Count is outside allowed range", count, item->GetCount());

    auto* new_item = CreateItem(item->GetProtoId(), count, &item->GetProperties());

    refcount_ptr new_item_holder = new_item;
    ignore_unused(new_item_holder);

    if (item->IsDestroyed() || item->IsDestroying() || count >= item->GetCount()) {
        DestroyItem(new_item);
        return nullptr;
    }

    const int32_t fresh_count = item->GetCount();
    item->SetCount(fresh_count - count);
    FO_VERIFY_AND_THROW(!new_item->IsDestroyed(), "Newly split item is already destroyed");

    return new_item;
}

void ItemManager::RestoreSplitItem(Item* item, Item* splitted_item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(item, "Missing item instance");
    FO_VERIFY_AND_THROW(splitted_item, "Missing required splitted item");

    if (!item->IsDestroyed() && item->GetProtoId() == splitted_item->GetProtoId()) {
        item->SetCount(item->GetCount() + splitted_item->GetCount());
    }

    DestroyItem(splitted_item);
}

auto ItemManager::MoveItem(Item* item, int32_t count, Critter* to_cr) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    EnsureEntitySynced(item);
    ValidateEntityAccess(to_cr);
    refcount_ptr item_holder = item;
    refcount_ptr to_cr_holder = to_cr;
    ignore_unused(item_holder);
    ignore_unused(to_cr_holder);

    if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritterId() == to_cr->GetId()) {
        return item;
    }

    auto* holder = GetItemHolder(item);
    refcount_ptr holder_holder = holder;
    ignore_unused(holder_holder);

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);

        if (item->IsDestroyed() || to_cr->IsDestroyed() || to_cr->IsDestroying()) {
            return nullptr;
        }

        return _engine->CrMngr.AddItemToCritter(to_cr, item, true);
    }
    else {
        auto* splitted_item = SplitItem(item, count);

        if (splitted_item == nullptr) {
            return nullptr;
        }

        refcount_ptr splitted_item_holder = splitted_item;
        ignore_unused(splitted_item_holder);

        if (to_cr->IsDestroyed() || to_cr->IsDestroying()) {
            RestoreSplitItem(item, splitted_item);
            return nullptr;
        }

        return _engine->CrMngr.AddItemToCritter(to_cr, splitted_item, true);
    }
}

auto ItemManager::MoveItem(Item* item, int32_t count, Map* to_map, mpos to_hex) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    EnsureEntitySynced(item);
    ValidateEntityAccess(to_map);
    refcount_ptr item_holder = item;
    refcount_ptr to_map_holder = to_map;
    ignore_unused(item_holder);
    ignore_unused(to_map_holder);

    if (item->GetOwnership() == ItemOwnership::MapHex && item->GetMapId() == to_map->GetId() && item->GetHex() == to_hex) {
        return item;
    }

    auto* holder = GetItemHolder(item);
    refcount_ptr holder_holder = holder;
    ignore_unused(holder_holder);
    Critter* dropper = dynamic_cast<Critter*>(holder);
    refcount_ptr dropper_holder = dropper;
    ignore_unused(dropper_holder);

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);

        if (item->IsDestroyed() || to_map->IsDestroyed()) {
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
    else {
        auto* splitted_item = SplitItem(item, count);

        if (splitted_item == nullptr) {
            return nullptr;
        }

        refcount_ptr splitted_item_holder = splitted_item;
        ignore_unused(splitted_item_holder);

        if (to_map->IsDestroyed()) {
            RestoreSplitItem(item, splitted_item);
            return nullptr;
        }

        to_map->AddItem(splitted_item, to_hex, dropper);

        if (splitted_item->IsDestroyed() || to_map->IsDestroyed()) {
            return nullptr;
        }
        if (splitted_item->GetOwnership() != ItemOwnership::MapHex || splitted_item->GetMapId() != to_map->GetId() || to_map->GetItem(splitted_item->GetId()) != splitted_item) {
            return nullptr;
        }

        return splitted_item;
    }
}

auto ItemManager::MoveItem(Item* item, int32_t count, Item* to_cont, const any_t& stack_id) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    EnsureEntitySynced(item);
    ValidateEntityAccess(to_cont);
    refcount_ptr item_holder = item;
    refcount_ptr to_cont_holder = to_cont;
    ignore_unused(item_holder);
    ignore_unused(to_cont_holder);

    if (item->GetOwnership() == ItemOwnership::ItemContainer && item->GetContainerId() == to_cont->GetId() && item->GetContainerStack() == stack_id) {
        return item;
    }

    auto* holder = GetItemHolder(item);
    refcount_ptr holder_holder = holder;
    ignore_unused(holder_holder);

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);

        if (item->IsDestroyed() || to_cont->IsDestroyed() || to_cont->IsDestroying()) {
            return nullptr;
        }

        return to_cont->AddItemToContainer(item, stack_id);
    }
    else {
        auto* splitted_item = SplitItem(item, count);

        if (splitted_item == nullptr) {
            return nullptr;
        }

        refcount_ptr splitted_item_holder = splitted_item;
        ignore_unused(splitted_item_holder);

        if (to_cont->IsDestroyed() || to_cont->IsDestroying()) {
            RestoreSplitItem(item, splitted_item);
            return nullptr;
        }

        return to_cont->AddItemToContainer(splitted_item, stack_id);
    }
}

auto ItemManager::AddItemContainer(Item* cont, hstring pid, int32_t count, const any_t& stack_id) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cont, "Missing required cont");
    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    ValidateEntityAccess(cont);

    const auto* proto = _engine->GetProtoItem(pid);

    if (proto == nullptr) {
        throw ItemManagerException("Item proto not found", pid);
    }

    auto* item = cont->GetInnerItemByPid(pid, stack_id);
    Item* result = nullptr;

    if (item != nullptr) {
        if (item->GetStackable()) {
            item->SetCount(item->GetCount() + count);
            result = item;
        }
        else {
            count = std::min(count, _engine->Settings.MaxAddUnstackableItems);

            for (int32_t i = 0; i < count; ++i) {
                auto* new_item = CreateItem(pid, 0, nullptr);
                result = cont->AddItemToContainer(new_item, stack_id);
            }
        }
    }
    else {
        if (proto->GetStackable()) {
            auto* new_item = CreateItem(pid, count, nullptr);
            result = cont->AddItemToContainer(new_item, stack_id);
        }
        else {
            count = std::min(count, _engine->Settings.MaxAddUnstackableItems);

            for (int32_t i = 0; i < count; ++i) {
                auto* new_item = CreateItem(pid, 0, nullptr);
                result = cont->AddItemToContainer(new_item, stack_id);
            }
        }
    }

    return result;
}

auto ItemManager::AddItemCritter(Critter* cr, hstring pid, int32_t count) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count > 0, "Count must be positive", count);
    ValidateEntityAccess(cr);

    const auto* proto = _engine->GetProtoItem(pid);

    if (proto == nullptr) {
        throw ItemManagerException("Item proto not found", pid);
    }

    auto* item = cr->GetInvItemByPid(pid);
    Item* result = nullptr;

    if (item != nullptr && item->GetStackable()) {
        item->SetCount(item->GetCount() + count);
        result = item;
    }
    else {
        if (proto->GetStackable()) {
            auto* new_item = CreateItem(pid, count, nullptr);
            result = _engine->CrMngr.AddItemToCritter(cr, new_item, true);
        }
        else {
            count = std::min(count, _engine->Settings.MaxAddUnstackableItems);

            for (int32_t i = 0; i < count; ++i) {
                auto* new_item = CreateItem(pid, 0, nullptr);
                result = _engine->CrMngr.AddItemToCritter(cr, new_item, true);
            }
        }
    }

    return result;
}

void ItemManager::SubItemCritter(Critter* cr, hstring pid, int32_t count)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);

    if (count <= 0) {
        return;
    }

    auto* item = _engine->CrMngr.GetItemByPidInvPriority(cr, pid);

    if (item == nullptr) {
        return;
    }

    if (item->GetStackable()) {
        if (count >= item->GetCount()) {
            DestroyItem(item);
        }
        else {
            item->SetCount(item->GetCount() - count);
        }
    }
    else {
        for (int32_t i = 0; i < count; ++i) {
            DestroyItem(item);

            item = _engine->CrMngr.GetItemByPidInvPriority(cr, pid);

            if (item == nullptr) {
                break;
            }
        }
    }
}

void ItemManager::SetItemCritter(Critter* cr, hstring pid, int32_t count)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);

    const auto cur_count = cr->CountInvItemByPid(pid);

    if (cur_count > count) {
        SubItemCritter(cr, pid, cur_count - count);
    }
    if (cur_count < count) {
        AddItemCritter(cr, pid, count - cur_count);
    }
}

FO_END_NAMESPACE
