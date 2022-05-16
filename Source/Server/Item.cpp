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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

Item::Item(FOServer* engine, uint id, const ProtoItem* proto) : ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), proto), ItemProperties(GetInitRef())
{
    RUNTIME_ASSERT(proto);
    RUNTIME_ASSERT(GetCount() > 0);
}

void Item::EvaluateSortValue(const vector<Item*>& items)
{
    short sort_value = 0;
    for (const auto* item : items) {
        if (item == this) {
            continue;
        }

        if (sort_value >= item->GetSortValue()) {
            sort_value = item->GetSortValue() - 1;
        }
    }

    SetSortValue(sort_value);
}

void Item::ChangeCount(int val)
{
    SetCount(GetCount() + val);
}

auto Item::ContGetItem(uint item_id, bool skip_hidden) -> Item*
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(item_id);

    if (_childItems == nullptr) {
        return nullptr;
    }

    for (auto* item : *_childItems) {
        if (item->GetId() == item_id) {
            if (skip_hidden && item->GetIsHidden()) {
                return nullptr;
            }
            return item;
        }
    }
    return nullptr;
}

auto Item::ContGetAllItems(bool skip_hidden) -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    if (_childItems == nullptr) {
        return {};
    }

    vector<Item*> items;
    items.reserve(_childItems->size());

    for (auto* item : *_childItems) {
        if (!skip_hidden || !item->GetIsHidden()) {
            items.push_back(item);
        }
    }
    return items;
}

auto Item::ContGetItemByPid(hstring pid, uint stack_id) -> Item*
{
    NON_CONST_METHOD_HINT();

    if (_childItems == nullptr) {
        return nullptr;
    }

    for (auto* item : *_childItems) {
        if (item->GetProtoId() == pid && (stack_id == static_cast<uint>(-1) || item->GetContainerStack() == stack_id)) {
            return item;
        }
    }
    return nullptr;
}

auto Item::ContGetItems(uint stack_id) -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    if (_childItems == nullptr) {
        return {};
    }

    vector<Item*> items;
    for (auto* item : *_childItems) {
        if (stack_id == static_cast<uint>(-1) || item->GetContainerStack() == stack_id) {
            items.push_back(item);
        }
    }
    return items;
}

auto Item::ContIsItems() const -> bool
{
    return _childItems != nullptr && !_childItems->empty();
}
