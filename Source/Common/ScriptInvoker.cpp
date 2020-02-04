//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: move script invoker to ScriptSystem

#include "ScriptInvoker.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
// Todo: rework FONLINE_
/*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
#include "DataBase.h"
#endif*/

ScriptInvoker::ScriptInvoker(TimerSettings& sett) : settings {sett}
{
}

uint ScriptInvoker::AddDeferredCall(uint delay, bool saved, asIScriptFunction* func, int* value, CScriptArray* values)
{
    // Todo: fix script system
    /*if (!func)
        SCRIPT_ERROR_R0("Function arg is null.");

    RUNTIME_ASSERT(func->GetReturnTypeId() == asTYPEID_VOID);
    uint func_num = Script::GetFuncNum(func);
    uint bind_id = Script::BindByFunc(func, false);
    RUNTIME_ASSERT(bind_id);

    uint time_mul = Globals->GetTimeMultiplier();
    if (time_mul == 0)
        time_mul = 1;

    DeferredCall call;
    call.Id = 0;
    call.FireFullSecond = (delay ? settings.FullSecond + delay * time_mul / 1000 : 0);
    call.FuncNum = func_num;
    call.BindId = bind_id;
    call.Saved = saved;

    if (value)
    {
        call.IsValue = true;
        call.Value = *value;
        int value_type_id = 0;
        func->GetParam(0, &value_type_id);
        RUNTIME_ASSERT((value_type_id == asTYPEID_INT32 || value_type_id == asTYPEID_UINT32));
        call.ValueSigned = (value_type_id == asTYPEID_INT32);
    }
    else
    {
        call.IsValue = false;
        call.Value = 0;
        call.ValueSigned = false;
    }

    if (values)
    {
        call.IsValues = true;
        Script::AssignScriptArrayInVector(call.Values, values);
        RUNTIME_ASSERT((values->GetElementTypeId() == asTYPEID_INT32 || values->GetElementTypeId() == asTYPEID_UINT32));
        call.ValuesSigned = (values->GetElementTypeId() == asTYPEID_INT32);
    }
    else
    {
        call.IsValues = false;
        call.ValuesSigned = false;
    }

    if (delay == 0)
    {
        RunDeferredCall(call);
    }
    else
    {
        RUNTIME_ASSERT(call.FireFullSecond != 0);
        call.Id = Globals->GetLastDeferredCallId() + 1;
        Globals->SetLastDeferredCallId(call.Id);
        deferredCalls.push_back(call);

        / * #if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
                if (call.Saved)
                {
                    DataBase::Document call_doc;
                    call_doc["Script"] = _str().parseHash(call.FuncNum);
                    call_doc["FireFullSecond"] = (int64)call.FireFullSecond;

                    if (call.IsValue)
                    {
                        call_doc["ValueSigned"] = call.ValueSigned;
                        call_doc["Value"] = call.Value;
                    }

                    if (call.IsValues)
                    {
                        call_doc["ValuesSigned"] = call.ValuesSigned;
                        DataBase::Array values;
                        for (int v : call.Values)
                            values.push_back(v);
                        call_doc["Values"] = values;
                    }

                    DbStorage->Insert("DeferredCalls", call.Id, call_doc);
                }
        #endif* /
    }
    return call.Id;
    */
    return 0;
}

bool ScriptInvoker::IsDeferredCallPending(uint id)
{
    for (auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it)
        if (it->Id == id)
            return true;
    return false;
}

bool ScriptInvoker::CancelDeferredCall(uint id)
{
    for (auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it)
    {
        if (it->Id == id)
        {
            /*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
                        if (it->Saved)
                            DbStorage->Delete("DeferredCalls", id);
            #endif*/

            deferredCalls.erase(it);
            return true;
        }
    }
    return false;
}

bool ScriptInvoker::GetDeferredCallData(uint id, DeferredCall& data)
{
    for (auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it)
    {
        DeferredCall& call = *it;
        if (call.Id == id)
        {
            data = call;
            return true;
        }
    }
    return false;
}

void ScriptInvoker::GetDeferredCallsList(IntVec& ids)
{
    ids.reserve(deferredCalls.size());
    for (auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it)
        ids.push_back(it->Id);
}

