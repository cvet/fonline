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

#include "Item.h"
#include "Location.h"
#include "Test_ServerMapOperationsHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ServerMapOperationsTests;

TEST_CASE("MapManagerLoadsStaticMapEntities")
{
    auto settings {ServerMapOperationsTests::MakeSettings()};
    auto server {safe_alloc::make_refcounted<ServerEngine>(&settings, ServerMapOperationsTests::MakeResources())};
    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};
    string startup_error {ServerMapOperationsTests::WaitForStart(server.get())};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    hstring static_map_pid {server->Hashes.to_hashed_string("StaticMap")};
    hstring static_critter_pid {server->Hashes.to_hashed_string("TestStaticCritter")};
    hstring visible_item_pid {server->Hashes.to_hashed_string("TestStaticItem")};
    hstring hidden_item_pid {server->Hashes.to_hashed_string("TestStaticHiddenItem")};
    auto map_proto {server->GetProtoMap(static_map_pid)};
    REQUIRE(map_proto);

    auto static_map {server->MapMngr.GetStaticMap(map_proto)};

    const_span<StaticMap::CritterBillet> critter_billets {static_map->GetCritterBillets()};
    REQUIRE(critter_billets.size() == 1);
    CHECK(critter_billets.front().first == ident_t {11});
    CHECK(critter_billets.front().second->GetProtoId() == static_critter_pid);
    CHECK(critter_billets.front().second->GetHex() == mpos {10, 11});

    REQUIRE(static_map->GetOwnedItemBillets().size() == 2);
    REQUIRE(static_map->GetStaticItems().size() == 2);
    REQUIRE(static_map->HasStaticItem(ident_t {21}));
    REQUIRE(static_map->HasStaticItem(ident_t {22}));

    auto visible_item {static_map->GetStaticItem(ident_t {21})};
    auto hidden_item {static_map->GetStaticItem(ident_t {22})};
    REQUIRE(visible_item);
    REQUIRE(hidden_item);
    CHECK(visible_item->GetProtoId() == visible_item_pid);
    CHECK(hidden_item->GetProtoId() == hidden_item_pid);
    CHECK(visible_item->GetHex() == mpos {12, 13});
    CHECK(hidden_item->GetHex() == mpos {14, 15});

    const auto& visible_field {static_map->GetField(mpos {12, 13})};
    CHECK(std::ranges::find(visible_field.StaticItems, visible_item) != visible_field.StaticItems.end());
    CHECK(visible_field.MoveBlocked);
    CHECK(visible_field.ShootBlocked);

    const auto& hidden_field {static_map->GetField(mpos {14, 15})};
    CHECK(std::ranges::find(hidden_field.StaticItems, hidden_item) != hidden_field.StaticItems.end());
    CHECK(hidden_field.MoveBlocked);
    CHECK(hidden_field.ShootBlocked);
}

