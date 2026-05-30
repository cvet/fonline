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

#include <future>

// Clang Thread Safety Analysis annotations (https://clang.llvm.org/docs/ThreadSafetyAnalysis.html).
// Under Clang/ClangCL the FO_TSA_* macros expand to capability attributes; Clang parses them on every build and
// diagnoses misuse when -Wthread-safety is enabled (the project enables it as -Werror on every Clang toolchain).
// On MSVC, GCC and any other compiler they are no-ops. Use them through the annotated fo::mutex / fo::shared_mutex
// primitives and the fo::scoped_lock / fo::shared_lock / fo::unique_lock guards defined below — raw std::mutex is not
// a capability under the platform STL (libc++ disabled), so FO_TSA_GUARDED_BY(std::mutex) would not be checked.
#if defined(__clang__)
#define FO_TSA_ATTR(...) __attribute__((__VA_ARGS__))
#else
#define FO_TSA_ATTR(...)
#endif

#define FO_TSA_CAPABILITY(x) FO_TSA_ATTR(capability(x))
#define FO_TSA_SCOPED_CAPABILITY FO_TSA_ATTR(scoped_lockable)
#define FO_TSA_GUARDED_BY(x) FO_TSA_ATTR(guarded_by(x))
#define FO_TSA_PT_GUARDED_BY(x) FO_TSA_ATTR(pt_guarded_by(x))
#define FO_TSA_ACQUIRED_BEFORE(...) FO_TSA_ATTR(acquired_before(__VA_ARGS__))
#define FO_TSA_ACQUIRED_AFTER(...) FO_TSA_ATTR(acquired_after(__VA_ARGS__))
#define FO_TSA_REQUIRES(...) FO_TSA_ATTR(requires_capability(__VA_ARGS__))
#define FO_TSA_REQUIRES_SHARED(...) FO_TSA_ATTR(requires_shared_capability(__VA_ARGS__))
#define FO_TSA_ACQUIRE(...) FO_TSA_ATTR(acquire_capability(__VA_ARGS__))
#define FO_TSA_ACQUIRE_SHARED(...) FO_TSA_ATTR(acquire_shared_capability(__VA_ARGS__))
#define FO_TSA_RELEASE(...) FO_TSA_ATTR(release_capability(__VA_ARGS__))
#define FO_TSA_RELEASE_SHARED(...) FO_TSA_ATTR(release_shared_capability(__VA_ARGS__))
#define FO_TSA_RELEASE_GENERIC(...) FO_TSA_ATTR(release_generic_capability(__VA_ARGS__))
#define FO_TSA_TRY_ACQUIRE(...) FO_TSA_ATTR(try_acquire_capability(__VA_ARGS__))
#define FO_TSA_TRY_ACQUIRE_SHARED(...) FO_TSA_ATTR(try_acquire_shared_capability(__VA_ARGS__))
#define FO_TSA_EXCLUDES(...) FO_TSA_ATTR(locks_excluded(__VA_ARGS__))
#define FO_TSA_ASSERT_CAPABILITY(x) FO_TSA_ATTR(assert_capability(x))
#define FO_TSA_ASSERT_SHARED_CAPABILITY(x) FO_TSA_ATTR(assert_shared_capability(x))
#define FO_TSA_RETURN_CAPABILITY(x) FO_TSA_ATTR(lock_returned(x))
#define FO_TSA_NO_ANALYSIS FO_TSA_ATTR(no_thread_safety_analysis)

FO_BEGIN_NAMESPACE

// Synchronization primitives — drop-in replacements for the std analogues, annotated for Clang Thread Safety
// Analysis. The platform STL (libc++ is disabled) does not annotate std::mutex / std::shared_mutex as capabilities,
// so guarded state must use these wrappers for the analysis to track it; see Engine/Docs/ThreadSafetyAnalysis.md.
// Method names mirror std so call sites are a plain std:: -> fo:: swap; fo::unique_lock is usable with
// std::condition_variable_any (wait() drives lock()/unlock()).

