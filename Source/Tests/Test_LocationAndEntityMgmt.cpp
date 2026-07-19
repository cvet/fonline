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
        BakerTests::OverrideSetting(settings.CustomCollections, vector<string> {"TestCollection:Int"});

        return settings;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        const auto script_source = string(R"(

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

        Item item1 = cr.AddItem("TestItem".hstr(), 1);
        Item item2 = cr.AddItem("TestItem".hstr(), 5);
        Item item3 = cr.AddItem("TestItem".hstr(), 10);

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

        Item item = cr.AddItem("TestItem".hstr(), 1);
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

        Item i1 = cr.AddItem("TestItem".hstr(), 3);
        if (i1 is null) return -2;

        int count = cr.CountItem("TestItem".hstr());
        if (count < 3) return -3;

        array<Item> items = cr.GetItems();
        if (items.length() == 0) return -4;

        Item? byId = cr.GetItem(i1.Id);
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

        Item container = cr.AddItem("TestItem".hstr(), 1);
        if (container is null) return -2;

        Item sub1 = container.AddItem("TestItem".hstr(), 1);
        Item sub2 = container.AddItem("TestItem".hstr(), 2);
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

        Item item = cr1.AddItem("TestItem".hstr(), 5);
        if (item is null) return -2;

        Game.MoveItem(item, cr2);

        Item? onCr2 = cr2.GetItem(item.Id);
        if (onCr2 is null) return -3;

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestItemMovePartial()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Item item = cr1.AddItem("TestItem".hstr(), 20);
        if (item is null) return -2;

        Game.MoveItem(item, 5, cr2);

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestItemDestroyVariants()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item i1 = cr.AddItem("TestItem".hstr(), 10);
        Item i2 = cr.AddItem("TestItem".hstr(), 5);
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

        Item container = cr.AddItem("TestItem".hstr(), 1);
        Item loose = cr.AddItem("TestItem".hstr(), 3);
        if (container is null || loose is null) return -2;

        Game.MoveItem(loose, container);

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestBulkItemDestruction()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item i1 = cr.AddItem("TestItem".hstr(), 1);
        Item i2 = cr.AddItem("TestItem".hstr(), 2);
        Item i3 = cr.AddItem("TestItem".hstr(), 3);
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

        Item item = cr.AddItem("TestItem".hstr(), 1);
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

        Item i1 = cr1.AddItem("TestItem".hstr(), 1);
        Item i2 = cr1.AddItem("TestItem".hstr(), 2);
        Item i3 = cr1.AddItem("TestItem".hstr(), 3);
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
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        return map_data;
    }

    static auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrator = proto_engine.GetPropertyRegistrator(type_name);
        REQUIRE(static_cast<bool>(registrator));

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), registrator};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WriteStringBytes(type_name.as_str());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WriteStringBytes(proto_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        if (!props_data.empty()) {
            writer.WriteBytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

    static auto MakeLocEntityMetadataBlob() -> vector<uint8_t>
    {
        return BakerTests::MakeMetadataBlob({
            {"Entity", {{"CoverageTarget"}}},
            {"EntityHolder", {{"Server", "Critter", "CoverageTarget", "CoverageItems", "Persistent"}}},
        });
    }

    static auto MakeResources() -> FileSystem
    {
        const auto metadata_blob = MakeLocEntityMetadataBlob();

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

    static auto WaitForStart(ptr<ServerEngine> server) -> string
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

    static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeResources());
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
    const auto startup_error = WaitForStart(server); \
    INFO(startup_error); \
    REQUIRE(startup_error.empty()); \
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}})); \
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); }); \
    const auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); }

