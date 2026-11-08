from __future__ import annotations

from pathlib import Path
import os
import shutil
import sys
import zipfile
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package


def packager_fixture(tmp_path: Path):
    packager = package.Packager.__new__(package.Packager)
    packager.args = SimpleNamespace(
        target='Server', platform='Linux', arch='x64+arm64', devname='LF', nicename='LastFrontier',
        config='PublicGame', binary_output_postfix='qa', linux_extra_library=[],
        linux_portable_tools=str(tmp_path / 'tools'), linux_ca_setting='',
    )
    packager.pack_args = {'Portable', 'Raw', 'NoRes', 'Headless', 'Daemon', 'TotalProfiling', 'OnDemandProfiling'}
    packager.output_path = str(tmp_path / 'package')
    packager.target_output_path = packager.build_target_output_path()
    Path(packager.target_output_path).mkdir(parents=True)
    packager.logical_file_modes = {}
    packager.resolve_binary_input_dir = lambda arch, variant, name: str(tmp_path / 'input' / arch / name)
    packager.resolve_game_version = lambda: '0.3.1588'
    packager.build_tools_path = str(Path(package.__file__).parent)

    def binary(input_path, input_name, output_name, extension, *args, **kwargs):
        destination = Path(packager.target_output_path) / (output_name + extension)
        destination.write_text(input_path + '|' + output_name)
        return str(destination)

    packager.package_platform_binary = binary
    return packager


def test_portable_variants_are_isolated_after_native_binary_patching(tmp_path: Path, monkeypatch):
    packager = packager_fixture(tmp_path)
    observed = []
    import linux_portable
    monkeypatch.setattr(linux_portable, 'resolve_tools', lambda *args, **kwargs: {'deploy': tmp_path / 'tool'})

    def prepare(source, destination, entrypoint, **kwargs):
        observed.append((destination.name, entrypoint, (source / entrypoint).read_text(), kwargs['identity']))
        (destination / 'usr/bin').mkdir(parents=True)
        shutil.copy2(source / entrypoint, destination / 'usr/bin' / entrypoint)
        (destination / 'AppRun').write_text('launcher')
        (destination / 'AppRun').chmod(0o755)
        return destination

    monkeypatch.setattr(linux_portable, 'prepare_portable', prepare)
    packager.package_linux()
    outputs = list(Path(packager.target_output_path).iterdir())
    assert len(outputs) == 18
    assert len({entry.name for entry in outputs}) == 18
    assert all(entry.name.endswith('.AppDir') for entry in outputs)
    assert len(observed) == 18
    assert {item[3]['architecture'] for item in observed} == {'x64', 'arm64'}
    assert {item[3]['role'] for item in observed} == {'', 'Headless', 'Daemon'}
    assert {item[3]['profiling'] for item in observed} == {'', 'TotalProfiling', 'OnDemandProfiling'}
    assert all(item[1] in item[2] for item in observed)
    assert all(item[3]['postfix'] == 'qa' for item in observed)
    assert Path(packager.target_output_path).name.endswith('-qa')
    assert len([mode for path, mode in packager.logical_file_modes.items() if path.endswith('/AppRun') and mode == 0o755]) == 18


def test_versioned_runtime_companions_are_copied(tmp_path: Path):
    packager = packager_fixture(tmp_path)
    inputs = tmp_path / 'companions'
    inputs.mkdir()
    for name in ['LF_Server', 'libnative.so.3', 'libnative.so.3.1', 'other.so', 'notes.txt']:
        (inputs / name).write_text(name)
    packager.copy_runtime_companions(str(inputs), 'LF_Server', '')
    assert {p.name for p in Path(packager.target_output_path).iterdir()} == {
        'libnative.so.3', 'libnative.so.3.1', 'other.so'}


@pytest.mark.parametrize('platform,packs', [
    ('Windows', 'Portable+Raw'), ('Linux', 'Portable'),
    ('Linux', 'Portable+Raw+Typo'), ('Linux', 'AppImage+Lib'),
])
def test_invalid_portable_request_is_rejected_before_output_changes(tmp_path: Path, platform, packs):
    output = tmp_path / 'existing'
    output.mkdir()
    marker = output / 'old-good-package'
    marker.write_text('preserve')
    args = SimpleNamespace(platform=platform, pack=packs, target='Server', arch='x64', output=str(output))
    with pytest.raises(ValueError):
        package.Packager(args, None).run()
    assert marker.read_text() == 'preserve'


