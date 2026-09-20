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

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "Client.h"
#include "Test_BakerHelpers.h"
#include "WorkScheduler.h"

FO_BEGIN_NAMESPACE

// The scheduler is the whole runtime half of client multithreading: the same call site runs serial or parallel by
// configuration alone, so the contract these cover is that both answers are the same answer

TEST_CASE("WorkSchedulerWorkerCount")
{
    SECTION("ZeroStaysSerial")
    {
        CHECK(WorkScheduler::ResolveWorkerCount(0) == 0);
    }

    SECTION("PositiveIsTakenAsWritten")
    {
        CHECK(WorkScheduler::ResolveWorkerCount(1) == 1);
        CHECK(WorkScheduler::ResolveWorkerCount(7) == 7);
        CHECK(WorkScheduler::ResolveWorkerCount(WorkScheduler::MAX_WORKER_THREADS) == WorkScheduler::MAX_WORKER_THREADS);
    }

    SECTION("AutoAsksTheMachine")
    {
        int32_t resolved = WorkScheduler::ResolveWorkerCount(WorkScheduler::AUTO_WORKER_THREADS);

        CHECK(resolved >= 1);
        CHECK(resolved <= WorkScheduler::MAX_WORKER_THREADS);
    }

    SECTION("OutOfRangeIsRefusedRatherThanDowngraded")
    {
        CHECK_THROWS(WorkScheduler::ResolveWorkerCount(-2));
        CHECK_THROWS(WorkScheduler::ResolveWorkerCount(WorkScheduler::MAX_WORKER_THREADS + 1));
    }
}

TEST_CASE("WorkSchedulerSerialMode")
{
    SECTION("NoWorkersStartedAndNothingIsParallel")
    {
        WorkScheduler scheduler {"test-serial", 0};

        CHECK(scheduler.GetWorkerCount() == 0);
        CHECK(!scheduler.IsParallel());
        CHECK(!scheduler.ShouldRunParallel(1000, 1));
        CHECK(scheduler.GetDiagnostics().ParallelBatches == 0);
    }

    SECTION("BatchStillRunsEveryItemInOrder")
    {
        WorkScheduler scheduler {"test-serial", 0};
        vector<size_t> visited;

        scheduler.RunBatch("items", 5, 1, [&visited](size_t index) { visited.emplace_back(index); });

        REQUIRE(visited.size() == 5);
        CHECK(visited == vector<size_t> {0, 1, 2, 3, 4});
        CHECK(scheduler.GetDiagnostics().ParallelBatches == 0);
    }

    SECTION("ExceptionPropagatesFromTheItem")
    {
        WorkScheduler scheduler {"test-serial", 0};

        CHECK_THROWS_AS(scheduler.RunBatch("items", 4, 1,
                            [](size_t index) {
                                if (index == 2) {
                                    throw GenericException("Item failed", index);
                                }
                            }),
            GenericException);
    }
}

