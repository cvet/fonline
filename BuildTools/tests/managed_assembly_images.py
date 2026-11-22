"""Synthetic managed assembly images for the payload selection tests.

The builder lays out the smallest PE image a metadata reader accepts: one section holding the CLI header and a
metadata root with the tables, strings and blob streams, written per ECMA-335 partition II.
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
    informational_version: str | None = None,
) -> bytes:
    strings = bytearray(b'\0')
    blobs = bytearray(b'\0')

    def add_string(value: str) -> int:
        offset = len(strings)
        strings.extend(value.encode('utf-8') + b'\0')
        return offset

    name_index = add_string(name)
    reference_indices = [add_string(reference) for reference in references]
    index = '<I' if large_heaps else '<H'
    attribute_rows = 1 if informational_version is not None else 0
    all_type_ref_rows = type_ref_rows + attribute_rows

    def coded(value: int, tag_bits: int, largest_table_rows: int) -> bytes:
        return struct.pack('<I' if largest_table_rows >= 1 << (16 - tag_bits) else '<H', value)

    type_ref_row = coded(0, 2, max(len(references), all_type_ref_rows)) + struct.pack(index, name_index) + struct.pack(index, 0)
    type_refs = type_ref_row * type_ref_rows
    member_refs = b''
    custom_attributes = b''
    if informational_version is not None:
        # The attribute names its constructor through a MemberRef on a TypeRef, as every library built against
        # the reference assemblies does, and carries the version as the constructor's one string argument
        type_refs += coded(0, 2, max(len(references), all_type_ref_rows)) + struct.pack(index, add_string('AssemblyInformationalVersionAttribute'))
        type_refs += struct.pack(index, add_string('System.Reflection'))
        member_refs = coded((all_type_ref_rows << 3) | 1, 3, all_type_ref_rows) + struct.pack(index, add_string('.ctor')) + struct.pack(index, 0)
        version = informational_version.encode('utf-8')
        value_index = len(blobs)
        blobs += bytes([len(version) + 5, 0x01, 0x00, len(version)]) + version + b'\0\0'
        custom_attributes = coded((1 << 5) | 14, 5, max(len(references), all_type_ref_rows)) + coded((1 << 3) | 3, 3, 1) + struct.pack(index, value_index)
    pad(strings)
    pad(blobs)

    present = [0x00, 0x20, 0x23]
    if all_type_ref_rows:
        present.append(0x01)
    if attribute_rows:
        present.extend((0x0A, 0x0C))
    row_counts = {0x00: 1, 0x01: all_type_ref_rows, 0x0A: attribute_rows, 0x0C: attribute_rows, 0x20: 1, 0x23: len(references)}
    valid = sum(1 << table for table in present)
    tables = bytearray(struct.pack('<IBBBBQQ', 0, 2, 0, 0x07 if large_heaps else 0, 1, valid, 0))
    for table in sorted(present):
        tables += struct.pack('<I', row_counts[table])
    # Module: Generation, Name, Mvid, EncId, EncBaseId
    tables += struct.pack('<H', 0) + struct.pack(index, name_index) + struct.pack(index, 0) * 3
    # TypeRef: ResolutionScope, TypeName, TypeNamespace; MemberRef: Class, Name, Signature; CustomAttribute: Parent, Type, Value
    tables += type_refs + member_refs + custom_attributes
    # Assembly: HashAlgId, four version parts, Flags, PublicKey, Name, Culture
    tables += struct.pack('<IHHHHI', 0x8004, 1, 0, 0, 0, 0) + struct.pack(index, 0) + struct.pack(index, name_index) + struct.pack(index, 0)
    # AssemblyRef: four version parts, Flags, PublicKeyOrToken, Name, Culture, HashValue
    for reference_index in reference_indices:
        tables += struct.pack('<HHHHI', 1, 0, 0, 0, 0) + struct.pack(index, 0) + struct.pack(index, reference_index)
        tables += struct.pack(index, 0) * 2
    pad(tables)

    headers_size = 80
    metadata = bytearray(struct.pack('<IHHII', 0x424A5342, 1, 1, 0, 12) + b'v4.0.30319\0\0')
    metadata += struct.pack('<HH', 0, 3)
    metadata += struct.pack('<II', headers_size, len(tables)) + b'#~\0\0'
    metadata += struct.pack('<II', headers_size + len(tables), len(strings)) + b'#Strings\0\0\0\0'
    metadata += struct.pack('<II', headers_size + len(tables) + len(strings), len(blobs)) + b'#Blob\0\0\0'
    assert len(metadata) == headers_size
    metadata += tables + strings + blobs

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


def write_runtime_payload(
    runtime_dir: Path, assemblies: dict[str, list[str]], marker: bytes = b'', informational_version: str | None = None,
) -> None:
    """Stage class libraries and their manifest the way managed_runtime_payload.stage_payload does."""
    class_library_dir = runtime_dir / 'lib' / 'netcoreapp'
    class_library_dir.mkdir(parents=True, exist_ok=True)
    manifest_lines = []
    for name in sorted(assemblies):
        content = make_assembly(name, assemblies[name], informational_version=informational_version) + marker
        (class_library_dir / (name + '.dll')).write_bytes(content)
        manifest_lines.append(f'{hashlib.sha256(content).hexdigest()}  lib/netcoreapp/{name}.dll')
    (runtime_dir / 'runtime.manifest').write_text('\n'.join(manifest_lines) + '\n', encoding='utf-8', newline='\n')


def pad(data: bytearray) -> None:
    data.extend(bytes(-len(data) % 4))
