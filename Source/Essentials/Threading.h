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
#include "MemorySystem.h"
#include "SmartPointers.h"

#include <future>

// FO_TSA_* maps to Clang capability attributes and is a no-op on other compilers.
// Use the annotated fo synchronization types because the platform STL types are not capabilities
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

// std-compatible synchronization primitives annotated for Clang Thread Safety Analysis.
// Guarded state must use these wrappers because the platform STL types are not capabilities

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

// Exclusive lock for the short critical sections that noexcept accessors run: it owns no OS resource, so unlike
// `mutex` it has no acquisition failure to report
class FO_TSA_CAPABILITY("atomic_mutex") atomic_mutex
{
public:
    atomic_mutex() = default;
    atomic_mutex(const atomic_mutex&) = delete;
    atomic_mutex(atomic_mutex&&) noexcept = delete;
    auto operator=(const atomic_mutex&) -> atomic_mutex& = delete;
    auto operator=(atomic_mutex&&) noexcept -> atomic_mutex& = delete;
    ~atomic_mutex() = default;

    void lock() noexcept FO_TSA_ACQUIRE()
    {
        uint32_t free_state = STATE_FREE;

        if (_state.compare_exchange_strong(free_state, STATE_HELD, std::memory_order_acquire, std::memory_order_relaxed)) {
            return;
        }

        // Every sleeper publishes STATE_CONTENDED before parking, so an unlock reading it back knows a wake is owed
        while (_state.exchange(STATE_CONTENDED, std::memory_order_acquire) != STATE_FREE) {
            _state.wait(STATE_CONTENDED, std::memory_order_relaxed);
        }
    }

    void unlock() noexcept FO_TSA_RELEASE()
    {
        if (_state.exchange(STATE_FREE, std::memory_order_release) == STATE_CONTENDED) {
            _state.notify_one();
        }
    }

private:
    static constexpr uint32_t STATE_FREE = 0;
    static constexpr uint32_t STATE_HELD = 1;
    static constexpr uint32_t STATE_CONTENDED = 2;

    std::atomic<uint32_t> _state {};
};

// Exclusive RAII guard for mutex or shared_mutex (drop-in for std::scoped_lock / std::lock_guard)
template<typename T>
class FO_TSA_SCOPED_CAPABILITY scoped_lock
{
public:
    explicit scoped_lock(T& mtx) FO_TSA_ACQUIRE(mtx) :
        _mutex {&mtx}
    {
        _mutex->lock();
    }
    scoped_lock(const scoped_lock&) = delete;
    scoped_lock(scoped_lock&&) noexcept = delete;
    auto operator=(const scoped_lock&) -> scoped_lock& = delete;
    auto operator=(scoped_lock&&) noexcept -> scoped_lock& = delete;
    ~scoped_lock() FO_TSA_RELEASE() { _mutex->unlock(); }

private:
    ptr<T> _mutex;
};

// Shared (reader) RAII guard for shared_mutex (drop-in for std::shared_lock)
template<typename T>
class FO_TSA_SCOPED_CAPABILITY shared_lock
{
public:
    explicit shared_lock(T& mtx) FO_TSA_ACQUIRE_SHARED(mtx) :
        _mutex {&mtx}
    {
        _mutex->lock_shared();
    }
    shared_lock(const shared_lock&) = delete;
    shared_lock(shared_lock&&) noexcept = delete;
    auto operator=(const shared_lock&) -> shared_lock& = delete;
    auto operator=(shared_lock&&) noexcept -> shared_lock& = delete;
    ~shared_lock() FO_TSA_RELEASE_GENERIC() { _mutex->unlock_shared(); }

private:
    ptr<T> _mutex;
};

// Movable-semantics exclusive lock with manual lock/unlock (drop-in for std::unique_lock). Works with
// std::condition_variable_any, whose wait() drives lock()/unlock() on the held lock
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
    ptr<T> _mutex;
    bool _owns {true};
};

