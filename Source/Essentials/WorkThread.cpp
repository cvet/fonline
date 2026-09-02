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

#include "WorkThread.h"
#include "ExceptionHandling.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

work_thread::work_thread(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    _name = name;
    _worker = run_thread(name, [this] { thread_entry(); });
}

work_thread::~work_thread()
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_data_locker};

        _finish = true;
    }

    _work_signal.notify_one();

    try {
        if (_worker.joinable()) {
            _worker.join();
        }
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

auto work_thread::get_jobs_count() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_data_locker};

    return _jobs.size() + (_job_active ? 1 : 0);
}

auto work_thread::get_diagnostics() const -> diagnostics
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_data_locker};

    return diagnostics {
        .queued_jobs = _jobs.size(),
        .job_active = _job_active,
        .completed_jobs = _completed_jobs,
    };
}

void work_thread::set_exception_handler(exception_handler handler)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_data_locker};

    _exception_handler = std::move(handler);
}

void work_thread::add_job(job next_job)
{
    FO_STACK_TRACE_ENTRY();

    add_job_internal(std::chrono::milliseconds {0}, std::move(next_job), false);
}

void work_thread::add_job(timespan delay, job next_job)
{
    FO_STACK_TRACE_ENTRY();

    add_job_internal(delay, std::move(next_job), false);
}

void work_thread::add_job_internal(timespan delay, job next_job, bool no_notify)
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_data_locker};

        nanotime fire_time = nanotime::now() + delay;

        if (_jobs.empty() || fire_time >= _jobs.back().first) {
            _jobs.emplace_back(fire_time, std::move(next_job));
        }
        else {
            for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                if (fire_time < it->first) {
                    _jobs.emplace(it, fire_time, std::move(next_job));
                    break;
                }
            }
        }
    }

    if (!no_notify) {
        _work_signal.notify_one();
    }
}

void work_thread::clear()
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker(_data_locker);

    _clear_jobs = true;

    locker.unlock();
    _work_signal.notify_one();
    locker.lock();

    while (_clear_jobs) {
        _done_signal.wait(locker);
    }
}

void work_thread::wait() const
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker(_data_locker);

    while (!_jobs.empty() || _job_active) {
        _done_signal.wait(locker);
    }
}

void work_thread::pause()
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker(_data_locker);

    _paused = true;

    while (_job_active) {
        _done_signal.wait(locker);
    }
}

void work_thread::resume()
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock locker {_data_locker};

        _paused = false;
    }

    _work_signal.notify_one();
}

void work_thread::thread_entry() noexcept
{
    FO_STACK_TRACE_ENTRY();

    exceptions::install_crash_handler_stack();

    try {
        while (true) {
            job next_job;

            {
                unique_lock locker(_data_locker);

                _job_active = false;

                if (_clear_jobs) {
                    _jobs.clear();
                    _clear_jobs = false;
                }

                locker.unlock();
                _done_signal.notify_all();
                locker.lock();

                while (_paused && !_finish) {
                    if (_clear_jobs) {
                        _jobs.clear();
                        _clear_jobs = false;

                        locker.unlock();
                        _done_signal.notify_all();
                        locker.lock();
                    }
                    else {
                        _work_signal.wait(locker);
                    }
                }

                if (_jobs.empty()) {
                    if (_clear_jobs) {
                        _clear_jobs = false;
                    }

                    if (_finish) {
                        break;
                    }

                    _work_signal.wait(locker);
                    continue;
                }

                if (!_jobs.empty()) {
                    nanotime cur_time = nanotime::now();
                    nanotime soonest_fire;
                    bool has_pending = false;

                    for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                        if (cur_time >= it->first) {
                            next_job = std::move(it->second);
                            _jobs.erase(it);
                            _job_active = true;
                            break;
                        }

                        if (!has_pending || it->first < soonest_fire) {
                            soonest_fire = it->first;
                            has_pending = true;
                        }
                    }

                    // Nothing is due yet — sleep until the soonest deadline instead of spinning. add_job /
                    // Wake / Clear / Pause notify _work_signal, so a new or earlier job breaks the wait
                    if (!next_job && has_pending) {
                        _work_signal.wait_until(locker, soonest_fire.value());
                        continue;
                    }
                }
            }

            if (next_job) {
                try {
                    auto next_call_delay = next_job();

                    // Schedule repeat
                    if (next_call_delay.has_value()) {
                        add_job_internal(next_call_delay.value(), std::move(next_job), true);
                    }
                }
                catch (const std::exception& ex) {
                    // Exception handling
                    {
                        scoped_lock locker {_data_locker};

                        if (_exception_handler) {
                            try {
                                if (_exception_handler(ex)) {
                                    _jobs.clear();
                                }
                            }
                            catch (const std::exception& ex2) {
                                exceptions::report_and_continue(ex2);
                            }
                        }
                    }

                    exceptions::report_and_continue(ex);
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }

                {
                    scoped_lock locker {_data_locker};
                    _completed_jobs++;
                }
            }
        }
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

FO_END_NAMESPACE
