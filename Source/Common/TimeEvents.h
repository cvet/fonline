// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "AnyData.h"
#include "Entity.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(TimeEventException);

class BaseEngine;

// Transient callback context for inspecting and changing the time event that is currently firing; requested changes are applied after script execution completes
///@ ExportRefType Common RefCounted Export = GetId, GetData, GetDataArray, HasData, IsStopped, GetRepeat, Stop, Repeat, SetData, SetDataArray
class TimeEventContext final : public refcounted<TimeEventContext>
{
public:
    explicit TimeEventContext(uint32_t id, timespan repeat, vector<any_t> data);
    TimeEventContext(const TimeEventContext&) = delete;
    TimeEventContext(TimeEventContext&&) noexcept = delete;
    auto operator=(const TimeEventContext&) = delete;
    auto operator=(TimeEventContext&&) noexcept = delete;
    ~TimeEventContext() = default;

    // Returns the unique identifier of the event currently invoking the callback
    [[nodiscard]] auto GetId() const noexcept -> uint32_t { return _id; }
    // Returns the first payload value, or an empty any value when the event has no payload
    [[nodiscard]] auto GetData() const noexcept -> any_t { return !_data.empty() ? _data.front() : any_t {}; }
    // Returns a copy of the complete payload array visible to this callback
    [[nodiscard]] auto GetDataArray() const noexcept -> vector<any_t> { return _data; }
    // Returns whether the callback context currently contains at least one payload value
    [[nodiscard]] auto HasData() const noexcept -> bool { return !_data.empty(); }
    // Returns whether Stop has been requested through this callback context
    [[nodiscard]] auto IsStopped() const noexcept -> bool { return _stopped; }
    // Returns the current repeat interval, including a value assigned by Repeat during this callback
    [[nodiscard]] auto GetRepeat() const noexcept -> timespan { return _repeat; }
    [[nodiscard]] auto IsRepeatChanged() const noexcept -> bool { return _repeatChanged; }
    [[nodiscard]] auto IsDataChanged() const noexcept -> bool { return _dataChanged; }
    [[nodiscard]] auto GetRawData() const noexcept -> const_span<any_t> { return _data; }

    // Marks the current event for removal after script execution completes
    void Stop() noexcept;
    // Replaces the repeat interval and schedules the next firing from callback completion, clamped to the Engine minimum interval
    void Repeat(timespan repeat) noexcept;
    // Replaces the event payload with one value after script execution completes
    void SetData(any_t data);
    // Replaces the complete event payload with a copy of the supplied array after script execution completes
    void SetDataArray(readonly_vector<any_t> data);

private:
    uint32_t _id {};
    timespan _repeat {};
    vector<any_t> _data {};
    bool _stopped {};
    bool _repeatChanged {};
    bool _dataChanged {};
};

// Accessed under the owning manager's mutex; a non-null execution pauses the registered timer
struct TimeEventExecution
{
    refcount_nptr<TimeEventContext> Context {};
    nanotime CompletionTime {};
    bool CallResult {};
    bool Completed {};
    bool NativeCallActive {};
};

class TimeEventManager
{
public:
    static const timespan MIN_REPEAT_TIME;

    struct Diagnostics
    {
        size_t EntityCount {};
        size_t EventCount {};
    };

    struct SerializableTimeEvent
    {
        string EntityTypeName {};
        ident_t EntityId {}; // Zero identifies the global engine entity
        uint32_t EventId {};
        string FuncName {};
        size_t FuncAlternative {};
        timespan FireDelay {};
        timespan RepeatDuration {};
        vector<any_t> Data {};
    };

    struct TimeEventCapture
    {
        vector<SerializableTimeEvent> Events {};
        size_t DelegateBoundCount {}; // Delegate object addresses cannot cross a process boundary
        size_t UnresolvedFunctionCount {};
        size_t ExecutingCount {};
        uint32_t LastEventId {};
    };

    struct ReadyTimeEvent
    {
        refcount_ptr<Entity> OwnerEntity;
        shared_ptr<Entity::TimeEventData> Event {};
    };

    struct DispatcherHooks
    {
        function<void(refcount_ptr<Entity> entity, uint32_t event_id, timespan delay)> Schedule {};
        function<void(uint32_t event_id)> Cancel {};
    };

    // The embedding server owns the database record that carries the persistent events of a stored entity
    struct StorageHooks
    {
        function<bool(ptr<const Entity> entity)> IsStored {};
        function<void(ptr<Entity> entity, AnyData::Array events)> Store {};
    };