TEST_CASE("MapLoadsAndUnloadsContentOfOtherMaps")
{
    auto settings {ServerMapOperationsTests::MakeSettings()};
    auto server {safe_alloc::make_refcounted<ServerEngine>(&settings, ServerMapOperationsTests::MakeResources())};
    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};
    string startup_error {ServerMapOperationsTests::WaitForStart(server.get())};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    hstring content_pid {server->Hashes.to_hashed_string("ContentMap")};
    hstring static_map_pid {server->Hashes.to_hashed_string("StaticMap")};
    auto content_proto {server->GetProtoMap(content_pid)};
    REQUIRE(content_proto);

    // Baked content is read on first use, never at startup
    CHECK_FALSE(server->MapMngr.IsStaticMapLoaded(content_proto));

    // Id offsets derive from the placement, as a project would make them
    auto id_offset {[](mpos dest_hex) -> int64_t { return (int64_t {1} << 50) + int64_t {dest_hex.x} * 1000000 + int64_t {dest_hex.y} * 1000; }};

    vector<hstring> map_pids {server->Hashes.to_hashed_string("TestMap"), static_map_pid};
    auto location {server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"), map_pids)};
    REQUIRE(location->GetMaps().size() == 2);
    ptr<Map> map {location->GetMaps().front()};
    ptr<Map> baked_map {location->GetMaps().back()};
    REQUIRE(map->GetStaticItems().empty());

    vector<ident_t> first_ids {server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {100, 120}, id_offset(mpos {100, 120}))};
    vector<ident_t> second_ids {server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {20, 20}, id_offset(mpos {20, 20}))};
    CHECK(server->MapMngr.IsStaticMapLoaded(content_proto));
    REQUIRE(first_ids.size() == 2);
    REQUIRE(second_ids.size() == 2);

    ident_t first_blocker_id {ident_t {(int64_t {1} << 50) + 100120031}};
    ident_t second_blocker_id {ident_t {(int64_t {1} << 50) + 20020031}};
    CHECK(std::ranges::find(first_ids, first_blocker_id) != first_ids.end());
    REQUIRE(map->GetStaticItem(first_blocker_id));
    REQUIRE(map->GetStaticItem(second_blocker_id));
    CHECK(map->GetStaticItem(first_blocker_id)->GetHex() == mpos {102, 123});
    CHECK(map->GetStaticItem(second_blocker_id)->GetHex() == mpos {22, 23});
    CHECK(map->IsRuntimeStaticItem(first_blocker_id));
    CHECK(map->GetStaticItems().size() == 4);
    CHECK(map->GetStaticItemsOnHex(mpos {102, 123}).size() == 1);
    CHECK_FALSE(map->IsHexMovable(mpos {102, 123}));
    CHECK_FALSE(map->IsHexMovable(mpos {22, 23}));
    CHECK(map->IsHexMovable(mpos {101, 123}));
    CHECK(map->IsHexMovable(mpos {2, 3}));

    // Runtime items leave through UnloadRuntimeStaticItems, not the persisted removal list
    CHECK_THROWS(map->RemoveStaticItem(first_blocker_id));
    server->MapMngr.UnloadRuntimeStaticItems(map, first_ids);
    CHECK_FALSE(map->GetStaticItem(first_blocker_id));
    CHECK_FALSE(map->HasStaticItem(first_blocker_id));
    CHECK(map->IsHexMovable(mpos {102, 123}));
    CHECK(map->GetStaticItems().size() == 2);
    CHECK_THROWS(server->MapMngr.UnloadRuntimeStaticItems(map, first_ids));

    // A failed load leaves the map as it was: ids in use, a zero or overflowing id, content outside the map
    CHECK_THROWS(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {20, 20}, id_offset(mpos {20, 20})));
    CHECK_THROWS(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {40, 40}, id_offset(mpos {20, 20})));
    CHECK_THROWS(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {40, 40}, -31));
    CHECK_THROWS(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {40, 40}, std::numeric_limits<int64_t>::max()));
    CHECK_THROWS(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {190, 190}, id_offset(mpos {190, 190})));
    CHECK(map->GetStaticItems().size() == 2);

    // An area of the source map lands with its first hex at offset and must lie inside the source map
    vector<ident_t> area_ids {server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {40, 40}, mpos {2, 3}, msize {1, 1}, id_offset(mpos {40, 40}))};
    REQUIRE(area_ids.size() == 1);
    REQUIRE(map->GetStaticItem(area_ids.front()));
    CHECK(area_ids.front() == ident_t {(int64_t {1} << 50) + 40040031});
    CHECK(map->GetStaticItem(area_ids.front())->GetHex() == mpos {40, 40});
    CHECK_FALSE(map->IsHexMovable(mpos {40, 40}));
    CHECK(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {50, 50}, mpos {5, 5}, msize {3, 3}, id_offset(mpos {50, 50})).empty());
    CHECK_THROWS(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {60, 60}, mpos {10, 10}, msize {7, 1}, id_offset(mpos {60, 60})));
    CHECK_THROWS(server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {60, 60}, mpos {0, 0}, msize {0, 1}, id_offset(mpos {60, 60})));
    server->MapMngr.UnloadRuntimeStaticItems(map, area_ids);
    CHECK(map->GetStaticItems().size() == 2);

    // Loaded items merge with baked ones on a shared hex and leave the baked one when unloaded
    vector<ident_t> merged_ids {server->MapMngr.LoadRuntimeStaticItems(baked_map, content_proto, mpos {10, 10}, id_offset(mpos {10, 10}))};
    CHECK(baked_map->GetStaticItemsOnHex(mpos {12, 13}).size() == 2);
    CHECK(baked_map->GetStaticItems().size() == 4);
    CHECK_FALSE(baked_map->IsHexMovable(mpos {12, 13}));
    REQUIRE(baked_map->RemoveStaticItem(ident_t {21}));
    CHECK(baked_map->GetStaticItemsOnHex(mpos {12, 13}).size() == 1);
    CHECK_FALSE(baked_map->IsHexMovable(mpos {12, 13}));
    server->MapMngr.UnloadRuntimeStaticItems(baked_map, merged_ids);
    CHECK(baked_map->GetStaticItemsOnHex(mpos {12, 13}).empty());
    CHECK(baked_map->IsHexMovable(mpos {12, 13}));
    CHECK(baked_map->GetStaticItems().size() == 1);

    // Critters and dynamic items come as entities of the map
    vector<ident_t> entity_ids {server->MapMngr.GenerateContent(map, content_proto, mpos {50, 60})};
    REQUIRE(entity_ids.size() == 2);
    auto critter {map->GetCritter(entity_ids[0])};
    auto item {map->GetItem(entity_ids[1])};
    REQUIRE(critter);
    REQUIRE(item);
    CHECK(critter->GetHex() == mpos {56, 67});
    CHECK(item->GetHex() == mpos {54, 65});
    CHECK_THROWS(server->MapMngr.GenerateContent(map, content_proto, mpos {195, 0}));

    // Cached content is released on request unless a map instance of that proto exists
    CHECK(server->MapMngr.UnloadStaticMap(content_proto));
    CHECK_FALSE(server->MapMngr.IsStaticMapLoaded(content_proto));
    CHECK(server->MapMngr.UnloadStaticMap(content_proto));
    CHECK_FALSE(server->MapMngr.UnloadStaticMap(baked_map->GetProtoMap()));
    server->MapMngr.LoadStaticMap(content_proto, false);
    CHECK(server->MapMngr.IsStaticMapLoaded(content_proto));
    REQUIRE(server->MapMngr.UnloadStaticMap(content_proto));
    server->MapMngr.LoadStaticMaps(vector<ptr<const ProtoMap>> {content_proto}, false);
    CHECK(server->MapMngr.IsStaticMapLoaded(content_proto));
    server->MapMngr.UnloadRuntimeStaticItems(map, second_ids);
    CHECK(map->GetStaticItems().empty());

    // Concurrent first requests for one map wait for a single read and share its result
    REQUIRE(server->MapMngr.UnloadStaticMap(content_proto));
    constexpr size_t request_count {8};
    vector<nptr<StaticMap>> results(request_count);
    vector<string> errors(request_count);
    std::atomic_bool start {};
    vector<std::thread> requests {};

    for (size_t i {0}; i < request_count; i++) {
        requests.emplace_back([&, i] {
            while (!start.load()) {
                std::this_thread::yield();
            }

            try {
                results[i] = server->MapMngr.GetStaticMap(content_proto);
            }
            catch (const std::exception& ex) {
                errors[i] = ex.what();
            }
        });
    }

    start.store(true);

    for (auto& request : requests) {
        request.join();
    }

    for (size_t i {0}; i < request_count; i++) {
        INFO(errors[i]);
        REQUIRE(results[i]);
        CHECK(results[i] == results.front());
    }

    CHECK(server->MapMngr.IsStaticMapLoaded(content_proto));
    CHECK(results.front()->GetStaticItems().size() == 2);

    // A background load returns at once; a request before it finishes waits for the same read
    REQUIRE(server->MapMngr.UnloadStaticMap(content_proto));
    server->MapMngr.LoadStaticMap(content_proto, true);
    CHECK(server->MapMngr.GetStaticMap(content_proto)->GetStaticItems().size() == 2);
    REQUIRE(server->MapMngr.UnloadStaticMap(content_proto));
    server->MapMngr.LoadStaticMaps(vector<ptr<const ProtoMap>> {content_proto, baked_map->GetProtoMap()}, true);
    auto preloaded {server->MapMngr.GetStaticMap(content_proto)};
    CHECK(preloaded->GetStaticItems().size() == 2);
    CHECK(server->MapMngr.IsStaticMapLoaded(content_proto));
    vector<ident_t> preloaded_ids {server->MapMngr.LoadRuntimeStaticItems(map, content_proto, mpos {60, 60}, id_offset(mpos {60, 60}))};
    CHECK(preloaded_ids.size() == 2);
    server->MapMngr.UnloadRuntimeStaticItems(map, preloaded_ids);
}

