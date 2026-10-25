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

#include "WorkerPool.h"

#include "WorkThread.h"

FO_BEGIN_NAMESPACE

static auto WorkerJobDeadline(nanotime now, timespan delay) -> nanotime;

WorkerPool::WorkerPool(string_view name, int32_t thread_count, ptr<const std::atomic<bool>> shutdown_flag, bool start_paused) :
    _name {name},
    _shutdownFlag {shutdown_flag},
    _paused {start_paused}
{
    FO_TRACE_ZONE(Threading);

    if (thread_count <= 0) {
        thread_count = std::max(1, numeric_cast<int32_t>(std::thread::hardware_concurrency()) - 1);
    }

    _workers.reserve(numeric_cast<size_t>(thread_count));

    try {
        for (int32_t i = 0; i < thread_count; i++) {
            _workers.emplace_back(run_thread(strex("{}-{}", _name, i), [this, i] { WorkerEntry(i); }));
        }
    }
    catch (...) {
        // Workers already started reference *this through their lambda, and an unwinding constructor never runs
        // ~WorkerPool, so they are stopped and joined here before the storage goes away
        StopWorkers();
        throw;
    }
}

WorkerPool::~WorkerPool()
{
    StopWorkers();
}

void WorkerPool::StopWorkers() noexcept
{
    FO_TRACE_ZONE(Threading);

    {
        scoped_lock locker {_mutex};

        _finish = true;
    }

    _workSignal.notify_all();

    for (auto& worker : _workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void WorkerPool::Submit(Job job)
{
    Submit(timespan::zero, std::move(job));
}

void WorkerPool::Submit(timespan delay, Job job)
{
    {
        scoped_lock locker {_mutex};

        if (_finish) {
            throw EntitySyncException("Cannot submit job to a stopped WorkerPool");
        }

        nanotime scheduling_time = GetSchedulingTime();
        auto fire_time = WorkerJobDeadline(std::move(scheduling_time), delay);
        EnqueueJob(std::move(fire_time), ANONYMOUS_JOB, std::move(job));
    }

    _workSignal.notify_one();
}

void WorkerPool::Submit(JobKey key, Job job)
{
    Submit(key, timespan::zero, std::move(job));
}

void WorkerPool::Submit(JobKey key, timespan delay, Job job)
{
    if (key == ANONYMOUS_JOB) {
        Submit(delay, std::move(job));
        return;
    }

    bool needs_notify = false;

    {
        scoped_lock locker {_mutex};

        if (_finish) {
            throw EntitySyncException("Cannot submit job to a stopped WorkerPool");
        }

        if (_runningKeys.contains(key)) {
            nanotime scheduling_time = GetJobSchedulingTime(key);
            auto fire_time = WorkerJobDeadline(std::move(scheduling_time), delay);
            _pendingRerun[key] = ScheduledJob {std::move(fire_time), key, std::move(job)};
            _cancelOnFinish.erase(key);
        }
        else if (!_queuedKeys.contains(key)) {
            nanotime scheduling_time = GetJobSchedulingTime(key);
            auto fire_time = WorkerJobDeadline(std::move(scheduling_time), delay);
            EnqueueJob(std::move(fire_time), key, std::move(job));
            _queuedKeys.insert(key);
            needs_notify = true;
        }
    }

    if (needs_notify) {
        _workSignal.notify_one();
    }
}

auto WorkerPool::Wake(JobKey key) -> bool
{
    if (key == ANONYMOUS_JOB) {
        return false;
    }

    bool wake_signal = false;
    bool result = false;

    {
        scoped_lock locker {_mutex};

        if (_queuedKeys.contains(key)) {
            // Pull the queued entry out, set its FireTime to now, and re-insert sorted so workers
            // that are wait_until-ing on the previous front fire time get woken to pick it up
            for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                if (it->Key == key) {
                    auto entry = std::move(*it);

                    if (entry.Key == ANONYMOUS_JOB) {
                        _anonymousScheduledJobs--;
                    }

                    _jobs.erase(it);
                    entry.FireTime = GetJobSchedulingTime(entry.Key);
                    EnqueueJob(entry.FireTime, entry.Key, std::move(entry.Body));
                    break;
                }
            }

            wake_signal = true;
            result = true;
        }
        else if (_runningKeys.contains(key)) {
            // The body is in flight; arm a wake-on-finish so its self-reschedule (return value)
            // is overridden to fire immediately when the worker finalizes
            _wakeRequests.insert(key);
            result = true;
        }
    }

    if (wake_signal) {
        _workSignal.notify_all();
    }

    return result;
}

auto WorkerPool::Cancel(JobKey key) -> bool
{
    if (key == ANONYMOUS_JOB) {
        return false;
    }

    bool removed = false;
    bool wake = false;

    {
        scoped_lock locker {_mutex};

        if (_queuedKeys.erase(key) != 0) {
            for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                if (it->Key == key) {
                    if (it->Key == ANONYMOUS_JOB) {
                        _anonymousScheduledJobs--;
                    }

                    _jobs.erase(it);
                    break;
                }
            }
            removed = true;
            wake = true;
        }

        if (_pendingRerun.erase(key) != 0) {
            removed = true;
        }

        if (_runningKeys.contains(key)) {
            // Drop the in-flight run's self-reschedule when it finishes
            _cancelOnFinish.insert(key);
            removed = true;
        }

        // Cancel supersedes any pending wake
        _wakeRequests.erase(key);
    }

    if (wake) {
        _workSignal.notify_all();
    }

    return removed;
}

