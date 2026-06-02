//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "Movement.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    constexpr msize SERVER_TEST_MAP_SIZE {200, 200};
    constexpr mpos SERVER_TEST_MOVE_START_HEX {20, 20};
    constexpr uint16_t SERVER_TEST_MOVE_SPEED = 100;
    const vector<mdir> SERVER_TEST_MOVE_STEPS {hdir::NorthEast, hdir::NorthEast, hdir::East, hdir::East, hdir::SouthEast};
    const vector<uint16_t> SERVER_TEST_MOVE_CONTROL_STEPS {2, 4, 5};

    class StableTestConnection final : public NetworkServerConnection
    {
    public:
        explicit StableTestConnection(ServerNetworkSettings& settings) :
            NetworkServerConnection(settings)
        {
            FO_STACK_TRACE_ENTRY();

            _host = "StableTest";
        }
        StableTestConnection(const StableTestConnection&) = delete;
        StableTestConnection(StableTestConnection&&) noexcept = delete;
        auto operator=(const StableTestConnection&) = delete;
        auto operator=(StableTestConnection&&) noexcept = delete;
        ~StableTestConnection() override = default;

    protected:
        void DispatchImpl() override
        {
            FO_STACK_TRACE_ENTRY();
        }

        void DisconnectImpl() override
        {
            FO_STACK_TRACE_ENTRY();
        }
    };

    static auto CreateStableTestConnection(ServerNetworkSettings& settings) -> shared_ptr<NetworkServerConnection>
    {
        FO_STACK_TRACE_ENTRY();

        return SafeAlloc::MakeShared<StableTestConnection>(settings);
    }

    static auto MakeServerTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = DataWriter(map_data);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        return map_data;
    }

    static auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), proto_engine.GetPropertyRegistrator(type_name)};
        proto.SetSize(map_size);
        proto.GetProperties().StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WritePtr(proto_name.data(), proto_name.length());
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        writer.WritePtr(props_data.data(), props_data.size());

        return protos_data;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ServerEngineScripts",
            {
                {"Scripts/ServerEngineTest.fos", R"(
namespace ServerEngineTest
{
    int InitCalls = 0;
    int CritterInitCalls = 0;
    int64 LastCritterId = 0;
    bool LastCritterFirstTime = false;
    int ManualCalls = 0;
    int AdminCallCounter = 0;
    int ModuleInitOrder = 0;
    int ImmediateInitOrder = 0;
    int DeferredInitOrder = 0;

    [[ModuleInit]]
    void RegisterHooks()
    {
        ImmediateInitOrder = ++ModuleInitOrder;
        Game.OnInit.Subscribe(OnInit);
        Game.OnCritterInit.Subscribe(OnCritterInit);
    }

    [[ModuleInit(2)]]
    void RunDeferredInit()
    {
        DeferredInitOrder = ++ModuleInitOrder;
    }

    [[Event]]
    void OnInit()
    {
        InitCalls++;
    }

    [[Event]]
    void OnCritterInit(Critter cr, bool firstTime)
    {
        CritterInitCalls++;
        LastCritterId = cr.Id.value;
        LastCritterFirstTime = firstTime;
    }

    void UnitTestNoop() {}

    void UnitTestMarkManualCall()
    {
        ManualCalls++;
    }

    void UnitTestResetAdminCallCounter()
    {
        AdminCallCounter = 0;
    }

    void UnitTestAdminEntryImpl()
    {
        AdminCallCounter++;
    }

    [[AdminRemoteCall]]
    void UnitTestAdminEntry()
    {
        UnitTestAdminEntryImpl();
    }

    void UnitTestRegularEntry()
    {
        AdminCallCounter += 100;
    }

    void UnitTestInvokeAdminEntry()
    {
        UnitTestAdminEntryImpl();
    }

    int UnitTestGetInitCalls()
    {
        return InitCalls;
    }

    int UnitTestGetCritterInitCalls()
    {
        return CritterInitCalls;
    }

    int64 UnitTestGetLastCritterId()
    {
        return LastCritterId;
    }

    bool UnitTestGetLastCritterFirstTime()
    {
        return LastCritterFirstTime;
    }

    int UnitTestGetManualCalls()
    {
        return ManualCalls;
    }

    int UnitTestGetAdminCallCounter()
    {
        return AdminCallCounter;
    }

    int UnitTestGetImmediateInitOrder()
    {
        return ImmediateInitOrder;
    }

    int UnitTestGetDeferredInitOrder()
    {
        return DeferredInitOrder;
    }

    int UnitTestSumArray(int[] values)
    {
        int total = 0;

        for (int i = 0; i < values.length(); i++) {
            total += values[i];
        }

        return total;
    }

    void UnitTestMutateArray(int[] & values)
    {
        if (values.length() > 0) {
            values[0] += 10;
        }

        values.insertLast(77);
    }

    int UnitTestSumDict(dict<int, int> values)
    {
        int total = 0;

        for (int i = 0; i < values.length(); i++) {
            total += values.getKey(i);
            total += values.getValue(i);
        }

        return total;
    }

    int64 UnitTestGetCritterIdValue(Critter cr)
    {
        return cr !is null ? cr.Id.value : 0;
    }

    bool UnitTestMatchesHash(hstring value)
    {
        return value == "UnitTestHash".hstr();
    }

}
)"},
            },
            [](string_view message) {
                const auto message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static auto MakeServerTestResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerEngineCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto location_type = proto_engine.Hashes.ToHashedString("Location");
        const auto map_type = proto_engine.Hashes.ToHashedString("Map");
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");
        const auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestRat");
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "UnitTestLocation");
        const auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "UnitTestMap", SERVER_TEST_MAP_SIZE);
        const auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        const auto fomap_blob = MakeEmptyMapBlob();
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ServerEngineTest.fopro-bin-server", proto_blob);
        runtime_source->AddFile("UnitTestLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("UnitTestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("UnitTestItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("UnitTestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("ServerEngineTest.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForServerStart(ServerEngine* server) -> string
    {
        FO_RUNTIME_ASSERT(server);

        for (int32_t i = 0; i < 6000; i++) {
            if (server->IsStarted()) {
                return {};
            }
            if (server->IsStartingError()) {
                return "ServerEngine startup failed";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }

        return "ServerEngine startup timed out";
    }

    static auto CreateLoggedPlayer(ServerEngine* server, string_view name) -> Player*
    {
        FO_RUNTIME_ASSERT(server);

        auto unlogined_player = server->CreateUnloginedPlayer(CreateStableTestConnection(server->Settings));

        if (unlogined_player == nullptr) {
            return nullptr;
        }

        unlogined_player->SetName(name);
        unlogined_player->SetLastControlledCritterId(ident_t {1});
        return server->LoginPlayerToNewRecord(unlogined_player);
    }

    static auto CreateStandalonePlayer(ServerEngine* server, string_view name) -> refcount_ptr<Player>
    {
        FO_RUNTIME_ASSERT(server);

        auto connection = SafeAlloc::MakeUnique<ServerConnection>(server->Settings, NetworkServer::CreateDummyConnection(server->Settings));
        auto player = SafeAlloc::MakeRefCounted<Player>(server, ident_t {}, std::move(connection));
        player->SetName(name);
        return player;
    }

    static auto MakeServerMovementContext(msize map_size, mpos start_hex, nanotime start_time) -> refcount_ptr<MovingContext>
    {
        return SafeAlloc::MakeRefCounted<MovingContext>(map_size, SERVER_TEST_MOVE_SPEED, SERVER_TEST_MOVE_STEPS, SERVER_TEST_MOVE_CONTROL_STEPS, start_time, timespan {}, start_hex, ipos16 {}, ipos16 {});
    }

    static auto WaitForUnlockedServerCondition(ServerEngine* server, bool& locked, const function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        FO_RUNTIME_ASSERT(server);
        FO_RUNTIME_ASSERT(locked);

        server->Unlock();
        locked = false;

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds {5});

            if (!server->Lock(timespan {std::chrono::seconds {10}})) {
                continue;
            }

            locked = true;

            if (condition()) {
                return true;
            }

            server->Unlock();
            locked = false;
        }

        if (!locked) {
            if (!server->Lock(timespan {std::chrono::seconds {10}})) {
                return false;
            }

            locked = true;
        }

        return condition();
    }
}

TEST_CASE("ServerEngineStartsAndCreatesCritter")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    REQUIRE(server->GetProtoCritter(critter_pid) != nullptr);

    const auto critter_count = server->EntityMngr.GetCrittersCount();
    const auto entity_count = server->EntityMngr.GetEntitiesCount();

    auto cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);

    const auto cr_id = cr->GetId();
    CHECK(cr->GetProtoId() == critter_pid);
    CHECK(server->EntityMngr.GetCritter(cr_id) == cr);
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count + 1);
    CHECK(server->EntityMngr.GetEntitiesCount() > entity_count);

    auto* player = CreateLoggedPlayer(server.get(), "UnitTestPlayer");
    REQUIRE(player != nullptr);

    const auto player_id = player->GetId();
    CHECK(player_id != ident_t {});
    CHECK(server->EntityMngr.GetPlayer(player_id) == player);

    server->CrMngr.DestroyCritter(cr);

    CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count);
}