TEST_CASE("MapManagerRejectsMapLargerThanEngineMaximum")
{
    if constexpr (GameSettings::MAX_MAP_SIZE < std::numeric_limits<mcoord_t>::max()) {
        auto settings {ServerMapOperationsTests::MakeSettings()};
        msize oversized {numeric_cast<mcoord_t>(GameSettings::MAX_MAP_SIZE + 1), 10};
        auto server {safe_alloc::make_refcounted<ServerEngine>(&settings, ServerMapOperationsTests::MakeResources(oversized))};
        auto shutdown {scope_exit([&server]() noexcept {
            safe_call([&server] {
                if (server->IsStarted() || server->IsStartingError()) {
                    server->Shutdown();
                }
            });
        })};

        // Baked map content is read on first use, so the oversized map fails there instead of at startup
        string startup_error {ServerMapOperationsTests::WaitForStart(server.get())};
        INFO(startup_error);
        REQUIRE(startup_error.empty());
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

        auto map_proto {server->GetProtoMap(server->Hashes.to_hashed_string("TestMap"))};
        REQUIRE(map_proto);
        CHECK_THROWS_WITH(server->MapMngr.GetStaticMap(map_proto), Catch::Matchers::ContainsSubstring("Map size exceeds the engine maximum"));
        CHECK_FALSE(server->MapMngr.IsStaticMapLoaded(map_proto));

        // A failed background read reaches the next request, which reads the map again
        server->MapMngr.LoadStaticMaps(vector<ptr<const ProtoMap>> {map_proto}, true);
        CHECK_THROWS_WITH(server->MapMngr.GetStaticMap(map_proto), Catch::Matchers::ContainsSubstring("Map size exceeds the engine maximum"));
        CHECK_FALSE(server->MapMngr.IsStaticMapLoaded(map_proto));
    }
}

