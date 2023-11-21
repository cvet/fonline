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
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), props != nullptr ? props : &proto->GetProperties()),
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

auto Item::IsInnerItems() const -> bool
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
