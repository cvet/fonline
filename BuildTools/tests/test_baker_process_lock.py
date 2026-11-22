from __future__ import annotations

import contextlib
import hashlib
import os
import subprocess
import time
from pathlib import Path

import pytest


pytestmark = pytest.mark.skipif(
    not os.environ.get('FO_TEST_UNIT_TEST_BINARY') or not os.environ.get('FO_TEST_BAKER_BINARY'),
    reason='Requires configured native unit tests and baker binaries',
)


@contextlib.contextmanager
def held_lock(path: Path, directory: Path, *, abrupt: bool = False):
    ready = directory / 'owner.ready'
    release = directory / 'owner.release'
    log = directory / 'owner.log'
    env = os.environ | {
        'FO_LOCK_PROBE_PATH': str(path),
        'FO_LOCK_PROBE_READY': str(ready),
        'FO_LOCK_PROBE_RELEASE': str(release),
    }
    with log.open('w', encoding='utf-8') as stream:
        process = subprocess.Popen(
            [os.environ['FO_TEST_UNIT_TEST_BINARY'], 'ExclusiveFileLockProcessProbe', '--skip-benchmarks'],
            cwd=directory, env=env, stdout=stream, stderr=subprocess.STDOUT,
        )
        try:
            deadline = time.monotonic() + 20
            while not ready.exists():
                assert process.poll() is None, log.read_text(encoding='utf-8', errors='replace')
                assert time.monotonic() < deadline, 'Lock owner did not become ready'
                time.sleep(0.01)
            yield process
        finally:
            if abrupt:
                process.kill()
            else:
                release.write_text('release', encoding='utf-8')
            try:
                code = process.wait(timeout=15)
                if not abrupt:
                    assert code == 0, log.read_text(encoding='utf-8', errors='replace')
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)


def bake(config: Path, directory: Path, *, force: bool = False) -> subprocess.CompletedProcess:
    return subprocess.run(
        [os.environ['FO_TEST_BAKER_BINARY'], '-ApplyConfig', str(config),
         '-Baking.ForceBaking', str(force)],
        cwd=directory, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30,
    )


def snapshot(directory: Path) -> dict[str, str]:
    return {
        path.relative_to(directory).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in directory.rglob('*') if path.is_file()
    }


@pytest.mark.parametrize('abrupt', [False, True])
def test_contending_baker_preserves_completed_outputs_and_retries_after_exit(tmp_path: Path, abrupt: bool):
    output = tmp_path / 'Ресурсы' / 'Baking'
    source = tmp_path / 'input'
    source.mkdir()
    (source / 'data.json').write_text('{"value":42}\n', encoding='utf-8')
    config = tmp_path / 'Lock.fomain'
    config.write_text(f'''Baking.BakeOutput = {output.as_posix()}
Baking.SingleThreadBaking = false
Baking.RawCopyFileExtensions = json
[ResourcePack]
Name = Core
InputDirs = input
Bakers = RawCopy
''', encoding='utf-8')
    first = bake(config, tmp_path)
    assert first.returncode == 0, first.stdout + first.stderr
    before = snapshot(output)
    assert 'Resources.build-hash' in before
    assert 'Resources.baking-config-hash' in before
    assert 'Baking.report.json' in before
    assert 'Baking.full.report.json' in before
    lock_path = output.with_name(output.name + '.lock')
    with held_lock(lock_path, tmp_path, abrupt=abrupt):
        for force in [False, True]:
            contender = bake(config, tmp_path, force=force)
            assert contender.returncode != 0
            assert 'Resource baking output lock is unavailable' in contender.stdout + contender.stderr
            assert snapshot(output) == before
    retried = bake(config, tmp_path, force=True)
    assert retried.returncode == 0, retried.stdout + retried.stderr
    assert (output / 'Core/data.json').read_text(encoding='utf-8') == '{"value":42}\n'
    assert lock_path.is_file()


def test_debug_diagnostics_owner_preserves_an_unstarted_output(tmp_path: Path):
    if not os.environ.get('FO_TEST_AS_DEBUG'):
        pytest.skip('Requires an AS_DEBUG baker')
    output = tmp_path / 'separate-output'
    config = tmp_path / 'DebugLock.fomain'
    (tmp_path / 'scripts').mkdir()
    (tmp_path / 'scripts/Test.fos').write_text('namespace DebugLock { void Touch() {} }\n', encoding='utf-8')
    config.write_text(f'''Baking.BakeOutput = {output.as_posix()}
[ResourcePack]
Name = Scripts
InputDirs = scripts
Bakers = AngelScript
''', encoding='utf-8')
    with held_lock(tmp_path / 'AS_DEBUG.lock', tmp_path):
        contender = bake(config, tmp_path)
        assert contender.returncode != 0
        assert 'AngelScript debug output lock is unavailable' in contender.stdout + contender.stderr
        assert not output.exists()
