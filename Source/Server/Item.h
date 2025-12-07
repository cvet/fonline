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

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE();

class Item;
class Critter;
using StaticItem = Item;

class Item final : public ServerEntity, public EntityWithProto, public ItemProperties
{
    friend class Entity;

public:
    Item() = delete;
    Item(FOServer* engine, ident_t id, const ProtoItem* proto, const Properties* props = nullptr) noexcept;
    Item(const Item&) = delete;
    Item(Item&&) noexcept = delete;
    auto operator=(const Item&) = delete;
    auto operator=(Item&&) noexcept = delete;
    ~Item() override = default;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _proto->GetName(); }
    [[nodiscard]] auto GetProtoItem() const noexcept -> const ProtoItem* { return static_cast<const ProtoItem*>(_proto.get()); }
    [[nodiscard]] auto GetInnerItem(ident_t item_id) noexcept -> Item*;
    [[nodiscard]] auto GetInnerItemByPid(hstring pid, const any_t& stack_id) noexcept -> Item*;
    [[nodiscard]] auto GetInnerItems(const any_t& stack_id) -> vector<Item*>;
    [[nodiscard]] auto HasInnerItems() const noexcept -> bool;
    [[nodiscard]] auto GetAllInnerItems() -> const vector<Item*>&;
    [[nodiscard]] auto GetRawInnerItems() -> vector<Item*>&;
    [[nodiscard]] auto CanSendItem(bool as_public) const noexcept -> bool;
    [[nodiscard]] auto HasMultihexEntries() const noexcept -> bool { return !!_multihexEntries; }
    [[nodiscard]] auto GetMultihexEntries() const noexcept -> const vector<mpos>& { return *_multihexEntries; }

    auto AddItemToContainer(Item* item, const any_t& stack_id) -> Item*;
    void RemoveItemFromContainer(Item* item);
    void SetItemToContainer(Item* item);
    void SetMultihexEntries(vector<mpos> entries);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterWalk, Critter* /*critter*/, bool /*isIn*/, uint8 /*dir*/);

    ScriptFunc<bool, Critter*, StaticItem*, Item*, any_t> StaticScriptFunc {};
    ScriptFunc<void, Critter*, StaticItem*, bool, uint8> TriggerScriptFunc {};

private:
    unique_ptr<vector<Item*>> _innerItems {};
    unique_ptr<vector<mpos>> _multihexEntries {};
};

FO_END_NAMESPACE();
