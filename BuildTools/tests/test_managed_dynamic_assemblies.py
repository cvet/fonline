"""Load assemblies compiled after the bake through the production host and run their entries through CoreScripts."""

from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
MANAGED = ENGINE / "Source/Scripting/Managed"
CORE = MANAGED / "CoreScripts"
CORE_FILES = (
    "Native.cs", "ScriptFunc.cs", "ScriptExceptions.cs", "Enums.cs", "Attributes.cs", "Invariant.cs", "ScriptTask.cs",
    "ScriptSynchronizationContext.cs", "ScriptEntryNames.cs", "Initializator.cs", "DynamicAssemblies.cs",
    "ScriptStaticCleanup.cs", "EntityWrapperTracker.cs",
)
PROBE_PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><OutputType>Exe</OutputType><Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings><TreatWarningsAsErrors>true</TreatWarningsAsErrors><NoWarn>$(NoWarn);CS8981</NoWarn></PropertyGroup>
</Project>
"""
FRAGMENT_PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><AssemblyName>{name}</AssemblyName><Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings><DebugType>portable</DebugType></PropertyGroup>
  <ItemGroup>{references}</ItemGroup>
</Project>
"""
REFERENCE = '<Reference Include="{name}"><HintPath>{path}</HintPath><Private>false</Private></Reference>'

PROBE_SOURCE = r"""
using System;
using System.IO;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Threading.Tasks;
using FOnline;
using FOnline.ManagedHost;

public static class Shared
{
    public static int Value;
    public static int Twice(int value) => value * 2;
}

internal static class Program
{
    private static object? Scope;
    private static string FragmentsDir = "";

    private static int Main(string[] args)
    {
        FragmentsDir = args[1];
        Scope = ManagedLoadContextHost.CreateLoadScope("probe", Array.Empty<string>(), Array.Empty<string>());

        try {
            return Run(args[0]).GetAwaiter().GetResult();
        }
        catch (Exception ex) {
            Console.WriteLine("FAILED " + ex);
            return 1;
        }
    }

    // Stands in for the native internal call: reads the name from the image and hands the image to the host
    internal static Assembly? LoadThroughHost(byte[] image, byte[]? symbols, out string? error)
    {
        error = null;

        try {
            using PEReader reader = new PEReader(new MemoryStream(image));
            MetadataReader metadata = reader.GetMetadataReader();
            string name = metadata.GetString(metadata.GetAssemblyDefinition().Name);
            return ManagedLoadContextHost.LoadDynamicAssembly(Scope ?? throw new InvalidOperationException("No scope"), name, image, symbols);
        }
        catch (Exception ex) {
            error = ex.Message;
            return null;
        }
    }

    private static Assembly Load(string name, bool symbols = true)
    {
        string path = Path.Combine(FragmentsDir, name + ".dll");
        byte[]? pdb = symbols ? File.ReadAllBytes(Path.ChangeExtension(path, ".pdb")) : null;
        return DynamicAssemblies.Load(File.ReadAllBytes(path), pdb);
    }

    private static MethodInfo Method(Assembly assembly, string type, string name)
    {
        return (assembly.GetType(type) ?? throw new InvalidOperationException("Missing type " + type)).GetMethod(name) ??
               throw new InvalidOperationException("Missing method " + name);
    }

    private static async Task<string> Failure(MethodInfo method)
    {
        try {
            await DynamicAssemblies.RunEntryAsync(method);
            return "none";
        }
        catch (Exception ex) {
            return ex.GetType().Name + ":" + ex.Message.Split(':')[0];
        }
    }

    private static string LoadFailure(string name, string directory)
    {
        try {
            string path = Path.Combine(directory, name + ".dll");
            DynamicAssemblies.Load(File.ReadAllBytes(path));
            return "none";
        }
        catch (Exception ex) {
            return ex.GetType().Name + ":" + ex.Message;
        }
    }

    private static async Task<int> Run(string kind)
    {
        if (kind == "run") {
            Assembly first = Load("FOnline.Dynamic.First");
            object? answer = await DynamicAssemblies.RunEntryAsync(Method(first, "Fragment", "Answer"));
            object? asyncAnswer = await DynamicAssemblies.RunEntryAsync(Method(first, "Fragment", "AsyncAnswer"));
            object? plain = await DynamicAssemblies.RunEntryAsync(Method(first, "Fragment", "AsyncPlain"));
            string thrown = await Failure(Method(first, "Fragment", "Throws"));
            string asyncThrown = await Failure(Method(first, "Fragment", "AsyncThrows"));
            string asyncVoid = await Failure(Method(first, "Fragment", "AsyncVoid"));
            string parameter = await Failure(Method(first, "Fragment", "WithParameter"));
            string name = ScriptEntryNames.Describe((Action)new DynamicAssemblies.EntryRun(Method(first, "Fragment", "Answer")).Run);
            Console.WriteLine("RUN answer=" + answer + " async=" + asyncAnswer + " plain=" + (plain ?? "null") + " shared=" + Shared.Value);
            Console.WriteLine("FAILURES thrown=" + thrown + " asyncThrown=" + asyncThrown + " asyncVoid=" + asyncVoid + " parameter=" + parameter);
            Console.WriteLine("NAME " + name);
            return 0;
        }
        if (kind == "chain") {
            Load("FOnline.Dynamic.First");
            Assembly second = Load("FOnline.Dynamic.Second");
            Console.WriteLine("CHAIN " + await DynamicAssemblies.RunEntryAsync(Method(second, "Chained", "Answer")));
            return 0;
        }
        if (kind == "rejections") {
            Load("FOnline.Dynamic.First", false);
            Console.WriteLine("DUPLICATE " + LoadFailure("FOnline.Dynamic.First", Path.Combine(FragmentsDir, "duplicate")));
            Console.WriteLine("UNPREFIXED " + LoadFailure("Unprefixed", FragmentsDir));
            return 0;
        }
        if (kind == "cleanup") {
            Assembly first = Load("FOnline.Dynamic.First");
            await DynamicAssemblies.RunEntryAsync(Method(first, "Fragment", "Hold"));
            FieldInfo held = first.GetType("Fragment")?.GetField("Held") ?? throw new InvalidOperationException("Missing field");
            bool heldBefore = held.GetValue(null) != null;
            ScriptStaticCleanup.ClearScriptStatics();
            Console.WriteLine("CLEANUP before=" + heldBefore + " after=" + (held.GetValue(null) != null));
            return 0;
        }
        if (kind == "version") {
            Console.WriteLine("VERSION " + (DynamicAssemblies.ScriptsVersionId == typeof(Program).Assembly.ManifestModule.ModuleVersionId));
            return 0;
        }

        return 2;
    }
}

namespace FOnline
{
    public class Entity { }
    public readonly struct timespan { public timespan(long value) { } }
    public static partial class Game
    {
        public static void StartTimeEvent(timespan duration, Action callback) { callback(); }
    }
    public static class ScriptFuncRegistration { public static void RegisterEngineAttributeFuncs() { } }
    public static class RemoteCallScriptFuncs { public static void RegisterRemoteCalls() { } }

    public enum EventResult { ContinueChain, StopChain }
    public readonly struct hstring
    {
        private readonly string value;
        private hstring(string value) { this.value = value; }
        public static hstring FromString(string value) => new(value);
        public override string ToString() => value;
    }
}
"""

