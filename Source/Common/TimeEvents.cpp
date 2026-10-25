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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
}

void TimeEventContext::Stop() noexcept
{
    _stopped = true;
}

void TimeEventContext::Repeat(timespan repeat) noexcept
{
    _repeat = repeat;
    _repeatChanged = true;
}

void TimeEventContext::SetData(any_t data)
{
    _data = {std::move(data)};
    _dataChanged = true;
}

void TimeEventContext::SetDataArray(readonly_vector<any_t> data)
{
    _data = to_vector(data);
    _dataChanged = true;
}

TimeEventManager::TimeEventManager(ptr<BaseEngine> engine) :
    _engine {engine}
{
}

auto TimeEventManager::GetTimeEventFuncAlternativeCount() noexcept -> size_t
{
    return std::variant_size_v<Entity::TimeEventData::FuncType>;
}

auto TimeEventManager::RestartTimeEvent(ptr<Entity> entity, hstring func_name, size_t func_alternative, uint32_t event_id, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t
{
    FO_VERIFY_AND_THROW(func_alternative < GetTimeEventFuncAlternativeCount(), "Unknown time event function alternative", func_name, func_alternative);
    FO_VERIFY_AND_THROW(event_id != 0, "Restored time event id cannot be zero", func_name);

    optional<Entity::TimeEventData::FuncType> func = ResolveTimeEventFunc(entity, func_name, func_alternative);

    if (!func.has_value()) {
        throw TimeEventException("Time event function does not exist with the recorded signature", func_name, func_alternative);
    }

    return StartTimeEventWithId(entity, std::move(*func), event_id, delay, repeat, std::move(data));
}

auto TimeEventManager::StartTimeEvent(ptr<Entity> entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t
{
    return StartTimeEventWithId(entity, std::move(func), std::nullopt, delay, repeat, std::move(data));
}

void TimeEventManager::ReserveTimeEventIds(uint32_t last_event_id)
{
    std::scoped_lock lock {_timeEventLocker};

    uint32_t current = _timeEventCounter.load(std::memory_order_relaxed);
    _timeEventCounter.store(std::max(current, last_event_id), std::memory_order_relaxed);
}

auto TimeEventManager::CountTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id, bool exclude_executing) const -> size_t
{
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
                return !exclude_executing || !te->Execution ? 1 : 0;
            }
        }
    }

    if (func_name != ScriptFuncName()) {
        size_t count = 0;

        for (const auto& te : *time_events) {
            if (te->FuncName == func_name && (!exclude_executing || !te->Execution)) {
                count++;
            }
        }

        return count;
    }

    return 0;
}

auto TimeEventManager::CaptureSerializableTimeEvents() const -> TimeEventCapture
{
    std::scoped_lock lock {_timeEventLocker};

    TimeEventCapture capture;
    capture.LastEventId = _timeEventCounter.load(std::memory_order_relaxed);
    nanotime now = _engine->GameTime.GetFrameTime();

    for (const auto& entity : _timeEventEntities) {
        nptr<const Entity::TimeEventList> time_events = entity->GetTimeEvents();

        if (!time_events) {
            continue;
        }

        for (const auto& te : *time_events) {
            // Delegate object addresses are valid only in the current process
            if (te->FuncName.second != 0) {
                capture.DelegateBoundCount++;
                continue;
            }
            if (te->Execution) {
                capture.ExecutingCount++;
                continue;
            }
            if (!te->FuncName.first || !ResolveTimeEventFunc(entity, te->FuncName.first, te->Func.index())) {
                capture.UnresolvedFunctionCount++;
                continue;
            }

            // An overdue event has no remaining wait
            timespan remaining = te->FireTime > now ? te->FireTime - now : timespan {};

            capture.Events.emplace_back(SerializableTimeEvent {
                .EntityTypeName = string {entity->GetTypeName().as_str()},
                .EntityId = entity->GetId(),
                .EventId = te->Id,
                .FuncName = string {te->FuncName.first.as_str()},
                .FuncAlternative = te->Func.index(),
                .FireDelay = remaining,
                .RepeatDuration = te->RepeatDuration,
                .Data = te->Data,
            });
        }
    }

    return capture;
}

