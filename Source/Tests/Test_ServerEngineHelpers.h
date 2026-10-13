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

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "DiskFileSystem.h"
#include "ImGuiStuff.h"
#include "Logging.h"
#include "Movement.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace ServerEngineTests
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

    inline auto MakeServerTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    inline auto MakeServerHealthFileName() -> string
    {
        auto exe_path = platform::get_exe_path();

        return strex("{}_Health.txt", exe_path ? strvex(exe_path.value()).extract_file_name().erase_file_extension() : string_view(FO_DEV_NAME));
    }

    inline auto MakeTempServerResourceDir(string_view name) -> string
    {
        auto base = std::filesystem::temp_directory_path() / std::format("lf_server_resources_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
        return fs::path_to_string(base);
    }

    inline void RemoveServerHealthFile(string_view name) noexcept
    {
        std::error_code ec;
        (void)std::filesystem::remove(std::filesystem::path {fs::make_path(name)}, ec);
    }

    inline auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = data_writer(map_data);
        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        return map_data;
    }

    // A sustained hold is impossible for a covered target, so this pins that the give-up comes from the retry
    // budget and not the watchdog, by requiring the contention to still be held when the throw lands
    inline void ExpectSustainedEnsureStateMutexContentionThrows(SyncContext& ctx, nptr<ServerEntity> target, ptr<EntityLock> state_lock)
    {
        std::atomic<bool> state_locked {};
        std::atomic<bool> release_state {};
        std::atomic<bool> ensure_finished {};
        std::jthread state_owner {[&](std::stop_token) {
            state_lock->LockStateMutex();
            state_locked.store(true, std::memory_order_release);

            while (!release_state.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            state_lock->UnlockStateMutex();
        }};
        auto release_state_guard = scope_exit([&release_state]() noexcept { release_state.store(true, std::memory_order_release); });

        while (!state_locked.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // A safety net that must stay well above the retry budget, or releasing early would turn the expected
        // throw into a success and mask a regression
        std::jthread contention_watchdog {[&](std::stop_token) {
            for (int32_t i = 0; i < 10000 && !ensure_finished.load(std::memory_order_acquire); i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }

            release_state.store(true, std::memory_order_release);
        }};

        CHECK_THROWS_WITH(ctx.EnsureEntitySynced(target), Catch::Matchers::ContainsSubstring("covered entity lock is contended"));

        // Read before the test releases the contention itself, so this reflects only the watchdog
        bool rescued_by_watchdog = release_state.load(std::memory_order_acquire);
        ensure_finished.store(true, std::memory_order_release);
        release_state.store(true, std::memory_order_release);
        state_owner.join();
        contention_watchdog.join();

        CHECK_FALSE(rescued_by_watchdog);
    }

    // The realistic counterpart, where a foreign thread holds the mutex only for its own non-blocking pass:
    // retention must retry through that window and land
    inline void ExpectTransientEnsureStateMutexContentionIsAbsorbed(SyncContext& ctx, nptr<ServerEntity> target, ptr<EntityLock> state_lock)
    {
        std::atomic<bool> state_locked {};
        std::jthread state_owner {[&](std::stop_token) {
            state_lock->LockStateMutex();
            state_locked.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds {40});
            state_lock->UnlockStateMutex();
        }};

        while (!state_locked.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        CHECK_NOTHROW(ctx.EnsureEntitySynced(target));
        state_owner.join();
    }

    inline auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrar = proto_engine.GetPropertyRegistrar(type_name);
        REQUIRE(static_cast<bool>(registrar));

        ProtoMap proto {proto_engine.Hashes.to_hashed_string(proto_name), registrar};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = data_writer(protos_data);

        writer.write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.write_string_bytes(type_name.as_str());
        writer.write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.write_string_bytes(proto_name);
        writer.write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        if (!props_data.empty()) {
            writer.write_bytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
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

    [[Async]]
    void UnitTestSuspend()
    {
        Yield(60000);
    }

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

    bool UnitTestIsItemNull(Item? item)
    {
        return item is null;
    }

    bool UnitTestMatchesHash(hstring value)
    {
        return value == "UnitTestHash".hstr();
    }

}
)"},
            },
            [](string_view message) {
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }
#endif

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeInitGateScriptBinary(const FileSystem& metadata_resources, ServerTestScriptMode mode) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        string_view body = mode == ServerTestScriptMode::StopOnInit ? string_view {"return EventResult::StopChain;"} : string_view {R"(throw("Unit test OnInit failure");
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
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }
#endif

    inline auto MakeServerTestResources(ServerTestScriptMode script_mode = ServerTestScriptMode::Default, vector<uint8_t> metadata_blob = BakerTests::MakeEmptyMetadataBlob()) -> FileSystem
    {
        auto compiler_resources_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerEngineCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        hstring location_type = proto_engine.Hashes.to_hashed_string("Location");
        hstring map_type = proto_engine.Hashes.to_hashed_string("Map");
        hstring item_type = proto_engine.Hashes.to_hashed_string("Item");
        auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestRat");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "UnitTestLocation");
        auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "UnitTestMap", SERVER_TEST_MAP_SIZE);
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        auto fomap_blob = MakeEmptyMapBlob();
#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = script_mode == ServerTestScriptMode::Default ? MakeScriptBinary(compiler_resources) : MakeInitGateScriptBinary(compiler_resources, script_mode);
#else
        REQUIRE(script_mode == ServerTestScriptMode::Default);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ServerEngineTest.fopro-bin-server", proto_blob);
        runtime_source->AddFile("UnitTestLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("UnitTestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("UnitTestItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("UnitTestMap.fomap-bin-server", fomap_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile(script_mode == ServerTestScriptMode::Default ? "ServerEngineTest.fos-bin-server" : "ServerEngineInitGateTest.fos-bin-server", std::move(script_blob));
#endif

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    inline auto WaitForServerStart(ptr<ServerEngine> server) -> string
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

    inline auto MakeServerEngine(GlobalSettings& settings, optional<ServerSnapshotRestore> restore_snapshot = std::nullopt) -> refcount_ptr<ServerEngine>
    {
        return safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerTestResources(), std::move(restore_snapshot));
    }

    inline void CheckServerStartupFailsSafely(GlobalSettings& settings, FileSystem&& resources)
    {
        auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources));

        string startup_error = WaitForServerStart(server);
        INFO(startup_error);
        CHECK_FALSE(startup_error.empty());
        CHECK(server->IsStartingError());
        CHECK_FALSE(server->IsStarted());

        REQUIRE_NOTHROW(server->Shutdown());
    }

    inline void CheckServerStartupFailsSafely(GlobalSettings& settings)
    {
        CheckServerStartupFailsSafely(settings, MakeServerTestResources());
    }

    inline auto MakeServerStorageDoc(int64_t value) -> AnyData::Document
    {
        AnyData::Document doc;
        doc.Assign(string {"value"}, value);
        return doc;
    }

    inline auto CreateLoggedPlayer(ptr<ServerEngine> server, string_view name) -> ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto not_logged_in_player = server->CreateNotLoggedInPlayer(std::move(net_connection));
        server->RequireCurrentSyncContext()->SyncEntity(not_logged_in_player);

        not_logged_in_player->SetName(name);
        not_logged_in_player->SetLastControlledCritterId(ident_t {1});
        auto player = server->LoginPlayerToNewRecord(not_logged_in_player);

        return player;
    }

    inline auto CreateStandalonePlayer(ptr<ServerEngine> server, string_view name) -> refcount_ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings);
        auto connection = safe_alloc::make_unique<ServerConnection>(server->Settings, std::move(net_connection), BakerTests::MakeTestChannelIdentity());
        auto player = safe_alloc::make_refcounted<Player>(server, ident_t {}, std::move(connection));

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

    inline auto MakeServerMovementContext(msize map_size, mpos start_hex, nanotime start_time) -> refcount_ptr<MovingContext>
    {
        return safe_alloc::make_refcounted<MovingContext>(map_size, SERVER_TEST_MOVE_SPEED, SERVER_TEST_MOVE_STEPS, SERVER_TEST_MOVE_CONTROL_STEPS, start_time, timespan {}, start_hex, ipos16 {}, ipos16 {});
    }

    inline auto WaitForUnlockedServerCondition(ptr<ServerEngine> server, bool& locked, const function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        FO_VERIFY_AND_THROW(locked, "Server must be locked before waiting on condition");

        server->Unlock();
        locked = false;

        auto deadline = std::chrono::steady_clock::now() + timeout;

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

FO_END_NAMESPACE

