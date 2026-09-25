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

// The rule is stated against limits of the test own, so it holds whatever values a project tunes its settings to
static constexpr WorkScheduler::WorkerCountLimits TEST_WORKER_LIMITS {.MaxWorkers = 8, .MaxMobileWorkers = 2, .HeadroomMinCores = 4};

TEST_CASE("WorkSchedulerWorkerCountHeuristic")
{
    auto choose = [](int32_t logical_cores, bool threads_supported = true, bool mobile_cpu = false, WorkScheduler::WorkerCountLimits limits = TEST_WORKER_LIMITS) { return WorkScheduler::ChooseWorkerCount({.LogicalCores = logical_cores, .ThreadsSupported = threads_supported, .MobileCpu = mobile_cpu}, limits).WorkerCount; };

    SECTION("ABuildWithoutThreadsStaysSerial")
    {
        CHECK(choose(16, false) == 0);
        CHECK(choose(16, false, true) == 0);
    }

    SECTION("AnUnknownCoreCountStaysSerial")
    {
        CHECK(choose(0) == 0);
        CHECK(choose(-1) == 0);
    }

    SECTION("TheApplicationThreadKeepsItsCore")
    {
        CHECK(choose(1) == 0);
        CHECK(choose(2) == 1);
        CHECK(choose(3) == 2);
    }

    SECTION("FromTheHeadroomThresholdOneMoreStaysFree")
    {
        CHECK(choose(TEST_WORKER_LIMITS.HeadroomMinCores) == TEST_WORKER_LIMITS.HeadroomMinCores - 2);
        CHECK(choose(6) == 4);
        CHECK(choose(8) == 6);

        CHECK(choose(3, true, false, {.MaxWorkers = 8, .MaxMobileWorkers = 2, .HeadroomMinCores = 0}) == 1);
        CHECK(choose(2, true, false, {.MaxWorkers = 8, .MaxMobileWorkers = 2, .HeadroomMinCores = 0}) == 0);
        CHECK(choose(8, true, false, {.MaxWorkers = 8, .MaxMobileWorkers = 2, .HeadroomMinCores = 16}) == 7);
    }

    SECTION("ADesktopIsCappedAtTheUsefulMaximum")
    {
        CHECK(choose(TEST_WORKER_LIMITS.MaxWorkers + 2) == TEST_WORKER_LIMITS.MaxWorkers);
        CHECK(choose(TEST_WORKER_LIMITS.MaxWorkers + 3) == TEST_WORKER_LIMITS.MaxWorkers);
        CHECK(choose(128) == TEST_WORKER_LIMITS.MaxWorkers);
        CHECK(choose(128, true, false, {.MaxWorkers = 3, .MaxMobileWorkers = 2, .HeadroomMinCores = 4}) == 3);
        CHECK(choose(128, true, false, {.MaxWorkers = WorkScheduler::MAX_WORKER_THREADS, .MaxMobileWorkers = 2, .HeadroomMinCores = 4}) == WorkScheduler::MAX_WORKER_THREADS);
    }

    SECTION("AMobileCpuIsCappedLower")
    {
        CHECK(choose(8, true, true) == TEST_WORKER_LIMITS.MaxMobileWorkers);
        CHECK(choose(2, true, true) == 1);
        CHECK(choose(1, true, true) == 0);
    }

    SECTION("TheMobileCapNeverLiftsTheGeneralOne")
    {
        CHECK(choose(128, true, true, {.MaxWorkers = 4, .MaxMobileWorkers = 16, .HeadroomMinCores = 4}) == 4);
        CHECK(choose(128, true, false, {.MaxWorkers = 4, .MaxMobileWorkers = 16, .HeadroomMinCores = 4}) == 4);
    }

    SECTION("AZeroCapKeepsThatMachineSerial")
    {
        CHECK(choose(16, true, false, {.MaxWorkers = 0, .MaxMobileWorkers = 2, .HeadroomMinCores = 4}) == 0);
        CHECK(choose(16, true, true, {.MaxWorkers = 0, .MaxMobileWorkers = 2, .HeadroomMinCores = 4}) == 0);
        CHECK(choose(16, true, true, {.MaxWorkers = 8, .MaxMobileWorkers = 0, .HeadroomMinCores = 4}) == 0);
        CHECK(choose(16, true, false, {.MaxWorkers = 8, .MaxMobileWorkers = 0, .HeadroomMinCores = 4}) == 8);
    }

    SECTION("ALimitOutsideTheSupportedRangeIsRefused")
    {
        WorkScheduler::WorkerCountInputs inputs {.LogicalCores = 8, .ThreadsSupported = true};

        CHECK_THROWS(WorkScheduler::ChooseWorkerCount(inputs, {.MaxWorkers = -1, .MaxMobileWorkers = 2, .HeadroomMinCores = 4}));
        CHECK_THROWS(WorkScheduler::ChooseWorkerCount(inputs, {.MaxWorkers = WorkScheduler::MAX_WORKER_THREADS + 1, .MaxMobileWorkers = 2, .HeadroomMinCores = 4}));
        CHECK_THROWS(WorkScheduler::ChooseWorkerCount(inputs, {.MaxWorkers = 8, .MaxMobileWorkers = -1, .HeadroomMinCores = 4}));
        CHECK_THROWS(WorkScheduler::ChooseWorkerCount(inputs, {.MaxWorkers = 8, .MaxMobileWorkers = WorkScheduler::MAX_WORKER_THREADS + 1, .HeadroomMinCores = 4}));
        CHECK_THROWS(WorkScheduler::ChooseWorkerCount(inputs, {.MaxWorkers = 8, .MaxMobileWorkers = 2, .HeadroomMinCores = -1}));

        // A bad cap is a configuration error everywhere, not only on the machines where it would change the answer
        CHECK_THROWS(WorkScheduler::ChooseWorkerCount({.LogicalCores = 8, .ThreadsSupported = false}, {.MaxWorkers = -1, .MaxMobileWorkers = 2, .HeadroomMinCores = 4}));
    }

    SECTION("MoreCoresNeverMeanFewerWorkersAndTheOwnerAlwaysKeepsACore")
    {
        for (bool mobile_cpu : {false, true}) {
            int32_t previous = 0;

            for (int32_t logical_cores = 0; logical_cores <= 256; logical_cores++) {
                CAPTURE(mobile_cpu, logical_cores);
                int32_t workers = choose(logical_cores, true, mobile_cpu);

                CHECK(workers >= previous);
                CHECK(workers <= std::max(logical_cores - 1, 0));
                CHECK(workers <= WorkScheduler::MAX_WORKER_THREADS);
                previous = workers;
            }
        }
    }

    SECTION("EveryAnswerNamesWhatLimitedIt")
    {
        for (int32_t logical_cores : {-1, 0, 1, 2, 4, 8, 64}) {
            for (bool mobile_cpu : {false, true}) {
                CAPTURE(logical_cores, mobile_cpu);
                CHECK(!WorkScheduler::ChooseWorkerCount({.LogicalCores = logical_cores, .ThreadsSupported = true, .MobileCpu = mobile_cpu}, TEST_WORKER_LIMITS).Reason.empty());
            }
        }

        CHECK(!WorkScheduler::ChooseWorkerCount({.LogicalCores = 8, .ThreadsSupported = false}, TEST_WORKER_LIMITS).Reason.empty());
    }
}