TEST_CASE("ServerEngineHandlesPlayerCritterUnloadAndMissingProto")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    SECTION("PlayerControlledCritterCanBeUnloaded")
    {
        const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
        const auto critter_count = server->EntityMngr.GetCrittersCount();

        auto cr = server->CreateCritter(critter_pid, true);
        REQUIRE(cr != nullptr);

        const auto cr_id = cr->GetId();
        CHECK(cr->GetControlledByPlayer());
        CHECK(server->EntityMngr.GetCritter(cr_id) == cr);

        server->UnloadCritter(cr);

        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
        CHECK(server->EntityMngr.GetCrittersCount() == critter_count);
    }

    SECTION("MissingProtoThrows")
    {
        const auto missing_pid = server->Hashes.ToHashedString("MissingUnitTestCritter");

        CHECK_THROWS(server->CreateCritter(missing_pid, false));
        CHECK_THROWS(server->CreateCritter(missing_pid, true));
    }
}

TEST_CASE("ServerEngineScriptModuleInitAndEventsAreCallable")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto get_func_name = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    int init_calls = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetInitCalls"), init_calls));
    CHECK(init_calls == 1);

    int manual_calls = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 0);

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestMarkManualCall")));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 1);

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    auto cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);

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

TEST_CASE("ServerEngineModuleInitAttributePriorityIsRespected")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto get_func_name = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    int immediate_init_order = 0;
    int deferred_init_order = 0;

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetImmediateInitOrder"), immediate_init_order));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetDeferredInitOrder"), deferred_init_order));

    CHECK(immediate_init_order == 1);
    CHECK(deferred_init_order == 2);
}

