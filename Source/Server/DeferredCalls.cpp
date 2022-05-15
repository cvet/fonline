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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "DeferredCalls.h"
#include "DataBase.h"
#include "Log.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"

DeferredCallManager::DeferredCallManager(FOServer* engine) : _engine {engine}
{
    UNUSED_VARIABLE(_engine);
}

auto DeferredCallManager::AddDeferredCall(uint delay, bool saved, string_view func_name, int* value, const vector<int>* values, uint* value2, const vector<uint>* values2) -> uint
{
    /*uint func_num = Script::GetFuncNum(func);
    uint bind_id = Script::BindByFunc(func, false);
    RUNTIME_ASSERT(bind_id);

    uint time_mul = Globals->GetTimeMultiplier();
    if (time_mul == 0)
        time_mul = 1;

    DeferredCall call;
    call.Id = 0;
    call.FireFullSecond = (delay ? GameOpt.FullSecond + delay * time_mul / 1000 : 0);
    call.FuncNum = func_num;
    call.BindId = bind_id;
    call.Saved = saved;

    if (value) {
        call.IsValue = true;
        call.Value = *value;
        int value_type_id = 0;
        func->GetParam(0, &value_type_id);
        RUNTIME_ASSERT((value_type_id == asTYPEID_INT32 || value_type_id == asTYPEID_UINT32));
        call.ValueSigned = (value_type_id == asTYPEID_INT32);
    }
    else {
        call.IsValue = false;
        call.Value = 0;
        call.ValueSigned = false;
    }

    if (values) {
        call.IsValues = true;
        Script::AssignScriptArrayInVector(call.Values, values);
        RUNTIME_ASSERT((values->GetElementTypeId() == asTYPEID_INT32 || values->GetElementTypeId() == asTYPEID_UINT32));
        call.ValuesSigned = (values->GetElementTypeId() == asTYPEID_INT32);
    }
    else {
        call.IsValues = false;
        call.ValuesSigned = false;
    }

    if (delay == 0) {
        RunDeferredCall(call);
    }
    else {
        RUNTIME_ASSERT(call.FireFullSecond != 0);
        call.Id = Globals->GetLastDeferredCallId() + 1;
        Globals->SetLastDeferredCallId(call.Id);
        _deferredCalls.push_back(call);

        if (call.Saved) {
            DataBase::Document call_doc;
            call_doc["Script"] = _str().parseHash(call.FuncNum);
            call_doc["FireFullSecond"] = (int64)call.FireFullSecond;

            if (call.IsValue) {
                call_doc["ValueSigned"] = call.ValueSigned;
                call_doc["Value"] = call.Value;
            }

            if (call.IsValues) {
                call_doc["ValuesSigned"] = call.ValuesSigned;
                DataBase::Array values;
                for (int v : call.Values)
                    values.push_back(v);
                call_doc["Values"] = values;
            }

            DbStorage->Insert("DeferredCalls", call.Id, call_doc);
        }
    }
    return call.Id;*/
    return 0;
}

auto DeferredCallManager::IsDeferredCallPending(uint id) -> bool
{
    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        if (it->Id == id) {
            return true;
        }
    }
    return false;
}

auto DeferredCallManager::CancelDeferredCall(uint id) -> bool
{
    /*for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        if (it->Id == id) {
            if (it->Saved) {
                DbStorage->Delete("DeferredCalls", id);
            }

            _deferredCalls.erase(it);
            return true;
        }
    }
    return false;*/
    return false;
}

auto DeferredCallManager::GetDeferredCallData(uint id, DeferredCall& data) -> bool
{
    /*for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        DeferredCall& call = *it;
        if (call.Id == id) {
            data = call;
            return true;
        }
    }*/
    return false;
}

auto DeferredCallManager::GetDeferredCallsList() -> vector<int>
{
    /*ids.reserve(_deferredCalls.size());
    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it)
        ids.push_back(it->Id);*/
    return {};
}

