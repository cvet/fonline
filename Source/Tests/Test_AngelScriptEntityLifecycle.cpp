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

#if FO_ANGELSCRIPT_SCRIPTING

#include "Test_EntityLifecycleHelpers.h"

FO_BEGIN_NAMESPACE

using namespace EntityLifecycleTests;

TEST_CASE("AngelScriptEntityInitEvents")
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

    SECTION("CritterInitEventFires")
    {
        // Reset counters
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        // Create critter
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        // Check critter init event fired
        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterInitCalls"), calls));
        CHECK(calls >= 1);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ItemInitEventFires")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto item = server->ItemMngr.CreateItem(fn("TestItem"), nullptr);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), calls));
        CHECK(calls >= 1);

        server->ItemMngr.DestroyItem(item);
    }

    SECTION("LocationInitEventFires")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetLocationInitCalls"), calls));
        CHECK(calls >= 1);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("ItemInitEventMayDestroyItem")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnInitMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        size_t initial_item_count = server->EntityMngr.GetItemsCount();
        REQUIRE_THROWS_AS(server->ItemMngr.CreateItem(fn("TestItem"), nullptr), ItemManagerException);
        CHECK(server->EntityMngr.GetItemsCount() == initial_item_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), calls));
        CHECK(calls >= 1);
    }

    SECTION("CritterInitEventMayDestroyCritter")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnInitMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(2));

        size_t initial_critter_count = server->EntityMngr.GetCrittersCount();
        REQUIRE_THROWS_AS(server->CreateCritter(fn("TestCritter"), false), GenericException);
        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterInitCalls"), calls));
        CHECK(calls >= 1);
    }

    SECTION("LocationInitEventMayDestroyLocation")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnInitMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(3));

        size_t initial_location_count = server->EntityMngr.GetLocationsCount();
        REQUIRE_THROWS_AS(server->MapMngr.CreateLocation(fn("TestLocation")), GenericException);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_location_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetLocationInitCalls"), calls));
        CHECK(calls >= 1);
    }

    SECTION("ItemFinishEventCannotTakeOverItemDestruction")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnFinishMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        size_t initial_item_count = server->EntityMngr.GetItemsCount();
        auto item = server->ItemMngr.CreateItem(fn("TestItem"), nullptr);
        ident_t item_id = item->GetId();

        server->ItemMngr.DestroyItem(item);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(item_id)));
        CHECK(server->EntityMngr.GetItemsCount() == initial_item_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemFinishCalls"), calls));
        CHECK(calls == 1);
    }

    SECTION("CritterFinishEventCannotTakeOverCritterDestruction")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnFinishMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(2));

        size_t initial_critter_count = server->EntityMngr.GetCrittersCount();
        auto cr = server->CreateCritter(fn("TestCritter"), false);
        ident_t cr_id = cr->GetId();

        server->CrMngr.DestroyCritter(cr);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterFinishCalls"), calls));
        CHECK(calls == 1);
    }

    SECTION("LocationFinishEventCannotTakeOverLocationDestruction")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnFinishMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(3));

        size_t initial_location_count = server->EntityMngr.GetLocationsCount();
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));
        ident_t loc_id = loc->GetId();

        server->MapMngr.DestroyLocation(loc);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetLocation(loc_id)));
        CHECK(server->EntityMngr.GetLocationsCount() == initial_location_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetLocationFinishCalls"), calls));
        CHECK(calls == 1);
    }

    SECTION("CritterUnloadEventMayTransferCritterBeforeTeardown")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});

        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        REQUIRE(cr->GetMapId() == map->GetId());
        REQUIRE(map->GetCritter(cr->GetId()) == cr.get());

        ident_t cr_id = cr->GetId();

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetCritterLifecycleEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        server->UnloadCritter(cr);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
        CHECK_FALSE(static_cast<bool>(map->GetCritter(cr_id)));

        int32_t unload_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterUnloadCalls"), unload_calls));
        CHECK(unload_calls == 1);

        int32_t saw_on_map_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterUnloadSawOnMapCalls"), saw_on_map_calls));
        CHECK(saw_on_map_calls == 1);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("CritterLoadEventMayDestroyLoadedCritterThrows")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->EntityMngr.MakePersistent(cr, true, true);
        ident_t cr_id = cr->GetId();

        server->UnloadCritter(cr);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetCritterLifecycleEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(2));

        REQUIRE_THROWS_AS(server->LoadCritter(cr_id, true), GenericException);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        int32_t load_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterLoadCalls"), load_calls));
        CHECK(load_calls == 1);
    }

    SECTION("CritterPreLoadRunsBeforeWorldEntryInitializationAndLoad")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->EntityMngr.MakePersistent(cr, true, true);
        ident_t cr_id = cr->GetId();

        server->UnloadCritter(cr);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetCritterLifecycleEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(3));

        auto loaded_cr = server->LoadCritter(cr_id, true);

        int32_t pre_load_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterPreLoadCalls"), pre_load_calls));
        CHECK(pre_load_calls == 1);

        int32_t load_order = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterLoadOrder"), load_order));
        CHECK(load_order == 4);

        bool transfer_blocked = false;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterPreLoadTransferBlocked"), transfer_blocked));
        CHECK(transfer_blocked);

        bool saw_controllable = false;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterPreLoadSawControllable"), saw_controllable));
        CHECK(saw_controllable);

        server->UnloadCritter(loaded_cr);
        server->DestroyUnloadedCritter(cr_id);
    }

    SECTION("CritterPreLoadMayDropPersistedCritterWithoutLoadError")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->EntityMngr.MakePersistent(cr, true, true);
        server->DbStorage.WaitCommitChanges();
        ident_t cr_id = cr->GetId();

        server->UnloadCritter(cr);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetCritterLifecycleEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(4));

        bool is_error = false;
        auto dropped_cr = server->EntityMngr.LoadCritter(cr_id, true, nullptr, is_error);

        CHECK_FALSE(is_error);
        CHECK_FALSE(static_cast<bool>(dropped_cr));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        server->DbStorage.WaitCommitChanges();
        auto persisted_cr_ids = server->DbStorage.GetAllIntIds(fn("Critters"));
        CHECK(std::ranges::find(persisted_cr_ids, cr_id) == persisted_cr_ids.end());
    }

    SECTION("CritterPreLoadExceptionFailsLoadWithoutDeletingPersistedRecord")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->EntityMngr.MakePersistent(cr, true, true);
        server->DbStorage.WaitCommitChanges();
        ident_t cr_id = cr->GetId();

        server->UnloadCritter(cr);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetCritterLifecycleEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(5));

        REQUIRE_THROWS_AS(server->LoadCritter(cr_id, true), GenericException);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        int32_t init_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterInitCalls"), init_calls));
        CHECK(init_calls == 0);

        server->DbStorage.WaitCommitChanges();
        auto persisted_cr_ids = server->DbStorage.GetAllIntIds(fn("Critters"));
        CHECK(std::ranges::find(persisted_cr_ids, cr_id) != persisted_cr_ids.end());

        server->DestroyUnloadedCritter(cr_id);
    }
}

