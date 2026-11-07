from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import engine_upgrade_report as report  # noqa: E402


CHANGELOG = 'Docs/en/reference/changelog.md'


def git(root: Path, *args: str) -> str:
    return subprocess.run(['git', '-c', f'core.hooksPath={os.devnull}', '-c', 'protocol.file.allow=always', *args],
                          cwd=root, check=True, capture_output=True, text=True, encoding='utf-8').stdout.strip()


def init(root: Path) -> None:
    root.mkdir(parents=True)
    git(root, 'init', '--quiet', '--initial-branch=main')
    git(root, 'config', 'user.name', 'Report fixture')
    git(root, 'config', 'user.email', 'fixture@localhost')
    git(root, 'config', 'core.autocrlf', 'false')


def changelog(entries: list[tuple[str, str, str]], unreleased: str = '') -> str:
    text = '# Changelog\n\n## Unreleased\n\n' + (unreleased + '\n\n' if unreleased else '')
    for index, (version, date, migration) in enumerate(entries):
        text += f'## {"Current" if index == 0 else version} - {date}\n\n### Changed\n\n- Change {version}.\n\n'
        text += f'### Migration\n\n{migration}\n\n'
    return text


def engine_commit(engine: Path, version: str, entries: list[tuple[str, str, str]], subject: str,
                  extra: dict[str, str] | None = None, unreleased: str = '') -> str:
    files = {'VERSION': version + '\n', CHANGELOG: changelog(entries, unreleased), **(extra or {})}
    for name, content in files.items():
        (engine / name).parent.mkdir(parents=True, exist_ok=True)
        (engine / name).write_text(content, encoding='utf-8', newline='\n')
    git(engine, 'add', '.')
    git(engine, 'commit', '--quiet', '-m', subject)
    return git(engine, 'rev-parse', 'HEAD')


class Fixture:
    def __init__(self, base: Path) -> None:
        self.origin = base / 'engine-origin'
        init(self.origin)
        first = ('2026.1.1-dev', '2026-01-01', '- No migration is required.')
        second = ('2026.1.2-dev', '2026-02-01', '- Rename `OldSetting` to `NewSetting` in every `.fomain`.')
        third = ('2026.1.3-dev', '2026-03-01', '- Replace `Game.OldCall` with `Game.NewCall`; see commit `0123abc`.')
        self.old = engine_commit(self.origin, '2026.1.1-dev', [first], 'Initial Engine')
        self.middle = engine_commit(self.origin, '2026.1.2-dev', [second, first], 'Rename setting | part one',
                                    {'Docs/generated/settings.json': '{"v": 2}\n'})
        git(self.origin, 'checkout', '--quiet', '-b', 'topic')
        (self.origin / 'Source').mkdir()
        (self.origin / 'Source' / 'Game.cpp').write_text('NewCall\n', encoding='utf-8', newline='\n')
        git(self.origin, 'add', '.')
        git(self.origin, 'commit', '--quiet', '-m', 'Branch work')
        git(self.origin, 'checkout', '--quiet', 'main')
        git(self.origin, 'merge', '--quiet', '--no-ff', 'topic', '-m', 'Merge topic')
        self.new = engine_commit(self.origin, '2026.1.3-dev', [third, second, first], 'Replace call',
                                 unreleased='- Draft note.')

        self.project = base / 'project'
        init(self.project)
        (self.project / 'Scripts').mkdir()
        (self.project / 'Scripts' / 'Main.fos').write_text('Game.OldCall();\nOldSetting = 1\n', encoding='utf-8',
                                                           newline='\n')
        (self.project / 'Game.fomain').write_text('OldSetting = 2\n', encoding='utf-8', newline='\n')
        git(self.project, 'add', '.')
        git(self.project, 'submodule', '--quiet', 'add', self.origin.as_uri(), 'Engine')
        self.engine = self.project / 'Engine'
        git(self.engine, 'checkout', '--quiet', '--detach', self.old)
        git(self.project, 'add', 'Engine')
        git(self.project, 'commit', '--quiet', '-m', 'Project with Engine')
        git(self.engine, 'checkout', '--quiet', '--detach', self.new)

    def state(self) -> tuple[str, ...]:
        return (git(self.project, 'status', '--porcelain'), git(self.project, 'ls-files', '--stage', 'Engine'),
                git(self.engine, 'status', '--porcelain'), git(self.engine, 'rev-parse', 'HEAD'),
                git(self.project, 'stash', 'list'), git(self.engine, 'for-each-ref'))


