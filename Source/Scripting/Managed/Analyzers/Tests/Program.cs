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

    [System.AttributeUsage(System.AttributeTargets.Parameter)]
    public sealed class RequiresCoverAttribute : System.Attribute
    {
        public RequiresCoverAttribute(CoverReach reach = CoverReach.None) { Reach = reach; }
        public CoverReach Reach { get; private set; }
    }

    [System.AttributeUsage(System.AttributeTargets.Parameter)]
    public sealed class SyncCoverAttribute : System.Attribute
    {
        public SyncCoverAttribute(CoverReach reach = CoverReach.None) { Reach = reach; }
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

    public class Entity { }
    public class Critter : Entity { }
    public class Map : Entity { }

    public static class Sync
    {
        public static bool Lock(Entity entity) { return true; }
        public static bool WidenCritterWithMap(Critter cr) { return true; }
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

            Check(failures, "an entry point receives covered arguments", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [Event]
        public static void OnSomething([SyncCover(CoverReach.Parent)] Critter cr) { Reads(cr); }
    }
}");

            Check(failures, "an entry point without SyncCover on the argument is still reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [Event]
        public static void OnSomething(Critter cr) { Reads(cr); }
    }
}", "FOSYNC002");

            Check(failures, "SyncCover on a non-entity is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [Event]
        public static void OnSomething([SyncCover] int hp) { }
    }
}", "FOSYNC001");

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

            Check(failures, "an entry point locking its own parameter is told to declare it", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [Event]
        public static void OnSomething(Critter cr) { Sync.Lock(cr); }
    }
}", "FOSYNC003");

            Check(failures, "a declared SyncCover parameter is not told twice", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [Event]
        public static void OnSomething([SyncCover] Critter cr) { Sync.Lock(cr); }
    }
}");

            Check(failures, "a non-entry method locking its own parameter is fine", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Helper(Critter cr) { Sync.Lock(cr); }
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
