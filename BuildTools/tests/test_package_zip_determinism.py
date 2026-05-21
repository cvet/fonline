from __future__ import annotations

import os
import sys
import zipfile
from pathlib import Path


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
