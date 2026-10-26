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

#include "Common.h"

#include "Test_ServerEngineHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ServerEngineTests;

class BoundaryArgAccessor final : public DataAccessor
{
public:
    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override { return 0; }
};

#if FO_ANGELSCRIPT_SCRIPTING
// Forward the real script resource, installing a native module initializer on its starter thread
class StartupProbeDataSource final : public DataSource
{
public:
    StartupProbeDataSource(ptr<const DataSource> source, function<void()> before_read) :
        _source {source},
        _beforeRead {std::move(before_read)}
    {
    }

    auto IsDiskDir() const -> bool override { return false; }
    auto GetPackName() const -> string_view override { return "StartupProbe"; }
    auto IsFileExists(string_view path) const -> bool override { return path == "ServerEngineTest.fos-bin-server" && _source->IsFileExists(path); }
    auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override { return IsFileExists(path) && _source->GetFileInfo(path, size, write_time); }
    auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override
    {
        auto names = _source->GetFileNames(dir, recursive, ext);
        std::erase_if(names, [](const string& path) { return path != "ServerEngineTest.fos-bin-server"; });
        return names;
    }
    auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override
    {
        if (!IsFileExists(path)) {
            return nullptr;
        }
        std::call_once(_readOnce, _beforeRead);
        return _source->OpenFile(path, size, write_time);
    }

private:
    ptr<const DataSource> _source;
    function<void()> _beforeRead;
    mutable std::once_flag _readOnce {};
};

TEST_CASE("ServerStartupKeepsItsPauseAndNamedHoldThroughWorldStart")
{
    auto settings = MakeServerTestSettings();
    auto resources = MakeServerTestResources();
    auto source = resources.ReadFileHeader("ServerEngineTest.fos-bin-server").GetDataSource();
    std::mutex publication_mutex;
    std::condition_variable publication;
    nptr<ServerEngine> engine;
    ScriptFuncDesc module;
    array<synctime, 3> phase_times {};
    bool startup_paused = false;
    bool startup_still_paused = false;
    bool hold_acquired = false;
    module.Call = [&](FuncCallData&) {
        startup_paused = engine->IsSimulationPaused() && engine->GameTime.IsPaused();
        phase_times[0] = engine->GameTime.GetSynchronizedTime();
        bool acquired = engine->HoldSimulation("temporary-startup");
        bool released = engine->ReleaseSimulationHold("temporary-startup");
        startup_still_paused = acquired && released && engine->IsSimulationPaused();
        hold_acquired = engine->HoldSimulation("session-startup");
        engine->OnInit.Subscribe(Entity::EventCallbackData {.Callback = [&](FuncCallData&) {
            phase_times[1] = engine->GameTime.GetSynchronizedTime();
            return Entity::EventResult::ContinueChain;
        }});
        engine->OnStart.Subscribe(Entity::EventCallbackData {.Callback = [&](FuncCallData&) {
            phase_times[2] = engine->GameTime.GetSynchronizedTime();
            return Entity::EventResult::ContinueChain;
        }});
    };
    resources.AddCustomSource(safe_alloc::make_unique<StartupProbeDataSource>(source, [&] {
        std::unique_lock lock {publication_mutex};
        publication.wait(lock, [&] { return !!engine; });
        engine->AddInitFunc(ScriptFunc<void> {&module}, 0);
    }));
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources));
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    {
        std::lock_guard lock {publication_mutex};
        engine = server.get();
    }
    publication.notify_one();
    REQUIRE(WaitForServerStart(server).empty());
    CHECK(startup_paused);
    CHECK(startup_still_paused);
    CHECK(hold_acquired);
    CHECK(server->IsSimulationPaused());
    for (synctime time : phase_times) {
        CHECK(time == synctime {std::chrono::milliseconds {1}});
    }
    coarse_sleep(std::chrono::milliseconds {50});
    CHECK(server->GameTime.GetSynchronizedTime() == phase_times[0]);
    CHECK(server->ReleaseSimulationHold("session-startup"));
    for (uint32_t attempt = 0; attempt < 200 && server->GameTime.GetSynchronizedTime() == phase_times[0]; attempt++) {
        coarse_sleep(std::chrono::milliseconds {5});
    }
    CHECK(server->GameTime.GetSynchronizedTime() > phase_times[0]);
}

TEST_CASE("ServerShutdownDoesNotDeadlockWithLateStartupSimulationControl")
{
    auto settings = MakeServerTestSettings();
    auto resources = MakeServerTestResources();
    auto source = resources.ReadFileHeader("ServerEngineTest.fos-bin-server").GetDataSource();
    std::mutex gate_mutex;
    std::condition_variable gate;
    nptr<ServerEngine> engine;
    bool entered = false;
    bool release = false;
    bool rejected = false;
    ScriptFuncDesc module;
    module.Call = [&](FuncCallData&) {
        {
            std::unique_lock lock {gate_mutex};
            entered = true;
            gate.notify_all();
            gate.wait(lock, [&] { return release; });
        }
        try {
            (void)engine->HoldSimulation("late-startup");
        }
        catch (const std::exception&) {
            rejected = true;
        }
    };
    resources.AddCustomSource(safe_alloc::make_unique<StartupProbeDataSource>(source, [&] {
        std::unique_lock lock {gate_mutex};
        gate.wait(lock, [&] { return !!engine; });
        engine->AddInitFunc(ScriptFunc<void> {&module}, 0);
    }));
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources));
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    {
        std::lock_guard lock {gate_mutex};
        engine = server.get();
    }
    gate.notify_all();
    bool reached_module;
    {
        std::unique_lock lock {gate_mutex};
        reached_module = gate.wait_for(lock, std::chrono::seconds {10}, [&] { return entered; });
        if (!reached_module) {
            release = true;
        }
    }
    gate.notify_all();
    REQUIRE(reached_module);
    CHECK(server->GetSessionState() == ServerSessionState::Starting);
    std::atomic<bool> finished {};
    std::exception_ptr shutdown_error;
    auto shutdown_thread = run_thread("startup-shutdown-test", [&] {
        try {
            server->Shutdown();
        }
        catch (...) {
            shutdown_error = std::current_exception();
        }
        finished = true;
    });
    for (uint32_t attempt = 0; attempt < 2000 && !server->IsShutdownInProgress(); attempt++) {
        coarse_sleep(std::chrono::milliseconds {5});
    }
    bool shutdown_started = server->IsShutdownInProgress();
    auto stopping_state = server->GetSessionState();
    {
        std::lock_guard lock {gate_mutex};
        release = true;
    }
    gate.notify_all();
    shutdown_thread.join(); // The focused test process has an external deadline for the deadlock control
    CHECK(shutdown_started);
    CHECK(stopping_state == ServerSessionState::Stopping);
    CHECK(finished.load());
    CHECK_FALSE(shutdown_error);
    CHECK(rejected);
    CHECK(server->GetSessionState() == ServerSessionState::Stopped);
}
#endif

TEST_CASE("ServerResourcesFollowPackagedSetting")
{
    if (IsPackaged()) {
        SKIP("Baked directory mounting is only used by unpackaged test binaries");
    }

    string temp_dir = MakeTempServerResourceDir("baked_entries");
    bool removed_before = fs::remove_dir_tree(temp_dir);
    ignore_unused(removed_before);

    auto cleanup = scope_exit([&temp_dir]() noexcept { fs::remove_dir_tree(temp_dir); });

    string baked_dir = strex(temp_dir).combine_path("Baked").str();
    string packaged_dir = strex(temp_dir).combine_path("Packaged").str();

    REQUIRE(fs::write_file(strex(baked_dir).combine_path("ServerPack/payload.txt").str(), string_view {"baked-server"}));
    REQUIRE(fs::write_file(strex(baked_dir).combine_path("ClientPack/client-only.txt").str(), string_view {"client"}));
    REQUIRE(fs::write_file(strex(packaged_dir).combine_path("ServerPack/payload.txt").str(), string_view {"packaged-server"}));

    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, baked_dir);
    BakerTests::OverrideSetting(settings.Baking.ServerResources, packaged_dir);
    auto pack_config = ConfigFile("[ResourcePack]\nName = ServerPack\nServerOnly = True\n");
    settings.ApplyConfigFile(pack_config, "");

    auto resources = GetServerResources(settings);

    CHECK(resources.ReadFileText("payload.txt") == "baked-server");
    CHECK(resources.IsFileExists("payload.txt"));
    CHECK_FALSE(resources.IsFileExists("client-only.txt"));

    BakerTests::OverrideSetting(settings.Common.Packaged, true);

    FileSystem packaged_resources = GetServerResources(settings);

    CHECK(packaged_resources.ReadFileText("payload.txt") == "packaged-server");
    CHECK(packaged_resources.IsFileExists("payload.txt"));
    CHECK_FALSE(packaged_resources.IsFileExists("client-only.txt"));
}

TEST_CASE("ServerEngineConnectionAcceptPredicates")
{
    // Pure accept-path decision logic for the network population cap + per-source rate guard
    // (launch-network-load-test-and-caps Track 1). Static + side-effect-free, so no live server is needed

    SECTION("PopulationCapAcceptsUnderLimitAndRejectsAtOrOver")
    {
        // 0 means unlimited on both axes
        CHECK(ServerEngine::ShouldAcceptConnection(1000, 1000, 0, 0));

        // Connection cap
        CHECK(ServerEngine::ShouldAcceptConnection(9, 5, 10, 0));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(10, 5, 10, 0));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(11, 5, 10, 0));

        // Player cap
        CHECK(ServerEngine::ShouldAcceptConnection(100, 4, 0, 5));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(100, 5, 0, 5));

        // Either ceiling rejects
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(10, 1, 10, 5));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(1, 5, 10, 5));
    }

    SECTION("RateGuardCountsPerSecondBucketAndResetsOnRollover")
    {
        ServerEngine::ConnRateState state {};

        // 0 disables the guard (always accept) and does not touch the bucket
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 0));

        // Up to the limit within the same second is accepted; beyond it is rejected
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK_FALSE(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK_FALSE(ServerEngine::EvaluateConnectionRate(state, 100, 3));

        // A new second rolls the window over and accepts again
        CHECK(ServerEngine::EvaluateConnectionRate(state, 101, 3));
        CHECK(ServerEngine::EvaluateConnectionRate(state, 101, 3));
    }
}

TEST_CASE("ServerEngineRandomGeneratorRoundTrip")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    random_generator::state_data captured_state = server->CaptureRandomState();
    CHECK_FALSE(std::ranges::all_of(captured_state, [](uint64_t part) { return part == 0; }));

    vector<int32_t> expected_sequence;
    for (size_t i = 0; i < 32; i++) {
        expected_sequence.emplace_back(server->Random(-100000, 100000));
    }

    // The generator stays behind the engine's own lock and its state travels as bare words, so a replay is a restore
    server->RestoreRandomState(captured_state);
    for (int32_t expected_value : expected_sequence) {
        CHECK(server->Random(-100000, 100000) == expected_value);
    }

    // The same words survive a round trip through the generator type itself
    random_generator rebuilt_generator {uint64_t {1}};
    rebuilt_generator.restore_state(captured_state);
    server->RestoreRandomState(rebuilt_generator.capture_state());
    for (int32_t expected_value : expected_sequence) {
        CHECK(server->Random(-100000, 100000) == expected_value);
    }

    // A rejected restore is validated before the state is touched, so the engine keeps drawing where it stood
    random_generator::state_data state_before_invalid_restore = server->CaptureRandomState();
    CHECK_THROWS(server->RestoreRandomState(random_generator::state_data {}));
    CHECK(server->CaptureRandomState() == state_before_invalid_restore);
}

template<typename Server>
static void CheckServerSimulationOwnership(Server& server)
{
    if constexpr (requires {
                      server.PauseSimulation();
                      server.ResumeSimulation();
                      server.HoldSimulation("owner");
                      server.ReleaseSimulationHold("owner");
                      server.IsSimulationPaused();
                  }) {
        SECTION("ManualPauseDoesNotReleaseNamedOwners")
        {
            REQUIRE(server.HoldSimulation("source"));
            CHECK_FALSE(server.HoldSimulation("source"));
            REQUIRE(server.HoldSimulation("other"));
            server.PauseSimulation();
            CHECK_THROWS(server.ResumeSimulation());
            CHECK(server.ReleaseSimulationHold("source"));
            CHECK(server.IsSimulationPaused());
            CHECK(server.ReleaseSimulationHold("other"));
            CHECK(server.IsSimulationPaused());
            server.ResumeSimulation();
            CHECK_FALSE(server.IsSimulationPaused());
            CHECK_FALSE(server.ReleaseSimulationHold("source"));
        }

        SECTION("QuiescenceAndItsFailurePreserveTheExistingPause")
        {
            REQUIRE(server.HoldSimulation("source"));
            synctime held_time = server.GameTime.GetSynchronizedTime();
            REQUIRE(server.RunInQuiescence(timespan {std::chrono::seconds {10}}, [&](const ServerQuiescenceState&) {
                CHECK(server.IsSimulationPaused());
                CHECK_FALSE(server.IsConnectionAdmissionOpen());
                CHECK(server.GameTime.GetSynchronizedTime() == held_time);
            }));
            CHECK_THROWS_AS(server.RunInQuiescence(timespan {std::chrono::seconds {10}}, [](const ServerQuiescenceState&) { throw GenericException("Intentional snapshot boundary failure"); }), GenericException);
            CHECK(server.IsSimulationPaused());
            CHECK(server.GameTime.IsPaused());
            CHECK(server.IsConnectionAdmissionOpen());
            REQUIRE(server.ReleaseSimulationHold("source"));
            CHECK_FALSE(server.GameTime.IsPaused());
        }

        SECTION("ServiceCallbacksContinueWhileSimulationTimeIsStopped")
        {
            std::atomic_bool serviced {};
            server.PauseSimulation();
            synctime held_time = server.GameTime.GetSynchronizedTime();
            server.ScheduleDelayedCallback(timespan {std::chrono::milliseconds {20}}, [&] { serviced.store(true); });

            for (uint32_t attempt = 0; attempt < 200 && !serviced.load(); attempt++) {
                coarse_sleep(std::chrono::milliseconds {5});
            }

            CHECK(serviced.load());
            CHECK(server.IsConnectionAdmissionOpen());
            CHECK(server.GameTime.GetSynchronizedTime() == held_time);
            CHECK(server.GameTime.GetFrameDeltaTime() == timespan::zero);
            CHECK(server.GetFrameDeltaTime() == timespan::zero);
            server.ResumeSimulation();
        }

        SECTION("ControlFromQuiescenceRefusesBeforeTakingItsMutex")
        {
            REQUIRE(server.RunInQuiescence(timespan {std::chrono::seconds {10}}, [&](const ServerQuiescenceState&) {
                CHECK_THROWS(server.PauseSimulation());
                CHECK_THROWS(server.ResumeSimulation());
                CHECK_THROWS(server.HoldSimulation("callback"));
                CHECK_THROWS(server.ReleaseSimulationHold("callback"));
            }));
            CHECK_FALSE(server.IsSimulationPaused());
        }

        SECTION("EmptyHoldAndUnownedResumeAreRefused")
        {
            CHECK_THROWS(server.HoldSimulation(""));
            CHECK_THROWS(server.ResumeSimulation());
            CHECK_FALSE(server.IsSimulationPaused());
        }
    }
    else {
        FAIL("ServerEngine does not support independent simulation pause owners");
    }
}