TEST_CASE("ServerEngineAdminRemoteCallsAreAllowlisted")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto get_func_name = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestResetAdminCallCounter")));
    REQUIRE(server->CallAdminFunc(get_func_name("ServerEngineTest::UnitTestAdminEntry")));

    int admin_call_counter = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetAdminCallCounter"), admin_call_counter));
    CHECK(admin_call_counter == 1);

    CHECK_FALSE(server->CallAdminFunc(get_func_name("ServerEngineTest::UnitTestRegularEntry")));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetAdminCallCounter"), admin_call_counter));
    CHECK(admin_call_counter == 1);
}

TEST_CASE("ServerEngineScriptCallsMarshalContainersAndEntities")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto get_func_name = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    vector<int32_t> values {1, 2, 3};
    auto sum_array_func = server->FindFunc<int32_t, vector<int32_t>>(get_func_name("ServerEngineTest::UnitTestSumArray"));
    REQUIRE(sum_array_func);
    REQUIRE(sum_array_func.Call(values));
    CHECK(sum_array_func.GetResult() == 6);

    auto mutate_array_func = server->FindFunc<void, vector<int32_t>&>(get_func_name("ServerEngineTest::UnitTestMutateArray"));
    REQUIRE(mutate_array_func);
    REQUIRE(mutate_array_func.Call(values));
    CHECK(values == vector<int32_t> {11, 2, 3, 77});

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    auto cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);

    auto critter_id_func = server->FindFunc<int64_t, Critter*>(get_func_name("ServerEngineTest::UnitTestGetCritterIdValue"));
    REQUIRE(critter_id_func);
    REQUIRE(critter_id_func.Call(cr));
    CHECK(critter_id_func.GetResult() == cr->GetId().underlying_value());

    auto matches_hash_func = server->FindFunc<bool, hstring>(get_func_name("ServerEngineTest::UnitTestMatchesHash"));
    REQUIRE(matches_hash_func);
    REQUIRE(matches_hash_func.Call(server->Hashes.ToHashedString("UnitTestHash")));
    CHECK(matches_hash_func.GetResult());

    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("ServerEngineProcessesOverdueMovementByHex")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
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

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    const auto location_pid = server->Hashes.ToHashedString("UnitTestLocation");
    const auto map_pid = server->Hashes.ToHashedString("UnitTestMap");

    SECTION("CompletesWholeRoute")
    {
        vector<hstring> map_pids {map_pid};
        auto loc = server->MapMngr.CreateLocation(location_pid, map_pids);
        REQUIRE(loc != nullptr);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (loc != nullptr && !loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto* map = loc->GetMapByIndex(0);
        REQUIRE(map != nullptr);

        auto cr = server->CreateCritter(critter_pid, false);
        REQUIRE(cr != nullptr);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr, moving, nullptr);
        REQUIRE(cr->IsMoving());

        REQUIRE(WaitForUnlockedServerCondition(server.get(), locked, [&cr] { return !cr->IsMoving(); }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::Success);
        CHECK(cr->GetHex() == path_hexes.back());

        const auto* finished_moving = cr->GetMovingContext();
        REQUIRE(finished_moving != nullptr);
        CHECK(finished_moving->GetCompleteReason() == MovingState::Success);
    }

    SECTION("CompletesWholeRouteForDeadCritter")
    {
        vector<hstring> map_pids {map_pid};
        auto* loc = server->MapMngr.CreateLocation(location_pid, map_pids);
        REQUIRE(loc != nullptr);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (loc != nullptr && !loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto* map = loc->GetMapByIndex(0);
        REQUIRE(map != nullptr);

        auto* cr = server->CreateCritter(critter_pid, false);
        REQUIRE(cr != nullptr);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);
        cr->SetCondition(CritterCondition::Dead);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr, moving, nullptr);
        REQUIRE(cr->IsMoving());

        REQUIRE(WaitForUnlockedServerCondition(server.get(), locked, [&cr] { return !cr->IsMoving(); }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::Success);
        CHECK(cr->GetHex() == path_hexes.back());
        CHECK(cr->IsDead());

        const auto* finished_moving = cr->GetMovingContext();
        REQUIRE(finished_moving != nullptr);
        CHECK(finished_moving->GetCompleteReason() == MovingState::Success);
    }

    SECTION("StopsAtFirstBlockedHexWithoutTeleport")
    {
        vector<hstring> map_pids {map_pid};
        auto loc = server->MapMngr.CreateLocation(location_pid, map_pids);
        REQUIRE(loc != nullptr);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (loc != nullptr && !loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto* map = loc->GetMapByIndex(0);
        REQUIRE(map != nullptr);

        auto cr = server->CreateCritter(critter_pid, false);
        REQUIRE(cr != nullptr);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto expected_stop_hex = path_hexes[1];
        const auto blocked_hex = path_hexes[2];

        map->SetHexManualBlock(blocked_hex, true, true);
        auto unblock_hex = scope_exit([map, blocked_hex]() noexcept {
            safe_call([map, blocked_hex] {
                if (!map->IsDestroyed()) {
                    map->SetHexManualBlock(blocked_hex, false, false);
                }
            });
        });

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr, moving, nullptr);
        REQUIRE(cr->IsMoving());

        REQUIRE(WaitForUnlockedServerCondition(server.get(), locked, [&cr] { return !cr->IsMoving(); }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::HexBusy);
        CHECK(cr->GetHex() == expected_stop_hex);

        const auto* finished_moving = cr->GetMovingContext();
        REQUIRE(finished_moving != nullptr);
        CHECK(finished_moving->GetCompleteReason() == MovingState::HexBusy);
        CHECK(finished_moving->GetPreBlockHex() == expected_stop_hex);
        CHECK(finished_moving->GetBlockHex() == blocked_hex);
    }
}

// ============================================================================
// Per-entity sync rail (multithreading): SyncContext minimal-cover / escalation /
// ValidateAccess hierarchy walk against REAL ServerEntity instances. The primitive
// tests in Test_EntitySync.cpp cannot reach this — they have no entity hierarchy
// (see its "ValidateAccessFailsOnUnheldLock" comment). World is built under the
// engine singleton lock, then the lock is released because SyncEntities refuses to
// run while a singleton lock is held (deadlock prevention).
// ============================================================================

TEST_CASE("ServerEngineSyncContextEntityCover")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    const auto location_pid = server->Hashes.ToHashedString("UnitTestLocation");
    const auto map_pid = server->Hashes.ToHashedString("UnitTestMap");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto* loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    REQUIRE(loc != nullptr);
    auto* map = loc->GetMapByIndex(0);
    REQUIRE(map != nullptr);

    auto* cr_a = server->CreateCritter(critter_pid, false);
    auto* cr_b = server->CreateCritter(critter_pid, false);
    auto* cr_c = server->CreateCritter(critter_pid, false);
    REQUIRE(cr_a != nullptr);
    REQUIRE(cr_b != nullptr);
    REQUIRE(cr_c != nullptr);

    server->MapMngr.TransferToMap(cr_a, map, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map, mpos {12, 12}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_c, map, mpos {14, 14}, mdir {}, std::nullopt);

    // Parent wiring (Critter._parent = Map) must be established for the cover logic.
    REQUIRE(cr_a->GetParentRaw() == static_cast<ServerEntity*>(map));
    REQUIRE(cr_b->GetParentRaw() == static_cast<ServerEntity*>(map));
    REQUIRE(cr_c->GetParentRaw() == static_cast<ServerEntity*>(map));

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

    // Two checks, two layers: `ctx.ValidateAccess(e)` reports whether e's OWN lock is held (used to
    // pin the exact cover, e.g. that escalation drops the children's own locks); `IsEntityAccessValid(e)`
    // is the production access gate — the hierarchy walk that accepts e's own OR any ancestor lock,
    // plus the null/empty short-circuits.

    // Empty context: no held locks → the access gate grants everything, including a null entity.
    CHECK(ctx.IsEmpty());
    CHECK(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(nullptr));

    // Single critter: only its own lock is held; access is granted to it but not siblings or the map.
    {
        vector<ServerEntity*> one {cr_a};
        ctx.SyncEntities(one);
        CHECK_FALSE(ctx.IsEmpty());
        CHECK(ctx.ValidateAccess(cr_a)); // cr_a's own lock is held
        CHECK_FALSE(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(cr_a));
        CHECK_FALSE(IsEntityAccessValid(cr_b)); // sibling: no lock in its chain is held
        CHECK_FALSE(IsEntityAccessValid(cr_c));
        CHECK_FALSE(IsEntityAccessValid(map));

        // Negative: the binding-level gate (ServerEntity::ValidateAccess, hit by every script
        // property read / method call) must THROW on an uncovered entity, not merely report false —
        // an unsynced access is a rejected error, never silently allowed. The covered entity passes.
        CHECK_NOTHROW(cr_a->ValidateAccess());
        CHECK_THROWS_AS(cr_b->ValidateAccess(), ScriptException);
        CHECK_THROWS_AS(map->ValidateAccess(), ScriptException);
    }
    ctx.Release();
    CHECK(ctx.IsEmpty());

    // Two siblings sharing a map escalate to the SINGLE map lock: their own locks are dropped, the
    // map's own lock is held, and the hierarchy walk grants access to every critter on the map.
    {
        vector<ServerEntity*> both {cr_a, cr_b};
        ctx.SyncEntities(both);
        CHECK(ctx.ValidateAccess(map)); // escalated → the map's own lock is held
        CHECK_FALSE(ctx.ValidateAccess(cr_a)); // the siblings' own locks were dropped
        CHECK_FALSE(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(cr_a)); // accessible via the map ancestor lock
        CHECK(IsEntityAccessValid(cr_b));
        CHECK(IsEntityAccessValid(cr_c)); // unrequested sibling, also covered by the map lock
        CHECK(IsEntityAccessValid(map));
    }
    ctx.Release();

    // Explicit {critter, its own map}: BOTH locks are kept (no parent-cover reduction), so the
    // critter survives a reparent that a parent-only cover would strand.
    {
        vector<ServerEntity*> pair {cr_a, map};
        ctx.SyncEntities(pair);
        CHECK(ctx.ValidateAccess(cr_a)); // own lock kept
        CHECK(ctx.ValidateAccess(map)); // map lock kept
        CHECK(IsEntityAccessValid(cr_a));
        CHECK(IsEntityAccessValid(cr_c)); // map lock covers siblings
    }
    ctx.Release();

    // EnsureEntitySynced is a no-op in fully empty/unrestricted mode: registering a fresh entity must
    // not accidentally turn the scope into a partial lock set. Once a scope is restricted (including
    // a singleton Game.Lock-style lock), it adds one lock and is idempotent when already covered.
    ctx.EnsureEntitySynced(cr_a);
    CHECK(ctx.IsEmpty());
    CHECK(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(cr_b));

    ctx.LockSingleton(server->GetEntityLock());
    ctx.EnsureEntitySynced(cr_a);
    CHECK(ctx.ValidateAccess(cr_a));
    CHECK(IsEntityAccessValid(cr_a));
    CHECK_FALSE(IsEntityAccessValid(cr_b));
    ctx.Release();

    ctx.SyncEntity(cr_a);
    ctx.EnsureEntitySynced(cr_a);
    CHECK(ctx.ValidateAccess(cr_a));
    ctx.EnsureEntitySynced(cr_a);
    CHECK(ctx.ValidateAccess(cr_a));
    ctx.EnsureEntitySynced(cr_b);
    CHECK(ctx.ValidateAccess(cr_b));
    CHECK(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(cr_b));
    ctx.Release();

    // SyncEntity REPLACES the held set (yield-on-Sync), it does not accumulate.
    ctx.SyncEntity(cr_a);
    CHECK(ctx.ValidateAccess(cr_a));
    ctx.SyncEntity(cr_b);
    CHECK_FALSE(ctx.ValidateAccess(cr_a));
    CHECK(ctx.ValidateAccess(cr_b));
    CHECK_FALSE(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(cr_b));
    ctx.Release();
    CHECK(ctx.IsEmpty());
    CHECK(IsEntityAccessValid(cr_a)); // empty again → unrestricted
}

