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

#include "ItemView.h"
#include "Client.h"

FO_BEGIN_NAMESPACE();

ItemView::ItemView(FOClient* engine, ident_t id, const ProtoItem* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    _name = strex("{}_{}", proto->GetName(), id);
}

void ItemView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    SetOwnership(ItemOwnership::Nowhere);
    SetCritterId(ident_t {});
    SetCritterSlot(CritterItemSlot::Inventory);

    for (auto& item : _innerItems) {
        item->DestroySelf();
    }

    _innerItems.clear();
}

auto ItemView::CreateRefClone() -> refcount_ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    auto ref_item = SafeAlloc::MakeRefCounted<ItemView>(_engine.get(), ident_t {}, dynamic_cast<const ProtoItem*>(_proto.get()), &GetProperties());

    ref_item->SetId(GetId(), false);

    return ref_item;
}

auto ItemView::AddMapperInnerItem(ident_t id, const ProtoItem* proto, const any_t& stack_id, const Properties* props) -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    auto item = SafeAlloc::MakeRefCounted<ItemView>(_engine.get(), id, proto, props);

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
    item->SetContainerStack(stack_id);

    return AddRawInnerItem(item.get());
}

auto ItemView::AddReceivedInnerItem(ident_t id, const ProtoItem* proto, const any_t& stack_id, const vector<vector<uint8>>& props_data) -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    auto item = SafeAlloc::MakeRefCounted<ItemView>(_engine.get(), id, proto, nullptr);

    item->RestoreData(props_data);
    item->SetContainerStack(stack_id);

    return AddRawInnerItem(item.get());
}

auto ItemView::AddRawInnerItem(ItemView* item) -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!item->GetStatic());
    FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::ItemContainer);
    FO_RUNTIME_ASSERT(item->GetContainerId() == GetId());

    vec_add_unique_value(_innerItems, item);

    return item;
}

void ItemView::DestroyInnerItem(ItemView* item)
{
    FO_STACK_TRACE_ENTRY();

    refcount_ptr item_ref_holder = item;
    vec_remove_unique_value(_innerItems, item);

    item->DestroySelf();
}

FO_END_NAMESPACE();
