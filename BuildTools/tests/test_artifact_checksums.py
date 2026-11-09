from __future__ import annotations

import hashlib
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import artifact_checksums as checksums  # noqa: E402


ABC_DIGEST = 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'


@pytest.fixture
def root(tmp_path: Path) -> Path:
    return tmp_path.resolve()


def artifact(root: Path, name: str, data: bytes = b'abc') -> Path:
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def sidecar(path: Path) -> Path:
    return path.with_name(path.name + '.sha256')


def make_symlink(link: Path, target: Path, directory: bool = False) -> None:
    try:
        link.symlink_to(target, target_is_directory=directory)
    except (OSError, NotImplementedError) as error:
        pytest.skip(f'Symlinks unavailable on this host: {error}')


def test_known_digest_and_standard_single_line(root: Path) -> None:
    client = artifact(root, 'client.zip')

    assert checksums.write_checksums(root) == 1
    assert sidecar(client).read_bytes() == ABC_DIGEST.encode() + b'  client.zip\n'


def test_default_scope_selects_root_archives_and_installers_only(root: Path) -> None:
    names = ['Client.ZIP', 'Server.tar.gz', 'Server.tar', 'Installer.msi', 'Android.apk']
    for name in names:
        artifact(root, name)
    artifact(root, 'symbols.pdb')
    artifact(root, 'Launcher.exe')
    artifact(root, 'Client/Resources/Embedded.zip')
    (root / 'directory.zip').mkdir()

    assert checksums.write_checksums(root) == len(names)
    assert {path.name for path in root.rglob('*.sha256')} == {name + '.sha256' for name in names}


def test_custom_suffixes_replace_the_default_selection(root: Path) -> None:
    artifact(root, 'client.zip')
    launcher = artifact(root, 'Launcher.EXE')

    assert checksums.main([str(root), '--suffix', '.exe']) == 0
    assert sidecar(launcher).is_file()
    assert not sidecar(root / 'client.zip').exists()


@pytest.mark.parametrize('suffix', ['exe', '.', '.a/b', '.a\\b', '.a:b'])
def test_invalid_suffix_is_rejected(root: Path, suffix: str) -> None:
    artifact(root, 'client.zip')

    assert checksums.main([str(root), '--suffix', suffix]) == 1
    assert not list(root.glob('*.sha256'))


def test_explicit_nested_paths_spaces_and_unicode_use_sibling_leaf_name(root: Path) -> None:
    nested = artifact(root, 'packages with spaces/Клиент final.zip')
    artifact(root, 'unselected.zip')
    selected = ['packages with spaces/Клиент final.zip']

    assert checksums.write_checksums(root, selected * 2) == 1
    assert sidecar(nested).read_text(encoding='utf-8') == hashlib.sha256(b'abc').hexdigest() + '  Клиент final.zip\n'
    assert not (root / 'unselected.zip.sha256').exists()


def test_manifest_lists_relative_paths_sorted(root: Path) -> None:
    artifact(root, 'b.zip', b'b')
    artifact(root, 'nested/a.zip', b'a')

    assert checksums.write_checksums(root, ['nested/a.zip', 'b.zip'], manifest='SHA256SUMS') == 2
    assert (root / 'SHA256SUMS').read_text(encoding='utf-8') == (
        f'{hashlib.sha256(b"b").hexdigest()}  b.zip\n'
        f'{hashlib.sha256(b"a").hexdigest()}  nested/a.zip\n')
    assert sidecar(root / 'b.zip').is_file()


def test_manifest_only_writes_no_sidecars_and_is_not_hashed_itself(root: Path) -> None:
    artifact(root, 'client.zip')

    assert checksums.main([str(root), '--manifest', 'client-sums.zip', '--no-sidecars']) == 0
    assert (root / 'client-sums.zip').read_text(encoding='utf-8') == f'{ABC_DIGEST}  client.zip\n'
    assert not list(root.glob('*.sha256'))
    assert checksums.main([str(root), '--manifest', 'client-sums.zip', '--no-sidecars']) == 0
    assert (root / 'client-sums.zip').read_text(encoding='utf-8') == f'{ABC_DIGEST}  client.zip\n'


def test_no_sidecars_requires_manifest(root: Path) -> None:
    with pytest.raises(SystemExit) as exit_info:
        checksums.main([str(root), '--no-sidecars'])
    assert exit_info.value.code == 2


