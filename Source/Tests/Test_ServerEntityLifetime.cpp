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
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedServerSettings(settings);
    BakerTests::OverrideSetting(settings.Server.DbStorage, string {"Memory"});
    BakerTests::OverrideSetting(settings.Server.WriteHealthFile, false);
    BakerTests::OverrideSetting(settings.Server.WorkerThreads, int32_t {1});
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    return settings;
}

static auto MakeServerEntityLifetimeResources() -> FileSystem
{
    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerEntityLifetimeCompiler");
    compiler_source->AddFile("Metadata.fometa-server", metadata);

    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerServerEngine proto_engine {compiler_resources};

    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerEntityLifetimeRuntime");
    source->AddFile("Metadata.fometa-server", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    source->AddFile("ServerEntityLifetime.fos-bin-server", BakerTests::CompileInlineScripts(&proto_engine, "ServerEntityLifetimeScripts", {{"Scripts/ServerEntityLifetime.fos", "void LifetimeFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    source->AddFile("LifetimeCritter.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, proto_engine.Hashes.to_hashed_string("Critter"), "LifetimeCritter"));
    source->AddFile("LifetimeItem.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, proto_engine.Hashes.to_hashed_string("Item"), "LifetimeItem"));
    source->AddFile("LifetimeLocation.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, proto_engine.Hashes.to_hashed_string("Location"), "LifetimeLocation"));
    vector<pair<string, function<void(ProtoMap&)>>> map_protos;
    map_protos.emplace_back("LifetimeMap", [](ProtoMap& proto) { proto.SetSize(msize {2, 2}); });
    source->AddFile("LifetimeMap.fopro-bin-server", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(proto_engine, proto_engine.Hashes.to_hashed_string("Map"), map_protos));

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

static auto WaitForServerEntityLifetimeStartup(ptr<ServerEngine> server) -> bool
{
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
    // The caller quiesces the server after complete startup; registrar setup and hash interning cannot race it
    auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
    auto item_proto = server->GetProtoItem(server->Hashes.to_hashed_string("LifetimeItem"));
    auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
    auto location_proto = server->GetProtoLocation(server->Hashes.to_hashed_string("LifetimeLocation"));
    REQUIRE(critter_proto);
    REQUIRE(item_proto);
    REQUIRE(map_proto);
    REQUIRE(location_proto);

    vector<refcount_ptr<ServerEntity>> entities;
    entities.emplace_back(safe_alloc::make_refcounted<Critter>(server, ident_t {1}, critter_proto));
    entities.emplace_back(safe_alloc::make_refcounted<Item>(server, ident_t {2}, item_proto));
    entities.emplace_back(safe_alloc::make_refcounted<Map>(server, ident_t {3}, map_proto, nullptr, static_map));
    entities.emplace_back(safe_alloc::make_refcounted<Location>(server, ident_t {4}, location_proto));

    auto network = NetworkServer::CreateDummyConnection(server->Settings);
    auto connection = safe_alloc::make_unique<ServerConnection>(server->Settings, std::move(network), BakerTests::MakeTestChannelIdentity());
    entities.emplace_back(safe_alloc::make_refcounted<Player>(server, ident_t {5}, std::move(connection)));
    return entities;
}

// A native owner may outlive Shutdown and be released from another thread, but never outlive the engine itself:
// every entity borrows the engine's registrars, protos and interned hashes, so ~ServerEngine asserts the count
TEST_CASE("ServerEntityOwnersReleasedAfterShutdownOnAnotherThread", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    bool shutdown_done = false;
    auto shutdown_guard = scope_exit([&]() noexcept {
        if (!shutdown_done) {
            safe_call([&server] { server->Shutdown(); });
        }
    });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    REQUIRE_FALSE(server->IsStartingError());

    vector<refcount_ptr<ServerEntity>> retained;
    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) { retained = MakeServerEntityLifetimeOwners(server, &static_map); }));
    REQUIRE(retained.size() == 5);

    for (const auto& entity : retained) {
        REQUIRE(entity->GetRefCount() == 1);
    }

    REQUIRE_NOTHROW(server->Shutdown());
    shutdown_done = true;
    REQUIRE(server->IsShutdownInProgress());

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

    // The engine outlives its last entity, so nothing above dereferenced a freed registrar
    CHECK(server->GetRefCount() == 1);
}

// Baked map files author static item ids from one upward and the client indexes them together with runtime
// items, so a generated world has to start above them exactly as a restored one does
TEST_CASE("ServerGeneratedWorldDrawsEntityIdsAboveTheConfiguredStart", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    ptr<Critter> cr = server->CreateCritter(server->Hashes.to_hashed_string("LifetimeCritter"), false);

    CHECK(cr->GetId().underlying_value() > settings.Server.EntityStartId);
    CHECK(server->GetLastEntityId().underlying_value() >= cr->GetId().underlying_value());
}

TEST_CASE("ServerEntityOwnersReleaseBeforeShutdown", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
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

// A released map goes straight to the job queued for it, so a widen that let go of the map would come back only after
// that job had finished; the queued waiter below takes the map exactly when this context lets it go
TEST_CASE("ServerSyncWidenOfCoveredEntityKeepsHeldLocks", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    // Unregistered owners are enough: the widen reads only each entity's own lock and its parent chain
    refcount_nptr<Map> map;
    refcount_nptr<Critter> holder;
    refcount_nptr<Critter> neighbour;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        holder = safe_alloc::make_refcounted<Critter>(server, ident_t {2}, critter_proto);
        holder->SetParent(map);
        neighbour = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        neighbour->SetParent(map);
    }));

    auto map_lock = map->GetEntityLock();
    REQUIRE(static_cast<bool>(map_lock));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    vector<ptr<ServerEntity>> held {holder, map};
    ctx.SyncEntities(held);
    REQUIRE(map_lock->IsLockedByCurrentThread());

    std::atomic_bool waiter_took_map {false};

    std::thread waiter([&]() {
        SyncContext waiter_ctx;
        waiter_ctx.Activate();
        vector<ptr<ServerEntity>> wanted {map};
        waiter_ctx.SyncEntities(wanted);
        waiter_took_map.store(true);
        waiter_ctx.Release();
        waiter_ctx.Deactivate();
    });
    auto join_waiter = scope_exit([&ctx, &waiter]() noexcept {
        ctx.Release();
        waiter.join();
    });

    while (map_lock->WaiterCount() == 0) {
        coarse_sleep(std::chrono::milliseconds {1});
    }

    // Both entry points: the native widen and a full request that keeps every held entity
    vector<ptr<ServerEntity>> extras {neighbour};
    ctx.WidenEntities(extras);

    CHECK_FALSE(waiter_took_map.load());
    CHECK(map_lock->IsLockedByCurrentThread());
    CHECK(neighbour->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(ctx.GetHeldEntities().size() == 3);

    vector<ptr<ServerEntity>> same_cover {holder, map, neighbour};
    ctx.SyncEntities(same_cover);

    CHECK_FALSE(waiter_took_map.load());
    CHECK(ctx.GetHeldEntities().size() == 3);

    // Only letting the cover go hands the map on
    ctx.Release();
    waiter.join();
    join_waiter.release();
    CHECK(waiter_took_map.load());
}

// A finish handler runs on the thread destroying its subject, so a widen inside it keeps that subject held
TEST_CASE("ServerSyncWidenKeepsHeldEntityBeingDestroyed", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    refcount_nptr<Map> map;
    refcount_nptr<Critter> dying;
    refcount_nptr<Critter> neighbour;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        dying = safe_alloc::make_refcounted<Critter>(server, ident_t {2}, critter_proto);
        dying->SetParent(map);
        neighbour = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        neighbour->SetParent(map);
    }));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    vector<ptr<ServerEntity>> held {dying};
    ctx.SyncEntities(held);
    dying->MarkAsDestroying();

    vector<ptr<ServerEntity>> extras {neighbour};
    ctx.WidenEntities(extras);

    CHECK(dying->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(neighbour->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(ctx.GetHeldEntities().size() == 2);

    // Destruction completes once the handler returns, and only then does a widen let the entity go
    dying->MarkAsDestroyed();
    ctx.WidenEntities({});

    CHECK_FALSE(dying->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(ctx.GetHeldEntities().size() == 1);
}

TEST_CASE("ServerSyncYieldHandsTheWholeThreadCoverToWaitersAndTakesItBack", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    refcount_nptr<Map> map;
    refcount_nptr<Critter> holder;
    refcount_nptr<Critter> neighbour;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        holder = safe_alloc::make_refcounted<Critter>(server, ident_t {2}, critter_proto);
        holder->SetParent(map);
        neighbour = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        neighbour->SetParent(map);
    }));

    auto map_lock = map->GetEntityLock();
    auto neighbour_lock = neighbour->GetEntityLock();
    REQUIRE(static_cast<bool>(map_lock));
    REQUIRE(static_cast<bool>(neighbour_lock));

    // The outer context stands for the job's own cover, which a nested Sync cannot give away
    SyncContext outer;
    outer.Activate();
    auto deactivate_outer = scope_exit([&outer]() noexcept {
        outer.Release();
        outer.Deactivate();
    });

    vector<ptr<ServerEntity>> job_cover {holder, map};
    outer.SyncEntities(job_cover);

    SyncContext nested;
    nested.Activate();
    auto deactivate_nested = scope_exit([&nested]() noexcept {
        nested.Release();
        nested.Deactivate();
    });

    vector<ptr<ServerEntity>> nested_cover {neighbour};
    nested.SyncEntities(nested_cover);

    int32_t map_recursion = map_lock->GetExclusiveRecursionForCurrentThread();
    REQUIRE(map_recursion > 0);
    REQUIRE(neighbour_lock->IsLockedByCurrentThread());

    std::atomic_bool waiter_took_map {false};

    std::thread waiter([&]() {
        SyncContext waiter_ctx;
        waiter_ctx.Activate();
        vector<ptr<ServerEntity>> wanted {map};
        waiter_ctx.SyncEntities(wanted);
        waiter_took_map.store(true);
        waiter_ctx.Release();
        waiter_ctx.Deactivate();
    });
    auto join_waiter = scope_exit([&outer, &nested, &waiter]() noexcept {
        nested.Release();
        outer.Release();
        waiter.join();
    });

    while (map_lock->WaiterCount() == 0) {
        coarse_sleep(std::chrono::milliseconds {1});
    }

    // A re-sync of the same cover keeps the map (the widen test pins that); the yield alone hands it on
    nested.YieldLocks();

    CHECK(waiter_took_map.load());
    CHECK(map_lock->GetExclusiveRecursionForCurrentThread() == map_recursion);
    CHECK(holder->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(neighbour_lock->IsLockedByCurrentThread());
    CHECK(outer.GetHeldEntities().size() == 2);
    CHECK(nested.GetHeldEntities().size() == 1);

    waiter.join();
    join_waiter.release();
}

