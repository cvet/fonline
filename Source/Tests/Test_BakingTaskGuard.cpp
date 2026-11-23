//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
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

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "BakingTaskGuard.h"

FO_BEGIN_NAMESPACE

template<bool Shared>
static void CheckBakingTaskUnwindBarrier()
{
    std::promise<void> worker_started;
    std::promise<void> release_worker;
    std::promise<void> unwind_started;
    std::promise<void> owner_returned;
    auto started = worker_started.get_future();
    auto release = release_worker.get_future().share();
    auto unwinding = unwind_started.get_future();
    auto returned = owner_returned.get_future();
    vector<std::future<void>> tasks;
    std::shared_future<void> shared;
    std::atomic<bool> worker_finished {};
    std::exception_ptr owner_error;
    bool released = false;
    thread owner;

    auto cleanup = scope_exit([&]() noexcept {
        if (!released) {
            release_worker.set_value();
        }

        if (owner.joinable()) {
            owner.join();
        }

        for (auto& task : tasks) {
            if (task.valid()) {
                task.wait();
            }
        }

        if (shared.valid()) {
            shared.wait();
        }
    });

    owner = run_thread("BakingTaskGuardOwner", [&] {
        auto signal_return = scope_exit([&]() noexcept { owner_returned.set_value(); });

        try {
            auto& guarded = [&]() -> auto& {
                if constexpr (Shared) {
                    return shared;
                }
                else {
                    return tasks;
                }
            }();
            BakingTaskGuard guard {guarded};
            auto signal_unwind = scope_exit([&]() noexcept { unwind_started.set_value(); });

            if constexpr (!Shared) {
                tasks.emplace_back(run_async(launch_deferred_only, "BakingTaskGuardFailure", [] {
                    throw GenericException("Baking task failed");
                }));
            }

            auto pending = run_async(launch_async_only, "BakingTaskGuardPending", [&] {
                worker_started.set_value();
                release.wait();
                worker_finished.store(true, std::memory_order_release);
            });

            if constexpr (Shared) {
                shared = pending.share();
                throw GenericException("Baking owner failed");
            }
            else {
                tasks.emplace_back(std::move(pending));
                tasks.front().get();
            }
        }
        catch (const std::exception&) {
            owner_error = std::current_exception();
        }
    });

    REQUIRE(started.wait_for(std::chrono::seconds {5}) == std::future_status::ready);
    REQUIRE(unwinding.wait_for(std::chrono::seconds {5}) == std::future_status::ready);
    CHECK(returned.wait_for(std::chrono::milliseconds {100}) == std::future_status::timeout);

    release_worker.set_value();
    released = true;
    REQUIRE(returned.wait_for(std::chrono::seconds {5}) == std::future_status::ready);
    owner.join();

    for (auto& task : tasks) {
        if (task.valid()) {
            task.wait();
        }
    }

    if (shared.valid()) {
        shared.wait();
    }

    CHECK(worker_finished.load(std::memory_order_acquire));
    REQUIRE(owner_error != nullptr);
    CHECK_THROWS_AS(std::rethrow_exception(owner_error), GenericException);
}

TEST_CASE("Baking task guard joins pending tasks after an earlier get fails")
{
    CheckBakingTaskUnwindBarrier<false>();
}

TEST_CASE("Baking task guard joins a shared task while its owner unwinds")
{
    CheckBakingTaskUnwindBarrier<true>();
}

TEST_CASE("Baking task guard preserves ready results and skips invalid states")
{
    std::promise<int32_t> ready;
    ready.set_value(42);
    vector<std::future<int32_t>> tasks;
    tasks.emplace_back();
    tasks.emplace_back(ready.get_future());

    {
        BakingTaskGuard guard {tasks};
    }

    CHECK_FALSE(tasks.front().valid());
    REQUIRE(tasks.back().valid());
    CHECK(tasks.back().get() == 42);

    {
        BakingTaskGuard guard {tasks};
    }

    std::promise<int32_t> single_ready;
    single_ready.set_value(17);
    auto single = single_ready.get_future();

    {
        BakingTaskGuard guard {single};
    }

    REQUIRE(single.valid());
    CHECK(single.get() == 17);

    {
        BakingTaskGuard guard {single};
    }

    std::shared_future<int32_t> invalid_shared;

    {
        BakingTaskGuard guard {invalid_shared};
    }

    CHECK_FALSE(invalid_shared.valid());
}

TEST_CASE("Baking task guard leaves task errors for get")
{
    auto failed = run_async(launch_async_only, "BakingTaskGuardException", []() -> int32_t {
        throw GenericException("Baking result failed");
    });

    {
        BakingTaskGuard guard {failed};
    }

    REQUIRE(failed.valid());
    CHECK_THROWS_AS(failed.get(), GenericException);
    CHECK_FALSE(failed.valid());

    auto shared = run_async(launch_async_only, "BakingTaskGuardSharedException", []() -> int32_t {
        throw GenericException("Shared baking result failed");
    }).share();

    {
        BakingTaskGuard guard {shared};
    }

    REQUIRE(shared.valid());
    CHECK_THROWS_AS(shared.get(), GenericException);
    CHECK_THROWS_AS(shared.get(), GenericException);
    CHECK(shared.valid());
}

TEST_CASE("Baking task guard leaves deferred tasks dormant")
{
    int32_t calls = 0;
    vector<std::future<int32_t>> tasks;
    tasks.emplace_back(run_async(launch_deferred_only, "BakingTaskGuardDeferred", [&] {
        calls++;
        return 23;
    }));
    auto shared = run_async(launch_deferred_only, "BakingTaskGuardSharedDeferred", [&] {
        calls++;
        return 29;
    }).share();

    {
        BakingTaskGuard guard {tasks};
        BakingTaskGuard shared_guard {shared};
    }

    CHECK(calls == 0);
    REQUIRE(tasks.front().valid());
    REQUIRE(shared.valid());
    CHECK(tasks.front().get() == 23);
    CHECK(calls == 1);
    CHECK(shared.get() == 29);
    CHECK(shared.get() == 29);
    CHECK(calls == 2);
}

FO_END_NAMESPACE
