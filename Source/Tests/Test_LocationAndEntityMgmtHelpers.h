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

#pragma once

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace LocationAndEntityMgmtTests
{
    inline auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);
        BakerTests::OverrideSetting(settings.DataBase.CustomCollections, vector<string> {"TestCollection:Int"});

        return settings;
    }

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        auto script_source = string(R"(

namespace LocEntity
{
    // ========== Location Tests ==========

    int TestCreateDestroyLocation()
    {
        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        int64 locId = loc.Id.value;
        if (locId == 0) return -2;

        Location? found = Game.GetLocation(loc.Id);
        if (found is null) return -3;

        if (loc.ProtoId != "TestLocation".hstr()) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMultipleLocations()
    {
        Location loc1 = Game.CreateLocation("TestLocation".hstr());
        Location loc2 = Game.CreateLocation("TestLocation".hstr());
        Location loc3 = Game.CreateLocation("TestLocation".hstr());

        if (loc1 is null || loc2 is null || loc3 is null) return -1;

        if (loc1.Id.value == loc2.Id.value) return -2;
        if (loc2.Id.value == loc3.Id.value) return -3;
        if (loc1.Id.value == loc3.Id.value) return -4;

        Game.DestroyLocation(loc3);
        Game.DestroyLocation(loc2);
        Game.DestroyLocation(loc1);
        return 0;
    }

    int TestLocationProperties()
    {
        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        hstring pid = loc.ProtoId;
        if (pid != "TestLocation".hstr()) return -2;

        if (loc.Id.value == 0) return -3;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Entity Manager Tests ==========

    int TestEntityManagerCritterOperations()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr3 = Game.CreateCritter("TestCritter".hstr(), false);

        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        if (cr1.Id.value == cr2.Id.value) return -2;
        if (cr2.Id.value == cr3.Id.value) return -3;

        Critter? found1 = Game.GetCritter(cr1.Id);
        if (found1 is null) return -4;
        if (found1.Id.value != cr1.Id.value) return -5;

        Game.DestroyCritter(cr3);
        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestEntityManagerItemOperations()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item1 = cr.AddItem("TestItem".hstr());
        Item item2 = cr.AddItem("TestItem".hstr());
        Item item3 = cr.AddItem("TestItem".hstr());

        if (item1 is null || item2 is null || item3 is null) return -2;

        Item? found = Game.GetItem(item1.Id);
        if (found is null) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityManagerDestroyEntity()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr());
        if (item is null) return -2;

        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -3;

        Game.DestroyEntity(loc);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityManagerBulkDestroyEntities()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr3 = Game.CreateCritter("TestCritter".hstr(), false);

        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        array<Entity> entities = {cr1, cr2, cr3};
        Game.DestroyEntities(entities);
        return 0;
    }

    // ========== Critter Advanced Operations ==========

    int TestCritterStateQueries()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        bool alive = cr.IsAlive();
        if (!alive) return -2;

        bool knockout = cr.IsKnockout();
        if (knockout) return -3;

        bool dead = cr.IsDead();
        if (dead) return -4;

        bool moving = cr.IsMoving();
        if (moving) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterDirection()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.SetDir(HDIR_NorthEast);
        if (cr.Dir.hex != HDIR_NorthEast) return -2;

        cr.SetDir(HDIR_SouthWest);
        if (cr.Dir.hex != HDIR_SouthWest) return -3;

        cr.SetDir(mdir(90));

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterItemManagement()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item i1 = cr.AddItem("TestItem".hstr());
        if (i1 is null) return -2;

        if (cr.GetItems("TestItem".hstr()).length() != 1) return -3;

        array<Item> items = cr.GetItems();
        if (items.length() == 0) return -4;

        Item? byId = cr.GetItem(i1.Id);
        if (byId is null) return -5;

        Game.DestroyItem(i1);
        if (cr.GetItems("TestItem".hstr()).length() != 0) return -6;

        cr.AddItem("TestItem".hstr());
        if (cr.GetItems("TestItem".hstr()).length() != 1) return -7;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterConditions()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        if (!cr.IsAlive()) return -2;

        cr.SetCondition(CritterCondition::Knockout, CritterActionAnim::None, null);
        if (!cr.IsKnockout()) return -3;
        if (cr.IsAlive()) return -4;

        cr.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);
        if (!cr.IsDead()) return -5;
        if (cr.IsAlive()) return -6;

        cr.SetCondition(CritterCondition::Alive, CritterActionAnim::None, null);
        if (!cr.IsAlive()) return -7;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Item Operations ==========

    int TestItemContainerOperations()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item container = cr.AddItem("TestItem".hstr());
        if (container is null) return -2;

        Item sub1 = container.AddItem("TestItem".hstr());
        Item sub2 = container.AddItem("TestItem".hstr());
        if (sub1 is null || sub2 is null) return -3;

        array<Item> subs = container.GetItems();
        if (subs.length() < 2) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemMoveOperations()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item item = cr1.AddItem("TestItem".hstr());
        if (item is null) return -2;

        Game.MoveItem(item, cr2);

        Item? onCr2 = cr2.GetItem(item.Id);
        if (onCr2 is null) return -3;

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestItemDestroyVariants()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item i1 = cr.AddItem("TestItem".hstr());
        Item i2 = cr.AddItem("TestItem".hstr());
        if (i1 is null || i2 is null) return -2;

        Game.DestroyItem(i2);
        Game.DestroyItem(i1);

        Game.DestroyCritter(cr);
        return 0;
    }

 )") + R"(
    // ========== NPC Queries ==========

    int TestNpcQueries()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr3 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        array<Critter> npcs = Game.GetAllNpc();
        if (npcs.length() < 3) return -2;

        array<Critter> byProto = Game.GetAllNpc("TestCritter".hstr());
        if (byProto.length() < 3) return -3;

        Game.DestroyCritter(cr3);
        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    // ========== Game Utility ==========

    int TestGameRandom()
    {
        for (int i = 0; i < 50; i++)
        {
            int val = Game.Random(1, 100);
            if (val < 1 || val > 100) return -1;
        }

        int same = Game.Random(42, 42);
        if (same != 42) return -2;

        int neg = Game.Random(-10, -5);
        if (neg < -10 || neg > -5) return -3;

        return 0;
    }

    int TestGameGetUnixTime()
    {
        int64 t = Game.GetUnixTime();
        if (t < 1000000000) return -1;

        int64 t2 = Game.GetUnixTime();
        if (t2 < t) return -2;

        return 0;
    }

    int TestHstringOperations()
    {
        hstring h1 = "TestItem".hstr();
        hstring h2 = "TestCritter".hstr();
        hstring h3 = "TestItem".hstr();

        if (h1 != h3) return -1;
        if (h1 == h2) return -2;

        string s = h1.str;
        if (s != "TestItem") return -3;

        hstring empty;
        if (empty == h1) return -4;

        return 0;
    }

    int TestDatabaseQueries()
    {
        ident testId;
        testId.value = 999888777;

        bool has = Game.DbHasRecord("TestCollection".hstr(), testId);
        if (has) return -1;

        return 0;
    }

 )" + R"(
    // ========== Advanced Item Management ==========

    int TestItemMoveToContainer()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item container = cr.AddItem("TestItem".hstr());
        Item loose = cr.AddItem("TestItem".hstr());
        if (container is null || loose is null) return -2;

        Game.MoveItem(loose, container);

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestBulkItemDestruction()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item i1 = cr.AddItem("TestItem".hstr());
        Item i2 = cr.AddItem("TestItem".hstr());
        Item i3 = cr.AddItem("TestItem".hstr());
        if (i1 is null || i2 is null || i3 is null) return -2;

        array<Item> items = {i1, i2, i3};
        Game.DestroyItems(items);

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetPlayer()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Player? player = cr.GetPlayer();
        if (player !is null) return -2;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterAttachments()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        bool attached = cr.IsAttached;
        if (attached) return -2;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Script and Hstring Advanced ==========

    int TestHstringToFromString()
    {
        hstring h1 = "alpha_bravo_charlie".hstr();
        hstring h2 = "delta_echo_foxtrot".hstr();

        if (h1.str != "alpha_bravo_charlie") return -1;
        if (h2.str != "delta_echo_foxtrot") return -2;

        hstring h3 = "alpha_bravo_charlie".hstr();
        if (h1 != h3) return -3;
        if (h1 == h2) return -4;

        return 0;
    }

    int TestArraySortAndSearch()
    {
        array<int> arr = {5, 3, 1, 4, 2};
        arr.sortAsc();
        if (arr[0] != 1 || arr[1] != 2 || arr[2] != 3 || arr[3] != 4 || arr[4] != 5) return -1;

        arr.sortDesc();
        if (arr[0] != 5 || arr[1] != 4 || arr[2] != 3 || arr[3] != 2 || arr[4] != 1) return -2;

        arr.sortAsc();
        int idx = arr.find(3);
        if (idx != 2) return -3;

        int missing = arr.find(99);
        if (missing != -1) return -4;

        return 0;
    }

    int TestDictAdvancedOps()
    {
        dict<string, int> d = {};
        d.set("one", 1);
        d.set("two", 2);
        d.set("three", 3);

        if (d.length() != 3) return -1;

        if (!d.exists("one")) return -2;
        if (d.exists("four")) return -3;

        if (d.get("two") != 2) return -4;

        d.remove("two");
        if (d.length() != 2) return -5;
        if (d.exists("two")) return -6;

        array<string> keys = d.getKeys();
        if (keys.length() != 2) return -7;

        d.clear();
        if (d.length() != 0) return -8;

        return 0;
    }

    int TestStringAdvancedOps()
    {
        string csv = "a,b,c,d,e";
        array<string> parts = csv.split(",");
        if (parts.length() != 5) return -1;

        string rejoined = ",".join(parts);
        if (rejoined != csv) return -2;

        string mixed = "Hello World 123";
        if (mixed.lower() != "hello world 123") return -3;
        if (mixed.upper() != "HELLO WORLD 123") return -4;

        if (mixed.find("World") < 0) return -5;
        if (mixed.find("xyz") >= 0) return -6;

        string replaced = mixed.replace("World", "Universe");
        if (replaced != "Hello Universe 123") return -7;

        string sub = mixed.substr(6, 5);
        if (sub != "World") return -8;

        if (!mixed.startsWith("Hello")) return -9;
        if (!mixed.endsWith("123")) return -10;
        if (mixed.startsWith("xyz")) return -11;
        if (mixed.endsWith("xyz")) return -12;

        return 0;
    }

 )" + R"(
    // ========== Server Entity Queries ==========

    int TestEntityGetById()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr());
        if (item is null) return -2;

        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -3;

        Critter? foundCr = Game.GetCritter(cr.Id);
        if (foundCr is null) return -4;

        Item? foundItem = Game.GetItem(item.Id);
        if (foundItem is null) return -5;

        Location? foundLoc = Game.GetLocation(loc.Id);
        if (foundLoc is null) return -6;

        Game.DestroyLocation(loc);
        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Math Operations ==========

    int TestMathOps()
    {
        mpos p1;
        p1.x = 100;
        p1.y = 100;
        mpos p2;
        p2.x = 105;
        p2.y = 100;

        mdir dir = Game.GetDirection(p1, p2);
        int dist = Game.GetDistance(p1, p2);
        if (dist == 0) return -2;

        int sameDist = Game.GetDistance(p1, p1);
        if (sameDist != 0) return -3;

        return 0;
    }

    int TestTimeOperations()
    {
        // place 2 = milliseconds, place 3 = seconds
        timespan ts1 = timespan(1000, 2);
        timespan ts2 = timespan(1, 3);
        if (ts1 != ts2) return -1;

        nanotime nt = Game.GetPrecisionTime();
        if (nt == ZERO_NANOTIME) return -2;

        return 0;
    }

 )" + R"(
    // ========== Load/Unload Critter ==========

    // Plain void callback resolved by the native time-event tests through Game.FindFunc
    void OnUnloadTimer()
    {
    }

    // Self-entity callbacks the native time-event tests fire through the manager. They record their
    // effect on the critter's LookDistance so C++ can observe that the dispatch really happened
    void OnCritterTickTimer(Entity self)
    {
        Critter cr = cast<Critter>(self);
        cr.LookDistance = cr.LookDistance + 1;
    }

    void OnCritterThrowingTimer(Entity self)
    {
        Critter cr = cast<Critter>(self);
        cr.LookDistance = cr.LookDistance + 1000;
        throw("Time event callback failure for the native dispatch test");
    }

    int TestUnloadCritter()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return -1;

        array<hstring> mapPids = {"TestMap".hstr()};
        Location loc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (loc is null) {
            Game.DestroyCritter(cr);
            return -2;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            Game.DestroyLocation(loc);
            Game.DestroyCritter(cr);
            return -3;
        }

        cr.TransferToMap(map, mpos(20, 20), mdir(0));
        if (cr.MapId.value != map.Id.value) {
            Game.DestroyLocation(loc);
            Game.DestroyCritter(cr);
            return -4;
        }

        cr.MakePersistent(true);

        ident crId = cr.Id;

        Game.UnloadCritter(cr);

        Critter? gone = Game.GetCritter(crId);
        if (gone !is null) {
            Game.DestroyLocation(loc);
            return -5;
        }

        Critter? loaded = Game.LoadCritter(crId, false);
        if (loaded is null) {
            Game.DestroyLocation(loc);
            return -6;
        }
        if (loaded.Id.value != crId.value) {
            Game.DestroyLocation(loc);
            Game.DestroyCritter(loaded);
            return -7;
        }
        if (loaded.MapId.value != 0) {
            Game.DestroyLocation(loc);
            Game.DestroyCritter(loaded);
            return -8;
        }

        Game.DestroyLocation(loc);
        Game.DestroyCritter(loaded);
        return 0;
    }

    int TestMoveItemsBulk()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item i1 = cr1.AddItem("TestItem".hstr());
        Item i2 = cr1.AddItem("TestItem".hstr());
        Item i3 = cr1.AddItem("TestItem".hstr());
        if (i1 is null || i2 is null || i3 is null) return -2;

        array<Item> items = {i1, i2, i3};
        Game.MoveItems(items, cr2);

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }
}

)";

        return BakerTests::CompileInlineScripts(&compiler_engine, "LocEntityScripts", {{"Scripts/LocEntity.fos", script_source}}, [](string_view message) {
            string message_str = string(message);

            if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                throw ScriptSystemException(message_str);
            }
        });
    }
