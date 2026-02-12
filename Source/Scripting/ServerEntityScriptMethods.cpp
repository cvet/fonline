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

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Server_Entity_StartTimeEvent(ServerEntity* self, timespan delay, ScriptFunc<void, ScriptSelfEntity*> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Server_Entity_StartTimeEvent(ServerEntity* self, timespan delay, ScriptFunc<void, ScriptSelfEntity*, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Server_Entity_StartTimeEvent(ServerEntity* self, timespan delay, ScriptFunc<void, ScriptSelfEntity*, vector<any_t>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, {}, to_vector(data));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Server_Entity_StartTimeEvent(ServerEntity* self, timespan delay, timespan repeat, ScriptFunc<void, ScriptSelfEntity*> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Server_Entity_StartTimeEvent(ServerEntity* self, timespan delay, timespan repeat, ScriptFunc<void, ScriptSelfEntity*, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Server_Entity_StartTimeEvent(ServerEntity* self, timespan delay, timespan repeat, ScriptFunc<void, ScriptSelfEntity*, vector<any_t>> func, readonly_vector<any_t> data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, std::move(func), delay, repeat, to_vector(data));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Server_Entity_CountTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*> func)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Server_Entity_CountTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, any_t> func)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Server_Entity_CountTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, vector<any_t>> func)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func.GetName(), {}));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Server_Entity_CountTimeEvent(ServerEntity* self, uint32 id)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, {}, id));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, any_t> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, vector<any_t>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func.GetName(), {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_StopTimeEvent(ServerEntity* self, uint32 id)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, {}, id);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, any_t> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, vector<any_t>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_RepeatTimeEvent(ServerEntity* self, uint32 id, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, any_t> func, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ServerEntity* self, ScriptFunc<void, ScriptSelfEntity*, vector<any_t>> func, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func.GetName(), {}, {}, to_vector(data));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ServerEntity* self, uint32 id, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Server_Entity_SetTimeEventData(ServerEntity* self, uint32 id, readonly_vector<any_t> data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, to_vector(data));
}

FO_END_NAMESPACE
