from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import release_preflight  # noqa: E402


def git(root: Path, *args: str) -> str:
    return subprocess.run(['git', '-c', f'core.hooksPath={os.devnull}', '-c', 'protocol.file.allow=always', *args],
                          cwd=root, check=True, capture_output=True, text=True).stdout.strip()


def init_repository(root: Path, version: str = '1.2.3') -> Path:
    root.mkdir(parents=True, exist_ok=True)
    git(root, 'init', '--quiet', '--initial-branch=main')
    git(root, 'config', 'user.name', 'Release fixture')
    git(root, 'config', 'user.email', 'fixture@localhost')
    git(root, 'config', 'core.autocrlf', 'false')
    (root / '.gitignore').write_text('/assets/\n', encoding='utf-8', newline='\n')
    (root / 'VERSION').write_text(version + '\n', encoding='utf-8', newline='\n')
    git(root, 'add', '.')
    git(root, 'commit', '--quiet', '-m', 'Version ' + version)
    return root


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    return init_repository(tmp_path.resolve() / 'project')


def run(repo: Path, *args: str) -> int:
    return release_preflight.main(['--root', str(repo), *args])


def assets(repo: Path, files: dict[str, bytes]) -> Path:
    directory = repo / 'assets'
    directory.mkdir(exist_ok=True)
    for name, data in files.items():
        (directory / name).write_bytes(data)
    return directory


