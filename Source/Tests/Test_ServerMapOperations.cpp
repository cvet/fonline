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
#include "MapBaker.h"
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

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        const auto script_source = string(R"(

namespace MapOpsTest
{
    Location CreateTestLocation()
    {
        array<hstring> mapPids = {"TestMap".hstr()};
        return Game.CreateLocation("TestLocation".hstr(), mapPids);
    }

    bool AllowItemPathGag(Item item)
    {
        return item.Id != ZERO_IDENT;
    }

    bool AllowCritterPathGag(Critter cr, Item item)
    {
        return cr.Id != ZERO_IDENT && item.Id != ZERO_IDENT;
    }

    // ========== Map Item Operations ==========

    int TestMapAddItemByPid()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(10, 10);
        Item item = map.AddItem(hex, "TestItem".hstr(), 1);
        if (item is null) return -3;

        // Verify item exists on map
        Item? found = map.GetItem(item.Id);
        if (found is null) return -4;
        if (found.Id != item.Id) return -5;
        if (map.GetItem(ZERO_IDENT) !is null) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapAddItemInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.AddItem(mpos(-1, -1), "TestItem".hstr(), 1);
    }

    void TestMapAddItemInvalidCountThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.AddItem(mpos(10, 10), "TestItem".hstr(), 0);
    }

    int TestMapAddItemMultiple()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex1(10, 10);
        mpos hex2(20, 20);
        Item item1 = map.AddItem(hex1, "TestItem".hstr(), 3);
        Item item2 = map.AddItem(hex2, "TestItem".hstr(), 5);
        if (item1 is null || item2 is null) return -3;

        // Query all items on map
        array<Item> allItems = map.GetItems();
        if (allItems.length() < 2) return -4;

        // Query items by pid
        array<Item> byPid = map.GetItems("TestItem".hstr());
        if (byPid.length() < 2) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestGameDistanceOverloads()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr1 = map.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        Critter cr2 = map.AddCritter("TestCritter".hstr(), mpos(13, 10), mdir(0));
        if (cr1 is null || cr2 is null) return -3;

        Item item1 = map.AddItem(mpos(11, 10), "TestItem".hstr(), 1);
        Item item2 = map.AddItem(mpos(15, 10), "TestItem2".hstr(), 1);
        if (item1 is null || item2 is null) return -4;

        int itemDistance = Game.GetDistance(item1, item2);
        if (itemDistance != Game.GetDistance(mpos(11, 10), mpos(15, 10))) return -5;

        int critterDistance = Game.GetDistance(cr1, cr2);
        if (critterDistance <= 0) return -6;

        if (Game.GetDistance(cr1, item1) != Game.GetDistance(item1, cr1)) return -7;
        if (Game.GetDistance(cr2, item2) != Game.GetDistance(item2, cr2)) return -8;
        if (Game.GetDistance(cr1, mpos(14, 10)) != Game.GetDistance(mpos(14, 10), cr1)) return -9;
        if (Game.GetDistance(item2, mpos(10, 10)) != Game.GetDistance(mpos(10, 10), item2)) return -10;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestGameDistanceExceptionBranches()
    {
        Critter looseCr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter looseCr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (looseCr1 is null || looseCr2 is null) return -1;

        Item looseItem1 = looseCr1.AddItem("TestItem".hstr(), 1);
        Item looseItem2 = looseCr2.AddItem("TestItem".hstr(), 1);
        if (looseItem1 is null || looseItem2 is null) return -2;

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -3;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -4;

        Critter mapCr1 = map1.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        Critter mapCr2 = map2.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        if (mapCr1 is null || mapCr2 is null) return -5;

        Item mapItem1 = map1.AddItem(mpos(11, 10), "TestItem".hstr(), 1);
        Item mapItem2 = map2.AddItem(mpos(11, 10), "TestItem".hstr(), 1);
        if (mapItem1 is null || mapItem2 is null) return -6;

        try {
            int distance = Game.GetDistance(looseCr1, looseCr2);
            if (distance >= 0) return -7;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(looseItem1, looseItem2);
            if (distance >= 0) return -8;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(looseCr1, looseItem1);
            if (distance >= 0) return -9;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(looseItem1, looseCr1);
            if (distance >= 0) return -10;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(looseCr1, mpos(1, 1));
            if (distance >= 0) return -11;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(mpos(1, 1), looseCr1);
            if (distance >= 0) return -12;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(looseItem1, mpos(1, 1));
            if (distance >= 0) return -13;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(mpos(1, 1), looseItem1);
            if (distance >= 0) return -14;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(mapCr1, mapCr2);
            if (distance >= 0) return -15;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(mapItem1, mapItem2);
            if (distance >= 0) return -16;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(mapCr1, mapItem2);
            if (distance >= 0) return -17;
        }
        catch {
        }

        try {
            int distance = Game.GetDistance(mapItem1, mapCr2);
            if (distance >= 0) return -18;
        }
        catch {
        }

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        Game.DestroyCritter(looseCr1);
        Game.DestroyCritter(looseCr2);

        return 0;
    }

    int TestGameMoveItemMapAndContainerOverloads()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        if (cr is null) return -3;

        Item item = cr.AddItem("TestItem".hstr(), 4);
        Item container = cr.AddItem("TestItem2".hstr(), 1);
        if (item is null || container is null) return -4;

        if (Game.MoveItem(item, 0, cr) !is null) return -5;
        if (Game.MoveItem(item, 0, map, mpos(20, 20)) !is null) return -6;
        if (Game.MoveItem(item, 0, container) !is null) return -7;

        Item? movedPartialToMap = Game.MoveItem(item, 1, map, mpos(20, 20));
        if (movedPartialToMap is null) return -8;

        Item item2 = cr.AddItem("TestItem".hstr(), 1);
        if (item2 is null) return -9;

        Item? movedAllToMap = Game.MoveItem(item2, map, mpos(21, 20));
        if (movedAllToMap is null) return -10;

        Item item3 = cr.AddItem("TestItem".hstr(), 1);
        if (item3 is null) return -11;
)"
                                          R"(
        Item? movedPartialToContainer = Game.MoveItem(item3, 1, container);
        if (movedPartialToContainer is null) return -12;

        Item item4 = cr.AddItem("TestItem".hstr(), 1);
        if (item4 is null) return -13;

        Item? movedAllToContainer = Game.MoveItem(item4, container);
        if (movedAllToContainer is null) return -14;

        bool caught = false;

        try {
            Game.MoveItem(item, map, mpos(-1, -1));
        }
        catch {
            caught = true;
        }
        if (!caught) return -15;

        caught = false;
        try {
            Game.MoveItem(item, 1, map, mpos(-1, -1));
        }
        catch {
            caught = true;
        }
        if (!caught) return -16;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestGameMoveItemsOverloads()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter source = map.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        Critter receiver = map.AddCritter("TestCritter".hstr(), mpos(11, 10), mdir(0));
        if (source is null || receiver is null) return -3;

        Item crItem1 = source.AddItem("TestItem".hstr(), 1);
        Item crItem2 = source.AddItem("TestItem2".hstr(), 1);
        if (crItem1 is null || crItem2 is null) return -4;

        array<Item> toCritter = {crItem1, crItem2};
        Game.MoveItems(toCritter, receiver);

        if (receiver.CountItem("TestItem".hstr()) != 1) return -5;
        if (receiver.CountItem("TestItem2".hstr()) != 1) return -6;

        Item destroyedCrItem = source.AddItem("TestItem".hstr(), 1);
        if (destroyedCrItem is null) return -14;
        Game.DestroyItem(destroyedCrItem);

        array<Item> destroyedToCritter = {destroyedCrItem};
        Game.MoveItems(destroyedToCritter, receiver);

        Item mapItem1 = source.AddItem("TestItem".hstr(), 1);
        Item mapItem2 = source.AddItem("TestItem2".hstr(), 1);
        if (mapItem1 is null || mapItem2 is null) return -7;

        ident mapItem1Id = mapItem1.Id;
        ident mapItem2Id = mapItem2.Id;

        array<Item> toMap = {mapItem1, mapItem2};
        Game.MoveItems(toMap, map, mpos(25, 25));

        if (map.GetItem(mapItem1Id) is null) return -8;
        if (map.GetItem(mapItem2Id) is null) return -9;

        Item destroyedMapItem = source.AddItem("TestItem".hstr(), 1);
        if (destroyedMapItem is null) return -15;
        Game.DestroyItem(destroyedMapItem);

        array<Item> destroyedToMap = {destroyedMapItem};
        Game.MoveItems(destroyedToMap, map, mpos(26, 26));
)"
                                          R"(
        Item container = source.AddItem("TestItem2".hstr(), 1);
        Item contItem1 = source.AddItem("TestItem".hstr(), 1);
        Item contItem2 = source.AddItem("TestItem2".hstr(), 1);
        if (container is null || contItem1 is null || contItem2 is null) return -10;

        array<Item> toContainer = {contItem1, contItem2};
        Game.MoveItems(toContainer, container);

        array<Item> containerItems = container.GetItems();
        if (containerItems.length() != 2) return -11;

        Item destroyedContItem = source.AddItem("TestItem".hstr(), 1);
        if (destroyedContItem is null) return -16;
        Game.DestroyItem(destroyedContItem);

        array<Item> destroyedToContainer = {destroyedContItem};
        Game.MoveItems(destroyedToContainer, container);

        Item badItem = source.AddItem("TestItem".hstr(), 1);
        if (badItem is null) return -12;

        array<Item> badItems = {badItem};
        bool caught = false;

        try {
            Game.MoveItems(badItems, map, mpos(-1, -1));
        }
        catch {
            caught = true;
        }
        if (!caught) return -13;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestCreateLocationInvalidProtoPropsThrows()
    {
        dict<LocationProperty, any> props = {{LocationProperty::InitScript, "MapOpsTest::OnLocationInit".hstr()}};
        Game.CreateLocation("MissingLocation".hstr(), props);
    }

    void TestCreateLocationInvalidProtoMapPropsThrows()
    {
        array<hstring> mapPids = {"TestMap".hstr()};
        dict<LocationProperty, any> props = {{LocationProperty::InitScript, "MapOpsTest::OnLocationInit".hstr()}};
        Game.CreateLocation("MissingLocation".hstr(), mapPids, props);
    }

    int TestGameDestroyItemByIdCountOverload()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        if (cr is null) return -3;

        Item item = cr.AddItem("TestItem".hstr(), 4);
        if (item is null) return -4;

        ident itemId = item.Id;

        Game.DestroyItem(itemId, 0);

        Item? unchanged = Game.GetItem(itemId);
        if (unchanged is null) return -5;
        if (unchanged.Count != 4) return -6;

        Game.DestroyItem(itemId, 1);

        Item? reduced = Game.GetItem(itemId);
        if (reduced is null) return -7;
        if (reduced.Count != 3) return -8;

        Game.DestroyItem(itemId, 3);
        if (Game.GetItem(itemId) !is null) return -9;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetItemOnHex()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(15, 15);
        Item item = map.AddItem(hex, "TestItem".hstr(), 1);
        if (item is null) return -3;

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -4;

        // Get item on hex by pid
        Item? onHex = map.GetItemOnHex(hex, "TestItem".hstr());
        if (onHex is null) return -5;

        // Get item on hex by proto
        Item? onHexProto = map.GetItemOnHex(hex, proto);
        if (onHexProto is null) return -6;

        // Get item on hex by property
        Item? onHexProp = map.GetItemOnHex(hex, ItemProperty::Count, item.Count);
        if (onHexProp is null) return -7;

        // Get items on hex (list)
        array<Item> hexItems = map.GetItemsOnHex(hex);
        if (hexItems.length() < 1) return -8;

        // Get items on hex by property
        array<Item> hexItemsProp = map.GetItemsOnHex(hex, ItemProperty::Count, item.Count);
        if (hexItemsProp.length() < 1) return -9;

        if (map.GetItemOnHex(hex, ItemProperty::Count, item.Count + 1) !is null) return -10;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapGetItemOnHexInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetItemOnHex(mpos(-1, -1), "TestItem".hstr());
    }

    void TestMapGetItemOnHexByProtoInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) {
            return;
        }

        map.GetItemOnHex(mpos(-1, -1), proto);
    }

    void TestMapGetItemOnHexByPropertyInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetItemOnHex(mpos(-1, -1), ItemProperty::Count, 1);
    }

    int TestMapGetItemInRadius()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos center(50, 50);
        mpos nearby(51, 50);
        Item item = map.AddItem(nearby, "TestItem".hstr(), 1);
        if (item is null) return -3;
)"
                                          R"(
        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -4;

        // Search in radius
        Item? found = map.GetItemInRadius(center, 5, "TestItem".hstr());
        if (found is null) return -5;

        // Search in radius by proto
        Item? foundProto = map.GetItemInRadius(center, 5, proto);
        if (foundProto is null) return -6;

        // Search in radius by property
        Item? foundProp = map.GetItemInRadius(center, 5, ItemProperty::Count, item.Count);
        if (foundProperty is null) return -7;

        // Get items in radius (list)
        array<Item> inRadius = map.GetItemsInRadius(center, 5);
        if (inRadius.length() < 1) return -8;

        // Get items in radius by pid
        array<Item> inRadiusByPid = map.GetItemsInRadius(center, 5, "TestItem".hstr());
        if (inRadiusByPid.length() < 1) return -9;

        // Get items in radius by proto
        array<Item> inRadiusByProto = map.GetItemsInRadius(center, 5, proto);
        if (inRadiusByProto.length() < 1) return -10;

        // Get items in radius by property
        array<Item> inRadiusByProp = map.GetItemsInRadius(center, 5, ItemProperty::Count, item.Count);
        if (inRadiusByProp.length() < 1) return -11;

        // Get items across the whole map by proto and property
        array<Item> mapItemsByProto = map.GetItems(proto);
        if (mapItemsByProto.length() < 1) return -12;

        array<Item> mapItemsByProp = map.GetItems(ItemProperty::Count, item.Count);
        if (mapItemsByProp.length() < 1) return -13;

        if (map.GetItemInRadius(center, 5, "MissingItem".hstr()) !is null) return -14;

        ProtoItem? otherProto = Game.GetProtoItem("TestItem2".hstr());
        if (otherProto is null) return -15;
        if (map.GetItemInRadius(center, 5, otherProto) !is null) return -16;
        if (map.GetItemInRadius(center, 5, ItemProperty::Count, item.Count + 1) !is null) return -17;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapGetItemsInRadiusNegativeRadiusThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetItemsInRadius(mpos(50, 50), -1);
    }

    void TestMapGetItemsInRadiusInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetItemsInRadius(mpos(-1, -1), 1);
    }

    int TestMapGetItemsByNullProtoThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return -1;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return -2;
        }

        ProtoItem? proto = null;
        bool caught = false;
        try {
            map.GetItems(proto);
        }
        catch {
            caught = true;
        }

        Game.DestroyLocation(loc);
        return caught ? 0 : -3;
    }

    // ========== Map Critter Operations ==========

    int TestMapAddCritter()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(30, 30);
        Critter cr = map.AddCritter("TestCritter".hstr(), hex, mdir(0));
        if (cr is null) return -3;

        // Critter should be on the map
        Critter? found = map.GetCritter(cr.Id);
        if (found is null) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapAddCritterInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.AddCritter("TestCritter".hstr(), mpos(-1, -1), mdir(0));
    }

    void TestMapAddCritterByProtoInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        ProtoCritter? proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) {
            return;
        }

        map.AddCritter(proto, mpos(-1, -1), mdir(0));
    }

    int TestMapGetCritterOnHex()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(40, 40);
        Critter cr = map.AddCritter("TestCritter".hstr(), hex, mdir(0));
        if (cr is null) return -3;

        Critter? exact = map.GetCritterOnHex(cr.Hex);
        if (exact is null) return -4;

        cr.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);
        Critter? deadExact = map.GetCritterOnHex(cr.Hex);
        if (deadExact is null) return -5;

        // Spawn may relocate to a nearby free hex, so validate lookup from the requested area.
        array<Critter> nearby = map.GetCrittersInRadius(hex, 2, CritterFindType::Any);
        if (nearby.length() < 1) return -6;

        bool found = false;
        for (uint i = 0; i < nearby.length(); i++) {
            if (nearby[i].Id == cr.Id) {
                found = true;
                break;
            }
        }
        if (!found) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapGetCritterOnHexInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
)"
                                          R"(
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetCritterOnHex(mpos(-1, -1));
    }

    int TestMapGetCrittersByFindType()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex1(30, 30);
        mpos hex2(31, 30);
        Critter cr1 = map.AddCritter("TestCritter".hstr(), hex1, mdir(0));
        Critter cr2 = map.AddCritter("TestCritter".hstr(), hex2, mdir(0));
        if (cr1 is null || cr2 is null) return -3;

        // Get all critters
        array<Critter> all = map.GetCritters(CritterFindType::Any);
        if (all.length() < 2) return -4;

        // Get non-dead critters
        array<Critter> alive = map.GetCritters(CritterFindType::NonDead);
        if (alive.length() < 2) return -5;

        // Kill one critter
        cr1.SetCondition(CritterCondition::Dead, CritterActionAnim::None, null);

        // Get dead critters
        array<Critter> dead = map.GetCritters(CritterFindType::Dead);
        if (dead.isEmpty()) return -6;

        // Get non-dead should be fewer
        array<Critter> aliveNow = map.GetCritters(CritterFindType::NonDead);
        if (aliveNow.length() >= all.length()) return -7;

        // Get by pid and find type
        array<Critter> byPid = map.GetCritters("TestCritter".hstr(), CritterFindType::Any);
        if (byPid.length() < 2) return -8;

        // Get by proto and find type
        ProtoCritter? proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -9;
        array<Critter> byProto = map.GetCritters(proto, CritterFindType::Any);
        if (byProto.length() < 2) return -10;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCrittersOnHex()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(35, 35);
        Critter cr = map.AddCritter("TestCritter".hstr(), hex, mdir(0));
        if (cr is null) return -3;

        // Spawn may relocate to a nearby free hex, so validate lookup from the requested area.
        array<Critter> onHex = map.GetCrittersInRadius(hex, 2, CritterFindType::Any);
        if (onHex.length() < 1) return -4;

        bool found = false;
        for (uint i = 0; i < onHex.length(); i++) {
            if (onHex[i].Id == cr.Id) {
                found = true;
                break;
            }
        }
        if (!found) return -5;

        array<Critter> exactHex = map.GetCrittersOnHex(cr.Hex, CritterFindType::Any);
        if (exactHex.length() < 1) return -6;

        array<Critter> zeroRadius = map.GetCrittersInRadius(cr.Hex, 0, CritterFindType::Any);
        if (zeroRadius.length() < 1) return -7;

        // Get critters in radius
        array<Critter> inRadius = map.GetCrittersInRadius(hex, 5, CritterFindType::Any);
        if (inRadius.length() < 1) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapGetCrittersInRadiusNegativeRadiusThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetCrittersInRadius(mpos(50, 50), -1, CritterFindType::Any);
    }

    void TestMapGetCrittersInRadiusInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetCrittersInRadius(mpos(-1, -1), 1, CritterFindType::Any);
    }

    int TestMapGetCrittersWhoSeeHex()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos critterHex(50, 50);
        mpos targetHex(52, 50);
        Critter cr = map.AddCritter("TestCritter".hstr(), critterHex, mdir(0));
        if (cr is null) return -3;
        cr.LookDistance = 10;

        // Get critters who can see target hex
        array<Critter> canSee = map.GetCrittersWhoSeeHex(targetHex, CritterFindType::Any);
        // Critter at (50,50) should see hex (52,50) if look dist is sufficient
        // May or may not be in list depending on look distance
        if (canSee.isEmpty()) return -4;

        // Get critters who see hex with radius bonus
        array<Critter> canSeeRadius = map.GetCrittersWhoSeeHex(targetHex, 10, CritterFindType::Any);
        if (canSeeRadius.isEmpty()) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCrittersWhoSeePath()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos critterHex(50, 50);
        Critter cr = map.AddCritter("TestCritter".hstr(), critterHex, mdir(0));
        if (cr is null) return -3;
        cr.LookDistance = 10;

        mpos from(48, 50);
        mpos to(52, 50);
        array<Critter> whoSeePath = map.GetCrittersWhoSeePath(from, to, CritterFindType::Any);
        if (whoSeePath.isEmpty()) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

 )") + R"(
    // ========== Map Hex Operations ==========

    int TestMapHexMovable()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(10, 10);
        bool movable = map.IsHexMovable(hex);
        // Empty map should have movable hexes
        if (!movable) return -3;

        // Check multiple hexes
        bool hexesMovable = map.IsHexesMovable(hex, 1);
        if (!hexesMovable) return -4;

        // Shootable check
        bool shootable = map.IsHexShootable(hex);
        if (!shootable) return -5;

        // Outside area check
        bool outside = map.IsOutsideArea(hex);
        if (outside) return -6;

        if (!map.IsHexValid(hex)) return -7;

        mpos invalidNegative(-1, -1);
        if (map.IsHexValid(invalidNegative)) return -8;

        mpos invalidOverflow(1000, 1000);
        if (map.IsHexValid(invalidOverflow)) return -9;

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -10;

        if (!map.CheckPlaceForItem(hex, proto)) return -11;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapIsHexMovableInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.IsHexMovable(mpos(-1, -1));
    }

    void TestMapIsHexesMovableNegativeRadiusThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.IsHexesMovable(mpos(10, 10), -1);
    }

    void TestMapIsHexShootableInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.IsHexShootable(mpos(-1, -1));
    }

    void TestMapIsOutsideAreaInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.IsOutsideArea(mpos(-1, -1));
    }

    int TestMapBlockUnblock()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
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
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(50, 50);
        mpos original = hex;

        // Move hex by direction (dir 0 = north-ish)
        bool moved = map.MoveHexByDir(hex, mdir(0));
        if (!moved) return -3;
        if (hex.x == original.x && hex.y == original.y) return -4;

        // Move multiple steps
        mpos hex2(50, 50);
        int steps = map.MoveHexByDir(hex2, mdir(1), 3);
        if (steps != 3) return -5;

        mpos invalid(-1, -1);
        if (map.MoveHexByDir(invalid, mdir(0))) return -6;

        mpos edge(0, 0);
        int edgeSteps = map.MoveHexByDir(edge, mdir(1), 10000);
        if (edgeSteps >= 10000) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapCheckPlaceForItem()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(25, 25);
        bool canPlace = map.CheckPlaceForItem(hex, "TestItem".hstr());

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapCheckPlaceForItemInvalidPidThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.CheckPlaceForItem(mpos(25, 25), "MissingItem".hstr());
    }

    // ========== Map Path Operations ==========

    int TestMapPathLength()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos from(10, 10);
        mpos to(15, 15);
        int len = map.GetPathLength(from, to, 0, null);
        // Path should have positive length between two valid hexes
        if (len <= 0) return -3;

        int lenWithCallback = map.GetPathLength(from, to, 0, AllowItemPathGag);
        if (lenWithCallback <= 0) return -4;

        // Path with critter
        mpos crHex(20, 20);
        Critter cr = map.AddCritter("TestCritter".hstr(), crHex, mdir(0));
        if (cr is null) return -5;

        mpos dest(25, 25);
        int crLen = map.GetPathLength(cr, dest, 0, null);
        if (crLen <= 0) return -6;

        int crLenWithCallback = map.GetPathLength(cr, dest, 0, AllowCritterPathGag);
        if (crLenWithCallback <= 0) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapPathLengthInvalidFromHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetPathLength(mpos(-1, -1), mpos(15, 15), 0, null);
    }

    void TestMapPathLengthInvalidToHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetPathLength(mpos(10, 10), mpos(-1, -1), 0, null);
    }

    void TestMapCritterPathLengthInvalidToHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(20, 20), mdir(0));
        if (cr is null) {
            return;
        }

        map.GetPathLength(cr, mpos(-1, -1), 0, null);
    }

    int TestMapGetHexInPath()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
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
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos crHex(50, 50);
        Critter cr = map.AddCritter("TestCritter".hstr(), crHex, mdir(0));
        if (cr is null) return -3;

        mpos from(48, 50);
        mpos to(52, 50);

        // Get critters in path (simple)
        array<Critter> inPath = map.GetCrittersInPath(from, to, 0.0f, 10, CritterFindType::Any);

        // Get critters in path (with block hexes)
        mpos preBlock(0, 0);
        mpos block(0, 0);
        array<Critter> inPath2 = map.GetCrittersInPath(from, to, 0.0f, 10, CritterFindType::Any, preBlock, block);

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Map Static Items ==========

    int TestMapGetStaticItems()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        // Empty map has no static items, but we can still call the functions
        array<StaticItem> allStatic = map.GetStaticItems();

        ident zeroId;
        StaticItem? missingById = map.GetStaticItem(zeroId);
        if (missingById !is null) return -3;

        mpos hex(10, 10);
        array<StaticItem> onHex = map.GetStaticItemsOnHex(hex);

        array<StaticItem> byPid = map.GetStaticItems("TestItem".hstr());

        array<StaticItem> inRadius = map.GetStaticItemsInRadius(hex, 5, "TestItem".hstr());

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -4;

        array<StaticItem> byProto = map.GetStaticItems(proto);

        array<StaticItem> inRadiusByProto = map.GetStaticItemsInRadius(hex, 5, proto);

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestGameStaticMapQueries()
    {
        ProtoMap? mapProto = Game.GetProtoMap("TestMap".hstr());
        if (mapProto is null) return -1;

        array<StaticItem> staticItems = Game.GetStaticItemsForProtoMap(mapProto);
        if (!staticItems.isEmpty()) return -2;

        array<ProtoCritter> protoCritters = Game.GetProtoCrittersForProtoMap(mapProto);
        if (!protoCritters.isEmpty()) return -3;

        return 0;
    }

    void TestMapGetStaticItemsOnHexInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetStaticItemsOnHex(mpos(-1, -1));
    }

    void TestMapGetStaticItemsInRadiusNegativeRadiusThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetStaticItemsInRadius(mpos(10, 10), -1, "TestItem".hstr());
    }

    void TestMapGetStaticItemsInRadiusInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetStaticItemsInRadius(mpos(-1, -1), 1, "TestItem".hstr());
    }

    void TestMapGetStaticItemsInRadiusByProtoInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) {
            return;
        }

        map.GetStaticItemsInRadius(mpos(-1, -1), 1, proto);
    }

    void TestMapGetStaticItemsOnHexByPropertyInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetStaticItemsOnHex(mpos(-1, -1), ItemProperty::Count, 0);
    }

    void TestMapGetStaticItemsInRadiusByPropertyInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        map.GetStaticItemsInRadius(mpos(-1, -1), 1, ItemProperty::Count, 0);
    }

 )" + R"(
    // ========== Map Location Relationship ==========

    int TestMapLocationRelationship()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        // Map should know its location
        Location? mapLoc = map.GetLocation();
        if (mapLoc is null) return -3;
        if (mapLoc.Id != loc.Id) return -4;

        // Location map queries
        int mapCount = loc.GetMapCount();
        if (mapCount != 1) return -5;

        Map? byPid = loc.GetMap("TestMap".hstr());
        if (byPid is null) return -6;

        array<Map> allMaps = loc.GetMaps();
        if (allMaps.length() != 1) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Map Regeneration ==========

    int TestMapRegenerate()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
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
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos crHex(50, 50);
        Critter cr = map.AddCritter("TestCritter".hstr(), crHex, mdir(0));
        if (cr is null) return -3;

        mpos triggerHex(51, 50);
        map.VerifyTrigger(cr, triggerHex, 0);

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Location Creation with Maps ==========

    int LocationInitCalls = 0;
    ident LastLocationInitId = ZERO_IDENT;
    bool LastLocationInitFirstTime = false;

    void OnLocationInit(Location loc, bool firstTime)
    {
        LocationInitCalls++;
        LastLocationInitId = loc.Id;
        LastLocationInitFirstTime = firstTime;
    }

    class LocationInitDelegateOwner
    {
        void OnLocationInit(Location loc, bool firstTime)
        {
            LocationInitCalls++;
        }
    }

    int TestLocationCreateWithMaps()
    {
        // Create location with map list
        array<hstring> mapPids = {"TestMap".hstr()};
        Location loc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (loc is null) return -1;

        if (loc.GetMapCount() != 1) return -2;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -3;

        // Verify map size is as configured
        msize sz = map.Size;
        if (sz.width == 0 || sz.height == 0) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestLocationMethodBindings()
    {
        LocationInitCalls = 0;
        LastLocationInitId = ZERO_IDENT;
        LastLocationInitFirstTime = false;

        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;
        if (loc.GetMapCount() != 0) return -2;

        loc.SetupScript(OnLocationInit);
        if (LocationInitCalls != 1) return -3;
        if (LastLocationInitId != loc.Id) return -4;
        if (!LastLocationInitFirstTime) return -5;
        if (loc.InitScript != "MapOpsTest::OnLocationInit".hstr()) return -6;

        loc.SetupScriptEx("MapOpsTest::OnLocationInit".hstr());
        if (LocationInitCalls != 2) return -7;
        if (loc.InitScript != "MapOpsTest::OnLocationInit".hstr()) return -8;

        try {
            loc.SetupScriptEx("MapOpsTest::MissingLocationInit".hstr());
            Game.DestroyLocation(loc);
            return -9;
        }
        catch {
        }

        LocationInitDelegateOwner delegateOwner;
        try {
            loc.SetupScript(delegateOwner.OnLocationInit);
            Game.DestroyLocation(loc);
            return -10;
        }
        catch {
        }

        ProtoMap? mapProto = Game.GetProtoMap("TestMap".hstr());
        if (mapProto is null) return -11;
        if (loc.GetMap(mapProto) !is null) return -12;

        Map addedByPid = loc.AddMap("TestMap".hstr());
        if (addedByPid is null) return -13;
        if (loc.GetMapCount() != 1) return -14;
        if (loc.GetMap("MissingMap".hstr()) !is null) return -15;
        if (loc.GetMap("TestMap".hstr()) is null) return -16;
        if (loc.GetMap(mapProto) is null) return -17;

        Map addedByProto = loc.AddMap(mapProto);
        if (addedByProto is null) return -18;
        if (loc.GetMapCount() != 2) return -19;

        array<Map> maps = loc.GetMaps();
        if (maps.length() != 2) return -20;

        Map secondMap = loc.GetMapByIndex(1);
        if (secondMap is null) return -21;
        if (secondMap.Id != addedByProto.Id) return -22;

        loc.Regenerate();

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestGameLocationAndMapOverloads()
    {
        ProtoLocation? locProto = Game.GetProtoLocation("TestLocation".hstr());
        if (locProto is null) return -1;

        ProtoMap? mapProto = Game.GetProtoMap("TestMap".hstr());
        if (mapProto is null) return -2;

        array<hstring> mapPids = {"TestMap".hstr()};
        dict<LocationProperty, any> initProps = {{LocationProperty::InitScript, "MapOpsTest::OnLocationInit".hstr()}};

        int initBefore = LocationInitCalls;
        Location byPidProps = Game.CreateLocation("TestLocation".hstr(), initProps);
        if (byPidProps is null) return -3;
        if (LocationInitCalls != initBefore + 1) return -4;
        if (LastLocationInitId != byPidProps.Id) return -5;
        if (!LastLocationInitFirstTime) return -6;
        if (byPidProps.InitScript != "MapOpsTest::OnLocationInit".hstr()) return -7;

        Location byProto = Game.CreateLocation(locProto);
        if (byProto is null) return -8;

        initBefore = LocationInitCalls;
        Location byProtoProps = Game.CreateLocation(locProto, initProps);
        if (byProtoProps is null) return -9;
        if (LocationInitCalls != initBefore + 1) return -10;
        if (LastLocationInitId != byProtoProps.Id) return -11;

        initBefore = LocationInitCalls;
        Location withMapProps1 = Game.CreateLocation("TestLocation".hstr(), mapPids, initProps);
        if (withMapProps1 is null) return -12;
        if (LocationInitCalls != initBefore + 1) return -13;
        if (withMapProps1.GetMapCount() != 1) return -14;

        Location withMapProps2 = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (withMapProps2 is null) return -15;
        if (withMapProps2.GetMapCount() != 1) return -16;

        Location? firstByPid = Game.GetLocation("TestLocation".hstr());
        if (firstByPid is null) return -17;

        Location? secondByPid = Game.GetLocation("TestLocation".hstr(), 1);
        if (secondByPid is null) return -18;
        if (secondByPid.Id == firstByPid.Id) return -19;

        Location? firstByProto = Game.GetLocation(locProto);
        if (firstByProto is null) return -20;

        Location? secondByProto = Game.GetLocation(locProto, 1);
        if (secondByProto is null) return -21;
        if (secondByProto.Id == firstByProto.Id) return -22;

        array<Location> byProtoList = Game.GetLocations(locProto);
        if (byProtoList.length() < 4) return -23;

        array<Location> byEmptyPid = Game.GetLocations("".hstr());
        if (byEmptyPid.length() < byProtoList.length()) return -24;

        Map? firstMapByPid = Game.GetMap("TestMap".hstr());
        if (firstMapByPid is null) return -25;

        Map? secondMapByPid = Game.GetMap("TestMap".hstr(), 1);
        if (secondMapByPid is null) return -26;
        if (secondMapByPid.Id == firstMapByPid.Id) return -27;

        Map? firstMapByProto = Game.GetMap(mapProto);
        if (firstMapByProto is null) return -28;

        Map? secondMapByProto = Game.GetMap(mapProto, 1);
        if (secondMapByProto is null) return -29;
        if (secondMapByProto.Id == firstMapByProto.Id) return -30;

        array<Map> allMaps = Game.GetMaps();
        if (allMaps.length() < 2) return -31;

        array<Map> mapsByPid = Game.GetMaps("TestMap".hstr());
        if (mapsByPid.length() < 2) return -32;

        array<Map> mapsByEmptyPid = Game.GetMaps("".hstr());
        if (mapsByEmptyPid.length() < mapsByPid.length()) return -33;

        array<Map> mapsByProto = Game.GetMaps(mapProto);
        if (mapsByProto.length() < 2) return -34;

        Location destroyByHandleLoc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (destroyByHandleLoc is null) return -35;

        Map destroyByHandleMap = destroyByHandleLoc.GetMapByIndex(0);
        if (destroyByHandleMap is null) return -36;

        ident destroyByHandleMapId = destroyByHandleMap.Id;
        Game.DestroyMap(destroyByHandleMap);
        if (Game.GetMap(destroyByHandleMapId) !is null) return -37;
        Game.DestroyLocation(destroyByHandleLoc);

        Location destroyByIdLoc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (destroyByIdLoc is null) return -38;

        Map destroyByIdMap = destroyByIdLoc.GetMapByIndex(0);
        if (destroyByIdMap is null) return -39;

        ident destroyByIdMapId = destroyByIdMap.Id;
        Game.DestroyMap(destroyByIdMapId);
        if (Game.GetMap(destroyByIdMapId) !is null) return -40;
        Game.DestroyLocation(destroyByIdLoc);

        ident byProtoId = byProto.Id;
        Game.DestroyLocation(byProtoId);
        if (Game.GetLocation(byProtoId) !is null) return -41;

        Game.DestroyMap(ZERO_IDENT);
        Game.DestroyLocation(ZERO_IDENT);
        if (Game.GetMap(ZERO_IDENT) !is null) return -42;
        if (Game.GetLocation(ZERO_IDENT) !is null) return -43;

        Game.DestroyLocation(withMapProps2);
        Game.DestroyLocation(withMapProps1);
        Game.DestroyLocation(byProtoProps);
        Game.DestroyLocation(byPidProps);

        return 0;
    }

    // ========== Location Queries ==========

    int TestGameGetLocations()
    {
        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        // Get all locations
        array<Location> allLocs = Game.GetLocations();
        if (allLocs.length() < 2) return -2;

        // Get locations by proto
        array<Location> byPid = Game.GetLocations("TestLocation".hstr());
        if (byPid.length() < 2) return -3;

        // Get location by id
        Location? found = Game.GetLocation(loc1.Id);
        if (found is null) return -4;
        if (found.Id != loc1.Id) return -5;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

 )" + R"(
    // ========== Proto-based overloads ==========

    int TestMapAddItemByProto()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -3;

        mpos hex(12, 12);
        Item item = map.AddItem(hex, proto, 2);
        if (item is null) return -4;

        // Get by proto
        Item? onHex = map.GetItemOnHex(hex, proto);
        if (onHex is null) return -5;

        // Get in radius by proto
        Item? inRadius = map.GetItemInRadius(hex, 3, proto);
        if (inRadius is null) return -6;

        // Get items list by proto
        array<Item> byProto = map.GetItems(proto);
        if (byProto.isEmpty()) return -7;

        // GetItemsInRadius by proto
        array<Item> inRadiusList = map.GetItemsInRadius(hex, 3, proto);
        if (inRadiusList.isEmpty()) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapAddItemByProtoInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) {
            return;
        }

        map.AddItem(mpos(-1, -1), proto, 1);
    }

    void TestMapAddItemByProtoInvalidCountThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) {
            return;
        }

        map.AddItem(mpos(12, 12), proto, 0);
    }

    int TestMapAddCritterByProto()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ProtoCritter? proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -3;

        mpos hex(33, 33);
        Critter cr = map.AddCritter(proto, hex, mdir(0));
        if (cr is null) return -4;

        Critter? found = map.GetCritter(cr.Id);
        if (found is null) return -5;

        // Get critters by proto + find type
        array<Critter> byProto = map.GetCritters(proto, CritterFindType::Any);
        if (byProto.isEmpty()) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Property-filter overloads ==========

    int TestMapGetItemByProperty()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(14, 14);
        Item item = map.AddItem(hex, "TestItem".hstr(), 1);
        if (item is null) return -3;

        // Get item on hex by property (Count is a common int32 property)
        Item? byProp = map.GetItemOnHex(hex, ItemProperty::Count, item.Count);

        // Get item in radius by property
        Item? inRadiusProp = map.GetItemInRadius(hex, 3, ItemProperty::Count, item.Count);

        // Get items list by property
        array<Item> listByProp = map.GetItems(ItemProperty::Count, item.Count);

        // GetItemsOnHex by property
        array<Item> onHexProp = map.GetItemsOnHex(hex, ItemProperty::Count, item.Count);

        // GetItemsInRadius by property
        array<Item> inRadiusListProp = map.GetItemsInRadius(hex, 3, ItemProperty::Count, item.Count);

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapGetCritterByProperty()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(36, 36);
        Critter cr = map.AddCritter("TestCritter".hstr(), hex, mdir(0));
        if (cr is null) return -3;

        // Get critter by property
        Critter? byProp = map.GetCritter(CritterProperty::Dir, cr.Dir.angle, CritterFindType::Any);

        // Get critters list by property
        array<Critter> listByProp = map.GetCritters(CritterProperty::Dir, cr.Dir.angle, CritterFindType::Any);

        if (map.GetCritter(CritterProperty::Dir, cr.Dir.angle + 1, CritterFindType::Any) !is null) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    // ========== Static item overloads ==========

    int TestMapGetStaticItemOverloads()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(10, 10);

        // GetStaticItem by hex + pid
        StaticItem? byHexPid = map.GetStaticItemOnHex(hex, "TestItem".hstr());

        // GetStaticItem by hex + proto
        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        StaticItem? byHexProto = map.GetStaticItemOnHex(hex, proto);

        // GetStaticItems by hex + proto + radius
        array<StaticItem> byRadiusProto = map.GetStaticItemsInRadius(hex, 5, proto);

        // GetStaticItems by hex + property
        array<StaticItem> byHexProp = map.GetStaticItemsOnHex(hex, ItemProperty::Count, 0);

        // GetStaticItems by hex + radius + property
        array<StaticItem> byRadiusProp = map.GetStaticItemsInRadius(hex, 5, ItemProperty::Count, 0);

        // GetStaticItems by proto
        array<StaticItem> byProto = map.GetStaticItems(proto);

        // GetStaticItems by property
        array<StaticItem> byProp = map.GetStaticItems(ItemProperty::Count, 0);

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== SetupScript ==========

    void OnMapInit(Map map, bool firstTime)
    {
        // Just a stub init function
    }

    class MapInitDelegateOwner
    {
        void OnMapInit(Map map, bool firstTime)
        {
        }
    }

    int TestMapSetupScriptEx()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        map.SetupScriptEx("MapOpsTest::OnMapInit".hstr());

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapSetupScript()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        map.SetupScript(OnMapInit);

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapSetupScriptExMissingThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return -1;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return -2;
        }

        bool caught = false;
        try {
            map.SetupScriptEx("MapOpsTest::MissingMapInit".hstr());
        }
        catch {
            caught = true;
        }

        Game.DestroyLocation(loc);
        return caught ? 0 : -3;
    }

    int TestMapSetupScriptDelegateThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return -1;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return -2;
        }

        MapInitDelegateOwner delegateOwner;
        bool caught = false;
        try {
            map.SetupScript(delegateOwner.OnMapInit);
        }
        catch {
            caught = true;
        }

        Game.DestroyLocation(loc);
        return caught ? 0 : -3;
    }

 )" + R"(
    // ========== AddItem with properties ==========

    int TestMapAddItemWithProperties()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        mpos hex(16, 16);

        // AddItem with hstring + properties map
        dict<ItemProperty, int> props = {{ItemProperty::Count, 5}};
        Item item1 = map.AddItem(hex, "TestItem".hstr(), 1, props);
        if (item1 is null) return -3;

        // AddItem with proto + properties map
        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) return -31;
        mpos hex2(17, 17);
        Item item2 = map.AddItem(hex2, proto, 1, props);
        if (item2 is null) return -4;

        dict<ItemProperty, int> emptyProps;
        Item item3 = map.AddItem(mpos(18, 17), "TestItem".hstr(), 1, emptyProps);
        if (item3 is null) return -5;

        Item item4 = map.AddItem(mpos(19, 17), proto, 1, emptyProps);
        if (item4 is null) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapAddItemWithPropertiesInvalidProtoThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        dict<ItemProperty, int> props = {{ItemProperty::Count, 5}};
        map.AddItem(mpos(16, 16), "MissingItem".hstr(), 1, props);
    }

    void TestMapAddItemWithPropertiesInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        dict<ItemProperty, int> props = {{ItemProperty::Count, 5}};
        map.AddItem(mpos(-1, -1), "TestItem".hstr(), 1, props);
    }

    void TestMapAddItemWithPropertiesInvalidCountThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        dict<ItemProperty, int> props = {{ItemProperty::Count, 5}};
        map.AddItem(mpos(16, 16), "TestItem".hstr(), 0, props);
    }

    void TestMapAddItemWithProtoPropertiesInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) {
            return;
        }

        dict<ItemProperty, int> props = {{ItemProperty::Count, 5}};
        map.AddItem(mpos(-1, -1), proto, 1, props);
    }

    void TestMapAddItemWithProtoPropertiesInvalidCountThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        ProtoItem? proto = Game.GetProtoItem("TestItem".hstr());
        if (proto is null) {
            return;
        }

        dict<ItemProperty, int> props = {{ItemProperty::Count, 5}};
        map.AddItem(mpos(16, 16), proto, 0, props);
    }

    // ========== AddCritter with properties ==========

    int TestMapAddCritterWithProperties()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ProtoCritter? proto = Game.GetProtoCritter("TestCritter".hstr());
        if (proto is null) return -3;

        // AddCritter with hstring + int properties
        dict<CritterProperty, int> intProps = {{CritterProperty::LookDistance, 10}};
        mpos hex1(38, 38);
        Critter cr1 = map.AddCritter("TestCritter".hstr(), hex1, mdir(0), intProps);
        if (cr1 is null) return -4;
        if (cr1.LookDistance != 10) return -41;

        // AddCritter with proto + int properties
        mpos hex2(39, 39);
        Critter cr2 = map.AddCritter(proto, hex2, mdir(0), intProps);
        if (cr2 is null) return -5;
        if (cr2.LookDistance != 10) return -51;

        // AddCritter with hstring + any properties
        dict<CritterProperty, any> anyProps = {{CritterProperty::LookDistance, 11}};
        mpos hex3(40, 40);
        Critter cr3 = map.AddCritter("TestCritter".hstr(), hex3, mdir(0), anyProps);
        if (cr3 is null) return -6;
        if (cr3.LookDistance != 11) return -61;

        // AddCritter with proto + any properties
        mpos hex4(41, 41);
        Critter cr4 = map.AddCritter(proto, hex4, mdir(0), anyProps);
        if (cr4 is null) return -7;
        if (cr4.LookDistance != 11) return -71;

        Game.DestroyLocation(loc);
        return 0;
    }

    void TestMapAddCritterWithIntPropertiesInvalidProtoThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        map.AddCritter("MissingCritter".hstr(), mpos(38, 38), mdir(0), props);
    }

    void TestMapAddCritterWithAnyPropertiesInvalidProtoThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        dict<CritterProperty, any> props = {{CritterProperty::LookDistance, 10}};
        map.AddCritter("MissingCritter".hstr(), mpos(38, 38), mdir(0), props);
    }

    void TestMapAddCritterWithIntPropertiesInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        map.AddCritter("TestCritter".hstr(), mpos(-1, -1), mdir(0), props);
    }

    void TestMapAddCritterWithAnyPropertiesInvalidHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) {
            return;
        }

        Map map = loc.GetMapByIndex(0);
        if (map is null) {
            return;
        }

        dict<CritterProperty, any> props = {{CritterProperty::LookDistance, 10}};
        map.AddCritter("TestCritter".hstr(), mpos(-1, -1), mdir(0), props);
    }

    // ========== Reentrant map events ==========

    int ReentrantEventMode = 0;
    ident ReentrantTargetCrId = ZERO_IDENT;
    ident ReentrantOtherCrId = ZERO_IDENT;
    ident ReentrantTargetItemId = ZERO_IDENT;
    ident ReentrantOtherItemId = ZERO_IDENT;
    ident ReentrantTargetMapId = ZERO_IDENT;
    ident ReentrantTargetLocId = ZERO_IDENT;
    int ReentrantMapOutCalls = 0;
    int ReentrantMapInCalls = 0;
    int ReentrantGlobalOutCalls = 0;
    int ReentrantGlobalInCalls = 0;
    int ReentrantCritterTransferCalls = 0;
    int ReentrantOtherCritterTransferCalls = 0;
    int ReentrantInitialInfoCalls = 0;
    int ReentrantCritterLoadCalls = 0;
    int ReentrantCritterInitCalls = 0;
    int ReentrantMapInitCalls = 0;
    int ReentrantMapFinishCalls = 0;
    int ReentrantMapAddedCalls = 0;
    int ReentrantMapRemovedCalls = 0;
    int ReentrantNestedDestroyCaught = 0;
    int ReentrantCritterAppearedCalls = 0;
    int ReentrantCritterDisappearedCalls = 0;
    int ReentrantCritterAppearedDist1Calls = 0;
    int ReentrantCritterAppearedDist2Calls = 0;
    int ReentrantCritterDisappearedDist1Calls = 0;
    int ReentrantItemAppearedCalls = 0;
    int ReentrantItemDisappearedCalls = 0;
    int ReentrantItemChangedCalls = 0;
    int ReentrantCritterItemMovedCalls = 0;
    int ReentrantOtherCritterItemMovedFromInventoryCalls = 0;
    int ReentrantOtherCritterItemMovedFromMainCalls = 0;
    int ReentrantCritterWalkCalls = 0;
    int ReentrantOtherCritterWalkCalls = 0;
    int ReentrantNestedTransferCaught = 0;
    bool ReentrantMapOutSawOnMap = false;
    bool ReentrantMapInSawAttached = false;
    bool ReentrantGlobalOutSawTrip = false;
    bool ReentrantCritterAppearedSawOnMap = false;
    bool ReentrantCritterDisappearedSawOnMap = false;
    bool ReentrantItemAppearedSawOnMap = false;
    bool ReentrantItemDisappearedSawDetached = false;
    bool ReentrantItemChangedSawOnMap = false;

    void ResetReentrantEventState()
    {
        ReentrantEventMode = 0;
        ReentrantTargetCrId = ZERO_IDENT;
        ReentrantOtherCrId = ZERO_IDENT;
        ReentrantTargetItemId = ZERO_IDENT;
        ReentrantOtherItemId = ZERO_IDENT;
        ReentrantTargetMapId = ZERO_IDENT;
        ReentrantTargetLocId = ZERO_IDENT;
        ReentrantMapOutCalls = 0;
        ReentrantMapInCalls = 0;
        ReentrantGlobalOutCalls = 0;
        ReentrantGlobalInCalls = 0;
        ReentrantCritterTransferCalls = 0;
        ReentrantOtherCritterTransferCalls = 0;
        ReentrantInitialInfoCalls = 0;
        ReentrantCritterLoadCalls = 0;
        ReentrantCritterInitCalls = 0;
        ReentrantMapInitCalls = 0;
        ReentrantMapFinishCalls = 0;
        ReentrantMapAddedCalls = 0;
        ReentrantMapRemovedCalls = 0;
        ReentrantNestedDestroyCaught = 0;
        ReentrantCritterAppearedCalls = 0;
        ReentrantCritterDisappearedCalls = 0;
        ReentrantCritterAppearedDist1Calls = 0;
        ReentrantCritterAppearedDist2Calls = 0;
        ReentrantCritterDisappearedDist1Calls = 0;
        ReentrantItemAppearedCalls = 0;
        ReentrantItemDisappearedCalls = 0;
        ReentrantItemChangedCalls = 0;
        ReentrantCritterItemMovedCalls = 0;
        ReentrantOtherCritterItemMovedFromInventoryCalls = 0;
        ReentrantOtherCritterItemMovedFromMainCalls = 0;
        ReentrantCritterWalkCalls = 0;
        ReentrantOtherCritterWalkCalls = 0;
        ReentrantNestedTransferCaught = 0;
        ReentrantMapOutSawOnMap = false;
        ReentrantMapInSawAttached = false;
        ReentrantGlobalOutSawTrip = false;
        ReentrantCritterAppearedSawOnMap = false;
        ReentrantCritterDisappearedSawOnMap = false;
        ReentrantItemAppearedSawOnMap = false;
        ReentrantItemDisappearedSawDetached = false;
        ReentrantItemChangedSawOnMap = false;
    }

 )" + R"(
    [[Event]]
    void OnReentrantMapCritterOut(Map map, Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantMapOutCalls++;

        if (ReentrantEventMode == 1) {
            ReentrantMapOutSawOnMap = cr.MapId == map.Id && map.GetCritter(cr.Id) !is null;
        }
        else if (ReentrantEventMode == 6) {
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 2) {
            Map? targetMap = Game.GetMap(ReentrantTargetMapId);

            if (targetMap !is null) {
                try {
                    cr.TransferToMap(targetMap, mpos(60, 60), mdir(0));
                }
                catch {
                    ReentrantNestedTransferCaught++;
                }
            }
        }
    }

    [[Event]]
    void OnReentrantMapCritterIn(Map map, Critter cr)
    {
        if (ReentrantTargetCrId == ZERO_IDENT) {
            ReentrantTargetCrId = cr.Id;
        }
        else if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantMapInCalls++;

        if (ReentrantEventMode == 3) {
            ReentrantMapInSawAttached = cr.MapId == map.Id && map.GetCritter(cr.Id) !is null;
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 18) {
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 35) {
            if (map.Id == ReentrantTargetMapId) {
                return;
            }

            ReentrantMapInSawAttached = cr.MapId == map.Id && map.GetCritter(cr.Id) !is null;

            Map? targetMap = Game.GetMap(ReentrantTargetMapId);

            if (targetMap !is null) {
                cr.TransferToMap(targetMap, mpos(60, 60), mdir(0));
            }
        }
        else if (ReentrantEventMode == 37) {
            ReentrantMapInSawAttached = cr.MapId == map.Id && map.GetCritter(cr.Id) !is null;
            Game.DestroyMap(map);
        }
    }

    [[Event]]
    void OnReentrantGlobalMapCritterIn(Critter cr)
    {
        if (ReentrantTargetCrId == ZERO_IDENT) {
            ReentrantTargetCrId = cr.Id;
        }
        else if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantGlobalInCalls++;

        if (ReentrantEventMode == 19) {
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 38) {
            cr.MakeControllable(false);
            Game.DestroyCritter(cr);
        }
    }

    [[Event]]
    void OnReentrantCritterTransfer(Critter cr, Map? prevMap)
    {
        if (cr.Id == ReentrantTargetCrId) {
            ReentrantCritterTransferCalls++;
        }
        else if (cr.Id == ReentrantOtherCrId) {
            ReentrantOtherCritterTransferCalls++;

            if (ReentrantEventMode == 34) {
                Critter? target = Game.GetCritter(ReentrantTargetCrId);

                if (target !is null) {
                    Game.DestroyCritter(target);
                }
            }
        }
    }

    [[Event]]
    void OnReentrantGlobalMapCritterOut(Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantGlobalOutCalls++;

        if (ReentrantEventMode == 4) {
            ReentrantGlobalOutSawTrip = cr.GlobalMapTripId != 0;
        }
        else if (ReentrantEventMode == 8) {
            Game.DestroyCritter(cr);
        }
    }

    [[Event]]
    void OnReentrantCritterSendInitialInfo(Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantInitialInfoCalls++;
    }

    [[Event]]
    void OnReentrantCritterLoad(Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantCritterLoadCalls++;
    }

 )" + R"(
    [[Event]]
    void OnReentrantCritterInit(Critter cr, bool firstTime)
    {
        if (ReentrantTargetCrId == ZERO_IDENT) {
            ReentrantTargetCrId = cr.Id;
        }
        else if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantCritterInitCalls++;

        if (ReentrantEventMode == 40) {
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 41) {
            Map? map = Game.GetMap(cr.MapId);

            if (map !is null) {
                Game.DestroyMap(map);
            }
        }
        else if (ReentrantEventMode == 42) {
            Map? targetMap = Game.GetMap(ReentrantTargetMapId);

            if (targetMap !is null && cr.MapId != targetMap.Id) {
                cr.TransferToMap(targetMap, mpos(60, 60), mdir(0));
            }
        }
    }

    [[Event]]
    void OnReentrantMapInit(Map map, bool firstTime)
    {
        if (ReentrantTargetMapId != ZERO_IDENT && map.Id != ReentrantTargetMapId) {
            return;
        }

        ReentrantTargetMapId = map.Id;
        ReentrantMapInitCalls++;

        if (ReentrantEventMode == 22) {
            Game.DestroyMap(map);
        }
    }

    [[Event]]
    void OnReentrantMapFinish(Map map)
    {
        if (map.Id != ReentrantTargetMapId) {
            return;
        }

        ReentrantMapFinishCalls++;

        if (ReentrantEventMode == 20 && ReentrantMapFinishCalls == 1) {
            Location? loc = map.GetLocation();

            if (loc !is null && loc.Id == ReentrantTargetLocId) {
                try {
                    Game.DestroyLocation(loc);
                }
                catch {
                    ReentrantNestedDestroyCaught++;
                }
            }
        }
    }

    [[Event]]
    void OnReentrantMapAdded(Location loc, Map map)
    {
        if (ReentrantTargetLocId != ZERO_IDENT && loc.Id != ReentrantTargetLocId) {
            return;
        }

        ReentrantTargetLocId = loc.Id;
        ReentrantTargetMapId = map.Id;
        ReentrantMapAddedCalls++;

        if (ReentrantEventMode == 23) {
            Game.DestroyLocation(loc);
        }
        else if (ReentrantEventMode == 39) {
            Game.DestroyMap(map);
        }
    }

    [[Event]]
    void OnReentrantMapRemoved(Location loc, Map map)
    {
        if (map.Id != ReentrantTargetMapId || loc.Id != ReentrantTargetLocId) {
            return;
        }

        ReentrantMapRemovedCalls++;

        if (ReentrantEventMode == 21) {
            try {
                Game.DestroyLocation(loc);
            }
            catch {
                ReentrantNestedDestroyCaught++;
            }
        }
    }

    [[Event]]
    void OnReentrantCritterAppeared(Critter observer, Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantCritterAppearedCalls++;

        if (ReentrantEventMode == 14) {
            Map? map = Game.GetMap(observer.MapId);
            ReentrantCritterAppearedSawOnMap = cr.MapId == observer.MapId && map !is null && map.GetCritter(cr.Id) !is null;
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 15) {
            Map? targetMap = Game.GetMap(ReentrantTargetMapId);

            if (targetMap !is null) {
                cr.TransferToMap(targetMap, mpos(60, 60), mdir(0));
            }
        }
    }

    [[Event]]
    void OnReentrantCritterDisappeared(Critter observer, Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantCritterDisappearedCalls++;

        if (ReentrantEventMode == 5) {
            Map? map = Game.GetMap(ReentrantTargetMapId);
            ReentrantCritterDisappearedSawOnMap = cr.MapId == ReentrantTargetMapId && map !is null && map.GetCritter(cr.Id) !is null;
        }
        else if (ReentrantEventMode == 7) {
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 17) {
            Map? targetMap = Game.GetMap(ReentrantTargetMapId);

            if (targetMap !is null) {
                cr.TransferToMap(targetMap, mpos(60, 60), mdir(0));
            }
        }
    }

    [[Event]]
    void OnReentrantCritterAppearedDist1(Critter observer, Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantCritterAppearedDist1Calls++;

        if (ReentrantEventMode == 16) {
            Map? targetMap = Game.GetMap(ReentrantTargetMapId);

            if (targetMap !is null) {
                cr.TransferToMap(targetMap, mpos(60, 60), mdir(0));
            }
        }
    }

    [[Event]]
    void OnReentrantCritterAppearedDist2(Critter observer, Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantCritterAppearedDist2Calls++;
    }

 )" + R"(
    [[Event]]
    void OnReentrantCritterDisappearedDist1(Critter observer, Critter cr)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        ReentrantCritterDisappearedDist1Calls++;
    }

    [[Event]]
    void OnReentrantItemOnMapAppeared(Critter observer, Item item, Critter dropper)
    {
        if (ReentrantTargetItemId == ZERO_IDENT) {
            ReentrantTargetItemId = item.Id;
        }
        else if (item.Id != ReentrantTargetItemId) {
            return;
        }

        ReentrantItemAppearedCalls++;

        if (ReentrantEventMode == 9) {
            ReentrantItemAppearedSawOnMap = item.MapId == observer.MapId && observer.MapId != ZERO_IDENT;
            Game.DestroyItem(item);
        }
        else if (ReentrantEventMode == 30) {
            ReentrantItemAppearedSawOnMap = item.MapId == observer.MapId && observer.MapId != ZERO_IDENT;

            Critter? receiver = Game.GetCritter(ReentrantTargetCrId);

            if (receiver !is null) {
                Game.MoveItem(item, receiver);
            }
        }
        else if (ReentrantEventMode == 38) {
            ReentrantItemAppearedSawOnMap = item.MapId == observer.MapId && observer.MapId != ZERO_IDENT;

            Map? map = Game.GetMap(observer.MapId);

            if (map !is null) {
                Game.DestroyMap(map);
            }
        }
    }

    [[Event]]
    void OnReentrantItemOnMapDisappeared(Critter observer, Item item, Critter picker)
    {
        if (item.Id != ReentrantTargetItemId) {
            return;
        }

        ReentrantItemDisappearedCalls++;

        if (ReentrantEventMode == 10) {
            ReentrantItemDisappearedSawDetached = item.MapId == ZERO_IDENT;
            Game.DestroyItem(item);
        }
    }

    [[Event]]
    void OnReentrantItemOnMapChanged(Critter observer, Item item)
    {
        if (item.Id != ReentrantTargetItemId) {
            return;
        }

        ReentrantItemChangedCalls++;

        if (ReentrantEventMode == 11) {
            ReentrantItemChangedSawOnMap = item.MapId == observer.MapId && observer.MapId != ZERO_IDENT;
            Game.DestroyItem(item);
        }
        else if (ReentrantEventMode == 33) {
            ReentrantItemChangedSawOnMap = item.MapId == observer.MapId && observer.MapId != ZERO_IDENT;

            if (ReentrantItemChangedCalls == 1) {
                Critter? receiver = Game.GetCritter(ReentrantTargetCrId);

                if (receiver !is null) {
                    Game.MoveItem(item, receiver);
                }
            }
        }
    }

    [[Event]]
    void OnReentrantCritterItemMoved(Critter cr, Item item, CritterItemSlot fromSlot)
    {
        if (ReentrantEventMode == 27) {
            if (item.Id == ReentrantOtherItemId) {
                if (fromSlot == CritterItemSlot::Inventory) {
                    ReentrantOtherCritterItemMovedFromInventoryCalls++;
                }
                else if (fromSlot == CritterItemSlot::Main) {
                    ReentrantOtherCritterItemMovedFromMainCalls++;
                }
            }
            else if (item.Id == ReentrantTargetItemId) {
                ReentrantCritterItemMovedCalls++;

                Item? movedItem = Game.GetItem(ReentrantOtherItemId);
                Map? targetMap = Game.GetMap(ReentrantTargetMapId);

                if (movedItem !is null && targetMap !is null) {
                    Game.MoveItem(movedItem, targetMap, mpos(52, 50));
                }
            }

            return;
        }
        if (ReentrantEventMode == 28) {
            if (item.Id == ReentrantTargetItemId && fromSlot == CritterItemSlot::Outside) {
                ReentrantCritterItemMovedCalls++;

                Map? targetMap = Game.GetMap(ReentrantTargetMapId);

                if (targetMap !is null) {
                    Game.MoveItem(item, targetMap, mpos(52, 50));
                }
            }

            return;
        }
        if (ReentrantEventMode == 36) {
            if (item.Id == ReentrantTargetItemId) {
                ReentrantCritterItemMovedCalls++;

                Item? victim = Game.GetItem(ReentrantOtherItemId);

                if (victim !is null) {
                    Game.DestroyItem(victim);
                }
            }
            else if (item.Id == ReentrantOtherItemId) {
                ReentrantOtherCritterItemMovedFromInventoryCalls++;
            }

            return;
        }

        if (item.Id != ReentrantTargetItemId) {
            return;
        }

        ReentrantCritterItemMovedCalls++;

        if (ReentrantEventMode == 12) {
            Game.DestroyItem(item);
        }
        else if (ReentrantEventMode == 13) {
            Game.DestroyCritter(cr);
        }
    }

 )" + R"(
    [[Event]]
    void OnReentrantCritterWalk(Item item, Critter cr, bool isIn, mdir dir)
    {
        if (cr.Id != ReentrantTargetCrId) {
            return;
        }

        if (ReentrantEventMode == 31) {
            if (item.Id == ReentrantTargetItemId) {
                ReentrantCritterWalkCalls++;

                Map? targetMap = Game.GetMap(ReentrantTargetMapId);

                if (targetMap !is null) {
                    cr.TransferToMap(targetMap, mpos(60, 60), mdir(0));
                }
            }
            else if (item.Id == ReentrantOtherItemId) {
                ReentrantOtherCritterWalkCalls++;
            }

            return;
        }
        if (ReentrantEventMode == 32) {
            if (item.Id == ReentrantTargetItemId) {
                ReentrantCritterWalkCalls++;

                Location? loc = Game.GetLocation(ReentrantTargetLocId);

                if (loc !is null) {
                    Game.DestroyLocation(loc);
                }
            }
            else if (item.Id == ReentrantOtherItemId) {
                ReentrantOtherCritterWalkCalls++;
            }

            return;
        }

        ReentrantCritterWalkCalls++;

        if (ReentrantEventMode == 24) {
            Game.DestroyCritter(cr);
        }
        else if (ReentrantEventMode == 25 && item.Id == ReentrantTargetItemId) {
            Game.DestroyItem(item);
        }
    }

    int TestMapCritterOutEventRunsBeforeDetach()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 1;

        Game.OnMapCritterOut.Subscribe(OnReentrantMapCritterOut);

        cr.TransferToGlobal();

        if (ReentrantMapOutCalls != 1) return -4;
        if (!ReentrantMapOutSawOnMap) return -5;
        if (cr.MapId != ZERO_IDENT) return -6;
        if (cr.GlobalMapTripId == 0) return -7;

        Game.DestroyCritter(cr);
        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapCritterOutEventMayDestroyCritter()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 6;

        Game.OnMapCritterOut.Subscribe(OnReentrantMapCritterOut);

        cr.TransferToGlobal();

        if (Game.GetCritter(crId) !is null) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapCritterOutEventBlocksNestedTransfer()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        Critter cr = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        ReentrantTargetCrId = cr.Id;
        ReentrantTargetMapId = map2.Id;
        ReentrantEventMode = 2;

        Game.OnMapCritterOut.Subscribe(OnReentrantMapCritterOut);

        cr.TransferToGlobal();

        if (ReentrantMapOutCalls != 1) return -4;
        if (ReentrantNestedTransferCaught != 1) return -5;
        if (cr.MapId != ZERO_IDENT) return -6;
        if (cr.GlobalMapTripId == 0) return -7;
        if (map2.GetCritter(cr.Id) !is null) return -8;

        Game.DestroyCritter(cr);
        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestCritterDisappearedEventRunsBeforeDetach()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), props);
        if (observer is null || cr is null) return -3;

        array<Critter> seen = observer.GetCritters(CritterSeeType::WhoISee, CritterFindType::Any);
        bool observerSeesCr = false;
        for (uint i = 0; i < seen.length(); i++) {
            if (seen[i].Id == cr.Id) {
                observerSeesCr = true;
                break;
            }
        }
        if (!observerSeesCr) return -4;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantTargetMapId = map.Id;
        ReentrantEventMode = 5;

        observer.OnCritterDisappeared.Subscribe(OnReentrantCritterDisappeared);

        cr.TransferToGlobal();

        if (ReentrantCritterDisappearedCalls != 1) return -5;
        if (!ReentrantCritterDisappearedSawOnMap) return -6;
        if (cr.MapId != ZERO_IDENT) return -7;
        if (cr.GlobalMapTripId == 0) return -8;

        Game.DestroyCritter(cr);
        Game.DestroyLocation(loc);
        return 0;
    }

    int TestCritterDisappearedEventMayDestroyCritter()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), props);
        if (observer is null || cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 7;

        observer.OnCritterDisappeared.Subscribe(OnReentrantCritterDisappeared);

        cr.TransferToGlobal();

        if (Game.GetCritter(crId) !is null) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestMapCritterInEventMayDestroyEnteringCritter()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        Critter cr = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 3;

        Game.OnMapCritterIn.Subscribe(OnReentrantMapCritterIn);

        cr.TransferToMap(map2, mpos(60, 60), mdir(0));

        if (ReentrantMapInCalls != 1) return -4;
        if (!ReentrantMapInSawAttached) return -5;
        if (Game.GetCritter(crId) !is null) return -6;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestMapCritterInEventMayDestroyBeforeInitialInfo()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        Critter cr = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 18;

        Game.OnMapCritterIn.Subscribe(OnReentrantMapCritterIn);
        Game.OnCritterSendInitialInfo.Subscribe(OnReentrantCritterSendInitialInfo);

        cr.TransferToMap(map2, mpos(60, 60), mdir(0));

        if (ReentrantMapInCalls != 1) return -4;
        if (ReentrantInitialInfoCalls != 0) return -5;
        if (Game.GetCritter(crId) !is null) return -6;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestMapAddCritterEventMayDestroyCritterThrows()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ReentrantEventMode = 3;

        Game.OnMapCritterIn.Subscribe(OnReentrantMapCritterIn);

        bool caught = false;

        try {
            map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        }
        catch {
            caught = true;
        }

        if (!caught) return -3;
        if (ReentrantMapInCalls != 1) return -4;
        if (!ReentrantMapInSawAttached) return -5;
        if (Game.GetCritter(ReentrantTargetCrId) !is null) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapAddCritterEventMayDestroyMapThrows()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ident locId = loc.Id;
        ident mapId = map.Id;
        ReentrantEventMode = 37;

        Game.OnMapCritterIn.Subscribe(OnReentrantMapCritterIn);

        bool caught = false;

        try {
            map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        }
        catch {
            caught = true;
        }

        if (!caught) return -3;
        if (ReentrantMapInCalls != 1) return -4;
        if (!ReentrantMapInSawAttached) return -5;
        if (Game.GetCritter(ReentrantTargetCrId) !is null) return -6;
        if (Game.GetMap(mapId) !is null) return -7;

        Location? aliveLoc = Game.GetLocation(locId);

        if (aliveLoc !is null) {
            Game.DestroyLocation(aliveLoc);
        }

        return 0;
    }

    int TestMapAddCritterEventMayMoveCritterAwayThrows()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        ReentrantTargetMapId = map2.Id;
        ReentrantEventMode = 35;

        Game.OnMapCritterIn.Subscribe(OnReentrantMapCritterIn);

        bool caught = false;

        try {
            map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        }
        catch {
            caught = true;
        }

        Critter? cr = Game.GetCritter(ReentrantTargetCrId);

        if (!caught) return -3;
        if (ReentrantMapInCalls != 2) return -4;
        if (!ReentrantMapInSawAttached) return -5;
        if (cr is null) return -6;
        if (map1.GetCritter(ReentrantTargetCrId) !is null) return -7;
        if (map2.GetCritter(ReentrantTargetCrId) is null) return -8;
        if (cr.MapId != map2.Id) return -9;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestMapAddCritterInitEventMayDestroyCritterThrows()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ReentrantEventMode = 40;

        Game.OnCritterInit.Subscribe(OnReentrantCritterInit);

        bool caught = false;

        try {
            map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        }
        catch {
            caught = true;
        }

        if (!caught) return -3;
        if (ReentrantCritterInitCalls != 1) return -4;
        if (ReentrantTargetCrId == ZERO_IDENT) return -5;
        if (Game.GetCritter(ReentrantTargetCrId) !is null) return -6;
        if (Game.GetMap(map.Id) is null) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestMapAddCritterInitEventMayDestroyMapThrows()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        ident locId = loc.Id;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ident mapId = map.Id;
        ReentrantEventMode = 41;

        Game.OnCritterInit.Subscribe(OnReentrantCritterInit);

        bool caught = false;

        try {
            map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        }
        catch {
            caught = true;
        }

        if (!caught) return -3;
        if (ReentrantCritterInitCalls != 1) return -4;
        if (ReentrantTargetCrId == ZERO_IDENT) return -5;
        if (Game.GetCritter(ReentrantTargetCrId) !is null) return -6;
        if (Game.GetMap(mapId) !is null) return -7;

        Location? aliveLoc = Game.GetLocation(locId);

        if (aliveLoc !is null) {
            Game.DestroyLocation(aliveLoc);
        }

        return 0;
    }

    int TestMapAddCritterInitEventMayMoveCritterAwayThrows()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        ReentrantTargetMapId = map2.Id;
        ReentrantEventMode = 42;

        Game.OnCritterInit.Subscribe(OnReentrantCritterInit);

        bool caught = false;

        try {
            map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        }
        catch {
            caught = true;
        }

        Critter? cr = Game.GetCritter(ReentrantTargetCrId);

        if (!caught) return -3;
        if (ReentrantCritterInitCalls != 1) return -4;
        if (ReentrantTargetCrId == ZERO_IDENT) return -5;
        if (cr is null) return -6;
        if (map1.GetCritter(ReentrantTargetCrId) !is null) return -7;
        if (map2.GetCritter(ReentrantTargetCrId) is null) return -8;
        if (cr.MapId != map2.Id) return -9;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestGlobalMapCritterOutEventKeepsTrip()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        cr.TransferToGlobal();
        if (cr.GlobalMapTripId == 0) return -4;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 4;

        Game.OnGlobalMapCritterOut.Subscribe(OnReentrantGlobalMapCritterOut);

        cr.TransferToMap(map, mpos(60, 60), mdir(0));

        if (ReentrantGlobalOutCalls != 1) return -5;
        if (!ReentrantGlobalOutSawTrip) return -6;
        if (cr.MapId != map.Id) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestGlobalMapCritterOutEventMayDestroyCritter()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        cr.TransferToGlobal();
        if (cr.GlobalMapTripId == 0) return -4;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 8;

        Game.OnGlobalMapCritterOut.Subscribe(OnReentrantGlobalMapCritterOut);

        cr.TransferToMap(map, mpos(60, 60), mdir(0));

        if (Game.GetCritter(crId) !is null) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestGlobalMapCritterInEventMayDestroyBeforeInitialInfo()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 19;

        Game.OnGlobalMapCritterIn.Subscribe(OnReentrantGlobalMapCritterIn);
        Game.OnCritterSendInitialInfo.Subscribe(OnReentrantCritterSendInitialInfo);

        cr.TransferToGlobal();

        if (ReentrantGlobalInCalls != 1) return -4;
        if (ReentrantInitialInfoCalls != 0) return -5;
        if (Game.GetCritter(crId) !is null) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestGlobalMapAddCritterEventMayDestroyCritterThrows()
    {
        ResetReentrantEventState();

        ReentrantEventMode = 19;

        Game.OnGlobalMapCritterIn.Subscribe(OnReentrantGlobalMapCritterIn);

        bool caught = false;

        try {
            Game.CreateCritter("TestCritter".hstr(), false);
        }
        catch {
            caught = true;
        }

        if (!caught) return -1;
        if (ReentrantGlobalInCalls != 1) return -2;
        if (ReentrantTargetCrId == ZERO_IDENT) return -3;
        if (Game.GetCritter(ReentrantTargetCrId) !is null) return -4;

        return 0;
    }

    int TestGlobalMapLoadCritterEventMayDestroyBeforeCritterLoad()
    {
        ResetReentrantEventState();

        Critter cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return -1;

        cr.MakePersistent(true);

        ident crId = cr.Id;

        Game.UnloadCritter(cr);

        ReentrantTargetCrId = crId;
        ReentrantEventMode = 38;

        Game.OnGlobalMapCritterIn.Subscribe(OnReentrantGlobalMapCritterIn);
        Game.OnCritterLoad.Subscribe(OnReentrantCritterLoad);

        bool caught = false;

        try {
            Game.LoadCritter(crId, true);
        }
        catch {
            caught = true;
        }

        if (!caught) return -2;
        if (ReentrantGlobalInCalls != 1) return -3;
        if (ReentrantCritterLoadCalls != 0) return -4;
        if (Game.GetCritter(crId) !is null) return -5;

        return 0;
    }

    int TestAttachedCritterTransferEventMayDestroyLeaderBeforeFinalTransfer()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        Critter leader = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        Critter follower = map1.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0));
        if (leader is null || follower is null) return -3;

        ident leaderId = leader.Id;
        ident followerId = follower.Id;
        ReentrantTargetCrId = leaderId;
        ReentrantOtherCrId = followerId;
        ReentrantEventMode = 34;

        follower.AttachToCritter(leader);

        Game.OnCritterTransfer.Subscribe(OnReentrantCritterTransfer);

        leader.TransferToMap(map2, mpos(60, 60), mdir(0));

        if (ReentrantOtherCritterTransferCalls != 1) return -4;
        if (ReentrantCritterTransferCalls != 0) return -5;
        if (Game.GetCritter(leaderId) !is null) return -6;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestCritterAppearedEventMayDestroyTarget()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> hiddenProps = {{CritterProperty::LookDistance, 0}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), hiddenProps);
        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), hiddenProps);
        if (observer is null || cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 14;

        observer.OnCritterAppeared.Subscribe(OnReentrantCritterAppeared);

        observer.LookDistance = 10;

        if (ReentrantCritterAppearedCalls != 1) return -4;
        if (!ReentrantCritterAppearedSawOnMap) return -5;
        if (Game.GetCritter(crId) !is null) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestCritterAppearedEventStopsDistanceGroupsAfterTransfer()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        dict<CritterProperty, int> observerProps = {{CritterProperty::LookDistance, 0}, {CritterProperty::ShowCritterDist1, 10}};
        dict<CritterProperty, int> targetProps = {{CritterProperty::LookDistance, 0}};
        Critter observer = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), observerProps);
        Critter cr = map1.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), targetProps);
        if (observer is null || cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantTargetMapId = map2.Id;
        ReentrantEventMode = 15;

        observer.OnCritterAppeared.Subscribe(OnReentrantCritterAppeared);
        observer.OnCritterAppearedDist1.Subscribe(OnReentrantCritterAppearedDist1);

        observer.LookDistance = 10;

        if (ReentrantCritterAppearedCalls != 1) return -4;
        if (ReentrantCritterAppearedDist1Calls != 0) return -5;
        if (Game.GetCritter(crId) is null) return -6;
        if (cr.MapId != map2.Id) return -7;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestCritterAppearedDistEventStopsNextDistanceGroupsAfterTransfer()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        dict<CritterProperty, int> observerProps = {{CritterProperty::LookDistance, 0}, {CritterProperty::ShowCritterDist1, 10}, {CritterProperty::ShowCritterDist2, 10}};
        dict<CritterProperty, int> targetProps = {{CritterProperty::LookDistance, 0}};
        Critter observer = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), observerProps);
        Critter cr = map1.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), targetProps);
        if (observer is null || cr is null) return -3;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantTargetMapId = map2.Id;
        ReentrantEventMode = 16;

        observer.OnCritterAppearedDist1.Subscribe(OnReentrantCritterAppearedDist1);
        observer.OnCritterAppearedDist2.Subscribe(OnReentrantCritterAppearedDist2);

        observer.LookDistance = 10;

        if (ReentrantCritterAppearedDist1Calls != 1) return -4;
        if (ReentrantCritterAppearedDist2Calls != 0) return -5;
        if (Game.GetCritter(crId) is null) return -6;
        if (cr.MapId != map2.Id) return -7;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestCritterDisappearedEventStopsDistanceGroupsAfterTransfer()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        dict<CritterProperty, int> observerProps = {{CritterProperty::LookDistance, 10}, {CritterProperty::ShowCritterDist1, 10}};
        dict<CritterProperty, int> targetProps = {{CritterProperty::LookDistance, 0}};
        Critter observer = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), observerProps);
        Critter cr = map1.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), targetProps);
        if (observer is null || cr is null) return -3;

        array<Critter> seen = observer.GetCritters(CritterSeeType::WhoISee, CritterFindType::Any);
        bool observerSeesCr = false;
        for (uint i = 0; i < seen.length(); i++) {
            if (seen[i].Id == cr.Id) {
                observerSeesCr = true;
                break;
            }
        }
        if (!observerSeesCr) return -4;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantTargetMapId = map2.Id;
        ReentrantEventMode = 17;

        observer.OnCritterDisappeared.Subscribe(OnReentrantCritterDisappeared);
        observer.OnCritterDisappearedDist1.Subscribe(OnReentrantCritterDisappearedDist1);

        observer.LookDistance = 0;

        if (ReentrantCritterDisappearedCalls != 1) return -5;
        if (ReentrantCritterDisappearedDist1Calls != 0) return -6;
        if (Game.GetCritter(crId) is null) return -7;
        if (cr.MapId != map2.Id) return -8;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestItemOnMapAppearedEventMayDestroyItem()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        Critter dropper = map.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), props);
        if (observer is null || dropper is null) return -3;

        Item item = dropper.AddItem("TestItem".hstr(), 1);
        if (item is null) return -4;

        ident itemId = item.Id;
        ReentrantTargetItemId = itemId;
        ReentrantEventMode = 9;

        observer.OnItemOnMapAppeared.Subscribe(OnReentrantItemOnMapAppeared);

        Item? moved = Game.MoveItem(item, map, mpos(52, 50));

        if (ReentrantItemAppearedCalls != 1) return -5;
        if (!ReentrantItemAppearedSawOnMap) return -6;
        if (moved !is null) return -7;
        if (Game.GetItem(itemId) !is null) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestItemOnMapAppearedEventMayMoveItemAway()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        Critter source = map.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), props);
        Critter receiver = map.AddCritter("TestCritter".hstr(), mpos(53, 50), mdir(0), props);
        if (observer is null || source is null || receiver is null) return -3;

        Item item = source.AddItem("TestItem2".hstr(), 1);
        if (item is null) return -4;

        ident itemId = item.Id;
        ReentrantTargetItemId = itemId;
        ReentrantTargetCrId = receiver.Id;
        ReentrantEventMode = 30;

        observer.OnItemOnMapAppeared.Subscribe(OnReentrantItemOnMapAppeared);

        Item? moved = Game.MoveItem(item, map, mpos(52, 50));

        if (ReentrantItemAppearedCalls != 1) return -5;
        if (!ReentrantItemAppearedSawOnMap) return -6;
        if (moved !is null) return -7;
        if (map.GetItem(itemId) !is null) return -8;
        if (receiver.GetItem(itemId) is null) return -9;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapAddItemEventMayDestroyItemThrows()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        if (observer is null) return -3;

        ReentrantEventMode = 9;

        observer.OnItemOnMapAppeared.Subscribe(OnReentrantItemOnMapAppeared);

        bool caught = false;

        try {
            map.AddItem(mpos(52, 50), "TestItem".hstr(), 1);
        }
        catch {
            caught = true;
        }

        if (!caught) return -4;
        if (ReentrantItemAppearedCalls != 1) return -5;
        if (!ReentrantItemAppearedSawOnMap) return -6;
        if (Game.GetItem(ReentrantTargetItemId) !is null) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestMapAddItemEventMayDestroyMapThrows()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ident locId = loc.Id;
        ident mapId = map.Id;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        if (observer is null) return -3;

        ReentrantEventMode = 38;

        observer.OnItemOnMapAppeared.Subscribe(OnReentrantItemOnMapAppeared);

        bool caught = false;

        try {
            map.AddItem(mpos(52, 50), "TestItem".hstr(), 1);
        }
        catch {
            caught = true;
        }

        if (!caught) return -4;
        if (ReentrantItemAppearedCalls != 1) return -5;
        if (!ReentrantItemAppearedSawOnMap) return -6;
        if (Game.GetItem(ReentrantTargetItemId) !is null) return -7;
        if (Game.GetMap(mapId) !is null) return -8;

        Location? aliveLoc = Game.GetLocation(locId);

        if (aliveLoc !is null) {
            Game.DestroyLocation(aliveLoc);
        }

        return 0;
    }

    int TestMapAddItemEventMayMoveItemAwayThrows()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        Critter receiver = map.AddCritter("TestCritter".hstr(), mpos(53, 50), mdir(0), props);
        if (observer is null || receiver is null) return -3;

        ReentrantTargetCrId = receiver.Id;
        ReentrantEventMode = 30;

        observer.OnItemOnMapAppeared.Subscribe(OnReentrantItemOnMapAppeared);

        bool caught = false;

        try {
            map.AddItem(mpos(52, 50), "TestItem2".hstr(), 1);
        }
        catch {
            caught = true;
        }

        if (!caught) return -4;
        if (ReentrantItemAppearedCalls != 1) return -5;
        if (!ReentrantItemAppearedSawOnMap) return -6;
        if (map.GetItem(ReentrantTargetItemId) !is null) return -7;
        if (receiver.GetItem(ReentrantTargetItemId) is null) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestItemOnMapDisappearedEventMayDestroyItem()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        Critter receiver = map.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), props);
        if (observer is null || receiver is null) return -3;

        Item item = map.AddItem(mpos(52, 50), "TestItem".hstr(), 1);
        if (item is null) return -4;

        ident itemId = item.Id;
        ReentrantTargetItemId = itemId;
        ReentrantEventMode = 10;

        observer.OnItemOnMapDisappeared.Subscribe(OnReentrantItemOnMapDisappeared);

        Item? moved = Game.MoveItem(item, receiver);

        if (ReentrantItemDisappearedCalls != 1) return -5;
        if (!ReentrantItemDisappearedSawDetached) return -6;
        if (moved !is null) return -7;
        if (Game.GetItem(itemId) !is null) return -8;
        if (receiver.GetItem(itemId) !is null) return -9;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestItemOnMapChangedEventMayDestroyItem()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        if (observer is null) return -3;

        Item item = map.AddItem(mpos(51, 50), "TestItem".hstr(), 1);
        if (item is null) return -4;

        ident itemId = item.Id;
        ReentrantTargetItemId = itemId;
        ReentrantEventMode = 11;

        observer.OnItemOnMapChanged.Subscribe(OnReentrantItemOnMapChanged);

        item.Count = item.Count + 1;

        if (ReentrantItemChangedCalls != 1) return -5;
        if (!ReentrantItemChangedSawOnMap) return -6;
        if (Game.GetItem(itemId) !is null) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestItemOnMapChangedEventMayMoveItemAwayBeforeNextObserver()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        dict<CritterProperty, int> props = {{CritterProperty::LookDistance, 10}};
        Critter observer1 = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0), props);
        Critter observer2 = map.AddCritter("TestCritter".hstr(), mpos(51, 50), mdir(0), props);
        Critter receiver = map.AddCritter("TestCritter".hstr(), mpos(53, 50), mdir(0), props);
        if (observer1 is null || observer2 is null || receiver is null) return -3;

        Item item = map.AddItem(mpos(52, 50), "TestItem".hstr(), 1);
        if (item is null) return -4;

        ident itemId = item.Id;
        ReentrantTargetItemId = itemId;
        ReentrantTargetCrId = receiver.Id;
        ReentrantEventMode = 33;

        observer1.OnItemOnMapChanged.Subscribe(OnReentrantItemOnMapChanged);
        observer2.OnItemOnMapChanged.Subscribe(OnReentrantItemOnMapChanged);

        item.Count = item.Count + 1;

        if (ReentrantItemChangedCalls != 1) return -5;
        if (!ReentrantItemChangedSawOnMap) return -6;
        if (map.GetItem(itemId) !is null) return -7;
        if (receiver.GetItem(itemId) is null) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestCritterItemMovedEventMayDestroyItem()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -4;

        ident itemId = item.Id;
        ReentrantTargetItemId = itemId;
        ReentrantEventMode = 12;

        Game.OnCritterItemMoved.Subscribe(OnReentrantCritterItemMoved);

        Item? moved = Game.MoveItem(item, map, mpos(51, 50));

        if (ReentrantCritterItemMovedCalls != 1) return -5;
        if (moved !is null) return -6;
        if (Game.GetItem(itemId) !is null) return -7;
        if (map.GetItem(itemId) !is null) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestCritterItemMovedAddEventMayMoveItemAway()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter receiver = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (receiver is null) return -3;

        Item item = map.AddItem(mpos(51, 50), "TestItem2".hstr(), 1);
        if (item is null) return -4;

        ident itemId = item.Id;
        ReentrantTargetItemId = itemId;
        ReentrantTargetMapId = map.Id;
        ReentrantEventMode = 28;

        Game.OnCritterItemMoved.Subscribe(OnReentrantCritterItemMoved);

        bool caught = false;

        try {
            Game.MoveItem(item, receiver);
        }
        catch {
            caught = true;
        }

        if (ReentrantCritterItemMovedCalls != 1) return -5;
        if (!caught) return -6;
        if (map.GetItem(itemId) is null) return -7;
        if (receiver.GetItem(itemId) !is null) return -8;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestCritterItemMovedSwapEventMayMoveOriginalItem()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        Item equipped = cr.AddItem("TestItem".hstr(), 1);
        Item moving = cr.AddItem("TestItem2".hstr(), 1);
        if (equipped is null || moving is null) return -4;

        cr.ChangeItemSlot(equipped.Id, CritterItemSlot::Main);
        if (equipped.CritterSlot != CritterItemSlot::Main) return -5;
        if (moving.CritterSlot != CritterItemSlot::Inventory) return -6;

        ident equippedId = equipped.Id;
        ident movingId = moving.Id;
        ReentrantTargetItemId = equippedId;
        ReentrantOtherItemId = movingId;
        ReentrantTargetMapId = map.Id;
        ReentrantEventMode = 27;

        Game.OnCritterItemMoved.Subscribe(OnReentrantCritterItemMoved);

        cr.ChangeItemSlot(moving.Id, CritterItemSlot::Main);

        if (ReentrantCritterItemMovedCalls != 1) return -7;
        if (ReentrantOtherCritterItemMovedFromMainCalls != 1) return -8;
        if (ReentrantOtherCritterItemMovedFromInventoryCalls != 1) return -9;
        if (map.GetItem(movingId) is null) return -10;
        if (cr.GetItem(movingId) !is null) return -11;
        if (equipped.CritterSlot != CritterItemSlot::Inventory) return -12;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestCritterItemMovedSwapEventMayDestroyOriginalItem()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        Item equipped = cr.AddItem("TestItem".hstr(), 1);
        Item moving = cr.AddItem("TestItem2".hstr(), 1);
        if (equipped is null || moving is null) return -4;

        cr.ChangeItemSlot(equipped.Id, CritterItemSlot::Main);
        if (equipped.CritterSlot != CritterItemSlot::Main) return -5;
        if (moving.CritterSlot != CritterItemSlot::Inventory) return -6;

        ident equippedId = equipped.Id;
        ident movingId = moving.Id;
        ReentrantTargetItemId = equippedId;
        ReentrantOtherItemId = movingId;
        ReentrantEventMode = 36;

        Game.OnCritterItemMoved.Subscribe(OnReentrantCritterItemMoved);

        cr.ChangeItemSlot(moving.Id, CritterItemSlot::Main);

        if (ReentrantCritterItemMovedCalls != 1) return -7;
        if (ReentrantOtherCritterItemMovedFromInventoryCalls != 1) return -8;
        if (Game.GetItem(movingId) !is null) return -9;
        if (cr.GetItem(movingId) !is null) return -10;
        if (equipped.CritterSlot != CritterItemSlot::Inventory) return -11;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestCritterWalkEventMayDestroyCritter()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        Item item = map.AddItem(mpos(51, 50), "TestItem".hstr(), 1);
        if (item is null) return -4;

        item.IsTrigger = true;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantEventMode = 24;

        item.OnCritterWalk.Subscribe(OnReentrantCritterWalk);

        map.VerifyTrigger(cr, mpos(51, 50), mdir(0));

        if (ReentrantCritterWalkCalls != 1) return -5;
        if (Game.GetCritter(crId) !is null) return -6;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestCritterWalkEventMayDestroyItem()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        Item item = map.AddItem(mpos(51, 50), "TestItem".hstr(), 1);
        if (item is null) return -4;

        item.IsTrigger = true;

        ident crId = cr.Id;
        ident itemId = item.Id;
        ReentrantTargetCrId = crId;
        ReentrantTargetItemId = itemId;
        ReentrantEventMode = 25;

        item.OnCritterWalk.Subscribe(OnReentrantCritterWalk);

        map.VerifyTrigger(cr, mpos(51, 50), mdir(0));

        if (ReentrantCritterWalkCalls != 1) return -5;
        if (Game.GetCritter(crId) is null) return -6;
        if (Game.GetItem(itemId) !is null) return -7;

        Game.DestroyLocation(loc);
        return 0;
    }

 )" + R"(
    int TestCritterWalkEventMayTransferCritterBeforeNextTrigger()
    {
        ResetReentrantEventState();

        Location loc1 = CreateTestLocation();
        Location loc2 = CreateTestLocation();
        if (loc1 is null || loc2 is null) return -1;

        Map map1 = loc1.GetMapByIndex(0);
        Map map2 = loc2.GetMapByIndex(0);
        if (map1 is null || map2 is null) return -2;

        Critter cr = map1.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        Item firstTrigger = map1.AddItem(mpos(51, 50), "TestItem".hstr(), 1);
        Item secondTrigger = map1.AddItem(mpos(51, 50), "TestItem2".hstr(), 1);
        if (firstTrigger is null || secondTrigger is null) return -4;

        firstTrigger.IsTrigger = true;
        secondTrigger.IsTrigger = true;

        ident crId = cr.Id;
        ReentrantTargetCrId = crId;
        ReentrantTargetItemId = firstTrigger.Id;
        ReentrantOtherItemId = secondTrigger.Id;
        ReentrantTargetMapId = map2.Id;
        ReentrantEventMode = 31;

        firstTrigger.OnCritterWalk.Subscribe(OnReentrantCritterWalk);
        secondTrigger.OnCritterWalk.Subscribe(OnReentrantCritterWalk);

        map1.VerifyTrigger(cr, mpos(51, 50), mdir(0));

        if (ReentrantCritterWalkCalls != 1) return -5;
        if (ReentrantOtherCritterWalkCalls != 0) return -6;
        if (Game.GetCritter(crId) is null) return -7;
        if (cr.MapId != map2.Id) return -8;
        if (map1.GetCritter(crId) !is null) return -9;
        if (map2.GetCritter(crId) is null) return -10;

        Game.DestroyLocation(loc1);
        Game.DestroyLocation(loc2);
        return 0;
    }

    int TestCritterWalkEventMayDestroyMapBeforeNextTrigger()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(50, 50), mdir(0));
        if (cr is null) return -3;

        Item firstTrigger = map.AddItem(mpos(51, 50), "TestItem".hstr(), 1);
        Item secondTrigger = map.AddItem(mpos(51, 50), "TestItem2".hstr(), 1);
        if (firstTrigger is null || secondTrigger is null) return -4;

        firstTrigger.IsTrigger = true;
        secondTrigger.IsTrigger = true;

        ident locId = loc.Id;
        ident mapId = map.Id;
        ident crId = cr.Id;
        ident secondTriggerId = secondTrigger.Id;
        ReentrantTargetLocId = locId;
        ReentrantTargetCrId = crId;
        ReentrantTargetItemId = firstTrigger.Id;
        ReentrantOtherItemId = secondTriggerId;
        ReentrantEventMode = 32;

        firstTrigger.OnCritterWalk.Subscribe(OnReentrantCritterWalk);
        secondTrigger.OnCritterWalk.Subscribe(OnReentrantCritterWalk);

        map.VerifyTrigger(cr, mpos(51, 50), mdir(0));

        if (ReentrantCritterWalkCalls != 1) return -5;
        if (ReentrantOtherCritterWalkCalls != 0) return -6;
        if (Game.GetLocation(locId) !is null) return -7;
        if (Game.GetMap(mapId) !is null) return -8;
        if (Game.GetCritter(crId) !is null) return -9;
        if (Game.GetItem(secondTriggerId) !is null) return -10;

        return 0;
    }

 )" + R"(
    int TestMapFinishEventCannotDestroyOwningLocation()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ident locId = loc.Id;
        ident mapId = map.Id;
        ReentrantTargetMapId = mapId;
        ReentrantTargetLocId = locId;
        ReentrantEventMode = 20;

        Game.OnMapFinish.Subscribe(OnReentrantMapFinish);

        Game.DestroyMap(map);

        if (ReentrantMapFinishCalls != 1) return -3;
        if (ReentrantNestedDestroyCaught != 1) return -4;
        if (Game.GetLocation(locId) is null) return -5;
        if (Game.GetMap(mapId) !is null) return -6;

        Location? aliveLoc = Game.GetLocation(locId);
        if (aliveLoc is null) return -7;
        Game.DestroyLocation(aliveLoc);
        return 0;
    }

    int TestMapRemovedEventCannotDestroyOwningLocation()
    {
        ResetReentrantEventState();

        Location loc = CreateTestLocation();
        if (loc is null) return -1;

        Map map = loc.GetMapByIndex(0);
        if (map is null) return -2;

        ident locId = loc.Id;
        ident mapId = map.Id;
        ReentrantTargetMapId = mapId;
        ReentrantTargetLocId = locId;
        ReentrantEventMode = 21;

        loc.OnMapRemoved.Subscribe(OnReentrantMapRemoved);

        Game.DestroyMap(map);

        if (ReentrantMapRemovedCalls != 1) return -3;
        if (ReentrantNestedDestroyCaught != 1) return -4;
        if (Game.GetLocation(locId) is null) return -5;
        if (Game.GetMap(mapId) !is null) return -6;

        Location? aliveLoc = Game.GetLocation(locId);
        if (aliveLoc is null) return -7;
        Game.DestroyLocation(aliveLoc);
        return 0;
    }

    int TestMapInitEventMayDestroyMap()
    {
        ResetReentrantEventState();

        ReentrantEventMode = 22;
        Game.OnMapInit.Subscribe(OnReentrantMapInit);

        array<hstring> mapPids = {"TestMap".hstr()};
        Location loc = Game.CreateLocation("TestLocation".hstr(), mapPids);
        if (loc is null) return -1;

        ident mapId = ReentrantTargetMapId;
        if (mapId == ZERO_IDENT) return -2;
        if (ReentrantMapInitCalls != 1) return -3;
        if (Game.GetMap(mapId) !is null) return -4;
        if (loc.GetMapCount() != 0) return -5;

        Game.DestroyLocation(loc);
        return 0;
    }

    int TestMapAddedEventMayDestroyLocation()
    {
        ResetReentrantEventState();

        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        ident locId = loc.Id;
        ReentrantTargetLocId = locId;
        ReentrantEventMode = 23;
        loc.OnMapAdded.Subscribe(OnReentrantMapAdded);

        bool caught = false;

        try {
            loc.AddMap("TestMap".hstr());
        }
        catch {
            caught = true;
        }

        ident mapId = ReentrantTargetMapId;
        if (!caught) return -2;
        if (ReentrantMapAddedCalls != 1) return -3;
        if (mapId == ZERO_IDENT) return -4;
        if (Game.GetMap(mapId) !is null) return -5;
        if (Game.GetLocation(locId) !is null) return -6;
        return 0;
    }

    int TestMapAddedEventMayDestroyMap()
    {
        ResetReentrantEventState();

        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        ident locId = loc.Id;
        ReentrantTargetLocId = locId;
        ReentrantEventMode = 39;
        loc.OnMapAdded.Subscribe(OnReentrantMapAdded);

        bool caught = false;

        try {
            loc.AddMap("TestMap".hstr());
        }
        catch {
            caught = true;
        }

        ident mapId = ReentrantTargetMapId;
        Location? aliveLoc = Game.GetLocation(locId);

        if (!caught) return -2;
        if (ReentrantMapAddedCalls != 1) return -3;
        if (mapId == ZERO_IDENT) return -4;
        if (Game.GetMap(mapId) !is null) return -5;
        if (aliveLoc is null) return -6;
        if (aliveLoc.GetMapCount() != 0) return -7;

        Game.DestroyLocation(aliveLoc);
        return 0;
    }

    // ========== Script-boundary argument validation (2026-06-16 hardening) ==========
    // Each function passes an out-of-range argument that must be rejected with an early, clearly
    // messaged ScriptException at the script-export boundary, instead of reaching a deep numeric_cast
    // / FO_VERIFY_* / std::string::resize. Driven by RUN_FUNC_THROWS, which asserts the message.

    bool ArgValidationDummyGag(Critter cr, Item item)
    {
        return false;
    }

    void TestArgItemsInRadiusNegativeRadiusThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return;
        map.GetItemsInRadius(mpos(10, 10), -1);
    }

    void TestArgCrittersInRadiusNegativeRadiusThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return;
        map.GetCrittersInRadius(mpos(10, 10), -1, CritterFindType::Any);
    }

    void TestArgIsHexesMovableNegativeRadiusThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return;
        map.IsHexesMovable(mpos(10, 10), -1);
    }

    void TestArgTransferToMapOutOfBoundsHexThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return;
        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        if (cr is null) return;
        cr.TransferToMap(map, mpos(250, 250));
    }

    void TestArgMoveToHexNegativeSpeedThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return;
        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        if (cr is null) return;
        cr.MoveToHex(mpos(12, 12), 0, -1, ArgValidationDummyGag);
    }

    void TestArgChangeMovingSpeedNegativeThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return;
        Critter cr = map.AddCritter("TestCritter".hstr(), mpos(10, 10), mdir(0));
        if (cr is null) return;
        cr.ChangeMovingSpeed(-1);
    }

    void TestArgTraceHexLineOffsetOutOfRangeThrows()
    {
        Location loc = CreateTestLocation();
        if (loc is null) return;
        Map map = loc.GetMapByIndex(0);
        if (map is null) return;
        Game.TraceHexLine(map.Size, mpos(5, 5), mpos(10, 10), 1, 0.0, ipos(100000, 0), ipos(0, 0));
    }

    void TestArgDestroyEntityPlayerCritterThrows()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), true);
        if (cr is null) return;
        Game.DestroyEntity(cr);
    }

    void TestArgStringRawResizeNegativeThrows()
    {
        string text = "abc";
        text.rawResize(-1);
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

    static auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = DataWriter(map_data);
        writer.Write<uint32_t>(uint32_t {0}); // hashes_count
        writer.Write<uint32_t>(uint32_t {0}); // cr_count
        writer.Write<uint32_t>(uint32_t {0}); // item_count
        return map_data;
    }

    static auto MakeMapProtoBlob(EngineMetadata& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrator = proto_engine.GetPropertyRegistrator(type_name);
        REQUIRE(static_cast<bool>(registrator));

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), registrator.as_ptr()};
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

    static auto MakeStackableItemProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrator = proto_engine.GetPropertyRegistrator(type_name);
        REQUIRE(static_cast<bool>(registrator));

        ProtoItem proto {proto_engine.Hashes.ToHashedString(proto_name), registrator.as_ptr()};
        proto.SetStackable(true);
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

    static auto MakeStaticItemProtoBlob(EngineMetadata& proto_engine, hstring type_name, bool set_hidden) -> vector<uint8_t>
    {
        return BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, type_name,
            {
                {"TestStaticItem",
                    [set_hidden](ProtoItem& proto) {
                        proto.SetStatic(true);
                        if (set_hidden) {
                            proto.SetHidden(false);
                        }
                    }},
                {"TestStaticHiddenItem",
                    [set_hidden](ProtoItem& proto) {
                        proto.SetStatic(true);
                        if (set_hidden) {
                            proto.SetHidden(true);
                        }
                    }},
            });
    }

    static auto MakeStaticMapBlob(const vector<uint8_t>& metadata_blob, const vector<uint8_t>& critter_blob, const vector<uint8_t>& server_item_blob, const vector<uint8_t>& client_item_blob, const vector<uint8_t>& server_map_blob, const vector<uint8_t>& client_map_blob) -> vector<uint8_t>
    {
        BakerTests::TestRig rig;
        rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
        rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
        rig.AddBakedFile("StaticMapCritter.fopro-bin-server", critter_blob);
        rig.AddBakedFile("StaticMapItems.fopro-bin-server", server_item_blob);
        rig.AddBakedFile("StaticMapItems.fopro-bin-client", client_item_blob);
        rig.AddBakedFile("StaticMap.fopro-bin-server", server_map_blob);
        rig.AddBakedFile("StaticMap.fopro-bin-client", client_map_blob);

#if FO_ANGELSCRIPT_SCRIPTING
        BakerServerEngine script_engine {rig.BakedFiles};
        const vector<uint8_t> script_blob = BakerTests::CompileInlineScripts(&script_engine, "StaticMapScripts", {{"Scripts/StaticMapScripts.fos", "namespace StaticMapScripts\n{\nvoid Dummy()\n{\n}\n}\n"}}, [](string_view message) {
            const string message_str = string(message);

            if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                throw ScriptSystemException(message_str);
            }
        });

        rig.AddBakedFile("StaticMapScripts.fos-bin-server", script_blob);
