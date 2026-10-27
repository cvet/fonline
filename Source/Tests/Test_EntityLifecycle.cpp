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

#include "PropertiesSerializer.h"
#include "Test_EntityLifecycleHelpers.h"

FO_BEGIN_NAMESPACE

using namespace EntityLifecycleTests;

// Same declaration shape as the other script exports driven directly from native tests
bool Server_Critter_MapTransfersLocked(ptr<Critter> self);
map<string, string> Server_Game_DbGetRecord(ptr<ServerEngine> server, hstring collectionName, ident_t id);

TEST_CASE("EntityManagerCppApi")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("GetEntitiesReturnsCorrectCollections")
    {
        size_t initial_critter_count = server->EntityMngr.GetCrittersCount();

        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count + 1);

        size_t after_critter_item_count = server->EntityMngr.GetItemsCount();

        auto item = server->ItemMngr.CreateItem(fn("TestItem"), nullptr);

        CHECK(server->EntityMngr.GetItemsCount() == after_critter_item_count + 1);

        server->ItemMngr.DestroyItem(item);
        CHECK(server->EntityMngr.GetItemsCount() == after_critter_item_count);

        server->CrMngr.DestroyCritter(cr);
        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count);
    }

    SECTION("GetEntityReturnsNullForInvalidId")
    {
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetEntity(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetLocation(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetMap(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetPlayer(ident_t {999999})));
    }

    SECTION("GetEntityFindsCreatedCritter")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        ident_t cr_id = cr->GetId();
        auto found_cr = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(found_cr);
        CHECK(found_cr == cr);

        auto found_entity = server->EntityMngr.GetEntity(cr_id);
        REQUIRE(found_entity);
        ptr<ServerEntity> cr_entity = cr;
        CHECK(found_entity == cr_entity);

        auto typed_found = server->EntityMngr.Get<Critter>(cr_id);
        REQUIRE(typed_found);
        CHECK(typed_found == cr);

        const EntityManager& const_entity_mngr = server->EntityMngr;
        auto const_typed_found = const_entity_mngr.Get<Critter>(cr_id);
        REQUIRE(const_typed_found);
        ptr<const Critter> const_cr = cr;
        CHECK(const_typed_found == const_cr);

        auto wrong_type = server->EntityMngr.Get<Item>(cr_id);
        CHECK_FALSE(static_cast<bool>(wrong_type));

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("GetEntityFindsCreatedItem")
    {
        auto item = server->ItemMngr.CreateItem(fn("TestItem"), nullptr);

        ident_t item_id = item->GetId();
        auto found = server->EntityMngr.GetItem(item_id);
        REQUIRE(found);
        CHECK(nptr<Item> {found} == item);

        server->ItemMngr.DestroyItem(item);
    }

    SECTION("GetEntityFindsCreatedLocation")
    {
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));

        ident_t loc_id = loc->GetId();
        auto found = server->EntityMngr.GetLocation(loc_id);
        REQUIRE(found);
        CHECK(nptr<Location> {found} == loc);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("LocationCount")
    {
        size_t initial_count = server->EntityMngr.GetLocationsCount();

        auto loc1 = server->MapMngr.CreateLocation(fn("TestLocation"));
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 1);

        auto loc2 = server->MapMngr.CreateLocation(fn("TestLocation"));
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 2);

        server->MapMngr.DestroyLocation(loc1);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 1);

        server->MapMngr.DestroyLocation(loc2);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count);
    }

    SECTION("PlayersCountIsZero")
    {
        CHECK(server->EntityMngr.GetPlayersCount() == 0);
    }

    SECTION("MapsCountIsZero")
    {
        CHECK(server->EntityMngr.GetMapsCount() == 0);
    }

    SECTION("MapTransfersLockedExportObservesNestedLocks")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        // Balances any lock still held if an assertion unwinds before the explicit unlock path completes
        int32_t held_locks = 0;
        auto balance_locks = scope_exit([&cr, &held_locks]() noexcept {
            while (held_locks > 0) {
                cr->UnlockMapTransfers();
                held_locks--;
            }
        });

        CHECK_FALSE(Server_Critter_MapTransfersLocked(cr));

        cr->LockMapTransfers();
        held_locks++;
        CHECK(Server_Critter_MapTransfersLocked(cr));

        cr->LockMapTransfers();
        held_locks++;

        // Repeated reads are snapshots: neither read releases nor resets the nesting counter
        CHECK(Server_Critter_MapTransfersLocked(cr));
        CHECK(Server_Critter_MapTransfersLocked(cr));

        cr->UnlockMapTransfers();
        held_locks--;
        CHECK(Server_Critter_MapTransfersLocked(cr));

        cr->UnlockMapTransfers();
        held_locks--;
        CHECK_FALSE(Server_Critter_MapTransfersLocked(cr));

        REQUIRE(held_locks == 0);
        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("CritterCppApi")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("CritterStateChecks")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK(cr->IsAlive());
        CHECK_FALSE(cr->IsDead());
        CHECK_FALSE(cr->IsKnockout());
        CHECK_FALSE(cr->IsMoving());
        CHECK_FALSE(cr->HasPlayer());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterIdentity")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK(cr->GetId() != ident_t {});
        CHECK(cr->GetProtoId() == fn("TestCritter"));
        CHECK_FALSE(cr->IsDestroyed());
        CHECK_FALSE(cr->IsDestroying());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterPlayerFlag")
    {
        auto npc = server->CreateCritter(fn("TestCritter"), false);

        CHECK_FALSE(npc->GetControlledByPlayer());

        server->CrMngr.DestroyCritter(npc);
    }

    SECTION("CritterInventoryOperations")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK_FALSE(cr->HasItems());

        auto item1 = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(fn("TestItem"), nullptr), true);
        CHECK(cr->HasItems());

        auto item2 = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(fn("TestItem"), nullptr), true);
        CHECK(item2->GetId() != item1->GetId());

        vector<ptr<Item>> inv_items = cr->GetInvItems();
        CHECK(inv_items.size() == 2);

        // Find by pid
        auto found = cr->GetItemByPidInvPriority(fn("TestItem"));
        CHECK(static_cast<bool>(found));

        // Find by id
        auto found_by_id = cr->GetInvItem(item1->GetId());
        CHECK(found_by_id == item1);

        // Destroy inventory
        server->CrMngr.DestroyInventory(cr);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("MultipleCritterCreation")
    {
        size_t count = 5;
        vector<ptr<Critter>> critters;

        for (size_t i = 0; i < count; i++) {
            auto cr = server->CreateCritter(fn("TestCritter"), false);
            critters.push_back(cr);
        }

        // All should have unique IDs
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                CHECK(critters[i]->GetId() != critters[j]->GetId());
            }
        }

        // Clean up
        for (ptr<Critter> cr : critters) {
            server->CrMngr.DestroyCritter(cr);
        }
    }

    SECTION("CritterDestroyState")
    {
        // Hold a ref so the critter object survives DestroyCritter (which drops the manager's last
        // reference and frees it) and the post-destroy IsDestroyed() check reads a valid object
        auto cr = server->CreateCritter(fn("TestCritter"), false).hold_ref();

        CHECK_FALSE(cr->IsDestroyed());
        server->CrMngr.DestroyCritter(cr.get());
        CHECK(cr->IsDestroyed());
    }
}

