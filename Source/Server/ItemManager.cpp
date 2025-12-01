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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "MapManager.h"
#include "ProtoManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE();

ItemManager::ItemManager(FOServer* engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto ItemManager::GetItemHolder(Item* item) -> Entity*
{
    FO_STACK_TRACE_ENTRY();

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory:
        return _engine->EntityMngr.GetCritter(item->GetCritterId());
    case ItemOwnership::MapHex:
        return _engine->EntityMngr.GetMap(item->GetMapId());
    case ItemOwnership::ItemContainer:
        return _engine->EntityMngr.GetItem(item->GetContainerId());
    default:
        break;
    }

    throw GenericException("Item does not has holder", item->GetId(), item->GetProtoId());
}

void ItemManager::RemoveItemHolder(Item* item, Entity* holder)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);
    FO_RUNTIME_ASSERT(holder);

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        if (auto* cr = dynamic_cast<Critter*>(holder); cr != nullptr) {
            _engine->CrMngr.RemoveItemFromCritter(cr, item, true);
        }
        else {
            throw GenericException("Item owner (critter inventory) not found");
        }
    } break;
    case ItemOwnership::MapHex: {
        if (auto* map = dynamic_cast<Map*>(holder); map != nullptr) {
            map->RemoveItem(item->GetId());
        }
        else {
            throw GenericException("Item owner (map) not found");
        }
    } break;
    case ItemOwnership::ItemContainer: {
        if (auto* cont = dynamic_cast<Item*>(holder); cont != nullptr) {
            cont->RemoveItemFromContainer(item);
        }
        else {
            throw GenericException("Item owner (container) not found");
        }
    } break;
    default:
        break;
    }

    item->SetOwnership(ItemOwnership::Nowhere);
}

auto ItemManager::CreateItem(hstring pid, int32 count, const Properties* props) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(count >= 0);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto item = SafeAlloc::MakeRefCounted<Item>(_engine.get(), ident_t {}, proto, props);

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

    _engine->EntityMngr.RegisterItem(item.get());

    if (count != 0 && item->GetStackable()) {
        item->SetCount(count);
    }
    else {
        item->SetCount(1);
    }

    _engine->EntityMngr.CallInit(item.get(), true);

    if (item->IsDestroyed()) {
        throw GenericException("Item destroyed during init", pid);
    }

    FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::Nowhere);

    return item.get();
}

void ItemManager::DestroyItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    // Redundant calls
    if (item->IsDestroying() || item->IsDestroyed()) {
        return;
    }

    item->MarkAsDestroying();

    // Finish events
    _engine->OnItemFinish.Fire(item);

    // Tear off from environment
    for (InfinityLoopDetector detector; item->GetOwnership() != ItemOwnership::Nowhere || item->HasInnerItems() || item->HasInnerEntities(); detector.AddLoop()) {
        try {
            if (item->GetOwnership() != ItemOwnership::Nowhere) {
                RemoveItemHolder(item, GetItemHolder(item));
            }

            while (item->HasInnerItems()) {
                DestroyItem(item->GetAllInnerItems().front());
            }

            if (item->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(item);
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }

    // Invalidate for use
    item->MarkAsDestroyed();

    // Erase from main collection
    _engine->EntityMngr.UnregisterItem(item, true);
}

auto ItemManager::SplitItem(Item* item, int32 count) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    const auto item_count = item->GetCount();
    FO_RUNTIME_ASSERT(item->GetStackable());
    FO_RUNTIME_ASSERT(count > 0);
    FO_RUNTIME_ASSERT(count < item_count);

    auto* new_item = CreateItem(item->GetProtoId(), count, &item->GetProperties());

    item->SetCount(item_count - count);
    FO_RUNTIME_ASSERT(!new_item->IsDestroyed());

    return new_item;
}

auto ItemManager::MoveItem(Item* item, int32 count, Critter* to_cr) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(count >= 0);

    if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritterId() == to_cr->GetId()) {
        return item;
    }

    auto* holder = GetItemHolder(item);

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);
        return _engine->CrMngr.AddItemToCritter(to_cr, item, true);
    }
    else {
        auto* splitted_item = SplitItem(item, count);
        return _engine->CrMngr.AddItemToCritter(to_cr, splitted_item, true);
    }
}