#endif

        rig.AddSourceFile("StaticMap.fomap",
            "[ProtoMap]\n"
            "$Name = StaticMap\n"
            "[Critter]\n"
            "$Id = 11\n"
            "$Proto = TestStaticCritter\n"
            "Hex = 10 11\n"
            "[Item]\n"
            "$Id = 21\n"
            "$Proto = TestStaticItem\n"
            "Hex = 12 13\n"
            "[Item]\n"
            "$Id = 22\n"
            "$Proto = TestStaticHiddenItem\n"
            "Hex = 14 15\n");

        MapBaker baker {rig.MakeContext("Maps")};
        baker.BakeFiles(rig.GetAllSourceFiles(), "StaticMap.fomap-bin-server");

        FO_VERIFY_AND_THROW(rig.Outputs.contains("StaticMap.fomap-bin-server"), "Static map was not baked");
        return std::move(rig.Outputs.at("StaticMap.fomap-bin-server"));
    }

    static auto MakeResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapOpsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);
        compiler_resources_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        BakerClientEngine client_proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");
        const auto location_type = proto_engine.Hashes.ToHashedString("Location");
        const auto map_type = proto_engine.Hashes.ToHashedString("Map");
        const auto client_item_type = client_proto_engine.Hashes.ToHashedString("Item");
        const auto client_map_type = client_proto_engine.Hashes.ToHashedString("Map");

        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        const auto static_critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestStaticCritter");
        const auto item_blob = MakeStackableItemProtoBlob(proto_engine, item_type, "TestItem");
        const auto item2_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem2");
        const auto static_item_blob = MakeStaticItemProtoBlob(proto_engine, item_type, true);
        const auto static_item_client_blob = MakeStaticItemProtoBlob(client_proto_engine, client_item_type, false);
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        const auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "TestMap", msize {200, 200});
        const auto static_map_proto_blob = MakeMapProtoBlob(proto_engine, map_type, "StaticMap", msize {50, 50});
        const auto static_map_client_proto_blob = MakeMapProtoBlob(client_proto_engine, client_map_type, "StaticMap", msize {50, 50});
        const auto fomap_blob = MakeEmptyMapBlob();
        const auto static_fomap_blob = MakeStaticMapBlob(metadata_blob, static_critter_blob, static_item_blob, static_item_client_blob, static_map_proto_blob, static_map_client_proto_blob);
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapOpsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("MapOpsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("MapOpsStaticCritter.fopro-bin-server", static_critter_blob);
        runtime_source->AddFile("MapOpsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("MapOpsItem2.fopro-bin-server", item2_blob);
        runtime_source->AddFile("MapOpsStaticItems.fopro-bin-server", static_item_blob);
        runtime_source->AddFile("MapOpsLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("TestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("StaticMap.fopro-bin-server", static_map_proto_blob);
        runtime_source->AddFile("TestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("StaticMap.fomap-bin-server", static_fomap_blob);
        runtime_source->AddFile("MapOpsTest.fos-bin-server", script_blob);

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

#define MAKE_SERVER \
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

#define RUN_FUNC(func_name) \
    { \
        auto func = server->FindFunc<int32_t>(get_func(func_name)); \
        REQUIRE(func); \
        REQUIRE(func.Call()); \
        CHECK(func.GetResult() == 0); \
    }

#define RUN_FUNC_THROWS(func_name, expected_message) \
    { \
        auto func = server->FindFunc<void>(get_func(func_name)); \
        REQUIRE(func); \
        const auto prev_callback = GetExceptionCallback(); \
        string message; \
        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData&, bool) { message = string(msg); }); \
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { SetExceptionCallback(std::move(prev)); }); \
        CHECK_FALSE(func.Call()); \
        CHECK(message.find(expected_message) != string::npos); \
    }

TEST_CASE("MapItemOperations")
{
    MAKE_SERVER;

    SECTION("AddItemByPid")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemByPid");
    }

    SECTION("AddItemInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddItemInvalidCountThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemInvalidCountThrows", "Count arg must be positive");
    }

    SECTION("AddItemMultiple")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemMultiple");
    }

    SECTION("GameDistanceOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameDistanceOverloads");
    }

    SECTION("GameDistanceExceptionBranches")
    {
        RUN_FUNC("MapOpsTest::TestGameDistanceExceptionBranches");
    }

    SECTION("GameMoveItemMapAndContainerOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameMoveItemMapAndContainerOverloads");
    }

    SECTION("GameMoveItemsOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameMoveItemsOverloads");
    }

    SECTION("GameDestroyItemByIdCountOverload")
    {
        RUN_FUNC("MapOpsTest::TestGameDestroyItemByIdCountOverload");
    }

    SECTION("GetItemOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemOnHex");
    }

    SECTION("GetItemOnHexInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemOnHexInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemOnHexByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemOnHexByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemOnHexByPropertyInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemOnHexByPropertyInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemInRadius")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemInRadius");
    }

    SECTION("GetItemsInRadiusNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemsInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("GetItemsInRadiusInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemsInRadiusInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemsByNullProtoThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemsByNullProtoThrows");
    }
}

