from __future__ import annotations

from pathlib import Path
import stat
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


CAPACITY = 400


def make_binary(path: Path) -> None:
    filler = b"0" * (CAPACITY - len(_package.INTERNAL_CONFIG_MARKER) - len(_package.INTERNAL_CONFIG_END_MARKER))
    path.write_bytes(b"head" + _package.INTERNAL_CONFIG_MARKER + filler + _package.INTERNAL_CONFIG_END_MARKER + b"tail")


def read_patched_config(path: Path) -> str:
    content = path.read_bytes()
    assert content.startswith(b"head") and content.endswith(b"tail")
    return content[len(b"head"):len(b"head") + CAPACITY].rstrip(b"#").decode("utf-8")


def test_variant_config_stays_a_top_level_setting_before_the_baked_pack_sections(tmp_path: Path) -> None:
    # The baked config closes with its [ResourcePack] sections, so a line appended after them would be read
    # as a key of the last pack instead of as the variant's setting
    binary = tmp_path / "LastFrontier_OpenGL.exe"
    make_binary(binary)
    packager = _package.Packager.__new__(_package.Packager)
    packager.config_data = b"Common.GameName=LastFrontier\n[ResourcePack]\nName=Art\nClientOnly=1\n"

    packager.patch_config(str(binary), "Render.ForceOpenGL=1")

    text = read_patched_config(binary)
    config = _package.load_config_from_data(text.encode("utf-8"))
    assert text.startswith("Render.ForceOpenGL=1\nCommon.GameName=LastFrontier\n")
    assert config.mainSection().getStr("Render.ForceOpenGL") == "1"
    assert config.mainSection().getStr("Common.GameName") == "LastFrontier"
    assert [section.getStr("Name") for section in config.getSections("ResourcePack")] == ["Art"]


def test_config_without_variant_data_is_written_unchanged(tmp_path: Path) -> None:
    binary = tmp_path / "LastFrontier.exe"
    make_binary(binary)
    packager = _package.Packager.__new__(_package.Packager)
    packager.config_data = b"Common.GameName=LastFrontier\n[ResourcePack]\nName=Art\nClientOnly=1\n"

    packager.patch_config(str(binary))

    assert read_patched_config(binary) == packager.config_data.decode("utf-8")


def test_all_reserved_fields_preserve_surrounding_native_binary_bytes(tmp_path: Path) -> None:
    binary = tmp_path / "server"
    prefix = b"native-prefix\x00" + b"p" * (1024 * 1024)
    separator = b"\x00native-separator\x00"
    suffix = b"native-suffix\x00" + b"s" * (1024 * 1024)
    config_filler = b"0" * (CAPACITY - len(_package.INTERNAL_CONFIG_MARKER) - len(_package.INTERNAL_CONFIG_END_MARKER))
    name_filler = b"\x00" * (_package.PACKAGED_BUILD_NAME_CAPACITY - len(_package.PACKAGED_BUILD_NAME_MARKER))
    binary.write_bytes(
        prefix + _package.make_embedded_marker(10000) + separator
        + _package.INTERNAL_CONFIG_MARKER + config_filler + _package.INTERNAL_CONFIG_END_MARKER
        + separator + _package.PACKAGED_BUILD_NAME_MARKER + name_filler + suffix
    )
    mode_before = stat.S_IMODE(binary.stat().st_mode)
    packager = _package.Packager.__new__(_package.Packager)
    packager.pack_args = {"Raw"}
    packager.embedded_data = b"embedded-archive-fixture"
    packager.config_data = b"Common.GameName=Fixture\n[ResourcePack]\nName=Art\n"

    packager.patch_packaged_binary(str(binary), "Fixture_Server", "Render.ForceOpenGL=1")

    expected_config = b"Render.ForceOpenGL=1\n" + packager.config_data
    expected = (
        prefix + packager.embedded_data.ljust(10000, b"#") + separator
        + expected_config.ljust(CAPACITY, b"#") + separator
        + b"Fixture_Server".ljust(_package.PACKAGED_BUILD_NAME_CAPACITY, b"\x00") + suffix
    )
    assert binary.read_bytes() == expected
    assert stat.S_IMODE(binary.stat().st_mode) == mode_before


def test_only_the_first_matching_reserved_field_is_patched(tmp_path: Path) -> None:
    binary = tmp_path / "duplicate-marker"
    binary.write_bytes(b"prefixFIELDseparatorFIELDsuffix")

    _package.patch_data(binary, b"FIELD", b"xy", 5)

    assert binary.read_bytes() == b"prefixxy###separatorFIELDsuffix"


@pytest.mark.parametrize(
    "marker,payload,capacity",
    [(b"FIELD", b"x", 32), (b"MISSING", b"x", 5), (b"FIELD", b"too-long", 5)],
    ids=["truncated-reservation", "missing-marker", "oversized-payload"],
)
def test_invalid_reserved_field_does_not_mutate_the_binary(
    tmp_path: Path, marker: bytes, payload: bytes, capacity: int
) -> None:
    binary = tmp_path / "invalid-field"
    original = b"prefixFIELDsuffix"
    binary.write_bytes(original)

    with pytest.raises(AssertionError):
        _package.patch_data(binary, marker, payload, capacity)

    assert binary.read_bytes() == original
