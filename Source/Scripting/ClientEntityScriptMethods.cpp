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

#include "Client.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Client_Entity_StartTimeEvent(ClientEntity* self, timespan delay, ScriptFuncName<void, ScriptSelfEntity*> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, func, delay, {}, {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Client_Entity_StartTimeEvent(ClientEntity* self, timespan delay, ScriptFuncName<void, ScriptSelfEntity*, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, func, delay, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Client_Entity_StartTimeEvent(ClientEntity* self, timespan delay, ScriptFuncName<void, ScriptSelfEntity*, vector<any_t>> func, const vector<any_t>& data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, func, delay, {}, data);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Client_Entity_StartTimeEvent(ClientEntity* self, timespan delay, timespan repeat, ScriptFuncName<void, ScriptSelfEntity*> func)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, func, delay, repeat, {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Client_Entity_StartTimeEvent(ClientEntity* self, timespan delay, timespan repeat, ScriptFuncName<void, ScriptSelfEntity*, any_t> func, any_t data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, func, delay, repeat, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API uint32 Client_Entity_StartTimeEvent(ClientEntity* self, timespan delay, timespan repeat, ScriptFuncName<void, ScriptSelfEntity*, vector<any_t>> func, const vector<any_t>& data)
{
    return self->GetEngine()->TimeEventMngr.StartTimeEvent(self, func, delay, repeat, data);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Client_Entity_CountTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*> func)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func, {}));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Client_Entity_CountTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, any_t> func)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func, {}));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Client_Entity_CountTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, vector<any_t>> func)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, func, {}));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API int32 Client_Entity_CountTimeEvent(ClientEntity* self, uint32 id)
{
    return numeric_cast<int32>(self->GetEngine()->TimeEventMngr.CountTimeEvent(self, {}, id));
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func, {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, any_t> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func, {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, vector<any_t>> func)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, func, {});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_StopTimeEvent(ClientEntity* self, uint32 id)
{
    self->GetEngine()->TimeEventMngr.StopTimeEvent(self, {}, id);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func, {}, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, any_t> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func, {}, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, vector<any_t>> func, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func, {}, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_RepeatTimeEvent(ClientEntity* self, uint32 id, timespan repeat)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, repeat, std::nullopt);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, any_t> func, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func, {}, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ClientEntity* self, ScriptFuncName<void, ScriptSelfEntity*, vector<any_t>> func, const vector<any_t>& data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, func, {}, {}, data);
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ClientEntity* self, uint32 id, any_t data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod TimeEventRelated
FO_SCRIPT_API void Client_Entity_SetTimeEventData(ClientEntity* self, uint32 id, const vector<any_t>& data)
{
    self->GetEngine()->TimeEventMngr.ModifyTimeEvent(self, {}, id, {}, data);
}

FO_END_NAMESPACE();
