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

FO_END_NAMESPACE

