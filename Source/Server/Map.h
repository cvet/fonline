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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "EntitySync.h"
#include "Geometry.h"
#include "MapLoader.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

class Item;
class StaticItem;
class Critter;
class Map;
class Location;
class Player;

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
    vector<pair<ident_t, refcount_ptr<StaticItem>>> ItemBillets {};
    vector<pair<ident_t, raw_ptr<StaticItem>>> HexItemBillets {};
    vector<pair<ident_t, raw_ptr<StaticItem>>> ChildItemBillets {};
    vector<raw_ptr<StaticItem>> StaticItems {};
    unordered_map<ident_t, raw_ptr<StaticItem>> StaticItemsById {};
};

class Map final : public ServerEntity, public EntityWithProto, public MapProperties
{
public:
    Map() = delete;
    Map(ServerEngine* engine, ident_t id, const ProtoMap* proto, Location* location, StaticMap* static_map, const Properties* props = nullptr) noexcept;
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override;
    [[nodiscard]] auto GetStaticMap() const noexcept -> const StaticMap*;
    [[nodiscard]] auto GetProtoMap() const noexcept -> const ProtoMap*;
    [[nodiscard]] auto GetLocation() noexcept -> Location*;
    [[nodiscard]] auto GetLocation() const noexcept -> const Location*;
    [[nodiscard]] auto IsHexMovable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexShootable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, int32_t radius) const -> bool;
    [[nodiscard]] auto HasLivingCritter(mpos hex, const Critter* ignore_cr) const noexcept -> bool;
    [[nodiscard]] auto IsBlockItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsTriggerItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id) noexcept -> Item*;
    [[nodiscard]] auto GetItemOnHex(mpos hex, hstring item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto HasItems() const noexcept -> bool;
    [[nodiscard]] auto GetItems() noexcept -> span<raw_ptr<Item>>;
    [[nodiscard]] auto GetItems() const noexcept -> const_span<raw_ptr<Item>>;
    [[nodiscard]] auto GetItemsOnHex(mpos hex) noexcept -> vector<raw_ptr<Item>>;
    [[nodiscard]] auto GetItemsInRadius(mpos hex, int32_t radius) -> vector<raw_ptr<Item>>;
    [[nodiscard]] auto GetTriggerItemsOnHex(mpos hex) noexcept -> vector<Item*>;
    [[nodiscard]] auto IsValidPlaceForItem(mpos hex, const ProtoItem* proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(mpos hex, int32_t multihex, int32_t seek_radius, bool skip_unsafe) const -> optional<mpos>;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, CritterFindType find_type) const -> bool;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, const Critter* cr) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) noexcept -> Critter*;
    [[nodiscard]] auto GetCritterOnHex(mpos hex, CritterFindType find_type) noexcept -> Critter*;
    [[nodiscard]] auto HasCritters() const noexcept -> bool;
    [[nodiscard]] auto GetCritters() noexcept -> span<raw_ptr<Critter>>;
    [[nodiscard]] auto GetCritters() const noexcept -> const_span<raw_ptr<Critter>>;
    [[nodiscard]] auto GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<const Critter*>;
    [[nodiscard]] auto GetCrittersInRadius(mpos hex, int32_t radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetPlayerCritters() noexcept -> span<raw_ptr<Critter>>;
    [[nodiscard]] auto GetPlayerCritters() const noexcept -> const_span<raw_ptr<Critter>>;
    [[nodiscard]] auto GetNonPlayerCritters() noexcept -> span<raw_ptr<Critter>>;
    [[nodiscard]] auto GetNonPlayerCritters() const noexcept -> const_span<raw_ptr<Critter>>;
    [[nodiscard]] auto IsTriggerStaticItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] bool HasSpectatorPlayers() const noexcept FO_TSA_NO_ANALYSIS;
    [[nodiscard]] span<raw_ptr<Player>> GetSpectatorPlayers() noexcept FO_TSA_NO_ANALYSIS;
    [[nodiscard]] const_span<raw_ptr<Player>> GetSpectatorPlayers() const noexcept FO_TSA_NO_ANALYSIS;
    [[nodiscard]] auto GetSpectatorPlayersForSend() -> vector<refcount_ptr<Player>>;
    [[nodiscard]] auto GetStaticItem(ident_t id) noexcept -> StaticItem*;
    [[nodiscard]] auto GetStaticItemOnHex(mpos hex, hstring pid) noexcept -> StaticItem*;
    [[nodiscard]] auto GetStaticItems() noexcept -> span<raw_ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItems() const noexcept -> const_span<raw_ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItems(hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsOnHex(mpos hex) noexcept -> span<raw_ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItemsInRadius(mpos hex, int32_t radius, hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetTriggerStaticItemsOnHex(mpos hex) noexcept -> span<raw_ptr<StaticItem>>;
    [[nodiscard]] auto IsOutsideArea(mpos hex) const noexcept -> bool;

    void SetLocation(Location* loc) noexcept;
    void AddCritter(Critter* cr);
    void RemoveCritter(Critter* cr);
    void RefreshCritterPlayerState(Critter* cr);
    void AddSpectatorPlayer(Player* player);
    void RemoveSpectatorPlayer(Player* player);
    void AddItem(Item* item, mpos hex, Critter* dropper);
    void SetItem(Item* item);
    void RemoveItem(ident_t item_id);
    void RefreshItemMultihex(Item* item);
    void SendProperty(NetProperty type, const Property* prop, ServerEntity* entity);
    void ChangeViewItem(Item* item);
    void SetHexManualBlock(mpos hex, bool enable, bool full);
    void AddCritterToField(Critter* cr);
    void RemoveCritterFromField(Critter* cr);
    void RecacheHexFlags(mpos hex);
    void VerifyTrigger(Critter* cr, mpos from_hex, mpos to_hex, mdir dir);
    auto CheckGagItems(mpos hex, int32_t radius, const function<bool(const Item*)>& gag_callback) const -> bool;
    auto CheckGagItem(mpos hex, const function<bool(const Item*)>& gag_callback) const -> bool;

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
        bool HasTriggerItem {};
        bool HasNoMoveItem {};
        bool HasNoShootItem {};
        bool MovableWithGag {};
        bool MoveBlocked {};
        bool ShootBlocked {};
        vector<raw_ptr<Critter>> Critters {};
        vector<raw_ptr<Item>> Items {};
        bool ManualBlock {};
        bool ManualBlockFull {};
    };

    void SetMultihexCritter(Critter* cr, bool set);
    void RecacheHexFlags(Field& field);
    [[nodiscard]] auto IsMapItemContextChanged(const Item* item, ident_t map_id, mpos hex) const -> bool;

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
    // Declared before _spectatorPlayers so it outlives the data it guards.
    shared_mutex _spectatorLock {};
    // FO_TSA_GUARDED_BY(_spectatorLock): the lock-free GetSpectatorPlayersForSend snapshot and the
    // Add/RemoveSpectatorPlayer mutators reach this without the map's entity cover, so TSA enforces the lock
    // there. The entity-cover getters (HasSpectatorPlayers/GetSpectatorPlayers) and the ~Map teardown invariant
    // reach it under the cooperative entity cover and are FO_TSA_NO_ANALYSIS.
    vector<raw_ptr<Player>> _spectatorPlayers FO_TSA_GUARDED_BY(_spectatorLock) {};
    EntityLock _ownedLock {};
};

FO_END_NAMESPACE
