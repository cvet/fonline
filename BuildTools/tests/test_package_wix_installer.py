from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
sys.path.insert(0, str(BUILDTOOLS_DIR / "msicreator"))
import package as _package  # noqa: E402
import foconfig  # noqa: E402
import createmsi  # noqa: E402


def _make_packager(
    tmp_path: Path,
    fomain_lines: list[str],
    *,
    nicename: str = "LastFrontier",
    platform: str = "Windows",
    target: str = "Client",
    arch: str = "win64",
    binary_output_postfix: str = "",
) -> _package.Packager:
    maincfg = tmp_path / "Main.fomain"
    maincfg.write_text("\n".join(fomain_lines) + "\n", encoding="utf-8")
    fomain = foconfig.ConfigParser()
    fomain.loadFromLines(fomain_lines)
    packager = _package.Packager.__new__(_package.Packager)
    packager.fomain = fomain
    packager.args = SimpleNamespace(
        maincfg=str(maincfg),
        nicename=nicename,
        platform=platform,
        target=target,
        arch=arch,
        binary_output_postfix=binary_output_postfix,
    )
    return packager


def test_resolve_game_version_resolves_file_directive(tmp_path: Path) -> None:
    (tmp_path / "VERSION").write_text("0.3.422\n", encoding="utf-8")
    packager = _make_packager(tmp_path, ["Common.GameVersion = $FILE{VERSION}"])
    assert packager.resolve_game_version() == "0.3.422"


def test_resolve_game_version_keeps_literal(tmp_path: Path) -> None:
    packager = _make_packager(tmp_path, ["Common.GameVersion = 1.2.3"])
    assert packager.resolve_game_version() == "1.2.3"


