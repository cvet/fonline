namespace FOnline.Analyzers.Tests;

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Diagnostics;

// Self-tests for the engine-contract analyzers: entry-point dispatch (FOENTRY), invariant helpers (FOVERIFY) and
// script statics (FOSTATIC). Snippets compile against a stand-in for the CoreScripts declarations the rules resolve by
// metadata name; the acceptance probe and the CoreScripts conformance pass use the real CoreScripts sources
internal static class EngineContractTests
{
    private const string Preamble = @"
namespace FOnline
{
    [System.AttributeUsage(System.AttributeTargets.Class)]
    public sealed class EntryPointMarkerAttribute : System.Attribute { }

    [EntryPointMarker]
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class EventAttribute : System.Attribute { }

    [EntryPointMarker]
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class TimeEventAttribute : System.Attribute { }

    [EntryPointMarker]
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class ServerRemoteCallAttribute : System.Attribute { }

    [EntryPointMarker]
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class ItemInitAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class ModuleInitAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class PropertyGetterAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class AnimCallbackAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class ScriptFuncRegistrarAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class CallableByNameAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class CallableFromNativeAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class CallableByEngineAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class PropertyMigratorAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class ProtoMigratorAttribute : System.Attribute { }

    public sealed class DatabaseDocument { }

    public readonly struct hstring
    {
        public hstring(string text) { }
    }

    public class Critter
    {
        public bool HasKit { get { return true; } }
        public int Level { get { return 1; } }
    }

    public sealed class EventSlot
    {
        public void Subscribe(System.Action<Critter> handler) { }
    }

    public static class Game
    {
        public static EventSlot OnCritterDamaged = new EventSlot();
    }

    public static class Invariant
    {
        public static void Verify([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool condition, string message) { }
        public static void Verify<T0>([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool condition, string message, T0 arg0) { }
        [System.Diagnostics.CodeAnalysis.DoesNotReturn]
        public static void Failed(string message) { throw new System.InvalidOperationException(message); }
        public static T VerifyNotNull<T>(T? value, string message) where T : class
        {
            if (value == null) {
                throw new System.InvalidOperationException(message);
            }

            return value;
        }
    }
}

namespace Decoy
{
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class EventAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class CallableByNameAttribute : System.Attribute { }

    public static class Invariant
    {
        public static void Verify(bool condition, string message) { }
    }
}

namespace Sample
{
    // A project dispatch marker joins the vocabulary by declaring [EntryPointMarker], as the engine markers do
    [FOnline.EntryPointMarker]
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class DemandAttribute : System.Attribute { }

    // Project markers that the engine vocabulary knows only through .editorconfig options
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class HookAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class ScriptAttribute : System.Attribute { }
}
";

    private const string ScriptPath = "/Project/Scripts/Probe.cs";

    public static void Run(List<string> failures)
    {
        CheckEntryPointCalls(failures);
        CheckEngineCalledAccess(failures);
        CheckEntryOptions(failures);
        CheckInvariants(failures);
        CheckScriptStatics(failures);
        CheckAcceptanceProbe(failures);
        CheckCoreScriptsConform(failures);
    }

    private static void CheckEntryPointCalls(List<string> failures)
    {
        Check(failures,
              "a direct call to an event handler is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        internal static void OnDamaged(Critter cr) { }

        public static void Caller(Critter cr) { OnDamaged(cr); }
    }
}",
              "FOENTRY001");

        Check(failures, "a method-group subscription is the prescribed path and stays silent", @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        internal static void OnDamaged(Critter cr) { }

        public static void Init() { Game.OnCritterDamaged.Subscribe(OnDamaged); }
    }
}");