@pytest.fixture(scope='module')
def fixture(tmp_path_factory: pytest.TempPathFactory) -> Fixture:
    return Fixture(tmp_path_factory.mktemp('upgrade').resolve())


def run(fixture: Fixture, tmp_path: Path, *args: str) -> tuple[int, str]:
    output = tmp_path.resolve() / 'record.md'
    code = report.main(['--project', str(fixture.project), '--output', str(output), *args])
    return code, output.read_text(encoding='utf-8') if output.exists() else ''


def test_records_identities_and_defaults_to_gitlink_and_checkout(fixture: Fixture, tmp_path: Path) -> None:
    code, text = run(fixture, tmp_path)

    assert code == 0
    assert text.startswith('# Engine update record: 2026.1.1-dev to 2026.1.3-dev\n')
    assert f'| Parent gitlink at HEAD | `{fixture.old}` | `2026.1.1-dev` |' in text
    assert f'| Index entry | `{fixture.old}` | - |' in text
    assert f'| Engine checkout | `{fixture.new}` (clean) | `2026.1.3-dev` |' in text
    assert f'`+{fixture.new} Engine' in text
    assert 'Ancestry check (baseline is an ancestor of the target): passed' in text


def test_lists_all_commits_oldest_first_and_publication_steps(fixture: Fixture, tmp_path: Path) -> None:
    _, text = run(fixture, tmp_path)

    commits = text.split('### Commit notes', 1)[1].split('## Changelog', 1)[0]
    subjects = [line.split(' | ')[1] for line in commits.splitlines() if line.startswith('| `')]
    assert subjects == ['Rename setting \\| part one', 'Branch work', 'Merge topic', 'Replace call']
    steps = text.split('### Publication steps', 1)[1].split('###', 1)[0]
    assert '`2026.1.2-dev`' in steps and '`2026.1.3-dev`' in steps and 'Branch work' not in steps
    assert '- `Docs/generated/settings.json`' in text
    assert 'Commits: 4' in text


def test_changelog_entries_and_migrations_oldest_first(fixture: Fixture, tmp_path: Path) -> None:
    _, text = run(fixture, tmp_path)

    second = text.index('### 2026.1.2-dev - 2026-02-01')
    third = text.index('### 2026.1.3-dev - 2026-03-01')
    assert second < third
    assert '> - Rename `OldSetting` to `NewSetting` in every `.fomain`.' in text
    assert '### 2026.1.1-dev' not in text
    assert '### Unreleased text at the target\n\n> - Draft note.' in text
    candidates = text.split('## Names quoted in the incoming migration notes', 1)[1].split('##', 1)[0]
    assert '- `OldSetting`' in candidates and '- `Game.NewCall`' in candidates
    assert '0123abc' not in candidates


def test_names_search_counts_project_matches_outside_engine(fixture: Fixture, tmp_path: Path) -> None:
    names = tmp_path.resolve() / 'names.txt'
    names.write_text('OldSetting\n\nGame.OldCall\nNewCall\nAbsentName\n', encoding='utf-8')

    code, text = run(fixture, tmp_path, '--names', str(names), '--max-locations', '1')
    assert code == 0
    assert '| `OldSetting` | 2 | `Game.fomain:1`, and 1 more |' in text
    assert '| `Game.OldCall` | 1 | `Scripts/Main.fos:1` |' in text
    assert '| `NewCall` | 0 | none | <fill in> | <fill in> | <fill in> |' in text
    assert '| `AbsentName` | 0 | none |' in text


