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
#include "NetworkServer.h"
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
        BakerTests::OverrideSetting(settings.CustomCollections, vector<string> {"test_collection:Int", "test_strings:Str"});

        return settings;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
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
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 5);
        if (item is null) return -2;

        int count = cr.CountItem("TestItem".hstr());
        if (count != 5) return -3;

        // Add more of the same item
        Item item2 = cr.AddItem("TestItem".hstr(), 3);
        if (item2 is null) return -4;

        count = cr.CountItem("TestItem".hstr());
        if (count != 8) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItems()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 2);
        cr.AddItem("TestItem2".hstr(), 3);

        array<Item> items = cr.GetItems();
        if (items.length() < 2) return -2;

        // Get items by pid
        array<Item> test_items = cr.GetItems("TestItem".hstr());
        if (test_items.length() < 1) return -3;

        array<Item> test_items2 = cr.GetItems("TestItem2".hstr());
        if (test_items2.length() < 1) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItemById()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item added = cr.AddItem("TestItem".hstr(), 1);
        if (added is null) return -2;

        ident item_id = added.Id;

        // Get by id
        Item? found = cr.GetItem(item_id);
        if (found is null) return -3;
        if (found.Id != item_id) return -4;

        // Get by proto id
        Item? found_by_pid = cr.GetItem("TestItem".hstr());
        if (found_by_pid is null) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterDestroyItems()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
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
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
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
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
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
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        ident cr_id = cr.Id;

        Critter? found = Game.GetCritter(cr_id);
        if (found is null) return -2;
        if (found.Id != cr_id) return -3;

        Game.DestroyCritter(cr);

        // After destroy, should not be found
        Critter? gone = Game.GetCritter(cr_id);
        if (gone !is null) return -4;

        return 0;
    }

    int TestGameGetAllNpc()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // Get all NPCs
        array<Critter> all_npc = Game.GetAllNpc();
        if (all_npc.length() < 2) return -2;

        // Get NPCs by proto
        array<Critter> by_pid = Game.GetAllNpc("TestCritter".hstr());
        if (by_pid.length() < 2) return -3;

        ProtoCritter proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -4;

        array<Critter> by_proto = Game.GetAllNpc(proto);
        if (by_proto.length() < 2) return -5;

        hstring empty_pid;
        array<Critter> by_empty_pid = Game.GetAllNpc(empty_pid);
        if (by_empty_pid.length() < 2) return -6;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestGameGetCrittersByFindType()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // All critters
        array<Critter> any_cr = Game.GetCritters(CritterFindType::Any);
        if (any_cr.length() < 2) return -2;

        // Non-dead
        array<Critter> non_dead = Game.GetCritters(CritterFindType::NonDead);
        if (non_dead.length() < 2) return -3;

        // NPC only
        array<Critter> npcs = Game.GetCritters(CritterFindType::Npc);
        if (npcs.length() < 2) return -4;

        // Kill one
        cr1.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);

        // Dead critters
        array<Critter> dead = Game.GetCritters(CritterFindType::Dead);
        if (dead.isEmpty()) return -5;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    // ========== Game Item Operations ==========

    int TestGameItemQueries()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 5);
        if (item is null) return -2;

        // Global item lookup by id
        Item? found = Game.GetItem(item.Id);
        if (found is null) return -3;
        if (found.Id != item.Id) return -4;

        // Get all items by pid
        array<Item> all_items = Game.GetAllItems("TestItem".hstr());
        if (all_items.isEmpty()) return -5;

        ProtoItem proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -6;

        array<Item> by_proto = Game.GetAllItems(proto);
        if (by_proto.isEmpty()) return -7;

        hstring empty_pid;
        array<Item> by_empty_pid = Game.GetAllItems(empty_pid);
        if (by_empty_pid.isEmpty()) return -8;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestGameMoveItemBetweenCritters()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item item = cr1.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident item_id = item.Id;

        // Move item to cr2
        Item? moved = Game.MoveItem(item, cr2);
        if (moved is null) return -3;

        // cr2 should have the item by id lookup
        Item? found = Game.GetItem(item_id);
        if (found is null) return -4;

        // Item should now belong to cr2
        Critter? owner = found.GetCritter();
        if (owner is null) return -5;
        if (owner.Id != cr2.Id) return -6;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestGameMoveItemPartial()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item item = cr1.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident orig_id = item.Id;

        // Move item to cr2 (full, since count=1)
        Item? moved = Game.MoveItem(item, 1, cr2);
        if (moved is null) return -3;

        // Verify item moved - lookup by original id
        Item? found = Game.GetItem(orig_id);
        if (found is null) return -4;

        // Verify new owner is cr2
        Critter? owner = found.GetCritter();
        if (owner is null) return -5;
        if (owner.Id != cr2.Id) return -6;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestGameDestroyItemById()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident item_id = item.Id;

        // Destroy item directly
        Game.DestroyItem(item);

        // Item should be gone from global lookup
        Item? check = Game.GetItem(item_id);
        if (check !is null) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestGameDestroyItemPartialById()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident item_id = item.Id;

        // Destroy by id
        Game.DestroyItem(item_id);

        // Verify destroyed
        Item? check = Game.GetItem(item_id);
        if (check !is null) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Critter Destroy Operations ==========

    int TestDestroyCritterById()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        ident cr_id = cr.Id;

        Game.DestroyCritter(cr_id);

        Critter? check = Game.GetCritter(cr_id);
        if (check !is null) return -2;

        return 0;
    }

    int TestBulkDestroyCritters()
    {
        array<Critter> critters = {};
        for (int i = 0; i < 5; i++)
        {
            Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
            if (cr is null) return -(i + 1);
            critters.insertLast(cr);
        }

        array<Critter> all = Game.GetAllNpc("TestCritter".hstr());
        if (all.length() < 5) return -10;

        Game.DestroyCritters(critters);
        return 0;
    }

    int TestBulkDestroyItems()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        array<Item> items = {};
        for (int i = 0; i < 4; i++)
        {
            Item item = cr.AddItem("TestItem".hstr(), 1);
            if (item is null) return -(i + 2);
            items.insertLast(item);
        }

        Game.DestroyItems(items);

        if (cr.CountItem("TestItem".hstr()) != 0) return -10;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestDestroyIdListAndCountOverloads()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item counted = cr.AddItem("TestItem".hstr(), 1);
        if (counted is null) return -2;

        ident countedId = counted.Id;
        ident zeroId;

        Game.DestroyItem(zeroId, 1);
        Game.DestroyItem(countedId, 0);

        Item? stillCounted = Game.GetItem(countedId);
        if (stillCounted is null) return -3;
        if (stillCounted.Count <= 0) return -4;

        int countedAmount = stillCounted.Count;

        Game.DestroyItem(countedId, countedAmount);
        if (Game.GetItem(countedId) !is null) return -5;

        Item item1 = cr.AddItem("TestItem".hstr(), 1);
        Item item2 = cr.AddItem("TestItem2".hstr(), 1);
        if (item1 is null || item2 is null) return -6;

        ident item1Id = item1.Id;
        ident item2Id = item2.Id;
        array<ident> itemIds = {zeroId, item1Id, item2Id};

        Game.DestroyItems(itemIds);

        if (Game.GetItem(item1Id) !is null) return -7;
        if (Game.GetItem(item2Id) !is null) return -8;

        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -9;

        ident cr1Id = cr1.Id;
        ident cr2Id = cr2.Id;
        array<ident> critterIds = {zeroId, cr1Id, cr2Id};

        Game.DestroyCritters(critterIds);

        if (Game.GetCritter(cr1Id) !is null) return -10;
        if (Game.GetCritter(cr2Id) !is null) return -11;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Entity Persistence ==========

    int TestEntityPersistence()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
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
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        // Item should know its owner
        Critter? owner = item.GetCritter();
        if (owner is null) return -3;
        if (owner.Id != cr.Id) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Critter Direction ==========

    int TestCritterDirection()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Set direction
        cr.SetDir(HDIR_SouthWest);

        // Set dir angle
        cr.SetDir(mdir(180));

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Player Critter Lifecycle ==========

    int TestPlayerCritterCreation()
    {
        // Create player-controlled critter
        Critter cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return -1;

        // Player critter should still be alive
        if (!cr.IsAlive()) return -2;

        // Unload (not destroy) player critter
        Game.UnloadCritter(cr);

        return 0;
    }

    int TestGameCreateCritterOverloads()
    {
        ProtoCritter? proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -1;

        Critter cr1 = Game.CreateCritter(proto, false);
        if (cr1 is null) return -2;

        dict<CritterProperty, any> anyProps = {{CritterProperty::LookDistance, 7}};

        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false, anyProps);
        if (cr2 is null) return -3;

        Critter cr3 = Game.CreateCritter(proto, false, anyProps);
        if (cr3 is null) return -4;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr3);

        return 0;
    }

    // ========== Comprehensive Critter Operations ==========

    int TestCritterAttachments()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // Attach cr2 to cr1
        cr2.AttachToCritter(cr1);

        // Get attached critters
        array<Critter> attached = cr1.GetAttachedCritters();
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

    int TestDatabaseRecordRoundtrip()
    {
        ident intId;
        intId.value = 1234501;

        Game.DbInsertRecord("test_collection".hstr(), intId, {{"Name", "Alpha"}, {"Kind", "Int"}});
        if (!Game.DbHasRecord("test_collection".hstr(), intId)) return -1;

        dict<string, string> intDoc = Game.DbGetRecord("test_collection".hstr(), intId);
        if (!intDoc.exists("Name")) return -2;
        if (intDoc.get("Name") != "Alpha") return -3;

        Game.DbUpdateRecordString("test_collection".hstr(), intId, "Name", "Beta");
        Game.DbUpdateRecordInt64("test_collection".hstr(), intId, "Count", 42);
        Game.DbUpdateRecordFloat64("test_collection".hstr(), intId, "Ratio", 1.5);
        Game.DbUpdateRecordBool("test_collection".hstr(), intId, "Enabled", true);

        intDoc = Game.DbGetRecord("test_collection".hstr(), intId);
        if (intDoc.get("Name") != "Beta") return -4;
        if (!intDoc.exists("Count")) return -5;
        if (!intDoc.exists("Ratio")) return -6;
        if (!intDoc.exists("Enabled")) return -7;

        string stringId = "record-key";
        Game.DbInsertRecord("test_strings".hstr(), stringId, {{"Name", "Gamma"}, {"Kind", "String"}});
        if (!Game.DbHasRecord("test_strings".hstr(), stringId)) return -8;

        dict<string, string> stringDoc = Game.DbGetRecord("test_strings".hstr(), stringId);
        if (stringDoc.get("Name") != "Gamma") return -9;

        Game.DbUpdateRecordString("test_strings".hstr(), stringId, "Name", "Delta");
        Game.DbUpdateRecordInt64("test_strings".hstr(), stringId, "Count", 7);
        Game.DbUpdateRecordFloat64("test_strings".hstr(), stringId, "Ratio", 2.5);
        Game.DbUpdateRecordBool("test_strings".hstr(), stringId, "Enabled", false);

        stringDoc = Game.DbGetRecord("test_strings".hstr(), stringId);
        if (stringDoc.get("Name") != "Delta") return -10;
        if (!stringDoc.exists("Count")) return -11;
        if (!stringDoc.exists("Ratio")) return -12;
        if (!stringDoc.exists("Enabled")) return -13;

        Game.DbRemoveRecord("test_strings".hstr(), stringId);
        if (Game.DbHasRecord("test_strings".hstr(), stringId)) return -14;
        Game.DbRemoveRecord("test_strings".hstr(), stringId);

        Game.DbRemoveRecord("test_collection".hstr(), intId);
        if (Game.DbHasRecord("test_collection".hstr(), intId)) return -15;
        Game.DbRemoveRecord("test_collection".hstr(), intId);

        return 0;
    }

    int TestDatabaseRecordEnumerationSetup()
    {
        ident intId;
        intId.value = 1234510;

        Game.DbInsertRecord("test_collection".hstr(), intId, {{"Name", "ListedInt"}});
        Game.DbInsertRecord("test_strings".hstr(), "record-list-key", {{"Name", "ListedString"}});

        return 0;
    }

    int TestDatabaseRecordEnumerationAfterCommit()
    {
        ident intId;
        intId.value = 1234510;

        array<ident> ids = Game.DbGetAllRecordIds("test_collection".hstr());
        bool intIdFound = false;
        for (uint i = 0; i < ids.length(); i++)
        {
            if (ids[i].value == intId.value) intIdFound = true;
        }
        if (!intIdFound) return -1;

        string stringId = "record-list-key";
        array<string> keys = Game.DbGetAllRecordKeys("test_strings".hstr());
        if (keys.find(stringId) < 0) return -2;

        Game.DbRemoveRecord("test_collection".hstr(), intId);
        Game.DbRemoveRecord("test_strings".hstr(), stringId);

        return 0;
    }

    void TestDatabaseEmptyCollectionThrows()
    {
        hstring collection;
        Game.DbGetAllRecordIds(collection);
    }

    void TestDatabaseKeysEmptyCollectionThrows()
    {
        hstring collection;
        Game.DbGetAllRecordKeys(collection);
    }

    void TestDatabaseGetPlayerDataZeroIdThrows()
    {
        ident playerId;
        Game.DbGetPlayerData(playerId);
    }

    void TestDatabaseHasRecordZeroIdThrows()
    {
        ident intId;
        Game.DbHasRecord("test_collection".hstr(), intId);
    }

    void TestDatabaseHasRecordEmptyStringIdThrows()
    {
        string stringId = "";
        Game.DbHasRecord("test_strings".hstr(), stringId);
    }

    void TestDatabaseGetRecordZeroIdThrows()
    {
        ident intId;
        Game.DbGetRecord("test_collection".hstr(), intId);
    }

    void TestDatabaseGetRecordEmptyStringIdThrows()
    {
        string stringId = "";
        Game.DbGetRecord("test_strings".hstr(), stringId);
    }

    void TestDatabaseInsertZeroIdThrows()
    {
        ident intId;
        Game.DbInsertRecord("test_collection".hstr(), intId, {{"Name", "Alpha"}});
    }

    void TestDatabaseInsertEmptyValuesThrows()
    {
        ident intId;
        intId.value = 1234502;

        dict<string, string> values = {};
        Game.DbInsertRecord("test_collection".hstr(), intId, values);
    }

    void TestDatabaseInsertEmptyKeyThrows()
    {
        ident intId;
        intId.value = 1234503;

        Game.DbInsertRecord("test_collection".hstr(), intId, {{"", "Alpha"}});
    }

    void TestDatabaseInsertDuplicateThrows()
    {
        ident intId;
        intId.value = 1234504;

        Game.DbInsertRecord("test_collection".hstr(), intId, {{"Name", "Alpha"}});
        Game.DbInsertRecord("test_collection".hstr(), intId, {{"Name", "Beta"}});
    }

    void TestDatabaseUpdateMissingRecordThrows()
    {
        ident intId;
        intId.value = 1234505;

        Game.DbUpdateRecordString("test_collection".hstr(), intId, "Name", "Beta");
    }

    void TestDatabaseUpdateEmptyKeyThrows()
    {
        ident intId;
        intId.value = 1234506;

        Game.DbInsertRecord("test_collection".hstr(), intId, {{"Name", "Alpha"}});
        Game.DbUpdateRecordString("test_collection".hstr(), intId, "", "Beta");
    }

    void TestDatabaseUpdateEmptyStringIdThrows()
    {
        string stringId = "";
        Game.DbUpdateRecordString("test_strings".hstr(), stringId, "Name", "Beta");
    }

    void TestDatabaseRemoveEmptyCollectionThrows()
    {
        ident intId;
        intId.value = 1234507;

        hstring collection;
        Game.DbRemoveRecord(collection, intId);
    }

    void TestDatabaseRemoveZeroIdThrows()
    {
        ident intId;
        Game.DbRemoveRecord("test_collection".hstr(), intId);
    }

    void TestDatabaseRemoveEmptyStringIdThrows()
    {
        string stringId = "";
        Game.DbRemoveRecord("test_strings".hstr(), stringId);
    }

    // ========== Entity Destroy Operations ==========

    int TestDestroyEntityGeneric()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        ident cr_id = cr.Id;

        // Destroy via generic entity destroy
        Game.DestroyEntity(cr_id);

        Critter? check = Game.GetCritter(cr_id);
        if (check !is null) return -2;

        return 0;
    }

    int TestDestroyEntitiesByHandles()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        ident cr1Id = cr1.Id;
        ident cr2Id = cr2.Id;

        Entity? ent1 = Game.GetEntity(cr1Id);
        Entity? ent2 = Game.GetEntity(cr2Id);
        if (ent1 is null || ent2 is null) return -2;

        array<Entity> entities = {ent1, ent2};
        Game.DestroyEntities(entities);

        if (Game.GetCritter(cr1Id) !is null) return -3;
        if (Game.GetCritter(cr2Id) !is null) return -4;

        return 0;
    }

    // ========== Text Pack Operations ==========

    int TestTextPresent()
    {
        bool present = Game.IsTextPresent(TextPackKey());
        int count = Game.GetTextCount(TextPackKey());
        if (count < 0) return -1;

        return 0;
    }

    // ========== Registered Player IDs ==========

    int TestRegisteredPlayerIds()
    {
        array<ident> ids = Game.GetRegisteredPlayerIds();
        // Empty is fine for fresh test server
        return 0;
    }

    bool ContainsPlayerId(array<Player> players, ident playerId)
    {
        for (uint i = 0; i < players.length(); i++) {
            Player? player = players[i];

            if (player !is null && player.Id == playerId) {
                return true;
            }
        }

        return false;
    }

    int TestCreateUnloginedPlayerHelper()
    {
        Player unlogined = Game.CreateUnloginedPlayer();
        if (unlogined is null) return -1;

        unlogined.HardDisconnect();
        return 0;
    }

    int TestPlayerConnectionAndCritterMethods(Player unlogined)
    {
        if (unlogined is null) return -1;
        if (unlogined.GetHost() != "Dummy") return -2;
        if (unlogined.GetPort() != 0) return -3;

        unlogined.SetName("ScriptPlayerMethods");

        Critter? current = unlogined.GetControlledCritter();
        if (current !is null) return -4;

        Critter cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return -5;

        unlogined.SwitchCritter(cr);
        Critter? controlled = unlogined.GetControlledCritter();
        if (controlled is null || controlled.Id != cr.Id) return -6;

        Player? crPlayer = cr.GetPlayer();
        if (crPlayer is null || crPlayer.Id != unlogined.Id) return -7;

        unlogined.SwitchCritter(null);
        if (unlogined.GetControlledCritter() !is null) return -8;
        if (cr.GetPlayer() !is null) return -9;

        Game.UnloadCritter(cr);

        unlogined.Disconnect();
        return 0;
    }

    int TestPlayerSetNameValidation(Player unlogined)
    {
        if (unlogined is null) return -1;

        try {
            unlogined.SetName("");
            return -2;
        }
        catch {
        }

        try {
            unlogined.SetName(" LeadingSpace");
            return -3;
        }
        catch {
        }

        try {
            unlogined.SetName("TrailingSpace ");
            return -4;
        }
        catch {
        }

        unlogined.SetName("ScriptPlayerNameOk");
        return 0;
    }

    int TestPlayerMapViewMethods(Player unlogined)
    {
        if (unlogined is null) return -1;

        Critter cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return -2;

        try {
            unlogined.RefreshCritterMoving(cr);
            Game.UnloadCritter(cr);
            return -3;
        }
        catch {
        }

        unlogined.SwitchCritter(cr);

        array<hstring> mapPids = {"TestMap".hstr()};
        Location loc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (loc is null) {
            unlogined.SwitchCritter(null);
            Game.UnloadCritter(cr);
            return -4;
        }

        Map? map = loc.GetMap("TestMap".hstr());
        if (map is null) {
            unlogined.SwitchCritter(null);
            Game.UnloadCritter(cr);
            Game.DestroyLocation(loc);
            return -5;
        }

        try {
            unlogined.ViewMap(map, mpos(0, 0));
            unlogined.SwitchCritter(null);
            Game.UnloadCritter(cr);
            Game.DestroyLocation(loc);
            return -6;
        }
        catch {
        }

        try {
            unlogined.UnloadMap();
            unlogined.SwitchCritter(null);
            Game.UnloadCritter(cr);
            Game.DestroyLocation(loc);
            return -7;
        }
        catch {
        }

        unlogined.SwitchCritter(null);

        Critter mapCr = map.AddCritter("TestCritter".hstr(), mpos(11, 10), mdir(0));
        if (mapCr is null) {
            Game.UnloadCritter(cr);
            Game.DestroyLocation(loc);
            return -8;
        }

        try {
            unlogined.ViewMap(map, mpos(9999, 9999));
            Game.UnloadCritter(cr);
            Game.DestroyLocation(loc);
            return -9;
        }
        catch {
        }

        unlogined.ViewMap(map, mpos(10, 10));
        unlogined.RefreshCritterMoving(mapCr);
        unlogined.ResetViewMap();
        unlogined.UnloadMap();

        Game.UnloadCritter(cr);
        Game.DestroyLocation(loc);
        return 0;
    }

    int TestLoginPlayerToNewRecordFromPreparedPlayer(Player unlogined)
    {
        if (unlogined is null) return -1;

        Player? new_player = Game.LoginPlayerToNewRecord(unlogined);
        if (new_player is null) return -2;
        if (new_player.Id == ZERO_IDENT) return -3;

        Player? fetched_player = Game.GetPlayer(new_player.Id);
        if (fetched_player is null || fetched_player.Id != new_player.Id) return -4;

        array<Player> online_players = Game.GetOnlinePlayers();
        if (!ContainsPlayerId(online_players, new_player.Id)) return -5;

        return 0;
    }

    int TestLoginPlayerToExistentRecordFromPreparedPlayer(Player unlogined, ident playerId)
    {
        if (unlogined is null) return -1;

        Player? reconnected_player = Game.LoginPlayerToExistentRecord(unlogined, playerId);
        if (reconnected_player is null) return -2;
        if (reconnected_player.Id != playerId) return -3;

        reconnected_player.HardDisconnect();
        return 0;
    }

    int TestLoginPlayerToTempSessionFromPreparedPlayer(Player unlogined)
    {
        if (unlogined is null) return -1;

        Player? temp_player = Game.LoginPlayerToTempSession(unlogined);
        if (temp_player is null) return -2;

        temp_player.HardDisconnect();
        return 0;
    }

    int TestLoginPlayerToExistentRecordZeroIdThrows()
    {
        Player unlogined = Game.CreateUnloginedPlayer();
        if (unlogined is null) return -1;

        unlogined.SetName("ScriptLoginZero");

        try {
            Player? player = Game.LoginPlayerToExistentRecord(unlogined, ZERO_IDENT);
            if (player !is null) return -2;
        }
        catch {
            unlogined.HardDisconnect();
            return 0;
        }

        unlogined.HardDisconnect();
        return -3;
    }

    int TestRuntimeHelpers()
    {
        int64 process_mem = Game.GetProcessMemoryUsage();
        int64 allocator_mem = Game.GetAllocatorMemoryUsage();

        if (process_mem < 0) return -1;
        if (allocator_mem < 0) return -2;

        array<Player> online_players = Game.GetOnlinePlayers();
        if (!online_players.isEmpty()) return -3;

        Player? zero_player = Game.GetPlayer(ZERO_IDENT);
        if (zero_player !is null) return -4;

        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -5;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) {
            Game.DestroyCritter(cr);
            return -6;
        }

        bool cr_exists = Game.DbHasEntity(cr);
        bool item_exists = Game.DbHasEntity(item);

        Game.DestroyCritter(cr);

        return 0;
    }

    int TestEntityPlainDataAccessors()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.SetAsInt(CritterProperty::LookDistance, 9);
        if (cr.GetAsInt(CritterProperty::LookDistance) != 9) {
            Game.DestroyCritter(cr);
            return -2;
        }

        any look_any = "11";
        cr.SetAsAny(CritterProperty::LookDistance, look_any);
        any read_look_any = cr.GetAsAny(CritterProperty::LookDistance);
        int look_from_any = read_look_any;
        if (look_from_any != 11) {
            Game.DestroyCritter(cr);
            return -3;
        }

        Item item = cr.AddItem("TestItem".hstr(), 3);
        if (item is null) {
            Game.DestroyCritter(cr);
            return -4;
        }

        item.SetAsInt(ItemProperty::Hidden, 1);
        if (item.GetAsInt(ItemProperty::Hidden) != 1) {
            Game.DestroyCritter(cr);
            return -5;
        }

        any hidden_any = "false";
        item.SetAsAny(ItemProperty::Hidden, hidden_any);
        any read_hidden_any = item.GetAsAny(ItemProperty::Hidden);
        bool hidden_from_any = read_hidden_any;
        if (hidden_from_any) {
            Game.DestroyCritter(cr);
            return -6;
        }

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestPropertyInfoHelpers()
    {
        bool isDisabled = true;
        bool isVirtual = true;
        bool isDict = true;
        bool isArray = true;
        bool isStringLike = true;
        string enumName = "unexpected";
        bool isInt = false;
        bool isFloat = true;
        bool isBool = true;
        int baseSize = 0;
        bool isSynced = false;

        Game.GetPropertyInfo(CritterProperty::LookDistance, isDisabled, isVirtual, isDict, isArray, isStringLike, enumName, isInt, isFloat, isBool, baseSize, isSynced);
        if (isDisabled || isVirtual || isDict || isArray || isStringLike) return -1;
        if (!isInt || isFloat || isBool) return -2;
        if (baseSize != 4) return -3;
        if (!isSynced) return -4;
        if (!enumName.isEmpty()) return -5;

        Game.GetPropertyInfo(ItemProperty::Hidden, isDisabled, isVirtual, isDict, isArray, isStringLike, enumName, isInt, isFloat, isBool, baseSize, isSynced);
        if (isDisabled || isVirtual || isDict || isArray || isStringLike) return -6;
        if (isInt || isFloat || !isBool) return -7;
        if (baseSize != 1) return -8;
        if (isSynced) return -9;

        Game.GetPropertyInfo(ItemProperty::PicMap, isDisabled, isVirtual, isDict, isArray, isStringLike, enumName, isInt, isFloat, isBool, baseSize, isSynced);
        if (isDisabled || isVirtual || isDict || isArray || !isStringLike) return -10;
        if (isInt || isFloat || isBool) return -11;
        if (baseSize <= 0) return -12;
        if (!isSynced) return -13;

        Game.GetPropertyInfo(CritterProperty::Condition, isDisabled, isVirtual, isDict, isArray, isStringLike, enumName, isInt, isFloat, isBool, baseSize, isSynced);
        if (isDisabled || isVirtual || isDict || isArray || isStringLike) return -14;
        if (isInt || isFloat || isBool) return -15;
        if (enumName != "CritterCondition") return -16;
        if (baseSize != 1) return -17;

        Game.GetPropertyInfo(ItemProperty::MultihexLines, isDisabled, isVirtual, isDict, isArray, isStringLike, enumName, isInt, isFloat, isBool, baseSize, isSynced);
        if (isDisabled || isVirtual || isDict || !isArray || isStringLike) return -18;
        if (!isInt || isFloat || isBool) return -19;
        if (!enumName.isEmpty()) return -20;
        if (baseSize != 1) return -21;

        return 0;
    }

    void TestPropertyInfoNoneThrows()
    {
        bool isDisabled;
        bool isVirtual;
        bool isDict;
        bool isArray;
        bool isStringLike;
        string enumName;
        bool isInt;
        bool isFloat;
        bool isBool;
        int baseSize;
        bool isSynced;

        Game.GetPropertyInfo(CritterProperty::None, isDisabled, isVirtual, isDict, isArray, isStringLike, enumName, isInt, isFloat, isBool, baseSize, isSynced);
    }

    void TestEntityAccessorsInvalidPropertyThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        cr.GetAsInt(CritterProperty::None);
    }

    void TestEntityGetAsIntNonPlainPropertyThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        Item item = cr.AddItem("TestItem".hstr(), 1);
        item.GetAsInt(ItemProperty::MultihexLines);
    }

    void TestEntitySetAsIntNonPlainPropertyThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        Item item = cr.AddItem("TestItem".hstr(), 1);
        item.SetAsInt(ItemProperty::MultihexLines, 1);
    }

    void TestEntityGetAsAnyNonPlainPropertyThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        Item item = cr.AddItem("TestItem".hstr(), 1);
        item.GetAsAny(ItemProperty::MultihexLines);
    }

    void TestEntitySetAsAnyNonPlainPropertyThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        Item item = cr.AddItem("TestItem".hstr(), 1);
        any value = 1;
        item.SetAsAny(ItemProperty::MultihexLines, value);
    }

    void TestEntitySetAsIntImmutablePropertyThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        cr.SetAsInt(CritterProperty::Condition, 0);
    }

    void TestEntitySetAsAnyImmutablePropertyThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        any value = 0;
        cr.SetAsAny(CritterProperty::Condition, value);
    }

    [[PropertyGetter]]
    int OnLookDistanceGetter(Critter cr)
    {
        return cr.LookDistance;
    }

    [[PropertySetter]]
    void OnLookDistanceSetter(Critter cr)
    {
        PropertySetterCallCount++;
    }

    [[PropertySetter]]
    void OnLookDistanceTransformSetter(Critter cr, CritterProperty prop, int& value)
    {
        if (prop != CritterProperty::LookDistance) return;

        PropertySetterTransformCallCount++;
        value += 4;
    }

    [[PropertySetter]]
    int OnInvalidLookDistanceSetter(Critter cr)
    {
        return cr.LookDistance;
    }

    int PropertySetterCallCount = 0;
    int PropertySetterTransformCallCount = 0;

    void TestPropertyGetterNoneThrows()
    {
        Game.SetPropertyGetter(CritterProperty::None, OnLookDistanceGetter);
    }

    void TestPropertyGetterNonVirtualThrows()
    {
        Game.SetPropertyGetter(CritterProperty::LookDistance, OnLookDistanceGetter);
    }

    void TestPropertySetterNoneThrows()
    {
        Game.AddPropertySetter(CritterProperty::None, OnLookDistanceSetter);
    }

    void TestPropertySetterInvalidFunctionThrows()
    {
        Game.AddPropertySetter(CritterProperty::LookDistance, OnInvalidLookDistanceSetter);
    }

    int TestPropertySetterCallback()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        PropertySetterCallCount = 0;
        Game.AddPropertySetter(CritterProperty::LookDistance, OnLookDistanceSetter);

        cr.LookDistance = 31;
        if (PropertySetterCallCount != 1) {
            Game.DestroyCritter(cr);
            return -2;
        }

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestPropertySetterTransformCallback()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        PropertySetterTransformCallCount = 0;
        Game.AddPropertySetter(CritterProperty::LookDistance, OnLookDistanceTransformSetter);

        cr.LookDistance = 21;
        if (PropertySetterTransformCallCount != 1) {
            Game.DestroyCritter(cr);
            return -2;
        }
        if (cr.LookDistance != 25) {
            Game.DestroyCritter(cr);
            return -3;
        }

        Game.DestroyCritter(cr);
        return 0;
    }

    bool ContainsEntity(array<Entity> entities, Entity target)
    {
        for (uint i = 0; i < entities.length(); i++) {
            Entity? entity = entities[i];

            if (entity is target) {
                return true;
            }
        }

        return false;
    }

    [[Async]]
    int TestSyncHelpers()
    {
        array<Entity> held = Game.GetHeldSyncEntities();
        if (!held.isEmpty()) return -1;

        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr3 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null || cr3 is null) return -2;

        if (!Game.IsEntityLocked(cr1)) return -3;

        Game.Sync(cr1);

        held = Game.GetHeldSyncEntities();
        if (!ContainsEntity(held, cr1)) return -4;
        if (!Game.IsEntityLocked(cr1)) return -5;

        Game.SyncEnsure(cr2);
        if (!Game.IsEntityLocked(cr2)) return -7;

        held = Game.GetHeldSyncEntities();
        if (!ContainsEntity(held, cr2)) return -8;

        Game.SyncRelease();
        held = Game.GetHeldSyncEntities();
        if (!held.isEmpty()) return -9;

        Game.Sync(cr1, cr2);
        if (!Game.IsEntityLocked(cr1) || !Game.IsEntityLocked(cr2)) return -10;

        Game.SyncRelease();

        Game.Sync(cr1, cr2, cr3);
        if (!Game.IsEntityLocked(cr1) || !Game.IsEntityLocked(cr2) || !Game.IsEntityLocked(cr3)) return -11;

        Game.SyncRelease();

        array<Entity> sync_entities = {cr1, cr2, cr3};
        Game.Sync(sync_entities);
        if (!Game.IsEntityLocked(cr1) || !Game.IsEntityLocked(cr2) || !Game.IsEntityLocked(cr3)) return -12;

        Game.SyncRelease();

        Game.Lock();
        Game.Unlock();

        Game.Lock();
        Game.SyncRelease();

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr3);

        return 0;
    }

    int TestRegistryCounts()
    {
        int entity_count_before = Game.GetEntityRegistryCount();
        int player_count_before = Game.GetPlayerRegistryCount();
        int critter_count_before = Game.GetCritterRegistryCount();
        int item_count_before = Game.GetItemRegistryCount();

        if (Game.GetLocationRegistryCount() != 0) return -1;
        if (Game.GetMapRegistryCount() != 0) return -2;

        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -3;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) {
            Game.DestroyCritter(cr);
            return -4;
        }

        if (Game.GetCritterRegistryCount() != critter_count_before + 1) return -5;
        if (Game.GetItemRegistryCount() != item_count_before + 1) return -6;
        if (Game.GetEntityRegistryCount() < entity_count_before + 2) return -7;
        if (Game.GetPlayerRegistryCount() != player_count_before) return -8;

        Game.DestroyCritter(cr);

        if (Game.GetCritterRegistryCount() != critter_count_before) return -9;
        if (Game.GetItemRegistryCount() != item_count_before) return -10;

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

    static auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = DataWriter(map_data);

        writer.Write<uint32_t>(uint32_t {0}); // hashes_count
        writer.Write<uint32_t>(uint32_t {0}); // cr_count
        writer.Write<uint32_t>(uint32_t {0}); // item_count

        return map_data;
    }

    static auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), proto_engine.GetPropertyRegistrator(type_name)};
        proto.SetSize(map_size);
        proto.GetProperties().StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WritePtr(proto_name.data(), proto_name.length());
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        writer.WritePtr(props_data.data(), props_data.size());

        return protos_data;
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
        const auto location_type = proto_engine.Hashes.ToHashedString("Location");
        const auto map_type = proto_engine.Hashes.ToHashedString("Map");
        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        const auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        const auto item2_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem2");
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        const auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "TestMap", msize {200, 200});
        const auto fomap_blob = MakeEmptyMapBlob();
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptMethodsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ScriptMethodsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("ScriptMethodsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("ScriptMethodsItem2.fopro-bin-server", item2_blob);
        runtime_source->AddFile("ScriptMethodsLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("TestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("TestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("ScriptMethodsTest.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForStart(ServerEngine* server) -> string
    {
        FO_VERIFY_AND_THROW(server, "Missing server instance");

        for (int32_t i = 0; i < 6000; i++) {
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
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterAddAndCountItems"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetItems")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterGetItems"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetItemById")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterGetItemById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyItems")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterDestroyItems"));
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
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterStateQueries"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("ConditionChange")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterConditionChange"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("Direction")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterDirection"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("Attachments")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCritterAttachments"));
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
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameGetCritter"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetAllNpc")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameGetAllNpc"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GetCrittersByFindType")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameGetCrittersByFindType"));
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
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameItemQueries"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("MoveItemBetweenCritters")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameMoveItemBetweenCritters"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("MoveItemPartial")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameMoveItemPartial"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyItemById")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameDestroyItemById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyItemPartialById")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameDestroyItemPartialById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("ItemOwnership")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestItemOwnership"));
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
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestEntityPersistence"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyCritterById")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDestroyCritterById"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("BulkDestroyCritters")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestBulkDestroyCritters"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("BulkDestroyItems")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestBulkDestroyItems"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyIdListAndCountOverloads")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDestroyIdListAndCountOverloads"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyEntityGeneric")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDestroyEntityGeneric"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DestroyEntitiesByHandles")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDestroyEntitiesByHandles"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("PlayerCritterCreation")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestPlayerCritterCreation"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("GameCreateCritterOverloads")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestGameCreateCritterOverloads"));
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
    const auto run_throwing_func = [&server, &get_func](string_view func_name, string_view expected_message) {
        auto func = server->FindFunc<void>(get_func(func_name));
        REQUIRE(func);

        const auto prev_callback = GetExceptionCallback();
        string message;
        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData&, bool) { message = string(msg); });
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { SetExceptionCallback(std::move(prev)); });

        CHECK_FALSE(func.Call());
        INFO(message);
        CHECK(message.find(expected_message) != string::npos);
    };

    SECTION("DatabaseQueries")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDatabaseQueries"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DatabaseRecordRoundtrip")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDatabaseRecordRoundtrip"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DatabaseRecordEnumerationAfterCommit")
    {
        auto setup_func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDatabaseRecordEnumerationSetup"));
        REQUIRE(setup_func);
        REQUIRE(setup_func.Call());
        REQUIRE(setup_func.GetResult() == 0);

        server->DbStorage.WaitCommitChanges();

        auto verify_func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestDatabaseRecordEnumerationAfterCommit"));
        REQUIRE(verify_func);
        REQUIRE(verify_func.Call());
        CHECK(verify_func.GetResult() == 0);
    }

    SECTION("DatabaseEmptyCollectionThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseEmptyCollectionThrows", "Collection name arg is empty");
    }

    SECTION("DatabaseKeysEmptyCollectionThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseKeysEmptyCollectionThrows", "Collection name arg is empty");
    }

    SECTION("DatabaseGetPlayerDataZeroIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseGetPlayerDataZeroIdThrows", "Player id arg is zero");
    }

    SECTION("LoginPlayerToExistentRecordZeroIdThrows")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestLoginPlayerToExistentRecordZeroIdThrows"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("DatabaseHasRecordZeroIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseHasRecordZeroIdThrows", "Record id arg is zero");
    }

    SECTION("DatabaseHasRecordEmptyStringIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseHasRecordEmptyStringIdThrows", "Record id arg is empty");
    }

    SECTION("DatabaseGetRecordZeroIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseGetRecordZeroIdThrows", "Record id arg is zero");
    }

    SECTION("DatabaseGetRecordEmptyStringIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseGetRecordEmptyStringIdThrows", "Record id arg is empty");
    }

    SECTION("DatabaseInsertZeroIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseInsertZeroIdThrows", "Record id arg is zero");
    }

    SECTION("DatabaseInsertEmptyValuesThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseInsertEmptyValuesThrows", "Record values are empty");
    }

    SECTION("DatabaseInsertEmptyKeyThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseInsertEmptyKeyThrows", "Record key is empty");
    }

    SECTION("DatabaseInsertDuplicateThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseInsertDuplicateThrows", "Record already exists");
    }

    SECTION("DatabaseUpdateMissingRecordThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseUpdateMissingRecordThrows", "Record not found");
    }

    SECTION("DatabaseUpdateEmptyKeyThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseUpdateEmptyKeyThrows", "Record key is empty");
    }

    SECTION("DatabaseUpdateEmptyStringIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseUpdateEmptyStringIdThrows", "Record id arg is empty");
    }

    SECTION("DatabaseRemoveEmptyCollectionThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseRemoveEmptyCollectionThrows", "Collection name arg is empty");
    }

    SECTION("DatabaseRemoveZeroIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseRemoveZeroIdThrows", "Record id arg is zero");
    }

    SECTION("DatabaseRemoveEmptyStringIdThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestDatabaseRemoveEmptyStringIdThrows", "Record id arg is empty");
    }

    SECTION("TextPresent")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestTextPresent"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("RegisteredPlayerIds")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestRegisteredPlayerIds"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("PlayerConnectionAndCritterMethods")
    {
        const auto create_unlogined_player = [&server](string_view name) -> Player* {
            auto* unlogined_player = server->CreateUnloginedPlayer(NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected));

            if (unlogined_player != nullptr) {
                unlogined_player->SetName(name);
            }

            return unlogined_player;
        };

        auto methods_func = server->FindFunc<int32_t, Player*>(get_func("ScriptMethodsTest::TestPlayerConnectionAndCritterMethods"));
        REQUIRE(methods_func);

        Player* methods_player = create_unlogined_player("ScriptPlayerMethodsStart");
        REQUIRE(methods_player != nullptr);

        auto methods_cleanup = scope_exit([&methods_player]() noexcept {
            safe_call([&methods_player] {
                if (methods_player != nullptr && !methods_player->IsDestroyed()) {
                    methods_player->GetConnection()->HardDisconnect();
                }
            });
        });

        REQUIRE(methods_func.Call(methods_player));
        CHECK(methods_func.GetResult() == 0);
        CHECK(methods_player->GetName() == "ScriptPlayerMethods");
        CHECK(methods_player->GetConnection()->IsGracefulDisconnected());
        CHECK(methods_player->GetControlledCritter() == nullptr);

        auto name_validation_func = server->FindFunc<int32_t, Player*>(get_func("ScriptMethodsTest::TestPlayerSetNameValidation"));
        REQUIRE(name_validation_func);

        Player* name_player = create_unlogined_player("ScriptPlayerNameStart");
        REQUIRE(name_player != nullptr);

        auto name_cleanup = scope_exit([&name_player]() noexcept {
            safe_call([&name_player] {
                if (name_player != nullptr && !name_player->IsDestroyed()) {
                    name_player->GetConnection()->HardDisconnect();
                }
            });
        });

        REQUIRE(name_validation_func.Call(name_player));
        CHECK(name_validation_func.GetResult() == 0);
        CHECK(name_player->GetName() == "ScriptPlayerNameOk");

        auto map_view_func = server->FindFunc<int32_t, Player*>(get_func("ScriptMethodsTest::TestPlayerMapViewMethods"));
        REQUIRE(map_view_func);

        Player* map_view_player = create_unlogined_player("ScriptPlayerMapView");
        REQUIRE(map_view_player != nullptr);

        auto map_view_cleanup = scope_exit([&map_view_player]() noexcept {
            safe_call([&map_view_player] {
                if (map_view_player != nullptr && !map_view_player->IsDestroyed()) {
                    map_view_player->GetConnection()->HardDisconnect();
                }
            });
        });

        REQUIRE(map_view_func.Call(map_view_player));
        CHECK(map_view_func.GetResult() == 0);
        CHECK(map_view_player->GetControlledCritter() == nullptr);
    }

    SECTION("PlayerLoginHelpers")
    {
        const auto create_unlogined_player = [&server](string_view name) -> Player* {
            auto* unlogined_player = server->CreateUnloginedPlayer(NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected));

            if (unlogined_player != nullptr) {
                unlogined_player->SetName(name);
                unlogined_player->SetLastControlledCritterId(ident_t {1});
            }

            return unlogined_player;
        };

        auto create_func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestCreateUnloginedPlayerHelper"));
        REQUIRE(create_func);
        REQUIRE(create_func.Call());
        REQUIRE(create_func.GetResult() == 0);

        auto login_new_func = server->FindFunc<int32_t, Player*>(get_func("ScriptMethodsTest::TestLoginPlayerToNewRecordFromPreparedPlayer"));
        REQUIRE(login_new_func);

        Player* new_unlogined = create_unlogined_player("ScriptLoginNew");
        REQUIRE(new_unlogined != nullptr);

        auto new_player_cleanup = scope_exit([&new_unlogined]() noexcept {
            safe_call([&new_unlogined] {
                if (new_unlogined != nullptr && !new_unlogined->IsDestroyed()) {
                    new_unlogined->GetConnection()->HardDisconnect();
                }
            });
        });

        REQUIRE(login_new_func.Call(new_unlogined));
        REQUIRE(login_new_func.GetResult() == 0);

        const auto player_id = new_unlogined->GetId();
        REQUIRE(player_id);

        auto reconnect_func = server->FindFunc<int32_t, Player*, ident_t>(get_func("ScriptMethodsTest::TestLoginPlayerToExistentRecordFromPreparedPlayer"));
        REQUIRE(reconnect_func);

        Player* reconnect_unlogined = create_unlogined_player("ScriptLoginReconnect");
        REQUIRE(reconnect_unlogined != nullptr);

        REQUIRE(reconnect_func.Call(reconnect_unlogined, player_id));
        CHECK(reconnect_func.GetResult() == 0);

        auto temp_func = server->FindFunc<int32_t, Player*>(get_func("ScriptMethodsTest::TestLoginPlayerToTempSessionFromPreparedPlayer"));
        REQUIRE(temp_func);

        Player* temp_unlogined = create_unlogined_player("ScriptLoginTemp");
        REQUIRE(temp_unlogined != nullptr);

        REQUIRE(temp_func.Call(temp_unlogined));
        CHECK(temp_func.GetResult() == 0);
    }

    SECTION("RuntimeHelpers")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestRuntimeHelpers"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("EntityPlainDataAccessors")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestEntityPlainDataAccessors"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("PropertyInfoHelpers")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestPropertyInfoHelpers"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("PropertyInfoNoneThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestPropertyInfoNoneThrows", "'None' is not valid property entry in this context");
    }

    SECTION("EntityAccessorsInvalidPropertyThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestEntityAccessorsInvalidPropertyThrows", "Property invalid enum");
    }

    SECTION("EntityAccessorGuardThrows")
    {
        run_throwing_func("ScriptMethodsTest::TestEntityGetAsIntNonPlainPropertyThrows", "Property in not plain data type");
        run_throwing_func("ScriptMethodsTest::TestEntitySetAsIntNonPlainPropertyThrows", "Property in not plain data type");
        run_throwing_func("ScriptMethodsTest::TestEntityGetAsAnyNonPlainPropertyThrows", "Property in not plain data type");
        run_throwing_func("ScriptMethodsTest::TestEntitySetAsAnyNonPlainPropertyThrows", "Property in not plain data type");
        run_throwing_func("ScriptMethodsTest::TestEntitySetAsIntImmutablePropertyThrows", "Property is not mutable");
        run_throwing_func("ScriptMethodsTest::TestEntitySetAsAnyImmutablePropertyThrows", "Property is not mutable");
    }

    SECTION("PropertyCallbackGuards")
    {
        run_throwing_func("ScriptMethodsTest::TestPropertyGetterNoneThrows", "'None' is not valid property entry in this context");
        run_throwing_func("ScriptMethodsTest::TestPropertyGetterNonVirtualThrows", "Property is not virtual");
        run_throwing_func("ScriptMethodsTest::TestPropertySetterNoneThrows", "'None' is not valid property entry in this context");
        run_throwing_func("ScriptMethodsTest::TestPropertySetterInvalidFunctionThrows", "Invalid setter function");
    }

    SECTION("PropertySetterCallback")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestPropertySetterCallback"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("PropertySetterTransformCallback")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestPropertySetterTransformCallback"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("SyncHelpers")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestSyncHelpers"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    SECTION("RegistryCounts")
    {
        auto func = server->FindFunc<int32_t>(get_func("ScriptMethodsTest::TestRegistryCounts"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

FO_END_NAMESPACE
