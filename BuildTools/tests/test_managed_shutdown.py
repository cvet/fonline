"""Exercise the production static cleanup and wrapper tracker in isolated managed processes."""

from pathlib import Path
import os
import shutil
import subprocess

import pytest


CORE = Path(__file__).resolve().parents[2] / "Source/Scripting/Managed/CoreScripts"
PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework><OutputType>Exe</OutputType>
    <Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings>
    <TreatWarningsAsErrors>true</TreatWarningsAsErrors>
  </PropertyGroup>
</Project>
"""
PROBE = r"""
namespace FOnline;

using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading;

internal sealed class CallableByEngineAttribute : Attribute { }

internal class Entity
{
    private readonly long TrackerId;
    internal Entity() { TrackerId = EntityWrapperTracker.Register(this, new IntPtr(1)); }
    ~Entity()
    {
        Native.Released = true;
        EntityWrapperTracker.Unregister(TrackerId);
    }
}

internal static class Native
{
    internal static bool CollectDuringDescription;
    internal static bool Released;

    internal static string GetEntityName(IntPtr entity)
    {
        if (CollectDuringDescription) {
            Fixtures.State.Held = null;
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }

        return "Probe";
    }

    internal static int GetEntityId(IntPtr entity)
    {
        if (Released) {
            throw new InvalidOperationException("Report dereferenced an entity after its final release");
        }

        return 1;
    }
}

internal static class Program
{
    private static readonly ManualResetEventSlim FinalizerStarted = new();
    private static readonly ManualResetEventSlim ReleaseFinalizer = new();

    private sealed class BlockingFinalizer
    {
        ~BlockingFinalizer()
        {
            FinalizerStarted.Set();
            ReleaseFinalizer.Wait();
        }
    }

    private static object? Invoke(Type type, string name, params object[] args)
    {
        return (type.GetMethod(name, BindingFlags.Static | BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("Missing method " + name)).Invoke(null, args);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Seed() { Fixtures.State.Held = new Entity(); }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void SeedBlocker() { _ = new BlockingFinalizer(); }

    private static int Main(string[] args)
    {
        if (args[0] == "blocked") {
            SeedBlocker();
            GC.Collect();

            if (!FinalizerStarted.Wait(5000)) {
                throw new InvalidOperationException("Finalizer did not start");
            }

            try {
                Invoke(typeof(EntityWrapperTracker), "CollectAndWaitForFinalizers", 8);
                throw new InvalidOperationException("Blocked finalizer wait was reported as success");
            }
            catch (TargetInvocationException ex) when (ex.InnerException is TimeoutException) {
                Console.WriteLine("Finalizer timeout reported");
            }
            finally {
                ReleaseFinalizer.Set();
            }

            GC.WaitForPendingFinalizers();
            return 0;
        }

        if (args[0] == "deep" || args[0] == "report-lifetime") {
            Invoke(typeof(EntityWrapperTracker), "EnableDeepWrapperTracking");
        }

        Seed();

        if (args[0] == "report-lifetime") {
            Native.CollectDuringDescription = true;
            Console.WriteLine(Invoke(typeof(EntityWrapperTracker), "DumpOutstandingWrappers"));
        }

        string report = (string)(Invoke(typeof(ScriptStaticCleanup), "ClearScriptStatics")
            ?? throw new InvalidOperationException("Missing cleanup report"));
        Console.WriteLine(report);
        Fixtures.State.Check(args[0]);
        int live = (int)(Invoke(typeof(EntityWrapperTracker), "CollectAndWaitForFinalizers", 8)
            ?? throw new InvalidOperationException("Missing wrapper count"));

        if (live != 0) {
            throw new InvalidOperationException("Wrapper is still rooted after cleanup: " + live);
        }

        return 0;
    }
}
"""
FIXTURES = r"""
namespace Fixtures;

using System;
using System.Collections.Generic;
using FOnline;

internal static class State
{
    internal static Entity? Held;
    private static readonly object?[] Array = { new object() };
    private static readonly object?[,] Matrix = { { new object() } };
    private static readonly Queue<object> Queue = new(new[] { new object() });
    private static readonly Stack<object> Stack = new(new[] { new object() });
    private static readonly List<object> List = new() { new object() };
    private static readonly HashSet<object> Set = new() { new object() };
    private static readonly Dictionary<int, object> Dictionary = new() { { 1, new object() } };
    private static readonly Trap Unrelated = new();

    private sealed class Trap
    {
        internal bool Called;
        public void Clear() { Called = true; }
    }

    internal static void Check(string kind)
    {
        if (Held != null || List.Count != 0 || Set.Count != 0 || Dictionary.Count != 0 || Unrelated.Called) {
            throw new InvalidOperationException("Ordinary static cleanup failed or invoked an unrelated Clear");
        }

        if (kind == "array" && (Array[0] != null || Matrix[0, 0] != null)) {
            throw new InvalidOperationException("Readonly arrays still hold references");
        }

        if (kind == "queue" && (Queue.Count != 0 || Stack.Count != 0)) {
            throw new InvalidOperationException("Readonly queue/stack still hold references");
        }
    }
}
"""


@pytest.fixture(scope="module")
def shutdown_probe(tmp_path_factory):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for managed shutdown tests")
    output = tmp_path_factory.mktemp("managed-shutdown")
    for name in ("ScriptStaticCleanup.cs", "EntityWrapperTracker.cs"):
        shutil.copyfile(CORE / name, output / name)
    (output / "Probe.csproj").write_text(PROJECT, encoding="utf-8")
    (output / "Program.cs").write_text(PROBE, encoding="utf-8")
    (output / "Fixtures.cs").write_text(FIXTURES, encoding="utf-8")
    result = subprocess.run([dotnet, "build", "Probe.csproj", "-c", "Release", "--nologo", "-v:q"],
                            cwd=output, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return dotnet, output / "bin/Release/net10.0/Probe.dll"


@pytest.mark.parametrize("kind", ["ordinary", "deep", "array", "queue", "blocked", "report-lifetime"])
def test_managed_shutdown(shutdown_probe, kind):
    dotnet, assembly = shutdown_probe
    # Tier-zero JIT keeps dead locals alive and hides an absent KeepAlive during the diagnostic lookup
    env = {**os.environ, "DOTNET_TieredCompilation": "0"}
    result = subprocess.run([dotnet, str(assembly), kind], env=env, capture_output=True, text=True, timeout=12)
    assert result.returncode == 0, result.stdout + result.stderr