@pytest.mark.parametrize('packs', ['Portable+Raw', 'Portable+TarGz', 'AppImage', 'AppImage+Raw'])
def test_portable_request_accepts_real_artifact_combinations(packs):
    package.validate_package_request(SimpleNamespace(platform='Linux', pack=packs, target='Server', arch='x64'))


@pytest.mark.parametrize('artifact', ['Zip', 'SingleZip'])
def test_failed_archive_does_not_leave_partial_output(tmp_path: Path, monkeypatch, artifact):
    import linux_portable
    packager = packager_fixture(tmp_path)
    packager.args.arch = 'x64'
    packager.pack_args = {'Portable', 'NoRes', 'Raw', artifact}
    packager.bundle_compress_level = 6
    monkeypatch.setattr(package.sys, 'platform', 'linux')
    monkeypatch.setattr(linux_portable, 'resolve_tools', lambda *args, **kwargs: {'deploy': tmp_path / 'tool'})

    def prepare(source, destination, entrypoint, **kwargs):
        destination.mkdir()
        (destination / 'AppRun').write_text('complete bundle')
        return destination

    monkeypatch.setattr(linux_portable, 'prepare_portable', prepare)
    unrelated = Path(packager.output_path) / 'unrelated.AppImage'
    unrelated.write_bytes(b'foreign')
    shared = Path(packager.output_path) / (Path(packager.output_path).name + '.zip')
    with zipfile.ZipFile(shared, 'w') as archive:
        archive.writestr('existing.txt', 'previous complete package')
    before = shared.read_bytes()

    def fail_archive(destination, *args, **kwargs):
        Path(destination).write_bytes(b'partial archive')
        raise RuntimeError('simulated archive failure')

    monkeypatch.setattr(package, 'make_zip', fail_archive)
    with pytest.raises(RuntimeError, match='simulated archive failure'):
        packager.run()
    assert unrelated.read_bytes() == b'foreign'
    assert shared.read_bytes() == before
    assert not Path(packager.target_output_path + '.zip').exists()
    assert not list(Path(packager.output_path).glob('*.portable-*'))
    assert not list(Path(packager.output_path).glob('portable-singlezip-*'))


def test_ca_bundle_rejects_private_key_material(tmp_path: Path):
    import linux_portable
    bundle = tmp_path / 'mixed.pem'
    bundle.write_text('-----BEGIN CERTIFICATE-----\ninvalid\n-----END CERTIFICATE-----\n'
                      '-----BEGIN PRIVATE KEY-----\nnot-a-real-key\n-----END PRIVATE KEY-----\n')
    with pytest.raises(ValueError, match='private key'):
        linux_portable.validate_ca_bundle(bundle)


@pytest.mark.parametrize('packs,mid_merge', [({'Portable', 'Raw', 'SingleZip', 'TarGz'}, False),
    ({'Portable', 'Raw', 'Root'}, False), ({'Portable', 'Raw', 'Root'}, True)])
def test_late_failure_rolls_back_shared_outputs(tmp_path, monkeypatch, packs, mid_merge):
    packager = packager_fixture(tmp_path)
    packager.pack_args = packs
    packager.bundle_compress_level = 1
    packager.portable_artifacts = []
    packager.persist_logical_file_modes = lambda: (_ for _ in ()).throw(RuntimeError('late metadata failure'))
    destination = Path(packager.output_path)
    shared = destination / (destination.name + '.zip')
    with zipfile.ZipFile(shared, 'w') as archive:
        archive.writestr('previous', 'complete')
    original = shared.read_bytes()
    (destination / 'same.AppDir').mkdir()
    (destination / 'same.AppDir/previous').write_text('preserve')
    (Path(packager.target_output_path) / 'same.AppDir').mkdir()
    (Path(packager.target_output_path) / 'same.AppDir/new').write_text('new')
    (Path(packager.target_output_path) / 'new.AppDir').mkdir()
    (Path(packager.target_output_path) / 'new.AppDir/new').write_text('partial')
    if mid_merge:
        copytree = shutil.copytree
        def fail_merge(source, target, *args, **kwargs):
            if Path(source) == Path(packager.target_output_path) and kwargs.get('dirs_exist_ok'):
                copytree(Path(source) / 'same.AppDir', destination / 'same.AppDir', dirs_exist_ok=True)
                copytree(Path(source) / 'new.AppDir', destination / 'new.AppDir')
                raise RuntimeError('late metadata failure during Root merge')
            return copytree(source, target, *args, **kwargs)
        monkeypatch.setattr(shutil, 'copytree', fail_merge)
    with pytest.raises(RuntimeError, match='late metadata'):
        packager.finalize_output()
    assert shared.read_bytes() == original
    assert (destination / 'same.AppDir/previous').read_text() == 'preserve'
    assert not (destination / 'same.AppDir/new').exists()
    assert not (destination / 'new.AppDir').exists()