TEST_CASE("MapPathGagChecksRespectStaticBlockers", "[map-path]")
{
    auto settings {ServerMapOperationsTests::MakeSettings()};
    auto server {safe_alloc::make_refcounted<ServerEngine>(&settings, ServerMapOperationsTests::MakeResources())};
    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};
    string startup_error {ServerMapOperationsTests::WaitForStart(server.get())};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    array<hstring, 1> map_pids {server->Hashes.to_hashed_string("StaticMap")};
    auto location {server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"), map_pids)};
    auto maps {location->GetMaps()};
    REQUIRE(maps.size() == 1);
    auto map {maps.front()};
    auto allow_gag {[](ptr<const Item>) { return true; }};

    for (mpos hex : {mpos {12, 13}, mpos {14, 15}, mpos {11, 14}}) {
        auto gag {server->ItemMngr.CreateItemOnHex(map, hex, server->Hashes.to_hashed_string("TestItem"), nullptr)};
        gag->SetNoBlock(false);
        gag->SetShootThru(true);
        gag->SetIsGag(true);
    }

    CHECK(map->IsHexMovable(mpos {11, 13}));
    CHECK_FALSE(map->IsHexMovable(mpos {11, 14}));
    CHECK_FALSE(map->CheckGagItem(mpos {11, 14}, {}));
    CHECK(map->CheckGagItem(mpos {11, 14}, allow_gag));
    CHECK(server->MapMngr.FindPath(map, nullptr, mpos {11, 13}, mpos {11, 14}, 0, 0, {}, allow_gag).Result == FindPathOutput::ResultType::Ok);

    for (mpos target : {mpos {12, 13}, mpos {14, 15}}) {
        CHECK_FALSE(map->IsHexMovable(target));
        CHECK_FALSE(map->CheckGagItem(target, {}));
        CHECK_FALSE(map->CheckGagItem(target, allow_gag));
        CHECK(server->MapMngr.FindPath(map, nullptr, mpos {11, 13}, target, 0, 0, {}, allow_gag).Result == FindPathOutput::ResultType::HexBusy);
    }

    array<mpos, 3> targets {mpos {12, 13}, mpos {14, 15}, mpos {11, 14}};
    CHECK(server->MapMngr.FindReachableHexes(map, mpos {11, 13}, targets, allow_gag) == vector<mpos> {mpos {11, 14}});

    REQUIRE(map->RemoveStaticItem(ident_t {21}));
    CHECK_FALSE(map->IsHexMovable(mpos {12, 13}));
    CHECK(map->CheckGagItem(mpos {12, 13}, allow_gag));
    CHECK(server->MapMngr.FindPath(map, nullptr, mpos {11, 13}, mpos {12, 13}, 0, 0, {}, allow_gag).Result == FindPathOutput::ResultType::Ok);
    CHECK_FALSE(map->CheckGagItem(mpos {14, 15}, allow_gag));
}