TEST_CASE("ServerSyncYieldRefusesWhileTheSingletonIsHeld", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    // Nothing held is nothing to hand on
    CHECK_NOTHROW(ctx.YieldLocks());

    ctx.LockSingleton(server->GetEntityLock());
    CHECK_THROWS_AS(ctx.YieldLocks(), EntitySyncException);
    CHECK(server->GetEntityLock()->IsLockedByCurrentThread());
}

TEST_CASE("ServerSyncRetainedCoverRefreshesReparentedAncestors", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, false};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    refcount_nptr<Map> old_map;
    refcount_nptr<Map> new_map;
    refcount_nptr<Critter> cr;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        old_map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        new_map = safe_alloc::make_refcounted<Map>(server, ident_t {2}, map_proto, nullptr, &static_map);
        cr = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        cr->SetParent(old_map);
    }));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    ctx.SyncEntity(cr);
    REQUIRE(old_map->GetEntityLock()->GetDescendantHoldCountForCurrentThread() == 1);

    // A nested transfer changes the parent while the outer context still records the old ancestor mark
    {
        SyncContext transfer_ctx;
        transfer_ctx.Activate();
        auto release_transfer = scope_exit([&transfer_ctx]() noexcept {
            transfer_ctx.Release();
            transfer_ctx.Deactivate();
        });
        vector<ptr<ServerEntity>> transfer {cr, old_map, new_map};
        transfer_ctx.SyncEntities(transfer);
        cr->SetParent(new_map);
    }

    SECTION("ReplaceSameCover")
    {
        ctx.SyncEntity(cr);
    }
    SECTION("WidenSameCover")
    {
        vector<ptr<ServerEntity>> extras {cr};
        ctx.WidenEntities(extras);
    }
    SECTION("WidenNoExtras")
    {
        ctx.WidenEntities({});
    }

    CHECK(cr->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(new_map->GetEntityLock()->GetDescendantHoldCountForCurrentThread() == 1);
    CHECK(old_map->GetEntityLock()->GetDescendantHoldCountForCurrentThread() == 0);

    // Probe from another thread without blocking: the new map must exclude a foreign exclusive acquisition
    std::atomic_bool took_map {false};
    std::thread probe([&]() {
        if (new_map->GetEntityLock()->TryAcquire()) {
            took_map.store(true);
            new_map->GetEntityLock()->Release();
        }
    });
    probe.join();
    CHECK_FALSE(took_map.load());
}

FO_END_NAMESPACE