void WorkerPool::Clear()
{
    scoped_lock locker {_mutex};

    _jobs.clear();
    _anonymousScheduledJobs = 0;
    _queuedKeys.clear();
    _pendingRerun.clear();
    _wakeRequests.clear();

    // Any in-flight run should drop its self-reschedule. We can't know its key from here, so mark
    // every currently-running key
    for (const auto& key : _runningKeys) {
        _cancelOnFinish.insert(key);
    }
}

void WorkerPool::WaitIdle() const
{
    FO_TRACE_ZONE(Threading);

    unique_lock locker {_mutex};

    while (!IsBarrierIdle()) {
        _idleSignal.wait(locker);
    }
}

auto WorkerPool::WaitIdle(timespan timeout) const -> bool
{
    FO_TRACE_ZONE(Threading);

    unique_lock locker {_mutex};

    nanotime deadline = nanotime::now() + timeout;

    while (!IsBarrierIdle()) {
        if (_idleSignal.wait_until(locker, deadline.value()) == std::cv_status::timeout) {
            return IsBarrierIdle();
        }
    }

    return true;
}

void WorkerPool::Resume()
{
    {
        scoped_lock locker {_mutex};

        if (!_paused) {
            return;
        }

        FO_VERIFY_AND_THROW(!_schedulingTimeFrozen, "Cannot resume WorkerPool while scheduling time is frozen");

        _paused = false;
    }

    _workSignal.notify_all();
}

void WorkerPool::Pause()
{
    FO_TRACE_ZONE(Threading);

    unique_lock locker {_mutex};

    _paused = true;

    while (_activeWorkers != 0) {
        _idleSignal.wait(locker);
    }
}

void WorkerPool::FreezeSchedulingTime()
{
    scoped_lock locker {_mutex};

    FO_VERIFY_AND_THROW(_paused && _activeWorkers == 0, "WorkerPool scheduling time can only freeze at a drained pause boundary", _paused, _activeWorkers);
    FO_VERIFY_AND_THROW(!_schedulingTimeFrozen, "WorkerPool scheduling time is already frozen");

    _schedulingTimeFrozenAt = GetSchedulingTime();
    _schedulingTimeFrozen = true;
}

void WorkerPool::ResumeSchedulingTime()
{
    scoped_lock locker {_mutex};

    FO_VERIFY_AND_THROW(_paused && _activeWorkers == 0, "WorkerPool scheduling time can only resume at a drained pause boundary", _paused, _activeWorkers);
    FO_VERIFY_AND_THROW(_schedulingTimeFrozen, "WorkerPool scheduling time is not frozen");

    nanotime current_scheduling_time = nanotime::now() - _schedulingTimeOffset;
    _schedulingTimeOffset += current_scheduling_time - _schedulingTimeFrozenAt;
    _schedulingTimeFrozenAt = {};
    _schedulingTimeFrozen = false;
}

