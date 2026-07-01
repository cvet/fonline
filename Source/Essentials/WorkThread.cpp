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

#include "WorkThread.h"
#include "ExceptionHandling.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

WorkThread::WorkThread(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    _name = name;
    _worker = run_thread(name, [this] { ThreadEntry(); });
}

WorkThread::~WorkThread()
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_dataLocker};

        _finish = true;
    }

    _workSignal.notify_one();

    try {
        if (_worker.joinable()) {
            _worker.join();
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

auto WorkThread::GetJobsCount() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_dataLocker};

    return _jobs.size() + (_jobActive ? 1 : 0);
}

auto WorkThread::GetDiagnostics() const -> Diagnostics
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_dataLocker};

    return Diagnostics {
        .QueuedJobs = _jobs.size(),
        .JobActive = _jobActive,
        .CompletedJobs = _completedJobs,
    };
}

void WorkThread::SetExceptionHandler(ExceptionHandler handler)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_dataLocker};

    _exceptionHandler = std::move(handler);
}

void WorkThread::AddJob(Job job)
{
    FO_STACK_TRACE_ENTRY();

    AddJobInternal(std::chrono::milliseconds {0}, std::move(job), false);
}

void WorkThread::AddJob(timespan delay, Job job)
{
    FO_STACK_TRACE_ENTRY();

    AddJobInternal(delay, std::move(job), false);
}

void WorkThread::AddJobInternal(timespan delay, Job job, bool no_notify)
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_dataLocker};

        const auto fire_time = nanotime::now() + delay;

        if (_jobs.empty() || fire_time >= _jobs.back().first) {
            _jobs.emplace_back(fire_time, std::move(job));
        }
        else {
            for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                if (fire_time < it->first) {
                    _jobs.emplace(it, fire_time, std::move(job));
                    break;
                }
            }
        }
    }

    if (!no_notify) {
        _workSignal.notify_one();
    }
}

void WorkThread::Clear()
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker(_dataLocker);

    _clearJobs = true;

    locker.unlock();
    _workSignal.notify_one();
    locker.lock();

    while (_clearJobs) {
        _doneSignal.wait(locker);
    }
}

void WorkThread::Wait() const
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker(_dataLocker);

    while (!_jobs.empty() || _jobActive) {
        _doneSignal.wait(locker);
    }
}

void WorkThread::Pause()
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker(_dataLocker);

    _paused = true;

    while (_jobActive) {
        _doneSignal.wait(locker);
    }
}

void WorkThread::Resume()
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_dataLocker};

        _paused = false;
    }

    _workSignal.notify_one();
}

void WorkThread::ThreadEntry() noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        while (true) {
            Job job;

            {
                unique_lock locker(_dataLocker);

                _jobActive = false;

                if (_clearJobs) {
                    _jobs.clear();
                    _clearJobs = false;
                }

                locker.unlock();
                _doneSignal.notify_all();
                locker.lock();

                while (_paused && !_finish) {
                    _workSignal.wait(locker);
                }

                if (_jobs.empty()) {
                    if (_clearJobs) {
                        _clearJobs = false;
                    }

                    if (_finish) {
                        break;
                    }

                    _workSignal.wait(locker);
                    continue;
                }

                if (!_jobs.empty()) {
                    const auto cur_time = nanotime::now();
                    nanotime soonest_fire;
                    bool has_pending = false;

                    for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                        if (cur_time >= it->first) {
                            job = std::move(it->second);
                            _jobs.erase(it);
                            _jobActive = true;
                            break;
                        }

                        if (!has_pending || it->first < soonest_fire) {
                            soonest_fire = it->first;
                            has_pending = true;
                        }
                    }

                    // Nothing is due yet — sleep until the soonest deadline instead of spinning. AddJob /
                    // Wake / Clear / Pause notify _workSignal, so a new or earlier job breaks the wait.
                    if (!job && has_pending) {
                        _workSignal.wait_until(locker, soonest_fire.value());
                        continue;
                    }
                }
            }

            if (job) {
                try {
                    const auto next_call_delay = job();

                    // Schedule repeat
                    if (next_call_delay.has_value()) {
                        AddJobInternal(next_call_delay.value(), std::move(job), true);
                    }
                }
                catch (const std::exception& ex) {
                    // Exception handling
                    {
                        scoped_lock locker {_dataLocker};

                        if (_exceptionHandler) {
                            try {
                                if (_exceptionHandler(ex)) {
                                    _jobs.clear();
                                }
                            }
                            catch (const std::exception& ex2) {
                                ReportExceptionAndContinue(ex2);
                            }
                        }
                    }

                    ReportExceptionAndContinue(ex);
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }

                {
                    scoped_lock locker {_dataLocker};
                    _completedJobs++;
                }
            }
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

FO_END_NAMESPACE
