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
//

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

namespace LocEntity
{
    // ========== Location Tests ==========

    int TestCreateDestroyLocation()
    {
        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        int64 locId = loc.Id.value;
        if (locId == 0) return -2;

        Location@ found = Game.GetLocation(loc.Id);
        if (found is null) return -3;

        if (loc.ProtoId != "TestLocation".hstr()) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMultipleLocations()
    {
        Location@ loc1 = Game.CreateLocation("TestLocation".hstr());
        Location@ loc2 = Game.CreateLocation("TestLocation".hstr());
        Location@ loc3 = Game.CreateLocation("TestLocation".hstr());

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
        Location@ loc = Game.CreateLocation("TestLocation".hstr());
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
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr3 = Game.CreateCritter("TestCritter".hstr(), false);

        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        if (cr1.Id.value == cr2.Id.value) return -2;
        if (cr2.Id.value == cr3.Id.value) return -3;

        Critter@ found1 = Game.GetCritter(cr1.Id);
        if (found1 is null) return -4;
        if (found1.Id.value != cr1.Id.value) return -5;

        Game.DestroyCritter(cr3);
        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestEntityManagerItemOperations()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item1 = cr.AddItem("TestItem".hstr(), 1);
        Item@ item2 = cr.AddItem("TestItem".hstr(), 5);
        Item@ item3 = cr.AddItem("TestItem".hstr(), 10);

        if (item1 is null || item2 is null || item3 is null) return -2;

        Item@ found = Game.GetItem(item1.Id);
        if (found is null) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityManagerDestroyEntity()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -3;

        Game.DestroyEntity(loc);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityManagerBulkDestroyEntities()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr3 = Game.CreateCritter("TestCritter".hstr(), false);

        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        array<ident> ids = {cr1.Id, cr2.Id, cr3.Id};
        Game.DestroyEntities(ids);
        return 0;
    }

    // ========== Critter Advanced Operations ==========

    int TestCritterStateQueries()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
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
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.SetDir(0);
        if (cr.Dir != 0) return -2;

        cr.SetDir(3);
        if (cr.Dir != 3) return -3;

        cr.SetDirAngle(90);

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterItemManagement()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ i1 = cr.AddItem("TestItem".hstr(), 3);
        if (i1 is null) return -2;

        int count = cr.CountItem("TestItem".hstr());
        if (count < 3) return -3;

        array<Item@> items = cr.GetItems();
        if (items.length() == 0) return -4;

        Item@ byId = cr.GetItem(i1.Id);
        if (byId is null) return -5;

        cr.DestroyItem("TestItem".hstr(), 1);
        int afterDestroy = cr.CountItem("TestItem".hstr());
        if (afterDestroy < 2) return -6;

        cr.AddItem("TestItem".hstr(), 10);
        int afterAdd = cr.CountItem("TestItem".hstr());
        if (afterAdd < 12) return -7;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterConditions()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
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
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ container = cr.AddItem("TestItem".hstr(), 1);
        if (container is null) return -2;

        Item@ sub1 = container.AddItem("TestItem".hstr(), 1);
        Item@ sub2 = container.AddItem("TestItem".hstr(), 2);
        if (sub1 is null || sub2 is null) return -3;

        array<Item@> subs = container.GetItems();
        if (subs.length() < 2) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemMoveOperations()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item@ item = cr1.AddItem("TestItem".hstr(), 5);
        if (item is null) return -2;

        Game.MoveItem(item, cr2);

        Item@ onCr2 = cr2.GetItem(item.Id);
        if (onCr2 is null) return -3;

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestItemMovePartial()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item@ item = cr1.AddItem("TestItem".hstr(), 20);
        if (item is null) return -2;

        Game.MoveItem(item, 5, cr2);

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestItemDestroyVariants()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ i1 = cr.AddItem("TestItem".hstr(), 10);
        Item@ i2 = cr.AddItem("TestItem".hstr(), 5);
        if (i1 is null || i2 is null) return -2;

        Game.DestroyItem(i2);
        Game.DestroyItem(i1, 3);

        Game.DestroyCritter(cr);
        return 0;
    }

 )") + R"(
    // ========== NPC Queries ==========

    int TestNpcQueries()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr3 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        array<Critter@> npcs = Game.GetAllNpc();
        if (npcs.length() < 3) return -2;

        array<Critter@> byProto = Game.GetAllNpc("TestCritter".hstr());
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
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ container = cr.AddItem("TestItem".hstr(), 1);
        Item@ loose = cr.AddItem("TestItem".hstr(), 3);
        if (container is null || loose is null) return -2;

        Game.MoveItem(loose, container);

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestBulkItemDestruction()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ i1 = cr.AddItem("TestItem".hstr(), 1);
        Item@ i2 = cr.AddItem("TestItem".hstr(), 2);
        Item@ i3 = cr.AddItem("TestItem".hstr(), 3);
        if (i1 is null || i2 is null || i3 is null) return -2;

        array<Item@> items = {i1, i2, i3};
        Game.DestroyItems(items);

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetPlayer()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Player@ player = cr.GetPlayer();
        if (player !is null) return -2;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterAttachments()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
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

        array<string>@ keys = d.getKeys();
        if (keys.length() != 2) return -7;

        d.clear();
        if (d.length() != 0) return -8;

        return 0;
    }

    int TestStringAdvancedOps()
    {
        string csv = "a,b,c,d,e";
        array<string>@ parts = csv.split(",");
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
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -3;

        Critter@ foundCr = Game.GetCritter(cr.Id);
        if (foundCr is null) return -4;

        Item@ foundItem = Game.GetItem(item.Id);
        if (foundItem is null) return -5;

        Location@ foundLoc = Game.GetLocation(loc.Id);
        if (foundLoc is null) return -6;

        Game.DestroyLocation(loc);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestDestroyEntityById()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -2;

        Game.DestroyEntity(loc.Id);
        Game.DestroyEntity(cr.Id);

        return 0;
    }

    int TestMultipleBulkDestroyEntitiesById()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr3 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null || cr3 is null) return -1;

        Location@ l1 = Game.CreateLocation("TestLocation".hstr());
        Location@ l2 = Game.CreateLocation("TestLocation".hstr());
        if (l1 is null || l2 is null) return -2;

        array<ident> crIds = {cr1.Id, cr2.Id, cr3.Id};
        Game.DestroyEntities(crIds);

        array<ident> locIds = {l1.Id, l2.Id};
        Game.DestroyEntities(locIds);

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

        uint8 dir = Game.GetDirection(p1, p2);
        if (dir > 5) return -1;

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

    int TestUnloadCritter()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return -1;

        array<hstring> mapPids = {"TestMap".hstr()};
        Location@ loc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (loc is null) {
            Game.DestroyCritter(cr);
            return -2;
        }

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) {
            Game.DestroyLocation(loc);
            Game.DestroyCritter(cr);
            return -3;
        }

        cr.TransferToMap(map, mpos(20, 20), 0);
        if (cr.MapId.value != map.Id.value) {
            Game.DestroyLocation(loc);
            Game.DestroyCritter(cr);
            return -4;
        }

        cr.MakePersistent(true);

        ident crId = cr.Id;

        Game.UnloadCritter(cr);

        Critter@ gone = Game.GetCritter(crId);
        if (gone !is null) {
            Game.DestroyLocation(loc);
            return -5;
        }

        Critter@ loaded = Game.LoadCritter(crId, false);
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
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item@ i1 = cr1.AddItem("TestItem".hstr(), 1);
        Item@ i2 = cr1.AddItem("TestItem".hstr(), 2);
        Item@ i3 = cr1.AddItem("TestItem".hstr(), 3);
        if (i1 is null || i2 is null || i3 is null) return -2;

        array<Item@> items = {i1, i2, i3};
        Game.MoveItems(items, cr2);

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }
}

)";

        return BakerTests::CompileInlineScripts(&compiler_engine, "LocEntityScripts", {{"Scripts/LocEntity.fos", script_source}}, [](string_view message) {
            const auto message_str = string(message);

            if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                throw ScriptSystemException(message_str);
            }
        });
    }

    static auto MakeEmptyMapBlob() -> vector<uint8>
    {
        vector<uint8> map_data;
        auto writer = DataWriter(map_data);
        writer.Write<uint32>(uint32 {0});
        writer.Write<uint32>(uint32 {0});
        writer.Write<uint32>(uint32 {0});
        return map_data;
    }

    static auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8>
    {
        vector<uint8> props_data;
        set<hstring> str_hashes;

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), proto_engine.GetPropertyRegistrator(type_name)};
        proto.SetSize(map_size);
        proto.GetProperties().StoreAllData(props_data, str_hashes);

        vector<uint8> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32>(uint32 {0});
        ignore_unused(str_hashes);
        writer.Write<uint32>(uint32 {1});
        writer.Write<uint32>(uint32 {1});
        writer.Write<uint16>(numeric_cast<uint16>(type_name.as_str().length()));
        writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());
        writer.Write<uint16>(numeric_cast<uint16>(proto_name.length()));
        writer.WritePtr(proto_name.data(), proto_name.length());
        writer.Write<uint32>(numeric_cast<uint32>(props_data.size()));
        writer.WritePtr(props_data.data(), props_data.size());

        return protos_data;
    }

    static auto MakeResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("LocEntityCompilerResources");
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
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        const auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "TestMap", msize {200, 200});
        const auto fomap_blob = MakeEmptyMapBlob();
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("LocEntityRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("LocEntityCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("LocEntityItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("LocEntityLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("TestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("TestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("LocEntity.fos-bin-server", script_blob);

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

#define MAKE_LEM_SERVER() \
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

#define RUN_LEM_FUNC(func_name) \
    auto func = server->FindFunc<int32>(get_func("LocEntity::" func_name)); \
    REQUIRE(func); \
    REQUIRE(func.Call()); \
    CHECK(func.GetResult() == 0)

// ========== Location Tests ==========

TEST_CASE("LocationCreationAndDestruction")
{
    MAKE_LEM_SERVER();

    SECTION("CreateDestroy")
    {
        RUN_LEM_FUNC("TestCreateDestroyLocation");
    }

    SECTION("MultipleLocations")
    {
        RUN_LEM_FUNC("TestMultipleLocations");
    }

    SECTION("LocationProperties")
    {
        RUN_LEM_FUNC("TestLocationProperties");
    }
}

// ========== Entity Manager Tests ==========

TEST_CASE("EntityManagerOperations")
{
    MAKE_LEM_SERVER();

    SECTION("CritterOperations")
    {
        RUN_LEM_FUNC("TestEntityManagerCritterOperations");
    }

    SECTION("ItemOperations")
    {
        RUN_LEM_FUNC("TestEntityManagerItemOperations");
    }

    SECTION("DestroyEntity")
    {
        RUN_LEM_FUNC("TestEntityManagerDestroyEntity");
    }

    SECTION("BulkDestroyEntities")
    {
        RUN_LEM_FUNC("TestEntityManagerBulkDestroyEntities");
    }
}

// ========== Critter Operations ==========

TEST_CASE("CritterAdvancedOperations")
{
    MAKE_LEM_SERVER();

    SECTION("StateQueries")
    {
        RUN_LEM_FUNC("TestCritterStateQueries");
    }

    SECTION("Direction")
    {
        RUN_LEM_FUNC("TestCritterDirection");
    }

    SECTION("ItemManagement")
    {
        RUN_LEM_FUNC("TestCritterItemManagement");
    }

    SECTION("Conditions")
    {
        RUN_LEM_FUNC("TestCritterConditions");
    }

    SECTION("GetPlayer")
    {
        RUN_LEM_FUNC("TestCritterGetPlayer");
    }

    SECTION("Attachments")
    {
        RUN_LEM_FUNC("TestCritterAttachments");
    }
}

// ========== Item Operations ==========

TEST_CASE("ItemAdvancedOperations")
{
    MAKE_LEM_SERVER();

    SECTION("ContainerOperations")
    {
        RUN_LEM_FUNC("TestItemContainerOperations");
    }

    SECTION("MoveOperations")
    {
        RUN_LEM_FUNC("TestItemMoveOperations");
    }

    SECTION("MovePartial")
    {
        RUN_LEM_FUNC("TestItemMovePartial");
    }

    SECTION("DestroyVariants")
    {
        RUN_LEM_FUNC("TestItemDestroyVariants");
    }

    SECTION("MoveToContainer")
    {
        RUN_LEM_FUNC("TestItemMoveToContainer");
    }

    SECTION("BulkDestruction")
    {
        RUN_LEM_FUNC("TestBulkItemDestruction");
    }
}

// ========== NPC and Entity Queries ==========

TEST_CASE("NpcAndEntityQueries")
{
    MAKE_LEM_SERVER();

    SECTION("NpcQueries")
    {
        RUN_LEM_FUNC("TestNpcQueries");
    }

    SECTION("EntityGetById")
    {
        RUN_LEM_FUNC("TestEntityGetById");
    }

    SECTION("DestroyEntityById")
    {
        RUN_LEM_FUNC("TestDestroyEntityById");
    }

    SECTION("BulkDestroyEntitiesById")
    {
        RUN_LEM_FUNC("TestMultipleBulkDestroyEntitiesById");
    }
}

// ========== Game Utility ==========

TEST_CASE("GameUtilityOperations")
{
    MAKE_LEM_SERVER();

    SECTION("Random")
    {
        RUN_LEM_FUNC("TestGameRandom");
    }

    SECTION("UnixTime")
    {
        RUN_LEM_FUNC("TestGameGetUnixTime");
    }

    SECTION("HstringOps")
    {
        RUN_LEM_FUNC("TestHstringOperations");
    }

    SECTION("DatabaseQueries")
    {
        RUN_LEM_FUNC("TestDatabaseQueries");
    }

    SECTION("MathOps")
    {
        RUN_LEM_FUNC("TestMathOps");
    }

    SECTION("TimeOps")
    {
        RUN_LEM_FUNC("TestTimeOperations");
    }
}

// ========== Collection and String Ops ==========

TEST_CASE("CollectionAndStringOps")
{
    MAKE_LEM_SERVER();

    SECTION("ArraySortSearch")
    {
        RUN_LEM_FUNC("TestArraySortAndSearch");
    }

    SECTION("DictAdvanced")
    {
        RUN_LEM_FUNC("TestDictAdvancedOps");
    }

    SECTION("StringAdvanced")
    {
        RUN_LEM_FUNC("TestStringAdvancedOps");
    }

    SECTION("HstringToFrom")
    {
        RUN_LEM_FUNC("TestHstringToFromString");
    }
}

// ========== Bulk Move Operations ==========

TEST_CASE("LoadUnloadCritter")
{
    MAKE_LEM_SERVER();

    SECTION("UnloadAndLoadClearsMapId")
    {
        RUN_LEM_FUNC("TestUnloadCritter");
    }
}

TEST_CASE("BulkMoveOperations")
{
    MAKE_LEM_SERVER();

    SECTION("MoveItemsBulk")
    {
        RUN_LEM_FUNC("TestMoveItemsBulk");
    }
}

// ========== C++ API Tests ==========

TEST_CASE("LocationCppApiTests")
{
    MAKE_LEM_SERVER();

    SECTION("CreateMultipleLocations")
    {
        const auto initial_count = server->EntityMngr.GetLocationsCount();

        auto* loc1 = server->MapMngr.CreateLocation(get_func("TestLocation"));
        REQUIRE(loc1 != nullptr);
        auto* loc2 = server->MapMngr.CreateLocation(get_func("TestLocation"));
        REQUIRE(loc2 != nullptr);
        auto* loc3 = server->MapMngr.CreateLocation(get_func("TestLocation"));
        REQUIRE(loc3 != nullptr);

        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 3);
        CHECK(loc1->GetId() != loc2->GetId());
        CHECK(loc2->GetId() != loc3->GetId());
        CHECK(loc1->GetMaps().empty());

        server->MapMngr.DestroyLocation(loc3);
        server->MapMngr.DestroyLocation(loc2);
        server->MapMngr.DestroyLocation(loc1);

        CHECK(server->EntityMngr.GetLocationsCount() == initial_count);
    }

    SECTION("LocationById")
    {
        auto* loc = server->MapMngr.CreateLocation(get_func("TestLocation"));
        REQUIRE(loc != nullptr);

        const auto loc_id = loc->GetId();
        auto* found = server->EntityMngr.GetLocation(loc_id);
        CHECK(found == loc);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("LocationProtoId")
    {
        auto* loc = server->MapMngr.CreateLocation(get_func("TestLocation"));
        REQUIRE(loc != nullptr);

        CHECK(loc->GetProtoId() == get_func("TestLocation"));

        server->MapMngr.DestroyLocation(loc);
    }
}

TEST_CASE("CritterCppApiAdvanced")
{
    MAKE_LEM_SERVER();

    SECTION("CreateMultipleCritters")
    {
        const auto initial_count = server->EntityMngr.GetCrittersCount();

        auto* cr1 = server->CreateCritter(get_func("TestCritter"), false);
        auto* cr2 = server->CreateCritter(get_func("TestCritter"), false);
        auto* cr3 = server->CreateCritter(get_func("TestCritter"), false);

        REQUIRE(cr1 != nullptr);
        REQUIRE(cr2 != nullptr);
        REQUIRE(cr3 != nullptr);

        CHECK(server->EntityMngr.GetCrittersCount() >= initial_count + 3);
        CHECK(cr1->GetId() != cr2->GetId());
        CHECK(cr2->GetId() != cr3->GetId());

        server->CrMngr.DestroyCritter(cr3);
        server->CrMngr.DestroyCritter(cr2);
        server->CrMngr.DestroyCritter(cr1);
    }

    SECTION("CritterConditionCpp")
    {
        auto* cr = server->CreateCritter(get_func("TestCritter"), false);
        REQUIRE(cr != nullptr);

        CHECK(cr->IsAlive());
        CHECK_FALSE(cr->IsDead());
        CHECK_FALSE(cr->IsKnockout());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterItemsCpp")
    {
        auto* cr = server->CreateCritter(get_func("TestCritter"), false);
        REQUIRE(cr != nullptr);

        auto* item = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 5);
        REQUIRE(item != nullptr);

        const auto inv_items = cr->GetInvItems();
        CHECK_FALSE(inv_items.empty());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterById")
    {
        auto* cr = server->CreateCritter(get_func("TestCritter"), false);
        REQUIRE(cr != nullptr);

        const auto cr_id = cr->GetId();
        auto* found = server->EntityMngr.GetCritter(cr_id);
        CHECK(found == cr);

        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("ItemCppApiAdvanced")
{
    MAKE_LEM_SERVER();

    SECTION("CreateItemOnCritter")
    {
        auto* cr = server->CreateCritter(get_func("TestCritter"), false);
        REQUIRE(cr != nullptr);

        const auto initial_item_count = server->EntityMngr.GetItemsCount();

        auto* item = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 10);
        REQUIRE(item != nullptr);
        CHECK(server->EntityMngr.GetItemsCount() >= initial_item_count + 1);

        const auto item_id = item->GetId();
        auto* found = server->EntityMngr.GetItem(item_id);
        CHECK(found == item);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("DestroyItemCpp")
    {
        auto* cr = server->CreateCritter(get_func("TestCritter"), false);
        REQUIRE(cr != nullptr);

        auto* item = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 1);
        REQUIRE(item != nullptr);

        const auto item_id = item->GetId();
        server->ItemMngr.DestroyItem(item);

        auto* gone = server->EntityMngr.GetItem(item_id);
        CHECK(gone == nullptr);

        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("ServerHealthInfoCpp")
{
    MAKE_LEM_SERVER();

    SECTION("HealthInfoContent")
    {
        const auto info = server->GetHealthInfo();
        CHECK_FALSE(info.empty());
        CHECK(info.find("Server uptime") != string::npos);
    }
}

TEST_CASE("EntityManagerCountsCpp")
{
    MAKE_LEM_SERVER();

    SECTION("InitialState")
    {
        CHECK(server->EntityMngr.GetCrittersCount() >= 0);
        CHECK(server->EntityMngr.GetItemsCount() >= 0);
        CHECK(server->EntityMngr.GetLocationsCount() >= 0);
        CHECK(server->EntityMngr.GetMapsCount() >= 0);
    }

    SECTION("CountsAfterCreation")
    {
        const auto initial_cr = server->EntityMngr.GetCrittersCount();
        const auto initial_loc = server->EntityMngr.GetLocationsCount();

        auto* cr = server->CreateCritter(get_func("TestCritter"), false);
        auto* loc = server->MapMngr.CreateLocation(get_func("TestLocation"));

        CHECK(server->EntityMngr.GetCrittersCount() >= initial_cr + 1);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_loc + 1);

        server->MapMngr.DestroyLocation(loc);
        server->CrMngr.DestroyCritter(cr);
    }
}

FO_END_NAMESPACE