def test_release_ref_publishes_and_other_refs_dry_run(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert run(repo, '--release-ref', 'main') == 0
    output = capsys.readouterr().out
    assert 'mode=publish' in output
    assert 'tag=v1.2.3' in output

    assert run(repo, '--release-ref', 'main', '--ref', 'refs/heads/feature') == 0
    assert 'mode=dry-run' in capsys.readouterr().out
    assert run(repo, '--release-ref', 'main', '--dry-run') == 0
    assert 'mode=dry-run' in capsys.readouterr().out
    assert run(repo) == 0
    assert 'mode=dry-run' in capsys.readouterr().out


def test_existing_tag_fails_publish_and_warns_in_dry_run(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    git(repo, 'tag', 'v1.2.3')

    assert run(repo, '--release-ref', 'main') == 1
    assert 'already exists' in capsys.readouterr().err
    assert run(repo, '--release-ref', 'main', '--ref', 'feature') == 0
    captured = capsys.readouterr()
    assert 'warning: release tag v1.2.3 already exists' in captured.err
    assert 'mode=dry-run' in captured.out


def test_tag_on_remote_only_is_found_with_remote_option(repo: Path, tmp_path: Path) -> None:
    remote = tmp_path.resolve() / 'remote.git'
    git(tmp_path, 'init', '--quiet', '--bare', str(remote))
    git(repo, 'remote', 'add', 'origin', str(remote))
    git(repo, 'tag', 'v1.2.3')
    git(repo, 'push', '--quiet', 'origin', 'refs/tags/v1.2.3')
    git(repo, 'tag', '--delete', 'v1.2.3')

    assert run(repo, '--release-ref', 'main') == 0
    assert run(repo, '--release-ref', 'main', '--remote', 'origin') == 1


def test_version_must_exceed_newest_release_tag(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    git(repo, 'tag', 'v1.2.2')
    git(repo, 'tag', 'v1.10.0')
    git(repo, 'tag', 'v-nightly')

    assert run(repo, '--release-ref', 'main') == 1
    assert 'does not exceed the newest release tag v1.10.0' in capsys.readouterr().err


def test_previous_tag_is_reported(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    git(repo, 'tag', 'v1.2.2')
    git(repo, 'tag', 'v1.1.9')
    git(repo, 'tag', 'release-9.9.9')

    assert run(repo, '--release-ref', 'main') == 0
    assert 'previous_tag=v1.2.2' in capsys.readouterr().out


@pytest.mark.parametrize('content', ['1.2\n', '01.2.3\n', '1.2.3 \n', '1.2.3\n\n', ''])
def test_malformed_version_file_fails(repo: Path, content: str) -> None:
    (repo / 'VERSION').write_text(content, encoding='utf-8', newline='\n')
    git(repo, 'commit', '--quiet', '--allow-empty', '-am', 'Malformed version')

    assert run(repo) == 1


def test_scheme_limits_and_tag_prefix_apply(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    git(repo, 'tag', 'release-1.2.2')

    assert run(repo, '--limit', 'minor=2') == 1
    assert run(repo, '--tag-prefix', 'release-', '--release-ref', 'main') == 0
    output = capsys.readouterr().out
    assert 'tag=release-1.2.3' in output
    assert 'previous_tag=release-1.2.2' in output


def test_config_file_shared_with_project_version(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    (repo / 'BUILD').write_text('41\n', encoding='utf-8', newline='\n')
    config = repo / 'version.json'
    config.write_text(json.dumps({'file': 'BUILD', 'scheme': 'counter', 'base_ref': 'origin/main'}), encoding='utf-8')
    git(repo, 'add', '.')
    git(repo, 'commit', '--quiet', '-m', 'Counter')

    assert run(repo, '--config', str(config)) == 0
    assert 'tag=v41' in capsys.readouterr().out


def test_dirty_tracked_tree_and_shallow_clone_fail(repo: Path, tmp_path: Path) -> None:
    (repo / 'VERSION').write_text('1.2.4\n', encoding='utf-8', newline='\n')
    assert run(repo) == 1
    git(repo, 'checkout', '--', 'VERSION')

    (repo / 'untracked.txt').write_text('ignored by the clean check\n', encoding='utf-8')
    assert run(repo) == 0

    git(repo, 'commit', '--quiet', '--allow-empty', '-m', 'Second')
    clone = tmp_path.resolve() / 'shallow'
    subprocess.run(['git', 'clone', '--quiet', '--depth', '1', repo.as_uri(), str(clone)], check=True)
    assert run(clone) == 1


def test_assets_are_listed_hashed_and_written_to_outputs(repo: Path, tmp_path: Path) -> None:
    directory = assets(repo, {'Client.zip': b'client', 'Server.zip': b'server', 'notes.txt': b'notes'})
    (directory / 'unpacked.zip').mkdir()
    outputs = tmp_path / 'outputs.txt'
    summary = tmp_path / 'summary.md'

    assert run(repo, '--release-ref', 'main', '--assets-dir', str(directory), '--asset-pattern', '*.zip',
               '--manifest', 'SHA256SUMS', '--outputs-file', str(outputs), '--summary-file', str(summary)) == 0
    assert outputs.read_text(encoding='utf-8') == (
        'version=1.2.3\ntag=v1.2.3\nprevious_tag=\npublish=true\nassets=Client.zip Server.zip\n')
    assert (directory / 'SHA256SUMS').read_text(encoding='utf-8') == (
        f'{hashlib.sha256(b"client").hexdigest()}  Client.zip\n{hashlib.sha256(b"server").hexdigest()}  Server.zip\n')
    text = summary.read_text(encoding='utf-8')
    assert '### Release publish' in text
    assert hashlib.sha256(b'server').hexdigest() in text


def test_missing_and_empty_assets_fail(repo: Path) -> None:
    directory = assets(repo, {'notes.txt': b'notes'})
    assert run(repo, '--assets-dir', str(directory), '--asset-pattern', '*.zip') == 1

    (directory / 'Client.zip').write_bytes(b'')
    assert run(repo, '--assets-dir', str(directory), '--asset-pattern', '*.zip') == 1


def test_large_asset_is_split_verified_and_reassembles(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    data = bytes(range(256)) * 41
    directory = assets(repo, {'Server.zip': data, 'Client.zip': b'small'})

    assert run(repo, '--assets-dir', str(directory), '--max-asset-bytes', '4K', '--manifest', 'SHA256SUMS') == 0
    parts = sorted(path.name for path in directory.glob('Server.zip.part*'))
    assert parts == ['Server.zip.part00', 'Server.zip.part01', 'Server.zip.part02']
    assert not (directory / 'Server.zip').exists()
    assert [(directory / name).stat().st_size for name in parts] == [4096, 4096, len(data) - 8192]
    assert b''.join((directory / name).read_bytes() for name in parts) == data
    manifest = (directory / 'SHA256SUMS').read_text(encoding='utf-8')
    assert 'Client.zip\n' in manifest and 'Server.zip.part02\n' in manifest
    assert 'asset=' in capsys.readouterr().out
    assert not list(directory.glob('.*.tmp'))

    assert run(repo, '--assets-dir', str(directory), '--max-asset-bytes', '4K', '--manifest', 'SHA256SUMS') == 0
    assert sorted(path.name for path in directory.glob('Server.zip.part*')) == parts


def test_split_refuses_stale_parts_and_too_many_parts(repo: Path) -> None:
    directory = assets(repo, {'Server.zip': b'x' * 300, 'Server.zip.part00': b'stale'})
    assert run(repo, '--assets-dir', str(directory), '--asset-pattern', 'Server.zip', '--max-asset-bytes', '100') == 1
    assert (directory / 'Server.zip').read_bytes() == b'x' * 300

    (directory / 'Server.zip.part00').unlink()
    (directory / 'Server.zip').write_bytes(b'x' * (release_preflight.MAX_PARTS + 1))
    assert run(repo, '--assets-dir', str(directory), '--max-asset-bytes', '1') == 1
    assert (directory / 'Server.zip').is_file()
    assert not list(directory.glob('Server.zip.part*'))


def test_split_verification_failure_keeps_the_original(repo: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    directory = assets(repo, {'Server.zip': b'y' * 300})
    original_open = Path.open

    def corrupting_open(path, mode='r', *args, **kwargs):
        handle = original_open(path, mode, *args, **kwargs)
        if path.name.startswith('.Server.zip.part01') and mode == 'rb':
            handle.read(1)
        return handle

    monkeypatch.setattr(Path, 'open', corrupting_open)
    assert run(repo, '--assets-dir', str(directory), '--max-asset-bytes', '100') == 1
    assert (directory / 'Server.zip').read_bytes() == b'y' * 300
    assert not list(directory.glob('Server.zip.part*'))
    assert not list(directory.glob('.*.tmp'))


def test_engine_pin_checks(repo: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    engine_origin = init_repository(tmp_path.resolve() / 'engine-origin', '2026.1.1-dev')
    git(repo, 'submodule', '--quiet', 'add', str(engine_origin), 'Engine')
    git(repo, 'commit', '--quiet', '-m', 'Add Engine')
    engine = repo / 'Engine'

    assert run(repo, '--engine', 'Engine', '--release-ref', 'main') == 0
    git(engine, 'config', 'user.name', 'Release fixture')
    git(engine, 'config', 'user.email', 'fixture@localhost')
    git(engine, 'commit', '--quiet', '--allow-empty', '-m', 'Local only')
    assert run(repo, '--engine', 'Engine', '--release-ref', 'main') == 1
    assert 'M Engine' in capsys.readouterr().err

    git(repo, 'add', 'Engine')
    git(repo, 'commit', '--quiet', '-m', 'Pin an unpublished Engine commit')
    assert run(repo, '--release-ref', 'main') == 0
    assert run(repo, '--engine', 'Engine', '--release-ref', 'main') == 1
    assert 'is not reachable from any remote-tracking branch' in capsys.readouterr().err
    assert run(repo, '--engine', 'Engine', '--release-ref', 'main', '--ref', 'feature') == 0
    assert 'warning:' in capsys.readouterr().err
    assert run(repo, '--engine', 'VERSION', '--release-ref', 'main') == 1
    assert 'not a submodule gitlink' in capsys.readouterr().err


def test_usage_errors_exit_with_code_two(repo: Path) -> None:
    for arguments in (['--manifest', 'SUMS'], ['--max-asset-bytes', '1K'], ['--max-asset-bytes', '0'],
                      ['--max-asset-bytes', '1T']):
        with pytest.raises(SystemExit) as exit_info:
            run(repo, *arguments)
        assert exit_info.value.code == 2


def test_cli_entry_point(repo: Path) -> None:
    result = subprocess.run([sys.executable, str(BUILDTOOLS / 'release_preflight.py'), '--release-ref', 'main'],
                            cwd=repo, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines()[:2] == ['mode=publish', 'version=1.2.3']
