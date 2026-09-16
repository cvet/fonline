from __future__ import annotations

from pathlib import Path
import struct
import sys
from types import SimpleNamespace
import zlib

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import package
from managed_assembly_images import make_assembly, write_runtime_payload

# Scripts reach System.Runtime and through it CoreLib; nothing reaches System.Xml, so no package may carry it
TARGET_RUNTIME = {
    'System.Private.CoreLib': [],
    'System.Runtime': ['System.Private.CoreLib'],
    'System.Xml': ['System.Private.CoreLib'],
}
SHIPPED_RUNTIME_FILES = {
    'ManagedRuntime/lib/netcoreapp/System.Private.CoreLib.dll',
    'ManagedRuntime/lib/netcoreapp/System.Runtime.dll',
}


def script_assembly(target: str) -> bytes:
    return make_assembly(f'Scripts.{target}', ['System.Runtime', 'FOnline.ManagedHost'])


def host_assembly() -> bytes:
    return make_assembly('FOnline.ManagedHost', ['System.Private.CoreLib'])


def runtime_entries(entries: dict[str, bytes]) -> set[str]:
    return {entry for entry in entries if entry.startswith('ManagedRuntime/')}


def shipped_manifest(runtime_dir: Path) -> bytes:
    lines = (runtime_dir / 'runtime.manifest').read_text(encoding='utf-8').splitlines()
    return ''.join(line + '\n' for line in lines if 'System.Xml' not in line).encode()


def make_packager(tmp_path: Path):
    packager = package.Packager.__new__(package.Packager)
    packager.args = SimpleNamespace(
        input=[str(tmp_path)],
        devname='LF',
        nicename='Game',
        buildhash='build',
        expect_client_runtime=[],
    )
    packager.pack_args = set()
    packager.target_output_path = str(tmp_path / 'output')
    packager.platform_binaries_dir = 'PlatformBinaries'
    packager.client_res_dir = 'Resources'
    packager.server_res_dir = 'ServerResources'
    packager.compress_level = 6
    packager.resource_pack_min_compress_gain = 5
    packager.zip_compress_level = 6
    packager.resource_archive_paths = {}
    packager.baking_path = str(tmp_path / 'Baking')
    packager.get_target_resource_packs = lambda target: []
    packager.embedded_data = b''
    packager.config_data = ''
    packager.make_embedded_data_for_target = lambda target: b''
    packager.read_config_data = lambda target: (None, '')
    packager.patch_packaged_binary = lambda *args: None
    return packager


def read_resource_pack(path: Path) -> dict[str, bytes]:
    data = path.read_bytes()
    index_offset, index_stored_size = struct.unpack_from('<QQ', data, 16)
    index_codec, entry_count = struct.unpack_from('<II', data, 40)
    stored_index = data[index_offset:index_offset + index_stored_size]
    index = zlib.decompress(stored_index) if index_codec == package.RESOURCE_PACK_CODEC_DEFLATE else stored_index
    entries: dict[str, bytes] = {}

    for ordinal in range(entry_count):
        path_offset, path_length, blob_offset, stored_size, _, codec, _, _ = struct.unpack_from(
            '<IIQQQIIQ', index, ordinal * package.RESOURCE_PACK_ENTRY_SIZE)
        name = index[path_offset:path_offset + path_length].decode('utf-8')
        stored = data[blob_offset:blob_offset + stored_size]
        entries[name] = zlib.decompress(stored) if codec == package.RESOURCE_PACK_CODEC_DEFLATE else stored

    return entries