void WorkerPool::HoldJobTypes(const_span<WorkerJobType> types)
{
    FO_TRACE_ZONE(Threading);

    scoped_lock locker {_mutex};

    FO_VERIFY_AND_THROW(!_jobTypesHeld, "WorkerPool job types are already held");

    decltype(_heldJobTypes) held_types {};
    bool any = false;

    for (WorkerJobType type : types) {
        // Anonymous engine jobs carry the None type and must keep running: they are what keeps the
        // pool's own accounting and diagnostics alive while gameplay is held
        if (type == WorkerJobType::None) {
            continue;
        }

        size_t index = static_cast<size_t>(type);
        FO_VERIFY_AND_THROW(index < held_types.size(), "Unknown worker job type", index);
        held_types[index] = true;
        any = true;
    }

    FO_VERIFY_AND_THROW(any, "WorkerPool job type hold requested without any holdable type");

    _heldJobTypes = held_types;
    _jobTypeHoldStartedAt = GetSchedulingTime();
    _jobTypesHeld = true;
    _workSignal.notify_all();
    _idleSignal.notify_all();
}

void WorkerPool::ReleaseHeldJobTypes()
{
    FO_TRACE_ZONE(Threading);

    scoped_lock locker {_mutex};

    FO_VERIFY_AND_THROW(_jobTypesHeld, "WorkerPool job types are not held");

    // Give every held job back the time it spent waiting, so a long hold does not turn into a burst
    // of overdue work the moment it ends
    timespan held_for = GetSchedulingTime() - _jobTypeHoldStartedAt;

    struct ResumedJob
    {
        size_t Index {};
        nanotime FireTime {};
    };

    // Sort local records first. Publication only moves jobs and swaps Engine-allocated storage;
    // original queue positions break equal-deadline ties
    static_assert(std::is_nothrow_move_constructible_v<ScheduledJob>);
    vector<ResumedJob> order;
    order.reserve(_jobs.size());

    for (size_t i = 0; i < _jobs.size(); i++) {
        const auto& job = _jobs[i];
        timespan delay = IsJobHeld(job.Key) ? held_for : timespan::zero;
        nanotime fire_time = WorkerJobDeadline(job.FireTime, std::move(delay));
        order.push_back({i, std::move(fire_time)});
    }

    for (const auto& [key, job] : _pendingRerun) {
        if (IsJobHeld(key)) {
            (void)WorkerJobDeadline(job.FireTime, held_for);
        }
    }

    std::sort(order.begin(), order.end(), [](const ResumedJob& lhs, const ResumedJob& rhs) { return lhs.FireTime != rhs.FireTime ? lhs.FireTime < rhs.FireTime : lhs.Index < rhs.Index; });

    vector<ScheduledJob> jobs;
    jobs.reserve(_jobs.size());

    for (const auto& entry : order) {
        auto job = std::move(_jobs[entry.Index]);
        job.FireTime = entry.FireTime;
        jobs.emplace_back(std::move(job));
    }

    _jobs.swap(jobs);

    for (auto& [key, job] : _pendingRerun) {
        if (IsJobHeld(key)) {
            job.FireTime += held_for;
        }
    }

    _heldJobTypes.fill(false);
    _jobTypesHeld = false;
    _jobTypeHoldStartedAt = {};

    _workSignal.notify_all();
}

auto WorkerPool::IsJobTypeHeld(WorkerJobType type) const -> bool
{
    scoped_lock locker {_mutex};

    size_t index = static_cast<size_t>(type);

    return index < _heldJobTypes.size() && _heldJobTypes[index];
}

auto WorkerPool::GetPendingJobCount() const -> size_t
{
    scoped_lock locker {_mutex};

    return _jobs.size();
}

