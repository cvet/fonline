from __future__ import annotations

from pathlib import Path
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


@pytest.mark.parametrize(("platform", "architecture"), [("win32-win7", "Win32"), ("win64-win7", "x64")])
def test_windows7_native_generator_uses_pinned_toolset(platform: str, architecture: str) -> None:
    assert _buildtools.make_windows_configure_cmd(platform, "source") == [
        "cmake",
        "-A",
        architecture,
        "-T",
        "v143,version=14.44",
        "source",
    ]


@pytest.mark.parametrize("platform", ["win32-win7", "win64-win7"])
def test_windows7_platform_does_not_select_binary_postfix(platform: str, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    captured: dict[str, object] = {}

    def capture_configure(*args: object, **kwargs: object) -> None:
        captured["args"] = args
        captured["kwargs"] = kwargs

    monkeypatch.setattr(_buildtools, "run_platform_configure_build", capture_configure)
    env = {
        "FO_WORKSPACE": str(tmp_path / "workspace"),
        "FO_OUTPUT": str(tmp_path / "output"),
        "FO_PROJECT_ROOT": str(tmp_path / "project"),
    }

    _buildtools.configure_build(platform, "client", "Release", env)

    assert captured["kwargs"] == {
        "extra_cmake_args": [
            f"-DFO_OUTPUT_PATH={(tmp_path / 'output').as_posix()}",
        ]
    }


def test_windows7_binary_postfix_is_selected_separately(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    captured: dict[str, object] = {}

    def capture_configure(*args: object, **kwargs: object) -> None:
        captured["args"] = args
        captured["kwargs"] = kwargs

    monkeypatch.setattr(_buildtools, "run_platform_configure_build", capture_configure)
    env = {
        "FO_WORKSPACE": str(tmp_path / "workspace"),
        "FO_OUTPUT": str(tmp_path / "output"),
        "FO_PROJECT_ROOT": str(tmp_path / "project"),
        "FO_BINARY_OUTPUT_POSTFIX": "Win7",
    }

    _buildtools.configure_build("win32-win7", "client", "Release", env)

    assert captured["kwargs"] == {
        "extra_cmake_args": [
            f"-DFO_OUTPUT_PATH={(tmp_path / 'output').as_posix()}",
            "-DFO_BINARY_OUTPUT_POSTFIX=Win7",
        ]
    }
