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

        return BakerTests::CompileInlineScripts(&compiler_engine, "EntityLifecycleScripts",
            {
                {"Scripts/EntityLifecycle.fos", R"(
namespace EntityLifecycle
{
    int ItemInitCalls = 0;
    int CritterInitCalls = 0;
    int LocationInitCalls = 0;

    [[ModuleInit]]
    void InitEntityLifecycle()
    {
        Game.OnInit.Subscribe(OnInit);
        Game.OnItemInit.Subscribe(OnItemInit);
        Game.OnCritterInit.Subscribe(OnCritterInit);
        Game.OnLocationInit.Subscribe(OnLocationInit);
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
    }

    [[Event]]
    void OnCritterInit(Critter cr, bool firstTime)
    {
        CritterInitCalls++;
    }

    [[Event]]
    void OnLocationInit(Location loc, bool firstTime)
    {
        LocationInitCalls++;
    }

    int GetItemInitCalls() { return ItemInitCalls; }
    int GetCritterInitCalls() { return CritterInitCalls; }
    int GetLocationInitCalls() { return LocationInitCalls; }

    void ResetCounters()
    {
        ItemInitCalls = 0;
        CritterInitCalls = 0;
        LocationInitCalls = 0;
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

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("EntityLifecycleCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");
        const auto location_type = proto_engine.Hashes.ToHashedString("Location");
        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        const auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("EntityLifecycleRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("EntityLifecycleCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("EntityLifecycleItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("EntityLifecycleLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("EntityLifecycle.fos-bin-server", script_blob);

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

// ========== Entity Init Event Tests ==========

TEST_CASE("EntityInitEvents")
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

    SECTION("CritterInitEventFires")
    {
        // Reset counters
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        // Create critter
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        // Check critter init event fired
        int32 calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterInitCalls"), calls));
        CHECK(calls >= 1);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ItemInitEventFires")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto* item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);
        REQUIRE(item != nullptr);

        int32 calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), calls));
        CHECK(calls >= 1);

        server->ItemMngr.DestroyItem(item);
    }

    SECTION("LocationInitEventFires")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto* loc = server->MapMngr.CreateLocation(fn("TestLocation"));
        REQUIRE(loc != nullptr);

        int32 calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetLocationInitCalls"), calls));
        CHECK(calls >= 1);

        server->MapMngr.DestroyLocation(loc);
    }
}

// ========== Entity Manager C++ API Tests ==========

TEST_CASE("EntityManagerCppApi")
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

    SECTION("GetEntitiesReturnsCorrectCollections")
    {
        auto initial_critter_count = server->EntityMngr.GetCrittersCount();

        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count + 1);

        auto after_critter_item_count = server->EntityMngr.GetItemsCount();

        auto* item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);
        REQUIRE(item != nullptr);

        CHECK(server->EntityMngr.GetItemsCount() == after_critter_item_count + 1);

        server->ItemMngr.DestroyItem(item);
        CHECK(server->EntityMngr.GetItemsCount() == after_critter_item_count);

        server->CrMngr.DestroyCritter(cr);
        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count);
    }

    SECTION("GetEntityReturnsNullForInvalidId")
    {
        CHECK(server->EntityMngr.GetEntity(ident_t {999999}) == nullptr);
        CHECK(server->EntityMngr.GetCritter(ident_t {999999}) == nullptr);
        CHECK(server->EntityMngr.GetItem(ident_t {999999}) == nullptr);
        CHECK(server->EntityMngr.GetLocation(ident_t {999999}) == nullptr);
        CHECK(server->EntityMngr.GetMap(ident_t {999999}) == nullptr);
        CHECK(server->EntityMngr.GetPlayer(ident_t {999999}) == nullptr);
    }

    SECTION("GetEntityFindsCreatedCritter")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        const auto cr_id = cr->GetId();
        auto* found = server->EntityMngr.GetCritter(cr_id);
        CHECK(found == cr);

        auto* found_entity = server->EntityMngr.GetEntity(cr_id);
        CHECK(found_entity != nullptr);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("GetEntityFindsCreatedItem")
    {
        auto* item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);
        REQUIRE(item != nullptr);

        const auto item_id = item->GetId();
        auto* found = server->EntityMngr.GetItem(item_id);
        CHECK(found == item);

        server->ItemMngr.DestroyItem(item);
    }

    SECTION("GetEntityFindsCreatedLocation")
    {
        auto* loc = server->MapMngr.CreateLocation(fn("TestLocation"));
        REQUIRE(loc != nullptr);

        const auto loc_id = loc->GetId();
        auto* found = server->EntityMngr.GetLocation(loc_id);
        CHECK(found == loc);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("LocationCount")
    {
        auto initial_count = server->EntityMngr.GetLocationsCount();

        auto* loc1 = server->MapMngr.CreateLocation(fn("TestLocation"));
        REQUIRE(loc1 != nullptr);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 1);

        auto* loc2 = server->MapMngr.CreateLocation(fn("TestLocation"));
        REQUIRE(loc2 != nullptr);
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
}

