from __future__ import annotations

import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import foconfig
import package


def make_packager(tmp_path: Path, icon: str | None) -> package.Packager:
    lines = []
    if icon is not None:
        lines.append(f"Android.Icon = {icon}")
    config = foconfig.ConfigParser()
    config.loadFromLines(lines)
    packager = package.Packager.__new__(package.Packager)
    packager.fomain = config
    packager.target_config = None
    packager.args = SimpleNamespace(maincfg=str(tmp_path / "Game.fomain"), target="Client")
    packager.target_output_path = str(tmp_path / "Output")
    return packager


@pytest.mark.parametrize("icon", [None, ""])
def test_omitted_icon_preserves_platform_launcher(tmp_path: Path, icon: str | None) -> None:
    packager = make_packager(tmp_path, icon)
    launcher = Path(packager.target_output_path) / "app/src/main/res/mipmap-mdpi/ic_launcher.png"
    launcher.parent.mkdir(parents=True)
    launcher.write_bytes(b"platform launcher")
    packager.patch_android_icon()
    assert launcher.read_bytes() == b"platform launcher"
    assert len(list(launcher.parents[1].rglob("ic_launcher.png"))) == 1


def test_explicit_icon_is_copied_to_all_launcher_densities(tmp_path: Path) -> None:
    png = bytes.fromhex("89504e470d0a1a0a") + b"project icon"
    (tmp_path / "Project.png").write_bytes(png)
    packager = make_packager(tmp_path, "Project.png")
    packager.patch_android_icon()
    for density in package.ANDROID_ICON_DENSITY_DIRS:
        launcher = Path(packager.target_output_path) / f"app/src/main/res/{density}/ic_launcher.png"
        assert launcher.read_bytes() == png

