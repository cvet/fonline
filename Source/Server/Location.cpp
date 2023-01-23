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

#include "Location.h"
#include "Critter.h"
#include "Map.h"
#include "Server.h"
#include "StringUtils.h"

Location::Location(FOServer* engine, uint id, const ProtoLocation* proto) : ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME)), EntityWithProto(this, proto), LocationProperties(GetInitRef())
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(proto);
}

void Location::BindScript()
{
    PROFILER_ENTRY();

    EntranceScriptBindId = 0;

    if (const auto func_name = GetEntranceScript()) {
        UNUSED_VARIABLE(func_name);
        // Todo: EntranceScriptBindId
        throw NotImplementedException(LINE_STR);
        /*EntranceScriptBindId =
            scriptSys.BindByFuncName(GetEntranceScript(), "bool %s(Location, Critter[], uint8 entranceIndex)", false);*/
    }
}

auto Location::GetProtoLoc() const -> const ProtoLocation*
{
    PROFILER_ENTRY();

    return static_cast<const ProtoLocation*>(_proto);
}

auto Location::IsLocVisible() const -> bool
{
    PROFILER_ENTRY();

    return !GetHidden() || (GetGeckVisible() && GeckCount > 0);
}

auto Location::GetMapsRaw() -> vector<Map*>&
{
    PROFILER_ENTRY();

    return _locMaps;
};

auto Location::GetMaps() -> vector<Map*>
{
    PROFILER_ENTRY();

    return _locMaps;
}

auto Location::GetMaps() const -> vector<const Map*>
{
    PROFILER_ENTRY();

    vector<const Map*> maps;
    maps.insert(maps.begin(), _locMaps.begin(), _locMaps.end());
    return maps;
}

auto Location::GetMapsCount() const -> uint
{
    PROFILER_ENTRY();

    return static_cast<uint>(_locMaps.size());
}

auto Location::GetMapByIndex(uint index) -> Map*
{
    PROFILER_ENTRY();

    NON_CONST_METHOD_HINT();

    if (index >= _locMaps.size()) {
        return nullptr;
    }
    return _locMaps[index];
}

auto Location::GetMapByPid(hstring map_pid) -> Map*
{
    PROFILER_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return map;
        }
    }
    return nullptr;
}

auto Location::GetMapIndex(hstring map_pid) const -> uint
{
    PROFILER_ENTRY();

    uint index = 0;
    for (const auto* map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return index;
        }
        index++;
    }
    return static_cast<uint>(-1);
}

auto Location::IsCanEnter(uint players_count) const -> bool
{
    PROFILER_ENTRY();

    const auto max_palyers = GetMaxPlayers();
    if (max_palyers == 0u) {
        return true;
    }

    for (const auto* map : _locMaps) {
        players_count += map->GetPlayersCount();
        if (players_count >= max_palyers) {
            return false;
        }
    }
    return true;
}

auto Location::IsNoCritter() const -> bool
{
    PROFILER_ENTRY();

    for (const auto* map : _locMaps) {
        if (map->GetCrittersCount() != 0u) {
            return false;
        }
    }
    return true;
}

auto Location::IsNoPlayer() const -> bool
{
    PROFILER_ENTRY();

    for (const auto* map : _locMaps) {
        if (map->GetPlayersCount() != 0u) {
            return false;
        }
    }
    return true;
}

auto Location::IsNoNpc() const -> bool
{
    PROFILER_ENTRY();

    for (const auto* map : _locMaps) {
        if (map->GetNpcsCount() != 0u) {
            return false;
        }
    }
    return true;
}

auto Location::IsCanDelete() const -> bool
{
    PROFILER_ENTRY();

    if (GeckCount > 0) {
        return false;
    }

    // Check for players
    for (const auto* map : _locMaps) {
        if (map->GetPlayersCount() != 0u) {
            return false;
        }
    }

    // Check for npc
    for (auto* map : _locMaps) {
        for (const auto* npc : map->GetNpcs()) {
            if (npc->GetIsGeck() || (!npc->GetIsNoHome() && npc->GetHomeMapId() != map->GetId()) || npc->IsHaveGeckItem()) {
                return false;
            }
        }
    }
    return true;
}