// ========== Critter C++ API Tests ==========

TEST_CASE("CritterCppApi")
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

    SECTION("CritterStateChecks")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        CHECK(cr->IsAlive());
        CHECK_FALSE(cr->IsDead());
        CHECK_FALSE(cr->IsKnockout());
        CHECK_FALSE(cr->IsMoving());
        CHECK_FALSE(cr->HasPlayer());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterIdentity")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        CHECK(cr->GetId() != ident_t {});
        CHECK(cr->GetProtoId() == fn("TestCritter"));
        CHECK_FALSE(cr->IsDestroyed());
        CHECK_FALSE(cr->IsDestroying());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterPlayerFlag")
    {
        auto* npc = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(npc != nullptr);

        CHECK_FALSE(npc->GetControlledByPlayer());

        server->CrMngr.DestroyCritter(npc);
    }

    SECTION("CritterInventoryOperations")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        CHECK_FALSE(cr->HasItems());

        auto* item1 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        REQUIRE(item1 != nullptr);
        CHECK(cr->HasItems());

        auto* item2 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        REQUIRE(item2 != nullptr);

        auto inv_items = cr->GetInvItems();
        CHECK(inv_items.size() >= 2);

        // Find by pid
        auto* found = cr->GetInvItemByPid(fn("TestItem"));
        CHECK(found != nullptr);

        // Count by pid
        CHECK(cr->CountInvItemByPid(fn("TestItem")) >= 2);

        // Find by id
        auto* found_by_id = cr->GetInvItem(item1->GetId());
        CHECK(found_by_id == item1);

        // Destroy inventory
        server->CrMngr.DestroyInventory(cr);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("MultipleCritterCreation")
    {
        const size_t count = 5;
        vector<Critter*> critters;

        for (size_t i = 0; i < count; i++) {
            auto* cr = server->CreateCritter(fn("TestCritter"), false);
            REQUIRE(cr != nullptr);
            critters.push_back(cr);
        }

        // All should have unique IDs
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                CHECK(critters[i]->GetId() != critters[j]->GetId());
            }
        }

        // Clean up
        for (auto* cr : critters) {
            server->CrMngr.DestroyCritter(cr);
        }
    }

    SECTION("CritterDestroyState")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        CHECK_FALSE(cr->IsDestroyed());
        server->CrMngr.DestroyCritter(cr);
        CHECK(cr->IsDestroyed());
    }
}

// ========== Item C++ API Tests ==========

TEST_CASE("ItemCppApi")
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

    SECTION("ItemCreationAndDestruction")
    {
        auto* item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);
        REQUIRE(item != nullptr);

        CHECK(item->GetId() != ident_t {});
        CHECK(item->GetProtoId() == fn("TestItem"));
        CHECK_FALSE(item->IsDestroyed());

        server->ItemMngr.DestroyItem(item);
        CHECK(item->IsDestroyed());
    }

    SECTION("ItemAddToCritterAndRemove")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        auto* item = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 5);
        REQUIRE(item != nullptr);
        CHECK(cr->HasItems());

        server->ItemMngr.SubItemCritter(cr, fn("TestItem"), 3);
        // Still has 2 items
        CHECK(cr->HasItems());

        server->ItemMngr.SubItemCritter(cr, fn("TestItem"), 2);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ItemSetCount")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        server->ItemMngr.SetItemCritter(cr, fn("TestItem"), 10);
        CHECK(cr->CountInvItemByPid(fn("TestItem")) == 10);

        server->ItemMngr.SetItemCritter(cr, fn("TestItem"), 3);
        CHECK(cr->CountInvItemByPid(fn("TestItem")) == 3);

        server->ItemMngr.SetItemCritter(cr, fn("TestItem"), 0);
        CHECK(cr->CountInvItemByPid(fn("TestItem")) == 0);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("MultipleItemTypes")
    {
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        auto* item1 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        auto* item2 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);

        REQUIRE(item1 != nullptr);
        REQUIRE(item2 != nullptr);

        // Different item instances
        CHECK(item1->GetId() != item2->GetId());

        auto inv = cr->GetInvItems();
        CHECK(inv.size() >= 2);

        server->CrMngr.DestroyInventory(cr);
        server->CrMngr.DestroyCritter(cr);
    }
}

// ========== Location C++ API Tests ==========

