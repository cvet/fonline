from __future__ import annotations

from pathlib import Path
import sys
from types import SimpleNamespace

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
    packager.target_output_path = str(tmp_path / 'output')
    packager.platform_binaries_dir = 'PlatformBinaries'
    packager.embedded_data = b''
    packager.config_data = ''
    packager.make_embedded_data_for_target = lambda target: b''
    packager.read_config_data = lambda target: (None, '')
    packager.patch_packaged_binary = lambda *args: None
    return packager


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
