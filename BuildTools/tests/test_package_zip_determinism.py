from __future__ import annotations

import io
import json
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


def test_resource_pack_validation_rejects_corrupted_entry(tmp_path: Path) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source_path = base_path / "payload.txt"
    source_path.write_bytes(b"resource payload" * 64)
    archive_path = tmp_path / "resources.zip"

    _write_resource_zip(archive_path, base_path, [source_path])

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


def test_resource_pack_write_validates_finished_archive(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source_path = base_path / "payload.txt"
    source_path.write_text("payload", encoding="utf-8")
    archive_path = tmp_path / "resources.zip"
    validated: list[tuple[Path, list[str]]] = []

    def record_validation(path: str | Path, entries: list[str]) -> None:
        validated.append((Path(path), entries))

    monkeypatch.setattr(_package, "validate_resource_zip", record_validation)

    _write_resource_zip(archive_path, base_path, [source_path])

    assert validated == [(archive_path, ["payload.txt"])]


def test_resource_archive_cache_key_uses_names_content_and_compression(tmp_path: Path) -> None:
    source = tmp_path / "payload.txt"
    source.write_bytes(b"payload")
    packager = _package.Packager.__new__(_package.Packager)
    packager.zip_compress_level = 6

    baseline = packager.resource_archive_cache_key([("payload.txt", str(source))])
    os.utime(source, (1_900_000_000, 1_900_000_000))
    assert packager.resource_archive_cache_key([("payload.txt", str(source))]) == baseline
    assert packager.resource_archive_cache_key([("renamed.txt", str(source))]) != baseline

    packager.zip_compress_level = 7
    assert packager.resource_archive_cache_key([("payload.txt", str(source))]) != baseline

    packager.zip_compress_level = 6
    source.write_bytes(b"changed")
    assert packager.resource_archive_cache_key([("payload.txt", str(source))]) != baseline


def test_resource_archive_cache_hit_skips_compression(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"payload")
    output = tmp_path / "cached.zip"
    packager = _package.Packager.__new__(_package.Packager)
    packager.zip_compress_level = 6
    packager.resource_archive_paths = {}

    def helper(action: str, _key: str, archive_path: str) -> int:
        assert action == "restore"
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("payload.txt", b"payload")
        return 0

    monkeypatch.setattr(packager, "run_resource_archive_cache_helper", helper)
    monkeypatch.setattr(
        packager, "write_stable_zip_entry",
        lambda *_: pytest.fail("a cache hit must not compress the resource files"))

    packager.write_files_zip(str(output), str(base_path), [str(source)])

    with zipfile.ZipFile(output) as archive:
        assert archive.read("payload.txt") == b"payload"


def test_resource_archive_cache_miss_is_stored_after_validation(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"payload")
    output = tmp_path / "created.zip"
    packager = _package.Packager.__new__(_package.Packager)
    packager.zip_compress_level = 6
    packager.resource_archive_paths = {}
    actions: list[str] = []

    def helper(action: str, _key: str, archive_path: str) -> int:
        actions.append(action)
        assert Path(archive_path) == output
        return _package.RESOURCE_ARCHIVE_CACHE_MISS if action == "restore" else 0

    monkeypatch.setattr(packager, "run_resource_archive_cache_helper", helper)
    packager.write_files_zip(str(output), str(base_path), [str(source)])

    assert actions == ["restore", "store"]
    with zipfile.ZipFile(output) as archive:
        assert archive.read("payload.txt") == b"payload"


def test_resource_archive_cache_helper_receives_the_generic_protocol(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    helper = tmp_path / "helper.py"
    report = tmp_path / "argv.json"
    helper.write_text(
        "import json, os, sys\n"
        "open(os.environ['HELPER_REPORT'], 'w', encoding='utf-8').write(json.dumps(sys.argv[1:]))\n"
        "raise SystemExit(2)\n",
        encoding="utf-8")
    monkeypatch.setenv(_package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, str(helper))
    monkeypatch.setenv("HELPER_REPORT", str(report))
    packager = _package.Packager.__new__(_package.Packager)

    status = packager.run_resource_archive_cache_helper("restore", "a" * 64, str(tmp_path / "pack.zip"))

    assert status == _package.RESOURCE_ARCHIVE_CACHE_MISS
    assert json.loads(report.read_text(encoding="utf-8")) == [
        "restore", "--key", "a" * 64, "--archive", str(tmp_path / "pack.zip")]


def test_repeated_resource_archive_in_one_package_is_copied_locally(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"payload")
    first = tmp_path / "first.zip"
    second = tmp_path / "second.zip"
    packager = _package.Packager.__new__(_package.Packager)
    packager.zip_compress_level = 6
    packager.resource_archive_paths = {}

    packager.write_files_zip(str(first), str(base_path), [str(source)])
    monkeypatch.setattr(
        packager, "write_stable_zip_entry",
        lambda *_: pytest.fail("an identical archive in one package must be copied locally"))
    packager.write_files_zip(str(second), str(base_path), [str(source)])

    assert second.read_bytes() == first.read_bytes()


def test_overwritten_resource_archive_does_not_leave_a_stale_local_key(tmp_path: Path) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"first")
    output = tmp_path / "resources.zip"
    packager = _package.Packager.__new__(_package.Packager)
    packager.zip_compress_level = 6
    packager.resource_archive_paths = {}
    first_key = packager.resource_archive_cache_key([("payload.txt", str(source))])

    packager.write_files_zip(str(output), str(base_path), [str(source)])
    source.write_bytes(b"second")
    second_key = packager.resource_archive_cache_key([("payload.txt", str(source))])
    packager.write_files_zip(str(output), str(base_path), [str(source)])

    assert first_key not in packager.resource_archive_paths
    assert packager.resource_archive_paths == {second_key: str(output)}


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
    packager.zip_compress_level = 6

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
