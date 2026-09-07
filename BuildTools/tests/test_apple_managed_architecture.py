from __future__ import annotations

from pathlib import Path
import shutil
import subprocess

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is required")
@pytest.mark.parametrize(
    ("platform", "processor", "expected"),
    [
        ("SIMULATOR64", "x86_64", "iOS-simulator|iossimulator|x64"),
        ("OS64", "aarch64", "iOS-arm64|ios|arm64"),
    ],
)
def test_ios_runtime_architecture_matches_native_target(tmp_path: Path, platform: str, processor: str, expected: str) -> None:
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(AppleRuntimeArchitecture NONE)\n"
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        "set(CMAKE_SYSTEM_NAME iOS)\n"
        f"set(CMAKE_SYSTEM_PROCESSOR {processor})\n"
        "set(CMAKE_CXX_COMPILER_ID AppleClang)\n"
        "set(CMAKE_CXX_COMPILER_VERSION 17.0)\n"
        "set(CMAKE_SIZEOF_VOID_P 8)\n"
        f"set(PLATFORM {platform})\n"
        "set(FO_HEADLESS_ONLY ON)\n"
        "set(FO_MANAGED_SCRIPTING ON)\n"
        "set(FO_ENGINE_ROOT Engine)\n"
        "set(FO_MAIN_CONFIG Test.fomain)\n"
        "set(FO_DEV_NAME Test)\n"
        "set(FO_NICE_NAME Test)\n"
        "set(FO_GEOMETRY HEXAGONAL)\n"
        "set(FO_MAP_HEX_WIDTH 32)\n"
        "set(FO_MAP_HEX_HEIGHT 16)\n"
        "set(FO_MAP_CAMERA_ANGLE 30)\n"
        "set(FO_APP_ICON test.ico)\n"
        'set(FO_OUTPUT_PATH "${CMAKE_BINARY_DIR}/output")\n'
        "StartProjectGeneration()\n"
        'file(WRITE "${CMAKE_BINARY_DIR}/architecture.txt" "${FO_BUILD_PLATFORM}|${FO_MONO_OS}|${FO_MONO_ARCH}")\n',
        encoding="utf-8",
    )
    build = tmp_path / "build"

    result = subprocess.run(
        ["cmake", "-S", str(tmp_path), "-B", str(build)],
        capture_output=True, text=True,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    assert (build / "architecture.txt").read_text() == expected
