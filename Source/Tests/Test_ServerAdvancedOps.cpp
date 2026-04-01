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

        return BakerTests::CompileInlineScripts(&compiler_engine, "AdvOpsScripts",
            {
                {"Scripts/AdvOps.fos", R"(
namespace AdvOps
{
    // ========== Location Operations ==========

    int TestCreateLocation()
    {
        // Create location with no maps
        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        // Check basic properties
        if (loc.IsDestroyed) return -2;
        if (loc.Id.value == 0) return -3;

        // Location should have the correct proto
        hstring pid = loc.ProtoId;
        if (pid != "TestLocation".hstr()) return -4;

        return 0;
    }

    int TestLocationLifecycle()
    {
        // Create and destroy
        Location@ loc1 = Game.CreateLocation("TestLocation".hstr());
        if (loc1 is null) return -1;

        int64 id1 = loc1.Id.value;

        Location@ loc2 = Game.CreateLocation("TestLocation".hstr());
        if (loc2 is null) return -2;

        // Two distinct locations
        if (loc1 is loc2) return -3;
        if (loc1.Id.value == loc2.Id.value) return -4;

        // Destroy one
        Game.DestroyLocation(loc1);
        if (!loc1.IsDestroyed) return -5;

        // Second should still be valid
        if (loc2.IsDestroyed) return -6;

        Game.DestroyLocation(loc2);
        if (!loc2.IsDestroyed) return -7;

        return 0;
    }

    int TestDestroyLocationById()
    {
        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        ident locId = loc.Id;
        Game.DestroyLocation(locId);
        if (!loc.IsDestroyed) return -2;

        return 0;
    }

    int TestGetLocationById()
    {
        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        ident locId = loc.Id;
        Location@ found = Game.GetLocation(locId);
        if (found is null) return -2;
        if (!(found is loc)) return -3;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Advanced Entity Manager ==========

    int TestEntityCounts()
    {
        // Create some entities
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);

        if (cr1 is null || cr2 is null) return -1;

        // Get all NPCs
        array<Critter@> allNpc = Game.GetAllNpc();
        if (allNpc.length() < 2) return -2;

        // Get all NPCs by proto
        array<Critter@> npcByProto = Game.GetAllNpc("TestCritter".hstr());
        if (npcByProto.length() < 2) return -3;

        // Destroy and verify count decreases
        int beforeCount = Game.GetAllNpc().length();
        Game.DestroyCritter(cr1);
        int afterCount = Game.GetAllNpc().length();
        if (afterCount != beforeCount - 1) return -4;

        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestBulkDestroyCritters()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr3 = Game.CreateCritter("TestCritter".hstr(), false);

        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        array<Critter@> toDestroy = {cr1, cr2, cr3};
        Game.DestroyCritters(toDestroy);

        if (!cr1.IsDestroyed || !cr2.IsDestroyed || !cr3.IsDestroyed) return -2;

        return 0;
    }

    int TestBulkDestroyItems()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item1 = cr.AddItem("TestItem".hstr(), 1);
        Item@ item2 = cr.AddItem("TestItem".hstr(), 1);
        Item@ item3 = cr.AddItem("TestItem".hstr(), 1);

        if (item1 is null || item2 is null || item3 is null) return -2;

        array<Item@> toDestroy = {item1, item2, item3};
        Game.DestroyItems(toDestroy);

        if (!item1.IsDestroyed || !item2.IsDestroyed || !item3.IsDestroyed) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestGetCritterById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        ident crId = cr.Id;
        Critter@ found = Game.GetCritter(crId);
        if (found is null) return -2;
        if (!(found is cr)) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Advanced Critter Operations ==========

    int TestCritterCondition()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Default state is alive
        if (!cr.IsAlive()) return -2;
        if (cr.IsDead()) return -3;
        if (cr.IsKnockout()) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterDirection()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Get initial direction
        uint8 dir = cr.Dir;

        // Set direction via script
        cr.SetDir(1);
        if (cr.Dir != 1) return -2;

        cr.SetDir(3);
        if (cr.Dir != 3) return -3;

        cr.SetDir(0);
        if (cr.Dir != 0) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterMultipleItems()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Add multiple items
        Item@ item1 = cr.AddItem("TestItem".hstr(), 5);
        Item@ item2 = cr.AddItem("TestItem".hstr(), 3);

        if (item1 is null || item2 is null) return -2;

        // Check total count
        int count = cr.CountItem("TestItem".hstr());
        if (count < 2) return -3;

        // Get all items
        array<Item@> items = cr.GetItems();
        if (items.length() < 2) return -4;

        // Destroy items via Game API
        Game.DestroyItem(item1);
        if (!item1.IsDestroyed) return -6;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterItemByPid()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        // Find item by pid
        Item@ found = cr.GetItem("TestItem".hstr());
        if (found is null) return -3;
        if (!(found is item)) return -4;

        // Count by pid
        int count = cr.CountItem("TestItem".hstr());
        if (count != 1) return -5;

        // Has items check
        if (cr.GetItems().length() == 0) return -6;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterItemById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident itemId = item.Id;
        Item@ found = cr.GetItem(itemId);
        if (found is null) return -3;
        if (!(found is item)) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Item Operations ==========

    int TestItemCreation()
    {
        // Create item via critter
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;
        if (item.IsDestroyed) return -3;
        if (item.Id.value == 0) return -4;

        hstring pid = item.ProtoId;
        if (pid != "TestItem".hstr()) return -5;

        Game.DestroyItem(item);
        if (!item.IsDestroyed) return -6;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemDestroyById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident itemId = item.Id;
        Game.DestroyItem(itemId);
        if (!item.IsDestroyed) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemDestroyPartial()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 10);

        // Destroy some by pid+count
        cr.DestroyItem("TestItem".hstr(), 3);

        // Should still have some remaining
        int remaining = cr.CountItem("TestItem".hstr());
        if (remaining != 7) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemGetById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        ident id = item.Id;
        Item@ found = Game.GetItem(id);
        if (found is null) return -3;
        if (!(found is item)) return -4;

        Game.DestroyItem(item);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestMoveItemBetweenCritters()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item@ item = cr1.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        // Move item from cr1 to cr2
        Game.MoveItem(item, cr2);

        // cr1 should have no items
        if (cr1.GetItems().length() != 0) return -3;

        // cr2 should have the item
        if (cr2.GetItems().length() == 0) return -4;
        if (cr2.CountItem("TestItem".hstr()) != 1) return -5;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestMoveItemPartial()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item@ item = cr1.AddItem("TestItem".hstr(), 10);
        if (item is null) return -2;

        // Move partial count
        Game.MoveItem(item, 5, cr2);

        // Both should have items
        if (cr1.GetItems().length() == 0) return -3;
        if (cr2.GetItems().length() == 0) return -4;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    // ========== Player ID Operations ==========

    int TestPlayerLookup()
    {
        // GetPlayer returns null for non-logged-in players
        Player@ p = Game.GetPlayer("NonexistentPlayer");
        if (p !is null) return -1;

        return 0;
    }

    // ========== Server Info ==========

    int TestServerRandom()
    {
        // Test that Random returns values in range
        for (int i = 0; i < 50; i++)
        {
            int val = Game.Random(1, 100);
            if (val < 1 || val > 100) return -1;
        }
        return 0;
    }

    int TestServerGetUnixTimeAlt()
    {
        uint32 t1 = Game.GetUnixTime();
        uint32 t2 = Game.GetUnixTime();
        // Time should not go backwards
        if (t2 < t1) return -1;
        return 0;
    }

    // ========== Hstring Operations ==========

    int TestHstringEquality()
    {
        hstring h1 = "TestHash".hstr();
        hstring h2 = "TestHash".hstr();
        hstring h3 = "OtherHash".hstr();

        if (h1 != h2) return -1;
        if (h1 == h3) return -2;
        if (h1 == EMPTY_HSTRING) return -3;

        return 0;
    }

    int TestHstringEmpty()
    {
        hstring empty;
        if (empty != EMPTY_HSTRING) return -1;

        hstring nonEmpty = "NonEmpty".hstr();
        if (nonEmpty == EMPTY_HSTRING) return -2;

        return 0;
    }

    // ========== More Array Operations ==========

    int TestArraySorting()
    {
        array<int> arr = {5, 2, 8, 1, 9, 3};

        arr.sortAsc();
        if (arr[0] != 1 || arr[1] != 2 || arr[2] != 3 || arr[3] != 5 || arr[4] != 8 || arr[5] != 9) return -1;

        arr.sortDesc();
        if (arr[0] != 9 || arr[1] != 8 || arr[2] != 5 || arr[3] != 3 || arr[4] != 2 || arr[5] != 1) return -2;

        return 0;
    }

    int TestArrayInsertRemove()
    {
        array<int> arr = {};

        arr.insertLast(10);
        arr.insertLast(20);
        arr.insertLast(30);
        if (arr.length() != 3) return -1;

        arr.insertFirst(5);
        if (arr[0] != 5) return -2;
        if (arr.length() != 4) return -3;

        arr.insertAt(2, 15);
        if (arr[2] != 15) return -4;
        if (arr.length() != 5) return -5;

        arr.removeFirst();
        if (arr[0] != 10) return -6;

        arr.removeLast();
        if (arr.length() != 3) return -7;

        arr.removeAt(1);
        if (arr.length() != 2) return -8;

        return 0;
    }

    int TestArrayFind()
    {
        array<int> arr = {10, 20, 30, 40, 50};

        int idx = arr.find(30);
        if (idx != 2) return -1;

        int missing = arr.find(99);
        if (missing != -1) return -2;

        return 0;
    }

    int TestArrayReverse()
    {
        array<int> arr = {1, 2, 3, 4, 5};
        arr.reverse();

        if (arr[0] != 5 || arr[1] != 4 || arr[2] != 3 || arr[3] != 2 || arr[4] != 1) return -1;

        return 0;
    }

    int TestArrayRemoveRange()
    {
        array<int> arr = {1, 2, 3, 4, 5, 6};

        arr.removeRange(1, 3);
        // Should remove indices 1,2,3 -> leaves {1, 5, 6}
        if (arr.length() != 3) return -1;
        if (arr[0] != 1 || arr[1] != 5 || arr[2] != 6) return -2;

        return 0;
    }

    int TestArrayRemoveAll()
    {
        array<int> arr = {1, 2, 3, 2, 4, 2};

        arr.removeAll(2);
        if (arr.length() != 3) return -1;
        if (arr.find(2) != -1) return -2;

        return 0;
    }

    int TestArrayResize()
    {
        array<int> arr = {};
        arr.resize(5);
        if (arr.length() != 5) return -1;

        // Default value should be 0
        for (int i = 0; i < arr.length(); i++)
        {
            if (arr[i] != 0) return -2;
        }

        arr.resize(2);
        if (arr.length() != 2) return -3;

        return 0;
    }

    int TestStringArrayOperations()
    {
        array<string> arr = {"hello", "world", "test"};

        arr.sortAsc();
        if (arr[0] != "hello" || arr[1] != "test" || arr[2] != "world") return -1;

        arr.sortDesc();
        if (arr[0] != "world" || arr[1] != "test" || arr[2] != "hello") return -2;

        int idx = arr.find("test");
        if (idx != 1) return -3;

        arr.reverse();
        if (arr[0] != "hello" || arr[1] != "test" || arr[2] != "world") return -4;

        return 0;
    }

    // ========== Dict Operations ==========

    int TestDictSetIfNotExist()
    {
        dict<string, int> d = {};

        d.setIfNotExist("key1", 100);
        if (d.get("key1") != 100) return -1;

        // Should not overwrite
        d.setIfNotExist("key1", 200);
        if (d.get("key1") != 100) return -2;

        return 0;
    }

    int TestDictClone()
    {
        dict<string, int> d = {};
        d.set("a", 1);
        d.set("b", 2);

        dict<string, int> clone = d.clone();
        if (clone.length() != 2) return -1;
        if (clone.get("a") != 1) return -2;
        if (clone.get("b") != 2) return -3;

        // Modify clone - original should be unaffected
        clone.set("c", 3);
        if (d.length() != 2) return -4;

        return 0;
    }

    int TestDictRemove()
    {
        dict<string, int> d = {};
        d.set("a", 1);
        d.set("b", 2);
        d.set("c", 3);

        d.remove("b");
        if (d.length() != 2) return -1;
        if (d.exists("b")) return -2;

        d.clear();
        if (d.length() != 0) return -3;
        if (!d.isEmpty()) return -4;

        return 0;
    }

    int TestDictWithEntities()
    {
        dict<ident, string> d = {};

        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        d.set(cr1.Id, "first");
        d.set(cr2.Id, "second");

        if (d.length() != 2) return -2;
        if (d.get(cr1.Id) != "first") return -3;
        if (d.get(cr2.Id) != "second") return -4;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestDictIntKeys()
    {
        dict<int, string> d = {};
        d.set(1, "one");
        d.set(2, "two");
        d.set(3, "three");

        if (d.length() != 3) return -1;
        if (d.get(1) != "one") return -2;
        if (d.get(2) != "two") return -3;

        // Remove by key
        d.remove(2);
        if (d.length() != 2) return -4;
        if (d.exists(2)) return -5;

        return 0;
    }

    // ========== String Operations ==========

    int TestStringTrim()
    {
        string s = "  hello  ";
        string trimmed = s.trim();
        if (trimmed != "hello") return -1;

        // trim() default only strips spaces; pass explicit chars for tabs/newlines
        string s2 = "  \t  test  \n  ";
        string trimmed2 = s2.trim(" \t\n\r");
        if (trimmed2 != "test") return -2;

        return 0;
    }

    int TestStringCase()
    {
        string s = "Hello World";

        if (s.lower() != "hello world") return -1;
        if (s.upper() != "HELLO WORLD") return -2;

        return 0;
    }

    int TestStringStartsEndsWith()
    {
        string s = "Hello World";

        if (!s.startsWith("Hello")) return -1;
        if (s.startsWith("World")) return -2;
        if (!s.endsWith("World")) return -3;
        if (s.endsWith("Hello")) return -4;

        return 0;
    }

    int TestStringSubstring()
    {
        string s = "Hello World";

        string sub = s.substr(6);
        if (sub != "World") return -1;

        string sub2 = s.substr(0, 5);
        if (sub2 != "Hello") return -2;

        return 0;
    }

    int TestStringReplace()
    {
        string s = "Hello World Hello";

        string replaced = s.replace("Hello", "Bye");
        if (replaced != "Bye World Bye") return -1;

        // Replace with empty
        string removed = s.replace("World ", "");
        if (removed != "Hello Hello") return -2;

        return 0;
    }

    int TestStringSplit()
    {
        string s = "a,b,c,d";

        array<string> parts = s.split(",");
        if (parts.length() != 4) return -1;
        if (parts[0] != "a" || parts[1] != "b" || parts[2] != "c" || parts[3] != "d") return -2;

        // Split with multiple separators
        string s2 = "one::two::three";
        array<string> parts2 = s2.split("::");
        if (parts2.length() != 3) return -3;
        if (parts2[0] != "one" || parts2[1] != "two" || parts2[2] != "three") return -4;

        return 0;
    }

    int TestStringJoin()
    {
        array<string> parts = {"a", "b", "c"};
        string joined = ",".join(parts);
        if (joined != "a,b,c") return -1;

        string joined2 = " - ".join(parts);
        if (joined2 != "a - b - c") return -2;

        // Empty separator
        string joined3 = "".join(parts);
        if (joined3 != "abc") return -3;

        return 0;
    }

    int TestStringConversionsAdv()
    {
        // toInt
        if ("0".toInt() != 0) return -1;
        if ("-100".toInt() != -100) return -2;
        if ("999999".toInt() != 999999) return -3;

        // toFloat
        float f = "3.14159".toFloat();
        if (f < 3.14f || f > 3.15f) return -4;

        // Concatenation
        string s = "abc" + "def";
        if (s != "abcdef") return -5;

        // Length
        string s2 = "hello";
        if (s2.length() != 5) return -6;

        return 0;
    }

    // ========== Math Operations ==========

    int TestRandomRange()
    {
        // Test that Random stays within bounds
        for (int i = 0; i < 100; i++)
        {
            int val = Game.Random(1, 10);
            if (val < 1 || val > 10) return -1;
        }

        // Single value range
        int single = Game.Random(5, 5);
        if (single != 5) return -2;

        return 0;
    }

    // ========== Critter Query Operations ==========

    int TestGetAllNpcOverloads()
    {
        // Create some critters
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // GetAllNpc()
        array<Critter@> all = Game.GetAllNpc();
        if (all.length() < 2) return -2;

        // GetAllNpc(pid)
        array<Critter@> byPid = Game.GetAllNpc("TestCritter".hstr());
        if (byPid.length() < 2) return -3;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    // ========== Entity Persistence Operations ==========

    int TestEntityPersistence()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Initially not persistent (for test critters)
        bool persistent = Game.DbHasEntity(cr);
        // Just exercise the API, don't check result as it depends on config

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Database Operations ==========

    int TestDatabaseHasRecordAdv()
    {
        hstring table = "TestTable".hstr();
        ident fakeId;
        fakeId.value = 999999;

        // Query non-existent record
        bool hasRecord = Game.DbHasRecord(table, fakeId);
        if (hasRecord) return -1;

        return 0;
    }

    int TestDatabaseGetAllRecordIds()
    {
        hstring table = "TestTable".hstr();

        // Get all records from non-existent table
        array<ident> ids = Game.DbGetAllRecordIds(table);
        // Should return empty array
        if (ids.length() != 0) return -1;

        return 0;
    }

    // ========== Geometry/Direction Operations ==========

    int TestGetDirection()
    {
        // Test hex direction calculation
        // These are basic tests that the geometry functions work
        mpos from = mpos(10, 10);
        mpos to = mpos(10, 11);

        // Just exercise the API
        uint8 dir = Game.GetDirection(from, to);
        // Direction should be valid (0-5 in hexagonal, 0-7 in square)
        if (dir > 7) return -1;

        return 0;
    }

    int TestGetDistance()
    {
        mpos p1 = mpos(10, 10);
        mpos p2 = mpos(10, 10);

        // Same position should be distance 0
        int32 dist = Game.GetDistance(p1, p2);
        if (dist != 0) return -1;

        // Adjacent hexes should be distance 1
        mpos p3 = mpos(10, 11);
        int32 dist2 = Game.GetDistance(p1, p3);
        if (dist2 != 1) return -2;

        return 0;
    }

    // ========== Time Operations ==========

    int TestUnixTime()
    {
        uint32 t = Game.GetUnixTime();
        // Should be a reasonable timestamp (after year 2000)
        if (t < 946684800) return -1;

        return 0;
    }

    int TestTimePacking()
    {
        // Pack current time
        uint32 t = Game.GetUnixTime();

        nanotime nt = nanotime(int64(t), 3);
        // Just verify it doesn't crash
        return 0;
    }

    // ========== Proto Access ==========

    int TestGetProtoCritter()
    {
        ProtoCritter@ proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -1;
        return 0;
    }

    int TestGetProtoItem()
    {
        ProtoItem@ proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -1;
        return 0;
    }

    int TestGetProtoLocation()
    {
        ProtoLocation@ proto = Game.GetProtoLocation("TestLocation".hstr());
        if (proto is null) return -1;
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

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("AdvOpsCompilerResources");
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

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("AdvOpsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("AdvOpsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("AdvOpsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("AdvOpsLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("AdvOps.fos-bin-server", script_blob);

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

#define MAKE_SERVER() \
    auto settings = MakeSettings(); \
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources()); \
    auto shutdown = scope_exit([&server]() noexcept { \
        safe_call([&server] { \
            if (server->IsStarted()) { \
                server->Shutdown(); \
            } \
        }); \
    }); \
    const auto startup_error = WaitForStart(server.get()); \
    INFO(startup_error); \
    REQUIRE(startup_error.empty()); \
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}})); \
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); }); \
    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); }

