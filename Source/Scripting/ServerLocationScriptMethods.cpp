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

#include "Common.h"

#include "Server.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod
FO_SCRIPT_API void Server_Location_SetupScript(Location* self, InitFunc<Map*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Location_SetupScriptEx(Location* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Location_AddMap(Location* self, hstring mapPid)
{
    Map* map = self->GetEngine()->MapMngr.CreateMap(mapPid, self);
    return map;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Location_GetMapCount(Location* self)
{
    return numeric_cast<int32>(self->GetMapsCount());
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Location_GetMap(Location* self, hstring mapPid)
{
    for (auto& map : self->GetMaps()) {
        if (map->GetProtoId() == mapPid) {
            return map.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Location_GetMapByIndex(Location* self, int32 index)
{
    auto& maps = self->GetMaps();

    if (index < 0 || index >= numeric_cast<int32>(maps.size())) {
        throw ScriptException("Invalid index arg", index);
    }

    return maps[index].get();
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Location_GetMaps(Location* self)
{
    auto& maps = self->GetMaps();
    vector<Map*> result = vec_transform(maps, [](auto&& map) -> Map* { return map.get(); });
    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Location_Regenerate(Location* self)
{
    for (auto& map : self->GetMaps()) {
        self->GetEngine()->MapMngr.RegenerateMap(map.get());
    }
}

FO_END_NAMESPACE();