auto ItemManager::MoveItem(Item* item, int32 count, Map* to_map, mpos to_hex) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(count >= 0);

    if (item->GetOwnership() == ItemOwnership::MapHex && item->GetMapId() == to_map->GetId() && item->GetHex() == to_hex) {
        return item;
    }

    auto* holder = GetItemHolder(item);

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);
        to_map->AddItem(item, to_hex, dynamic_cast<Critter*>(holder));
        return item;
    }
    else {
        auto* splitted_item = SplitItem(item, count);
        to_map->AddItem(splitted_item, to_hex, dynamic_cast<Critter*>(holder));
        return splitted_item;
    }
}

auto ItemManager::MoveItem(Item* item, int32 count, Item* to_cont, const any_t& stack_id) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(count >= 0);

    if (item->GetOwnership() == ItemOwnership::ItemContainer && item->GetContainerId() == to_cont->GetId() && item->GetContainerStack() == stack_id) {
        return item;
    }

    auto* holder = GetItemHolder(item);

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);
        return to_cont->AddItemToContainer(item, stack_id);
    }
    else {
        auto* splitted_item = SplitItem(item, count);
        return to_cont->AddItemToContainer(splitted_item, stack_id);
    }
}

auto ItemManager::AddItemContainer(Item* cont, hstring pid, int32 count, const any_t& stack_id) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cont);
    FO_RUNTIME_ASSERT(count >= 0);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = cont->GetInnerItemByPid(pid, stack_id);
    Item* result = nullptr;

    if (item != nullptr) {
        if (item->GetStackable()) {
            item->SetCount(item->GetCount() + count);
            result = item;
        }
        else {
            count = std::min(count, _engine->Settings.MaxAddUnstackableItems);

            for (int32 i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                item = cont->AddItemToContainer(item, stack_id);
                result = item;
            }
        }
    }
    else {
        if (proto->GetStackable()) {
            item = CreateItem(pid, count, nullptr);
            item = cont->AddItemToContainer(item, stack_id);
            result = item;
        }
        else {
            count = std::min(count, _engine->Settings.MaxAddUnstackableItems);

            for (int32 i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                item = cont->AddItemToContainer(item, stack_id);
                result = item;
            }
        }
    }

    return result;
}

auto ItemManager::AddItemCritter(Critter* cr, hstring pid, int32 count) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(count > 0);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = cr->GetInvItemByPid(pid);
    Item* result = nullptr;

    if (item != nullptr && item->GetStackable()) {
        item->SetCount(item->GetCount() + count);
        result = item;
    }
    else {
        if (proto->GetStackable()) {
            item = CreateItem(pid, count, nullptr);
            item = _engine->CrMngr.AddItemToCritter(cr, item, true);
            result = item;
        }
        else {
            count = std::min(count, _engine->Settings.MaxAddUnstackableItems);

            for (int32 i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                item = _engine->CrMngr.AddItemToCritter(cr, item, true);
                result = item;
            }
        }
    }

    return result;
}

void ItemManager::SubItemCritter(Critter* cr, hstring pid, int32 count)
{
    FO_STACK_TRACE_ENTRY();

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
        for (int32 i = 0; i < count; ++i) {
            DestroyItem(item);

            item = _engine->CrMngr.GetItemByPidInvPriority(cr, pid);

            if (item == nullptr) {
                break;
            }
        }
    }
}

void ItemManager::SetItemCritter(Critter* cr, hstring pid, int32 count)
{
    FO_STACK_TRACE_ENTRY();

    const auto cur_count = cr->CountInvItemByPid(pid);

    if (cur_count > count) {
        SubItemCritter(cr, pid, cur_count - count);
    }
    if (cur_count < count) {
        AddItemCritter(cr, pid, count - cur_count);
    }
}

FO_END_NAMESPACE();
