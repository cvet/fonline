// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "EntitySync.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE

class Item;
class Critter;
class StaticItem;

class Item : public ServerEntity, public EntityWithProto, public ItemProperties
{
    friend class Entity;

public:
    Item() = delete;
    Item(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props = nullptr) noexcept;
    Item(const Item&) = delete;
    Item(Item&&) noexcept = delete;
    auto operator=(const Item&) = delete;
    auto operator=(Item&&) noexcept = delete;
    ~Item() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override;
    [[nodiscard]] auto GetProtoItem() const noexcept -> ptr<const ProtoItem>;
    [[nodiscard]] auto GetInnerItem(ident_t item_id) noexcept -> nptr<Item>;
    [[nodiscard]] auto GetInnerItems(const any_t& stack_id) -> vector<ptr<Item>>;
    [[nodiscard]] auto HasInnerItems() const noexcept -> bool;
    [[nodiscard]] auto GetAllInnerItems() -> vector<ptr<Item>>;
    [[nodiscard]] auto GetAllInnerItems() const -> vector<ptr<const Item>>;
    [[nodiscard]] auto CanSendItem(bool as_public) const noexcept -> bool;
    [[nodiscard]] auto HasMultihexEntries() const noexcept -> bool;
    [[nodiscard]] auto GetMultihexEntries() const noexcept -> nptr<const vector<mpos>>;
    [[nodiscard]] auto GetOwnedLock() noexcept -> ptr<EntityLock>;
    [[nodiscard]] auto IsInsideContainer(ptr<const Item> container) -> bool;

    auto TakeAllInnerItems() -> vector<refcount_ptr<Item>>;
    auto AddItemToContainer(ptr<Item> item, const any_t& stack_id) -> ptr<Item>;
    void RemoveItemFromContainer(ptr<Item> item);
    void SetItemToContainer(ptr<Item> item);
    void SetMultihexEntries(vector<mpos> entries);
    void ClearAllAssociations() noexcept;

    // Runs while this item is being finished
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    // Runs when a critter crosses this static item's trigger footprint; `isIn` distinguishes entry from exit
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterWalk, ptr<Critter> /*critter*/, bool /*isIn*/, mdir /*dir*/);

    ScriptFunc<bool, ptr<Critter>, ptr<StaticItem>, nptr<Item>, any_t> StaticScriptFunc {};
    ScriptFunc<void, ptr<Critter>, ptr<StaticItem>, bool, mdir> TriggerScriptFunc {};

private:
    ptr<const ProtoItem> _protoItem;
    optional<vector<refcount_ptr<Item>>> _innerItems {};
    optional<vector<mpos>> _multihexEntries {};
    EntityLock _ownedLock {};
};

class StaticItem final : public Item
{
    using Item::Item;
};

FO_END_NAMESPACE
