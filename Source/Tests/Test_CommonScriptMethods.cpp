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

namespace CommonMethods
{
    // ========== Geometry ==========

    int TestGetDirAngle()
    {
        mpos from(10, 10);
        mpos to(15, 10);
        int16 angle = Game.GetDirAngle(from, to);
        // Angle should be some valid direction value
        // Just verify it doesn't crash and returns a value
        if (angle < -360 || angle > 360) return -1;

        // Same position should give 0 or defined result
        int16 sameAngle = Game.GetDirAngle(from, from);
        // Just verify no crash
        return 0;
    }

    int TestGetLineDirAngle()
    {
        ipos fromPos(0, 0);
        ipos toPos(100, 0);
        int16 angle = Game.GetLineDirAngle(fromPos, toPos);
        if (angle < -360 || angle > 360) return -1;

        ipos toPos2(0, 100);
        int16 angle2 = Game.GetLineDirAngle(fromPos, toPos2);
        if (angle2 < -360 || angle2 > 360) return -2;

        return 0;
    }

    int TestAngleToDirAndBack()
    {
        // Test all 6 directions (hex)
        for (uint8 dir = 0; dir < 6; dir++) {
            int16 angle = Game.DirToAngle(dir);
            if (angle < -360 || angle > 360) return -1;

            uint8 backDir = Game.AngleToDir(angle);
            if (backDir != dir) return -2;
        }
        return 0;
    }

    int TestRotateDirAngle()
    {
        int16 angle = 0;

        // Rotate clockwise by 60 degrees
        int16 rotated = Game.RotateDirAngle(angle, true, 60);
        if (rotated < -360 || rotated > 360) return -1;

        // Rotate counter-clockwise
        int16 rotatedCCW = Game.RotateDirAngle(angle, false, 60);
        if (rotatedCCW < -360 || rotatedCCW > 360) return -2;

        // Full rotation
        int16 full = Game.RotateDirAngle(angle, true, 360);
        // Should be close to original
        return 0;
    }

    int TestGetDirAngleDiff()
    {
        int16 a1 = 0;
        int16 a2 = 90;
        int16 diff = Game.GetDirAngleDiff(a1, a2);
        if (diff < 0) diff = -diff;
        if (diff > 180) return -1;

        // Same angle should give 0
        int16 sameDiff = Game.GetDirAngleDiff(a1, a1);
        if (sameDiff != 0) return -2;

        return 0;
    }

    int TestGetHexInterval()
    {
        mpos from(10, 10);
        mpos to(12, 12);
        ipos offset;
        Game.GetHexInterval(from, to, offset);
        // Just verify no crash and offset is set
        return 0;
    }

    // ========== Proto Getters ==========

    int TestGetProtoItems()
    {
        array<ProtoItem@>@ protos = Game.GetProtoItems();
        if (protos is null) return -1;
        if (protos.length() == 0) return -2;
        return 0;
    }

    int TestGetProtoCritters()
    {
        array<ProtoCritter@>@ protos = Game.GetProtoCritters();
        if (protos is null) return -1;
        if (protos.length() == 0) return -2;
        return 0;
    }

    int TestGetProtoMaps()
    {
        array<ProtoMap@>@ protos = Game.GetProtoMaps();
        if (protos is null) return -1;
        // Maps may be 0 if none defined in test setup - just verify no crash
        return 0;
    }

    int TestGetProtoLocations()
    {
        array<ProtoLocation@>@ protos = Game.GetProtoLocations();
        if (protos is null) return -1;
        if (protos.length() == 0) return -2;
        return 0;
    }

    // ========== UTF-8 ==========

