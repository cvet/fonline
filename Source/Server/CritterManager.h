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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Player.h"
#include "Settings.h"
#include "Timer.h"

class FOServer;
class ProtoManager;
class EntityManager;
class MapManager;
class ItemManager;

class CritterManager final
{
public:
    CritterManager() = delete;
    explicit CritterManager(FOServer* engine);
    CritterManager(const CritterManager&) = delete;
    CritterManager(CritterManager&&) noexcept = delete;
    auto operator=(const CritterManager&) = delete;
    auto operator=(CritterManager&&) noexcept = delete;
    ~CritterManager() = default;

    [[nodiscard]] auto GetNonPlayerCritters() -> vector<Critter*>;
    [[nodiscard]] auto GetPlayerCritters(bool on_global_map_only) -> vector<Critter*>;
    [[nodiscard]] auto GetGlobalMapCritters(uint16 wx, uint16 wy, uint radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritter(ident_t cr_id) -> Critter*;
    [[nodiscard]] auto GetCritter(ident_t cr_id) const -> const Critter*;
    [[nodiscard]] auto GetPlayerById(ident_t id) -> Player*;
    [[nodiscard]] auto GetPlayerByName(string_view name) -> Player*;
    [[nodiscard]] auto GetItemByPidInvPriority(Critter* cr, hstring item_pid) -> Item*;
    [[nodiscard]] auto PlayersInGame() const -> size_t;
    [[nodiscard]] auto CrittersInGame() const -> size_t;

    auto CreateCritter(hstring proto_id, const Properties* props, Map* map, uint16 hx, uint16 hy, uint8 dir, bool accuracy) -> Critter*;
    void DeleteCritter(Critter* cr);
    void DeleteInventory(Critter* cr);
    auto AddItemToCritter(Critter* cr, Item* item, bool send) -> Item*;
    void EraseItemFromCritter(Critter* cr, Item* item, bool send);
    void ProcessTalk(Critter* cr, bool force);
    void CloseTalk(Critter* cr);

private:
    FOServer* _engine;
    bool _nonConstHelper {};
};
