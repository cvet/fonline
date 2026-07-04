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

#include "TimeEvents.h"
#include "EngineBase.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

const timespan TimeEventManager::MIN_REPEAT_TIME = timespan(std::chrono::milliseconds {1});

TimeEventContext::TimeEventContext(uint32_t id, timespan repeat, vector<any_t> data) :
    _id {id},
    _repeat {repeat},
    _data {std::move(data)}
{
    FO_STACK_TRACE_ENTRY();
}

void TimeEventContext::Stop() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _stopped = true;
}

void TimeEventContext::Repeat(timespan repeat) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _repeat = repeat;
    _repeatChanged = true;
}

void TimeEventContext::SetData(any_t data)
{
    FO_STACK_TRACE_ENTRY();

    _data = {std::move(data)};
    _dataChanged = true;
}

void TimeEventContext::SetDataArray(readonly_vector<any_t> data)
{
    FO_STACK_TRACE_ENTRY();

    _data = to_vector(data);
    _dataChanged = true;
}

TimeEventManager::TimeEventManager(ptr<BaseEngine> engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto TimeEventManager::StartTimeEvent(ptr<Entity> entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!entity->IsDestroyed(), "Entity is already destroyed");

    const auto event_id = ++_timeEventCounter;
    const auto effective_delay = std::max(delay, MIN_REPEAT_TIME);

    auto te = SafeAlloc::MakeShared<Entity::TimeEventData>();
    te->Id = event_id;
    te->FuncName = std::visit([](auto&& f) -> ScriptFuncName { return f.GetName(); }, func);
    te->Func = std::move(func);
    te->FireTime = _engine->GameTime.GetFrameTime() + effective_delay;
    te->RepeatDuration = repeat;
    te->Data = std::move(data);

    {
        std::scoped_lock lock {_timeEventLocker};

        auto time_events = entity->EnsureTimeEvents();
        time_events->emplace_back(std::move(te));

        AddEntityTimeEventPolling(entity);
    }

    NotifySchedule(entity, event_id, effective_delay);
    return event_id;
}

auto TimeEventManager::CountTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN_VALUE(!entity->IsDestroyed(), 0, "Destroyed entity used to count time events", entity->GetName(), entity->GetTypeName(), entity->GetId(), id);

    std::scoped_lock lock {_timeEventLocker};

    auto time_events = entity->GetTimeEvents();

    if (!time_events || time_events->empty()) {
        return 0;
    }

    if (id != 0) {
        FO_VERIFY_AND_THROW(func_name == ScriptFuncName(), "Time event callback function name changed unexpectedly");

        for (const auto& te : *time_events) {
            if (te->Id == id) {
                return 1;
            }
        }
    }

    if (func_name != ScriptFuncName()) {
        size_t count = 0;

        for (const auto& te : *time_events) {
            if (te->FuncName == func_name) {
                count++;
            }
        }

        return count;
    }

    return 0;
}

void TimeEventManager::ModifyTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id, optional<timespan> repeat, optional<vector<any_t>> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!entity->IsDestroyed(), "Entity is already destroyed");

    struct ResubmitInfo
    {
        uint32_t EventId {};
        timespan Delay {};
    };

    vector<ResubmitInfo> to_resubmit;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto time_events = entity->GetTimeEvents();

        if (!time_events || time_events->empty()) {
            return;
        }

        const auto effective_delay = repeat.has_value() ? std::max(repeat.value(), MIN_REPEAT_TIME) : timespan::zero;
        const auto fire_time = repeat.has_value() ? _engine->GameTime.GetFrameTime() + effective_delay : nanotime::zero;

        for (size_t i = 0; i < time_events->size(); i++) {
            auto& te = (*time_events)[i];

            if (func_name != ScriptFuncName() && te->FuncName != func_name) {
                continue;
            }
            if (id != 0 && te->Id != id) {
                continue;
            }

            if (repeat.has_value()) {
                te->FireTime = fire_time;
                te->RepeatDuration = repeat.value();
                to_resubmit.push_back({te->Id, effective_delay});
            }
            if (data.has_value()) {
                te->Data = data.value();
            }

            // Identifier may be only one
            if (id != 0) {
                break;
            }
        }
    }

    // Re-fire the dispatcher hook outside the manager's lock so the dispatcher can take its own
    // mutex without nesting concerns.
    for (const auto& info : to_resubmit) {
        NotifyCancel(info.EventId);
        NotifySchedule(entity, info.EventId, info.Delay);
    }
}

