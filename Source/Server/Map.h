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
        vector<ptr<StaticItem>> StaticItems {};
        vector<ptr<StaticItem>> TriggerItems {};
    };

    StaticMap() = delete;
    StaticMap(msize map_size, bool static_grid) :
        HexField {CreateHexField(map_size, static_grid)}
    {
        FO_STACK_TRACE_ENTRY();
    }

    [[nodiscard]] static auto CreateHexField(msize map_size, bool static_grid) -> unique_ptr<TwoDimensionalGrid<Field, mpos, msize>>
    {
        FO_STACK_TRACE_ENTRY();

        if (static_grid) {
            return SafeAlloc::MakeUnique<StaticTwoDimensionalGrid<Field, mpos, msize>>(map_size);
        }

        return SafeAlloc::MakeUnique<DynamicTwoDimensionalGrid<Field, mpos, msize>>(map_size);
    }

    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> HexField;
    vector<pair<ident_t, refcount_ptr<Critter>>> CritterBillets {};
    vector<pair<ident_t, refcount_ptr<StaticItem>>> ItemBillets {};
    vector<pair<ident_t, ptr<StaticItem>>> HexItemBillets {};
    vector<pair<ident_t, ptr<StaticItem>>> ChildItemBillets {};
    vector<ptr<StaticItem>> StaticItems {};
    unordered_map<ident_t, ptr<StaticItem>> StaticItemsById {};
};

