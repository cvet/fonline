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

#include "Critter.h"
#include "Geometry.h"
#include "Item.h"
#include "Map.h"

FO_BEGIN_NAMESPACE

class ServerEngine;
class ProtoManager;
class EntityManager;
class MapManager;
class CritterManager;

FO_DECLARE_EXCEPTION(ItemManagerException);

class ItemManager final
{
public:
    ItemManager() = delete;
    explicit ItemManager(ptr<ServerEngine> engine);
    ItemManager(const ItemManager&) = delete;
    ItemManager(ItemManager&&) noexcept = delete;
    auto operator=(const ItemManager&) = delete;
    auto operator=(ItemManager&&) noexcept = delete;
    ~ItemManager() = default;

    auto CreateItem(hstring pid, int32_t count, nptr<const Properties> props) -> ptr<Item>;
    auto CreateItemOnHex(ptr<Map> map, mpos hex, hstring pid, int32_t count, nptr<const Properties> props) -> ptr<Item>;
    auto SplitItem(ptr<Item> item, int32_t count) -> nptr<Item>;
    auto AddItemContainer(ptr<Item> cont, hstring pid, int32_t count, const any_t& stack_id) -> nptr<Item>;
    auto AddItemCritter(ptr<Critter> cr, hstring pid, int32_t count) -> nptr<Item>;
    void SubItemCritter(ptr<Critter> cr, hstring pid, int32_t count);
    void SetItemCritter(ptr<Critter> cr, hstring pid, int32_t count);
    void DestroyItem(ptr<Item> item);
    auto MoveItem(ptr<Item> item, int32_t count, ptr<Critter> to_cr) -> nptr<Item>;
    auto MoveItem(ptr<Item> item, int32_t count, ptr<Map> to_map, mpos to_hex) -> nptr<Item>;
    auto MoveItem(ptr<Item> item, int32_t count, ptr<Item> to_cont, const any_t& stack_id) -> nptr<Item>;

private:
    auto GetItemHolder(ptr<Item> item) -> ptr<Entity>;
    void RemoveItemHolder(ptr<Item> item, ptr<Entity> holder);
    void RestoreSplitItem(ptr<Item> item, ptr<Item> splitted_item);

    ptr<ServerEngine> _engine;
};

FO_END_NAMESPACE