TEST_CASE("WorkSchedulerReadsThisMachine")
{
    WorkScheduler::WorkerCountInputs inputs = WorkScheduler::ReadWorkerCountInputs();

    CHECK(inputs.LogicalCores == numeric_cast<int32_t>(std::thread::hardware_concurrency()));
    CHECK(inputs.ThreadsSupported == !FO_WEB);
    CHECK(inputs.MobileCpu == (FO_ANDROID || FO_IOS));
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
static auto MakeWorkSchedulerTestSettings(bool multithreading, optional<int32_t> max_workers = std::nullopt) -> GlobalSettings
{
    GlobalSettings settings(false);

    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    BakerTests::OverrideSetting(settings.Client.Multithreading, multithreading);

    if (max_workers.has_value()) {
        BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, max_workers.value());
    }

    return settings;
}

// The count the rule gives this host under these settings, so a test knows whether parallel mode can happen here
// and what a client built from the same settings must start
static auto GetThisMachineWorkerCount(const GlobalSettings& settings) -> int32_t
{
    WorkScheduler::WorkerCountLimits limits {
        .MaxWorkers = settings.Client.MultithreadingMaxWorkers,
        .MaxMobileWorkers = settings.Client.MultithreadingMaxMobileWorkers,
        .HeadroomMinCores = settings.Client.MultithreadingHeadroomMinCores,
    };

    return WorkScheduler::ChooseWorkerCount(WorkScheduler::ReadWorkerCountInputs(), limits).WorkerCount;
}

static auto MakeWorkSchedulerTestEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
{
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

    auto run_one_frame = [](bool multithreading) {
        auto settings = MakeWorkSchedulerTestSettings(multithreading);
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
        auto sprites = run_one_frame(false);

        for (const auto& sprite : sprites) {
            CHECK(sprite->PrepareCalls == 0);
            CHECK(sprite->RunCalls == 0);
            CHECK(sprite->UpdateCalls == 1);
        }
    }

    SECTION("ParallelClientPreparesEverySpriteBeforeAnyEvaluationAndUpdatesAfterAll")
    {
        if (GetThisMachineWorkerCount(MakeWorkSchedulerTestSettings(true)) == 0) {
            SKIP("This host leaves no core for a client worker, so the client stays serial with multithreading on");
        }

        auto sprites = run_one_frame(true);

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

TEST_CASE("ClientMultithreadingFollowsTheSetting")
{
    SECTION("OffKeepsTheClientSerial")
    {
        auto settings = MakeWorkSchedulerTestSettings(false);
        auto client = MakeWorkSchedulerTestEngine(settings);
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

        CHECK(client->WorkSched.GetWorkerCount() == 0);
        CHECK(!client->WorkSched.IsParallel());
        CHECK(client->WorkSched.GetDiagnostics().ParallelBatches == 0);
    }

    SECTION("OnStartsTheWorkersTheMachineWarrants")
    {
        auto settings = MakeWorkSchedulerTestSettings(true);
        auto client = MakeWorkSchedulerTestEngine(settings);
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
        int32_t machine_workers = GetThisMachineWorkerCount(settings);

        CHECK(client->WorkSched.GetWorkerCount() == machine_workers);
        CHECK(client->WorkSched.IsParallel() == (machine_workers > 0));
    }

    SECTION("TheCapSettingsReachTheRule")
    {
        // A cap below what this host would otherwise get proves the client reads the setting rather than a constant
        for (int32_t max_workers : {0, 1}) {
            CAPTURE(max_workers);
            auto settings = MakeWorkSchedulerTestSettings(true, max_workers);
            auto client = MakeWorkSchedulerTestEngine(settings);
            auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

            CHECK(client->WorkSched.GetWorkerCount() == GetThisMachineWorkerCount(settings));
            CHECK(client->WorkSched.GetWorkerCount() <= max_workers);
        }
    }

    SECTION("AFrameRunsInBothModes")
    {
        auto run_frames = [](bool multithreading) {
            auto settings = MakeWorkSchedulerTestSettings(multithreading);
            auto client = MakeWorkSchedulerTestEngine(settings);
            auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

            for (int32_t frame = 0; frame < 4; frame++) {
                REQUIRE_NOTHROW(client->MainLoop());
            }
        };

        run_frames(false);
        run_frames(true);
    }
}

FO_END_NAMESPACE