#define RUN_SCRIPT_FUNC(func_name) \
    auto func = server->FindFunc<int32>(get_func("AdvOps::" func_name)); \
    REQUIRE(func); \
    REQUIRE(func.Call()); \
    CHECK(func.GetResult() == 0)

// ========== Location Tests ==========

TEST_CASE("LocationCreation")
{
    MAKE_SERVER();

    SECTION("Basic")
    {
        RUN_SCRIPT_FUNC("TestCreateLocation");
    }

    SECTION("Lifecycle")
    {
        RUN_SCRIPT_FUNC("TestLocationLifecycle");
    }

    SECTION("DestroyById")
    {
        RUN_SCRIPT_FUNC("TestDestroyLocationById");
    }

    SECTION("GetById")
    {
        RUN_SCRIPT_FUNC("TestGetLocationById");
    }
}

// ========== Entity Manager Tests ==========

TEST_CASE("AdvancedEntityManager")
{
    MAKE_SERVER();

    SECTION("EntityCounts")
    {
        RUN_SCRIPT_FUNC("TestEntityCounts");
    }

    SECTION("BulkDestroyCritters")
    {
        RUN_SCRIPT_FUNC("TestBulkDestroyCritters");
    }

    SECTION("BulkDestroyItems")
    {
        RUN_SCRIPT_FUNC("TestBulkDestroyItems");
    }

    SECTION("GetCritterById")
    {
        RUN_SCRIPT_FUNC("TestGetCritterById");
    }
}

