from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import project_version  # noqa: E402


class Repository:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.git('init', '--quiet', '--initial-branch=main')
        self.git('config', 'user.name', 'Version fixture')
        self.git('config', 'user.email', 'fixture@localhost')
        self.git('config', 'core.autocrlf', 'false')

    def git(self, *args: str) -> str:
        return subprocess.run(['git', '-c', f'core.hooksPath={os.devnull}', *args], cwd=self.root, check=True,
                              capture_output=True, text=True).stdout.strip()

    def commit_version(self, version: str, name: str = 'VERSION') -> str:
        (self.root / name).write_text(version + '\n', encoding='utf-8', newline='\n')
        self.git('add', name)
        self.git('commit', '--quiet', '-m', 'Version ' + version)
        return self.git('rev-parse', 'HEAD')

    def version(self, name: str = 'VERSION') -> str:
        return (self.root / name).read_text(encoding='utf-8')

    def run(self, *args: str) -> int:
        return project_version.main(['--root', str(self.root), *args])


@pytest.fixture
def repo(tmp_path: Path) -> Repository:
    return Repository(tmp_path.resolve())


@pytest.mark.parametrize(('limits', 'value', 'expected'), [
    ({}, (0, 3, 998), (0, 3, 999)),
    ({}, (0, 3, 9999), (0, 3, 10000)),
    ({'patch': 10000, 'minor': 10}, (0, 3, 9998), (0, 3, 9999)),
    ({'patch': 10000, 'minor': 10}, (0, 3, 9999), (0, 4, 0)),
    ({'patch': 10000, 'minor': 10}, (0, 9, 9999), (1, 0, 0)),
])
def test_patch_per_commit_increments_and_carries(limits: dict[str, int], value: tuple[int, ...],
                                                 expected: tuple[int, ...]) -> None:
    assert project_version.patch_per_commit_scheme(limits).bump(value) == expected


def test_exhausted_major_limit_fails() -> None:
    scheme = project_version.patch_per_commit_scheme({'major': 2, 'minor': 10, 'patch': 10})
    with pytest.raises(ValueError, match='major limit 2 is exhausted'):
        scheme.bump((1, 9, 9))


@pytest.mark.parametrize('text', ['0.3', '0.3.x', '0.03.1', '-1.0.0', '1.2.3.4', ' 1.2.3', '1.2.+3'])
def test_patch_per_commit_rejects_malformed_values(text: str) -> None:
    with pytest.raises(ValueError):
        project_version.patch_per_commit_scheme({}).parse(text)


@pytest.mark.parametrize('text', ['0.3.10000', '0.10.0'])
def test_patch_per_commit_rejects_values_outside_limits(text: str) -> None:
    with pytest.raises(ValueError, match='outside the limit'):
        project_version.patch_per_commit_scheme({'patch': 10000, 'minor': 10}).parse(text)


@pytest.mark.parametrize('limits', [{'build': 3}, {'patch': 0}, {'patch': True}])
def test_invalid_limits_are_rejected(limits: dict[str, object]) -> None:
    with pytest.raises(ValueError):
        project_version.patch_per_commit_scheme(limits)  # type: ignore[arg-type]


def test_counter_scheme() -> None:
    scheme = project_version.counter_scheme({'value': 100})
    assert scheme.bump(scheme.parse('41')) == (42,)
    with pytest.raises(ValueError):
        scheme.parse('1.0')
    with pytest.raises(ValueError, match='exhausted'):
        scheme.bump((99,))


@pytest.mark.parametrize('text', ['1.2.3\n', '1.2.3', '1.2.3\r\n'])
def test_one_line_with_one_line_ending_is_accepted(text: str) -> None:
    assert project_version.normalize_text(text, 'VERSION') == '1.2.3'


@pytest.mark.parametrize('text', ['', '\n', ' 1.2.3\n', '1.2.3 \n', '1.2.3\n\n', '1.2.3\r', '1.2.3\n1.2.4\n',
                                  '\ufeff1.2.3\n'])
def test_other_file_shapes_are_rejected(text: str) -> None:
    with pytest.raises(ValueError, match='exactly one line'):
        project_version.normalize_text(text, 'VERSION')


def test_generation_bumps_base_value(repo: Repository) -> None:
    base = repo.commit_version('1.4.316')
    repo.git('update-ref', 'refs/remotes/origin/main', base)

    assert repo.run('--base-ref', 'origin/main') == 0
    assert repo.version() == '1.4.317\n'


def test_generation_preserves_modified_file(repo: Repository) -> None:
    base = repo.commit_version('1.4.316')
    repo.git('update-ref', 'refs/remotes/origin/main', base)
    (repo.root / 'VERSION').write_text('1.5.0\n', encoding='utf-8', newline='\n')

    assert repo.run('--base-ref', 'origin/main') == 0
    assert repo.version() == '1.5.0\n'


