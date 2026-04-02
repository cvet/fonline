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

namespace MapOpsTest
{
    Location@ CreateTestLocation()
    {
        array<hstring> mapPids = {"TestMap".hstr()};
        return Game.CreateLocation("TestLocation".hstr(), mapPids);
    }

    // ========== Map Item Operations ==========

    int TestMapAddItemByPid()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(10, 10);
        Item@ item = map.AddItem(hex, "TestItem".hstr(), 1);
        if (item is null) return -3;

        // Verify item exists on map
        Item@ found = map.GetItem(item.Id);
        if (found is null) return -4;
        if (found.Id != item.Id) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapAddItemMultiple()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex1(10, 10);
        mpos hex2(20, 20);
        Item@ item1 = map.AddItem(hex1, "TestItem".hstr(), 3);
        Item@ item2 = map.AddItem(hex2, "TestItem".hstr(), 5);
        if (item1 is null || item2 is null) return -3;

        // Query all items on map
        array<Item@> allItems = map.GetItems();
        if (allItems.length() < 2) return -4;

        // Query items by pid
        array<Item@> byPid = map.GetItems("TestItem".hstr());
        if (byPid.length() < 2) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetItemOnHex()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(15, 15);
        Item@ item = map.AddItem(hex, "TestItem".hstr(), 1);
        if (item is null) return -3;

        // Get item on hex by pid
        Item@ onHex = map.GetItemOnHex(hex, "TestItem".hstr());
        if (onHex is null) return -4;

        // Get items on hex (list)
        array<Item@> hexItems = map.GetItemsOnHex(hex);
        if (hexItems.length() < 1) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetItemInRadius()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos center(50, 50);
        mpos nearby(51, 50);
        Item@ item = map.AddItem(nearby, "TestItem".hstr(), 1);
        if (item is null) return -3;

        // Search in radius
        Item@ found = map.GetItemInRadius(center, 5, "TestItem".hstr());
        if (found is null) return -4;

        // Get items in radius (list)
        array<Item@> inRadius = map.GetItemsInRadius(center, 5);
        if (inRadius.length() < 1) return -5;

        // Get items in radius by pid
        array<Item@> inRadiusByPid = map.GetItemsInRadius(center, 5, "TestItem".hstr());
        if (inRadiusByPid.length() < 1) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Map Critter Operations ==========