TEST_CASE("AngelScriptPlayerRegistrationCppApi")
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

    SECTION("LoginPlayerToNewRecordThrowsWhenPlayerLoginStopsChain")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(5));

        auto not_logged_in_player = CreatePreparedNotLoggedInPlayer(server, "RejectNewRecord");
        CHECK_THROWS_WITH(server->LoginPlayerToNewRecord(not_logged_in_player), Catch::Matchers::ContainsSubstring("New player login rejected by OnPlayerLogin"));
    }

    SECTION("LoginPlayerToExistentRecordThrowsWhenPlayerLoginStopsChain")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, "RejectReconnect");

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(5));

        auto not_logged_in_player = CreatePreparedNotLoggedInPlayer(server, "RejectReconnectNext");
        small_vector<ptr<ServerEntity>, 2> reconnect_cover {player, not_logged_in_player};
        server->RequireCurrentSyncContext()->SyncEntities(reconnect_cover);
        CHECK_THROWS_WITH(server->LoginPlayerToExistentRecord(not_logged_in_player, player->GetId()), Catch::Matchers::ContainsSubstring("Player reconnect rejected by OnPlayerLogin"));
    }

    SECTION("LoginPlayerToExistentRecordReportsFailureWhenPlayerLoginThrows")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, "FailingReconnect");

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(6));

        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto not_logged_in_player = CreatePreparedNotLoggedInPlayer(server, test_connection, "FailingReconnectNext");
        small_vector<ptr<ServerEntity>, 2> reconnect_cover {player, not_logged_in_player};
        server->RequireCurrentSyncContext()->SyncEntities(reconnect_cover);

        CHECK_THROWS_WITH(server->LoginPlayerToExistentRecord(not_logged_in_player, player->GetId()), Catch::Matchers::ContainsSubstring("Player reconnect rejected by OnPlayerLogin"));

        test_connection->Dispatch();
        CHECK(test_connection->GetSentInfoMessageCount() == 1);
        CHECK(test_connection->GetLastSentInfoMessage() == EngineInfoMessage::NetLoginScriptFail);
        CHECK(test_connection->GetSentMessageCount(NetMessage::Disconnect) == 1);
        CHECK_FALSE(test_connection->IsDisconnected());
    }

    SECTION("LoginPlayerToExistentRecordUsesCallerProvidedLocalMapCover")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, "ReconnectLocalMap");
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

        REQUIRE(reset_func.Call());

        auto reconnect_not_logged_in = CreatePreparedNotLoggedInPlayer(server, "ReconnectLocalMapNext");
        small_vector<ptr<ServerEntity>, 5> reconnect_cover {player, reconnect_not_logged_in, cr, map, loc};
        server->RequireCurrentSyncContext()->SyncEntities(reconnect_cover);
        auto reconnected_player = server->LoginPlayerToExistentRecord(reconnect_not_logged_in, player->GetId());

        CHECK(reconnected_player == player);
        CHECK(reconnect_not_logged_in->IsDestroyed());
        CHECK(player->GetControlledCritter() == cr.get());
        CHECK(cr->GetMapId() == map->GetId());

        int32_t initial_info_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterSendInitialInfoCalls"), initial_info_calls));
        CHECK(initial_info_calls == 1);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("LoginPlayerToExistentRecordUsesCallerProvidedGlobalGroupCover")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, "ReconnectGlobalGroup");
        auto cr = server->CreateCritter(fn("TestCritter"), true);
        auto group_member = server->CreateCritter(fn("TestCritter"), false);
        server->MapMngr.RemoveCritterFromMap(group_member, nullptr);
        server->MapMngr.AddCritterToMap(group_member, nullptr, {}, mdir {}, cr->GetId());
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());
        REQUIRE(cr->GetGlobalMapGroup().size() == 2);

        REQUIRE(reset_func.Call());

        auto reconnect_not_logged_in = CreatePreparedNotLoggedInPlayer(server, "ReconnectGlobalGroupNext");
        small_vector<ptr<ServerEntity>, 4> reconnect_cover {player, reconnect_not_logged_in, cr, group_member};
        server->RequireCurrentSyncContext()->SyncEntities(reconnect_cover);
        auto reconnected_player = server->LoginPlayerToExistentRecord(reconnect_not_logged_in, player->GetId());

        CHECK(reconnected_player == player);
        CHECK(reconnect_not_logged_in->IsDestroyed());
        CHECK(player->GetControlledCritter() == cr.get());

        int32_t initial_info_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterSendInitialInfoCalls"), initial_info_calls));
        CHECK(initial_info_calls == 1);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(group_member);
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("LoginPlayerToTempSessionThrowsWhenPlayerLoginStopsChain")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(5));

        auto not_logged_in_player = CreatePreparedNotLoggedInPlayer(server, "RejectTempSession");
        CHECK_THROWS_WITH(server->LoginPlayerToTempSession(not_logged_in_player), Catch::Matchers::ContainsSubstring("Temporary player login rejected by OnPlayerLogin"));
    }

    SECTION("PlayerLoginHardDisconnectDefersDestructionToPlayerJob")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        size_t initial_player_count = server->EntityMngr.GetPlayersCount();
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto not_logged_in_player = server->CreateNotLoggedInPlayer(std::move(net_connection));
        not_logged_in_player->SetName("LoginDisconnect");
        not_logged_in_player->SetLastControlledCritterId(ident_t {1});

        (void)server->LoginPlayerToNewRecord(not_logged_in_player);

        int32_t login_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLoginCalls"), login_calls));
        CHECK(login_calls == 1);

        int32_t destroyed_after_disconnect_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerDestroyedAfterLoginDisconnectCalls"), destroyed_after_disconnect_calls));
        CHECK(destroyed_after_disconnect_calls == 0);

        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, server_locked, [&server, initial_player_count] { return server->EntityMngr.GetPlayersCount() == initial_player_count; }, std::chrono::milliseconds {2000}));

        CHECK(server->EntityMngr.GetPlayersCount() == initial_player_count);

        int32_t logout_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLogoutCalls"), logout_calls));
        CHECK(logout_calls == 1);
    }

    SECTION("TempSessionPlayerHardDisconnectDefersDestructionToPlayerJob")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        size_t initial_player_count = server->EntityMngr.GetPlayersCount();
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto not_logged_in_player = server->CreateNotLoggedInPlayer(std::move(net_connection));
        not_logged_in_player->SetName("TempSessionDisconnect");

        auto player = server->LoginPlayerToTempSession(not_logged_in_player);

        int32_t login_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLoginCalls"), login_calls));
        CHECK(login_calls == 1);

        player->GetConnection()->HardDisconnect(DisconnectReason::ScriptRequest);

        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, server_locked, [&server, initial_player_count] { return server->EntityMngr.GetPlayersCount() == initial_player_count; }, std::chrono::milliseconds {2000}));

        CHECK(server->EntityMngr.GetPlayersCount() == initial_player_count);

        int32_t logout_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLogoutCalls"), logout_calls));
        CHECK(logout_calls == 1);
    }

    SECTION("CritterInitialInfoMayDetachPlayerBeforeSwitchNotification")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, "InitialInfoDetach");

        auto prev_cr = server->CreateCritter(fn("TestCritter"), true);
        auto next_cr = server->CreateCritter(fn("TestCritter"), true);

        server->SwitchPlayerCritter(player, prev_cr);
        CHECK(player->GetControlledCritter() == prev_cr.get());

        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(3));

        server->SwitchPlayerCritter(player, next_cr);

        CHECK_FALSE(static_cast<bool>(player->GetControlledCritter()));
        CHECK_FALSE(static_cast<bool>(next_cr->GetPlayer()));
        CHECK_FALSE(static_cast<bool>(prev_cr->GetPlayer()));

        int32_t initial_info_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterSendInitialInfoCalls"), initial_info_calls));
        CHECK(initial_info_calls == 1);

        int32_t switched_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerCritterSwitchedCalls"), switched_calls));
        CHECK(switched_calls == 0);

        next_cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(next_cr);

        prev_cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(prev_cr);
    }

    SECTION("StopMoveDirEventMayDetachPlayerBeforeStoppingMovement")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "StopMoveDetach");

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

        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());
        REQUIRE(cr->GetPlayer() == player);

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {1}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());
        uint32_t moving_uid = cr->GetMovingUid();

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(4));

        SendStopCritterMove(test_connection, server, map->GetId(), cr->GetId(), cr->GetHex(), ipos16 {}, mdir {});

        int32_t dir_calls = 0;
        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, server_locked, [&server, &fn, &dir_calls] { return server->CallFunc(fn("EntityLifecycle::GetPlayerDirCritterCalls"), dir_calls) && dir_calls == 1; }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 3> sync_entities {player, cr, map};
        ctx->SyncEntities(sync_entities);

        CHECK_FALSE(static_cast<bool>(player->GetControlledCritter()));
        CHECK_FALSE(static_cast<bool>(cr->GetPlayer()));
        CHECK(cr->GetMapId() == map->GetId());
        CHECK(map->GetCritter(cr->GetId()) == cr.get());
        CHECK(cr->IsMoving());
        CHECK(cr->GetMovingUid() == moving_uid);

        REQUIRE(set_mode_func.Call(0));
        server->StopCritterMoving(cr.get());
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("StopMoveRejectedDirectionCorrectsOwnerForActiveAndFinishedPlans")
    {
        bool after_end = GENERATE(false, true);
        CAPTURE(after_end);

        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto test_connection = safe_alloc::make_shared<TestNetworkConnection>(server->Settings);
        auto player = EntityLifecycleTests::CreateLoggedPlayer(server, test_connection, "StopMoveDirCorrection");
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
        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {1}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        if (after_end) {
            server->StopCritterMoving(cr, MovingState::Success);
            REQUIRE_FALSE(cr->IsMoving());
            REQUIRE(cr->GetFinishedPlayerMoving());
        }

        mpos server_hex = cr->GetHex();
        ipos16 server_offset = cr->GetHexOffset();
        mdir server_dir = cr->GetDir();
        mdir client_dir = server_dir.reverse();
        REQUIRE(client_dir != server_dir);

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(7));
        test_connection->Dispatch();
        test_connection->ResetSentPacketCount();
        SendStopCritterMove(test_connection, server, map->GetId(), cr->GetId(), server_hex, server_offset, client_dir);

        int32_t dir_calls = 0;
        REQUIRE(EntityLifecycleTests::WaitForUnlockedServerCondition(server, server_locked, [&server, &fn, &dir_calls] { return server->CallFunc(fn("EntityLifecycle::GetPlayerDirCritterCalls"), dir_calls) && dir_calls == 1; }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 3> sync_entities {player, cr, map};
        ctx->SyncEntities(sync_entities);
        test_connection->Dispatch();

        CHECK(cr->GetDir() == server_dir);
        CHECK(cr->GetHex() == server_hex);
        CHECK(cr->GetHexOffset() == server_offset);
        CHECK_FALSE(cr->IsMoving());
        REQUIRE(test_connection->GetSentMessageCount(NetMessage::CritterDir) == 1);
        CHECK(test_connection->GetLastSentCritterDirId() == cr->GetId());
        CHECK(test_connection->GetLastSentCritterDir() == server_dir);

        REQUIRE(set_mode_func.Call(0));
        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }

    // Counterpart of the blocked case above: the guard must decline only a blocked rounding, so a rounding
    // into a passable neighbour still has to advance the logical hex instead of piling up as a sub-hex offset
}

TEST_CASE("AngelScriptScriptFunctionCalls")
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

    SECTION("CallFuncWithReturnValue")
    {
        int32_t result = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), result));
        // Result is valid (we just check the API works)
    }

    SECTION("FindFuncReturnsFalseForMissing")
    {
        auto func = server->FindFunc<int32_t>(fn("NonExistent::Function"));
        CHECK_FALSE(func);
    }

    SECTION("FindFuncWorksForExisting")
    {
        auto func = server->FindFunc<int32_t>(fn("EntityLifecycle::GetItemInitCalls"));
        CHECK(func);
        if (func) {
            REQUIRE(func.Call());
        }
    }

    SECTION("FindFuncVoid")
    {
        auto func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(func);
        REQUIRE(func.Call());
    }
}

FO_END_NAMESPACE

#endif
