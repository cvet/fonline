from __future__ import annotations

from pathlib import Path
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


@pytest.mark.parametrize(
    ("version", "compatible"),
    [
        ((3, 31, 6), True),
        ((4, 2, 0), False),
        ((4, 2, 5), False),
        ((4, 2, 6), True),
        ((4, 3, 0), False),
        ((4, 3, 2), False),
        ((4, 3, 3), True),
    ],
)
def test_emscripten_cmake_compatibility(version: tuple[int, int, int], compatible: bool) -> None:
    assert _buildtools.is_emscripten_cmake_version_compatible(version) is compatible


def test_resolve_emscripten_cmake_skips_incompatible_path_version(monkeypatch: pytest.MonkeyPatch) -> None:
    path_cmake = "C:/CMake/bin/cmake.exe"
    visual_studio_cmake = "C:/VisualStudio/CMake/bin/cmake.exe"
    monkeypatch.setattr(_buildtools.shutil, "which", lambda _name: path_cmake)
    monkeypatch.setattr(_buildtools.os, "name", "nt")
    monkeypatch.setattr(_buildtools, "discover_windows_cmake_candidates", lambda: [visual_studio_cmake])
    monkeypatch.setattr(
        _buildtools,
        "parse_cmake_version",
        lambda executable: (4, 2, 0) if executable == path_cmake else (3, 31, 6),
    )

    assert _buildtools.resolve_emscripten_cmake() == visual_studio_cmake


@pytest.mark.parametrize("platform", ["web", "android-arm64"])
def test_windows_multiconfig_cross_build_does_not_set_build_type(
    platform: str,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(_buildtools.os, "name", "nt")

    assert "-DCMAKE_BUILD_TYPE=RelWithDebInfo" not in _buildtools.make_platform_build_flag_args(
        platform,
        "client",
        "RelWithDebInfo",
    )


@pytest.mark.parametrize("platform", ["web", "android-arm64"])
def test_unix_single_config_cross_build_sets_build_type(platform: str, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(_buildtools.os, "name", "posix")

    assert "-DCMAKE_BUILD_TYPE=RelWithDebInfo" in _buildtools.make_platform_build_flag_args(
        platform,
        "client",
        "RelWithDebInfo",
    )
