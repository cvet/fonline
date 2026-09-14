from __future__ import annotations

from pathlib import Path
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import managed_runtime_payload as payload


def make_pe(*, managed: bool) -> bytes:
    data = bytearray(512)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3C, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<H', data, 0x80 + 20, 224)
    optional_header = 0x80 + 24
    struct.pack_into('<H', data, optional_header, 0x10B)
    struct.pack_into('<I', data, optional_header + 92, 16)
    if managed:
        struct.pack_into('<II', data, optional_header + 96 + 14 * 8, 0x2000, 72)
    return bytes(data)


def write_runtime_file(runtime: Path, name: str, content: bytes) -> Path:
    path = runtime / 'lib' / 'netcoreapp' / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def test_payload_contains_only_managed_class_library_assemblies(tmp_path: Path) -> None:
    runtime = tmp_path / 'published-runtime'
    write_runtime_file(runtime, 'System.Private.CoreLib.dll', make_pe(managed=True))
    write_runtime_file(runtime, 'System.Collections.dll', make_pe(managed=True) + b'collections')
    write_runtime_file(runtime, 'coreclr.dll', make_pe(managed=False))
    write_runtime_file(runtime, 'clrjit.dll', b'native jit')
    (runtime / 'include').mkdir()
    (runtime / 'include' / 'mono.h').write_text('build header')
    output = tmp_path / 'payload'
    stamp = tmp_path / 'payload.ready'

    staged = payload.stage_payload(runtime, output, stamp)

    assert {path.relative_to(output).as_posix() for path in staged} == {
        'lib/netcoreapp/System.Collections.dll',
        'lib/netcoreapp/System.Private.CoreLib.dll',
        payload.MANIFEST_NAME,
    }
    assert not (output / 'lib/netcoreapp/coreclr.dll').exists()
    assert not (output / 'lib/netcoreapp/clrjit.dll').exists()
    assert not (output / 'include').exists()
    manifest = (output / payload.MANIFEST_NAME).read_text(encoding='utf-8').splitlines()
    assert [line.split('  ', 1)[1] for line in manifest] == [
        'lib/netcoreapp/System.Collections.dll',
        'lib/netcoreapp/System.Private.CoreLib.dll',
    ]
    assert len(stamp.read_text(encoding='utf-8').strip()) == 64


def test_payload_requires_mono_corelib(tmp_path: Path) -> None:
    runtime = tmp_path / 'published-runtime'
    write_runtime_file(runtime, 'System.Collections.dll', make_pe(managed=True))

    with pytest.raises(ValueError, match='System.Private.CoreLib.dll'):
        payload.stage_payload(runtime, tmp_path / 'payload', tmp_path / 'payload.ready')