// ========== Critter Operations ==========

TEST_CASE("AdvancedCritterOperations")
{
    MAKE_SERVER();

    SECTION("Condition")
    {
        RUN_SCRIPT_FUNC("TestCritterCondition");
    }

    SECTION("Direction")
    {
        RUN_SCRIPT_FUNC("TestCritterDirection");
    }

    SECTION("MultipleItems")
    {
        RUN_SCRIPT_FUNC("TestCritterMultipleItems");
    }

    SECTION("ItemByPid")
    {
        RUN_SCRIPT_FUNC("TestCritterItemByPid");
    }

    SECTION("ItemById")
    {
        RUN_SCRIPT_FUNC("TestCritterItemById");
    }
}

// ========== Item Operations ==========

TEST_CASE("AdvancedItemOperations")
{
    MAKE_SERVER();

    SECTION("Creation")
    {
        RUN_SCRIPT_FUNC("TestItemCreation");
    }

    SECTION("DestroyById")
    {
        RUN_SCRIPT_FUNC("TestItemDestroyById");
    }

    SECTION("DestroyPartial")
    {
        RUN_SCRIPT_FUNC("TestItemDestroyPartial");
    }

    SECTION("GetById")
    {
        RUN_SCRIPT_FUNC("TestItemGetById");
    }

    SECTION("MoveBetweenCritters")
    {
        RUN_SCRIPT_FUNC("TestMoveItemBetweenCritters");
    }

    SECTION("MovePartial")
    {
        RUN_SCRIPT_FUNC("TestMoveItemPartial");
    }
}

