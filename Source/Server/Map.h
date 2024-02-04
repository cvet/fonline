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
#include "TwoDimensionalGrid.h"

class Item;
using StaticItem = Item;
class Critter;
class Map;
class Location;

struct StaticMap
{
    // Todo: memory optimization - improve dynamic grid for map hex fields
    struct Field
    {
        bool IsMoveBlocked {};
        bool IsShootBlocked {};
        vector<StaticItem*> StaticItems {};
        vector<StaticItem*> TriggerItems {};
    };

    TwoDimensionalGrid<Field, mpos, msize, true> HexField {};
    vector<pair<uint, const Critter*>> CritterBillets {};
    vector<pair<uint, const Item*>> ItemBillets {};
    vector<pair<uint, const Item*>> HexItemBillets {};
    vector<pair<uint, const Item*>> ChildItemBillets {};
    vector<StaticItem*> StaticItems {};
    unordered_map<ident_t, StaticItem*> StaticItemsById {};
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
    [[nodiscard]] auto IsHexMovable(mpos hex) const -> bool;
    [[nodiscard]] auto IsHexShootable(mpos hex) const -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, uint radius) const -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, uint radius, Critter* skip_cr) -> bool;
    [[nodiscard]] auto IsBlockItem(mpos hex) const -> bool;
    [[nodiscard]] auto IsItemTrigger(mpos hex) const -> bool;
    [[nodiscard]] auto IsItemGag(mpos hex) const -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id) -> Item*;
    [[nodiscard]] auto GetItemHex(mpos hex, hstring item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto GetItemGag(mpos hex) -> Item*;
    [[nodiscard]] auto GetItems() -> const vector<Item*>&;
    [[nodiscard]] auto GetItems(mpos hex) -> const vector<Item*>&;
    [[nodiscard]] auto GetItemsInRadius(mpos hex, uint radius, hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsByProto(hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsTrigger(mpos hex) -> vector<Item*>;
    [[nodiscard]] auto IsPlaceForProtoItem(mpos hex, const ProtoItem* proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(mpos hex, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<mpos>;
    [[nodiscard]] auto IsAnyCritter(mpos hex) const -> bool;
    [[nodiscard]] auto IsNonDeadCritter(mpos hex) const -> bool;
    [[nodiscard]] auto IsDeadCritter(mpos hex) const -> bool;
    [[nodiscard]] auto IsCritter(mpos hex, const Critter* cr) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) -> Critter*;
    [[nodiscard]] auto GetNonDeadCritter(mpos hex) -> Critter*;
    [[nodiscard]] auto GetDeadCritter(mpos hex) -> Critter*;
    [[nodiscard]] auto GetCritters(mpos hex, uint radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetPlayerCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetNonPlayerCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetCrittersCount() const -> uint;
    [[nodiscard]] auto GetPlayerCrittersCount() const -> uint;
    [[nodiscard]] auto GetNonPlayerCrittersCount() const -> uint;
    [[nodiscard]] auto IsStaticItemTrigger(mpos hex) const -> bool;
    [[nodiscard]] auto GetStaticItem(ident_t id) -> StaticItem*;
    [[nodiscard]] auto GetStaticItem(mpos hex, hstring pid) -> StaticItem*;
    [[nodiscard]] auto GetStaticItemsHex(mpos hex) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsHexEx(mpos hex, uint radius, hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsByPid(hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsTrigger(mpos hex) -> vector<StaticItem*>;

    void SetLocation(Location* loc);
    void Process();
    void ProcessLoop(int index, uint time, uint tick);
    void SetText(mpos hex, uint color, string_view text, bool unsafe_text);
    void SetTextMsg(mpos hex, uint color, uint16 msg_num, uint str_num);
    void SetTextMsgLex(mpos hex, uint color, uint16 msg_num, uint str_num, string_view lexems);
    void AddCritter(Critter* cr);
    void EraseCritter(Critter* cr);
    void AddItem(Item* item, mpos hex, Critter* dropper);
    void SetItem(Item* item, mpos hex);
    void EraseItem(ident_t item_id);
    void SendProperty(NetProperty type, const Property* prop, ServerEntity* entity);
    void ChangeViewItem(Item* item);
    void AnimateItem(Item* item, hstring anim_name, bool looped, bool reversed);
    void SendEffect(hstring eff_pid, mpos hex, uint16 radius);
    void SendFlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, mpos from_hex, mpos to_hex);
    void SetHexManualBlock(mpos hex, bool enable, bool full);
    void AddCritterToField(Critter* cr);
    void RemoveCritterFromField(Critter* cr);
    void RecacheHexFlags(mpos hex);

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
    struct Field
    {
        bool IsNonDeadCritter {};
        bool IsDeadCritter {};
        bool IsGagItem {};
        bool IsTriggerItem {};
        bool IsNoMoveItem {};
        bool IsNoShootItem {};
        bool IsMoveBlocked {};
        bool IsShootBlocked {};
        vector<Critter*> Critters {};
        vector<Critter*> MultihexCritters {};
        vector<Item*> Items {};
        vector<Item*> BlockLines {};
        bool ManualBlock {};
        bool ManualBlockFull {};
    };

    void SetMultihexCritter(Critter* cr, bool set);
    void RecacheHexFlags(Field& field);

    const StaticMap* _staticMap {};
    msize _mapSize {};
    TwoDimensionalGrid<Field, mpos, msize, true> _hexField {};
    vector<Critter*> _critters {};
    unordered_map<ident_t, Critter*> _crittersMap {};
    vector<Critter*> _playerCritters {};
    vector<Critter*> _nonPlayerCritters {};
    vector<Item*> _items {};
    unordered_map<ident_t, Item*> _itemsMap {};
    Location* _mapLocation {};
    uint _loopLastTick[5] {};
};
