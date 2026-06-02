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

#include "WorkerPool.h"

#include "WorkThread.h"

FO_BEGIN_NAMESPACE

WorkerPool::WorkerPool(string_view name, int32_t thread_count, const std::atomic<bool>& shutdown_flag, bool start_paused) :
    _name {name},
    _shutdownFlag {&shutdown_flag},
    _paused {start_paused}
{
    FO_STACK_TRACE_ENTRY();

    if (thread_count <= 0) {
        thread_count = std::max(1, numeric_cast<int32_t>(std::thread::hardware_concurrency()) - 1);
    }

    _workers.reserve(numeric_cast<size_t>(thread_count));

    for (int32_t i = 0; i < thread_count; i++) {
        _workers.emplace_back(run_thread(strex("{}-{}", _name, i), [this, i] { WorkerEntry(i); }));
    }
}

WorkerPool::~WorkerPool()
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_mutex};

        if (_finish) {
            throw EntitySyncException("Cannot submit job to a stopped WorkerPool");
        }

        EnqueueJob(nanotime::now() + delay, ANONYMOUS_JOB, std::move(job));
    }

    _workSignal.notify_one();
}

void WorkerPool::Submit(JobKey key, Job job)
{
    Submit(key, timespan::zero, std::move(job));
}

void WorkerPool::Submit(JobKey key, timespan delay, Job job)
{
    FO_STACK_TRACE_ENTRY();

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
            _pendingRerun[key] = std::move(job);
            _cancelOnFinish.erase(key);
        }
        else if (!_queuedKeys.contains(key)) {
            EnqueueJob(nanotime::now() + delay, key, std::move(job));
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
    FO_STACK_TRACE_ENTRY();

    if (key == ANONYMOUS_JOB) {
        return false;
    }

    bool wake_signal = false;
    bool result = false;

    {
        scoped_lock locker {_mutex};

        if (_queuedKeys.contains(key)) {
            // Pull the queued entry out, set its FireTime to now, and re-insert sorted so workers
            // that are wait_until-ing on the previous front fire time get woken to pick it up.
            for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                if (it->Key == key) {
                    auto entry = std::move(*it);
                    _jobs.erase(it);
                    entry.FireTime = nanotime::now();
                    EnqueueJob(entry.FireTime, entry.Key, std::move(entry.Body));
                    break;
                }
            }

            wake_signal = true;
            result = true;
        }
        else if (_runningKeys.contains(key)) {
            // The body is in flight; arm a wake-on-finish so its self-reschedule (return value)
            // is overridden to fire immediately when the worker finalizes.
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
    FO_STACK_TRACE_ENTRY();

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
            // Drop the in-flight run's self-reschedule when it finishes.
            _cancelOnFinish.insert(key);
            removed = true;
        }

        // Cancel supersedes any pending wake.
        _wakeRequests.erase(key);
    }

    if (wake) {
        _workSignal.notify_all();
    }

    return removed;
}

void WorkerPool::Clear()
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_mutex};

    _jobs.clear();
    _queuedKeys.clear();
    _pendingRerun.clear();
    _wakeRequests.clear();

    // Any in-flight run should drop its self-reschedule. We can't know its key from here, so mark
    // every currently-running key.
    for (const auto& key : _runningKeys) {
        _cancelOnFinish.insert(key);
    }
}

void WorkerPool::WaitIdle() const
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker {_mutex};

    while (!IsBarrierIdle()) {
        _idleSignal.wait(locker);
    }
}

void WorkerPool::Resume()
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_mutex};

        if (!_paused) {
            return;
        }

        _paused = false;
    }

    _workSignal.notify_all();
}

auto WorkerPool::GetPendingJobCount() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_mutex};

    return _jobs.size();
}

auto WorkerPool::IsKeyActive(JobKey key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (key == ANONYMOUS_JOB) {
        return false;
    }

    scoped_lock locker {_mutex};

    return _queuedKeys.contains(key) || _runningKeys.contains(key) || _pendingRerun.contains(key);
}

void WorkerPool::EnqueueJob(nanotime fire_time, JobKey key, Job job)
{
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
    return !_jobs.empty() && _jobs.front().FireTime <= nanotime::now();
}

auto WorkerPool::IsBarrierIdle() const noexcept -> bool
{
    return !IsAnyJobReadyNow() && _activeWorkers == 0 && _pendingRerun.empty();
}

void WorkerPool::WorkerEntry(int32_t worker_index) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

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

                if (_jobs.empty()) {
                    _workSignal.wait(locker);
                    continue;
                }

                const auto front_fire = _jobs.front().FireTime;
                const auto now = nanotime::now();

                if (front_fire > now) {
                    // Wait until the earliest job becomes due, or until something nearer arrives.
                    _workSignal.wait_until(locker, front_fire.value());
                    continue;
                }

                job = std::move(_jobs.front());
                _jobs.erase(_jobs.begin());

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

        {
            SyncContext sync_ctx;
            sync_ctx.Activate();
            auto cleanup = scope_exit([&]() noexcept {
                sync_ctx.Release();
                sync_ctx.Deactivate();
            });

            const bool shutdown = _shutdownFlag->load(std::memory_order_acquire);

            if (!shutdown) {
                try {
                    next_delay = job.Body();
                }
                catch (const std::exception& ex) {
                    if (!_shutdownFlag->load(std::memory_order_acquire)) {
                        ReportExceptionAndContinue(ex);
                    }
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }
        }

        bool need_wake = false;

        {
            scoped_lock locker {_mutex};

            if (job.Key != ANONYMOUS_JOB) {
                _runningKeys.erase(job.Key);

                const bool cancelled = _cancelOnFinish.erase(job.Key) != 0;
                const bool wake_requested = _wakeRequests.erase(job.Key) != 0;

                if (auto it = _pendingRerun.find(job.Key); it != _pendingRerun.end()) {
                    EnqueueJob(nanotime::now(), job.Key, std::move(it->second));
                    _queuedKeys.insert(job.Key);
                    _pendingRerun.erase(it);
                    need_wake = true;
                }
                else if (next_delay.has_value() && !cancelled) {
                    const auto reschedule_delay = wake_requested ? timespan::zero : next_delay.value();
                    EnqueueJob(nanotime::now() + reschedule_delay, job.Key, std::move(job.Body));
                    _queuedKeys.insert(job.Key);
                    need_wake = true;
                }
            }
            else if (next_delay.has_value()) {
                EnqueueJob(nanotime::now() + next_delay.value(), ANONYMOUS_JOB, std::move(job.Body));
                need_wake = true;
            }

            _activeWorkers--;
        }

        if (need_wake) {
            _workSignal.notify_one();
        }

        _idleSignal.notify_all();
    }
}

FO_END_NAMESPACE
