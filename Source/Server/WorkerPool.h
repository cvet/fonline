// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "EntitySync.h"

FO_BEGIN_NAMESPACE

enum class WorkerJobType : size_t
{
    None = 0,
    Player = 1,
    NotLoggedInPlayer = 2,
    CritterMovement = 3,
    TimeEvent = 4,
};

struct WorkerJobKey
{
    WorkerJobType Type {};
    size_t Id {};
};
inline auto operator==(const WorkerJobKey& lhs, const WorkerJobKey& rhs) noexcept -> bool
{
    return lhs.Type == rhs.Type && lhs.Id == rhs.Id;
}
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE WorkerJobKey);

class WorkerPool final
{
public:
    using JobKey = WorkerJobKey;
    using Job = function<optional<timespan>()>;

    struct Diagnostics
    {
        int32_t ThreadCount {};
        size_t ScheduledJobs {};
        size_t AnonymousScheduledJobs {};
        size_t QueuedKeys {};
        size_t RunningJobs {};
        size_t PendingReruns {};
        int32_t ActiveWorkers {};
        bool Paused {};
        bool SchedulingTimeFrozen {};
        bool JobTypesHeld {};
        size_t HeldJobs {};
        uint64_t CompletedJobs {};
    };

    static constexpr JobKey ANONYMOUS_JOB {};

    explicit WorkerPool(string_view name, int32_t thread_count, ptr<const std::atomic<bool>> shutdown_flag, bool start_paused = false);
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&) noexcept = delete;
    auto operator=(const WorkerPool&) = delete;
    auto operator=(WorkerPool&&) noexcept = delete;
    ~WorkerPool();

    [[nodiscard]] auto GetThreadCount() const noexcept -> int32_t { return numeric_cast<int32_t>(_workers.size()); }
    [[nodiscard]] auto GetPendingJobCount() const -> size_t;
    [[nodiscard]] auto GetDiagnostics() const -> Diagnostics;
    [[nodiscard]] auto IsKeyActive(JobKey key) const -> bool;
    [[nodiscard]] auto IsJobTypeHeld(WorkerJobType type) const -> bool;

    void Resume();
    void Pause();
    void FreezeSchedulingTime();
    void ResumeSchedulingTime();
    void HoldJobTypes(const_span<WorkerJobType> types);
    void ReleaseHeldJobTypes();
    void Submit(Job job);
    void Submit(timespan delay, Job job);
    void Submit(JobKey key, Job job);
    void Submit(JobKey key, timespan delay, Job job);
    auto Wake(JobKey key) -> bool;
    auto Cancel(JobKey key) -> bool;
    void Clear();
    void WaitIdle() const;
    auto WaitIdle(timespan timeout) const -> bool;

private:
    struct ScheduledJob
    {
        nanotime FireTime {};
        JobKey Key {};
        Job Body {};
    };

    bool IsAnyJobReadyNow() const noexcept FO_TSA_REQUIRES(_mutex);
    bool IsBarrierIdle() const noexcept FO_TSA_REQUIRES(_mutex);
    nanotime GetSchedulingTime() const noexcept FO_TSA_REQUIRES(_mutex);
    bool IsJobHeld(JobKey key) const noexcept FO_TSA_REQUIRES(_mutex);
    size_t FindNextRunnableJob() const noexcept FO_TSA_REQUIRES(_mutex);
    nanotime GetJobSchedulingTime(JobKey key) const noexcept FO_TSA_REQUIRES(_mutex);

    void EnqueueJob(nanotime fire_time, JobKey key, Job job) noexcept FO_TSA_REQUIRES(_mutex);
    void WorkerEntry(int32_t worker_index) noexcept;
    void StopWorkers() noexcept;

    string _name {};
    ptr<const std::atomic<bool>> _shutdownFlag;
    vector<thread> _workers {};
    mutable mutex _mutex {};
    vector<ScheduledJob> _jobs FO_TSA_GUARDED_BY(_mutex) {}; // Sorted ascending by FireTime
    unordered_set<JobKey> _queuedKeys FO_TSA_GUARDED_BY(_mutex) {};
    unordered_set<JobKey> _runningKeys FO_TSA_GUARDED_BY(_mutex) {};
    unordered_map<JobKey, ScheduledJob> _pendingRerun FO_TSA_GUARDED_BY(_mutex) {};
    unordered_set<JobKey> _cancelOnFinish FO_TSA_GUARDED_BY(_mutex) {};
    unordered_set<JobKey> _wakeRequests FO_TSA_GUARDED_BY(_mutex) {};
    uint64_t _completedJobs FO_TSA_GUARDED_BY(_mutex) {};
    std::condition_variable_any _workSignal {};
    mutable std::condition_variable_any _idleSignal {};
    int32_t _activeWorkers FO_TSA_GUARDED_BY(_mutex) {};
    bool _finish FO_TSA_GUARDED_BY(_mutex) {};
    bool _paused FO_TSA_GUARDED_BY(_mutex) {};
    size_t _anonymousScheduledJobs FO_TSA_GUARDED_BY(_mutex) {};
    timespan _schedulingTimeOffset FO_TSA_GUARDED_BY(_mutex) {};
    nanotime _schedulingTimeFrozenAt FO_TSA_GUARDED_BY(_mutex) {};
    bool _schedulingTimeFrozen FO_TSA_GUARDED_BY(_mutex) {};
    array<bool, static_cast<size_t>(WorkerJobType::TimeEvent) + 1> _heldJobTypes FO_TSA_GUARDED_BY(_mutex) {};
    bool _jobTypesHeld FO_TSA_GUARDED_BY(_mutex) {};
    nanotime _jobTypeHoldStartedAt FO_TSA_GUARDED_BY(_mutex) {};
};

FO_END_NAMESPACE
