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

#pragma once

#include "Common.h"

#include "EntitySync.h"

FO_BEGIN_NAMESPACE

enum class WorkerJobType : size_t
{
    None = 0,
    Player = 1,
    UnloginedPlayer = 2,
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
        size_t QueuedKeys {};
        size_t RunningJobs {};
        size_t PendingReruns {};
        int32_t ActiveWorkers {};
        bool Paused {};
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

    void Resume();
    void Pause();
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

    [[nodiscard]] bool IsAnyJobReadyNow() const noexcept FO_TSA_REQUIRES(_mutex);
    [[nodiscard]] bool IsBarrierIdle() const noexcept FO_TSA_REQUIRES(_mutex);

    void EnqueueJob(nanotime fire_time, JobKey key, Job job) FO_TSA_REQUIRES(_mutex);
    void WorkerEntry(int32_t worker_index) noexcept;
    void StopWorkers() noexcept;

    string _name;
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
};

FO_END_NAMESPACE