        Check(failures, "nameof is not an invocation", @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        internal static void OnDamaged(Critter cr) { }

        public static string Name() { return nameof(OnDamaged); }
    }
}");

        Check(failures, "a plain helper without an attribute is called freely", @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        public static void ApplyDamage(Critter cr) { }

        public static void Caller(Critter cr) { ApplyDamage(cr); }
    }
}");

        Check(failures,
              "every engine dispatch attribute and a project [EntryPointMarker] marker are covered",
              @"
namespace Sample
{
    using FOnline;
    public static class Quests
    {
        [TimeEvent]
        internal static void OnTick() { }

        [ServerRemoteCall]
        internal static void Request(Critter cr) { }

        [ItemInit]
        internal static void InitItem(Critter cr) { }

        [ModuleInit]
        internal static void Init() { }

        [PropertyGetter]
        internal static int Getter(Critter cr) { return 0; }

        [AnimCallback]
        internal static void Anim(Critter cr) { }

        [ScriptFuncRegistrar]
        internal static void Register() { }

        [Demand]
        internal static bool Demand(Critter cr) { return true; }

        public static void Caller(Critter cr)
        {
            OnTick();
            Request(cr);
            InitItem(cr);
            Init();
            _ = Getter(cr);
            Anim(cr);
            Register();
            _ = Demand(cr);
        }
    }
}",
              "FOENTRY001",
              "FOENTRY001",
              "FOENTRY001",
              "FOENTRY001",
              "FOENTRY001",
              "FOENTRY001",
              "FOENTRY001",
              "FOENTRY001");

        Check(failures, "a registered method keeps ordinary C# calls", @"
namespace Sample
{
    using FOnline;
    public static class Named
    {
        [CallableByName]
        internal static void Apply(Critter cr) { }

        [CallableFromNative]
        internal static void Native(Critter cr) { }

        [PropertyMigrator]
        internal static bool Migrate(ref int value, DatabaseDocument document) { return false; }

        public static bool Caller(Critter cr, ref int value, DatabaseDocument document)
        {
            Apply(cr);
            Native(cr);
            return Migrate(ref value, document);
        }
    }
}");

        Check(failures,
              "a qualified call reads the same as an unqualified one",
              @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        internal static void OnDamaged(Critter cr) { }
    }

    public static class Ai
    {
        public static void Caller(Critter cr) { Combat.OnDamaged(cr); }
    }
}",
              "FOENTRY001");

        Check(failures, "an attribute that only shares an engine name is not an entry point", @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Decoy.Event]
        public static void OnDamaged(Critter cr) { }

        public static void Caller(Critter cr) { OnDamaged(cr); }
    }
}");

        CheckDetailed(failures,
                      "the diagnostic spans the invocation",
                      @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        internal static void OnDamaged(Critter cr) { }

        public static void Caller(Critter cr) { OnDamaged(cr); }
    }
}",
                      ("FOENTRY001", "OnDamaged(cr)"));
    }

    private static void CheckEngineCalledAccess(List<string> failures)
    {
        Check(failures,
              "a private engine entry point is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        private static void OnDamaged(Critter cr) { }
    }
}",
              "FOENTRY002");

        Check(failures,
              "a public engine entry point is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        public static void OnDamaged(Critter cr) { }
    }
}",
              "FOENTRY002");

        Check(failures, "an internal engine entry point stays silent", @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        internal static void OnDamaged(Critter cr) { }
    }
}");

        foreach (string attribute in new[] { "CallableByName",
                                             "CallableFromNative",
                                             "CallableByEngine",
                                             "ModuleInit",
                                             "ScriptFuncRegistrar",
                                             "Demand" }) {
            string snippet = $@"
namespace Sample
{{
    using FOnline;
    public static class Named
    {{
        [{attribute}]
        internal static void Entry() {{ }}
    }}
}}";

            Check(failures, $"{attribute}: internal satisfies the contract", snippet);
            Check(failures,
                  $"{attribute}: private breaks the contract",
                  snippet.Replace("internal static", "private static"),
                  "FOENTRY002");
            Check(failures,
                  $"{attribute}: public breaks the contract",
                  snippet.Replace("internal static", "public static"),
                  "FOENTRY002");
        }

        foreach ((string attribute, string valueType)
                     in new[] { ("PropertyMigrator", "int"), ("ProtoMigrator", "hstring") }) {
            string snippet = $@"
namespace Sample
{{
    using FOnline;
    public static class Migrations
    {{
        [{attribute}]
        internal static bool Rewrite(ref {valueType} value, DatabaseDocument document) {{ return false; }}
    }}
}}";

            Check(failures, $"{attribute}: internal satisfies the contract", snippet);
            Check(failures,
                  $"{attribute}: private breaks the contract",
                  snippet.Replace("internal static", "private static"),
                  "FOENTRY002");
        }

        Check(failures, "a decoy attribute does not set engine access", @"
namespace Sample
{
    public static class Named
    {
        [Decoy.CallableByName]
        private static void Entry() { }
    }
}");

        Check(failures, "a private helper without an engine attribute is not judged", @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        private static void ApplyDamage(Critter cr) { }

        internal static void Run(Critter cr) { ApplyDamage(cr); }
    }
}");

        Check(failures, "an override repeats the base access and is not judged", @"
namespace Sample
{
    using FOnline;
    public abstract class ProbeBase
    {
        public abstract void OnDamaged(Critter cr);
    }

    public sealed class ProbeDerived : ProbeBase
    {
        [Event]
        public override void OnDamaged(Critter cr) { }
    }
}");

        Check(failures, "an explicit interface implementation is not judged", @"
namespace Sample
{
    using FOnline;
    public interface IHandler
    {
        void OnDamaged(Critter cr);
    }

    public sealed class Handler : IHandler
    {
        [Event]
        void IHandler.OnDamaged(Critter cr) { }
    }
}");

        Check(failures,
              "engine CoreScripts are held to the same contract",
              @"
namespace FOnline
{
    public static class Initializator
    {
        [CallableByEngine]
        private static void InitializeEarly() { }
    }
}",
              "FOENTRY002");

        Check(failures, "the baker's generated wrappers keep their private lookup members", @"// <auto-generated />
namespace FOnline
{
    public sealed class NativeHolder
    {
        [CallableByEngine]
        private void __AdoptNativeReference() { }
    }
}");

        Check(failures,
              "generated code of a project generator is judged",
              @"// <auto-generated />
namespace Sample
{
    using FOnline;
    public sealed class Screen
    {
        [CallableByName]
        public static void OnPress() { }
    }
}",
              "FOENTRY002");

        CheckDetailed(failures,
                      "the diagnostic names the method declaration",
                      @"
namespace Sample
{
    using FOnline;
    public static class Combat
    {
        [Event]
        public static void OnDamaged(Critter cr) { }
    }
}",
                      ("FOENTRY002", "OnDamaged"));
    }

    private static void CheckEntryOptions(List<string> failures)
    {
        const string Hooks = @"
namespace Sample
{
    using FOnline;
    public static class Hooks
    {
        [Hook]
        public static void OnHook(Critter cr) { }

        [Script]
        public static void Scripted(Critter cr) { }

        public static void Caller(Critter cr)
        {
            OnHook(cr);
            Scripted(cr);
        }
    }
}";

        CheckOptions(failures, "project markers are ordinary attributes without options", Hooks, null);

        CheckOptions(failures,
                     "a dispatched option makes a project attribute an entry point",
                     Hooks,
                     new Dictionary<string, string> {
                         [EntryPointAnalyzer.DispatchedAttributesOption] = "Sample.HookAttribute",
                     },
                     "FOENTRY001",
                     "FOENTRY002");

        CheckOptions(failures,
                     "a registered option requires internal access only",
                     Hooks,
                     new Dictionary<string, string> {
                         [EntryPointAnalyzer.RegisteredAttributesOption] = "Sample.ScriptAttribute",
                     },
                     "FOENTRY002");

        CheckOptions(
            failures,
            "both options accept comma-separated lists",
            Hooks,
            new Dictionary<string, string> {
                [EntryPointAnalyzer.DispatchedAttributesOption] = "Sample.HookAttribute, Sample.DemandAttribute",
                [EntryPointAnalyzer.RegisteredAttributesOption] = "Sample.ScriptAttribute",
            },
            "FOENTRY001",
            "FOENTRY002",
            "FOENTRY002");

        CheckOptions(
            failures,
            "a misspelt attribute name is reported instead of ignored",
            Hooks,
            new Dictionary<string, string> {
                [EntryPointAnalyzer.RegisteredAttributesOption] = "Sample.ScriptAttribute,Sample.ScritpAttribute",
            },
            "FOENTRY002",
            "FOENTRY003");
    }

    private static void CheckInvariants(List<string> failures)
    {
        Check(failures,
              "a Verify message repeating its condition verbatim",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.HasKit, ""cr.HasKit""); }
    }
}",
              "FOVERIFY001");

        Check(failures,
              "a message that is a member chain with no whitespace",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.HasKit == true, ""cr.HasKit""); }
    }
}",
              "FOVERIFY001");

        Check(failures,
              "a message equal to the condition, operators and all",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.Level >= 3, ""cr.Level >= 3""); }
    }
}",
              "FOVERIFY001");

        Check(failures, "a human description of the invariant stays silent", @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.HasKit, ""Build kit must carry a construction kit""); }
    }
}");

        Check(failures, "a sentence ending in a period is not a member chain", @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.HasKit, ""Kit is required.""); }
    }
}");

        Check(failures, "context arguments do not change how the message is read", @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.HasKit, ""Build kit must carry a kit"", cr.Level); }
    }
}");

        Check(failures,
              "a Verify with a constant false condition is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { if (!cr.HasKit) { Invariant.Verify(false, ""Build kit is required"", cr.Level); } }
    }
}",
              "FOVERIFY002");

        Check(failures, "a Verify with a real condition is not FOVERIFY002", @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(!cr.HasKit || cr.Level > 0, ""Build kit needs a leveled owner""); }
    }
}");

        Check(failures, "Invariant.Failed is the prescribed form", @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { if (!cr.HasKit) { Invariant.Failed(""Build kit is required""); } }
    }
}");

        Check(failures, "a Verify outside FOnline.Invariant is not judged", @"
namespace Sample
{
    using FOnline;
    public static class Other
    {
        public static void Verify(bool condition, string message) { }
        public static void Check(Critter cr)
        {
            Verify(cr.HasKit, ""cr.HasKit"");
            Decoy.Invariant.Verify(false, ""cr.HasKit"");
        }
    }
}");

        Check(failures,
              "an await in the Verify condition is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static async System.Threading.Tasks.Task Check(Critter cr)
        {
            Invariant.Verify(await System.Threading.Tasks.Task.FromResult(cr.HasKit), ""Build kit must stay covered"");
        }
    }
}",
              "FOVERIFY003");

        Check(failures,
              "an out argument in the Verify condition is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(string text)
        {
            Invariant.Verify(int.TryParse(text, out int value), ""Value must be an integer"");
        }
    }
}",
              "FOVERIFY003");

        Check(failures,
              "an assignment in the Verify condition is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check()
        {
            int n = 0;
            Invariant.Verify((n = 1) > 0, ""Counter must become positive"");
        }
    }
}",
              "FOVERIFY003");

        Check(failures,
              "an increment in the Verify condition is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(int n)
        {
            Invariant.Verify(n++ < 10, ""Counter must stay below the limit"");
        }
    }
}",
              "FOVERIFY003");

        Check(failures,
              "an await in VerifyNotNull is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static async System.Threading.Tasks.Task Check(Critter cr)
        {
            Critter live = Invariant.VerifyNotNull(await System.Threading.Tasks.Task.FromResult<Critter?>(cr), ""Critter is required"");
            _ = live;
        }
    }
}",
              "FOVERIFY003");

        Check(failures, "a predicate Verify is not FOVERIFY003", @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.HasKit, ""Build kit must carry a kit""); }
    }
}");

        Check(failures, "an out in a context argument is not FOVERIFY003", @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(System.Collections.Generic.Dictionary<int, int> map, int key)
        {
            Invariant.Verify(map.ContainsKey(key), ""Key must be present"", map.TryGetValue(key, out int value) ? value : 0);
        }
    }
}");

        CheckDetailed(failures,
                      "the echo diagnostic spans the message literal",
                      @"
namespace Sample
{
    using FOnline;
    public static class Build
    {
        public static void Check(Critter cr) { Invariant.Verify(cr.HasKit, ""cr.HasKit""); }
    }
}",
                      ("FOVERIFY001", "\"cr.HasKit\""));
    }

    private static void CheckScriptStatics(List<string> failures)
    {
        Check(failures,
              "a static reference in a generic type is reported",
              @"
namespace Sample
{
    using FOnline;
    public static class Cache<TValue>
    {
        private static Critter? LastCritter;
        public static void Remember(Critter cr) { LastCritter = cr; }
        public static Critter? Recall() { return LastCritter; }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a static in a type nested in a generic type is reported",
              @"
namespace Sample
{
    public static class Outer<T>
    {
        public static class Inner
        {
            public static object? Value;
        }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a thread static reference is reported",
              @"
namespace Sample
{
    using System;
    using FOnline;
    public static class Current
    {
        [ThreadStatic]
        private static Critter? ActiveCritter;
        public static void Remember(Critter cr) { ActiveCritter = cr; }
        public static Critter? Recall() { return ActiveCritter; }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a static value type carrying a reference is reported",
              @"
namespace Sample
{
    using FOnline;
    public struct Slot
    {
        public Critter? Owner;
    }
    public static class Holder
    {
        private static Slot CurrentSlot;
        public static void Remember(Critter cr) { CurrentSlot.Owner = cr; }
        public static Critter? Recall() { return CurrentSlot.Owner; }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a generic auto-property is reported",
              @"
namespace Sample
{
    public static class Cache<T>
    {
        public static object? Value { get; set; }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a generic field-like event is reported",
              @"
namespace Sample
{
    public static class Cache<T>
    {
        public static event System.Action? Changed;
        public static void Fire() { Changed?.Invoke(); }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a thread static field-like event is reported",
              @"
namespace Sample
{
    public static class Cache
    {
        [field: System.ThreadStatic]
        public static event System.Action? Changed;
        public static void Fire() { Changed?.Invoke(); }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "an auto-property hiding a reference in a value type is reported",
              @"
namespace Sample
{
    public static class Cache
    {
        public static (object? Value, int Count) State { get; set; }
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a value type with an event carries references",
              @"
namespace Sample
{
    public struct Handler
    {
        public event System.Action? Changed;
        public void Fire() { Changed?.Invoke(); }
    }
    public static class Cache
    {
        public static Handler Value;
    }
}",
              "FOSTATIC001");

        Check(failures,
              "a metadata value type with private reference storage is reported",
              @"
namespace Sample
{
    public static class Cache
    {
        public static System.Memory<object> Value;
    }
}",
              "FOSTATIC001");

        Check(failures, "an unmanaged type parameter carries no references", @"
namespace Sample
{
    public static class Cache<T> where T : unmanaged
    {
        public static T Value;
    }
}");

        Check(failures, "a computed generic property has no static storage", @"
namespace Sample
{
    public static class Cache<T>
    {
        public static object Value => new object();
    }
}");

        Check(failures, "an ordinary static reference the engine clears is silent", @"
namespace Sample
{
    using System.Collections.Generic;
    using FOnline;
    public static class Tracked
    {
        private static List<Critter> Critters = new List<Critter>();
        public static void Remember(Critter cr) { Critters.Add(cr); }
    }
}");

        Check(failures, "a static value type without references is silent", @"
namespace Sample
{
    public struct Counters
    {
        public int Hits;
    }
    public static class Stats
    {
        private static Counters Totals;
        public static void Count() { Totals.Hits++; }
    }
}");

        Check(failures, "constants have no storage", @"
namespace Sample
{
    public static class Cache<T>
    {
        public const string Name = ""Cache"";
    }
}");

        Check(failures, "the engine namespace is outside the sweep and outside the rule", @"
namespace FOnline.Runtime
{
    public static class Cache<T>
    {
        [System.ThreadStatic]
        public static object? Value;
    }
}");

        Check(failures,
              "a project namespace that only starts with the engine name is judged",
              @"
namespace FOnlineGame
{
    public static class Cache<T>
    {
        public static object? Value;
    }
}",
              "FOSTATIC001");

        CheckDetailed(failures,
                      "the diagnostic names the storage",
                      @"
namespace Sample
{
    public static class Cache<T>
    {
        public static object? Value { get; set; }
    }
}",
                      ("FOSTATIC001", "Value"));
    }

    // The probe compiles against the real CoreScripts declarations, so a rule that stops resolving an engine attribute or
    // the Invariant type fails here even when every stand-in snippet above still passes
    private static void CheckAcceptanceProbe(List<string> failures)
    {
        string managedRoot = ManagedSourceRoot();
        string probePath = Path.Combine(managedRoot, "Analyzers", "Acceptance", "EngineContractProbe.cs");
        string probeSource = File.ReadAllText(probePath);
        var trees = new List<SyntaxTree> { CSharpSyntaxTree.ParseText(probeSource, path: probePath) };

        foreach (string coreScript in new[] { "Attributes.cs", "Invariant.cs" }) {
            string path = Path.Combine(managedRoot, "CoreScripts", coreScript);
            trees.Add(CSharpSyntaxTree.ParseText(File.ReadAllText(path), path: path));
        }

        CSharpCompilation compilation = CreateCompilation(trees);
        ImmutableArray<Diagnostic> compileErrors =
            compilation.GetDiagnostics().Where(d => d.Severity == DiagnosticSeverity.Error).ToImmutableArray();

        if (!compileErrors.IsEmpty) {
            failures.Add("acceptance probe does not compile against CoreScripts: " + compileErrors[0]);
            return;
        }

        List<Diagnostic> reported = Analyze(compilation, null).ToList();
        string[] lines = probeSource.Split('\n');
        var markers = new List<(int Line, string Id)>();
        var notApplicable = new HashSet<string>(StringComparer.Ordinal);

        for (int i = 0; i < lines.Length; i++) {
            Match accept = Regex.Match(lines[i], @"//\s*ACCEPT:\s*([A-Z]+\d+)\s*$");
            Match skip = Regex.Match(lines[i], @"//\s*ACCEPT-NA:\s*([A-Z]+\d+)\s+-\s+\S");

            if (accept.Success) {
                markers.Add((i, accept.Groups[1].Value));
            }
            else if (skip.Success) {
                notApplicable.Add(skip.Groups[1].Value);
            }
        }

        var claimed = new HashSet<Diagnostic>();

        for (int m = 0; m < markers.Count; m++) {
            int first = markers[m].Line + 1;
            int last = m + 1 < markers.Count ? markers[m + 1].Line : lines.Length;
            Diagnostic? hit = reported.FirstOrDefault(d => d.Id == markers[m].Id && !claimed.Contains(d) &&
                                                           d.Location.GetLineSpan().StartLinePosition.Line >= first &&
                                                           d.Location.GetLineSpan().StartLinePosition.Line < last);

            if (hit == null) {
                failures.Add($"acceptance probe: {markers[m].Id} at line {markers[m].Line + 1} did not fire");
                continue;
            }

            claimed.Add(hit);
        }

        foreach (Diagnostic unexpected in reported.Where(d => !claimed.Contains(d))) {
            failures.Add("acceptance probe: unexpected " + unexpected);
        }

        foreach (string id in ContractRuleIds()) {
            if (!markers.Any(marker => marker.Id == id) && !notApplicable.Contains(id)) {
                failures.Add($"acceptance probe: {id} has neither an ACCEPT case nor an ACCEPT-NA reason");
            }
        }
    }

    // Every project compiles the engine CoreScripts with these analyzers enabled, so CoreScripts must not trip them. The
    // generated API is absent here; calls that do not bind are skipped, and the bound part still proves the contract
    private static void CheckCoreScriptsConform(List<string> failures)
    {
        string coreScripts = Path.Combine(ManagedSourceRoot(), "CoreScripts");
        string[] files =
            Directory.GetFiles(coreScripts, "*.cs").OrderBy(path => path, StringComparer.Ordinal).ToArray();

        if (files.Length == 0) {
            failures.Add("CoreScripts conformance: no sources found in " + coreScripts);
            return;
        }

        foreach (string role in new[] { "SERVER", "CLIENT", "MAPPER" }) {
            var parseOptions = new CSharpParseOptions(LanguageVersion.Latest, preprocessorSymbols: new[] { role });
            SyntaxTree[] trees =
                files.Select(path => CSharpSyntaxTree.ParseText(File.ReadAllText(path), parseOptions, path)).ToArray();

            foreach (Diagnostic diagnostic in Analyze(CreateCompilation(trees), null)) {
                failures.Add($"CoreScripts conformance ({role}): {diagnostic}");
            }
        }
    }

    private static IEnumerable<string> ContractRuleIds()
    {
        return Analyzers().SelectMany(analyzer => analyzer.SupportedDiagnostics).Select(rule => rule.Id).Distinct();
    }

    private static string ManagedSourceRoot()
    {
        string? root = typeof(EngineContractTests)
                           .Assembly.GetCustomAttributes<AssemblyMetadataAttribute>()
                           .FirstOrDefault(attribute => attribute.Key == "FOnline.ManagedSourceRoot")
                           ?.Value;

        if (root == null || !Directory.Exists(Path.Combine(root, "CoreScripts"))) {
            throw new InvalidOperationException("analyzer self-tests cannot locate Source/Scripting/Managed: " + root);
        }

        return root;
    }

    private static void Check(List<string> failures, string name, string snippet, params string[] expected)
    {
        CheckOptions(failures, name, snippet, null, expected);
    }

    private static void CheckOptions(List<string> failures, string name, string snippet,
                                     Dictionary<string, string>? options, params string[] expected)
    {
        ImmutableArray<Diagnostic> reported = RunSnippet(snippet, options);
        string[] actual = reported.Select(d => d.Id).OrderBy(id => id, StringComparer.Ordinal).ToArray();
        string[] wanted = expected.OrderBy(id => id, StringComparer.Ordinal).ToArray();

        if (!actual.SequenceEqual(wanted)) {
            failures.Add($"{name}: expected [{string.Join(", ", wanted)}] but got [{string.Join(", ", actual)}]");
        }
    }

    private static void CheckDetailed(List<string> failures, string name, string snippet,
                                      (string Id, string Span)expected)
    {
        ImmutableArray<Diagnostic> reported = RunSnippet(snippet, null);

        if (reported.Length != 1 || reported[0].Id != expected.Id) {
            failures.Add(
                $"{name}: expected one {expected.Id} but got [{string.Join(", ", reported.Select(d => d.Id))}]");
            return;
        }

        string span =
            reported[0].Location.SourceTree?.GetText().ToString(reported[0].Location.SourceSpan) ?? string.Empty;

        if (span != expected.Span) {
            failures.Add($"{name}: {expected.Id} spans '{span}', expected '{expected.Span}'");
        }
    }

    private static ImmutableArray<Diagnostic> RunSnippet(string snippet, Dictionary<string, string>? options)
    {
        CSharpCompilation compilation = CreateCompilation(new[] {
            CSharpSyntaxTree.ParseText(Preamble, path: "/Engine/Preamble.cs"),
            CSharpSyntaxTree.ParseText(snippet, path: ScriptPath),
        });

        // A snippet that does not compile would make a diagnostic expectation meaningless
        ImmutableArray<Diagnostic> compileErrors =
            compilation.GetDiagnostics().Where(d => d.Severity == DiagnosticSeverity.Error).ToImmutableArray();

        if (!compileErrors.IsEmpty) {
            throw new InvalidOperationException("analyzer self-test snippet does not compile: " + compileErrors[0]);
        }

        return Analyze(compilation, options);
    }

    private static ImmutableArray<Diagnostic> Analyze(CSharpCompilation compilation,
                                                      Dictionary<string, string>? options)
    {
        var analyzerOptions =
            new AnalyzerOptions(ImmutableArray<AdditionalText>.Empty,
                                new ScriptOptionsProvider(options ?? new Dictionary<string, string>()));

        return compilation.WithAnalyzers(Analyzers(), analyzerOptions)
            .GetAnalyzerDiagnosticsAsync()
            .GetAwaiter()
            .GetResult();
    }

    private static ImmutableArray<DiagnosticAnalyzer> Analyzers()
    {
        return ImmutableArray.Create<DiagnosticAnalyzer>(new EntryPointAnalyzer(),
                                                         new InvariantAnalyzer(),
                                                         new ScriptStaticAnalyzer());
    }

    private static CSharpCompilation CreateCompilation(IEnumerable<SyntaxTree> trees)
    {
        List<MetadataReference> references =
            ((string?)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") ?? string.Empty)
                .Split(Path.PathSeparator)
                .Where(path => path.Length != 0)
                .Select(path => (MetadataReference)MetadataReference.CreateFromFile(path))
                .ToList();

        return CSharpCompilation.Create(
            "EngineContractSelfTest",
            trees,
            references,
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                                         nullableContextOptions: NullableContextOptions.Enable));
    }

    // Stands in for an .editorconfig section that covers the snippet file
    private sealed class ScriptOptionsProvider : AnalyzerConfigOptionsProvider
    {
        private readonly AnalyzerConfigOptions ScriptOptions;

        public ScriptOptionsProvider(Dictionary<string, string> values)
        {
            ScriptOptions = new ValueOptions(values);
        }

        public override AnalyzerConfigOptions GlobalOptions => new ValueOptions(new Dictionary<string, string>());

        public override AnalyzerConfigOptions GetOptions(SyntaxTree tree)
        {
            return tree.FilePath == ScriptPath ? ScriptOptions : new ValueOptions(new Dictionary<string, string>());
        }

        public override AnalyzerConfigOptions GetOptions(AdditionalText textFile)
        {
            return new ValueOptions(new Dictionary<string, string>());
        }
    }

    private sealed class ValueOptions : AnalyzerConfigOptions
    {
        private readonly Dictionary<string, string> Values;

        public ValueOptions(Dictionary<string, string> values)
        {
            Values = values;
        }

        public override bool TryGetValue(string key, out string value)
        {
            if (Values.TryGetValue(key, out string? found)) {
                value = found;
                return true;
            }

            value = string.Empty;
            return false;
        }
    }
}
