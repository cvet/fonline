from __future__ import annotations

from pathlib import Path
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import managed_runtime_payload as payload
from managed_assembly_images import make_assembly, write_runtime_payload


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


@pytest.mark.parametrize('pe32_plus', [False, True])
@pytest.mark.parametrize('large_heaps', [False, True])
# 0x4000 type references widen the ResolutionScope coded index, which shifts every table after TypeRef
@pytest.mark.parametrize('type_ref_rows', [0, 3, 0x4000])
def test_assembly_references_are_read_from_metadata_tables(pe32_plus: bool, large_heaps: bool, type_ref_rows: int) -> None:
    references = ('System.Runtime', 'System.Collections', 'Unit.Helper')
    image = make_assembly('Unit.Scripts', references, pe32_plus=pe32_plus, large_heaps=large_heaps, type_ref_rows=type_ref_rows)

    assert payload.read_assembly_identity(image) == payload.AssemblyIdentity('Unit.Scripts', references)
    assert payload.read_assembly_identity(make_assembly('Leaf')).references == ()


@pytest.mark.parametrize('length', [0, 0x40, 0x100, 0x248, -8])
def test_truncated_assembly_is_rejected(length: int) -> None:
    image = make_assembly('Unit.Scripts', ['System.Runtime'])

    with pytest.raises(payload.ManagedAssemblyError):
        payload.read_assembly_identity(image[:length])


def test_native_pe_and_text_are_not_assemblies() -> None:
    with pytest.raises(payload.ManagedAssemblyError, match='native PE'):
        payload.read_assembly_identity(make_pe(managed=False))
    with pytest.raises(payload.ManagedAssemblyError):
        payload.read_assembly_identity(b'entry-Server\n')


RUNTIME = {
    'System.Runtime': ['System.Private.CoreLib', 'System.Private.Uri'],
    'System.Private.Uri': ['System.Private.CoreLib'],
    'System.Private.CoreLib': [],
    'System.Linq': ['System.Collections'],
    'System.Collections': ['System.Linq'],
    'System.Xml': ['System.Private.CoreLib'],
    'mscorlib': ['System.Runtime', 'System.Security.Permissions'],
}


def find_in(runtime: dict[str, list[str]], lookups: list[str] | None = None):
    def find(name: str) -> payload.AssemblyIdentity | None:
        if lookups is not None:
            lookups.append(name)
        return payload.AssemblyIdentity(name, tuple(runtime[name])) if name in runtime else None
    return find


def test_selection_is_the_reference_closure_of_the_pack() -> None:
    lookups: list[str] = []
    pack = [
        payload.AssemblyIdentity('Scripts.Server', ('System.Runtime', 'Unit.Helper', 'System.Linq')),
        payload.AssemblyIdentity('Unit.Helper', ('System.Runtime',)),
    ]

    selected = payload.select_runtime_assemblies(pack, find_in(RUNTIME, lookups))

    # A pack assembly is never looked up in the runtime, a cycle ends, and an unreferenced library stays out
    assert selected == {'System.Collections', 'System.Linq', 'System.Private.CoreLib', 'System.Private.Uri', 'System.Runtime'}
    assert 'Unit.Helper' not in lookups
    assert lookups.count('System.Runtime') == 1
    assert payload.select_runtime_assemblies([], find_in(RUNTIME)) == {'System.Private.CoreLib'}


def test_facade_references_outside_the_runtime_are_not_required() -> None:
    pack = [payload.AssemblyIdentity('NetStandard.Helper', ('mscorlib',))]

    assert payload.select_runtime_assemblies(pack, find_in(RUNTIME)) == {
        'System.Private.CoreLib', 'System.Private.Uri', 'System.Runtime', 'mscorlib'}


def test_unresolved_pack_reference_is_rejected() -> None:
    pack = [payload.AssemblyIdentity('Scripts.Client', ('System.Runtime', 'Missing.Library'))]

    with pytest.raises(payload.ManagedAssemblyError, match='Missing.Library of Scripts.Client'):
        payload.select_runtime_assemblies(pack, find_in(RUNTIME))


def test_selection_requires_corelib() -> None:
    with pytest.raises(payload.ManagedAssemblyError, match='does not publish CoreLib'):
        payload.select_runtime_assemblies([], find_in({}))


def test_selected_payload_lists_only_shipped_files(tmp_path: Path) -> None:
    runtime = tmp_path / 'runtime'
    write_runtime_payload(runtime, RUNTIME)
    all_lines = (runtime / payload.MANIFEST_NAME).read_text(encoding='utf-8').splitlines()
    pack = [payload.read_assembly_identity(make_assembly('Scripts.Client', ['System.Runtime']))]

    files, manifest = payload.select_payload(runtime, pack)

    assert [path.as_posix() for path in files] == [
        'lib/netcoreapp/System.Private.CoreLib.dll',
        'lib/netcoreapp/System.Private.Uri.dll',
        'lib/netcoreapp/System.Runtime.dll',
    ]
    assert manifest.splitlines() == [line for line in all_lines if line.split('  ', 1)[1] in {path.as_posix() for path in files}]


def test_selected_payload_rejects_a_misnamed_corelib(tmp_path: Path) -> None:
    runtime = tmp_path / 'runtime'
    write_runtime_payload(runtime, RUNTIME)
    (runtime / 'lib/netcoreapp/System.Private.CoreLib.dll').write_bytes(make_assembly('Wrong.CoreLib'))

    with pytest.raises(payload.ManagedAssemblyError, match='defines another assembly'):
        payload.select_payload(runtime, [])


def test_file_identity_must_match_its_assembly_file_name(tmp_path: Path) -> None:
    assembly_path = tmp_path / 'Alias.dll'
    assembly_path.write_bytes(make_assembly('Real.Name'))

    with pytest.raises(payload.ManagedAssemblyError, match='Alias.dll defines another assembly: Real.Name'):
        payload.read_assembly_identity_file(assembly_path)


@pytest.mark.parametrize('manifest_problem', ['digest', 'duplicate'])
def test_payload_manifest_rejects_ambiguous_entries(tmp_path: Path, manifest_problem: str) -> None:
    runtime = tmp_path / 'runtime'
    write_runtime_payload(runtime, RUNTIME)
    manifest_path = runtime / payload.MANIFEST_NAME
    lines = manifest_path.read_text(encoding='utf-8').splitlines()

    if manifest_problem == 'digest':
        lines[0] = 'g' + lines[0][1:]
    else:
        lines.append(lines[0])

    manifest_path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')

    with pytest.raises(payload.ManagedAssemblyError, match='digest|Duplicate'):
        payload.select_payload(runtime, [])
