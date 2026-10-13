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

#include "Test_ServerItemsHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ServerItemsTests;

TEST_CASE("ServerItemDetachedMoves")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    hstring critter_pid = fn("TestCritter");
    hstring item_pid = fn("TestItem");

    auto owner = server->CreateCritter(critter_pid, false);
    auto receiver = server->CreateCritter(critter_pid, false);
    auto source = server->CrMngr.AddItemToCritter(owner, server->ItemMngr.CreateItem(item_pid, nullptr), true);

    SECTION("CloneIsDetachedCopyPlacedByMove")
    {
        auto clone = server->ItemMngr.CloneItem(source);
        CHECK(clone->GetId() != source->GetId());
        CHECK(clone->GetProtoId() == item_pid);
        CHECK(clone->GetOwnership() == ItemOwnership::Nowhere);

        auto moved = server->ItemMngr.MoveItem(clone, receiver);
        REQUIRE(static_cast<bool>(moved));
        CHECK(moved->GetId() == clone->GetId());
        CHECK(clone->GetOwnership() == ItemOwnership::CritterInventory);
        CHECK(static_cast<bool>(receiver->GetInvItem(clone->GetId())));
        CHECK(static_cast<bool>(owner->GetInvItem(source->GetId())));
    }

    SECTION("DetachedItemIsPlacedByMove")
    {
        auto detached = server->ItemMngr.CreateItem(item_pid, nullptr);
        CHECK(detached->GetOwnership() == ItemOwnership::Nowhere);

        auto moved = server->ItemMngr.MoveItem(detached, receiver);
        REQUIRE(static_cast<bool>(moved));
        CHECK(moved->GetId() == detached->GetId());
        CHECK(static_cast<bool>(receiver->GetInvItem(detached->GetId())));
    }

    SECTION("AbandonedCloneIsDestroyedByCreator")
    {
        auto clone = server->ItemMngr.CloneItem(source);
        ident_t clone_id = clone->GetId();
        server->ItemMngr.DestroyItem(clone);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(clone_id)));
        CHECK(static_cast<bool>(owner->GetInvItem(source->GetId())));
    }

    SECTION("ContainerWithContentsIsNotCloned")
    {
        (void)source->AddItemToContainer(server->ItemMngr.CreateItem(item_pid, nullptr), {});
        CHECK_THROWS(server->ItemMngr.CloneItem(source));
    }

    server->CrMngr.DestroyCritter(receiver);
    server->CrMngr.DestroyCritter(owner);
}

TEST_CASE("ServerEntityManagerQueries")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    hstring critter_pid = fn("TestCritter");
    hstring item_pid = fn("TestItem");

    // Initial entity counts
    CHECK(server->EntityMngr.GetLocationsCount() == 0);
    CHECK(server->EntityMngr.GetMapsCount() == 0);
    CHECK(server->EntityMngr.GetPlayersCount() == 0);

    // Entities collection
    vector<refcount_ptr<ServerEntity>> entities = server->EntityMngr.GetEntities();
    auto entities_before = entities.size();

    // Items collection access
    vector<refcount_ptr<Item>> items = server->EntityMngr.GetItems();
    auto items_before = items.size();

    auto item = server->ItemMngr.CreateItem(item_pid, nullptr);
    CHECK(server->EntityMngr.GetItems().size() == items_before + 1);

    // Critters collection access
    vector<refcount_ptr<Critter>> critters = server->EntityMngr.GetCritters();
    auto cr_before = critters.size();

    auto cr = server->CreateCritter(critter_pid, false);
    CHECK(server->EntityMngr.GetCritters().size() == cr_before + 1);

    // Locations collection (should be empty in this test)
    vector<refcount_ptr<Location>> locations = server->EntityMngr.GetLocations();
    CHECK(locations.empty());

    // Maps collection (should be empty)
    vector<refcount_ptr<Map>> maps = server->EntityMngr.GetMaps();
    CHECK(maps.empty());

    // Players collection (no players connected)
    vector<refcount_ptr<Player>> players = server->EntityMngr.GetPlayers();
    CHECK(players.empty());

    // Entities grew after creating item and critter
    CHECK(server->EntityMngr.GetEntities().size() >= entities_before + 2);

    // Non-existent entity returns null
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetEntity(ident_t {999999})));
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(ident_t {999999})));
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(ident_t {999999})));
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetLocation(ident_t {999999})));
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetMap(ident_t {999999})));
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetPlayer(ident_t {999999})));

    // Cleanup
    server->ItemMngr.DestroyItem(item);
    server->CrMngr.DestroyCritter(cr);
}

FO_END_NAMESPACE

