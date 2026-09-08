from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(not shutil.which("cmake"), reason="CMake is required")
@pytest.mark.parametrize(
    ("cache_value", "environment_value", "expected_source"),
    [
        ("cache", None, "cache"),
        ("cache with spaces", None, "cache with spaces"),
        (None, "environment with spaces", "environment with spaces"),
        (None, None, None),
        ("cache", "environment", "cache"),
        ("", "environment", "environment"),
    ],
)
def test_runtime_directory_resolution_preserves_configuration_input(
    tmp_path: Path, cache_value: str | None, environment_value: str | None, expected_source: str | None,
) -> None:
    third_party = (BUILDTOOLS_DIR / "cmake/stages/ThirdParty.cmake").read_text()
    begin = third_party.index('    if(NOT FO_DOTNET_DIR AND NOT "$ENV{FO_DOTNET_DIR}" STREQUAL "")')
    end = third_party.index("    FileMakeDirectory(${FO_DOTNET_DIR})", begin)
    directory_resolution = third_party[begin:end]
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(ManagedRuntimeDirectory NONE)\n"
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        + directory_resolution
        + '\nfile(WRITE "${CMAKE_BINARY_DIR}/resolved.txt" "${FO_DOTNET_DIR}")\n',
        encoding="utf-8",
    )
    build = tmp_path / "build"
    command = ["cmake", "-S", str(tmp_path), "-B", str(build)]
    if cache_value is not None:
        value = str(tmp_path / cache_value) if cache_value else ""
        command.append(f"-DFO_DOTNET_DIR:PATH={value}")
    environment = os.environ.copy()
    environment.pop("FO_DOTNET_DIR", None)
    if environment_value is not None:
        environment["FO_DOTNET_DIR"] = str(tmp_path / environment_value)

    result = subprocess.run(command, env=environment, capture_output=True, text=True)

    assert result.returncode == 0, result.stdout + result.stderr
    assert "CMake Warning" not in result.stdout + result.stderr
    expected = tmp_path / expected_source if expected_source else build / "dotnet"
    assert (build / "resolved.txt").read_text() == str(expected)
