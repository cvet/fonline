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

#include "Threading.h"
#include "BaseLogging.h"
#include "ExceptionHandling.h"
#include "GlobalData.h"
#include "Platform.h"
#include "StackTrace.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

// Thread-local name slot. Backs `set_this_thread_name` / `get_this_thread_name`; populated
// lazily with a numeric default the first time the slot is read on a thread that never set
// itself. Same allocator/layout as before the move from WorkThread — only the symbol's home
// is different.
static thread_local std::string ThreadName;

struct ThreadingData
{
    ThreadingData() { set_this_thread_name("Main"); }
};
FO_GLOBAL_DATA(ThreadingData, ThreadingState);

struct PoolTask
{
    string Name;
    std::function<void()> Body;
};

struct Pool
{
    std::mutex Locker {};
    std::condition_variable WorkSignal {};
    std::deque<PoolTask> Pending {};
    vector<std::thread> Workers {};
    size_t IdleCount {};
    size_t MaxWorkers {};
    string NamePrefix {};
    bool Initialized {};
    bool Stopping {};
};

static void worker_loop(Pool* pool) noexcept;
static void internal_shutdown(Pool& pool) noexcept;
static void spawn_pool_worker(Pool& pool, const string& worker_name);

struct GlobalPools
{
    Pool RunPool {};
    Pool AsyncPool {};

    GlobalPools() = default;

    // Drain both pools before this struct's mutexes / condvars / deques are destroyed by the
    // FO_GLOBAL_DATA delete callback. Any worker still parked on `WorkSignal.wait` at that
    // point would touch a half-destroyed condition_variable / mutex — UB.
    ~GlobalPools() noexcept
    {
        internal_shutdown(RunPool);
        internal_shutdown(AsyncPool);
    }
};

FO_GLOBAL_DATA(GlobalPools, Pools);

// Caller must hold `pool.Locker`. Initialises the pool's `MaxWorkers` / `NamePrefix` on first
// use; idempotent for subsequent calls.
static void ensure_initialized_locked(Pool& pool, size_t max_workers, string_view name_prefix)
{
    if (pool.Initialized) {
        return;
    }

    pool.MaxWorkers = max_workers;
    pool.NamePrefix = string(name_prefix);
    pool.Initialized = true;
}

// Common submit path for both pools. Returns `true` if the task was accepted (either by an
// idle worker or by a newly spawned worker), or queued when `can_queue` is `true`. Returns
// `false` when `can_queue` is `false` and the pool has no idle worker AND is already at its
// `MaxWorkers` cap — that signal is used by `try_submit_async` to let the caller fall back
// to inline synchronous execution.
static auto submit_impl(Pool& pool, string_view task_name, std::function<void()> task, bool can_queue) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!task) {
        return true;
    }

    {
        std::lock_guard locker(pool.Locker);

        if (pool.Stopping) {
            throw GenericException("threading pool called after shutdown");
        }

        const bool has_idle_worker = pool.IdleCount > 0;
        const bool can_spawn_worker = pool.Workers.size() < pool.MaxWorkers;

        if (!has_idle_worker && !can_spawn_worker && !can_queue) {
            // Pool saturated and caller asked for try-only — let them fall back to sync.
            return false;
        }

        pool.Pending.push_back(PoolTask {string(task_name), std::move(task)});

        // Spawn a new worker whenever the pending tasks outnumber the parked (idle) workers,
        // not merely when IdleCount is zero. A worker counted in IdleCount may already have
        // been woken by a previous task's notify_one and is committed to dequeuing that task —
        // it is not actually available for this one. With long-lived tasks that never return
        // to the idle list (WorkerPool::WorkerEntry loops, every WorkThread/_mainWorker loop,
        // parallel-test session drivers), the old `!has_idle_worker` test could leave a freshly
        // submitted task queued with no worker to ever run it: the apparently-idle worker takes
        // the earlier long-lived task and never parks again. Keeping workers >= pending tasks
        // guarantees forward progress for every submitted task.
        if (pool.Pending.size() > pool.IdleCount && can_spawn_worker) {
            // Reserve/register the worker while still holding the pool lock so concurrent
            // submitters see the updated size and cannot overshoot MaxWorkers.
            spawn_pool_worker(pool, strex("{}-{}", pool.NamePrefix, pool.Workers.size()));
        }
    }

    pool.WorkSignal.notify_one();
    return true;
}

// Caller must hold `pool.Locker`.
static void spawn_pool_worker(Pool& pool, const string& worker_name)
{
    FO_STACK_TRACE_ENTRY();

    pool.Workers.emplace_back([worker_name, pool_ptr = &pool] {
        try {
            set_this_thread_name(worker_name);
        }
        catch (...) {
            // Naming failure is non-fatal; the worker still runs tasks.
        }

        worker_loop(pool_ptr);
    });
}

