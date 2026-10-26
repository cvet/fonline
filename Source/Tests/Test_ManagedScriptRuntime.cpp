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

#if FO_MANAGED_SCRIPTING

#include "ManagedScriptBackend.h"
#include "ManagedScriptBaker.h"
#include "Test_ClientEngineHelpers.h"
#include "Test_MapperHelpers.h"
#include "Test_ServerEngineHelpers.h"

FO_BEGIN_NAMESPACE

class ManagedRuntimeTestResources final
{
public:
    static auto MakeProbeMetadata(string_view target) -> vector<uint8_t>
    {
        return target == "server" ? BakerTests::MakeMetadataBlob({
                                        {"Property",
                                            {
                                                {"Critter", "Server", "string", "RuntimeStoredText", "Mutable"},
                                                {"Critter", "Server", "string", "RuntimeVirtualText", "Virtual"},
                                            }},
                                        {"RemoteCall",
                                            {
                                                {"UnitTestRuntimeRpc", "ManagedRuntimeProbe.cs", "In", "Limits", "0", "0"},
                                                {"UnitTestRuntimeGenericRpc", "ManagedRuntimeProbe.cs", "In", "Limits", "0", "0"},
                                                {"UnitTestRuntimeImmediateCanceledRpc", "ManagedRuntimeProbe.cs", "In", "Limits", "0", "0"},
                                                {"UnitTestRuntimeImmediateSuccessRpc", "ManagedRuntimeProbe.cs", "In", "Limits", "0", "0"},
                                            }},
                                    }) :
                                    BakerTests::MakeEmptyMetadataBlob();
    }

    ManagedRuntimeTestResources() :
        Root {std::filesystem::temp_directory_path() / fs::make_path(strex("FOnlineManagedRuntimeTest_{}_{}", platform::get_current_process_id_str(), nanotime::now().milliseconds()).str())}
    {
        bool created = fs::create_directories(fs::path_to_string(Root));
        REQUIRE(created);

        auto cleanup_failed_construction = scope_fail([this]() noexcept { (void)fs::remove_dir_tree(fs::path_to_string(Root)); });

        std::filesystem::path engine_root = std::filesystem::path {fs::make_path(__FILE__)}.parent_path().parent_path().parent_path();
        std::filesystem::path config_path = Root / "Runtime.fomain";
        bool config_written = fs::write_file(fs::path_to_string(config_path), string_view {});
        REQUIRE(config_written);

        Rig.Settings.ApplyConfigAtPath("Runtime.fomain", fs::path_to_string(Root));
        BakerTests::OverrideSetting(Rig.Settings.Baking.BakeOutput, fs::path_to_string(Root / "Baking"));
        BakerTests::OverrideSetting(Rig.Settings.ManagedScript.GeneratedDir, fs::path_to_string(Root / "Generated"));
        BakerTests::OverrideSetting(Rig.Settings.ManagedScript.ProjectName, string {"RuntimeProbe"});
        BakerTests::OverrideSetting(Rig.Settings.ManagedScript.Dirs,
            vector<string> {
                fs::path_to_string(engine_root / "Source/Scripting/Managed/CoreScripts"),
                fs::path_to_string(engine_root / "Source/Tests/Fixtures"),
            });

        for (string_view target : {"server", "client", "mapper"}) {
            Rig.AddBakedFile(strex("Metadata.fometa-{}", target).str(), MakeProbeMetadata(target));
        }

        ManagedScriptBaker baker {Rig.MakeContext("RuntimeProbe")};
        baker.BakeFiles(BakerTests::TestRig::MakeEmptyFiles(), "");
        REQUIRE(Rig.Outputs.contains("Assemblies/Assemblies-server/RuntimeProbe.Server.dll"));
        REQUIRE(Rig.Outputs.contains("Assemblies/Assemblies-client/RuntimeProbe.Client.dll"));
        REQUIRE(Rig.Outputs.contains("Assemblies/Assemblies-mapper/RuntimeProbe.Mapper.dll"));
    }
    ManagedRuntimeTestResources(const ManagedRuntimeTestResources&) = delete;
    ManagedRuntimeTestResources(ManagedRuntimeTestResources&&) noexcept = delete;
    auto operator=(const ManagedRuntimeTestResources&) = delete;
    auto operator=(ManagedRuntimeTestResources&&) noexcept = delete;
    ~ManagedRuntimeTestResources() { (void)fs::remove_dir_tree(fs::path_to_string(Root)); }

    void AddTo(FileSystem& resources) const
    {
        auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ManagedRuntimeAssemblies");

        source->AddFile("Metadata.fometa-server", MakeProbeMetadata("server"));

        for (const auto& [path, bytes] : Rig.Outputs) {
            source->AddFile(path, bytes);
        }

        resources.AddCustomSource(std::move(source));
    }

private:
    std::filesystem::path Root;
    BakerTests::TestRig Rig {};
};