TEST_CASE("ServerEngineSimulationPauseOwners")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    CheckServerSimulationOwnership(*server);
}

TEST_CASE("ServerEngineSnapshotAcceptsNamedTimeEvents")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        auto event_func = server->FindFunc<void>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestNoop"));
        if (!event_func) {
            SKIP("This server fixture has no registered named timer callback");
        }
        (void)server->TimeEventMngr.StartTimeEvent(server.get(), std::move(event_func), timespan {std::chrono::hours {1}}, timespan {}, {});
    }

    // Memory storage has no byte representation: reaching it proves the named timer passed eligibility
    CHECK_THROWS_AS(server->CreateSnapshot(timespan {std::chrono::seconds {10}}), DataBaseException);
}

TEST_CASE("ServerEngineQuiescenceFreezesAndCleansUp")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->IsConnectionAdmissionOpen());

    refcount_nptr<Critter> critter;
    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

        hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
        critter = server->CreateCritter(critter_pid, false).hold_ref();
    }

    REQUIRE(critter);

    bool callback_invoked = false;
    REQUIRE(server->RunInQuiescence(timespan {std::chrono::seconds {10}}, [&](const ServerQuiescenceState& state) {
        callback_invoked = true;

        CHECK(server->GameTime.IsPaused());
        CHECK_FALSE(server->IsConnectionAdmissionOpen());
        CHECK(IsEntityAccessValid(critter));
        CHECK(state.SynchronizedTime == server->GameTime.GetSynchronizedTime());

        CHECK(state.RandomState == server->CaptureRandomState());

        nanotime paused_frame_time = server->GameTime.GetFrameTime();
        server->FrameAdvance();
        CHECK(server->GameTime.GetFrameTime() == paused_frame_time);
        CHECK(server->GameTime.GetFrameDeltaTime() == timespan::zero);
    }));

    CHECK(callback_invoked);
    CHECK_FALSE(server->GameTime.IsPaused());
    CHECK(server->IsConnectionAdmissionOpen());
    CHECK_FALSE(server->GetCurrentSyncContext());

    CHECK_THROWS_AS(server->RunInQuiescence(timespan {std::chrono::seconds {10}}, [](const ServerQuiescenceState&) { throw GenericException("Intentional quiescence callback failure"); }), GenericException);
    CHECK_FALSE(server->GameTime.IsPaused());
    CHECK(server->IsConnectionAdmissionOpen());
    CHECK_FALSE(server->GetCurrentSyncContext());

    REQUIRE(server->RunInQuiescence(timespan {std::chrono::seconds {10}}, [&server](const ServerQuiescenceState&) { CHECK_THROWS_AS(server->Shutdown(), VerificationException); }));
    CHECK(server->IsStarted());
    CHECK(server->IsConnectionAdmissionOpen());

    server->RunScriptContext([&server] { CHECK_THROWS_AS(server->RunInQuiescence(timespan {std::chrono::seconds {10}}, [](const ServerQuiescenceState&) { }), ServerQuiescenceException); });

    REQUIRE(server->RunInQuiescence(timespan {std::chrono::seconds {10}}, [](const ServerQuiescenceState&) { }));

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    // The external lock opens a context of its own but covers nothing, and the destroy call only retains
    // an already covered entity, so the caller establishes the cover
    refcount_ptr<Critter> held_critter = critter.take_not_null();
    server->RequireCurrentSyncContext()->SyncEntity(held_critter);
    server->CrMngr.DestroyCritter(held_critter);
}

TEST_CASE("ServerEngineSnapshotEligibilityRejectsRuntimeOnlyState")
{
    auto snapshot_root = std::filesystem::temp_directory_path() / std::format("fo_engine_snapshot_blockers_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code remove_error;
    std::filesystem::remove_all(snapshot_root, remove_error);

    auto cleanup_snapshot = scope_exit([snapshot_root]() noexcept {
        safe_call([snapshot_root] {
            std::error_code ignored;
            std::filesystem::remove_all(snapshot_root, ignored);
        });
    });

    auto settings = MakeServerTestSettings();
#if FO_DATABASE_SQLITE
    BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}", fs::path_to_string(snapshot_root)).str());
#endif

    ScriptFuncDesc event_desc {};
    auto server = MakeServerEngine(settings);
    event_desc = BakerTests::MakeNoopCallbackDesc(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestNoop"));

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    auto has_blocker = [](const ServerSnapshotCaptureResult& result, ServerSnapshotBlockerKind kind) { return std::ranges::any_of(result.Blockers, [kind](const ServerSnapshotBlocker& blocker) { return blocker.Kind == kind && blocker.Count != 0; }); };

    SECTION("DelayedCallback")
    {
        server->ScheduleDelayedCallback(timespan {std::chrono::hours {1}}, [] { });

        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});

        CHECK(result.ReachedQuiescence);
        CHECK_FALSE(result.State.has_value());
        CHECK(result.Payload.empty());
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::DelayedCallbacks));
        CHECK_FALSE(fs::exists(fs::path_to_string(snapshot_root / "delayed" / "Storage.sqlite")));
    }

    SECTION("DelegateTimeEvent")
    {
        event_desc.DelegateObj = 1;
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            ScriptFunc<void> event_func {&event_desc};
            REQUIRE(event_func);
            ignore_unused(server->TimeEventMngr.StartTimeEvent(server.get(), std::move(event_func), timespan {std::chrono::hours {1}}, timespan::zero, {}));
        }

        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});

        CHECK(result.ReachedQuiescence);
        CHECK_FALSE(result.State.has_value());
        CHECK(result.Payload.empty());
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::DelegateBoundTimeEvents));
    }

#if FO_ANGELSCRIPT_SCRIPTING
    SECTION("ExecutingTimeEvent")
    {
        bool completed = GENERATE(false, true);
        server->PauseSimulation();
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            auto callback = server->FindFunc<void>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestNoop"));
            REQUIRE(callback);
            uint32_t id = server->TimeEventMngr.StartTimeEvent(server.get(), std::move(callback), timespan {std::chrono::hours {1}}, {}, {});

            auto events = server->GetTimeEvents();
            REQUIRE(events);
            auto it = std::ranges::find_if(*events, [id](const auto& candidate) { return candidate->Id == id; });
            REQUIRE(it != events->end());
            auto event = *it;

            event->Execution = safe_alloc::make_shared<TimeEventExecution>();
            event->Execution->Context = safe_alloc::make_refcounted<TimeEventContext>(id, timespan {}, vector<any_t> {});
            event->Execution->Completed = completed;
        }

        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
        REQUIRE(result.ReachedQuiescence);
        CHECK_FALSE(result.State.has_value());
        CHECK(result.Payload.empty());
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::TimeEvents));
    }
#endif

    SECTION("UnregisteredTimeEvent")
    {
        event_desc.Name = server->Hashes.to_hashed_string("ServerEngineTest::MissingTimer");
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            (void)server->TimeEventMngr.StartTimeEvent(server.get(), ScriptFunc<void> {&event_desc}, timespan {std::chrono::hours {1}}, {}, {});
        }
        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
        REQUIRE(result.ReachedQuiescence);
        CHECK_FALSE(result.State.has_value());
        CHECK(result.Payload.empty());
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::TimeEvents));
    }

    SECTION("Movement")
    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            auto critter = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false);
            critter->SetMoving(MakeServerMovementContext(SERVER_TEST_MAP_SIZE, SERVER_TEST_MOVE_START_HEX, server->GameTime.GetFrameTime()));
        }

        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});

        CHECK(result.ReachedQuiescence);
        CHECK_FALSE(result.State.has_value());
        CHECK(result.Payload.empty());
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::CritterMovements));
    }

    SECTION("PersistentPlayerMovementRequiresProjectRestoration")
    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            hstring map_pids[] = {server->Hashes.to_hashed_string("UnitTestMap")};
            auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"), map_pids);
            server->EntityMngr.MakePersistent(location, true, true);
            auto map = location->GetMapByIndex(0);
            REQUIRE(map);
            auto critter = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), true);
            server->MapMngr.AddCritterToMap(critter, map.get(), SERVER_TEST_MOVE_START_HEX, mdir {}, {});
            CHECK(critter->IsPersistent());
            critter->SetMoving(MakeServerMovementContext(SERVER_TEST_MAP_SIZE, SERVER_TEST_MOVE_START_HEX, server->GameTime.GetFrameTime()));
        }
        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
        REQUIRE(result.ReachedQuiescence);
        CHECK_FALSE(result.State.has_value());
        CHECK(result.Payload.empty());
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::CritterMovements));
    }
}

TEST_CASE("ServerEngineSnapshotEligibilityIncludesEmbeddingStateInsideQuiescence")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    size_t check_calls = 0;
    server->RegisterSnapshotEligibilityCheck([&check_calls](ptr<const ServerEngine> current_server) {
        check_calls++;
        CHECK(current_server->GameTime.IsPaused());
        CHECK(current_server->GetCurrentSyncContext());
        return vector<ServerSnapshotEligibilityIssue> {{.Identifier = "unit-test.transaction", .Count = 2}};
    });
    server->RegisterSnapshotEligibilityCheck([](ptr<const ServerEngine>) { return vector<ServerSnapshotEligibilityIssue> {{.Identifier = "unit-test.transaction", .Count = 3}}; });

    ServerSnapshotCaptureResult result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});

    REQUIRE(result.ReachedQuiescence);
    CHECK(check_calls == 1);
    CHECK_FALSE(result.State.has_value());
    CHECK(result.Payload.empty());
    REQUIRE(result.Blockers.size() == 1);
    CHECK(result.Blockers[0].Kind == ServerSnapshotBlockerKind::EmbeddingState);
    CHECK(result.Blockers[0].Identifier == "unit-test.transaction");
    CHECK(result.Blockers[0].Count == 5);
}

TEST_CASE("ServerEngineSnapshotEligibilityRejectsInvalidEmbeddingDiagnostics")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    SECTION("EmptyCallback")
    {
        CHECK_THROWS_AS(server->RegisterSnapshotEligibilityCheck({}), ServerSnapshotException);
    }

    SECTION("EmptyIdentifier")
    {
        server->RegisterSnapshotEligibilityCheck([](ptr<const ServerEngine>) { return vector<ServerSnapshotEligibilityIssue> {{.Identifier = {}, .Count = 1}}; });
        CHECK_THROWS_AS(server->CreateSnapshot(timespan {std::chrono::seconds {10}}), ServerSnapshotException);
    }

    SECTION("ZeroCount")
    {
        server->RegisterSnapshotEligibilityCheck([](ptr<const ServerEngine>) { return vector<ServerSnapshotEligibilityIssue> {{.Identifier = "unit-test.invalid", .Count = 0}}; });
        CHECK_THROWS_AS(server->CreateSnapshot(timespan {std::chrono::seconds {10}}), ServerSnapshotException);
    }
}

#if FO_DATABASE_SQLITE
TEST_CASE("ServerEngineSnapshotRoundTripsThroughFreshSQLiteSession")
{
    auto storage_root = std::filesystem::temp_directory_path() / std::format("fo_engine_server_snapshot_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    auto source_storage = storage_root / "source";
    auto restored_storage = storage_root / "restored-live";
    std::error_code remove_error;
    std::filesystem::remove_all(storage_root, remove_error);
    std::filesystem::create_directories(storage_root);

    auto cleanup_storage = scope_exit([storage_root]() noexcept {
        safe_call([storage_root] {
            std::error_code ignored;
            std::filesystem::remove_all(storage_root, ignored);
        });
    });

    ident_t saved_location_id;
    ident_t expected_next_location_id;
    vector<int32_t> expected_random_sequence;
    ServerSnapshotState captured_state;
    vector<uint8_t> captured_payload;

    {
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}", source_storage.generic_string()).str());
        auto server = MakeServerEngine(settings);

        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        {
            REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

            auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"));
            server->EntityMngr.MakePersistent(location, true, true);
            saved_location_id = location->GetId();
        }

        // The Engine hands back bytes and state; naming, versioning and publishing them is the embedder's job
        auto capture_result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
        REQUIRE(capture_result.ReachedQuiescence);
        REQUIRE(capture_result.Blockers.empty());
        REQUIRE(capture_result.State.has_value());
        REQUIRE_FALSE(capture_result.Payload.empty());
        captured_state = *capture_result.State;
        captured_payload = std::move(capture_result.Payload);

        REQUIRE(captured_payload.size() > 16);
        CHECK(string_view {reinterpret_cast<const char*>(captured_payload.data()), 15} == string_view {"SQLite format 3"});

        for (int32_t i = 0; i < 8; i++) {
            expected_random_sequence.emplace_back(server->Random(-1000000, 1000000));
        }

        {
            REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            expected_next_location_id = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"))->GetId();
        }

        server->Shutdown();
    }

    std::filesystem::create_directories(restored_storage);

    {
        // A state that disagrees with its payload must fail before gameplay hooks run
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}", restored_storage.generic_string()).str());
        auto mismatched_state = captured_state;
        mismatched_state.LastEntityId = ident_t {captured_state.LastEntityId.underlying_value() + 1};
        auto server = MakeServerEngine(settings, ServerSnapshotRestore {mismatched_state, captured_payload});

        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        CHECK_FALSE(startup_error.empty());
        CHECK(server->IsStartingError());
        CHECK_FALSE(server->IsStarted());
        REQUIRE_NOTHROW(server->Shutdown());
    }

    {
        // An empty payload is rejected at construction rather than producing an empty world
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}", (storage_root / "empty-payload").generic_string()).str());
        CHECK_THROWS_AS(MakeServerEngine(settings, ServerSnapshotRestore {captured_state, {}}), ServerSnapshotException);
    }

    {
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}", restored_storage.generic_string()).str());
        auto server = MakeServerEngine(settings, ServerSnapshotRestore {captured_state, captured_payload});

        auto shutdown = scope_exit([&server]() noexcept {
            safe_call([&server] {
                if (server->IsStarted()) {
                    server->Shutdown();
                }
            });
        });

        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

        REQUIRE(static_cast<bool>(server->EntityMngr.GetLocation(saved_location_id)));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetLocation(expected_next_location_id)));

        for (int32_t expected : expected_random_sequence) {
            CHECK(server->Random(-1000000, 1000000) == expected);
        }

        auto next_location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"));
        CHECK(next_location->GetId() == expected_next_location_id);
    }
}
#endif