void TimeEventManager::StopTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN(!entity->IsDestroyed(), "Destroyed entity used to stop time events", entity->GetName(), entity->GetTypeName(), entity->GetId(), id);

    vector<uint32_t> cancelled_ids;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto time_events = entity->GetTimeEvents();

        if (!time_events || time_events->empty()) {
            return;
        }

        for (size_t i = 0; i < time_events->size();) {
            auto& te = (*time_events)[i];

            if (func_name != ScriptFuncName() && te->FuncName != func_name) {
                i++;
                continue;
            }
            if (id != 0 && te->Id != id) {
                i++;
                continue;
            }

            const auto removed_id = te->Id;
            te->Id = 0;
            time_events->erase(time_events->begin() + numeric_cast<ptrdiff_t>(i)); // te is not valid anymore
            cancelled_ids.push_back(removed_id);

            // Identifier may be only one.
            if (id != 0) {
                break;
            }
        }
    }

    for (auto cid : cancelled_ids) {
        NotifyCancel(cid);
    }
}

void TimeEventManager::AddEntityTimeEventPolling(ptr<Entity> entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock lock {_timeEventLocker};

    _timeEventEntities.emplace(entity.hold_ref());
}

void TimeEventManager::RemoveEntityTimeEventPolling(ptr<Entity> entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock lock {_timeEventLocker};

    _timeEventEntities.erase(entity.hold_ref());
}

