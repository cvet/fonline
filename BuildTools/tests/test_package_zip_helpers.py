from __future__ import annotations

import io
import os
import sys
import warnings
import zipfile
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


def test_resource_pack_validation_rejects_corrupted_entry(tmp_path: Path) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source_path = base_path / "payload.txt"
    source_path.write_bytes(b"resource payload" * 64)
    archive_path = tmp_path / "resources.zip"

    packager = _package.Packager.__new__(_package.Packager)
    packager.compress_level = 6

    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        packager.write_stable_zip_entry(archive, str(source_path), "payload.txt")

    with zipfile.ZipFile(archive_path) as archive:
        info = archive.getinfo("payload.txt")
        with archive_path.open("r+b") as archive_file:
            archive_file.seek(info.header_offset)
            local_header = archive_file.read(30)
            name_length = int.from_bytes(local_header[26:28], "little")
            extra_length = int.from_bytes(local_header[28:30], "little")
            corrupt_offset = info.header_offset + 30 + name_length + extra_length + info.compress_size // 2
            archive_file.seek(corrupt_offset)
            original = archive_file.read(1)
            archive_file.seek(corrupt_offset)
            archive_file.write(bytes([original[0] ^ 0xFF]))

    with pytest.raises(AssertionError, match="Resource pack validation failed.*payload.txt"):
        _package.validate_resource_zip(archive_path, ["payload.txt"])


def test_embedded_pack_is_validated_before_it_is_embedded(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    nested_path = base_path / "nested"
    nested_path.mkdir(parents=True)
    first_file = base_path / "z.txt"
    second_file = nested_path / "b.txt"
    first_file.write_text("embedded payload", encoding="utf-8")
    second_file.write_text("nested payload", encoding="utf-8")
    validated: list[list[str]] = []

    packager = _package.Packager.__new__(_package.Packager)
    packager.compress_level = 6

    real_validate = _package.validate_resource_zip

    def record_validation(source: object, entries: list[str], description: str | None = None) -> None:
        validated.append(entries)
        real_validate(source, entries, description)

    monkeypatch.setattr(_package, "validate_resource_zip", record_validation)

    data = packager.make_embedded_pack([str(second_file), str(first_file)], str(base_path))

    assert validated == [["nested/b.txt", "z.txt"]]

    with zipfile.ZipFile(io.BytesIO(data[4:])) as archive:
        assert [info.filename for info in archive.infolist()] == ["nested/b.txt", "z.txt"]


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
