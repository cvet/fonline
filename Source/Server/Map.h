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
#include "GeometryHelper.h"
#include "MapLoader.h"
#include "ScriptSystem.h"
#include "Settings.h"

#define FO_API_MAP_HEADER
#include "ScriptApi.h"

class Item;
using ItemVec = vector<Item*>;
using ItemMap = map<uint, Item*>;
class Critter;
using CritterMap = map<uint, Critter*>;
using CritterVec = vector<Critter*>;
class Npc;
using NpcMap = map<uint, Npc*>;
using NpcVec = vector<Npc*>;
class Client;
using ClientMap = map<uint, Client*>;
using ClientVec = vector<Client*>;
class Map;
using MapVec = vector<Map*>;
using MapMap = map<uint, Map*>;
class Location;
using LocationVec = vector<Location*>;
using LocationMap = map<uint, Location*>;

struct StaticMap : public NonCopyable
{
    UCharVec SceneryData {};
    hash HashTiles {};
    hash HashScen {};
    CritterVec CrittersVec {};
    ItemVec AllItemsVec {};
    ItemVec HexItemsVec {};
    ItemVec ChildItemsVec {};
    ItemVec StaticItemsVec {};
    ItemVec TriggerItemsVec {};
    uchar* HexFlags {};
    vector<MapTile> Tiles {};
};

class Map : public Entity
{
    friend class MapManager;

public:
    Map(uint id, ProtoMap* proto, Location* location, StaticMap* static_map, MapSettings& sett,
        ScriptSystem& script_sys);
    ~Map();
    StaticMap* GetStaticMap() { return staticMap; }

    void PlaceItemBlocks(ushort hx, ushort hy, Item* item);
    void RemoveItemBlocks(ushort hx, ushort hy, Item* item);

    void Process();
    void ProcessLoop(int index, uint time, uint tick);

    ProtoMap* GetProtoMap();
    Location* GetLocation();
    void SetLocation(Location* loc);

    void SetText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text);
    void SetTextMsg(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str);
    void SetTextMsgLex(
        ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, const char* lexems, ushort lexems_len);

    bool FindStartHex(ushort& hx, ushort& hy, uint multihex, uint seek_radius, bool skip_unsafe);

    void AddCritter(Critter* cr);
    void EraseCritter(Critter* cr);

    bool AddItem(Item* item, ushort hx, ushort hy);
    void SetItem(Item* item, ushort hx, ushort hy);
    void EraseItem(uint item_id);
    void SendProperty(NetProperty::Type type, Property* prop, Entity* entity);
    void ChangeViewItem(Item* item);
    void AnimateItem(Item* item, uchar from_frm, uchar to_frm);

    Item* GetItem(uint item_id);
    Item* GetItemHex(ushort hx, ushort hy, hash item_pid, Critter* picker);
    Item* GetItemGag(ushort hx, ushort hy);

    ItemVec GetItems();
    void GetItemsHex(ushort hx, ushort hy, ItemVec& items);
    void GetItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items);
    void GetItemsPid(hash pid, ItemVec& items);
    void GetItemsTrigger(ushort hx, ushort hy, ItemVec& items);

    bool IsPlaceForProtoItem(ushort hx, ushort hy, ProtoItem* proto_item);
    void RecacheHexFlags(ushort hx, ushort hy);
    ushort GetHexFlags(ushort hx, ushort hy);
    void SetHexFlag(ushort hx, ushort hy, uchar flag);
    void UnsetHexFlag(ushort hx, ushort hy, uchar flag);

    bool IsHexPassed(ushort hx, ushort hy);
    bool IsHexRaked(ushort hx, ushort hy);
    bool IsHexesPassed(ushort hx, ushort hy, uint radius);
    bool IsMovePassed(ushort hx, ushort hy, uchar dir, uint multihex);
    bool IsHexTrigger(ushort hx, ushort hy);
    bool IsHexCritter(ushort hx, ushort hy);
    bool IsHexGag(ushort hx, ushort hy);
    bool IsHexStaticTrigger(ushort hx, ushort hy);

    bool IsFlagCritter(ushort hx, ushort hy, bool dead);
    void SetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead);
    void UnsetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead);
    uint GetNpcCount(int npc_role, int find_type);
    Critter* GetCritter(uint crid);
    Critter* GetNpc(int npc_role, int find_type, uint skip_count);
    Critter* GetHexCritter(ushort hx, ushort hy, bool dead);
    void GetCrittersHex(ushort hx, ushort hy, uint radius, int find_type, CritterVec& critters);

    CritterVec GetCritters();
    ClientVec GetPlayers();
    NpcVec GetNpcs();
    CritterVec& GetCrittersRaw();
    ClientVec& GetPlayersRaw();
    NpcVec& GetNpcsRaw();
    uint GetCrittersCount();
    uint GetPlayersCount();
    uint GetNpcsCount();

    void SendEffect(hash eff_pid, ushort hx, ushort hy, ushort radius);
    void SendFlyEffect(
        hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);

    bool SetScript(asIScriptFunction* func, bool first_time);

    void GetStaticItemTriggers(ushort hx, ushort hy, ItemVec& triggers);
    Item* GetStaticItem(ushort hx, ushort hy, hash pid);
    void GetStaticItemsHex(ushort hx, ushort hy, ItemVec& items);
    void GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items);
    void GetStaticItemsByPid(hash pid, ItemVec& items);

#define FO_API_MAP_CLASS
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_MAP_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

private:
    MapSettings& settings;
    GeometryHelper geomHelper;
    ScriptSystem& scriptSys;
    StaticMap* staticMap {};
    uchar* hexFlags {};
    int hexFlagsSize {};
    CritterVec mapCritters {};
    ClientVec mapPlayers {};
    NpcVec mapNpcs {};
    ItemVec mapItems {};
    ItemMap mapItemsById {};
    map<uint, ItemVec> mapItemsByHex {};
    map<uint, ItemVec> mapBlockLinesByHex {};
    Location* mapLocation {};
    uint loopLastTick[5] {};
};
