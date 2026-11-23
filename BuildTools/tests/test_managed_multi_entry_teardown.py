"""Exercise native backend teardown with two independently loaded entry images."""

import os
from pathlib import Path
import shutil
import subprocess
from xml.sax.saxutils import escape

import pytest


ENGINE = Path(__file__).resolve().parents[2]
HOST_SOURCE = ENGINE / "Source/Scripting/Managed/ManagedHost/ManagedLoadContextHost.cs"
TRACKER_SOURCE = ENGINE / "Source/Scripting/Managed/CoreScripts/EntityWrapperTracker.cs"
ENTRY_SOURCE = r"""
namespace FOnline;
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;

internal static class Native
{
    private static bool Began;
    private static IntPtr BoundBackend;
    internal static bool IsBackendAlive => BoundBackend != IntPtr.Zero;
    internal static void BindBackend(IntPtr backend) { BoundBackend = backend; }
    internal static void UnbindBackend() { BoundBackend = IntPtr.Zero; FixtureTrace.Record("unbind"); }
    [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void AddRefEntity(IntPtr entity);
    [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void ReleaseEntity(IntPtr entity);
    internal static string GetEntityName(IntPtr entity) => "fixture";
    internal static long GetEntityId(IntPtr entity) => 1;
    internal static void PumpContinuations() { }
    internal static long GetSnapshotWorkCount() => 0;
    internal static void ShutdownContinuations()
    {
        if (!Began) throw new InvalidOperationException("Entry teardown was not announced");
        if (Environment.GetEnvironmentVariable("FO_TEST_MANAGED_MULTI_ENTRY_FAIL_BEGIN") != null) {
            string marker = Environment.GetEnvironmentVariable("FO_TEST_MANAGED_MULTI_ENTRY_MARKER") ?? throw new InvalidOperationException();
            File.AppendAllText(marker, "unexpected continuation shutdown\n");
        }
    }
    internal static void BeginBackendTeardown()
    {
        string name = typeof(Native).Assembly.GetName().Name ?? throw new InvalidOperationException();
        if (name == "FixtureB.Server" && Environment.GetEnvironmentVariable("FO_TEST_MANAGED_MULTI_ENTRY_FAIL_BEGIN") != null) {
            throw new InvalidOperationException("Injected teardown failure");
        }
        if (name == "FixtureB.Server" && Environment.GetEnvironmentVariable("FO_TEST_MANAGED_MULTI_ENTRY_SKIP_B") != null) {
            Began = true;
            return;
        }
        string marker = Environment.GetEnvironmentVariable("FO_TEST_MANAGED_MULTI_ENTRY_MARKER") ?? throw new InvalidOperationException();
        File.AppendAllText(marker, name + "\n");
        Began = true;
    }
}
internal static class NativeRefTypeLifetime
{
    internal static void Drain() { }
    internal static void Shutdown() { }
}
internal static class EntityWrapperTracker
{
    internal static int CollectAndWaitForFinalizers(int limit)
    {
        FixtureTrace.Record("collect");
        return 0;
    }
    internal static string DumpOutstandingWrappers() => string.Empty;
    internal static void StopFinalizerReleases() { FixtureOwnership.Stop(); FixtureTrace.Record("stop"); }
    internal static long CloseAndReleaseReferences()
    {
        if (Native.IsBackendAlive) throw new InvalidOperationException("A bound backend cannot release entity references");
        long released = FixtureOwnership.Close();
        FixtureTrace.Record("release");
        return released;
    }
}
internal static class FixtureOwnership
{
    private static readonly EntityReferenceRegistry References = new();
    internal static void Acquire(IntPtr pointer) { References.Acquire(pointer); }
    internal static void Stop() { References.StopFinalizerReleases(); }
    internal static long Close() => References.Close();
}
internal static class ScriptStaticCleanup
{
    internal static string ClearScriptStatics()
    {
        FixtureTrace.Record("clear");
        return string.Empty;
    }
}
internal static class FixtureTrace
{
    internal static void Record(string phase)
    {
        string path = Environment.GetEnvironmentVariable("FO_TEST_MANAGED_MULTI_ENTRY_PHASES") ?? throw new InvalidOperationException();
        string name = typeof(Native).Assembly.GetName().Name ?? throw new InvalidOperationException();
        File.AppendAllText(path, phase + ":" + name + "\n");
    }
}
internal static class Initializator
{
    internal static void InitializeEarly()
    {
        string? name = typeof(Native).Assembly.GetName().Name;
        if (name == "FixtureB.Server" && Environment.GetEnvironmentVariable("FO_TEST_MANAGED_MULTI_ENTRY_FAIL_STARTUP") != null) {
            throw new InvalidOperationException("Injected partial startup failure");
        }
    }
}
"""