TEST_CASE("IndependentRootCoverEnumeration")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("GlobalMapGroupIdsReportEveryMemberWithAStableRevision")
    {
        auto leader = server->CreateCritter(fn("TestCritter"), false);
        auto member = server->CreateCritter(fn("TestCritter"), false);
        server->MapMngr.RemoveCritterFromMap(member, nullptr);
        server->MapMngr.AddCritterToMap(member, nullptr, {}, mdir {}, leader->GetId());
        REQUIRE(leader->GetGlobalMapGroup().size() == 2);

        uint64_t revision = 0;
        vector<ident_t> ids = leader->GetGlobalMapGroupIds(revision);
        CHECK(ids.size() == 2);
        CHECK(std::ranges::find(ids, leader->GetId()) != ids.end());
        CHECK(std::ranges::find(ids, member->GetId()) != ids.end());

        // One group object is shared by every member, so each of them reports the same membership and revision
        uint64_t member_revision = 0;
        CHECK(member->GetGlobalMapGroupIds(member_revision).size() == ids.size());
        CHECK(member_revision == revision);

        // Reading again without a membership change must not move the revision
        uint64_t repeated_revision = 0;
        CHECK(leader->GetGlobalMapGroupIds(repeated_revision).size() == 2);
        CHECK(repeated_revision == revision);

        // Leaving is a membership change: the reported ids shrink and the revision advances
        server->MapMngr.RemoveCritterFromMap(member, nullptr);

        uint64_t shrunk_revision = 0;
        CHECK(leader->GetGlobalMapGroupIds(shrunk_revision) == vector<ident_t> {leader->GetId()});
        CHECK(shrunk_revision > revision);

        // A critter with no global-map group has nothing to enumerate
        uint64_t detached_revision = 42;
        CHECK(member->GetGlobalMapGroupIds(detached_revision).empty());
        CHECK(detached_revision == 0);

        server->MapMngr.AddCritterToMap(member, nullptr, {}, mdir {}, {});
        server->CrMngr.DestroyCritter(member);
        server->CrMngr.DestroyCritter(leader);
    }

    SECTION("CoveringTheReportedGroupIdsLetsTheGroupFanOutSucceed")
    {
        auto leader = server->CreateCritter(fn("TestCritter"), false);
        auto member = server->CreateCritter(fn("TestCritter"), false);
        server->MapMngr.RemoveCritterFromMap(member, nullptr);
        server->MapMngr.AddCritterToMap(member, nullptr, {}, mdir {}, leader->GetId());

        auto ctx = server->RequireCurrentSyncContext();

        // Bootstrap: with nothing but the critter itself covered, the enumeration still reports the group
        small_vector<ptr<ServerEntity>, 1> leader_only_cover {leader};
        ctx->SyncEntities(leader_only_cover);

        uint64_t revision = 0;
        vector<ident_t> ids = leader->GetGlobalMapGroupIds(revision);
        REQUIRE(ids.size() == 2);
        REQUIRE(std::ranges::find(ids, member->GetId()) != ids.end());

        // The group fan-out validates every group member, which that minimal cover does not include
        CHECK_THROWS_WITH(leader->Send_AddCritter(member), Catch::Matchers::ContainsSubstring("Entity access without sync"));

        // Covering exactly what the enumeration reported makes the same fan-out legal
        small_vector<ptr<ServerEntity>, 2> group_cover {leader, member};
        ctx->SyncEntities(group_cover);
        CHECK_NOTHROW(leader->Send_AddCritter(member));

        uint64_t final_revision = 0;
        CHECK(leader->GetGlobalMapGroupIds(final_revision).size() == 2);
        CHECK(final_revision == revision);

        server->CrMngr.DestroyCritter(member);
        server->CrMngr.DestroyCritter(leader);
    }

    SECTION("InitialInfoLeavesTheGlobalGroupFanOutToTheScript")
    {
        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "GlobalGroupInitialInfo");

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        auto group_member = server->CreateCritter(fn("TestCritter"), false);
        server->MapMngr.RemoveCritterFromMap(group_member, nullptr);
        server->MapMngr.AddCritterToMap(group_member, nullptr, {}, mdir {}, cr->GetId());
        REQUIRE(cr->GetGlobalMapGroup().size() == 2);

        auto ctx = server->RequireCurrentSyncContext();

        // The critter to attach is chosen inside the login callback, so the travelling companion is an
        // independent root the caller could not pre-cover. Initial info must work with the attach pair alone
        small_vector<ptr<ServerEntity>, 2> attach_cover {player, cr};
        ctx->SyncEntities(attach_cover);

        test_connection->Dispatch();
        test_connection->ResetSentPacketCount();
        CHECK_NOTHROW(server->SwitchPlayerCritter(player, cr));
        test_connection->Dispatch();

        CHECK(player->GetControlledCritter() == cr.get());
        // Only the chosen critter itself: the companion is delivered later by Critter.SendGlobalMapGroupInfo
        CHECK(test_connection->GetSentMessageCount(NetMessage::AddCritter) == 1);

        server->SwitchPlayerCritter(player, nullptr);

        small_vector<ptr<ServerEntity>, 3> group_cover {player, cr, group_member};
        ctx->SyncEntities(group_cover);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(group_member);
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("SendGlobalMapGroupInfoRequiresTheCallerToCoverEveryGroupMember")
    {
        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "GlobalGroupFanOut");

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        auto group_member = server->CreateCritter(fn("TestCritter"), false);
        server->MapMngr.RemoveCritterFromMap(group_member, nullptr);
        server->MapMngr.AddCritterToMap(group_member, nullptr, {}, mdir {}, cr->GetId());

        auto ctx = server->RequireCurrentSyncContext();

        small_vector<ptr<ServerEntity>, 2> attach_cover {player, cr};
        ctx->SyncEntities(attach_cover);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        test_connection->Dispatch();
        test_connection->ResetSentPacketCount();

        // The export validates the group instead of acquiring it, so an incomplete cover sends nothing at all
        ctx->SyncEntities(attach_cover);
        CHECK_THROWS_WITH(Server_Critter_SendGlobalMapGroupInfo(cr), Catch::Matchers::ContainsSubstring("Entity access without sync"));
        test_connection->Dispatch();
        CHECK(test_connection->GetSentMessageCount(NetMessage::AddCritter) == 0);

        // Covering exactly what Critter.GetGlobalMapCritterIds reports delivers the travelling companion
        small_vector<ptr<ServerEntity>, 3> group_cover {player, cr, group_member};
        ctx->SyncEntities(group_cover);
        CHECK_NOTHROW(Server_Critter_SendGlobalMapGroupInfo(cr));
        test_connection->Dispatch();
        CHECK(test_connection->GetSentMessageCount(NetMessage::AddCritter) == 1);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(group_member);
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CoveringTheReportedSpectatorsLetsTheMapDestroySucceed")
    {
        auto ctx = server->RequireCurrentSyncContext();

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto spectator = MakeSpectatorPlayer(server);
        small_vector<ptr<ServerEntity>, 3> full_cover {loc, map, spectator.as_ptr()};
        ctx->SyncEntities(full_cover);
        spectator->SetViewMap(map, mpos {10, 10});

        vector<refcount_ptr<Player>> spectators = map->GetSpectatorPlayersForSend();
        REQUIRE(spectators.size() == 1);
        CHECK(spectators.front() == spectator);

        // The spectator is not a map descendant, so the map tree cover alone cannot eject it
        small_vector<ptr<ServerEntity>, 2> tree_only_cover {loc, map};
        ctx->SyncEntities(tree_only_cover);
        CHECK_THROWS_WITH(server->MapMngr.DestroyLocation(loc), Catch::Matchers::ContainsSubstring("Entity access without sync"));

        // Detach the spectator from the location that stayed half-destroyed by that throw-as-signal failure
        ctx->SyncEntities(full_cover);
        spectator->ResetViewMap();

        // The same destroy succeeds once the reported spectator is part of the cover
        auto covered_loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto covered_map = covered_loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(covered_map));

        small_vector<ptr<ServerEntity>, 3> covered_cover {covered_loc, covered_map, spectator.as_ptr()};
        ctx->SyncEntities(covered_cover);
        spectator->SetViewMap(covered_map, mpos {10, 10});
        REQUIRE(covered_map->GetSpectatorPlayersForSend().size() == 1);

        CHECK_NOTHROW(server->MapMngr.DestroyLocation(covered_loc));
        CHECK(covered_loc->IsDestroyed());
        CHECK_FALSE(static_cast<bool>(spectator->GetViewMapTarget()));
    }
}

