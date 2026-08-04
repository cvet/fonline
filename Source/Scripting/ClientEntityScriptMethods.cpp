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

#include "Client.h"
#include "ScriptSystem.h"
#include "TimeEvents.h"

FO_BEGIN_NAMESPACE

// Schedules a one-shot client time event bound to this entity after the requested delay and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, {});
}

// Schedules a one-shot client time event bound to this entity, passes one payload value to the callback, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

// Schedules a one-shot client time event bound to this entity, passes the payload array to the callback, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, to_vector(data));
}

// Schedules a one-shot client time event whose callback receives a context for inspecting or modifying that event, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, {});
}

// Schedules a one-shot client time event whose callback context exposes one payload value, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

// Schedules a one-shot client time event whose callback context exposes the payload array, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, to_vector(data));
}

// Schedules a repeating client time event bound to this entity, with its first firing after delay and later firings at repeat, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, {});
}

// Schedules a repeating client time event with one callback payload value and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

// Schedules a repeating client time event with a callback payload array and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, to_vector(data));
}

// Schedules a repeating client time event whose callback receives a context for inspecting or modifying that event, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, {});
}

// Schedules a repeating client time event whose callback context exposes one payload value, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

// Schedules a repeating client time event whose callback context exposes the payload array, and returns its event id.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32_t Client_Entity_StartTimeEvent(ptr<ClientEntity> self, timespan delay, timespan repeat, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, to_vector(data));
}

// Counts all client time events bound to this entity that use the selected callback.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Client_Entity_CountTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>> func)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}));
}

// Counts all client time events bound to this entity that use the selected callback.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Client_Entity_CountTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}));
}

// Counts all client time events bound to this entity that use the selected callback.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Client_Entity_CountTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}));
}

// Counts all client time events bound to this entity that use the selected callback.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Client_Entity_CountTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}));
}

// Returns one when a client time event with this id is bound to the entity, or zero when it is absent.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32_t Client_Entity_CountTimeEvent(ptr<ClientEntity> self, uint32_t id)
{
    return numeric_cast<int32_t>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, {}, id));
}

// Stops all client time events bound to this entity that use the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stops all client time events bound to this entity that use the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stops all client time events bound to this entity that use the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stops all client time events bound to this entity that use the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

// Stops the client time event with this id when it is bound to the entity; does nothing when it is absent.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ptr<ClientEntity> self, uint32_t id)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, {}, id);
}

// Sets the repeat interval for every matching callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for every matching callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for every matching callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for every matching callback event and reschedules each next firing from now; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

// Sets the repeat interval for the event with this id and reschedules its next firing from now; does nothing when it is absent.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ptr<ClientEntity> self, uint32_t id, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, repeat, std::nullopt);
}

// Replaces the single payload value for every time event using the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> func, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

// Replaces the payload array for every time event using the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> func, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, to_vector(data));
}

// Replaces the single context payload value for every time event using the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

// Replaces the context payload array for every time event using the selected callback; does nothing when none match.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ptr<ClientEntity> self, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> func, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, to_vector(data));
}

// Replaces the single payload value for the time event with this id; does nothing when it is absent.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ptr<ClientEntity> self, uint32_t id, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, vector<any_t> {std::move(data)});
}

// Replaces the payload array for the time event with this id; does nothing when it is absent.
///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ptr<ClientEntity> self, uint32_t id, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, to_vector(data));
}

FO_END_NAMESPACE
