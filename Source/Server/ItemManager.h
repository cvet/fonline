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
#include "Entity.h"
#include "Item.h"
#include "Map.h"

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

    auto CreateItem(hstring pid, uint count, const Properties* props) -> NON_NULL Item*;
    auto SplitItem(Item* item, uint count) -> NON_NULL Item*;
    auto AddItemContainer(Item* cont, hstring pid, uint count, const any_t& stack_id) -> NON_NULL Item*;
    auto AddItemCritter(Critter* cr, hstring pid, uint count) -> NON_NULL Item*;
    void SubItemCritter(Critter* cr, hstring pid, uint count);
    void SetItemCritter(Critter* cr, hstring pid, uint count);
    void DestroyItem(Item* item);
    auto MoveItem(Item* item, uint count, Critter* to_cr) -> NON_NULL Item*;
    auto MoveItem(Item* item, uint count, Map* to_map, mpos to_hex) -> NON_NULL Item*;
    auto MoveItem(Item* item, uint count, Item* to_cont, const any_t& stack_id) -> NON_NULL Item*;
    void RegisterRadio(Item* radio);
    void UnregisterRadio(Item* radio);
    void RadioSendText(Critter* cr, string_view text, bool unsafe_text, TextPackName text_pack, TextPackKey str_num, vector<uint16>& channels);
    void RadioSendTextEx(uint16 channel, uint8 broadcast_type, ident_t from_map_id, string_view text, bool unsafe_text, TextPackName text_pack, TextPackKey str_num, string_view lexems);

private:
    [[nodiscard]] auto GetItemHolder(Item* item) -> NON_NULL Entity*;

    void RemoveItemHolder(Item* item, Entity* holder);

    FOServer* _engine;
    unordered_set<Item*> _radioItems {};
    uint _radioSendCounter {};
    bool _nonConstHelper {};
};