TEST_CASE("ManagedScriptRuntime", "[managed][scripting]")
{
    ManagedRuntimeTestResources assemblies;
    auto settings = ServerEngineTests::MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Network.DisableInterthreadCommunication, true);
    BakerTests::OverrideSetting(settings.DataBase.CustomCollections, vector<string> {"test_collection:Int", "test_strings:Str"});
    auto resources = ServerEngineTests::MakeServerTestResources(ServerEngineTests::ServerTestScriptMode::Default, ManagedRuntimeTestResources::MakeProbeMetadata("server"));
    assemblies.AddTo(resources);
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources));
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    string startup_error = ServerEngineTests::WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    auto name = [&server](string_view method) { return server->Hashes.to_hashed_string(strex("ManagedRuntimeProbe::{}", method).str()); };

    int32_t init_calls = 0;
    REQUIRE(server->CallFunc(name("GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    CHECK_FALSE(server->FindFunc<void>(name("MissingFunction")));

    auto transport = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
    auto rpc_player = server->CreateNotLoggedInPlayer(std::move(transport));
    hstring rpc_name = server->Hashes.to_hashed_string("UnitTestRuntimeRpc");
    auto finish_rpc = server->FindFunc<void, int32_t>(name("FinishRuntimeRpc"));
    REQUIRE(finish_rpc);

    auto finish_off_thread = server->FindFunc<void, int32_t>(name("FinishRuntimeRpcOffThread"));
    auto script_errors = server->FindFunc<int32_t>(name("GetRuntimeScriptErrors"));
    REQUIRE(finish_off_thread);
    REQUIRE(script_errors);
    REQUIRE(script_errors.Call());
    int32_t errors_before = script_errors.GetResult();

    for (int32_t outcome : {0, 1, 2}) {
        auto invocation = rpc_player->GetConnection()->BeginRemoteCall(rpc_name, rpc_player->GetId());
        REQUIRE(invocation.Accepted);
        REQUIRE(invocation.Completion);
        server->HandleInboundRemoteCall(rpc_name, rpc_player, span<uint8_t> {}, std::move(invocation.Completion));
        CHECK(rpc_player->GetConnection()->GetDiagnostics().PendingRemoteCalls == 1);
        // The world is stopped and no script pump runs while this foreign thread completes the Task
        REQUIRE(finish_off_thread.Call(outcome));
        CHECK(rpc_player->GetConnection()->GetDiagnostics().PendingRemoteCalls == 0);
    }
    REQUIRE(script_errors.Call());
    CHECK(script_errors.GetResult() == errors_before + 1); // only the fault, neither cancellation

    for (string_view method : {"UnitTestRuntimeImmediateSuccessRpc", "UnitTestRuntimeImmediateCanceledRpc"}) {
        auto immediate_name = server->Hashes.to_hashed_string(method);
        auto invocation = rpc_player->GetConnection()->BeginRemoteCall(immediate_name, rpc_player->GetId());
        REQUIRE(invocation.Accepted);
        REQUIRE(invocation.Completion);
        server->HandleInboundRemoteCall(immediate_name, rpc_player, span<uint8_t> {}, std::move(invocation.Completion));
        CHECK(rpc_player->GetConnection()->GetDiagnostics().PendingRemoteCalls == 0);
    }
    REQUIRE(script_errors.Call());
    CHECK(script_errors.GetResult() == errors_before + 1);

    // Flush only the nested, ownerless generic calls; their completion never owns a tracked RPC
    server->ProcessBackends();
    auto generic_finishes = server->FindFunc<int32_t>(name("GetGenericRpcFinishes"));
    REQUIRE(generic_finishes);
    REQUIRE(generic_finishes.Call());
    int32_t generic_before = generic_finishes.GetResult();
    hstring generic_name = server->Hashes.to_hashed_string("UnitTestRuntimeGenericRpc");
    server->HandleInboundRemoteCall(generic_name, rpc_player, span<uint8_t> {});
    REQUIRE(generic_finishes.Call());
    CHECK(generic_finishes.GetResult() == generic_before); // Task<T> must not synchronously return its wire result
    server->ProcessBackends();
    REQUIRE(generic_finishes.Call());
    CHECK(generic_finishes.GetResult() == generic_before + 1);

    int64_t rpc_metadata_lookups = -1;
    REQUIRE(server->CallFunc(name("MeasureRuntimeRpcMetadataLookups"), rpc_metadata_lookups));
    CHECK(rpc_metadata_lookups == 0);

    int32_t database_result = -1;
    REQUIRE(server->CallFunc(name("TestConditionalDatabaseRecords"), database_result));
    CHECK(database_result == 0);

    auto sum_array = server->FindFunc<int32_t, vector<int32_t>>(name("SumArray"));
    REQUIRE(sum_array);
    vector<int32_t> values {1, 2, 3};
    REQUIRE(sum_array.Call(values));
    CHECK(sum_array.GetResult() == 6);

    auto transform_array = server->FindFunc<vector<int32_t>, vector<int32_t>>(name("TransformArray"));
    REQUIRE(transform_array);
    REQUIRE(transform_array.Call(values));
    CHECK(transform_array.GetResult() == vector<int32_t> {11, 2, 3, 77});

    auto sum_dict = server->FindFunc<int32_t, map<int32_t, int32_t>>(name("SumDictionary"));
    REQUIRE(sum_dict);
    REQUIRE(sum_dict.Call(map<int32_t, int32_t> {{1, 10}, {2, 20}}));
    CHECK(sum_dict.GetResult() == 33);

    auto transform_dict = server->FindFunc<map<int32_t, int32_t>, map<int32_t, int32_t>>(name("TransformDictionary"));
    REQUIRE(transform_dict);
    REQUIRE(transform_dict.Call(map<int32_t, int32_t> {{1, 10}, {2, 20}}));
    CHECK(transform_dict.GetResult() == map<int32_t, int32_t> {{1, 15}, {2, 20}, {3, 30}});

    auto echo_string = server->FindFunc<string, string>(name("EchoString"));
    REQUIRE(echo_string);
    string text = "UTF-8 \xD0\x9C\xD0\xB8\xD1\x80";
    REQUIRE(echo_string.Call(text));
    CHECK(echo_string.GetResult() == text);

    auto echo_hash = server->FindFunc<hstring, hstring>(name("EchoHash"));
    REQUIRE(echo_hash);
    hstring hash = server->Hashes.to_hashed_string("RuntimeHash");
    REQUIRE(echo_hash.Call(hash));
    CHECK(echo_hash.GetResult() == hash);

    auto critter = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false);
    int32_t critter_init_calls = 0;
    int64_t last_critter_id = 0;
    REQUIRE(server->CallFunc(name("GetCritterInitCalls"), critter_init_calls));
    REQUIRE(server->CallFunc(name("GetLastCritterId"), last_critter_id));
    CHECK(critter_init_calls == 1);
    CHECK(last_critter_id == critter->GetId().underlying_value());

    auto property_bridge = server->FindFunc<string, ptr<Critter>>(name("ProbePropertyBridge"));
    REQUIRE(property_bridge);
    REQUIRE(property_bridge.Call(critter));
    CHECK(property_bridge.GetResult() == "set:probe|get:set:probe|1|1");

    auto critter_id = server->FindFunc<int64_t, ptr<Critter>>(name("GetCritterId"));
    REQUIRE(critter_id);
    REQUIRE(critter_id.Call(critter));
    CHECK(critter_id.GetResult() == critter->GetId().underlying_value());

    auto update_string = server->FindFunc<void, ptr<Critter>, ptr<Critter>, string&>(name("UpdateString"));
    REQUIRE(update_string);
    string updated_text = "value";
    REQUIRE(update_string.Call(critter, critter, updated_text));
    CHECK(updated_text == "same:value");

    auto item_null = server->FindFunc<bool, nptr<Item>>(name("IsItemNull"));
    REQUIRE(item_null);
    REQUIRE(item_null.Call(nptr<Item> {}));
    CHECK(item_null.GetResult());
    auto item = server->ItemMngr.CreateItem(server->Hashes.to_hashed_string("TestItem"), nullptr);
    REQUIRE(item_null.Call(item));
    CHECK_FALSE(item_null.GetResult());

    string exception_message;
    auto previous_exception_callback = exceptions::get_callback();
    exceptions::set_callback([&exception_message](string_view message, const stack_trace::catched_data&, bool) { exception_message = message; });
    auto restore_exception_callback = scope_exit([&previous_exception_callback]() noexcept { exceptions::set_callback(std::move(previous_exception_callback)); });
    auto throwing = server->FindFunc<void>(name("ThrowFromScript"));
    REQUIRE(throwing);
    CHECK_FALSE(throwing.Call());
    CHECK(exception_message.find("Managed runtime probe failure") != string::npos);
    exceptions::set_callback(std::move(previous_exception_callback));
    restore_exception_callback.release();
    auto first_pending = rpc_player->GetConnection()->BeginRemoteCall(rpc_name, rpc_player->GetId());
    REQUIRE(first_pending.Accepted);
    server->HandleInboundRemoteCall(rpc_name, rpc_player, span<uint8_t> {}, std::move(first_pending.Completion));
    auto first_runtime = rpc_player->GetConnection()->GetRuntimeState();
    server->Unlock();
    unlock.release();

    // A second live engine must initialize its own assembly statics and event subscriptions
    auto second_settings = ServerEngineTests::MakeServerTestSettings();
    BakerTests::OverrideSetting(second_settings.Network.DisableInterthreadCommunication, true);
    auto second_resources = ServerEngineTests::MakeServerTestResources(ServerEngineTests::ServerTestScriptMode::Default, ManagedRuntimeTestResources::MakeProbeMetadata("server"));
    assemblies.AddTo(second_resources);
    auto second = safe_alloc::make_refcounted<ServerEngine>(&second_settings, std::move(second_resources));
    auto shutdown_second = scope_exit([&second]() noexcept { safe_call([&second] { second->Shutdown(); }); });
    startup_error = ServerEngineTests::WaitForServerStart(second);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(second->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock_second = scope_exit([&second]() noexcept { safe_call([&second] { second->Unlock(); }); });
    REQUIRE(second->CallFunc(second->Hashes.to_hashed_string("ManagedRuntimeProbe::GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    REQUIRE(second->CallFunc(second->Hashes.to_hashed_string("ManagedRuntimeProbe::GetCritterInitCalls"), critter_init_calls));
    CHECK(critter_init_calls == 0);
    auto second_transport = NetworkServer::CreateDummyConnection(second->Settings, NetworkServer::DummyConnectionState::Connected);
    auto second_player = second->CreateNotLoggedInPlayer(std::move(second_transport));
    hstring second_rpc_name = second->Hashes.to_hashed_string("UnitTestRuntimeRpc");
    auto second_pending = second_player->GetConnection()->BeginRemoteCall(second_rpc_name, second_player->GetId());
    REQUIRE(second_pending.Accepted);
    second->HandleInboundRemoteCall(second_rpc_name, second_player, span<uint8_t> {}, std::move(second_pending.Completion));
    auto second_runtime = second_player->GetConnection()->GetRuntimeState();
    auto race = second->FindFunc<void>(second->Hashes.to_hashed_string("ManagedRuntimeProbe::RaceRuntimeRpcCompletionWithTeardown"));
    REQUIRE(race);
    REQUIRE(race.Call());
    second->Unlock();
    unlock_second.release();
    second->Shutdown();
    CHECK(ServerConnection::GetRuntimeDiagnostics(second_runtime).PendingRemoteCalls == 0);
    CHECK(ServerConnection::GetRuntimeDiagnostics(first_runtime).PendingRemoteCalls == 1);

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock_first_again = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    REQUIRE(finish_off_thread.Call(0));
    CHECK(ServerConnection::GetRuntimeDiagnostics(first_runtime).PendingRemoteCalls == 0);
    REQUIRE(server->CallFunc(name("GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    REQUIRE(server->CallFunc(name("GetCritterInitCalls"), critter_init_calls));
    CHECK(critter_init_calls == 1);
    server->Unlock();
    unlock_first_again.release();

    // An externally completed Task must remain blocked while its script continuation is paused
    server->PauseSimulation();
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    {
        auto unlock_gate = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        REQUIRE(server->CallFunc(name("WaitForSnapshotGate")));
    }
    auto pending = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
    REQUIRE(pending.ReachedQuiescence);
    REQUIRE(std::ranges::any_of(pending.Blockers, [](const ServerSnapshotBlocker& blocker) { return blocker.Kind == ServerSnapshotBlockerKind::ManagedScriptWork && blocker.Count > 0; }));
    CHECK_FALSE(pending.State.has_value());
    CHECK(pending.Payload.empty());
    // The counter belongs to the first backend, not the other live engine's assembly statics
    CHECK_THROWS_AS(second->CreateSnapshot(timespan {std::chrono::seconds {10}}), DataBaseException);
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    {
        auto unlock_gate = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        REQUIRE(server->CallFunc(name("ReleaseSnapshotGate")));
    }
    coarse_sleep(std::chrono::milliseconds {50});
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    {
        auto unlock_gate = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        int32_t resumed = -1;
        REQUIRE(server->CallFunc(name("GetSnapshotResumed"), resumed));
        CHECK(resumed == 0);
    }
    pending = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
    REQUIRE(std::ranges::any_of(pending.Blockers, [](const ServerSnapshotBlocker& blocker) { return blocker.Kind == ServerSnapshotBlockerKind::ManagedScriptWork && blocker.Count > 0; }));
    server->ResumeSimulation();
    bool finished = false;
    for (uint32_t attempt = 0; attempt < 200 && !finished; attempt++) {
        coarse_sleep(std::chrono::milliseconds {5});
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock_gate = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        int32_t resumed = -1;
        REQUIRE(server->CallFunc(name("GetSnapshotResumed"), resumed));
        finished = resumed == 1;
    }
    REQUIRE(finished);
    CHECK_THROWS_AS(server->CreateSnapshot(timespan {std::chrono::seconds {10}}), DataBaseException);

    // A cancelled external Task must release its accounting once the real Mono continuation runs
    server->PauseSimulation();
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    {
        auto unlock_gate = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        REQUIRE(server->CallFunc(name("WaitForSnapshotGate")));
        REQUIRE(server->CallFunc(name("CancelSnapshotGate")));
    }
    auto backend = server->GetBackend<ManagedScriptBackend>(ScriptSystemBackend::MANAGED_BACKEND_INDEX);
    REQUIRE(backend);
    CHECK(backend->GetSnapshotWorkCount() > 0);
    server->ResumeSimulation();
    for (uint32_t attempt = 0; attempt < 200 && backend->GetSnapshotWorkCount() != 0; attempt++) {
        coarse_sleep(std::chrono::milliseconds {5});
    }
    CHECK(backend->GetSnapshotWorkCount() == 0);

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    {
        auto unlock_timer = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        hstring timer_name = name("OnSnapshotTimer");
        auto callback = server->FindFunc<void>(timer_name);
        REQUIRE(callback);
        uint32_t timer_id = server->TimeEventMngr.StartTimeEvent(server.get(), std::move(callback), timespan {std::chrono::hours {1}}, {}, {});
        auto captured = server->TimeEventMngr.CaptureSerializableTimeEvents();
        REQUIRE(captured.Events.size() == 1);
        CHECK(captured.UnresolvedFunctionCount == 0);
        CHECK(captured.Events[0].EventId == timer_id);
        server->TimeEventMngr.StopTimeEvent(server.get(), {}, timer_id);
        CHECK(server->TimeEventMngr.RestartTimeEvent(server.get(), timer_name, captured.Events[0].FuncAlternative, timer_id, timespan {std::chrono::milliseconds {20}}, {}, {}) == timer_id);
    }
    finished = false;
    for (uint32_t attempt = 0; attempt < 200 && !finished; attempt++) {
        coarse_sleep(std::chrono::milliseconds {5});
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock_timer = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        int32_t calls = 0;
        REQUIRE(server->CallFunc(name("GetSnapshotTimerCalls"), calls));
        finished = calls == 1;
    }
    REQUIRE(finished);

    auto client_settings = ClientEngineTests::MakeClientTestSettings();
    auto client_resources = ClientEngineTests::MakeClientTestResources();
    assemblies.AddTo(client_resources);
    auto client = ClientEngineTests::MakeClientEngine(client_settings, std::move(client_resources));
    auto shutdown_client = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ManagedRuntimeProbe::GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    client->MainLoop();

    auto mapper_settings = MapperTests::MakeMapperTestSettings();
    auto mapper_resources = MapperTests::MakeMapperTestResources();
    assemblies.AddTo(mapper_resources);
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&mapper_settings, std::move(mapper_resources), &GetApp()->MainWindow);
    auto shutdown_mapper = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });
    REQUIRE(mapper->CallFunc(mapper->Hashes.to_hashed_string("ManagedRuntimeProbe::GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
}

TEST_CASE("ManagedTimeEventsAwaitCompletion", "[managed][scripting][time-events]")
{
    ManagedRuntimeTestResources assemblies;
    auto settings = ServerEngineTests::MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Network.DisableInterthreadCommunication, true);
    auto resources = ServerEngineTests::MakeServerTestResources(ServerEngineTests::ServerTestScriptMode::Default, ManagedRuntimeTestResources::MakeProbeMetadata("server"));
    assemblies.AddTo(resources);
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources));
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    string startup_error = ServerEngineTests::WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    vector<timespan> scheduled_delays;
    TimeEventManager::DispatcherHooks hooks;
    hooks.Schedule = [&scheduled_delays](refcount_ptr<Entity>, uint32_t, timespan delay) { scheduled_delays.push_back(delay); };
    server->TimeEventMngr.SetDispatcherHooks(std::move(hooks));
    auto clear_hooks = scope_exit([&server]() noexcept { server->TimeEventMngr.ClearDispatcherHooks(); });

    auto name = [&server](string_view method) { return server->Hashes.to_hashed_string(strex("ManagedRuntimeProbe::{}", method).str()); };
    auto start = server->FindFunc<uint32_t, int32_t>(name("StartAwaitedTimer"));
    auto release = server->FindFunc<void>(name("ReleaseAwaitedTimer"));
    auto starts = server->FindFunc<int32_t>(name("GetTimerStarts"));
    auto finishes = server->FindFunc<int32_t>(name("GetTimerFinishes"));
    auto finish_frame = server->FindFunc<int64_t>(name("GetTimerFinishFrame"));
    auto errors = server->FindFunc<int32_t>(name("GetTimerErrors"));
    REQUIRE(start);
    REQUIRE(release);
    REQUIRE(starts);
    REQUIRE(finishes);
    REQUIRE(finish_frame);
    REQUIRE(errors);

    auto previous_exception_callback = exceptions::get_callback();
    exceptions::set_callback([previous = previous_exception_callback](string_view message, const stack_trace::catched_data& trace, bool fatal) {
        if (message.find("Awaited timer probe failure") == string_view::npos && previous) {
            previous(message, trace, fatal);
        }
    });
    auto restore_exception_callback = scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); });

    {
        auto value = server->FindFunc<int32_t>(name("GetCompletedTaskValue"));
        REQUIRE(value);
        int32_t completions = 0;
        bool success = false;
        REQUIRE(value.CallWithCompletion([&](bool result) {
            completions++;
            success = result;
        }));
        CHECK(value.GetResult() == 42);
        CHECK(completions == 1);
        CHECK(success);

        auto text = server->FindFunc<string>(name("GetCompletedTaskText"));
        REQUIRE(text);
        success = false;
        REQUIRE(text.CallWithCompletion([&](bool result) {
            completions++;
            success = result;
        }));
        CHECK(text.GetResult() == "completed");
        CHECK(completions == 2);
        CHECK(success);
    }

    for (int32_t index = 0; index < 11; index++) {
        int32_t mode = index == 10 ? 7 : index >= 7 ? index + 1 : index;
        CAPTURE(mode);
        REQUIRE(start.Call(mode));
        uint32_t id = start.GetResult();
        scheduled_delays.clear();
        auto events = server->GetTimeEvents();
        REQUIRE(events);
        auto found = std::ranges::find_if(*events, [id](const auto& event) { return event->Id == id; });
        REQUIRE(found != events->end());
        auto event = *found;
        CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id) == 1);
        CHECK(server->TimeEventMngr.CountTimeEvent(server, event->FuncName, {}) == 1);

        event->FireTime = server->GameTime.GetFrameTime();
        (void)server->TimeEventMngr.FireAndAdvance(server, id);

        REQUIRE(starts.Call());
        CHECK(starts.GetResult() == 1);
        CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id, false) == 1);
        CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id, true) == 0);
        CHECK(server->TimeEventMngr.CountTimeEvent(server, event->FuncName, {}, true) == 0);
        CHECK(server->TimeEventMngr.CountTimeEvent(server, event->FuncName, {}, false) == 1);
        REQUIRE(finishes.Call());
        CHECK(finishes.GetResult() == 0);

        if (mode == 0) {
            auto queue = server->FindFunc<uint32_t>(name("QueueAwaitedTimer"));
            REQUIRE(queue);

            bool queued = queue.Call();
            REQUIRE(queued);

            uint32_t queued_id = queue.GetResult();
            CHECK(server->TimeEventMngr.CountTimeEvent(server, event->FuncName, {}, true) == 1);
            CHECK(server->TimeEventMngr.CountTimeEvent(server, event->FuncName, {}, false) == 2);
            CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, queued_id) == 1);
            CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, queued_id, false) == 1);

            server->TimeEventMngr.StopTimeEvent(server, {}, queued_id);
            scheduled_delays.clear();

            server->TimeEventMngr.ModifyTimeEvent(server, {}, id, timespan {std::chrono::seconds {7}}, vector<any_t> {any_t {"external"}});
        }
        else if (mode == 5) {
            server->TimeEventMngr.StopTimeEvent(server, {}, id);
            CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id, false) == 0);
        }

        if (mode == 6) {
            server->TimeEventMngr.ClearTimeEvents();
            CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id, false) == 0);
        }

        for (int32_t frame = 0; frame < 3; frame++) {
            server->FrameAdvance();
        }

        CHECK(scheduled_delays.empty());
        event->FireTime = server->GameTime.GetFrameTime();
        (void)server->TimeEventMngr.FireAndAdvance(server, id);
        server->TimeEventMngr.ProcessTimeEvents();
        optional<timespan> next;
        auto ready = server->TimeEventMngr.CollectReadyTimeEvents(next);
        CHECK(std::ranges::none_of(ready, [id](const auto& candidate) { return candidate.Event->Id == id; }));
        REQUIRE(starts.Call());
        CHECK(starts.GetResult() == 1);

        if (mode == 7) {
            break;
        }

        REQUIRE(release.Call());

        for (int32_t frame = 0; frame < 4; frame++) {
            server->FrameAdvance();
        }

        REQUIRE(finishes.Call());
        CHECK(finishes.GetResult() == 1);
        REQUIRE(errors.Call());
        CHECK(errors.GetResult() == (mode == 4 || mode == 8 || mode == 9 || mode == 10 ? 1 : 0));

        if (mode == 5) {
            CHECK(scheduled_delays.empty());
        }
        else {
            REQUIRE(scheduled_delays.size() == 1);
            CHECK(scheduled_delays.front() == TimeEventManager::MIN_REPEAT_TIME);
        }

        auto next_delay = server->TimeEventMngr.FireAndAdvance(server, id);

        if (mode == 0 || mode == 1 || mode == 8 || mode == 9) {
            CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id) == 1);
            CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id, false) == 1);
            REQUIRE(next_delay.has_value());
            CHECK(next_delay.value() > timespan {});
            timespan interval {std::chrono::seconds {mode == 0 ? 7 : 5}};
            CHECK(event->RepeatDuration == interval);
            REQUIRE(finish_frame.Call());
            CHECK(event->FireTime.nanoseconds() == finish_frame.GetResult() + interval.nanoseconds());
            REQUIRE(event->Data.size() == 1);
            CHECK(event->Data.front() == any_t {mode == 0 ? "external" : mode == 9 ? "before await" : "after await"});
        }
        else {
            CHECK(server->TimeEventMngr.CountTimeEvent(server, {}, id, false) == 0);
            CHECK_FALSE(next_delay.has_value());
        }

        server->TimeEventMngr.StopTimeEvent(server, {}, id);
    }

    {
        auto owner = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false).hold_ref();
        auto start_owned = server->FindFunc<uint32_t, ptr<Critter>>(name("StartAwaitedCritterTimer"));
        REQUIRE(start_owned);
        REQUIRE(start_owned.Call(owner));
        uint32_t id = start_owned.GetResult();
        auto events = owner->GetTimeEvents();
        REQUIRE(events);
        REQUIRE(events->size() == 1);
        auto event = events->front();
        event->FireTime = server->GameTime.GetFrameTime();
        CHECK_FALSE(server->TimeEventMngr.FireAndAdvance(owner, id).has_value());
        scheduled_delays.clear();
        server->CrMngr.DestroyCritter(owner);
        REQUIRE(release.Call());

        for (int32_t frame = 0; frame < 4; frame++) {
            server->FrameAdvance();
        }

        CHECK(scheduled_delays.empty());
        CHECK_FALSE(server->TimeEventMngr.FireAndAdvance(owner, id).has_value());
    }

    server->Unlock();
    unlock.release();
    server->Shutdown();
    shutdown.release();
    CHECK(server->TimeEventMngr.GetDiagnostics().EventCount == 0);
    CHECK(server->GetRefCount() == 1);

    {
        auto live_resources = ServerEngineTests::MakeServerTestResources(ServerEngineTests::ServerTestScriptMode::Default, ManagedRuntimeTestResources::MakeProbeMetadata("server"));
        assemblies.AddTo(live_resources);
        auto live_server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(live_resources));
        auto shutdown_live = scope_exit([&live_server]() noexcept { safe_call([&live_server] { live_server->Shutdown(); }); });
        REQUIRE(ServerEngineTests::WaitForServerStart(live_server).empty());
        REQUIRE(live_server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock_live = scope_exit([&live_server]() noexcept { safe_call([&live_server] { live_server->Unlock(); }); });
        auto live_name = [&live_server](string_view method) { return live_server->Hashes.to_hashed_string(strex("ManagedRuntimeProbe::{}", method).str()); };
        auto live_start = live_server->FindFunc<uint32_t, ptr<Critter>>(live_name("StartAwaitedCritterTimer"));
        auto live_starts = live_server->FindFunc<int32_t>(live_name("GetTimerStarts"));
        auto live_finishes = live_server->FindFunc<int32_t>(live_name("GetTimerFinishes"));
        auto live_finish_frame = live_server->FindFunc<int64_t>(live_name("GetTimerFirstFinishFrame"));
        auto live_second_start = live_server->FindFunc<int64_t>(live_name("GetTimerSecondStartFrame"));
        auto live_release = live_server->FindFunc<void>(live_name("ReleaseAwaitedTimer"));
        REQUIRE(live_start);
        REQUIRE(live_starts);
        REQUIRE(live_finishes);
        REQUIRE(live_finish_frame);
        REQUIRE(live_second_start);
        REQUIRE(live_release);
        {
            auto owner = live_server->CreateCritter(live_server->Hashes.to_hashed_string("UnitTestRat"), false).hold_ref();
            REQUIRE(live_start.Call(owner));
            uint32_t id = live_start.GetResult();
            auto events = owner->GetTimeEvents();
            REQUIRE(events);
            REQUIRE(events->size() == 1);
            auto event = events->front();
            live_server->Unlock();
            unlock_live.release();

            auto wait_for = [&](const function<bool()>& condition) {
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {30};

                while (std::chrono::steady_clock::now() < deadline) {
                    {
                        REQUIRE(live_server->Lock(timespan {std::chrono::seconds {10}}));
                        auto unlock_check = scope_exit([&live_server]() noexcept { safe_call([&live_server] { live_server->Unlock(); }); });

                        if (condition()) {
                            return true;
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds {10});
                }

                return false;
            };
            REQUIRE(wait_for([&] { return live_starts.Call() && live_starts.GetResult() == 1; }));
            REQUIRE(live_server->Lock(timespan {std::chrono::seconds {10}}));
            auto unlock_modify = scope_exit([&live_server]() noexcept { safe_call([&live_server] { live_server->Unlock(); }); });
            live_server->RequireCurrentSyncContext()->SyncEntity(owner);
            timespan interval {std::chrono::milliseconds {200}};
            live_server->TimeEventMngr.ModifyTimeEvent(owner, {}, id, interval, std::nullopt);
            live_server->Unlock();
            unlock_modify.release();
            std::this_thread::sleep_for(std::chrono::milliseconds {300});
            REQUIRE(live_server->Lock(timespan {std::chrono::seconds {10}}));
            auto unlock_release = scope_exit([&live_server]() noexcept { safe_call([&live_server] { live_server->Unlock(); }); });
            live_server->RequireCurrentSyncContext()->SyncEntity(owner);
            REQUIRE(live_starts.Call());
            CHECK(live_starts.GetResult() == 1);
            CHECK(live_server->TimeEventMngr.CountTimeEvent(owner, {}, id, false) == 1);
            CHECK(live_server->TimeEventMngr.CountTimeEvent(owner, {}, id, true) == 0);
            REQUIRE(live_release.Call());
            live_server->Unlock();
            unlock_release.release();
            REQUIRE(wait_for([&] { return live_starts.Call() && live_starts.GetResult() >= 2; }));
            REQUIRE(live_server->Lock(timespan {std::chrono::seconds {10}}));
            auto unlock_stop = scope_exit([&live_server]() noexcept { safe_call([&live_server] { live_server->Unlock(); }); });
            REQUIRE(live_finish_frame.Call());
            REQUIRE(live_second_start.Call());
            REQUIRE(live_finishes.Call());
            CHECK(live_finishes.GetResult() >= 2);
            CHECK(live_second_start.GetResult() >= live_finish_frame.GetResult() + interval.nanoseconds());
            live_server->RequireCurrentSyncContext()->SyncEntity(owner);
            live_server->TimeEventMngr.StopTimeEvent(owner, {}, id);
            live_server->CrMngr.DestroyCritter(owner);
            event.reset();
            live_server->Unlock();
            unlock_stop.release();
        }

        live_server->Shutdown();
        shutdown_live.release();
        CHECK(live_server->GetRefCount() == 1);
    }

    auto client_settings = ClientEngineTests::MakeClientTestSettings();
    auto client_resources = ClientEngineTests::MakeClientTestResources();
    assemblies.AddTo(client_resources);
    auto client = ClientEngineTests::MakeClientEngine(client_settings, std::move(client_resources));
    auto shutdown_client = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto client_name = [&client](string_view method) { return client->Hashes.to_hashed_string(strex("ManagedRuntimeProbe::{}", method).str()); };
    auto client_start = client->FindFunc<uint32_t, int32_t>(client_name("StartAwaitedTimer"));
    auto client_release = client->FindFunc<void>(client_name("ReleaseAwaitedTimer"));
    auto client_starts = client->FindFunc<int32_t>(client_name("GetTimerStarts"));
    auto client_finish_frame = client->FindFunc<int64_t>(client_name("GetTimerFinishFrame"));
    REQUIRE(client_start);
    REQUIRE(client_release);
    REQUIRE(client_starts);
    REQUIRE(client_finish_frame);
    REQUIRE(client_start.Call(0));
    uint32_t client_id = client_start.GetResult();
    auto client_events = client->GetTimeEvents();
    REQUIRE(client_events);
    auto client_found = std::ranges::find_if(*client_events, [client_id](const auto& candidate) { return candidate->Id == client_id; });
    REQUIRE(client_found != client_events->end());
    auto client_event = *client_found;
    client_event->FireTime = client->GameTime.GetFrameTime();
    client->MainLoop();
    CHECK(client->TimeEventMngr.CountTimeEvent(client, {}, client_id, false) == 1);
    CHECK(client->TimeEventMngr.CountTimeEvent(client, {}, client_id, true) == 0);
    client_event->FireTime = client->GameTime.GetFrameTime();
    client->MainLoop();
    REQUIRE(client_starts.Call());
    CHECK(client_starts.GetResult() == 1);

    timespan client_interval {std::chrono::seconds {7}};
    client->TimeEventMngr.ModifyTimeEvent(client, {}, client_id, client_interval, std::nullopt);
    REQUIRE(client_release.Call());

    for (int32_t frame = 0; frame < 4; frame++) {
        client->MainLoop();
    }

    REQUIRE(client_finish_frame.Call());
    CHECK(client_event->FireTime.nanoseconds() == client_finish_frame.GetResult() + client_interval.nanoseconds());
    REQUIRE(client_starts.Call());
    CHECK(client_starts.GetResult() == 1);
    client->TimeEventMngr.StopTimeEvent(client, {}, client_id);
    client_event.reset();
}

