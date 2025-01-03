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

#include "DeferredCalls.h"

DeferredCallManager::DeferredCallManager(FOEngineBase* engine) :
    _engine {engine}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_engine);
}

auto DeferredCallManager::AddDeferredCall(uint delay, bool repeating, ScriptFunc<void> func) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.EmptyFunc = func;
    call.Repeating = repeating;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, bool repeating, ScriptFunc<void, any_t> func, any_t value) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.AnyFunc = func;
    call.FuncValue = {std::move(value)};
    call.Repeating = repeating;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, bool repeating, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values) -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);

    auto call = DeferredCall();
    call.AnyArrayFunc = func;
    call.FuncValue = values;
    call.Repeating = repeating;
    return AddDeferredCall(delay, call);
}

auto DeferredCallManager::AddDeferredCall(uint delay, DeferredCall& call) -> ident_t
{
    STACK_TRACE_ENTRY();

    call.Delay = tick_t {delay};
    call.Id = GetNextCallId();

    if (delay != 0) {
        const auto time_mul = _engine->GetTimeMultiplier();
        call.FireTime = tick_t {_engine->GameTime.GetServerTime().underlying_value() + delay * time_mul / 1000};
    }

    _deferredCalls.emplace_back(std::move(call));

    return _deferredCalls.back().Id;
}

auto DeferredCallManager::IsDeferredCallPending(ident_t id) const noexcept -> bool
{
    STACK_TRACE_ENTRY();

    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        if (it->Id == id) {
            return true;
        }
    }

    return false;
}

auto DeferredCallManager::CancelDeferredCall(ident_t id) -> bool
{
    STACK_TRACE_ENTRY();

    for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
        if (it->Id == id) {
            OnDeferredCallRemoved(*it);
            _deferredCalls.erase(it);
            return true;
        }
    }

    return false;
}

void DeferredCallManager::ProcessDeferredCalls()
{
    STACK_TRACE_ENTRY();

    if (_deferredCalls.empty()) {
        return;
    }

    const auto server_time = _engine->GameTime.GetServerTime();
    const auto time_mul = _engine->GetTimeMultiplier();

    unordered_set<ident_t> skip_ids;
    bool done = false;

    while (!done) {
        done = true;

        for (auto it = _deferredCalls.begin(); it != _deferredCalls.end(); ++it) {
            if (server_time.underlying_value() >= it->FireTime.underlying_value() && skip_ids.count(it->Id) == 0) {
                DeferredCall call = copy(*it);

                if (call.Repeating) {
                    if (call.Delay.underlying_value() != 0) {
                        it->FireTime = tick_t {server_time.underlying_value() + call.Delay.underlying_value() * time_mul / 1000};
                    }

                    skip_ids.emplace(call.Id);
                }
                else {
                    it = _deferredCalls.erase(it);

                    OnDeferredCallRemoved(call);
                }

                RunDeferredCall(call);

                done = false;
                break;
            }
        }
    }
}

auto DeferredCallManager::GetNextCallId() -> ident_t
{
    STACK_TRACE_ENTRY();

    return ident_t {static_cast<ident_t::underlying_type>(++_idCounter)};
}

void DeferredCallManager::OnDeferredCallRemoved(const DeferredCall& call)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(call);
}

auto DeferredCallManager::RunDeferredCall(DeferredCall& call) const -> bool
{
    STACK_TRACE_ENTRY();

    if (call.AnyFunc) {
        return call.AnyFunc(call.FuncValue.front());
    }
    else if (call.AnyArrayFunc) {
        return call.AnyArrayFunc(call.FuncValue);
    }
    else {
        return call.EmptyFunc();
    }
}