// ============================================================================
// Symmetric Critter<->Player auto-widening + the ancestor-coverage verify fix.
// Critter::GetSyncWidenEntity returns its Player and Player::GetSyncWidenEntity
// returns its controlled Critter, so a Sync of either half must cover both.
// ============================================================================

TEST_CASE("ServerEngineSyncContextWidenAndAncestorCover")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    const auto location_pid = server->Hashes.ToHashedString("UnitTestLocation");
    const auto map_pid = server->Hashes.ToHashedString("UnitTestMap");
    const auto item_pid = server->Hashes.ToHashedString("TestItem");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto* loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    REQUIRE(loc != nullptr);
    auto* map = loc->GetMapByIndex(0);
    REQUIRE(map != nullptr);

    auto* cr_a = server->CreateCritter(critter_pid, true);
    auto* cr_b = server->CreateCritter(critter_pid, true);
    REQUIRE(cr_a != nullptr);
    REQUIRE(cr_b != nullptr);

    auto player_a_holder = CreateStandalonePlayer(server.get(), "SyncWidenPlayerA");
    auto player_b_holder = CreateStandalonePlayer(server.get(), "SyncWidenPlayerB");
    auto* player_a = player_a_holder.get();
    auto* player_b = player_b_holder.get();
    REQUIRE(player_a != nullptr);
    REQUIRE(player_b != nullptr);

    cr_a->AttachPlayer(player_a);
    player_a->SetControlledCritter(cr_a);
    cr_b->AttachPlayer(player_b);
    player_b->SetControlledCritter(cr_b);

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

            if (cr_a->GetPlayer() != nullptr) {
                cr_a->DetachPlayer();
            }
            if (cr_b->GetPlayer() != nullptr) {
                cr_b->DetachPlayer();
            }
        });
    });

    server->MapMngr.TransferToMap(cr_a, map, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map, mpos {12, 12}, mdir {}, std::nullopt);
    auto* item_a = server->ItemMngr.AddItemCritter(cr_a, item_pid, 1);
    REQUIRE(item_a != nullptr);

    // The widen link must be live in both directions before we test the cover.
    REQUIRE(cr_a->GetSyncWidenEntity() == static_cast<ServerEntity*>(player_a));
    REQUIRE(player_a->GetSyncWidenEntity() == static_cast<ServerEntity*>(cr_a));

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
    // the other player is untouched.
    {
        vector<ServerEntity*> one {cr_a};
        ctx.SyncEntities(one);
        CHECK(ctx.ValidateAccess(cr_a)); // own lock
        CHECK(ctx.ValidateAccess(player_a)); // widened: player's own lock held
        CHECK_FALSE(ctx.ValidateAccess(player_b));
        CHECK_FALSE(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(cr_a));
        CHECK(IsEntityAccessValid(player_a));
        CHECK_FALSE(IsEntityAccessValid(player_b));
    }
    ctx.Release();

    // Symmetric: syncing the Player widens to lock its controlled critter (both own locks held).
    {
        vector<ServerEntity*> one {player_a};
        ctx.SyncEntities(one);
        CHECK(ctx.ValidateAccess(player_a));
        CHECK(ctx.ValidateAccess(cr_a));
        CHECK(IsEntityAccessValid(player_a));
        CHECK(IsEntityAccessValid(cr_a));
    }
    ctx.Release();

    // Duplicate-lock regression: an inventory item can share the holder critter's propagated lock.
    // The lock-set representative may be the item, but widening still has to inspect the explicitly
    // requested holder so the controlled Player lock is included.
    {
        vector<ServerEntity*> item_holder_map {item_a, cr_a, map};
        REQUIRE_NOTHROW(ctx.SyncEntities(item_holder_map));
        CHECK(ctx.ValidateAccess(item_a));
        CHECK(ctx.ValidateAccess(cr_a));
        CHECK(ctx.ValidateAccess(map));
        CHECK(ctx.ValidateAccess(player_a));
        CHECK(IsEntityAccessValid(item_a));
        CHECK(IsEntityAccessValid(player_a));
    }
    ctx.Release();

    // Ancestor-coverage regression (the engine fix): syncing BOTH players widens each to its critter;
    // the two critters share the map and escalate to the single map lock (their own locks dropped), so
    // each widen target is now covered only by its ANCESTOR (map) lock, not its own. The verify-after-
    // acquire step must accept that ancestor coverage; otherwise it re-walks and burns the retry budget,
    // throwing EntitySyncException. This is the exact path the engine fix (widen re-check walks the
    // parent chain) repaired.
    {
        vector<ServerEntity*> players {player_a, player_b};
        REQUIRE_NOTHROW(ctx.SyncEntities(players));
        CHECK(ctx.ValidateAccess(player_a)); // players' own locks held
        CHECK(ctx.ValidateAccess(player_b));
        CHECK(ctx.ValidateAccess(map)); // critters escalated → the map's own lock is held
        CHECK_FALSE(ctx.ValidateAccess(cr_a)); // the critters' own locks were dropped by escalation
        CHECK_FALSE(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(player_a));
        CHECK(IsEntityAccessValid(cr_a)); // accessible via the map ancestor lock — the fix
        CHECK(IsEntityAccessValid(cr_b));
        CHECK(IsEntityAccessValid(map));
    }
    ctx.Release();
}

