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

#include "Threading.h"
#include "BaseLogging.h"
#include "ExceptionHandling.h"
#include "GlobalData.h"
#include "Platform.h"
#include "StackTrace.h"
#include "StringUtils.h"
#include "WinApi.h"

FO_BEGIN_NAMESPACE

// Lazily filled with a numeric default the first time a thread that never named itself reads it
static thread_local string thread_name;

struct threading_data
{
    threading_data() { set_this_thread_name("Main"); }
};
FO_GLOBAL_DATA(threading_data, threading_state);

struct pool_task
{
    string name;
    function<void()> body;
};

struct thread_pool
{
    std::mutex locker {};
    std::condition_variable work_signal {};
    deque<pool_task> pending {};
    vector<std::thread> workers {};
    size_t idle_count {};
    size_t max_workers {};
    string name_prefix {};
    bool initialized {};
    bool stopping {};
};

static void worker_loop(thread_pool* pool) noexcept;
static void internal_shutdown(thread_pool& pool) noexcept;
static void spawn_pool_worker(thread_pool& pool, const string& worker_name);
static void park_until(std::chrono::steady_clock::time_point deadline) noexcept;

// The OS wait overshoots its deadline by a few hundred microseconds, so the tail of a sleep is spun out
// instead. Also the cutoff under which a whole sleep is spun, since parking that briefly is not possible
static constexpr std::chrono::nanoseconds PRECISE_SLEEP_SPIN_BUDGET = std::chrono::milliseconds {1};

struct global_pools
{
    thread_pool run_pool {};
    thread_pool async_pool {};

    global_pools() = default;

    // Drain before the FO_GLOBAL_DATA delete callback destroys these mutexes and condvars: a worker
    // still parked on `work_signal.wait` would touch a half-destroyed condition_variable
    ~global_pools() noexcept
    {
        internal_shutdown(run_pool);
        internal_shutdown(async_pool);
    }
};

FO_GLOBAL_DATA(global_pools, pools);

// Caller must hold `pool.locker`
static void ensure_initialized_locked(thread_pool& pool, size_t max_workers, string_view name_prefix)
{
    if (pool.initialized) {
        return;
    }

    pool.max_workers = max_workers;
    pool.name_prefix = string(name_prefix);
    pool.initialized = true;
}

// Returns `false` only when the pool cannot queue, has no idle worker, and is at `max_workers`; that
// is how `try_submit_async` learns to run the task inline instead
static auto submit_impl(thread_pool& pool, string_view task_name, function<void()> task, bool can_queue) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!task) {
        return true;
    }

    {
        std::lock_guard locker(pool.locker);

        if (pool.stopping) {
            throw GenericException("threading pool called after shutdown");
        }

        bool has_idle_worker = pool.idle_count > 0;
        bool can_spawn_worker = pool.workers.size() < pool.max_workers;

        if (!has_idle_worker && !can_spawn_worker && !can_queue) {
            // thread_pool saturated and caller asked for try-only — let them fall back to sync
            return false;
        }

        pool.pending.push_back(pool_task {string(task_name), std::move(task)});

        // workers must outnumber pending tasks, not merely be non-idle: a worker inside idle_count may
        // already be committed to an earlier notify, and a long-lived task never parks again
        if (pool.pending.size() > pool.idle_count && can_spawn_worker) {
            // Reserve/register the worker while still holding the pool lock so concurrent
            // submitters see the updated size and cannot overshoot max_workers
            try {
                spawn_pool_worker(pool, strex("{}-{}", pool.name_prefix, pool.workers.size()));
            }
            catch (...) {
                // Thread exhaustion is recoverable, unlike terminate-on-OOM allocation, so restore the
                // invariant that every queued task has a worker: an orphan keeps a dangling capture
                pool.pending.pop_back();
                throw;
            }
        }
    }

    pool.work_signal.notify_one();
    return true;
}

// Caller must hold `pool.locker`. Deliberately not noexcept: OS thread exhaustion is a recoverable
// std::system_error, so `submit_impl` rolls back instead of terminating
static void spawn_pool_worker(thread_pool& pool, const string& worker_name)
{
    FO_STACK_TRACE_ENTRY();

    pool.workers.emplace_back([worker_name, pool_ptr = &pool] {
        try {
            set_this_thread_name(worker_name);
        }
        catch (...) {
            // Naming failure is non-fatal; the worker still runs tasks
        }

        worker_loop(pool_ptr);
    });
}

static void worker_loop(thread_pool* pool) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // Tasks transiently rename the thread so Tracy and debugger lists stay informative, so keep an
    // owning copy of the spawn name to restore afterwards
    string_view base_name = get_this_thread_name();
    string base_thread_name {base_name.data(), base_name.size()};

    while (true) {
        pool_task task;

        {
            std::unique_lock locker(pool->locker);

            ++pool->idle_count;
            pool->work_signal.wait(locker, [pool] { return pool->stopping || !pool->pending.empty(); });
            --pool->idle_count;

            if (pool->pending.empty()) {
                // stopping is true and queue drained — exit. workers are joined in
                // `internal_shutdown`
                return;
            }

            task = std::move(pool->pending.front());
            pool->pending.pop_front();
        }

        if (!task.name.empty()) {
            set_this_thread_name(task.name);
        }

        try {
            if (task.body) {
                task.body();
            }
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }

        if (!task.name.empty()) {
            set_this_thread_name(base_thread_name);
        }
    }
}

