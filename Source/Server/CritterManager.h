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
#include "Player.h"
#include "Settings.h"

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
    [[nodiscard]] auto GetGlobalMapCritters(CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetItemByPidInvPriority(Critter* cr, hstring item_pid) -> Item*;

    auto CreateCritterOnMap(hstring proto_id, const Properties* props, Map* map, mpos hex, uint8 dir) -> NON_NULL Critter*;
    void DestroyCritter(Critter* cr);
    void DestroyInventory(Critter* cr);
    auto AddItemToCritter(Critter* cr, Item* item, bool send) -> Item*;
    void RemoveItemFromCritter(Critter* cr, Item* item, bool send);
    void ProcessTalk(Critter* cr, bool force);
    void CloseTalk(Critter* cr);

private:
    FOServer* _engine;
    bool _nonConstHelper {};
};
