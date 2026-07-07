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
#include "DiskFileSystem.h"
#include "Logging.h"
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

    enum class ServerTestScriptMode : uint8_t
    {
        Default,
        StopOnInit,
        ThrowOnInit,
    };

    static auto MakeServerTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeServerHealthFileName() -> string
    {
        const auto exe_path = Platform::GetExePath();

        return strex("{}_Health.txt", exe_path ? strvex(exe_path.value()).extract_file_name().erase_file_extension() : string_view(FO_DEV_NAME));
    }

    static auto MakeTempServerResourceDir(string_view name) -> string
    {
        const auto base = std::filesystem::temp_directory_path() / std::format("lf_server_resources_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
        return fs_path_to_string(base);
    }

    static void RemoveServerHealthFile(string_view name) noexcept
    {
        std::error_code ec;
        (void)std::filesystem::remove(std::filesystem::path {fs_make_path(name)}, ec);
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

        auto registrator = proto_engine.GetPropertyRegistrator(type_name);
        REQUIRE(static_cast<bool>(registrator));

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), registrator};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WriteStringBytes(type_name.as_str());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WriteStringBytes(proto_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        if (!props_data.empty()) {
            writer.WriteBytes({props_data.data(), props_data.size()});
        }

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

    // Deterministic seam for the item-move conservation regression: when armed, the next OnItemInit
    // (which fires from inside CreateItem) mutates the SOURCE stack's count. CreateItem runs between
    // SplitItem's count read and its write, so this reproduces — single-threaded and deterministically —
    // the exact effect of a concurrent split/merge landing during CreateItem's sync-cover yield.
    ident SplitInjectSourceId;
    int SplitInjectAmount = 0;

    [[ModuleInit]]
    void RegisterHooks()
    {
        ImmediateInitOrder = ++ModuleInitOrder;
        Game.OnInit.Subscribe(OnInit);
        Game.OnCritterInit.Subscribe(OnCritterInit);
        Game.OnItemInit.Subscribe(OnItemInit);
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

    [[Event]]
    void OnItemInit(Item item, bool firstTime)
    {
        if (SplitInjectAmount != 0) {
            int amount = SplitInjectAmount;
            SplitInjectAmount = 0; // fire exactly once
            Item? src = Game.GetItem(SplitInjectSourceId);
            if (src !is null) {
                src.Count += amount; // mutate the source mid-split, before SplitItem's write
            }
        }
    }

    void UnitTestArmSplitInjection(ident sourceId, int amount)
    {
        SplitInjectSourceId = sourceId;
        SplitInjectAmount = amount;
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

    static auto MakeInitGateScriptBinary(const FileSystem& metadata_resources, ServerTestScriptMode mode) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        const string_view body = mode == ServerTestScriptMode::StopOnInit ? string_view {"return EventResult::StopChain;"} : string_view {R"(throw("Unit test OnInit failure");
        return EventResult::ContinueChain;)"};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ServerEngineInitGateScripts",
            {
                {"Scripts/ServerEngineInitGateTest.fos",
                    strex(R"(
namespace ServerEngineInitGateTest
{{
    [[ModuleInit]]
    void RegisterHooks()
    {{
        Game.OnInit.Subscribe(OnInit);
    }}

    [[Event]]
    EventResult OnInit()
    {{
        {}
    }}
}}
)",
                        body)
                        .str()},
            },
            [](string_view message) {
                const auto message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    // `MakeSingleProtoResourceBlob` builds a proto with default properties, which leaves `Stackable`
    // false. The conservation stress (ServerEngineConcurrentItemTransferConservesTotal) needs a
    // stackable item so `MoveItem` exercises the split/merge `count` read-modify-write. Mirror of the
    // helper in Test_ServerMapOperations.cpp.
    static auto MakeStackableItemProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrator = proto_engine.GetPropertyRegistrator(type_name);
        ProtoItem proto {proto_engine.Hashes.ToHashedString(proto_name), registrator};
        proto.SetStackable(true);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WriteStringBytes(type_name.as_str());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WriteStringBytes(proto_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        writer.WriteBytes(props_data);

        return protos_data;
    }

    static auto MakeServerTestResources(ServerTestScriptMode script_mode = ServerTestScriptMode::Default) -> FileSystem
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
        const auto stackable_blob = MakeStackableItemProtoBlob(proto_engine, item_type, "UnitTestStackable");
        const auto fomap_blob = MakeEmptyMapBlob();
        auto script_blob = script_mode == ServerTestScriptMode::Default ? MakeScriptBinary(compiler_resources) : MakeInitGateScriptBinary(compiler_resources, script_mode);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ServerEngineTest.fopro-bin-server", proto_blob);
        runtime_source->AddFile("UnitTestLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("UnitTestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("UnitTestItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("UnitTestStackable.fopro-bin-server", stackable_blob);
        runtime_source->AddFile("UnitTestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile(script_mode == ServerTestScriptMode::Default ? "ServerEngineTest.fos-bin-server" : "ServerEngineInitGateTest.fos-bin-server", std::move(script_blob));

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForServerStart(ptr<ServerEngine> server) -> string
    {
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

    static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeServerTestResources());
    }

    static void CheckServerStartupFailsSafely(GlobalSettings& settings, FileSystem&& resources)
    {
        auto server = SafeAlloc::MakeRefCounted<ServerEngine>(&settings, std::move(resources));

        const auto startup_error = WaitForServerStart(server);
        INFO(startup_error);
        CHECK_FALSE(startup_error.empty());
        CHECK(server->IsStartingError());
        CHECK_FALSE(server->IsStarted());

        REQUIRE_NOTHROW(server->Shutdown());
    }

    static void CheckServerStartupFailsSafely(GlobalSettings& settings)
    {
        CheckServerStartupFailsSafely(settings, MakeServerTestResources());
    }

    static auto MakeServerStorageDoc(int64_t value) -> AnyData::Document
    {
        AnyData::Document doc;
        doc.Assign(string {"value"}, value);
        return doc;
    }

    static auto CreateLoggedPlayer(ptr<ServerEngine> server, string_view name) -> ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto unlogined_player = server->CreateUnloginedPlayer(std::move(net_connection));

        unlogined_player->SetName(name);
        unlogined_player->SetLastControlledCritterId(ident_t {1});
        auto player = server->LoginPlayerToNewRecord(unlogined_player);
        FO_VERIFY_AND_THROW(player, "Player login to new record failed");

        return player;
    }

    static auto CreateStandalonePlayer(ptr<ServerEngine> server, string_view name) -> refcount_ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings);
        auto connection = SafeAlloc::MakeUnique<ServerConnection>(server->Settings, std::move(net_connection));
        auto player = SafeAlloc::MakeRefCounted<Player>(server, ident_t {}, std::move(connection));

        SyncContext ctx;
        ctx.Activate();
        auto release = scope_exit([&ctx]() noexcept {
            safe_call([&ctx] {
                ctx.Release();
                ctx.Deactivate();
            });
        });

        ctx.SyncEntity(player);
        player->SetName(name);
        return player;
    }

    static auto MakeServerMovementContext(msize map_size, mpos start_hex, nanotime start_time) -> refcount_ptr<MovingContext>
    {
        return SafeAlloc::MakeRefCounted<MovingContext>(map_size, SERVER_TEST_MOVE_SPEED, SERVER_TEST_MOVE_STEPS, SERVER_TEST_MOVE_CONTROL_STEPS, start_time, timespan {}, start_hex, ipos16 {}, ipos16 {});
    }

    static auto WaitForUnlockedServerCondition(ptr<ServerEngine> server, bool& locked, const function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        FO_VERIFY_AND_THROW(locked, "Server must be locked before waiting on condition");

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

TEST_CASE("ServerResourcesMountBakedServerEntries")
{
    if (IsPackaged()) {
        SKIP("Baked directory mounting is only used by unpackaged test binaries");
    }

    const string temp_dir = MakeTempServerResourceDir("baked_entries");
    const bool removed_before = fs_remove_dir_tree(temp_dir);
    ignore_unused(removed_before);

    auto cleanup = scope_exit([&temp_dir]() noexcept { fs_remove_dir_tree(temp_dir); });

    const string baked_dir = strex(temp_dir).combine_path("Baked").str();
    const string packaged_dir = strex(temp_dir).combine_path("Packaged").str();

    REQUIRE(fs_write_file(strex(baked_dir).combine_path("ServerPack/payload.txt").str(), string_view {"baked-server"}));
    REQUIRE(fs_write_file(strex(baked_dir).combine_path("ClientPack/client-only.txt").str(), string_view {"client"}));
    REQUIRE(fs_write_file(strex(packaged_dir).combine_path("ServerPack/payload.txt").str(), string_view {"packaged-server"}));

    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.BakeOutput, baked_dir);
    BakerTests::OverrideSetting(settings.ServerResources, packaged_dir);
    BakerTests::OverrideSetting(settings.ServerResourceEntries, vector<string> {"ServerPack"});

    auto resources = GetServerResources(settings);

    CHECK(resources.ReadFileText("payload.txt") == "baked-server");
    CHECK(resources.IsFileExists("payload.txt"));
    CHECK_FALSE(resources.IsFileExists("client-only.txt"));
}

TEST_CASE("ServerEngineConnectionAcceptPredicates")
{
    // Pure accept-path decision logic for the network population cap + per-source rate guard
    // (launch-network-load-test-and-caps Track 1). Static + side-effect-free, so no live server is needed.

    SECTION("PopulationCapAcceptsUnderLimitAndRejectsAtOrOver")
    {
        // 0 means unlimited on both axes.
        CHECK(ServerEngine::ShouldAcceptConnection(1000, 1000, 0, 0));

        // Connection cap.
        CHECK(ServerEngine::ShouldAcceptConnection(9, 5, 10, 0));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(10, 5, 10, 0));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(11, 5, 10, 0));

        // Player cap.
        CHECK(ServerEngine::ShouldAcceptConnection(100, 4, 0, 5));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(100, 5, 0, 5));

        // Either ceiling rejects.
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(10, 1, 10, 5));
        CHECK_FALSE(ServerEngine::ShouldAcceptConnection(1, 5, 10, 5));
    }

    SECTION("RateGuardCountsPerSecondBucketAndResetsOnRollover")
    {
        ServerEngine::ConnRateState state {};

        // 0 disables the guard (always accept) and does not touch the bucket.
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 0));

        // Up to the limit within the same second is accepted; beyond it is rejected.
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK_FALSE(ServerEngine::EvaluateConnectionRate(state, 100, 3));
        CHECK_FALSE(ServerEngine::EvaluateConnectionRate(state, 100, 3));

        // A new second rolls the window over and accepts again.
        CHECK(ServerEngine::EvaluateConnectionRate(state, 101, 3));
        CHECK(ServerEngine::EvaluateConnectionRate(state, 101, 3));
    }
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

    const auto startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    REQUIRE(static_cast<bool>(server->GetProtoCritter(critter_pid)));

    const auto critter_count = server->EntityMngr.GetCrittersCount();
    const auto entity_count = server->EntityMngr.GetEntitiesCount();

    auto cr = server->CreateCritter(critter_pid, false);

    const auto cr_id = cr->GetId();
    CHECK(cr->GetProtoId() == critter_pid);
    auto registered_cr = server->EntityMngr.GetCritter(cr_id);
    REQUIRE(registered_cr);
    CHECK(registered_cr.as_ptr() == cr);
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count + 1);
    CHECK(server->EntityMngr.GetEntitiesCount() > entity_count);

    auto player = CreateLoggedPlayer(server, "UnitTestPlayer");

    const auto player_id = player->GetId();
    CHECK(player_id != ident_t {});
    auto registered_player = server->EntityMngr.GetPlayer(player_id);
    REQUIRE(registered_player);
    CHECK(registered_player.as_ptr() == player);

    server->CrMngr.DestroyCritter(cr);

    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count);
}

