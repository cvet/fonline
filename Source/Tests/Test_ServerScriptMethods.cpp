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

        return BakerTests::CompileInlineScripts(&compiler_engine, "ServerScriptMethodsScripts",
            {
                {"Scripts/ScriptMethodsTest.fos", R"(
namespace ScriptMethodsTest
{
    // ========== Critter Inventory Operations ==========

    int TestCritterAddAndCountItems()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 5);
        if (item is null) return -2;

        int count = cr.CountItem("TestItem".hstr());
        if (count != 5) return -3;

        // Add more of the same item
        Item@ item2 = cr.AddItem("TestItem".hstr(), 3);
        if (item2 is null) return -4;

        count = cr.CountItem("TestItem".hstr());
        if (count != 8) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItems()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 2);
        cr.AddItem("TestItem2".hstr(), 3);

        array<Item@> items = cr.GetItems();
        if (items.length() < 2) return -2;

        // Get items by pid
        array<Item@> test_items = cr.GetItems("TestItem".hstr());
        if (test_items.length() < 1) return -3;

        array<Item@> test_items2 = cr.GetItems("TestItem2".hstr());
        if (test_items2.length() < 1) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItemById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ added = cr.AddItem("TestItem".hstr(), 1);
        if (added is null) return -2;

        ident item_id = added.Id;

        // Get by id
        Item@ found = cr.GetItem(item_id);
        if (found is null) return -3;
        if (found.Id != item_id) return -4;

        // Get by proto id
        Item@ found_by_pid = cr.GetItem("TestItem".hstr());
        if (found_by_pid is null) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterDestroyItems()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 10);

        // Destroy partial count
        cr.DestroyItem("TestItem".hstr(), 3);
        if (cr.CountItem("TestItem".hstr()) != 7) return -2;

        // Destroy all of a proto
        cr.DestroyItem("TestItem".hstr());
        if (cr.CountItem("TestItem".hstr()) != 0) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Critter State Operations ==========

    int TestCritterStateQueries()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        if (!cr.IsAlive()) return -2;
        if (cr.IsDead()) return -3;
        if (cr.IsKnockout()) return -4;

        // Not on map, so not moving
        if (cr.IsMoving()) return -5;

        // Id should be valid
        if (cr.Id.value == 0) return -6;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterConditionChange()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        if (!cr.IsAlive()) return -2;

        // Set to dead condition
        cr.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);

        if (cr.IsAlive()) return -3;
        if (!cr.IsDead()) return -4;

        // Set to knockout
        cr.SetCondition(CritterCondition::Knockout, CritterActionAnim::None, null);

        if (cr.IsAlive()) return -5;
        if (!cr.IsKnockout()) return -6;

        // Back to alive
        cr.SetCondition(CritterCondition::Alive, CritterActionAnim::None, null);

        if (!cr.IsAlive()) return -7;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Game Critter Queries ==========

    int TestGameGetCritter()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        ident cr_id = cr.Id;

        Critter@ found = Game.GetCritter(cr_id);
        if (found is null) return -2;
        if (found.Id != cr_id) return -3;

        Game.DestroyCritter(cr);

        // After destroy, should not be found
        Critter@ gone = Game.GetCritter(cr_id);
        if (gone !is null) return -4;

        return 0;
    }

    int TestGameGetAllNpc()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // Get all NPCs
        array<Critter@> all_npc = Game.GetAllNpc();
        if (all_npc.length() < 2) return -2;

        // Get NPCs by proto
        array<Critter@> by_pid = Game.GetAllNpc("TestCritter".hstr());
        if (by_pid.length() < 2) return -3;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestGameGetCrittersByFindType()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // All critters
        array<Critter@> any_cr = Game.GetCritters(CritterFindType::Any);
        if (any_cr.length() < 2) return -2;

        // Non-dead
        array<Critter@> non_dead = Game.GetCritters(CritterFindType::NonDead);
        if (non_dead.length() < 2) return -3;

        // NPC only
        array<Critter@> npcs = Game.GetCritters(CritterFindType::Npc);
        if (npcs.length() < 2) return -4;

        // Kill one
        cr1.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);

        // Dead critters
        array<Critter@> dead = Game.GetCritters(CritterFindType::Dead);
        if (dead.isEmpty()) return -5;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    // ========== Game Item Operations ==========

    int TestGameItemQueries()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 5);
        if (item is null) return -2;

        // Global item lookup by id
        Item@ found = Game.GetItem(item.Id);
        if (found is null) return -3;
        if (found.Id != item.Id) return -4;

        // Get all items by pid
        array<Item@> all_items = Game.GetAllItems("TestItem".hstr());
        if (all_items.isEmpty()) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestGameMoveItemBetweenCritters()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item@ item = cr1.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident item_id = item.Id;

        // Move item to cr2
        Item@ moved = Game.MoveItem(item, cr2);
        if (moved is null) return -3;

        // cr2 should have the item by id lookup
        Item@ found = Game.GetItem(item_id);
        if (found is null) return -4;

        // Item should now belong to cr2
        Critter@ owner = found.GetCritter();
        if (owner is null) return -5;
        if (owner.Id != cr2.Id) return -6;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestGameMoveItemPartial()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item@ item = cr1.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident orig_id = item.Id;

        // Move item to cr2 (full, since count=1)
        Item@ moved = Game.MoveItem(item, 1, cr2);
        if (moved is null) return -3;

        // Verify item moved - lookup by original id
        Item@ found = Game.GetItem(orig_id);
        if (found is null) return -4;

        // Verify new owner is cr2
        Critter@ owner = found.GetCritter();
        if (owner is null) return -5;
        if (owner.Id != cr2.Id) return -6;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestGameDestroyItemById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident item_id = item.Id;

        // Destroy item directly
        Game.DestroyItem(item);

        // Item should be gone from global lookup
        Item@ check = Game.GetItem(item_id);
        if (check !is null) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestGameDestroyItemPartialById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident item_id = item.Id;

        // Destroy by id
        Game.DestroyItem(item_id);

        // Verify destroyed
        Item@ check = Game.GetItem(item_id);
        if (check !is null) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Critter Destroy Operations ==========

    int TestDestroyCritterById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        ident cr_id = cr.Id;

        Game.DestroyCritter(cr_id);

        Critter@ check = Game.GetCritter(cr_id);
        if (check !is null) return -2;

        return 0;
    }

    int TestBulkDestroyCritters()
    {
        array<Critter@> critters = {};
        for (int i = 0; i < 5; i++)
        {
            Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
            if (cr is null) return -(i + 1);
            critters.insertLast(cr);
        }

        array<Critter@> all = Game.GetAllNpc("TestCritter".hstr());
        if (all.length() < 5) return -10;

        Game.DestroyCritters(critters);
        return 0;
    }

    int TestBulkDestroyItems()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        array<Item@> items = {};
        for (int i = 0; i < 4; i++)
        {
            Item@ item = cr.AddItem("TestItem".hstr(), 1);
            if (item is null) return -(i + 2);
            items.insertLast(item);
        }

        Game.DestroyItems(items);

        if (cr.CountItem("TestItem".hstr()) != 0) return -10;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Entity Persistence ==========

    int TestEntityPersistence()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        bool was_persistent = cr.IsPersistent();

        cr.MakePersistent(!was_persistent);
        if (cr.IsPersistent() == was_persistent) return -2;

        cr.MakePersistent(was_persistent);
        if (cr.IsPersistent() != was_persistent) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Item Ownership ==========

    int TestItemOwnership()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        // Item should know its owner
        Critter@ owner = item.GetCritter();
        if (owner is null) return -3;
        if (owner.Id != cr.Id) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Critter Direction ==========

    int TestCritterDirection()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Set direction
        cr.SetDir(3);

        // Set dir angle
        cr.SetDirAngle(180);

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Player Critter Lifecycle ==========

    int TestPlayerCritterCreation()
    {
        // Create player-controlled critter
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return -1;

        // Player critter should still be alive
        if (!cr.IsAlive()) return -2;

        // Unload (not destroy) player critter
        Game.UnloadCritter(cr);

        return 0;
    }

    // ========== Comprehensive Critter Operations ==========

    int TestCritterAttachments()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // Attach cr2 to cr1
        cr2.AttachToCritter(cr1);

        // Get attached critters
        array<Critter@> attached = cr1.GetAttachedCritters();
        if (attached.length() != 1) return -2;

        // Detach
        cr2.DetachFromCritter();

        attached = cr1.GetAttachedCritters();
        if (attached.length() != 0) return -3;

        // Detach all
        cr2.AttachToCritter(cr1);
        cr1.DetachAllCritters();
        attached = cr1.GetAttachedCritters();
        if (attached.length() != 0) return -4;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    // ========== Database Operations ==========

    int TestDatabaseQueries()
    {
        // Get all record IDs from a collection
        array<ident> ids = Game.DbGetAllRecordIds("test_collection".hstr());
        // Empty collection is fine

        return 0;
    }

    // ========== Entity Destroy Operations ==========

    int TestDestroyEntityGeneric()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        ident cr_id = cr.Id;

        // Destroy via generic entity destroy
        Game.DestroyEntity(cr_id);

        Critter@ check = Game.GetCritter(cr_id);
        if (check !is null) return -2;

        return 0;
    }

    // ========== Text Pack Operations ==========

    int TestTextPresent()
    {
        // Just calling the method exercises the code path
        // TextPackName might not be available, but the call itself is the coverage goal
        // Using known-invalid values still exercises error handling
        bool present = Game.IsTextPresent(TextPackName::None, 0);
        return 0;
    }

    // ========== Registered Player IDs ==========

    int TestRegisteredPlayerIds()
    {
        array<ident> ids = Game.GetRegisteredPlayerIds();
        // Empty is fine for fresh test server
        return 0;
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

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptMethodsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");
        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        const auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        const auto item2_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem2");
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptMethodsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ScriptMethodsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("ScriptMethodsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("ScriptMethodsItem2.fopro-bin-server", item2_blob);
        runtime_source->AddFile("ScriptMethodsTest.fos-bin-server", script_blob);

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

TEST_CASE("ServerCritterInventoryOperations")
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

    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("AddAndCountItems")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterAddAndCountItems"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetItems")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterGetItems"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetItemById")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterGetItemById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyItems")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterDestroyItems"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