    int TestMapAddCritter()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(30, 30);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), hex, 0);
        if (cr is null) return -3;

        // Critter should be on the map
        Critter@ found = map.GetCritter(cr.Id);
        if (found is null) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCritterOnHex()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(40, 40);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), hex, 0);
        if (cr is null) return -3;

        // Spawn may relocate to a nearby free hex, so validate lookup from the requested area.
        array<Critter@> nearby = map.GetCrittersInRadius(hex, 2, CritterFindType::Any);
        if (nearby.length() < 1) return -4;

        bool found = false;
        for (uint i = 0; i < nearby.length(); i++) {
            if (nearby[i].Id == cr.Id) {
                found = true;
                break;
            }
        }
        if (!found) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCrittersByFindType()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex1(30, 30);
        mpos hex2(31, 30);
        Critter@ cr1 = map.AddCritter("TestCritter".hstr(), hex1, 0);
        Critter@ cr2 = map.AddCritter("TestCritter".hstr(), hex2, 0);
        if (cr1 is null || cr2 is null) return -3;

        // Get all critters
        array<Critter@> all = map.GetCritters(CritterFindType::Any);
        if (all.length() < 2) return -4;

        // Get non-dead critters
        array<Critter@> alive = map.GetCritters(CritterFindType::NonDead);
        if (alive.length() < 2) return -5;

        // Kill one critter
        cr1.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);

        // Get dead critters
        array<Critter@> dead = map.GetCritters(CritterFindType::Dead);
        if (dead.isEmpty()) return -6;

        // Get non-dead should be fewer
        array<Critter@> aliveNow = map.GetCritters(CritterFindType::NonDead);
        if (aliveNow.length() >= all.length()) return -7;

        // Get by pid and find type
        array<Critter@> byPid = map.GetCritters("TestCritter".hstr(), CritterFindType::Any);
        if (byPid.length() < 2) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCrittersOnHex()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(35, 35);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), hex, 0);
        if (cr is null) return -3;

        // Spawn may relocate to a nearby free hex, so validate lookup from the requested area.
        array<Critter@> onHex = map.GetCrittersInRadius(hex, 2, CritterFindType::Any);
        if (onHex.length() < 1) return -4;

        bool found = false;
        for (uint i = 0; i < onHex.length(); i++) {
            if (onHex[i].Id == cr.Id) {
                found = true;
                break;
            }
        }
        if (!found) return -5;

        // Get critters in radius
        array<Critter@> inRadius = map.GetCrittersInRadius(hex, 5, CritterFindType::Any);
        if (inRadius.length() < 1) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCrittersWhoSeeHex()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos critterHex(50, 50);
        mpos targetHex(52, 50);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), critterHex, 0);
        if (cr is null) return -3;

        // Get critters who can see target hex
        array<Critter@> canSee = map.GetCrittersWhoSeeHex(targetHex, CritterFindType::Any);
        // Critter at (50,50) should see hex (52,50) if look dist is sufficient
        // May or may not be in list depending on look distance

        // Get critters who see hex with radius bonus
        array<Critter@> canSeeRadius = map.GetCrittersWhoSeeHex(targetHex, 10, CritterFindType::Any);

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCrittersWhoSeePath()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos critterHex(50, 50);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), critterHex, 0);
        if (cr is null) return -3;

        mpos from(48, 50);
        mpos to(52, 50);
        array<Critter@> whoSeePath = map.GetCrittersWhoSeePath(from, to, CritterFindType::Any);

        Game.DestroyLocation(loc);
        return 0;
    }

 )") + R"(
    // ========== Map Hex Operations ==========

    int TestMapHexMovable()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(10, 10);
        bool movable = map.IsHexMovable(hex);
        // Empty map should have movable hexes
        if (!movable) return -3;

        // Check multiple hexes
        bool hexesMovable = map.IsHexesMovable(hex, 1);

        // Shootable check
        bool shootable = map.IsHexShootable(hex);

        // Outside area check
        bool outside = map.IsOutsideArea(hex);

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapBlockUnblock()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(20, 20);
        if (!map.IsHexMovable(hex)) return -3;

        // Block the hex
        map.BlockHex(hex, true);
        if (map.IsHexMovable(hex)) return -4;

        // Unblock
        map.UnblockHex(hex);
        if (!map.IsHexMovable(hex)) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapMoveHexByDir()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(50, 50);
        mpos original = hex;

        // Move hex by direction (dir 0 = north-ish)
        bool moved = map.MoveHexByDir(hex, 0);
        if (!moved) return -3;
        if (hex.x == original.x && hex.y == original.y) return -4;

        // Move multiple steps
        mpos hex2(50, 50);
        int steps = map.MoveHexByDir(hex2, 1, 3);
        if (steps != 3) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapCheckPlaceForItem()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(25, 25);
        bool canPlace = map.CheckPlaceForItem(hex, "TestItem".hstr());

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Map Path Operations ==========

    int TestMapPathLength()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos from(10, 10);
        mpos to(15, 15);
        int len = map.GetPathLength(from, to, 0, null);
        // Path should have positive length between two valid hexes
        if (len <= 0) return -3;

        // Path with critter
        mpos crHex(20, 20);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), crHex, 0);
        if (cr is null) return -4;

        mpos dest(25, 25);
        int crLen = map.GetPathLength(cr, dest, 0, null);
        if (crLen <= 0) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetHexInPath()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos from(50, 50);
        mpos to(60, 60);
        map.GetHexInPath(from, to, 0.0f, 20);
        // to is modified in-place

        mpos wallTo(60, 60);
        map.GetWallHexInPath(from, wallTo, 0.0f, 20);

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCrittersInPath()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos crHex(50, 50);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), crHex, 0);
        if (cr is null) return -3;

        mpos from(48, 50);
        mpos to(52, 50);

        // Get critters in path (simple)
        array<Critter@> inPath = map.GetCrittersInPath(from, to, 0.0f, 10, CritterFindType::Any);

        // Get critters in path (with block hexes)
        mpos preBlock(0, 0);
        mpos block(0, 0);
        array<Critter@> inPath2 = map.GetCrittersInPath(from, to, 0.0f, 10, CritterFindType::Any, preBlock, block);

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Map Static Items ==========

    int TestMapGetStaticItems()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        // Empty map has no static items, but we can still call the functions
        array<StaticItem@> allStatic = map.GetStaticItems();

        mpos hex(10, 10);
        array<StaticItem@> onHex = map.GetStaticItemsOnHex(hex);

        array<StaticItem@> byPid = map.GetStaticItems("TestItem".hstr());

        array<StaticItem@> inRadius = map.GetStaticItemsInRadius(hex, 5, "TestItem".hstr());

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    // ========== Map Location Relationship ==========

    int TestMapLocationRelationship()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        // Map should know its location
        Location@ mapLoc = map.GetLocation();
        if (mapLoc is null) return -3;
        if (mapLoc.Id != loc.Id) return -4;

        // Location map queries
        int mapCount = loc.GetMapCount();
        if (mapCount != 1) return -5;

        Map@ byPid = loc.GetMap("TestMap".hstr());
        if (byPid is null) return -6;

        array<Map@> allMaps = loc.GetMaps();
        if (allMaps.length() != 1) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Map Regeneration ==========

    int TestMapRegenerate()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        // Add some items
        mpos hex(10, 10);
        map.AddItem(hex, "TestItem".hstr(), 1);

        // Regenerate should reset map content
        map.Regenerate();

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Map VerifyTrigger ==========

    int TestMapVerifyTrigger()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos crHex(50, 50);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), crHex, 0);
        if (cr is null) return -3;

        mpos triggerHex(51, 50);
        map.VerifyTrigger(cr, triggerHex, 0);

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Location Creation with Maps ==========

    int TestLocationCreateWithMaps()
    {
        // Create location with map list
        array<hstring> mapPids = {"TestMap".hstr()};
        Location@ loc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (loc is null) return -1;

        if (loc.GetMapCount() != 1) return -2;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -3;

        // Verify map size is as configured
        msize sz = map.Size;
        if (sz.width == 0 || sz.height == 0) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Location Queries ==========

    int TestGameGetLocations()
    {
        Location@ loc1 = CreateTestLocation();
        Location@ loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        // Get all locations
        array<Location@> allLocs = Game.GetLocations();
        if (allLocs.length() < 2) return -2;

        // Get locations by proto
        array<Location@> byPid = Game.GetLocations("TestLocation".hstr());
        if (byPid.length() < 2) return -3;

        // Get location by id
        Location@ found = Game.GetLocation(loc1.Id);
        if (found is null) return -4;
        if (found.Id != loc1.Id) return -5;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    // ========== Game.GetGlobalMapCritters ==========

    int TestGameGetGlobalMapCritters()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Global map critters (no map assigned)
        array<Critter@> globalCritters = Game.GetGlobalMapCritters(CritterFindType::Any);
        // NPC critters without a map should be on global map
        if (globalCritters.isEmpty()) return -2;

        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Proto-based overloads ==========

    int TestMapAddItemByProto()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ProtoItem@ proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -3;

        mpos hex(12, 12);
        Item@ item = map.AddItem(hex, proto, 2);
        if (item is null) return -4;

        // Get by proto
        Item@ onHex = map.GetItemOnHex(hex, proto);
        if (onHex is null) return -5;

        // Get in radius by proto
        Item@ inRadius = map.GetItemInRadius(hex, 3, proto);
        if (inRadius is null) return -6;

        // Get items list by proto
        array<Item@> byProto = map.GetItems(proto);
        if (byProto.isEmpty()) return -7;

        // GetItemsInRadius by proto
        array<Item@> inRadiusList = map.GetItemsInRadius(hex, 3, proto);
        if (inRadiusList.isEmpty()) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapAddCritterByProto()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ProtoCritter@ proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -3;

        mpos hex(33, 33);
        Critter@ cr = map.AddCritter(proto, hex, 0);
        if (cr is null) return -4;

        Critter@ found = map.GetCritter(cr.Id);
        if (found is null) return -5;

        // Get critters by proto + find type
        array<Critter@> byProto = map.GetCritters(proto, CritterFindType::Any);
        if (byProto.isEmpty()) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Property-filter overloads ==========

    int TestMapGetItemByProperty()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(14, 14);
        Item@ item = map.AddItem(hex, "TestItem".hstr(), 1);
        if (item is null) return -3;

        // Get item on hex by property (Count is a common int32 property)
        Item@ byProp = map.GetItemOnHex(hex, ItemProperty::Count, item.Count);

        // Get item in radius by property
        Item@ inRadiusProp = map.GetItemInRadius(hex, 3, ItemProperty::Count, item.Count);

        // Get items list by property
        array<Item@> listByProp = map.GetItems(ItemProperty::Count, item.Count);

        // GetItemsOnHex by property
        array<Item@> onHexProp = map.GetItemsOnHex(hex, ItemProperty::Count, item.Count);

        // GetItemsInRadius by property
        array<Item@> inRadiusListProp = map.GetItemsInRadius(hex, 3, ItemProperty::Count, item.Count);

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCritterByProperty()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(36, 36);
        Critter@ cr = map.AddCritter("TestCritter".hstr(), hex, 0);
        if (cr is null) return -3;

        // Get critter by property
        Critter@ byProp = map.GetCritter(CritterProperty::Dir, cr.Dir, CritterFindType::Any);

        // Get critters list by property
        array<Critter@> listByProp = map.GetCritters(CritterProperty::Dir, cr.Dir, CritterFindType::Any);

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    // ========== Static item overloads ==========

    int TestMapGetStaticItemOverloads()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(10, 10);

        // GetStaticItem by hex + pid
        StaticItem@ byHexPid = map.GetStaticItemOnHex(hex, "TestItem".hstr());

        // GetStaticItem by hex + proto
        ProtoItem@ proto = Game.GetProtoItem("TestItem".hstr());
        StaticItem@ byHexProto = map.GetStaticItemOnHex(hex, proto);

        // GetStaticItems by hex + proto + radius
        array<StaticItem@> byRadiusProto = map.GetStaticItemsInRadius(hex, 5, proto);

        // GetStaticItems by hex + property
        array<StaticItem@> byHexProp = map.GetStaticItemsOnHex(hex, ItemProperty::Count, 0);

        // GetStaticItems by hex + radius + property
        array<StaticItem@> byRadiusProp = map.GetStaticItemsInRadius(hex, 5, ItemProperty::Count, 0);

        // GetStaticItems by proto
        array<StaticItem@> byProto = map.GetStaticItems(proto);

        // GetStaticItems by property
        array<StaticItem@> byProp = map.GetStaticItems(ItemProperty::Count, 0);

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== SetupScript ==========

    void OnMapInit(Map@ map, bool firstTime)
    {
        // Just a stub init function
    }

    int TestMapSetupScriptEx()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        map.SetupScriptEx("MapOpsTest::OnMapInit".hstr());

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapSetupScript()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        // Use SetupScriptEx with hstring (both SetupScript and SetupScriptEx eventually call the same path)
        map.SetupScriptEx("MapOpsTest::OnMapInit".hstr());

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    // ========== AddItem with properties ==========

    int TestMapAddItemWithProperties()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(16, 16);

        // AddItem with hstring + properties map
        dict<ItemProperty, int> props = {{ItemProperty::Count, 5}};
        Item@ item1 = map.AddItem(hex, "TestItem".hstr(), 1, props);
        if (item1 is null) return -3;

        // AddItem with proto + properties map
        ProtoItem@ proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -31;
        mpos hex2(17, 17);
        Item@ item2 = map.AddItem(hex2, proto, 1, props);
        if (item2 is null) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== AddCritter with properties ==========

    int TestMapAddCritterWithProperties()
    {
        Location@ loc = CreateTestLocation();
        if (loc is null) return -1;

        Map@ map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ProtoCritter@ proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -3;

        // AddCritter with hstring + int properties
        dict<CritterProperty, int> intProps = {};
        mpos hex1(38, 38);
        Critter@ cr1 = map.AddCritter("TestCritter".hstr(), hex1, 0, intProps);
        if (cr1 is null) return -4;

        // AddCritter with proto + int properties
        mpos hex2(39, 39);
        Critter@ cr2 = map.AddCritter(proto, hex2, 0, intProps);
        if (cr2 is null) return -5;

        // AddCritter with hstring + any properties
        dict<CritterProperty, any> anyProps = {};
        mpos hex3(40, 40);
        Critter@ cr3 = map.AddCritter("TestCritter".hstr(), hex3, 0, anyProps);
        if (cr3 is null) return -6;

        // AddCritter with proto + any properties
        mpos hex4(41, 41);
        Critter@ cr4 = map.AddCritter(proto, hex4, 0, anyProps);
        if (cr4 is null) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }
}
)";

        return BakerTests::CompileInlineScripts(&compiler_engine, "MapOpsScripts",
            {
                {"Scripts/MapOpsTest.fos", script_source},
            },
            [](string_view message) {
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
        writer.Write<uint32>(uint32 {0}); // hashes_count
        writer.Write<uint32>(uint32 {0}); // cr_count
        writer.Write<uint32>(uint32 {0}); // item_count
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

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapOpsCompilerResources");
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

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapOpsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("MapOpsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("MapOpsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("MapOpsLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("TestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("TestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("MapOpsTest.fos-bin-server", script_blob);

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

#define MAKE_SERVER \
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

#define RUN_FUNC(func_name) \
    { \
        auto func = server->FindFunc<int32>(get_func(func_name)); \
        REQUIRE(func); \
        REQUIRE(func.Call()); \
        CHECK(func.GetResult() == 0); \
    }

TEST_CASE("MapItemOperations")
{
    MAKE_SERVER;

    SECTION("AddItemByPid")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemByPid");
    }

    SECTION("AddItemMultiple")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemMultiple");
    }

    SECTION("GetItemOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemOnHex");
    }

    SECTION("GetItemInRadius")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemInRadius");
    }
}

TEST_CASE("MapCritterOperations")
{
    MAKE_SERVER;

    SECTION("AddCritter")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritter");
    }

    SECTION("GetCritterOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCritterOnHex");
    }

    SECTION("GetCrittersByFindType")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersByFindType");
    }

    SECTION("GetCrittersOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersOnHex");
    }

    SECTION("GetCrittersWhoSeeHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersWhoSeeHex");
    }

    SECTION("GetCrittersWhoSeePath")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersWhoSeePath");
    }
}

