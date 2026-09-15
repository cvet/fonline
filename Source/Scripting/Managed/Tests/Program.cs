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
        if (args.Length != 0) {
            return BootstrapScenarios.Run(args[0]);
        }
        var cases = new(string Name, Action Run)[] {
            ("bootstrap outside a source tree", () => BootstrapScenarios.RunIsolated("outside-source-tree")),
            ("bootstrap alongside source files", () => BootstrapScenarios.RunIsolated("source-tree")),
            ("static initialization failure stops startup", () => BootstrapScenarios.RunIsolated("static-failure")),
            ("direction constructors normalize both map geometries",
             () =>
             {
                 var samples = new(int Value, int Hex, int Square)[] { (0, 0, 0),
                                                                       (6, 0, 6),
                                                                       (7, 1, 7),
                                                                       (8, 2, 0),
                                                                       (-1, 5, 7),
                                                                       (-7, 5, 1),
                                                                       (int.MinValue, 4, 0),
                                                                       (int.MaxValue, 1, 7) };
                 int previous = Settings.Geometry.MapDirCount;
                 try {
                     foreach (int count in new[] { 6, 8 }) {
                         Settings.Geometry.MapDirCount = count;
                         foreach (var sample in samples) {
                             Check(new hdir(sample.Value).value == (count == 6 ? sample.Hex : sample.Square),
                                   "Wrong normalized direction for " + sample.Value);
                         }
                         sbyte signedDirection = -1;
                         byte unsignedDirection = 255;
                         Check(new hdir(signedDirection).value == count - 1,
                               "Signed narrow direction bypassed normalization");
                         Check(new hdir(unsignedDirection).value == (count == 6 ? 3 : 7),
                               "Unsigned direction narrowed before normalization");
                     }
                 }
                 finally {
                     Settings.Geometry.MapDirCount = previous;
                 }
                 Check(System.Runtime.InteropServices.Marshal.SizeOf<hdir>() == 1, "Direction ABI size changed");
                 Check(typeof(hdir).GetConstructor(new[] { typeof(sbyte) })?.GetParameters()[0].ParameterType ==
                           typeof(sbyte),
                       "Existing narrow direction constructor entrypoint was lost");
             }),
            ("angle constructors normalize narrow and full-width inputs",
             () =>
             {
                 var samples = new(
                     int Value,
                     int Angle)[] { (0, 0), (-1, 359), (360, 0), (721, 1), (int.MinValue, 232), (int.MaxValue, 127) };
                 foreach (var sample in samples) {
                     Check(new mdir(sample.Value).angle == sample.Angle, "Wrong normalized angle for " + sample.Value);
                 }
                 short negativeAngle = -1;
                 short fullTurn = 360;
                 short minimum = short.MinValue;
                 short maximum = short.MaxValue;
                 Check(new mdir(negativeAngle).angle == 359, "Negative short angle bypassed normalization");
                 Check(new mdir(fullTurn).angle == 0, "Full-turn short angle bypassed normalization");
                 Check(new mdir(minimum).angle == 352 && new mdir(maximum).angle == 7,
                       "Narrow angle limits normalized incorrectly");
                 Check(System.Runtime.InteropServices.Marshal.SizeOf<mdir>() == 2, "Angle ABI size changed");
                 Check(typeof(mdir).GetConstructor(new[] { typeof(short) })?.GetParameters()[0].ParameterType ==
                           typeof(short),
                       "Existing narrow angle constructor entrypoint was lost");
             }),
            ("ref result numeric widening",
             () =>
             {
                 long result = 1;
                 ScriptFunc.Invoke("DispatchProbe::WriteInt", ref result);
                 Check(result == 42, "Converted result was lost");
             }),
            ("managed return value populates ref result",
             () =>
             {
                 long result = 1;
                 ScriptFunc.Invoke("DispatchProbe::ReturnInt", ref result);
                 Check(result == 42, "Managed return value was lost");

                 string formatted = "";
                 ScriptFunc.Invoke("DispatchProbe::FormatInt", 7, ref formatted);
                 Check(formatted == "[7]", "Managed return value with an argument was lost");
             }),
            ("ref conversion failure reaches the caller",
             () =>
             {
                 byte result = 1;
                 int before = ScriptExceptions.GlobalCount;
                 ExpectThrows<OverflowException>(() => ScriptFunc.Invoke("DispatchProbe::WriteLargeInt", ref result),
                                                 "Overflow must reach the caller");
                 Check(result == 1, "Failed conversion changed caller result");
                 Check(ScriptExceptions.GlobalCount == before, "A propagated failure must not be counted as handled");
             }),
            ("target exception reaches the caller unwrapped",
             () =>
             {
                 int before = ScriptExceptions.GlobalCount;
                 ExpectThrows<ExampleGame.ProbeFailure>(() => ScriptFunc.Invoke("DispatchProbe::Throws"),
                                                        "The target's own exception must reach the caller");
                 Check(ScriptExceptions.GlobalCount == before, "A propagated failure must not be counted as handled");
             }),
            ("qualified enum argument",
             () =>
             {
                 ScriptFunc.Invoke("DispatchProbe::TakeEnum", "CritterProperty::strength");
                 Check(ExampleGame.DispatchProbe.EnumValue == CritterProperty.Strength, "Wrong enum value");
             }),
            ("qualified module", () => ScriptFunc.Invoke("ExampleGame.DispatchProbe::NoArgs")),
            ("nested module type",
             () =>
             {
                 ScriptFunc.Invoke("ExampleGame.DispatchProbe+Inner::Mark");
                 Check(ExampleGame.DispatchProbe.Inner.Marked, "Nested invocation did not run");
             }),
            ("overload candidates retain argument matching",
             () =>
             {
                 ScriptFunc.Invoke("DispatchProbe::Overload", new ExampleGame.First());
                 Check(ExampleGame.DispatchProbe.OverloadValue == 1, "Wrong first overload");
                 ScriptFunc.Invoke("DispatchProbe::Overload", new ExampleGame.Second());
                 Check(ExampleGame.DispatchProbe.OverloadValue == 2, "Cache reused an incompatible overload");
             }),
            ("warm dispatch avoids reflection rescans",
             () =>
             {
                 for (int i = 0; i < 100; i++) {
                     ScriptFunc.Invoke("DispatchProbe::NoArgs");
                 }
                 long before = GC.GetAllocatedBytesForCurrentThread();
                 for (int i = 0; i < 1000; i++) {
                     ScriptFunc.Invoke("DispatchProbe::NoArgs");
                 }
                 Check(GC.GetAllocatedBytesForCurrentThread() - before < 128_000,
                       "Warm dispatch repeatedly allocates reflection inventories");
             }),
            ("registered dictionary signatures",
             () =>
             {
                 Check(ScriptFuncRegistration.EngineTypeName(typeof(Dictionary<string, int>)) == "string=>int32",
                       "Wrong dictionary signature");
                 Check(ScriptFuncRegistration.EngineTypeName(typeof(Dictionary<string, List<int>>)) ==
                           "string=>int32[]",
                       "Wrong dictionary-of-array signature");
                 try {
                     ScriptFuncRegistration.EngineTypeName(typeof(HashSet<int>));
                 }
                 catch (NotSupportedException) {
                     return;
                 }
                 throw new CheckFailedException("Unsupported generic signature was accepted");
             }),
            ("unmarked methods are not callable by name",
             () =>
             {
                 int before = Native.FallbackCalls;
                 ExpectThrows<InvalidOperationException>(() => ScriptFunc.Invoke("DispatchProbe::Unmarked"),
                                                         "An unmarked managed method must not resolve by name");
                 Check(Native.FallbackCalls == before + 1, "Unmarked method did not fall through to native lookup");
             }),
            ("admin and internal named calls use separate allowlists",
             () =>
             {
                 int beforeFallback = Native.FallbackCalls;
                 Check(ScriptFunc.TryInvokeAdmin("DispatchProbe::AdminOnly"), "Admin method was not callable by name");
                 Check(ExampleGame.DispatchProbe.AdminCallCount == 1, "Admin method did not run");
                 Check(!ScriptFunc.TryInvokeAdmin("DispatchProbe::NoArgs"),
                       "Internal named method leaked into the admin allowlist");
                 ExpectThrows<InvalidOperationException>(() => ScriptFunc.Invoke("DispatchProbe::AdminOnly"),
                                                         "Admin method leaked into the internal named-call allowlist");
                 Check(ExampleGame.DispatchProbe.AdminCallCount == 1,
                       "Rejected internal invocation still ran the admin method");
                 Check(Native.FallbackCalls == beforeFallback + 1,
                       "Rejected internal invocation did not preserve the native fallback");
             }),
            ("a name that resolves nowhere throws after the native fallback",
             () =>
             {
                 int before = Native.FallbackCalls;
                 ExpectThrows<InvalidOperationException>(() => ScriptFunc.Invoke("Missing::Function"),
                                                         "A missing function must throw");
                 ExpectThrows<InvalidOperationException>(() => ScriptFunc.Invoke("Missing::Function"),
                                                         "A cached missing function must throw");
                 Check(Native.FallbackCalls == before + 2, "Cached miss bypassed native fallback");
             }),
            ("exact enum namespace wins",
             () =>
             {
                 Check(Enums.Parse("FirstEnums::Shared", "Value") == 1, "First exact name was shadowed");
                 Check(Enums.Parse("SecondEnums::Shared", "Value") == 2, "Second exact name was shadowed");
             }),
            ("foreign assembly enums are isolated",
             () =>
             {
                 var assembly = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName("ForeignEngineEnums"),
                                                                      AssemblyBuilderAccess.Run);
                 var type = assembly.DefineDynamicModule("Enums").DefineEnum("ForeignEnum",
                                                                             TypeAttributes.Public,
                                                                             typeof(int));
                 type.DefineLiteral("Value", 1);
                 type.CreateTypeInfo();
                 try {
                     Enums.Parse("ForeignEnum", "Value");
                 }
                 catch (InvalidOperationException) {
                     return;
                 }
                 throw new CheckFailedException("An enum from a foreign assembly was accepted");
             }),
            ("script entries are named the way dispatch by name spells them",
             () =>
             {
                 Check(ScriptEntryNames.Describe((Action)ExampleGame.DispatchProbe.NoArgs) == "DispatchProbe::NoArgs",
                       "A static script method is not named Type::Method");
                 Check(ScriptEntryNames.Describe((Action)ExampleGame.DispatchProbe.Inner.Mark) == "Inner::Mark",
                       "A method of a nested script type is not named after that type");
                 Check(ScriptEntryNames.Describe(ExampleGame.DispatchProbe.MakeLambda()) == "DispatchProbe::MakeLambda",
                       "A lambda is not named after the script method that wrote it");
             }),
            ("a resumed await is named after its async method",
             () =>
             {
                 Task task;

                 using (ScriptSynchronizationContext.Enter())
                 {
                     task = ExampleGame.DispatchProbe.YieldOnce();
                 }

                 Native.LastContinuationName = "";
                 ScriptSynchronizationContext.Pump();
                 Check(task.IsCompleted, "The pumped continuation did not resume the async method");
                 Check(Native.LastContinuationName == "DispatchProbe::YieldOnce (continuation)",
                       "A resumed await is not named after its async method: " + Native.LastContinuationName);
             }),
            ("duration formatting across signs and extremes",
             () =>
             {
                 var samples = new(long Value, string Text)[] { (0, "0.000 us"),
                                                                (-1, "-0.001 us"),
                                                                (1500, "1.500 us"),
                                                                (-1500, "-1.500 us"),
                                                                (-250000000, "-250.000 ms"),
                                                                (-1000000000, "-1.000 sec"),
                                                                (-90000000000, "-00:01:30 sec"),
                                                                (-86400000000000, "-1 day 00:00:00 sec"),
                                                                (-172800000000000, "-2 days 00:00:00 sec"),
                                                                (long.MinValue, "-106751 days 23:47:16 sec"),
                                                                (long.MaxValue, "106751 days 23:47:16 sec") };
                 foreach (var sample in samples) {
                     Check(new timespan(sample.Value).ToString() == sample.Text, "Wrong format for " + sample.Value);
                 }
             }),
        };
        int failures = 0;

        foreach (var test in cases) {
            try {
                test.Run();
                Console.WriteLine("PASS " + test.Name);
            }
            catch (Exception ex) {
                failures++;
                Console.WriteLine("FAIL " + test.Name + ": " + ex.Message);
            }
        }

        try {
            await ScriptFunc.InvokeAsync("DispatchProbe::AsyncCall");
            Check(ExampleGame.DispatchProbe.AsyncFinished, "Async invocation returned before completion");
            Console.WriteLine("PASS awaited invocation");
        }
        catch (Exception ex) {
            failures++;
            Console.WriteLine("FAIL awaited invocation: " + ex.Message);
        }

        Console.WriteLine($"{cases.Length + 1 - failures}/{cases.Length + 1} passed");
        return failures == 0 ? 0 : 1;
    }

    private static void Check(bool condition, string message)
    {
        if (!condition) {
            throw new CheckFailedException(message);
        }
    }

    private static void ExpectThrows<TException>(Action action, string message)
        where TException : Exception
    {
        try {
            action();
        }
        catch (TException exception) when (exception.GetType() == typeof(TException)) {
            return;
        }

        throw new CheckFailedException(message);
    }
}

