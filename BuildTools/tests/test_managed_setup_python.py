from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(os.name == "nt" or shutil.which("cmake") is None, reason="POSIX shell and CMake are required")
@pytest.mark.parametrize("target_os", ["osx", "iossimulator"])
def test_managed_setup_keeps_configured_python_after_path_changes(tmp_path: Path, target_os: str) -> None:
    source = tmp_path / "project with spaces"
    engine = source / "Engine"
    (engine / "BuildTools").mkdir(parents=True)
    (engine / "ThirdParty").mkdir()
    (engine / "ThirdParty/dotnet-runtime").write_text("test-runtime\n")
    shutil.copy2(BUILDTOOLS_DIR / "setup-mono.sh", engine / "BuildTools/setup-mono.sh")
    (engine / "BuildTools/buildtools.py").write_text(
        "import json, os, sys\n"
        "from pathlib import Path\n"
        "Path('invocation.json').write_text(json.dumps({"
        "'python': sys.executable, 'args': sys.argv[1:], 'workspace': os.environ['FO_WORKSPACE']}))\n"
    )
    stage = (BUILDTOOLS_DIR / "cmake/stages/ThirdParty.cmake").read_text()
    managed_stage = stage[stage.index("# Managed scripting runtime (Mono)"):]
    (source / "managed.cmake").write_text(managed_stage)
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(ManagedSetupPython NONE)\n"
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        "set(FO_MANAGED_SCRIPTING ON)\n"
        "set(FO_ENGINE_ROOT Engine)\n"
        f"set(FO_MONO_OS {target_os})\n"
        "set(FO_MONO_ARCH x64)\n"
        "set(expr_DebugBuild 0)\n"
        'include("${CMAKE_CURRENT_SOURCE_DIR}/managed.cmake")\n'
    )
    build = tmp_path / "build with spaces"
    cmake = shutil.which("cmake")
    result = subprocess.run(
        [cmake, "-S", str(source), "-B", str(build), f"-DPython3_EXECUTABLE={sys.executable}"],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr

    poisoned_path = tmp_path / "old xcode python"
    poisoned_path.mkdir()
    for name in ("python3", "python"):
        program = poisoned_path / name
        program.write_text("#!/bin/sh\necho 'wrong build-phase interpreter' >&2\nexit 93\n")
        program.chmod(0o755)
    build_env = dict(os.environ, PATH=str(poisoned_path) + os.pathsep + os.environ["PATH"])
    result = subprocess.run(
        [cmake, "--build", str(build), "--target", "SetupManagedRuntime"],
        env=build_env, capture_output=True, text=True,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    invocation = json.loads((build / "dotnet/invocation.json").read_text())
    assert Path(invocation["python"]).resolve() == Path(sys.executable).resolve()
    assert invocation["args"] == ["setup-mono", target_os, "x64", "Release"]
    assert invocation["workspace"] == str(build / "dotnet")