auto TimeEventManager::GetDiagnostics() const -> Diagnostics
{
    std::scoped_lock lock {_timeEventLocker};

    Diagnostics diagnostics;
    diagnostics.EntityCount = _timeEventEntities.size();

    for (const auto& entity : _timeEventEntities) {
        if (auto time_events = entity->GetTimeEvents(); time_events) {
            diagnostics.EventCount += time_events->size();
        }
    }

    return diagnostics;
}

void TimeEventManager::ModifyTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id, optional<timespan> repeat, optional<vector<any_t>> data)
{
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

        auto effective_delay = repeat.has_value() ? std::max(repeat.value(), MIN_REPEAT_TIME) : timespan::zero;
        nanotime fire_time = repeat.has_value() ? TimeEventDeadline(_engine->GameTime.GetFrameTime(), effective_delay) : nanotime::zero;

        for (size_t i = 0; i < time_events->size(); i++) {
            auto& te = (*time_events)[i];

            if (func_name != ScriptFuncName() && te->FuncName != func_name) {
                continue;
            }
            if (id != 0 && te->Id != id) {
                continue;
            }

            if (repeat.has_value()) {
                te->RepeatDuration = repeat.value();

                if (!te->Execution) {
                    te->FireTime = fire_time;
                    to_resubmit.push_back({te->Id, effective_delay});
                }
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
    // mutex without nesting concerns
    size_t resubmitted = 0;
    auto remove_unscheduled = scope_fail([&]() noexcept {
        for (size_t i = resubmitted; i < to_resubmit.size(); i++) {
            safe_call([&] { StopTimeEvent(entity, {}, to_resubmit[i].EventId); });
        }
    });

    for (const auto& info : to_resubmit) {
        NotifyCancel(info.EventId);
        NotifySchedule(entity, info.EventId, info.Delay);
        resubmitted++;
    }
}

void TimeEventManager::StopTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id)
{
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

            uint32_t removed_id = te->Id;
            te->Id = 0;
            time_events->erase(time_events->begin() + numeric_cast<ptrdiff_t>(i)); // te is not valid anymore
            cancelled_ids.push_back(removed_id);

            // Identifier may be only one
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
    std::scoped_lock lock {_timeEventLocker};

    _timeEventEntities.emplace(entity.hold_ref());
}

void TimeEventManager::RemoveEntityTimeEventPolling(ptr<Entity> entity)
{
    std::scoped_lock lock {_timeEventLocker};

    _timeEventEntities.erase(entity.hold_ref());
}

void TimeEventManager::ProcessTimeEvents()
{
    FO_TRACE_ZONE(Entity);

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
    FO_TRACE_ZONE(Entity);

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
    small_vector<uint32_t, 8> ready_ids;
    nanotime time = _engine->GameTime.GetFrameTime();

    {
        std::scoped_lock lock {_timeEventLocker};

        auto time_events = entity->GetTimeEvents();

        if (!time_events) {
            return;
        }

        for (const auto& te : *time_events) {
            if (te->Execution ? te->Execution->Completed && !te->Execution->NativeCallActive : te->FireTime <= time) {
                ready_ids.emplace_back(te->Id);
            }
        }
    }

    for (uint32_t id : ready_ids) {
        (void)FireAndAdvance(entity, id);
    }
}

auto TimeEventManager::CollectReadyTimeEvents(optional<timespan>& time_until_next) -> vector<ReadyTimeEvent>
{
    FO_TRACE_ZONE(Entity);

    std::scoped_lock lock {_timeEventLocker};

    vector<ReadyTimeEvent> ready;
    optional<nanotime> next_future_fire_time;
    nanotime time = _engine->GameTime.GetFrameTime();

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

            if (te->Execution) {
                if (te->Execution->Completed && !te->Execution->NativeCallActive) {
                    ready.emplace_back(ReadyTimeEvent {entity.hold_ref(), te});
                }

                continue;
            }

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

void TimeEventManager::PostFireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te)
{
    if (entity->IsDestroyed()) {
        return;
    }

    std::scoped_lock lock {_timeEventLocker};
    auto time_events = entity->GetTimeEvents();

    if (!time_events || te->Id == 0 || !te->Execution || !te->Execution->Completed || te->Execution->NativeCallActive) {
        return;
    }

    auto it = std::ranges::find(*time_events, te);

    if (it == time_events->end()) {
        return;
    }

    auto execution = te->Execution;
    auto context = execution->Context;
    FO_VERIFY_AND_THROW(context, "Time event execution context is missing");

    auto remove_event = [&] {
        te->Id = 0;
        te->Execution.reset();
        time_events->erase(it);
    };

    // A failed deadline cannot retain a record whose dispatcher job has terminated
    auto remove_failed_event = scope_fail([&]() noexcept { remove_event(); });

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
    }

    if (context->IsRepeatChanged() || (te->RepeatDuration && execution->CallResult)) {
        te->FireTime = TimeEventDeadline(execution->CompletionTime, std::max(te->RepeatDuration, MIN_REPEAT_TIME));
        te->Execution.reset();
    }
    else {
        if (!execution->CallResult && te->RepeatDuration) {
            logging::write("Time event {}{} stopped due to exception", te->FuncName.first, te->FuncName.second != 0 ? " (delegate)" : "");
        }

        remove_event();
    }
}