#define RUN_LEM_FUNC(func_name) \
    auto func = server->FindFunc<int32_t>(get_func("LocEntity::" func_name)); \
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

    SECTION("DirectLoadReportsAndPrunesMissingInventoryItem")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), true);

        auto item = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 1);
        REQUIRE(static_cast<bool>(item));

        server->EntityMngr.MakePersistent(cr, true, true);

        const ident_t cr_id = cr->GetId();
        const ident_t item_id = item->GetId();

        server->DbStorage.Delete(get_func("Items"), item_id);
        server->UnloadCritter(cr);

        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
        CHECK(server->EntityMngr.GetItem(item_id) == nullptr);

        bool is_error = false;
        auto loaded_holder = server->EntityMngr.LoadCritter(cr_id, false, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK(is_error);
        CHECK(loaded->GetItemIds().empty());
        CHECK_FALSE(loaded->HasItems());
        CHECK(server->EntityMngr.GetCritter(cr_id) == loaded);

        loaded->MarkAsDestroying();
        server->UnloadCritterInnerEntities(loaded);
        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterCritter(loaded);

        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
    }

    SECTION("DirectCustomInnerEntityLifecycle")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        server->EntityMngr.MakePersistent(cr, true, true);

        const hstring custom_type = get_func("CoverageTarget");
        const hstring custom_entry = get_func("CoverageItems");
        auto custom = server->EntityMngr.CreateCustomInnerEntity(cr, custom_entry, {});
        auto custom_holder = custom.hold_ref();

        const ident_t custom_id = custom->GetId();
        REQUIRE(custom_id);
        CHECK(custom->GetTypeName() == custom_type);
        CHECK(custom->GetCustomHolderId() == cr->GetId());
        CHECK(custom->GetCustomHolderEntry() == custom_entry);
        CHECK(custom->IsPersistent());

        const auto parent = custom->GetParent();
        REQUIRE(parent);
        CHECK(parent == cr);

        const auto holder_prop = server->GetEntityHolderIdsProp(cr, custom_entry);
        CHECK(cr->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()) == vector<ident_t> {custom_id});

        const auto inner_entities = cr->GetInnerEntities(custom_entry);
        REQUIRE(inner_entities);
        REQUIRE(inner_entities->size() == 1);
        CHECK(inner_entities->front() == custom);
        CHECK(server->EntityMngr.GetCustomEntity(custom_type, custom_id) == custom);

        server->EntityMngr.DestroyInnerEntities(cr);

        CHECK_FALSE(cr->HasInnerEntities());
        CHECK(cr->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()).empty());
        CHECK(custom->IsDestroyed());
        CHECK(server->EntityMngr.GetCustomEntity(custom_type, custom_id) == nullptr);

        server->EntityMngr.DestroyEntity(cr);
    }

    SECTION("DirectLoadRestoresContainerItemTree")
    {
        auto container = server->ItemMngr.CreateItem(get_func("TestItem"), 1, nullptr);
        auto container_holder = container.hold_ref();

        auto inner = server->ItemMngr.AddItemContainer(container, get_func("TestItem"), 1, {});
        REQUIRE(inner);
        auto inner_holder = inner.hold_ref();

        server->EntityMngr.MakePersistent(container, true, true);
        server->DbStorage.WaitCommitChanges();

        const ident_t container_id = container->GetId();
        const ident_t inner_id = inner->GetId();

        REQUIRE(container_id);
        REQUIRE(inner_id);
        CHECK(container->GetInnerItemIds() == vector<ident_t> {inner_id});
        CHECK(inner->GetOwnership() == ItemOwnership::ItemContainer);
        CHECK(inner->GetContainerId() == container_id);

        (void)container->TakeAllInnerItems();
        inner->MarkAsDestroyed();
        server->EntityMngr.UnregisterItem(inner, false);
        container->MarkAsDestroyed();
        server->EntityMngr.UnregisterItem(container, false);

        CHECK(server->EntityMngr.GetItem(container_id) == nullptr);
        CHECK(server->EntityMngr.GetItem(inner_id) == nullptr);

        bool is_error = false;
        refcount_nptr<Item> loaded_holder = server->EntityMngr.LoadItem(container_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == container_id);
        CHECK(loaded->GetProtoId() == get_func("TestItem"));
        CHECK(loaded->GetOwnership() == ItemOwnership::Nowhere);
        CHECK(loaded->GetInnerItemIds() == vector<ident_t> {inner_id});
        CHECK(server->EntityMngr.GetItem(container_id) == loaded);

        const auto loaded_inner_items = loaded->GetAllInnerItems();
        REQUIRE(loaded_inner_items.size() == 1);

        auto loaded_inner = loaded_inner_items.front();
        CHECK(loaded_inner->GetId() == inner_id);
        CHECK(loaded_inner->GetProtoId() == get_func("TestItem"));
        CHECK(loaded_inner->GetOwnership() == ItemOwnership::ItemContainer);
        CHECK(loaded_inner->GetContainerId() == container_id);
        CHECK(loaded_inner->GetParent() == loaded);
        CHECK(server->EntityMngr.GetItem(inner_id) == loaded_inner);

        server->ItemMngr.DestroyItem(loaded);
        CHECK(server->EntityMngr.GetItem(container_id) == nullptr);
        CHECK(server->EntityMngr.GetItem(inner_id) == nullptr);
    }

    SECTION("DirectLoadMapRestoresCrittersAndItems")
    {
        const vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        const auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        server->MapMngr.AddCritterToMap(cr, map, mpos {20, 21}, mdir {0}, ident_t {});

        auto item = server->ItemMngr.CreateItemOnHex(map, mpos {22, 23}, get_func("TestItem"), 1, nullptr);

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        const ident_t loc_id = loc->GetId();
        const ident_t map_id = map->GetId();
        const ident_t cr_id = cr->GetId();
        const ident_t item_id = item->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        REQUIRE(cr_id);
        REQUIRE(item_id);
        CHECK(map->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(map->GetItemIds() == vector<ident_t> {item_id});

        const auto locations_collection = get_func("Locations");
        const auto maps_collection = get_func("Maps");
        const auto critters_collection = get_func("Critters");
        const auto items_collection = get_func("Items");

        const auto loc_doc = server->DbStorage.Get(locations_collection, loc_id);
        const auto map_doc = server->DbStorage.Get(maps_collection, map_id);
        const auto cr_doc = server->DbStorage.Get(critters_collection, cr_id);
        const auto item_doc = server->DbStorage.Get(items_collection, item_id);

        REQUIRE_FALSE(loc_doc.Empty());
        REQUIRE_FALSE(map_doc.Empty());
        REQUIRE_FALSE(cr_doc.Empty());
        REQUIRE_FALSE(item_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK(server->EntityMngr.GetLocation(loc_id) == nullptr);
        CHECK(server->EntityMngr.GetMap(map_id) == nullptr);
        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
        CHECK(server->EntityMngr.GetItem(item_id) == nullptr);

        server->DbStorage.Insert(locations_collection, loc_id, loc_doc);
        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.Insert(critters_collection, cr_id, cr_doc);
        server->DbStorage.Insert(items_collection, item_id, item_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Map> loaded_holder = server->EntityMngr.LoadMap(map_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == map_id);
        CHECK(loaded->GetLocId() == loc_id);
        CHECK(loaded->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(loaded->GetItemIds() == vector<ident_t> {item_id});
        CHECK(server->EntityMngr.GetMap(map_id) == loaded);

        auto loaded_cr = server->EntityMngr.GetCritter(cr_id);
        CHECK(loaded_cr->GetMapId() == map_id);
        CHECK(loaded_cr->GetHex() == mpos {20, 21});
        CHECK(loaded->GetCritter(cr_id) == loaded_cr);

        auto loaded_item = server->EntityMngr.GetItem(item_id);
        CHECK(loaded_item->GetOwnership() == ItemOwnership::MapHex);
        CHECK(loaded_item->GetMapId() == map_id);
        CHECK(loaded_item->GetHex() == mpos {22, 23});
        CHECK(loaded->GetItem(item_id) == loaded_item);

        server->CrMngr.DestroyCritter(loaded_cr.get());
        loaded_cr.reset();

        server->ItemMngr.DestroyItem(loaded_item.get());
        loaded_item.reset();

        CHECK(loaded->GetCritters().empty());
        CHECK(loaded->GetItems().empty());

        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterMap(loaded);
    }

    SECTION("DirectLoadMapClampsInvalidCritterAndItemHex")
    {
        const vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        const auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        server->MapMngr.AddCritterToMap(cr, map, mpos {20, 21}, mdir {0}, ident_t {});

        auto item = server->ItemMngr.CreateItemOnHex(map, mpos {22, 23}, get_func("TestItem"), 1, nullptr);

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        const ident_t loc_id = loc->GetId();
        const ident_t map_id = map->GetId();
        const ident_t cr_id = cr->GetId();
        const ident_t item_id = item->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        REQUIRE(cr_id);
        REQUIRE(item_id);

        const auto maps_collection = get_func("Maps");
        const auto critters_collection = get_func("Critters");
        const auto items_collection = get_func("Items");

        const auto map_doc = server->DbStorage.Get(maps_collection, map_id);
        auto cr_doc = server->DbStorage.Get(critters_collection, cr_id);
        auto item_doc = server->DbStorage.Get(items_collection, item_id);

        REQUIRE_FALSE(map_doc.Empty());
        REQUIRE_FALSE(cr_doc.Empty());
        REQUIRE_FALSE(item_doc.Empty());

        cr_doc.Assign("Hex", AnyData::Value {string {"-5 300"}});
        item_doc.Assign("Hex", AnyData::Value {string {"300 -5"}});

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK(server->EntityMngr.GetLocation(loc_id) == nullptr);
        CHECK(server->EntityMngr.GetMap(map_id) == nullptr);
        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
        CHECK(server->EntityMngr.GetItem(item_id) == nullptr);

        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.Insert(critters_collection, cr_id, cr_doc);
        server->DbStorage.Insert(items_collection, item_id, item_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Map> loaded_holder = server->EntityMngr.LoadMap(map_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == map_id);
        CHECK(loaded->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(loaded->GetItemIds() == vector<ident_t> {item_id});
        CHECK(server->EntityMngr.GetMap(map_id) == loaded);

        auto loaded_cr = server->EntityMngr.GetCritter(cr_id);
        CHECK(loaded_cr->GetMapId() == map_id);
        CHECK(loaded_cr->GetHex() == mpos {0, 199});
        CHECK(loaded->GetCritter(cr_id) == loaded_cr);

        auto loaded_item = server->EntityMngr.GetItem(item_id);
        CHECK(loaded_item->GetOwnership() == ItemOwnership::MapHex);
        CHECK(loaded_item->GetMapId() == map_id);
        CHECK(loaded_item->GetHex() == mpos {199, 0});
        CHECK(loaded->GetItem(item_id) == loaded_item);

        server->CrMngr.DestroyCritter(loaded_cr.get());
        loaded_cr.reset();

        server->ItemMngr.DestroyItem(loaded_item.get());
        loaded_item.reset();

        CHECK(loaded->GetCritters().empty());
        CHECK(loaded->GetItems().empty());

        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterMap(loaded);
    }

    SECTION("DirectLoadMapPrunesMissingCritterAndItemRefs")
    {
        const vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        const auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        server->MapMngr.AddCritterToMap(cr, map, mpos {20, 21}, mdir {0}, ident_t {});

        auto item = server->ItemMngr.CreateItemOnHex(map, mpos {22, 23}, get_func("TestItem"), 1, nullptr);

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        const ident_t loc_id = loc->GetId();
        const ident_t map_id = map->GetId();
        const ident_t cr_id = cr->GetId();
        const ident_t item_id = item->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        REQUIRE(cr_id);
        REQUIRE(item_id);
        CHECK(map->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(map->GetItemIds() == vector<ident_t> {item_id});

        const auto maps_collection = get_func("Maps");
        const auto map_doc = server->DbStorage.Get(maps_collection, map_id);
        REQUIRE_FALSE(map_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK(server->EntityMngr.GetLocation(loc_id) == nullptr);
        CHECK(server->EntityMngr.GetMap(map_id) == nullptr);
        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
        CHECK(server->EntityMngr.GetItem(item_id) == nullptr);

        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Map> loaded_holder = server->EntityMngr.LoadMap(map_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK(is_error);
        CHECK(loaded->GetId() == map_id);
        CHECK(loaded->GetLocId() == loc_id);
        CHECK(loaded->GetCritterIds().empty());
        CHECK(loaded->GetItemIds().empty());
        CHECK(loaded->GetCritters().empty());
        CHECK(loaded->GetItems().empty());
        CHECK(server->EntityMngr.GetMap(map_id) == loaded);
        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
        CHECK(server->EntityMngr.GetItem(item_id) == nullptr);

        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterMap(loaded);
    }

    SECTION("DirectLoadLocationRestoresMapRefs")
    {
        const vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        const auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        const ident_t loc_id = loc->GetId();
        const ident_t map_id = map->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        CHECK(loc->GetMapIds() == vector<ident_t> {map_id});
        CHECK(map->GetLocId() == loc_id);
        CHECK(map->GetLocMapIndex() == 0);
        CHECK(map->GetLocation() == loc);

        const auto locations_collection = get_func("Locations");
        const auto maps_collection = get_func("Maps");

        const auto loc_doc = server->DbStorage.Get(locations_collection, loc_id);
        const auto map_doc = server->DbStorage.Get(maps_collection, map_id);

        REQUIRE_FALSE(loc_doc.Empty());
        REQUIRE_FALSE(map_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK(server->EntityMngr.GetLocation(loc_id) == nullptr);
        CHECK(server->EntityMngr.GetMap(map_id) == nullptr);

        server->DbStorage.Insert(locations_collection, loc_id, loc_doc);
        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Location> loaded_holder = server->EntityMngr.LoadLocation(loc_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == loc_id);
        CHECK(loaded->GetProtoId() == get_func("TestLocation"));
        CHECK(loaded->GetMapIds() == vector<ident_t> {map_id});
        CHECK(server->EntityMngr.GetLocation(loc_id) == loaded);

        const auto loaded_maps = loaded->GetMaps();
        REQUIRE(loaded_maps.size() == 1);

        auto loaded_map = loaded_maps.front();
        CHECK(loaded_map->GetId() == map_id);
        CHECK(loaded_map->GetProtoId() == get_func("TestMap"));
        CHECK(loaded_map->GetLocId() == loc_id);
        CHECK(loaded_map->GetLocMapIndex() == 0);
        CHECK(loaded_map->GetLocation() == loaded);
        CHECK(server->EntityMngr.GetMap(map_id) == loaded_map);

        server->MapMngr.DestroyLocation(loaded);
        loaded_holder.reset();
        server->DbStorage.WaitCommitChanges();

        CHECK(server->EntityMngr.GetLocation(loc_id) == nullptr);
        CHECK(server->EntityMngr.GetMap(map_id) == nullptr);
    }

    SECTION("DirectLoadLocationPrunesMissingMapRefs")
    {
        const vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        const auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        const ident_t loc_id = loc->GetId();
        const ident_t map_id = map->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        CHECK(loc->GetMapIds() == vector<ident_t> {map_id});

        const auto locations_collection = get_func("Locations");
        const auto loc_doc = server->DbStorage.Get(locations_collection, loc_id);
        REQUIRE_FALSE(loc_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK(server->EntityMngr.GetLocation(loc_id) == nullptr);
        CHECK(server->EntityMngr.GetMap(map_id) == nullptr);

        server->DbStorage.Insert(locations_collection, loc_id, loc_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Location> loaded_holder = server->EntityMngr.LoadLocation(loc_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK(is_error);
        CHECK(loaded->GetId() == loc_id);
        CHECK(loaded->GetMapIds().empty());
        CHECK_FALSE(loaded->HasMaps());
        CHECK(server->EntityMngr.GetLocation(loc_id) == loaded);
        CHECK(server->EntityMngr.GetMap(map_id) == nullptr);

        server->MapMngr.DestroyLocation(loaded);
        loaded_holder.reset();
        server->DbStorage.WaitCommitChanges();

        CHECK(server->EntityMngr.GetLocation(loc_id) == nullptr);
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

        auto loc1 = server->MapMngr.CreateLocation(get_func("TestLocation"));
        auto loc2 = server->MapMngr.CreateLocation(get_func("TestLocation"));
        auto loc3 = server->MapMngr.CreateLocation(get_func("TestLocation"));

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
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"));

        const auto loc_id = loc->GetId();
        auto found_loc = server->EntityMngr.GetLocation(loc_id);
        REQUIRE(found_loc);
        CHECK(found_loc == loc);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("LocationProtoId")
    {
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"));

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

        auto cr1 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr2 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr3 = server->CreateCritter(get_func("TestCritter"), false);

        CHECK(server->EntityMngr.GetCrittersCount() >= initial_count + 3);
        CHECK(cr1->GetId() != cr2->GetId());
        CHECK(cr2->GetId() != cr3->GetId());

        server->CrMngr.DestroyCritter(cr3);
        server->CrMngr.DestroyCritter(cr2);
        server->CrMngr.DestroyCritter(cr1);
    }

    SECTION("CritterConditionCpp")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        CHECK(cr->IsAlive());
        CHECK_FALSE(cr->IsDead());
        CHECK_FALSE(cr->IsKnockout());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterItemsCpp")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        auto item = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 5);
        REQUIRE(static_cast<bool>(item));

        const auto inv_items = cr->GetInvItems();
        CHECK_FALSE(inv_items.empty());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterById")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        const auto cr_id = cr->GetId();
        auto found_cr = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(found_cr);
        CHECK(found_cr == cr);

        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("ItemCppApiAdvanced")
{
    MAKE_LEM_SERVER();

    SECTION("CreateItemOnCritter")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        const auto initial_item_count = server->EntityMngr.GetItemsCount();

        auto item = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 10);
        REQUIRE(static_cast<bool>(item));
        CHECK(server->EntityMngr.GetItemsCount() >= initial_item_count + 1);

        const auto item_id = item->GetId();
        auto found = server->EntityMngr.GetItem(item_id);
        REQUIRE(found);
        CHECK(nptr<Item> {found} == item);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("DestroyItemCpp")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        auto item = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 1);
        REQUIRE(static_cast<bool>(item));

        const auto item_id = item->GetId();
        server->ItemMngr.DestroyItem(item);

        auto gone = server->EntityMngr.GetItem(item_id);
        CHECK_FALSE(static_cast<bool>(gone));

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

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"));

        CHECK(server->EntityMngr.GetCrittersCount() >= initial_cr + 1);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_loc + 1);

        server->MapMngr.DestroyLocation(loc);
        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("TimeEventCancellationContinuesAfterDispatcherFailure")
{
    MAKE_LEM_SERVER();

    auto cr = server->CreateCritter(get_func("TestCritter"), false).hold_ref();
    constexpr size_t event_count = 2;

    for (size_t i = 0; i < event_count; i++) {
        auto timer_func = server->FindFunc<void>(get_func("LocEntity::OnUnloadTimer"));
        REQUIRE(timer_func);
        REQUIRE(server->TimeEventMngr.StartTimeEvent(cr, Entity::TimeEventData::FuncType {std::move(timer_func)}, timespan {std::chrono::seconds {60}}, {}, {}) != 0);
    }

    size_t cancel_calls = 0;
    TimeEventManager::DispatcherHooks hooks;
    hooks.Cancel = [&](uint32_t) {
        cancel_calls++;
        throw GenericException("Injected time-event cancellation notification failure");
    };
    server->TimeEventMngr.SetDispatcherHooks(std::move(hooks));
    auto clear_dispatcher_hooks = scope_exit([&server]() noexcept { safe_call([&server] { server->TimeEventMngr.ClearDispatcherHooks(); }); });

    size_t cancellation_exception_reports = 0;
    auto previous_exception_callback = GetExceptionCallback();
    SetExceptionCallback([&cancellation_exception_reports](string_view message, const CatchedStackTraceData&, bool) {
        if (message.find("Injected time-event cancellation notification failure") != string_view::npos) {
            cancellation_exception_reports++;
        }
    });
    auto restore_exception_callback = scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { SetExceptionCallback(std::move(previous)); });

    REQUIRE_NOTHROW(server->TimeEventMngr.CancelAllForEntity(cr));
    CHECK(cancel_calls == event_count);
    CHECK(cancellation_exception_reports == event_count);
    CHECK_FALSE(cr->HasTimeEvents());

    server->TimeEventMngr.ClearDispatcherHooks();
    clear_dispatcher_hooks.release();
    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("DestroyingEntityRejectsNewTimeEvents")
{
    MAKE_LEM_SERVER();

    auto cr = server->CreateCritter(get_func("TestCritter"), false).hold_ref();
    bool finish_called = false;
    bool timer_func_resolved = false;
    string rejection;
    Entity::EventCallbackData callback;
    callback.Callback = [&server, &cr, &get_func, &finish_called, &timer_func_resolved, &rejection](FuncCallData&) {
        finish_called = true;
        auto timer_func = server->FindFunc<void>(get_func("LocEntity::OnUnloadTimer"));
        timer_func_resolved = static_cast<bool>(timer_func);

        if (timer_func) {
            try {
                (void)server->TimeEventMngr.StartTimeEvent(cr, Entity::TimeEventData::FuncType {std::move(timer_func)}, timespan {std::chrono::seconds {60}}, {}, {});
            }
            catch (const std::exception& ex) {
                rejection = ex.what();
            }
        }

        return Entity::EventResult::ContinueChain;
    };
    callback.SubscriptionPtr = reinterpret_cast<uintptr_t>(&finish_called);
    server->OnCritterFinish.Subscribe(std::move(callback));
    auto unsubscribe = scope_exit([&server, &finish_called]() noexcept { server->OnCritterFinish.Unsubscribe(reinterpret_cast<uintptr_t>(&finish_called)); });

    server->CrMngr.DestroyCritter(cr);

    CHECK(finish_called);
    CHECK(timer_func_resolved);
    CHECK(rejection.find("Cannot start a time event for an entity that is being destroyed") != string::npos);
    CHECK_FALSE(cr->HasTimeEvents());
}

FO_END_NAMESPACE