def test_generation_rejects_modified_rollback_without_writing(repo: Repository) -> None:
    repo.commit_version('1.4.317')
    (repo.root / 'VERSION').write_text('1.4.300\n', encoding='utf-8', newline='\n')

    assert repo.run('--base-ref', 'HEAD') == 1
    assert repo.version() == '1.4.300\n'


def test_untracked_seed_is_accepted_as_modified(repo: Repository) -> None:
    (repo.root / 'README').write_text('seed\n', encoding='utf-8')
    repo.git('add', 'README')
    repo.git('commit', '--quiet', '-m', 'Initial')
    (repo.root / 'VERSION').write_text('0.1.0\n', encoding='utf-8', newline='\n')

    assert repo.run('--base-ref', 'HEAD') == 0
    assert repo.version() == '0.1.0\n'


def test_generation_requires_base_ref_and_readable_base(repo: Repository, capsys: pytest.CaptureFixture[str]) -> None:
    repo.commit_version('1.0.0')

    assert repo.run() == 1
    assert 'requires --base-ref' in capsys.readouterr().err
    assert repo.run('--base-ref', 'origin/missing') == 1
    assert repo.version() == '1.0.0\n'


def test_invalid_base_value_fails_without_writing(repo: Repository) -> None:
    repo.commit_version('1.0.0')
    repo.git('checkout', '--quiet', '-b', 'upstream')
    upstream = repo.commit_version('1.0.x')
    repo.git('checkout', '--quiet', 'main')
    repo.git('update-ref', 'refs/remotes/origin/main', upstream)

    assert repo.run('--base-ref', 'origin/main') == 1
    assert repo.version() == '1.0.0\n'


def test_stale_base_cannot_lower_local_version(repo: Repository) -> None:
    repo.commit_version('0.3.1004')
    old = repo.commit_version('0.3.1406')
    repo.git('update-ref', 'refs/remotes/origin/main', old)
    repo.commit_version('0.3.1432')

    assert repo.run('--base-ref', 'origin/main') == 0
    assert repo.version() == '0.3.1433\n'


def test_newer_base_is_used_when_generating(repo: Repository) -> None:
    repo.commit_version('0.3.1004')
    repo.git('checkout', '--quiet', '-b', 'upstream')
    newer = repo.commit_version('0.3.1450')
    repo.git('update-ref', 'refs/remotes/origin/main', newer)
    repo.git('checkout', '--quiet', 'main')
    repo.commit_version('0.3.1432')

    assert repo.run('--base-ref', 'origin/main') == 0
    assert repo.version() == '0.3.1451\n'


def test_check_rejects_committed_rollback_without_writing(repo: Repository) -> None:
    repo.commit_version('0.3.1417')
    repo.commit_version('0.3.1406')

    assert repo.run('--check') == 1
    assert repo.version() == '0.3.1406\n'


def test_check_accepts_equal_and_restored_values(repo: Repository) -> None:
    repo.commit_version('0.3.1417')
    assert repo.run('--check') == 0
    repo.commit_version('0.3.1406')
    repo.commit_version('0.3.1432')
    assert repo.run('--check') == 0


def test_merged_branch_higher_version_survives_lower_merge_result(repo: Repository) -> None:
    repo.commit_version('0.3.1004')
    repo.git('checkout', '--quiet', '-b', 'feature')
    repo.commit_version('0.3.1450')
    repo.git('checkout', '--quiet', 'main')
    repo.commit_version('0.3.1432')
    repo.git('merge', '--quiet', '--no-ff', '-s', 'ours', 'feature', '-m', 'Keep lower VERSION fixture')

    assert repo.run('--check') == 1


def test_since_commit_ignores_older_encoding(repo: Repository) -> None:
    repo.commit_version('0.4.0')
    epoch = repo.commit_version('0.3.1004')
    repo.commit_version('0.3.1432')

    assert repo.run('--check') == 1
    assert repo.run('--check', '--since-commit', epoch) == 0


def test_since_commit_excludes_merged_pre_epoch_branch(repo: Repository) -> None:
    repo.commit_version('0.4.0')
    epoch = repo.commit_version('0.3.1004')
    repo.git('checkout', '--quiet', '-b', 'old-encoding', epoch + '^')
    repo.commit_version('0.4.1')
    repo.git('checkout', '--quiet', 'main')
    repo.commit_version('0.3.1432')
    repo.git('merge', '--quiet', '--no-ff', '-s', 'ours', 'old-encoding', '-m', 'Merge pre-epoch branch fixture')

    assert repo.run('--check', '--since-commit', epoch) == 0


def test_since_commit_must_be_an_ancestor(repo: Repository) -> None:
    repo.commit_version('1.0.0')
    repo.git('checkout', '--quiet', '-b', 'side')
    side = repo.commit_version('1.0.1')
    repo.git('checkout', '--quiet', 'main')

    assert repo.run('--check', '--since-commit', side) == 1


