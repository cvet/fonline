from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


@pytest.mark.parametrize("variable", ["TARGETNAME", "TargetName", "targetname"])
def test_runtime_publish_does_not_inherit_the_outer_target_name(tmp_path, monkeypatch, variable):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for actual MSBuild publishing")
    version = subprocess.check_output([dotnet, "--version"], text=True).strip()
    framework = f"net{version.split('.')[0]}.0"
    runtime = tmp_path / "runtime with spaces"
    for name in ("Alpha", "Beta"):
        project = runtime / name
        project.mkdir(parents=True)
        reference = '<ItemGroup><ProjectReference Include="../Alpha/Alpha.csproj" /></ItemGroup>' if name == "Beta" else ""
        (project / f"{name}.csproj").write_text(
            '<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup>'
            f'<TargetFramework>{framework}</TargetFramework><UseSharedCompilation>false</UseSharedCompilation>'
            '</PropertyGroup>' + reference + '</Project>\n', encoding="utf-8",
        )
        (project / f"{name}.cs").write_text(f"namespace {name}; public sealed class Marker {{ }}\n", encoding="utf-8")

    monkeypatch.setenv(variable, "SetupManagedRuntime")
    monkeypatch.setenv("TARGET_NAME", "outer-xcode-target")
    monkeypatch.setenv("MAKEFLAGS", "--jobserver-auth=invalid")
    monkeypatch.setenv("MFLAGS", "invalid")
    observed = runtime / "environment.json"
    shim = runtime / "record_environment.py"
    shim.write_text(
        "import json, os\nfrom pathlib import Path\n"
        f"Path({str(observed)!r}).write_text(json.dumps({{"
        "key: value for key, value in os.environ.items() "
        "if key.casefold() in ('targetname', 'target_name', 'makeflags', 'mflags')}))\n",
        encoding="utf-8",
    )
    if os.name == "nt":
        (runtime / "build.cmd").write_text(
            f'@echo off\n"{sys.executable}" "{shim}"\n"{dotnet}" publish Beta/Beta.csproj -c Release --nologo -v:minimal\n',
            encoding="utf-8",
        )
        monkeypatch.setattr(buildtools, "resolve_visual_studio_2022_dev_cmd", lambda: None)
    else:
        import shlex
        script = runtime / "build.sh"
        script.write_text(
            f"#!/bin/sh\nset -eu\n{shlex.quote(sys.executable)} {shlex.quote(str(shim))}\n"
            f"exec {shlex.quote(dotnet)} publish Beta/Beta.csproj -c Release --nologo -v:minimal\n", encoding="utf-8",
        )
        script.chmod(0o755)

    buildtools.run_runtime_build([], runtime)

    assert json.loads(observed.read_text()) == {"TARGET_NAME": "outer-xcode-target"}
    publish = runtime / "Beta/bin/Release" / framework / "publish"
    assert {path.name for path in publish.glob("*.dll")} == {"Alpha.dll", "Beta.dll"}
