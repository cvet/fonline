// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "EntitySync.h"
#include "Geometry.h"
#include "MapLoader.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"
#include "StaticMap.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

class Item;
class Critter;
class Map;
class Location;
class Player;

class Map final : public ServerEntity, public EntityWithProto, public MapProperties
{
public:
    Map() = delete;
    Map(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoMap> proto, nptr<Location> location, ptr<StaticMap> static_map, nptr<const Properties> props = nullptr);
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override;
    [[nodiscard]] auto GetStaticMap() const noexcept -> ptr<const StaticMap>;
    [[nodiscard]] auto GetProtoMap() const noexcept -> ptr<const ProtoMap>;
    [[nodiscard]] auto GetLocation() noexcept -> nptr<Location>;
    [[nodiscard]] auto GetLocation() const noexcept -> nptr<const Location>;
    [[nodiscard]] auto IsHexMovable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexShootable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, int32_t radius) const -> bool;
    [[nodiscard]] auto GetMovableHexesInSight(mpos origin, int32_t radius) const -> vector<mpos>;
    [[nodiscard]] auto HasPathfindingObstacle(mpos hex, nptr<const Critter> ignore_cr) const noexcept -> bool;
    [[nodiscard]] auto IsBlockItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsTriggerItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id) noexcept -> nptr<Item>;
    [[nodiscard]] auto GetItemOnHex(mpos hex, hstring item_pid, nptr<const Critter> picker) -> nptr<Item>;
    [[nodiscard]] auto HasItems() const noexcept -> bool;
    [[nodiscard]] auto GetItems() noexcept -> span<ptr<Item>>;
    [[nodiscard]] auto GetItems() const noexcept -> const_span<ptr<Item>>;
    [[nodiscard]] auto GetItemsOnHex(mpos hex) noexcept -> vector<ptr<Item>>;
    [[nodiscard]] auto GetItemsInRadius(mpos hex, int32_t radius) -> vector<ptr<Item>>;
    [[nodiscard]] auto GetTriggerItemsOnHex(mpos hex) noexcept -> vector<ptr<Item>>;
    [[nodiscard]] auto IsValidPlaceForItem(mpos hex, ptr<const ProtoItem> proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(mpos hex, int32_t multihex, int32_t seek_radius, bool skip_unsafe) const -> optional<mpos>;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, CritterFindType find_type) const -> bool;
    [[nodiscard]] auto IsCritterOnHex(mpos hex, ptr<const Critter> cr) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) noexcept -> nptr<Critter>;
    [[nodiscard]] auto GetCritterOnHex(mpos hex, CritterFindType find_type) noexcept -> nptr<Critter>;
    [[nodiscard]] auto HasCritters() const noexcept -> bool;
    [[nodiscard]] auto GetCritters() noexcept -> span<ptr<Critter>>;
    [[nodiscard]] auto GetCritters() const noexcept -> const_span<ptr<Critter>>;
    [[nodiscard]] auto GetCritters(CritterFindType find_type) -> vector<ptr<Critter>>;
    [[nodiscard]] auto GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<ptr<Critter>>;
    [[nodiscard]] auto GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<ptr<const Critter>>;
    [[nodiscard]] auto GetCrittersInRadius(mpos hex, int32_t radius, CritterFindType find_type) -> vector<ptr<Critter>>;
    [[nodiscard]] auto GetPlayerCritters() noexcept -> span<ptr<Critter>>;
    [[nodiscard]] auto GetPlayerCritters() const noexcept -> const_span<ptr<Critter>>;
    [[nodiscard]] auto GetNonPlayerCritters() noexcept -> span<ptr<Critter>>;
    [[nodiscard]] auto GetNonPlayerCritters() const noexcept -> const_span<ptr<Critter>>;
    [[nodiscard]] auto IsTriggerStaticItemOnHex(mpos hex) const noexcept -> bool;
    [[nodiscard]] bool HasSpectatorPlayers() const noexcept FO_TSA_NO_ANALYSIS;
    [[nodiscard]] span<ptr<Player>> GetSpectatorPlayers() noexcept FO_TSA_NO_ANALYSIS;
    [[nodiscard]] const_span<ptr<Player>> GetSpectatorPlayers() const noexcept FO_TSA_NO_ANALYSIS;
    [[nodiscard]] auto GetSpectatorPlayersForSend() -> vector<refcount_ptr<Player>>;
    [[nodiscard]] auto GetSpectatorPlayersForSend(ident_t entity_id, mpos hex) -> vector<refcount_ptr<Player>>;
    [[nodiscard]] bool IsSpectatorEntityVisible(ident_t player_id, ident_t entity_id);
    [[nodiscard]] auto GetSpectatorCritterVisibility(ident_t player_id, ident_t critter_id) -> CritterVisibilityMode;
    [[nodiscard]] auto GetStaticItem(ident_t id) const noexcept -> nptr<StaticItem>;
    [[nodiscard]] auto GetStaticItemOnHex(mpos hex, hstring pid) const noexcept -> nptr<StaticItem>;
    [[nodiscard]] auto GetStaticItems() const noexcept -> const_span<ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItems(hstring pid) const -> vector<ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItemsOnHex(mpos hex) const noexcept -> const_span<ptr<StaticItem>>;
    [[nodiscard]] auto GetStaticItemsInRadius(mpos hex, int32_t radius, hstring pid) const -> vector<ptr<StaticItem>>;
    [[nodiscard]] auto GetTriggerStaticItemsOnHex(mpos hex) const noexcept -> const_span<ptr<StaticItem>>;
    [[nodiscard]] auto HasStaticItem(ident_t static_item_id) const noexcept -> bool;
    [[nodiscard]] auto IsRuntimeStaticItem(ident_t static_item_id) const noexcept -> bool;
    [[nodiscard]] auto HasRemovedStaticItems() const noexcept -> bool;
    [[nodiscard]] auto IsOutsideArea(mpos hex) const noexcept -> bool;

    void SetLocation(nptr<Location> loc) noexcept;
    auto RemoveStaticItem(ident_t static_item_id) -> bool;
    void AddRuntimeStaticItems(span<refcount_ptr<StaticItem>> static_items);
    void RemoveRuntimeStaticItems(const unordered_set<ident_t>& static_item_ids);
    void RefreshRemovedStaticItems();
    void AddCritter(ptr<Critter> cr);
    void RemoveCritter(ptr<Critter> cr);
    void RefreshCritterPlayerState(ptr<Critter> cr);
    void AddSpectatorPlayer(ptr<Player> player);
    void RemoveSpectatorPlayer(ptr<Player> player);
    void SetSpectatorArea(ptr<Player> player, mpos hex, int32_t radius, nptr<Entity> context = nullptr);
    void RefreshSpectatorVisibility(ptr<Player> player);
    void RefreshAreaSpectators(ptr<const Critter> cr, bool removed = false);
    void RefreshAreaSpectators(ptr<const Item> item, bool removed = false);
    void AddItem(ptr<Item> item, mpos hex, nptr<Critter> dropper);
    void SetItem(ptr<Item> item);
    void RemoveItem(ident_t item_id);
    void SendProperty(NetProperty type, ptr<const Property> prop, ptr<ServerEntity> entity);
    void ChangeViewItem(ptr<Item> item);
    void SetHexManualBlock(mpos hex, bool enable, bool full);
    void AddCritterToField(ptr<Critter> cr);
    void RemoveCritterFromField(ptr<Critter> cr);
    void MoveCritterOnField(ptr<Critter> cr, mpos hex);
    void RecacheHexFlags(mpos hex);
    void VerifyTrigger(ptr<Critter> cr, mpos from_hex, mpos to_hex, mdir dir);
    auto CheckGagItems(mpos hex, int32_t radius, const function<bool(ptr<const Item>)>& gag_callback) const -> bool;
    auto CheckGagItem(mpos hex, const function<bool(ptr<const Item>)>& gag_callback) const -> bool;
    void ClearAllAssociations() noexcept;

    // Runs while this map is being finished
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    // Exposes a project-defined critter visibility check; Engine does not fire it directly
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCheckLook, ptr<Critter> /*cr*/, ptr<Critter> /*target*/);
    // Exposes a project-defined trap visibility check; Engine does not fire it directly
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCheckTrapLook, ptr<Critter> /*cr*/, ptr<Item> /*item*/);