// run_thread uses an unbounded pool for long-lived jobs; run_async caps short jobs at hardware concurrency.
// Pool workers live until process teardown so rpmalloc thread heaps remain adoptable

// Set or query the calling thread's Tracy, OS, and log name
extern void set_this_thread_name(const string& name) noexcept;
extern auto get_this_thread_name() noexcept -> string_view;

// Move-only run_thread completion handle exposing join, detach, and get_id without the future type.
// Destruction detaches rather than terminating; call join when completion is required
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

    thread(std::future<void> fut, shared_ptr<std::atomic<std::thread::id>> running_thread_id) noexcept :
        _future {std::move(fut)},
        _runningThreadId {std::move(running_thread_id)}
    {
    }

    std::future<void> _future {};
    shared_ptr<std::atomic<std::thread::id>> _runningThreadId {};
};

// Submit to the unbounded run-thread pool and return a joinable or discardable handle.
// Workers persist for process lifetime so rpmalloc thread heaps remain adoptable
[[nodiscard]] extern auto run_thread(string_view task_name, function<void()> task) -> thread;

// Launch mode for `run_async`. There are three meaningful modes, exposed as the named constants below — pass one of
// them to `run_async(mode, ...)`
struct async_launch_mode
{
    bool use_async {}; // submit to the bounded async pool
    bool use_deferred {}; // allow synchronous execution (lazy-deferred, or inline fallback when the pool is saturated)
};

// Submit to the async pool; the caller never runs the task inline (it waits in the FIFO queue if the pool is at cap)
inline constexpr async_launch_mode launch_async_only {.use_async = true, .use_deferred = false};
// Run synchronously on the calling thread when the future is queried — like std::async(std::launch::deferred, ...)
inline constexpr async_launch_mode launch_deferred_only {.use_async = false, .use_deferred = true};
// Try the pool first; if every worker is busy and the pool is at cap, run the task synchronously inline right now
inline constexpr async_launch_mode launch_async_and_deferred {.use_async = true, .use_deferred = true};

// submit_async always queues; try_submit_async returns false at a fully busy cap for inline fallback
extern void submit_async(string_view task_name, function<void()> task);
extern auto try_submit_async(string_view task_name, function<void()> task) -> bool;

// Run under the selected launch mode and propagate task exceptions through future.get().
// The bounded pool is for short jobs; use run_thread for long-lived loops
template<typename Func>
[[nodiscard]] auto run_async(async_launch_mode mode, string_view task_name, Func&& task) -> std::future<std::invoke_result_t<std::decay_t<Func>>>
{
    using ResultType = std::invoke_result_t<std::decay_t<Func>>;

    // Pure deferred: skip the pool entirely. std::async(std::launch::deferred, ...) returns a future whose .get()
    // invokes the task on the calling thread — lazy synchronous, exactly what launch_deferred_only specifies
    if (!mode.use_async) {
        return std::async(std::launch::deferred, std::forward<Func>(task));
    }

    auto packaged = SafeAlloc::MakeShared<std::packaged_task<ResultType()>>(std::forward<Func>(task));
    auto future = packaged->get_future();

    if (mode.use_deferred) {
        // Mixed launch mode falls back inline when the pool is saturated, avoiding an unbounded queue
        // packaged_task preserves exceptions for future.get()
        if (try_submit_async(task_name, [packaged]() mutable { (*packaged)(); })) {
            return future;
        }

        (*packaged)();
        return future;
    }

    // launch_async_only — must run on a pool worker. If every worker is busy and the cap is reached, submit_async
    // queues the task; a worker picks it up when it frees. The caller's thread returns immediately with the future
    submit_async(task_name, [packaged]() mutable { (*packaged)(); });
    return future;
}

// Convenience overload for callers that always want async execution with no deferred fallback
template<typename Func>
[[nodiscard]] auto run_async(string_view task_name, Func&& task) -> std::future<std::invoke_result_t<std::decay_t<Func>>>
{
    return run_async(launch_async_only, task_name, std::forward<Func>(task));
}

FO_END_NAMESPACE