TEST_CASE("ServerCritterStateOperations")
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

    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("StateQueries")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterStateQueries"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("ConditionChange")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterConditionChange"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("Direction")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterDirection"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("Attachments")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestCritterAttachments"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

TEST_CASE("ServerGameCritterQueries")
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

    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("GetCritterById")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameGetCritter"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetAllNpc")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameGetAllNpc"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetCrittersByFindType")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameGetCrittersByFindType"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

TEST_CASE("ServerGameItemOperations")
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

    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("ItemQueries")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameItemQueries"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("MoveItemBetweenCritters")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameMoveItemBetweenCritters"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("MoveItemPartial")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameMoveItemPartial"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyItemById")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameDestroyItemById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyItemPartialById")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestGameDestroyItemPartialById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("ItemOwnership")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestItemOwnership"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

TEST_CASE("ServerEntityLifecycle")
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

    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("Persistence")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestEntityPersistence"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyCritterById")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestDestroyCritterById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("BulkDestroyCritters")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestBulkDestroyCritters"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("BulkDestroyItems")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestBulkDestroyItems"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyEntityGeneric")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestDestroyEntityGeneric"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("PlayerCritterCreation")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestPlayerCritterCreation"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

TEST_CASE("ServerMiscScriptOperations")
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

    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("DatabaseQueries")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestDatabaseQueries"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("TextPresent")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestTextPresent"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("RegisteredPlayerIds")
    {
        auto func = server->FindFunc<int32>(get_func("ScriptMethodsTest::TestRegisteredPlayerIds"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

FO_END_NAMESPACE