TEST_CASE("MapCritterOperations")
{
    MAKE_SERVER;

    SECTION("AddCritter")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritter");
    }

    SECTION("AddCritterInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddCritterByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetCritterOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCritterOnHex");
    }

    SECTION("GetCritterOnHexInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetCritterOnHexInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetCrittersByFindType")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersByFindType");
    }

    SECTION("GetCrittersOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersOnHex");
    }

    SECTION("GetCrittersInRadiusNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetCrittersInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("GetCrittersInRadiusInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetCrittersInRadiusInvalidHexThrows", "Invalid hex arg");
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

    SECTION("HexMovableInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsHexMovableInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("HexesMovableNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsHexesMovableNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("HexShootableInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsHexShootableInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("OutsideAreaInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsOutsideAreaInvalidHexThrows", "Invalid hex arg");
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

    SECTION("CheckPlaceForItemInvalidPidThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapCheckPlaceForItemInvalidPidThrows", "Invalid item proto id arg");
    }
}

TEST_CASE("MapPathOperations")
{
    MAKE_SERVER;

    SECTION("PathLength")
    {
        RUN_FUNC("MapOpsTest::TestMapPathLength");
    }

    SECTION("PathLengthInvalidFromHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapPathLengthInvalidFromHexThrows", "Invalid from hex args");
    }

    SECTION("PathLengthInvalidToHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapPathLengthInvalidToHexThrows", "Invalid to hex args");
    }

    SECTION("CritterPathLengthInvalidToHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapCritterPathLengthInvalidToHexThrows", "Invalid to hex args");
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

    SECTION("GameStaticMapQueries")
    {
        RUN_FUNC("MapOpsTest::TestGameStaticMapQueries");
    }

    SECTION("GetStaticItemsOnHexInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsOnHexInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsInRadiusNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("GetStaticItemsInRadiusInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsInRadiusByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsOnHexByPropertyInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsOnHexByPropertyInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsInRadiusByPropertyInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusByPropertyInvalidHexThrows", "Invalid hex arg");
    }
}