void DeferredCallManager::Process()
{
    /*bool done = false;
    while (!done) {
        done = true;

        for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
            RUNTIME_ASSERT(it->FireFullSecond != 0);
            if (GameOpt.FullSecond >= it->FireFullSecond) {
                DeferredCall call = *it;
                it = _deferredCalls.erase(it);

                if (call.Saved)
                    DbStorage->Delete("DeferredCalls", call.Id);

                RunDeferredCall(call);
                done = false;
                break;
            }
        }
    }*/
}

void DeferredCallManager::RunDeferredCall(DeferredCall& call)
{
    /*Script::PrepareContext(call.BindId, "Invoker");

    CScriptArray* arr = nullptr;

    if (call.IsValue) {
        Script::SetArgUInt(call.Value);
    }
    else if (call.IsValues) {
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

auto DeferredCallManager::GetStatistics() -> string
{
    /*uint time_mul = Globals->GetTimeMultiplier();
    string result = _str("Deferred calls count: {}\n", (uint)_deferredCalls.size());
    result += "Id         Delay      Saved    Function                                                              Values\n";
    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        DeferredCall& call = *it;
        string func_name = Script::GetBindFuncName(call.BindId);
        uint delay = call.FireFullSecond > GameOpt.FullSecond ? (call.FireFullSecond - GameOpt.FullSecond) * time_mul / 1000 : 0;

        result += _str("{:<10} {:<10} {:<8} {:<70}", call.Id, delay, call.Saved ? "true" : "false", func_name.c_str());

        if (call.IsValue) {
            result += "Single:";
            result += _str(" {}", call.Value);
        }
        else if (call.IsValues) {
            result += "Multiple:";
            for (size_t i = 0; i < call.Values.size(); i++)
                result += _str(" {}", call.Values[i]);
        }
        else {
            result += "None";
        }

        result += "\n";
    }
    return result;*/
    return "";
}

auto DeferredCallManager::LoadDeferredCalls() -> bool
{
    /*WriteLog("Load deferred calls...");

    Uvector<int> call_ids = DbStorage->GetAllIds("DeferredCalls");
    int errors = 0;
    for (uint call_id : call_ids) {
        DataBase::Document call_doc = DbStorage->Get("DeferredCalls", call_id);

        DeferredCall call;
        call.Id = call_doc["Id"].get<int>();
        call.FireFullSecond = (uint)call_doc["FireFullSecond"].get<int64>();
        RUNTIME_ASSERT(call.FireFullSecond != 0);

        call.IsValue = (call_doc.count("Value") > 0);
        if (call.IsValue) {
            call.ValueSigned = call_doc["ValueSigned"].get<bool>();
            call.Value = call_doc["Value"].get<int>();
        }
        else {
            call.ValueSigned = false;
            call.Value = 0;
        }

        call.IsValues = (call_doc.count("Values") > 0);
        if (call.IsValues) {
            call.ValuesSigned = call_doc["ValuesSigned"].get<bool>();
            const DataBase::Array& arr = call_doc["Values"].get<DataBase::Array>();
            for (auto& v : arr)
                call.Values.push_back(v.get<int>());
        }
        else {
            call.ValuesSigned = false;
            call.Values.clear();
        }

        if (call.IsValue && call.IsValues) {
            WriteLog("Deferred call {} have value and values", call.Id);
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

        call.FuncNum = Script::BindScriptFuncNumByFuncName(call_doc["Script"].get<string>(), decl);
        if (!call.FuncNum) {
            WriteLog("Unable to find function '{}' with declaration '{}' for deferred call {}", call_doc["Script"].get<string>(), decl, call.Id);
            errors++;
            continue;
        }

        call.BindId = Script::BindByFuncNum(call.FuncNum, false);
        if (!call.BindId) {
            WriteLog("Unable to bind script function '{}' for deferred call {}", _str().parseHash(call.FuncNum), call.Id);
            errors++;
            continue;
        }

        call.Saved = true;
        _deferredCalls.push_back(call);
    }

    WriteLog("Load deferred calls complete, count {}", (uint)_deferredCalls.size());
    return errors == 0;*/
    return false;
}