class FO_TSA_CAPABILITY("mutex") mutex
{
public:
    mutex() = default;
    mutex(const mutex&) = delete;
    mutex(mutex&&) noexcept = delete;
    auto operator=(const mutex&) -> mutex& = delete;
    auto operator=(mutex&&) noexcept -> mutex& = delete;
    ~mutex() = default;

    void lock() FO_TSA_ACQUIRE() { _impl.lock(); }
    void unlock() FO_TSA_RELEASE() { _impl.unlock(); }
    bool try_lock() FO_TSA_TRY_ACQUIRE(true) { return _impl.try_lock(); }

private:
    std::mutex _impl;
};

class FO_TSA_CAPABILITY("shared_mutex") shared_mutex
{
public:
    shared_mutex() = default;
    shared_mutex(const shared_mutex&) = delete;
    shared_mutex(shared_mutex&&) noexcept = delete;
    auto operator=(const shared_mutex&) -> shared_mutex& = delete;
    auto operator=(shared_mutex&&) noexcept -> shared_mutex& = delete;
    ~shared_mutex() = default;

    void lock() FO_TSA_ACQUIRE() { _impl.lock(); }
    void unlock() FO_TSA_RELEASE() { _impl.unlock(); }
    bool try_lock() FO_TSA_TRY_ACQUIRE(true) { return _impl.try_lock(); }
    void lock_shared() FO_TSA_ACQUIRE_SHARED() { _impl.lock_shared(); }
    void unlock_shared() FO_TSA_RELEASE_SHARED() { _impl.unlock_shared(); }
    bool try_lock_shared() FO_TSA_TRY_ACQUIRE_SHARED(true) { return _impl.try_lock_shared(); }

private:
    std::shared_mutex _impl;
};

// Exclusive RAII guard for mutex or shared_mutex (drop-in for std::scoped_lock / std::lock_guard).
template<typename T>
class FO_TSA_SCOPED_CAPABILITY scoped_lock
{
public:
    explicit scoped_lock(T& mtx) FO_TSA_ACQUIRE(mtx) :
        _mutex {mtx}
    {
        _mutex.lock();
    }
    scoped_lock(const scoped_lock&) = delete;
    scoped_lock(scoped_lock&&) noexcept = delete;
    auto operator=(const scoped_lock&) -> scoped_lock& = delete;
    auto operator=(scoped_lock&&) noexcept -> scoped_lock& = delete;
    ~scoped_lock() FO_TSA_RELEASE() { _mutex.unlock(); }

private:
    T& _mutex;
};

// Shared (reader) RAII guard for shared_mutex (drop-in for std::shared_lock).
template<typename T>
class FO_TSA_SCOPED_CAPABILITY shared_lock
{
public:
    explicit shared_lock(T& mtx) FO_TSA_ACQUIRE_SHARED(mtx) :
        _mutex {mtx}
    {
        _mutex.lock_shared();
    }
    shared_lock(const shared_lock&) = delete;
    shared_lock(shared_lock&&) noexcept = delete;
    auto operator=(const shared_lock&) -> shared_lock& = delete;
    auto operator=(shared_lock&&) noexcept -> shared_lock& = delete;
    ~shared_lock() FO_TSA_RELEASE_GENERIC() { _mutex.unlock_shared(); }

private:
    T& _mutex;
};

// Movable-semantics exclusive lock with manual lock/unlock (drop-in for std::unique_lock). Works with
// std::condition_variable_any, whose wait() drives lock()/unlock() on the held lock.
template<typename T>
class FO_TSA_SCOPED_CAPABILITY unique_lock
{
public:
    explicit unique_lock(T& mtx) FO_TSA_ACQUIRE(mtx) :
        _mutex {&mtx}
    {
        _mutex->lock();
    }
    unique_lock(const unique_lock&) = delete;
    unique_lock(unique_lock&&) noexcept = delete;
    auto operator=(const unique_lock&) -> unique_lock& = delete;
    auto operator=(unique_lock&&) noexcept -> unique_lock& = delete;
    ~unique_lock() FO_TSA_RELEASE()
    {
        if (_owns) {
            _mutex->unlock();
        }
    }