#if FO_DATABASE_SQLITE && FO_ANGELSCRIPT_SCRIPTING
TEST_CASE("ServerEngineSnapshotRestoresTimerAndLeasedMovementExecution")
{
    string storage_root = MakeTempServerResourceDir("session_restore");
    auto cleanup = scope_exit([&storage_root]() noexcept { (void)fs::remove_dir_tree(storage_root); });
    ServerSnapshotState state;
    vector<uint8_t> payload;
    mpos expected_end;
    mpos captured_hex;
    ident_t critter_id;

    {
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}/source", storage_root).str());
        auto server = MakeServerEngine(settings);
        auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
        REQUIRE(WaitForServerStart(server).empty());
        server->PauseSimulation();
        timespan advance_time;
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            hstring map_pids[] = {server->Hashes.to_hashed_string("UnitTestMap")};
            auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"), map_pids);
            server->EntityMngr.MakePersistent(location, true, true);
            auto map = location->GetMapByIndex(0);
            REQUIRE(map);
            auto critter = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false);
            server->MapMngr.AddCritterToMap(critter, map.get(), SERVER_TEST_MOVE_START_HEX, mdir {}, {});
            server->EntityMngr.MakePersistent(critter, true, true);
            critter_id = critter->GetId();
            auto steps = SERVER_TEST_MOVE_STEPS;
            steps.push_back(hdir::West); // The final control step defines the interpreted path
            auto control_steps = SERVER_TEST_MOVE_CONTROL_STEPS;
            control_steps.insert(control_steps.begin(), 0); // A zero-length initial segment is valid
            auto moving = safe_alloc::make_refcounted<MovingContext>(SERVER_TEST_MAP_SIZE, SERVER_TEST_MOVE_SPEED, std::move(steps), std::move(control_steps), server->GameTime.GetFrameTime(), timespan {}, SERVER_TEST_MOVE_START_HEX, ipos16 {}, ipos16 {});
            expected_end = moving->GetEndHex();
            advance_time = timespan {moving->GetWholeTime().value() / 4};
            moving->SetLeaseTime(timespan {moving->GetWholeTime().value() / 2}, server->GameTime.GetFrameTime());
            server->StartCritterMoving(critter, std::move(moving), nullptr);
        }

        server->ResumeSimulation();
        coarse_sleep(advance_time.value());
        server->PauseSimulation();
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            auto critter = server->EntityMngr.GetCritter(critter_id);
            REQUIRE(critter);
            server->RequireCurrentSyncContext()->SyncEntity(critter);
            captured_hex = critter->GetHex();
            auto callback = server->FindFunc<void>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestMarkManualCall"));
            REQUIRE(callback);
            (void)server->TimeEventMngr.StartTimeEvent(server.get(), std::move(callback), timespan {std::chrono::milliseconds {200}}, {}, {});
            auto repeating = server->FindFunc<void>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestNoop"));
            REQUIRE(repeating);
            (void)server->TimeEventMngr.StartTimeEvent(server.get(), std::move(repeating), timespan {std::chrono::hours {1}}, timespan {std::chrono::nanoseconds {-1}}, {});
            auto retired = server->FindFunc<void>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestNoop"));
            REQUIRE(retired);
            uint32_t retired_id = server->TimeEventMngr.StartTimeEvent(server.get(), std::move(retired), timespan {std::chrono::hours {1}}, {}, {});
            server->TimeEventMngr.StopTimeEvent(server.get(), {}, retired_id);
        }

        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
        REQUIRE(result.ReachedQuiescence);
        REQUIRE(result.Blockers.empty());
        REQUIRE(result.State.has_value());
        state = std::move(*result.State);
        payload = std::move(result.Payload);
        REQUIRE(state.TimeEvents.size() == 2);
        CHECK(state.TimeEvents[1].RepeatDuration == timespan {std::chrono::nanoseconds {-1}});
        REQUIRE(state.CritterMovements.size() == 1);
        CHECK(state.CritterMovements[0].ElapsedTimeMs > 0.0f);
        CHECK(state.CritterMovements[0].LeaseTimeMs > 0.0f);
        CHECK(server->IsSimulationPaused());
    }

    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}/restored", storage_root).str());
    SECTION("RejectMalformedStateBeforeRestoringDatabase")
    {
        SECTION("NegativeSubMillisecondDelay")
        {
            state.TimeEvents[0].FireDelay = std::chrono::nanoseconds {-1};
        }
        SECTION("DuplicateTimerId")
        {
            state.TimeEvents.push_back(state.TimeEvents[0]);
        }
        SECTION("GlobalFunctionWithEntityOwner")
        {
            state.TimeEvents[0].EntityId = critter_id;
            state.TimeEvents[0].EntityTypeName = "Critter";
        }
        SECTION("EntityFunctionWithGlobalOwner")
        {
            state.TimeEvents[0].FuncAlternative = 4;
        }
        SECTION("UnrepresentableDelay")
        {
            state.TimeEvents[0].FireDelay = timespan {std::numeric_limits<int64_t>::max()};
        }
        SECTION("UnrepresentableRepeat")
        {
            state.TimeEvents[0].RepeatDuration = timespan {std::numeric_limits<int64_t>::max()};
        }
        SECTION("InvalidLease")
        {
            state.CritterMovements[0].LeaseTimeMs = std::numeric_limits<float32_t>::quiet_NaN();
        }
        SECTION("UnrepresentableWholeTime")
        {
            state.CritterMovements[0].WholeTimeMs = std::numeric_limits<float32_t>::max();
        }
        SECTION("UnrepresentableLeaseTime")
        {
            state.CritterMovements[0].LeaseTimeMs = std::numeric_limits<float32_t>::max();
        }
        SECTION("InvalidControlStep")
        {
            state.CritterMovements[0].ControlSteps[0] = numeric_cast<uint16_t>(state.CritterMovements[0].Steps.size() + 1);
        }
        SECTION("OldFormat")
        {
            state.FormatVersion = 1;
        }
        CHECK_THROWS_AS(MakeServerEngine(settings, ServerSnapshotRestore {state, payload}), ServerSnapshotException);
        CHECK_FALSE(fs::exists(strex("{}/restored/Storage.sqlite", storage_root).str()));
    }
    SECTION("AnUnresolvableFunctionFailsStartup")
    {
        state.TimeEvents[0].FuncName = "ServerEngineTest::MissingSnapshotFunction";
        auto server = MakeServerEngine(settings, ServerSnapshotRestore {state, payload});
        auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
        CHECK_FALSE(WaitForServerStart(server).empty());
        CHECK(server->IsStartingError());
        CHECK_FALSE(server->IsStarted());
    }
    SECTION("AChangedOwnerTypeFailsStartup")
    {
        state.TimeEvents[0].EntityTypeName = "UnknownOwnerType";
        auto server = MakeServerEngine(settings, ServerSnapshotRestore {state, payload});
        auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
        CHECK_FALSE(WaitForServerStart(server).empty());
        CHECK(server->IsStartingError());
        CHECK_FALSE(server->IsStarted());
    }
    SECTION("RestoredWorkRunsOnTheNewClockAndHonorsItsLease")
    {
        coarse_sleep(std::chrono::milliseconds {300});
        auto resources = MakeServerTestResources();
        auto source = resources.ReadFileHeader("ServerEngineTest.fos-bin-server").GetDataSource();
        std::mutex publication_mutex;
        std::condition_variable publication;
        nptr<ServerEngine> engine;
        ScriptFuncDesc module;
        module.Call = [&](FuncCallData&) { (void)engine->HoldSimulation("snapshot-inspection"); };
        resources.AddCustomSource(safe_alloc::make_unique<StartupProbeDataSource>(source, [&] {
            std::unique_lock lock {publication_mutex};
            publication.wait(lock, [&] { return !!engine; });
            engine->AddInitFunc(ScriptFunc<void> {&module}, 0);
        }));
        auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources), ServerSnapshotRestore {state, payload});
        auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
        {
            std::lock_guard lock {publication_mutex};
            engine = server.get();
        }
        publication.notify_one();
        REQUIRE(WaitForServerStart(server).empty());
        CHECK(server->IsSimulationPaused());
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            auto critter = server->EntityMngr.GetCritter(critter_id);
            REQUIRE(critter);
            server->RequireCurrentSyncContext()->SyncEntity(critter);
            REQUIRE(critter->GetMoving());
            CHECK(critter->GetHex() == captured_hex);
            CHECK(critter->GetMoving()->GetLeaseTime().to_ms<float32_t>() == state.CritterMovements[0].LeaseTimeMs);
            CHECK(server->TimeEventMngr.CountTimeEvent(server.get(), {}, state.TimeEvents[0].EventId) == 1);
            auto fresh = server->FindFunc<void>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestNoop"));
            REQUIRE(fresh);
            uint32_t fresh_id = server->TimeEventMngr.StartTimeEvent(server.get(), std::move(fresh), timespan {std::chrono::hours {1}}, {}, {});
            CHECK(fresh_id > state.LastTimeEventId);
            server->TimeEventMngr.StopTimeEvent(server.get(), {}, fresh_id);
        }
        REQUIRE(server->ReleaseSimulationHold("snapshot-inspection"));
        coarse_sleep(std::chrono::milliseconds {800});
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            auto critter = server->EntityMngr.GetCritter(critter_id);
            REQUIRE(critter);
            server->RequireCurrentSyncContext()->SyncEntity(critter);
            REQUIRE(critter->GetMoving());
            CHECK(critter->GetMoving()->GetElapsedTime().to_ms<float32_t>() <= state.CritterMovements[0].LeaseTimeMs + 1.0f);
            CHECK(critter->GetHex() != expected_end);
            auto calls = server->FindFunc<int32_t>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestGetManualCalls"));
            REQUIRE(calls);
            REQUIRE(calls.Call());
            CHECK(calls.GetResult() == 1);
            CHECK(server->TimeEventMngr.CountTimeEvent(server.get(), {}, state.TimeEvents[0].EventId) == 0);
            critter->GetMoving()->SetLeaseTime(timespan::zero, server->GameTime.GetFrameTime());
        }
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        bool locked = true;
        auto unlock = scope_exit([&server, &locked]() noexcept {
            if (locked) {
                safe_call([&server] { server->Unlock(); });
            }
        });
        REQUIRE(WaitForUnlockedServerCondition(
            server, locked,
            [&server, critter_id, expected_end] {
                auto critter = server->EntityMngr.GetCritter(critter_id);
                if (!critter) {
                    return false;
                }
                server->RequireCurrentSyncContext()->SyncEntity(critter);
                return !critter->IsMoving() && critter->GetHex() == expected_end;
            },
            std::chrono::seconds {5}));
    }
}
#endif

TEST_CASE("ServerEngineClearsTheStartingUpFlagOnceItServes")
{
    // The flag suppresses the script responsiveness report while the engine comes up. Leaving it set
    // would silence that report for the whole life of the server, and nothing else would say so
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    CHECK_FALSE(server->IsStartingUp());
}

TEST_CASE("ServerEngineStartsAndCreatesCritter")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    REQUIRE(static_cast<bool>(server->GetProtoCritter(critter_pid)));

    size_t critter_count = server->EntityMngr.GetCrittersCount();
    size_t entity_count = server->EntityMngr.GetEntitiesCount();

    auto cr = server->CreateCritter(critter_pid, false);

    ident_t cr_id = cr->GetId();
    CHECK(cr->GetProtoId() == critter_pid);
    auto registered_cr = server->EntityMngr.GetCritter(cr_id);
    REQUIRE(registered_cr);
    CHECK(registered_cr == cr);
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count + 1);
    CHECK(server->EntityMngr.GetEntitiesCount() > entity_count);

    auto player = CreateLoggedPlayer(server, "UnitTestPlayer");

    ident_t player_id = player->GetId();
    CHECK(player_id != ident_t {});
    auto registered_player = server->EntityMngr.GetPlayer(player_id);
    REQUIRE(registered_player);
    CHECK(registered_player == player);

    // Login work re-synced the context onto the player, so the critter cover must be re-established
    // by the caller before the destroy call — the callee only retains an already covered entity
    server->RequireCurrentSyncContext()->SyncEntity(cr);

    server->CrMngr.DestroyCritter(cr);

    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count);
}

TEST_CASE("ServerEngineSingleThreadedLogicRunsWithoutEntityCover")
{
    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Server.SingleThreadedLogic, true);
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    // One worker is what serializes the jobs, which is what lets the cover requirement go away
    CHECK(server->GetWorkerThreadCount() == 1);

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    auto cr = server->CreateCritter(critter_pid, false);
    ident_t cr_id = cr->GetId();

    // ServerEngineStartsAndCreatesCritter with every synchronization call removed: login re-syncs the context
    // onto the player, so in the multithreaded mode the critter below would be uncovered and every access throw
    auto player = CreateLoggedPlayer(server, "UnitTestSingleThreaded");
    CHECK(player->GetId() != ident_t {});

    CHECK(IsEntityAccessValid(cr));
    CHECK_NOTHROW(ignore_unused(cr->GetProtoId()));
    CHECK_NOTHROW(server->CrMngr.DestroyCritter(cr));
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
}

TEST_CASE("ServerEngineDelayedCallbackAndSharedPropertyLock")
{
    auto settings = MakeServerTestSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    server->LockForPropertyAccessShared();
    server->UnlockForPropertyAccessShared();

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    std::atomic_bool callback_ran {false};
    uint64_t completed_jobs = server->GetCompletedServerJobsCount();

    server->ScheduleDelayedCallback(timespan {std::chrono::milliseconds {1}}, [&callback_ran] { callback_ran.store(true, std::memory_order_release); });

    REQUIRE(WaitForUnlockedServerCondition(server.get(), locked, [&server, &callback_ran, completed_jobs] { return callback_ran.load(std::memory_order_acquire) && server->GetCompletedServerJobsCount() > completed_jobs; }));

    CHECK(callback_ran.load(std::memory_order_acquire));
    CHECK(server->GetCompletedServerJobsCount() > completed_jobs);
}

