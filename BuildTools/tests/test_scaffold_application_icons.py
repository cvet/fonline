from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import struct
import subprocess

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
SCAFFOLDS = (
    "BuildTools/validation-project",
    "Examples/MinimalProject",
    "Examples/MinimalMultiplayer",
    "Examples/ContentShowcase",
    "Examples/NativeExtensionSample",
    "Examples/PackagingMatrix",
)
PLATFORM_PROBE = """
set(WIN32 FALSE)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_CXX_COMPILER_ID Clang)
set(CMAKE_CXX_COMPILER_VERSION 20.0)
set(CMAKE_SIZEOF_VOID_P 8)
"""
pytestmark = pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is required")


def _assert_readable_icon(path: Path) -> None:
    payload = path.read_bytes()
    reserved, kind, count = struct.unpack_from("<HHH", payload)
    width, height, _, _, planes, bits, size, offset = struct.unpack_from("<BBBBHHII", payload, 6)

    assert (reserved, kind, count) == (0, 1, 1)
    assert (width, height, planes, bits) == (16, 16, 1, 32)
    assert offset + size == len(payload)
    assert struct.unpack_from("<I", payload, offset)[0] == 40


def _configure_scaffold(
    tmp_path: Path, scaffold: str, arguments: list[str] | None = None,
) -> subprocess.CompletedProcess[str]:
    source_dir = ENGINE_ROOT / scaffold
    prefix = (source_dir / "CMakeLists.txt").read_text(encoding="utf-8-sig").split("StartProjectGeneration()", 1)[0]
    prefix = re.sub(r"project\(([^)]+)\)", r"project(\1 NONE)", prefix, count=1)
    prefix = prefix.replace(
        "include(Engine/BuildTools/Init.cmake)",
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")',
    )
    assert " NONE)" in prefix

    (tmp_path / "App.ico").write_bytes((source_dir / "App.ico").read_bytes())
    (tmp_path / "CMakeLists.txt").write_text(
        prefix
        + PLATFORM_PROBE
        + "StartProjectGeneration()\n"
        + 'file(WRITE "${CMAKE_BINARY_DIR}/resolved-icon.txt" "${FO_APP_ICON}")\n',
        encoding="utf-8",
    )

    command = ["cmake"]
    if shutil.which("ninja"):
        command.extend(["-G", "Ninja"])
    command.extend(["-S", str(tmp_path), "-B", str(tmp_path / "build"), *(arguments or [])])
    environment = {name: value for name, value in os.environ.items() if not name.startswith("FO_")}
    return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=60, env=environment)


def _resolved_icon(tmp_path: Path) -> Path:
    return Path((tmp_path / "build" / "resolved-icon.txt").read_text(encoding="utf-8"))


@pytest.mark.parametrize("scaffold", SCAFFOLDS)
def test_scaffold_defaults_to_its_own_icon(tmp_path: Path, scaffold: str) -> None:
    result = _configure_scaffold(tmp_path, scaffold)

    assert result.returncode == 0, result.stdout + result.stderr
    resolved = _resolved_icon(tmp_path)
    assert resolved.resolve() == (tmp_path / "App.ico").resolve()
    assert resolved.read_bytes() == (ENGINE_ROOT / scaffold / "App.ico").read_bytes()
    _assert_readable_icon(resolved)


@pytest.mark.parametrize("scaffold", SCAFFOLDS)
def test_scaffold_preserves_external_icon_override(tmp_path: Path, scaffold: str) -> None:
    external = tmp_path / "caller-icons" / "External.ico"
    external.parent.mkdir()
    external.write_bytes((ENGINE_ROOT / scaffold / "App.ico").read_bytes())

    result = _configure_scaffold(tmp_path, scaffold, [f"-DFO_APP_ICON={external.as_posix()}"])

    assert result.returncode == 0, result.stdout + result.stderr
    resolved = _resolved_icon(tmp_path)
    assert resolved.as_posix() == external.as_posix()
    assert resolved.read_bytes() == external.read_bytes()
    _assert_readable_icon(resolved)


@pytest.mark.parametrize("scaffold", SCAFFOLDS)
def test_scaffold_refuses_explicit_empty_icon(tmp_path: Path, scaffold: str) -> None:
    result = _configure_scaffold(tmp_path, scaffold, ["-DFO_APP_ICON="])

    output = result.stdout + result.stderr
    assert result.returncode != 0
    assert "FO_APP_ICON not specified" in output
    assert not (tmp_path / "build" / "resolved-icon.txt").exists()
