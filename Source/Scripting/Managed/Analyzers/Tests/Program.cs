#nullable enable

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Threading.Tasks;
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
        // The analyzer resolves both the attribute and the engine-owned FOnline.Sync type by metadata name, so a
        // small stand-in is enough and keeps the tests independent of the real CoreScripts build.
        private const string Preamble = @"
namespace FOnline
{
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class RequiresCoverAttribute : System.Attribute
    {
        public RequiresCoverAttribute(params string[] entities) { Entities = entities; }
        public string[] Entities { get; private set; }
    }

    public class Entity { }
    public class Critter : Entity { }
    public class Map : Entity { }

    public static class Sync
    {
        public static bool Lock(Entity entity) { return true; }
        public static bool Widen(Entity entity) { return true; }
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

            Check(failures, "well-formed contract is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(nameof(cr))]
        public static void Reads(Critter cr) { }
    }
}");

            Check(failures, "unknown entity name is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(""critter"")]
        public static void Reads(Critter cr) { }
    }
}", "FOSYNC001");

            Check(failures, "dotted path resolves through its root parameter", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(""cr.map"")]
        public static void Reads(Critter cr) { }
    }
}");

            Check(failures, "dotted path with an unknown root is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(""npc.map"")]
        public static void Reads(Critter cr) { }
    }
}", "FOSYNC001");

            Check(failures, "undischarged call is reported", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(nameof(cr))]
        public static void Reads(Critter cr) { }

        public static void Caller(Critter cr) { Reads(cr); }
    }
}", "FOSYNC002");

            Check(failures, "acquiring cover discharges the obligation", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(nameof(cr))]
        public static void Reads(Critter cr) { }

        public static void Caller(Critter cr)
        {
            Sync.Lock(cr);
            Reads(cr);
        }
    }
}");

            Check(failures, "a Widen family call also discharges", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(nameof(cr))]
        public static void Reads(Critter cr) { }

        public static void Caller(Critter cr)
        {
            Sync.WidenCritterWithMap(cr);
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
        [RequiresCover(nameof(cr))]
        public static void Reads(Critter cr) { }

        [RequiresCover(nameof(cr))]
        public static void Caller(Critter cr) { Reads(cr); }
    }
}");

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

            Check(failures, "a look-alike project type does not discharge", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [RequiresCover(nameof(cr))]
        public static void Reads(Critter cr) { }

        public static void Caller(Critter cr)
        {
            Sync2.Lock(cr);
            Reads(cr);
        }
    }
}", "FOSYNC002");

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
