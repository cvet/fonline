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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "AnyData.h"
#include "DataBase.h"
#include "Server.h"

ServerDeferredCallManager::ServerDeferredCallManager(FOServer* engine) :
    DeferredCallManager(engine),
    _serverEngine {engine}
{
    STACK_TRACE_ENTRY();
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, ScriptFunc<void> func) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(!func.IsDelegate());

    auto call = DeferredCall();
    call.EmptyFunc = func;
    return AddSavedDeferredCall(delay, call);
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, ScriptFunc<void, any_t> func, any_t value) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(!func.IsDelegate());

    auto call = DeferredCall();
    call.AnyFunc = func;
    call.FuncValue = {std::move(value)};
    return AddSavedDeferredCall(delay, call);
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(!func.IsDelegate());

    auto call = DeferredCall();
    call.AnyArrayFunc = func;
    call.FuncValue = values;
    return AddSavedDeferredCall(delay, call);
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, DeferredCall& call) -> ident_t
{
    STACK_TRACE_ENTRY();

    call.Id = GetNextCallId();

    if (delay > 0) {
        const auto time_mul = _engine->GetTimeMultiplier();

        if (time_mul != 0) {
            call.FireTime = tick_t {_engine->GameTime.GetServerTime().underlying_value() + delay * time_mul / 1000};
        }
    }

    _savedCalls.emplace(call.Id);

    AnyData::Document call_doc;

    if (call.EmptyFunc) {
        call_doc.Emplace("Script", string(call.EmptyFunc.GetName()));
    }
    else if (call.AnyFunc) {
        call_doc.Emplace("Script", string(call.AnyFunc.GetName()));
        call_doc.Emplace("Value", string(call.FuncValue.front()));
    }
    else if (call.AnyArrayFunc) {
        call_doc.Emplace("Script", string(call.AnyArrayFunc.GetName()));

        AnyData::Array values;

        for (const auto& str : call.FuncValue) {
            values.EmplaceBack(string(str));
        }

        call_doc.Emplace("Values", std::move(values));
    }

    RUNTIME_ASSERT(call_doc.Contains("Script") && !call_doc["Script"].AsString().empty());

    call_doc.Emplace("FireTime", static_cast<int64>(call.FireTime.underlying_value()));

    _serverEngine->DbStorage.Insert(_serverEngine->DeferredCallsCollectionName, call.Id, call_doc);

    _deferredCalls.emplace_back(std::move(call));

    return _deferredCalls.back().Id;
}

auto ServerDeferredCallManager::GetNextCallId() -> ident_t
{
    STACK_TRACE_ENTRY();

    const auto next_id_num = _serverEngine->GetLastDeferredCallId().underlying_value() + 1;
    const auto next_id = ident_t {next_id_num};

    _serverEngine->SetLastDeferredCallId(next_id);

    return next_id;
}

void ServerDeferredCallManager::OnDeferredCallRemoved(const DeferredCall& call)
{
    STACK_TRACE_ENTRY();

    if (const auto it = _savedCalls.find(call.Id); it != _savedCalls.end()) {
        _serverEngine->DbStorage.Delete(_serverEngine->DeferredCallsCollectionName, call.Id);
        _savedCalls.erase(it);
    }
}

void ServerDeferredCallManager::LoadDeferredCalls()
{
    STACK_TRACE_ENTRY();

    WriteLog("Load deferred calls");

    int errors = 0;

    const auto call_ids = _serverEngine->DbStorage.GetAllIds(_serverEngine->DeferredCallsCollectionName);

    for (const auto call_id : call_ids) {
        auto call_doc = _serverEngine->DbStorage.Get(_serverEngine->DeferredCallsCollectionName, call_id);

        DeferredCall call;

        call.Id = call_id;
        call.FireTime = tick_t {static_cast<tick_t::underlying_type>(call_doc["FireTime"].AsInt64())};

        const auto func_name = _serverEngine->ToHashedString(call_doc["Script"].AsString());
        call.EmptyFunc = _serverEngine->ScriptSys->FindFunc<void>(func_name);
        call.AnyFunc = _serverEngine->ScriptSys->FindFunc<void, any_t>(func_name);
        call.AnyArrayFunc = _serverEngine->ScriptSys->FindFunc<void, vector<any_t>>(func_name);

        const auto func_count = static_cast<int>(!!call.EmptyFunc) + static_cast<int>(!!call.AnyFunc) + static_cast<int>(!!call.AnyArrayFunc);

        if (func_count == 0) {
            WriteLog("Unable to find function {} for deferred call {}", func_name, call.Id);
            errors++;
            continue;
        }

        if (func_count > 1) {
            WriteLog("Found multiple functions {} for deferred call {}", func_name, call.Id);
            errors++;
            continue;
        }

        if (call_doc.Contains("Value")) {
            if (call.AnyFunc || call.AnyArrayFunc) {
                call.FuncValue = {any_t {call_doc["Value"].AsString()}};
            }
            else {
                WriteLog("Value present for empty func {} in deferred call {}", func_name, call.Id);
                errors++;
                continue;
            }
        }
        else if (call_doc.Contains("Values")) {
            if (call.AnyArrayFunc) {
                vector<any_t> values;

                for (auto&& arr_entry : call_doc["Values"].AsArray()) {
                    values.push_back(any_t {arr_entry.AsString()});
                }

                call.FuncValue = values;
            }
            else {
                WriteLog("Array of values present for func {} in deferred call {}", func_name, call.Id);
                errors++;
                continue;
            }
        }
        else {
            if (call.AnyFunc) {
                call.FuncValue.resize(1);
            }
        }

        _savedCalls.emplace(call_id);
        _deferredCalls.emplace_back(std::move(call));
    }

    if (errors != 0) {
        throw DeferredCallsLoadException("Not all deffered calls can be loaded");
    }

    WriteLog("Loaded {} deferred calls", _savedCalls.size());
}
