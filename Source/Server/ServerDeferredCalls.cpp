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

#include "ServerDeferredCalls.h"
#include "DataBase.h"
#include "Server.h"

ServerDeferredCallManager::ServerDeferredCallManager(FOServer* engine) : DeferredCallManager(engine), _serverEngine {engine}
{
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, string_view func_name, const int* value, const vector<int>* values, const uint* value2, const vector<uint>* values2) -> uint
{
    const auto id = AddDeferredCall(delay, func_name, value, values, value2, values2);

    if (id != 0u) {
        _savedCalls.emplace(id);

        /*
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
        */
    }

    return id;
}

auto ServerDeferredCallManager::GetNextId() -> uint
{
    const auto next_id = _serverEngine->GetLastDeferredCallId() + 1;
    _serverEngine->SetLastDeferredCallId(next_id);
    return next_id;
}

void ServerDeferredCallManager::OnDeferredCallRemoved(const DeferredCall& call)
{
    if (const auto it = _savedCalls.find(call.Id); it != _savedCalls.end()) {
        _serverEngine->DbStorage.Delete("DeferredCalls", call.Id);
        _savedCalls.erase(it);
    }
}

void ServerDeferredCallManager::LoadDeferredCalls()
{
    // WriteLog("Load deferred calls...");

    const auto ids = _serverEngine->DbStorage.GetAllIds("DeferredCalls");
    for (const auto id : ids) {
        throw NotImplementedException(LINE_STR);

        _savedCalls.emplace(id);
        _deferredCalls.push_back({});
    }

    /*Uvector<int> call_ids = DbStorage->GetAllIds("DeferredCalls");
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
}
