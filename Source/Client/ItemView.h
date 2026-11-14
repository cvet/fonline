// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ClientEntity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

class ItemView : public ClientEntity, public EntityWithProto, public ItemProperties
{
public:
    ItemView() = delete;
    ItemView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props = nullptr);
    ItemView(const ItemView&) = delete;
    ItemView(ItemView&&) noexcept = delete;
    auto operator=(const ItemView&) = delete;
    auto operator=(ItemView&&) noexcept = delete;
    ~ItemView() override;

    [[nodiscard]] auto GetInnerItems() const noexcept -> const_span<refcount_ptr<ItemView>> { return _innerItems; }
    [[nodiscard]] auto GetInnerItems() noexcept -> span<refcount_ptr<ItemView>> { return _innerItems; }

    auto CreateRefClone() -> refcount_ptr<ItemView>;
    auto AddMapperInnerItem(ident_t id, ptr<const ProtoItem> proto, const any_t& stack_id, nptr<const Properties> props) -> ptr<ItemView>;
    auto AddReceivedInnerItem(ident_t id, ptr<const ProtoItem> proto, const any_t& stack_id, const vector<vector<uint8_t>>& props_data) -> ptr<ItemView>;
    auto AddRawInnerItem(ptr<ItemView> item) -> ptr<ItemView>;
    void DestroyInnerItem(ptr<ItemView> item);

protected:
    void OnDestroySelf() override;

    vector<refcount_ptr<ItemView>> _innerItems {};
};

FO_END_NAMESPACE
