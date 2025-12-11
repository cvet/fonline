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

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "Geometry.h"
#include "MapLoader.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE();

class Item;
using StaticItem = Item;
class Critter;
class Map;
class Location;

struct StaticMap
{
    struct Field
    {
        bool MoveBlocked {};
        bool ShootBlocked {};
        vector<raw_ptr<StaticItem>> StaticItems {};
        vector<raw_ptr<StaticItem>> TriggerItems {};
    };

    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> HexField {};
    vector<pair<ident_t, refcount_ptr<Critter>>> CritterBillets {};
    vector<pair<ident_t, refcount_ptr<Item>>> ItemBillets {};
    vector<pair<ident_t, raw_ptr<Item>>> HexItemBillets {};
    vector<pair<ident_t, raw_ptr<Item>>> ChildItemBillets {};
    vector<raw_ptr<StaticItem>> StaticItems {};
    unordered_map<ident_t, raw_ptr<StaticItem>> StaticItemsById {};
};

class Map final : public ServerEntity, public EntityWithProto, public MapProperties
{
public:
    Map() = delete;
    Map(FOServer* engine, ident_t id, const ProtoMap* proto, Location* location, StaticMap* static_map, const Properties* props = nullptr) noexcept;
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _proto->GetName(); }
    [[nodiscard]] auto GetStaticMap() const noexcept -> const StaticMap* { return _staticMap.get(); }
    [[nodiscard]] auto GetProtoMap() const noexcept -> const ProtoMap* { return static_cast<const ProtoMap*>(_proto.get()); }
    [[nodiscard]] auto GetLocation() noexcept -> Location* { return _mapLocation.get(); }
    [[nodiscard]] auto GetLocation() const noexcept -> const Location* { return _mapLocation.get(); }
    [[nodiscard]] auto IsHexMovable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexShootable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, int32 radius) const -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, int32 radius, Critter* ignore_cr) -> bool;
    [[nodiscard]] auto IsBlockItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsTriggerItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsGagItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id) noexcept -> Item*;
    [[nodiscard]] auto GetItemOnHex(mpos hex, hstring item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto GetGagItemOnHex(mpos hex) const noexcept -> const Item*;
    [[nodiscard]] auto HasItems() const noexcept -> bool { return !_items.empty(); }
    [[nodiscard]] auto GetItems() noexcept -> span<raw_ptr<Item>> { return _items; }
    [[nodiscard]] auto GetItemsOnHex(mpos hex) noexcept -> span<raw_ptr<Item>>;
    [[nodiscard]] auto GetItemsInRadius(mpos hex, int32 radius) -> vector<raw_ptr<Item>>;
    [[nodiscard]] auto GetTriggerItemsOnHex(mpos hex) -> vector<Item*>;
    [[nodiscard]] auto IsValidPlaceForItem(mpos hex, const ProtoItem* proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(mpos hex, int32 multihex, int32 seek_radius, bool skip_unsafe) const -> optional<mpos>;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, CritterFindType find_type) const -> bool;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, const Critter* cr) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) noexcept -> Critter*;
    [[nodiscard]] auto GetCritterOnHex(mpos hex, CritterFindType find_type) noexcept -> Critter*;
    [[nodiscard]] auto HasCritters() const noexcept -> bool { return !_critters.empty(); }
    [[nodiscard]] auto GetCritters() noexcept -> span<raw_ptr<Critter>> { return _critters; }
    [[nodiscard]] auto GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCrittersInRadius(mpos hex, int32 radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetPlayerCritters() noexcept -> span<raw_ptr<Critter>> { return _playerCritters; }
    [[nodiscard]] auto GetNonPlayerCritters() noexcept -> span<raw_ptr<Critter>> { return _nonPlayerCritters; }
    [[nodiscard]] auto IsTriggerStaticItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetStaticItem(ident_t id) noexcept -> StaticItem*;
    [[nodiscard]] auto GetStaticItemOnHex(mpos hex, hstring pid) noexcept -> StaticItem*;
    [[nodiscard]] auto GetStaticItems() noexcept -> span<raw_ptr<StaticItem>> { return _staticMap->StaticItems; }
    [[nodiscard]] auto GetStaticItems(hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsOnHex(mpos hex) noexcept -> span<raw_ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItemsInRadius(mpos hex, int32 radius, hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetTriggerStaticItemsOnHex(mpos hex) noexcept -> span<raw_ptr<StaticItem>>;
    [[nodiscard]] auto IsOutsideArea(mpos hex) const -> bool;

    void SetLocation(Location* loc) noexcept;
    void AddCritter(Critter* cr);
    void RemoveCritter(Critter* cr);
    void AddItem(Item* item, mpos hex, Critter* dropper);
    void SetItem(Item* item);
    void RemoveItem(ident_t item_id);
    void SendProperty(NetProperty type, const Property* prop, ServerEntity* entity);
    void ChangeViewItem(Item* item);
    void SetHexManualBlock(mpos hex, bool enable, bool full);
    void AddCritterToField(Critter* cr);
    void RemoveCritterFromField(Critter* cr);
    void RecacheHexFlags(mpos hex);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCheckLook, Critter* /*cr*/, Critter* /*target*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCheckTrapLook, Critter* /*cr*/, Item* /*item*/);

private:
    struct Field
    {
        bool HasCritter {};
        bool HasBlockCritter {};
        bool HasGagItem {};
        bool HasTriggerItem {};
        bool HasNoMoveItem {};
        bool HasNoShootItem {};
        bool MoveBlocked {};
        bool ShootBlocked {};
        vector<raw_ptr<Critter>> Critters {};
        vector<raw_ptr<Item>> Items {};
        bool ManualBlock {};
        bool ManualBlockFull {};
    };

    void SetMultihexCritter(Critter* cr, bool set);
    void RecacheHexFlags(Field& field);

    raw_ptr<StaticMap> _staticMap {};
    msize _mapSize {};
    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> _hexField {};
    vector<raw_ptr<Critter>> _critters {};
    unordered_map<ident_t, raw_ptr<Critter>> _crittersMap {};
    vector<raw_ptr<Critter>> _playerCritters {};
    vector<raw_ptr<Critter>> _nonPlayerCritters {};
    vector<raw_ptr<Item>> _items {};
    unordered_map<ident_t, raw_ptr<Item>> _itemsMap {};
    raw_ptr<Location> _mapLocation {};
};

FO_END_NAMESPACE();