def build_fixture(dotnet, directory, name, source):
    directory.mkdir(parents=True)
    project = f"""<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework><AssemblyName>{escape(name)}</AssemblyName>
    <Nullable>enable</Nullable><EnableDefaultCompileItems>false</EnableDefaultCompileItems>
    <UseSharedCompilation>false</UseSharedCompilation>
  </PropertyGroup>
  <ItemGroup><Compile Include="{escape(str(source))}" /></ItemGroup>
</Project>
"""
    (directory / "Fixture.csproj").write_text(project, encoding="utf-8")
    result = subprocess.run([dotnet, "build", "Fixture.csproj", "-c", "Release", "--nologo", "-v:q", "-m:1", "-nr:false"],
                            cwd=directory, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return directory / "bin/Release/net10.0" / f"{name}.dll"


@pytest.mark.parametrize("scenario", ["normal", "partial", "negative", "fail_teardown", "ownership", "untracked"])
def test_native_multi_entry_teardown(tmp_path, scenario):
    dotnet = shutil.which("dotnet")
    unit_binary = os.environ.get("FO_TEST_UNIT_BINARY")
    if dotnet is None or not unit_binary:
        pytest.skip("dotnet and an explicit FO_TEST_UNIT_BINARY are required")

    fixture_root = tmp_path / "bake"
    assembly_dir = fixture_root / "FixturePack/Assemblies/Assemblies-server"
    assembly_dir.mkdir(parents=True)
    entry_source = tmp_path / "Entry.cs"
    tracker = TRACKER_SOURCE.read_text(encoding="utf-8")
    registry_start = tracker.index("internal sealed class EntityReferenceRegistry")
    registry_end = tracker.index("\n// Tracks wrapper diagnostics", registry_start)
    entry_source.write_text(ENTRY_SOURCE + "\n" + tracker[registry_start:registry_end], encoding="utf-8")

    for name in ("FixtureA.Server", "FixtureB.Server"):
        shutil.copyfile(build_fixture(dotnet, tmp_path / name, name, entry_source), assembly_dir / f"{name}.dll")
    shutil.copyfile(build_fixture(dotnet, tmp_path / "host", "FOnline.ManagedHost", HOST_SOURCE),
                    assembly_dir / "FOnline.ManagedHost.dll")

    marker = tmp_path / "teardown.txt"
    phases = tmp_path / "phases.txt"
    env = {**os.environ, "FO_TEST_MANAGED_MULTI_ENTRY_ROOT": str(fixture_root),
           "FO_TEST_MANAGED_MULTI_ENTRY_MARKER": str(marker), "FO_TEST_MANAGED_MULTI_ENTRY_PHASES": str(phases)}
    env.pop("FO_TEST_MANAGED_MULTI_ENTRY_FAIL_STARTUP", None)
    env.pop("FO_TEST_MANAGED_MULTI_ENTRY_SKIP_B", None)
    env.pop("FO_TEST_MANAGED_MULTI_ENTRY_FAIL_BEGIN", None)
    env.pop("FO_TEST_MANAGED_MULTI_ENTRY_UNTRACKED", None)
    if scenario == "partial":
        env["FO_TEST_MANAGED_MULTI_ENTRY_FAIL_STARTUP"] = "1"
    if scenario == "negative":
        env["FO_TEST_MANAGED_MULTI_ENTRY_SKIP_B"] = "1"
    if scenario == "fail_teardown":
        env["FO_TEST_MANAGED_MULTI_ENTRY_FAIL_BEGIN"] = "1"
    if scenario == "untracked":
        env["FO_TEST_MANAGED_MULTI_ENTRY_UNTRACKED"] = "1"

    test_name = ("Managed backend returns every entry assembly entity reference" if scenario in ("ownership", "untracked")
                 else "Managed backend begins teardown in every entry assembly")
    result = subprocess.run([unit_binary, test_name],
                            env=env, capture_output=True, text=True, timeout=120)
    if scenario in ("negative", "fail_teardown"):
        assert result.returncode != 0, result.stdout + result.stderr
        assert marker.exists() and "FixtureA.Server" in marker.read_text(encoding="utf-8")
        assert "FixtureB.Server" not in marker.read_text(encoding="utf-8")
        if scenario == "fail_teardown":
            assert "unexpected continuation shutdown" not in marker.read_text(encoding="utf-8")
            assert "Managed backend teardown did not complete" in result.stdout + result.stderr
        else:
            assert "FAILED:" in result.stdout + result.stderr
            assert "FixtureB.Server" in result.stdout + result.stderr
    elif scenario in ("ownership", "untracked"):
        assert result.returncode == 0, result.stdout + result.stderr
        assert phases.read_text(encoding="utf-8").splitlines() == [
            "stop:FixtureA.Server", "stop:FixtureB.Server",
            "clear:FixtureA.Server", "clear:FixtureB.Server", "collect:FixtureA.Server", "collect:FixtureB.Server",
            "unbind:FixtureA.Server", "unbind:FixtureB.Server", "release:FixtureA.Server", "release:FixtureB.Server"]
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        assert sorted(marker.read_text(encoding="utf-8").splitlines()) == ["FixtureA.Server", "FixtureB.Server"]
        assert phases.read_text(encoding="utf-8").splitlines() == [
            "stop:FixtureA.Server", "stop:FixtureB.Server",
            "clear:FixtureA.Server", "clear:FixtureB.Server", "collect:FixtureA.Server", "collect:FixtureB.Server",
            "unbind:FixtureA.Server", "unbind:FixtureB.Server", "release:FixtureA.Server", "release:FixtureB.Server"]