class Map final : public ServerEntity, public EntityWithProto, public MapProperties
{
public:
    Map() = delete;
    Map(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoMap> proto, nptr<Location> location, ptr<StaticMap> static_map, nptr<const Properties> props = nullptr) noexcept;
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return _protoMap->GetName();
    }
    [[nodiscard]] auto GetStaticMap() const noexcept -> ptr<const StaticMap>
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return _staticMap;
    }
    [[nodiscard]] auto GetProtoMap() const noexcept -> ptr<const ProtoMap>
    {
        FO_NO_VALIDATE_ENTITY_ACCESS();
        return _protoMap;
    }
    [[nodiscard]] auto GetLocation() noexcept -> nptr<Location>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _mapLocation;
    }
    [[nodiscard]] auto GetLocation() const noexcept -> nptr<const Location>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _mapLocation;
    }
    [[nodiscard]] auto IsHexMovable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexShootable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, int32_t radius) const -> bool;
    [[nodiscard]] auto HasLivingCritter(mpos hex, nptr<const Critter> ignore_cr) const noexcept -> bool;
    [[nodiscard]] auto IsBlockItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsTriggerItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id) noexcept -> nptr<Item>;
    [[nodiscard]] auto GetItemOnHex(mpos hex, hstring item_pid, nptr<const Critter> picker) -> nptr<Item>;
    [[nodiscard]] auto HasItems() const noexcept -> bool
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return !_items.empty();
    }
    [[nodiscard]] auto GetItems() noexcept -> span<ptr<Item>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _items;
    }
    [[nodiscard]] auto GetItems() const noexcept -> const_span<ptr<Item>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _items;
    }
    [[nodiscard]] auto GetItemsOnHex(mpos hex) noexcept -> vector<ptr<Item>>;
    [[nodiscard]] auto GetItemsInRadius(mpos hex, int32_t radius) -> vector<ptr<Item>>;
    [[nodiscard]] auto GetTriggerItemsOnHex(mpos hex) noexcept -> vector<ptr<Item>>;
    [[nodiscard]] auto IsValidPlaceForItem(mpos hex, ptr<const ProtoItem> proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(mpos hex, int32_t multihex, int32_t seek_radius, bool skip_unsafe) const -> optional<mpos>;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, CritterFindType find_type) const -> bool;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, ptr<const Critter> cr) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) noexcept -> nptr<Critter>;
    [[nodiscard]] auto GetCritterOnHex(mpos hex, CritterFindType find_type) noexcept -> nptr<Critter>;
    [[nodiscard]] auto HasCritters() const noexcept -> bool
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return !_critters.empty();
    }
    [[nodiscard]] auto GetCritters() noexcept -> span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _critters;
    }
    [[nodiscard]] auto GetCritters() const noexcept -> const_span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _critters;
    }
    [[nodiscard]] auto GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<ptr<Critter>>;
    [[nodiscard]] auto GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<ptr<const Critter>>;
    [[nodiscard]] auto GetCrittersInRadius(mpos hex, int32_t radius, CritterFindType find_type) -> vector<ptr<Critter>>;
    [[nodiscard]] auto GetPlayerCritters() noexcept -> span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _playerCritters;
    }
    [[nodiscard]] auto GetPlayerCritters() const noexcept -> const_span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _playerCritters;
    }
    [[nodiscard]] auto GetNonPlayerCritters() noexcept -> span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _nonPlayerCritters;
    }
    [[nodiscard]] auto GetNonPlayerCritters() const noexcept -> const_span<ptr<Critter>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _nonPlayerCritters;
    }
    [[nodiscard]] auto IsTriggerStaticItemOnHex(mpos hex) const noexcept -> bool;
    // The three getters below reach _spectatorPlayers under the map's entity cover (the cooperative scheme that
    // also excludes Add/RemoveSpectatorPlayer); GetSpectatorPlayers leaks a span by design, so the _spectatorLock
    // that guards _spectatorPlayers cannot be expressed here — hence FO_TSA_NO_ANALYSIS (leading return type per
    // the TSA doc). The lock-free recipient resolution uses GetSpectatorPlayersForSend instead.
    [[nodiscard]] bool HasSpectatorPlayers() const noexcept FO_TSA_NO_ANALYSIS
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return !_spectatorPlayers.empty();
    }
    [[nodiscard]] span<ptr<Player>> GetSpectatorPlayers() noexcept FO_TSA_NO_ANALYSIS
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _spectatorPlayers;
    }
    [[nodiscard]] const_span<ptr<Player>> GetSpectatorPlayers() const noexcept FO_TSA_NO_ANALYSIS
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _spectatorPlayers;
    }
    [[nodiscard]] auto GetSpectatorPlayersForSend() -> vector<refcount_ptr<Player>>;
    [[nodiscard]] auto GetStaticItem(ident_t id) noexcept -> nptr<StaticItem>;
    [[nodiscard]] auto GetStaticItemOnHex(mpos hex, hstring pid) noexcept -> nptr<StaticItem>;
    [[nodiscard]] auto GetStaticItems() noexcept -> span<ptr<StaticItem>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _staticMap->StaticItems;
    }
    [[nodiscard]] auto GetStaticItems() const noexcept -> const_span<ptr<StaticItem>>
    {
        FO_VALIDATE_ENTITY_ACCESS();
        return _staticMap->StaticItems;
    }
    [[nodiscard]] auto GetStaticItems(hstring pid) -> vector<ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItemsOnHex(mpos hex) noexcept -> span<ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItemsInRadius(mpos hex, int32_t radius, hstring pid) -> vector<ptr<StaticItem>>;
    [[nodiscard]] auto GetTriggerStaticItemsOnHex(mpos hex) noexcept -> span<ptr<StaticItem>>;
    [[nodiscard]] auto IsOutsideArea(mpos hex) const noexcept -> bool;

    void SetLocation(nptr<Location> loc) noexcept;
    void AddCritter(ptr<Critter> cr);
    void RemoveCritter(ptr<Critter> cr);
    void RefreshCritterPlayerState(ptr<Critter> cr);
    void AddSpectatorPlayer(ptr<Player> player);
    void RemoveSpectatorPlayer(ptr<Player> player);
    void AddItem(ptr<Item> item, mpos hex, nptr<Critter> dropper);
    void SetItem(ptr<Item> item);
    void RemoveItem(ident_t item_id);
    void SendProperty(NetProperty type, ptr<const Property> prop, ptr<ServerEntity> entity);
    void ChangeViewItem(ptr<Item> item);
    void SetHexManualBlock(mpos hex, bool enable, bool full);
    void AddCritterToField(ptr<Critter> cr);
    void RemoveCritterFromField(ptr<Critter> cr);
    void RecacheHexFlags(mpos hex);
    void VerifyTrigger(ptr<Critter> cr, mpos from_hex, mpos to_hex, mdir dir);
    auto CheckGagItems(mpos hex, int32_t radius, const function<bool(ptr<const Item>)>& gag_callback) const -> bool;
    auto CheckGagItem(mpos hex, const function<bool(ptr<const Item>)>& gag_callback) const -> bool;

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
        vector<ptr<Critter>> Critters {};
        vector<ptr<Item>> Items {};
        bool ManualBlock {};
        bool ManualBlockFull {};
    };

    [[nodiscard]] static auto CreateHexField(msize map_size, bool static_grid) -> unique_ptr<TwoDimensionalGrid<Field, mpos, msize>>;

    void SetMultihexCritter(ptr<Critter> cr, bool set);
    void RecacheHexFlags(ptr<Field> field);
    [[nodiscard]] auto IsMapItemContextChanged(ptr<const Item> item, ident_t map_id, mpos hex) const -> bool;

    ptr<const ProtoMap> _protoMap;
    ptr<StaticMap> _staticMap;
    msize _mapSize;
    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> _hexField;
    vector<ptr<Critter>> _critters {};
    unordered_map<ident_t, ptr<Critter>> _crittersMap {};
    vector<ptr<Critter>> _playerCritters {};
    vector<ptr<Critter>> _nonPlayerCritters {};
    vector<ptr<Item>> _items {};
    unordered_map<ident_t, ptr<Item>> _itemsMap {};
    nptr<Location> _mapLocation {};
    // Declared before _spectatorPlayers so it outlives the data it guards.
    shared_mutex _spectatorLock {};
    // FO_TSA_GUARDED_BY(_spectatorLock): the lock-free GetSpectatorPlayersForSend snapshot and the
    // Add/RemoveSpectatorPlayer mutators reach this without the map's entity cover, so TSA enforces the lock
    // there. The entity-cover getters (HasSpectatorPlayers/GetSpectatorPlayers) and the ~Map teardown invariant
    // reach it under the cooperative entity cover and are FO_TSA_NO_ANALYSIS.
    vector<ptr<Player>> _spectatorPlayers FO_TSA_GUARDED_BY(_spectatorLock) {};
    EntityLock _ownedLock {};
};

FO_END_NAMESPACE
