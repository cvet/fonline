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

#if FO_ANGELSCRIPT_SCRIPTING

#include "Test_ServerEngineHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ServerEngineTests;

TEST_CASE("AngelScriptServerEngineStopsStartupWhenInitEventStopsChain")
{
    auto settings = MakeServerTestSettings();

    SECTION("ExplicitStopChain")
    {
        CheckServerStartupFailsSafely(settings, MakeServerTestResources(ServerTestScriptMode::StopOnInit));
    }

    SECTION("ExplicitResultException")
    {
        CheckServerStartupFailsSafely(settings, MakeServerTestResources(ServerTestScriptMode::ThrowOnInit));
    }
}

TEST_CASE("AngelScriptServerEngineScriptModuleInitAndEventsAreCallable")
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

    auto get_func_name = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    int init_calls = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetInitCalls"), init_calls));
    CHECK(init_calls == 1);

    int manual_calls = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 0);

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestMarkManualCall")));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 1);

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    auto cr = server->CreateCritter(critter_pid, false);

    int critter_init_calls = 0;
    int64_t last_critter_id = 0;
    bool last_first_time = false;

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetCritterInitCalls"), critter_init_calls));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetLastCritterId"), last_critter_id));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetLastCritterFirstTime"), last_first_time));

    CHECK(critter_init_calls >= 1);
    CHECK(last_critter_id == cr->GetId().underlying_value());
    CHECK(last_first_time);

    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("AngelScriptServerEngineModuleInitAttributePriorityIsRespected")
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

    auto get_func_name = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    int immediate_init_order = 0;
    int deferred_init_order = 0;

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetImmediateInitOrder"), immediate_init_order));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetDeferredInitOrder"), deferred_init_order));

    CHECK(immediate_init_order == 1);
    CHECK(deferred_init_order == 2);
}

TEST_CASE("AngelScriptServerEngineAdminRemoteCallsAreAllowlisted")
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

    auto get_func_name = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestResetAdminCallCounter")));
    REQUIRE(server->CallAdminFunc(get_func_name("ServerEngineTest::UnitTestAdminEntry")));

    int admin_call_counter = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetAdminCallCounter"), admin_call_counter));
    CHECK(admin_call_counter == 1);

    CHECK_FALSE(server->CallAdminFunc(get_func_name("ServerEngineTest::UnitTestRegularEntry")));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetAdminCallCounter"), admin_call_counter));
    CHECK(admin_call_counter == 1);
}

TEST_CASE("AngelScriptServerEngineScriptCallsMarshalContainersAndEntities")
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

    auto get_func_name = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    vector<int32_t> values {1, 2, 3};
    auto sum_array_func = server->FindFunc<int32_t, vector<int32_t>>(get_func_name("ServerEngineTest::UnitTestSumArray"));
    REQUIRE(sum_array_func);
    REQUIRE(sum_array_func.Call(values));
    CHECK(sum_array_func.GetResult() == 6);

    auto mutate_array_func = server->FindFunc<void, vector<int32_t>&>(get_func_name("ServerEngineTest::UnitTestMutateArray"));
    REQUIRE(mutate_array_func);
    REQUIRE(mutate_array_func.Call(values));
    CHECK(values == vector<int32_t> {11, 2, 3, 77});

    hstring critter_pid = server->Hashes.to_hashed_string("UnitTestRat");
    auto cr = server->CreateCritter(critter_pid, false);

    auto critter_id_func = server->FindFunc<int64_t, ptr<Critter>>(get_func_name("ServerEngineTest::UnitTestGetCritterIdValue"));
    REQUIRE(critter_id_func);
    REQUIRE(critter_id_func.Call(cr));
    CHECK(critter_id_func.GetResult() == cr->GetId().underlying_value());

    auto nullable_item_func = server->FindFunc<bool, nptr<Item>>(get_func_name("ServerEngineTest::UnitTestIsItemNull"));
    REQUIRE(nullable_item_func);
    REQUIRE(nullable_item_func.Call(nptr<Item> {}));
    CHECK(nullable_item_func.GetResult());

    auto matches_hash_func = server->FindFunc<bool, hstring>(get_func_name("ServerEngineTest::UnitTestMatchesHash"));
    REQUIRE(matches_hash_func);
    REQUIRE(matches_hash_func.Call(server->Hashes.to_hashed_string("UnitTestHash")));
    CHECK(matches_hash_func.GetResult());

    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("AngelScriptServerSnapshotRejectsSuspendedScript")
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

    auto has_blocker = [](const ServerSnapshotCaptureResult& result, ServerSnapshotBlockerKind kind) { return std::ranges::any_of(result.Blockers, [kind](const ServerSnapshotBlocker& blocker) { return blocker.Kind == kind && blocker.Count != 0; }); };

    SECTION("SuspendedAngelScript")
    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            auto suspend_func = server->FindFunc<void>(server->Hashes.to_hashed_string("ServerEngineTest::UnitTestSuspend"));
            REQUIRE(suspend_func);
            REQUIRE(suspend_func.Call());
        }

        auto result = server->CreateSnapshot(timespan {std::chrono::seconds {10}});

        CHECK(result.ReachedQuiescence);
        CHECK_FALSE(result.State.has_value());
        CHECK(result.Payload.empty());
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::SuspendedScriptContexts));
        CHECK(has_blocker(result, ServerSnapshotBlockerKind::DelayedCallbacks));
    }
}

FO_END_NAMESPACE

#endif