TEST_CASE("ServerEngineWritesHealthFile")
{
    string health_file_name = MakeServerHealthFileName();
    RemoveServerHealthFile(health_file_name);

    auto cleanup_health_file = scope_exit([&health_file_name]() noexcept { RemoveServerHealthFile(health_file_name); });

    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Server.WriteHealthFile, true);
    BakerTests::OverrideSetting(settings.Server.HealthFilePeriodMs, int32_t {5});

    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    string health_content;
    REQUIRE(WaitForUnlockedServerCondition(
        server.get(), locked,
        [&health_file_name, &health_content] {
            auto content = fs::read_file(health_file_name);

            if (!content.has_value() || content->find("Server uptime:") == string::npos || content->find("Connections:") == string::npos) {
                return false;
            }

            health_content = *content;
            return true;
        },
        std::chrono::milliseconds {2000}));

    CHECK(health_content.find("FOnline v0.0.0") != string::npos);
    CHECK(health_content.find("Version: 0.0.0") != string::npos);
    CHECK(health_content.find("Starting...") == string::npos);
    CHECK(health_content.find("Server uptime:") != string::npos);
    CHECK(health_content.find("Connections:") != string::npos);
}

TEST_CASE("ServerEngineShutdownIsSafeAfterStartupFailure")
{
    // Shutdown must be safe on an engine whose startup aborted before the worker pool existed; an unrecognized
    // DbStorage reproduces that state deterministically
    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Server.DbStorage, string {"UnreachableStorageForTest"});

    CheckServerStartupFailsSafely(settings);
}

#if FO_DATABASE_JSON
TEST_CASE("ServerReloadsAPersistedWorldFromDisk")
{
    // File-backed storage is what makes the restart world-load path reachable at all: the rest of the suite is
    // in-memory, so one server writes the world and a second reads it back
    auto storage_dir = std::filesystem::temp_directory_path() / std::format("fo_engine_world_reload_test_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code remove_error;
    std::filesystem::remove_all(storage_dir, remove_error);
    std::filesystem::create_directories(storage_dir);

    auto cleanup_storage = scope_exit([storage_dir]() noexcept {
        safe_call([storage_dir] {
            std::error_code ignored;
            std::filesystem::remove_all(storage_dir, ignored);
        });
    });

    string storage_option = strex("JSON {}", storage_dir.generic_string()).str();

    ident_t location_id;
    ident_t critter_id;

    {
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, storage_option);

        auto server = MakeServerEngine(settings);
        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        {
            REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

            // Runtime entities are temporary by default, so they have to be marked persistent before the
            // restart has anything to read back
            auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"));
            server->EntityMngr.MakePersistent(location, true, true);
            location_id = location->GetId();

            auto critter = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false);
            server->EntityMngr.MakePersistent(critter, true, true);
            critter_id = critter->GetId();
        }

        CHECK(server->EntityMngr.GetLocationsCount() >= 1);
        CHECK(server->EntityMngr.GetCrittersCount() >= 1);

        server->Shutdown();
    }

    {
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, storage_option);

        auto server = MakeServerEngine(settings);
        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        auto shutdown = scope_exit([&server]() noexcept {
            safe_call([&server] {
                if (server->IsStarted()) {
                    server->Shutdown();
                }
            });
        });

        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server.get_no_const()->Unlock(); }); });

        // The critter is deliberately not asserted on: it was created off-map, and a placeless critter has
        // nothing to load it from
        CHECK(server->EntityMngr.GetLocationsCount() >= 1);
        CHECK(static_cast<bool>(server->EntityMngr.GetLocation(location_id)));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetLocation(ident_t {})));
        ignore_unused(critter_id);
    }
}
TEST_CASE("ServerShutdownPreservesPersistentCustomEntityTreesAcrossRestarts")
{
    auto storage_dir = std::filesystem::temp_directory_path() / std::format("fo_custom_shutdown_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(storage_dir);

    auto cleanup_storage = scope_exit([storage_dir]() noexcept {
        safe_call([storage_dir] {
            std::error_code ignored;
            std::filesystem::remove_all(storage_dir, ignored);
        });
    });

    auto metadata = BakerTests::MakeMetadataBlob({
        {"Entity", {{"ShutdownRoot"}, {"ShutdownLeaf"}}},
        {"EntityHolder",
            {
                {"Server", "Game", "ShutdownRoot", "RootState", "Persistent"},
                {"Server", "ShutdownRoot", "ShutdownLeaf", "LeafState", "Persistent"},
                {"Server", "Critter", "ShutdownLeaf", "CritterState", "Persistent"},
            }},
        {"Property",
            {
                {"ShutdownRoot", "Server", "int32", "Marker", "Mutable", "Persistent"},
                {"ShutdownLeaf", "Server", "int32", "Marker", "Mutable", "Persistent"},
            }},
    });

    ident_t root_id;
    ident_t leaf_id;
    ident_t location_id;
    ident_t map_id;
    ident_t critter_id;
    ident_t critter_leaf_id;

    for (int32_t attempt = 0; attempt < 3; attempt++) {
        INFO(attempt);
        auto settings = MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, strex("JSON {}", storage_dir.generic_string()).str());
        auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerTestResources(ServerTestScriptMode::Default, metadata));
        auto shutdown = scope_exit([&server]() noexcept {
            safe_call([&server] {
                if (server->IsStarted()) {
                    server->Shutdown();
                }
            });
        });

        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        {
            REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            auto ctx = server->RequireCurrentSyncContext();
            ctx->LockSingleton(server->GetEntityLock());

            auto hash = [&server](string_view value) { return server->Hashes.to_hashed_string(value); };
            auto root_entry = hash("RootState");
            auto leaf_entry = hash("LeafState");
            auto critter_entry = hash("CritterState");

            auto marker_property = [&server](ptr<Entity> entity) {
                auto prop = server->GetPropertyRegistrar(entity->GetTypeName())->FindProperty("Marker");
                REQUIRE(prop);
                return prop;
            };

            if (attempt == 0) {
                auto root = server->EntityMngr.CreateCustomInnerEntity(server, root_entry, {});
                auto leaf = server->EntityMngr.CreateCustomInnerEntity(root, leaf_entry, {});
                root->GetPropertiesForEdit()->SetValue(marker_property(root), int32_t {101});
                leaf->GetPropertiesForEdit()->SetValue(marker_property(leaf), int32_t {-202});
                root_id = root->GetId();
                leaf_id = leaf->GetId();

                auto location = server->MapMngr.CreateLocation(hash("UnitTestLocation"), vector<hstring> {hash("UnitTestMap")});
                auto map = location->GetMapByIndex(0);
                REQUIRE(map);
                server->EntityMngr.MakePersistent(location, true, true);
                auto critter = server->CreateCritter(hash("UnitTestRat"), false);
                server->MapMngr.TransferToMap(critter, map, mpos {20, 20}, mdir {}, std::nullopt);
                server->EntityMngr.MakePersistent(critter, true, true);
                auto leaf_on_critter = server->EntityMngr.CreateCustomInnerEntity(critter, critter_entry, {});
                leaf_on_critter->GetPropertiesForEdit()->SetValue(marker_property(leaf_on_critter), int32_t {303});
                location_id = location->GetId();
                map_id = map->GetId();
                critter_id = critter->GetId();
                critter_leaf_id = leaf_on_critter->GetId();
            }

            auto root = server->EntityMngr.GetCustomEntity(hash("ShutdownRoot"), root_id);
            auto leaf = server->EntityMngr.GetCustomEntity(hash("ShutdownLeaf"), leaf_id);
            auto leaf_on_critter = server->EntityMngr.GetCustomEntity(hash("ShutdownLeaf"), critter_leaf_id);
            auto critter = server->EntityMngr.GetCritter(critter_id);
            auto location = server->EntityMngr.GetLocation(location_id);
            REQUIRE(root);
            REQUIRE(leaf);
            REQUIRE(leaf_on_critter);
            REQUIRE(critter);
            REQUIRE(location);
            ctx->UnlockSingleton(server->GetEntityLock());
            ctx->SyncEntity(location);
            ctx->LockSingleton(server->GetEntityLock());

            auto check_holder = [](ptr<Entity> holder, hstring entry, ident_t expected_id) {
                auto children = holder->GetInnerEntities(entry);
                REQUIRE(children);
                REQUIRE(children->size() == 1);
                CHECK(children->front()->GetId() == expected_id);
            };
            check_holder(server, root_entry, root_id);
            check_holder(root, leaf_entry, leaf_id);
            check_holder(critter, critter_entry, critter_leaf_id);

            CHECK(root->IsPersistent());
            CHECK(leaf->IsPersistent());
            CHECK(leaf_on_critter->IsPersistent());

            CHECK(root->GetCustomHolderId() == ident_t {});
            CHECK(root->GetCustomHolderEntry() == root_entry);
            CHECK(leaf->GetCustomHolderId() == root_id);
            CHECK(leaf->GetCustomHolderEntry() == leaf_entry);
            CHECK(leaf_on_critter->GetCustomHolderId() == critter_id);
            CHECK(leaf_on_critter->GetCustomHolderEntry() == critter_entry);

            CHECK(critter->GetMapId() == map_id);
            REQUIRE(location->GetMapByIndex(0));
            CHECK(location->GetMapByIndex(0)->GetId() == map_id);

            CHECK(root->GetProperties()->GetValueFast<int32_t>(marker_property(root)) == 101);
            CHECK(leaf->GetProperties()->GetValueFast<int32_t>(marker_property(leaf)) == -202);
            CHECK(leaf_on_critter->GetProperties()->GetValueFast<int32_t>(marker_property(leaf_on_critter)) == 303);
        }

        // Every pass ends through the real graceful path; the next pass reads the same files
        server->Shutdown();
    }
}
#endif