TEST_CASE("MapPathfindingObstaclePolicyIsIndependentOfLifeCondition")
{
    auto settings {ServerMapOperationsTests::MakeSettings()};
    auto server {safe_alloc::make_refcounted<ServerEngine>(&settings, ServerMapOperationsTests::MakeResources())};
    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};
    string startup_error {ServerMapOperationsTests::WaitForStart(server.get())};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    const hstring map_pids[] {server->Hashes.to_hashed_string("TestMap")};
    auto loc {server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"), map_pids)};
    auto map {loc->GetMaps().front()};
    auto cr {server->CrMngr.CreateCritterOnMap(server->Hashes.to_hashed_string("TestCritter"), nullptr, map, mpos {20, 20}, mdir {0})};
    auto from_hex {cr->GetHex()};
    auto to_hex {cr->GetHex()};

    for (int32_t i {0}; i < 2; i++) {
        REQUIRE(GeometryHelper::MoveHexByDir(from_hex, mdir {0}.reverse(), map->GetSize()));
        REQUIRE(GeometryHelper::MoveHexByDir(to_hex, mdir {0}, map->GetSize()));
    }

    auto crosses_critter {[&](const FindPathOutput& path) {
        auto hex {from_hex};

        for (auto dir : path.Steps) {
            REQUIRE(GeometryHelper::MoveHexByDir(hex, dir, map->GetSize()));

            if (hex == cr->GetHex()) {
                return true;
            }
        }

        return false;
    }};

    auto check_paths {[&](bool should_cross) {
        auto path {server->MapMngr.FindPath(map, nullptr, from_hex, to_hex, 0, 0)};
        REQUIRE(path.Result == FindPathOutput::ResultType::Ok);
        CHECK(crosses_critter(path) == should_cross);

        const mpos targets[] {to_hex};
        auto path_to_any {server->MapMngr.FindPathToAny(map, nullptr, from_hex, targets, 0)};
        REQUIRE(path_to_any.Result == FindPathOutput::ResultType::Ok);
        CHECK(crosses_critter(path_to_any) == should_cross);
    }};

    cr->SetCondition(CritterCondition::Alive);
    cr->SetPathfindingObstacle(false);
    CHECK_FALSE(map->HasPathfindingObstacle(cr->GetHex(), nullptr));
    check_paths(true);

    cr->SetCondition(CritterCondition::Dead);
    cr->SetPathfindingObstacle(true);
    CHECK(map->HasPathfindingObstacle(cr->GetHex(), nullptr));
    CHECK_FALSE(map->HasPathfindingObstacle(cr->GetHex(), cr));
    check_paths(false);

    cr->SetPathfindingObstacle(false);
    CHECK_FALSE(map->HasPathfindingObstacle(cr->GetHex(), nullptr));
    check_paths(true);
}

