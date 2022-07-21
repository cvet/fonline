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

DeferredCallManager::DeferredCallManager(FOEngineBase* engine) : _engine {engine}
{
    RUNTIME_ASSERT(_engine);
}

auto DeferredCallManager::AddDeferredCall(uint delay, hstring func_name, const int* value, const vector<int>* values, const uint* value2, const vector<uint>* values2) -> uint
{
    DeferredCall call = {};

    if (value != nullptr) {
        call.SignedIntFunc = _engine->ScriptSys->FindFunc<void, int>(func_name);
        call.FuncValue = *value;
    }
    else if (values != nullptr) {
        call.SignedIntArrayFunc = _engine->ScriptSys->FindFunc<void, vector<int>>(func_name);
        call.FuncValue = *values;
    }
    else if (value2 != nullptr) {
        call.UnsignedIntFunc = _engine->ScriptSys->FindFunc<void, uint>(func_name);
        call.FuncValue = *value2;
    }
    else if (values2 != nullptr) {
        call.UnsignedIntArrayFunc = _engine->ScriptSys->FindFunc<void, vector<uint>>(func_name);
        call.FuncValue = *values2;
    }
    else {
        call.EmptyFunc = _engine->ScriptSys->FindFunc<void>(func_name);
    }

    if (!call.SignedIntFunc && !call.SignedIntArrayFunc && !call.UnsignedIntFunc && !call.UnsignedIntArrayFunc && !call.EmptyFunc) {
        throw DeferredCallException("Function not found or invalid signature", func_name);
    }

    if (delay != 0u) {
        call.Id = GetNextId();

        const auto time_mul = _engine->GetTimeMultiplier();
        call.FireFullSecond = _engine->GameTime.GetFullSecond() + delay * time_mul / 1000;

        _deferredCalls.push_back(call);
    }
    else {
        RunDeferredCall(call);
    }

    return call.Id;
}

auto DeferredCallManager::IsDeferredCallPending(uint id) const -> bool
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
    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        if (it->Id == id) {
            OnDeferredCallRemoved(*it);
            _deferredCalls.erase(it);
            return true;
        }
    }

    return false;
}

auto DeferredCallManager::GetNextId() -> uint
{
    return ++_idCounter;
}

void DeferredCallManager::Process()
{
    bool done = false;
    while (!done) {
        done = true;

        for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
            if (_engine->GameTime.GetFullSecond() >= it->FireFullSecond) {
                DeferredCall call = copy(*it);
                it = _deferredCalls.erase(it);
                OnDeferredCallRemoved(call);

                RunDeferredCall(call);

                done = false;
                break;
            }
        }
    }
}

auto DeferredCallManager::RunDeferredCall(DeferredCall& call) const -> bool
{
    if (call.SignedIntFunc) {
        return call.SignedIntFunc(std::get<int>(call.FuncValue));
    }
    else if (call.UnsignedIntFunc) {
        return call.UnsignedIntFunc(std::get<uint>(call.FuncValue));
    }
    else if (call.SignedIntArrayFunc) {
        return call.SignedIntArrayFunc(std::get<vector<int>>(call.FuncValue));
    }
    else if (call.UnsignedIntArrayFunc) {
        return call.UnsignedIntArrayFunc(std::get<vector<uint>>(call.FuncValue));
    }
    else {
        return call.EmptyFunc();
    }
}
