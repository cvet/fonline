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
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ServerItemsScripts",
            {
                {"Scripts/ServerItemsTest.fos", R"(
namespace ServerItemsTest
{
    int ItemInitCalls = 0;
    int64 LastItemId = 0;

    [[ModuleInit]]
    void InitServerItemsTest()
    {
        Game.OnInit.Subscribe(OnInit);
        Game.OnItemInit.Subscribe(OnItemInit);
    }

    [[Event]]
    bool OnInit()
    {
        return true;
    }

    [[Event]]
    void OnItemInit(Item item, bool firstTime)
    {
        ItemInitCalls++;
        LastItemId = item.Id.value;
    }

    int GetItemInitCalls()
    {
        return ItemInitCalls;
    }

    int64 GetLastItemId()
    {
        return LastItemId;
    }

    int64 GetCritterItemCount(Critter cr)
    {
        array<Item@> items = cr.GetItems();
        return items.length();
    }

    bool TestCritterIsAlive(Critter cr)
    {
        return cr.IsAlive();
    }

    bool TestCritterIsDead(Critter cr)
    {
        return cr.IsDead();
    }

    string GetCritterName(Critter cr)
    {
        return cr.Name;
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

    static auto MakeResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerItemsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");
        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        const auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerItemsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ServerItemsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("ServerItemsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("ServerItemsTest.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForStart(ServerEngine* server) -> string
    {
        FO_RUNTIME_ASSERT(server);

        for (int32 i = 0; i < 6000; i++) {
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
}

TEST_CASE("ServerItemCreationAndDestruction")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    const auto item_pid = fn("TestItem");
    REQUIRE(server->GetProtoItem(item_pid) != nullptr);

    const auto initial_item_count = server->EntityMngr.GetItemsCount();
    const auto initial_entity_count = server->EntityMngr.GetEntitiesCount();

    // Create item
    auto* item = server->ItemMngr.CreateItem(item_pid, 1, nullptr);
    REQUIRE(item != nullptr);

    const auto item_id = item->GetId();
    CHECK(item->GetProtoId() == item_pid);
    CHECK(server->EntityMngr.GetItem(item_id) == item);
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count + 1);
    CHECK(server->EntityMngr.GetEntitiesCount() > initial_entity_count);

    // OnItemInit should have been called
    int init_calls = 0;
    REQUIRE(server->CallFunc(fn("ServerItemsTest::GetItemInitCalls"), init_calls));
    CHECK(init_calls >= 1);

    int64 last_item_id = 0;
    REQUIRE(server->CallFunc(fn("ServerItemsTest::GetLastItemId"), last_item_id));
    CHECK(last_item_id == item_id.underlying_value());

    // Create second item
    auto* item2 = server->ItemMngr.CreateItem(item_pid, 1, nullptr);
    REQUIRE(item2 != nullptr);
    CHECK(item2->GetId() != item_id);
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count + 2);

    // Destroy items
    const auto item2_id = item2->GetId();
    server->ItemMngr.DestroyItem(item2);
    CHECK(server->EntityMngr.GetItem(item2_id) == nullptr);
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count + 1);

    server->ItemMngr.DestroyItem(item);
    CHECK(server->EntityMngr.GetItem(item_id) == nullptr);
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count);
}

TEST_CASE("ServerItemAddedToCritterInventory")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    const auto critter_pid = fn("TestCritter");
    const auto item_pid = fn("TestItem");

    // Create critter
    auto* cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);
    CHECK_FALSE(cr->HasItems());

    // Add item to critter inventory via pid
    auto* item = server->ItemMngr.AddItemCritter(cr, item_pid, 1);
    REQUIRE(item != nullptr);
    CHECK(cr->HasItems());

    auto inv_items = cr->GetInvItems();
    CHECK_FALSE(inv_items.empty());

    const auto item_id = item->GetId();
    CHECK(cr->GetInvItem(item_id) == item);

    // Add second item
    auto* item2 = server->ItemMngr.AddItemCritter(cr, item_pid, 1);
    REQUIRE(item2 != nullptr);

    auto inv_items2 = cr->GetInvItems();
    CHECK(inv_items2.size() >= 2);

    // Check via script
    auto cr_item_count_func = server->FindFunc<int64, Critter*>(fn("ServerItemsTest::GetCritterItemCount"));
    REQUIRE(cr_item_count_func);
    REQUIRE(cr_item_count_func.Call(cr));
    CHECK(cr_item_count_func.GetResult() >= 2);

    // SubItemCritter removes by pid/count
    server->ItemMngr.SubItemCritter(cr, item_pid, 1);

    // SetItemCritter sets exact count
    server->ItemMngr.SetItemCritter(cr, item_pid, 3);

    // Destroy inventory
    server->CrMngr.DestroyInventory(cr);
    CHECK_FALSE(cr->HasItems());
    CHECK(cr->GetInvItems().empty());

    // Cleanup critter
    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("ServerCritterLifecycleOperations")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    const auto critter_pid = fn("TestCritter");

    // Create multiple critters
    const auto cr_count_before = server->EntityMngr.GetCrittersCount();

    auto* cr1 = server->CreateCritter(critter_pid, false);
    auto* cr2 = server->CreateCritter(critter_pid, false);
    auto* cr3 = server->CreateCritter(critter_pid, true);
    REQUIRE(cr1 != nullptr);
    REQUIRE(cr2 != nullptr);
    REQUIRE(cr3 != nullptr);

    CHECK(server->EntityMngr.GetCrittersCount() == cr_count_before + 3);

    // All IDs should be unique
    CHECK(cr1->GetId() != cr2->GetId());
    CHECK(cr2->GetId() != cr3->GetId());
    CHECK(cr1->GetId() != cr3->GetId());

    // Check player status
    CHECK_FALSE(cr1->GetControlledByPlayer());
    CHECK_FALSE(cr2->GetControlledByPlayer());
    CHECK(cr3->GetControlledByPlayer());

    // Entity lookup
    CHECK(server->EntityMngr.GetEntity(cr1->GetId()) != nullptr);
    CHECK(server->EntityMngr.GetCritter(cr1->GetId()) == cr1);
    CHECK(server->EntityMngr.GetCritter(cr2->GetId()) == cr2);
    CHECK(server->EntityMngr.GetCritter(cr3->GetId()) == cr3);

    // Script alive/dead check
    auto is_alive_func = server->FindFunc<bool, Critter*>(fn("ServerItemsTest::TestCritterIsAlive"));
    REQUIRE(is_alive_func);
    REQUIRE(is_alive_func.Call(cr1));
    CHECK(is_alive_func.GetResult() == true);

    // MakePlayerId idempotent
    const auto pid1 = server->MakePlayerId("TestPlayer1");
    const auto pid2 = server->MakePlayerId("TestPlayer1");
    const auto pid3 = server->MakePlayerId("TestPlayer2");
    CHECK(pid1 == pid2);
    CHECK(pid1 != pid3);

    // Unload player critter
    const auto cr3_id = cr3->GetId();
    server->UnloadCritter(cr3);
    CHECK(server->EntityMngr.GetCritter(cr3_id) == nullptr);

    // Health info contains something
    const auto health = server->GetHealthInfo();
    CHECK_FALSE(health.empty());

    // Destroy remaining critters
    server->CrMngr.DestroyCritter(cr2);
    server->CrMngr.DestroyCritter(cr1);

    CHECK(server->EntityMngr.GetCrittersCount() == cr_count_before);
}