TEST_CASE("MapMovableHexesInSightMatchesScalarLineTrace", "[map-visibility]")
{
    auto settings {ServerMapOperationsTests::MakeSettings()};
    auto server {safe_alloc::make_refcounted<ServerEngine>(&settings, ServerMapOperationsTests::MakeResources())};
    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};
    string startup_error {ServerMapOperationsTests::WaitForStart(server.get())};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    hstring static_map_pid {server->Hashes.to_hashed_string("StaticMap")};
    array<hstring, 1> map_pids {static_map_pid};
    auto location {server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"), map_pids)};
    auto isolated_location {server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"), map_pids)};
    auto maps {location->GetMaps()};
    auto isolated_maps {isolated_location->GetMaps()};
    REQUIRE(maps.size() == 1);
    REQUIRE(isolated_maps.size() == 1);
    ptr<Map> map {maps.front()};
    ptr<Map> isolated_map {isolated_maps.front()};

    auto collect_reference {[&](ptr<const Map> target_map, mpos origin, int32_t radius) -> vector<mpos> {
        vector<mpos> expected {};
        msize map_size {target_map->GetSize()};

        for (int32_t x {0}; x < map_size.width; x++) {
            for (int32_t y {0}; y < map_size.height; y++) {
                mpos probe {numeric_cast<int16_t>(x), numeric_cast<int16_t>(y)};

                if (!target_map->IsHexMovable(probe)) {
                    continue;
                }

                int32_t distance {GeometryHelper::GetDistance(origin, probe)};

                if (distance > radius) {
                    continue;
                }

                mpos traced {probe};

                if (distance > 0) {
                    traced = server->MapMngr.TracePath(target_map, origin, probe, distance, 0.0f).PreBlock;
                }

                if (traced == probe) {
                    expected.emplace_back(probe);
                }
            }
        }

        return expected;
    }};

    auto check_query {[&](ptr<const Map> target_map, mpos origin, int32_t radius) {
        vector<mpos> expected {collect_reference(target_map, origin, radius)};

        CHECK(target_map->GetMovableHexesInSight(origin, radius) == expected);
    }};

    check_query(map, mpos {25, 25}, 0);
    check_query(map, mpos {25, 25}, 7);
    check_query(map, mpos {25, 25}, std::numeric_limits<int32_t>::max());

    for (mpos origin : {mpos {0, 25}, mpos {49, 25}, mpos {25, 0}, mpos {25, 49}}) {
        check_query(map, origin, 7);
    }

    mpos control_origin {11, 12};
    mpos blocked_probe {12, 13};
    mpos clear_probe {13, 12};
    int32_t control_radius {3};
    vector<mpos> visible {map->GetMovableHexesInSight(control_origin, control_radius)};
    REQUIRE_FALSE(map->IsHexMovable(blocked_probe));
    REQUIRE(map->IsHexMovable(clear_probe));
    CHECK(std::ranges::find(visible, blocked_probe) == visible.end());
    CHECK(std::ranges::find(visible, clear_probe) != visible.end());
    CHECK(visible == collect_reference(map, control_origin, control_radius));

    vector<mpos> before_invalid {visible};
    CHECK_THROWS(map->GetMovableHexesInSight(control_origin, -1));
    CHECK_THROWS(map->GetMovableHexesInSight(mpos {-1, 0}, control_radius));
    CHECK_THROWS(map->GetMovableHexesInSight(mpos {50, 0}, control_radius));
    CHECK(map->GetMovableHexesInSight(control_origin, control_radius) == before_invalid);

    map->SetHexManualBlock(clear_probe, true, true);
    vector<mpos> manually_blocked {map->GetMovableHexesInSight(control_origin, control_radius)};
    CHECK(std::ranges::find(manually_blocked, clear_probe) == manually_blocked.end());
    CHECK(manually_blocked == collect_reference(map, control_origin, control_radius));

    map->SetHexManualBlock(clear_probe, false, false);
    CHECK(map->GetMovableHexesInSight(control_origin, control_radius) == visible);
    CHECK(isolated_map->GetMovableHexesInSight(control_origin, control_radius) == visible);

    REQUIRE(map->RemoveStaticItem(ident_t {21}));
    CHECK(map->IsHexMovable(blocked_probe));
    CHECK_FALSE(isolated_map->IsHexMovable(blocked_probe));

    vector<mpos> statically_unblocked {map->GetMovableHexesInSight(control_origin, control_radius)};
    CHECK(std::ranges::find(statically_unblocked, blocked_probe) != statically_unblocked.end());
    CHECK(statically_unblocked == collect_reference(map, control_origin, control_radius));
    CHECK(isolated_map->GetMovableHexesInSight(control_origin, control_radius) == visible);
}

FO_END_NAMESPACE
