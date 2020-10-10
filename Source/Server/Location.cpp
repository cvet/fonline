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

#include "Location.h"
#include "Critter.h"
#include "Map.h"
#include "StringUtils.h"

#define FO_API_LOCATION_IMPL
#include "ScriptApi.h"

PROPERTIES_IMPL(Location, "Location", true);
#define FO_API_LOCATION_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(Location, access, type, name, __VA_ARGS__);
#include "ScriptApi.h"

Location::Location(uint id, const ProtoLocation* proto, ServerScriptSystem& script_sys) : Entity(id, EntityType::Location, PropertiesRegistrator, proto), _scriptSys {script_sys}
{
    RUNTIME_ASSERT(proto);
}

void Location::BindScript()
{
    EntranceScriptBindId = 0;

    if (GetEntranceScript() != 0u) {
        string func_name = _str().parseHash(GetEntranceScript());
        /*EntranceScriptBindId =
            scriptSys.BindByFuncName(func_name, "bool %s(Location, Critter[], uint8 entranceIndex)", false);*/
    }
}

auto Location::GetProtoLoc() const -> const ProtoLocation*
{
    return dynamic_cast<const ProtoLocation*>(Proto);
}

auto Location::IsLocVisible() const -> bool
{
    return !GetHidden() || GetGeckVisible() && GeckCount > 0;
}

auto Location::GetMapsRaw() -> MapVec&
{
    return _locMaps;
};

auto Location::GetMaps() const -> MapVec
{
    return _locMaps;
}

auto Location::GetMapsCount() const -> uint
{
    return static_cast<uint>(_locMaps.size());
}

auto Location::GetMapByIndex(uint index) -> Map*
{
    if (index >= _locMaps.size()) {
        return nullptr;
    }
    return _locMaps[index];
}

auto Location::GetMapByPid(hash map_pid) -> Map*
{
    for (auto* map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return map;
        }
    }
    return nullptr;
}

auto Location::GetMapIndex(hash map_pid) -> uint
{
    uint index = 0;
    for (auto* map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return index;
        }
        index++;
    }
    return static_cast<uint>(-1);
}

auto Location::IsCanEnter(uint players_count) -> bool
{
    const auto max_palyers = GetMaxPlayers();
    if (max_palyers == 0u) {
        return true;
    }

    for (auto* map : _locMaps) {
        players_count += map->GetPlayersCount();
        if (players_count >= max_palyers) {
            return false;
        }
    }
    return true;
}

auto Location::IsNoCrit() -> bool
{
    for (auto* map : _locMaps) {
        if (map->GetCrittersCount() != 0u) {
            return false;
        }
    }
    return true;
}

auto Location::IsNoPlayer() -> bool
{
    for (auto* map : _locMaps) {
        if (map->GetPlayersCount() != 0u) {
            return false;
        }
    }
    return true;
}

auto Location::IsNoNpc() -> bool
{
    for (auto* map : _locMaps) {
        if (map->GetNpcsCount() != 0u) {
            return false;
        }
    }
    return true;
}

auto Location::IsCanDelete() -> bool
{
    if (GeckCount > 0) {
        return false;
    }

    // Check for players
    for (auto* map : _locMaps) {
        if (map->GetPlayersCount() != 0u) {
            return false;
        }
    }

    // Check for npc
    auto maps = _locMaps;
    for (auto* map : maps) {
        for (auto* npc : map->GetNpcs()) {
            if (npc->GetIsGeck() || !npc->GetIsNoHome() && npc->GetHomeMapId() != map->GetId() || npc->IsHaveGeckItem()) {
                return false;
            }
        }
    }
    return true;
}
