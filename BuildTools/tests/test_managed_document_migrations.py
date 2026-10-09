from pathlib import Path
import os
import shutil
import subprocess

import pytest


def test_document_migrator_result_lifetime_and_culture(tmp_path: Path):
    dotnet = shutil.which('dotnet')
    if not dotnet:
        pytest.skip('dotnet SDK is required for managed document migration tests')
    core = Path(__file__).resolve().parents[2] / 'Source/Scripting/Managed/CoreScripts'
    for name in ['DatabaseDocument.cs', 'Native.DatabaseMigrations.cs']:
        shutil.copy2(core / name, tmp_path / name)
    (tmp_path / 'Probe.csproj').write_text('''<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net10.0</TargetFramework>
    <LangVersion>latest</LangVersion><Nullable>enable</Nullable><UseSharedCompilation>false</UseSharedCompilation>
    <TreatWarningsAsErrors>true</TreatWarningsAsErrors><NoWarn>CS8981</NoWarn></PropertyGroup>
</Project>''', encoding='utf-8')
    (tmp_path / 'Probe.cs').write_text('''using System;
using System.Collections.Generic;
using System.Globalization;
using FOnline;

namespace FOnline {
    [AttributeUsage(AttributeTargets.Method)] public sealed class CallableByEngineAttribute : Attribute {}
    public readonly struct hstring { public hstring(string value) { Value = value; } public string Value { get; } }
    internal static class Initializator {}
    internal static partial class Native {
        static partial void ClearPropertyMigrators();
        internal static IntPtr BoundBackend => IntPtr.Zero;
        internal static void ThrowNativeError(string? error) { if (error != null) throw new Exception(error); }
    }
}

internal static class Probe {
    private static DatabaseDocument? Retained;
    private static bool Clear(ref string? value, DatabaseDocument document) { Retained = document; value = null; return true; }
    private static bool Keep(ref string? value, DatabaseDocument document) { value = "discarded"; return false; }
    private static bool Reject(ref string? value, DatabaseDocument document) { Retained = document; throw new InvalidOperationException("rejected"); }
    private static void Expect(bool value) { if (!value) throw new Exception("document migration regression"); }
    private static void ExpectClosed() {
        try { Retained!.Read<string>("Sibling"); throw new Exception("retained context still active"); }
        catch (InvalidOperationException) {}
    }
    private static void Main() {
        object? output = Native.InvokePropertyMigrator(Native.CreateMigratorAdapter<string?>(Clear), "original", IntPtr.Zero, "Critter", out bool changed);
        Expect(changed && output == null); ExpectClosed();
        output = Native.InvokePropertyMigrator(Native.CreateMigratorAdapter<string?>(Keep), "original", IntPtr.Zero, "Critter", out changed);
        Expect(!changed && output == null);
        try { Native.InvokePropertyMigrator(Native.CreateMigratorAdapter<string?>(Reject), "original", IntPtr.Zero, "Critter", out changed); throw new Exception("exception swallowed"); }
        catch (InvalidOperationException error) when (error.Message == "rejected") {}
        ExpectClosed();
        CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("fr-FR");
        var owned = new DatabaseDocument("Critter", new Dictionary<string, object> { ["Number"] = "1.25", ["Identity"] = "Original" });
        Expect(owned.Read<double>("Number") == 1.25);
        Expect(owned.Read<hstring>("Identity").Value == "Original");
        Console.WriteLine("DOCUMENT_MIGRATOR_RESULT_LIFETIME_CULTURE_PASSED");
    }
}''', encoding='utf-8')
    env = dict(os.environ, DOTNET_PROCESSOR_COUNT='8', DOTNET_CLI_USE_MSBUILD_SERVER='0')
    completed = subprocess.run([dotnet, 'run', '--project', str(tmp_path / 'Probe.csproj'), '--configuration', 'Release'],
                               capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120, env=env)
    assert completed.returncode == 0, completed.stdout + completed.stderr
    assert 'DOCUMENT_MIGRATOR_RESULT_LIFETIME_CULTURE_PASSED' in completed.stdout
