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

#include "Item.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

Item::Item(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME).as_ptr(), props ? props : nptr<const Properties> {&proto->GetProperties()}, &proto->GetProperties()),
    EntityWithProto(proto),
    ItemProperties(*GetInitRef()),
    _protoItem {proto}
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    SetEntityLock(&_ownedLock);
}

Item::~Item()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (!_engine->IsShutdownInProgress()) {
        FO_VERIFY_AND_CONTINUE(!_innerItems || _innerItems->empty(), "Server item has inner items during destruction", GetId());
    }
}

auto Item::GetName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _protoItem->GetName();
}

auto Item::GetProtoItem() const noexcept -> ptr<const ProtoItem>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _protoItem;
}

auto Item::HasMultihexEntries() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return !!_multihexEntries;
}

auto Item::GetMultihexEntries() const noexcept -> nptr<const vector<mpos>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _multihexEntries ? nptr<const vector<mpos>> {&*_multihexEntries} : nullptr;
}

auto Item::GetOwnedLock() noexcept -> ptr<EntityLock>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return &_ownedLock;
}

auto Item::GetInnerItem(ident_t item_id) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (!_innerItems) {
        return nullptr;
    }

    for (auto& item : *_innerItems) {
        if (item->GetId() == item_id) {
            return item.as_nptr();
        }
    }

    return nullptr;
}

auto Item::GetInnerItemByPid(hstring pid, const any_t& stack_id) noexcept -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (!_innerItems) {
        return nullptr;
    }

    for (auto& item : *_innerItems) {
        if (item->GetProtoId() == pid && (stack_id.empty() || item->GetContainerStack() == stack_id)) {
            return item.as_nptr();
        }
    }

    return nullptr;
}

auto Item::GetInnerItems(const any_t& stack_id) -> vector<ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    vector<ptr<Item>> result;

    if (!_innerItems) {
        return result;
    }

    result.reserve(_innerItems->size());

    for (auto& item : *_innerItems) {
        if (stack_id.empty() || item->GetContainerStack() == stack_id) {
            result.emplace_back(item.as_ptr());
        }
    }

    return result;
}

auto Item::HasInnerItems() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _innerItems && !_innerItems->empty();
}

auto Item::GetAllInnerItems() -> vector<ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");

    vector<ptr<Item>> result;
    result.reserve(_innerItems->size());

    for (auto& item : *_innerItems) {
        result.emplace_back(item.as_ptr());
    }

    return result;
}

auto Item::GetAllInnerItems() const -> vector<ptr<const Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");

    vector<ptr<const Item>> result;
    result.reserve(_innerItems->size());

    for (const auto& item : *_innerItems) {
        result.emplace_back(item.as_ptr());
    }

    return result;
}

auto Item::TakeAllInnerItems() -> vector<refcount_ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");

    vector<refcount_ptr<Item>> result = std::move(*_innerItems);
    _innerItems.reset();

    return result;
}

void Item::SetItemToContainer(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    if (!_innerItems) {
        _innerItems.emplace();
    }

    vec_add_unique_value(*_innerItems, item.hold_ref());

    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(GetId());
    ptr<Item> cont = this;
    item->SetParent(cont);

    auto nullable_entity_lock = GetEntityLock();
    FO_VERIFY_AND_THROW(nullable_entity_lock, "Container item has no entity lock");
    auto entity_lock = nullable_entity_lock.as_ptr();
    PropagateEntityLock(item, entity_lock);
}

auto Item::AddItemToContainer(ptr<Item> item, const any_t& stack_id) -> ptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);

    if (item->GetStackable()) {
        auto nullable_item_already = GetInnerItemByPid(item->GetProtoId(), stack_id);

        if (nullable_item_already) {
            const auto count = item->GetCount();
            _engine->ItemMngr.DestroyItem(item);
            nullable_item_already->SetCount(nullable_item_already->GetCount() + count);
            return nullable_item_already.as_ptr();
        }
    }

    if (!_innerItems) {
        _innerItems.emplace();
    }

    item->SetContainerStack(stack_id);
    SetItemToContainer(item);

    auto inner_item_ids = GetInnerItemIds();
    vec_add_unique_value(inner_item_ids, item->GetId());
    SetInnerItemIds(inner_item_ids);

    if (IsPersistent()) {
        _engine->EntityMngr.MakePersistent(item, true);
    }

    return item;
}

void Item::RemoveItemFromContainer(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    FO_VERIFY_AND_THROW(_innerItems, "Item inner container storage is missing");

    RevertEntityLock(item);
    auto ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(item);

    item->SetParent(nullptr);
    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetContainerId({});
    item->SetContainerStack({});

    vec_remove_unique_value(*_innerItems, item.hold_ref());

    if (_innerItems->empty()) {
        _innerItems.reset();
    }

    auto inner_item_ids = GetInnerItemIds();
    vec_remove_unique_value(inner_item_ids, item->GetId());
    SetInnerItemIds(inner_item_ids);

    if (item->IsPersistent() && !item->IsExplicitlyPersistent()) {
        _engine->EntityMngr.MakePersistent(item, false);
    }
}

auto Item::CanSendItem(bool as_public) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    switch (GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto slot = GetCritterSlot();
        const auto slot_num = static_cast<size_t>(slot);

        if (slot_num >= _engine->Settings->CritterSlotEnabled.size() || !_engine->Settings->CritterSlotEnabled[slot_num]) {
            return false;
        }

        if (as_public) {
            if (slot_num >= _engine->Settings->CritterSlotSendData.size() || !_engine->Settings->CritterSlotSendData[slot_num]) {
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
