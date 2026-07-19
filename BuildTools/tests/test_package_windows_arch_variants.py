from __future__ import annotations

from pathlib import Path
import sys
from types import SimpleNamespace

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


def make_packager(tmp_path: Path, arch: str, binary_output_postfix: str = "") -> _package.Packager:
    packager = _package.Packager.__new__(_package.Packager)
    packager.args = SimpleNamespace(
        output=str(tmp_path),
        devname="LF",
        target="Client",
        config="PublicGame",
        platform="Windows",
        arch=arch,
        binary_output_postfix=binary_output_postfix,
    )
    packager.output_path = str(tmp_path)
    packager.pack_args = set()

    return packager


def test_win32_win7_resolves_arch_and_uses_explicit_postfix(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win32-win7", "Win7")

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-PublicGame-Win7")
    assert packager.build_binary_entry("win32-win7", _package.BinaryVariant()) == "Client-Windows-win32-Win7"


def test_win64_win7_resolves_arch_and_uses_explicit_postfix(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win64-win7", "Win7")

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-PublicGame-Win7")
    assert packager.build_binary_entry("win64-win7", _package.BinaryVariant()) == "Client-Windows-win64-Win7"


@pytest.mark.parametrize(
    ("platform", "binary_arch"),
    [("win32", "win32"), ("win32-clang", "win32"), ("win64", "win64"), ("win64-clang", "win64")],
)
def test_windows_build_platform_uses_canonical_binary_arch(tmp_path: Path, platform: str, binary_arch: str) -> None:
    packager = make_packager(tmp_path, platform)

    assert packager.build_binary_entry(platform, _package.BinaryVariant()) == f"Client-Windows-{binary_arch}"


def test_windows7_arch_does_not_imply_postfix(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win32-win7")

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-PublicGame")
    assert packager.build_binary_entry("win32-win7", _package.BinaryVariant()) == "Client-Windows-win32"


def test_postfix_matching_config_does_not_duplicate_output_name(tmp_path: Path) -> None:
    packager = make_packager(tmp_path, "win64", "Steam")
    packager.args.config = "Steam"

    assert packager.build_target_output_path() == str(tmp_path / "LF-Client-Steam")