// ============================================================================
// Concurrency stress — hammer the lock-free cover computation + verify-after-acquire /
// retry-if-escaped loop. Many reader threads run independent SyncContexts (Sync a random
// entity pair, validate, release) while reparenter threads flip entity parents lock-free
// via SetParent — the exact race the verify loop is built to tolerate (a concurrent
// SetParent moving a requested entity out of the just-computed cover). Assertions are
// timing-INDEPENDENT so the test never flakes: every reader iteration is accounted for
// exactly once (no deadlock / lost work), forward progress is made, and nothing crashes
// or corrupts (the atomic _parent + TryAddRef-after-load + the engine asserts would trip
// otherwise). EntitySyncException is an ACCEPTED outcome under extreme churn — the bounded
// retry deliberately gives up rather than livelock.
// ============================================================================

TEST_CASE("ServerEngineSyncContextReparentStress")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    const auto location_pid = server->Hashes.ToHashedString("UnitTestLocation");
    const auto map_pid = server->Hashes.ToHashedString("UnitTestMap");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto* loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid, map_pid, map_pid});
    REQUIRE(loc != nullptr);
    auto* map_a = loc->GetMapByIndex(0);
    auto* map_b = loc->GetMapByIndex(1);
    auto* map_c = loc->GetMapByIndex(2);
    REQUIRE(map_a != nullptr);
    REQUIRE(map_b != nullptr);
    REQUIRE(map_c != nullptr);

    constexpr int32_t CRITTER_COUNT = 12;
    constexpr int32_t READER_THREADS = 6;
    constexpr int32_t READER_ITERS = 12000;
    constexpr int32_t REPARENTER_THREADS = 4;

    vector<Critter*> critters;
    for (int32_t i = 0; i < CRITTER_COUNT; i++) {
        auto* cr = server->CreateCritter(critter_pid, false);
        REQUIRE(cr != nullptr);
        cr->SetParent(map_a); // parent only — escalation reads _parent, not the map critter list
        critters.push_back(cr);
    }

    server->Unlock();
    locked = false;

    // Reparent targets include nullptr so the detach / no-parent / cover-recompute paths are churned
    // alongside same-map (escalation) and cross-map (escape → retry) transitions.
    ServerEntity* const targets[] = {map_a, map_b, map_c, nullptr};

    std::atomic_bool stop {false};
    std::atomic<int64_t> syncs_ok {0};
    std::atomic<int64_t> sync_giveups {0};
    std::atomic<int64_t> reparents {0};

    const auto reparenter_fn = [&](int32_t tid) {
        uint64_t step = numeric_cast<uint64_t>(tid);
        while (!stop.load(std::memory_order_relaxed)) {
            for (int32_t i = tid; i < CRITTER_COUNT; i += REPARENTER_THREADS) {
                critters[numeric_cast<size_t>(i)]->SetParent(targets[(step + numeric_cast<uint64_t>(i)) % 4U]);
                reparents.fetch_add(1, std::memory_order_relaxed);
            }
            step++;
        }
    };

    const auto reader_fn = [&](int32_t tid) {
        SyncContext ctx;
        ctx.Activate();

        vector<ServerEntity*> req(2);
        for (int32_t it = 0; it < READER_ITERS; it++) {
            req[0] = critters[numeric_cast<size_t>((it + tid) % CRITTER_COUNT)];
            req[1] = critters[numeric_cast<size_t>((it * 3 + tid + 1) % CRITTER_COUNT)];

            try {
                ctx.SyncEntities(req);
                syncs_ok.fetch_add(1, std::memory_order_relaxed);
                // Exercise the validator's own walk-and-verify under concurrent reparenting; the
                // result legitimately races a SetParent that lands after Sync returns, so it is
                // observed (to drive the code path) but not asserted.
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
    // No deadlock (every thread joined) and no crash (control reached here). Every reader
    // iteration completed exactly once — either a valid cover or a bounded give-up — so no
    // work was lost or double-counted.
    CHECK(syncs_ok.load() + sync_giveups.load() == int64_t {READER_THREADS} * int64_t {READER_ITERS});
    CHECK(syncs_ok.load() > 0); // forward progress — the retry loop is not failing every call
    CHECK(reparents.load() > 0);

    // Detach the critters so the maps' refcounts drop cleanly before Shutdown.
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    locked = true;
    for (auto* cr : critters) {
        cr->SetParent(nullptr);
    }
}

// ============================================================================
// Flat acquisition: an ancestor (map) cover GRANTS access to a descendant but does NOT exclude
// another thread's own-lock on that descendant. This is the load-bearing design invariant of the
// whole sync model — a regression that made ancestor locks exclude descendant locks would deadlock
// or starve. ReparentStress hammers overlapping covers with reparent noise; this isolates and
// directly proves the positive non-exclusion property.
// ============================================================================

TEST_CASE("ServerEngineSyncContextFlatAcquisition")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    const auto location_pid = server->Hashes.ToHashedString("UnitTestLocation");
    const auto map_pid = server->Hashes.ToHashedString("UnitTestMap");

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    auto* loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    REQUIRE(loc != nullptr);
    auto* map = loc->GetMapByIndex(0);
    REQUIRE(map != nullptr);

    auto* cr_a = server->CreateCritter(critter_pid, false);
    auto* cr_b = server->CreateCritter(critter_pid, false);
    REQUIRE(cr_a != nullptr);
    REQUIRE(cr_b != nullptr);
    server->MapMngr.TransferToMap(cr_a, map, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map, mpos {12, 12}, mdir {}, std::nullopt);

    server->Unlock();
    locked = false;

    SECTION("AncestorAndDescendantLocksHeldSimultaneously")
    {
        // T1 holds the map (ancestor) lock and keeps holding it. While it does, T2 must be able to
        // acquire cr_a's OWN lock — proving the ancestor cover does not exclude the descendant lock.
        std::atomic_bool t1_holds_map {false};
        std::atomic_bool t2_got_cr {false};
        std::atomic_bool t2_may_finish {false};

        std::thread t1([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<ServerEntity*> req {map};
            ctx.SyncEntities(req); // ancestor cover: the map's own lock
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
            vector<ServerEntity*> req {cr_a};
            ctx.SyncEntities(req); // descendant own lock — must succeed while T1 holds the map lock
            t2_got_cr.store(true);
            ctx.Release();
            ctx.Deactivate();
        });

        // Bounded wait: a correct (flat) model lets T2 take cr_a's own lock immediately. A model that
        // made the ancestor cover EXCLUDE the descendant lock would leave t2_got_cr false here.
        for (int i = 0; i < 2000 && !t2_got_cr.load(std::memory_order_acquire); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(t2_got_cr.load());

        // Release T1 regardless, so both threads finish cleanly even on failure (no hang).
        t2_may_finish.store(true, std::memory_order_release);
        t1.join();
        t2.join();
    }

    SECTION("ConcurrentAncestorAndSiblingLockLivenessLoop")
    {
        // Both threads hammer their (independent) locks: one always covers via the map ancestor, the
        // other always takes cr_a's own lock. Both must complete every iteration — no deadlock/starve.
        constexpr int32_t ITERS = 20000;
        std::atomic<int64_t> ancestor_done {0};
        std::atomic<int64_t> sibling_done {0};

        std::thread ancestor_thread([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<ServerEntity*> req {map};
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
            vector<ServerEntity*> req {cr_a};
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

    // Detach so the map refcount drops cleanly before Shutdown.
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    locked = true;
    cr_a->SetParent(nullptr);
    cr_b->SetParent(nullptr);
}

FO_END_NAMESPACE
