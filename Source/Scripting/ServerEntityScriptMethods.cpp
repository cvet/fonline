// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "ScriptSystem.h"
#include "Server.h"
#include "TimeEvents.h"

FO_BEGIN_NAMESPACE

// Return whether this server entity is marked persistent
///@ ExportMethod
FO_SCRIPT_API bool Server_Entity_IsPersistent(ptr<ServerEntity> self)
{
    return self->IsPersistent();
}

// Apply the persistent flag and cascade it to child entities
///@ ExportMethod
FO_SCRIPT_API void Server_Entity_MakePersistent(ptr<ServerEntity> self, bool persistent)
{
    self->GetEngine()->EntityMngr.MakePersistent(self, persistent, true);
}

// A reference-leak diagnostic: every script wrapper and in-flight native pin is counted, so only a difference
// measured around a controlled sequence of calls means anything
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Entity_GetRefCount(ptr<ServerEntity> self)
{
    return self->GetRefCount();
}

// Register a one-shot time event and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, {});
}

// Register a one-shot time event with one data argument and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

// Register a one-shot time event with a data list and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, to_vector(data));
}

// Register a one-shot time event with a context argument and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, {});
}

// Register a one-shot time event with a context and one data argument and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

// Register a one-shot time event with a context and a data list and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, to_vector(data));
}

// Register a repeating time event and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, {});
}

// Register a repeating time event with one data argument and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

// Register a repeating time event with a data list and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, to_vector(data));
}

// Register a repeating time event with a context argument and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, {});
}

// Register a repeating time event with a context and one data argument and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

// Register a repeating time event with a context and a data list and return its id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Server_Entity_StartTimeEvent(ptr<ServerEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, to_vector(data));
}

// Count registered time events for this entity and callback, optionally excluding executing callbacks
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Server_Entity_CountTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>> func, bool excludeExecuting = false)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}, excludeExecuting));
}

// Count registered time events for this entity and callback, optionally excluding executing callbacks
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Server_Entity_CountTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, bool excludeExecuting = false)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}, excludeExecuting));
}

// Count registered time events for this entity and callback, optionally excluding executing callbacks
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Server_Entity_CountTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, bool excludeExecuting = false)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}, excludeExecuting));
}

// Count registered time events for this entity and callback, optionally excluding executing callbacks
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Server_Entity_CountTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, bool excludeExecuting = false)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}, excludeExecuting));
}

// Count registered time events with the given event id, optionally excluding executing callbacks
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Server_Entity_CountTimeEvent(ptr<ServerEntity> self, uint32_t id, bool excludeExecuting = false)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, {}, id, excludeExecuting));
}

// Stop time events registered for this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stop time events registered for this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stop time events registered for this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stop time events registered for this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stop the time event with the given id for this entity
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ptr<ServerEntity> self, uint32_t id)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, {}, id);
}

// Update the repeat interval for time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Update the repeat interval for time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Update the repeat interval for time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Update the repeat interval for time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Update the repeat interval for the time event with the given id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ptr<ServerEntity> self, uint32_t id, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, repeat, std::nullopt);
}

// Replace the data payload of time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

// Replace the data payload of time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, to_vector(data));
}

// Replace the data payload of time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

// Replace the data payload of time events bound to this entity and callback
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ptr<ServerEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, to_vector(data));
}

// Replace the data payload of the time event with the given id
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ptr<ServerEntity> self, uint32_t id, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, vector<any_t> {std::move(data)});
}

// Replace the data payload of the time event with the given id using a data list
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ptr<ServerEntity> self, uint32_t id, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, to_vector(data));
}

FO_END_NAMESPACE
