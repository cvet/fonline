// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "EntitySync.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE

class Map;
class Location;

class Location final : public ServerEntity, public EntityWithProto, public LocationProperties
{
public:
    Location() = delete;
    Location(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoLocation> proto, nptr<const Properties> props = nullptr) noexcept;
    Location(const Location&) = delete;
    Location(Location&&) noexcept = delete;
    auto operator=(const Location&) = delete;
    auto operator=(Location&&) noexcept = delete;
    ~Location() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override;
    [[nodiscard]] auto GetProtoLoc() const noexcept -> ptr<const ProtoLocation>;
    [[nodiscard]] auto HasMaps() const -> bool;
    [[nodiscard]] auto GetMaps() const -> vector<ptr<const Map>>;
    [[nodiscard]] auto GetMaps() -> vector<ptr<Map>>;
    [[nodiscard]] auto GetRawMaps() noexcept -> vector<refcount_ptr<Map>>&;
    [[nodiscard]] auto GetMapByIndex(int32_t index) noexcept -> nptr<Map>;
    [[nodiscard]] auto GetMapsCount() const -> size_t;
    [[nodiscard]] auto GetMapByPid(hstring map_pid) noexcept -> nptr<Map>;
    [[nodiscard]] auto GetMapIndex(hstring map_pid) const -> size_t;

    void RestoreMap(ptr<Map> map);
    void AddMap(ptr<Map> map);
    void RemoveMap(ptr<Map> map);
    void ClearAllAssociations() noexcept;

    // Runs while this location is being finished
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    // Runs after a map is added to this location
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapAdded, ptr<Map> /*map*/);
    // Runs when a map is removed from this location
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapRemoved, ptr<Map> /*map*/);

private:
    vector<refcount_ptr<Map>> _locMaps {};
    EntityLock _ownedLock {};
};

FO_END_NAMESPACE