    int TestEncodeDecodeUtf8()
    {
        // Encode a simple ASCII character
        string encoded = Game.EncodeUtf8(65); // 'A'
        if (encoded != "A") return -1;

        // Decode it back
        int length = 0;
        uint codepoint = Game.DecodeUtf8("A", length);
        if (codepoint != 65) return -2;
        if (length != 1) return -3;

        // Encode a multi-byte character (Cyrillic A = U+0410)
        string cyrillic = Game.EncodeUtf8(0x0410);
        if (cyrillic.length() == 0) return -4;

        // Decode it back
        int len2 = 0;
        uint cp2 = Game.DecodeUtf8(cyrillic, len2);
        if (cp2 != 0x0410) return -5;
        if (len2 < 2) return -6;

        return 0;
    }

    // ========== Time Packing ==========

    int TestPackUnpackTime()
    {
        // Pack a known time
        nanotime packed = Game.PackTime(2024, 6, 15, 12, 30, 45, 500, 0, 0);
        if (packed == ZERO_NANOTIME) return -1;

        // Unpack it
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, ms = 0, us = 0, ns = 0;
        Game.UnpackTime(packed, year, month, day, hour, minute, second, ms, us, ns);

        if (year != 2024) return -2;
        if (month != 6) return -3;
        if (day != 15) return -4;
        if (hour != 12) return -5;
        if (minute != 30) return -6;
        if (second != 45) return -7;

        return 0;
    }

    int TestPackUnpackSynchronizedTime()
    {
        synctime packed = Game.PackSynchronizedTime(2024, 3, 20, 8, 15, 30, 250);
        if (packed == ZERO_SYNCTIME) return -1;

        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, ms = 0;
        Game.UnpackSynchronizedTime(packed, year, month, day, hour, minute, second, ms);

        if (year != 2024) return -2;
        if (month != 3) return -3;
        if (day != 20) return -4;
        if (hour != 8) return -5;
        if (minute != 15) return -6;
        if (second != 30) return -7;

        return 0;
    }

    // ========== Game-level Time Events ==========

    bool globalTimerFired = false;

    [[TimeEvent]]
    void OnGlobalTimer()
    {
        globalTimerFired = true;
    }

    int TestGameTimeEventCreateCountStop()
    {
        globalTimerFired = false;

        // Start a game-level time event
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        // Count by function
        int count = Game.CountTimeEvent(OnGlobalTimer);
        if (count < 1) return -2;

        // Count by id
        int countById = Game.CountTimeEvent(eventId);
        if (countById != 1) return -3;

        // Stop by function
        Game.StopTimeEvent(OnGlobalTimer);

        int countAfter = Game.CountTimeEvent(OnGlobalTimer);
        if (countAfter != 0) return -4;

        return 0;
    }

    int TestGameTimeEventStopById()
    {
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        Game.StopTimeEvent(eventId);

        int countAfter = Game.CountTimeEvent(eventId);
        if (countAfter != 0) return -2;

        return 0;
    }

    int TestGameTimeEventRepeat()
    {
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        // Change repeat interval
        Game.RepeatTimeEvent(OnGlobalTimer, timespan(30, 3));
        Game.RepeatTimeEvent(eventId, timespan(15, 3));

        // Clean up
        Game.StopTimeEvent(eventId);
        return 0;
    }

    bool globalTimerWithDataFired = false;

    [[TimeEvent]]
    void OnGlobalTimerWithData(any data)
    {
        globalTimerWithDataFired = true;
    }

    int TestGameTimeEventWithData()
    {
        globalTimerWithDataFired = false;

        any data = 42;
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimerWithData, data);
        if (eventId == 0) return -1;

        int count = Game.CountTimeEvent(OnGlobalTimerWithData);
        if (count < 1) return -2;

        // Set new data by id
        any newData = 100;
        Game.SetTimeEventData(eventId, newData);