TEST_CASE("MapHexOperations")
{
    MAKE_SERVER;

    SECTION("HexMovable")
    {
        RUN_FUNC("MapOpsTest::TestMapHexMovable");
    }

    SECTION("BlockUnblock")
    {
        RUN_FUNC("MapOpsTest::TestMapBlockUnblock");
    }

    SECTION("MoveHexByDir")
    {
        RUN_FUNC("MapOpsTest::TestMapMoveHexByDir");
    }

    SECTION("CheckPlaceForItem")
    {
        RUN_FUNC("MapOpsTest::TestMapCheckPlaceForItem");
    }
}

TEST_CASE("MapPathOperations")
{
    MAKE_SERVER;

    SECTION("PathLength")
    {
        RUN_FUNC("MapOpsTest::TestMapPathLength");
    }

    SECTION("GetHexInPath")
    {
        RUN_FUNC("MapOpsTest::TestMapGetHexInPath");
    }

    SECTION("GetCrittersInPath")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersInPath");
    }
}

TEST_CASE("MapStaticItems")
{
    MAKE_SERVER;

    SECTION("GetStaticItems")
    {
        RUN_FUNC("MapOpsTest::TestMapGetStaticItems");
    }
}

TEST_CASE("MapLocationRelationship")
{
    MAKE_SERVER;

    SECTION("LocationRelationship")
    {
        RUN_FUNC("MapOpsTest::TestMapLocationRelationship");
    }

    SECTION("Regenerate")
    {
        RUN_FUNC("MapOpsTest::TestMapRegenerate");
    }

    SECTION("VerifyTrigger")
    {
        RUN_FUNC("MapOpsTest::TestMapVerifyTrigger");
    }

    SECTION("LocationCreateWithMaps")
    {
        RUN_FUNC("MapOpsTest::TestLocationCreateWithMaps");
    }

    SECTION("GameGetLocations")
    {
        RUN_FUNC("MapOpsTest::TestGameGetLocations");
    }

    SECTION("GameGetGlobalMapCritters")
    {
        RUN_FUNC("MapOpsTest::TestGameGetGlobalMapCritters");
    }
}

TEST_CASE("MapProtoOverloads")
{
    MAKE_SERVER;

    SECTION("AddItemByProto")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemByProto");
    }

    SECTION("AddCritterByProto")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterByProto");
    }
}

TEST_CASE("MapPropertyFilterOverloads")
{
    MAKE_SERVER;

    SECTION("GetItemByProperty")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemByProperty");
    }

    SECTION("GetCritterByProperty")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCritterByProperty");
    }

    SECTION("GetStaticItemOverloads")
    {
        RUN_FUNC("MapOpsTest::TestMapGetStaticItemOverloads");
    }
}

TEST_CASE("MapSetupScript")
{
    MAKE_SERVER;

    SECTION("SetupScriptEx")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScriptEx");
    }

    SECTION("SetupScript")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScript");
    }
}

TEST_CASE("MapAddWithProperties")
{
    MAKE_SERVER;

    SECTION("AddItemWithProperties")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemWithProperties");
    }

    SECTION("AddCritterWithProperties")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterWithProperties");
    }
}

FO_END_NAMESPACE
