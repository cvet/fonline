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

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, ScriptFunc<void, int> func, int value) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(!func.IsDelegate());

    auto call = DeferredCall();
    call.SignedIntFunc = func;
    call.FuncValue = value;
    return AddSavedDeferredCall(delay, call);
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, ScriptFunc<void, uint> func, uint value) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(!func.IsDelegate());

    auto call = DeferredCall();
    call.UnsignedIntFunc = func;
    call.FuncValue = value;
    return AddSavedDeferredCall(delay, call);
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, ScriptFunc<void, vector<int>> func, const vector<int>& values) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(!func.IsDelegate());

    auto call = DeferredCall();
    call.SignedIntArrayFunc = func;
    call.FuncValue = values;
    return AddSavedDeferredCall(delay, call);
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, ScriptFunc<void, vector<uint>> func, const vector<uint>& values) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(!func.IsDelegate());

    auto call = DeferredCall();
    call.UnsignedIntArrayFunc = func;
    call.FuncValue = values;
    return AddSavedDeferredCall(delay, call);
}

auto ServerDeferredCallManager::AddSavedDeferredCall(uint delay, DeferredCall& call) -> ident_t
{
    STACK_TRACE_ENTRY();

    call.Id = GetNextCallId();

    if (delay > 0) {
        const auto time_mul = _engine->GetTimeMultiplier();
        call.FireFullSecond = tick_t {_engine->GameTime.GetFullSecond().underlying_value() + delay * time_mul / 1000};
    }

    _savedCalls.emplace(call.Id);

    AnyData::Document call_doc;

    if (call.EmptyFunc) {
        call_doc["Script"] = string(call.EmptyFunc.GetName());
    }
    else if (call.SignedIntFunc) {
        call_doc["Script"] = string(call.SignedIntFunc.GetName());
        call_doc["Value"] = static_cast<int64>(std::get<int>(call.FuncValue));
    }
    else if (call.UnsignedIntFunc) {
        call_doc["Script"] = string(call.UnsignedIntFunc.GetName());
        call_doc["Value"] = static_cast<int64>(std::get<uint>(call.FuncValue));
    }
    else if (call.SignedIntArrayFunc) {
        call_doc["Script"] = string(call.SignedIntArrayFunc.GetName());
        AnyData::Array values;
        for (const auto i : std::get<vector<int>>(call.FuncValue)) {
            values.push_back(static_cast<int64>(i));
        }
        call_doc["Values"] = values;
    }
    else if (call.UnsignedIntArrayFunc) {
        call_doc["Script"] = string(call.UnsignedIntArrayFunc.GetName());
        AnyData::Array values;
        for (const auto i : std::get<vector<uint>>(call.FuncValue)) {
            values.push_back(static_cast<int64>(i));
        }
        call_doc["Values"] = values;
    }

    RUNTIME_ASSERT(call_doc.count("Script") && !std::get<string>(call_doc["Script"]).empty());

    call_doc["FireFullSecond"] = static_cast<int64>(call.FireFullSecond.underlying_value());

    _serverEngine->DbStorage.Insert("DeferredCalls", call.Id, call_doc);

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
        _serverEngine->DbStorage.Delete("DeferredCalls", call.Id);
        _savedCalls.erase(it);
    }
}

void ServerDeferredCallManager::LoadDeferredCalls()
{
    STACK_TRACE_ENTRY();

    WriteLog("Load deferred calls");

    int errors = 0;

    const auto call_ids = _serverEngine->DbStorage.GetAllIds("DeferredCalls");
    for (const auto call_id : call_ids) {
        auto call_doc = _serverEngine->DbStorage.Get("DeferredCalls", call_id);

        DeferredCall call;

        call.Id = call_id;
        call.FireFullSecond = tick_t {static_cast<tick_t::underlying_type>(std::get<int64>(call_doc["FireFullSecond"]))};

        const auto func_name = _serverEngine->ToHashedString(std::get<string>(call_doc["Script"]));
        call.EmptyFunc = _serverEngine->ScriptSys->FindFunc<void>(func_name);
        call.SignedIntFunc = _serverEngine->ScriptSys->FindFunc<void, int>(func_name);
        call.UnsignedIntFunc = _serverEngine->ScriptSys->FindFunc<void, uint>(func_name);
        call.SignedIntArrayFunc = _serverEngine->ScriptSys->FindFunc<void, vector<int>>(func_name);
        call.UnsignedIntArrayFunc = _serverEngine->ScriptSys->FindFunc<void, vector<uint>>(func_name);

        const auto func_count = static_cast<int>(!!call.EmptyFunc) + //
            static_cast<int>(!!call.SignedIntFunc) + static_cast<int>(!!call.UnsignedIntFunc) + //
            static_cast<int>(!!call.SignedIntArrayFunc) + static_cast<int>(!!call.UnsignedIntArrayFunc);

        if (func_count != 1) {
            WriteLog("Unable to find function {} for deferred call {}", func_name, call.Id);
            errors++;
            continue;
        }

        if (call_doc.count("Value") != 0) {
            if (call.SignedIntFunc) {
                call.FuncValue = static_cast<int>(std::get<int64>(call_doc["Value"]));
            }
            else if (call.UnsignedIntFunc) {
                call.FuncValue = static_cast<uint>(std::get<int64>(call_doc["Value"]));
            }
            else if (call.SignedIntArrayFunc) {
                call.FuncValue = vector {static_cast<int>(std::get<int64>(call_doc["Value"]))};
            }
            else if (call.UnsignedIntArrayFunc) {
                call.FuncValue = vector {static_cast<uint>(std::get<int64>(call_doc["Value"]))};
            }
            else {
                WriteLog("Value present for empty func {} in deferred call {}", func_name, call.Id);
                errors++;
                continue;
            }
        }
        else if (call_doc.count("Values") != 0) {
            if (call.SignedIntArrayFunc) {
                vector<int> values;
                for (auto&& arr_value : std::get<AnyData::Array>(call_doc["Values"])) {
                    values.push_back(static_cast<int>(std::get<int64>(arr_value)));
                }
                call.FuncValue = values;
            }
            else if (call.UnsignedIntArrayFunc) {
                vector<uint> values;
                for (auto&& arr_value : std::get<AnyData::Array>(call_doc["Values"])) {
                    values.push_back(static_cast<uint>(std::get<int64>(arr_value)));
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
            if (call.SignedIntFunc) {
                call.FuncValue = static_cast<int>(0);
            }
            else if (call.UnsignedIntFunc) {
                call.FuncValue = static_cast<uint>(0);
            }
            else if (call.SignedIntArrayFunc) {
                call.FuncValue = vector<int>();
            }
            else if (call.UnsignedIntArrayFunc) {
                call.FuncValue = vector<uint>();
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