TEST_CASE("ItemCppApi")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("ItemCreationAndDestruction")
    {
        // Hold a ref so the item survives DestroyItem (which drops the manager's last reference and
        // frees it) and the post-destroy IsDestroyed() check reads a valid object
        auto item = server->ItemMngr.CreateItem(fn("TestItem"), nullptr).hold_ref();

        CHECK(item->GetId() != ident_t {});
        CHECK(item->GetProtoId() == fn("TestItem"));
        CHECK_FALSE(item->IsDestroyed());

        server->ItemMngr.DestroyItem(item.get());
        CHECK(item->IsDestroyed());
    }

    SECTION("ItemAddToCritterAndRemove")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        auto item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(fn("TestItem"), nullptr), true);
        CHECK(cr->HasItems());

        server->ItemMngr.DestroyItem(item);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("MultipleItemTypes")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        auto item1 = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(fn("TestItem"), nullptr), true);
        auto item2 = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(fn("TestItem"), nullptr), true);

        // Different item instances
        CHECK(item1->GetId() != item2->GetId());

        vector<ptr<Item>> inv = cr->GetInvItems();
        CHECK(inv.size() >= 2);

        server->CrMngr.DestroyInventory(cr);
        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("LocationCppApi")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("CreateAndDestroyLocation")
    {
        // Hold a ref so the location survives DestroyLocation (which drops the manager's last
        // reference and frees it) and the post-destroy IsDestroyed() check reads a valid object
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation")).hold_ref();

        CHECK(loc->GetId() != ident_t {});
        CHECK(loc->GetProtoId() == fn("TestLocation"));
        CHECK_FALSE(loc->IsDestroyed());

        server->MapMngr.DestroyLocation(loc.get());
        CHECK(loc->IsDestroyed());
    }

    SECTION("MultipleLocations")
    {
        auto loc1 = server->MapMngr.CreateLocation(fn("TestLocation"));
        auto loc2 = server->MapMngr.CreateLocation(fn("TestLocation"));

        CHECK(loc1->GetId() != loc2->GetId());

        server->MapMngr.DestroyLocation(loc1);
        CHECK_FALSE(loc2->IsDestroyed());

        server->MapMngr.DestroyLocation(loc2);
    }

    SECTION("LocationMapsEmpty")
    {
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));

        vector<ptr<Map>> maps = loc->GetMaps();
        CHECK(maps.empty());

        server->MapMngr.DestroyLocation(loc);
    }

    // The find type is honoured as a whole: the player/NPC half picks the map partition, the dead/alive half still filters it
    SECTION("MapCrittersByFindTypeHonourWholeFindType")
    {
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto player_cr = server->CreateCritter(fn("TestCritter"), true);
        auto npc = server->CreateCritter(fn("TestCritter"), false);
        auto dead_npc = server->CreateCritter(fn("TestCritter"), false);
        server->MapMngr.TransferToMap(player_cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        server->MapMngr.TransferToMap(npc, map, mpos {24, 20}, mdir {}, std::nullopt);
        server->MapMngr.TransferToMap(dead_npc, map, mpos {28, 20}, mdir {}, std::nullopt);
        dead_npc->SetCondition(CritterCondition::Dead);

        auto contains = [](const vector<ptr<Critter>>& critters, ptr<Critter> cr) { return std::ranges::find(critters, cr) != critters.end(); };

        CHECK(contains(map->GetCritters(CritterFindType::Players), player_cr.get()));
        CHECK_FALSE(contains(map->GetCritters(CritterFindType::Players), npc.get()));
        CHECK(contains(map->GetCritters(CritterFindType::NonDeadNpc), npc.get()));
        CHECK_FALSE(contains(map->GetCritters(CritterFindType::NonDeadNpc), dead_npc.get()));
        CHECK_FALSE(contains(map->GetCritters(CritterFindType::NonDeadNpc), player_cr.get()));
        CHECK(contains(map->GetCritters(CritterFindType::DeadNpc), dead_npc.get()));
        CHECK_FALSE(contains(map->GetCritters(CritterFindType::DeadNpc), npc.get()));
        CHECK(map->GetCritters(CritterFindType::DeadPlayers).empty());
        CHECK(map->GetCritters(CritterFindType::Any).size() == map->GetCritters().size());
        CHECK(map->GetCritters(CritterFindType::NonDead).size() + 1 == map->GetCritters().size());
        CHECK(map->GetCritters(CritterFindType::Dead).size() == 1);
        CHECK(contains(map->GetCritters(CritterFindType::Dead), dead_npc.get()));

        CritterFindType players_and_npc = combine_enum(CritterFindType::Players, CritterFindType::Npc);
        size_t expected_players_and_npc = 0;

        for (ptr<Critter> cr : map->GetCritters()) {
            if (cr->CheckFind(players_and_npc)) {
                expected_players_and_npc++;
            }
        }

        CHECK(map->GetCritters(players_and_npc).size() == expected_players_and_npc);

        player_cr->UnmarkIsForPlayer();
        CHECK(contains(map->GetCritters(CritterFindType::NonDeadNpc), player_cr.get()));
        CHECK_FALSE(contains(map->GetCritters(CritterFindType::Players), player_cr.get()));

        server->CrMngr.DestroyCritter(dead_npc);
        server->CrMngr.DestroyCritter(player_cr);
        server->CrMngr.DestroyCritter(npc);
        server->MapMngr.DestroyLocation(loc);
    }
}

