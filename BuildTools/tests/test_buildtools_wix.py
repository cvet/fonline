from __future__ import annotations

import io
import os
import sys
import zipfile
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


def _wix_archive() -> bytes:
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w") as archive:
        for name in ("candle.exe", "light.exe", "WixUIExtension.dll"):
            archive.writestr(name, name.encode())
    return output.getvalue()


def test_wix_download_spec_tracks_the_pinned_patch_release() -> None:
    archive_name, url = _buildtools.build_wix_download_spec({"FO_WIX_VERSION": "3.14.1"})

    assert archive_name == "wix314-binaries.zip"
    assert url == "https://github.com/wixtoolset/wix3/releases/download/wix3141rtm/wix314-binaries.zip"


def test_prepare_wix_workspace_downloads_and_extracts_portable_tools(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    if os.name != "nt":
        pytest.skip("WiX v3 portable binaries are prepared only on Windows")
    archive = _wix_archive()
    seen_urls: list[str] = []

    def fake_download(url: str, target_path: Path, _label: str) -> None:
        seen_urls.append(url)
        target_path.write_bytes(archive)

    monkeypatch.setattr(_buildtools, "download_file", fake_download)
    workspace = tmp_path / "Workspace"
    _buildtools.prepare_wix_workspace({
        "FO_WORKSPACE": str(workspace),
        "FO_WIX_VERSION": "3.14.1",
    })

    assert seen_urls == ["https://github.com/wixtoolset/wix3/releases/download/wix3141rtm/wix314-binaries.zip"]
    assert all((workspace / "wix3" / name).is_file()
               for name in ("candle.exe", "light.exe", "WixUIExtension.dll"))
    assert not (workspace / "wix314-binaries.zip").exists()
