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

TimeEventManager::TimeEventManager(BaseEngine& engine) :
    _engine {&engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto TimeEventManager::StartTimeEvent(Entity* entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto effective_delay = std::max(delay, MIN_REPEAT_TIME);

    auto te = SafeAlloc::MakeShared<Entity::TimeEventData>();
    te->Id = ++_timeEventCounter;
    te->FuncName = std::visit([](auto&& f) -> ScriptFuncName { return f.GetName(); }, func);
    te->Func = std::move(func);
    te->FireTime = _engine->GetFrameTime() + effective_delay;
    te->RepeatDuration = repeat;
    te->Data = std::move(data);

    const auto event_id = te->Id;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto& timeEvents = entity->GetRawTimeEvents();
        make_if_not_exists(timeEvents);
        timeEvents->emplace_back(std::move(te));

        AddEntityTimeEventPolling(entity);
    }

    NotifySchedule(entity, event_id, effective_delay);
    return event_id;
}

auto TimeEventManager::CountTimeEvent(Entity* entity, ScriptFuncName func_name, uint32_t id) const -> size_t
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

void TimeEventManager::ModifyTimeEvent(Entity* entity, ScriptFuncName func_name, uint32_t id, optional<timespan> repeat, optional<vector<any_t>> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    struct ResubmitInfo
    {
        uint32_t EventId {};
        timespan Delay {};
    };

    vector<ResubmitInfo> to_resubmit;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto& timeEvents = entity->GetRawTimeEvents();

        if (!timeEvents || timeEvents->empty()) {
            return;
        }

        const auto effective_delay = repeat.has_value() ? std::max(repeat.value(), MIN_REPEAT_TIME) : timespan::zero;
        const auto fire_time = repeat.has_value() ? _engine->GetFrameTime() + effective_delay : nanotime::zero;

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

void TimeEventManager::StopTimeEvent(Entity* entity, ScriptFuncName func_name, uint32_t id)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    vector<uint32_t> cancelled_ids;

    {
        std::scoped_lock lock {_timeEventLocker};

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

            const auto removed_id = te->Id;
            te->Id = 0;
            timeEvents->erase(timeEvents->begin() + numeric_cast<ptrdiff_t>(i)); // te is not valid anymore
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

void TimeEventManager::AddEntityTimeEventPolling(Entity* entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock lock {_timeEventLocker};

    _timeEventEntities.emplace(entity);
}

void TimeEventManager::RemoveEntityTimeEventPolling(Entity* entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock lock {_timeEventLocker};

    _timeEventEntities.erase(entity);
}

void TimeEventManager::ProcessTimeEvents()
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock lock {_timeEventLocker};

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

    vector<uint32_t> cancelled_ids;

    {
        std::scoped_lock lock {_timeEventLocker};

        for (auto* entity : copy_hold_ref(_timeEventEntities)) {
            const auto& time_events = entity->GetRawTimeEvents();

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

        if (te->FireTime > time) {
            continue;
        }

        const auto result = FireTimeEvent(entity, te);

        if (entity->IsDestroyed()) {
            return;
        }

        PostFireTimeEvent(entity, te, result);

        if (te->Id == 0) {
            i--;
        }
    }
}

auto TimeEventManager::CollectReadyTimeEvents(optional<timespan>& time_until_next) -> vector<ReadyTimeEvent>
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock lock {_timeEventLocker};

    vector<ReadyTimeEvent> ready;
    optional<nanotime> next_future_fire_time;
    const auto time = _engine->GetFrameTime();

    for (auto* entity : copy_hold_ref(_timeEventEntities)) {
        if (entity->IsDestroyed()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        auto& timeEvents = entity->GetRawTimeEvents();

        if (!timeEvents || timeEvents->empty()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        for (size_t i = 0; i < timeEvents->size(); i++) {
            auto& te = (*timeEvents)[i];

            if (te->FireTime <= time) {
                ready.emplace_back(ReadyTimeEvent {refcount_ptr<Entity> {entity}, te});
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

void TimeEventManager::PostFireTimeEvent(Entity* entity, shared_ptr<Entity::TimeEventData> te, const FiredTimeEvent& result)
{
    FO_STACK_TRACE_ENTRY();

    if (entity->IsDestroyed()) {
        return;
    }

    if (te->Id == 0) {
        return;
    }

    const auto time = _engine->GetFrameTime();

    const auto remove_event = [entity, &te] {
        auto& timeEvents = entity->GetRawTimeEvents();

        if (timeEvents) {
            const auto id = te->Id;
            const auto it = std::ranges::find_if(*timeEvents, [id](const shared_ptr<Entity::TimeEventData>& te2) { return te2->Id == id; });

            if (it != timeEvents->end()) {
                timeEvents->erase(it);
                te->Id = 0;
            }
        }
    };

    if (result.Context && result.Context->IsStopped()) {
        remove_event();
        return;
    }

    if (result.Context && result.Context->IsDataChanged()) {
        te->Data = result.Context->GetDataRaw();
    }

    if (result.Context && result.Context->IsRepeatChanged()) {
        te->RepeatDuration = result.Context->GetRepeat();
        te->FireTime = time + std::max(te->RepeatDuration, MIN_REPEAT_TIME);
        return;
    }

    if (te->FireTime > time) {
        return;
    }

    if (te->RepeatDuration && result.CallResult) {
        const auto next_fire_time = std::max(te->FireTime + te->RepeatDuration, time + MIN_REPEAT_TIME);
        te->FireTime = next_fire_time;
    }
    else {
        remove_event();
    }
}

auto TimeEventManager::FireTimeEvent(Entity* entity, shared_ptr<Entity::TimeEventData> te) -> FiredTimeEvent
{
    FO_STACK_TRACE_ENTRY();

    auto context = SafeAlloc::MakeRefCounted<TimeEventContext>(te->Id, te->RepeatDuration, te->Data);
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
    else if (auto* func4 = std::get_if<ScriptFunc<void, TimeEventContext*>>(&te->Func); func4 != nullptr) {
        FO_RUNTIME_ASSERT(entity->IsGlobal());
        call_result = func4->Call(context.get());
    }
    else if (auto* func5 = std::get_if<ScriptFunc<void, ScriptSelfEntity*>>(&te->Func); func5 != nullptr) {
        FO_RUNTIME_ASSERT(!entity->IsGlobal());
        call_result = func5->Call(entity);
    }
    else if (auto* func6 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, any_t>>(&te->Func); func6 != nullptr) {
        FO_RUNTIME_ASSERT(!entity->IsGlobal());
        call_result = func6->Call(entity, te->Data.empty() ? _emptyAnyValue : te->Data.front());
    }
    else if (auto* func7 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, vector<any_t>>>(&te->Func); func7 != nullptr) {
        FO_RUNTIME_ASSERT(!entity->IsGlobal());
        call_result = func7->Call(entity, te->Data);
    }
    else if (auto* func8 = std::get_if<ScriptFunc<void, ScriptSelfEntity*, TimeEventContext*>>(&te->Func); func8 != nullptr) {
        FO_RUNTIME_ASSERT(!entity->IsGlobal());
        call_result = func8->Call(entity, context.get());
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    if (!call_result && te->RepeatDuration) {
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

void TimeEventManager::NotifySchedule(Entity* entity, uint32_t event_id, timespan delay)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_dispatcher.Schedule) {
        _dispatcher.Schedule(refcount_ptr<Entity> {entity}, event_id, delay);
    }
}

void TimeEventManager::NotifyCancel(uint32_t event_id)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_dispatcher.Cancel) {
        _dispatcher.Cancel(event_id);
    }
}

void TimeEventManager::CancelAllForEntity(Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    vector<uint32_t> cancelled_ids;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto& timeEvents = entity->GetRawTimeEvents();

        if (timeEvents && !timeEvents->empty()) {
            cancelled_ids.reserve(timeEvents->size());

            for (auto& te : *timeEvents) {
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

auto TimeEventManager::FireAndAdvance(Entity* entity, uint32_t event_id) -> optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    if (entity->IsDestroyed()) {
        return std::nullopt;
    }

    shared_ptr<Entity::TimeEventData> te;

    {
        std::scoped_lock lock {_timeEventLocker};

        auto& timeEvents = entity->GetRawTimeEvents();

        if (!timeEvents || timeEvents->empty()) {
            return std::nullopt;
        }

        const auto it = std::ranges::find_if(*timeEvents, [event_id](const shared_ptr<Entity::TimeEventData>& candidate) { return candidate->Id == event_id; });

        if (it == timeEvents->end()) {
            return std::nullopt;
        }

        te = *it;
    }

    const auto fired = FireTimeEvent(entity, te);
    PostFireTimeEvent(entity, te, fired);

    if (entity->IsDestroyed() || te->Id == 0) {
        // Either the entity was destroyed during fire, or PostFire removed the event because it
        // was a one-shot or the script asked to stop.
        return std::nullopt;
    }

    const auto now = _engine->GetFrameTime();

    if (te->FireTime <= now) {
        // Edge case: handler took long enough that next FireTime is already in the past. Schedule
        // an immediate rerun (tiny non-zero delay so the dispatcher doesn't busy-loop).
        return MIN_REPEAT_TIME;
    }

    return te->FireTime - now;
}

FO_END_NAMESPACE
