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

#pragma once

#include "Common.h"

#include "Entity.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(TimeEventException);

class BaseEngine;

// Transient callback context for inspecting and changing the time event that is currently firing; requested changes are applied after the callback returns.
///@ ExportRefType Common RefCounted Export = GetId, GetData, GetDataArray, HasData, IsStopped, GetRepeat, Stop, Repeat, SetData, SetDataArray
class TimeEventContext final : public RefCounted<TimeEventContext>
{
public:
    explicit TimeEventContext(uint32_t id, timespan repeat, vector<any_t> data);
    TimeEventContext(const TimeEventContext&) = delete;
    TimeEventContext(TimeEventContext&&) noexcept = delete;
    auto operator=(const TimeEventContext&) = delete;
    auto operator=(TimeEventContext&&) noexcept = delete;
    ~TimeEventContext() = default;

    // Returns the unique identifier of the event currently invoking the callback.
    [[nodiscard]] auto GetId() const noexcept -> uint32_t { return _id; }
    // Returns the first payload value, or an empty any value when the event has no payload.
    [[nodiscard]] auto GetData() const noexcept -> any_t { return !_data.empty() ? _data.front() : any_t {}; }
    // Returns a copy of the complete payload array visible to this callback.
    [[nodiscard]] auto GetDataArray() const noexcept -> vector<any_t> { return _data; }
    // Returns whether the callback context currently contains at least one payload value.
    [[nodiscard]] auto HasData() const noexcept -> bool { return !_data.empty(); }
    // Returns whether Stop has been requested through this callback context.
    [[nodiscard]] auto IsStopped() const noexcept -> bool { return _stopped; }
    // Returns the current repeat interval, including a value assigned by Repeat during this callback.
    [[nodiscard]] auto GetRepeat() const noexcept -> timespan { return _repeat; }
    [[nodiscard]] auto IsRepeatChanged() const noexcept -> bool { return _repeatChanged; }
    [[nodiscard]] auto IsDataChanged() const noexcept -> bool { return _dataChanged; }
    [[nodiscard]] auto GetRawData() const noexcept -> const_span<any_t> { return _data; }

    // Marks the current event for removal after the callback returns.
    void Stop() noexcept;
    // Replaces the repeat interval and schedules the next firing from callback completion, clamped to the Engine minimum interval.
    void Repeat(timespan repeat) noexcept;
    // Replaces the event payload with one value after the callback returns.
    void SetData(any_t data);
    // Replaces the complete event payload with a copy of the supplied array after the callback returns.
    void SetDataArray(readonly_vector<any_t> data);

private:
    uint32_t _id;
    timespan _repeat;
    vector<any_t> _data;
    bool _stopped {};
    bool _repeatChanged {};
    bool _dataChanged {};
};

class TimeEventManager
{
public:
    static const timespan MIN_REPEAT_TIME;

    struct ReadyTimeEvent
    {
        refcount_ptr<Entity> OwnerEntity;
        shared_ptr<Entity::TimeEventData> Event {};
    };

    struct FiredTimeEvent
    {
        bool CallResult {};
        refcount_nptr<TimeEventContext> Context {};
    };

    struct DispatcherHooks
    {
        function<void(refcount_ptr<Entity> entity, uint32_t event_id, timespan delay)> Schedule {};
        function<void(uint32_t event_id)> Cancel {};
    };

    explicit TimeEventManager(ptr<BaseEngine> engine);
    TimeEventManager(const TimeEventManager&) = delete;
    TimeEventManager(TimeEventManager&&) noexcept = delete;
    auto operator=(const TimeEventManager&) = delete;
    auto operator=(TimeEventManager&&) noexcept = delete;
    ~TimeEventManager() = default;

    [[nodiscard]] auto CountTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id) const -> size_t;

    void SetDispatcherHooks(DispatcherHooks hooks);
    void PauseDispatcherHooks();
    void ClearDispatcherHooks();

    auto StartTimeEvent(ptr<Entity> entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t;
    void ModifyTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id, optional<timespan> repeat, optional<vector<any_t>> data);
    void StopTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id);
    void ProcessTimeEvents();
    void ClearTimeEvents();
    void CancelAllForEntity(ptr<Entity> entity) noexcept;
    auto CollectReadyTimeEvents(optional<timespan>& time_until_next) -> vector<ReadyTimeEvent>;
    auto FireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te) -> FiredTimeEvent;
    void PostFireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te, const FiredTimeEvent& result);
    auto FireAndAdvance(ptr<Entity> entity, uint32_t event_id) -> optional<timespan>;

private:
    void AddEntityTimeEventPolling(ptr<Entity> entity);
    void RemoveEntityTimeEventPolling(ptr<Entity> entity);
    void ProcessEntityTimeEvents(ptr<Entity> entity);
    void NotifySchedule(ptr<Entity> entity, uint32_t event_id, timespan delay);
    void NotifyCancel(uint32_t event_id);

    ptr<BaseEngine> _engine;
    mutable std::recursive_mutex _timeEventLocker {}; // Recursive: not modelable by TSA
    unordered_set<refcount_ptr<Entity>> _timeEventEntities {};
    std::atomic_uint32_t _timeEventCounter {};
    DispatcherHooks _dispatcher {};
    std::atomic_bool _dispatcherPaused {};
    any_t _emptyAnyValue {};
};

FO_END_NAMESPACE
