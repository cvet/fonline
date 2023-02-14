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

#include "ItemView.h"
#include "Client.h"
#include "StringUtils.h"

ItemView::ItemView(FOClient* engine, uint id, const ProtoItem* proto) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME)),
    EntityWithProto(this, proto),
    ItemProperties(GetInitRef())
{
    STACK_TRACE_ENTRY();

    _name = _str("{}_{}", proto->GetName(), id);
}

void ItemView::MarkAsDestroyed()
{
    STACK_TRACE_ENTRY();

    for (auto* item : _innerItems) {
        item->MarkAsDestroyed();
        item->Release();
    }
    _innerItems.clear();

    Entity::MarkAsDestroying();
    Entity::MarkAsDestroyed();
}

auto ItemView::CreateRefClone() const -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* clone = new ItemView(_engine, GetId(), dynamic_cast<const ProtoItem*>(_proto));
    clone->SetProperties(GetProperties());
    return clone;
}

auto ItemView::LightGetHash() const -> uint
{
    STACK_TRACE_ENTRY();

    return GetIsLight() ? GetLightIntensity() + GetLightDistance() + GetLightFlags() + GetLightColor() : 0;
}

auto ItemView::AddInnerItem(uint id, const ProtoItem* proto, uint stack_id, const vector<vector<uchar>>& properties_data) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = new ItemView(_engine, id, proto);
    item->RestoreData(properties_data);

    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
    item->SetContainerStack(stack_id);

    _innerItems.push_back(item);

    std::sort(_innerItems.begin(), _innerItems.end(), [](const ItemView* l, const ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });

    return item;
}

void ItemView::DeleteInnerItem(ItemView* item)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find(_innerItems.begin(), _innerItems.end(), item);
    RUNTIME_ASSERT(it != _innerItems.end());
    _innerItems.erase(it);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetContainerId(0);
    item->SetContainerStack(0);

    item->MarkAsDestroyed();
    item->Release();
}

auto ItemView::GetInnerItems() -> vector<ItemView*>
{
    STACK_TRACE_ENTRY();

    return _innerItems;
}
