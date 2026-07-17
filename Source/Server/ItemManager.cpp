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

ItemManager::ItemManager(ptr<ServerEngine> engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto ItemManager::GetItemHolder(ptr<Item> item) -> ptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

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

    item->SetOwnership(ItemOwnership::Nowhere);
}

auto ItemManager::CreateItem(hstring pid, int32_t count, nptr<const Properties> props) -> ptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);

    auto proto = _engine->GetProtoItem(pid);

    if (!proto) {
        throw ItemManagerException("Item proto not found", pid);
    }

    auto item = SafeAlloc::MakeRefCounted<Item>(_engine, ident_t {}, proto, props);
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

    if (count != 0 && item->GetStackable()) {
        item->SetCount(count);
    }
    else {
        item->SetCount(1);
    }

    _engine->EntityMngr.CallInit(item, true);

    if (item->IsDestroyed()) {
        throw ItemManagerException("Item destroyed during init", pid);
    }

    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::Nowhere, "Item is already owned by another holder");

    return item;
}

auto ItemManager::CreateItemOnHex(ptr<Map> map, mpos hex, hstring pid, int32_t count, nptr<const Properties> props) -> ptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    auto map_holder = map.hold_ref();
    ignore_unused(map_holder);
    ValidateEntityAccess(map);

    if (count <= 0) {
        throw ItemManagerException("Invalid items cound");
    }

    auto proto = _engine->GetProtoItem(pid);

    if (!proto) {
        throw ItemManagerException("Item proto not found", pid);
    }

    const auto add_item = [&]() -> ptr<Item> {
        auto item = CreateItem(pid, proto->GetStackable() ? count : 1, props);
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
    };

    ptr<Item> item = add_item();

    // Non-stacked items
    if (!proto->GetStackable() && count > 1) {
        const auto fixed_count = std::min(count, _engine->Settings->MaxAddUnstackableItems);

        for (int32_t i = 0; i < fixed_count; i++) {
            (void)add_item();
        }
    }

    return item;
}

void ItemManager::DestroyItem(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

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
                // DestroyItem takes the inner item's own lock itself as its teardown capture.
                ValidateEntityAccess(inner);
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

auto ItemManager::SplitItem(ptr<Item> item, int32_t count) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(item);

    FO_VERIFY_AND_THROW(item->GetStackable(), "Item is not stackable");
    FO_VERIFY_AND_THROW(count > 0, "Count must be positive", count);
    FO_VERIFY_AND_THROW(count < item->GetCount(), "Count is outside allowed range", count, item->GetCount());

    auto new_item = CreateItem(item->GetProtoId(), count, item->GetProperties());

    auto new_item_holder = new_item.hold_ref();
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

void ItemManager::RestoreSplitItem(ptr<Item> item, ptr<Item> splitted_item)
{
    FO_STACK_TRACE_ENTRY();

    if (!item->IsDestroyed() && item->GetProtoId() == splitted_item->GetProtoId()) {
        item->SetCount(item->GetCount() + splitted_item->GetCount());
    }

    DestroyItem(splitted_item);
}

auto ItemManager::MoveItem(ptr<Item> item, int32_t count, ptr<Critter> to_cr) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    EnsureEntitySynced(item);
    ValidateEntityAccess(to_cr);
    auto item_holder = item.hold_ref();
    auto to_cr_holder = to_cr.hold_ref();
    ignore_unused(item_holder);
    ignore_unused(to_cr_holder);

    if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritterId() == to_cr->GetId()) {
        return item;
    }

    auto holder = GetItemHolder(item);
    auto holder_holder = holder.hold_ref();
    ignore_unused(holder_holder);

    if (count > 0 && item->GetStackable()) {
        auto result_item = to_cr->GetInvItemByPid(item->GetProtoId());

        if (result_item && result_item != item) {
            auto result_item_holder = result_item.hold_ref();
            ignore_unused(result_item_holder);

            const int32_t source_count = item->GetCount();
            const int32_t result_count = result_item->GetCount();
            const int32_t transfer_count = std::min(count, source_count);

            _engine->OnCritterItemTransferIn.Fire(to_cr, item, result_item, transfer_count);

            FO_VERIFY_AND_THROW(!item->IsDestroyed() && !item->IsDestroying(), "Critter item transfer event destroyed the source item", item->GetId());
            FO_VERIFY_AND_THROW(!to_cr->IsDestroyed() && !to_cr->IsDestroying(), "Critter item transfer event destroyed the destination critter", to_cr->GetId());
            FO_VERIFY_AND_THROW(!result_item->IsDestroyed() && !result_item->IsDestroying(), "Critter item transfer event destroyed the destination stack", result_item->GetId());
            FO_VERIFY_AND_THROW(GetItemHolder(item) == holder, "Critter item transfer event moved the source item", item->GetId());
            FO_VERIFY_AND_THROW(item->GetCount() == source_count, "Critter item transfer event changed the source count", item->GetId(), item->GetCount(), source_count);
            FO_VERIFY_AND_THROW(to_cr->GetInvItemByPid(item->GetProtoId()) == result_item, "Critter item transfer event replaced the destination stack", to_cr->GetId(), item->GetProtoId());
            FO_VERIFY_AND_THROW(result_item->GetCount() == result_count, "Critter item transfer event changed the destination count", result_item->GetId(), result_item->GetCount(), result_count);
        }
    }

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);

        if (item->IsDestroyed() || to_cr->IsDestroyed() || to_cr->IsDestroying()) {
            return nullptr;
        }

        return _engine->CrMngr.AddItemToCritter(to_cr, item, true);
    }
    else {
        auto splitted_item = SplitItem(item, count);

        if (!splitted_item) {
            return nullptr;
        }

        auto splitted_item_holder = splitted_item.hold_ref();
        ignore_unused(splitted_item_holder);

        if (to_cr->IsDestroyed() || to_cr->IsDestroying()) {
            RestoreSplitItem(item, splitted_item);
            return nullptr;
        }

        return _engine->CrMngr.AddItemToCritter(to_cr, splitted_item, true);
    }
}