// ========== Player/Health/Text ==========

TEST_CASE("ServerUtilityOperations")
{
    MAKE_SERVER();

    SECTION("PlayerLookup")
    {
        RUN_SCRIPT_FUNC("TestPlayerLookup");
    }

    SECTION("ServerRandom")
    {
        RUN_SCRIPT_FUNC("TestServerRandom");
    }

    SECTION("ServerGetUnixTimeAlt")
    {
        RUN_SCRIPT_FUNC("TestServerGetUnixTimeAlt");
    }

    SECTION("DatabaseHasRecord")
    {
        RUN_SCRIPT_FUNC("TestDatabaseHasRecordAdv");
    }

    SECTION("DatabaseGetAllRecordIds")
    {
        RUN_SCRIPT_FUNC("TestDatabaseGetAllRecordIds");
    }
}

// ========== Hstring Operations ==========

TEST_CASE("HstringOperations")
{
    MAKE_SERVER();

    SECTION("Equality")
    {
        RUN_SCRIPT_FUNC("TestHstringEquality");
    }

    SECTION("Empty")
    {
        RUN_SCRIPT_FUNC("TestHstringEmpty");
    }
}

// ========== Extended Array Tests ==========

TEST_CASE("ExtendedArrayOperations")
{
    MAKE_SERVER();

    SECTION("Sorting")
    {
        RUN_SCRIPT_FUNC("TestArraySorting");
    }

    SECTION("InsertRemove")
    {
        RUN_SCRIPT_FUNC("TestArrayInsertRemove");
    }

    SECTION("Find")
    {
        RUN_SCRIPT_FUNC("TestArrayFind");
    }

    SECTION("Reverse")
    {
        RUN_SCRIPT_FUNC("TestArrayReverse");
    }

    SECTION("RemoveRange")
    {
        RUN_SCRIPT_FUNC("TestArrayRemoveRange");
    }

    SECTION("RemoveAll")
    {
        RUN_SCRIPT_FUNC("TestArrayRemoveAll");
    }

    SECTION("Resize")
    {
        RUN_SCRIPT_FUNC("TestArrayResize");
    }

    SECTION("StringArray")
    {
        RUN_SCRIPT_FUNC("TestStringArrayOperations");
    }
}

