// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ItemView.h"

#include "Client.h"
#include "MapLoader.h"

FO_BEGIN_NAMESPACE

ItemView::ItemView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrar(ENTITY_TYPE_NAME), props ? props : nptr<const Properties> {proto->GetProperties()}, proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(*GetInitRef())
{
    _name = strex("{}_{}", proto->GetName(), id);
}

ItemView::~ItemView()
{
    FO_VERIFY_AND_CONTINUE(_innerItems.empty(), "Client item view has inner items during destruction", GetId(), _innerItems.size());
}

void ItemView::OnDestroySelf()
{
    SetOwnership(ItemOwnership::Nowhere);
    SetCritterId(ident_t {});
    SetCritterSlot(CritterItemSlot::Inventory);

    for (auto& inner_item : _innerItems) {
        safe_call([&] { inner_item->DestroySelf(); });
    }

    _innerItems.clear();
}

auto ItemView::CreateRefClone() -> refcount_ptr<ItemView>
{
    FO_TRACE_ZONE(Entity);

    auto proto {require_refcount_ptr(_proto.dyn_cast<const ProtoItem>())};

    auto ref_item {safe_alloc::make_refcounted<ItemView>(_engine, ident_t {}, proto, GetProperties())};

    ref_item->SetId(GetId(), false);

    return ref_item;
}

auto ItemView::AddMapperInnerItem(ident_t id, ptr<const ProtoItem> proto, const any_t& stack_id, nptr<const Properties> props) -> ptr<ItemView>
{
    FO_TRACE_ZONE(Entity);

    auto item {safe_alloc::make_refcounted<ItemView>(_engine, id, proto, props)};

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
    item->SetContainerStack(stack_id);

    MapLoader::ValidateItemProperties(*item->GetProperties(), *proto->GetProperties(), proto->GetName(), id);

    return AddRawInnerItem(item);
}

auto ItemView::AddReceivedInnerItem(ident_t id, ptr<const ProtoItem> proto, const any_t& stack_id, const vector<vector<uint8_t>>& props_data) -> ptr<ItemView>
{
    FO_TRACE_ZONE(Entity);

    auto item {safe_alloc::make_refcounted<ItemView>(_engine, id, proto, nullptr)};

    item->RestoreData(props_data);
    item->SetContainerStack(stack_id);

    return AddRawInnerItem(item);
}

auto ItemView::AddRawInnerItem(ptr<ItemView> item) -> ptr<ItemView>
{
    FO_VERIFY_AND_THROW(!item->GetStatic(), "Item is static and cannot be attached here");
    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::ItemContainer, "Item is not owned by this container");
    FO_VERIFY_AND_THROW(item->GetContainerId() == GetId(), "Item belongs to a different container");

    vec_add_unique_value(_innerItems, item.hold_ref());

    return item;
}

void ItemView::DestroyInnerItem(ptr<ItemView> item)
{
    auto item_ref_holder {item.hold_ref()};
    vec_remove_unique_value(_innerItems, item_ref_holder);

    item->DestroySelf();
}

FO_END_NAMESPACE
