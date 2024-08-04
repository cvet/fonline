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

#include "Location.h"
#include "Critter.h"
#include "Map.h"
#include "Server.h"
#include "StringUtils.h"

Location::Location(FOServer* engine, ident_t id, const ProtoLocation* proto, const Properties* props) :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    LocationProperties(GetInitRef())
{
    STACK_TRACE_ENTRY();

    _name = _str("{}_{}", proto->GetName(), id);
}

void Location::BindScript()
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

    return static_cast<const ProtoLocation*>(_proto);
}

auto Location::IsLocVisible() const -> bool
{
    STACK_TRACE_ENTRY();

    return !GetHidden() || (GetGeckVisible() && GeckCount > 0);
}

auto Location::GetMapsRaw() -> vector<Map*>&
{
    STACK_TRACE_ENTRY();

    return _locMaps;
};

auto Location::GetMaps() -> vector<Map*>
{
    STACK_TRACE_ENTRY();

    return _locMaps;
}

auto Location::GetMaps() const -> vector<const Map*>
{
    STACK_TRACE_ENTRY();

    return vec_static_cast<const Map*>(_locMaps);
}

auto Location::GetMapsCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_locMaps.size());
}

auto Location::GetMapByIndex(uint index) -> Map*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (index >= _locMaps.size()) {
        return nullptr;
    }
    return _locMaps[index];
}

auto Location::GetMapByPid(hstring map_pid) -> Map*
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

    uint index = 0;
    for (const auto* map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return index;
        }
        index++;
    }
    return static_cast<uint>(-1);
}

auto Location::IsCanDelete() const -> bool
{
    STACK_TRACE_ENTRY();

    if (GeckCount > 0) {
        return false;
    }

    // Check for players
    for (const auto* map : _locMaps) {
        if (map->GetPlayerCrittersCount() != 0) {
            return false;
        }
    }

    // Check for npc
    for (auto* map : _locMaps) {
        for (const auto* npc : map->GetNonPlayerCritters()) {
            if (npc->GetIsGeck() || (!npc->GetIsNoHome() && npc->GetHomeMapId() != map->GetId()) || npc->IsHaveGeckItem()) {
                return false;
            }
        }
    }
    return true;
}
