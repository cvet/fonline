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

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "MapLoader.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"

class Item;
using StaticItem = Item;
class Critter;
class Map;
class Location;

struct StaticMap
{
    vector<pair<uint, const Critter*>> CritterBillets {};
    vector<pair<uint, const Item*>> ItemBillets {};
    vector<pair<uint, const Item*>> HexItemBillets {};
    vector<pair<uint, const Item*>> ChildItemBillets {};
    vector<StaticItem*> StaticItems {};
    vector<StaticItem*> TriggerItems {};
    vector<uchar> HexFlags {};
};

class Map final : public ServerEntity, public EntityWithProto, public MapProperties
{
    friend class MapManager;

public:
    Map() = delete;
    Map(FOServer* engine, uint id, const ProtoMap* proto, Location* location, const StaticMap* static_map);
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetStaticMap() const -> const StaticMap* { return _staticMap; }
    [[nodiscard]] auto GetProtoMap() const -> const ProtoMap*;
    [[nodiscard]] auto GetLocation() -> Location*;
    [[nodiscard]] auto GetLocation() const -> const Location*;
    [[nodiscard]] auto GetItem(uint item_id) -> Item*;
    [[nodiscard]] auto GetItemHex(ushort hx, ushort hy, hstring item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto GetItemGag(ushort hx, ushort hy) -> Item*;
    [[nodiscard]] auto GetItems() -> vector<Item*>;
    [[nodiscard]] auto GetItemsHex(ushort hx, ushort hy) -> vector<Item*>;
    [[nodiscard]] auto GetItemsHexEx(ushort hx, ushort hy, uint radius, hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsByProto(hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsTrigger(ushort hx, ushort hy) -> vector<Item*>;
    [[nodiscard]] auto IsPlaceForProtoItem(ushort hx, ushort hy, const ProtoItem* proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(ushort hx, ushort hy, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<tuple<ushort, ushort>>;
    [[nodiscard]] auto FindPlaceOnMap(ushort hx, ushort hy, Critter* cr, uint radius) const -> optional<tuple<ushort, ushort>>;
    [[nodiscard]] auto GetHexFlags(ushort hx, ushort hy) const -> ushort;
    [[nodiscard]] auto IsHexPassed(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexRaked(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexesPassed(ushort hx, ushort hy, uint radius) const -> bool;
    [[nodiscard]] auto IsMovePassed(Critter* cr, ushort to_hx, ushort to_hy, uint multihex) -> bool;
    [[nodiscard]] auto IsHexTrigger(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexCritter(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexGag(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexBlockItem(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsHexStaticTrigger(ushort hx, ushort hy) const -> bool;
    [[nodiscard]] auto IsFlagCritter(ushort hx, ushort hy, bool dead) const -> bool;
    [[nodiscard]] auto GetCritter(uint cr_id) -> Critter*;
    [[nodiscard]] auto GetHexCritter(ushort hx, ushort hy, bool dead) -> Critter*;
    [[nodiscard]] auto GetCrittersHex(ushort hx, ushort hy, uint radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritters() -> vector<Critter*>;
    [[nodiscard]] auto GetPlayers() -> vector<Critter*>;
    [[nodiscard]] auto GetNpcs() -> vector<Critter*>;
    [[nodiscard]] auto GetCrittersRaw() -> vector<Critter*>&;
    [[nodiscard]] auto GetPlayersRaw() -> vector<Critter*>&;
    [[nodiscard]] auto GetNpcsRaw() -> vector<Critter*>&;
    [[nodiscard]] auto GetCrittersCount() const -> uint;
    [[nodiscard]] auto GetPlayersCount() const -> uint;
    [[nodiscard]] auto GetNpcsCount() const -> uint;
    [[nodiscard]] auto GetStaticItemTriggers(ushort hx, ushort hy) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItem(ushort hx, ushort hy, hstring pid) -> StaticItem*;
    [[nodiscard]] auto GetStaticItemsHex(ushort hx, ushort hy) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsByPid(hstring pid) -> vector<StaticItem*>;

    void SetLocation(Location* loc);
    void Process();
    void ProcessLoop(int index, uint time, uint tick);
    void PlaceItemBlocks(ushort hx, ushort hy, Item* item);
    void RemoveItemBlocks(ushort hx, ushort hy, Item* item);
    void SetText(ushort hx, ushort hy, uint color, string_view text, bool unsafe_text);
    void SetTextMsg(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str);
    void SetTextMsgLex(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, string_view lexems);
    void AddCritter(Critter* cr);
    void EraseCritter(Critter* cr);
    auto AddItem(Item* item, ushort hx, ushort hy) -> bool;
    void SetItem(Item* item, ushort hx, ushort hy);
    void EraseItem(uint item_id);
    void SendProperty(NetProperty type, const Property* prop, ServerEntity* entity);
    void ChangeViewItem(Item* item);
    void AnimateItem(Item* item, uchar from_frm, uchar to_frm);
    void SendEffect(hstring eff_pid, ushort hx, ushort hy, ushort radius);
    void SendFlyEffect(hstring eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);
    void SetHexFlag(ushort hx, ushort hy, uchar flag);
    void UnsetHexFlag(ushort hx, ushort hy, uchar flag);
    void SetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead);
    void UnsetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead);
    void RecacheHexFlags(ushort hx, ushort hy);

    ///@ ExportEvent
    ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoop);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoopEx, int /*loopIndex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterIn, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterOut, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCheckLook, Critter* /*cr*/, Critter* /*target*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCheckTrapLook, Critter* /*cr*/, Item* /*item*/);

private:
    struct HexesHash
    {
        std::size_t operator()(const tuple<ushort, ushort>& hexes) const noexcept { return std::get<0>(hexes) << 16 | std::get<1>(hexes); }
    };

    const StaticMap* _staticMap {};
    uchar* _hexFlags {};
    int _hexFlagsSize {};
    vector<Critter*> _critters {};
    unordered_map<uint, Critter*> _crittersMap {};
    vector<Critter*> _playerCritters {};
    vector<Critter*> _nonPlayerCritters {};
    vector<Item*> _items {};
    unordered_map<uint, Item*> _itemsMap {};
    unordered_map<tuple<ushort, ushort>, vector<Item*>, HexesHash> _itemsByHex {};
    unordered_map<tuple<ushort, ushort>, vector<Item*>, HexesHash> _blockLinesByHex {};
    Location* _mapLocation {};
    uint _loopLastTick[5] {};
};
