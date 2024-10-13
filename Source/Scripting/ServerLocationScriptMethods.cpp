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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Common.h"

#include "Server.h"

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Location_SetupScript(Location* self, InitFunc<Map*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Location_SetupScriptEx(Location* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Location_GetMapCount(Location* self)
{
    return self->GetMapsCount();
}

///# ...
///# param mapPid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Location_GetMap(Location* self, hstring mapPid)
{
    for (auto* map : self->GetMaps()) {
        if (map->GetProtoId() == mapPid) {
            return map;
        }
    }
    return static_cast<Map*>(nullptr);
}

///# ...
///# param index ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Location_GetMapByIndex(Location* self, uint index)
{
    const auto maps = self->GetMaps();

    if (index >= maps.size()) {
        throw ScriptException("Invalid index arg");
    }

    return maps[index];
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Map*> Server_Location_GetMaps(Location* self)
{
    return self->GetMaps();
}

///# ...
///# param entranceIndex ...
///# param mapIndex ...
///# param entrance ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Location_GetEntrance(Location* self, uint entranceIndex, uint& mapIndex, hstring& entrance)
{
    const auto map_entrances = self->GetMapEntrances();
    const auto count = static_cast<uint>(map_entrances.size()) / 2u;
    if (entranceIndex >= count) {
        throw ScriptException("Invalid entrance");
    }

    mapIndex = self->GetMapIndex(map_entrances[entranceIndex * 2]);
    entrance = map_entrances[entranceIndex * 2 + 1];
}

///# ...
///# param mapsIndex ...
///# param entrances ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Location_GetEntrances(Location* self, vector<int>& mapsIndex, vector<hstring>& entrances)
{
    const auto map_entrances = self->GetMapEntrances();
    const auto count = static_cast<uint>(map_entrances.size()) / 2u;

    for (const auto i : xrange(count)) {
        int index = self->GetMapIndex(map_entrances[i * 2]);
        mapsIndex.push_back(index);
        auto entrance = map_entrances[i * 2 + 1];
        entrances.push_back(entrance);
    }

    return count;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Location_Regenerate(Location* self)
{
    for (auto* map : self->GetMaps()) {
        self->GetEngine()->MapMngr.RegenerateMap(map);
    }
}