static void worker_loop(Pool* pool) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // Remember the base worker name so we can restore it after each task's per-task name
    // override. The spawn helper already set it (e.g. "Pool-0" or "AsyncPool-3"); tasks
    // transiently rename via `set_this_thread_name` while they execute, which makes Tracy /
    // debugger thread lists informative. `get_this_thread_name` returns a `std::string` (default
    // allocator); convert to `fo::string` for the `set_this_thread_name` overload.
    const auto& base_name_std = get_this_thread_name();
    const string base_thread_name {base_name_std.data(), base_name_std.size()};

    while (true) {
        PoolTask task;

        {
            std::unique_lock locker(pool->Locker);

            ++pool->IdleCount;
            pool->WorkSignal.wait(locker, [pool] { return pool->Stopping || !pool->Pending.empty(); });
            --pool->IdleCount;

            if (pool->Pending.empty()) {
                // Stopping is true and queue drained — exit. Workers are joined in
                // `internal_shutdown`.
                return;
            }

            task = std::move(pool->Pending.front());
            pool->Pending.pop_front();
        }

        if (!task.Name.empty()) {
            set_this_thread_name(task.Name);
        }

        try {
            if (task.Body) {
                task.Body();
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }

        if (!task.Name.empty()) {
            set_this_thread_name(base_thread_name);
        }
    }
}

static void internal_shutdown(Pool& pool) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<std::thread> workers_to_join;

    {
        std::lock_guard locker(pool.Locker);

        if (pool.Stopping) {
            return;
        }

        pool.Stopping = true;
        workers_to_join = std::move(pool.Workers);
        pool.Workers.clear();
    }

    pool.WorkSignal.notify_all();

    for (auto& worker : workers_to_join) {
        try {
            if (worker.joinable()) {
                worker.join();
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }
}

static auto hardware_concurrency_or_one() noexcept -> size_t
{
    const auto hw = std::thread::hardware_concurrency();
    return hw != 0 ? static_cast<size_t>(hw) : size_t {1};
}

static void submit_run_thread(string_view task_name, std::function<void()> task)
{
    FO_STACK_TRACE_ENTRY();

    auto& pool = Pools->RunPool;

    {
        std::lock_guard locker(pool.Locker);
        // Unbounded — every submit either reuses an idle worker or spawns a new one.
        ensure_initialized_locked(pool, std::numeric_limits<size_t>::max(), "RunPool");
    }

    submit_impl(pool, task_name, std::move(task), /*can_queue*/ true);
}

extern void set_this_thread_name(const string& name) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        ThreadName = name;
    }
    catch (...) {
    }

    Platform::SetThreadName(name);

#if FO_TRACY
    tracy::SetThreadName(name.c_str());
#endif
}

extern auto get_this_thread_name() noexcept -> const std::string&
{
    FO_STACK_TRACE_ENTRY();

    if (ThreadName.empty()) {
        static std::atomic_int32_t thread_counter = 0;

        try {
            ThreadName = strex("{}", ++thread_counter);
        }
        catch (...) {
        }
    }

    return ThreadName;
}

extern auto run_thread(string_view task_name, function<void()> task) -> thread
{
    FO_STACK_TRACE_ENTRY();

    // `std::promise<void>` (not `std::packaged_task`) so the pool worker_loop's outer try/catch
    // still sees and reports the task body's exception via `ReportExceptionAndContinue` — a
    // `packaged_task` would silently store the exception into the future where a `.join()`
    // caller never observes it. The promise is set unconditionally inside a `try`/`catch`
    // around the body so a body exception both gets reported AND wakes any `.join()`.
    auto promise = std::make_shared<std::promise<void>>();
    // Shared with the lambda below so the pool body and the returned handle's `get_id()` read
    // from the same slot. The body stores the worker thread id at entry and clears it at exit;
    // readers see a valid id only while the body is actually executing.
    auto running_thread_id = std::make_shared<std::atomic<std::thread::id>>();
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
            ReportExceptionAndContinue(ex);
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

auto try_submit_async(string_view task_name, std::function<void()> task) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto& pool = Pools->AsyncPool;

    {
        std::lock_guard locker(pool.Locker);
        // Capped at hardware concurrency — many short async tasks shouldn't be allowed to
        // grow the thread count past what the host can actually run in parallel.
        ensure_initialized_locked(pool, hardware_concurrency_or_one(), "AsyncPool");
    }

    return submit_impl(pool, task_name, std::move(task), /*can_queue*/ false);
}

void submit_async(string_view task_name, std::function<void()> task)
{
    FO_STACK_TRACE_ENTRY();

    auto& pool = Pools->AsyncPool;

    {
        std::lock_guard locker(pool.Locker);
        ensure_initialized_locked(pool, hardware_concurrency_or_one(), "AsyncPool");
    }

    submit_impl(pool, task_name, std::move(task), /*can_queue*/ true);
}

void thread::join()
{
    FO_STACK_TRACE_ENTRY();

    if (_future.valid()) {
        auto future = std::move(_future);
        _runningThreadId.reset();
        future.get();
    }
}

void thread::detach() noexcept
{
    _future = {};
    _runningThreadId.reset();
}

FO_END_NAMESPACE