#endif

    inline auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = data_writer(map_data);
        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        return map_data;
    }

    inline auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrar = proto_engine.GetPropertyRegistrar(type_name);
        REQUIRE(static_cast<bool>(registrar));

        ProtoMap proto {proto_engine.Hashes.to_hashed_string(proto_name), registrar};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = data_writer(protos_data);

        writer.write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.write_string_bytes(type_name.as_str());
        writer.write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.write_string_bytes(proto_name);
        writer.write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        if (!props_data.empty()) {
            writer.write_bytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

    inline auto MakeLocEntityMetadataBlob() -> vector<uint8_t>
    {
        return BakerTests::MakeMetadataBlob({
            {"Entity", {{"CoverageTarget"}}},
            {"EntityHolder",
                {
                    {"Server", "Critter", "CoverageTarget", "CoverageItems", "Persistent"},
                    // A location survives a restart while an off-map critter does not, so the reload path
                    // needs a holder on something that comes back
                    {"Server", "Location", "CoverageTarget", "CoverageLocItems", "Persistent"},
                }},
        });
    }

    inline auto MakeResources() -> FileSystem
    {
        auto metadata_blob = MakeLocEntityMetadataBlob();

        auto compiler_resources_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("LocEntityCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        hstring item_type = proto_engine.Hashes.to_hashed_string("Item");
        hstring location_type = proto_engine.Hashes.to_hashed_string("Location");
        hstring map_type = proto_engine.Hashes.to_hashed_string("Map");

        auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "TestMap", msize {200, 200});
        auto fomap_blob = MakeEmptyMapBlob();
#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = MakeScriptBinary(compiler_resources);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("LocEntityRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("LocEntityCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("LocEntityItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("LocEntityLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("TestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("TestMap.fomap-bin-server", fomap_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("LocEntity.fos-bin-server", script_blob);
#endif

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    inline auto WaitForStart(ptr<ServerEngine> server) -> string
    {
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

    inline auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return safe_alloc::make_refcounted<ServerEngine>(&settings, MakeResources());
    }
}

#define MAKE_LEM_SERVER() \
    auto settings = MakeSettings(); \
    refcount_ptr<ServerEngine> server = MakeServerEngine(settings); \
    auto shutdown = scope_exit([&server]() noexcept { \
        safe_call([&server] { \
            if (server->IsStarted()) { \
                server->Shutdown(); \
            } \
        }); \
    }); \
    string startup_error = WaitForStart(server); \
    INFO(startup_error); \
    REQUIRE(startup_error.empty()); \
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}})); \
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); }); \
    auto get_func = [&server](string_view name) { return server->Hashes.to_hashed_string(name); }

#define RUN_LEM_FUNC(func_name) \
    auto func = server->FindFunc<int32_t>(get_func("LocEntity::" func_name)); \
    REQUIRE(func); \
    REQUIRE(func.Call()); \
    CHECK(func.GetResult() == 0)

// ========== Location Tests ==========

FO_END_NAMESPACE

