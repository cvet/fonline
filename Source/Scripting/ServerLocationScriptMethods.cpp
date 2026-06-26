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

#include "Common.h"

#include "ScriptSystem.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

static auto ReturnScriptMap(ptr<Map> map) noexcept -> Map*
{
    FO_NO_STACK_TRACE_ENTRY();

    return map.get_no_const();
}

static auto ReturnNullableScriptMap(nptr<Map> map) noexcept -> Map*
{
    FO_NO_STACK_TRACE_ENTRY();

    return map.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Location_SetupScript(ptr<Location> self, ScriptFunc<void, Map*, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }

    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Location_SetupScriptEx(ptr<Location> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API ptr<Map> Server_Location_AddMap(ptr<Location> self, hstring mapPid)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a map to a location that is being destroyed", self->GetId());
    }

    auto map = self->GetEngine()->MapMngr.CreateMap(mapPid, self);
    return ReturnScriptMap(map);
}

///@ ExportMethod
FO_SCRIPT_API ptr<Map> Server_Location_AddMap(ptr<Location> self, ptr<ProtoMap> mapProto)
{
    ptr<const ProtoMap> map_proto_ptr = mapProto;

    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a map to a location that is being destroyed", self->GetId());
    }

    auto map = self->GetEngine()->MapMngr.CreateMap(map_proto_ptr->GetProtoId(), self);
    return ReturnScriptMap(map);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Location_GetMapCount(ptr<Location> self)
{
    return numeric_cast<int32_t>(self->GetMapsCount());
}

///@ ExportMethod
FO_SCRIPT_API nptr<Map> Server_Location_GetMap(ptr<Location> self, hstring mapPid)
{
    vector<ptr<Map>> maps = self->GetMaps();

    for (ptr<Map> map : maps) {
        if (map->GetProtoId() == mapPid) {
            return ReturnScriptMap(map);
        }
    }

    return ReturnNullableScriptMap(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API nptr<Map> Server_Location_GetMap(ptr<Location> self, ptr<ProtoMap> mapProto)
{
    ptr<const ProtoMap> map_proto_ptr = mapProto;
    vector<ptr<Map>> maps = self->GetMaps();

    for (ptr<Map> map : maps) {
        if (map->GetProtoId() == map_proto_ptr->GetProtoId()) {
            return ReturnScriptMap(map);
        }
    }

    return ReturnNullableScriptMap(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API ptr<Map> Server_Location_GetMapByIndex(ptr<Location> self, int32_t index)
{
    auto map = self->GetMapByIndex(index).as_ptr();
    return ReturnScriptMap(map);
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Location_GetMaps(ptr<Location> self)
{
    vector<ptr<Map>> maps = self->GetMaps();

    return MakeScriptHandleVector<Map>(maps);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Location_Regenerate(ptr<Location> self)
{
    vector<ptr<Map>> maps = self->GetMaps();

    for (ptr<Map> map : maps) {
        self->GetEngine()->MapMngr.RegenerateMap(map);
    }
}

FO_END_NAMESPACE