TEST_CASE("MapManagerLoadsStaticMapEntities")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeResources());
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

    const auto static_map_pid = server->Hashes.ToHashedString("StaticMap");
    const auto static_critter_pid = server->Hashes.ToHashedString("TestStaticCritter");
    const auto visible_item_pid = server->Hashes.ToHashedString("TestStaticItem");
    const auto hidden_item_pid = server->Hashes.ToHashedString("TestStaticHiddenItem");
    const auto map_proto = server->GetProtoMap(static_map_pid);
    REQUIRE(map_proto);

    auto static_map = server->MapMngr.GetStaticMap(map_proto.as_ptr());

    REQUIRE(static_map->CritterBillets.size() == 1);
    CHECK(static_map->CritterBillets.front().first == ident_t {11});
    CHECK(static_map->CritterBillets.front().second->GetProtoId() == static_critter_pid);
    CHECK(static_map->CritterBillets.front().second->GetHex() == mpos {10, 11});

    REQUIRE(static_map->ItemBillets.size() == 2);
    REQUIRE(static_map->StaticItems.size() == 2);
    REQUIRE(static_map->StaticItemsById.contains(ident_t {21}));
    REQUIRE(static_map->StaticItemsById.contains(ident_t {22}));

    const auto visible_item = static_map->StaticItemsById.at(ident_t {21});
    const auto hidden_item = static_map->StaticItemsById.at(ident_t {22});
    CHECK(visible_item->GetProtoId() == visible_item_pid);
    CHECK(hidden_item->GetProtoId() == hidden_item_pid);
    CHECK(visible_item->GetHex() == mpos {12, 13});
    CHECK(hidden_item->GetHex() == mpos {14, 15});

    const auto& visible_field = static_map->HexField->GetCellForReading(mpos {12, 13});
    CHECK(std::ranges::find(visible_field.StaticItems, visible_item) != visible_field.StaticItems.end());
    CHECK(visible_field.MoveBlocked);
    CHECK(visible_field.ShootBlocked);

    const auto& hidden_field = static_map->HexField->GetCellForReading(mpos {14, 15});
    CHECK(std::ranges::find(hidden_field.StaticItems, hidden_item) != hidden_field.StaticItems.end());
    CHECK(hidden_field.MoveBlocked);
    CHECK(hidden_field.ShootBlocked);
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

    SECTION("LocationMethodBindings")
    {
        RUN_FUNC("MapOpsTest::TestLocationMethodBindings");
    }

    SECTION("GameLocationAndMapOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameLocationAndMapOverloads");
    }

    SECTION("CreateLocationInvalidProtoPropsThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestCreateLocationInvalidProtoPropsThrows", "Invalid location proto id arg");
    }

    SECTION("CreateLocationInvalidProtoMapPropsThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestCreateLocationInvalidProtoMapPropsThrows", "Invalid location proto id arg");
    }

    SECTION("GameGetLocations")
    {
        RUN_FUNC("MapOpsTest::TestGameGetLocations");
    }
}

