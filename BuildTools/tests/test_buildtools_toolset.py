from __future__ import annotations

from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


def test_toolset_configure_respects_project_ascompiler_default() -> None:
    args = _buildtools.make_toolset_cmake_args("output", config_name="Release")

    assert "-DFO_BUILD_BAKER=1" in args
    assert "-DFO_BUILD_CLIENT=0" in args
    assert "-DFO_BUILD_SERVER=0" in args
    assert "-DFO_BUILD_MAPPER=0" in args
    assert "-DFO_UNIT_TESTS=0" in args
    assert "-DFO_CODE_COVERAGE=0" in args
    assert "-DCMAKE_BUILD_TYPE=Release" in args
    assert not any(arg.startswith("-DFO_BUILD_ASCOMPILER=") for arg in args)


def test_toolset_workspace_version_tracks_configure_flags(monkeypatch) -> None:
    baseline = _buildtools.build_toolset_version()

    monkeypatch.setitem(_buildtools.BUILD_TARGETS, "toolset", _buildtools.make_flag_map("FO_BUILD_BAKER", "FO_BUILD_SERVER"))

    assert _buildtools.build_toolset_version() != baseline


@pytest.mark.parametrize("target", ["full", "toolset"])
@pytest.mark.parametrize("angelscript", ["ON", "OFF"])
def test_aggregate_build_configures_with_either_scripting_backend(tmp_path: Path, target: str, angelscript: str) -> None:
    source = tmp_path / "source"
    source.mkdir()
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(BackendSelection NONE)\n"
        "option(FO_ANGELSCRIPT_SCRIPTING \"\" OFF)\n"
        "option(FO_BUILD_ASCOMPILER \"\" ${FO_ANGELSCRIPT_SCRIPTING})\n"
        "if(NOT FO_BUILD_ASCOMPILER STREQUAL FO_ANGELSCRIPT_SCRIPTING)\n"
        "  message(FATAL_ERROR \"Aggregate build changed the project backend selection\")\n"
        "endif()\n",
        encoding="utf-8",
    )

    result = subprocess.run(
        ["cmake", "-S", str(source), "-B", str(tmp_path / "build"),
         f"-DFO_ANGELSCRIPT_SCRIPTING={angelscript}",
         *_buildtools.make_platform_build_flag_args("win64", target, "Release")],
        capture_output=True, text=True,
    )

    assert result.returncode == 0, result.stdout + result.stderr


def test_explicit_ascompiler_build_still_enables_the_compiler() -> None:
    assert "-DFO_BUILD_ASCOMPILER=1" in _buildtools.build_flag_args("ascompiler")