    // Reserved key of the entity's own database document; the property loader skips keys that start with an underscore
    static constexpr string_view STORED_KEY {"_TimeEvents"};

    explicit TimeEventManager(ptr<BaseEngine> engine);
    TimeEventManager(const TimeEventManager&) = delete;
    TimeEventManager(TimeEventManager&&) noexcept = delete;
    auto operator=(const TimeEventManager&) = delete;
    auto operator=(TimeEventManager&&) noexcept = delete;
    ~TimeEventManager() = default;

    [[nodiscard]] auto CountTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id, bool exclude_executing = false) const -> size_t;
    [[nodiscard]] auto GetDiagnostics() const -> Diagnostics;
    [[nodiscard]] static auto GetTimeEventFuncAlternativeCount() noexcept -> size_t;

    void SetDispatcherHooks(DispatcherHooks hooks);
    void PauseDispatcherHooks();
    void ClearDispatcherHooks();
    void SetStorageHooks(StorageHooks hooks);
    void ClearStorageHooks();

    auto StartTimeEvent(ptr<Entity> entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t;
    // Starts the event unless the entity already has one for the same callback, running or waiting; returns whether it started
    auto EnsureTimeEvent(ptr<Entity> entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> bool;
    // Starts an event that is saved with the entity's database record and restored when the entity is loaded again
    auto StartPersistentTimeEvent(ptr<Entity> entity, Entity::TimeEventData::FuncType func, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t;
    auto RestartTimeEvent(ptr<Entity> entity, hstring func_name, size_t func_alternative, uint32_t event_id, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t;
    // Restarts the persistent events recorded in the entity's document before the entity's init events; returns their count
    auto RestoreStoredTimeEvents(ptr<Entity> entity, const AnyData::Document& doc) -> size_t;
    void ReserveTimeEventIds(uint32_t last_event_id);
    void ModifyTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id, optional<timespan> repeat, optional<vector<any_t>> data);
    void StopTimeEvent(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id);
    void ProcessTimeEvents();
    void ClearTimeEvents();
    void CancelAllForEntity(ptr<Entity> entity) noexcept;
    auto CollectReadyTimeEvents(optional<timespan>& time_until_next) -> vector<ReadyTimeEvent>;
    auto FireAndAdvance(ptr<Entity> entity, uint32_t event_id) -> optional<timespan>;
    auto CaptureSerializableTimeEvents() const -> TimeEventCapture;
    // Records of the entity's persistent events; the deadline is absolute synchronized time, so game time that passes while the server is down does not count
    auto CaptureStoredTimeEvents(ptr<const Entity> entity) const -> AnyData::Array;
    // Turns the entity's persistent events into ordinary ones when its database record goes away
    void ReleaseStoredTimeEvents(ptr<Entity> entity) noexcept;

private:
    void FireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te);
    void PostFireTimeEvent(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te);
    auto StartTimeEventWithId(ptr<Entity> entity, Entity::TimeEventData::FuncType func, optional<uint32_t> restored_id, bool persistent, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t;
    auto RestoreTimeEvent(ptr<Entity> entity, hstring func_name, size_t func_alternative, optional<uint32_t> restored_id, bool persistent, timespan delay, timespan repeat, vector<any_t> data) -> uint32_t;
    void StopTimeEventInternal(ptr<Entity> entity, ScriptFuncName func_name, uint32_t id, bool store_changes);
    auto PostFireTimeEventState(ptr<Entity> entity, shared_ptr<Entity::TimeEventData> te) -> bool;
    void StoreTimeEvents(ptr<Entity> entity);
    void AddEntityTimeEventPolling(ptr<Entity> entity);
    void RemoveEntityTimeEventPolling(ptr<Entity> entity);
    void ProcessEntityTimeEvents(ptr<Entity> entity);
    void NotifySchedule(ptr<Entity> entity, uint32_t event_id, timespan delay);
    void NotifyCancel(uint32_t event_id);
    auto ResolveTimeEventFunc(ptr<const Entity> entity, hstring func_name, size_t alternative) const -> optional<Entity::TimeEventData::FuncType>;
    static auto TimeEventDeadline(nanotime now, timespan delay) -> nanotime;

    ptr<BaseEngine> _engine;
    mutable std::recursive_mutex _timeEventLocker {}; // Recursive: not modelable by TSA
    unordered_set<refcount_ptr<Entity>> _timeEventEntities {};
    std::atomic_uint32_t _timeEventCounter {};
    DispatcherHooks _dispatcher {};
    StorageHooks _storage {};
    std::atomic_bool _dispatcherPaused {};
    any_t _emptyAnyValue {};
};

FO_END_NAMESPACE
