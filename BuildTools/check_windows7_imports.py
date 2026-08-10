#!/usr/bin/python3

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct
import sys


FORBIDDEN_IMPORTS = frozenset({"CreateFile2"})


class PeFormatError(ValueError):
    pass


@dataclass(frozen=True)
class ImportedSymbol:
    library: str
    name: str


def _unpack_from(fmt: str, data: bytes, offset: int) -> tuple[int, ...]:
    size = struct.calcsize(fmt)
    if offset < 0 or offset + size > len(data):
        raise PeFormatError(f"PE structure at offset 0x{offset:X} exceeds the file")
    return struct.unpack_from(fmt, data, offset)


def _read_ascii_z(data: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        raise PeFormatError(f"PE string offset 0x{offset:X} exceeds the file")
    end = data.find(b"\0", offset)
    if end < 0:
        raise PeFormatError(f"PE string at offset 0x{offset:X} is not terminated")
    try:
        return data[offset:end].decode("ascii")
    except UnicodeDecodeError as error:
        raise PeFormatError(f"PE string at offset 0x{offset:X} is not ASCII") from error


def read_imports(binary: Path) -> set[ImportedSymbol]:
    data = binary.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise PeFormatError("missing DOS header")

    (pe_offset,) = _unpack_from("<I", data, 0x3C)
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise PeFormatError("missing PE signature")

    coff_offset = pe_offset + 4
    (_, section_count, _, _, _, optional_header_size, _) = _unpack_from("<HHIIIHH", data, coff_offset)
    optional_offset = coff_offset + 20
    (optional_magic,) = _unpack_from("<H", data, optional_offset)
    if optional_magic == 0x10B:
        number_of_directories_offset = optional_offset + 92
        directories_offset = optional_offset + 96
        thunk_format = "<I"
        thunk_size = 4
        ordinal_flag = 0x80000000
    elif optional_magic == 0x20B:
        number_of_directories_offset = optional_offset + 108
        directories_offset = optional_offset + 112
        thunk_format = "<Q"
        thunk_size = 8
        ordinal_flag = 0x8000000000000000
    else:
        raise PeFormatError(f"unsupported optional-header magic 0x{optional_magic:X}")

    (number_of_directories,) = _unpack_from("<I", data, number_of_directories_offset)
    if number_of_directories <= 1:
        return set()
    (import_rva, import_size) = _unpack_from("<II", data, directories_offset + 8)
    if import_rva == 0 or import_size == 0:
        return set()

    sections_offset = optional_offset + optional_header_size
    sections: list[tuple[int, int, int]] = []
    for section_index in range(section_count):
        section_offset = sections_offset + section_index * 40
        (virtual_size, virtual_address, raw_size, raw_offset) = _unpack_from("<IIII", data, section_offset + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_offset))

    def rva_to_offset(rva: int) -> int:
        for virtual_address, mapped_size, raw_offset in sections:
            if virtual_address <= rva < virtual_address + mapped_size:
                return raw_offset + rva - virtual_address
        raise PeFormatError(f"RVA 0x{rva:X} is outside mapped sections")

    imports: set[ImportedSymbol] = set()
    descriptor_index = 0
    while True:
        descriptor_offset = rva_to_offset(import_rva + descriptor_index * 20)
        descriptor = _unpack_from("<IIIII", data, descriptor_offset)
        if descriptor == (0, 0, 0, 0, 0):
            break

        original_first_thunk, _, _, name_rva, first_thunk = descriptor
        library = _read_ascii_z(data, rva_to_offset(name_rva))
        thunk_rva = original_first_thunk or first_thunk
        thunk_index = 0
        while True:
            thunk_offset = rva_to_offset(thunk_rva + thunk_index * thunk_size)
            (thunk_value,) = _unpack_from(thunk_format, data, thunk_offset)
            if thunk_value == 0:
                break
            if not thunk_value & ordinal_flag:
                import_name_offset = rva_to_offset(thunk_value) + 2
                imports.add(ImportedSymbol(library=library, name=_read_ascii_z(data, import_name_offset)))
            thunk_index += 1

        descriptor_index += 1

    return imports


def check_binary(binary: Path) -> list[ImportedSymbol]:
    return sorted((item for item in read_imports(binary) if item.name in FORBIDDEN_IMPORTS), key=lambda item: (item.library, item.name))


def main() -> int:
    parser = argparse.ArgumentParser(description="Reject CreateFile2 from Windows 7-compatible PE binaries")
    parser.add_argument("binaries", nargs="+", type=Path)
    args = parser.parse_args()

    failed = False
    for binary in args.binaries:
        try:
            forbidden = check_binary(binary)
        except (OSError, PeFormatError) as error:
            print(f"[Windows7Imports] {binary}: unable to inspect PE imports: {error}", file=sys.stderr)
            failed = True
            continue

        if forbidden:
            failed = True
            details = ", ".join(f"{item.library}!{item.name}" for item in forbidden)
            print(f"[Windows7Imports] {binary}: unsupported imports: {details}", file=sys.stderr)
        else:
            print(f"[Windows7Imports] {binary}: compatible")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