// A failed check, typed apart so a case that expects an engine exception cannot swallow it
internal sealed class CheckFailedException : Exception
{
    public CheckFailedException(string message) : base(message)
    {
    }
}

namespace ExampleGame
{
public sealed class First
{
}
public sealed class Second
{
}
public sealed class ProbeFailure : Exception
{
    public ProbeFailure() : base("Dispatch probe failure")
    {
    }
}
public static class DispatchProbe
{
    public static CritterProperty EnumValue;
    public static int OverloadValue;
    public static int AdminCallCount;
    public static bool AsyncFinished;
    [CallableByName]
    public static void WriteInt(ref int value)
    {
        value = 42;
    }
    [CallableByName]
    public static void WriteLargeInt(ref int value)
    {
        value = 300;
    }
    [CallableByName]
    public static int ReturnInt()
    {
        return 42;
    }
    [CallableByName]
    public static string FormatInt(int value)
    {
        return "[" + value + "]";
    }
    [CallableByName]
    public static void TakeEnum(CritterProperty value)
    {
        EnumValue = value;
    }
    [CallableByName]
    public static void NoArgs()
    {
    }
    [CallableByName]
    public static void Throws()
    {
        throw new ProbeFailure();
    }
    [CallableByName]
    public static void Overload(First value)
    {
        OverloadValue = 1;
    }
    [CallableByName]
    public static void Overload(Second value)
    {
        OverloadValue = 2;
    }
    public static Action MakeLambda()
    {
        return () => NoArgs();
    }
    public static async Task YieldOnce()
    {
        await Task.Yield();
    }
    [CallableByName]
    public static async Task AsyncCall()
    {
        await Task.Yield();
        AsyncFinished = true;
    }
    [AdminRemoteCall]
    public static void AdminOnly()
    {
        AdminCallCount++;
    }
    public static class Inner
    {
        public static bool Marked;
        [CallableByName]
        public static void Mark()
        {
            Marked = true;
        }
    }
    public static void Unmarked()
    {
    }
}
}

namespace FirstEnums
{
public enum Shared
{
    Value = 1
}
}
namespace SecondEnums
{
public enum Shared
{
    Value = 2
}
}