TEST_CASE("WorkSchedulerParallelMode")
{
    SECTION("EveryItemRunsExactlyOnce")
    {
        WorkScheduler scheduler {"test-parallel", 3};
        constexpr size_t item_count = 5000;
        vector<std::atomic<int32_t>> counters(item_count);

        scheduler.RunBatch("items", item_count, 1, [&counters](size_t index) { counters[index].fetch_add(1, std::memory_order_relaxed); });

        for (size_t i = 0; i < item_count; i++) {
            CHECK(counters[i].load() == 1);
        }

        WorkScheduler::Diagnostics diagnostics = scheduler.GetDiagnostics();

        CHECK(diagnostics.WorkerCount == 3);
        CHECK(diagnostics.ParallelBatches == 1);
        CHECK(diagnostics.ParallelItems == item_count);
        CHECK(diagnostics.ParallelChunks == 4);
    }

    SECTION("RepeatedBatchesNeverCrossOver")
    {
        WorkScheduler scheduler {"test-parallel", 4};
        constexpr size_t item_count = 64;

        for (int32_t round = 0; round < 200; round++) {
            vector<std::atomic<int32_t>> counters(item_count);

            scheduler.RunBatch("items", item_count, 1, [&counters, round](size_t index) { counters[index].fetch_add(round + 1, std::memory_order_relaxed); });

            for (size_t i = 0; i < item_count; i++) {
                CHECK(counters[i].load() == round + 1);
            }
        }
    }

    SECTION("EmptyAndTinyBatchesAreLegal")
    {
        WorkScheduler scheduler {"test-parallel", 2};
        std::atomic<int32_t> runs {0};

        scheduler.RunBatch("items", 0, 1, [&runs](size_t) { runs.fetch_add(1, std::memory_order_relaxed); });
        CHECK(runs.load() == 0);

        scheduler.RunBatch("items", 1, 1, [&runs](size_t) { runs.fetch_add(1, std::memory_order_relaxed); });
        CHECK(runs.load() == 1);
    }

    SECTION("ChunkCountRespectsTheMinimumItemsPerChunk")
    {
        WorkScheduler scheduler {"test-parallel", 7};

        scheduler.RunBatch("items", 10, 5, [](size_t) { });

        // Ten items at five per chunk is two chunks, whatever the worker count says
        CHECK(scheduler.GetDiagnostics().ParallelChunks == 2);
    }

    SECTION("ThresholdKeepsSmallBatchesOffTheWorkers")
    {
        WorkScheduler scheduler {"test-parallel", 2};

        CHECK(!scheduler.ShouldRunParallel(3, 4));
        CHECK(scheduler.ShouldRunParallel(4, 4));
        // A single item can never be spread, whatever the caller asks for
        CHECK(!scheduler.ShouldRunParallel(1, 1));
        CHECK(scheduler.ShouldRunParallel(2, 1));
    }

    SECTION("ItemExceptionIsRethrownOnTheOwnerAndTheSchedulerStaysUsable")
    {
        WorkScheduler scheduler {"test-parallel", 3};

        CHECK_THROWS_AS(scheduler.RunBatch("items", 512, 1,
                            [](size_t index) {
                                if (index == 300) {
                                    throw GenericException("Item failed", index);
                                }
                            }),
            GenericException);

        std::atomic<int32_t> runs {0};

        scheduler.RunBatch("items", 128, 1, [&runs](size_t) { runs.fetch_add(1, std::memory_order_relaxed); });
        CHECK(runs.load() == 128);
    }

    SECTION("NestedBatchIsRefusedRatherThanDeadlocked")
    {
        WorkScheduler scheduler {"test-parallel", 2};
        std::atomic<int32_t> nested_failures {0};

        scheduler.RunBatch("outer", 4, 1, [&scheduler, &nested_failures](size_t) {
            try {
                scheduler.RunBatch("inner", 2, 1, [](size_t) { });
            }
            catch (const std::exception&) {
                nested_failures.fetch_add(1, std::memory_order_relaxed);
            }
        });

        CHECK(nested_failures.load() > 0);
    }

    SECTION("ShutdownWithIdleWorkersJoinsThemAll")
    {
        std::atomic<int32_t> runs {0};

        {
            WorkScheduler scheduler {"test-parallel", 4};

            scheduler.RunBatch("items", 256, 1, [&runs](size_t) { runs.fetch_add(1, std::memory_order_relaxed); });
        }

        CHECK(runs.load() == 256);
    }

    SECTION("TwoSchedulersOwnSeparateWorkers")
    {
        WorkScheduler first {"test-first", 2};
        WorkScheduler second {"test-second", 3};
        std::atomic<int32_t> first_runs {0};
        std::atomic<int32_t> second_runs {0};

        first.RunBatch("items", 100, 1, [&first_runs](size_t) { first_runs.fetch_add(1, std::memory_order_relaxed); });
        second.RunBatch("items", 100, 1, [&second_runs](size_t) { second_runs.fetch_add(1, std::memory_order_relaxed); });

        CHECK(first_runs.load() == 100);
        CHECK(second_runs.load() == 100);
        CHECK(first.GetDiagnostics().WorkerCount == 2);
        CHECK(second.GetDiagnostics().WorkerCount == 3);
    }

    SECTION("SerialAndParallelProduceTheSameResult")
    {
        constexpr size_t item_count = 1024;
        auto compute = [](size_t index) -> uint64_t { return numeric_cast<uint64_t>(index) * 2654435761ULL % 1000003ULL; };
        vector<uint64_t> serial_result(item_count);
        vector<uint64_t> parallel_result(item_count);

        {
            WorkScheduler scheduler {"test-serial", 0};

            scheduler.RunBatch("items", item_count, 1, [&serial_result, &compute](size_t index) { serial_result[index] = compute(index); });
        }

        {
            WorkScheduler scheduler {"test-parallel", 4};

            scheduler.RunBatch("items", item_count, 1, [&parallel_result, &compute](size_t index) { parallel_result[index] = compute(index); });
        }

        CHECK(serial_result == parallel_result);
    }
}

// A client engine with no resources beyond its metadata: enough to see which mode the setting selected and to
// run a few frames through the sprite update in both of them
static auto MakeWorkSchedulerTestSettings(int32_t worker_threads) -> GlobalSettings
{
    FO_STACK_TRACE_ENTRY();

    GlobalSettings settings(false);

    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    BakerTests::OverrideSetting(settings.Client.WorkerThreads, worker_threads);

    return settings;
}

static auto MakeWorkSchedulerTestEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("WorkScheduler");
    source->AddFile("Metadata.fometa-client", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("WorkSchedulerCompiler");
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    source->AddFile("WorkScheduler.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "WorkSchedulerScripts", {{"Scripts/WorkScheduler.fos", "void WorkSchedulerFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    FileSystem resources;
    resources.AddCustomSource(std::move(source));

    return safe_alloc::make_refcounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
}

// A sprite that records what the frame did to it and on which thread, so the two-phase update contract can be
// checked without a baked model behind it
class WorkSchedulerTestSprite final : public Sprite
{
public:
    WorkSchedulerTestSprite(ptr<SpriteManager> spr_mngr, ptr<std::atomic<int32_t>> order) :
        Sprite(spr_mngr, isize32 {1, 1}, ipos32 {}),
        _order {order}
    {
    }

    auto FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override
    {
        ignore_unused(dbuf, pos, colors);
        return 0;
    }

    auto PrepareUpdate() -> bool override
    {
        PrepareCalls++;
        PrepareOrder = _order->fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void RunPreparedUpdate() override
    {
        RunCalls++;
        RunOrder = _order->fetch_add(1, std::memory_order_relaxed);
        RunThread = std::this_thread::get_id();
    }

    auto Update() -> bool override
    {
        UpdateCalls++;
        UpdateOrder = _order->fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void Register() { StartUpdate(); }

    int32_t PrepareCalls {};
    int32_t RunCalls {};
    int32_t UpdateCalls {};
    int32_t PrepareOrder {-1};
    int32_t RunOrder {-1};
    int32_t UpdateOrder {-1};
    std::thread::id RunThread {};

private:
    ptr<std::atomic<int32_t>> _order;
};

TEST_CASE("ClientSpriteUpdatePhasesFollowTheClientMode")
{
    constexpr size_t SPRITE_COUNT = 16;

    auto run_one_frame = [](int32_t worker_threads) {
        auto settings = MakeWorkSchedulerTestSettings(worker_threads);
        auto client = MakeWorkSchedulerTestEngine(settings);
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
        std::atomic<int32_t> order {0};
        vector<shared_ptr<WorkSchedulerTestSprite>> sprites;

        for (size_t i = 0; i < SPRITE_COUNT; i++) {
            auto sprite = safe_alloc::make_shared<WorkSchedulerTestSprite>(make_ptr(&client->SprMngr), make_ptr(&order));
            sprite->Register();
            sprites.emplace_back(std::move(sprite));
        }

        REQUIRE_NOTHROW(client->MainLoop());

        return sprites;
    };

    SECTION("SerialClientRunsTheSinglePassUpdateOnly")
    {
        auto sprites = run_one_frame(0);

        for (const auto& sprite : sprites) {
            CHECK(sprite->PrepareCalls == 0);
            CHECK(sprite->RunCalls == 0);
            CHECK(sprite->UpdateCalls == 1);
        }
    }

    SECTION("ParallelClientPreparesEverySpriteBeforeAnyEvaluationAndUpdatesAfterAll")
    {
        auto sprites = run_one_frame(3);

        int32_t last_prepare = -1;
        int32_t first_run = std::numeric_limits<int32_t>::max();
        int32_t last_run = -1;
        int32_t first_update = std::numeric_limits<int32_t>::max();

        for (const auto& sprite : sprites) {
            CHECK(sprite->PrepareCalls == 1);
            CHECK(sprite->RunCalls == 1);
            CHECK(sprite->UpdateCalls == 1);

            last_prepare = std::max(last_prepare, sprite->PrepareOrder);
            first_run = std::min(first_run, sprite->RunOrder);
            last_run = std::max(last_run, sprite->RunOrder);
            first_update = std::min(first_update, sprite->UpdateOrder);
        }

        // The batch boundary is what the worker safety argument rests on: nothing is evaluated until every sprite
        // has settled its inputs on the owner, and nothing is finished until the whole batch has drained
        CHECK(last_prepare < first_run);
        CHECK(last_run < first_update);
    }
}

TEST_CASE("ClientWorkerThreadsFollowTheSetting")
{
    SECTION("ZeroKeepsTheClientSerial")
    {
        auto settings = MakeWorkSchedulerTestSettings(0);
        auto client = MakeWorkSchedulerTestEngine(settings);
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

        CHECK(client->WorkSched.GetWorkerCount() == 0);
        CHECK(!client->WorkSched.IsParallel());
        CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == 0);
    }

    SECTION("APositiveCountStartsThatManyWorkers")
    {
        auto settings = MakeWorkSchedulerTestSettings(2);
        auto client = MakeWorkSchedulerTestEngine(settings);
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

        CHECK(client->WorkSched.GetWorkerCount() == 2);
        CHECK(client->WorkSched.IsParallel());
    }

    SECTION("AFrameRunsInBothModes")
    {
        auto run_frames = [](int32_t worker_threads) {
            auto settings = MakeWorkSchedulerTestSettings(worker_threads);
            auto client = MakeWorkSchedulerTestEngine(settings);
            auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

            for (int32_t frame = 0; frame < 4; frame++) {
                REQUIRE_NOTHROW(client->MainLoop());
            }
        };

        run_frames(0);
        run_frames(3);
    }
}

FO_END_NAMESPACE
