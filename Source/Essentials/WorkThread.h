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

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHandling.h"
#include "Threading.h"
#include "TimeRelated.h"

FO_BEGIN_NAMESPACE

class WorkThread
{
public:
    using Job = function<optional<timespan>()>;
    using ExceptionHandler = function<bool(const std::exception&)>; // Return true to clear jobs

    struct Diagnostics
    {
        size_t QueuedJobs {};
        bool JobActive {};
        uint64_t CompletedJobs {};
    };

    explicit WorkThread(string_view name);
    WorkThread(const WorkThread&) = delete;
    WorkThread(WorkThread&&) noexcept = delete;
    auto operator=(const WorkThread&) -> WorkThread& = delete;
    auto operator=(WorkThread&&) noexcept -> WorkThread& = delete;
    ~WorkThread();

    [[nodiscard]] auto GetThreadId() const -> std::thread::id { return _worker.get_id(); }
    [[nodiscard]] auto GetJobsCount() const -> size_t;
    [[nodiscard]] auto GetDiagnostics() const -> Diagnostics;

    void SetExceptionHandler(ExceptionHandler handler);
    void AddJob(Job job);
    void AddJob(timespan delay, Job job);
    void Clear();
    void Wait() const;
    void Pause();
    void Resume();

private:
    void AddJobInternal(timespan delay, Job job, bool no_notify);
    void ThreadEntry() noexcept;

    string _name {};
    thread _worker {};
    mutable mutex _dataLocker {};
    ExceptionHandler _exceptionHandler FO_TSA_GUARDED_BY(_dataLocker) {};
    vector<pair<nanotime, Job>> _jobs FO_TSA_GUARDED_BY(_dataLocker) {};
    uint64_t _completedJobs FO_TSA_GUARDED_BY(_dataLocker) {};
    bool _jobActive FO_TSA_GUARDED_BY(_dataLocker) {};
    bool _paused FO_TSA_GUARDED_BY(_dataLocker) {};
    bool _clearJobs FO_TSA_GUARDED_BY(_dataLocker) {};
    bool _finish FO_TSA_GUARDED_BY(_dataLocker) {};
    std::condition_variable_any _workSignal {};
    mutable std::condition_variable_any _doneSignal {};
};

FO_END_NAMESPACE
