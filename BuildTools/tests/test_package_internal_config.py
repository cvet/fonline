from __future__ import annotations

from pathlib import Path
import sys


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
