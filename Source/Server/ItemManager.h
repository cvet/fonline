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

#include "Critter.h"
#include "Geometry.h"
#include "Item.h"
#include "Map.h"

FO_BEGIN_NAMESPACE();

class FOServer;
class ProtoManager;
class EntityManager;
class MapManager;
class CritterManager;

class ItemManager final
{
public:
    ItemManager() = delete;
    explicit ItemManager(FOServer* engine);
    ItemManager(const ItemManager&) = delete;
    ItemManager(ItemManager&&) noexcept = delete;
    auto operator=(const ItemManager&) = delete;
    auto operator=(ItemManager&&) noexcept = delete;
    ~ItemManager() = default;

    auto CreateItem(hstring pid, int32 count, const Properties* props) -> FO_NON_NULL Item*;
    auto SplitItem(Item* item, int32 count) -> FO_NON_NULL Item*;
    auto AddItemContainer(Item* cont, hstring pid, int32 count, const any_t& stack_id) -> FO_NON_NULL Item*;
    auto AddItemCritter(Critter* cr, hstring pid, int32 count) -> FO_NON_NULL Item*;
    void SubItemCritter(Critter* cr, hstring pid, int32 count);
    void SetItemCritter(Critter* cr, hstring pid, int32 count);
    void DestroyItem(Item* item);
    auto MoveItem(Item* item, int32 count, Critter* to_cr) -> FO_NON_NULL Item*;
    auto MoveItem(Item* item, int32 count, Map* to_map, mpos to_hex) -> FO_NON_NULL Item*;
    auto MoveItem(Item* item, int32 count, Item* to_cont, const any_t& stack_id) -> FO_NON_NULL Item*;

private:
    [[nodiscard]] auto GetItemHolder(Item* item) -> FO_NON_NULL Entity*;

    void RemoveItemHolder(Item* item, Entity* holder);

    raw_ptr<FOServer> _engine;
};

FO_END_NAMESPACE();
