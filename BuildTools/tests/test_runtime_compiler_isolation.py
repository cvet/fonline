from __future__ import annotations

import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import time
import uuid
from xml.sax.saxutils import quoteattr

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


def test_runtime_generator_dependencies_survive_another_build_tree_cleanup(tmp_path, monkeypatch):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for actual compiler-server isolation")
    version = subprocess.check_output([dotnet, "--version"], cwd=tmp_path, text=True).strip()
    sdk_lines = subprocess.check_output([dotnet, "--list-sdks"], cwd=tmp_path, text=True).splitlines()
    sdk_line = next(line for line in sdk_lines if line.startswith(version + " "))
    sdk = Path(re.fullmatch(r"\S+ \[(.+)\]", sdk_line).group(1)) / version
    compiler = sdk / "Roslyn/bincore"
    if not (compiler / "VBCSCompiler.dll").is_file():
        pytest.skip("the SDK does not expose a compiler server")
    major = version.split(".")[0]
    framework = f"net{major}.0"
    packs = sdk.parents[1] / "packs/Microsoft.NETCore.App.Ref"
    versions = sorted((path for path in packs.glob(major + ".*") if path.is_dir()), key=lambda path: tuple(map(int, re.findall(r"\d+", path.name))))
    if not versions:
        pytest.skip("the SDK reference pack is required for the generator fixture")
    analyzers = versions[-1] / "analyzers/dotnet/cs"
    companion = analyzers / "Microsoft.Interop.SourceGeneration.dll"
    generator = analyzers / "Microsoft.Interop.LibraryImportGenerator.dll"
    if not companion.is_file() or not generator.is_file():
        pytest.skip("the SDK LibraryImport source generator and companion are required")
    (tmp_path / "global.json").write_text('{"sdk":{"version":"' + version + '","rollForward":"disable"}}\n')
    (tmp_path / "NuGet.Config").write_text('<configuration><packageSources><clear /></packageSources></configuration>\n')

    roots = [tmp_path / name for name in ("a-runtime", "b-runtime")]
    for root in roots:
        for folder, source in (("shared", companion), ("generator", generator)):
            destination = root / folder / source.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        (root / "Consumer.cs").write_text(
            'using System.Runtime.InteropServices;\ninternal static partial class Consumer {\n'
            ' [LibraryImport("unused-library", EntryPoint="probe")]\n internal static partial int Probe();\n}\n'
        )
        (root / "Consumer.csproj").write_text(
            '<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup>'
            f'<TargetFramework>{framework}</TargetFramework><EnableDefaultCompileItems>false</EnableDefaultCompileItems>'
            '<EnableNETAnalyzers>false</EnableNETAnalyzers><TreatWarningsAsErrors>true</TreatWarningsAsErrors>'
            '<AllowUnsafeBlocks>true</AllowUnsafeBlocks>'
            '<EmitCompilerGeneratedFiles>true</EmitCompilerGeneratedFiles>'
            '<CompilerGeneratedFilesOutputPath>generated</CompilerGeneratedFilesOutputPath>'
            '</PropertyGroup><ItemGroup><Compile Include="Consumer.cs" /></ItemGroup>'
            '<Target Name="SelectFixtureAnalyzers" BeforeTargets="CoreCompile"><ItemGroup>'
            '<Analyzer Remove="@(Analyzer)" />'
            f'<Analyzer Include={quoteattr(str(root / "shared" / companion.name))} />'
            f'<Analyzer Include={quoteattr(str(root / "generator" / generator.name))} />'
            '</ItemGroup></Target></Project>\n'
        )

    pipe = "fo_runtime_test_" + uuid.uuid4().hex
    server_log = tmp_path / "compiler-server.log"
    monkeypatch.setenv("RoslynCommandLineLogFile", str(server_log))
    # A private server and pipe keep this regression independent of every other build on the host
    with (tmp_path / "compiler-server-output.log").open("w") as output:
        server = subprocess.Popen([dotnet, str(compiler / "VBCSCompiler.dll"), "-pipename:" + pipe], cwd=tmp_path, stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 30
            while not server_log.exists() or "Constructing pipe and waiting" not in server_log.read_text():
                assert server.poll() is None, "the private compiler server exited before becoming ready"
                assert time.monotonic() < deadline, "the private compiler server did not become ready"
                time.sleep(0.02)
            result = subprocess.run(
                [dotnet, "build", "Consumer.csproj", "--nologo", "-v:minimal", "-p:UseSharedCompilation=true", "-p:SharedCompilationId=" + pipe],
                cwd=roots[0], capture_output=True, text=True, timeout=90,
            )
            (tmp_path / "first-build.log").write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stdout + result.stderr
            assert "CompilerServer: server - server processed compilation" in server_log.read_text()
            assert len(list((roots[0] / "generated").rglob("LibraryImports.g.cs"))) == 1
            shutil.rmtree(roots[0])

            runtime = roots[1]
            if os.name == "nt":
                (runtime / "build.cmd").write_text(f'@echo off\n"{dotnet}" build Consumer.csproj --nologo -v:minimal %*\n')
                monkeypatch.setattr(buildtools, "resolve_visual_studio_2022_dev_cmd", lambda: None)
            else:
                script = runtime / "build.sh"
                script.write_text(f'#!/bin/sh\nexec {shlex.quote(dotnet)} build Consumer.csproj --nologo -v:minimal "$@"\n')
                script.chmod(0o755)
            buildtools.run_runtime_build(["-p:SharedCompilationId=" + pipe], runtime, target_os="linux")

            generated = list((runtime / "generated").rglob("LibraryImports.g.cs"))
            assert len(generated) == 1
            assert 'EntryPoint = "probe"' in generated[0].read_text()
            assert (runtime / "bin/Debug" / framework / "Consumer.dll").is_file()
        finally:
            # Terminate only the process this test created, never the host's other compiler servers
            server.terminate()
            try:
                server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=10)