static void internal_shutdown(thread_pool& pool) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<std::thread> workers_to_join;

    {
        std::lock_guard locker(pool.locker);

        if (pool.stopping) {
            return;
        }

        pool.stopping = true;
        workers_to_join = std::move(pool.workers);
        pool.workers.clear();
    }

    pool.work_signal.notify_all();

    for (auto& worker : workers_to_join) {
        try {
            if (worker.joinable()) {
                worker.join();
            }
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }
}

static auto hardware_concurrency_or_one() noexcept -> size_t
{
    auto hw = std::thread::hardware_concurrency();
    return hw != 0 ? static_cast<size_t>(hw) : size_t {1};
}

static void submit_run_thread(string_view task_name, function<void()> task)
{
    FO_STACK_TRACE_ENTRY();

    auto& pool = pools->run_pool;

    {
        std::lock_guard locker(pool.locker);
        // Unbounded — every submit either reuses an idle worker or spawns a new one
        ensure_initialized_locked(pool, std::numeric_limits<size_t>::max(), "run_pool");
    }

    submit_impl(pool, task_name, std::move(task), /*can_queue*/ true);
}

void set_this_thread_name(const string& name) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        thread_name = name;
    }
    catch (...) {
    }

    platform::set_thread_name(name);

#if FO_TRACY
    tracy::SetThreadName(name.c_str());
#endif
}

auto get_this_thread_name() noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (thread_name.empty()) {
        static std::atomic_int32_t thread_counter = 0;

        try {
            thread_name = strex("{}", ++thread_counter);
        }
        catch (...) {
        }
    }

    return {thread_name.data(), thread_name.size()};
}

void coarse_sleep(std::chrono::nanoseconds duration) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (duration > std::chrono::nanoseconds::zero()) {
        park_until(std::chrono::steady_clock::now() + duration);
    }
}

void precise_sleep(std::chrono::nanoseconds duration) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (duration <= std::chrono::nanoseconds::zero()) {
        return;
    }

    auto deadline = std::chrono::steady_clock::now() + duration;

    if (duration > PRECISE_SLEEP_SPIN_BUDGET) {
        park_until(deadline - PRECISE_SLEEP_SPIN_BUDGET);
    }

    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
}

auto run_thread(string_view task_name, function<void()> task) -> thread
{
    FO_STACK_TRACE_ENTRY();

    // A promise rather than `std::packaged_task`: the latter swallows the body's exception into the
    // future, where `worker_loop` can no longer report it and a caller that never joins loses it
    auto promise = safe_alloc::make_shared<std::promise<void>>();
    // Shared with the body below, which fills the slot at entry and clears it at exit, so `get_id()`
    // reports an id only while the task actually runs
    auto running_thread_id = safe_alloc::make_shared<std::atomic<std::thread::id>>();
    auto handle = thread {promise->get_future(), running_thread_id};

    submit_run_thread(task_name, [body = std::move(task), promise = std::move(promise), running_thread_id = std::move(running_thread_id)]() mutable {
        running_thread_id->store(std::this_thread::get_id(), std::memory_order_release);

        try {
            if (body) {
                body();
            }
        }
        catch (const std::exception& ex) {
            running_thread_id->store({}, std::memory_order_release);
            try {
                promise->set_value();
            }
            catch (...) {
            }
            exceptions::report_and_continue(ex);
            return;
        }
        catch (...) {
            running_thread_id->store({}, std::memory_order_release);
            try {
                promise->set_value();
            }
            catch (...) {
            }
            FO_UNKNOWN_EXCEPTION();
            return;
        }

        running_thread_id->store({}, std::memory_order_release);
        try {
            promise->set_value();
        }
        catch (...) {
        }
    });

    return handle;
}

auto try_submit_async(string_view task_name, function<void()> task) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto& pool = pools->async_pool;

    {
        std::lock_guard locker(pool.locker);
        // Capped at hardware concurrency — many short async tasks shouldn't be allowed to
        // grow the thread count past what the host can actually run in parallel
        ensure_initialized_locked(pool, hardware_concurrency_or_one(), "async_pool");
    }

    return submit_impl(pool, task_name, std::move(task), /*can_queue*/ false);
}

void submit_async(string_view task_name, function<void()> task)
{
    FO_STACK_TRACE_ENTRY();

    auto& pool = pools->async_pool;

    {
        std::lock_guard locker(pool.locker);
        ensure_initialized_locked(pool, hardware_concurrency_or_one(), "async_pool");
    }

    submit_impl(pool, task_name, std::move(task), /*can_queue*/ true);
}

void thread::join()
{
    FO_STACK_TRACE_ENTRY();

    if (_future.valid()) {
        auto future = std::move(_future);
        _running_thread_id.reset();
        future.get();
    }
}

void thread::detach() noexcept
{
    _future = {};
    _running_thread_id.reset();
}

static void park_until(std::chrono::steady_clock::time_point deadline) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto remaining = deadline - std::chrono::steady_clock::now();

    if (remaining <= std::chrono::steady_clock::duration::zero()) {
        return;
    }

#if FO_WINDOWS
    // std::this_thread::sleep_for rounds up to the timer tick, so the wait goes through a waitable timer
    nptr<void> timer = winapi::create_high_resolution_timer();

    if (!timer) {
        std::this_thread::sleep_for(remaining);
        return;
    }

    int64_t delay_100ns = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count() / 100;

    if (winapi::set_relative_timer(timer, delay_100ns)) {
        winapi::wait_for_object(timer);
    }
    else {
        std::this_thread::sleep_for(remaining);
    }

    winapi::close_handle(timer);

#else
    std::this_thread::sleep_for(remaining);
#endif
}

FO_END_NAMESPACE