void TimeEventManager::FireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te)
{
    FO_TRACE_ZONE(Entity);

    shared_ptr<TimeEventExecution> execution;
    vector<any_t> data;

    {
        std::scoped_lock lock {_timeEventLocker};
        auto time_events = entity->GetTimeEvents();

        if (entity->IsDestroyed() || !time_events || te->Id == 0 || te->Execution || std::ranges::find(*time_events, te) == time_events->end()) {
            return;
        }

        FO_VERIFY_AND_THROW(entity->IsGlobal() == (te->Func.index() < 4), "Time event callback owner kind does not match");
        data = te->Data;
        execution = safe_alloc::make_shared<TimeEventExecution>();
        execution->Context = safe_alloc::make_refcounted<TimeEventContext>(te->Id, te->RepeatDuration, data);
        execution->NativeCallActive = true;
        te->Execution = execution;
    }

    auto complete = [this, engine = _engine.hold_ref(), entity = entity.hold_ref(), te, execution](bool success) mutable {
        uint32_t event_id = 0;

        {
            std::scoped_lock lock {_timeEventLocker};

            execution->CallResult = success;
            execution->CompletionTime = engine->GameTime.GetFrameTime();
            execution->Completed = true;

            if (!execution->NativeCallActive && !entity->IsDestroyed()) {
                event_id = te->Id;
            }
        }

        // The dispatcher reacquires the owner before touching its event list after a suspended script
        if (event_id != 0) {
            NotifySchedule(entity, event_id, MIN_REPEAT_TIME);
        }
    };
    auto context = execution->Context.as_ptr();

    if (auto func1 = std::get_if<ScriptFunc<void>>(&te->Func); func1) {
        (void)func1->CallWithCompletion(std::move(complete));
    }
    else if (auto func2 = std::get_if<ScriptFunc<void, any_t>>(&te->Func); func2) {
        (void)func2->CallWithCompletion(std::move(complete), data.empty() ? _emptyAnyValue : data.front());
    }
    else if (auto func3 = std::get_if<ScriptFunc<void, vector<any_t>>>(&te->Func); func3) {
        (void)func3->CallWithCompletion(std::move(complete), data);
    }
    else if (auto func4 = std::get_if<ScriptFunc<void, ptr<TimeEventContext>>>(&te->Func); func4) {
        (void)func4->CallWithCompletion(std::move(complete), context);
    }
    else if (auto func5 = std::get_if<ScriptFunc<void, ptr<ScriptSelfEntity>>>(&te->Func); func5) {
        (void)func5->CallWithCompletion(std::move(complete), entity);
    }
    else if (auto func6 = std::get_if<ScriptFunc<void, ptr<ScriptSelfEntity>, any_t>>(&te->Func); func6) {
        (void)func6->CallWithCompletion(std::move(complete), entity, data.empty() ? _emptyAnyValue : data.front());
    }
    else if (auto func7 = std::get_if<ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>>>(&te->Func); func7) {
        (void)func7->CallWithCompletion(std::move(complete), entity, data);
    }
    else if (auto func8 = std::get_if<ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>>>(&te->Func); func8) {
        (void)func8->CallWithCompletion(std::move(complete), entity, context);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    {
        std::scoped_lock lock {_timeEventLocker};

        execution->NativeCallActive = false;
    }

    PostFireTimeEvent(entity, te);
}

void TimeEventManager::SetDispatcherHooks(DispatcherHooks hooks)
{
    _dispatcher = std::move(hooks);
    _dispatcherPaused.store(false, std::memory_order_release);
}

void TimeEventManager::PauseDispatcherHooks()
{
    // Workers read notifications lock-free, so hook objects cannot change until every worker exits.
    // Pause through the atomic flag while workers drain
    _dispatcherPaused.store(true, std::memory_order_release);
}

void TimeEventManager::ClearDispatcherHooks()
{
    _dispatcher = DispatcherHooks {};
}

void TimeEventManager::NotifySchedule(ptr<Entity> entity, uint32_t event_id, timespan delay)
{
    if (_dispatcher.Schedule && !_dispatcherPaused.load(std::memory_order_acquire)) {
        _dispatcher.Schedule(entity.hold_ref(), event_id, delay);
    }
}

void TimeEventManager::NotifyCancel(uint32_t event_id)
{
    if (_dispatcher.Cancel && !_dispatcherPaused.load(std::memory_order_acquire)) {
        _dispatcher.Cancel(event_id);
    }
}

void TimeEventManager::CancelAllForEntity(ptr<Entity> entity) noexcept
{
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
        try {
            NotifyCancel(cid);
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }
}

auto TimeEventManager::FireAndAdvance(ptr<Entity> entity, uint32_t event_id) -> optional<timespan>
{
    if (entity->IsDestroyed()) {
        return std::nullopt;
    }

    shared_ptr<Entity::TimeEventData> te;
    bool finalize = false;

    {
        std::scoped_lock lock {_timeEventLocker};
        auto time_events = entity->GetTimeEvents();

        if (!time_events) {
            return std::nullopt;
        }

        auto it = std::ranges::find_if(*time_events, [event_id](const auto& candidate) { return candidate->Id == event_id; });

        if (it == time_events->end()) {
            return std::nullopt;
        }

        te = *it;

        if (te->Execution) {
            if (!te->Execution->Completed || te->Execution->NativeCallActive) {
                return std::nullopt;
            }

            finalize = true;
        }
        else {
            nanotime dispatch_time = _engine->GameTime.GetFrameTime();

            // The dispatcher clock can wake before engine time reaches the deadline
            if (te->FireTime > dispatch_time) {
                return te->FireTime - dispatch_time;
            }
        }
    }

    if (finalize) {
        PostFireTimeEvent(entity, te);
    }
    else {
        FireTimeEvent(entity, te);
    }

    std::scoped_lock lock {_timeEventLocker};

    if (entity->IsDestroyed()) {
        return std::nullopt;
    }

    auto time_events = entity->GetTimeEvents();

    if (!time_events || te->Id == 0 || te->Execution || std::ranges::find(*time_events, te) == time_events->end()) {
        return std::nullopt;
    }

    nanotime now = _engine->GameTime.GetFrameTime();
    return te->FireTime > now ? te->FireTime - now : MIN_REPEAT_TIME;
}

auto TimeEventManager::StartTimeEventWithId(ptr<Entity> entity, Entity::TimeEventData::FuncType func, optional<uint32_t> restored_id, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t
{
    FO_VERIFY_AND_THROW(!entity->IsDestroyed(), "Entity is already destroyed");
    FO_VERIFY_AND_THROW(!entity->IsDestroying(), "Cannot start a time event for an entity that is being destroyed", entity->GetTypeName(), entity->GetId());

    timespan effective_delay = std::max(delay, MIN_REPEAT_TIME);
    auto te = safe_alloc::make_shared<Entity::TimeEventData>();
    te->FuncName = std::visit([](auto&& f) -> ScriptFuncName { return f.GetName(); }, func);
    te->Func = std::move(func);
    te->FireTime = TimeEventDeadline(_engine->GameTime.GetFrameTime(), effective_delay);
    // The first repeat must also be representable; negative repeats retain their usual minimum interval
    (void)TimeEventDeadline(te->FireTime, repeat);
    te->RepeatDuration = repeat;
    te->Data = std::move(data);
    uint32_t event_id;

    {
        std::scoped_lock lock {_timeEventLocker};

        uint32_t counter = _timeEventCounter.load(std::memory_order_relaxed);
        FO_VERIFY_AND_THROW(restored_id.has_value() || counter < std::numeric_limits<uint32_t>::max(), "Time event id space is exhausted");
        event_id = restored_id ? *restored_id : counter + 1;

        if (restored_id) {
            for (const auto& owner : _timeEventEntities) {
                if (auto events = owner->GetTimeEvents(); events && std::ranges::any_of(*events, [event_id](const shared_ptr<Entity::TimeEventData>& event) { return event->Id == event_id; })) {
                    throw TimeEventException("Restored time event id is already active", event_id);
                }
            }
        }

        te->Id = event_id;
        _timeEventCounter.store(std::max(counter, event_id), std::memory_order_relaxed);
        entity->EnsureTimeEvents()->emplace_back(std::move(te));
        AddEntityTimeEventPolling(entity);
    }

    auto remove_unscheduled = scope_fail([&]() noexcept { safe_call([&] { StopTimeEvent(entity, {}, event_id); }); });
    NotifySchedule(entity, event_id, effective_delay);
    return event_id;
}

auto TimeEventManager::ResolveTimeEventFunc(ptr<const Entity> entity, hstring func_name, size_t alternative) const -> optional<Entity::TimeEventData::FuncType>
{
    if (entity->IsGlobal() != (alternative < 4)) {
        return std::nullopt;
    }

    ptr<BaseEngine> engine = _engine;

    auto wrap = [](auto func) -> optional<Entity::TimeEventData::FuncType> {
        if (!func) {
            return std::nullopt;
        }

        return Entity::TimeEventData::FuncType {std::move(func)};
    };

    if (alternative >= 4 && alternative <= 7) {
        array<ComplexTypeDesc, 2> args {{ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = engine->GetBaseType(entity->GetTypeName().as_str())}, {}}};
        size_t arg_count = 1;

        if (alternative == 5) {
            args[1] = ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = engine->GetBaseType("any")};
            arg_count = 2;
        }
        else if (alternative == 6) {
            args[1] = ComplexTypeDesc {.Kind = ComplexTypeKind::Array, .BaseType = engine->GetBaseType("any")};
            arg_count = 2;
        }
        else if (alternative == 7) {
            args[1] = ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = engine->GetBaseType("TimeEventContext")};
            arg_count = 2;
        }

        nptr<ScriptFuncDesc> func = engine->FindFunc(func_name, const_span<ComplexTypeDesc> {args.data(), arg_count});

        if (!func) {
            return std::nullopt;
        }

        switch (alternative) {
        case 4:
            return wrap(ScriptFunc<void, ptr<ScriptSelfEntity>> {func});
        case 5:
            return wrap(ScriptFunc<void, ptr<ScriptSelfEntity>, any_t> {func});
        case 6:
            return wrap(ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>> {func});
        case 7:
            return wrap(ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>> {func});
        default:
            return std::nullopt;
        }
    }

    switch (alternative) {
    case 0:
        return wrap(engine->FindFunc<void>(func_name));
    case 1:
        return wrap(engine->FindFunc<void, any_t>(func_name));
    case 2:
        return wrap(engine->FindFunc<void, vector<any_t>>(func_name));
    case 3:
        return wrap(engine->FindFunc<void, ptr<TimeEventContext>>(func_name));
    default:
        return std::nullopt;
    }
}

auto TimeEventManager::TimeEventDeadline(nanotime now, timespan delay) -> nanotime
{
    int64_t value = now.nanoseconds();
    int64_t offset = delay.nanoseconds();

    if ((offset > 0 && value > std::numeric_limits<int64_t>::max() - offset) || (offset < 0 && value < std::numeric_limits<int64_t>::min() - offset)) {
        throw TimeEventException("Time event deadline is outside the clock range", now, delay);
    }

    return now + delay;
}

FO_END_NAMESPACE