def test_search_path_restricts_the_search(fixture: Fixture, tmp_path: Path) -> None:
    names = tmp_path.resolve() / 'names.txt'
    names.write_text('OldSetting\n', encoding='utf-8')

    _, text = run(fixture, tmp_path, '--names', str(names), '--search-path', 'Scripts')
    assert '| `OldSetting` | 1 | `Scripts/Main.fos:2` |' in text


def test_explicit_range_and_tag_target(fixture: Fixture, tmp_path: Path) -> None:
    git(fixture.engine, 'tag', 'v2026.1.2-dev', fixture.middle)

    code, text = run(fixture, tmp_path, '--old', fixture.old, '--new', 'v2026.1.2-dev')
    assert code == 0
    assert f'| Target (new) | `{fixture.middle}` | `2026.1.2-dev` |' in text
    assert '### 2026.1.3-dev' not in text
    assert 'Commits: 1' in text


def test_old_version_without_entry_is_reported(fixture: Fixture, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    code, text = run(fixture, tmp_path, '--old', fixture.new, '--new', fixture.new)

    assert code == 0
    assert 'The target adds no dated changelog entry after the baseline.' in text
    git(fixture.engine, 'tag', '-f', 'unlisted', fixture.old)
    names_only = tmp_path / 'other'
    names_only.mkdir()
    code = report.main(['--project', str(fixture.project), '--old', fixture.old, '--new', fixture.new,
                        '--changelog', 'VERSION', '--output', str(names_only / 'record.md')])
    assert code == 0
    assert 'has no entry in `VERSION`' in (names_only / 'record.md').read_text(encoding='utf-8')
    assert 'review the complete commit range' in capsys.readouterr().err


def test_non_ancestor_target_writes_the_record_and_fails(fixture: Fixture, tmp_path: Path) -> None:
    code, text = run(fixture, tmp_path, '--old', fixture.new, '--new', fixture.old)

    assert code == 1
    assert 'FAILED, a different history line' in text


def test_existing_output_needs_force(fixture: Fixture, tmp_path: Path) -> None:
    output = tmp_path.resolve() / 'record.md'
    output.write_text('filled-in plan\n', encoding='utf-8')

    with pytest.raises(SystemExit) as exit_info:
        report.main(['--project', str(fixture.project), '--output', str(output)])
    assert exit_info.value.code == 2
    assert output.read_text(encoding='utf-8') == 'filled-in plan\n'
    assert report.main(['--project', str(fixture.project), '--output', str(output), '--force']) == 0
    assert output.read_text(encoding='utf-8').startswith('# Engine update record')


def test_report_is_deterministic_and_read_only(fixture: Fixture, tmp_path: Path) -> None:
    before = fixture.state()
    first = run(fixture, tmp_path)[1]
    (tmp_path / 'record.md').unlink()

    assert run(fixture, tmp_path)[1] == first
    assert fixture.state() == before


def test_unknown_revision_and_uninitialized_engine_fail(fixture: Fixture, tmp_path: Path) -> None:
    assert run(fixture, tmp_path, '--new', 'no-such-revision')[0] == 1

    plain = tmp_path.resolve() / 'plain'
    init(plain)
    (plain / 'Engine').mkdir()
    (plain / 'README').write_text('x\n', encoding='utf-8')
    git(plain, 'add', 'README')
    git(plain, 'commit', '--quiet', '-m', 'Plain')
    assert report.main(['--project', str(plain), '--old', fixture.old, '--new', fixture.new]) == 1


def test_stdout_output_is_utf8(fixture: Fixture) -> None:
    result = subprocess.run([sys.executable, str(BUILDTOOLS / 'engine_upgrade_report.py')], cwd=fixture.project,
                            capture_output=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.decode('utf-8').startswith('# Engine update record: 2026.1.1-dev to 2026.1.3-dev')