void TimeEventManager::ProcessTimeEvents()
{
    FO_STACK_TRACE_ENTRY();

    auto entities = [&] {
        std::scoped_lock lock {_timeEventLocker};

        return copy_hold_ref(_timeEventEntities);
    }();

    for (ptr<Entity> entity : entities) {
        if (entity->IsDestroyed()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        ProcessEntityTimeEvents(entity);

        if (entity->IsDestroyed()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        std::scoped_lock lock {_timeEventLocker};

        if (!entity->HasTimeEvents()) {
            RemoveEntityTimeEventPolling(entity);
        }
    }
}

void TimeEventManager::ClearTimeEvents()
{
    FO_STACK_TRACE_ENTRY();

    vector<uint32_t> cancelled_ids;

    {
        std::scoped_lock lock {_timeEventLocker};

        for (ptr<Entity> entity : copy_hold_ref(_timeEventEntities)) {
            auto time_events = entity->GetTimeEvents();

            if (time_events && !time_events->empty()) {
                cancelled_ids.reserve(cancelled_ids.size() + time_events->size());

                for (auto& te : *time_events) {
                    if (te->Id != 0) {
                        cancelled_ids.push_back(te->Id);
                    }
                }
            }

            entity->ClearAllTimeEvents();
        }

        _timeEventEntities.clear();
    }

    for (auto cid : cancelled_ids) {
        NotifyCancel(cid);
    }
}

void TimeEventManager::ProcessEntityTimeEvents(ptr<Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    vector<shared_ptr<Entity::TimeEventData>> ready_events;
    const auto time = _engine->GameTime.GetFrameTime();

    {
        std::scoped_lock lock {_timeEventLocker};

        auto time_events = entity->GetTimeEvents();

        if (!time_events || time_events->empty()) {
            return;
        }

        ready_events.reserve(time_events->size());

        for (auto& te : *time_events) {
            if (te->FireTime <= time) {
                ready_events.emplace_back(te);
            }
        }
    }

    for (auto& te : ready_events) {
        {
            std::scoped_lock lock {_timeEventLocker};

            auto time_events = entity->GetTimeEvents();

            if (!time_events || time_events->empty()) {
                return;
            }

            if (te->Id == 0) {
                continue;
            }

            const auto it = std::ranges::find(time_events->begin(), time_events->end(), te);

            if (it == time_events->end()) {
                continue;
            }
            if (te->FireTime > time) {
                continue;
            }
        }

        const FiredTimeEvent result = FireTimeEvent(entity, te);

        if (entity->IsDestroyed()) {
            return;
        }

        PostFireTimeEvent(entity, te, result);
    }
}

auto TimeEventManager::CollectReadyTimeEvents(optional<timespan>& time_until_next) -> vector<ReadyTimeEvent>
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock lock {_timeEventLocker};

    vector<ReadyTimeEvent> ready;
    optional<nanotime> next_future_fire_time;
    const auto time = _engine->GameTime.GetFrameTime();

    for (ptr<Entity> entity : copy_hold_ref(_timeEventEntities)) {
        if (entity->IsDestroyed()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        auto time_events = entity->GetTimeEvents();

        if (!time_events || time_events->empty()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        for (size_t i = 0; i < time_events->size(); i++) {
            auto& te = (*time_events)[i];

            if (te->FireTime <= time) {
                ready.emplace_back(ReadyTimeEvent {entity.hold_ref(), te});
            }
            else if (!next_future_fire_time.has_value() || te->FireTime < next_future_fire_time.value()) {
                next_future_fire_time = te->FireTime;
            }
        }

        if (!entity->HasTimeEvents()) {
            RemoveEntityTimeEventPolling(entity);
        }
    }

    time_until_next = next_future_fire_time.has_value() ? optional {next_future_fire_time.value() - time} : std::nullopt;
    return ready;
}

void TimeEventManager::PostFireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te, const FiredTimeEvent& result)
{
    FO_STACK_TRACE_ENTRY();

    if (entity->IsDestroyed()) {
        return;
    }

    std::scoped_lock lock {_timeEventLocker};

    if (te->Id == 0) {
        return;
    }

    const auto time = _engine->GameTime.GetFrameTime();

    auto remove_event = [entity, &te]() mutable {
        auto time_events = entity->GetTimeEvents();

        if (time_events) {
            const auto id = te->Id;
            const auto it = std::ranges::find_if(*time_events, [id](const shared_ptr<Entity::TimeEventData>& te2) { return te2->Id == id; });

            if (it != time_events->end()) {
                time_events->erase(it);
                te->Id = 0;
            }
        }
    };

    if (result.Context) {
        auto context = result.Context.as_ptr();

        if (context->IsStopped()) {
            remove_event();
            return;
        }

        if (context->IsDataChanged()) {
            const_span<any_t> data = context->GetRawData();
            te->Data.assign(data.begin(), data.end());
        }

        if (context->IsRepeatChanged()) {
            te->RepeatDuration = context->GetRepeat();
            te->FireTime = time + std::max(te->RepeatDuration, MIN_REPEAT_TIME);
            return;
        }
    }

    if (te->RepeatDuration && result.CallResult) {
        const auto next_fire_time = std::max(te->FireTime + te->RepeatDuration, time + MIN_REPEAT_TIME);
        te->FireTime = next_fire_time;
    }
    else {
        remove_event();
    }
}

auto TimeEventManager::FireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te) -> FiredTimeEvent
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN_VALUE(!entity->IsDestroyed(), {}, "Destroyed entity tried to fire a time event", entity->GetName(), entity->GetTypeName(), entity->GetId(), te->Id);

    uint32_t event_id;
    timespan repeat_duration;
    vector<any_t> data;

    {
        std::scoped_lock lock {_timeEventLocker};

        event_id = te->Id;
        repeat_duration = te->RepeatDuration;
        data = te->Data;
    }

    if (event_id == 0) {
        return {};
    }

    auto context = SafeAlloc::MakeRefCounted<TimeEventContext>(event_id, repeat_duration, data);
    bool call_result = false;

    if (nptr<ScriptFunc<void>> nullable_func1 = std::get_if<ScriptFunc<void>>(&te->Func); nullable_func1) {
        auto func1 = nullable_func1.as_ptr();
        FO_VERIFY_AND_THROW(entity->IsGlobal(), "Time event expects a global entity");
        call_result = func1->Call();
    }
    else if (nptr<ScriptFunc<void, any_t>> nullable_func2 = std::get_if<ScriptFunc<void, any_t>>(&te->Func); nullable_func2) {
        auto func2 = nullable_func2.as_ptr();
        FO_VERIFY_AND_THROW(entity->IsGlobal(), "Time event expects a global entity");
        call_result = func2->Call(data.empty() ? _emptyAnyValue : data.front());
    }
    else if (nptr<ScriptFunc<void, vector<any_t>>> nullable_func3 = std::get_if<ScriptFunc<void, vector<any_t>>>(&te->Func); nullable_func3) {
        auto func3 = nullable_func3.as_ptr();
        FO_VERIFY_AND_THROW(entity->IsGlobal(), "Time event expects a global entity");
        call_result = func3->Call(data);
    }
    else if (nptr<ScriptFunc<void, TimeEventContext*>> nullable_func4 = std::get_if<ScriptFunc<void, TimeEventContext*>>(&te->Func); nullable_func4) {
        auto func4 = nullable_func4.as_ptr();
        FO_VERIFY_AND_THROW(entity->IsGlobal(), "Time event expects a global entity");
        call_result = func4->Call(context.get());
    }
    else if (nptr<ScriptFunc<void, ScriptSelfEntity*>> nullable_func5 = std::get_if<ScriptFunc<void, ScriptSelfEntity*>>(&te->Func); nullable_func5) {
        auto func5 = nullable_func5.as_ptr();
        FO_VERIFY_AND_THROW(!entity->IsGlobal(), "Time event expects a non-global entity");
        call_result = func5->Call(entity.get());
    }
    else if (nptr<ScriptFunc<void, ScriptSelfEntity*, any_t>> nullable_func6 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, any_t>>(&te->Func); nullable_func6) {
        auto func6 = nullable_func6.as_ptr();
        FO_VERIFY_AND_THROW(!entity->IsGlobal(), "Time event expects a non-global entity");
        call_result = func6->Call(entity.get(), data.empty() ? _emptyAnyValue : data.front());
    }
    else if (nptr<ScriptFunc<void, ScriptSelfEntity*, vector<any_t>>> nullable_func7 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, vector<any_t>>>(&te->Func); nullable_func7) {
        auto func7 = nullable_func7.as_ptr();
        FO_VERIFY_AND_THROW(!entity->IsGlobal(), "Time event expects a non-global entity");
        call_result = func7->Call(entity.get(), data);
    }
    else if (nptr<ScriptFunc<void, ScriptSelfEntity*, TimeEventContext*>> nullable_func8 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, TimeEventContext*>>(&te->Func); nullable_func8) {
        auto func8 = nullable_func8.as_ptr();
        FO_VERIFY_AND_THROW(!entity->IsGlobal(), "Time event expects a non-global entity");
        call_result = func8->Call(entity.get(), context.get());
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    if (!call_result && repeat_duration) {
        WriteLog("Time event {}{} stopped due to exception", te->FuncName.first, te->FuncName.second != 0 ? " (delegate)" : "");
    }

    return FiredTimeEvent {.CallResult = call_result, .Context = std::move(context)};
}

void TimeEventManager::SetDispatcherHooks(DispatcherHooks hooks)
{
    FO_STACK_TRACE_ENTRY();

    _dispatcher = std::move(hooks);
}

void TimeEventManager::ClearDispatcherHooks()
{
    FO_STACK_TRACE_ENTRY();

    _dispatcher = DispatcherHooks {};
}

void TimeEventManager::NotifySchedule(ptr<Entity> entity, uint32_t event_id, timespan delay)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_dispatcher.Schedule) {
        _dispatcher.Schedule(entity.hold_ref(), event_id, delay);
    }
}

