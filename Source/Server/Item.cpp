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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Item.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Server.h"
#include "StringUtils.h"

Item::Item(FOServer* engine, ident_t id, const ProtoItem* proto, const Properties* props) :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(GetInitRef())
{
    STACK_TRACE_ENTRY();

    _name = _str("{}_{}", proto->GetName(), id);
}

void Item::EvaluateSortValue(const vector<Item*>& items)
{
    STACK_TRACE_ENTRY();

    int16 sort_value = 0;
    for (const auto* item : items) {
        if (item == this) {
            continue;
        }

        if (sort_value >= item->GetSortValue()) {
            sort_value = static_cast<int16>(item->GetSortValue() - 1);
        }
    }

    SetSortValue(sort_value);
}

auto Item::GetInnerItem(ident_t item_id, bool skip_hidden) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(item_id);

    if (!_innerItems) {
        return nullptr;
    }

    for (auto* item : *_innerItems) {
        if (item->GetId() == item_id) {
            if (skip_hidden && item->GetIsHidden()) {
                return nullptr;
            }
            return item;
        }
    }
    return nullptr;
}

auto Item::GetAllInnerItems(bool skip_hidden) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_innerItems) {
        return {};
    }

    vector<Item*> items;
    items.reserve(_innerItems->size());

    for (auto* item : *_innerItems) {
        if (!skip_hidden || !item->GetIsHidden()) {
            items.push_back(item);
        }
    }
    return items;
}

auto Item::GetInnerItemByPid(hstring pid, ContainerItemStack stack_id) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_innerItems) {
        return nullptr;
    }

    for (auto* item : *_innerItems) {
        if (item->GetProtoId() == pid && (stack_id == ContainerItemStack::Any || item->GetContainerStack() == stack_id)) {
            return item;
        }
    }
    return nullptr;
}

auto Item::GetInnerItems(ContainerItemStack stack_id) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_innerItems) {
        return {};
    }

    vector<Item*> items;
    for (auto* item : *_innerItems) {
        if (stack_id == ContainerItemStack::Any || item->GetContainerStack() == stack_id) {
            items.push_back(item);
        }
    }
    return items;
}

auto Item::HasInnerItems() const -> bool
{
    STACK_TRACE_ENTRY();

    return _innerItems && !_innerItems->empty();
}

auto Item::GetRawInnerItems() -> vector<Item*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_innerItems);

    return *_innerItems;
}

void Item::SetItemToContainer(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(item);

    if (!_innerItems) {
        _innerItems = std::make_unique<vector<Item*>>();
    }

    RUNTIME_ASSERT(std::find(_innerItems->begin(), _innerItems->end(), item) == _innerItems->end());

    _innerItems->push_back(item);
    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
}

auto Item::AddItemToContainer(Item* item, ContainerItemStack stack_id) -> Item*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(stack_id != ContainerItemStack::Any);

    if (!_innerItems) {
        _innerItems = std::make_unique<vector<Item*>>();
    }

    if (item->GetStackable()) {
        auto* item_already = GetInnerItemByPid(item->GetProtoId(), stack_id);
        if (item_already != nullptr) {
            const auto count = item->GetCount();
            _engine->ItemMngr.DestroyItem(item);
            item_already->SetCount(item_already->GetCount() + count);
            _engine->OnItemStackChanged.Fire(item_already, +static_cast<int>(count));
            return item_already;
        }
    }

    item->SetContainerStack(stack_id);
    item->EvaluateSortValue(*_innerItems);
    SetItemToContainer(item);

    auto inner_item_ids = GetInnerItemIds();
    RUNTIME_ASSERT(std::find(inner_item_ids.begin(), inner_item_ids.end(), item->GetId()) == inner_item_ids.end());
    inner_item_ids.emplace_back(item->GetId());
    SetInnerItemIds(std::move(inner_item_ids));

    return item;
}

void Item::RemoveItemFromContainer(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_innerItems);
    RUNTIME_ASSERT(item);

    const auto it = std::find(_innerItems->begin(), _innerItems->end(), item);
    RUNTIME_ASSERT(it != _innerItems->end());
    _innerItems->erase(it);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetContainerId(ident_t {});
    item->SetContainerStack(ContainerItemStack::Root);

    if (_innerItems->empty()) {
        _innerItems.reset();
    }

    auto inner_item_ids = GetInnerItemIds();
    const auto inner_item_id_it = std::find(inner_item_ids.begin(), inner_item_ids.end(), item->GetId());
    RUNTIME_ASSERT(inner_item_id_it != inner_item_ids.end());
    inner_item_ids.erase(inner_item_id_it);
    SetInnerItemIds(std::move(inner_item_ids));
}

auto Item::CanSendItem(bool as_public) const -> bool
{
    switch (GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto slot = GetCritterSlot();
        const auto slot_num = static_cast<size_t>(slot);

        if (slot_num >= _engine->Settings.CritterSlotEnabled.size() || !_engine->Settings.CritterSlotEnabled[slot_num]) {
            return false;
        }

        if (as_public) {
            if (slot_num >= _engine->Settings.CritterSlotSendData.size() || !_engine->Settings.CritterSlotSendData[slot_num]) {
                return false;
            }
        }
    } break;
    case ItemOwnership::MapHex:
        break;
    case ItemOwnership::ItemContainer:
        return false;
    default:
        return false;
    }

    if (GetIsHidden()) {
        return false;
    }

    return true;
}