// ========== Extended Dict Tests ==========

TEST_CASE("ExtendedDictOperations")
{
    MAKE_SERVER();

    SECTION("SetIfNotExist")
    {
        RUN_SCRIPT_FUNC("TestDictSetIfNotExist");
    }

    SECTION("Clone")
    {
        RUN_SCRIPT_FUNC("TestDictClone");
    }

    SECTION("Remove")
    {
        RUN_SCRIPT_FUNC("TestDictRemove");
    }

    SECTION("WithEntities")
    {
        RUN_SCRIPT_FUNC("TestDictWithEntities");
    }

    SECTION("IntKeys")
    {
        RUN_SCRIPT_FUNC("TestDictIntKeys");
    }
}

// ========== Extended String Tests ==========

TEST_CASE("ExtendedStringOperations")
{
    MAKE_SERVER();

    SECTION("Trim")
    {
        RUN_SCRIPT_FUNC("TestStringTrim");
    }

    SECTION("Case")
    {
        RUN_SCRIPT_FUNC("TestStringCase");
    }

    SECTION("StartsEndsWith")
    {
        RUN_SCRIPT_FUNC("TestStringStartsEndsWith");
    }

    SECTION("Substring")
    {
        RUN_SCRIPT_FUNC("TestStringSubstring");
    }

    SECTION("Replace")
    {
        RUN_SCRIPT_FUNC("TestStringReplace");
    }

    SECTION("Split")
    {
        RUN_SCRIPT_FUNC("TestStringSplit");
    }

    SECTION("Join")
    {
        RUN_SCRIPT_FUNC("TestStringJoin");
    }

    SECTION("Conversions")
    {
        RUN_SCRIPT_FUNC("TestStringConversionsAdv");
    }
}

