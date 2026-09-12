from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
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
    monkeypatch.setenv("INCLUDE", "outer-include")
    monkeypatch.setenv("LiB", "outer-lib")
    monkeypatch.setenv("libpath", "outer-libpath")
    observed = runtime / "environment.json"
    shim = runtime / "record_environment.py"
    shim.write_text(
        "import json, os\nfrom pathlib import Path\n"
        f"Path({str(observed)!r}).write_text(json.dumps({{"
        "key: value for key, value in os.environ.items() "
        "if key.casefold() in ('targetname', 'target_name', 'makeflags', 'mflags', 'include', 'lib', 'libpath')}))\n",
        encoding="utf-8",
    )
    if os.name == "nt":
        (runtime / "build.cmd").write_text(
            f'@echo off\n"{sys.executable}" "{shim}"\n"{dotnet}" publish Beta/Beta.csproj -c Release --nologo -v:minimal\n',
            encoding="utf-8",
        )
        monkeypatch.setattr(buildtools, "resolve_visual_studio_2022_dev_cmd", lambda: None)
    else:
        script = runtime / "build.sh"
        script.write_text(
            f"#!/bin/sh\nset -eu\n{shlex.quote(sys.executable)} {shlex.quote(str(shim))}\n"
            f"exec {shlex.quote(dotnet)} publish Beta/Beta.csproj -c Release --nologo -v:minimal\n", encoding="utf-8",
        )
        script.chmod(0o755)

    buildtools.run_runtime_build([], runtime, target_os="windows" if os.name == "nt" else "osx")

    assert json.loads(observed.read_text()) == {"TARGET_NAME": "outer-xcode-target"}
    publish = runtime / "Beta/bin/Release" / framework / "publish"
    assert {path.name for path in publish.glob("*.dll")} == {"Alpha.dll", "Beta.dll"}


