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
#include "DataBase.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "ScriptSystem.h"
#include "Settings.h"

class ProtoManager;
class EntityManager;
class MapManager;
class ItemManager;

class CritterManager
{
public:
    CritterManager(ServerSettings& sett, ProtoManager& proto_mngr, EntityManager& entity_mngr, MapManager& map_mngr,
        ItemManager& item_mngr, ScriptSystem& script_sys);

    Npc* CreateNpc(hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy);
    bool RestoreNpc(uint id, hash proto_id, const DataBase::Document& doc);
    void DeleteNpc(Critter* cr);
    void DeleteInventory(Critter* cr);

    void AddItemToCritter(Critter* cr, Item*& item, bool send);
    void EraseItemFromCritter(Critter* cr, Item* item, bool send);

    void GetCritters(CritterVec& critters);
    void GetNpcs(NpcVec& npcs);
    void GetClients(ClientVec& players, bool on_global_map = false);
    void GetGlobalMapCritters(ushort wx, ushort wy, uint radius, int find_type, CritterVec& critters);
    Critter* GetCritter(uint crid);
    Client* GetPlayer(uint crid);
    Client* GetPlayer(const char* name);
    Npc* GetNpc(uint crid);

    Item* GetItemByPidInvPriority(Critter* cr, hash item_pid);

    void ProcessTalk(Client* cl, bool force);
    void CloseTalk(Client* cl);

    uint PlayersInGame();
    uint NpcInGame();
    uint CrittersInGame();

private:
    ServerSettings& settings;
    GeometryHelper geomHelper;
    ProtoManager& protoMngr;
    EntityManager& entityMngr;
    MapManager& mapMngr;
    ItemManager& itemMngr;
    ScriptSystem& scriptSys;
};
