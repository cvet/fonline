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


def _uleb(value: int) -> bytes:
    data: list[int] = []
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            data.append(byte | 0x80)
        else:
            data.append(byte)
            return bytes(data)


def _wasm_name(value: str) -> bytes:
    data = value.encode("utf-8")
    return _uleb(len(data)) + data


def _wasm_section(section_id: int, payload: bytes) -> bytes:
    return bytes([section_id]) + _uleb(len(payload)) + payload


def test_wasm_manifest_parser_extracts_exports_and_imports(tmp_path: Path) -> None:
    wasm = b"\x00asm" + b"\x01\x00\x00\x00"
    wasm += _wasm_section(1, b"\x04\x60\x01\x7f\x01\x7f\x60\x01\x7c\x00\x60\x00\x01\x7f\x60\x02\x7f\x7f\x00")
    wasm += _wasm_section(2, b"\x03" + _wasm_name("fonline") + _wasm_name("log_f64") + b"\x00\x01" + _wasm_name("fonline") + _wasm_name("get_side") + b"\x00\x02" + _wasm_name("fonline") + _wasm_name("log_utf8") + b"\x00\x03")
    wasm += _wasm_section(3, b"\x01\x00")
    wasm += _wasm_section(7, b"\x01" + _wasm_name("add") + b"\x00\x03")

    wasm_path = tmp_path / "math.client.wasm"
    wasm_path.write_bytes(wasm)

    manifest = _package.parse_wasm_manifest(
        wasm_path,
        _package.make_wasm_module_name(wasm_path.name),
        "WasmScripts/math.client.wasm",
        "math.client.wasm",
    )

    assert manifest == {
        "name": "math",
        "path": "WasmScripts/math.client.wasm",
        "sourcePath": "math.client.wasm",
        "imports": [
            {
                "module": "fonline",
                "name": "log_f64",
                "kind": "func",
                "params": ["f64"],
                "results": [],
            },
            {
                "module": "fonline",
                "name": "get_side",
                "kind": "func",
                "params": [],
                "results": ["i32"],
            },
            {
                "module": "fonline",
                "name": "log_utf8",
                "kind": "func",
                "params": ["i32", "i32"],
                "results": [],
            }
        ],
        "exports": [
            {
                "name": "add",
                "kind": "func",
                "params": ["i32"],
                "results": ["i32"],
            }
        ],
    }


def test_wasm_target_visibility_uses_side_suffixes() -> None:
    assert _package.is_wasm_visible_to_target("Scripts/shared.wasm", "Client")
    assert _package.is_wasm_visible_to_target("Scripts/shared.wasm", "Server")
    assert _package.is_wasm_visible_to_target("Scripts/shared.wasm", "Mapper")

    assert _package.is_wasm_visible_to_target("Scripts/ui.client.wasm", "Client")
    assert not _package.is_wasm_visible_to_target("Scripts/ui.client.wasm", "Server")
    assert not _package.is_wasm_visible_to_target("Scripts/ui.client.wasm", "Mapper")

    assert _package.is_wasm_visible_to_target("Scripts/rules.server.wasm", "Server")
    assert not _package.is_wasm_visible_to_target("Scripts/rules.server.wasm", "Client")

    assert _package.is_wasm_visible_to_target("Scripts/tools.mapper.wasm", "Mapper")
    assert not _package.is_wasm_visible_to_target("Scripts/tools.mapper.wasm", "Client")


def test_wasm_module_name_strips_side_suffixes_only() -> None:
    assert _package.make_wasm_module_name("math.wasm") == "math"
    assert _package.make_wasm_module_name("math.client.wasm") == "math"
    assert _package.make_wasm_module_name("math.server.wasm") == "math"
    assert _package.make_wasm_module_name("math.mapper.wasm") == "math"
    assert _package.make_wasm_module_name("math.preview.wasm") == "math.preview"


def test_wasm_debug_sidecars_use_adjacent_source_map(tmp_path: Path) -> None:
    wasm_path = tmp_path / "math.client.wasm"
    wasm_path.write_bytes(b"\x00asm\x01\x00\x00\x00")

    assert list(_package.iter_wasm_debug_sidecars(wasm_path)) == []

    source_map_path = tmp_path / "math.client.wasm.map"
    source_map_path.write_text("{}", encoding="utf-8")

    assert list(_package.iter_wasm_debug_sidecars(wasm_path)) == [source_map_path]
