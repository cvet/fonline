namespace EngineTests;

using System;
using System.Collections.Generic;
using FOnline;

public static class ManagedRuntimeProbe
{
    private static int InitCalls;
#if SERVER
    private static int CritterInitCalls;
    private static long LastCritterId;
#endif

    [ModuleInit]
    internal static void Init()
    {
        InitCalls++;
#if SERVER
        Game.OnCritterInit.Subscribe(OnCritterInit);
#endif
    }

#if SERVER
    [Event]
    internal static void OnCritterInit(Critter critter, bool firstTime)
    {
        CritterInitCalls++;
        LastCritterId = critter.Id.value;
    }

    [CallableFromNative]
    internal static int TestConditionalDatabaseRecords()
    {
        hstring collection = "test_collection".hstr();
        ident id = new ident(1234511);
        Dictionary<string, string> first = new Dictionary<string, string> { { "Name", "First" } };
        Dictionary<string, string> second = new Dictionary<string, string> { { "Name", "Second" } };

        if (!Game.DbTryInsertRecord(collection, id, first)) {
            return -1;
        }
        if (Game.DbTryInsertRecord(collection, id, second)) {
            return -2;
        }
        if (Game.DbGetRecord(collection, id)["Name"] != "First") {
            return -3;
        }
        if (!Game.DbCompareExchangeRecordString(collection, id, "Name", "First", "Advanced")) {
            return -4;
        }
        if (Game.DbCompareExchangeRecordString(collection, id, "Name", "First", "Wrong")) {
            return -5;
        }
        if (Game.DbCompareExchangeRecordString(collection, id, "Missing", "", "Wrong")) {
            return -6;
        }

        collection = "test_strings".hstr();
        string key = "conditional-key";

        if (!Game.DbTryInsertRecord(collection, key, first)) {
            return -7;
        }
        if (Game.DbTryInsertRecord(collection, key, second)) {
            return -8;
        }
        if (!Game.DbCompareExchangeRecordString(collection, key, "Name", "First", "Advanced")) {
            return -9;
        }
        if (Game.DbCompareExchangeRecordString(collection, key, "Name", "First", "Wrong")) {
            return -10;
        }
        if (Game.DbGetRecord(collection, key)["Name"] != "Advanced") {
            return -11;
        }

        return 0;
    }

    [CallableFromNative]
    internal static int GetCritterInitCalls() => CritterInitCalls;

    [CallableFromNative]
    internal static long GetLastCritterId() => LastCritterId;

    [CallableFromNative]
    internal static long GetCritterId(Critter critter) => critter.Id.value;

    [CallableFromNative]
    internal static bool IsItemNull(Item? item) => item == null;

    [CallableFromNative]
    internal static void UpdateString(Critter first, Critter second, ref string value)
    {
        value = first.Id == second.Id ? "same:" + value : "different:" + value;
    }
#endif

    [CallableFromNative]
    internal static int GetInitCalls() => InitCalls;

    [CallableFromNative]
    internal static int SumArray(List<int> values)
    {
        int sum = 0;

        foreach (int value in values) {
            sum += value;
        }

        return sum;
    }

    [CallableFromNative]
    internal static List<int> TransformArray(List<int> values)
    {
        values[0] += 10;
        values.Add(77);
        return values;
    }

    [CallableFromNative]
    internal static int SumDictionary(Dictionary<int, int> values)
    {
        int sum = 0;

        foreach (KeyValuePair<int, int> value in values) {
            sum += value.Key + value.Value;
        }

        return sum;
    }

    [CallableFromNative]
    internal static Dictionary<int, int> TransformDictionary(Dictionary<int, int> values)
    {
        values[1] += 5;
        values[3] = 30;
        return values;
    }

    [CallableFromNative]
    internal static string EchoString(string value) => value;

    [CallableFromNative]
    internal static hstring EchoHash(hstring value) => value;

    [CallableFromNative]
    internal static void ThrowFromScript() => throw new InvalidOperationException("Managed runtime probe failure");
}