TEST_CASE("ServerHealthInfo")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    SECTION("HealthInfoNotEmpty")
    {
        string info = server->GetHealthInfo();
        CHECK_FALSE(info.empty());
    }

    SECTION("HealthInfoContainsUptime")
    {
        string info = server->GetHealthInfo();
        CHECK(info.find("Server uptime") != string::npos);
    }
}

TEST_CASE("CritterManagerCppApi")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("GetNonPlayerCritters")
    {
        auto cr1 = server->CreateCritter(fn("TestCritter"), false);
        auto cr2 = server->CreateCritter(fn("TestCritter"), false);

        auto npcs = server->CrMngr.GetNonPlayerCritters();
        CHECK(npcs.size() >= 2);

        bool found1 = false;
        bool found2 = false;
        for (auto& npc : npcs) {
            if (npc == cr1.get()) {
                found1 = true;
            }
            if (npc == cr2.get()) {
                found2 = true;
            }
        }
        CHECK(found1);
        CHECK(found2);

        server->CrMngr.DestroyCritter(cr1);
        server->CrMngr.DestroyCritter(cr2);
    }

    SECTION("DestroyInventory")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        for (int32_t i = 0; i < 3; i++) {
            (void)server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(fn("TestItem"), nullptr), true);
        }

        CHECK(cr->HasItems());

        server->CrMngr.DestroyInventory(cr);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("ProtoAccessCppApi")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("GetProtoCritter")
    {
        auto proto = server->GetProtoCritter(fn("TestCritter"));
        CHECK(static_cast<bool>(proto));
    }

    SECTION("GetProtoItem")
    {
        auto proto = server->GetProtoItem(fn("TestItem"));
        CHECK(static_cast<bool>(proto));
    }

    SECTION("GetProtoLocation")
    {
        auto proto = server->GetProtoLocation(fn("TestLocation"));
        CHECK(static_cast<bool>(proto));
    }

    SECTION("GetProtoCritterReturnsNullForMissing")
    {
        auto proto = server->GetProtoCritter(fn("NonExistentProto"));
        CHECK_FALSE(static_cast<bool>(proto));
    }

    SECTION("GetProtoItemReturnsNullForMissing")
    {
        auto proto = server->GetProtoItem(fn("NonExistentProto"));
        CHECK_FALSE(static_cast<bool>(proto));
    }

    SECTION("GetProtoLocationReturnsNullForMissing")
    {
        auto proto = server->GetProtoLocation(fn("NonExistentProto"));
        CHECK_FALSE(static_cast<bool>(proto));
    }
}

TEST_CASE("CommonCppApiTests")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    auto get_func = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("ProtoManagerLookups")
    {
        // Verify that protos loaded in our test setup
        CHECK(server->EntityMngr.GetCrittersCount() == 0);
        CHECK(server->EntityMngr.GetLocationsCount() == 0);
        CHECK(server->EntityMngr.GetItemsCount() == 0);
    }

    SECTION("EntityManagerMultipleEntities")
    {
        // Create multiple entities and verify counts
        auto cr1 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr2 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr3 = server->CreateCritter(get_func("TestCritter"), false);

        CHECK(server->EntityMngr.GetCrittersCount() >= 3);

        // Clean up
        server->CrMngr.DestroyCritter(cr3);
        server->CrMngr.DestroyCritter(cr2);
        server->CrMngr.DestroyCritter(cr1);
    }

    SECTION("ItemManagerContainerOps")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        auto item1 = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), true);
        auto item2 = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), true);

        const auto& inv = cr->GetInvItems();
        CHECK(inv.size() == 2);

        server->ItemMngr.DestroyItem(item2);
        server->ItemMngr.DestroyItem(item1);
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ServerHealthCheck")
    {
        string health = server->GetHealthInfo();
        CHECK(health.find("Server uptime") != string::npos);
    }
}