def test_sdl_dlopen_libraries_come_from_compiled_elf_strings(tmp_path):
    import linux_portable
    executable = tmp_path / 'client'
    executable.write_bytes(b'\x7fELF\x00libXext.so.6\x00libXcursor.so.1\x00libasound.so.2\x00'
                           b'https://libXrandr.so.2/docs\x00libX11.so.6.trailing\x00')
    assert set(linux_portable.sdl_dynamic_libraries([executable])) == {'libXext.so.6', 'libXcursor.so.1', 'libasound.so.2'}


def test_tool_catalog_uses_versioned_assets():
    import linux_portable
    assert all('/continuous/' not in asset['url']
               for tool in linux_portable.tool_catalog()['tools'].values()
               for asset in tool['architectures'].values())


def test_explicit_plugin_path_keeps_the_requested_dlopen_name(tmp_path, monkeypatch):
    import linux_portable
    original = tmp_path / 'plugin-version-42'
    original.write_bytes(b'plugin')
    alias = tmp_path / 'plugin.so'
    try:
        alias.symlink_to(original)
    except OSError:
        pytest.skip('symlink creation is unavailable')
    monkeypatch.setattr(linux_portable, 'inspect_elf', lambda path: SimpleNamespace(bits=64, machine=62))
    assert linux_portable.find_system_library(str(alias), bits=64, machine=62).name == 'plugin.so'


def test_repack_replaces_modes_for_removed_variant_files(tmp_path):
    import json
    packager = packager_fixture(tmp_path)
    output = Path(packager.output_path)
    prefix = Path(packager.target_output_path).name + '/'
    (output / 'unrelated').write_text('preserved')
    (Path(packager.target_output_path) / 'new-file').write_text('new')
    (output / package.PACKAGE_MODE_MANIFEST).write_text(json.dumps({'version': 1, 'files': {
        prefix + 'removed-file': '644', 'unrelated': '644'}}))
    packager.logical_file_modes = {'new-file': 0o755}
    packager.persist_logical_file_modes()
    assert package.read_package_mode_manifest(output) == {'unrelated': 0o644, prefix + 'new-file': 0o755}


@pytest.mark.parametrize('change', ['extra-file', 'mode'])
def test_image_preflight_rejects_unvalidated_bundle_changes(tmp_path, monkeypatch, change):
    import hashlib
    import json
    import linux_portable
    monkeypatch.setattr(linux_portable.sys, 'platform', 'linux')
    app = tmp_path / 'bundle'
    app.mkdir()
    (app / 'AppRun.conf').write_text('policy=external\n')
    (app / 'main').write_bytes(b'validated')
    (app / 'main').chmod(0o644)
    metadata = {path.name: {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                           'mode': format(path.stat().st_mode & 0o777, '04o')}
                for path in app.iterdir()}
    (app / linux_portable.MANIFEST).write_text(json.dumps({'files': metadata, 'identity': {}}))
    if change == 'extra-file':
        (app / 'unvalidated.so').write_bytes(b'new plugin')
    else:
        (app / 'main').chmod(0o755 if os.name != 'nt' else 0o444)
    monkeypatch.setattr(linux_portable, 'verify_tool', lambda *args: None)
    monkeypatch.setattr(linux_portable, 'run_tool', lambda *args, **kwargs: (_ for _ in ()).throw(AssertionError('unsafe creator invocation')))
    with pytest.raises(ValueError, match='AppDir changed'):
        linux_portable.make_appimage(app, tmp_path / 'image.AppImage', tools={'appimagetool': tmp_path, 'runtime': tmp_path}, architecture='x64')
