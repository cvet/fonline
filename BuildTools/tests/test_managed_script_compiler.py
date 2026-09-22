"""Compile fragments with the production script compiler and run them against a stand-in script assembly."""

from __future__ import annotations

from pathlib import Path
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
COMPILER_PROJECT = ENGINE / "Source/Scripting/Managed/Compiler/FOnline.ScriptCompiler.csproj"

SCRIPTS_PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><AssemblyName>{name}</AssemblyName><Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings></PropertyGroup>
</Project>
"""

SERVER_SCRIPTS = r"""
using System.Collections.Generic;

namespace Probe;

public static class World
{
    private static int Secret = 7;
    internal static int Counter;
    public static readonly List<string> Log = new List<string>();

    private static int Twice(int value) => value * 2;
    public static int ReadSecret() => Secret;
}
"""

CLIENT_SCRIPTS = r"""
namespace Probe;

public static class ClientWorld
{
    private static int Value = 3;
}
"""

PROBE_PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><OutputType>Exe</OutputType><Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings><TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="{compiler}" />
    <ProjectReference Include="{server}" />
  </ItemGroup>
</Project>
"""

PROBE_SOURCE = r"""
using System;
using System.IO;
using System.Reflection;
using System.Runtime.Loader;
using System.Threading.Tasks;
using FOnline.ScriptCompiler;
using Probe;

internal static class Program
{
    private static int Main(string[] args)
    {
        try {
            Run(args[0], args[1]).GetAwaiter().GetResult();
            return 0;
        }
        catch (Exception ex) {
            Console.WriteLine("FAILED " + ex);
            return 1;
        }
    }

    private static DynamicCompileResult Compile(string source, byte[]? scriptsImage = null, params string[] symbols)
    {
        return DynamicScriptCompiler.CompileAsync(new DynamicCompileRequest {
            Source = source,
            ScriptsAssembly = typeof(World).Assembly,
            ScriptsImage = scriptsImage,
            PreprocessorSymbols = symbols,
            Usings = new[] { "System", "System.Threading.Tasks", "Probe" },
        }).GetAwaiter().GetResult();
    }

    private static async Task<object?> Execute(DynamicCompileResult result, AssemblyLoadContext? context = null)
    {
        if (!result.Succeeded) {
            throw new InvalidOperationException(string.Join(" | ", result.Errors));
        }

        if (result.Symbols.Length != 0) {
            throw new InvalidOperationException("A fragment must be emitted without a PDB");
        }

        Assembly fragment = (context ?? AssemblyLoadContext.Default).LoadFromStream(new MemoryStream(result.Image));
        MethodInfo entry = fragment.GetType(DynamicScriptCompiler.EntryTypeName)?.GetMethod(DynamicScriptCompiler.EntryMethodName) ??
                           throw new InvalidOperationException("Fragment entry is missing");
        return await (Task<object>)(entry.Invoke(null, null) ?? throw new InvalidOperationException("Entry returned no task"));
    }

    private static async Task Run(string kind, string clientPath)
    {
        if (kind == "values") {
            object? privateCall = await Execute(Compile("World.Twice(21)"));
            object? internalWrite = await Execute(Compile("World.Counter += 5;\nreturn World.Counter;"));
            object? privateWrite = await Execute(Compile("World.Secret = 11;\nreturn World.Secret;"));
            object? voidCall = await Execute(Compile("World.Log.Add(\"entry\")"));
            object? awaited = await Execute(Compile("await Task.Delay(1);\nreturn \"done\";"));
            object? hoisted = await Execute(Compile("// comment\nusing System.Text;\nreturn new StringBuilder(\"a\").Append('b').ToString();"));
            object? symbol = await Execute(Compile("#if SERVER\nreturn \"server\";\n#else\nreturn \"other\";\n#endif", null, "SERVER"));
            Console.WriteLine("VALUES private=" + privateCall + " internal=" + internalWrite + " write=" + privateWrite + " secret=" + World.ReadSecret() +
                              " void=" + (voidCall ?? "null") + " log=" + World.Log.Count + " awaited=" + awaited + " hoisted=" + hoisted + " symbol=" + symbol);
            return;
        }
        if (kind == "errors") {
            DynamicCompileResult failed = Compile("int value = 1;\nreturn missing;");
            Console.WriteLine("ERRORS succeeded=" + failed.Succeeded + " count=" + failed.Errors.Count + " first=" + failed.Errors[0]);
            return;
        }
        if (kind == "initializers") {
            object? array = await Execute(Compile("new[] { 1, 2 }"));
            object? record = await Execute(Compile("new { Value = 42 }"));
            Console.WriteLine("INITIALIZERS array=" + string.Join(",", (int[])(array ?? throw new InvalidOperationException("No array"))) +
                              " object=" + record);
            return;
        }
        if (kind == "comments") {
            object? value = await Execute(Compile("World.Twice(21) // answer"));
            object? plain = await Execute(Compile("World.Log.Add(\"comment\") // no return value"));
            DynamicCompileResult failed = Compile("World.Missing // diagnostic");
            Console.WriteLine("COMMENTS value=" + value + " void=" + (plain ?? "null") + " log=" + World.Log.Count);
            Console.WriteLine("COMMENT_ERROR " + string.Join(" | ", failed.Errors));
            return;
        }
        if (kind == "name") {
            DynamicCompileResult first = Compile("1");
            DynamicCompileResult second = Compile("2");
            Console.WriteLine("NAME prefixed=" + first.AssemblyName.StartsWith(DynamicScriptCompiler.AssemblyNamePrefix, StringComparison.Ordinal) +
                              " unique=" + (first.AssemblyName != second.AssemblyName) +
                              " image=" + (DynamicScriptCompiler.ReadAssemblyName(first.Image) == first.AssemblyName));
            return;
        }
        if (kind == "image") {
            byte[] clientImage = File.ReadAllBytes(clientPath);
            AssemblyLoadContext clientContext = new AssemblyLoadContext("client");
            Assembly client = clientContext.LoadFromAssemblyPath(clientPath);
            object? value = await Execute(Compile("ClientWorld.Value", clientImage), clientContext);
            DynamicCompileResult serverOnly = Compile("World.ReadSecret()", clientImage);
            Console.WriteLine("IMAGE value=" + value + " serverVisible=" + serverOnly.Succeeded +
                              " mvid=" + (DynamicScriptCompiler.ReadModuleVersionId(clientImage) == client.ManifestModule.ModuleVersionId));
            return;
        }

        throw new InvalidOperationException("Unknown probe kind " + kind);
    }
}
"""


