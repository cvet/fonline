from __future__ import annotations

import hashlib
import datetime
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path
from unittest.mock import patch


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import codegen
import engine_version
import docs_engine_version


class EngineVersionTests(unittest.TestCase):
    def test_current_changelog_uses_version_and_preserves_previous_release(self) -> None:
        old, new = '2026.1.21-dev', '2026.1.22-dev'
        inputs = {('show', 'base:VERSION'): old + '\n', ('show', 'target:VERSION'): new + '\n',
                  ('show', '-s', '--format=%cI', 'target'): '2026-10-04T12:00:00+00:00',
                  ('diff', '--name-only', 'base', 'target'): 'VERSION\nDocs/en/reference/changelog.md'}
        for path, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция')):
            previous = f'## Current - 2026-10-04\n\n### {label}\nNo migration.\n'
            inputs['show', f'base:{path}'] = '# Log\n\n## Unreleased\n\n' + previous
            inputs['show', f'target:{path}'] = inputs['show', f'base:{path}'] + '\n' + previous.replace('Current', old)
        with patch.object(docs_engine_version, 'git', side_effect=lambda root, *args, **kwargs: inputs[args]):
            docs_engine_version.validate_committed_update(Path('.'), 'base', 'target', 'master')
            for path in engine_version.CHANGELOG_PATHS:
                inputs['show', f'target:{path}'] = inputs['show', f'base:{path}']
            with self.assertRaisesRegex(ValueError, 'preceding|previous|retain'):
                docs_engine_version.validate_committed_update(Path('.'), 'base', 'target', 'master')
        notes = [inputs['show', f'base:{path}'] for path in engine_version.CHANGELOG_PATHS]
        engine_version.validate_changelog_texts(new, notes)
        notes[0] += f'\n## {new} - 2026-10-04\n\n### Migration\nDuplicate.\n'
        with self.assertRaisesRegex(ValueError, 'duplicate|Duplicate'):
            engine_version.validate_changelog_texts(new, notes)
        notes = [inputs['show', f'base:{path}'].replace('## Unreleased\n\n',
                 f'## Unreleased\n\n## {old} - 2026-10-04\n\n### {label}\nExisting notes.\n\n')
                 for path, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция'))]
        with self.assertRaisesRegex(ValueError, 'Current must be the first'):
            engine_version.validate_changelog_texts(new, notes)

    def test_notation_and_file_shape(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for version in ("2026.1.21-dev", "2026.13.0-dev", "2026.1.21.0-rc",
                            "2026.1.21.1-rc", "2026.1.21.1", "2026.1.21.2"):
                for newline in ("", "\n", "\r\n"):
                    with self.subTest(version=version, newline=newline):
                        (root / "VERSION").write_bytes((version + newline).encode())
                        self.assertEqual(version, engine_version.read_engine_version(root))
            for value in ("", "2022.1.0.wip", "0.1.0-dev", "2026.0.0-dev", "2026.1.21",
                          "2026.01.0-dev", "2026.1.01-dev", "2026.1.21-rc", "2026.1.21.0-rc.1",
                          "2026.1.21.0-dev", "2026.1.21.01", "2026.1.21-alpha.1", "2026.1.21.0-beta.1",
                          "2026.1.21-dev.1", "2026.1.21+git.123", "v2026.1.21-dev",
                          " 2026.1.21-dev", "2026.1.21-dev\n\n", "2026.1.21-dev\n2026.1.22-dev", "\ufeff2026.1.21-dev"):
                with self.subTest(invalid=value):
                    (root / "VERSION").write_text(value, encoding="utf-8")
                    with self.assertRaises(ValueError):
                        engine_version.read_engine_version(root)
            (root / "VERSION").unlink()
            with self.assertRaisesRegex(ValueError, "Unable to read"):
                engine_version.read_engine_version(root)

    @unittest.skipUnless(shutil.which("git"), "Git is required")
    def test_engine_git_identity_dirty_state_and_nested_archive(self) -> None:
        def git(root: Path, *args: str) -> str:
            return subprocess.check_output(
                ["git", "-C", str(root), "-c", "user.name=Version Test",
                 "-c", "user.email=version@example.invalid", *args],
                text=True, stderr=subprocess.STDOUT,
            ).strip()

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            git(root, "init")
            git(root, "commit", "--allow-empty", "-m", "project")
            engine = root / "Engine with spaces"
            engine.mkdir()
            self.assertEqual("unknown", engine_version.read_engine_revision(engine))
            git(engine, "init")
            (engine / "VERSION").write_text("2026.1.1-dev\n")
            git(engine, "add", "VERSION")
            git(engine, "commit", "-m", "engine")
            revision = git(engine, "rev-parse", "HEAD")
            self.assertNotEqual(git(root, "rev-parse", "HEAD"), revision)
            self.assertEqual(revision, engine_version.read_engine_revision(engine))
            (engine / "VERSION").write_text("2026.1.2-dev\n")
            self.assertEqual(revision + "-dirty", engine_version.read_engine_revision(engine))
            git(engine, "checkout", "--", "VERSION")
            git(engine, "checkout", "--detach")
            self.assertEqual(revision, engine_version.read_engine_revision(engine))

    def test_native_metadata_keeps_engine_project_and_compatibility_separate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            args = Namespace(enginedefine=[], buildhash="project-sha", devname="Probe",
                             nicename="Probe", genoutput=directory)
            compatibility = hashlib.sha256(b"runtime contract")
            headers = []
            for version in ("2026.1.21-dev", "2026.1.21.2"):
                output = codegen.GeneratedOutput()
                with patch.object(codegen, "args", args), patch.object(codegen, "generated_output", output), \
                     patch.object(codegen, "compatibility_hasher", compatibility), \
                     patch.object(codegen, "read_engine_version", return_value=version), \
                     patch.object(codegen, "read_engine_revision", return_value="engine-sha-dirty"):
                    codegen.write_engine_config()
                header = "\n".join(next(iter(output.files.values())))
                self.assertIn(f'#define FO_ENGINE_VERSION "{version}"', header)
                self.assertIn('#define FO_ENGINE_REVISION "engine-sha-dirty"', header)
                self.assertIn('#define FO_BUILD_HASH "project-sha"', header)
                self.assertIn(f'#define FO_COMPATIBILITY_VERSION "{compatibility.hexdigest()[:16]}"', header)
                headers.append(header)
            self.assertNotEqual(headers[0], headers[1])

    def test_bilingual_release_notes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative_path in engine_version.CHANGELOG_PATHS:
                path = root / relative_path
                path.parent.mkdir(parents=True)
                path.write_text("# Changelog\n\n## Unreleased\n")
            (root / "VERSION").write_text("2026.1.21-dev\n")
            with self.assertRaisesRegex(ValueError, "no dated bilingual"):
                engine_version.validate_changelogs(root)
            for relative_path, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция')):
                (root / relative_path).write_text(
                    f"# Changelog\n\n## Unreleased\n\n## 2026.1.21-dev - 2026-10-04\n\n### {label}\n\nNo migration required.\n",
                    encoding='utf-8')
            engine_version.validate_changelogs(root)
            russian = root / engine_version.CHANGELOG_PATHS[1]
            russian.write_text(russian.read_text(encoding='utf-8').replace("2026-10-04", "2026-10-05"), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, "versions/dates differ"):
                engine_version.validate_changelogs(root)
            russian.write_text(russian.read_text(encoding='utf-8').replace("2026-10-05", "2027-10-05"), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, "year differs"):
                engine_version.validate_changelogs(root)
            russian.write_text("# Changelog\n")
            with self.assertRaisesRegex(ValueError, "exactly one Unreleased"):
                engine_version.validate_changelogs(root)

    def test_master_and_release_progression(self) -> None:
        date = datetime.date(2026, 10, 4)
        for old, new, branch in (
            ('2022.1.0.wip', '2026.1.1-dev', 'master'),
            ('2026.1.21-dev', '2026.1.22-dev', 'master'),
            ('2026.1.21-dev', '2026.2.0-dev', 'master'),
            ('2025.3.14-dev', '2026.1.0-dev', 'master'),
            ('2026.1.21-dev', '2026.1.21.0-rc', 'release/2026.1'),
            ('2026.1.21.0-rc', '2026.1.21.1-rc', 'release/2026.1'),
            ('2026.1.21.1-rc', '2026.1.21.1', 'release/2026.1'),
            ('2026.1.21.1', '2026.1.21.2', 'release/2026.1'),
            ('2025.1.21.1', '2025.1.21.2', 'release/2025.1'),
        ):
            with self.subTest(old=old, new=new):
                engine_version.validate_transition(old, new, branch, date)
        for old, new, branch in (
            ('2026.1.21-dev', '2026.1.21-dev', 'master'),
            ('2026.1.21-dev', '2026.1.23-dev', 'master'),
            ('2026.1.21-dev', '2026.3.0-dev', 'master'),
            ('2026.1.21-dev', '2026.2.1-dev', 'master'),
            ('2025.1.21-dev', '2025.1.22-dev', 'master'),
            ('2026.1.21-dev', '2026.1.21.0', 'master'),
            ('2026.1.21-dev', '2026.1.21.1-rc', 'release/2026.1'),
            ('2026.1.21.1', '2026.1.21.2-rc', 'release/2026.1'),
            ('2026.1.21.1', '2026.1.22.2', 'release/2026.1'),
            ('2026.1.21.1', '2026.1.21.3', 'release/2026.1'),
            ('2026.1.21.1', '2026.1.21.2', 'release/2026.2'),
        ):
            with self.subTest(old=old, new=new):
                with self.assertRaises(ValueError):
                    engine_version.validate_transition(old, new, branch, date)

    @unittest.skipUnless(shutil.which('git'), 'Git is required')
    def test_publication_history_detects_an_unversioned_intermediate_commit(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                environment = {**os.environ, 'GIT_AUTHOR_DATE': '2026-10-04T12:00:00+00:00',
                               'GIT_COMMITTER_DATE': '2026-10-04T12:00:00+00:00'}
                return subprocess.check_output(['git', '-C', str(root), '-c', 'user.name=Version Test',
                    '-c', 'user.email=version@example.invalid', *args], env=environment,
                    text=True, stderr=subprocess.STDOUT).strip()
            def commit(version):
                (root / 'VERSION').write_text(version + '\n')
                for relative, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция')):
                    path = root / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    older = path.read_text(encoding='utf-8').split('## Unreleased\n', 1)[1] if path.exists() else ''
                    path.write_text(f'# Changelog\n\n## Unreleased\n\n## {version} - 2026-10-04\n\n### {label}\nNo migration needed.\n' + older, encoding='utf-8')
                git('add', '.')
                git('commit', '-m', version)
                return git('rev-parse', 'HEAD')
            git('init')
            base = commit('2026.1.0-dev')
            first = commit('2026.1.1-dev')
            second = commit('2026.1.2-dev')
            self.assertEqual(2, docs_engine_version.validate_history(root, base, second, 'master'))
            (root / 'Source.txt').write_text('unversioned code change\n')
            git('add', '.')
            git('commit', '-m', 'missing version bump')
            final = commit('2026.1.3-dev')
            with self.assertRaisesRegex(ValueError, 'increment minor'):
                docs_engine_version.validate_history(root, first, final, 'master')

    @unittest.skipUnless(shutil.which('git'), 'Git is required')
    def test_published_parent_history_and_invalid_children(self) -> None:
        for case in ('valid', 'missing-bump', 'lost-current', 'direct-skip'):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                hooks = root / 'empty-hooks'
                hooks.mkdir()
                environment = {key: value for key, value in os.environ.items()
                               if not key.startswith('GIT_')}
                environment.update(GIT_DIR=str(root / ".git"), GIT_WORK_TREE=str(root),
                                   GIT_AUTHOR_DATE='2026-10-10T12:00:00+00:00',
                                   GIT_COMMITTER_DATE='2026-10-10T12:00:00+00:00')

                def git(*args: str) -> str:
                    return subprocess.check_output(
                        ['git', '-C', str(root), '-c', 'user.name=Version Test',
                         '-c', 'user.email=version@example.invalid',
                         '-c', f'core.hooksPath={hooks}', *args],
                        env=environment, text=True, stderr=subprocess.STDOUT).strip()

                previous_version = None
                step = 0

                def commit(version: str, preserve: bool = True) -> str:
                    nonlocal previous_version, step
                    (root / 'VERSION').write_text(version + '\n', encoding='utf-8')
                    for relative, label in zip(engine_version.CHANGELOG_PATHS,
                                               ('Migration', 'Миграция'), strict=True):
                        path = root / relative
                        path.parent.mkdir(parents=True, exist_ok=True)
                        history = ''
                        if preserve and path.exists():
                            history = path.read_text(encoding='utf-8').split('## Unreleased\n', 1)[1]
                            history = history.replace('## Current -', f'## {previous_version} -', 1)
                        path.write_text(
                            f'# Log\n\n## Unreleased\n\n## Current - 2026-10-10\n\n'
                            f'### {label}\nNo migration required.\n' + history,
                            encoding='utf-8')
                    step += 1
                    (root / 'Step.txt').write_text(str(step), encoding='utf-8')
                    git('add', '.')
                    git('commit', '-m', version)
                    previous_version = version
                    return git('rev-parse', 'HEAD')

                git('init', '-b', 'master')
                base = commit('2026.1.20-dev')
                parent = commit('2026.1.21-dev') if case != 'direct-skip' else base
                target = commit('2026.1.21-dev' if case == 'missing-bump' else '2026.1.22-dev',
                                preserve=case != 'lost-current')
                with patch.dict(os.environ, environment, clear=True):
                    if case == 'valid':
                        self.assertEqual(2, docs_engine_version.validate_history(root, base, target, 'master'))
                        self.assertEqual(1, docs_engine_version.validate_history(root, parent, target, 'master'))
                        with self.assertRaisesRegex(ValueError, 'increment minor'):
                            docs_engine_version.validate_committed_update(root, base, target, 'master')
                    else:
                        diagnostic = 'missing preceding change notes' if case == 'lost-current' else 'increment minor'
                        with self.assertRaisesRegex(ValueError, diagnostic):
                            docs_engine_version.validate_history(root, base, target, 'master')

    def test_missing_migration_disposition_and_later_year_release_patch(self) -> None:
        notes = [f'# Log\n\n## Unreleased\n\n## 2026.1.21.2 - 2027-02-04\n\n### {label}\nNo data conversion needed.\n'
                 for label in ('Migration', 'Миграция')]
        engine_version.validate_changelog_texts('2026.1.21.2', notes)
        with self.assertRaisesRegex(ValueError, 'migration disposition'):
            engine_version.validate_changelog_texts('2026.1.21.2', [notes[0].replace('### Migration', '### Changed'), notes[1]])

    def test_candidate_promotion_is_metadata_only(self) -> None:
        old, new = '2026.1.21.1-rc', '2026.1.21.1'
        inputs = {('show', 'base:VERSION'): old + '\n', ('show', 'target:VERSION'): new + '\n',
                  ('show', '-s', '--format=%cI', 'target'): '2026-10-04T12:00:00+00:00',
                  ('diff', '--name-only', 'base', 'target'): 'VERSION\nDocs/en/reference/changelog.md'}
        for path, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция')):
            previous = f'## {old} - 2026-10-04\n\n### {label}\nNo migration.\n'
            inputs['show', f'base:{path}'] = '# Log\n\n## Unreleased\n\n' + previous
            inputs['show', f'target:{path}'] = f'# Log\n\n## Unreleased\n\n## {new} - 2026-10-04\n\n### {label}\nOnly suffix removed.\n\n' + previous
        with patch.object(docs_engine_version, 'git', side_effect=lambda root, *args, **kwargs: inputs[args]):
            docs_engine_version.validate_committed_update(Path('.'), 'base', 'target', 'release/2026.1')
            inputs['diff', '--name-only', 'base', 'target'] += '\nSource/Changed.cpp'
            with self.assertRaisesRegex(ValueError, 'code changes require a patch'):
                docs_engine_version.validate_committed_update(Path('.'), 'base', 'target', 'release/2026.1')
            inputs['show', 'target:VERSION'] = ' ' + new + '\n'
            with self.assertRaisesRegex(ValueError, 'Invalid Engine VERSION'):
                docs_engine_version.validate_committed_update(Path('.'), 'base', 'target', 'release/2026.1')

    def test_incomplete_range_arguments_fail_closed(self) -> None:
        with patch.object(sys, 'argv', ['docs_engine_version.py', '--target-git-ref', 'HEAD']):
            with self.assertRaises(SystemExit) as exit_result:
                docs_engine_version.main()
        self.assertEqual(exit_result.exception.code, 1)

    def test_repository_changelogs_match_current_version(self) -> None:
        engine_version.validate_changelogs(BUILDTOOLS.parent)


if __name__ == "__main__":
    unittest.main()

