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

    _name = strex("{}_{}", proto->GetName(), id);
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

auto Item::GetInnerItem(ident_t item_id) noexcept -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_innerItems) {
        return nullptr;
    }

    for (auto* item : *_innerItems) {
        if (item->GetId() == item_id) {
            return item;
        }
    }

    return nullptr;
}

auto Item::GetInnerItemByPid(hstring pid, ContainerItemStack stack_id) noexcept -> Item*
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

    return vec_filter(*_innerItems, [stack_id](const Item* item) -> bool { //
        return stack_id == ContainerItemStack::Any || item->GetContainerStack() == stack_id;
    });
}

auto Item::HasInnerItems() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _innerItems && !_innerItems->empty();
}

auto Item::GetAllInnerItems() -> const vector<Item*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_innerItems);

    return *_innerItems;
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

    make_if_not_exists(_innerItems);
    vec_add_unique_value(*_innerItems, item);

    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
}

auto Item::AddItemToContainer(Item* item, ContainerItemStack stack_id) -> Item*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(stack_id != ContainerItemStack::Any);

    if (item->GetStackable()) {
        auto* item_already = GetInnerItemByPid(item->GetProtoId(), stack_id);

        if (item_already != nullptr) {
            const auto count = item->GetCount();

            _engine->ItemMngr.DestroyItem(item);
            item_already->SetCount(item_already->GetCount() + count);

            return item_already;
        }
    }

    make_if_not_exists(_innerItems);

    item->SetContainerStack(stack_id);
    item->EvaluateSortValue(*_innerItems);
    SetItemToContainer(item);

    auto inner_item_ids = GetInnerItemIds();
    vec_add_unique_value(inner_item_ids, item->GetId());
    SetInnerItemIds(inner_item_ids);

    return item;
}

void Item::RemoveItemFromContainer(Item* item)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_innerItems);
    RUNTIME_ASSERT(item);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetContainerId(ident_t {});
    item->SetContainerStack(ContainerItemStack::Root);

    vec_remove_unique_value(*_innerItems, item);
    destroy_if_empty(_innerItems);

    auto inner_item_ids = GetInnerItemIds();
    vec_remove_unique_value(inner_item_ids, item->GetId());
    SetInnerItemIds(inner_item_ids);
}

auto Item::CanSendItem(bool as_public) const noexcept -> bool
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
