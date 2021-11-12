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

#include "Critter.h"
#include "Entity.h"
#include "Item.h"
#include "Map.h"
#include "ServerScripting.h"

class ProtoManager;
class EntityManager;
class MapManager;
class CritterManager;

class ItemManager final
{
public:
    ItemManager() = delete;
    ItemManager(ProtoManager& proto_mngr, EntityManager& entity_mngr, MapManager& map_mngr, CritterManager& cr_mngr, ServerScriptSystem& script_sys);
    ItemManager(const ItemManager&) = delete;
    ItemManager(ItemManager&&) noexcept = delete;
    auto operator=(const ItemManager&) = delete;
    auto operator=(ItemManager&&) noexcept = delete;
    ~ItemManager() = default;

    [[nodiscard]] auto GetItem(uint item_id) -> Item*;
    [[nodiscard]] auto GetItem(uint item_id) const -> const Item*;
    [[nodiscard]] auto GetItems() -> vector<Item*>;
    [[nodiscard]] auto GetItemsCount() const -> uint;
    [[nodiscard]] auto GetItemStatistics(hash pid) const -> int64;
    [[nodiscard]] auto GetItemsStatistics() const -> string;

    auto CreateItem(hash pid, uint count, const Properties* props) -> Item*;
    auto SplitItem(Item* item, uint count) -> Item*;
    auto AddItemContainer(Item* cont, hash pid, uint count, uint stack_id) -> Item*;
    auto AddItemCritter(Critter* cr, hash pid, uint count) -> Item*;
    auto SubItemCritter(Critter* cr, hash pid, uint count, vector<Item*>* erased_items) -> bool;
    auto SetItemCritter(Critter* cr, hash pid, uint count) -> bool;

    void LinkItems();
    void InitAfterLoad();
    void DeleteItem(Item* item);
    void MoveItem(Item* item, uint count, Critter* to_cr, bool skip_checks);
    void MoveItem(Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks);
    void MoveItem(Item* item, uint count, Item* to_cont, uint stack_id, bool skip_checks);
    void AddItemToContainer(Item* cont, Item*& item, uint stack_id);
    void EraseItemFromContainer(Item* cont, Item* item);
    void SetItemToContainer(Item* cont, Item* item);
    void RegisterRadio(Item* radio);
    void UnregisterRadio(Item* radio);
    void RadioSendText(Critter* cr, const string& text, bool unsafe_text, ushort text_msg, uint num_str, vector<ushort>& channels);
    void RadioSendTextEx(ushort channel, uchar broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy, const string& text, bool unsafe_text, ushort text_msg, uint num_str, const string& lexems);
    void ChangeItemStatistics(hash pid, int val) const;

private:
    [[nodiscard]] auto ItemCheckMove(Item* item, uint count, Entity* from, Entity* to) const -> bool;
    [[nodiscard]] auto GetItemHolder(Item* item) -> Entity*;

    void EraseItemHolder(Item* item, Entity* holder);

    ProtoManager& _protoMngr;
    EntityManager& _entityMngr;
    MapManager& _mapMngr;
    CritterManager& _crMngr;
    ServerScriptSystem& _scriptSys;
    vector<Item*> _radioItems {};
    bool _nonConstHelper {};
};