def add_managed_runtime_pack(
    packager, tmp_path: Path, baked_corelib: bytes = b'baker platform', target: str = 'Client',
) -> Path:
    scripts_dir = tmp_path / 'Baking' / 'Scripts'
    baked_runtime = scripts_dir / 'ManagedRuntime'
    (baked_runtime / 'lib' / 'netcoreapp').mkdir(parents=True)
    (scripts_dir / 'Game.dll').write_bytes(b'game scripts')
    for managed_target in ('Server', 'Client', 'Mapper'):
        target_dir = scripts_dir / 'Assemblies' / f'Assemblies-{managed_target.lower()}'
        target_dir.mkdir(parents=True)
        (target_dir / f'Scripts.{managed_target}.dll').write_bytes(script_assembly(managed_target))
        (target_dir / 'FOnline.ManagedHost.dll').write_bytes(host_assembly())
    (baked_runtime / 'runtime.manifest').write_bytes(b'baker manifest')
    (baked_runtime / 'lib' / 'netcoreapp' / 'System.Private.CoreLib.dll').write_bytes(baked_corelib)
    packager.get_target_resource_packs = lambda requested_target: ['Scripts'] if requested_target == target else []
    return scripts_dir


def add_binary_managed_runtime(binary_dir: Path, marker: bytes) -> Path:
    # The marker trails each image, so an archive shows which platform's class libraries it took
    runtime_dir = binary_dir / 'ManagedRuntime'
    write_runtime_payload(runtime_dir, TARGET_RUNTIME, marker)
    return runtime_dir


def read_corelib(entries: dict[str, bytes]) -> bytes:
    return entries['ManagedRuntime/lib/netcoreapp/System.Private.CoreLib.dll']


def test_native_package_does_not_copy_managed_runtime_companions(tmp_path: Path) -> None:
    binary_dir = tmp_path / 'Binaries' / 'Client-Windows-win64'
    binary_dir.mkdir(parents=True)
    (binary_dir / 'LF_Client.exe').write_bytes(b'client')
    (binary_dir / 'SDL3.dll').write_bytes(b'sdl')
    (binary_dir / 'ManagedRuntime' / 'lib').mkdir(parents=True)
    (binary_dir / 'ManagedRuntime' / 'lib' / 'coreclr.dll').write_bytes(b'coreclr')
    output = tmp_path / 'output'
    output.mkdir()
    packager = make_packager(tmp_path)

    packager.copy_runtime_companions(str(binary_dir), 'LF_Client', '.exe')

    assert (output / 'SDL3.dll').read_bytes() == b'sdl'
    assert not (output / 'ManagedRuntime').exists()


@pytest.mark.parametrize(('target', 'included_targets'), [
    ('Server', {'Server'}),
    ('Client', {'Client'}),
    ('Mapper', {'Client', 'Mapper'}),
])
def test_resource_collection_applies_target_suffixes_to_directories(
    tmp_path: Path, target: str, included_targets: set[str],
) -> None:
    packager = make_packager(tmp_path)
    scripts_dir = add_managed_runtime_pack(packager, tmp_path)

    files = packager.collect_resource_files('Scripts', target)
    entries = {Path(file_path).relative_to(scripts_dir).as_posix() for file_path in files}

    assert 'Game.dll' in entries
    for managed_target in ('Server', 'Client', 'Mapper'):
        assembly_prefix = f'Assemblies/Assemblies-{managed_target.lower()}/'
        if managed_target in included_targets:
            assert assembly_prefix + f'Scripts.{managed_target}.dll' in entries
        else:
            assert not any(entry.startswith(assembly_prefix) for entry in entries)


@pytest.mark.parametrize('missing_binary', [False, True])
def test_native_payload_uses_platform_target_without_runtime_suffix(tmp_path: Path, missing_binary: bool) -> None:
    binary_dir = tmp_path / 'Binaries' / 'Client-Linux-x64'
    binary_dir.mkdir(parents=True)
    (binary_dir / 'LF_ClientLib.build-hash').write_text('build')
    (binary_dir / 'LF_ClientLib.so').write_bytes(b'native runtime')
    if missing_binary:
        (binary_dir / 'LF_ClientLib.so').unlink()
    packager = make_packager(tmp_path)

    packager.package_all_client_runtime_update_payloads()

    output = Path(packager.target_output_path)
    if missing_binary:
        assert not output.exists()
    else:
        root = output / 'PlatformBinaries'
        assert (root / 'Linux-x64' / 'Game.so').read_bytes() == b'native runtime'


