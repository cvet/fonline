using System;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using System.Threading.Tasks;
using FOnline;

internal static class Program
{
    private static async Task<int> Main(string[] args)
    {
        if (args.Length != 0)
        {
            return BootstrapScenarios.Run(args[0]);
        }
        var cases = new (string Name, Action Run)[]
        {
            ("bootstrap outside a source tree", () => BootstrapScenarios.RunIsolated("outside-source-tree")),
            ("bootstrap alongside source files", () => BootstrapScenarios.RunIsolated("source-tree")),
            ("static initialization failure stops startup", () => BootstrapScenarios.RunIsolated("static-failure")),
            ("ref result numeric widening", () => {
                long result = 1;
                Check(Game.Invoke("DispatchProbe::WriteInt", ref result), "Invocation failed");
                Check(result == 42, "Converted result was lost");
            }),
            ("ref conversion failure is accounted", () => {
                byte result = 1;
                int before = Game.GetGlobalExceptionCount();
                Check(!Game.Invoke("DispatchProbe::WriteLargeInt", ref result), "Overflow must fail");
                Check(result == 1, "Failed conversion changed caller result");
                Check(Game.GetGlobalExceptionCount() == before + 1, "Overflow was not recorded once");
            }),
            ("qualified enum argument", () => {
                Check(Game.Invoke("DispatchProbe::TakeEnum", "CritterProperty::strength"), "Enum argument failed");
                Check(ExampleGame.DispatchProbe.EnumValue == CritterProperty.Strength, "Wrong enum value");
            }),
            ("qualified module", () => Check(Game.Invoke("ExampleGame.DispatchProbe::NoArgs"), "Qualified module was not found")),
            ("overload candidates retain argument matching", () => {
                Check(Game.Invoke("DispatchProbe::Overload", new ExampleGame.First()), "First overload failed");
                Check(ExampleGame.DispatchProbe.OverloadValue == 1, "Wrong first overload");
                Check(Game.Invoke("DispatchProbe::Overload", new ExampleGame.Second()), "Second overload failed");
                Check(ExampleGame.DispatchProbe.OverloadValue == 2, "Cache reused an incompatible overload");
            }),
            ("warm dispatch avoids reflection rescans", () => {
                for (int i = 0; i < 100; i++) { Check(Game.Invoke("DispatchProbe::NoArgs"), "Warmup failed"); }
                long before = GC.GetAllocatedBytesForCurrentThread();
                for (int i = 0; i < 1000; i++) { Check(Game.Invoke("DispatchProbe::NoArgs"), "Warm invocation failed"); }
                Check(GC.GetAllocatedBytesForCurrentThread() - before < 128_000, "Warm dispatch repeatedly allocates reflection inventories");
            }),
            ("registered dictionary signatures", () => {
                Check(ScriptFuncRegistration.EngineTypeName(typeof(Dictionary<string, int>)) == "string=>int32", "Wrong dictionary signature");
                Check(ScriptFuncRegistration.EngineTypeName(typeof(Dictionary<string, List<int>>)) == "string=>int32[]", "Wrong dictionary-of-array signature");
                try { ScriptFuncRegistration.EngineTypeName(typeof(HashSet<int>)); }
                catch (NotSupportedException) { return; }
                throw new Exception("Unsupported generic signature was accepted");
            }),
            ("native fallback remains available", () => {
                int before = Native.FallbackCalls;
                Check(!Game.Invoke("Missing::Function"), "Missing method succeeded");
                Check(!Game.Invoke("Missing::Function"), "Cached missing method succeeded");
                Check(Native.FallbackCalls == before + 2, "Cached miss bypassed native fallback");
            }),
            ("exact enum namespace wins", () => {
                Check(Game.ParseGenericEnum("FirstEnums::Shared", "Value") == 1, "First exact name was shadowed");
                Check(Game.ParseGenericEnum("SecondEnums::Shared", "Value") == 2, "Second exact name was shadowed");
            }),
            ("foreign assembly enums are isolated", () => {
                var assembly = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName("ForeignEngineEnums"), AssemblyBuilderAccess.Run);
                var type = assembly.DefineDynamicModule("Enums").DefineEnum("ForeignEnum", TypeAttributes.Public, typeof(int));
                type.DefineLiteral("Value", 1);
                type.CreateTypeInfo();
                try { Game.ParseGenericEnum("ForeignEnum", "Value"); }
                catch (InvalidOperationException) { return; }
                throw new Exception("An enum from a foreign assembly was accepted");
            }),
            ("duration formatting across signs and extremes", () => {
                var samples = new (long Value, string Text)[] {
                    (0, "0.000 us"), (-1, "-0.001 us"), (1500, "1.500 us"), (-1500, "-1.500 us"),
                    (-250000000, "-250.000 ms"), (-1000000000, "-1.000 sec"),
                    (-90000000000, "-00:01:30 sec"), (-86400000000000, "-1 day 00:00:00 sec"),
                    (-172800000000000, "-2 days 00:00:00 sec"),
                    (long.MinValue, "-106751 days 23:47:16 sec"), (long.MaxValue, "106751 days 23:47:16 sec")
                };
                foreach (var sample in samples) {
                    Check(new timespan(sample.Value).ToString() == sample.Text, "Wrong format for " + sample.Value);
                }
            }),
        };
        int failures = 0;

        foreach (var test in cases) {
            try { test.Run(); Console.WriteLine("PASS " + test.Name); }
            catch (Exception ex) { failures++; Console.WriteLine("FAIL " + test.Name + ": " + ex.Message); }
        }

        try {
            Check(await Game.InvokeAsync("DispatchProbe::AsyncCall"), "Async invocation failed");
            Check(ExampleGame.DispatchProbe.AsyncFinished, "Async invocation returned before completion");
            Console.WriteLine("PASS awaited invocation");
        }
        catch (Exception ex) { failures++; Console.WriteLine("FAIL awaited invocation: " + ex.Message); }

        Console.WriteLine($"{cases.Length + 1 - failures}/{cases.Length + 1} passed");
        return failures == 0 ? 0 : 1;
    }

    private static void Check(bool condition, string message)
    {
        if (!condition) { throw new Exception(message); }
    }
}

namespace ExampleGame
{
    public sealed class First { }
    public sealed class Second { }
    public static class DispatchProbe
    {
        public static CritterProperty EnumValue;
        public static int OverloadValue;
        public static bool AsyncFinished;
        public static void WriteInt(ref int value) { value = 42; }
        public static void WriteLargeInt(ref int value) { value = 300; }
        public static void TakeEnum(CritterProperty value) { EnumValue = value; }
        public static void NoArgs() { }
        public static void Overload(First value) { OverloadValue = 1; }
        public static void Overload(Second value) { OverloadValue = 2; }
        public static async Task AsyncCall() { await Task.Yield(); AsyncFinished = true; }
    }
}

namespace FirstEnums { public enum Shared { Value = 1 } }
namespace SecondEnums { public enum Shared { Value = 2 } }

