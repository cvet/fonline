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
    friend void PropagateEntityLock(ptr<Item> item, ptr<EntityLock> parent_lock);
    friend void RevertEntityLock(ptr<Item> item);

public:
    Item() = delete;
    Item(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props = nullptr) noexcept;
    Item(const Item&) = delete;
    Item(Item&&) noexcept = delete;
    auto operator=(const Item&) = delete;
    auto operator=(Item&&) noexcept = delete;
    ~Item() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return _protoItem->GetName();
    }
    [[nodiscard]] auto GetProtoItem() const noexcept -> ptr<const ProtoItem>
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return _protoItem;
    }
    [[nodiscard]] auto GetInnerItem(ident_t item_id) noexcept -> nptr<Item>;
    [[nodiscard]] auto GetInnerItemByPid(hstring pid, const any_t& stack_id) noexcept -> nptr<Item>;
    [[nodiscard]] auto GetInnerItems(const any_t& stack_id) -> vector<ptr<Item>>;
    [[nodiscard]] auto HasInnerItems() const noexcept -> bool;
    [[nodiscard]] auto GetAllInnerItems() -> vector<ptr<Item>>;
    [[nodiscard]] auto GetAllInnerItems() const -> vector<ptr<const Item>>;
    [[nodiscard]] auto TakeAllInnerItems() -> vector<refcount_ptr<Item>>;
    [[nodiscard]] auto CanSendItem(bool as_public) const noexcept -> bool;
    [[nodiscard]] auto HasMultihexEntries() const noexcept -> bool
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return !!_multihexEntries;
    }
    [[nodiscard]] auto GetMultihexEntries() const noexcept -> nptr<const vector<mpos>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _multihexEntries ? nptr<const vector<mpos>> {&*_multihexEntries} : nullptr;
    }
    [[nodiscard]] auto GetOwnedLock() noexcept -> ptr<EntityLock>
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return &_ownedLock;
    }

    auto AddItemToContainer(ptr<Item> item, const any_t& stack_id) -> ptr<Item>;
    void RemoveItemFromContainer(ptr<Item> item);
    void SetItemToContainer(ptr<Item> item);
    void SetMultihexEntries(vector<mpos> entries);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterWalk, ptr<Critter> /*critter*/, bool /*isIn*/, mdir /*dir*/);

    ScriptFunc<bool, Critter*, StaticItem*, Item*, any_t> StaticScriptFunc {};
    ScriptFunc<void, Critter*, StaticItem*, bool, mdir> TriggerScriptFunc {};

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