TEST_CASE("ServerEngineDelayedCallbackAndSharedPropertyLock")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeServerTestResources());

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
    const uint64_t completed_jobs = server->GetCompletedServerJobsCount();

    server->ScheduleDelayedCallback(timespan {std::chrono::milliseconds {1}}, [&callback_ran] { callback_ran.store(true, std::memory_order_release); });

    REQUIRE(WaitForUnlockedServerCondition(server.get(), locked, [&server, &callback_ran, completed_jobs] { return callback_ran.load(std::memory_order_acquire) && server->GetCompletedServerJobsCount() > completed_jobs; }));

    CHECK(callback_ran.load(std::memory_order_acquire));
    CHECK(server->GetCompletedServerJobsCount() > completed_jobs);
}

TEST_CASE("ServerEngineWritesHealthFile")
{
    const string health_file_name = MakeServerHealthFileName();
    RemoveServerHealthFile(health_file_name);

    auto cleanup_health_file = scope_exit([&health_file_name]() noexcept { RemoveServerHealthFile(health_file_name); });

    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.WriteHealthFile, true);
    BakerTests::OverrideSetting(settings.HealthFilePeriodMs, int32_t {5});

    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeServerTestResources());

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

    string health_content;
    REQUIRE(WaitForUnlockedServerCondition(
        server.get(), locked,
        [&health_file_name, &health_content] {
            const auto content = fs_read_file(health_file_name);

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
    // Regression for the cvet-server-1 Staging crash: MongoDB was down, so InitStorageJob threw and
    // startup aborted before the worker pool, database connection and time-sync were established. A
    // later quit ran Shutdown(), which dereferenced the null worker pool — SIGSEGV in
    // WorkerPool::Clear() locking the pool mutex through a null `this`. Shutdown() must be safe to
    // call on such a partially-initialized engine. An unrecognized DbStorage makes ConnectToDataBase
    // throw "Wrong storage options", reproducing the same aborted-startup state deterministically.
    auto settings = MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.DbStorage, string {"UnreachableStorageForTest"});

    CheckServerStartupFailsSafely(settings);
}

TEST_CASE("ServerEngineStopsStartupWhenInitEventStopsChain")
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

TEST_CASE("ServerEngineCustomCollectionStartupValidation")
{
    auto settings = MakeServerTestSettings();

    SECTION("RejectsMissingSeparator")
    {
        BakerTests::OverrideSetting(settings.CustomCollections, vector<string> {"BrokenCollection"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("RejectsEmptyTrimmedCollectionName")
    {
        BakerTests::OverrideSetting(settings.CustomCollections, vector<string> {"   : Int"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("RejectsUnknownKeyType")
    {
        BakerTests::OverrideSetting(settings.CustomCollections, vector<string> {"BadType:Uuid"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("RejectsDuplicateCollectionName")
    {
        BakerTests::OverrideSetting(settings.CustomCollections, vector<string> {"Duplicate:Int", "Duplicate:Str"});
        CheckServerStartupFailsSafely(settings);
    }

    SECTION("AcceptsTrimmedCaseInsensitiveKeyTypes")
    {
        BakerTests::OverrideSetting(settings.CustomCollections, vector<string> {"  TrimmedInt : int  ", "  TrimmedStr : STR  "});

        auto server = MakeServerEngine(settings);

        auto shutdown = scope_exit([&server]() noexcept {
            safe_call([&server] {
                if (server->IsStarted()) {
                    server->Shutdown();
                }
            });
        });

        const auto startup_error = WaitForServerStart(server);
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        const auto int_collection = server->Hashes.ToHashedString("TrimmedInt");
        const auto string_collection = server->Hashes.ToHashedString("TrimmedStr");
        const auto int_id = ident_t {1001};
        const auto string_id = string {"custom:key"};

        CHECK(server->DbStorage.GetAllIntIds(int_collection).empty());
        CHECK(server->DbStorage.GetAllStringIds(string_collection).empty());

        server->DbStorage.Insert(int_collection, DataBaseKey {int_id}, MakeServerStorageDoc(10));
        server->DbStorage.Insert(string_collection, DataBaseKey {string_id}, MakeServerStorageDoc(20));
        server->DbStorage.WaitCommitChanges();

        const auto int_ids = server->DbStorage.GetAllIntIds(int_collection);
        const auto string_ids = server->DbStorage.GetAllStringIds(string_collection);

        REQUIRE(int_ids.size() == 1);
        CHECK(int_ids.front() == int_id);
        REQUIRE(string_ids.size() == 1);
        CHECK(string_ids.front() == string_id);

        const auto int_doc = server->DbStorage.Get(int_collection, DataBaseKey {int_id});
        const auto string_doc = server->DbStorage.Get(string_collection, DataBaseKey {string_id});

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

    const auto startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    SECTION("PlayerControlledCritterCanBeUnloaded")
    {
        const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
        const auto critter_count = server->EntityMngr.GetCrittersCount();

        auto cr = server->CreateCritter(critter_pid, true);

        const auto cr_id = cr->GetId();
        CHECK(cr->GetControlledByPlayer());
        auto registered_cr = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(registered_cr);
        CHECK(registered_cr.as_ptr() == cr);

        server->UnloadCritter(cr);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
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
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server);
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
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server);
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
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server);
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
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server);
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

    auto critter_id_func = server->FindFunc<int64_t, ptr<Critter>>(get_func_name("ServerEngineTest::UnitTestGetCritterIdValue"));
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
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server);
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

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(critter_pid, false);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr.get(), moving, nullptr);
        REQUIRE(cr->GetMovingContext() != nullptr);

        const auto critter_stopped = [&cr] {
            auto sync_ctx = SyncContext::GetCurrentOnThisThread();
            FO_VERIFY_AND_THROW(sync_ctx, "Sync context is null");
            ptr<Critter> synced_cr = cr;
            sync_ctx->SyncEntity(synced_cr);
            return !synced_cr->IsMoving();
        };

        REQUIRE(WaitForUnlockedServerCondition(server, locked, critter_stopped));

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
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(critter_pid, false);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);
        cr->SetCondition(CritterCondition::Dead);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr.get(), moving, nullptr);
        REQUIRE(cr->GetMovingContext() != nullptr);

        const auto critter_stopped = [&cr] {
            auto sync_ctx = SyncContext::GetCurrentOnThisThread();
            FO_VERIFY_AND_THROW(sync_ctx, "Sync context is null");
            ptr<Critter> synced_cr = cr;
            sync_ctx->SyncEntity(synced_cr);
            return !synced_cr->IsMoving();
        };

        REQUIRE(WaitForUnlockedServerCondition(server, locked, critter_stopped));

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
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(critter_pid, false);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, mdir {}, std::nullopt);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto expected_stop_hex = path_hexes[1];
        const auto blocked_hex = path_hexes[2];

        map->SetHexManualBlock(blocked_hex, true, true);
        auto unblock_hex = scope_exit([map, blocked_hex]() noexcept {
            safe_call([map, blocked_hex] {
                auto sync_ctx = SyncContext::GetCurrentOnThisThread();
                if (sync_ctx) {
                    auto synced_map = map;
                    sync_ctx->SyncEntity(synced_map);

                    if (synced_map && !synced_map->IsDestroyed()) {
                        synced_map->SetHexManualBlock(blocked_hex, false, false);
                    }
                }
            });
        });

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32_t>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr.get(), moving, nullptr);
        REQUIRE(cr->GetMovingContext() != nullptr);

        const auto critter_stopped = [&cr] {
            auto sync_ctx = SyncContext::GetCurrentOnThisThread();
            FO_VERIFY_AND_THROW(sync_ctx, "Sync context is null");
            ptr<Critter> synced_cr = cr;
            sync_ctx->SyncEntity(synced_cr);
            return !synced_cr->IsMoving();
        };

        REQUIRE(WaitForUnlockedServerCondition(server, locked, critter_stopped));

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

// ============================================================================
// Per-entity sync rail (multithreading): SyncContext minimal-cover / escalation /
// ValidateAccess hierarchy walk against REAL ServerEntity instances. The primitive
// tests in Test_EntitySync.cpp cannot reach this - they have no entity hierarchy
// (see its "ValidateAccessFailsOnUnheldLock" comment). World is built under the
// engine singleton lock, then the lock is released because SyncEntities refuses to
// run while a singleton lock is held (deadlock prevention).
// ============================================================================

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

    const auto startup_error = WaitForServerStart(server);
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

    auto loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(static_cast<bool>(map));
    ptr<ServerEntity> map_entity = map;

    auto cr_a = server->CreateCritter(critter_pid, false);
    auto cr_b = server->CreateCritter(critter_pid, false);
    auto cr_c = server->CreateCritter(critter_pid, false);

    server->MapMngr.TransferToMap(cr_a, map, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map, mpos {12, 12}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_c, map, mpos {14, 14}, mdir {}, std::nullopt);

    // Parent wiring (Critter._parent = Map) must be established for the cover logic.
    auto cr_a_parent = cr_a->GetParentRaw();
    auto cr_b_parent = cr_b->GetParentRaw();
    auto cr_c_parent = cr_c->GetParentRaw();
    REQUIRE(cr_a_parent);
    REQUIRE(cr_b_parent);
    REQUIRE(cr_c_parent);
    REQUIRE(cr_a_parent.as_ptr() == map_entity);
    REQUIRE(cr_b_parent.as_ptr() == map_entity);
    REQUIRE(cr_c_parent.as_ptr() == map_entity);

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
    // is the production access gate - the hierarchy walk that accepts e's own OR any ancestor lock,
    // plus the null/empty short-circuits.

    // Empty context: no held locks → the access gate grants everything, including a null entity.
    CHECK(ctx.IsEmpty());
    CHECK(IsEntityAccessValid(cr_a));
    CHECK(IsEntityAccessValid(nullptr));

    // Single critter: only its own lock is held; access is granted to it but not siblings or the map.
    {
        vector<nptr<ServerEntity>> one {cr_a};
        ctx.SyncEntities(one);
        CHECK_FALSE(ctx.IsEmpty());
        CHECK(ctx.ValidateAccess(cr_a)); // cr_a's own lock is held
        CHECK_FALSE(ctx.ValidateAccess(cr_b));
        CHECK(IsEntityAccessValid(cr_a));

        {
            bool sync_diag_seen = false;
            SetLogCallback("entity_access_valid_diagnose_test", [&sync_diag_seen](LogType, string_view message, nptr<const CatchedStackTraceData>) {
                if (message.find("SyncDiag access-without-sync") != string_view::npos) {
                    sync_diag_seen = true;
                }
            });
            const auto clear_log_callback = scope_exit([]() noexcept { SetLogCallback("entity_access_valid_diagnose_test", {}); });

            CHECK_FALSE(IsEntityAccessValid(cr_b, false));
            CHECK_FALSE(sync_diag_seen);

            CHECK_FALSE(IsEntityAccessValid(cr_b));
            CHECK(sync_diag_seen);
        }

        CHECK_FALSE(IsEntityAccessValid(cr_b)); // sibling: no lock in its chain is held
        CHECK_FALSE(IsEntityAccessValid(cr_c));
        CHECK_FALSE(IsEntityAccessValid(map));

        // Negative: the binding-level gate (ServerEntity::ValidateAccess, hit by every script
        // property read / method call) must THROW on an uncovered entity, not merely report false -
        // an unsynced access is a rejected error, never silently allowed. The covered entity passes.
        CHECK_NOTHROW(cr_a->ValidateAccess());
        CHECK_THROWS_AS(cr_b->ValidateAccess(), ScriptException);
        CHECK_THROWS_AS(map->ValidateAccess(), ScriptException);
    }
    ctx.Release();
    CHECK(ctx.IsEmpty());

    // Two siblings are locked INDIVIDUALLY — there is deliberately no sibling-to-parent escalation, so
    // `Sync({cr_a, cr_b})` holds exactly those two critters' own locks. The shared map is only
    // descendant-MARKED (not exclusively held), so the map itself is not accessible and an unrequested
    // third sibling on the same map is NOT covered.
    {
        vector<nptr<ServerEntity>> both {cr_a, cr_b};
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
    // critter survives a reparent that a parent-only cover would strand.
    {
        vector<nptr<ServerEntity>> pair {cr_a, map};
        ctx.SyncEntities(pair);
        CHECK(ctx.ValidateAccess(cr_a)); // own lock kept
        CHECK(ctx.ValidateAccess(map)); // map lock kept
        CHECK(IsEntityAccessValid(cr_a));
        CHECK(IsEntityAccessValid(cr_c)); // map lock covers siblings
    }
    ctx.Release();

    // EnsureEntitySynced ALWAYS takes the entity's own lock, including under a still-empty context:
    // a freshly registered entity is a real, uncovered lock the moment it exists, and leaving it
    // unlocked would let a concurrent job mutate it while the creator is still initializing it.
    // The context thereby becomes restricted — the pulled entity is covered, an unrelated sibling
    // is not. With a singleton lock held it likewise adds the one lock and stays idempotent.
    ctx.EnsureEntitySynced(cr_a);
    CHECK_FALSE(ctx.IsEmpty());
    CHECK(ctx.ValidateAccess(cr_a));
    CHECK(IsEntityAccessValid(cr_a));
    CHECK_FALSE(IsEntityAccessValid(cr_b));
    ctx.Release();

    ctx.LockSingleton(server->GetEntityLock().get());
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
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server);
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
    setup_ctx->EnsureEntitySynced(player_a_holder);
    setup_ctx->EnsureEntitySynced(player_b_holder);

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
    auto item_a = server->ItemMngr.AddItemCritter(cr_a, item_pid, 1);
    REQUIRE(static_cast<bool>(item_a));

    // The widen link must be live in both directions before we test the cover.
    auto player_entity_lookup = player_a_holder.as_ptr().cast<ServerEntity>();
    auto cr_entity_lookup = cr_a.cast<ServerEntity>();
    REQUIRE(cr_a->GetSyncWidenEntity() == player_entity_lookup);
    REQUIRE(player_a_holder->GetSyncWidenEntity() == cr_entity_lookup);

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
        vector<nptr<ServerEntity>> one {cr_a};
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

    // Symmetric: syncing the Player widens to lock its controlled critter (both own locks held).
    {
        vector<nptr<ServerEntity>> one {player_a_holder};
        ctx.SyncEntities(one);
        CHECK(ctx.ValidateAccess(player_a_holder));
        CHECK(ctx.ValidateAccess(cr_a));
        CHECK(IsEntityAccessValid(player_a_holder));
        CHECK(IsEntityAccessValid(cr_a));
    }
    ctx.Release();

    // Duplicate-lock regression: an inventory item can share the holder critter's propagated lock.
    // The lock-set representative may be the item, but widening still has to inspect the explicitly
    // requested holder so the controlled Player lock is included.
    {
        vector<nptr<ServerEntity>> item_holder_map {item_a, cr_a, map};
        REQUIRE_NOTHROW(ctx.SyncEntities(item_holder_map));
        CHECK(ctx.ValidateAccess(item_a));
        CHECK(ctx.ValidateAccess(cr_a));
        CHECK(ctx.ValidateAccess(map));
        CHECK(ctx.ValidateAccess(player_a_holder));
        CHECK(IsEntityAccessValid(item_a));
        CHECK(IsEntityAccessValid(player_a_holder));
    }
    ctx.Release();

    // No propagation, no escalation: Sync({item, holder, recipient}) holds each one's OWN lock — the item
    // keeps its own lock (it does not share the holder's), and the two critters are NOT collapsed onto
    // their shared map. Widening still adds each critter's Player. The shared map is only marked, not held.
    {
        vector<nptr<ServerEntity>> item_holder_recipient {item_a, cr_a, cr_b};
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

    // Syncing BOTH players widens each to its controlled critter. With no escalation those critters keep
    // their OWN locks (not collapsed onto the shared map), so each is covered by its own held lock and the
    // widen verify-after-acquire is satisfied directly. The shared map is only marked, so the map itself
    // is not accessible.
    {
        vector<nptr<ServerEntity>> players {player_a_holder, player_b_holder};
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
}

// ============================================================================
// Concurrency stress - hammer the lock-free cover computation + verify-after-acquire /
// retry-if-escaped loop. Many reader threads run independent SyncContexts (Sync a random
// entity pair, validate, release) while reparenter threads flip entity parents lock-free
// via SetParent - the exact race the verify loop is built to tolerate (a concurrent
// SetParent moving a requested entity out of the just-computed cover). Assertions are
// timing-INDEPENDENT so the test never flakes: every reader iteration is accounted for
// exactly once (no deadlock / lost work), forward progress is made, and nothing crashes
// or corrupts (the atomic _parent + TryAddRef-after-load + the engine asserts would trip
// otherwise). EntitySyncException is an ACCEPTED outcome under extreme churn - the bounded
// retry deliberately gives up rather than livelock.
// ============================================================================

TEST_CASE("ServerEngineSyncContextFlatAcquisitionAncestorAndSiblingLiveness")
{
    // Load-bearing design invariant (mt-additional-test-coverage, Critical): under hierarchical exclusion a
    // map (ancestor) lock and a critter (descendant) lock ARE mutually exclusive cross-thread, but that
    // exclusion must never deadlock or starve. Two threads — one repeatedly taking the map, the other
    // repeatedly taking the descendant critter — serialize against each other yet must both make full
    // progress (FIFO queue + anti-starvation). A model that turned the exclusion into a lock cycle would
    // hang here (the joins never return); completion is the deterministic deadlock/starvation-freedom proof.
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(ptr<GlobalSettings> {&settings}, MakeServerTestResources());

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

    const auto ancestor_fn = [&]() {
        SyncContext ctx;
        ctx.Activate();
        vector<nptr<ServerEntity>> req {flat_map};
        for (int32_t i = 0; i < FLAT_ITERS; i++) {
            ctx.SyncEntities(req);
            ancestor_progress.fetch_add(1, std::memory_order_relaxed);
            ctx.Release();
        }
        ctx.Deactivate();
    };

    const auto sibling_fn = [&]() {
        SyncContext ctx;
        ctx.Activate();
        vector<nptr<ServerEntity>> req {flat_critter};
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

    const auto startup_error = WaitForServerStart(server);
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
    // alongside same-map (escalation) and cross-map (escape → retry) transitions.
    const nptr<ServerEntity> targets[] = {map_a, map_b, map_c, nullptr};

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

        vector<nptr<ServerEntity>> req(2);
        for (int32_t it = 0; it < READER_ITERS; it++) {
            req[0] = critters[numeric_cast<size_t>((it + tid) % CRITTER_COUNT)];
            req[1] = critters[numeric_cast<size_t>((it * 3 + tid + 1) % CRITTER_COUNT)];

            try {
                ctx.SyncEntities(req);
                syncs_ok.fetch_add(1, std::memory_order_relaxed);
                // Drive the single-pass access validator under concurrent lock-free reparenting (now an
                // out-of-contract adversary — production reparents hold the entity's cover) to confirm it
                // never crashes or reads freed memory; the racing SetParent makes the boolean result
                // nondeterministic, so it is observed to drive the code path, not asserted.
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
    // iteration completed exactly once - either a valid cover or a bounded give-up - so no
    // work was lost or double-counted.
    CHECK(syncs_ok.load() + sync_giveups.load() == int64_t {READER_THREADS} * int64_t {READER_ITERS});
    CHECK(syncs_ok.load() > 0); // forward progress - the retry loop is not failing every call
    CHECK(reparents.load() > 0);

    // Detach the critters so the maps' refcounts drop cleanly before Shutdown.
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    locked = true;
    for (ptr<Critter> cr : critters) {
        cr->SetParent(nullptr);
    }
}

// ============================================================================
// Item-transfer conservation (#12 launch MT gap): concurrent MoveItem of stackable units between
// holders must conserve the total count. ItemManager::SplitItem reads the pre-split count, then
// CreateItem can release+re-acquire the sync cover (EnsureEntitySynced) so a concurrent split/merge
// of the same stack lands a lost update (199/200). Red before the leaf-lock fix, green after. Mirrors
// the script-stress sync_stress.concurrent_item_transfer_conserves_total. High-contention iteration
// rather than a strict-deterministic interleave: many threads × many moves so the window is hit.
// ============================================================================

TEST_CASE("ServerEngineConcurrentItemTransferConservesTotal")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(ptr<GlobalSettings> {&settings}, MakeServerTestResources());

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
    const auto coin_pid = server->Hashes.ToHashedString("UnitTestStackable");

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

    constexpr int32_t HOLDER_COUNT = 8;
    constexpr int32_t COINS_PER_HOLDER = 25;
    constexpr int64_t EXPECTED_TOTAL = int64_t {HOLDER_COUNT} * int64_t {COINS_PER_HOLDER};

    vector<ptr<Critter>> holders;
    for (int32_t i = 0; i < HOLDER_COUNT; i++) {
        auto cr = server->CreateCritter(critter_pid, false);
        cr->SetParent(map); // parent only — keeps the critter (and its inventory) covered via the map
        auto coins = server->ItemMngr.AddItemCritter(cr, coin_pid, COINS_PER_HOLDER);
        REQUIRE(coins != nullptr);
        REQUIRE(coins->GetCount() == COINS_PER_HOLDER);
        holders.push_back(cr);
    }

    server->Unlock();
    locked = false;

    constexpr int32_t MOVER_THREADS = 6;
    constexpr int32_t MOVES_PER_THREAD = 30000;

    std::atomic<int64_t> moves_done {0};
    std::atomic<int64_t> move_skips {0};

    const auto mover_fn = [&](int32_t tid) {
        SyncContext ctx;
        ctx.Activate();

        // Per-thread LCG — deterministic per thread but overlaps other threads' holder pairs so the
        // covers on shared stacks contend (no Math::Random in engine code; vary the stream by tid).
        uint64_t rng = numeric_cast<uint64_t>(tid) * 0x9E3779B97F4A7C15ULL + 1U;

        vector<nptr<ServerEntity>> req(2);
        for (int32_t it = 0; it < MOVES_PER_THREAD; it++) {
            rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
            const int32_t from = numeric_cast<int32_t>((rng >> 33) % numeric_cast<uint64_t>(HOLDER_COUNT));
            const int32_t step = numeric_cast<int32_t>((rng >> 17) % numeric_cast<uint64_t>(HOLDER_COUNT - 1));
            const int32_t to = (from + 1 + step) % HOLDER_COUNT;

            auto from_cr = holders[numeric_cast<size_t>(from)];
            auto to_cr = holders[numeric_cast<size_t>(to)];

            try {
                req[0] = from_cr;
                req[1] = to_cr;
                ctx.SyncEntities(req);

                auto stack = from_cr->GetInvItemByPid(coin_pid);
                if (stack != nullptr && stack->GetCount() > 0) {
                    server->ItemMngr.MoveItem(stack, 1, to_cr);
                    moves_done.fetch_add(1, std::memory_order_relaxed);
                }
                else {
                    move_skips.fetch_add(1, std::memory_order_relaxed);
                }
            }
            catch (const EntitySyncException&) {
                move_skips.fetch_add(1, std::memory_order_relaxed);
            }

            ctx.Release();
        }

        ctx.Deactivate();
    };

    vector<std::thread> movers;
    for (int32_t i = 0; i < MOVER_THREADS; i++) {
        movers.emplace_back(mover_fn, i);
    }
    for (auto& t : movers) {
        t.join();
    }

    // Sum the surviving stacks under a single cover — conservation must hold regardless of how the
    // coins redistributed across holders.
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    locked = true;

    int64_t total = 0;
    for (auto cr : holders) {
        auto stack = cr->GetInvItemByPid(coin_pid);
        total += (stack != nullptr) ? int64_t {stack->GetCount()} : int64_t {0};
    }

    INFO("moves_done=" << moves_done.load() << " skips=" << move_skips.load() << " total=" << total << " expected=" << EXPECTED_TOTAL);
    CHECK(moves_done.load() > 0);
    CHECK(total == EXPECTED_TOTAL);

    // Detach so item/critter refcounts drop before Shutdown.
    for (auto cr : holders) {
        cr->SetParent(nullptr);
    }
}

// ============================================================================
// Deterministic conservation regression for the item-move fix. The contention stress above only
// reproduces the lost update by a rare timing coincidence; this drives the exact stale-snapshot effect
// deterministically: an OnItemInit script handler mutates the SOURCE stack from inside CreateItem —
// i.e. between SplitItem's count read and its write, the same point a concurrent split/merge would
// land during CreateItem's sync-cover yield. Pre-fix, SplitItem's stale write clobbers the mutation
// (a lost unit when the source grew, a duplicated unit when it shrank); the reorder fix re-reads fresh
// and conserves. This is the only engine-resident test that covers the fix's fresh-read invariant AND
// its post-yield re-validation (drain -> DestroyItem -> nullptr) cleanup branch.
// ============================================================================

TEST_CASE("ServerEngineSplitItemUsesFreshCountAfterInitYield")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(ptr<GlobalSettings> {&settings}, MakeServerTestResources());

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
    const auto coin_pid = server->Hashes.ToHashedString("UnitTestStackable");
    const auto arm_func = server->Hashes.ToHashedString("ServerEngineTest::UnitTestArmSplitInjection");

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

    auto h1 = server->CreateCritter(critter_pid, false);
    auto h2 = server->CreateCritter(critter_pid, false);
    h1->SetParent(map);
    h2->SetParent(map);

    SECTION("source grows mid-split: fresh read conserves the injected units")
    {
        auto source = server->ItemMngr.AddItemCritter(h1, coin_pid, 20);
        REQUIRE(source != nullptr);

        // The split product's OnItemInit (fires inside CreateItem) adds 5 to the source, mid-split.
        REQUIRE(server->CallFunc(arm_func, source->GetId(), int32_t {5}));

        auto moved = server->ItemMngr.MoveItem(source.as_ptr(), 1, h2);
        REQUIRE(moved != nullptr);

        auto src_after = h1->GetInvItemByPid(coin_pid);
        auto dst_after = h2->GetInvItemByPid(coin_pid);
        const int32_t src_count = src_after != nullptr ? src_after->GetCount() : 0;
        const int32_t dst_count = dst_after != nullptr ? dst_after->GetCount() : 0;

        INFO("src=" << src_count << " dst=" << dst_count << " total=" << (src_count + dst_count));
        // 20 spawned + 5 injected during the split = 25 must survive. Pre-fix: 20 (the +5 was clobbered
        // by SplitItem writing the stale pre-CreateItem count).
        CHECK(src_count + dst_count == 25);
    }

    SECTION("source drained below the split count mid-split: re-validation undoes the move")
    {
        auto source = server->ItemMngr.AddItemCritter(h1, coin_pid, 2);
        REQUIRE(source != nullptr);

        // The split product's OnItemInit removes 1 from the source (2 -> 1), so the fresh post-yield
        // re-validation (count >= GetCount()) trips and the split is undone.
        REQUIRE(server->CallFunc(arm_func, source->GetId(), int32_t {-1}));

        auto moved = server->ItemMngr.MoveItem(source.as_ptr(), 1, h2);
        CHECK(moved == nullptr); // re-validation cleanup -> nullptr; the move did not happen

        auto src_after = h1->GetInvItemByPid(coin_pid);
        auto dst_after = h2->GetInvItemByPid(coin_pid);
        const int32_t src_count = src_after != nullptr ? src_after->GetCount() : 0;
        const int32_t dst_count = dst_after != nullptr ? dst_after->GetCount() : 0;

        INFO("src=" << src_count << " dst=" << dst_count);
        // 2 spawned - 1 drained = 1 unit total, all on the source; no phantom split on h2. Pre-fix the
        // stale write would leave src=1 AND a phantom split=1 on h2 (a duplicated unit).
        CHECK(src_count == 1);
        CHECK(dst_count == 0);
    }

    h1->SetParent(nullptr);
    h2->SetParent(nullptr);
}

// ============================================================================
// Flat acquisition: an ancestor (map) cover GRANTS access to a descendant but does NOT exclude
// another thread's own-lock on that descendant. This is the load-bearing design invariant of the
// whole sync model - a regression that made ancestor locks exclude descendant locks would deadlock
// or starve. ReparentStress hammers overlapping covers with reparent noise; this isolates and
// directly proves the positive non-exclusion property.
// ============================================================================

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

    const auto startup_error = WaitForServerStart(server);
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
        // Hierarchical exclusion: T1 holds the map (ancestor) lock and keeps holding it. While it does,
        // T2's Sync(cr_a) — a descendant of that map — must BLOCK (Sync marks the critter's ancestors,
        // and the mark on the map cannot be registered while T1 owns the map exclusively). T2 must only
        // acquire cr_a once T1 releases the map. This is the core ancestor↔descendant mutual exclusion.
        std::atomic_bool t1_holds_map {false};
        std::atomic_bool t2_got_cr {false};
        std::atomic_bool t2_may_finish {false};

        std::thread t1([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<nptr<ServerEntity>> req {map};
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
            vector<nptr<ServerEntity>> req {cr_a};
            ctx.SyncEntities(req); // descendant — must block until T1 releases the ancestor map
            t2_got_cr.store(true);
            ctx.Release();
            ctx.Deactivate();
        });

        // While T1 holds the map, T2 must be excluded: t2_got_cr stays false. A generous window rules
        // out the descendant being acquired concurrently (the old flat model would set it almost
        // immediately). A correct hierarchical model never sets it while the ancestor is held.
        const auto exclusion_window = nanotime::now() + timespan {std::chrono::milliseconds {300}};
        while (nanotime::now() < exclusion_window) {
            CHECK_FALSE(t2_got_cr.load(std::memory_order_acquire));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // Release T1 — T2's blocked Sync must now complete.
        t2_may_finish.store(true, std::memory_order_release);
        t1.join();
        t2.join();
        CHECK(t2_got_cr.load());
    }

    SECTION("ConcurrentAncestorAndSiblingLockLivenessLoop")
    {
        // Both threads hammer locks that now mutually exclude under hierarchical exclusion: one always
        // takes the map ancestor, the other always takes cr_a's descendant lock. They serialize against
        // each other, but both must complete every iteration — no deadlock, no starvation.
        constexpr int32_t ITERS = 20000;
        std::atomic<int64_t> ancestor_done {0};
        std::atomic<int64_t> sibling_done {0};

        std::thread ancestor_thread([&]() {
            SyncContext ctx;
            ctx.Activate();
            vector<nptr<ServerEntity>> req {map};
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
            vector<nptr<ServerEntity>> req {cr_a};
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

// ============================================================================
// Nested cross-entity acquisition must not deadlock. Two worker threads each run a PRIMARY
// SyncContext that already covers one critter (mirrors a per-player job whose cover is {player,
// controlled critter} via auto-widen), then each fires a callback whose NESTED SyncContext requests
// BOTH critters in the OPPOSITE order — exactly what two concurrent group-logout cleanups do
// (`Groups::OnPlayerLogout` → `KickCritterFromGroup` → `Sync::Lock(cr, leader)`). Thread 1 holds
// cr_a (parent) and wants {cr_a, cr_b}; thread 2 holds cr_b (parent) and wants {cr_b, cr_a}: a
// genuine hold-and-wait 2-cycle. A nested acquire that only spins non-parking (holding the parent's
// lock the whole time) can never break this — the contended lock is permanently held by the other
// thread's parent. The acquire must escalate to a globally-ordered blocking acquire of the full
// thread-held union (releasing parent locks across the transition, since no entity state is observed
// during a lock-set change) so both threads converge. Guarded by a watchdog so a regression reports
// a clean failure instead of hanging the whole unit-test process.
// ============================================================================

TEST_CASE("ServerEngineSyncContextNestedCrossEntityNoDeadlock")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(ptr<GlobalSettings> {&settings}, MakeServerTestResources());

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

    // Two critters on DIFFERENT maps so each keeps its OWN lock (no sibling escalation to a shared
    // map lock would collapse the pair and hide the cross-lock contention).
    auto loc = server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid, map_pid});
    auto map_a = loc->GetMapByIndex(0);
    auto map_b = loc->GetMapByIndex(1);
    REQUIRE(static_cast<bool>(map_a));
    REQUIRE(static_cast<bool>(map_b));

    auto cr_a = server->CreateCritter(critter_pid, false);
    auto cr_b = server->CreateCritter(critter_pid, false);
    server->MapMngr.TransferToMap(cr_a, map_a, mpos {10, 10}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(cr_b, map_b, mpos {12, 12}, mdir {}, std::nullopt);

    server->Unlock();
    locked = false;

    constexpr int32_t ROUNDS = 400;

    std::atomic<int64_t> rounds_done {0};
    std::atomic_bool failed {false};

    // Per-round rendezvous taken BEFORE any lock is acquired, so neither thread ever waits at the
    // barrier while holding a lock (that would be a harness-level deadlock independent of the rail).
    // Once both threads pass the barrier they acquire {primary cover} then {nested cross cover}
    // back-to-back, so the two opposite-order cross-acquires overlap and the 2-cycle is hammered
    // every round. No lock is held across any wait point in the harness itself.
    std::atomic<int32_t> arrive {0};
    std::atomic<int32_t> generation {0};
    std::atomic<int32_t> primary_held {0};

    const auto barrier = [&](int32_t round) {
        const int32_t gen = generation.load(std::memory_order_acquire);
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

    const auto cross_thread = [&](ServerEntity* own, ServerEntity* peer) {
        for (int32_t round = 0; round < ROUNDS && !failed.load(std::memory_order_acquire); round++) {
            // Outer rendezvous while holding NO lock, so both threads start the locked section
            // together (and the per-round primary_held counter starts clean).
            primary_held.store(0, std::memory_order_relaxed);
            barrier(round);

            SyncContext primary;
            primary.Activate();

            // Primary cover: this thread's "own" critter (like a player job's controlled critter,
            // already covered when an event fires).
            vector<nptr<ServerEntity>> primary_req {own};
            try {
                primary.SyncEntities(primary_req);

                // Inner rendezvous holding ONLY the primary: busy-yield (no lock-ordering hazard —
                // just an atomic) until the peer also holds its primary. This makes the nested
                // cross-acquire below near-deterministically collide: both threads request {own, peer}
                // in OPPOSITE order while each holds (via its primary) the lock the other needs — a
                // genuine cross hold-and-wait 2-cycle. The fixed engine breaks it via the ordered-fair
                // escalation; an unfixed engine spins forever (regression caught by the watchdog).
                // The give-up threshold is a generous wall-clock deadline, not a fixed spin count, so a
                // descheduled-but-progressing peer on a loaded host cannot trip a false "setup broken".
                primary_held.fetch_add(1, std::memory_order_acq_rel);
                const auto rendezvous_deadline = nanotime::now() + timespan {std::chrono::seconds {10}};
                while (primary_held.load(std::memory_order_acquire) < 2 && !failed.load(std::memory_order_acquire) && nanotime::now() < rendezvous_deadline) {
                    std::this_thread::yield();
                }

                // The rendezvous MUST be met before the nested cross-acquire: if the peer never
                // reached its primary, the two opposite-order acquires don't overlap, the 2-cycle
                // never forms, and even a broken engine would slip through as a false PASS. The peer
                // only has to take one uncontended primary lock, so missing the generous deadline means
                // the setup is broken — fail hard instead of running a non-diagnostic acquire.
                if (primary_held.load(std::memory_order_acquire) < 2) {
                    failed.store(true, std::memory_order_release);
                }
                else {
                    // Nested context (like a per-callback FireEvent scope) requesting BOTH critters.
                    SyncContext nested;
                    nested.Activate();
                    auto nested_cleanup = scope_exit([&]() noexcept {
                        nested.Release();
                        nested.Deactivate();
                    });

                    vector<nptr<ServerEntity>> nested_req {own, peer};
                    nested.SyncEntities(nested_req);
                    // Catch2's CHECK/REQUIRE macros are not thread-safe (they race on RunContext's assertion
                    // fast-path, a process-global), and this runs on a worker thread. Record the result through
                    // the `failed` atomic instead; the main thread reports it via CHECK_FALSE(failed.load()).
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

    std::thread t1(cross_thread, cr_a.get(), cr_b.get());
    std::thread t2(cross_thread, cr_b.get(), cr_a.get());

    // Watchdog: a correct implementation finishes the cross-rounds in well under a second even on a
    // loaded host. If the threads deadlock, detect it and fail cleanly. We can't safely unblock a real
    // deadlock, so on timeout we Shutdown (aborts pending entity-lock waiters) and detach — but the
    // fixed engine never reaches the timeout. Both threads run ROUNDS rounds, so the target is 2*ROUNDS.
    constexpr int64_t total_rounds = int64_t {ROUNDS} * 2;
    const auto deadline = nanotime::now() + timespan {std::chrono::seconds {30}};
    bool timed_out = false;
    while (rounds_done.load(std::memory_order_acquire) < total_rounds && !failed.load(std::memory_order_acquire)) {
        if (nanotime::now() > deadline) {
            timed_out = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK_FALSE(timed_out); // a hang here is the sync-rail nested cross-entity deadlock (regression)
    CHECK_FALSE(failed.load());

    if (timed_out) {
        // Best-effort: wake any thread stuck in a shutdown-abortable wait so the process can exit
        // rather than hang the whole suite. The CHECK above already recorded the failure.
        failed.store(true, std::memory_order_release);
        server->Shutdown();
        t1.detach();
        t2.detach();
    }
    else {
        t1.join();
        t2.join();
        CHECK(rounds_done.load() == total_rounds);
    }

    if (!timed_out) {
        // Detach so the map refcounts drop cleanly before Shutdown.
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        locked = true;
        cr_a->SetParent(nullptr);
        cr_b->SetParent(nullptr);
    }
}

FO_END_NAMESPACE
