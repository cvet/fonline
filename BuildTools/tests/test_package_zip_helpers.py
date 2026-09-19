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


def _make_resource_packager() -> _package.Packager:
    packager = _package.Packager.__new__(_package.Packager)
    packager.compress_level = 6
    packager.resource_pack_min_compress_gain = 5
    packager.resource_archive_paths = {}
    return packager


def test_resource_pack_write_validates_finished_archive(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source_path = base_path / "payload.txt"
    source_path.write_text("payload", encoding="utf-8")
    archive_path = tmp_path / "resources.fores"
    validated: list[tuple[Path, list[str]]] = []

    def record_validation(path: str | Path, entries: list[str]) -> None:
        validated.append((Path(path), entries))

    monkeypatch.setattr(_package, "validate_resource_pack", record_validation)

    packager = _make_resource_packager()
    packager.write_resource_pack_files(str(archive_path), str(base_path), [str(source_path)])

    assert validated == [(archive_path, ["payload.txt"])]


def test_resource_archive_cache_key_uses_names_content_and_compression(tmp_path: Path) -> None:
    source = tmp_path / "payload.txt"
    source.write_bytes(b"payload")
    packager = _make_resource_packager()

    baseline = packager.resource_archive_cache_key([("payload.txt", str(source))])
    os.utime(source, (1_900_000_000, 1_900_000_000))
    assert packager.resource_archive_cache_key([("payload.txt", str(source))]) == baseline
    assert packager.resource_archive_cache_key([("renamed.txt", str(source))]) != baseline

    packager.compress_level = 7
    assert packager.resource_archive_cache_key([("payload.txt", str(source))]) != baseline

    packager.compress_level = 6
    packager.resource_pack_min_compress_gain = 6
    assert packager.resource_archive_cache_key([("payload.txt", str(source))]) != baseline

    packager.resource_pack_min_compress_gain = 5
    source.write_bytes(b"changed")
    assert packager.resource_archive_cache_key([("payload.txt", str(source))]) != baseline


def test_resource_archive_cache_hit_skips_compression(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"payload")
    output = tmp_path / "cached.fores"
    packager = _make_resource_packager()
    real_write = _package.write_resource_pack

    def helper(action: str, _key: str, archive_path: str) -> int:
        assert action == "restore"
        real_write(archive_path, [("payload.txt", str(source))], 6, 5)
        return 0

    monkeypatch.setattr(packager, "run_resource_archive_cache_helper", helper)
    monkeypatch.setattr(
        _package, "write_resource_pack",
        lambda *_: pytest.fail("a cache hit must not compress the resource files"))

    packager.write_resource_pack_files(str(output), str(base_path), [str(source)])

    _package.validate_resource_pack(output, ["payload.txt"])


def test_resource_archive_cache_miss_is_stored_after_validation(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"payload")
    output = tmp_path / "created.fores"
    packager = _make_resource_packager()
    actions: list[str] = []

    def helper(action: str, _key: str, archive_path: str) -> int:
        actions.append(action)
        assert Path(archive_path) == output
        return _package.RESOURCE_ARCHIVE_CACHE_MISS if action == "restore" else 0

    monkeypatch.setattr(packager, "run_resource_archive_cache_helper", helper)
    packager.write_resource_pack_files(str(output), str(base_path), [str(source)])

    assert actions == ["restore", "store"]
    _package.validate_resource_pack(output, ["payload.txt"])


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

    status = packager.run_resource_archive_cache_helper("restore", "a" * 64, str(tmp_path / "pack.fores"))

    assert status == _package.RESOURCE_ARCHIVE_CACHE_MISS
    assert json.loads(report.read_text(encoding="utf-8")) == [
        "restore", "--key", "a" * 64, "--archive", str(tmp_path / "pack.fores")]


def test_unavailable_resource_archive_cache_is_not_probed_again(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    helper = tmp_path / "helper.py"
    helper.write_text("raise SystemExit(3)\n", encoding="utf-8")
    monkeypatch.setenv(_package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, str(helper))
    packager = _package.Packager.__new__(_package.Packager)

    first_status = packager.run_resource_archive_cache_helper(
        "restore", "a" * 64, str(tmp_path / "first.fores"))
    second_status = packager.run_resource_archive_cache_helper(
        "restore", "b" * 64, str(tmp_path / "second.fores"))

    assert first_status == _package.RESOURCE_ARCHIVE_CACHE_UNAVAILABLE
    assert second_status is None


def test_unavailable_resource_archive_cache_falls_back_to_compression(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"payload")
    output = tmp_path / "created.fores"
    helper = tmp_path / "helper.py"
    helper.write_text("raise SystemExit(3)\n", encoding="utf-8")
    monkeypatch.setenv(_package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, str(helper))
    packager = _make_resource_packager()

    packager.write_resource_pack_files(str(output), str(base_path), [str(source)])

    _package.validate_resource_pack(output, ["payload.txt"])


def test_repeated_resource_archive_in_one_package_is_copied_locally(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"payload")
    first = tmp_path / "first.fores"
    second = tmp_path / "second.fores"
    packager = _make_resource_packager()

    packager.write_resource_pack_files(str(first), str(base_path), [str(source)])
    monkeypatch.setattr(
        _package, "write_resource_pack",
        lambda *_: pytest.fail("an identical archive in one package must be copied locally"))
    packager.write_resource_pack_files(str(second), str(base_path), [str(source)])

    assert second.read_bytes() == first.read_bytes()


def test_overwritten_resource_archive_does_not_leave_a_stale_local_key(tmp_path: Path) -> None:
    base_path = tmp_path / "Pack"
    base_path.mkdir()
    source = base_path / "payload.txt"
    source.write_bytes(b"first")
    output = tmp_path / "resources.fores"
    packager = _make_resource_packager()
    first_key = packager.resource_archive_cache_key([("payload.txt", str(source))])

    packager.write_resource_pack_files(str(output), str(base_path), [str(source)])
    source.write_bytes(b"second")
    second_key = packager.resource_archive_cache_key([("payload.txt", str(source))])
    packager.write_resource_pack_files(str(output), str(base_path), [str(source)])

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
