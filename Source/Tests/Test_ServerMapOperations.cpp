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
    auto settings = MakeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeResources());
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });
    string startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    hstring static_map_pid = server->Hashes.to_hashed_string("StaticMap");
    hstring static_critter_pid = server->Hashes.to_hashed_string("TestStaticCritter");
    hstring visible_item_pid = server->Hashes.to_hashed_string("TestStaticItem");
    hstring hidden_item_pid = server->Hashes.to_hashed_string("TestStaticHiddenItem");
    auto map_proto = server->GetProtoMap(static_map_pid);
    REQUIRE(map_proto);

    auto static_map = server->MapMngr.GetStaticMap(map_proto);

    const_span<StaticMap::CritterBillet> critter_billets = static_map->GetCritterBillets();
    REQUIRE(critter_billets.size() == 1);
    CHECK(critter_billets.front().first == ident_t {11});
    CHECK(critter_billets.front().second->GetProtoId() == static_critter_pid);
    CHECK(critter_billets.front().second->GetHex() == mpos {10, 11});

    REQUIRE(static_map->GetOwnedItemBillets().size() == 2);
    REQUIRE(static_map->GetStaticItems().size() == 2);
    REQUIRE(static_map->HasStaticItem(ident_t {21}));
    REQUIRE(static_map->HasStaticItem(ident_t {22}));

    auto visible_item = static_map->GetStaticItem(ident_t {21});
    auto hidden_item = static_map->GetStaticItem(ident_t {22});
    REQUIRE(visible_item);
    REQUIRE(hidden_item);
    CHECK(visible_item->GetProtoId() == visible_item_pid);
    CHECK(hidden_item->GetProtoId() == hidden_item_pid);
    CHECK(visible_item->GetHex() == mpos {12, 13});
    CHECK(hidden_item->GetHex() == mpos {14, 15});

    const auto& visible_field = static_map->GetField(mpos {12, 13});
    CHECK(std::ranges::find(visible_field.StaticItems, visible_item) != visible_field.StaticItems.end());
    CHECK(visible_field.MoveBlocked);
    CHECK(visible_field.ShootBlocked);

    const auto& hidden_field = static_map->GetField(mpos {14, 15});
    CHECK(std::ranges::find(hidden_field.StaticItems, hidden_item) != hidden_field.StaticItems.end());
    CHECK(hidden_field.MoveBlocked);
    CHECK(hidden_field.ShootBlocked);
}

TEST_CASE("MapPathGagChecksRespectStaticBlockers", "[map-path]")
{
    auto settings = MakeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeResources());
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });
    string startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    array<hstring, 1> map_pids {server->Hashes.to_hashed_string("StaticMap")};
    auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"), map_pids);
    auto maps = location->GetMaps();
    REQUIRE(maps.size() == 1);
    auto map = maps.front();
    auto allow_gag = [](ptr<const Item>) { return true; };

    for (mpos hex : {mpos {12, 13}, mpos {14, 15}, mpos {11, 14}}) {
        auto gag = server->ItemMngr.CreateItemOnHex(map, hex, server->Hashes.to_hashed_string("TestItem"), nullptr);
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
    auto settings = MakeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeResources());
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });
    string startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const hstring map_pids[] = {server->Hashes.to_hashed_string("TestMap")};
    auto loc = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"), map_pids);
    auto map = loc->GetMaps().front();
    auto cr = server->CrMngr.CreateCritterOnMap(server->Hashes.to_hashed_string("TestCritter"), nullptr, map, mpos {20, 20}, mdir {0});
    auto from_hex = cr->GetHex();
    auto to_hex = cr->GetHex();

    for (int32_t i = 0; i < 2; i++) {
        REQUIRE(GeometryHelper::MoveHexByDir(from_hex, mdir {0}.reverse(), map->GetSize()));
        REQUIRE(GeometryHelper::MoveHexByDir(to_hex, mdir {0}, map->GetSize()));
    }

    auto crosses_critter = [&](const FindPathOutput& path) {
        auto hex = from_hex;

        for (auto dir : path.Steps) {
            REQUIRE(GeometryHelper::MoveHexByDir(hex, dir, map->GetSize()));

            if (hex == cr->GetHex()) {
                return true;
            }
        }

        return false;
    };

    auto check_paths = [&](bool should_cross) {
        auto path = server->MapMngr.FindPath(map, nullptr, from_hex, to_hex, 0, 0);
        REQUIRE(path.Result == FindPathOutput::ResultType::Ok);
        CHECK(crosses_critter(path) == should_cross);

        const mpos targets[] = {to_hex};
        auto path_to_any = server->MapMngr.FindPathToAny(map, nullptr, from_hex, targets, 0);
        REQUIRE(path_to_any.Result == FindPathOutput::ResultType::Ok);
        CHECK(crosses_critter(path_to_any) == should_cross);
    };

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

FO_END_NAMESPACE

