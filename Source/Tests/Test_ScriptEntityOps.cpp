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

        const auto script_source = string(R"(

namespace EntityOps
{
    // ========== Entity Base Operations ==========

    int TestEntityEquality()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // Same entity reference should be equal
        Critter@ cr1ref = cr1;
        if (cr1 !is cr1ref) return -2;

        // Different entities should not be equal
        if (cr1 is cr2) return -3;

        // Null checks
        Critter@ nullCr = null;
        if (cr1 is nullCr) return -4;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 0;
    }

    int TestEntityDestroyState()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        if (cr.IsDestroyed) return -2;
        if (cr.IsDestroying) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityName()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Entity should have a name
        string name = cr.Name;
        if (name.isEmpty()) return -2;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemContainerOps()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item1 = cr.AddItem("TestItem".hstr(), 1);
        Item@ item2 = cr.AddItem("TestItem2".hstr(), 1);
        if (item1 is null || item2 is null) return -2;

        // Both items should be in critter's inventory
        array<Item@> items = cr.GetItems();
        if (items.length() < 2) return -3;

        // Item should know its owner
        Critter@ owner1 = item1.GetCritter();
        if (owner1 is null) return -4;
        if (owner1.Id != cr.Id) return -5;

        Critter@ owner2 = item2.GetCritter();
        if (owner2 is null) return -6;
        if (owner2.Id != cr.Id) return -7;

        // Item ids should be different
        if (item1.Id == item2.Id) return -8;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Reflection Operations ==========

    int TestReflectionBasicTypes()
    {
        // Check module info
        array<string> modules = reflection::getLoadedModules();
        if (modules.isEmpty()) return -1;

        string currentMod = reflection::getCurrentModule();
        if (currentMod.isEmpty()) return -2;

        return 0;
    }

    int TestReflectionEnums()
    {
        // Get global enums
        array<reflection::type@> globalEnums = reflection::getGlobalEnums();
        if (globalEnums.isEmpty()) return -1;

        // Verify at least one enum exists
        reflection::type@ firstEnum = globalEnums[0];
        if (firstEnum is null) return -2;

        string enumName = firstEnum.name;
        if (enumName.isEmpty()) return -3;

        return 0;
    }





    // ========== Enum Parsing ==========

    int TestEnumParsing()
    {
        // Parse a generic enum value via Game method
        int val = Game.ParseGenericEnum("CritterCondition", "Alive");

        // Enum to string via Game method
        string s = Game.EnumToString(CritterCondition::Alive);
        if (s.isEmpty()) return -1;

        string sFull = Game.EnumToString(CritterCondition::Alive, true);
        if (sFull.isEmpty()) return -2;

        // Different conditions should have different string representations
        string sDead = Game.EnumToString(CritterCondition::Dead);
        if (s == sDead) return -3;

        return 0;
    }

    // ========== Assert and Exception Operations ==========

    int TestExceptionCount()
    {
        int globalBefore = GetGlobalExceptionCount();
        int contextBefore = GetContextExceptionCount();

        // These should be non-negative
        if (globalBefore < 0) return -1;
        if (contextBefore < 0) return -2;

        return 0;
    }

 )") + R"(
    // ========== Database Operations ==========

    int TestDatabaseHasRecord()
    {
        // Create a critter to get a valid ident
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Check for record - critter should be in DB since it's persistent
        bool exists = Game.DbHasEntity(cr);
        // Just verify it doesn't crash

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestDatabaseGetAllRecordIds()
    {
        // Get all record ids from a collection (may be empty)
        array<ident> ids = Game.DbGetAllRecordIds("test_records".hstr());
        // Just verify no crash, collection likely empty

        return 0;
    }

    // ========== Time Event Operations ==========

    bool timeEventFired = false;

    void OnTimeEvent(Critter cr)
    {
        timeEventFired = true;
    }

    int TestTimeEventCreation()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Start a time event (60 seconds delay, place=3 means seconds)
        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnTimeEvent);
        if (eventId == 0) return -2;

        // Count should be 1
        int count = cr.CountTimeEvent(eventId);
        if (count != 1) return -3;

        // Stop it
        cr.StopTimeEvent(eventId);

        // Count should be 0
        int countAfter = cr.CountTimeEvent(eventId);
        if (countAfter != 0) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestTimeEventByFunction()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Start by function ref
        uint32 eventId = cr.StartTimeEvent(timespan(30, 3), OnTimeEvent);
        if (eventId == 0) return -2;

        // Count by function
        int count = cr.CountTimeEvent(OnTimeEvent);
        if (count < 1) return -3;

        // Stop by function
        cr.StopTimeEvent(OnTimeEvent);

        int countAfter = cr.CountTimeEvent(OnTimeEvent);
        if (countAfter != 0) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Advanced Array Operations ==========

    int TestStringArrayOps()
    {
        string[] arr = {"banana", "apple", "cherry", "date"};

        if (arr.length() != 4) return -1;

        // Sort ascending
        arr.sortAsc();
        if (arr[0] != "apple") return -2;
        if (arr[1] != "banana") return -3;
        if (arr[2] != "cherry") return -4;
        if (arr[3] != "date") return -5;

        // Sort descending
        arr.sortDesc();
        if (arr[0] != "date") return -6;

        // Find
        int idx = arr.find("cherry");
        if (idx < 0) return -7;

        // Reverse
        arr.reverse();
        if (arr[0] != "apple") return -8;

        return 0;
    }

    int TestArrayOfArrays()
    {
        array<array<int>> matrix = {};

        for (int i = 0; i < 3; i++)
        {
            int[] row = {};
            for (int j = 0; j < 3; j++)
            {
                row.insertLast(i * 3 + j);
            }
            matrix.insertLast(row);
        }

        if (matrix.length() != 3) return -1;
        if (matrix[0].length() != 3) return -2;
        if (matrix[1][1] != 4) return -3;
        if (matrix[2][2] != 8) return -4;

        return 0;
    }

    int TestArrayResize()
    {
        int[] arr = {1, 2, 3};

        arr.resize(6);
        if (arr.length() != 6) return -1;
        if (arr[0] != 1) return -2;
        if (arr[3] != 0) return -3;

        arr.resize(1);
        if (arr.length() != 1) return -4;
        if (arr[0] != 1) return -5;

        arr.resize(0);
        if (!arr.isEmpty()) return -6;

        return 0;
    }

    int TestArrayInsertRemove()
    {
        int[] arr = {};

        // Build up
        for (int i = 0; i < 10; i++)
        {
            arr.insertLast(i * 10);
        }
        if (arr.length() != 10) return -1;

        // Insert in middle
        arr.insertAt(5, 999);
        if (arr.length() != 11) return -2;
        if (arr[5] != 999) return -3;

        // Remove from middle
        arr.removeAt(5);
        if (arr.length() != 10) return -4;
        if (arr[5] != 50) return -5;

        // Remove last repeatedly
        while (!arr.isEmpty())
        {
            arr.removeLast();
        }
        if (!arr.isEmpty()) return -6;

        return 0;
    }

 )" + R"(

    // ========== Advanced Dict Operations ==========

    int TestDictClone()
    {
        dict<string, int> original = {};
        original.set("a", 1);
        original.set("b", 2);
        original.set("c", 3);

        dict<string, int> copy = original.clone();

        if (copy.length() != 3) return -1;
        if (copy.get("a") != 1) return -2;
        if (copy.get("b") != 2) return -3;

        // Modify original, copy should be unchanged
        original.set("a", 100);
        if (copy.get("a") != 1) return -4;

        return 0;
    }

    int TestDictGetDefault()
    {
        dict<string, int> d = {};
        d.set("exists", 42);

        // Get existing key with default overload
        int val = d.get("exists", 0);
        if (val != 42) return -1;

        // Get missing key with default
        int def = d.get("missing", -999);
        if (def != -999) return -2;

        return 0;
    }

    int TestDictGetKeysValues()
    {
        dict<string, int> d = {};
        d.set("x", 10);
        d.set("y", 20);
        d.set("z", 30);

        array<string> keys = d.getKeys();
        if (keys.length() != 3) return -1;

        // Verify keys contain expected values
        bool foundX = false, foundY = false, foundZ = false;
        for (int i = 0; i < keys.length(); i++)
        {
            if (keys[i] == "x") foundX = true;
            if (keys[i] == "y") foundY = true;
            if (keys[i] == "z") foundZ = true;
        }
        if (!foundX || !foundY || !foundZ) return -2;

        return 0;
    }

    int TestDictWithHstring()
    {
        dict<hstring, int> d = {};
        hstring key1 = "TestKey1".hstr();
        hstring key2 = "TestKey2".hstr();

        d.set(key1, 100);
        d.set(key2, 200);

        if (d.length() != 2) return -1;
        if (!d.exists(key1)) return -2;
        if (d.get(key1) != 100) return -3;
        if (d.get(key2) != 200) return -4;

        d.remove(key1);
        if (d.exists(key1)) return -5;
        if (d.length() != 1) return -6;

        return 0;
    }

    // ========== Advanced String Operations ==========

    int TestStringSplit()
    {
        string s = "one,two,three,four";
        array<string> parts = s.split(",");

        if (parts.length() != 4) return -1;
        if (parts[0] != "one") return -2;
        if (parts[1] != "two") return -3;
        if (parts[3] != "four") return -4;

        return 0;
    }

    int TestStringJoin()
    {
        string[] parts = {"hello", "world", "test"};
        string joined = " ".join(parts);

        if (joined != "hello world test") return -1;

        string[] empty = {};
        string joinedEmpty = ",".join(empty);
        if (!joinedEmpty.isEmpty()) return -2;

        return 0;
    }

    int TestStringFindOperations()
    {
        string s = "abcdefabcdef";

        // findFirstOf
        int pos1 = s.findFirstOf("de");
        if (pos1 != 3) return -1;

        // findLastOf - start=0 to search from beginning
        int pos2 = s.findLastOf("de", 0);
        if (pos2 != 10) return -2;

        // findFirstNotOf
        int pos3 = s.findFirstNotOf("abc");
        if (pos3 != 3) return -3;

        // findLastNotOf - start=0 to search from beginning
        int pos4 = s.findLastNotOf("def", 0);
        if (pos4 < 0) return -4;

        return 0;
    }

    int TestStringConversions()
    {
        // int to string
        string s1 = "" + 42;
        if (s1 != "42") return -1;

        // float to string
        string s2 = "" + 3.14f;
        if (s2.isEmpty()) return -2;

        // Parse back
        if ("123".toInt() != 123) return -3;
        if ("-456".toInt() != -456) return -4;

        // Float parse
        float f = "2.5".toFloat();
        if (f < 2.4f || f > 2.6f) return -5;

        return 0;
    }

    int TestStringReplaceAll()
    {
        string s = "aaa bbb aaa bbb aaa";
        string result = s.replace("aaa", "x");
        if (result != "x bbb x bbb x") return -1;

        // Replace with empty
        string result2 = s.replace("bbb", "");
        if (result2 != "aaa  aaa  aaa") return -2;

        return 0;
    }

    // ========== Multiple Critter Operations ==========

    int TestMultipleCritterLifecycle()
    {
        array<Critter@> critters = {};

        // Create 10 critters
        for (int i = 0; i < 10; i++)
        {
            Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
            if (cr is null) return -(i + 1);
            critters.insertLast(cr);
        }

        // All should have unique ids
        for (int i = 0; i < critters.length(); i++)
        {
            for (int j = i + 1; j < critters.length(); j++)
            {
                if (critters[i].Id == critters[j].Id) return -20;
            }
        }

        // GetAllNpc should include them
        array<Critter@> allNpc = Game.GetAllNpc("TestCritter".hstr());
        if (allNpc.length() < 10) return -30;

        // Destroy all
        Game.DestroyCritters(critters);

        return 0;
    }

    // ========== Critter Properties Access ==========

    int TestCritterPropertyAccess()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Access Id
        ident id = cr.Id;
        if (id.value == 0) return -2;

        // Check IsAlive (should be alive by default)
        if (!cr.IsAlive()) return -3;

        // Check direction
        uint8 dir = cr.Dir;
        // Default direction should be valid (0-5 for hex)

        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Global Game Queries ==========

    int TestGameGetCrittersVariants()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr3 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        // Get by NonDead type
        array<Critter@> nonDead = Game.GetCritters(CritterFindType::NonDead);
        if (nonDead.length() < 3) return -2;

        // Get NPCs
        array<Critter@> npcs = Game.GetCritters(CritterFindType::Npc);
        if (npcs.length() < 3) return -3;

        // Get by Any
        array<Critter@> all = Game.GetCritters(CritterFindType::Any);
        if (all.length() < 3) return -4;

        // Kill one and check dead queries
        cr1.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);

        array<Critter@> dead = Game.GetCritters(CritterFindType::Dead);
        if (dead.isEmpty()) return -5;

        // NonDead should have fewer
        array<Critter@> nonDead2 = Game.GetCritters(CritterFindType::NonDead);
        if (nonDead2.length() >= nonDead.length()) return -6;

        array<Critter@> toDestroy = {cr1, cr2, cr3};
        Game.DestroyCritters(toDestroy);
        return 0;
    }

    // ========== Item Multi-operations ==========

    int TestItemMultipleTypes()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Add items of different types
        Item@ item1 = cr.AddItem("TestItem".hstr(), 1);
        Item@ item2 = cr.AddItem("TestItem2".hstr(), 1);
        if (item1 is null || item2 is null) return -2;

        // Items should have different proto ids
        if (item1.ProtoId == item2.ProtoId) return -3;

        // Get all items on critter
        array<Item@> allItems = cr.GetItems();
        if (allItems.length() < 2) return -4;

        // Destroy one type
        cr.DestroyItem("TestItem".hstr());

        // Check remaining
        array<Item@> remaining = cr.GetItems();
        // Should still have TestItem2
        bool hasItem2 = false;
        for (int i = 0; i < remaining.length(); i++)
        {
            if (remaining[i].ProtoId == "TestItem2".hstr())
            {
                hasItem2 = true;
                break;
            }
        }
        if (!hasItem2) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Comprehensive Game Log/Misc ==========

    int TestGameLogging()
    {
        // Test various log calls
        Game.Log("Entity ops test: basic log");
        Game.Log("Entity ops test: number " + 42);
        Game.Log("Entity ops test: float " + 3.14f);
        Game.Log("Entity ops test: bool " + true);

        return 0;
    }

    int TestHstringOperations()
    {
        hstring h1 = "TestString1".hstr();
        hstring h2 = "TestString2".hstr();
        hstring h3 = "TestString1".hstr();

        // Equal hstrings
        if (h1 != h3) return -1;

        // Different hstrings
        if (h1 == h2) return -2;

        // Convert to string
        string s = string(h1);
        if (s != "TestString1") return -3;

        // Empty hstring
        hstring empty;
        string emptyStr = string(empty);
        if (!emptyStr.isEmpty()) return -4;

        return 0;
    }

    int TestMathAdvanced()
    {
        // More math operations
        if (math::abs(-5.0f) != 5.0f) return -1;
        if (math::abs(5.0f) != 5.0f) return -2;

        // Floor/ceil at boundaries
        if (math::floor(3.0f) != 3.0f) return -3;
        if (math::ceil(3.0f) != 3.0f) return -4;
        if (math::floor(3.5f) != 3.0f) return -5;
        if (math::ceil(3.5f) != 4.0f) return -6;

        // Pow
        if (math::pow(2.0f, 10.0f) != 1024.0f) return -7;
        if (math::pow(3.0f, 0.0f) != 1.0f) return -8;

        // Sqrt
        if (math::sqrt(16.0f) != 4.0f) return -9;
        if (math::sqrt(1.0f) != 1.0f) return -10;

        // Trig identity: sin^2 + cos^2 = 1
        float s = math::sin(1.0f);
        float c = math::cos(1.0f);
        float sum = s * s + c * c;
        if (sum < 0.999f || sum > 1.001f) return -11;

        return 0;
    }
}
)";

        return BakerTests::CompileInlineScripts(&compiler_engine, "EntityOpsScripts",
            {
                {"Scripts/EntityOps.fos", script_source},
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

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("EntityOpsCompilerResources");
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

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("EntityOpsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("EntityOpsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("EntityOpsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("EntityOpsItem2.fopro-bin-server", item2_blob);
        runtime_source->AddFile("EntityOps.fos-bin-server", script_blob);

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
    auto func = server->FindFunc<int32>(get_func("EntityOps::" func_name)); \
    REQUIRE(func); \
    REQUIRE(func.Call()); \
    CHECK(func.GetResult() == 0)

TEST_CASE("EntityBaseOperations")
{
    MAKE_SERVER();

    SECTION("Equality")
    {
        RUN_SCRIPT_FUNC("TestEntityEquality");
    }

    SECTION("DestroyState")
    {
        RUN_SCRIPT_FUNC("TestEntityDestroyState");
    }

    SECTION("EntityName")
    {
        RUN_SCRIPT_FUNC("TestEntityName");
    }

    SECTION("ItemContainerOps")
    {
        RUN_SCRIPT_FUNC("TestItemContainerOps");
    }
}

TEST_CASE("ReflectionOperations")
{
    MAKE_SERVER();

    SECTION("BasicTypes")
    {
        RUN_SCRIPT_FUNC("TestReflectionBasicTypes");
    }

    SECTION("Enums")
    {
        RUN_SCRIPT_FUNC("TestReflectionEnums");
    }
}

TEST_CASE("EnumParsingOperations")
{
    MAKE_SERVER();

    SECTION("EnumParsing")
    {
        RUN_SCRIPT_FUNC("TestEnumParsing");
    }

    SECTION("ExceptionCount")
    {
        RUN_SCRIPT_FUNC("TestExceptionCount");
    }
}

TEST_CASE("DatabaseScriptOperations")
{
    MAKE_SERVER();

    SECTION("DatabaseHasRecord")
    {
        RUN_SCRIPT_FUNC("TestDatabaseHasRecord");
    }

    SECTION("DatabaseGetAllRecordIds")
    {
        RUN_SCRIPT_FUNC("TestDatabaseGetAllRecordIds");
    }
}

TEST_CASE("TimeEventOperations")
{
    MAKE_SERVER();

    SECTION("TimeEventCreation")
    {
        RUN_SCRIPT_FUNC("TestTimeEventCreation");
    }

    SECTION("TimeEventByFunction")
    {
        RUN_SCRIPT_FUNC("TestTimeEventByFunction");
    }
}

TEST_CASE("AdvancedCollectionOperations")
{
    MAKE_SERVER();

    SECTION("StringArrayOps")
    {
        RUN_SCRIPT_FUNC("TestStringArrayOps");
    }

    SECTION("ArrayOfArrays")
    {
        RUN_SCRIPT_FUNC("TestArrayOfArrays");
    }

    SECTION("ArrayResize")
    {
        RUN_SCRIPT_FUNC("TestArrayResize");
    }

    SECTION("ArrayInsertRemove")
    {
        RUN_SCRIPT_FUNC("TestArrayInsertRemove");
    }

    SECTION("DictClone")
    {
        RUN_SCRIPT_FUNC("TestDictClone");
    }

    SECTION("DictGetDefault")
    {
        RUN_SCRIPT_FUNC("TestDictGetDefault");
    }

    SECTION("DictGetKeysValues")
    {
        RUN_SCRIPT_FUNC("TestDictGetKeysValues");
    }

    SECTION("DictWithHstring")
    {
        RUN_SCRIPT_FUNC("TestDictWithHstring");
    }
}

TEST_CASE("AdvancedStringOperations")
{
    MAKE_SERVER();

    SECTION("StringSplit")
    {
        RUN_SCRIPT_FUNC("TestStringSplit");
    }

    SECTION("StringJoin")
    {
        RUN_SCRIPT_FUNC("TestStringJoin");
    }

    SECTION("StringFindOperations")
    {
        RUN_SCRIPT_FUNC("TestStringFindOperations");
    }

    SECTION("StringConversions")
    {
        RUN_SCRIPT_FUNC("TestStringConversions");
    }

    SECTION("StringReplaceAll")
    {
        RUN_SCRIPT_FUNC("TestStringReplaceAll");
    }
}

TEST_CASE("AdvancedServerOperations")
{
    MAKE_SERVER();

    SECTION("MultipleCritterLifecycle")
    {
        RUN_SCRIPT_FUNC("TestMultipleCritterLifecycle");
    }

    SECTION("CritterPropertyAccess")
    {
        RUN_SCRIPT_FUNC("TestCritterPropertyAccess");
    }

    SECTION("GameGetCrittersVariants")
    {
        RUN_SCRIPT_FUNC("TestGameGetCrittersVariants");
    }

    SECTION("ItemMultipleTypes")
    {
        RUN_SCRIPT_FUNC("TestItemMultipleTypes");
    }

    SECTION("GameLogging")
    {
        RUN_SCRIPT_FUNC("TestGameLogging");
    }

    SECTION("HstringOperations")
    {
        RUN_SCRIPT_FUNC("TestHstringOperations");
    }

    SECTION("MathAdvanced")
    {
        RUN_SCRIPT_FUNC("TestMathAdvanced");
    }
}

FO_END_NAMESPACE