    void lock() FO_TSA_ACQUIRE()
    {
        _mutex->lock();
        _owns = true;
    }
    void unlock() FO_TSA_RELEASE()
    {
        _mutex->unlock();
        _owns = false;
    }

private:
    T* _mutex;
    bool _owns {true};
};

// Reusable worker threads for asynchronous work.
//
// Two separate pools:
//
//   - `run_thread` pool — unbounded, grows on demand, workers never exit during process lifetime. For long-lived
//     loops and tasks that previously used an owned `std::thread` plus a `.join()` at destruction. The pool is
//     created lazily on first `run_thread` call.
//
//   - `run_async` pool — capped at `std::thread::hardware_concurrency()` workers, grows on demand up to that cap.
//     For potentially-many short asynchronous tasks where unbounded growth could swamp the host. Created lazily on
//     first `run_async` call.
//
// Workers in either pool only exit at process teardown — so the rpmalloc per-thread heap a task allocated on stays
// alive and adoptable, and cross-thread frees of those allocations get processed normally on the next allocation
// instead of piling up as orphan-heap deferred frees.

// Sets / queries the name of the calling thread. The name is surfaced to Tracy, the OS thread list, and to log lines
// that capture the `[ThreadName]` tag. The pool's `worker_loop` sets the name inline for each picked-up task; the rest
// of the engine just calls these as plain free functions.
extern void set_this_thread_name(const string& name) noexcept;
extern auto get_this_thread_name() noexcept -> const std::string&;

// Move-only completion handle for a task submitted to `run_thread`. Mirrors the slim subset of `std::thread` the engine
// actually uses (`join` / `joinable` / `detach` / `get_id`) without exposing the `std::future` implementation detail to
// callers, so storage sites can declare a `fo::thread` member instead of leaking the future type.
//
// Unlike `std::thread`, the destructor does NOT call `std::terminate` when the handle is still joinable — it silently
// releases the handle, leaving the pool worker to finish its body. This matches the engine's fire-and-forget usage
// (Panic watchdog, CurlRequest) where the caller explicitly wants the task to outlive the handle. Use `join()` (or
// store the handle in a member that outlives the task) when you need join-style semantics.
class thread
{
public:
    thread() noexcept = default;
    thread(thread&&) noexcept = default;
    thread(const thread&) = delete;
    auto operator=(thread&&) noexcept -> thread& = default;
    auto operator=(const thread&) -> thread& = delete;
    ~thread() noexcept = default;

    [[nodiscard]] auto joinable() const noexcept -> bool { return _future.valid(); }
    [[nodiscard]] auto get_id() const noexcept -> std::thread::id { return _runningThreadId ? _runningThreadId->load(std::memory_order_acquire) : std::thread::id {}; }

    void join();
    void detach() noexcept;

private:
    friend auto run_thread(string_view, function<void()>) -> thread;

    thread(std::future<void> fut, std::shared_ptr<std::atomic<std::thread::id>> running_thread_id) noexcept :
        _future {std::move(fut)},
        _runningThreadId {std::move(running_thread_id)}
    {
    }

    std::future<void> _future {};
    std::shared_ptr<std::atomic<std::thread::id>> _runningThreadId {};
};