void ScriptInvoker::Process()
{
    bool done = false;
    while (!done)
    {
        done = true;

        for (auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it)
        {
            RUNTIME_ASSERT(it->FireFullSecond != 0);
            if (settings.FullSecond >= it->FireFullSecond)
            {
                DeferredCall call = *it;
                it = deferredCalls.erase(it);

                /*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
                                if (call.Saved)
                                    DbStorage->Delete("DeferredCalls", call.Id);
                #endif*/

                RunDeferredCall(call);
                done = false;
                break;
            }
        }
    }
}

void ScriptInvoker::RunDeferredCall(DeferredCall& call)
{
    // Todo: fix script system
    /*Script::PrepareContext(call.BindId, "Invoker");

    CScriptArray* arr = nullptr;

    if (call.IsValue)
    {
        Script::SetArgUInt(call.Value);
    }
    else if (call.IsValues)
    {
        arr = Script::CreateArray(call.ValuesSigned ? "int[]" : "uint[]");
        Script::AppendVectorToArray(call.Values, arr);
        Script::SetArgObject(arr);
    }

    if (call.FireFullSecond == 0)
        Script::RunPreparedSuspend();
    else
        Script::RunPrepared();

    if (arr)
        arr->Release();*/
}

string ScriptInvoker::GetStatistics()
{
    /*uint time_mul = Globals->GetTimeMultiplier();
    string result = _str("Deferred calls count: {}\n", (uint)deferredCalls.size());
    result +=
        "Id         Delay      Saved    Function                                                              Values\n";
    for (auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it)
    {
        DeferredCall& call = *it;
        string func_name = Script::GetBindFuncName(call.BindId);
        uint delay = call.FireFullSecond > settings.FullSecond ?
            (call.FireFullSecond - settings.FullSecond) * time_mul / 1000 :
            0;

        result += _str("{:<10} {:<10} {:<8} {:<70}", call.Id, delay, call.Saved ? "true" : "false", func_name.c_str());

        if (call.IsValue)
        {
            result += "Single:";
            result += _str(" {}", call.Value);
        }
        else if (call.IsValues)
        {
            result += "Multiple:";
            for (size_t i = 0; i < call.Values.size(); i++)
                result += _str(" {}", call.Values[i]);
        }
        else
        {
            result += "None";
        }

        result += "\n";
    }
    return result;*/
    return "";
}

/*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
bool ScriptInvoker::LoadDeferredCalls()
{
    WriteLog("Load deferred calls...\n");

    UIntVec call_ids = DbStorage->GetAllIds("DeferredCalls");
    int errors = 0;
    for (uint call_id : call_ids)
    {
        DataBase::Document call_doc = DbStorage->Get("DeferredCalls", call_id);

        DeferredCall call;
        call.Id = std::get<int>(call_doc["Id"]);
        call.FireFullSecond = (uint)std::get<int64>(call_doc["FireFullSecond"]);
        RUNTIME_ASSERT(call.FireFullSecond != 0);

        call.IsValue = (call_doc.count("Value") > 0);
        if (call.IsValue)
        {
            call.ValueSigned = std::get<bool>(call_doc["ValueSigned"]);
            call.Value = std::get<int>(call_doc["Value"]);
        }
        else
        {
            call.ValueSigned = false;
            call.Value = 0;
        }

        call.IsValues = (call_doc.count("Values") > 0);
        if (call.IsValues)
        {
            call.ValuesSigned = std::get<bool>(call_doc["ValuesSigned"]);
            const DataBase::Array& arr = std::get<DataBase::Array>(call_doc["Values"]);
            for (auto& v : arr)
                call.Values.push_back(std::get<int>(v));
        }
        else
        {
            call.ValuesSigned = false;
            call.Values.clear();
        }

        if (call.IsValue && call.IsValues)
        {
            WriteLog("Deferred call {} have value and values.\n", call.Id);
            errors++;
            continue;
        }

        const char* decl;
        if (call.IsValue && call.ValueSigned)
            decl = "void %s(int)";
        else if (call.IsValue && !call.ValueSigned)
            decl = "void %s(uint)";
        else if (call.IsValues && call.ValuesSigned)
            decl = "void %s(int[]&)";
        else if (call.IsValues && !call.ValuesSigned)
            decl = "void %s(uint[]&)";
        else
            decl = "void %s()";

        call.FuncNum = Script::BindScriptFuncNumByFuncName(std::get<string>(call_doc["Script"]), decl);
        if (!call.FuncNum)
        {
            WriteLog("Unable to find function '{}' with declaration '{}' for deferred call {}.\n",
                std::get<string>(call_doc["Script"]), decl, call.Id);
            errors++;
            continue;
        }

        call.BindId = Script::BindByFuncNum(call.FuncNum, false);
        if (!call.BindId)
        {
            WriteLog(
                "Unable to bind script function '{}' for deferred call {}.\n", _str().parseHash(call.FuncNum), call.Id);
            errors++;
            continue;
        }

        call.Saved = true;
        deferredCalls.push_back(call);
    }

    WriteLog("Load deferred calls complete, count {}.\n", (uint)deferredCalls.size());
    return errors == 0;
}
#endif*/