TEST_CASE("ServerEntityManagerQueries")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    const auto critter_pid = fn("TestCritter");
    const auto item_pid = fn("TestItem");

    // Initial entity counts
    CHECK(server->EntityMngr.GetLocationsCount() == 0);
    CHECK(server->EntityMngr.GetMapsCount() == 0);
    CHECK(server->EntityMngr.GetPlayersCount() == 0);

    // Entities collection
    const auto& entities = server->EntityMngr.GetEntities();
    const auto entities_before = entities.size();

    // Items collection access
    const auto& items = server->EntityMngr.GetItems();
    const auto items_before = items.size();

    auto* item = server->ItemMngr.CreateItem(item_pid, 1, nullptr);
    REQUIRE(item != nullptr);
    CHECK(server->EntityMngr.GetItems().size() == items_before + 1);

    // Critters collection access
    const auto& critters = server->EntityMngr.GetCritters();
    const auto cr_before = critters.size();

    auto* cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);
    CHECK(server->EntityMngr.GetCritters().size() == cr_before + 1);

    // Locations collection (should be empty in this test)
    const auto& locations = server->EntityMngr.GetLocations();
    CHECK(locations.empty());

    // Maps collection (should be empty)
    const auto& maps = server->EntityMngr.GetMaps();
    CHECK(maps.empty());

    // Players collection (no players connected)
    const auto& players = server->EntityMngr.GetPlayers();
    CHECK(players.empty());

    // Entities grew after creating item and critter
    CHECK(server->EntityMngr.GetEntities().size() >= entities_before + 2);

    // Non-existent entity returns null
    CHECK(server->EntityMngr.GetEntity(ident_t {999999}) == nullptr);
    CHECK(server->EntityMngr.GetCritter(ident_t {999999}) == nullptr);
    CHECK(server->EntityMngr.GetItem(ident_t {999999}) == nullptr);
    CHECK(server->EntityMngr.GetLocation(ident_t {999999}) == nullptr);
    CHECK(server->EntityMngr.GetMap(ident_t {999999}) == nullptr);
    CHECK(server->EntityMngr.GetPlayer(ident_t {999999}) == nullptr);

    // Cleanup
    server->ItemMngr.DestroyItem(item);
    server->CrMngr.DestroyCritter(cr);
}

FO_END_NAMESPACE
