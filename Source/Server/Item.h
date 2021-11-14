//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Entity.h"
#include "ServerScripting.h"

#define FO_API_ITEM_HEADER 1
#include "ScriptApi.h"

class Item;
class Critter;

class Item final : public Entity
{
    friend class Entity;
    friend class ItemManager;

public:
    Item() = delete;
    Item(uint id, const ProtoItem* proto, ServerScriptSystem& script_sys);
    Item(const Item&) = delete;
    Item(Item&&) noexcept = delete;
    auto operator=(const Item&) = delete;
    auto operator=(Item&&) noexcept = delete;
    ~Item() override = default;

    [[nodiscard]] auto IsStatic() const -> bool { return GetIsStatic(); }
    [[nodiscard]] auto IsAnyScenery() const -> bool { return IsScenery() || IsWall(); }
    [[nodiscard]] auto IsScenery() const -> bool { return GetIsScenery(); }
    [[nodiscard]] auto IsWall() const -> bool { return GetIsWall(); }
    [[nodiscard]] auto RadioIsSendActive() const -> bool { return !IsBitSet(GetRadioFlags(), RADIO_DISABLE_SEND); }
    [[nodiscard]] auto RadioIsRecvActive() const -> bool { return !IsBitSet(GetRadioFlags(), RADIO_DISABLE_RECV); }
    [[nodiscard]] auto GetProtoItem() const -> const ProtoItem* { return dynamic_cast<const ProtoItem*>(Proto); }
    [[nodiscard]] auto ContGetItem(uint item_id, bool skip_hide) -> Item*;
    [[nodiscard]] auto ContGetAllItems(bool skip_hidden) -> vector<Item*>;
    [[nodiscard]] auto ContGetItemByPid(hash pid, uint stack_id) -> Item*;
    [[nodiscard]] auto ContGetItems(uint stack_id) -> vector<Item*>;
    [[nodiscard]] auto ContIsItems() const -> bool;

    auto SetScript(string_view func, bool first_time) -> bool;
    void EvaluateSortValue(const vector<Item*>& items);
    void ChangeCount(int val);

    bool ViewPlaceOnMap {};
    ScriptFunc<bool, Critter*, Item*, bool, int> SceneryScriptFunc {};
    ScriptFunc<void, Critter*, Item*, bool, uchar> TriggerScriptFunc {};
    Critter* ViewByCritter {};

#define FO_API_ITEM_CLASS 1
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_ITEM_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

private:
    ServerScriptSystem& _scriptSys;
    vector<Item*>* _childItems {};
    bool _nonConstHelper {};
};
