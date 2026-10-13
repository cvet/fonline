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

#if CLIENT
    [CallableFromNative]
    internal static int CheckTextLayoutBatch()
    {
        Game.BindFont(FontType.Default, "UnitTestFont.fofnt");
        List<string> texts = new List<string> { "AB", "A\nB", "" };
        List<ucolor> colors = new List<ucolor> { new ucolor(30, 40, 50, 120), default, default };
        List<TextLayoutResult> results = Game.PrepareTextLayouts(texts, new isize(200, 500), default, colors);

        if (results.Count != 3 || results[0].Width != 10 || results[0].Height != 8 || results[0].Lines != 1 ||
            results[1].Width != 5 || results[1].Height != 17 || results[1].Lines != 2 ||
            results[2].Width != 0 || results[2].Height != 0 || results[2].Lines != 0 ||
            Game.CanPrepareTextLayoutsInParallel()) {
            return -1;
        }

        bool rejected = false;

        try {
            Game.PrepareTextLayouts(texts, new isize(200, 500), default, new List<ucolor>());
        }
        catch (Exception ex) {
            rejected = ex.Message.Contains("Text layout batch requires one color per text", StringComparison.Ordinal);
        }

        if (!rejected) {
            return -2;
        }

        Game.BindFont(FontType.Default, "UnitTestFont.fofnt", 0.5f);
        results = Game.PrepareTextLayouts(texts, new isize(200, 500), default, colors);
        return results[0].Width == 6 && results[0].Height == 4 && results[0].Lines == 1 ? 0 : -3;
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