TEST_CASE("MapProtoOverloads")
{
    MAKE_SERVER;

    SECTION("AddItemByProto")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemByProto");
    }

    SECTION("AddItemByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddItemByProtoInvalidCountThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemByProtoInvalidCountThrows", "Count arg must be positive");
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

    SECTION("SetupScriptExMissingThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScriptExMissingThrows");
    }

    SECTION("SetupScriptDelegateThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScriptDelegateThrows");
    }
}

TEST_CASE("MapAddWithProperties")
{
    MAKE_SERVER;

    SECTION("AddItemWithProperties")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemWithProperties");
    }

    SECTION("AddItemWithPropertiesInvalidProtoThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithPropertiesInvalidProtoThrows", "Invalid item proto id arg");
    }

    SECTION("AddItemWithPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithPropertiesInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddItemWithPropertiesInvalidCountThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithPropertiesInvalidCountThrows", "Count arg must be positive");
    }

    SECTION("AddItemWithProtoPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithProtoPropertiesInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddItemWithProtoPropertiesInvalidCountThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithProtoPropertiesInvalidCountThrows", "Count arg must be positive");
    }

    SECTION("AddCritterWithProperties")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterWithProperties");
    }

    SECTION("AddCritterWithIntPropertiesInvalidProtoThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithIntPropertiesInvalidProtoThrows", "Invalid critter proto id arg");
    }

    SECTION("AddCritterWithAnyPropertiesInvalidProtoThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithAnyPropertiesInvalidProtoThrows", "Invalid critter proto id arg");
    }

    SECTION("AddCritterWithIntPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithIntPropertiesInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddCritterWithAnyPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithAnyPropertiesInvalidHexThrows", "Invalid hex arg");
    }
}

