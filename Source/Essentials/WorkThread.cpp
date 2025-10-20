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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ExceptionHadling.h"
#include "GlobalData.h"
#include "Platform.h"
#include "StackTrace.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE();

struct WorkThreadData
{
    WorkThreadData() { SetThisThreadName("Main"); }
};
FO_GLOBAL_DATA(WorkThreadData, WorkThread);

static thread_local string ThreadName;

extern void SetThisThreadName(const string& name) noexcept
{
    FO_STACK_TRACE_ENTRY();

    ThreadName = name;

    Platform::SetThreadName(ThreadName);

#if FO_TRACY
    tracy::SetThreadName(ThreadName.c_str());
#endif
}

extern auto GetThisThreadName() noexcept -> const string&
{
    FO_STACK_TRACE_ENTRY();

    if (ThreadName.empty()) {
        static size_t thread_counter = 0;
        thread_local size_t thread_num = ++thread_counter;

        ThreadName = strex(strex::safe_format, "{}", thread_num);
    }

    return ThreadName;
}

WorkThread::WorkThread(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    _name = name;
    _thread = std::thread(&WorkThread::ThreadEntry, this);
}

WorkThread::~WorkThread()
{
    FO_STACK_TRACE_ENTRY();

    {
        std::unique_lock locker(_dataLocker);

        _finish = true;
    }

    _workSignal.notify_one();

    try {
        _thread.join();
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

    std::unique_lock locker(_dataLocker);

    return _jobs.size() + (_jobActive ? 1 : 0);
}

void WorkThread::SetExceptionHandler(ExceptionHandler handler)
{
    FO_STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

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
        std::unique_lock locker(_dataLocker);

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

    std::unique_lock locker(_dataLocker);

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

    std::unique_lock locker(_dataLocker);

    while (!_jobs.empty() || _jobActive) {
        _doneSignal.wait(locker);
    }
}

void WorkThread::ThreadEntry() noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        SetThisThreadName(_name);

        while (true) {
            Job job;

            {
                std::unique_lock locker(_dataLocker);

                _jobActive = false;

                if (_clearJobs) {
                    _jobs.clear();
                    _clearJobs = false;
                }

                if (_jobs.empty()) {
                    locker.unlock();
                    _doneSignal.notify_all();
                    locker.lock();
                }

                if (_jobs.empty()) {
                    if (_clearJobs) {
                        _clearJobs = false;
                    }

                    if (_finish) {
                        break;
                    }

                    _workSignal.wait(locker);
                }

                if (!_jobs.empty()) {
                    const auto cur_time = nanotime::now();

                    for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                        if (cur_time >= it->first) {
                            job = std::move(it->second);
                            _jobs.erase(it);
                            _jobActive = true;
                            break;
                        }
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
                    ReportExceptionAndContinue(ex);

                    // Exception handling
                    {
                        std::unique_lock locker(_dataLocker);

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

                    // Todo: schedule job repeat with last duration?
                    try {
                        // AddJobInternal(next_call_duration.value(), std::move(job), true);
                    }
                    catch (const std::exception& ex2) {
                        ReportExceptionAndContinue(ex2);
                    }
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
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

FO_END_NAMESPACE();