TEST_CASE("PlayerRegistrationNativeApi")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = EntityLifecycleTests::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool server_locked = true;
    auto unlock = scope_exit([&server, &server_locked]() noexcept {
        if (server_locked) {
            safe_call([&server] { server->Unlock(); });
        }
    });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("LoginPlayerToNewRecordAllocatesNonZeroId")
    {
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, "TestPlayer1");
        CHECK(player->GetId() != ident_t {});
    }

    SECTION("LoginPlayerToNewRecordRegistersPlayer")
    {
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, "Player1");
        auto registered_player = server->EntityMngr.GetPlayer(player->GetId());
        REQUIRE(registered_player);
        CHECK(registered_player == player);
    }

    SECTION("LoginPlayerToNewRecordProducesUniqueIds")
    {
        auto player1 = EntityLifecycleTests::CreateLoggedPlayer(server, "Player1");
        auto player2 = EntityLifecycleTests::CreateLoggedPlayer(server, "Player2");
        CHECK(player1->GetId() != player2->GetId());
    }

    SECTION("RegisterPlayerRejectsDuplicateIdBeforeMutatingCandidate")
    {
        auto registered_player = EntityLifecycleTests::CreateLoggedPlayer(server, "RegisteredPlayer").hold_ref();
        ident_t registered_id = registered_player->GetId();
        auto net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto connection = safe_alloc::make_unique<ServerConnection>(server->Settings, std::move(net_connection), BakerTests::MakeTestChannelIdentity());
        auto candidate = safe_alloc::make_refcounted<Player>(server, ident_t {}, std::move(connection));
        server->RequireCurrentSyncContext()->SyncEntity(candidate);

        CHECK_THROWS_WITH(server->EntityMngr.RegisterPlayer(candidate, registered_id), Catch::Matchers::ContainsSubstring("Player id is already registered"));

        CHECK(candidate->GetId() == ident_t {});
        CHECK(server->EntityMngr.GetPlayer(registered_id) == registered_player);
        CHECK(server->EntityMngr.GetEntity(registered_id) == registered_player);

        candidate->MarkAsDestroyed();
    }

    SECTION("LoginPlayerToExistentRecordKeepsHardDisconnectForCompletionFailure")
    {
        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto not_logged_in_player = CreatePreparedNotLoggedInPlayer(server, test_connection, "MissingRecord");

        CHECK_THROWS_WITH(server->LoginPlayerToExistentRecord(not_logged_in_player, ident_t {999999}), Catch::Matchers::ContainsSubstring("Player data not found"));

        test_connection->Dispatch();

        CHECK(test_connection->GetSentInfoMessageCount() == 0);
        CHECK(test_connection->GetSentMessageCount(NetMessage::Disconnect) == 0);
        CHECK(test_connection->IsDisconnected());
    }

    SECTION("DetachPlayerCritterResendsPreviousChosenAsOrdinaryCritter")
    {
        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "ChosenDetach");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        test_connection->Dispatch();
        test_connection->ResetSentPacketCount();
        server->SwitchPlayerCritter(player, nullptr);
        test_connection->Dispatch();

        CHECK_FALSE(static_cast<bool>(player->GetControlledCritter()));
        CHECK_FALSE(static_cast<bool>(cr->GetPlayer()));
        CHECK(test_connection->GetSentPacketCount() > 0);
        CHECK(test_connection->GetSentMessageCount(NetMessage::RemoveCritter) == 1);
        CHECK(test_connection->GetSentMessageCount(NetMessage::AddCritter) == 1);
        REQUIRE(test_connection->GetSentTrackedMessageCount() == 2);
        CHECK(test_connection->GetSentTrackedMessage(0) == NetMessage::RemoveCritter);
        CHECK(test_connection->GetSentTrackedMessage(1) == NetMessage::AddCritter);

        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("StopMoveFailedReconciliationSendsAuthoritativePosition")
    {
        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "StopMoveCorrection");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    // The wait loop unlocked/relocked the server and the later SyncEntities cover holds only
                    // the map subtree, so the caller re-establishes the location cover for the destroy
                    server->RequireCurrentSyncContext()->SyncEntity(loc);
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};
        mpos blocked_client_hex = server_hex;
        REQUIRE(GeometryHelper::MoveHexByDir(blocked_client_hex, hdir::NorthWest, map->GetSize()));

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());
        REQUIRE(cr->GetPlayer() == player);

        map->SetHexManualBlock(blocked_client_hex, true, true);

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr.get(), uint16_t {1}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        test_connection->ResetSentPacketCount();
        SendStopCritterMove(test_connection, server, map->GetId(), cr->GetId(), blocked_client_hex, ipos16 {}, mdir {});

        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 3> sync_entities {player, cr, map};
        ctx->SyncEntities(sync_entities);

        CHECK(cr->GetHex() == server_hex);
        CHECK(test_connection->GetSentPacketCount() > 0);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("StopMoveKeepsPassableHexWhenOffsetRoundsIntoBlockedNeighbour")
    {
        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "StopMoveBlockedOffset");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    server->RequireCurrentSyncContext()->SyncEntity(loc);
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};
        mpos blocked_neighbour = server_hex;
        REQUIRE(GeometryHelper::MoveHexByDir(blocked_neighbour, hdir::NorthWest, map->GetSize()));

        ipos32 full_offset = GeometryHelper::GetHexPos(blocked_neighbour) - GeometryHelper::GetHexPos(server_hex);
        ipos16 client_offset {numeric_cast<int16_t>(full_offset.x), numeric_cast<int16_t>(full_offset.y)};
        ipos16 expected_offset {numeric_cast<int16_t>(std::clamp(client_offset.x, numeric_cast<int16_t>(-GameSettings::MAP_HEX_WIDTH / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_WIDTH / 2))), numeric_cast<int16_t>(std::clamp(client_offset.y, numeric_cast<int16_t>(-GameSettings::MAP_HEX_HEIGHT / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_HEIGHT / 2)))};
        REQUIRE(expected_offset != ipos16 {});

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());
        REQUIRE(cr->GetPlayer() == player);

        map->SetHexManualBlock(blocked_neighbour, true, true);

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {1}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        test_connection->ResetSentPacketCount();
        SendStopCritterMove(test_connection, server, map->GetId(), cr->GetId(), server_hex, client_offset, mdir {});

        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 3> sync_entities {player, cr, map};
        ctx->SyncEntities(sync_entities);

        CHECK(cr->GetHex() == server_hex);
        CHECK(cr->GetHexOffset() == expected_offset);
        CHECK(test_connection->GetSentPacketCount() == 0);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("StopMoveNormalizesOffsetIntoPassableNeighbour")
    {
        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "StopMovePassableOffset");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    server->RequireCurrentSyncContext()->SyncEntity(loc);
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};
        mpos passable_neighbour = server_hex;
        REQUIRE(GeometryHelper::MoveHexByDir(passable_neighbour, hdir::East, map->GetSize()));

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());
        REQUIRE(cr->GetPlayer() == player);
        REQUIRE(map->IsHexMovable(passable_neighbour));

        ipos32 full_offset = GeometryHelper::GetHexPos(passable_neighbour) - GeometryHelper::GetHexPos(server_hex);
        ipos16 client_offset {numeric_cast<int16_t>(full_offset.x), numeric_cast<int16_t>(full_offset.y)};

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {1}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        test_connection->ResetSentPacketCount();
        SendStopCritterMove(test_connection, server, map->GetId(), cr->GetId(), server_hex, client_offset, mdir {});

        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 3> sync_entities {player, cr, map};
        ctx->SyncEntities(sync_entities);

        CHECK(cr->GetHex() == passable_neighbour);
        CHECK(cr->GetHexOffset() == ipos16 {});
        CHECK(test_connection->GetSentPacketCount() == 0);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("PlayerPreLoginRunsBeforeEveryRegistrationRoute", "[player][admission]")
{
    int32_t route = GENERATE(0, 1, 2, 3);
    int32_t refusal = GENERATE(0, 1, 2, 3, 4);
    CAPTURE(route, refusal);

    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(EntityLifecycleTests::WaitForStart(server).empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&]() noexcept {
        if (locked) {
            safe_call([&server] { server->Unlock(); });
        }
    });

    refcount_nptr<Player> existing;
    ident_t existing_id {999999};
    nptr<ServerConnection> existing_connection;

    if (route == 2) {
        existing = EntityLifecycleTests::CreateLoggedPlayer(server, "ExistingSession").hold_ref();
        existing_id = existing->GetId();
        existing_connection = existing->GetConnection();
    }

    auto candidate = CreatePreparedNotLoggedInPlayer(server, "RejectedSession").hold_ref();
    auto connection = candidate->GetConnection();

    if (route == 1) {
        auto stored = PropertiesSerializer::SaveToDocument(candidate->GetProperties(), nullptr, server->Hashes, *server);
        server->DbStorage.Insert(server->Hashes.to_hashed_string("Players"), existing_id, stored);
    }

    auto before_ids = server->DbStorage.GetAllIntIds(server->Hashes.to_hashed_string("Players"));
    size_t before_count = server->EntityMngr.GetPlayersCount();
    size_t gate_calls = 0;
    size_t login_calls = 0;

    Entity::EventCallbackData gate;
    gate.SubscriptionPtr = reinterpret_cast<uintptr_t>(&gate_calls);
    gate.Callback = [&](FuncCallData&) {
        gate_calls++;
        CHECK(candidate->GetId() == ident_t {});
        CHECK_FALSE(candidate->GetLoggedIn());

        if (refusal == 1) {
            connection->GracefulDisconnect();
        }
        else if (refusal == 2) {
            connection->HardDisconnect(DisconnectReason::ScriptRequest);
        }
        else if (refusal == 3) {
            connection->HardDisconnect(DisconnectReason::ScriptRequest);
            candidate->MarkAsDestroyed();
        }
        else if (refusal == 4) {
            candidate->MarkAsDestroying();
        }

        return refusal == 0 ? Entity::EventResult::StopChain : Entity::EventResult::ContinueChain;
    };
    server->OnPlayerPreLogin.Subscribe(std::move(gate));

    Entity::EventCallbackData logged_in;
    logged_in.SubscriptionPtr = reinterpret_cast<uintptr_t>(&login_calls);
    logged_in.Callback = [&](FuncCallData&) {
        login_calls++;
        return Entity::EventResult::ContinueChain;
    };
    server->OnPlayerLogin.Subscribe(std::move(logged_in));
    auto unsubscribe = scope_exit([&]() noexcept {
        server->OnPlayerPreLogin.Unsubscribe(reinterpret_cast<uintptr_t>(&gate_calls));
        server->OnPlayerLogin.Unsubscribe(reinterpret_cast<uintptr_t>(&login_calls));
    });

    auto login = [&]() {
        if (route == 0) {
            return server->LoginPlayerToNewRecord(candidate);
        }
        if (route == 3) {
            return server->LoginPlayerToTempSession(candidate);
        }

        return server->LoginPlayerToExistentRecord(candidate, existing_id);
    };

    auto has_connection_count = [&](size_t count) { return server->GetHealthInfo().find(strex("Connections: {}\n", count).str()) != string::npos; };

    if (refusal >= 3 && route == 0) {
        // Observe the occupied slot before destruction, so a stale initial zero cannot satisfy cleanup.
        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, locked, [&]() { return has_connection_count(before_count + 1); }, std::chrono::milliseconds {3000}));
        server->RequireCurrentSyncContext()->SyncEntity(candidate);
    }

    CHECK_THROWS_AS(login(), BaseEngineException);
    CHECK(gate_calls == 1);
    CHECK(login_calls == 0);
    CHECK(candidate->GetId() == ident_t {});
    CHECK(server->EntityMngr.GetPlayersCount() == before_count);
    CHECK(server->DbStorage.GetAllIntIds(server->Hashes.to_hashed_string("Players")) == before_ids);

    if (refusal >= 3 && route == 0) {
        CHECK(EntityLifecycleTests::WaitForUnlockedServerCondition(server, locked, [&]() { return has_connection_count(before_count); }, std::chrono::milliseconds {3000}));
    }

    if (existing) {
        CHECK(server->EntityMngr.GetPlayer(existing_id) == existing);
        CHECK(existing->GetConnection() == existing_connection);
        CHECK_FALSE(existing_connection->IsHardDisconnected());
        CHECK_FALSE(existing_connection->IsGracefulDisconnected());
    }
}

