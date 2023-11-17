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

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"

class Item;
class Critter;
using StaticItem = Item;

class Item final : public ServerEntity, public EntityWithProto, public ItemProperties
{
    friend class Entity;
    friend class ItemManager;

public:
    Item() = delete;
    Item(FOServer* engine, ident_t id, const ProtoItem* proto, const Properties* props = nullptr);
    Item(const Item&) = delete;
    Item(Item&&) noexcept = delete;
    auto operator=(const Item&) = delete;
    auto operator=(Item&&) noexcept = delete;
    ~Item() override = default;

    [[nodiscard]] auto RadioIsSendActive() const -> bool { return !IsBitSet(GetRadioFlags(), RADIO_DISABLE_SEND); }
    [[nodiscard]] auto RadioIsRecvActive() const -> bool { return !IsBitSet(GetRadioFlags(), RADIO_DISABLE_RECV); }
    [[nodiscard]] auto GetProtoItem() const -> const ProtoItem* { return static_cast<const ProtoItem*>(_proto); }
    [[nodiscard]] auto GetInnerItem(ident_t item_id, bool skip_hidden) -> Item*;
    [[nodiscard]] auto GetAllInnerItems(bool skip_hidden) -> vector<Item*>;
    [[nodiscard]] auto GetInnerItemByPid(hstring pid, uint stack_id) -> Item*;
    [[nodiscard]] auto GetInnerItems(uint stack_id) -> vector<Item*>;
    [[nodiscard]] auto IsInnerItems() const -> bool;
    [[nodiscard]] auto GetRawInnerItems() -> vector<Item*>&;

    void EvaluateSortValue(const vector<Item*>& items);

    ///@ ExportEvent
    ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterWalk, Critter* /*critter*/, bool /*isIn*/, uint8 /*dir*/);

    ScriptFunc<bool, Critter*, StaticItem*, Item*, int> SceneryScriptFunc {};
    ScriptFunc<void, Critter*, StaticItem*, bool, uint8> TriggerScriptFunc {};

private:
    unique_ptr<vector<Item*>> _innerItems {};
};
