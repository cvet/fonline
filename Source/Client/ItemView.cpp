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

#include "ItemView.h"
#include "Client.h"

FO_BEGIN_NAMESPACE

ItemView::ItemView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME).as_ptr(), props ? props : nptr<const Properties> {&proto->GetProperties()}, &proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(*GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    _name = strex("{}_{}", proto->GetName(), id);
}

ItemView::~ItemView()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_CONTINUE(_innerItems.empty(), "Client item view has inner items during destruction", GetId(), _innerItems.size());
}

void ItemView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    SetOwnership(ItemOwnership::Nowhere);
    SetCritterId(ident_t {});
    SetCritterSlot(CritterItemSlot::Inventory);

    for (auto& inner_item : _innerItems) {
        auto item = inner_item.as_ptr();
        safe_call([&] { item->DestroySelf(); });
    }

    _innerItems.clear();
}

auto ItemView::CreateRefClone() -> refcount_ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    auto proto = require_refcount_ptr(_proto.dyn_cast<const ProtoItem>());

    auto ref_item = SafeAlloc::MakeRefCounted<ItemView>(_engine, ident_t {}, proto, &GetProperties());

    ref_item->SetId(GetId(), false);

    return ref_item;
}

auto ItemView::AddMapperInnerItem(ident_t id, ptr<const ProtoItem> proto, const any_t& stack_id, nptr<const Properties> props) -> ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    refcount_ptr<ItemView> item = SafeAlloc::MakeRefCounted<ItemView>(_engine, id, proto, props);

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
    item->SetContainerStack(stack_id);

    return AddRawInnerItem(item);
}

auto ItemView::AddReceivedInnerItem(ident_t id, ptr<const ProtoItem> proto, const any_t& stack_id, const vector<vector<uint8_t>>& props_data) -> ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    refcount_ptr<ItemView> item = SafeAlloc::MakeRefCounted<ItemView>(_engine, id, proto, nullptr);

    item->RestoreData(props_data);
    item->SetContainerStack(stack_id);

    return AddRawInnerItem(item);
}

auto ItemView::AddRawInnerItem(ptr<ItemView> item) -> ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!item->GetStatic(), "Item is static and cannot be attached here");
    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::ItemContainer, "Item is not owned by this container");
    FO_VERIFY_AND_THROW(item->GetContainerId() == GetId(), "Item belongs to a different container");

    vec_add_unique_value(_innerItems, item.hold_ref());

    return item;
}

void ItemView::DestroyInnerItem(ptr<ItemView> item)
{
    FO_STACK_TRACE_ENTRY();

    auto item_ref_holder = item.hold_ref();
    vec_remove_unique_value(_innerItems, item_ref_holder);

    item->DestroySelf();
}

FO_END_NAMESPACE