auto WorkerPool::GetDiagnostics() const -> Diagnostics
{
    scoped_lock locker {_mutex};

    size_t held_jobs = 0;

    for (const auto& job : _jobs) {
        if (IsJobHeld(job.Key)) {
            held_jobs++;
        }
    }

    return Diagnostics {
        .ThreadCount = numeric_cast<int32_t>(_workers.size()),
        .ScheduledJobs = _jobs.size(),
        .AnonymousScheduledJobs = _anonymousScheduledJobs,
        .QueuedKeys = _queuedKeys.size(),
        .RunningJobs = _runningKeys.size(),
        .PendingReruns = _pendingRerun.size(),
        .ActiveWorkers = _activeWorkers,
        .Paused = _paused,
        .SchedulingTimeFrozen = _schedulingTimeFrozen,
        .JobTypesHeld = _jobTypesHeld,
        .HeldJobs = held_jobs,
        .CompletedJobs = _completedJobs,
    };
}

auto WorkerPool::IsKeyActive(JobKey key) const -> bool
{
    if (key == ANONYMOUS_JOB) {
        return false;
    }

    scoped_lock locker {_mutex};

    return _queuedKeys.contains(key) || _runningKeys.contains(key) || _pendingRerun.contains(key);
}

void WorkerPool::EnqueueJob(nanotime fire_time, JobKey key, Job job) noexcept
{
    if (key == ANONYMOUS_JOB) {
        _anonymousScheduledJobs++;
    }

    ScheduledJob entry {fire_time, key, std::move(job)};

    if (_jobs.empty() || fire_time >= _jobs.back().FireTime) {
        _jobs.emplace_back(std::move(entry));
        return;
    }

    for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
        if (fire_time < it->FireTime) {
            _jobs.emplace(it, std::move(entry));
            return;
        }
    }

    _jobs.emplace_back(std::move(entry));
}

auto WorkerPool::IsAnyJobReadyNow() const noexcept -> bool
{
    size_t index = FindNextRunnableJob();
    return index < _jobs.size() && _jobs[index].FireTime <= GetSchedulingTime();
}

auto WorkerPool::IsBarrierIdle() const noexcept -> bool
{
    return !IsAnyJobReadyNow() && _activeWorkers == 0 && _pendingRerun.empty();
}

auto WorkerPool::GetSchedulingTime() const noexcept -> nanotime
{
    return _schedulingTimeFrozen ? _schedulingTimeFrozenAt : nanotime::now() - _schedulingTimeOffset;
}

auto WorkerPool::IsJobHeld(JobKey key) const noexcept -> bool
{
    size_t index = static_cast<size_t>(key.Type);
    return _jobTypesHeld && index < _heldJobTypes.size() && _heldJobTypes[index];
}

auto WorkerPool::FindNextRunnableJob() const noexcept -> size_t
{
    size_t index = 0;

    while (index < _jobs.size() && IsJobHeld(_jobs[index].Key)) {
        index++;
    }

    return index;
}

auto WorkerPool::GetJobSchedulingTime(JobKey key) const noexcept -> nanotime
{
    // A job submitted or rearmed during a hold starts its delay on the frozen type clock.
    // Release shifts every held deadline by the same amount, including pending reruns
    return IsJobHeld(key) ? _jobTypeHoldStartedAt : GetSchedulingTime();
}

