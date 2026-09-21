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

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHandling.h"
#include "Threading.h"
#include "TimeRelated.h"

FO_BEGIN_NAMESPACE

class work_thread
{
public:
    using job = function<optional<timespan>()>;
    using exception_handler = function<bool(const std::exception&)>; // Return true to clear jobs

    struct diagnostics
    {
        size_t queued_jobs {};
        bool job_active {};
        uint64_t completed_jobs {};
    };

    explicit work_thread(string_view name);
    work_thread(const work_thread&) = delete;
    work_thread(work_thread&&) noexcept = delete;
    auto operator=(const work_thread&) -> work_thread& = delete;
    auto operator=(work_thread&&) noexcept -> work_thread& = delete;
    ~work_thread();

    [[nodiscard]] auto get_thread_id() const -> std::thread::id { return _worker.get_id(); }
    [[nodiscard]] auto get_jobs_count() const -> size_t;
    [[nodiscard]] auto get_diagnostics() const -> diagnostics;

    void set_exception_handler(exception_handler handler);
    void add_job(job next_job);
    void add_job(timespan delay, job next_job);
    void clear();
    void wait() const;
    void pause();
    void resume();

private:
    void add_job_internal(timespan delay, job next_job, bool no_notify);
    void thread_entry() noexcept;

    string _name {};
    thread _worker {};
    mutable mutex _data_locker {};
    exception_handler _exception_handler FO_TSA_GUARDED_BY(_data_locker) {};
    vector<pair<nanotime, job>> _jobs FO_TSA_GUARDED_BY(_data_locker) {};
    uint64_t _completed_jobs FO_TSA_GUARDED_BY(_data_locker) {};
    bool _job_active FO_TSA_GUARDED_BY(_data_locker) {};
    bool _paused FO_TSA_GUARDED_BY(_data_locker) {};
    bool _clear_jobs FO_TSA_GUARDED_BY(_data_locker) {};
    bool _finish FO_TSA_GUARDED_BY(_data_locker) {};
    std::condition_variable_any _work_signal {};
    mutable std::condition_variable_any _done_signal {};
};

FO_END_NAMESPACE
