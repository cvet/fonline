from __future__ import annotations

import os
import sys
import warnings
import zipfile
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


def _write_resource_zip(archive_path: Path, base_path: Path, files: list[Path]) -> bytes:
    packager = _package.Packager.__new__(_package.Packager)
    packager.zip_compress_level = 6
    packager.write_files_zip(str(archive_path), str(base_path), [str(path) for path in files])
    return archive_path.read_bytes()


def test_resource_pack_zip_ignores_input_mtime_and_file_order(tmp_path: Path) -> None:
    base_path = tmp_path / "Pack"
    nested_path = base_path / "nested"
    nested_path.mkdir(parents=True)

    first_file = base_path / "z.txt"
    second_file = nested_path / "b.txt"
    first_file.write_text("same content\n", encoding="utf-8")
    second_file.write_text("more content\n", encoding="utf-8")

    first_archive = tmp_path / "first.zip"
    second_archive = tmp_path / "second.zip"

    first_bytes = _write_resource_zip(first_archive, base_path, [second_file, first_file])

    os.utime(first_file, (1_800_000_000, 1_800_000_000))
    os.utime(second_file, (1_900_000_000, 1_900_000_000))

    second_bytes = _write_resource_zip(second_archive, base_path, [first_file, second_file])

    assert second_bytes == first_bytes

    with zipfile.ZipFile(second_archive) as archive:
        infos = archive.infolist()
        assert [info.filename for info in infos] == ["nested/b.txt", "z.txt"]
        assert {info.date_time for info in infos} == {(1980, 1, 1, 0, 0, 0)}
        assert {info.create_system for info in infos} == {3}
        assert {info.external_attr for info in infos} == {0o644 << 16}


def test_single_zip_merge_coalesces_identical_package_entries(tmp_path: Path) -> None:
    first_part = tmp_path / "first"
    second_part = tmp_path / "second"
    first_part.mkdir()
    second_part.mkdir()

    (first_part / "shared.dll").write_bytes(b"same runtime")
    (first_part / "first.exe").write_bytes(b"first")
    (second_part / "shared.dll").write_bytes(b"same runtime")
    (second_part / "second.exe").write_bytes(b"second")

    archive_path = tmp_path / "package.zip"
    _package.make_zip(archive_path, first_part, 6)
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        _package.make_zip(archive_path, second_part, 6, "a")

    with zipfile.ZipFile(archive_path) as archive:
        archive_names = [info.filename for info in archive.infolist()]
        assert len(archive_names) == len(set(archive_names)) == 3
        assert set(archive_names) == {"shared.dll", "first.exe", "second.exe"}
        assert archive.read("shared.dll") == b"same runtime"


def test_single_zip_merge_rejects_conflicting_package_entries(tmp_path: Path) -> None:
    first_part = tmp_path / "first"
    second_part = tmp_path / "second"
    first_part.mkdir()
    second_part.mkdir()

    (first_part / "shared.dll").write_bytes(b"first runtime")
    (second_part / "shared.dll").write_bytes(b"second runtime")

    archive_path = tmp_path / "package.zip"
    _package.make_zip(archive_path, first_part, 6)

    with pytest.raises(AssertionError, match="Conflicting zip entry while merging package parts: shared.dll"):
        _package.make_zip(archive_path, second_part, 6, "a")