void TimeEventManager::NotifyCancel(uint32_t event_id)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_dispatcher.Cancel) {
        _dispatcher.Cancel(event_id);
    }
}

void TimeEventManager::CancelAllForEntity(ptr<Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    vector<uint32_t> cancelled_ids;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto time_events = entity->GetTimeEvents();

        if (time_events && !time_events->empty()) {
            cancelled_ids.reserve(time_events->size());

            for (auto& te : *time_events) {
                if (te->Id != 0) {
                    cancelled_ids.push_back(te->Id);
                }
            }
        }

        entity->ClearAllTimeEvents();
        RemoveEntityTimeEventPolling(entity);
    }

    for (auto cid : cancelled_ids) {
        NotifyCancel(cid);
    }
}

auto TimeEventManager::FireAndAdvance(ptr<Entity> entity, uint32_t event_id) -> optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    if (entity->IsDestroyed()) {
        return std::nullopt;
    }

    shared_ptr<Entity::TimeEventData> te;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto time_events = entity->GetTimeEvents();

        if (!time_events || time_events->empty()) {
            return std::nullopt;
        }

        const auto it = std::ranges::find_if(*time_events, [event_id](const shared_ptr<Entity::TimeEventData>& candidate) { return candidate->Id == event_id; });

        if (it == time_events->end()) {
            return std::nullopt;
        }

        te = *it;
    }

    const auto fired = FireTimeEvent(entity, te);

    nanotime next_fire_time;

    {
        std::scoped_lock lock {_timeEventLocker};

        PostFireTimeEvent(entity, te, fired);

        if (entity->IsDestroyed()) {
            // The handler destroyed its owner; the fired event is done and must not be rescheduled.
            return std::nullopt;
        }

        if (te->Id == 0) {
            // PostFire removed the event because it was a one-shot or the script asked to stop.
            return std::nullopt;
        }

        next_fire_time = te->FireTime;
    }

    const auto now = _engine->GameTime.GetFrameTime();

    if (next_fire_time <= now) {
        // Edge case: handler took long enough that next FireTime is already in the past. Schedule
        // an immediate rerun (tiny non-zero delay so the dispatcher doesn't busy-loop).
        return MIN_REPEAT_TIME;
    }

    return next_fire_time - now;
}

FO_END_NAMESPACE