#if FO_DATABASE_SQLITE
TEST_CASE("ManagedSnapshotRestoresTimersAndMovementInAFreshSession", "[managed][scripting][snapshot]")
{
    ManagedRuntimeTestResources assemblies;
    string storage_root = ServerEngineTests::MakeTempServerResourceDir("managed_session_restore");
    auto cleanup = scope_exit([&storage_root]() noexcept { (void)fs::remove_dir_tree(storage_root); });
    ServerSnapshotState state;
    vector<uint8_t> payload;
    ident_t critter_id;
    mpos expected_end;

    {
        auto settings = ServerEngineTests::MakeServerTestSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}/source", storage_root).str());
        auto resources = ServerEngineTests::MakeServerTestResources();
        assemblies.AddTo(resources);
        auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources));
        auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
        REQUIRE(ServerEngineTests::WaitForServerStart(server).empty());
        server->PauseSimulation();
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
            hstring map_pids[] = {server->Hashes.to_hashed_string("UnitTestMap")};
            auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestLocation"), map_pids);
            server->EntityMngr.MakePersistent(location, true, true);
            auto map = location->GetMapByIndex(0);
            REQUIRE(map);
            auto critter = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false);
            server->MapMngr.AddCritterToMap(critter, map.get(), ServerEngineTests::SERVER_TEST_MOVE_START_HEX, mdir {}, {});
            server->EntityMngr.MakePersistent(critter, true, true);
            critter_id = critter->GetId();
            auto moving = ServerEngineTests::MakeServerMovementContext(map->GetSize(), critter->GetHex(), server->GameTime.GetFrameTime());
            expected_end = moving->GetEndHex();
            server->StartCritterMoving(critter, std::move(moving), nullptr);
            auto global = server->FindFunc<void>(server->Hashes.to_hashed_string("ManagedRuntimeProbe::OnSnapshotTimer"));
            REQUIRE(global);
            (void)server->TimeEventMngr.StartTimeEvent(server.get(), std::move(global), timespan {std::chrono::milliseconds {100}}, {}, {});
            array args {ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = server->GetBaseType("Critter")}};
            nptr<ScriptFuncDesc> desc = server->FindFunc(server->Hashes.to_hashed_string("ManagedRuntimeProbe::OnSnapshotCritterTimer"), const_span<ComplexTypeDesc> {args});
            REQUIRE(desc);
            (void)server->TimeEventMngr.StartTimeEvent(critter, ScriptFunc<void, ptr<ScriptSelfEntity>> {desc}, timespan {std::chrono::milliseconds {100}}, {}, {});
        }
        auto captured = server->CreateSnapshot(timespan {std::chrono::seconds {10}});
        REQUIRE(captured.Blockers.empty());
        REQUIRE(captured.State.has_value());
        state = std::move(*captured.State);
        payload = std::move(captured.Payload);
        REQUIRE(state.TimeEvents.size() == 2);
        REQUIRE(state.CritterMovements.size() == 1);
    }

    auto settings = ServerEngineTests::MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Server.DbStorage, strex("DbSQLite {}/restored", storage_root).str());
    auto resources = ServerEngineTests::MakeServerTestResources();
    assemblies.AddTo(resources);
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources), ServerSnapshotRestore {state, payload});
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(ServerEngineTests::WaitForServerStart(server).empty());
    bool complete = false;
    nanotime deadline = nanotime::now() + std::chrono::seconds {5};
    while (!complete && nanotime::now() < deadline) {
        coarse_sleep(std::chrono::milliseconds {5});
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        auto critter = server->EntityMngr.GetCritter(critter_id);
        REQUIRE(critter);
        server->RequireCurrentSyncContext()->SyncEntity(critter);
        int32_t calls = 0;
        REQUIRE(server->CallFunc(server->Hashes.to_hashed_string("ManagedRuntimeProbe::GetSnapshotTimerCalls"), calls));
        complete = calls == 2 && !critter->IsMoving() && critter->GetHex() == expected_end;
    }
    CHECK(complete);
}
#endif

FO_END_NAMESPACE

#endif