@pytest.mark.parametrize('default_present', [False, True])
@pytest.mark.parametrize('headless_revision', [None, 'old-build', 'build'])
def test_each_native_variant_requires_its_own_build_revision(
    tmp_path: Path, default_present: bool, headless_revision: str | None,
) -> None:
    binary_dir = tmp_path / 'Binaries' / 'Client-Linux-x64'
    binary_dir.mkdir(parents=True)
    expected = set()
    if default_present:
        (binary_dir / 'LF_ClientLib.so').write_bytes(b'gui')
        (binary_dir / 'LF_ClientLib.build-hash').write_text('build')
        expected.add('Game.so')
    (binary_dir / 'LF_ClientLibHeadless.so').write_bytes(b'headless')
    if headless_revision is not None:
        (binary_dir / 'LF_ClientLibHeadless.build-hash').write_text(headless_revision)
    if default_present and headless_revision == 'build':
        expected.add('Game_Headless.so')
    packager = make_packager(tmp_path)

    packager.package_all_client_runtime_update_payloads()

    payloads = Path(packager.target_output_path) / 'PlatformBinaries' / 'Linux-x64'
    assert {path.name for path in payloads.glob('*.so')} == expected


@pytest.mark.parametrize(('entry_suffix', 'output_suffix'), [
    ('-Debug_Profiling_Total', '_Debug_Profiling_Total'),
    ('-Profiling_OnDemand_Custom', '_Profiling_OnDemand_Custom'),
    ('-DebugProfile', '_DebugProfile'),
    ('-Profiling_Total', '_Profiling'),
    ('-Profiling_Total-Debug-Steam', '_Profiling_Steam'),
    ('-Debug-Steam', '_Steam'),
])
def test_native_payload_postfix_preserves_reserved_word_prefixes(
    tmp_path: Path, entry_suffix: str, output_suffix: str,
) -> None:
    binary_dir = tmp_path / 'Binaries' / ('Client-Linux-x64' + entry_suffix)
    binary_dir.mkdir(parents=True)
    (binary_dir / 'LF_ClientLib.so').write_bytes(b'native runtime')
    (binary_dir / 'LF_ClientLib.build-hash').write_text('build')
    packager = make_packager(tmp_path)

    packager.package_all_client_runtime_update_payloads()

    payloads = Path(packager.target_output_path) / 'PlatformBinaries' / 'Linux-x64'
    assert {path.name for path in payloads.glob('*.so')} == {'Game' + output_suffix + '.so'}


def test_server_payload_accepts_windows_build(tmp_path: Path, monkeypatch) -> None:
    binary_dir = tmp_path / 'Binaries' / 'Client-Windows-win64'
    binary_dir.mkdir(parents=True)
    (binary_dir / 'LF_ClientLib.build-hash').write_text('build')
    (binary_dir / 'LF_ClientLib.dll').write_bytes(b'native runtime')
    (binary_dir / 'LF_ClientLib.pdb').write_bytes(b'symbols')
    monkeypatch.setattr(package, 'patch_pe_pdb_path', lambda *args: True)
    packager = make_packager(tmp_path)

    packager.package_all_client_runtime_update_payloads()

    payloads = Path(packager.target_output_path) / 'PlatformBinaries' / 'Windows-win64'
    assert {path.name for path in payloads.glob('*.dll')} == {'Game.dll', 'Game_OpenGL.dll'}
    assert all(path.read_bytes() == b'native runtime' for path in payloads.glob('*.dll'))