TEST_CASE("MapReentrantEvents")
{
    MAKE_SERVER;

    SECTION("MapCritterOutEventRunsBeforeDetach")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterOutEventRunsBeforeDetach");
    }

    SECTION("MapCritterOutEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterOutEventMayDestroyCritter");
    }

    SECTION("MapCritterOutEventBlocksNestedTransfer")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterOutEventBlocksNestedTransfer");
    }

    SECTION("CritterDisappearedEventRunsBeforeDetach")
    {
        RUN_FUNC("MapOpsTest::TestCritterDisappearedEventRunsBeforeDetach");
    }

    SECTION("CritterDisappearedEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestCritterDisappearedEventMayDestroyCritter");
    }

    SECTION("MapCritterInEventMayDestroyEnteringCritter")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterInEventMayDestroyEnteringCritter");
    }

    SECTION("MapCritterInEventMayDestroyBeforeInitialInfo")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterInEventMayDestroyBeforeInitialInfo");
    }

    SECTION("MapAddCritterEventMayDestroyCritterThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterEventMayDestroyCritterThrows");
    }

    SECTION("MapAddCritterEventMayDestroyMapThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterEventMayDestroyMapThrows");
    }

    SECTION("MapAddCritterEventMayMoveCritterAwayThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterEventMayMoveCritterAwayThrows");
    }

    SECTION("MapAddCritterInitEventMayDestroyCritterThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterInitEventMayDestroyCritterThrows");
    }

    SECTION("MapAddCritterInitEventMayDestroyMapThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterInitEventMayDestroyMapThrows");
    }

    SECTION("MapAddCritterInitEventMayMoveCritterAwayThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterInitEventMayMoveCritterAwayThrows");
    }

    SECTION("GlobalMapCritterOutEventKeepsTrip")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapCritterOutEventKeepsTrip");
    }

    SECTION("GlobalMapCritterOutEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapCritterOutEventMayDestroyCritter");
    }

    SECTION("GlobalMapCritterInEventMayDestroyBeforeInitialInfo")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapCritterInEventMayDestroyBeforeInitialInfo");
    }

    SECTION("GlobalMapAddCritterEventMayDestroyCritterThrows")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapAddCritterEventMayDestroyCritterThrows");
    }

    SECTION("GlobalMapLoadCritterEventMayDestroyBeforeCritterLoad")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapLoadCritterEventMayDestroyBeforeCritterLoad");
    }

    SECTION("AttachedCritterTransferEventMayDestroyLeaderBeforeFinalTransfer")
    {
        RUN_FUNC("MapOpsTest::TestAttachedCritterTransferEventMayDestroyLeaderBeforeFinalTransfer");
    }

    SECTION("CritterAppearedEventMayDestroyTarget")
    {
        RUN_FUNC("MapOpsTest::TestCritterAppearedEventMayDestroyTarget");
    }

    SECTION("CritterAppearedEventStopsDistanceGroupsAfterTransfer")
    {
        RUN_FUNC("MapOpsTest::TestCritterAppearedEventStopsDistanceGroupsAfterTransfer");
    }

    SECTION("CritterAppearedDistEventStopsNextDistanceGroupsAfterTransfer")
    {
        RUN_FUNC("MapOpsTest::TestCritterAppearedDistEventStopsNextDistanceGroupsAfterTransfer");
    }

    SECTION("CritterDisappearedEventStopsDistanceGroupsAfterTransfer")
    {
        RUN_FUNC("MapOpsTest::TestCritterDisappearedEventStopsDistanceGroupsAfterTransfer");
    }

    SECTION("ItemOnMapAppearedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapAppearedEventMayDestroyItem");
    }

    SECTION("ItemOnMapAppearedEventMayMoveItemAway")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapAppearedEventMayMoveItemAway");
    }

    SECTION("MapAddItemEventMayDestroyItemThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemEventMayDestroyItemThrows");
    }

    SECTION("MapAddItemEventMayDestroyMapThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemEventMayDestroyMapThrows");
    }

    SECTION("MapAddItemEventMayMoveItemAwayThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemEventMayMoveItemAwayThrows");
    }

    SECTION("ItemOnMapDisappearedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapDisappearedEventMayDestroyItem");
    }

    SECTION("ItemOnMapChangedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapChangedEventMayDestroyItem");
    }

    SECTION("ItemOnMapChangedEventMayMoveItemAwayBeforeNextObserver")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapChangedEventMayMoveItemAwayBeforeNextObserver");
    }

    SECTION("CritterItemMovedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedEventMayDestroyItem");
    }

    SECTION("CritterItemMovedAddEventMayMoveItemAway")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedAddEventMayMoveItemAway");
    }

    SECTION("CritterItemMovedSwapEventMayMoveOriginalItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedSwapEventMayMoveOriginalItem");
    }

    SECTION("CritterItemMovedSwapEventMayDestroyOriginalItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedSwapEventMayDestroyOriginalItem");
    }

    SECTION("CritterWalkEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayDestroyCritter");
    }

    SECTION("CritterWalkEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayDestroyItem");
    }

    SECTION("CritterWalkEventMayTransferCritterBeforeNextTrigger")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayTransferCritterBeforeNextTrigger");
    }

    SECTION("CritterWalkEventMayDestroyMapBeforeNextTrigger")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayDestroyMapBeforeNextTrigger");
    }

    SECTION("MapFinishEventCannotDestroyOwningLocation")
    {
        RUN_FUNC("MapOpsTest::TestMapFinishEventCannotDestroyOwningLocation");
    }

    SECTION("MapRemovedEventCannotDestroyOwningLocation")
    {
        RUN_FUNC("MapOpsTest::TestMapRemovedEventCannotDestroyOwningLocation");
    }

    SECTION("MapInitEventMayDestroyMap")
    {
        RUN_FUNC("MapOpsTest::TestMapInitEventMayDestroyMap");
    }

    SECTION("MapAddedEventMayDestroyLocation")
    {
        RUN_FUNC("MapOpsTest::TestMapAddedEventMayDestroyLocation");
    }

    SECTION("MapAddedEventMayDestroyMap")
    {
        RUN_FUNC("MapOpsTest::TestMapAddedEventMayDestroyMap");
    }
}

TEST_CASE("ScriptArgValidation")
{
    MAKE_SERVER;

    SECTION("ItemsInRadiusNegativeRadius")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgItemsInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("CrittersInRadiusNegativeRadius")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgCrittersInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("IsHexesMovableNegativeRadius")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgIsHexesMovableNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("TransferToMapOutOfBoundsHex")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgTransferToMapOutOfBoundsHexThrows", "Invalid target hex arg");
    }

    SECTION("MoveToHexNegativeSpeed")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgMoveToHexNegativeSpeedThrows", "Speed arg out of range");
    }

    SECTION("ChangeMovingSpeedNegative")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgChangeMovingSpeedNegativeThrows", "Speed arg out of range");
    }

    SECTION("TraceHexLineOffsetOutOfRange")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgTraceHexLineOffsetOutOfRangeThrows", "Hex offset arg out of range");
    }

    SECTION("DestroyEntityPlayerCritter")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgDestroyEntityPlayerCritterThrows", "Cannot destroy a player-controlled critter");
    }

    SECTION("StringRawResizeNegative")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgStringRawResizeNegativeThrows", "String resize length must not be negative");
    }
}

FO_END_NAMESPACE