def build(dotnet, project, output=None):
    command = [dotnet, "build", str(project), "-c", "Release", "--nologo", "-v:minimal"]
    if output is not None:
        command += ["-o", str(output)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=300)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.fixture(scope="module")
def compiler_probe(tmp_path_factory):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for script compiler tests")

    root = tmp_path_factory.mktemp("managed-script-compiler")
    server_dir = root / "server"
    client_dir = root / "client"
    probe_dir = root / "probe"

    for directory, name, source in ((server_dir, "Probe.Server", SERVER_SCRIPTS), (client_dir, "Probe.Client", CLIENT_SCRIPTS)):
        directory.mkdir()
        (directory / "Scripts.csproj").write_text(SCRIPTS_PROJECT.format(name=name), encoding="utf-8")
        (directory / "Scripts.cs").write_text(source, encoding="utf-8")

    build(dotnet, client_dir / "Scripts.csproj", root / "client-bin")
    probe_dir.mkdir()
    (probe_dir / "Probe.csproj").write_text(PROBE_PROJECT.format(compiler=COMPILER_PROJECT, server=server_dir / "Scripts.csproj"), encoding="utf-8")
    (probe_dir / "Program.cs").write_text(PROBE_SOURCE, encoding="utf-8")
    build(dotnet, probe_dir / "Probe.csproj", root / "probe-bin")
    return dotnet, root / "probe-bin" / "Probe.dll", root / "client-bin" / "Probe.Client.dll"


def run_probe(compiler_probe, kind):
    dotnet, probe, client = compiler_probe
    result = subprocess.run([dotnet, str(probe), kind, str(client)], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def test_fragments_answer_and_reach_private_members(compiler_probe):
    output = run_probe(compiler_probe, "values")
    assert ("VALUES private=42 internal=5 write=11 secret=11 void=null log=1 awaited=done hoisted=ab symbol=server") in output


def test_compile_errors_point_at_the_fragment_line(compiler_probe):
    output = run_probe(compiler_probe, "errors")
    assert "ERRORS succeeded=False count=1 first=fragment(2," in output
    assert "CS0103" in output


def test_initializer_expressions_return_their_value(compiler_probe):
    assert "INITIALIZERS array=1,2 object={ Value = 42 }" in run_probe(compiler_probe, "initializers")


def test_expression_trailing_comments_do_not_hide_wrapper_syntax(compiler_probe):
    output = run_probe(compiler_probe, "comments")
    assert "COMMENTS value=42 void=null log=1" in output
    assert "COMMENT_ERROR fragment(1," in output
    assert "CS0117" in output
    assert "CS1026" not in output
    assert "CS1002" not in output


def test_every_fragment_gets_a_new_prefixed_name(compiler_probe):
    assert "NAME prefixed=True unique=True image=True" in run_probe(compiler_probe, "name")


def test_fragment_compiles_against_another_sides_scripts(compiler_probe):
    assert "IMAGE value=3 serverVisible=False mvid=True" in run_probe(compiler_probe, "image")
