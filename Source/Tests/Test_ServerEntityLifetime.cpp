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

#include "Server.h"
#include "StaticMap.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeServerEntityLifetimeSettings() -> GlobalSettings
{
    FO_STACK_TRACE_ENTRY();

    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedServerSettings(settings);
    BakerTests::OverrideSetting(settings.DbStorage, string {"Memory"});
    BakerTests::OverrideSetting(settings.WriteHealthFile, false);
    BakerTests::OverrideSetting(settings.WorkerThreads, int32_t {1});
    BakerTests::OverrideSetting(settings.Packaged, false);
    BakerTests::OverrideSetting(settings.BakeOutput, string {});
    return settings;
}

static auto MakeServerEntityLifetimeResources() -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerEntityLifetimeCompiler");
    compiler_source->AddFile("Metadata.fometa-server", metadata);

    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerServerEngine proto_engine {compiler_resources};

    auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerEntityLifetimeRuntime");
    source->AddFile("Metadata.fometa-server", metadata);
    source->AddFile("LifetimeCritter.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, proto_engine.Hashes.ToHashedString("Critter"), "LifetimeCritter"));
    source->AddFile("LifetimeItem.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, proto_engine.Hashes.ToHashedString("Item"), "LifetimeItem"));
    source->AddFile("LifetimeLocation.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, proto_engine.Hashes.ToHashedString("Location"), "LifetimeLocation"));
    vector<pair<string, function<void(ProtoMap&)>>> map_protos;
    map_protos.emplace_back("LifetimeMap", [](ProtoMap& proto) { proto.SetSize(msize {2, 2}); });
    source->AddFile("LifetimeMap.fopro-bin-server", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(proto_engine, proto_engine.Hashes.ToHashedString("Map"), map_protos));

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

static auto WaitForServerEntityLifetimeStartup(ptr<ServerEngine> server) -> bool
{
    FO_STACK_TRACE_ENTRY();

    nanotime deadline = nanotime::now() + std::chrono::seconds {30};

    while (nanotime::now() < deadline) {
        if (server->IsStartingError()) {
            return false;
        }
        if (server->IsStarted()) {
            return true;
        }

        coarse_sleep(std::chrono::milliseconds {10});
    }

    return false;
}

static auto MakeServerEntityLifetimeOwners(ptr<ServerEngine> server, ptr<StaticMap> static_map) -> vector<refcount_ptr<ServerEntity>>
{
    FO_STACK_TRACE_ENTRY();

    // The caller quiesces the server after complete startup; registrar setup and hash interning cannot race it
    auto critter_proto = server->GetProtoCritter(server->Hashes.ToHashedString("LifetimeCritter"));
    auto item_proto = server->GetProtoItem(server->Hashes.ToHashedString("LifetimeItem"));
    auto map_proto = server->GetProtoMap(server->Hashes.ToHashedString("LifetimeMap"));
    auto location_proto = server->GetProtoLocation(server->Hashes.ToHashedString("LifetimeLocation"));
    REQUIRE(critter_proto);
    REQUIRE(item_proto);
    REQUIRE(map_proto);
    REQUIRE(location_proto);

    vector<refcount_ptr<ServerEntity>> entities;
    entities.emplace_back(SafeAlloc::MakeRefCounted<Critter>(server, ident_t {1}, critter_proto));
    entities.emplace_back(SafeAlloc::MakeRefCounted<Item>(server, ident_t {2}, item_proto));
    entities.emplace_back(SafeAlloc::MakeRefCounted<Map>(server, ident_t {3}, map_proto, nullptr, static_map));
    entities.emplace_back(SafeAlloc::MakeRefCounted<Location>(server, ident_t {4}, location_proto));

    auto network = NetworkServer::CreateDummyConnection(server->Settings);
    auto connection = SafeAlloc::MakeUnique<ServerConnection>(server->Settings, std::move(network));
    entities.emplace_back(SafeAlloc::MakeRefCounted<Player>(server, ident_t {5}, std::move(connection)));
    return entities;
}

TEST_CASE("ServerEntityOwnersOutliveServer", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    vector<refcount_ptr<ServerEntity>> retained;

    {
        auto server = SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
        bool shutdown_done = false;
        auto shutdown_guard = scope_exit([&]() noexcept {
            if (!shutdown_done) {
                safe_call([&server] { server->Shutdown(); });
            }
        });

        REQUIRE(WaitForServerEntityLifetimeStartup(server));
        REQUIRE_FALSE(server->IsStartingError());
        REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) { retained = MakeServerEntityLifetimeOwners(server, &static_map); }));
        REQUIRE(retained.size() == 5);

        for (const auto& entity : retained) {
            REQUIRE(entity->GetRefCount() == 1);
        }

        // No entity owns the server; this scope must drop its final native owner before the deferred releases
        REQUIRE(server->GetRefCount() == 1);
        REQUIRE_NOTHROW(server->Shutdown());
        shutdown_done = true;
        REQUIRE(server->IsShutdownInProgress());
        REQUIRE(server->GetRefCount() == 1);
    }

    std::atomic_size_t released {0};
    std::thread::id releasing_thread;
    std::thread::id owning_thread = std::this_thread::get_id();
    std::jthread finalizer([entities = std::move(retained), &released, &releasing_thread]() mutable {
        releasing_thread = std::this_thread::get_id();

        while (!entities.empty()) {
            entities.pop_back();
            released.fetch_add(1, std::memory_order_relaxed);
        }
    });
    finalizer.join();

    CHECK(releasing_thread != owning_thread);
    CHECK(released.load(std::memory_order_relaxed) == 5);
}

TEST_CASE("ServerEntityOwnersReleaseBeforeShutdown", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    bool shutdown_done = false;
    auto shutdown_guard = scope_exit([&]() noexcept {
        if (!shutdown_done) {
            safe_call([&server] { server->Shutdown(); });
        }
    });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    REQUIRE_FALSE(server->IsShutdownInProgress());
    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        vector<refcount_ptr<ServerEntity>> entities = MakeServerEntityLifetimeOwners(server, &static_map);
        REQUIRE(entities.size() == 5);

        for (const auto& entity : entities) {
            REQUIRE(entity->GetRefCount() == 1);
        }

        // Ordinary destruction still executes the existing empty-association checks while the engine is alive
        entities.clear();
        CHECK(entities.empty());
    }));
    CHECK_FALSE(server->IsShutdownInProgress());
    REQUIRE_NOTHROW(server->Shutdown());
    shutdown_done = true;
    CHECK(server->IsShutdownInProgress());
}

FO_END_NAMESPACE
