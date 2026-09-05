from __future__ import annotations

from pathlib import Path
import sys


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