@pytest.mark.parametrize('name', ['', '.', '..', 'nested/SUMS', 'nested\\SUMS', 'SUMS:stream'])
def test_unsafe_manifest_name_is_rejected(root: Path, name: str) -> None:
    artifact(root, 'client.zip')

    with pytest.raises(ValueError, match='plain file name'):
        checksums.write_checksums(root, manifest=name)
    assert not list(root.glob('*.sha256'))


def test_rerun_hashes_final_bytes_and_never_hashes_sidecars(root: Path) -> None:
    client = artifact(root, 'client.zip', b'old package')
    checksums.write_checksums(root)
    original = sidecar(client).read_bytes()
    client.write_bytes(b'new package')

    assert checksums.write_checksums(root) == 1
    updated = sidecar(client).read_bytes()
    assert updated != original
    assert updated == (hashlib.sha256(b'new package').hexdigest() + '  client.zip\n').encode()
    checksums.write_checksums(root)
    assert sidecar(client).read_bytes() == updated
    assert sorted(path.name for path in root.iterdir()) == ['client.zip', 'client.zip.sha256']


def test_reads_are_bounded_and_hash_all_chunks(root: Path) -> None:
    data = bytes(range(256)) * (checksums.HASH_CHUNK_SIZE // 128)
    server = artifact(root, 'server.zip', data)
    original_open = Path.open
    reads = []

    def bounded_open(path, *args, **kwargs):
        source = original_open(path, *args, **kwargs)
        if path == server and args == ('rb',):
            original_read = source.read

            def read(size=-1):
                assert 0 < size <= checksums.HASH_CHUNK_SIZE
                reads.append(size)
                return original_read(size)

            source.read = read
        return source

    with patch.object(Path, 'open', bounded_open):
        checksums.write_checksums(root)
    assert len(reads) > 2
    assert sidecar(server).read_text().startswith(hashlib.sha256(data).hexdigest())


def test_hash_failure_preserves_all_existing_sidecars(root: Path) -> None:
    first = artifact(root, 'a.zip')
    second = artifact(root, 'b.zip')
    sidecar(first).write_bytes(b'previous complete sidecar\n')
    original_hash = checksums.hash_artifact

    def failing_hash(path):
        if path == second:
            raise OSError('simulated read failure')
        return original_hash(path)

    with patch.object(checksums, 'hash_artifact', failing_hash):
        with pytest.raises(OSError, match='read failure'):
            checksums.write_checksums(root, manifest='SHA256SUMS')
    assert sidecar(first).read_bytes() == b'previous complete sidecar\n'
    assert not sidecar(second).exists()
    assert not (root / 'SHA256SUMS').exists()
    assert not list(root.glob('.*.tmp'))


def test_stage_write_failure_leaves_no_partial_sidecar_or_temporary(root: Path) -> None:
    first = artifact(root, 'a.zip')
    second = artifact(root, 'b.zip')
    sidecar(first).write_bytes(b'previous complete sidecar\n')

    with patch.object(os, 'fsync', side_effect=[None, OSError('simulated write failure')]):
        with pytest.raises(OSError, match='write failure'):
            checksums.write_checksums(root)
    assert sidecar(first).read_bytes() == b'previous complete sidecar\n'
    assert not sidecar(second).exists()
    assert not list(root.glob('.*.tmp'))


def test_replace_failure_preserves_previous_complete_sidecar(root: Path) -> None:
    client = artifact(root, 'client.zip')
    sidecar(client).write_bytes(b'previous complete sidecar\n')

    with patch.object(os, 'replace', side_effect=OSError('simulated rename failure')):
        with pytest.raises(OSError, match='rename failure'):
            checksums.write_checksums(root)
    assert sidecar(client).read_bytes() == b'previous complete sidecar\n'
    assert not list(root.glob('.*.tmp'))


def test_changed_input_is_rejected(root: Path) -> None:
    client = artifact(root, 'client.zip')
    original_identity = checksums.file_identity
    calls = 0

    def identity_and_mutation(metadata):
        nonlocal calls
        calls += 1
        if calls == 1:
            with client.open('ab') as target:
                target.write(b'changed')
        return original_identity(metadata)

    with patch.object(checksums, 'file_identity', identity_and_mutation):
        with pytest.raises(OSError, match='changed while hashing'):
            checksums.write_checksums(root)
    assert not sidecar(client).exists()


def test_input_changed_after_hashing_is_rejected(root: Path) -> None:
    client = artifact(root, 'client.zip')
    original_hash = checksums.hash_artifact

    def hash_then_change(path):
        result = original_hash(path)
        with path.open('ab') as target:
            target.write(b'changed')
        return result

    with patch.object(checksums, 'hash_artifact', hash_then_change):
        with pytest.raises(OSError, match='changed after hashing'):
            checksums.write_checksums(root)
    assert not sidecar(client).exists()


@pytest.mark.skipif(os.name != 'nt', reason='Windows fstat/path.stat regression')
def test_windows_fstat_and_path_stat_identity_is_stable(root: Path) -> None:
    for index in range(100):
        path = artifact(root, f'client-{index}.zip')
        with path.open('rb') as source:
            handle_identity = checksums.file_identity(os.fstat(source.fileno()))
        assert checksums.file_identity(path.stat()) == handle_identity


@pytest.mark.parametrize('name', ['../outside.zip', '/absolute.zip', 'C:/absolute.zip', 'nested\\client.zip',
                                  'line\nbreak.zip', 'line\rbreak.zip', 'nested/client:stream.zip', 'wildcard*.zip',
                                  'client.zip.sha256', 'notes.txt', ''])
def test_unsafe_explicit_paths_are_rejected_before_any_write(root: Path, name: str) -> None:
    artifact(root, 'good.zip')

    with pytest.raises(ValueError):
        checksums.write_checksums(root, ['good.zip', name])
    assert not (root / 'good.zip.sha256').exists()


def test_symlink_artifact_is_rejected(root: Path) -> None:
    original = artifact(root, 'original.zip')
    make_symlink(root / 'link.zip', original)

    with pytest.raises(ValueError, match='links'):
        checksums.write_checksums(root)
    assert not list(root.glob('*.sha256'))


def test_symlink_parent_is_rejected(root: Path) -> None:
    artifact(root, 'real/client.zip')
    make_symlink(root / 'link', root / 'real', directory=True)

    with pytest.raises(ValueError, match='links'):
        checksums.write_checksums(root, ['link/client.zip'])


def test_symlink_sidecar_does_not_overwrite_target(root: Path) -> None:
    artifact(root, 'client.zip')
    target = artifact(root, 'untouched.txt', b'keep')
    make_symlink(sidecar(root / 'client.zip'), target)

    with pytest.raises(ValueError, match='destination'):
        checksums.write_checksums(root)
    assert target.read_bytes() == b'keep'


def test_verify_accepts_current_outputs_and_rejects_changed_bytes(root: Path) -> None:
    client = artifact(root, 'client.zip')
    artifact(root, 'nested/server.zip', b'server')
    selection = ['client.zip', 'nested/server.zip']
    checksums.write_checksums(root, selection, manifest='SHA256SUMS')

    assert checksums.verify_checksums(root, selection, manifest='SHA256SUMS') == 2
    client.write_bytes(b'tampered')
    with pytest.raises(ValueError, match='sidecar does not match client.zip.*manifest entry does not match client.zip'):
        checksums.verify_checksums(root, selection, manifest='SHA256SUMS')


def test_verify_reports_missing_sidecar_and_unlisted_artifact(root: Path) -> None:
    artifact(root, 'client.zip')
    checksums.write_checksums(root, manifest='SHA256SUMS', sidecars=False)
    artifact(root, 'server.zip')

    with pytest.raises(ValueError, match='missing sidecar client.zip.sha256'):
        checksums.verify_checksums(root)
    with pytest.raises(ValueError, match='manifest does not list server.zip'):
        checksums.verify_checksums(root, manifest='SHA256SUMS', sidecars=False)


@pytest.mark.parametrize('text', ['abc  client.zip\n', f'{ABC_DIGEST} client.zip\n',
                                  f'{ABC_DIGEST}  client.zip\n{ABC_DIGEST}  client.zip\n',
                                  f'{ABC_DIGEST}  ../client.zip\n'])
def test_verify_rejects_malformed_manifest(root: Path, text: str) -> None:
    artifact(root, 'client.zip')
    (root / 'SHA256SUMS').write_text(text, encoding='utf-8', newline='\n')

    with pytest.raises(ValueError):
        checksums.verify_checksums(root, manifest='SHA256SUMS', sidecars=False)


def test_cli_succeeds_for_finished_package_and_fails_for_empty_directory(root: Path) -> None:
    command = [sys.executable, str(BUILDTOOLS / 'artifact_checksums.py'), str(root)]
    missing = subprocess.run(command, capture_output=True, text=True)
    assert missing.returncode == 1
    assert 'No release artifacts' in missing.stderr

    client = artifact(root, 'client.zip')
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert sidecar(client).is_file()
    verified = subprocess.run([*command, '--verify'], capture_output=True, text=True)
    assert verified.returncode == 0, verified.stderr
    assert 'verified 1 artifact(s)' in verified.stdout