void WorkerPool::WorkerEntry(int32_t worker_index) noexcept
{
    set_this_thread_name(strex("{}-{}", _name, worker_index));

    while (true) {
        ScheduledJob job;
        bool got_job = false;

        {
            unique_lock locker {_mutex};

            while (!_finish) {
                if (_paused) {
                    _workSignal.wait(locker);
                    continue;
                }

                size_t runnable_index = FindNextRunnableJob();

                if (runnable_index == _jobs.size()) {
                    _workSignal.wait(locker);
                    continue;
                }

                nanotime front_fire = _jobs[runnable_index].FireTime;
                nanotime now = GetSchedulingTime();

                if (front_fire > now) {
                    // Wait until the earliest job becomes due, or until something nearer arrives
                    _workSignal.wait_for(locker, (front_fire - now).value());
                    continue;
                }

                job = std::move(_jobs[runnable_index]);
                _jobs.erase(_jobs.begin() + numeric_cast<ptrdiff_t>(runnable_index));

                if (job.Key == ANONYMOUS_JOB) {
                    _anonymousScheduledJobs--;
                }

                if (job.Key != ANONYMOUS_JOB) {
                    _queuedKeys.erase(job.Key);
                    _runningKeys.insert(job.Key);
                }

                _activeWorkers++;
                got_job = true;
                break;
            }

            if (_finish && !got_job) {
                return;
            }
        }

        optional<timespan> next_delay;
        bool job_executed = false;

        {
            ScopedSyncContext sync_ctx;

            bool shutdown = _shutdownFlag->load(std::memory_order_acquire);

            if (!shutdown) {
                job_executed = true;

                try {
                    next_delay = job.Body();
                }
                catch (const std::exception& ex) {
                    if (!_shutdownFlag->load(std::memory_order_acquire)) {
                        exceptions::report_and_continue(ex);
                    }
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }
        }

        bool need_wake = false;
        bool body_rescheduled = false;
        std::exception_ptr scheduling_error;

        {
            scoped_lock locker {_mutex};

            if (job.Key != ANONYMOUS_JOB) {
                _runningKeys.erase(job.Key);

                bool cancelled = _cancelOnFinish.erase(job.Key) != 0;
                bool wake_requested = _wakeRequests.erase(job.Key) != 0;

                if (auto it = _pendingRerun.find(job.Key); it != _pendingRerun.end()) {
                    auto entry = std::move(it->second);
                    _pendingRerun.erase(it);

                    if (wake_requested) {
                        entry.FireTime = GetJobSchedulingTime(entry.Key);
                    }

                    EnqueueJob(entry.FireTime, entry.Key, std::move(entry.Body));
                    _queuedKeys.insert(job.Key);
                    need_wake = true;
                }
                else if (next_delay.has_value() && !cancelled) {
                    timespan reschedule_delay = wake_requested ? timespan::zero : next_delay.value();
                    try {
                        nanotime scheduling_time = GetJobSchedulingTime(job.Key);
                        auto fire_time = WorkerJobDeadline(std::move(scheduling_time), reschedule_delay);
                        EnqueueJob(std::move(fire_time), job.Key, std::move(job.Body));
                        body_rescheduled = true;
                        _queuedKeys.insert(job.Key);
                        need_wake = true;
                    }
                    catch (const EntitySyncException&) {
                        scheduling_error = std::current_exception();
                    }
                }
            }
            else if (next_delay.has_value()) {
                try {
                    nanotime scheduling_time = GetSchedulingTime();
                    auto fire_time = WorkerJobDeadline(std::move(scheduling_time), next_delay.value());
                    EnqueueJob(std::move(fire_time), ANONYMOUS_JOB, std::move(job.Body));
                    body_rescheduled = true;
                    need_wake = true;
                }
                catch (const EntitySyncException&) {
                    scheduling_error = std::current_exception();
                }
            }

            if (job_executed) {
                _completedJobs++;
            }

            if (!scheduling_error) {
                _activeWorkers--;
            }
        }

        // Report after releasing the scheduler mutex: the exception observer can submit service work
        if (scheduling_error && !_shutdownFlag->load(std::memory_order_acquire)) {
            ScopedSyncContext sync_ctx;

            try {
                std::rethrow_exception(scheduling_error);
            }
            catch (const std::exception& ex) {
                exceptions::report_and_continue(ex);
            }
        }

        // A non-rescheduled body may own the last entity reference after its execution context closes.
        // Destroy the closure under an empty sync context so entity validation remains legal
        if (!body_rescheduled) {
            ScopedSyncContext sync_ctx;

            job.Body = {};
        }

        if (scheduling_error) {
            scoped_lock locker {_mutex};
            _activeWorkers--;
        }

        if (need_wake) {
            _workSignal.notify_one();
        }

        _idleSignal.notify_all();
    }
}

static auto WorkerJobDeadline(nanotime now, timespan delay) -> nanotime
{
    int64_t value = now.nanoseconds();
    int64_t offset = delay.nanoseconds();

    if ((offset > 0 && value > std::numeric_limits<int64_t>::max() - offset) || (offset < 0 && value < std::numeric_limits<int64_t>::min() - offset)) {
        throw EntitySyncException("Worker job deadline is outside the clock range", now, delay);
    }

    return now + delay;
}

FO_END_NAMESPACE
