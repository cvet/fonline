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

#include "Item.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE();

Item::Item(FOServer* engine, ident_t id, const ProtoItem* proto, const Properties* props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(GetInitRef())
{
    FO_STACK_TRACE_ENTRY();
}

auto Item::GetInnerItem(ident_t item_id) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

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

auto Item::GetInnerItemByPid(hstring pid, const any_t& stack_id) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    if (!_innerItems) {
        return nullptr;
    }

    for (auto* item : *_innerItems) {
        if (item->GetProtoId() == pid && (stack_id.empty() || item->GetContainerStack() == stack_id)) {
            return item;
        }
    }

    return nullptr;
}

auto Item::GetInnerItems(const any_t& stack_id) -> vector<Item*>
{
    FO_STACK_TRACE_ENTRY();

    if (!_innerItems) {
        return {};
    }

    if (stack_id.empty()) {
        return *_innerItems;
    }
    else {
        return vec_filter(*_innerItems, [stack_id](const Item* item) -> bool { //
            return item->GetContainerStack() == stack_id;
        });
    }
}

auto Item::HasInnerItems() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _innerItems && !_innerItems->empty();
}

auto Item::GetAllInnerItems() -> const vector<Item*>&
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_innerItems);

    return *_innerItems;
}

auto Item::GetRawInnerItems() -> vector<Item*>&
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_innerItems);

    return *_innerItems;
}

void Item::SetItemToContainer(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);

    make_if_not_exists(_innerItems);
    vec_add_unique_value(*_innerItems, item);

    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
}

auto Item::AddItemToContainer(Item* item, const any_t& stack_id) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);

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
    SetItemToContainer(item);

    auto inner_item_ids = GetInnerItemIds();
    vec_add_unique_value(inner_item_ids, item->GetId());
    SetInnerItemIds(inner_item_ids);

    return item;
}

void Item::RemoveItemFromContainer(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_innerItems);
    FO_RUNTIME_ASSERT(item);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetContainerId({});
    item->SetContainerStack({});

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

    if (GetHidden()) {
        return false;
    }

    return true;
}

void Item::SetMultihexEntries(vector<mpos> entries)
{
    FO_STACK_TRACE_ENTRY();

    if (!entries.empty()) {
        make_if_not_exists(_multihexEntries);
        *_multihexEntries = std::move(entries);
    }
    else {
        _multihexEntries.reset();
    }
}

FO_END_NAMESPACE();
