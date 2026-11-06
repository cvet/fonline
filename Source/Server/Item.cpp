// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Item.h"

#include "CritterManager.h"
#include "ItemManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

Item::Item(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrar(ENTITY_TYPE_NAME), props ? props : nptr<const Properties> {proto->GetProperties()}, proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(*GetInitRef()),
    _protoItem {proto}
{
    FO_VALIDATE_ENTITY(NONE);
    SetEntityLock(&_ownedLock);
}

Item::~Item()
{
    FO_VALIDATE_ENTITY(NONE);

    FO_VERIFY_AND_CONTINUE(!_innerItems || _innerItems->empty(), "Server item has inner items during destruction", GetId());
}

void Item::ClearAllAssociations() noexcept
{
    FO_VALIDATE_ENTITY(NONE);

    _innerItems.reset();
}

auto Item::GetName() const noexcept -> string_view
{
    FO_VALIDATE_ENTITY(NONE);
    return _protoItem->GetName();
}

auto Item::GetProtoItem() const noexcept -> ptr<const ProtoItem>
{
    FO_VALIDATE_ENTITY(NONE);
    return _protoItem;
}

auto Item::HasMultihexEntries() const noexcept -> bool
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return !!_multihexEntries;
}

auto Item::GetMultihexEntries() const noexcept -> nptr<const vector<mpos>>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _multihexEntries ? nptr<const vector<mpos>> {&*_multihexEntries} : nullptr;
}

auto Item::GetOwnedLock() noexcept -> ptr<EntityLock>
{
    FO_VALIDATE_ENTITY(NONE);
    return &_ownedLock;
}

auto Item::IsInsideContainer(ptr<const Item> container) -> bool
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    for (auto current {make_nptr(this).try_hold_ref()}; current;) {
        if (current == container.get()) {
            return true;
        }

        if (current->GetOwnership() != ItemOwnership::ItemContainer) {
            break;
        }

        current = current->GetParent<Item>();
    }

    return false;
}

auto Item::GetInnerItem(ident_t item_id) noexcept -> nptr<Item>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (!_innerItems) {
        return nullptr;
    }

    for (auto& item : *_innerItems) {
        if (item->GetId() == item_id) {
            return item;
        }
    }

    return nullptr;
}

auto Item::GetInnerItems(const any_t& stack_id) -> vector<ptr<Item>>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    vector<ptr<Item>> result {};

    if (!_innerItems) {
        return result;
    }

    result.reserve(_innerItems->size());

    for (auto& item : *_innerItems) {
        if (stack_id.empty() || item->GetContainerStack() == stack_id) {
            result.emplace_back(item);
        }
    }

    return result;
}

auto Item::HasInnerItems() const noexcept -> bool
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _innerItems && !_innerItems->empty();
}

auto Item::GetAllInnerItems() -> vector<ptr<Item>>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");

    vector<ptr<Item>> result {};
    result.reserve(_innerItems->size());

    for (auto& item : *_innerItems) {
        result.emplace_back(item);
    }

    return result;
}

auto Item::GetAllInnerItems() const -> vector<ptr<const Item>>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");

    vector<ptr<const Item>> result {};
    result.reserve(_innerItems->size());

    for (const auto& item : *_innerItems) {
        result.emplace_back(item);
    }

    return result;
}

auto Item::TakeAllInnerItems() -> vector<refcount_ptr<Item>>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");

    vector<refcount_ptr<Item>> result {std::move(*_innerItems)};
    _innerItems.reset();

    return result;
}

void Item::SetItemToContainer(ptr<Item> item)
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    EnsureEntitySynced(item);

    if (!_innerItems) {
        _innerItems.emplace();
    }

    vec_add_unique_value(*_innerItems, item.hold_ref());

    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
    item->SetParent(this);
}

auto Item::AddItemToContainer(ptr<Item> item, const any_t& stack_id) -> ptr<Item>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    EnsureEntitySynced(item);

    // A container moved into its own subtree tears that branch off the world and makes every holder walk endless,
    // so the cycle is refused before any ownership is written
    FO_VERIFY_AND_THROW(!IsInsideContainer(item), "Container cannot be placed inside itself", GetId(), item->GetId());

    if (!_innerItems) {
        _innerItems.emplace();
    }

    item->SetContainerStack(stack_id);
    SetItemToContainer(item);

    auto inner_item_ids {GetInnerItemIds()};
    vec_add_unique_value(inner_item_ids, item->GetId());
    SetInnerItemIds(inner_item_ids);

    if (IsPersistent()) {
        _engine->EntityMngr.MakePersistent(item, true);
    }

    return item;
}

void Item::RemoveItemFromContainer(ptr<Item> item)
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");
    EnsureEntitySynced(item);

    item->SetParent(nullptr);
    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetContainerId({});
    item->SetContainerStack({});

    vec_remove_unique_value(*_innerItems, item.hold_ref());

    if (_innerItems->empty()) {
        _innerItems.reset();
    }

    auto inner_item_ids {GetInnerItemIds()};
    vec_remove_unique_value(inner_item_ids, item->GetId());
    SetInnerItemIds(inner_item_ids);

    if (item->IsPersistent() && !item->IsExplicitlyPersistent()) {
        _engine->EntityMngr.MakePersistent(item, false);
    }
}

auto Item::CanSendItem(bool as_public) const noexcept -> bool
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    switch (GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto slot {GetCritterSlot()};
        size_t slot_num {static_cast<size_t>(slot)};

        if (slot_num >= _engine->Settings->Critter.CritterSlotEnabled.size() || !_engine->Settings->Critter.CritterSlotEnabled[slot_num]) {
            return false;
        }

        if (as_public) {
            if (slot_num >= _engine->Settings->Critter.CritterSlotSendData.size() || !_engine->Settings->Critter.CritterSlotSendData[slot_num]) {
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
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (!entries.empty()) {
        if (!_multihexEntries) {
            _multihexEntries.emplace();
        }

        *_multihexEntries = std::move(entries);
    }
    else {
        _multihexEntries.reset();
    }
}

FO_END_NAMESPACE
