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
#include "ServerScripting.h"
#include "Settings.h"
#include "Timer.h"

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

struct StaticMap
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

class Map final : public Entity
{
    friend class MapManager;

public:
    Map() = delete;
    Map(uint id, const ProtoMap* proto, Location* location, const StaticMap* static_map, MapSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time);
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetStaticMap() const -> const StaticMap* { return _staticMap; }

    [[nodiscard]] auto IsPlaceForProtoItem(ushort hx, ushort hy, const ProtoItem* proto_item) const -> bool;
    void PlaceItemBlocks(ushort hx, ushort hy, Item* item);
    void RemoveItemBlocks(ushort hx, ushort hy, Item* item);

    void Process();
    void ProcessLoop(int index, uint time, uint tick);

    [[nodiscard]] auto GetProtoMap() const -> const ProtoMap*;
    [[nodiscard]] auto GetLocation() -> Location*;
    void SetLocation(Location* loc);

    void SetText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text);
    void SetTextMsg(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str);
    void SetTextMsgLex(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, const char* lexems, ushort lexems_len);

    auto FindStartHex(ushort& hx, ushort& hy, uint multihex, uint seek_radius, bool skip_unsafe) const -> bool;
    auto FindPlaceOnMap(Critter* cr, ushort& hx, ushort& hy, uint radius) const -> bool;

    void AddCritter(Critter* cr);
    void EraseCritter(Critter* cr);

    auto AddItem(Item* item, ushort hx, ushort hy) -> bool;
    void SetItem(Item* item, ushort hx, ushort hy);
    void EraseItem(uint item_id);
    void SendProperty(NetProperty::Type type, Property* prop, Entity* entity);
    void ChangeViewItem(Item* item);
    void AnimateItem(Item* item, uchar from_frm, uchar to_frm);

    [[nodiscard]] auto GetItem(uint item_id) -> Item*;
    [[nodiscard]] auto GetItemHex(ushort hx, ushort hy, hash item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto GetItemGag(ushort hx, ushort hy) -> Item*;

    [[nodiscard]] auto GetItems() -> ItemVec;
    void GetItemsHex(ushort hx, ushort hy, ItemVec& items);
    void GetItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items);
    void GetItemsPid(hash pid, ItemVec& items);
    void GetItemsTrigger(ushort hx, ushort hy, ItemVec& traps);

    void RecacheHexFlags(ushort hx, ushort hy);
    [[nodiscard]] auto GetHexFlags(ushort hx, ushort hy) const -> ushort;
    void SetHexFlag(ushort hx, ushort hy, uchar flag);
    void UnsetHexFlag(ushort hx, ushort hy, uchar flag);

    [[nodiscard]] auto IsHexPassed(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexRaked(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexesPassed(ushort hx, ushort hy, uint radius) const -> bool;
    [[nodiscard]] auto IsMovePassed(ushort hx, ushort hy, uchar dir, uint multihex) const -> bool;
    [[nodiscard]] auto IsHexTrigger(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexCritter(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexGag(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexStaticTrigger(ushort hx, ushort hy) const -> bool;

    [[nodiscard]] auto IsFlagCritter(ushort hx, ushort hy, bool dead) const -> bool;
    void SetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead);
    void UnsetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead);
    [[nodiscard]] auto GetNpcCount(hash npc_role, int find_type) const -> uint;
    [[nodiscard]] auto GetCritter(uint crid) -> Critter*;
    [[nodiscard]] auto GetNpc(hash npc_role, int find_type, uint skip_count) -> Critter*;
    [[nodiscard]] auto GetHexCritter(ushort hx, ushort hy, bool dead) -> Critter*;
    void GetCrittersHex(ushort hx, ushort hy, uint radius, int find_type, CritterVec& critters);

    [[nodiscard]] auto GetCritters() -> CritterVec;
    [[nodiscard]] auto GetPlayers() -> ClientVec;
    [[nodiscard]] auto GetNpcs() -> NpcVec;
    [[nodiscard]] auto GetCrittersRaw() -> CritterVec&;
    [[nodiscard]] auto GetPlayersRaw() -> ClientVec&;
    [[nodiscard]] auto GetNpcsRaw() -> NpcVec&;
    [[nodiscard]] auto GetCrittersCount() const -> uint;
    [[nodiscard]] auto GetPlayersCount() const -> uint;
    [[nodiscard]] auto GetNpcsCount() const -> uint;

    void SendEffect(hash eff_pid, ushort hx, ushort hy, ushort radius);
    void SendFlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);

    auto SetScript(const string& func, bool first_time) -> bool;

    void GetStaticItemTriggers(ushort hx, ushort hy, ItemVec& triggers);
    [[nodiscard]] auto GetStaticItem(ushort hx, ushort hy, hash pid) -> Item*;
    void GetStaticItemsHex(ushort hx, ushort hy, ItemVec& items);
    void GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items);
    void GetStaticItemsByPid(hash pid, ItemVec& items);

#define FO_API_MAP_CLASS
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_MAP_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

private:
    MapSettings& _settings;
    GeometryHelper _geomHelper;
    ServerScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    const StaticMap* _staticMap {};
    uchar* _hexFlags {};
    int _hexFlagsSize {};
    CritterVec _mapCritters {};
    ClientVec _mapPlayers {};
    NpcVec _mapNpcs {};
    ItemVec _mapItems {};
    ItemMap _mapItemsById {};
    map<uint, ItemVec> _mapItemsByHex {};
    map<uint, ItemVec> _mapBlockLinesByHex {};
    Location* _mapLocation {};
    uint _loopLastTick[5] {};
};
