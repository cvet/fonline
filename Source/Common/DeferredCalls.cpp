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
    PROFILER_ENTRY();

    RUNTIME_ASSERT(_engine);
}

auto DeferredCallManager::AddDeferredCall(uint delay, ScriptFunc<void> func) -> uint
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.EmptyFunc = func;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, ScriptFunc<void, int> func, int value) -> uint
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.SignedIntFunc = func;
    call.FuncValue = value;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, ScriptFunc<void, uint> func, uint value) -> uint
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.UnsignedIntFunc = func;
    call.FuncValue = value;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, ScriptFunc<void, vector<int>> func, const vector<int>& values) -> uint
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.SignedIntArrayFunc = func;
    call.FuncValue = values;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, ScriptFunc<void, vector<uint>> func, const vector<uint>& values) -> uint
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.UnsignedIntArrayFunc = func;
    call.FuncValue = values;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, DeferredCall& call) -> uint
{
    PROFILER_ENTRY();

    call.Id = GetNextCallId();

    if (delay > 0) {
        const auto time_mul = _engine->GetTimeMultiplier();
        call.FireFullSecond = _engine->GameTime.GetFullSecond() + delay * time_mul / 1000;
    }

    _deferredCalls.emplace_back(std::move(call));

    return _deferredCalls.back().Id;
}

auto DeferredCallManager::IsDeferredCallPending(uint id) const -> bool
{
    PROFILER_ENTRY();

    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        if (it->Id == id) {
            return true;
        }
    }

    return false;
}

auto DeferredCallManager::CancelDeferredCall(uint id) -> bool
{
    PROFILER_ENTRY();

    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        if (it->Id == id) {
            OnDeferredCallRemoved(*it);
            _deferredCalls.erase(it);
            return true;
        }
    }

    return false;
}

void DeferredCallManager::Process()
{
    PROFILER_ENTRY();

    if (_deferredCalls.empty()) {
        return;
    }

    const auto full_second = _engine->GameTime.GetFullSecond();

    bool done = false;
    while (!done) {
        done = true;

        for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
            if (full_second >= it->FireFullSecond) {
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

auto DeferredCallManager::GetNextCallId() -> uint
{
    PROFILER_ENTRY();

    return ++_idCounter;
}

auto DeferredCallManager::RunDeferredCall(DeferredCall& call) const -> bool
{
    PROFILER_ENTRY();

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
