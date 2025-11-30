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

#include "Location.h"
#include "Critter.h"
#include "Map.h"
#include "Server.h"

FO_BEGIN_NAMESPACE();

Location::Location(FOServer* engine, ident_t id, const ProtoLocation* proto, const Properties* props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    LocationProperties(GetInitRef())
{
    FO_STACK_TRACE_ENTRY();
}

auto Location::GetMapByIndex(int32 index) noexcept -> Map*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (index < 0 || index >= numeric_cast<int32>(_locMaps.size())) {
        return nullptr;
    }

    return _locMaps[index].get();
}

auto Location::GetMapByPid(hstring map_pid) noexcept -> Map*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return map.get();
        }
    }

    return nullptr;
}

auto Location::GetMapIndex(hstring map_pid) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t index = 0;

    for (const auto& map : _locMaps) {
        if (map->GetProtoId() == map_pid) {
            return index;
        }

        index++;
    }

    throw GenericException("Map not found", map_pid);
}

void Location::AddMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);

    vec_add_unique_value(_locMaps, map);

    auto map_ids = GetMapIds();
    vec_add_unique_value(map_ids, map->GetId());
    SetMapIds(map_ids);

    map->SetLocId(GetId());
    map->SetLocMapIndex(numeric_cast<int32>(_locMaps.size()) - 1);
    map->SetLocation(this);
}

void Location::RemoveMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);

    vec_remove_unique_value(_locMaps, map);

    auto map_ids = GetMapIds();
    vec_remove_unique_value(map_ids, map->GetId());
    SetMapIds(map_ids);

    map->SetLocId({});
    map->SetLocMapIndex({});
    map->SetLocation(nullptr);
}

FO_END_NAMESPACE();
