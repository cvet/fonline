// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "ScriptSystem.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

// Run the init callback and store its name as the init script
///@ ExportMethod
FO_SCRIPT_API void Server_Location_SetupScript(ptr<Location> self, ScriptFunc<void, ptr<Location>, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }

    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

// Run the named init script and store its name
///@ ExportMethod
FO_SCRIPT_API void Server_Location_SetupScriptEx(ptr<Location> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

// Create a child map from a prototype id and attach it to a location
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Map> Server_Location_AddMap(ptr<Location> self, hstring mapPid)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a map to a location that is being destroyed", self->GetId());
    }

    auto map {self->GetEngine()->MapMngr.CreateMap(mapPid, self)};
    return map;
}

// Create a child map from a prototype descriptor and attach it to a location
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Map> Server_Location_AddMap(ptr<Location> self, ptr<ProtoMap> mapProto)
{
    ptr<const ProtoMap> map_proto_ptr {mapProto};

    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a map to a location that is being destroyed", self->GetId());
    }

    auto map {self->GetEngine()->MapMngr.CreateMap(map_proto_ptr->GetProtoId(), self)};
    return map;
}

// Return the number of child maps attached to this location
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Location_GetMapCount(ptr<Location> self)
{
    return numeric_cast<int32_t>(self->GetMapsCount());
}

// Return the first child map with the given prototype id, or null
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Map> Server_Location_GetMap(ptr<Location> self, hstring mapPid)
{
    vector<ptr<Map>> maps {self->GetMaps()};

    for (ptr<Map> map : maps) {
        if (map->GetProtoId() == mapPid) {
            return map;
        }
    }

    return nullptr;
}

// Return the first child map with the given prototype descriptor, or null
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Map> Server_Location_GetMap(ptr<Location> self, ptr<ProtoMap> mapProto)
{
    ptr<const ProtoMap> map_proto_ptr {mapProto};
    vector<ptr<Map>> maps {self->GetMaps()};

    for (ptr<Map> map : maps) {
        if (map->GetProtoId() == map_proto_ptr->GetProtoId()) {
            return map;
        }
    }

    return nullptr;
}

// Return the child map at the given index, or throw when it is missing
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Map> Server_Location_GetMapByIndex(ptr<Location> self, int32_t index)
{
    auto map {self->GetMapByIndex(index)};
    FO_VERIFY_AND_THROW(map, "Map is null");
    return map;
}

// Return all child maps attached to this location
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Map>> Server_Location_GetMaps(ptr<Location> self)
{
    vector<ptr<Map>> maps {self->GetMaps()};

    return maps;
}

// Regenerate every child map of a location that is not being destroyed
///@ ExportMethod
FO_SCRIPT_API void Server_Location_Regenerate(ptr<Location> self)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot regenerate a location that is being destroyed", self->GetId());
    }

    vector<ptr<Map>> maps {self->GetMaps()};

    for (ptr<Map> map : maps) {
        self->GetEngine()->MapMngr.RegenerateMap(map);
    }
}

FO_END_NAMESPACE