TEST_CASE("LocationCppApi")
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

    SECTION("CreateAndDestroyLocation")
    {
        auto* loc = server->MapMngr.CreateLocation(fn("TestLocation"));
        REQUIRE(loc != nullptr);

        CHECK(loc->GetId() != ident_t {});
        CHECK(loc->GetProtoId() == fn("TestLocation"));
        CHECK_FALSE(loc->IsDestroyed());

        server->MapMngr.DestroyLocation(loc);
        CHECK(loc->IsDestroyed());
    }

    SECTION("MultipleLocations")
    {
        auto* loc1 = server->MapMngr.CreateLocation(fn("TestLocation"));
        auto* loc2 = server->MapMngr.CreateLocation(fn("TestLocation"));
        REQUIRE(loc1 != nullptr);
        REQUIRE(loc2 != nullptr);

        CHECK(loc1->GetId() != loc2->GetId());

        server->MapMngr.DestroyLocation(loc1);
        CHECK_FALSE(loc2->IsDestroyed());

        server->MapMngr.DestroyLocation(loc2);
    }

    SECTION("LocationMapsEmpty")
    {
        auto* loc = server->MapMngr.CreateLocation(fn("TestLocation"));
        REQUIRE(loc != nullptr);

        auto maps = loc->GetMaps();
        CHECK(maps.empty());

        server->MapMngr.DestroyLocation(loc);
    }
}

// ========== Health Info C++ API Tests ==========

TEST_CASE("ServerHealthInfo")
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

    SECTION("HealthInfoNotEmpty")
    {
        const auto info = server->GetHealthInfo();
        CHECK_FALSE(info.empty());
    }

    SECTION("HealthInfoContainsUptime")
    {
        const auto info = server->GetHealthInfo();
        CHECK(info.find("Server uptime") != string::npos);
    }
}

// ========== Player ID C++ API Tests ==========

TEST_CASE("PlayerIdCppApi")
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

    SECTION("MakePlayerIdDeterministic")
    {
        const auto id1 = server->MakePlayerId("TestPlayer1");
        const auto id2 = server->MakePlayerId("TestPlayer1");
        CHECK(id1 == id2);
    }

    SECTION("MakePlayerIdDifferentForDifferentNames")
    {
        const auto id1 = server->MakePlayerId("Player1");
        const auto id2 = server->MakePlayerId("Player2");
        CHECK(id1 != id2);
    }

    SECTION("MakePlayerIdNonZero")
    {
        const auto id = server->MakePlayerId("SomePlayer");
        CHECK(id != ident_t {});
    }
}

// ========== NPC Manager C++ API Tests ==========

TEST_CASE("CritterManagerCppApi")
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

    SECTION("GetNonPlayerCritters")
    {
        auto* cr1 = server->CreateCritter(fn("TestCritter"), false);
        auto* cr2 = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr1 != nullptr);
        REQUIRE(cr2 != nullptr);

        auto npcs = server->CrMngr.GetNonPlayerCritters();
        CHECK(npcs.size() >= 2);

        bool found1 = false;
        bool found2 = false;
        for (auto* npc : npcs) {
            if (npc == cr1) {
                found1 = true;
            }
            if (npc == cr2) {
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
        auto* cr = server->CreateCritter(fn("TestCritter"), false);
        REQUIRE(cr != nullptr);

        server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        CHECK(cr->HasItems());

        server->CrMngr.DestroyInventory(cr);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }
}

// ========== Proto Access C++ API Tests ==========

TEST_CASE("ProtoAccessCppApi")
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

    SECTION("GetProtoCritter")
    {
        const auto* proto = server->GetProtoCritter(fn("TestCritter"));
        CHECK(proto != nullptr);
    }

    SECTION("GetProtoItem")
    {
        const auto* proto = server->GetProtoItem(fn("TestItem"));
        CHECK(proto != nullptr);
    }

    SECTION("GetProtoLocation")
    {
        const auto* proto = server->GetProtoLocation(fn("TestLocation"));
        CHECK(proto != nullptr);
    }

    SECTION("GetProtoCritterReturnsNullForMissing")
    {
        const auto* proto = server->GetProtoCritter(fn("NonExistentProto"));
        CHECK(proto == nullptr);
    }

    SECTION("GetProtoItemReturnsNullForMissing")
    {
        const auto* proto = server->GetProtoItem(fn("NonExistentProto"));
        CHECK(proto == nullptr);
    }

    SECTION("GetProtoLocationReturnsNullForMissing")
    {
        const auto* proto = server->GetProtoLocation(fn("NonExistentProto"));
        CHECK(proto == nullptr);
    }
}

// ========== Script Function Call Tests ==========

TEST_CASE("ScriptFunctionCalls")
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

    SECTION("CallFuncWithReturnValue")
    {
        int32 result = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), result));
        // Result is valid (we just check the API works)
    }

    SECTION("FindFuncReturnsFalseForMissing")
    {
        auto func = server->FindFunc<int32>(fn("NonExistent::Function"));
        CHECK_FALSE(func);
    }

    SECTION("FindFuncWorksForExisting")
    {
        auto func = server->FindFunc<int32>(fn("EntityLifecycle::GetItemInitCalls"));
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
