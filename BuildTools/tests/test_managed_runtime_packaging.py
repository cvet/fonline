from __future__ import annotations

from pathlib import Path
import sys
from types import SimpleNamespace
import zipfile

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package


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
    packager.zip_compress_level = 6
    packager.baking_path = str(tmp_path / 'Baking')
    packager.get_target_resource_packs = lambda target: []
    packager.embedded_data = b''
    packager.config_data = ''
    packager.make_embedded_data_for_target = lambda target: b''
    packager.read_config_data = lambda target: (None, '')
    packager.patch_packaged_binary = lambda *args: None
    return packager


def add_managed_runtime_pack(packager, tmp_path: Path, baked_corelib: bytes = b'baker platform') -> Path:
    scripts_dir = tmp_path / 'Baking' / 'Scripts'
    baked_runtime = scripts_dir / 'ManagedRuntime'
    (baked_runtime / 'lib' / 'netcoreapp').mkdir(parents=True)
    (scripts_dir / 'Game.dll').write_bytes(b'game scripts')
    (baked_runtime / 'runtime.manifest').write_bytes(b'baker manifest')
    (baked_runtime / 'lib' / 'netcoreapp' / 'System.Private.CoreLib.dll').write_bytes(baked_corelib)
    packager.get_target_resource_packs = lambda target: ['Scripts']
    return scripts_dir


def add_binary_managed_runtime(binary_dir: Path, identity: bytes, corelib: bytes) -> None:
    runtime_dir = binary_dir / 'ManagedRuntime'
    (runtime_dir / 'lib' / 'netcoreapp').mkdir(parents=True)
    (runtime_dir / 'runtime.manifest').write_bytes(identity)
    (runtime_dir / 'lib' / 'netcoreapp' / 'System.Private.CoreLib.dll').write_bytes(corelib)


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
    add_binary_managed_runtime(binary_dir, b'windows manifest', b'windows corelib')
    packager.args.arch = 'win64'
    packager.args.platform = 'Windows'
    packager.args.target = 'Client'
    packager.args.binary_output_postfix = ''
    output_resources = Path(packager.target_output_path) / packager.client_res_dir
    output_resources.mkdir(parents=True)

    packager.package_client_managed_runtime_resources()

    with zipfile.ZipFile(output_resources / 'Scripts.zip') as archive:
        assert archive.read('Game.dll') == b'game scripts'
        assert archive.read('ManagedRuntime/runtime.manifest') == b'windows manifest'
        assert archive.read('ManagedRuntime/lib/netcoreapp/System.Private.CoreLib.dll') == b'windows corelib'


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
    add_binary_managed_runtime(windows_dir, b'windows manifest', b'windows corelib')

    web_dir = tmp_path / 'Binaries' / 'Client-Web-wasm'
    web_dir.mkdir(parents=True)
    (web_dir / 'LF_Client.build-hash').write_text('build')
    add_binary_managed_runtime(web_dir, b'web manifest', b'web corelib')

    packager.package_all_client_runtime_update_payloads()

    platform_root = Path(packager.target_output_path) / 'PlatformBinaries'
    with zipfile.ZipFile(platform_root / 'Windows-win64' / 'Scripts.zip') as archive:
        assert archive.read('ManagedRuntime/lib/netcoreapp/System.Private.CoreLib.dll') == b'windows corelib'
    with zipfile.ZipFile(platform_root / 'Web-wasm' / 'Scripts.zip') as archive:
        assert archive.read('ManagedRuntime/lib/netcoreapp/System.Private.CoreLib.dll') == b'web corelib'
    assert (platform_root / 'Windows-win64' / 'Game.dll').is_file()
    assert list((platform_root / 'Web-wasm').glob('*')) == [platform_root / 'Web-wasm' / 'Scripts.zip']


def test_shared_update_target_prefers_unqualified_managed_runtime(tmp_path: Path) -> None:
    packager = make_packager(tmp_path)
    add_managed_runtime_pack(packager, tmp_path)

    # Create the qualified variant first: filesystem enumeration order must not decide
    # the single managed-resource payload shared by this platform/architecture target
    for suffix, identity in (('-Steam', b'steam manifest'), ('', b'default manifest')):
        binary_dir = tmp_path / 'Binaries' / ('Client-Linux-x64' + suffix)
        binary_dir.mkdir(parents=True)
        (binary_dir / 'LF_Client.build-hash').write_text('build')
        add_binary_managed_runtime(binary_dir, identity, identity)

    packager.package_all_client_runtime_update_payloads()

    with zipfile.ZipFile(Path(packager.target_output_path) / 'PlatformBinaries' / 'Linux-x64' / 'Scripts.zip') as archive:
        assert archive.read('ManagedRuntime/runtime.manifest') == b'default manifest'
        assert archive.read('ManagedRuntime/lib/netcoreapp/System.Private.CoreLib.dll') == b'default manifest'
