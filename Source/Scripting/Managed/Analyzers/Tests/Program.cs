#nullable enable

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Diagnostics;

namespace FOnline.Analyzers.Tests
{
    // Self-tests for the managed script analyzers. Each case compiles a snippet against a minimal stand-in for
    // the pieces of the script surface the analyzer reasons about, runs the analyzer over it, and compares the
    // reported diagnostic ids against the expectation.
    internal static class Program
    {
        // The analyzer resolves the attributes, FOnline.Entity and the engine-owned FOnline.Sync by metadata
        // name, so a small stand-in keeps the tests independent of the real CoreScripts build.
        private const string Preamble = @"
namespace FOnline
{
    [System.Flags]
    public enum CoverReach { None = 0, Parent = 1, Ancestors = 2, DestroyGraph = 4 }

    [System.AttributeUsage(System.AttributeTargets.Parameter | System.AttributeTargets.Method)]
    public sealed class RequiresCoverAttribute : System.Attribute
    {
        public RequiresCoverAttribute(CoverReach reach = CoverReach.None) { Reach = reach; }
        public CoverReach Reach { get; private set; }
    }


    [System.AttributeUsage(System.AttributeTargets.Parameter | System.AttributeTargets.ReturnValue)]
    public sealed class ProvidesCoverAttribute : System.Attribute
    {
        public ProvidesCoverAttribute(CoverReach reach = CoverReach.None) { Reach = reach; }
        public CoverReach Reach { get; private set; }
    }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class EventAttribute : System.Attribute { }

    // The baker puts this on the base and overrides it on the generated prototype/static classes; the analyzer
    // reads the override rather than a list of type names.
    public class Entity
    {
        public virtual bool IsAlwaysCovered { get { return false; } }
    }
    public class Critter : Entity
    {
        [RequiresCover]
        public void SendGroupInfo() { }

        public void Untracked() { }
    }
    public class Map : Entity { }

    // Item methods are declared once on the shared base and inherited by both, which is what makes the
    // static side worth modelling explicitly.
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class PreservesCoverAttribute : System.Attribute
    {
    }

    public class AbstractItem : Entity
    {
        [RequiresCover]
        public int GetCount() { return 0; }

        public void Give([RequiresCover] Critter cr) { }

        public static void Compare([RequiresCover] AbstractItem other) { }
    }

    public class Item : AbstractItem { }
    public class StaticItem : AbstractItem
    {
        public override bool IsAlwaysCovered { get { return true; } }
    }
    public class ProtoItem : AbstractItem
    {
        public override bool IsAlwaysCovered { get { return true; } }
    }

    public class ProtoCritter : Critter
    {
        public override bool IsAlwaysCovered { get { return true; } }
    }

    // Stands in for the engine exports that hand back a covered set, such as Map.GetCrittersInRadius: the
    // acquisition covered the map, and the map's cover reaches the critters on it.
    public static class Roster
    {
        [return: ProvidesCover]
        public static System.Collections.Generic.List<Critter> Nearby(Map map) { return null; }
    }

    public static partial class Sync
    {
        public static System.Threading.Tasks.Task<bool> Widen(Entity entity) { return System.Threading.Tasks.Task.FromResult(true); }
        public static bool Lock(Entity entity) { return true; }
        public static bool WidenCritterWithMap(Critter cr) { return true; }
        public static bool IsCovered(Entity entity) { return true; }
        public static System.Threading.Tasks.Task<bool> LockAsync(Entity entity) { return System.Threading.Tasks.Task.FromResult(true); }
    }

    public static class Game
    {
        public static bool IsEntityLocked(Entity entity) { return true; }
        public static bool TrySyncEntity(int id) { return true; }
        public static void Sync(Entity entity) { }

        // The singleton bucket lock, deliberately not part of the raw-primitive rule.
        public static void Lock() { }
        public static void Unlock() { }

        // The rest of the surface, which shares the type but takes entities as ordinary arguments.
        public static void Verify(bool condition, string message, params object[] context) { }
    }
}

namespace LastFrontier
{
    using FOnline;