TEST_CASE("PlayerPreLoginAllowsReconnectWithoutDuplicatingTheCritter", "[player][admission]")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(EntityLifecycleTests::WaitForStart(server).empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto original = EntityLifecycleTests::CreateLoggedPlayer(server, "ReconnectSession").hold_ref();
    auto cr = server->CreateCritter(server->Hashes.to_hashed_string("TestCritter"), true);
    server->SwitchPlayerCritter(original, cr);
    auto incoming = CreatePreparedNotLoggedInPlayer(server, "ReconnectSession").hold_ref();
    auto incoming_connection = incoming->GetConnection();
    size_t gate_calls = 0;
    Entity::EventCallbackData gate;
    gate.SubscriptionPtr = reinterpret_cast<uintptr_t>(&gate_calls);
    gate.Callback = [&](FuncCallData&) {
        gate_calls++;
        CHECK(original->GetControlledCritter() == cr);
        CHECK_FALSE(incoming->GetLoggedIn());
        return Entity::EventResult::ContinueChain;
    };
    server->OnPlayerPreLogin.Subscribe(std::move(gate));
    auto unsubscribe = scope_exit([&]() noexcept { server->OnPlayerPreLogin.Unsubscribe(reinterpret_cast<uintptr_t>(&gate_calls)); });

    auto reconnected = server->LoginPlayerToExistentRecord(incoming, original->GetId());
    CHECK(gate_calls == 1);
    CHECK(reconnected == original);
    CHECK(reconnected->GetConnection() == incoming_connection);
    CHECK(reconnected->GetControlledCritter() == cr);
    CHECK(server->EntityMngr.GetPlayersCount() == 1);
    CHECK(server->EntityMngr.GetCrittersCount() == 1);
    CHECK(server->EntityMngr.GetCritter(cr->GetId()) == cr);
    CHECK(incoming->IsDestroyed());
}

TEST_CASE("PlayerPreProcessChecksEachQueuedMessageBeforeReadingIt", "[player][admission]")
{
    int32_t refusal = GENERATE(0, 1, 2, 3);
    CAPTURE(refusal);

    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(EntityLifecycleTests::WaitForStart(server).empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked = true;
    auto unlock = scope_exit([&]() noexcept {
        if (locked) {
            safe_call([&server] { server->Unlock(); });
        }
    });

    auto transport = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
    auto player = EntityLifecycleTests::CreateLoggedPlayer(server, transport, "QueuedInput").hold_ref();
    auto connection = player->GetConnection();
    size_t gate_calls = 0;
    size_t refused_unread = 0;
    Entity::EventCallbackData gate;
    gate.SubscriptionPtr = reinterpret_cast<uintptr_t>(&gate_calls);
    gate.Callback = [&](FuncCallData&) {
        gate_calls++;

        if (gate_calls != 2) {
            return Entity::EventResult::ContinueChain;
        }

        refused_unread = connection->ReadBuf()->GetBufferedUnreadSize();

        if (refusal == 1) {
            connection->GracefulDisconnect();
        }
        else if (refusal == 2) {
            connection->HardDisconnect(DisconnectReason::ScriptRequest);
        }
        else if (refusal == 3) {
            connection->HardDisconnect(DisconnectReason::ScriptRequest);
            player->MarkAsDestroyed();
            server->EntityMngr.UnregisterPlayer(player);
        }

        return refusal == 0 ? Entity::EventResult::StopChain : Entity::EventResult::ContinueChain;
    };
    server->OnPlayerPreProcess.Subscribe(std::move(gate));
    auto unsubscribe = scope_exit([&]() noexcept { server->OnPlayerPreProcess.Unsubscribe(reinterpret_cast<uintptr_t>(&gate_calls)); });

    server->Unlock();
    locked = false;
    coarse_sleep(std::chrono::milliseconds {25});
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    locked = true;
    server->RequireCurrentSyncContext()->SyncEntity(player);
    CHECK(gate_calls == 0);

    NetOutBuffer messages {256};

    for (int32_t i = 0; i < 3; i++) {
        messages.StartMsg(NetMessage::Ping);
        messages.Write<bool>(true);
        messages.Write<int64_t>(0);
        messages.EndMsg();
    }

    size_t one_message_size = messages.GetData().size() / 3;
    transport->Receive(messages.GetData());
    REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, locked, [&]() { return gate_calls >= 2; }));
    CHECK(gate_calls == 2);

    if (!player->IsDestroyed()) {
        server->RequireCurrentSyncContext()->SyncEntity(player);
    }

    CHECK(refused_unread == one_message_size * 2);
}