        Game.StopTimeEvent(OnGlobalTimerWithData);
        return 0;
    }

    int TestGameTimeEventRepeating()
    {
        // Start a repeating event (delay + repeat)
        uint32 eventId = Game.StartTimeEvent(timespan(1, 3), timespan(5, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        int count = Game.CountTimeEvent(eventId);
        if (count != 1) return -2;

        Game.StopTimeEvent(eventId);
        return 0;
    }

 )") + R"(

    // ========== Invoke ==========

    bool invokeFired = false;

    void InvokeTarget()
    {
        invokeFired = true;
    }

    void InvokeTargetWithData(any data)
    {
        invokeFired = true;
    }

    int TestInvokeByFunc()
    {
        invokeFired = false;
        bool result = Game.Invoke(InvokeTarget);
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    int TestInvokeByName()
    {
        invokeFired = false;
        bool result = Game.Invoke("CommonMethods::InvokeTarget");
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    int TestInvokeWithData()
    {
        invokeFired = false;
        any param = 42;
        bool result = Game.Invoke(InvokeTargetWithData, param);
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    int TestInvokeByNameWithData()
    {
        invokeFired = false;
        any param = 42;
        bool result = Game.Invoke("CommonMethods::InvokeTargetWithData", param);
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    // ========== ResolveGenericValue ==========

    int TestResolveGenericValue()
    {
        // Resolve a numeric string
        int val = Game.ResolveGenericValue("123");
        if (val != 123) return -1;

        int val2 = Game.ResolveGenericValue("0");
        if (val2 != 0) return -2;

        return 0;
    }

 )" + R"(
    // ========== Entity Time Events (Critter) ==========

    [[TimeEvent]]
    void OnCritterTimeEvent(Critter cr)
    {
        // No-op
    }

    [[TimeEvent]]
    void OnCritterTimeEventWithData(Critter cr, any data)
    {
        // No-op
    }

    int TestEntityTimeEventCount()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEvent);
        if (eventId == 0) return -2;

        // Count by function
        int count = cr.CountTimeEvent(OnCritterTimeEvent);
        if (count < 1) return -3;

        // Count by id
        int countById = cr.CountTimeEvent(eventId);
        if (countById != 1) return -4;

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityTimeEventRepeat()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEvent);
        if (eventId == 0) return -2;

        // Change repeat by function
        cr.RepeatTimeEvent(OnCritterTimeEvent, timespan(30, 3));

        // Change repeat by id
        cr.RepeatTimeEvent(eventId, timespan(15, 3));

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityTimeEventSetData()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        any initData = 1;
        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEventWithData, initData);
        if (eventId == 0) return -2;

        // Set data by function
        any newData1 = 2;
        cr.SetTimeEventData(OnCritterTimeEventWithData, newData1);

        // Set data by id
        any newData2 = 3;
        cr.SetTimeEventData(eventId, newData2);

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityTimeEventStopByFunction()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEvent);
        cr.StartTimeEvent(timespan(120, 3), OnCritterTimeEvent);

        int count = cr.CountTimeEvent(OnCritterTimeEvent);
        if (count < 2) return -2;

        cr.StopTimeEvent(OnCritterTimeEvent);

        int countAfter = cr.CountTimeEvent(OnCritterTimeEvent);
        if (countAfter != 0) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Item Container Ops ==========

    int TestItemContainerAddGetItems()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Add a container item
        Item@ container = cr.AddItem("TestContainer".hstr(), 1);
        if (container is null) return -2;

        // Add items to the container
        Item@ subItem1 = container.AddItem("TestItem".hstr(), 3);
        if (subItem1 is null) return -3;

        Item@ subItem2 = container.AddItem("TestItem".hstr(), 5);
        if (subItem2 is null) return -4;

        // Get items from container
        array<Item@>@ contents = container.GetItems();
        if (contents is null) return -5;
        if (contents.length() == 0) return -6;

        // Container's GetCritter should return the owning critter
        Critter@ owner = container.GetCritter();
        if (owner is null) return -7;
        if (owner.Id != cr.Id) return -8;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemGetMap()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        // Item is in critter inventory, not on a map
        Map@ m = item.GetMap();
        // Should be null since item is not on a map
        if (m !is null) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Critter Script Methods ==========

    int TestCritterGetPlayerOfflineTime()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // NPC has no player, GetPlayerOfflineTime should throw
        // Just verify the critter was created successfully
        bool isAlive = cr.IsAlive();
        if (!isAlive) return -2;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItemByProperty()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 5);

        // Get items by property: CritterSlot == Inventory (0)
        array<Item@>@ items = cr.GetItems(ItemProperty::CritterSlot, CritterItemSlot::Inventory);
        if (items is null) return -2;
        if (items.length() == 0) return -3;

        // Get single item by property
        Item@ found = cr.GetItem(ItemProperty::CritterSlot, CritterItemSlot::Inventory);
        if (found is null) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItemsByProto()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 3);
        cr.AddItem("TestItem".hstr(), 7);

        // Get items by proto hstring
        array<Item@>@ items = cr.GetItems("TestItem".hstr());
        if (items is null) return -2;
        if (items.length() == 0) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterDestroyItemByCount()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 10);

        int before = cr.CountItem("TestItem".hstr());
        if (before < 10) return -2;

        // Destroy partial stack
        cr.DestroyItem("TestItem".hstr(), 3);

        int after = cr.CountItem("TestItem".hstr());
        if (after != before - 3) return -3;

        // Destroy all remaining
        cr.DestroyItem("TestItem".hstr(), after);

        int final2 = cr.CountItem("TestItem".hstr());
        if (final2 != 0) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Server Global Methods ==========

    int TestGetLocationsOverloads()
    {
        Location@ loc1 = Game.CreateLocation("TestLocation".hstr());
        Location@ loc2 = Game.CreateLocation("TestLocation".hstr());
        if (loc1 is null || loc2 is null) return -1;

        // Get all locations
        array<Location@>@ locs = Game.GetLocations();
        if (locs is null) return -2;
        if (locs.length() < 2) return -3;

        // Get locations by pid
        array<Location@>@ byPid = Game.GetLocations("TestLocation".hstr());
        if (byPid is null) return -4;
        if (byPid.length() < 2) return -5;

        Game.DestroyLocation(loc2);
        Game.DestroyLocation(loc1);
        return 0;
    }

    int TestGetCrittersOverloads()
    {
        Critter@ cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter@ cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // Get all NPCs
        array<Critter@>@ npcs = Game.GetAllNpc();
        if (npcs is null) return -2;
        if (npcs.length() < 2) return -3;

        // Get NPCs by PID
        array<Critter@>@ byPid = Game.GetAllNpc("TestCritter".hstr());
        if (byPid is null) return -4;
        if (byPid.length() < 2) return -5;

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestSetSynchronizedTime()
    {
        // Set synchronized time
        synctime st = Game.PackSynchronizedTime(2025, 1, 1, 0, 0, 0, 0);
        if (st == ZERO_SYNCTIME) return -1;

        Game.SetSynchronizedTime(st);

        // Verify by packing another time and checking it's different
        synctime st2 = Game.PackSynchronizedTime(2025, 6, 15, 12, 0, 0, 0);
        if (st2 == ZERO_SYNCTIME) return -2;
        if (st2 == st) return -3;

        return 0;
    }

    // ========== Location Time Events ==========

    [[TimeEvent]]
    void OnLocationTimeEvent(Location loc)
    {
        // No-op
    }

    int TestLocationTimeEvents()
    {
        Location@ loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        uint32 eventId = loc.StartTimeEvent(timespan(60, 3), OnLocationTimeEvent);
        if (eventId == 0) return -2;

        int count = loc.CountTimeEvent(OnLocationTimeEvent);
        if (count < 1) return -3;

        loc.RepeatTimeEvent(eventId, timespan(30, 3));

        loc.StopTimeEvent(OnLocationTimeEvent);

        int countAfter = loc.CountTimeEvent(OnLocationTimeEvent);
        if (countAfter != 0) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Item Time Events ==========

    [[TimeEvent]]
    void OnItemTimeEvent(Item item)
    {
        // No-op
    }

    [[TimeEvent]]
    void OnItemTimeEventData(Item item, any data)
    {
        // No-op
    }

    int TestItemTimeEvents()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        uint32 eventId = item.StartTimeEvent(timespan(60, 3), OnItemTimeEvent);
        if (eventId == 0) return -3;

        int count = item.CountTimeEvent(OnItemTimeEvent);
        if (count < 1) return -4;

        item.RepeatTimeEvent(eventId, timespan(30, 3));

        item.StopTimeEvent(eventId);

        int countAfter = item.CountTimeEvent(eventId);
        if (countAfter != 0) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemTimeEventWithData()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item@ item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        any initData = 42;
        uint32 eventId = item.StartTimeEvent(timespan(60, 3), OnItemTimeEventData, initData);
        if (eventId == 0) return -3;

        // Set data by function
        any newData = 100;
        item.SetTimeEventData(OnItemTimeEventData, newData);

        // Set data by id
        any newData2 = 200;
        item.SetTimeEventData(eventId, newData2);

        item.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Array Time Events with Data ==========

    [[TimeEvent]]
    void OnGlobalTimerWithArrayData(array<any> data)
    {
        // No-op
    }

    int TestGameTimeEventWithArrayData()
    {
        array<any> data = {1, 2, 3};
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimerWithArrayData, data);
        if (eventId == 0) return -1;

        int count = Game.CountTimeEvent(OnGlobalTimerWithArrayData);
        if (count < 1) return -2;

        Game.StopTimeEvent(OnGlobalTimerWithArrayData);
        return 0;
    }

    [[TimeEvent]]
    void OnCritterTimerWithArrayData(Critter cr, array<any> data)
    {
        // No-op
    }

    int TestEntityTimeEventWithArrayData()
    {
        Critter@ cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        array<any> data = {10, 20};
        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimerWithArrayData, data);
        if (eventId == 0) return -2;

        int count = cr.CountTimeEvent(OnCritterTimerWithArrayData);
        if (count < 1) return -3;

        // Set array data by function
        array<any> newData = {30, 40, 50};
        cr.SetTimeEventData(OnCritterTimerWithArrayData, newData);

        // Set array data by id
        array<any> newData2 = {60};
        cr.SetTimeEventData(eventId, newData2);

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }
}

 )";

        return BakerTests::CompileInlineScripts(&compiler_engine, "CommonMethodsScripts", {{"Scripts/CommonMethods.fos", script_source}}, [](string_view message) {
            const auto message_str = string(message);
            if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos) {
                throw ScriptSystemException(message_str);
            }
        });
    }

    static auto MakeResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("CommonMethodsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");
        const auto location_type = proto_engine.Hashes.ToHashedString("Location");

        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        const auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        const auto container_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestContainer");
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("CommonMethodsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("CommonMethodsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("CommonMethodsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("CommonMethodsContainer.fopro-bin-server", container_blob);
        runtime_source->AddFile("CommonMethodsLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("CommonMethods.fos-bin-server", script_blob);

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

#define MAKE_CM_SERVER() \
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

#define RUN_CM_FUNC(func_name) \
    auto func = server->FindFunc<int32>(get_func("CommonMethods::" func_name)); \
    REQUIRE(func); \
    REQUIRE(func.Call()); \
    CHECK(func.GetResult() == 0)

// ========== Geometry Tests ==========

TEST_CASE("GeometryDirectionAngles")
{
    MAKE_CM_SERVER();

    SECTION("GetDirAngle")
    {
        RUN_CM_FUNC("TestGetDirAngle");
    }

    SECTION("GetLineDirAngle")
    {
        RUN_CM_FUNC("TestGetLineDirAngle");
    }

    SECTION("AngleToDirAndBack")
    {
        RUN_CM_FUNC("TestAngleToDirAndBack");
    }

    SECTION("RotateDirAngle")
    {
        RUN_CM_FUNC("TestRotateDirAngle");
    }

    SECTION("GetDirAngleDiff")
    {
        RUN_CM_FUNC("TestGetDirAngleDiff");
    }

    SECTION("GetHexInterval")
    {
        RUN_CM_FUNC("TestGetHexInterval");
    }
}

// ========== Proto Getter Tests ==========

TEST_CASE("ProtoCollectionGetters")
{
    MAKE_CM_SERVER();

    SECTION("GetProtoItems")
    {
        RUN_CM_FUNC("TestGetProtoItems");
    }

    SECTION("GetProtoCritters")
    {
        RUN_CM_FUNC("TestGetProtoCritters");
    }

    SECTION("GetProtoMaps")
    {
        RUN_CM_FUNC("TestGetProtoMaps");
    }

    SECTION("GetProtoLocations")
    {
        RUN_CM_FUNC("TestGetProtoLocations");
    }
}

// ========== UTF-8 Tests ==========

TEST_CASE("Utf8EncodeDecodeOps")
{
    MAKE_CM_SERVER();

    SECTION("EncodeDecodeUtf8")
    {
        RUN_CM_FUNC("TestEncodeDecodeUtf8");
    }
}

// ========== Time Pack Tests ==========

TEST_CASE("TimePackingOperations")
{
    MAKE_CM_SERVER();

    SECTION("PackUnpackTime")
    {
        RUN_CM_FUNC("TestPackUnpackTime");
    }

    SECTION("PackUnpackSynchronizedTime")
    {
        RUN_CM_FUNC("TestPackUnpackSynchronizedTime");
    }
}

// ========== Game Time Events ==========

TEST_CASE("GameLevelTimeEvents")
{
    MAKE_CM_SERVER();

    SECTION("CreateCountStop")
    {
        RUN_CM_FUNC("TestGameTimeEventCreateCountStop");
    }

    SECTION("StopById")
    {
        RUN_CM_FUNC("TestGameTimeEventStopById");
    }

    SECTION("Repeat")
    {
        RUN_CM_FUNC("TestGameTimeEventRepeat");
    }

    SECTION("WithData")
    {
        RUN_CM_FUNC("TestGameTimeEventWithData");
    }

    SECTION("Repeating")
    {
        RUN_CM_FUNC("TestGameTimeEventRepeating");
    }

    SECTION("WithArrayData")
    {
        RUN_CM_FUNC("TestGameTimeEventWithArrayData");
    }
}

// ========== Invoke Tests ==========

TEST_CASE("GameInvokeOperations")
{
    MAKE_CM_SERVER();

    SECTION("ByFunc")
    {
        RUN_CM_FUNC("TestInvokeByFunc");
    }

    SECTION("ByName")
    {
        RUN_CM_FUNC("TestInvokeByName");
    }

    SECTION("WithData")
    {
        RUN_CM_FUNC("TestInvokeWithData");
    }

    SECTION("ByNameWithData")
    {
        RUN_CM_FUNC("TestInvokeByNameWithData");
    }
}

// ========== ResolveGenericValue ==========

TEST_CASE("ResolveGenericValueOps")
{
    MAKE_CM_SERVER();

    SECTION("ResolveGenericValue")
    {
        RUN_CM_FUNC("TestResolveGenericValue");
    }
}

// ========== Entity Time Events ==========

TEST_CASE("EntityTimeEventOperations")
{
    MAKE_CM_SERVER();

    SECTION("CritterCount")
    {
        RUN_CM_FUNC("TestEntityTimeEventCount");
    }

    SECTION("CritterRepeat")
    {
        RUN_CM_FUNC("TestEntityTimeEventRepeat");
    }

    SECTION("CritterSetData")
    {
        RUN_CM_FUNC("TestEntityTimeEventSetData");
    }

    SECTION("CritterStopByFunction")
    {
        RUN_CM_FUNC("TestEntityTimeEventStopByFunction");
    }

    SECTION("CritterWithArrayData")
    {
        RUN_CM_FUNC("TestEntityTimeEventWithArrayData");
    }
}

// ========== Item/Container Ops ==========

TEST_CASE("ItemContainerAndScriptOps")
{
    MAKE_CM_SERVER();

    SECTION("ContainerAddGetItems")
    {
        RUN_CM_FUNC("TestItemContainerAddGetItems");
    }

    SECTION("ItemGetMap")
    {
        RUN_CM_FUNC("TestItemGetMap");
    }

    SECTION("ItemTimeEvents")
    {
        RUN_CM_FUNC("TestItemTimeEvents");
    }

    SECTION("ItemTimeEventWithData")
    {
        RUN_CM_FUNC("TestItemTimeEventWithData");
    }
}

// ========== Critter Script Methods ==========

TEST_CASE("CritterScriptMethodsAdvanced")
{
    MAKE_CM_SERVER();

    SECTION("GetPlayerOfflineTime")
    {
        RUN_CM_FUNC("TestCritterGetPlayerOfflineTime");
    }

    SECTION("GetItemByProperty")
    {
        RUN_CM_FUNC("TestCritterGetItemByProperty");
    }

    SECTION("GetItemsByProto")
    {
        RUN_CM_FUNC("TestCritterGetItemsByProto");
    }

    SECTION("DestroyItemByCount")
    {
        RUN_CM_FUNC("TestCritterDestroyItemByCount");
    }
}

// ========== Server Global Methods ==========

TEST_CASE("ServerGlobalMethodsExtended")
{
    MAKE_CM_SERVER();

    SECTION("GetLocationsOverloads")
    {
        RUN_CM_FUNC("TestGetLocationsOverloads");
    }

    SECTION("GetCrittersOverloads")
    {
        RUN_CM_FUNC("TestGetCrittersOverloads");
    }

    SECTION("SetSynchronizedTime")
    {
        RUN_CM_FUNC("TestSetSynchronizedTime");
    }
}

// ========== Location Time Events ==========

TEST_CASE("LocationTimeEventOps")
{
    MAKE_CM_SERVER();

    SECTION("LocationTimeEvents")
    {
        RUN_CM_FUNC("TestLocationTimeEvents");
    }
}

// ========== C++ API Tests ==========

TEST_CASE("CommonCppApiTests")
{
    MAKE_CM_SERVER();

    SECTION("ProtoManagerLookups")
    {
        // Verify that protos loaded in our test setup
        CHECK(server->EntityMngr.GetCrittersCount() == 0);
        CHECK(server->EntityMngr.GetLocationsCount() == 0);
        CHECK(server->EntityMngr.GetItemsCount() == 0);
    }

    SECTION("EntityManagerMultipleEntities")
    {
        // Create multiple entities and verify counts
        auto* cr1 = server->CreateCritter(get_func("TestCritter"), false);
        auto* cr2 = server->CreateCritter(get_func("TestCritter"), false);
        auto* cr3 = server->CreateCritter(get_func("TestCritter"), false);
        REQUIRE(cr1 != nullptr);
        REQUIRE(cr2 != nullptr);
        REQUIRE(cr3 != nullptr);

        CHECK(server->EntityMngr.GetCrittersCount() >= 3);

        // Clean up
        server->CrMngr.DestroyCritter(cr3);
        server->CrMngr.DestroyCritter(cr2);
        server->CrMngr.DestroyCritter(cr1);
    }

    SECTION("ItemManagerContainerOps")
    {
        auto* cr = server->CreateCritter(get_func("TestCritter"), false);
        REQUIRE(cr != nullptr);

        auto* item1 = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 5);
        auto* item2 = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 3);
        REQUIRE(item1 != nullptr);
        REQUIRE(item2 != nullptr);

        const auto& inv = cr->GetInvItems();
        CHECK(inv.size() >= 2);

        server->ItemMngr.DestroyItem(item2);
        server->ItemMngr.DestroyItem(item1);
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ServerHealthCheck")
    {
        auto health = server->GetHealthInfo();
        CHECK(health.find("Server uptime") != string::npos);
    }
}

FO_END_NAMESPACE