auto ItemManager::MoveItem(ptr<Item> item, int32_t count, ptr<Map> to_map, mpos to_hex) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    EnsureEntitySynced(item);
    ValidateEntityAccess(to_map);
    auto item_holder = item.hold_ref();
    auto to_map_holder = to_map.hold_ref();
    ignore_unused(item_holder);
    ignore_unused(to_map_holder);

    if (item->GetOwnership() == ItemOwnership::MapHex && item->GetMapId() == to_map->GetId() && item->GetHex() == to_hex) {
        return item;
    }

    auto holder = GetItemHolder(item);
    auto holder_holder = holder.hold_ref();
    ignore_unused(holder_holder);
    auto dropper = holder.dyn_cast<Critter>();
    auto dropper_holder = dropper.try_hold_ref();
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
        auto splitted_item = SplitItem(item, count);

        if (!splitted_item) {
            return nullptr;
        }

        auto splitted_item_holder = splitted_item.hold_ref();
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

auto ItemManager::MoveItem(ptr<Item> item, int32_t count, ptr<Item> to_cont, const any_t& stack_id) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    EnsureEntitySynced(item);
    ValidateEntityAccess(to_cont);
    auto item_holder = item.hold_ref();
    auto to_cont_holder = to_cont.hold_ref();
    ignore_unused(item_holder);
    ignore_unused(to_cont_holder);

    if (item->GetOwnership() == ItemOwnership::ItemContainer && item->GetContainerId() == to_cont->GetId() && item->GetContainerStack() == stack_id) {
        return item;
    }

    auto holder = GetItemHolder(item);
    auto holder_holder = holder.hold_ref();
    ignore_unused(holder_holder);

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);

        if (item->IsDestroyed() || to_cont->IsDestroyed() || to_cont->IsDestroying()) {
            return nullptr;
        }

        return to_cont->AddItemToContainer(item, stack_id);
    }
    else {
        auto splitted_item = SplitItem(item, count);

        if (!splitted_item) {
            return nullptr;
        }

        auto splitted_item_holder = splitted_item.hold_ref();
        ignore_unused(splitted_item_holder);

        if (to_cont->IsDestroyed() || to_cont->IsDestroying()) {
            RestoreSplitItem(item, splitted_item);
            return nullptr;
        }

        return to_cont->AddItemToContainer(splitted_item, stack_id);
    }
}

auto ItemManager::AddItemContainer(ptr<Item> cont, hstring pid, int32_t count, const any_t& stack_id) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count >= 0, "Count is negative", count);
    ValidateEntityAccess(cont);

    auto proto = _engine->GetProtoItem(pid);

    if (!proto) {
        throw ItemManagerException("Item proto not found", pid);
    }

    auto item = cont->GetInnerItemByPid(pid, stack_id);
    nptr<Item> result = nullptr;

    if (item) {
        if (item->GetStackable()) {
            item->SetCount(item->GetCount() + count);
            result = item;
        }
        else {
            count = std::min(count, _engine->Settings->MaxAddUnstackableItems);

            for (int32_t i = 0; i < count; ++i) {
                auto new_item = CreateItem(pid, 0, nullptr);
                result = cont->AddItemToContainer(new_item, stack_id);
            }
        }
    }
    else {
        if (proto->GetStackable()) {
            auto new_item = CreateItem(pid, count, nullptr);
            result = cont->AddItemToContainer(new_item, stack_id);
        }
        else {
            count = std::min(count, _engine->Settings->MaxAddUnstackableItems);

            for (int32_t i = 0; i < count; ++i) {
                auto new_item = CreateItem(pid, 0, nullptr);
                result = cont->AddItemToContainer(new_item, stack_id);
            }
        }
    }

    return result;
}

auto ItemManager::AddItemCritter(ptr<Critter> cr, hstring pid, int32_t count) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(count > 0, "Count must be positive", count);
    ValidateEntityAccess(cr);

    auto proto = _engine->GetProtoItem(pid);

    if (!proto) {
        throw ItemManagerException("Item proto not found", pid);
    }

    auto item = cr->GetInvItemByPid(pid);
    nptr<Item> result = nullptr;

    if (item && item->GetStackable()) {
        item->SetCount(item->GetCount() + count);
        result = item;
    }
    else {
        if (proto->GetStackable()) {
            auto new_item = CreateItem(pid, count, nullptr);
            result = _engine->CrMngr.AddItemToCritter(cr, new_item, true);
        }
        else {
            count = std::min(count, _engine->Settings->MaxAddUnstackableItems);

            for (int32_t i = 0; i < count; ++i) {
                auto new_item = CreateItem(pid, 0, nullptr);
                result = _engine->CrMngr.AddItemToCritter(cr, new_item, true);
            }
        }
    }

    return result;
}

void ItemManager::SubItemCritter(ptr<Critter> cr, hstring pid, int32_t count)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);

    if (count <= 0) {
        return;
    }

    auto item = cr->GetItemByPidInvPriority(pid);

    if (!item) {
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

            item = cr->GetItemByPidInvPriority(pid);

            if (!item) {
                break;
            }
        }
    }
}

void ItemManager::SetItemCritter(ptr<Critter> cr, hstring pid, int32_t count)
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