TEST_CASE("ServerEngineCustomCollectionStartupValidation")
{
    auto settings = MakeServerTestSettings();

    SECTION("RejectsMissingSeparator")
    {
        BakerTests::OverrideSetting(settings.DataBase.CustomCollections, vector<string> {"BrokenCollection"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("RejectsEmptyTrimmedCollectionName")
    {
        BakerTests::OverrideSetting(settings.DataBase.CustomCollections, vector<string> {"   : Int"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("RejectsUnknownKeyType")
    {
        BakerTests::OverrideSetting(settings.DataBase.CustomCollections, vector<string> {"BadType:Uuid"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("RejectsDuplicateCollectionName")
    {
        BakerTests::OverrideSetting(settings.DataBase.CustomCollections, vector<string> {"Duplicate:Int", "Duplicate:Str"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("AcceptsTrimmedCaseInsensitiveKeyTypes")
    {
        BakerTests::OverrideSetting(settings.DataBase.CustomCollections, vector<string> {"  TrimmedInt : int  ", "  TrimmedStr : STR  "});

        auto server = MakeServerEngine(settings);

        auto shutdown = scope_exit([&server]() noexcept {
            safe_call([&server] {
                if (server->IsStarted()) {
                    server->Shutdown();
                }
            });
        });

        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        hstring int_collection = server->Hashes.to_hashed_string("TrimmedInt");
        hstring string_collection = server->Hashes.to_hashed_string("TrimmedStr");
        ident_t int_id = ident_t {1001};
        string string_id = string {"custom:key"};

        CHECK(server->DbStorage.GetAllIntIds(int_collection).empty());
        CHECK(server->DbStorage.GetAllStringIds(string_collection).empty());

        server->DbStorage.Insert(int_collection, DataBaseKey {int_id}, MakeServerStorageDoc(10));
        server->DbStorage.Insert(string_collection, DataBaseKey {string_id}, MakeServerStorageDoc(20));
        server->DbStorage.WaitCommitChanges();

        auto int_ids = server->DbStorage.GetAllIntIds(int_collection);
        auto string_ids = server->DbStorage.GetAllStringIds(string_collection);

        REQUIRE(int_ids.size() == 1);
        CHECK(int_ids.front() == int_id);
        REQUIRE(string_ids.size() == 1);
        CHECK(string_ids.front() == string_id);

        auto int_doc = server->DbStorage.Get(int_collection, DataBaseKey {int_id});
        auto string_doc = server->DbStorage.Get(string_collection, DataBaseKey {string_id});

        REQUIRE(!int_doc.Empty());
        CHECK(int_doc["value"].AsInt64() == 10);
        REQUIRE(!string_doc.Empty());
        CHECK(string_doc["value"].AsInt64() == 20);
    }
}

TEST_CASE("ServerEngineHandlesPlayerCritterUnloadAndMissingProto")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    SECTION("PlayerControlledCritterCanBeUnloaded")
    {
        hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
        size_t critter_count = server->EntityMngr.GetCrittersCount();

        auto cr = server->CreateCritter(critter_pid, true);

        ident_t cr_id = cr->GetId();
        CHECK(cr->GetControlledByPlayer());
        auto registered_cr = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(registered_cr);
        CHECK(registered_cr == cr);

        server->UnloadCritter(cr);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
        CHECK(server->EntityMngr.GetCrittersCount() == critter_count);
    }

    SECTION("MissingProtoThrows")
    {
        hstring missing_pid = server->Hashes.to_hashed_string("MissingUnitTestCritter");

        CHECK_THROWS(server->CreateCritter(missing_pid, false));
        CHECK_THROWS(server->CreateCritter(missing_pid, true));
    }
}

TEST_CASE("ServerEngineProcessesOverdueMovementByHex")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    hstring location_pid = server->Hashes.to_hashed_string("UnitTestLocation");
    hstring map_pid = server->Hashes.to_hashed_string("UnitTestMap");

    SECTION("CompletesWholeRoute")
    {
        vector<hstring> map_pids {map_pid};
        auto loc = server->MapMngr.CreateLocation(location_pid, map_pids);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    // The wait loop unlocked/relocked the server, so this context starts empty — the caller
                    // establishes the destroy cover itself
                    server->RequireCurrentSyncContext()->SyncEntity(loc);
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(critter_pid, false);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);

        auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        timespan overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime().to_ms<float64_t>()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr.get(), moving, nullptr);
        REQUIRE(cr->GetMovingContext());

        REQUIRE(WaitForUnlockedServerCondition(server, locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::Success);
        CHECK(cr->GetHex() == path_hexes.back());

        auto finished_moving = cr->GetMovingContext();
        REQUIRE(static_cast<bool>(finished_moving));
        CHECK(finished_moving->GetCompleteReason() == MovingState::Success);
    }

    SECTION("CompletesWholeRouteForDeadCritter")
    {
        vector<hstring> map_pids {map_pid};
        auto loc = server->MapMngr.CreateLocation(location_pid, map_pids);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    // The wait loop unlocked/relocked the server, so this context starts empty — the caller
                    // establishes the destroy cover itself
                    server->RequireCurrentSyncContext()->SyncEntity(loc);
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(critter_pid, false);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);
        cr->SetCondition(CritterCondition::Dead);

        auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        timespan overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime().to_ms<float64_t>()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr.get(), moving, nullptr);
        REQUIRE(cr->GetMovingContext());

        REQUIRE(WaitForUnlockedServerCondition(server, locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::Success);
        CHECK(cr->GetHex() == path_hexes.back());
        CHECK(cr->IsDead());

        auto finished_moving = cr->GetMovingContext();
        REQUIRE(static_cast<bool>(finished_moving));
        CHECK(finished_moving->GetCompleteReason() == MovingState::Success);
    }

    SECTION("StopsAtFirstBlockedHexWithoutTeleport")
    {
        vector<hstring> map_pids {map_pid};
        auto loc = server->MapMngr.CreateLocation(location_pid, map_pids);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    // The wait loop unlocked/relocked the server, so this context starts empty — the caller
                    // establishes the destroy cover itself
                    server->RequireCurrentSyncContext()->SyncEntity(loc);
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(critter_pid, false);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);

        auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        auto expected_stop_hex = path_hexes[1];
        auto blocked_hex = path_hexes[2];

        map->SetHexManualBlock(blocked_hex, true, true);
        auto unblock_hex = scope_exit([server, map, blocked_hex]() noexcept {
            safe_call([server, map, blocked_hex] {
                if (!map->IsDestroyed()) {
                    auto ctx = server->RequireCurrentSyncContext();
                    ctx->SyncEntity(map);
                    map.get_no_const()->SetHexManualBlock(blocked_hex, false, false);
                }
            });
        });

        timespan overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime().to_ms<float64_t>()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr.get(), moving, nullptr);
        REQUIRE(cr->GetMovingContext());

        REQUIRE(WaitForUnlockedServerCondition(server, locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::HexBusy);
        CHECK(cr->GetHex() == expected_stop_hex);

        auto finished_moving = cr->GetMovingContext();
        REQUIRE(static_cast<bool>(finished_moving));
        CHECK(finished_moving->GetCompleteReason() == MovingState::HexBusy);
        CHECK(finished_moving->GetPreBlockHex() == expected_stop_hex);
        CHECK(finished_moving->GetBlockHex() == blocked_hex);
    }
}

TEST_CASE("ServerEngineSyncContextEntityCover")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    hstring location_pid = server->Hashes.to_hashed_string("UnitTestLocation");
    hstring map_pid = server->Hashes.to_hashed_string("UnitTestMap");
    hstring item_pid = server->Hashes.to_hashed_string("TestItem");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(static_cast<bool>(map));
    ptr<ServerEntity> map_entity = map;

    auto cr_a = server->CreateCritter(critter_pid, false);
    auto cr_b = server->CreateCritter(critter_pid, false);
    auto cr_c = server->CreateCritter(critter_pid, false);
    auto nested_item = server->ItemMngr.CreateItem(item_pid, nullptr).hold_ref();
    ptr<ServerEntity> cr_a_entity = cr_a;
    ptr<ServerEntity> cr_b_entity = cr_b;
    ptr<ServerEntity> nested_item_entity = nested_item.as_ptr();

    server->MapMngr.TransferToMap(cr_a, map, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map, mpos {12, 12}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_c, map, mpos {14, 14}, mdir {}, std::nullopt);
    server->CrMngr.AddItemToCritter(cr_a, nested_item, false);

    // Parent wiring (Critter._parent = Map) must be established for the cover logic
    auto cr_a_parent = cr_a->GetParentRaw();
    auto cr_b_parent = cr_b->GetParentRaw();
    auto cr_c_parent = cr_c->GetParentRaw();
    REQUIRE(cr_a_parent);
    REQUIRE(cr_b_parent);
    REQUIRE(cr_c_parent);
    REQUIRE(cr_a_parent == map_entity);
    REQUIRE(cr_b_parent == map_entity);
    REQUIRE(cr_c_parent == map_entity);
    REQUIRE(nested_item->GetParentRaw() == cr_a);

    server->Unlock();
    locked = false;

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        safe_call([&ctx] {
            ctx.Release();
            ctx.Deactivate();
        });
    });

    // Two layers: ValidateAccess reports whether the entity's own lock is held, pinning the exact cover, while
    // IsEntityAccessValid is the production gate that also accepts an ancestor

    // Strict model: an empty context is NOT exempt — no held lock means no entity access. Only a
    // null entity short-circuits to granted
    CHECK(ctx.IsEmpty());
    CHECK_FALSE(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(nullptr));

    // Single critter: only its own lock is held; access is granted to it but not siblings or the map
    {
        vector<ptr<ServerEntity>> one {cr_a};
        ctx.SyncEntities(one);
        CHECK_FALSE(ctx.IsEmpty());
        CHECK(ctx.ValidateAccess(cr_a)); // cr_a's own lock is held
        CHECK_FALSE(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(cr_a));

        {
            bool sync_diag_seen = false;
            logging::set_callback("entity_access_valid_diagnose_test", [&sync_diag_seen](logging::type, string_view message, nptr<const stack_trace::catched_data>) {
                if (message.find("SyncDiag access-without-sync") != string_view::npos) {
                    sync_diag_seen = true;
                }
            });
            auto clear_log_callback = scope_exit([]() noexcept { logging::set_callback("entity_access_valid_diagnose_test", {}); });

            CHECK_FALSE(IsEntityAccessValid(cr_b, false));
            CHECK_FALSE(sync_diag_seen);

            CHECK_FALSE(IsEntityAccessValid(cr_b));
            CHECK(sync_diag_seen);
        }

        CHECK_FALSE(IsEntityAccessValid(cr_b)); // sibling: no lock in its chain is held
        CHECK_FALSE(IsEntityAccessValid(cr_c));
        CHECK_FALSE(IsEntityAccessValid(map));

        // The binding-level gate must throw on an uncovered entity rather than report false, because an unsynced
        // access is a rejected error and never silently allowed
        CHECK_NOTHROW(cr_a->ValidateAccess());
        CHECK_THROWS_AS(cr_b->ValidateAccess(), ScriptException);
        CHECK_THROWS_AS(map->ValidateAccess(), ScriptException);
    }
    ctx.Release();
    CHECK(ctx.IsEmpty());

    // Siblings are locked individually with no escalation onto the shared parent, so the map is only marked and
    // an unrequested third sibling stays uncovered
    {
        vector<ptr<ServerEntity>> both {cr_a, cr_b};
        ctx.SyncEntities(both);
        CHECK_FALSE(ctx.ValidateAccess(map)); // map's own lock is NOT held (only marked)
        CHECK(ctx.ValidateAccess(cr_a)); // each requested sibling's own lock is held
        CHECK(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(cr_a)); // covered by its own held lock
        CHECK(IsEntityAccessValid(cr_b));
        CHECK_FALSE(IsEntityAccessValid(cr_c)); // unrequested sibling: not covered (map only marked, not held)
        CHECK_FALSE(IsEntityAccessValid(map)); // holding descendants does not cover the ancestor
    }
    ctx.Release();

    // Explicit {critter, its own map}: BOTH locks are kept (no parent-cover reduction), so the
    // critter survives a reparent that a parent-only cover would strand
    {
        vector<ptr<ServerEntity>> pair {cr_a, map};
        ctx.SyncEntities(pair);
        CHECK(ctx.ValidateAccess(cr_a)); // own lock kept
        CHECK(ctx.ValidateAccess(map)); // map lock kept
        CHECK(IsEntityAccessValid(cr_a));
        CHECK(IsEntityAccessValid(cr_c)); // map lock covers siblings
    }
    ctx.Release();

    // Ordinary EnsureEntitySynced only retains the own lock of an entity that is already covered. It never turns an
    // empty context or an unrelated held entity into implicit synchronization
    CHECK_THROWS_WITH(ctx.EnsureEntitySynced(cr_a), Catch::Matchers::ContainsSubstring("neither locked nor covered"));
    CHECK(ctx.IsEmpty());

    ctx.LockSingleton(server->GetEntityLock());
    CHECK_THROWS_WITH(ctx.EnsureEntitySynced(cr_a), Catch::Matchers::ContainsSubstring("neither locked nor covered"));
    CHECK_FALSE(ctx.ValidateAccess(cr_a));
    CHECK_FALSE(IsEntityAccessValid(cr_a));
    CHECK_FALSE(IsEntityAccessValid(cr_b));
    ctx.Release();

    ctx.SyncEntity(cr_a);
    ctx.EnsureEntitySynced(cr_a);
    CHECK(ctx.ValidateAccess(cr_a));
    ctx.EnsureEntitySynced(cr_a);
    CHECK(ctx.ValidateAccess(cr_a));
    CHECK_THROWS_WITH(ctx.EnsureEntitySynced(cr_b), Catch::Matchers::ContainsSubstring("neither locked nor covered"));
    CHECK_FALSE(ctx.ValidateAccess(cr_b));
    CHECK(IsEntityAccessValid(cr_a));
    CHECK_FALSE(IsEntityAccessValid(cr_b));
    ctx.Release();

    // The runtime counterpart of the structural-loop contract: one acquisition per newly visited child is not a
    // retry of the same target
    ctx.SyncEntity(map);
    ctx.EnsureEntitySynced(cr_a);
    ctx.EnsureEntitySynced(cr_b);
    CHECK(ctx.ValidateAccess(map));
    CHECK(ctx.ValidateAccess(cr_a));
    CHECK(ctx.ValidateAccess(cr_b));
    CHECK(IsEntityAccessValid(cr_b));
    auto retained_children = ctx.GetHeldEntities();
    CHECK(std::ranges::find(retained_children, cr_a_entity) != retained_children.end());
    CHECK(std::ranges::find(retained_children, cr_b_entity) != retained_children.end());
    ctx.Release();

    // A successful multi-op retention commits both the nested target's exclusive lock and the
    // intermediate ancestor mark, and Release balances both counters
    ctx.SyncEntity(map);
    auto successful_item_lock = nested_item->GetEntityLock();
    auto successful_cr_lock = cr_a->GetEntityLock();
    REQUIRE(successful_item_lock);
    REQUIRE(successful_cr_lock);
    REQUIRE(successful_item_lock != successful_cr_lock);
    CHECK(successful_item_lock->GetExclusiveRecursionForCurrentThread() == 0);
    CHECK(successful_cr_lock->GetDescendantHoldCountForCurrentThread() == 0);
    REQUIRE_NOTHROW(ctx.EnsureEntitySynced(nested_item));
    CHECK(successful_item_lock->GetExclusiveRecursionForCurrentThread() == 1);
    CHECK(successful_cr_lock->GetDescendantHoldCountForCurrentThread() == 1);
    CHECK(ctx.ValidateAccess(map));
    CHECK(ctx.ValidateAccess(nested_item));
    ctx.Release();
    CHECK(successful_item_lock->GetExclusiveRecursionForCurrentThread() == 0);
    CHECK(successful_cr_lock->GetDescendantHoldCountForCurrentThread() == 0);

    // Taking the child's raw lock bypasses the intention marks to simulate contention that cannot occur under a
    // held ancestor, so retention retries as if transient and then reports the corrupt invariant
    ctx.SyncEntity(map);
    auto cr_lock = cr_a->GetEntityLock();
    REQUIRE(cr_lock);
    std::atomic<bool> child_locked {};
    std::atomic<bool> release_child {};
    std::atomic<bool> ensure_finished {};
    std::jthread child_owner {[&](std::stop_token) {
        cr_lock->Acquire(NextSyncTicket());
        child_locked.store(true, std::memory_order_release);

        while (!release_child.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        cr_lock->Release();
    }};

    while (!child_locked.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::jthread contention_watchdog {[&](std::stop_token) {
        for (int32_t i = 0; i < 10000 && !ensure_finished.load(std::memory_order_acquire); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        release_child.store(true, std::memory_order_release);
    }};
    CHECK_THROWS_WITH(ctx.EnsureEntitySynced(cr_a), Catch::Matchers::ContainsSubstring("covered entity lock is contended"));

    // Read before the test releases the contention itself, so this reflects only the watchdog
    bool rescued_by_watchdog = release_child.load(std::memory_order_acquire);
    ensure_finished.store(true, std::memory_order_release);
    release_child.store(true, std::memory_order_release);
    child_owner.join();
    contention_watchdog.join();

    // Bounded and self-terminating: it gave up on its own retry budget while the child lock was still held,
    // rather than being rescued by the watchdog letting go
    CHECK_FALSE(rescued_by_watchdog);
    CHECK(ctx.ValidateAccess(map));
    auto retained_cover = ctx.GetHeldEntities();
    CHECK(std::ranges::find(retained_cover, map_entity) != retained_cover.end());
    CHECK(std::ranges::find(retained_cover, cr_a_entity) == retained_cover.end());
    ctx.Release();

    // A busy state mutex normally means a foreign pass is mid-flight, so an unending one must make retention
    // give up on its own budget without changing the parent cover
    {
        ctx.SyncEntity(map);
        auto target_lock = cr_a->GetEntityLock();
        REQUIRE(target_lock);
        ExpectSustainedEnsureStateMutexContentionThrows(ctx, cr_a, target_lock);
        CHECK(target_lock->GetExclusiveRecursionForCurrentThread() == 0);
        CHECK(ctx.ValidateAccess(map));
        auto cover_after_target_state_mutex_contention = ctx.GetHeldEntities();
        CHECK(std::ranges::find(cover_after_target_state_mutex_contention, map_entity) != cover_after_target_state_mutex_contention.end());
        CHECK(std::ranges::find(cover_after_target_state_mutex_contention, cr_a_entity) == cover_after_target_state_mutex_contention.end());
        ctx.Release();
    }

    // The same guarantee applies to an intermediate ancestor mark. Giving up on its state mutex must not
    // leave either the descendant's exclusive count or the intermediate mark incremented
    {
        ctx.SyncEntity(map);
        auto target_lock = nested_item->GetEntityLock();
        auto intermediate_lock = cr_a->GetEntityLock();
        REQUIRE(target_lock);
        REQUIRE(intermediate_lock);
        REQUIRE(target_lock != intermediate_lock);
        ExpectSustainedEnsureStateMutexContentionThrows(ctx, nested_item, intermediate_lock);
        CHECK(target_lock->GetExclusiveRecursionForCurrentThread() == 0);
        CHECK(intermediate_lock->GetDescendantHoldCountForCurrentThread() == 0);
        CHECK(ctx.ValidateAccess(map));
        auto cover_after_intermediate_state_mutex_contention = ctx.GetHeldEntities();
        CHECK(std::ranges::find(cover_after_intermediate_state_mutex_contention, map_entity) != cover_after_intermediate_state_mutex_contention.end());
        CHECK(std::ranges::find(cover_after_intermediate_state_mutex_contention, nested_item_entity) == cover_after_intermediate_state_mutex_contention.end());
        ctx.Release();
    }

    // Contending the second address-ordered mutex proves every unlanded attempt rolls the first one back and
    // leaves both ownership counters at zero
    {
        ctx.SyncEntity(map);
        auto target_lock = nested_item->GetEntityLock();
        auto intermediate_lock = cr_a->GetEntityLock();
        REQUIRE(target_lock);
        REQUIRE(intermediate_lock);
        REQUIRE(target_lock != intermediate_lock);
        auto first_lock = target_lock < intermediate_lock ? target_lock : intermediate_lock;
        auto second_lock = target_lock < intermediate_lock ? intermediate_lock : target_lock;
        ExpectSustainedEnsureStateMutexContentionThrows(ctx, nested_item, second_lock);
        CHECK(target_lock->GetExclusiveRecursionForCurrentThread() == 0);
        CHECK(intermediate_lock->GetDescendantHoldCountForCurrentThread() == 0);
        bool first_state_mutex_free = first_lock->TryLockStateMutex();
        REQUIRE(first_state_mutex_free);
        if (first_state_mutex_free) {
            first_lock->UnlockStateMutex();
        }
        CHECK(ctx.ValidateAccess(map));
        auto cover_after_second_state_mutex_contention = ctx.GetHeldEntities();
        CHECK(std::ranges::find(cover_after_second_state_mutex_contention, map_entity) != cover_after_second_state_mutex_contention.end());
        CHECK(std::ranges::find(cover_after_second_state_mutex_contention, nested_item_entity) == cover_after_second_state_mutex_contention.end());
        ctx.Release();
    }

    // The reason retention retries at all: reporting the transient window instead would fail a caller that
    // provably owns the cover, as DestroyLocation did while a worker drained its marks
    {
        ctx.SyncEntity(map);
        auto target_lock = cr_a->GetEntityLock();
        REQUIRE(target_lock);
        ExpectTransientEnsureStateMutexContentionIsAbsorbed(ctx, cr_a, target_lock);
        CHECK(target_lock->GetExclusiveRecursionForCurrentThread() == 1);
        CHECK(ctx.ValidateAccess(map));
        CHECK(ctx.ValidateAccess(cr_a));
        auto cover_after_transient_contention = ctx.GetHeldEntities();
        CHECK(std::ranges::find(cover_after_transient_contention, map_entity) != cover_after_transient_contention.end());
        CHECK(std::ranges::find(cover_after_transient_contention, cr_a_entity) != cover_after_transient_contention.end());
        ctx.Release();
        CHECK(target_lock->GetExclusiveRecursionForCurrentThread() == 0);
    }

    // The same absorption for an intermediate ancestor mark: a transient hold on the mark's state mutex must
    // not fail the nested target's retention
    {
        ctx.SyncEntity(map);
        auto target_lock = nested_item->GetEntityLock();
        auto intermediate_lock = cr_a->GetEntityLock();
        REQUIRE(target_lock);
        REQUIRE(intermediate_lock);
        REQUIRE(target_lock != intermediate_lock);
        ExpectTransientEnsureStateMutexContentionIsAbsorbed(ctx, nested_item, intermediate_lock);
        CHECK(target_lock->GetExclusiveRecursionForCurrentThread() == 1);
        CHECK(intermediate_lock->GetDescendantHoldCountForCurrentThread() == 1);
        CHECK(ctx.ValidateAccess(map));
        CHECK(ctx.ValidateAccess(nested_item));
        ctx.Release();
        CHECK(target_lock->GetExclusiveRecursionForCurrentThread() == 0);
        CHECK(intermediate_lock->GetDescendantHoldCountForCurrentThread() == 0);
    }

    // Raw-locking whichever op sorts second proves the batch is checked for compatibility as a whole, before any
    // earlier operation can commit
    ctx.SyncEntity(map);
    auto item_lock = nested_item->GetEntityLock();
    auto nested_cr_lock = cr_a->GetEntityLock();
    REQUIRE(item_lock);
    REQUIRE(nested_cr_lock);
    REQUIRE(item_lock != nested_cr_lock);
    auto later_lock = item_lock < nested_cr_lock ? nested_cr_lock : item_lock;
    std::atomic<bool> later_locked {};
    std::atomic<bool> release_later {};
    std::jthread later_owner {[&](std::stop_token) {
        later_lock->Acquire(NextSyncTicket());
        later_locked.store(true, std::memory_order_release);

        while (!release_later.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        later_lock->Release();
    }};
    auto release_later_guard = scope_exit([&release_later]() noexcept { release_later.store(true, std::memory_order_release); });

    while (!later_locked.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    CHECK_THROWS_WITH(ctx.EnsureEntitySynced(nested_item), Catch::Matchers::ContainsSubstring("covered entity lock is contended"));
    CHECK(item_lock->GetExclusiveRecursionForCurrentThread() == 0);
    CHECK(nested_cr_lock->GetDescendantHoldCountForCurrentThread() == 0);

    if (item_lock < nested_cr_lock) {
        REQUIRE(item_lock->TryAcquire());
        item_lock->Release();
    }
    else {
        REQUIRE(nested_cr_lock->TryRegisterDescendantHold());
        nested_cr_lock->UnregisterDescendantHold();
    }

    CHECK(ctx.ValidateAccess(map));
    auto retained_cover_after_partial_rollback = ctx.GetHeldEntities();
    CHECK(std::ranges::find(retained_cover_after_partial_rollback, map_entity) != retained_cover_after_partial_rollback.end());
    CHECK(std::ranges::find(retained_cover_after_partial_rollback, nested_item_entity) == retained_cover_after_partial_rollback.end());
    release_later.store(true, std::memory_order_release);
    later_owner.join();
    ctx.Release();

    // Fresh registration uses the dedicated trusted capture path and therefore remains safe under an
    // otherwise empty context without weakening ordinary EnsureEntitySynced
    auto fresh_cr = server->CreateCritter(critter_pid, false);
    CHECK(ctx.ValidateAccess(fresh_cr));
    CHECK(IsEntityAccessValid(fresh_cr));
    server->CrMngr.DestroyCritter(fresh_cr);
    ctx.Release();

    // SyncEntity REPLACES the held set (yield-on-Sync), it does not accumulate
    ctx.SyncEntity(cr_a);
    CHECK(ctx.ValidateAccess(cr_a));
    ctx.SyncEntity(cr_b);
    CHECK_FALSE(ctx.ValidateAccess(cr_a));
    CHECK(ctx.ValidateAccess(cr_b));
    CHECK_FALSE(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(cr_b));
    ctx.Release();
    CHECK(ctx.IsEmpty());
    CHECK_FALSE(IsEntityAccessValid(cr_a)); // empty again → strict model: still no cover, access denied
}

TEST_CASE("ServerEngineSyncContextWidenAndAncestorCover")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    hstring location_pid = server->Hashes.to_hashed_string("UnitTestLocation");
    hstring map_pid = server->Hashes.to_hashed_string("UnitTestMap");
    hstring item_pid = server->Hashes.to_hashed_string("TestItem");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    auto setup_ctx = server->GetCurrentSyncContext();
    REQUIRE(static_cast<bool>(setup_ctx));
    setup_ctx->EnsureEntitySynced(loc);
    auto map = loc->GetMapByIndex(0);
    REQUIRE(static_cast<bool>(map));
    setup_ctx->EnsureEntitySynced(map);

    auto cr_a = server->CreateCritter(critter_pid, true);
    auto cr_b = server->CreateCritter(critter_pid, true);
    setup_ctx->EnsureEntitySynced(cr_a);
    setup_ctx->EnsureEntitySynced(cr_b);

    auto player_a_holder = CreateStandalonePlayer(server, "SyncWidenPlayerA");
    auto player_b_holder = CreateStandalonePlayer(server, "SyncWidenPlayerB");

    // The standalone players were built inside their own already-released contexts, so this context has no
    // cover for them and retention alone cannot create one — the whole setup scope is Sync'd by the caller
    vector<ptr<ServerEntity>> setup_scope {loc, map, cr_a, cr_b, player_a_holder, player_b_holder};
    setup_ctx->SyncEntities(setup_scope);

    cr_a->AttachPlayer(player_a_holder);
    player_a_holder->SetControlledCritter(cr_a);
    cr_b->AttachPlayer(player_b_holder);
    player_b_holder->SetControlledCritter(cr_b);

    auto detach_players = scope_exit([&]() noexcept {
        safe_call([&] {
            SyncContext cleanup_ctx;
            cleanup_ctx.Activate();
            auto deactivate_cleanup = scope_exit([&cleanup_ctx]() noexcept {
                safe_call([&cleanup_ctx] {
                    cleanup_ctx.Release();
                    cleanup_ctx.Deactivate();
                });
            });

            vector<ptr<ServerEntity>> attached_players {cr_a, cr_b, player_a_holder, player_b_holder};
            cleanup_ctx.SyncEntities(attached_players);

            if (cr_a->GetPlayer()) {
                cr_a->DetachPlayer();
            }
            if (cr_b->GetPlayer()) {
                cr_b->DetachPlayer();
            }
        });
    });

    server->MapMngr.TransferToMap(cr_a, map, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map, mpos {12, 12}, mdir {}, std::nullopt);
    auto item_a = server->CrMngr.AddItemToCritter(cr_a, server->ItemMngr.CreateItem(item_pid, nullptr), true);

    // The widen link must be live in both directions before we test the cover
    REQUIRE(cr_a->GetSyncWidenEntity() == player_a_holder);
    REQUIRE(player_a_holder->GetSyncWidenEntity() == cr_a);

    server->Unlock();
    locked = false;

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        safe_call([&ctx] {
            ctx.Release();
            ctx.Deactivate();
        });
    });

    // Syncing a player-controlled critter auto-widens to also lock its Player (both own locks held);
    // the other player is untouched
    {
        vector<ptr<ServerEntity>> one {cr_a};
        ctx.SyncEntities(one);
        CHECK(ctx.ValidateAccess(cr_a)); // own lock
        CHECK(ctx.ValidateAccess(player_a_holder)); // widened: player's own lock held
        CHECK_FALSE(ctx.ValidateAccess(player_b_holder));
        CHECK_FALSE(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(cr_a));
        CHECK(IsEntityAccessValid(player_a_holder));
        CHECK_FALSE(IsEntityAccessValid(player_b_holder));
    }
    ctx.Release();

    // Symmetric: syncing the Player widens to lock its controlled critter (both own locks held)
    {
        vector<ptr<ServerEntity>> one {player_a_holder};
        ctx.SyncEntities(one);
        CHECK(ctx.ValidateAccess(player_a_holder));
        CHECK(ctx.ValidateAccess(cr_a));
        CHECK(IsEntityAccessValid(player_a_holder));
        CHECK(IsEntityAccessValid(cr_a));
    }
    ctx.Release();

    // An inventory item shares its holder's propagated lock, so widening must still inspect the requested holder
    // or the controlled Player lock is missed
    {
        vector<ptr<ServerEntity>> item_holder_map {item_a, cr_a, map};
        REQUIRE_NOTHROW(ctx.SyncEntities(item_holder_map));
        CHECK(ctx.ValidateAccess(item_a));
        CHECK(ctx.ValidateAccess(cr_a));
        CHECK(ctx.ValidateAccess(map));
        CHECK(ctx.ValidateAccess(player_a_holder));
        CHECK(IsEntityAccessValid(item_a));
        CHECK(IsEntityAccessValid(player_a_holder));
    }
    ctx.Release();

    // Each requested entity keeps its own lock — the item does not share the holder's and the critters are not
    // collapsed onto their shared map — while widening still adds each critter's Player
    {
        vector<ptr<ServerEntity>> item_holder_recipient {item_a, cr_a, cr_b};
        REQUIRE_NOTHROW(ctx.SyncEntities(item_holder_recipient));
        CHECK_FALSE(ctx.ValidateAccess(map)); // map is only marked, not held
        CHECK(ctx.ValidateAccess(player_a_holder)); // widened from the requested holder
        CHECK(ctx.ValidateAccess(player_b_holder)); // widened from the requested recipient
        CHECK(ctx.ValidateAccess(item_a)); // item's own lock is held
        CHECK(ctx.ValidateAccess(cr_a)); // holder's own lock is held
        CHECK(ctx.ValidateAccess(cr_b)); // recipient's own lock is held
        CHECK(IsEntityAccessValid(item_a));
        CHECK(IsEntityAccessValid(cr_a));
        CHECK(IsEntityAccessValid(cr_b));
    }
    ctx.Release();

    // Widening both players reaches both controlled critters, which keep their own locks, so the widen verify
    // is satisfied directly and the shared map stays merely marked
    {
        vector<ptr<ServerEntity>> players {player_a_holder, player_b_holder};
        REQUIRE_NOTHROW(ctx.SyncEntities(players));
        CHECK(ctx.ValidateAccess(player_a_holder)); // players' own locks held
        CHECK(ctx.ValidateAccess(player_b_holder));
        CHECK(ctx.ValidateAccess(cr_a)); // widened critters' own locks held
        CHECK(ctx.ValidateAccess(cr_b));
        CHECK_FALSE(ctx.ValidateAccess(map)); // map is only marked, not held
        CHECK(IsEntityAccessValid(player_a_holder));
        CHECK(IsEntityAccessValid(cr_a));
        CHECK(IsEntityAccessValid(cr_b));
        CHECK_FALSE(IsEntityAccessValid(map)); // holding descendants does not cover the ancestor
    }
    ctx.Release();

    // Login starts with only the Player covered, so the critter switch must retain the new pair in the outer
    // context before the next subscriber reads the critter's inventory
    {
        vector<ptr<ServerEntity>> attached_pair {player_a_holder, cr_a};
        ctx.SyncEntities(attached_pair);
        cr_a->DetachPlayer();
    }
    ctx.Release();

    ctx.SyncEntity(player_a_holder);
    server->RunScriptContext([&] {
        auto attach_ctx = server->GetCurrentSyncContext();
        REQUIRE(static_cast<bool>(attach_ctx));
        vector<ptr<ServerEntity>> switch_scope {player_a_holder, cr_a, map, loc};
        attach_ctx->SyncEntities(switch_scope);
        server->SwitchPlayerCritter(player_a_holder, cr_a);
    });

    REQUIRE(player_a_holder->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(cr_a->GetEntityLock()->IsLockedByCurrentThread());
    CHECK_FALSE(item_a->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(ctx.ValidateAccess(cr_a));
    CHECK(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(item_a));
    ctx.Release();
}

TEST_CASE("ServerEngineEntityLinkPinSurvivesConcurrentDetach")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto cr = server->CreateCritter(critter_pid, true);
    auto setup_ctx = server->GetCurrentSyncContext();
    REQUIRE(static_cast<bool>(setup_ctx));
    setup_ctx->EnsureEntitySynced(cr);

    auto player = CreateStandalonePlayer(server, "LinkPinPlayer");

    vector<ptr<ServerEntity>> setup_scope {cr, player};
    setup_ctx->SyncEntities(setup_scope);

    cr->AttachPlayer(player);

    // The link carries its own reference, which is exactly what an unpinned read would be relying on, so the
    // accessor's contribution has to show up on top of it
    int32_t linked_player_refs = player->GetRefCount();

    {
        auto widen = cr->GetSyncWidenEntity();
        REQUIRE(widen == player);
        CHECK(player->GetRefCount() == linked_player_refs + 1);
    }

    CHECK(player->GetRefCount() == linked_player_refs);

    {
        auto pinned = cr->GetPlayerForSend();
        REQUIRE(pinned == player);
        CHECK(player->GetRefCount() == linked_player_refs + 1);
    }

    // The Player-to-Critter half of the pair is a non-owning link, so there the pin is the caller's only hold
    int32_t linked_critter_refs = cr->GetRefCount();

    {
        auto widen_back = player->GetSyncWidenEntity();
        REQUIRE(widen_back == cr);
        CHECK(cr->GetRefCount() == linked_critter_refs + 1);
    }

    CHECK(cr->GetRefCount() == linked_critter_refs);

    // Detach drops the link's reference; a handle taken beforehand keeps the player addressable past that point
    auto held = cr->GetSyncWidenEntity();
    REQUIRE(held);
    int32_t attached_player_refs = player->GetRefCount();

    cr->DetachPlayer();

    CHECK(player->GetRefCount() == attached_player_refs - 1);
    CHECK_FALSE(static_cast<bool>(cr->GetSyncWidenEntity()));
    CHECK_FALSE(static_cast<bool>(player->GetSyncWidenEntity()));
    CHECK(held == player);
    CHECK(held->GetName() == "LinkPinPlayer");
}

TEST_CASE("ServerEngineSyncContextFlatAcquisitionAncestorAndSiblingLiveness")
{
    // Ancestor and descendant locks exclude each other, and this pins that the exclusion never deadlocks or
    // starves: a model that turned it into a cycle would simply never join
    auto settings = MakeServerTestSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(ptr<GlobalSettings> {&settings}, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    hstring location_pid = server->Hashes.to_hashed_string("UnitTestLocation");
    hstring map_pid = server->Hashes.to_hashed_string("UnitTestMap");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool flat_locked = true;
    auto flat_unlock = scope_exit([&server, &flat_locked]() noexcept {
        safe_call([&server, &flat_locked] {
            if (flat_locked) {
                server->Unlock();
            }
        });
    });

    auto flat_loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    auto flat_map = flat_loc->GetMapByIndex(0);

    auto flat_critter = server->CreateCritter(critter_pid, false);
    flat_critter->SetParent(flat_map);

    server->Unlock();
    flat_locked = false;

    constexpr int32_t FLAT_ITERS = 20000;
    std::atomic<int64_t> ancestor_progress {0};
    std::atomic<int64_t> sibling_progress {0};

    auto ancestor_fn = [&]() {
        SyncContext ctx;
        ctx.Activate();
        vector<ptr<ServerEntity>> req {flat_map};
        for (int32_t i = 0; i < FLAT_ITERS; i++) {
            ctx.SyncEntities(req);
            ancestor_progress.fetch_add(1, std::memory_order_relaxed);
            ctx.Release();
        }
        ctx.Deactivate();
    };

    auto sibling_fn = [&]() {
        SyncContext ctx;
        ctx.Activate();
        vector<ptr<ServerEntity>> req {flat_critter};
        for (int32_t i = 0; i < FLAT_ITERS; i++) {
            ctx.SyncEntities(req);
            sibling_progress.fetch_add(1, std::memory_order_relaxed);
            ctx.Release();
        }
        ctx.Deactivate();
    };

    std::thread ancestor_thread(ancestor_fn);
    std::thread sibling_thread(sibling_fn);
    ancestor_thread.join();
    sibling_thread.join();

    CHECK(ancestor_progress.load() == FLAT_ITERS);
    CHECK(sibling_progress.load() == FLAT_ITERS);
}

TEST_CASE("ServerEngineSyncContextReparentStress")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    hstring location_pid = server->Hashes.to_hashed_string("UnitTestLocation");
    hstring map_pid = server->Hashes.to_hashed_string("UnitTestMap");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid, map_pid, map_pid});
    auto map_a = loc->GetMapByIndex(0);
    auto map_b = loc->GetMapByIndex(1);
    auto map_c = loc->GetMapByIndex(2);
    REQUIRE(static_cast<bool>(map_a));
    REQUIRE(static_cast<bool>(map_b));
    REQUIRE(static_cast<bool>(map_c));

    constexpr int32_t CRITTER_COUNT = 12;
    constexpr int32_t READER_THREADS = 6;
    constexpr int32_t READER_ITERS = 12000;
    constexpr int32_t REPARENTER_THREADS = 4;

    vector<ptr<Critter>> critters;
    for (int32_t i = 0; i < CRITTER_COUNT; i++) {
        auto cr = server->CreateCritter(critter_pid, false);
        cr->SetParent(map_a); // parent only - escalation reads _parent, not the map critter list
        critters.push_back(cr);
    }

    server->Unlock();
    locked = false;

    // Reparent targets include nullptr so the detach / no-parent / cover-recompute paths are churned
    // alongside same-map (escalation) and cross-map (escape → retry) transitions
    const nptr<ServerEntity> targets[] = {map_a, map_b, map_c, nullptr};

    std::atomic_bool stop {false};
    std::atomic<int64_t> syncs_ok {0};
    std::atomic<int64_t> sync_giveups {0};
    std::atomic<int64_t> reparents {0};

    auto reparenter_fn = [&](int32_t tid) {
        SyncContext ctx;
        ctx.Activate();

        uint64_t step = numeric_cast<uint64_t>(tid);
        while (!stop.load(std::memory_order_relaxed)) {
            for (int32_t i = tid; i < CRITTER_COUNT; i += REPARENTER_THREADS) {
                try {
                    auto& cr = critters[numeric_cast<size_t>(i)];
                    ctx.SyncEntity(cr);
                    cr->SetParent(targets[(step + numeric_cast<uint64_t>(i)) % 4U]);
                    reparents.fetch_add(1, std::memory_order_relaxed);
                }
                catch (const EntitySyncException&) {
                }

                ctx.Release();
            }
            step++;
        }

        ctx.Deactivate();
    };

    auto reader_fn = [&](int32_t tid) {
        SyncContext ctx;
        ctx.Activate();

        vector<ptr<ServerEntity>> req;
        req.reserve(2);

        for (int32_t it = 0; it < READER_ITERS; it++) {
            req.clear();
            req.emplace_back(critters[numeric_cast<size_t>((it + tid) % CRITTER_COUNT)]);
            req.emplace_back(critters[numeric_cast<size_t>((it * 3 + tid + 1) % CRITTER_COUNT)]);

            try {
                ctx.SyncEntities(req);
                syncs_ok.fetch_add(1, std::memory_order_relaxed);
                // The racing SetParent makes the result nondeterministic, so it is observed to drive the code path
                // rather than asserted
                (void)IsEntityAccessValid(req[0]);
                (void)IsEntityAccessValid(req[1]);
            }
            catch (const EntitySyncException&) {
                sync_giveups.fetch_add(1, std::memory_order_relaxed);
            }

            ctx.Release();
        }

        ctx.Deactivate();
    };

    vector<std::thread> reparenter_threads;
    vector<std::thread> reader_threads;
    for (int32_t i = 0; i < REPARENTER_THREADS; i++) {
        reparenter_threads.emplace_back(reparenter_fn, i);
    }
    for (int32_t i = 0; i < READER_THREADS; i++) {
        reader_threads.emplace_back(reader_fn, i);
    }
    for (auto& t : reader_threads) {
        t.join();
    }
    stop.store(true);
    for (auto& t : reparenter_threads) {
        t.join();
    }

    INFO("syncs_ok=" << syncs_ok.load() << " giveups=" << sync_giveups.load() << " reparents=" << reparents.load());
    // Reaching here already proves no deadlock or crash; the counts prove every reader iteration completed
    // exactly once, as a valid cover or a bounded give-up
    CHECK(syncs_ok.load() + sync_giveups.load() == int64_t {READER_THREADS} * int64_t {READER_ITERS});
    CHECK(syncs_ok.load() > 0); // forward progress - the retry loop is not failing every call
    CHECK(reparents.load() > 0);

    // Detach the critters so the maps' refcounts drop cleanly before Shutdown
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    locked = true;
    auto cleanup_ctx = SyncContext::GetCurrentOnThisThread();
    REQUIRE(cleanup_ctx);
    vector<ptr<ServerEntity>> cleanup_req;
    cleanup_req.reserve(critters.size());
    for (const auto& cr : critters) {
        cleanup_req.emplace_back(cr);
    }
    cleanup_ctx->SyncEntities(cleanup_req);
    for (auto& cr : critters) {
        cr->SetParent(nullptr);
    }
}

TEST_CASE("ServerEngineSyncContextFlatAcquisition")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    hstring location_pid = server->Hashes.to_hashed_string("UnitTestLocation");
    hstring map_pid = server->Hashes.to_hashed_string("UnitTestMap");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(static_cast<bool>(map));

    auto cr_a = server->CreateCritter(critter_pid, false);
    auto cr_b = server->CreateCritter(critter_pid, false);
    server->MapMngr.TransferToMap(cr_a, map, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map, mpos {12, 12}, mdir {}, std::nullopt);

    server->Unlock();
    locked = false;

    SECTION("AncestorExcludesDescendantCrossThread")
    {
        // Syncing a descendant must block while another thread owns the ancestor exclusively, because the
        // critter's ancestor mark cannot be registered until that owner releases
        std::atomic_bool t1_holds_map {false};
        std::atomic_bool t2_got_cr {false};
        std::atomic_bool t2_may_finish {false};

        std::thread t1([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<ptr<ServerEntity>> req {map};
            ctx.SyncEntities(req); // ancestor: the map's own lock, held exclusively
            t1_holds_map.store(true);
            while (!t2_may_finish.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            ctx.Release();
            ctx.Deactivate();
        });

        while (!t1_holds_map.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::thread t2([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<ptr<ServerEntity>> req {cr_a};
            ctx.SyncEntities(req); // descendant — must block until T1 releases the ancestor map
            t2_got_cr.store(true);
            ctx.Release();
            ctx.Deactivate();
        });

        // The generous window is what rules out a concurrent acquisition: a flat model would set the flag almost
        // immediately, a hierarchical one never while the ancestor is held
        nanotime exclusion_window = nanotime::now() + timespan {std::chrono::milliseconds {300}};
        while (nanotime::now() < exclusion_window) {
            CHECK_FALSE(t2_got_cr.load(std::memory_order_acquire));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // Release T1 — T2's blocked Sync must now complete
        t2_may_finish.store(true, std::memory_order_release);
        t1.join();
        t2.join();
        CHECK(t2_got_cr.load());
    }

    SECTION("ConcurrentAncestorAndSiblingLockLivenessLoop")
    {
        // Two threads hammering mutually exclusive ancestor and descendant locks must still complete every
        // iteration, which is the deadlock- and starvation-freedom proof
        constexpr int32_t ITERS = 20000;
        std::atomic<int64_t> ancestor_done {0};
        std::atomic<int64_t> sibling_done {0};

        std::thread ancestor_thread([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<ptr<ServerEntity>> req {map};
            for (int32_t i = 0; i < ITERS; i++) {
                ctx.SyncEntities(req);
                CHECK(IsEntityAccessValid(cr_a)); // descendant reachable via the ancestor cover
                CHECK(IsEntityAccessValid(cr_b));
                ctx.Release();
                ancestor_done.fetch_add(1, std::memory_order_relaxed);
            }
            ctx.Deactivate();
        });

        std::thread sibling_thread([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<ptr<ServerEntity>> req {cr_a};
            for (int32_t i = 0; i < ITERS; i++) {
                ctx.SyncEntities(req);
                CHECK(IsEntityAccessValid(cr_a)); // own lock
                ctx.Release();
                sibling_done.fetch_add(1, std::memory_order_relaxed);
            }
            ctx.Deactivate();
        });

        ancestor_thread.join();
        sibling_thread.join();

        CHECK(ancestor_done.load() == int64_t {ITERS});
        CHECK(sibling_done.load() == int64_t {ITERS});
    }

    SECTION("SingletonOwnedEntityEnsureIsIdempotent")
    {
        EntityLock singleton_lock;
        auto registrar = server->GetPropertyRegistrar("Critter");
        REQUIRE(registrar);
        auto singleton_owned_entity = safe_alloc::make_refcounted<CustomEntity>(server, ident_t {1}, registrar, nullptr);
        CHECK_FALSE(singleton_owned_entity->GetEntityLock());
        singleton_owned_entity->SetEntityLock(make_nptr(&singleton_lock));

        SyncContext ctx;
        ctx.Activate();
        ctx.LockSingleton(make_ptr(&singleton_lock));

        // A Game-owned custom entity shares Game's singleton lock, so retention under an active Game.Lock() must
        // reuse the tracked acquisition instead of opening a second bucket
        ctx.EnsureEntitySynced(singleton_owned_entity);
        ctx.UnlockSingleton(make_ptr(&singleton_lock));

        CHECK_FALSE(singleton_lock.IsLockedByCurrentThread());
        ctx.Release();
        ctx.Deactivate();
    }

    // Detach so the map refcount drops cleanly before Shutdown
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    locked = true;
    auto cleanup_ctx = SyncContext::GetCurrentOnThisThread();
    REQUIRE(cleanup_ctx);
    vector<ptr<ServerEntity>> cleanup_req {cr_a, cr_b};
    cleanup_ctx->SyncEntities(cleanup_req);
    cr_a->SetParent(nullptr);
    cr_b->SetParent(nullptr);
}

TEST_CASE("ServerEngineSyncContextNestedCrossEntityNoDeadlock")
{
    auto settings = MakeServerTestSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(ptr<GlobalSettings> {&settings}, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    hstring location_pid = server->Hashes.to_hashed_string("UnitTestLocation");
    hstring map_pid = server->Hashes.to_hashed_string("UnitTestMap");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    // Two critters on DIFFERENT maps so each keeps its OWN lock (no sibling escalation to a shared
    // map lock would collapse the pair and hide the cross-lock contention)
    auto loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid, map_pid});
    auto map_a = loc->GetMapByIndex(0);
    auto map_b = loc->GetMapByIndex(1);
    REQUIRE(static_cast<bool>(map_a));
    REQUIRE(static_cast<bool>(map_b));

    auto cr_a = server->CreateCritter(critter_pid, false).hold_ref();
    auto cr_b = server->CreateCritter(critter_pid, false).hold_ref();

    server->MapMngr.TransferToMap(cr_a, map_a, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map_b, mpos {12, 12}, mdir {}, std::nullopt);

    server->Unlock();
    locked = false;

    constexpr int32_t ROUNDS = 400;

    std::atomic<int64_t> rounds_done {0};
    std::atomic_bool failed {false};

    // The outer rendezvous is taken before any lock, or the harness itself would deadlock; the inner one
    // deliberately waits while each thread holds only its primary lock, so the cycle is hit every round
    std::atomic<int32_t> arrive {0};
    std::atomic<int32_t> generation {0};
    std::atomic<int32_t> primary_arrive {0};
    std::atomic<int32_t> primary_generation {0};

    auto barrier = [&](int32_t round) {
        int32_t gen = generation.load(std::memory_order_acquire);
        if (arrive.fetch_add(1, std::memory_order_acq_rel) + 1 == 2) {
            arrive.store(0, std::memory_order_release);
            generation.store(round + 1, std::memory_order_release);
        }
        else {
            while (generation.load(std::memory_order_acquire) == gen && !failed.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    };

    auto primary_barrier = [&](int32_t round) -> bool {
        int32_t gen = primary_generation.load(std::memory_order_acquire);
        if (primary_arrive.fetch_add(1, std::memory_order_acq_rel) + 1 == 2) {
            primary_arrive.store(0, std::memory_order_release);
            primary_generation.store(round + 1, std::memory_order_release);
            return true;
        }

        nanotime deadline = nanotime::now() + timespan {std::chrono::seconds {10}};
        while (primary_generation.load(std::memory_order_acquire) == gen && !failed.load(std::memory_order_acquire) && nanotime::now() < deadline) {
            std::this_thread::yield();
        }

        return primary_generation.load(std::memory_order_acquire) != gen && !failed.load(std::memory_order_acquire);
    };

    auto cross_thread = [&](ptr<ServerEntity> own, ptr<ServerEntity> peer) {
        for (int32_t round = 0; round < ROUNDS && !failed.load(std::memory_order_acquire); round++) {
            // Outer rendezvous while holding NO lock, so both threads start the locked section
            // together
            barrier(round);

            SyncContext primary;
            primary.Activate();

            // Primary cover: this thread's "own" critter (like a player job's controlled critter,
            // already covered when an event fires)
            vector<ptr<ServerEntity>> primary_req {own};
            try {
                primary.SyncEntities(primary_req);

                // Waiting until the peer also holds its primary is what makes the cross-acquire below collide
                // near-deterministically; the generation counter keeps a fast thread from skipping the rendezvous
                bool primary_met = primary_barrier(round);

                // Without the rendezvous the two acquires never overlap and a broken engine would pass, so a
                // missed deadline means the setup itself is broken and must fail hard
                if (!primary_met) {
                    failed.store(true, std::memory_order_release);
                }
                else {
                    // Nested context (like script execution) requesting BOTH critters
                    SyncContext nested;
                    nested.Activate();
                    auto nested_cleanup = scope_exit([&]() noexcept {
                        nested.Release();
                        nested.Deactivate();
                    });

                    vector<ptr<ServerEntity>> nested_req {own, peer};
                    nested.SyncEntities(nested_req);
                    // Catch2's assertion macros race on a process-global fast path, so a worker thread records
                    // through the atomic and the main thread reports it
                    if (!IsEntityAccessValid(own) || !IsEntityAccessValid(peer)) {
                        failed.store(true, std::memory_order_release);
                    }
                }
            }
            catch (const std::exception&) {
                failed.store(true, std::memory_order_release);
            }

            primary.Release();
            primary.Deactivate();
            rounds_done.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::thread t1(cross_thread, cr_a.as_ptr(), cr_b.as_ptr());
    std::thread t2(cross_thread, cr_b.as_ptr(), cr_a.as_ptr());

    // Scheduling delays do not imply deadlock while rounds keep completing. Bound both stalled progress
    // and the complete run; the owning critter references outlive both joins, including timeout shutdown.
    constexpr int64_t total_rounds = int64_t {ROUNDS} * 2;
    nanotime started = nanotime::now();
    nanotime last_progress = started;
    int64_t last_rounds = 0;
    int64_t rounds_at_timeout = 0;
    timespan idle_at_timeout {};
    bool timed_out = false;

    while (rounds_done.load(std::memory_order_acquire) < total_rounds && !failed.load(std::memory_order_acquire)) {
        nanotime now = nanotime::now();
        int64_t completed = rounds_done.load(std::memory_order_acquire);

        if (completed != last_rounds) {
            last_rounds = completed;
            last_progress = now;
        }

        if (now - last_progress > timespan {std::chrono::seconds {30}} || now - started > timespan {std::chrono::seconds {120}}) {
            timed_out = true;
            rounds_at_timeout = completed;
            idle_at_timeout = now - last_progress;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds {5});
    }

    if (timed_out) {
        // Best-effort: wake any thread stuck in a shutdown-abortable wait so the process can exit
        failed.store(true, std::memory_order_release);
        server->Shutdown();
    }

    t1.join();
    t2.join();

    INFO("Nested sync timeout rounds=" << rounds_at_timeout << ", idle_ms=" << idle_at_timeout.milliseconds());
    CHECK_FALSE(timed_out);
    CHECK_FALSE(failed.load());

    if (!timed_out && !failed.load()) {
        CHECK(rounds_done.load() == total_rounds);
    }

    if (!timed_out) {
        // Detach so the map refcounts drop cleanly before Shutdown
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        locked = true;
        auto cleanup_ctx = SyncContext::GetCurrentOnThisThread();
        REQUIRE(static_cast<bool>(cleanup_ctx));
        vector<ptr<ServerEntity>> cleanup_req {cr_a.get(), cr_b.get()};
        cleanup_ctx->SyncEntities(cleanup_req);
        cr_a->SetParent(nullptr);
        cr_b->SetParent(nullptr);
    }
}

TEST_CASE("ServerEngineDestroyedEntityArgumentReportsMissingCoverFirst")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto setup_ctx = server->GetCurrentSyncContext();
    REQUIRE(static_cast<bool>(setup_ctx));

    BoundaryArgAccessor accessor;

    // A borrow wrapper is exactly the pointer-sized handle slot the script ABI passes, so it stands in without
    // unwrapping; the entity branch never touches the scratch parameter
    auto convert = [&accessor](ptr<Critter> cr) {
        nptr<Entity> slot = cr;
        nptr<Critter> unused_scratch;
        return NativeDataCaller::ConvertArg<ptr<Critter>, nptr<Critter>>(make_ptr(&slot).void_cast(), accessor, unused_scratch);
    };

    SECTION("LiveCoveredArgumentConverts")
    {
        auto cr = server->CreateCritter(critter_pid, false);
        setup_ctx->EnsureEntitySynced(cr);

        CHECK(convert(cr) == cr);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("DestroyedButStillCoveredNamesTheDestroyedHandle")
    {
        // The destroyer keeps the victim's own lock, so this is the caller that destroyed the entity
        // and kept using the handle — the one case the destroyed-entity message actually describes
        auto cr = server->CreateCritter(critter_pid, false);
        setup_ctx->EnsureEntitySynced(cr);
        server->CrMngr.DestroyCritter(cr);

        REQUIRE(IsEntityAccessValid(cr));
        CHECK_THROWS_WITH(convert(cr), Catch::Matchers::ContainsSubstring("destroyed entity"));
    }

    SECTION("DestroyedAndUncoveredNamesTheMissingCover")
    {
        // Destroying inside a nested context leaves exactly the shape a racing caller sees; the extra ref keeps
        // the argument observable after that context drops the last one
        refcount_nptr<Critter> cr;

        server->RunScriptContext([&] {
            auto nested_ctx = server->GetCurrentSyncContext();
            REQUIRE(static_cast<bool>(nested_ctx));
            ptr<Critter> created = server->CreateCritter(critter_pid, false);
            cr = created.hold_ref();
            nested_ctx->EnsureEntitySynced(created);
            server->CrMngr.DestroyCritter(created);
        });

        REQUIRE(static_cast<bool>(cr));
        REQUIRE_FALSE(IsEntityAccessValid(cr));
        CHECK_THROWS_WITH(convert(cr.as_ptr()), Catch::Matchers::ContainsSubstring("Entity access without sync"));
    }

    SECTION("EntityArrayRejectsPrototypeAtTheNativeBoundary")
    {
        nptr<const ProtoCritter> proto = server->GetProtoCritter(critter_pid);
        REQUIRE(proto);

        vector<nptr<const Entity>> script_entities {proto};
        NativeDataProvider::StorageEntryType storage;
        ptr<void> array_data = NativeDataProvider::NormalizeArg(script_entities, storage);
        optional<vector<nptr<const ServerEntity>>> converted_entities;

        auto convert_collection = [&]() { return NativeDataCaller::ConvertArg<readonly_vector<nptr<const ServerEntity>>, optional<vector<nptr<const ServerEntity>>>, true>(array_data, NativeDataProvider::NATIVE_DATA_ACCESSOR, converted_entities); };

        CHECK_THROWS_WITH(convert_collection(), Catch::Matchers::ContainsSubstring("Script entity is not usable as this entity type"));
    }
}

TEST_CASE("ServerEngineDrawsDiagnosticGuiHeadlessly")
{
    auto settings = MakeServerTestSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    // Populate the world so the per-entity panels render real rows instead of empty tables
    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

        (void)server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false);
        (void)CreateLoggedPlayer(server, "UnitTestGuiPlayer");
        (void)server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"));

        shared_ptr<NetworkServerConnection> not_logged_in_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        (void)server->CreateNotLoggedInPlayer(std::move(not_logged_in_connection));
    }

    REQUIRE(server->EntityMngr.GetCrittersCount() >= 1);
    REQUIRE(server->EntityMngr.GetPlayersCount() >= 1);
    REQUIRE(server->EntityMngr.GetLocationsCount() >= 1);

    // The panels are normally reachable only through the windowed application, so a backend-less ImGui context
    // runs every panel builder for real and discards the draw data
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;

    // The legacy atlas-upload entry points are compiled out, so declare the modern texture contract instead
    // and let ImGui own the atlas; nothing consumes the resulting texture requests here
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // A collapsed node never runs its body and has no stored open state to flip, so logging — which
    // auto-expands every node — is the supported way in
    constexpr int32_t GUI_FRAMES = 3;

    // Collapsing headers opt out of the log auto-expansion, so their stored state is seeded by hand. The
    // ids mirror the root panels of ServerEngine::DrawGui; the log assertions below catch any drift
    constexpr std::array ROOT_PANEL_IDS = {"Info", "Performance details", "###Players", "###NotLoggedInPlayers", "###Locations", "Data base"};

    string drawn_text;

    for (int32_t frame = 0; frame < GUI_FRAMES; frame++) {
        ImGui::NewFrame();

        if (ImGui::Begin("ServerDiagnostics")) {
            ptr<ImGuiWindow> window = ImGui::GetCurrentWindow();

            for (string_view panel_id : ROOT_PANEL_IDS) {
                window->StateStorage.SetInt(ImGui::GetID(panel_id.data(), panel_id.data() + panel_id.size()), 1);
            }

            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(server->DrawGui());

            // LogFinish() drops the captured text, so take it while the log is still open
            drawn_text.assign(ImGui::GetCurrentContext()->LogBuffer.c_str());
            ImGui::LogFinish();
        }

        ImGui::End();
        ImGui::Render();
    }

    CHECK(ImGui::GetCurrentContext() != nullptr);
    CHECK(ImGui::GetFrameCount() == GUI_FRAMES);

    // Prove the walk actually descended into the panels instead of silently rendering collapsed headers
    INFO(drawn_text);
    CHECK(drawn_text.find("Data base") != string::npos);
    CHECK(drawn_text.find("Memory documents") != string::npos);
    CHECK(drawn_text.find("Performance details") != string::npos);
    CHECK(drawn_text.find("NotLoggedIn players (1)") != string::npos);
}

TEST_CASE("EngineAppliesGameSettingMetadataOnlyToUnconfiguredValues")
{
    auto settings = MakeServerTestSettings();

    ConfigFile runtime_config {"TestSettings.Int32Value = 7\nUngroupedTestSetting = 9\n"};
    settings.ApplyConfigFile(runtime_config, "");

    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings,
        MakeServerTestResources(ServerTestScriptMode::Default,
            BakerTests::MakeMetadataBlob({{"Setting",
                {
                    {"TestSettings.Int32Value", "int32", "0"},
                    {"UngroupedTestSetting", "int32", "0"},
                    {"TestSettings.Int64Value", "int64", "0"},
                    {"TestSettings.StringValue", "string", "0"},
                    {"TestSettings.BoolValue", "bool", "0"},
                }}})));

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    // The configuration named these before the engine existed, so the metadata baseline of "0" must not
    // have replaced them; the ungrouped one proves the bare spelling matches the same way
    CHECK(settings.GetCustomSetting("TestSettings.Int32Value") == "7");
    CHECK(settings.GetCustomSetting("UngroupedTestSetting") == "9");

    // The configuration never named these, so they arrive from the metadata baseline
    CHECK(settings.GetCustomSetting("TestSettings.Int64Value") == "0");
    CHECK(settings.GetCustomSetting("TestSettings.StringValue") == "0");
    CHECK(settings.GetCustomSetting("TestSettings.BoolValue") == "0");
}

FO_END_NAMESPACE