TEST_CASE("CritterRestoresOnItsStoredMap", "[player][persistence]")
{
    int32_t scenario = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
    bool for_player = GENERATE(false, true);
    CAPTURE(scenario, for_player);

    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(EntityLifecycleTests::WaitForStart(server).empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    auto hash = [&](string_view value) { return server->Hashes.to_hashed_string(value); };
    auto stored_map_id = [&](ident_t critter_id) {
        auto record = Server_Game_DbGetRecord(server, hash("Critters"), critter_id);
        auto map_id_field = record.find("MapId");
        return map_id_field != record.end() ? map_id_field->second : string {};
    };

    auto loc = server->MapMngr.CreateLocation(hash("TestLocation"), vector<hstring> {hash("TestMap")});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(map);
    auto cr = server->CreateCritter(hash("TestCritter"), for_player);
    cr->SetLookDistance(10);
    auto item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(hash("TestItem"), nullptr), true);
    ident_t cr_id = cr->GetId();
    ident_t item_id = item->GetId();
    ident_t map_id = map->GetId();
    mpos saved_hex {20, 20};
    mdir saved_dir {hdir::East};
    server->MapMngr.TransferToMap(cr, map, saved_hex, saved_dir, std::nullopt);
    server->EntityMngr.MakePersistent(loc, true, true);
    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();
    CHECK(stored_map_id(cr_id) == strex("{}", map_id).str());
    server->UnloadCritter(cr);
    server->DbStorage.WaitCommitChanges();
    REQUIRE_FALSE(server->EntityMngr.GetCritter(cr_id));
    CHECK(stored_map_id(cr_id) == strex("{}", map_id).str());

    if (scenario == 1) {
        auto other_loc = server->MapMngr.CreateLocation(hash("TestLocation"), vector<hstring> {hash("TestMap")});
        map = other_loc->GetMapByIndex(0);
        REQUIRE(map);
    }
    else if (scenario == 2) {
        auto props = server->GetProtoCritter(hash("TestCritter"))->GetProperties()->Copy();
        CritterProperties(props).SetHex(mpos {200, 200});
        auto doc = PropertiesSerializer::SaveToDocument(&props, nullptr, server->Hashes, *server);
        server->DbStorage.Update(hash("Critters"), cr_id, "Hex", doc["Hex"]);
    }

    refcount_nptr<Critter> observer;
    refcount_nptr<Map> destination;

    if (scenario >= 9) {
        auto other_loc = server->MapMngr.CreateLocation(hash("TestLocation"), vector<hstring> {hash("TestMap")});
        destination = other_loc->GetMapByIndex(0).try_hold_ref();
        REQUIRE(destination);
    }

    if (scenario == 0 || scenario == 8) {
        observer = server->CreateCritter(hash("TestCritter"), false).hold_ref();
        server->MapMngr.TransferToMap(observer, map, mpos {21, 20}, mdir {}, std::nullopt);
    }

    vector<string> order;
    refcount_nptr<Critter> visibility_owner;
    Entity::EventCallbackData preload;
    preload.SubscriptionPtr = reinterpret_cast<uintptr_t>(&order);
    preload.Callback = [&](FuncCallData&) {
        order.emplace_back("pre");
        auto restored = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(restored);
        CHECK(restored->GetControlledByPlayer() == for_player);
        CHECK(restored->GetMapId() == ident_t {});
        CHECK(restored->GetInvItems().size() == 1);

        if (scenario == 4) {
            if (for_player) {
                restored->UnmarkIsForPlayer();
            }

            server->CrMngr.DestroyCritter(restored);
        }

        return scenario == 3 ? Entity::EventResult::StopChain : Entity::EventResult::ContinueChain;
    };
    server->OnCritterPreLoad.Subscribe(std::move(preload));

    Entity::EventCallbackData entered;
    entered.SubscriptionPtr = reinterpret_cast<uintptr_t>(&order);
    entered.Callback = [&](FuncCallData&) {
        auto restored = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(restored);

        if (restored->GetMapId() != map_id) {
            return Entity::EventResult::ContinueChain;
        }

        order.emplace_back("in");
        CHECK(restored->GetMapId() == map_id);
        CHECK(map->GetCritter(cr_id) == restored);
        CHECK(restored->IsExplicitlyPersistent());
        CHECK_FALSE(restored->IsInitCalled());

        if (scenario == 7) {
            if (for_player) {
                restored->UnmarkIsForPlayer();
            }

            server->CrMngr.DestroyCritter(restored);
        }
        else if (scenario == 9) {
            server->MapMngr.TransferToMap(restored, destination, mpos {30, 30}, mdir {}, std::nullopt);
        }

        return Entity::EventResult::ContinueChain;
    };
    server->OnMapCritterIn.Subscribe(std::move(entered));

    Entity::EventCallbackData init;
    init.SubscriptionPtr = reinterpret_cast<uintptr_t>(&order);
    init.Callback = [&](FuncCallData&) {
        order.emplace_back("init");
        refcount_nptr<Critter> restored = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(restored);
        nptr<Map> expected_map = map;

        if (scenario == 9) {
            expected_map = destination;
        }

        CHECK(restored->GetMapId() == expected_map->GetId());
        CHECK(expected_map->GetCritter(cr_id) == restored);

        if (observer) {
            CHECK(restored->GetLookDistance() == 10);
            visibility_owner = restored;
            Entity::EventCallbackData appeared;
            appeared.SubscriptionPtr = reinterpret_cast<uintptr_t>(&order);
            appeared.Callback = [&](FuncCallData&) {
                order.emplace_back("visible");
                CHECK(visibility_owner->IsInitCalled());

                if (scenario == 8) {
                    if (for_player) {
                        visibility_owner->UnmarkIsForPlayer();
                    }

                    server->CrMngr.DestroyCritter(visibility_owner);
                }

                return Entity::EventResult::ContinueChain;
            };
            restored->OnCritterAppeared.Subscribe(std::move(appeared));
        }

        if (scenario == 5) {
            if (for_player) {
                restored->UnmarkIsForPlayer();
            }

            server->CrMngr.DestroyCritter(restored);
        }
        else if (scenario == 10) {
            server->MapMngr.TransferToMap(restored, destination, mpos {30, 30}, mdir {}, std::nullopt);
        }

        return Entity::EventResult::ContinueChain;
    };
    server->OnCritterInit.Subscribe(std::move(init));

    Entity::EventCallbackData load;
    load.SubscriptionPtr = reinterpret_cast<uintptr_t>(&order);
    load.Callback = [&](FuncCallData&) {
        order.emplace_back("load");
        auto restored = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(restored);
        nptr<Map> expected_map = map;

        if (scenario == 9 || scenario == 10) {
            expected_map = destination;
        }

        CHECK(restored->GetMapId() == expected_map->GetId());
        CHECK(expected_map->GetCritter(cr_id) == restored);

        if (scenario == 6) {
            if (for_player) {
                restored->UnmarkIsForPlayer();
            }

            server->CrMngr.DestroyCritter(restored);
        }
        else if (scenario == 11) {
            server->MapMngr.TransferToMap(restored, destination, mpos {30, 30}, mdir {}, std::nullopt);
        }

        return Entity::EventResult::ContinueChain;
    };
    server->OnCritterLoad.Subscribe(std::move(load));
    auto unsubscribe = scope_exit([&]() noexcept {
        if (visibility_owner && !visibility_owner->IsDestroyed()) {
            visibility_owner->OnCritterAppeared.Unsubscribe(reinterpret_cast<uintptr_t>(&order));
        }

        server->OnMapCritterIn.Unsubscribe(reinterpret_cast<uintptr_t>(&order));
        server->OnCritterPreLoad.Unsubscribe(reinterpret_cast<uintptr_t>(&order));
        server->OnCritterInit.Unsubscribe(reinterpret_cast<uintptr_t>(&order));
        server->OnCritterLoad.Unsubscribe(reinterpret_cast<uintptr_t>(&order));
    });

    if (scenario == 0) {
        auto restored = server->LoadCritter(cr_id, for_player, map);
        CHECK(restored->GetId() == cr_id);
        CHECK(restored->GetMapId() == map_id);
        CHECK(restored->GetHex() == saved_hex);
        CHECK(restored->GetDir() == saved_dir);
        CHECK(restored->GetControlledByPlayer() == for_player);
        CHECK_FALSE(restored->GetRawGlobalMapGroup());
        CHECK(restored->GetInvItems().size() == 1);
        CHECK(server->EntityMngr.GetItem(item_id));

        // Map::AddCritter buckets by the controlled flag observed at add time
        span<ptr<Critter>> player_critters = map->GetPlayerCritters();
        span<ptr<Critter>> npc_critters = map->GetNonPlayerCritters();
        CHECK((std::ranges::find(player_critters, restored) != player_critters.end()) == for_player);
        CHECK((std::ranges::find(npc_critters, restored) != npc_critters.end()) == !for_player);

        // MapManager::AddCritterToMap persists the id only for non-player critters and keeps it unique
        CHECK(std::ranges::count(map->GetCritterIds(), cr_id) == (for_player ? 0 : 1));

        CHECK_THROWS_AS(server->LoadCritter(cr_id, for_player, map), GenericException);
        CHECK(map->GetCritters().size() == 2);
    }
    else if (scenario >= 9) {
        CHECK_THROWS_WITH(server->LoadCritter(cr_id, for_player, map), Catch::Matchers::ContainsSubstring("Critter load event moved the restored critter"));
        auto restored = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(restored);
        CHECK(restored->GetMapId() == destination->GetId());
        CHECK(restored->GetHex() == mpos {30, 30});
        CHECK(destination->GetCritter(cr_id) == restored);
        CHECK_FALSE(map->GetCritter(cr_id));
        CHECK(restored->GetInvItems().size() == 1);
        CHECK(server->EntityMngr.GetItem(item_id));
        CHECK(server->DbStorage.Valid(hash("Critters"), cr_id));
        CHECK(stored_map_id(cr_id) == strex("{}", destination->GetId()).str());
    }
    else {
        CHECK_THROWS_AS(server->LoadCritter(cr_id, for_player, map), GenericException);
        CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
        CHECK_FALSE(server->EntityMngr.GetItem(item_id));
        CHECK_FALSE(map->GetCritter(cr_id));

        if (scenario <= 3) {
            CHECK(server->DbStorage.Valid(hash("Critters"), cr_id));
            CHECK(server->DbStorage.Valid(hash("Items"), item_id));
            CHECK(stored_map_id(cr_id) == strex("{}", map_id).str());
        }
        else {
            CHECK_FALSE(server->DbStorage.Valid(hash("Critters"), cr_id));
        }
    }

    vector<vector<string>> expected_orders {
        {"pre", "in", "init", "visible", "load"},
        {"pre"},
        {"pre"},
        {"pre"},
        {"pre"},
        {"pre", "in", "init"},
        {"pre", "in", "init", "load"},
        {"pre", "in"},
        {"pre", "in", "init", "visible"},
        {"pre", "in", "init", "load"},
        {"pre", "in", "init", "load"},
        {"pre", "in", "init", "load"},
    };
    CHECK(order == expected_orders[scenario]);
}