@pytest.mark.skipif(os.name == "nt", reason="Apple runtime builds use the POSIX build.sh entry point")
@pytest.mark.parametrize("target_os,target_sdk,architecture", [("ios", "iPhoneOS", "arm64"), ("iossimulator", "iPhoneSimulator", "x86_64")])
def test_runtime_host_cmake_does_not_inherit_the_target_sdk(tmp_path, monkeypatch, target_os, target_sdk, architecture):
    cmake = shutil.which("cmake")
    if cmake is None:
        pytest.skip("CMake is required for actual Darwin SDK selection")

    runtime = tmp_path / "runtime with spaces"
    runtime.mkdir()
    developer = tmp_path / "Xcode with spaces.app/Contents/Developer"
    host_sdk = developer / "Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk"
    device_sdk = developer / f"Platforms/{target_sdk}.platform/Developer/SDKs/{target_sdk}26.5.sdk"
    host_sdk.mkdir(parents=True)
    device_sdk.mkdir(parents=True)
    discovery = tmp_path / "apple discovery"
    discovery.mkdir()
    for name, output in (("xcode-select", str(developer)), ("xcrun", str(host_sdk)), ("sw_vers", "26.5")):
        executable = discovery / name
        executable.write_text(f"#!/bin/sh\nprintf '%s\\n' {shlex.quote(output)}\n", encoding="utf-8")
        executable.chmod(0o755)

    # Only SDK discovery is supplied; the installed CMake runs its real Darwin/iOS initialization
    (runtime / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(RuntimeSdkIsolation NONE)\n"
        'file(WRITE "${CMAKE_BINARY_DIR}/selected-sdk.txt" "${CMAKE_OSX_SYSROOT}\\n")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/developer-dir.txt" "$ENV{DEVELOPER_DIR}\\n")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/platform.txt" "${CMAKE_OSX_ARCHITECTURES};${CMAKE_OSX_DEPLOYMENT_TARGET}\\n")\n',
        encoding="utf-8",
    )
    probe = runtime / "configure.py"
    probe.write_text(
        "import json, os, subprocess\nfrom pathlib import Path\n"
        f"runtime = Path({str(runtime)!r})\n"
        f"cmake = {cmake!r}\n"
        "commands = {}\n"
        "for role, arguments in (\n"
        f"    ('host', ['-DCMAKE_SYSTEM_NAME=Darwin', '-DCMAKE_OSX_ARCHITECTURES={architecture}', '-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0']),\n"
        f"    ('target', ['-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_ARCHITECTURES={architecture}', '-DCMAKE_OSX_DEPLOYMENT_TARGET=12.2', '-DCMAKE_OSX_SYSROOT={device_sdk.as_posix()}']),\n"
        "):\n"
        "    command = [cmake, '-G', 'Unix Makefiles', '-S', str(runtime), '-B', str(runtime / role), *arguments]\n"
        "    result = subprocess.run(command, capture_output=True, text=True)\n"
        "    commands[role] = {'command': command, 'stdout': result.stdout, 'stderr': result.stderr, 'returncode': result.returncode}\n"
        "    assert result.returncode == 0, result.stdout + result.stderr\n"
        "    assert 'CMake Warning' not in result.stdout + result.stderr, result.stdout + result.stderr\n"
        "(runtime / 'configure-results.json').write_text(json.dumps({'commands': commands, 'SDKROOT': os.environ.get('SDKROOT')}))\n",
        encoding="utf-8",
    )
    script = runtime / "build.sh"
    script.write_text(f"#!/bin/sh\nexec {shlex.quote(sys.executable)} {shlex.quote(str(probe))}\n", encoding="utf-8")
    script.chmod(0o755)
    monkeypatch.setenv("PATH", str(discovery) + os.pathsep + os.environ["PATH"])
    monkeypatch.setenv("SDKROOT", str(device_sdk))
    monkeypatch.setenv("DEVELOPER_DIR", str(developer))

    negative = subprocess.run([str(script)], cwd=runtime, capture_output=True, text=True)
    assert negative.returncode == 0, negative.stdout + negative.stderr
    assert (runtime / "host/selected-sdk.txt").read_text().strip() == str(device_sdk)
    assert (runtime / "target/selected-sdk.txt").read_text().strip() == str(device_sdk)
    (runtime / "negative-configure-results.json").write_bytes((runtime / "configure-results.json").read_bytes())
    shutil.rmtree(runtime / "host")
    shutil.rmtree(runtime / "target")

    runtime_arch = "x64" if architecture == "x86_64" else architecture
    buildtools.run_runtime_build(["-os", target_os, "-arch", runtime_arch, "-c", "Release"], runtime, target_os=target_os)

    assert (runtime / "host/selected-sdk.txt").read_text().strip() == str(host_sdk)
    assert (runtime / "target/selected-sdk.txt").read_text().strip() == str(device_sdk)
    assert (runtime / "host/developer-dir.txt").read_text().strip() == str(developer)
    assert (runtime / "host/platform.txt").read_text().strip() == f"{architecture};12.0"
    assert (runtime / "target/platform.txt").read_text().strip() == f"{architecture};12.2"
    assert json.loads((runtime / "configure-results.json").read_text())["SDKROOT"] is None
    (runtime / "positive-configure-results.json").write_bytes((runtime / "configure-results.json").read_bytes())
    assert os.environ["SDKROOT"] == str(device_sdk)

    # A caller-selected macOS SDK remains meaningful for ordinary macOS runtime builds
    monkeypatch.setenv("SDKROOT", str(host_sdk))
    shutil.rmtree(runtime / "host")
    shutil.rmtree(runtime / "target")
    buildtools.run_runtime_build(["-os", "osx", "-arch", runtime_arch, "-c", "Release"], runtime, target_os="osx")
    assert (runtime / "host/selected-sdk.txt").read_text().strip() == str(host_sdk)
    assert json.loads((runtime / "configure-results.json").read_text())["SDKROOT"] == str(host_sdk)
    assert (runtime / "host/developer-dir.txt").read_text().strip() == str(developer)