FIRST_SOURCE = r"""
using System;
using System.Threading.Tasks;

public static class Fragment
{
    public static object? Held;

    public static int Answer() => 42;

    public static async Task<int> AsyncAnswer()
    {
        await Task.Yield();
        return Shared.Twice(21) + 1;
    }

    public static async Task AsyncPlain()
    {
        await Task.Yield();
        Shared.Value = 9;
    }

    public static void Throws() => throw new InvalidOperationException("fragment-failure");

    public static async Task AsyncThrows()
    {
        await Task.Yield();
        throw new InvalidOperationException("fragment-async-failure");
    }

    public static async void AsyncVoid() => await Task.Yield();

    public static int WithParameter(int value) => value;

    public static void Hold() => Held = new object();
}
"""

SECOND_SOURCE = r"""
public static class Chained
{
    public static int Answer() => Fragment.Answer() + 1;
}
"""

STANDALONE_SOURCE = r"""
public static class Standalone
{
    public static int Answer() => 1;
}
"""


# Matched token by token, so a declaration the formatter wrapped over several lines still matches
def replace_internal_call(source, declaration, replacement):
    tokens = r"\s+".join(re.escape(token) for token in declaration.split())
    pattern = re.compile(
        r"(?m)^(?P<indent>[ \t]*)\[MethodImpl\(MethodImplOptions\.InternalCall\)\]\r?\n"
        + r"(?P=indent)" + tokens + r"\r?$"
    )
    source, count = pattern.subn(lambda match: match.group("indent") + replacement, source)
    assert count == 1
    return source