def test_client_package_replaces_baker_runtime_with_target_runtime(tmp_path: Path) -> None:
    packager = make_packager(tmp_path)
    add_managed_runtime_pack(packager, tmp_path)
    binary_dir = tmp_path / 'Binaries' / 'Client-Windows-win64'
    binary_dir.mkdir(parents=True)
    (binary_dir / 'LF_Client.build-hash').write_text('build')
    runtime_dir = add_binary_managed_runtime(binary_dir, b'windows corelib')
    packager.args.arch = 'win64'
    packager.args.platform = 'Windows'
    packager.args.target = 'Client'
    packager.args.binary_output_postfix = ''
    output_resources = Path(packager.target_output_path) / packager.client_res_dir
    output_resources.mkdir(parents=True)

    packager.package_target_managed_runtime_resources('Client')

    entries = read_resource_pack(output_resources / 'Scripts.fores')
    assert entries['Game.dll'] == b'game scripts'
    assert entries['Assemblies/Assemblies-client/Scripts.Client.dll'] == script_assembly('Client')
    assert entries['Assemblies/Assemblies-client/FOnline.ManagedHost.dll'] == host_assembly()
    assert not any(entry.startswith('Assemblies/Assemblies-server/') for entry in entries)
    assert not any(entry.startswith('Assemblies/Assemblies-mapper/') for entry in entries)
    assert runtime_entries(entries) == SHIPPED_RUNTIME_FILES | {'ManagedRuntime/runtime.manifest'}
    assert entries['ManagedRuntime/runtime.manifest'] == shipped_manifest(runtime_dir)
    assert read_corelib(entries).endswith(b'windows corelib')


def test_package_rejects_a_script_reference_the_target_runtime_lacks(tmp_path: Path) -> None:
    packager = make_packager(tmp_path)
    scripts_dir = add_managed_runtime_pack(packager, tmp_path, target='Server')
    (scripts_dir / 'Assemblies' / 'Assemblies-server' / 'Scripts.Server.dll').write_bytes(
        make_assembly('Scripts.Server', ['System.Runtime', 'System.Windows.Forms']))
    binary_dir = tmp_path / 'Binaries' / 'Server-Linux-x64'
    binary_dir.mkdir(parents=True)
    (binary_dir / 'LF_Server.build-hash').write_text('build')
    add_binary_managed_runtime(binary_dir, b'linux server corelib')
    packager.args.arch = 'x64'
    packager.args.platform = 'Linux'
    packager.args.target = 'Server'
    packager.args.binary_output_postfix = ''
    (Path(packager.target_output_path) / packager.server_res_dir).mkdir(parents=True)

    with pytest.raises(ValueError, match='System.Windows.Forms of Scripts.Server'):
        packager.package_target_managed_runtime_resources('Server')


def test_server_package_replaces_baker_runtime_with_target_runtime(tmp_path: Path) -> None:
    packager = make_packager(tmp_path)
    add_managed_runtime_pack(packager, tmp_path, target='Server')
    binary_dir = tmp_path / 'Binaries' / 'Server-Linux-x64'
    binary_dir.mkdir(parents=True)
    (binary_dir / 'LF_Server.build-hash').write_text('build')
    runtime_dir = add_binary_managed_runtime(binary_dir, b'linux server corelib')
    packager.args.arch = 'x64'
    packager.args.platform = 'Linux'
    packager.args.target = 'Server'
    packager.args.binary_output_postfix = ''
    output_resources = Path(packager.target_output_path) / packager.server_res_dir
    output_resources.mkdir(parents=True)

    packager.package_target_managed_runtime_resources('Server')

    entries = read_resource_pack(output_resources / 'Scripts.fores')
    assert entries['Game.dll'] == b'game scripts'
    assert runtime_entries(entries) == SHIPPED_RUNTIME_FILES | {'ManagedRuntime/runtime.manifest'}
    assert entries['ManagedRuntime/runtime.manifest'] == shipped_manifest(runtime_dir)
    assert read_corelib(entries).endswith(b'linux server corelib')


