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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    unordered_map<ident_t, StaticItem*> StaticItemsById {};
    vector<StaticItem*> TriggerItems {};
    vector<uint8> HexFlags {};
};

class Map final : public ServerEntity, public EntityWithProto, public MapProperties
{
    friend class MapManager;

public:
    Map() = delete;
    Map(FOServer* engine, ident_t id, const ProtoMap* proto, Location* location, const StaticMap* static_map, const Properties* props = nullptr);
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetStaticMap() const -> const StaticMap* { return _staticMap; }
    [[nodiscard]] auto GetProtoMap() const -> const ProtoMap*;
    [[nodiscard]] auto GetLocation() -> Location*;
    [[nodiscard]] auto GetLocation() const -> const Location*;
    [[nodiscard]] auto GetItem(ident_t item_id) -> Item*;
    [[nodiscard]] auto GetItemHex(uint16 hx, uint16 hy, hstring item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto GetItemGag(uint16 hx, uint16 hy) -> Item*;
    [[nodiscard]] auto GetItems() -> const vector<Item*>&;
    [[nodiscard]] auto GetItemsHex(uint16 hx, uint16 hy) -> vector<Item*>;
    [[nodiscard]] auto GetItemsHexEx(uint16 hx, uint16 hy, uint radius, hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsByProto(hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsTrigger(uint16 hx, uint16 hy) -> vector<Item*>;
    [[nodiscard]] auto IsPlaceForProtoItem(uint16 hx, uint16 hy, const ProtoItem* proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(uint16 hx, uint16 hy, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<tuple<uint16, uint16>>;
    [[nodiscard]] auto GetHexFlags(uint16 hx, uint16 hy) const -> uint16;
    [[nodiscard]] auto IsHexPassed(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexRaked(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexesPassed(uint16 hx, uint16 hy, uint radius) const -> bool;
    [[nodiscard]] auto IsMovePassed(const Critter* cr, uint16 to_hx, uint16 to_hy, uint multihex) -> bool;
    [[nodiscard]] auto IsHexTrigger(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexCritter(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexGag(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexBlockItem(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexStaticTrigger(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsFlagCritter(uint16 hx, uint16 hy, bool dead) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) -> Critter*;
    [[nodiscard]] auto GetHexCritter(uint16 hx, uint16 hy, bool dead) -> Critter*;
    [[nodiscard]] auto GetCrittersHex(uint16 hx, uint16 hy, uint radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetPlayerCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetNonPlayerCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetCrittersRaw() -> vector<Critter*>&;
    [[nodiscard]] auto GetPlayersRaw() -> vector<Critter*>&;
    [[nodiscard]] auto GetNpcsRaw() -> vector<Critter*>&;
    [[nodiscard]] auto GetCrittersCount() const -> uint;
    [[nodiscard]] auto GetPlayersCount() const -> uint;
    [[nodiscard]] auto GetNpcsCount() const -> uint;
    [[nodiscard]] auto GetStaticItem(ident_t id) -> StaticItem*;
    [[nodiscard]] auto GetStaticItem(uint16 hx, uint16 hy, hstring pid) -> StaticItem*;
    [[nodiscard]] auto GetStaticItemsHex(uint16 hx, uint16 hy) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsHexEx(uint16 hx, uint16 hy, uint radius, hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsByPid(hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsTrigger(uint16 hx, uint16 hy) -> vector<StaticItem*>;

    void SetLocation(Location* loc);
    void Process();
    void ProcessLoop(int index, uint time, uint tick);
    void PlaceItemBlocks(uint16 hx, uint16 hy, Item* item);
    void RemoveItemBlocks(uint16 hx, uint16 hy, Item* item);
    void SetText(uint16 hx, uint16 hy, uint color, string_view text, bool unsafe_text);
    void SetTextMsg(uint16 hx, uint16 hy, uint color, uint16 msg_num, uint str_num);
    void SetTextMsgLex(uint16 hx, uint16 hy, uint color, uint16 msg_num, uint str_num, string_view lexems);
    void AddCritter(Critter* cr);
    void EraseCritter(Critter* cr);
    auto AddItem(Item* item, uint16 hx, uint16 hy, Critter* dropper) -> bool;
    void SetItem(Item* item, uint16 hx, uint16 hy);
    void EraseItem(ident_t item_id);
    void SendProperty(NetProperty type, const Property* prop, ServerEntity* entity);
    void ChangeViewItem(Item* item);
    void AnimateItem(Item* item, hstring anim_name, bool looped, bool reversed);
    void SendEffect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius);
    void SendFlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy);
    void SetHexFlag(uint16 hx, uint16 hy, uint8 flag);
    void UnsetHexFlag(uint16 hx, uint16 hy, uint8 flag);
    void SetFlagCritter(uint16 hx, uint16 hy, uint multihex, bool dead);
    void UnsetFlagCritter(uint16 hx, uint16 hy, uint multihex, bool dead);
    void RecacheHexFlags(uint16 hx, uint16 hy);

    ///@ ExportEvent
    ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoop);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoopEx, int /*loopIndex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCheckLook, Critter* /*cr*/, Critter* /*target*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCheckTrapLook, Critter* /*cr*/, Item* /*item*/);

private:
    struct HexesHash
    {
        std::size_t operator()(const tuple<uint16, uint16>& hexes) const noexcept { return std::get<0>(hexes) << 16 | std::get<1>(hexes); }
    };

    const StaticMap* _staticMap {};
    uint8* _hexFlags {};
    int _hexFlagsSize {};
    vector<Critter*> _critters {};
    unordered_map<ident_t, Critter*> _crittersMap {};
    vector<Critter*> _playerCritters {};
    vector<Critter*> _nonPlayerCritters {};
    vector<Item*> _items {};
    unordered_map<ident_t, Item*> _itemsMap {};
    unordered_map<tuple<uint16, uint16>, vector<Item*>, HexesHash> _itemsByHex {};
    unordered_map<tuple<uint16, uint16>, vector<Item*>, HexesHash> _blockLinesByHex {};
    Location* _mapLocation {};
    uint _loopLastTick[5] {};
};