private:
    struct SpectatorArea
    {
        mpos Hex {};
        int32_t Radius {};
        refcount_nptr<Entity> Context {};
        unordered_set<ident_t> VisibleEntities {};
        unordered_map<ident_t, CritterVisibilityMode> CritterModes {};
        bool RefreshPending {};
    };

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

    auto CreateHexField(string_view grid_type) -> unique_ptr<TwoDimensionalGrid<Field, mpos, msize>>;
    auto GetStaticField(mpos hex) const noexcept -> const StaticMap::Field&;
    void RebuildStaticOverlay();
    void RefreshStaticFieldOverride(mpos hex);
    void RebuildVisibleStaticItems();

    void SetMultihexCritter(ptr<Critter> cr, bool set);
    void RecacheHexFlags(ptr<Field> field);
    auto IsMapItemContextChanged(ptr<const Item> item, ident_t map_id, mpos hex) const -> bool;
    auto GetSpectatorAreas() -> vector<pair<refcount_ptr<Player>, shared_ptr<SpectatorArea>>>;
    auto HasSpectatorContextCover(const SpectatorArea& area) const -> bool;
    auto GetSpectatorCritterVisibility(const SpectatorArea& area, ptr<const Critter> cr) const -> CritterVisibilityMode;
    auto IsSpectatorItemVisible(const SpectatorArea& area, ptr<const Item> item) const -> bool;
    void ScheduleSpectatorRefresh(ptr<Player> player, shared_ptr<SpectatorArea> area);
    void RefreshSpectatorVisibility(ptr<Player> player, shared_ptr<SpectatorArea> area);
    void RefreshSpectatorCritter(ptr<Player> player, shared_ptr<SpectatorArea> area, ptr<const Critter> cr, bool removed, bool send_movement);
    void RefreshSpectatorItem(ptr<Player> player, shared_ptr<SpectatorArea> area, ptr<const Item> item, bool removed);

    ptr<const ProtoMap> _protoMap;
    ptr<StaticMap> _staticMap;
    msize _mapSize {};
    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> _hexField;
    vector<ptr<Critter>> _critters {};
    unordered_map<ident_t, ptr<Critter>> _crittersMap {};
    vector<ptr<Critter>> _playerCritters {};
    vector<ptr<Critter>> _nonPlayerCritters {};
    vector<ptr<Item>> _items {};
    unordered_map<ident_t, ptr<Item>> _itemsMap {};
    unordered_set<ident_t> _removedStaticItems {};
    unordered_map<mpos, StaticMap::Field> _staticFieldOverrides {};
    vector<ptr<StaticItem>> _visibleStaticItems {};
    unique_nptr<StaticMap> _runtimeStatics {};
    nptr<Location> _mapLocation {};
    shared_mutex _spectatorLock {};
    vector<ptr<Player>> _spectatorPlayers FO_TSA_GUARDED_BY(_spectatorLock) {};
    unordered_map<ident_t, shared_ptr<SpectatorArea>> _spectatorAreas FO_TSA_GUARDED_BY(_spectatorLock) {};
    EntityLock _ownedLock {};
};

FO_END_NAMESPACE