def test_ensure_msi_toolset_requires_wixl_on_posix(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    if os.name == "nt":
        pytest.skip("wixl is the POSIX toolset; Windows uses candle/light")
    packager = _make_packager(tmp_path, ["Common.GameVersion = 1.0.0"])

    monkeypatch.setattr(_package.shutil, "which", lambda name: None)
    with pytest.raises(AssertionError, match="wixl"):
        packager.ensure_msi_toolset()

    monkeypatch.setattr(_package.shutil, "which", lambda name: "/usr/bin/" + name)
    packager.ensure_msi_toolset()


def _staged_client(output_dir: Path) -> Path:
    staged = output_dir / "LF-Client"
    (staged / "Resources").mkdir(parents=True)
    (staged / "LastFrontier.exe").write_text("exe", encoding="utf-8")
    (staged / "Resources" / "pack.zip").write_text("data", encoding="utf-8")
    return staged


def test_make_wix_installer_builds_config_and_xml(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    output_dir = tmp_path / "output"
    staged = _staged_client(output_dir)
    (tmp_path / "app.ico").write_bytes(b"\x00\x00\x01\x00")

    fomain_lines = [
        "Common.GameName = Last Frontier",
        "Common.GameVersion = 0.3.422",
        "Auth.UriScheme = lastfrontier",
        "Packaging.AppIcon = app.ico",
        "Packaging.MsiUpgradeCode = B6A1F2C0-3D4E-4A5B-9C7D-0E1F2A3B4C5D",
    ]
    packager = _make_packager(tmp_path, fomain_lines)
    packager.target_output_path = str(staged)
    monkeypatch.setattr(packager, "ensure_msi_toolset", lambda: None)
    monkeypatch.setattr(createmsi.platform, "system", lambda: "Windows")

    captured: dict[str, object] = {}

    def fake_run(cmd: list[str], cwd: str | None = None, check: bool = False) -> SimpleNamespace:
        captured["cmd"] = cmd
        captured["cwd"] = cwd
        # The writable-data marker must exist inside the staged payload at MSI build time.
        assert (Path(cwd) / "LF-Client" / "INSTALLED").is_file()
        # Drive only createmsi's WiX XML generation (no wixl/light needed) to validate the wiring.
        previous = os.getcwd()
        os.chdir(cwd)
        try:
            createmsi.PackageGenerator(cmd[-1]).generate_files()
        finally:
            os.chdir(previous)
        return SimpleNamespace(returncode=0)

    monkeypatch.setattr(_package.subprocess, "run", fake_run)

    packager.make_wix_installer()

    # createmsi requires a bare json filename resolved against work_dir as cwd.
    assert captured["cmd"][-1] == "LastFrontier.wix.json"
    assert "/" not in str(captured["cmd"][-1]) and "\\" not in str(captured["cmd"][-1])
    assert captured["cwd"] == str(output_dir)

    # The marker is added only for the MSI and removed afterwards (portable Raw/Zip stay portable).
    assert not (staged / "INSTALLED").exists()

    config = json.loads((output_dir / "LastFrontier.wix.json").read_text(encoding="utf-8"))
    assert config["version"] == "0.3.422"
    assert config["upgrade_guid"] == "B6A1F2C0-3D4E-4A5B-9C7D-0E1F2A3B4C5D"
    assert config["product_name"] == "Last Frontier"
    assert config["installdir"] == "LastFrontier"
    assert config["startmenu_shortcut"] == "LastFrontier.exe"
    assert config["desktop_shortcut"] == "LastFrontier.exe"
    assert os.path.basename(str(config["addremove_icon"])) == "app.ico"

    wxs = (output_dir / "LastFrontier.wxs").read_text(encoding="utf-8")
    assert "Software\\Classes\\lastfrontier" in wxs
    assert "--DeepLinkUri" in wxs
    assert "Shortcut" in wxs
    assert "ARPPRODUCTICON" in wxs
    assert 'Version="0.3.422"' in wxs


def test_make_wix_installer_rejects_missing_upgrade_code(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    output_dir = tmp_path / "output"
    staged = _staged_client(output_dir)
    fomain_lines = [
        "Common.GameVersion = 0.3.422",
        "Auth.UriScheme = lastfrontier",
    ]
    packager = _make_packager(tmp_path, fomain_lines)
    packager.target_output_path = str(staged)
    monkeypatch.setattr(packager, "ensure_msi_toolset", lambda: None)

    with pytest.raises(AssertionError, match="MsiUpgradeCode"):
        packager.make_wix_installer()


def test_make_wix_installer_uses_distinct_legacy_x86_artifact_names(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    output_dir = tmp_path / "output"
    staged = output_dir / "LF-Client-Win7"
    staged.mkdir(parents=True)
    (staged / "LastFrontier_Win7.exe").write_text("exe", encoding="utf-8")

    fomain_lines = [
        "Common.GameName = Last Frontier",
        "Common.GameVersion = 0.3.422",
        "Auth.UriScheme = lastfrontier",
        "Packaging.MsiUpgradeCode = B6A1F2C0-3D4E-4A5B-9C7D-0E1F2A3B4C5D",
    ]
    packager = _make_packager(tmp_path, fomain_lines, arch="win32-win7", binary_output_postfix="Win7")
    packager.target_output_path = str(staged)
    monkeypatch.setattr(packager, "ensure_msi_toolset", lambda: None)

    captured: dict[str, object] = {}

    def fake_run(cmd: list[str], cwd: str | None = None, check: bool = False) -> SimpleNamespace:
        captured["cmd"] = cmd
        captured["cwd"] = cwd
        assert (staged / "INSTALLED").is_file()
        return SimpleNamespace(returncode=0)

    monkeypatch.setattr(_package.subprocess, "run", fake_run)

    packager.make_wix_installer()

    assert captured["cmd"][-1] == "LastFrontier_Win7.wix.json"
    config = json.loads((output_dir / "LastFrontier_Win7.wix.json").read_text(encoding="utf-8"))
    assert config["name_base"] == "LastFrontier_Win7"
    assert config["arch"] == 32
    assert config["startmenu_shortcut"] == "LastFrontier_Win7.exe"