def build(dotnet, project_dir, output_dir=None):
    command = [dotnet, "build", str(project_dir), "-c", "Release", "--nologo", "-v:minimal"]
    if output_dir is not None:
        command += ["-o", str(output_dir)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, result.stdout + result.stderr


def write_fragment(root, name, source, references):
    project_dir = root / name
    project_dir.mkdir(parents=True)
    reference_items = "".join(REFERENCE.format(name=Path(path).stem, path=path) for path in references)
    (project_dir / "Fragment.csproj").write_text(FRAGMENT_PROJECT.format(name=name, references=reference_items), encoding="utf-8")
    (project_dir / "Fragment.cs").write_text(source, encoding="utf-8")
    return project_dir


@pytest.fixture(scope="module")
def dynamic_probe(tmp_path_factory):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for dynamic assembly tests")

    root = tmp_path_factory.mktemp("managed-dynamic-assemblies")
    probe_dir = root / "probe"
    probe_dir.mkdir()

    for name in CORE_FILES:
        source = (CORE / name).read_text(encoding="utf-8")
        if name == "Native.cs":
            source = replace_internal_call(source, "internal static extern void Log(string text);",
                                           'internal static void Log(string text) => Console.WriteLine("ENGINE_RECORDED " + text);')
            source = replace_internal_call(source, "private static extern void ReportExceptionInternal(string summary, string? nativeError, long[] frames);",
                                           'private static void ReportExceptionInternal(string summary, string? nativeError, long[] frames) => Log(summary);')
            source = replace_internal_call(source, "private static extern string? RunScriptContinuationInternal(IntPtr backend, Action continuation);",
                                           "private static string? RunScriptContinuationInternal(IntPtr backend, Action continuation) { continuation(); return null; }")
            source = replace_internal_call(source, "private static extern Assembly? LoadDynamicAssemblyInternal(IntPtr backend, byte[] image, byte[]? symbols, out string? error);",
                                           "private static Assembly? LoadDynamicAssemblyInternal(IntPtr backend, byte[] image, byte[]? symbols, out string? error) => Program.LoadThroughHost(image, symbols, out error);")
        (probe_dir / name).write_text(source, encoding="utf-8")

    shutil.copyfile(MANAGED / "ManagedHost/ManagedLoadContextHost.cs", probe_dir / "ManagedLoadContextHost.cs")
    (probe_dir / "Probe.csproj").write_text(PROBE_PROJECT, encoding="utf-8")
    (probe_dir / "Program.cs").write_text(PROBE_SOURCE, encoding="utf-8")
    build(dotnet, probe_dir)
    probe = probe_dir / "bin/Release/net10.0/Probe.dll"

    fragments = root / "fragments"
    build(dotnet, write_fragment(root / "src", "FOnline.Dynamic.First", FIRST_SOURCE, [probe]), fragments)
    first = fragments / "FOnline.Dynamic.First.dll"
    build(dotnet, write_fragment(root / "src", "FOnline.Dynamic.Second", SECOND_SOURCE, [probe, first]), fragments)
    build(dotnet, write_fragment(root / "src", "Unprefixed", STANDALONE_SOURCE, []), fragments)
    build(dotnet, write_fragment(root / "src-duplicate", "FOnline.Dynamic.First", STANDALONE_SOURCE, []), fragments / "duplicate")
    return dotnet, probe, fragments


def run_probe(dynamic_probe, kind):
    dotnet, probe, fragments = dynamic_probe
    result = subprocess.run([dotnet, str(probe), kind, str(fragments)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def test_entries_run_with_their_results_and_exceptions(dynamic_probe):
    output = run_probe(dynamic_probe, "run")
    assert "RUN answer=42 async=43 plain=null shared=9" in output
    assert ("FAILURES thrown=InvalidOperationException:fragment-failure "
            "asyncThrown=InvalidOperationException:fragment-async-failure "
            "asyncVoid=ArgumentException:Async void script methods are not supported; return Task "
            "parameter=ArgumentException:Script entry must be a static method without parameters") in output
    assert "NAME Fragment::Answer" in output


def test_dynamic_assembly_references_an_earlier_one(dynamic_probe):
    assert "CHAIN 43" in run_probe(dynamic_probe, "chain")


def test_taken_and_unprefixed_names_are_refused(dynamic_probe):
    output = run_probe(dynamic_probe, "rejections")
    assert "DUPLICATE NativeCallException:Duplicate managed assembly name: FOnline.Dynamic.First" in output
    assert "UNPREFIXED NativeCallException:Dynamic managed assembly name must start with FOnline.Dynamic.: Unprefixed" in output


def test_static_cleanup_reaches_dynamic_assemblies(dynamic_probe):
    assert "CLEANUP before=True after=False" in run_probe(dynamic_probe, "cleanup")


def test_scripts_version_names_the_entry_assembly(dynamic_probe):
    assert "VERSION True" in run_probe(dynamic_probe, "version")