def test_invalid_committed_value_fails_strictly(repo: Repository, capsys: pytest.CaptureFixture[str]) -> None:
    repo.commit_version('1.0')
    repo.commit_version('1.0.1')

    assert repo.run('--check') == 1
    assert 'a committed VERSION' in capsys.readouterr().err


def test_shallow_history_cannot_claim_monotonic_check(repo: Repository, tmp_path: Path) -> None:
    repo.commit_version('1.0.0')
    repo.commit_version('1.0.1')
    clone = tmp_path.resolve().parent / (tmp_path.name + '-shallow')
    subprocess.run(['git', 'clone', '--quiet', '--depth', '1', repo.root.as_uri(), str(clone)], check=True)

    assert project_version.main(['--root', str(clone), '--check']) == 1


def test_limits_apply_to_history_and_generation(repo: Repository) -> None:
    base = repo.commit_version('0.3.9999')
    repo.git('update-ref', 'refs/remotes/origin/main', base)

    assert repo.run('--base-ref', 'origin/main', '--limit', 'patch=10000', '--limit', 'minor=10') == 0
    assert repo.version() == '0.4.0\n'


def test_config_file_supplies_settings_and_command_line_wins(repo: Repository) -> None:
    (repo.root / 'meta').mkdir()
    base = repo.commit_version('7', 'meta/BUILD_NUMBER')
    repo.git('update-ref', 'refs/remotes/origin/main', base)
    config = repo.root / 'version.json'
    config.write_text(json.dumps({'file': 'meta/BUILD_NUMBER', 'base_ref': 'origin/missing', 'scheme': 'counter'}),
                      encoding='utf-8')

    assert repo.run('--config', str(config)) == 1
    assert repo.run('--config', str(config), '--base-ref', 'origin/main') == 0
    assert repo.version('meta/BUILD_NUMBER') == '8\n'


@pytest.mark.parametrize('content', ['[]', '{"scheme": 3}', '{"limits": [1]}', '{"unknown": 1}', '{'])
def test_invalid_config_is_rejected(repo: Repository, content: str) -> None:
    repo.commit_version('1.0.0')
    config = repo.root / 'version.json'
    config.write_text(content, encoding='utf-8')

    assert repo.run('--config', str(config), '--check') == 1


SCHEME_SOURCE = '''
class EvenScheme:
    def __init__(self, limits):
        self.step = limits.get("step", 2)

    def parse(self, text):
        if not text.startswith("r") or not text[1:].isdigit():
            raise ValueError("expected r<number>")
        return (int(text[1:]),)

    def bump(self, value):
        return (value[0] + self.step,)

    def format(self, value):
        return f"r{value[0]}"


def create_scheme(limits):
    return EvenScheme(limits)
'''


def test_project_scheme_from_file_and_module(repo: Repository) -> None:
    (repo.root / 'tools').mkdir()
    (repo.root / 'tools' / 'version_scheme.py').write_text(SCHEME_SOURCE, encoding='utf-8')
    base = repo.commit_version('r10')
    repo.git('update-ref', 'refs/remotes/origin/main', base)

    assert repo.run('--base-ref', 'origin/main', '--scheme', 'tools/version_scheme.py:create_scheme') == 0
    assert repo.version() == 'r12\n'
    repo.git('commit', '--quiet', '-am', 'Bump')
    assert repo.run('--base-ref', 'origin/main', '--scheme', 'tools.version_scheme:create_scheme',
                    '--limit', 'step=5') == 0
    assert repo.version() == 'r17\n'


@pytest.mark.parametrize('scheme', ['semantic', 'tools/missing.py:create_scheme', 'missing_module:create_scheme',
                                    'tools/version_scheme.py:absent', 'tools/version_scheme.py:not-an-identifier'])
def test_unknown_or_broken_scheme_is_rejected(repo: Repository, scheme: str) -> None:
    (repo.root / 'tools').mkdir()
    (repo.root / 'tools' / 'version_scheme.py').write_text(SCHEME_SOURCE, encoding='utf-8')
    repo.commit_version('r10')

    assert repo.run('--check', '--scheme', scheme) == 1


def test_version_file_outside_work_tree_is_rejected(repo: Repository) -> None:
    repo.commit_version('1.0.0')

    assert repo.run('--check', '--file', '../VERSION') == 1


def test_cli_entry_point_reports_usage_errors_with_exit_code_two(repo: Repository) -> None:
    result = subprocess.run([sys.executable, str(BUILDTOOLS / 'project_version.py'), '--unknown'],
                            cwd=repo.root, capture_output=True, text=True)
    assert result.returncode == 2

    repo.commit_version('2.0.0')
    result = subprocess.run([sys.executable, str(BUILDTOOLS / 'project_version.py'), '--check'],
                            cwd=repo.root, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert 'VERSION 2.0.0 is valid' in result.stdout
