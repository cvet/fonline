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

#include "TimeEventManager.h"
#include "EngineBase.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

const timespan TimeEventManager::MIN_REPEAT_TIME = timespan(std::chrono::milliseconds {1});

TimeEventManager::TimeEventManager(BaseEngine& engine) :
    _engine {&engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto TimeEventManager::StartTimeEvent(Entity* entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> uint32
{
    FO_STACK_TRACE_ENTRY();

    auto te = SafeAlloc::MakeShared<Entity::TimeEventData>();
    te->Id = ++_timeEventCounter;
    te->Func = std::move(func);
    te->FuncName = std::visit([](auto&& f) -> ScriptFuncName { return f.GetName(); }, func);
    te->FireTime = _engine->GetFrameTime() + std::max(delay, MIN_REPEAT_TIME);
    te->RepeatDuration = repeat;
    te->Data = std::move(data);

    auto& timeEvents = entity->GetRawTimeEvents();
    make_if_not_exists(timeEvents);
    timeEvents->emplace_back(std::move(te));

    AddEntityTimeEventPolling(entity);
    return _timeEventCounter;
}

auto TimeEventManager::CountTimeEvent(Entity* entity, ScriptFuncName func_name, uint32 id) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    const auto& timeEvents = entity->GetRawTimeEvents();

    if (!timeEvents || timeEvents->empty()) {
        return 0;
    }

    if (id != 0) {
        FO_RUNTIME_ASSERT(func_name == ScriptFuncName());

        for (const auto& te : *timeEvents) {
            if (te->Id == id) {
                return 1;
            }
        }
    }

    if (func_name != ScriptFuncName()) {
        size_t count = 0;

        for (const auto& te : *timeEvents) {
            if (te->FuncName == func_name) {
                count++;
            }
        }

        return count;
    }

    return 0;
}

void TimeEventManager::ModifyTimeEvent(Entity* entity, ScriptFuncName func_name, uint32 id, optional<timespan> repeat, optional<vector<any_t>> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto& timeEvents = entity->GetRawTimeEvents();

    if (!timeEvents || timeEvents->empty()) {
        return;
    }

    const auto fire_time = repeat.has_value() ? _engine->GetFrameTime() + std::max(repeat.value(), MIN_REPEAT_TIME) : nanotime::zero;

    for (size_t i = 0; i < timeEvents->size(); i++) {
        auto& te = (*timeEvents)[i];

        if (func_name != ScriptFuncName() && te->FuncName != func_name) {
            continue;
        }
        if (id != 0 && te->Id != id) {
            continue;
        }

        if (repeat.has_value()) {
            te->FireTime = fire_time;
            te->RepeatDuration = repeat.value();
        }
        if (data.has_value()) {
            te->Data = data.value();
        }

        // Identifier may be only one
        if (id != 0) {
            return;
        }
    }
}

void TimeEventManager::StopTimeEvent(Entity* entity, ScriptFuncName func_name, uint32 id)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto& timeEvents = entity->GetRawTimeEvents();

    if (!timeEvents || timeEvents->empty()) {
        return;
    }

    for (size_t i = 0; i < timeEvents->size();) {
        auto& te = (*timeEvents)[i];

        if (func_name != ScriptFuncName() && te->FuncName != func_name) {
            i++;
            continue;
        }
        if (id != 0 && te->Id != id) {
            i++;
            continue;
        }

        te->Id = 0;
        timeEvents->erase(timeEvents->begin() + numeric_cast<ptrdiff_t>(i)); // te is not valid anymore

        // Identifier may be only one
        if (id != 0) {
            return;
        }
    }
}

void TimeEventManager::AddEntityTimeEventPolling(Entity* entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    _timeEventEntities.emplace(entity);
}

void TimeEventManager::RemoveEntityTimeEventPolling(Entity* entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    _timeEventEntities.erase(entity);
}