// Submit `task` to the unbounded run-thread pool. Returns a `fo::thread` handle that can be `.join()`-ed or discarded
// (fire-and-forget). Use this for any task that runs to completion — short-lived fire-and-forget, or long-lived loop
// bodies (network backends, database committer, WorkerPool entry, etc.). The pool grows on demand: every submit either
// reuses an idle worker or spawns a new one. Workers never exit during process lifetime, so the rpmalloc heap a task
// allocated on stays adoptable across the worker's idle reuse.
[[nodiscard]] extern auto run_thread(string_view task_name, function<void()> task) -> thread;

// Launch mode for `run_async`. There are three meaningful modes, exposed as the named constants below — pass one of
// them to `run_async(mode, ...)`.
struct async_launch_mode
{
    bool use_async {}; // submit to the bounded async pool
    bool use_deferred {}; // allow synchronous execution (lazy-deferred, or inline fallback when the pool is saturated)
};

// Submit to the async pool; the caller never runs the task inline (it waits in the FIFO queue if the pool is at cap).
inline constexpr async_launch_mode launch_async_only {.use_async = true, .use_deferred = false};
// Run synchronously on the calling thread when the future is queried — like std::async(std::launch::deferred, ...).
inline constexpr async_launch_mode launch_deferred_only {.use_async = false, .use_deferred = true};
// Try the pool first; if every worker is busy and the pool is at cap, run the task synchronously inline right now.
inline constexpr async_launch_mode launch_async_and_deferred {.use_async = true, .use_deferred = true};

// Submit primitives backing the templated `run_async` wrapper below (which must live in the header to handle arbitrary
// callable types). `submit_async` always enqueues onto the bounded async pool; `try_submit_async` instead returns false
// when every worker is busy and the pool is at cap, letting `run_async` fall back to inline execution.
extern void submit_async(string_view task_name, std::function<void()> task);
extern auto try_submit_async(string_view task_name, std::function<void()> task) -> bool;

// Runs `task` and returns a `std::future` for its result. Drop-in replacement for `std::async(mode, ...)` with the
// launch-mode semantics described on the `launch_*` constants above. Exceptions raised by the task propagate through
// `future.get()`. The async pool is intentionally bounded so a burst of short async tasks can't grow the process
// thread count past the host's real parallelism; for long-lived loops use `run_thread` instead.
template<typename Func>
[[nodiscard]] auto run_async(async_launch_mode mode, string_view task_name, Func&& task) -> std::future<std::invoke_result_t<std::decay_t<Func>>>
{
    using ResultType = std::invoke_result_t<std::decay_t<Func>>;

    // Pure deferred: skip the pool entirely. std::async(std::launch::deferred, ...) returns a future whose .get()
    // invokes the task on the calling thread — lazy synchronous, exactly what launch_deferred_only specifies.
    if (!mode.use_async) {
        return std::async(std::launch::deferred, std::forward<Func>(task));
    }

    auto packaged = std::make_shared<std::packaged_task<ResultType()>>(std::forward<Func>(task));
    auto future = packaged->get_future();

    if (mode.use_deferred) {
        // launch_async_and_deferred — try the pool, fall back to inline synchronous execution if the pool is
        // saturated. The caller asked for either mode, so running sync is in-spec and avoids letting the FIFO queue
        // grow unbounded. packaged_task::operator() captures the task's exception into the future, so the caller
        // still observes failure through future.get().
        if (try_submit_async(task_name, [packaged] { (*packaged)(); })) {
            return future;
        }

        (*packaged)();
        return future;
    }

    // launch_async_only — must run on a pool worker. If every worker is busy and the cap is reached, submit_async
    // queues the task; a worker picks it up when it frees. The caller's thread returns immediately with the future.
    submit_async(task_name, [packaged] { (*packaged)(); });
    return future;
}

// Convenience overload for callers that always want async execution with no deferred fallback.
template<typename Func>
[[nodiscard]] auto run_async(string_view task_name, Func&& task) -> std::future<std::invoke_result_t<std::decay_t<Func>>>
{
    return run_async(launch_async_only, task_name, std::forward<Func>(task));
}

FO_END_NAMESPACE