    // A same-named project type must NOT be able to discharge an engine cover obligation.
    public static class Sync2
    {
        public static bool Lock(Entity entity) { return true; }
    }
}
";

        private static int Main()
        {
            var failures = new List<string>();

            Check(failures, "annotation on an entity parameter is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "reach flags on an entity parameter are silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover(CoverReach.Parent | CoverReach.Ancestors)] Critter cr) { }
    }
}");

            Check(failures, "a collection of entities carries the contract element-wise", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Reads([RequiresCover] List<Critter> crs) { }
    }
}");

            Check(failures, "annotation on a non-entity is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] int hp) { }
    }
}", "FOSYNC001");

            Check(failures, "ProvidesCover on a non-entity return is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover]
        public static int Make() { return 0; }
    }
}", "FOSYNC001");

            Check(failures, "undischarged call is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller(Critter cr) { Reads(cr); }
    }
}", "FOSYNC002");

            Check(failures, "acquiring cover discharges the obligation", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller(Critter cr)
        {
            Sync.Lock(cr);
            Reads(cr);
        }
    }
}");

            Check(failures, "propagating the obligation discharges it", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller([RequiresCover] Critter cr) { Reads(cr); }
    }
}");

            Check(failures, "a ProvidesCover return discharges through a local", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [return: ProvidesCover(CoverReach.Parent)]
        public static Critter Spawn() { return null!; }

        public static void Caller()
        {
            Critter spawned = Spawn();
            Reads(spawned);
        }
    }
}");

            Check(failures, "a ProvidesCover call discharges inline", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [return: ProvidesCover]
        public static Critter Spawn() { return null!; }

        public static void Caller() { Reads(Spawn()); }
    }
}");

            Check(failures, "an unprovided local is still reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static Critter Make() { return null!; }

        public static void Caller()
        {
            Critter made = Make();
            Reads(made);
        }
    }
}", "FOSYNC002");

            Check(failures, "only the demanding argument is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr, Critter other) { }

        public static void Caller(Critter a, Critter b) { Reads(a, b); }
    }
}", "FOSYNC002");

            Check(failures, "each demanding argument is reported separately", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr, [RequiresCover] Critter killer) { }

        public static void Caller(Critter a, Critter b) { Reads(a, b); }
    }
}", "FOSYNC002", "FOSYNC002");

            Check(failures, "a look-alike project type does not discharge", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller(Critter cr)
        {
            Sync2.Lock(cr);
            Reads(cr);
        }
    }
}", "FOSYNC002");

            Check(failures, "an unannotated callee is not policed", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr) { }

        public static void Caller(Critter cr) { Reads(cr); }
    }
}");




            Check(failures, "a non-entry caller with the same shape is still reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void PlainHelper(Critter cr) { Reads(cr); }
    }
}", "FOSYNC002");




            Check(failures, "a ProvidesCover parameter discharges that argument", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Establish([ProvidesCover] Critter cr) { }

        public static void Caller(Critter cr)
        {
            Establish(cr);
            Reads(cr);
        }
    }
}");

            Check(failures, "an awaited ProvidesCover return discharges through a local", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [return: ProvidesCover]
        public static Task<Critter> SpawnAsync() { return null!; }

        public static async Task Caller()
        {
            Critter spawned = await SpawnAsync();
            Reads(spawned);
        }
    }
}");

            Check(failures, "a ProvidesCover call on a different value does not discharge", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Establish([ProvidesCover] Critter cr) { }

        public static void Caller(Critter a, Critter b)
        {
            Establish(b);
            Reads(a);
        }
    }
}", "FOSYNC002");

            Check(failures, "an entry point must declare the cover the engine gives it", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [Event]
        public static void OnSomething(Critter cr) { }
    }
}", "FOSYNC003");

            Check(failures, "probing whether cover is held is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr)
        {
            if (Sync.IsCovered(cr)) { }
        }
    }
}", "FOSYNC004");

            Check(failures, "the engine probe is reported the same way", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr)
        {
            if (Game.IsEntityLocked(cr)) { }
        }
    }
}", "FOSYNC004");

            Check(failures, "a raw cover primitive outside Sync is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr) { Game.Sync(cr); }
    }
}", "FOSYNC005");

            Check(failures, "the Game singleton bucket lock is not a cover primitive", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads()
        {
            Game.Lock();
            Game.Unlock();
        }
    }
}");

            Check(failures, "Sync itself may probe and use the primitives", @"
namespace FOnline
{
    public static partial class Sync
    {
        public static void Helper(Critter cr)
        {
            if (IsCovered(cr)) { }
            Game.Sync(cr);
        }
    }
}");

            Check(failures, "resolving an id to a live entity is not a raw primitive", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Work(int playerId)
        {
            if (!Game.TrySyncEntity(playerId)) { }
        }
    }
}");

            // FOSYNC009 -- the value-aware half: cover that an await released and nothing took back.
            Check(failures, "a value used after an await with no re-proof is reported", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr)
        {
            await Pause();
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}", "FOSYNC009");

            Check(failures, "re-proving the value after the await clears it", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr)
        {
            await Pause();
            if (!await Sync.Widen(cr)) { return; }
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "awaiting a preserving callee releases nothing", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        [PreservesCover]
        static Task Keeps() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr)
        {
            await Keeps();
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            // Source order is not execution order; these two are what the position-only version got wrong.
            Check(failures, "an await in a sibling branch does not reach the other branch", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr, bool flag)
        {
            if (flag) { await Pause(); }
            else { Needs(cr); }
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "an await in a guard branch that returns does not reach past it", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr, bool bad)
        {
            if (bad) { await Pause(); return; }
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "a value the await itself produced is fresh", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        [return: ProvidesCover]
        static Task<Critter> Find() { return Task.FromResult<Critter>(null); }

        public static async Task Run()
        {
            Critter found = await Find();
            Needs(found);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            // A covered collection covers what is taken out of it -- the acquisition reached the elements too.
            Check(failures, "an element read by index carries the collection cover", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan(Map map)
        {
            List<Critter> found = Roster.Nearby(map);
            Critter other = found[0];
            Needs(other);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "a foreach variable carries it too", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan(Map map)
        {
            foreach (Critter other in Roster.Nearby(map)) { Needs(other); }
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "a collection parameter declaring it passes it on", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan([ProvidesCover] List<Critter> critters)
        {
            Needs(critters[0]);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "an element of an undeclared collection is still reported", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan(List<Critter> critters)
        {
            Needs(critters[0]);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}", "FOSYNC002");

            Check(failures, "static map data needs no cover", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(StaticItem item) { item.GetCount(); }
    }
}");

            Check(failures, "the same call on a mutable item still needs cover", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Item item) { item.GetCount(); }
    }
}", "FOSYNC002");

            Check(failures, "annotating always-covered data is legal -- it is still an entity", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] StaticItem item) { }
    }
}");

            // The reason the exemption belongs to the value and not to the type system: a prototype derives
            // from the concrete entity, so excluding it from "entity" would drop the contract on the upcast.
            Check(failures, "a prototype satisfies the obligation on its own", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(ProtoCritter proto) { Needs(proto); }
        static void Needs([RequiresCover] Critter cr) { }
    }
}");

            Check(failures, "the same parameter still demands cover for a live critter", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr) { Needs(cr); }
        static void Needs([RequiresCover] Critter cr) { }
    }
}", "FOSYNC002");

            Check(failures, "acquiring cover on always-covered data is legal and silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Takes(StaticItem item) { Sync.Lock(item); }
    }
}");

            Check(failures, "static map data as an argument needs no cover either", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Compares(StaticItem item) { AbstractItem.Compare(item); }
    }
}");

            Check(failures, "a mutable item in the same position still does", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Compares(Item item) { AbstractItem.Compare(item); }
    }
}", "FOSYNC002");

            // Exempting the receiver must not exempt the call.
            Check(failures, "an argument is still checked on a static-data receiver", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Gives(StaticItem item, Critter cr) { item.Give(cr); }
    }
}", "FOSYNC002");

            // Game carries the whole script surface, so the rule must be scoped to the acquisition methods.
            Check(failures, "static map data as an ordinary Game argument is not an acquisition", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reports(StaticItem item) { Game.Verify(false, ""Context"", item); }
    }
}");

            Check(failures, "a balanced singleton lock is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Work()
        {
            Game.Lock();
            Game.Unlock();
        }
    }
}");

            Check(failures, "a singleton lock left held is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Work()
        {
            Game.Lock();
        }
    }
}", "FOSYNC006");

            Check(failures, "returning before the release is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Work(bool skip)
        {
            Game.Lock();
            if (skip) { return; }
            Game.Unlock();
        }
    }
}", "FOSYNC006");

            Check(failures, "awaiting while the singleton lock is held is reported", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        public static async Task Work(Critter cr)
        {
            Game.Lock();
            await Sync.LockAsync(cr);
            Game.Unlock();
        }
    }
}", "FOSYNC007");

            Check(failures, "a declaring entry point is silent and discharges its callees", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [Event]
        public static void OnSomething([RequiresCover] Critter cr) { Reads(cr); }
    }
}");

            Check(failures, "only the first entity parameter of an entry point is required to declare", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [Event]
        public static void OnSomething([RequiresCover] Critter cr, Critter other) { }
    }
}");

            Check(failures, "a non-entry method needs no entry-point declaration", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Helper(Critter cr) { }
    }
}");

            Check(failures, "an uncovered receiver is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Caller(Critter cr) { cr.SendGroupInfo(); }
    }
}", "FOSYNC002");

            Check(failures, "a covered receiver is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Caller([RequiresCover] Critter cr) { cr.SendGroupInfo(); }
    }
}");

            Check(failures, "a receiver from a ProvidesCover source is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover]
        public static Critter Spawn() { return null!; }

        public static void Caller()
        {
            Critter spawned = Spawn();
            spawned.SendGroupInfo();
        }
    }
}");

            Check(failures, "a method without the attribute does not police its receiver", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Caller(Critter cr) { cr.Untracked(); }
    }
}");

            foreach (string failure in failures) {
                Console.Error.WriteLine("FAIL: " + failure);
            }

            Console.Out.WriteLine(failures.Count == 0
                ? "OK: analyzer self-tests passed"
                : $"FAILED: {failures.Count} analyzer self-test case(s)");

            return failures.Count == 0 ? 0 : 1;
        }

        private static void Check(List<string> failures, string name, string snippet, params string[] expected)
        {
            ImmutableArray<Diagnostic> reported = Run(Preamble + snippet);
            string[] actual = reported.Select(d => d.Id).OrderBy(id => id, StringComparer.Ordinal).ToArray();
            string[] wanted = expected.OrderBy(id => id, StringComparer.Ordinal).ToArray();

            if (!actual.SequenceEqual(wanted)) {
                failures.Add($"{name}: expected [{string.Join(", ", wanted)}] but got [{string.Join(", ", actual)}]");
            }
        }

        private static ImmutableArray<Diagnostic> Run(string source)
        {
            var references = ((string?)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") ?? string.Empty)
                .Split(System.IO.Path.PathSeparator)
                .Where(path => path.Length != 0)
                .Select(path => (MetadataReference)MetadataReference.CreateFromFile(path))
                .ToList();

            CSharpCompilation compilation = CSharpCompilation.Create(
                "AnalyzerSelfTest",
                new[] {CSharpSyntaxTree.ParseText(source)},
                references,
                new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

            // A snippet that does not compile would make a diagnostic expectation meaningless.
            ImmutableArray<Diagnostic> compileErrors = compilation.GetDiagnostics()
                .Where(d => d.Severity == DiagnosticSeverity.Error)
                .ToImmutableArray();

            if (!compileErrors.IsEmpty) {
                throw new InvalidOperationException(
                    "analyzer self-test snippet does not compile: " + compileErrors[0]);
            }

            CompilationWithAnalyzers withAnalyzers = compilation.WithAnalyzers(
                ImmutableArray.Create<DiagnosticAnalyzer>(new SyncCoverAnalyzer()));

            return withAnalyzers.GetAnalyzerDiagnosticsAsync().GetAwaiter().GetResult();
        }
    }
}