TEST_CASE("LoadedCritterVisibilityStopsAfterItemCallbackDestroysIt", "[player][persistence]")
{
    auto settings = EntityLifecycleTests::MakeSettings();
    auto server = EntityLifecycleTests::MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(EntityLifecycleTests::WaitForStart(server).empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    auto hash = [&](string_view value) { return server->Hashes.to_hashed_string(value); };

    auto loc = server->MapMngr.CreateLocation(hash("TestLocation"), vector<hstring> {hash("TestMap")});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(map);
    auto cr = server->CreateCritter(hash("TestCritter"), false);
    cr->SetLookDistance(10);
    server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
    server->EntityMngr.MakePersistent(loc, true, true);
    server->EntityMngr.MakePersistent(cr, true, true);
    auto first_item = server->ItemMngr.CreateItemOnHex(map, mpos {21, 20}, hash("TestItem"), nullptr);
    auto second_item = server->ItemMngr.CreateItemOnHex(map, mpos {22, 20}, hash("TestItem"), nullptr);
    REQUIRE(map->GetItems().size() == 2);
    REQUIRE(cr->CanSeeItemOnMap(first_item));
    REQUIRE(cr->CanSeeItemOnMap(second_item));
    ident_t cr_id = cr->GetId();
    server->UnloadCritter(cr);

    size_t visible_items = 0;
    refcount_nptr<Critter> restored;
    Entity::EventCallbackData init;
    init.SubscriptionPtr = reinterpret_cast<uintptr_t>(&visible_items);
    init.Callback = [&](FuncCallData&) {
        restored = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(restored);
        Entity::EventCallbackData appeared;
        appeared.SubscriptionPtr = reinterpret_cast<uintptr_t>(&visible_items);
        appeared.Callback = [&](FuncCallData&) {
            visible_items++;
            server->CrMngr.DestroyCritter(restored);
            return Entity::EventResult::ContinueChain;
        };
        restored->OnItemOnMapAppeared.Subscribe(std::move(appeared));
        return Entity::EventResult::ContinueChain;
    };
    server->OnCritterInit.Subscribe(std::move(init));
    auto unsubscribe = scope_exit([&]() noexcept {
        server->OnCritterInit.Unsubscribe(reinterpret_cast<uintptr_t>(&visible_items));

        if (restored && !restored->IsDestroyed()) {
            restored->OnItemOnMapAppeared.Unsubscribe(reinterpret_cast<uintptr_t>(&visible_items));
        }
    });

    CHECK_THROWS_WITH(server->LoadCritter(cr_id, false, map), Catch::Matchers::ContainsSubstring("Critter destroyed during loading"));
    CHECK(visible_items == 1);
    REQUIRE(restored);
    CHECK(restored->IsDestroyed());
    CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
    CHECK(map->GetItems().size() == 2);
}

FO_END_NAMESPACE
