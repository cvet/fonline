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

#include "Location.h"
#include "Critter.h"
#include "Map.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

Location::Location(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoLocation> proto, nptr<const Properties> props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME).as_ptr(), props ? props : nptr<const Properties> {&proto->GetProperties()}, &proto->GetProperties()),
    EntityWithProto(proto),
    LocationProperties(*GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    SetEntityLock(&_ownedLock);
}

Location::~Location()
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (!_engine->IsShutdownInProgress()) {
        FO_VERIFY_AND_CONTINUE(_locMaps.empty(), "Server location has maps during destruction", GetId(), _locMaps.size());
    }
}

auto Location::GetRawMaps() noexcept -> vector<refcount_ptr<Map>>&
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _locMaps;
}

auto Location::GetName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _proto->GetName();
}

auto Location::GetProtoLoc() const noexcept -> ptr<const ProtoLocation>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _proto.dyn_cast<ProtoLocation>().as_ptr();
}

auto Location::GetMaps() const -> vector<ptr<const Map>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    vector<ptr<const Map>> result;
    result.reserve(_locMaps.size());

    for (const auto& map : _locMaps) {
        result.emplace_back(map.as_ptr());
    }

    return result;
}

auto Location::GetMaps() -> vector<ptr<Map>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    vector<ptr<Map>> result;
    result.reserve(_locMaps.size());

    for (auto& map : _locMaps) {
        result.emplace_back(map.as_ptr());
    }

    return result;
}

auto Location::GetMapsCount() const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _locMaps.size();
}

auto Location::HasMaps() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return !_locMaps.empty();
}

auto Location::GetMapByIndex(int32_t index) noexcept -> nptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (index < 0 || index >= numeric_cast<int32_t>(_locMaps.size())) {
        return nullptr;
    }

    return _locMaps[index].as_nptr();
}

auto Location::GetMapByPid(hstring map_pid) noexcept -> nptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    for (auto& map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return map.as_nptr();
        }
    }

    return nullptr;
}

auto Location::GetMapIndex(hstring map_pid) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    size_t index = 0;

    for (const auto& map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return index;
        }

        index++;
    }

    throw GenericException("Map not found", map_pid);
}

void Location::RestoreMap(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    FO_VERIFY_AND_THROW(!IsDestroyed(), "Cannot add a map to an already destroyed location", GetId());
    FO_VERIFY_AND_THROW(!IsDestroying(), "Cannot add a map to a location that is being destroyed", GetId());
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Cannot add an already destroyed map to a location", map->GetId());
    FO_VERIFY_AND_THROW(!map->IsDestroying(), "Cannot add a map that is being destroyed to a location", map->GetId());

    const size_t maps_count = _locMaps.size();
    vec_add_unique_value(_locMaps, map.hold_ref());
    FO_VERIFY_AND_THROW(_locMaps.size() == maps_count + 1, "Restored map was not added to the location map list");
    ptr<Location> loc = this;
    map->SetLocation(loc);
}

void Location::AddMap(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    vec_add_unique_value(_locMaps, map.hold_ref());

    auto map_ids = GetMapIds();
    vec_add_unique_value(map_ids, map->GetId());
    SetMapIds(map_ids);

    map->SetLocId(GetId());
    map->SetLocMapIndex(numeric_cast<int32_t>(_locMaps.size()) - 1);
    ptr<Location> loc = this;
    map->SetLocation(loc);

    if (IsPersistent()) {
        _engine->EntityMngr.MakePersistent(map, true);
    }
}

void Location::RemoveMap(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    vec_remove_unique_value(_locMaps, map.hold_ref());

    auto map_ids = GetMapIds();
    vec_remove_unique_value(map_ids, map->GetId());
    SetMapIds(map_ids);

    map->SetLocId({});
    map->SetLocMapIndex({});
    map->SetLocation(nullptr);

    for (size_t index = 0; index < _locMaps.size(); index++) {
        _locMaps[index]->SetLocMapIndex(numeric_cast<int32_t>(index));
    }

    // Currently all maps are destroyed on this stage but in future maps can be reused or
    // moved to another location, so keep the persistence flag in sync with the location.
    if (map->IsPersistent() && !map->IsExplicitlyPersistent() && !map->IsDestroying() && !map->IsDestroyed()) {
        _engine->EntityMngr.MakePersistent(map, false);
    }
}

FO_END_NAMESPACE