uint ScriptInvoker::Global_DeferredCall(uint delay, asIScriptFunction* func)
{
    // Todo: take Invoker from func
    // return Script::GetInvoker()->AddDeferredCall(delay, false, func, nullptr, nullptr);
    return 0;
}

uint ScriptInvoker::Global_DeferredCallWithValue(uint delay, asIScriptFunction* func, int value)
{
    // return Script::GetInvoker()->AddDeferredCall(delay, false, func, &value, nullptr);
    return 0;
}

uint ScriptInvoker::Global_DeferredCallWithValues(uint delay, asIScriptFunction* func, CScriptArray* values)
{
    // return Script::GetInvoker()->AddDeferredCall(delay, false, func, nullptr, values);
    return 0;
}

uint ScriptInvoker::Global_SavedDeferredCall(uint delay, asIScriptFunction* func)
{
    // return Script::GetInvoker()->AddDeferredCall(delay, true, func, nullptr, nullptr);
    return 0;
}

uint ScriptInvoker::Global_SavedDeferredCallWithValue(uint delay, asIScriptFunction* func, int value)
{
    // return Script::GetInvoker()->AddDeferredCall(delay, true, func, &value, nullptr);
    return 0;
}

uint ScriptInvoker::Global_SavedDeferredCallWithValues(uint delay, asIScriptFunction* func, CScriptArray* values)
{
    // return Script::GetInvoker()->AddDeferredCall(delay, true, func, nullptr, values);
    return 0;
}

bool ScriptInvoker::Global_IsDeferredCallPending(uint id)
{
    // return Script::GetInvoker()->IsDeferredCallPending(id);
    return 0;
}

bool ScriptInvoker::Global_CancelDeferredCall(uint id)
{
    // return Script::GetInvoker()->CancelDeferredCall(id);
    return 0;
}

bool ScriptInvoker::Global_GetDeferredCallData(uint id, uint& delay, CScriptArray* values)
{
    /*ScriptInvoker* self = Script::GetInvoker();
    DeferredCall call;
    if (self->GetDeferredCallData(id, call))
    {
        delay = (call.FireFullSecond > self->settings.FullSecond ?
                (call.FireFullSecond - self->settings.FullSecond) * Globals->GetTimeMultiplier() / 1000 :
                0);
        if (values)
        {
            if (call.IsValue)
            {
                values->Resize(1);
                *(int*)values->At(0) = call.Value;
            }
            else if (call.IsValues)
            {
                values->Resize(0);
                Script::AppendVectorToArray(call.Values, values);
            }
        }
        return true;
    }*/
    return false;
}

uint ScriptInvoker::Global_GetDeferredCallsList(CScriptArray* ids)
{
    /*ScriptInvoker* self = Script::GetInvoker();
    IntVec ids_;
    self->GetDeferredCallsList(ids_);
    if (ids)
        Script::AppendVectorToArray(ids_, ids);
    return (uint)ids_.size();*/
    return 0;
}