// ========== Math/Geometry Tests ==========

TEST_CASE("MathGeometryOperations")
{
    MAKE_SERVER();

    SECTION("Random")
    {
        RUN_SCRIPT_FUNC("TestRandomRange");
    }

    SECTION("Direction")
    {
        RUN_SCRIPT_FUNC("TestGetDirection");
    }

    SECTION("Distance")
    {
        RUN_SCRIPT_FUNC("TestGetDistance");
    }
}

// ========== Time/Misc Tests ==========

TEST_CASE("TimeAndMiscOperations")
{
    MAKE_SERVER();

    SECTION("UnixTime")
    {
        RUN_SCRIPT_FUNC("TestUnixTime");
    }

    SECTION("TimePacking")
    {
        RUN_SCRIPT_FUNC("TestTimePacking");
    }
}

// ========== NPC Query Tests ==========

TEST_CASE("NpcQueryOperations")
{
    MAKE_SERVER();

    SECTION("GetAllNpc")
    {
        RUN_SCRIPT_FUNC("TestGetAllNpcOverloads");
    }
}

// ========== Proto Access Tests ==========

TEST_CASE("ProtoAccessOperations")
{
    MAKE_SERVER();

    SECTION("ProtoCritter")
    {
        RUN_SCRIPT_FUNC("TestGetProtoCritter");
    }

    SECTION("ProtoItem")
    {
        RUN_SCRIPT_FUNC("TestGetProtoItem");
    }

    SECTION("ProtoLocation")
    {
        RUN_SCRIPT_FUNC("TestGetProtoLocation");
    }
}

FO_END_NAMESPACE
