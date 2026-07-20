from __future__ import annotations

from pathlib import Path
import struct
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import check_windows7_imports as _check  # noqa: E402


def _make_pe(import_name: str, is_64bit: bool) -> bytes:
    data = bytearray(0x400)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    machine = 0x8664 if is_64bit else 0x14C
    optional_header_size = 0xF0 if is_64bit else 0xE0
    optional_magic = 0x20B if is_64bit else 0x10B
    number_of_directories_offset = 108 if is_64bit else 92
    directories_offset = 112 if is_64bit else 96
    thunk_format = "<QQ" if is_64bit else "<II"
    struct.pack_into("<HHIIIHH", data, 0x84, machine, 1, 0, 0, 0, optional_header_size, 0x102)

    optional_offset = 0x98
    struct.pack_into("<H", data, optional_offset, optional_magic)
    struct.pack_into("<I", data, optional_offset + number_of_directories_offset, 16)
    struct.pack_into("<II", data, optional_offset + directories_offset + 8, 0x1000, 40)

    section_offset = optional_offset + optional_header_size
    data[section_offset : section_offset + 8] = b".rdata\0\0"
    struct.pack_into("<IIII", data, section_offset + 8, 0x200, 0x1000, 0x200, 0x200)

    struct.pack_into("<IIIII", data, 0x200, 0x1040, 0, 0, 0x1030, 0x1050)
    data[0x230:0x23D] = b"KERNEL32.dll\0"
    struct.pack_into(thunk_format, data, 0x240, 0x1060, 0)
    struct.pack_into(thunk_format, data, 0x250, 0x1060, 0)
    struct.pack_into("<H", data, 0x260, 0)
    encoded_name = import_name.encode("ascii") + b"\0"
    data[0x262 : 0x262 + len(encoded_name)] = encoded_name
    return bytes(data)


@pytest.mark.parametrize("is_64bit", [False, True])
def test_read_imports_returns_named_import(is_64bit: bool, tmp_path: Path) -> None:
    binary = tmp_path / "fixture.exe"
    binary.write_bytes(_make_pe("CreateFileW", is_64bit))

    assert _check.read_imports(binary) == {_check.ImportedSymbol(library="KERNEL32.dll", name="CreateFileW")}
    assert _check.check_binary(binary) == []


@pytest.mark.parametrize("is_64bit", [False, True])
def test_check_binary_rejects_windows_8_import(is_64bit: bool, tmp_path: Path) -> None:
    binary = tmp_path / "fixture.exe"
    binary.write_bytes(_make_pe("CreateFile2", is_64bit))

    assert _check.check_binary(binary) == [_check.ImportedSymbol(library="KERNEL32.dll", name="CreateFile2")]