void TimeEventManager::ProcessTimeEvents()
{
    FO_STACK_TRACE_ENTRY();

    for (auto* entity : copy_hold_ref(_timeEventEntities)) {
        if (entity->IsDestroyed()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        ProcessEntityTimeEvents(entity);

        if (entity->IsDestroyed() || !entity->HasTimeEvents()) {
            RemoveEntityTimeEventPolling(entity);
        }
    }
}

void TimeEventManager::ClearTimeEvents()
{
    FO_STACK_TRACE_ENTRY();

    _timeEventEntities.clear();
}

void TimeEventManager::ProcessEntityTimeEvents(Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    auto& timeEvents = entity->GetRawTimeEvents();

    if (!timeEvents || timeEvents->empty()) {
        return;
    }

    const auto time = _engine->GetFrameTime();

    for (size_t i = 0; i < timeEvents->size(); i++) {
        auto te = (*timeEvents)[i];
        const auto id = te->Id;

        if (te->FireTime > time) {
            continue;
        }

        const bool result = FireTimeEvent(entity, te);

        if (entity->IsDestroyed()) {
            return;
        }

        if (te->Id == 0) {
            // Event was stopped
            continue;
        }

        if (te->FireTime > time) {
            // Event was already prolonged
            continue;
        }

        if (te->RepeatDuration && result) {
            // Prolong event
            const auto next_fire_time = std::max(te->FireTime + te->RepeatDuration, time + MIN_REPEAT_TIME);

            te->FireTime = next_fire_time;
        }
        else {
            // Remove event
            const auto it = std::ranges::find_if(*timeEvents, [id](const shared_ptr<Entity::TimeEventData>& te2) { return te2->Id == id; });
            FO_RUNTIME_ASSERT(it != timeEvents->end());
            const auto actual_index = numeric_cast<size_t>(std::distance(timeEvents->begin(), it));

            timeEvents->erase(timeEvents->begin() + numeric_cast<ptrdiff_t>(actual_index));
            te->Id = 0;
            i--;
        }
    }
}

auto TimeEventManager::FireTimeEvent(Entity* entity, shared_ptr<Entity::TimeEventData> te) -> bool
{
    FO_STACK_TRACE_ENTRY();

    _curTimeEventEntity = entity;
    auto revert_cur_entity = scope_exit([this]() noexcept { _curTimeEventEntity = nullptr; });
    _curTimeEvent = te.get();
    auto revert_cur_event = scope_exit([this]() noexcept { _curTimeEvent = nullptr; });

    bool call_result = false;

    if (auto* func1 = std::get_if<ScriptFunc<void>>(&te->Func); func1 != nullptr) {
        FO_RUNTIME_ASSERT(entity->IsGlobal());
        call_result = func1->Call();
    }
    else if (auto* func2 = std::get_if<ScriptFunc<void, any_t>>(&te->Func); func2 != nullptr) {
        FO_RUNTIME_ASSERT(entity->IsGlobal());
        call_result = func2->Call(te->Data.empty() ? _emptyAnyValue : te->Data.front());
    }
    else if (auto* func3 = std::get_if<ScriptFunc<void, vector<any_t>>>(&te->Func); func3 != nullptr) {
        FO_RUNTIME_ASSERT(entity->IsGlobal());
        call_result = func3->Call(te->Data);
    }
    else if (auto* func4 = std::get_if<ScriptFunc<void, ScriptSelfEntity*>>(&te->Func); func4 != nullptr) {
        FO_RUNTIME_ASSERT(!entity->IsGlobal());
        call_result = func4->Call(entity);
    }
    else if (auto* func5 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, any_t>>(&te->Func); func5 != nullptr) {
        FO_RUNTIME_ASSERT(!entity->IsGlobal());
        call_result = func5->Call(entity, te->Data.empty() ? _emptyAnyValue : te->Data.front());
    }
    else if (auto* func6 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, vector<any_t>>>(&te->Func); func6 != nullptr) {
        FO_RUNTIME_ASSERT(!entity->IsGlobal());
        call_result = func6->Call(entity, te->Data);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    if (!call_result && te->RepeatDuration) {
        WriteLog("Time event {}{} stopped due to exception", te->FuncName.first, te->FuncName.second != 0 ? " (delegate)" : "");
    }

    return call_result;
}

FO_END_NAMESPACE
