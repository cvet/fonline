from __future__ import annotations

from pathlib import Path
import sys
from unittest.mock import ANY

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


def test_android_sdk_workspace_uses_android_cli(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    workspace = tmp_path / "workspace"
    android_cli = workspace / "android-sdk" / "cmdline-tools" / "latest" / "bin" / "android"
    captured: list[tuple[list[object], dict[str, object]]] = []

    monkeypatch.setattr(_buildtools, "download_file", lambda *_args: None)
    monkeypatch.setattr(_buildtools, "extract_zip_with_permissions", lambda *_args: android_cli.parent.mkdir(parents=True))
    monkeypatch.setattr(_buildtools.shutil, "move", lambda *_args: None)
    monkeypatch.setattr(_buildtools, "remove_path_if_exists", lambda *_args: None)
    monkeypatch.setattr(_buildtools, "resolve_android_cli", lambda _path: android_cli)
    monkeypatch.setattr(_buildtools, "run", lambda command, **kwargs: captured.append((command, kwargs)))

    _buildtools.prepare_android_sdk_workspace(
        {
            "FO_WORKSPACE": str(workspace),
            "FO_ANDROID_SDK_VERSION": "15859902",
        }
    )

    assert captured == [
        (
            [
                android_cli,
                "--no-metrics",
                f"--sdk={workspace / 'android-sdk'}",
                "sdk",
                "install",
                "platform-tools",
                "build-tools/36.0.0",
                "platforms/android-35",
            ],
            {"env": ANY},
        )
    ]
