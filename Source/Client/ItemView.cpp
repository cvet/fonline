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

#include "ItemView.h"
#include "Client.h"
#include "StringUtils.h"

ItemView::ItemView(FOClient* engine, ident_t id, const ProtoItem* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(GetInitRef())
{
    STACK_TRACE_ENTRY();

    _name = _str("{}_{}", proto->GetName(), id);
}

void ItemView::OnDestroySelf()
{
    STACK_TRACE_ENTRY();

    SetOwnership(ItemOwnership::Nowhere);
    SetCritterId(ident_t {});
    SetCritterSlot(CritterItemSlot::Inventory);

    for (auto* item : _innerItems) {
        item->DestroySelf();
    }

    _innerItems.clear();
}

auto ItemView::CreateRefClone() const -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* ref_item = new ItemView(_engine, {}, dynamic_cast<const ProtoItem*>(_proto), &GetProperties());

    ref_item->SetId(GetId(), false);

    return ref_item;
}

auto ItemView::AddInnerItem(ident_t id, const ProtoItem* proto, ContainerItemStack stack_id, const Properties* props) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = new ItemView(_engine, id, proto, props);

    item->SetIsStatic(false);
    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
    item->SetContainerStack(stack_id);

    vec_add_unique_value(_innerItems, item);

    std::sort(_innerItems.begin(), _innerItems.end(), [](const ItemView* l, const ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });

    return item;
}

auto ItemView::AddInnerItem(ident_t id, const ProtoItem* proto, ContainerItemStack stack_id, const vector<vector<uint8>>& props_data) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = AddInnerItem(id, proto, stack_id, nullptr);

    item->RestoreData(props_data);

    RUNTIME_ASSERT(!item->GetIsStatic());
    RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::ItemContainer);
    RUNTIME_ASSERT(item->GetContainerId() == GetId());
    RUNTIME_ASSERT(item->GetContainerStack() == stack_id);

    std::sort(_innerItems.begin(), _innerItems.end(), [](const ItemView* l, const ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });

    return item;
}

void ItemView::DestroyInnerItem(ItemView* item)
{
    STACK_TRACE_ENTRY();

    vec_remove_unique_value(_innerItems, item);

    item->DestroySelf();
}

auto ItemView::GetInnerItems() -> const vector<ItemView*>&
{
    STACK_TRACE_ENTRY();

    return _innerItems;
}

auto ItemView::GetConstInnerItems() const -> vector<const ItemView*>
{
    STACK_TRACE_ENTRY();

    return vec_static_cast<const ItemView*>(_innerItems);
}
