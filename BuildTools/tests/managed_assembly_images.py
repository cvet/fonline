"""Synthetic managed assembly images for the payload selection tests.

The builder lays out the smallest PE image a metadata reader accepts: one section holding the CLI header and a
metadata root with the tables and strings streams, written per ECMA-335 partition II.
"""
from __future__ import annotations

import hashlib
import struct
from pathlib import Path


def make_assembly(
    name: str,
    references: list[str] | tuple[str, ...] = (),
    *,
    pe32_plus: bool = False,
    large_heaps: bool = False,
    type_ref_rows: int = 0,
) -> bytes:
    strings = bytearray(b'\0')

    def add_string(value: str) -> int:
        offset = len(strings)
        strings.extend(value.encode('utf-8') + b'\0')
        return offset

    name_index = add_string(name)
    reference_indices = [add_string(reference) for reference in references]
    pad(strings)

    index = '<I' if large_heaps else '<H'
    wide_resolution_scope = max(len(references), type_ref_rows) >= 1 << 14
    valid = (1 << 0x00) | (1 << 0x20) | (1 << 0x23) | ((1 << 0x01) if type_ref_rows else 0)
    tables = bytearray(struct.pack('<IBBBBQQ', 0, 2, 0, 0x07 if large_heaps else 0, 1, valid, 0))
    tables += struct.pack('<I', 1)
    if type_ref_rows:
        tables += struct.pack('<I', type_ref_rows)
    tables += struct.pack('<II', 1, len(references))
    # Module: Generation, Name, Mvid, EncId, EncBaseId
    tables += struct.pack('<H', 0) + struct.pack(index, name_index) + struct.pack(index, 0) * 3
    # TypeRef: ResolutionScope coded index, TypeName, TypeNamespace
    type_ref_row = struct.pack('<I' if wide_resolution_scope else '<H', 0) + struct.pack(index, name_index) + struct.pack(index, 0)
    tables += type_ref_row * type_ref_rows
    # Assembly: HashAlgId, four version parts, Flags, PublicKey, Name, Culture
    tables += struct.pack('<IHHHHI', 0x8004, 1, 0, 0, 0, 0) + struct.pack(index, 0) + struct.pack(index, name_index) + struct.pack(index, 0)
    # AssemblyRef: four version parts, Flags, PublicKeyOrToken, Name, Culture, HashValue
    for reference_index in reference_indices:
        tables += struct.pack('<HHHHI', 1, 0, 0, 0, 0) + struct.pack(index, 0) + struct.pack(index, reference_index)
        tables += struct.pack(index, 0) * 2
    pad(tables)

    headers_size = 64
    metadata = bytearray(struct.pack('<IHHII', 0x424A5342, 1, 1, 0, 12) + b'v4.0.30319\0\0')
    metadata += struct.pack('<HH', 0, 2)
    metadata += struct.pack('<II', headers_size, len(tables)) + b'#~\0\0'
    metadata += struct.pack('<II', headers_size + len(tables), len(strings)) + b'#Strings\0\0\0\0'
    assert len(metadata) == headers_size
    metadata += tables + strings

    section_rva = 0x2000
    section_file_offset = 0x200
    cli_header_size = 72
    section = bytearray(struct.pack('<IHHII', cli_header_size, 2, 5, section_rva + cli_header_size, len(metadata)))
    section += bytes(cli_header_size - len(section)) + metadata

    optional_header_size = 240 if pe32_plus else 224
    optional_header = 0x80 + 24
    image = bytearray(section_file_offset)
    image[:2] = b'MZ'
    struct.pack_into('<I', image, 0x3C, 0x80)
    struct.pack_into('<IHH', image, 0x80, 0x00004550, 0x14C, 1)
    struct.pack_into('<H', image, 0x80 + 20, optional_header_size)
    struct.pack_into('<H', image, optional_header, 0x20B if pe32_plus else 0x10B)
    directory_count = optional_header + (108 if pe32_plus else 92)
    struct.pack_into('<I', image, directory_count, 16)
    struct.pack_into('<II', image, directory_count + 4 + 14 * 8, section_rva, cli_header_size)
    section_header = optional_header + optional_header_size
    struct.pack_into('<8sIIII', image, section_header, b'.text', len(section), section_rva, len(section), section_file_offset)
    return bytes(image + section)


def write_runtime_payload(runtime_dir: Path, assemblies: dict[str, list[str]], marker: bytes = b'') -> None:
    """Stage class libraries and their manifest the way managed_runtime_payload.stage_payload does."""
    class_library_dir = runtime_dir / 'lib' / 'netcoreapp'
    class_library_dir.mkdir(parents=True, exist_ok=True)
    manifest_lines = []
    for name in sorted(assemblies):
        content = make_assembly(name, assemblies[name]) + marker
        (class_library_dir / (name + '.dll')).write_bytes(content)
        manifest_lines.append(f'{hashlib.sha256(content).hexdigest()}  lib/netcoreapp/{name}.dll')
    (runtime_dir / 'runtime.manifest').write_text('\n'.join(manifest_lines) + '\n', encoding='utf-8', newline='\n')


def pad(data: bytearray) -> None:
    data.extend(bytes(-len(data) % 4))