def test_server_stages_target_specific_scripts_for_web_and_windows(tmp_path: Path, monkeypatch) -> None:
    packager = make_packager(tmp_path)
    add_managed_runtime_pack(packager, tmp_path)
    packager.args.expect_client_runtime = ['Windows:win64:', 'Web:wasm:']
    monkeypatch.setattr(package, 'patch_pe_pdb_path', lambda *args: True)

    windows_dir = tmp_path / 'Binaries' / 'Client-Windows-win64'
    windows_dir.mkdir(parents=True)
    (windows_dir / 'LF_Client.build-hash').write_text('build')
    (windows_dir / 'LF_ClientLib.build-hash').write_text('build')
    (windows_dir / 'LF_ClientLib.dll').write_bytes(b'native runtime')
    (windows_dir / 'LF_ClientLib.pdb').write_bytes(b'symbols')
    add_binary_managed_runtime(windows_dir, b'windows corelib')

    web_dir = tmp_path / 'Binaries' / 'Client-Web-wasm'
    web_dir.mkdir(parents=True)
    (web_dir / 'LF_Client.build-hash').write_text('build')
    add_binary_managed_runtime(web_dir, b'web corelib')

    packager.package_all_client_runtime_update_payloads()

    platform_root = Path(packager.target_output_path) / 'PlatformBinaries'
    entries = read_resource_pack(platform_root / 'Windows-win64' / 'Scripts.fores')
    assert entries['Assemblies/Assemblies-client/Scripts.Client.dll'] == script_assembly('Client')
    assert not any(entry.startswith('Assemblies/Assemblies-server/') for entry in entries)
    assert not any(entry.startswith('Assemblies/Assemblies-mapper/') for entry in entries)
    assert runtime_entries(entries) == SHIPPED_RUNTIME_FILES | {'ManagedRuntime/runtime.manifest'}
    assert read_corelib(entries).endswith(b'windows corelib')
    entries = read_resource_pack(platform_root / 'Web-wasm' / 'Scripts.fores')
    assert entries['Assemblies/Assemblies-client/Scripts.Client.dll'] == script_assembly('Client')
    assert not any(entry.startswith('Assemblies/Assemblies-server/') for entry in entries)
    assert not any(entry.startswith('Assemblies/Assemblies-mapper/') for entry in entries)
    assert runtime_entries(entries) == SHIPPED_RUNTIME_FILES | {'ManagedRuntime/runtime.manifest'}
    assert read_corelib(entries).endswith(b'web corelib')
    assert (platform_root / 'Windows-win64' / 'Game.dll').is_file()
    assert list((platform_root / 'Web-wasm').glob('*')) == [platform_root / 'Web-wasm' / 'Scripts.fores']


def test_shared_update_target_prefers_unqualified_managed_runtime(tmp_path: Path) -> None:
    packager = make_packager(tmp_path)
    add_managed_runtime_pack(packager, tmp_path)

    # Create the qualified variant first: filesystem enumeration order must not decide
    # the single managed-resource payload shared by this platform/architecture target
    runtime_dirs = {}
    for suffix, marker in (('-Steam', b'steam corelib'), ('', b'default corelib')):
        binary_dir = tmp_path / 'Binaries' / ('Client-Linux-x64' + suffix)
        binary_dir.mkdir(parents=True)
        (binary_dir / 'LF_Client.build-hash').write_text('build')
        runtime_dirs[suffix] = add_binary_managed_runtime(binary_dir, marker)

    packager.package_all_client_runtime_update_payloads()

    entries = read_resource_pack(Path(packager.target_output_path) / 'PlatformBinaries' / 'Linux-x64' / 'Scripts.fores')
    assert entries['ManagedRuntime/runtime.manifest'] == shipped_manifest(runtime_dirs[''])
    assert read_corelib(entries).endswith(b'default corelib')
