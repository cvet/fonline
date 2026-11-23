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

    @unittest.skipUnless(shutil.which("git") and shutil.which("cmake") and shutil.which("ninja"),
                         "Git, CMake and Ninja are required")
    def test_cmake_packed_branch_dependencies_and_next_loose_commit(self) -> None:
        def git(root: Path, *args: str) -> str:
            return subprocess.check_output(
                ["git", "-C", str(root), "-c", "user.name=Version Test",
                 "-c", "user.email=version@example.invalid", *args],
                text=True, stderr=subprocess.STDOUT, timeout=30).strip()

        with tempfile.TemporaryDirectory(prefix="engine-version-cmake-") as directory:
            fixture = Path(directory).resolve()
            repository = fixture / "engine"
            repository.mkdir()
            (repository / "BuildTools").mkdir()
            shutil.copyfile(BUILDTOOLS / "engine_version.py", repository / "BuildTools/engine_version.py")
            (repository / "VERSION").write_text("2026.1.13-dev\n", encoding="utf-8")
            git(repository, "init", "-b", "main")
            git(repository, "add", ".")
            git(repository, "commit", "-m", "Initial fixture")
            git(repository, "pack-refs", "--all", "--prune")
            branch = repository / ".git/refs/heads/main"
            self.assertFalse(branch.exists())
            source = fixture / "project"
            source.mkdir()
            helper = (BUILDTOOLS / "cmake/helpers/EngineVersion.cmake").as_posix()
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.22)\nproject(VersionDependencyFixture NONE)\n'
                f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
                f'include("{helper}")\n'
                f'ReadEngineVersion("{repository.as_posix()}" version dependencies)\n'
                f'execute_process(COMMAND git -C "{repository.as_posix()}" rev-parse HEAD '
                'OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE)\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/tracked.txt" "${dependencies}")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/configured-head.txt" "${revision}")\n'
                'add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/consumed-head.txt"\n'
                ' COMMAND "${CMAKE_COMMAND}" -E copy_if_different '
                '"${CMAKE_BINARY_DIR}/configured-head.txt" "${CMAKE_BINARY_DIR}/consumed-head.txt"\n'
                ' DEPENDS ${dependencies} "${CMAKE_BINARY_DIR}/configured-head.txt")\n'
                'add_custom_target(version_stamp ALL DEPENDS "${CMAKE_BINARY_DIR}/consumed-head.txt")\n',
                encoding="utf-8")
            build = fixture / "build"
            subprocess.run(["cmake", "-G", "Ninja", "-S", str(source), "-B", str(build)],
                           check=True, capture_output=True, text=True, timeout=60)

            def assert_consumed(expected: str) -> None:
                subprocess.run(["cmake", "--build", str(build)], check=True,
                               capture_output=True, text=True, timeout=60)
                self.assertEqual(expected, (build / "consumed-head.txt").read_text(encoding="utf-8"))
                dependencies = (build / "tracked.txt").read_text(encoding="utf-8").split(";")
                self.assertTrue(dependencies)
                self.assertTrue(all(Path(entry).is_file() for entry in dependencies), dependencies)

            baseline = git(repository, "rev-parse", "HEAD")
            assert_consumed(baseline)
            git(repository, "commit", "--allow-empty", "-m", "First loose fixture commit")
            first_loose = git(repository, "rev-parse", "HEAD")
            self.assertNotEqual(baseline, first_loose)
            self.assertTrue(branch.is_file())
            assert_consumed(first_loose)
            dependencies = (build / "tracked.txt").read_text(encoding="utf-8").split(";")
            self.assertIn(branch.as_posix(), dependencies)
            git(repository, "commit", "--allow-empty", "-m", "Next loose fixture commit")
            subsequent = git(repository, "rev-parse", "HEAD")
            self.assertNotEqual(first_loose, subsequent)
            assert_consumed(subsequent)
            dependencies = (build / "tracked.txt").read_text(encoding="utf-8").split(";")
            self.assertIn(branch.as_posix(), dependencies)

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


@unittest.skipUnless(shutil.which('git'), 'Git is required')
class EngineVersionWriterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix='engine-version-writer-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.today = datetime.datetime.now(datetime.timezone.utc).date()
        self.previous = f'{self.today.year}.1.119-dev'
        self.next = f'{self.today.year}.1.120-dev'
        self.git('init', '-b', 'master')
        (self.root / 'VERSION').write_text(self.previous + '\n', encoding='utf-8')
        self.notes(current=False)
        self.git('add', '.')
        self.git('commit', '-m', 'published baseline')
        self.baseline = self.git('rev-parse', 'HEAD')
        self.git('update-ref', 'refs/remotes/origin/master', self.baseline)
        self.notes(current=True)
        (self.root / 'VERSION').write_text(self.previous + '\n', encoding='utf-8')

    def git(self, *args: str) -> str:
        return subprocess.check_output(['git', '-C', str(self.root), '-c', 'user.name=Version Test',
                                        '-c', 'user.email=version@example.invalid', *args],
                                       text=True, stderr=subprocess.STDOUT).strip()

    def notes(self, *, current: bool) -> None:
        for relative, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция'), strict=True):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            previous = f'## {self.previous if current else "Current"} - {self.today}\n\n### {label}\nPrevious.\n'
            candidate = f'## Current - {self.today}\n\n### {label}\nMerged.\n\n' if current else ''
            path.write_text('# Log\n\n## Unreleased\n\n' + candidate + previous, encoding='utf-8')

    def run_writer(self, baseline: str, *extra: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run([sys.executable, '-X', 'utf8', '-B', str(BUILDTOOLS / 'docs_engine_version.py'),
                               '--root', str(self.root), '--write-next', '--branch', 'master',
                               '--baseline-git-ref', baseline, *extra], capture_output=True, text=True, encoding='utf-8')

    def test_write_next_uses_exact_fetched_master_and_preserves_notes(self) -> None:
        before = [(self.root / path).read_bytes() for path in engine_version.CHANGELOG_PATHS]
        result = self.run_writer(self.baseline)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / 'VERSION').read_text(encoding='utf-8'), self.next + '\n')
        self.assertEqual(before, [(self.root / path).read_bytes() for path in engine_version.CHANGELOG_PATHS])
        engine_version.validate_changelogs(self.root)

    def test_writer_rejects_invalid_authority_or_notes_without_changing_version(self) -> None:
        version = self.root / 'VERSION'
        for scenario in ('short', 'stale', 'other_tip', 'missing', 'missing_migration', 'lost_history', 'bad_date', 'both_bad_date', 'target', 'history', 'check', 'foreign_version', 'fake_conflict', 'alter_history'):
            with self.subTest(scenario=scenario):
                self.notes(current=True)
                baseline = self.baseline
                extra = ()
                if scenario == 'short':
                    baseline = self.baseline[:8]
                elif scenario == 'stale':
                    self.git('update-ref', '-d', 'refs/remotes/origin/master')
                elif scenario == 'other_tip':
                    self.git('commit', '--allow-empty', '-m', 'later fetched tip')
                    self.git('update-ref', 'refs/remotes/origin/master', self.git('rev-parse', 'HEAD'))
                elif scenario == 'missing':
                    baseline = 'f' * 40
                elif scenario in ('missing_migration', 'lost_history', 'bad_date', 'both_bad_date'):
                    path = self.root / engine_version.CHANGELOG_PATHS[1]
                    text = path.read_text(encoding='utf-8')
                    if scenario == 'missing_migration':
                        text = text.replace('### Миграция', '### Изменено', 1)
                    elif scenario == 'lost_history':
                        text = text.split(f'## {self.previous}', 1)[0]
                    else:
                        text = text.replace(str(self.today), str(self.today - datetime.timedelta(days=1)), 1)
                    path.write_text(text, encoding='utf-8')
                    if scenario == 'both_bad_date':
                        path = self.root / engine_version.CHANGELOG_PATHS[0]
                        path.write_text(path.read_text(encoding='utf-8').replace(str(self.today), str(self.today - datetime.timedelta(days=1)), 1), encoding='utf-8')
                elif scenario == 'target':
                    extra = ('--target-git-ref', self.baseline)
                elif scenario == 'check':
                    extra = ('--check',)
                elif scenario == 'foreign_version':
                    version.write_text(f'{self.today.year}.1.9999-dev\n', encoding='utf-8')
                elif scenario == 'fake_conflict':
                    version.write_text(f'<<<<<<< HEAD\n{self.previous}\n=======\n{self.previous}\n>>>>>>> origin/master\n', encoding='utf-8')
                elif scenario == 'alter_history':
                    for relative in engine_version.CHANGELOG_PATHS:
                        path = self.root / relative
                        path.write_text(path.read_text(encoding='utf-8').replace('Previous.', 'Altered.'), encoding='utf-8')
                else:
                    extra = ('--history',)
                before = version.read_bytes()
                result = self.run_writer(baseline, *extra)
                self.assertNotEqual(result.returncode, 0, scenario)
                self.assertEqual(version.read_bytes(), before, scenario)
                self.git('update-ref', 'refs/remotes/origin/master', self.baseline)
                version.write_text(self.previous + '\n', encoding='utf-8')

    def test_writer_accepts_only_exact_real_two_parent_version_conflict(self) -> None:
        self.notes(current=False)
        self.git('checkout', '-b', 'local-feature')
        local = f'{self.today.year}.1.101-dev'
        (self.root / 'VERSION').write_text(local + '\n', encoding='utf-8')
        self.git('add', 'VERSION')
        self.git('commit', '-m', 'local published version')
        self.git('checkout', 'master')
        self.previous = f'{self.today.year}.1.120-dev'
        (self.root / 'VERSION').write_text(self.previous + '\n', encoding='utf-8')
        self.git('add', 'VERSION')
        self.git('commit', '-m', 'fetched master version')
        fetched = self.git('rev-parse', 'HEAD')
        self.git('update-ref', 'refs/remotes/origin/master', fetched)
        self.git('checkout', 'local-feature')
        merge = subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Version Test',
                                '-c', 'user.email=version@example.invalid', 'merge', '--no-commit',
                                '--no-ff', 'origin/master'], capture_output=True, text=True)
        self.assertNotEqual(merge.returncode, 0)
        self.assertIn('<<<<<<< HEAD', (self.root / 'VERSION').read_text(encoding='utf-8'))
        self.notes(current=True)
        result = self.run_writer(fetched)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / 'VERSION').read_text(encoding='utf-8'), f'{self.today.year}.1.121-dev\n')

    def test_writer_rejects_reordered_historical_release_blocks(self) -> None:
        self.notes(current=False)
        older = f'{self.today.year}.1.118-dev'
        for relative, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция'), strict=True):
            path = self.root / relative
            with path.open('a', encoding='utf-8', newline='\n') as output:
                output.write(f'\n## {older} - {self.today}\n\n### {label}\nOlder.\n')
        self.git('add', '.')
        self.git('commit', '-m', 'baseline with previous records')
        baseline = self.git('rev-parse', 'HEAD')
        self.git('update-ref', 'refs/remotes/origin/master', baseline)
        self.notes(current=True)
        for relative, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция'), strict=True):
            path = self.root / relative
            text = path.read_text(encoding='utf-8')
            current, previous = text.split(f'## {self.previous}', 1)
            path.write_text(current + f'## {older} - {self.today}\n\n### {label}\nOlder.\n\n' +
                            f'## {self.previous}' + previous, encoding='utf-8')
        version = self.root / 'VERSION'
        before = version.read_bytes()
        result = self.run_writer(baseline)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(version.read_bytes(), before)

    def test_writer_rejects_notes_changed_during_preparation(self) -> None:
        real_git = docs_engine_version.git
        checks = 0
        def mutate_notes(root: Path, *args: str, **kwargs: object) -> str:
            nonlocal checks
            result = real_git(root, *args, **kwargs)
            if args == ('rev-parse', '--verify', 'refs/remotes/origin/master^{commit}'):
                checks += 1
                if checks == 2:
                    path = root / engine_version.CHANGELOG_PATHS[0]
                    path.write_text('changed while preparing\n', encoding='utf-8')
            return result
        before = (self.root / 'VERSION').read_bytes()
        with patch.object(docs_engine_version, 'git', side_effect=mutate_notes):
            with self.assertRaisesRegex(ValueError, 'change notes changed'):
                docs_engine_version.write_next_master_version(self.root, self.baseline, self.today)
        self.assertEqual((self.root / 'VERSION').read_bytes(), before)
        self.assertEqual(list(self.root.glob('.engine-version-*')), [])

    def test_writer_preserves_version_and_cleans_temporary_file_when_replace_fails(self) -> None:
        before = (self.root / 'VERSION').read_bytes()
        with patch.object(docs_engine_version.os, 'replace', side_effect=OSError('replace failed')):
            with self.assertRaisesRegex(OSError, 'replace failed'):
                docs_engine_version.write_next_master_version(self.root, self.baseline, self.today)
        self.assertEqual((self.root / 'VERSION').read_bytes(), before)
        self.assertEqual(list(self.root.glob('.engine-version-*')), [])

    def test_writer_does_not_overwrite_concurrent_version_change(self) -> None:
        real_git = docs_engine_version.git
        checks = 0
        def change_version(root: Path, *args: str, **kwargs: object) -> str:
            nonlocal checks
            result = real_git(root, *args, **kwargs)
            if args == ('rev-parse', '--verify', 'refs/remotes/origin/master^{commit}'):
                checks += 1
                if checks == 2:
                    (root / 'VERSION').write_text('concurrent change\n', encoding='utf-8')
            return result
        with patch.object(docs_engine_version, 'git', side_effect=change_version):
            with self.assertRaisesRegex(ValueError, 'VERSION changed'):
                docs_engine_version.write_next_master_version(self.root, self.baseline, self.today)
        self.assertEqual((self.root / 'VERSION').read_text(encoding='utf-8'), 'concurrent change\n')
        self.assertEqual(list(self.root.glob('.engine-version-*')), [])

    def test_writer_applies_original_version_mode_to_replacement(self) -> None:
        version = self.root / 'VERSION'
        expected_mode = version.stat().st_mode & 0o7777
        with patch.object(docs_engine_version.os, 'chmod', wraps=os.chmod) as chmod:
            docs_engine_version.write_next_master_version(self.root, self.baseline, self.today)
        chmod.assert_called_once()
        self.assertEqual(chmod.call_args.args[1], expected_mode)
        self.assertEqual(version.stat().st_mode & 0o7777, expected_mode)

    @unittest.skipIf(os.name == 'nt', 'POSIX file mode semantics are not available on Windows')
    def test_writer_preserves_readable_posix_version_mode(self) -> None:
        version = self.root / 'VERSION'
        version.chmod(0o644)
        docs_engine_version.write_next_master_version(self.root, self.baseline, self.today)
        self.assertEqual(version.stat().st_mode & 0o7777, 0o644)

    def test_writer_accepts_initial_policy_adoption(self) -> None:
        (self.root / 'VERSION').write_text('2022.1.0.wip\n', encoding='utf-8')
        self.git('add', 'VERSION')
        self.git('commit', '-m', 'historical version')
        historical = self.git('rev-parse', 'HEAD')
        self.git('update-ref', 'refs/remotes/origin/master', historical)
        for relative, label in zip(engine_version.CHANGELOG_PATHS, ('Migration', 'Миграция'), strict=True):
            (self.root / relative).write_text(f'# Log\n\n## Unreleased\n\n## Current - {self.today}\n\n### {label}\nInitial policy.\n', encoding='utf-8')
        result = self.run_writer(historical)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / 'VERSION').read_text(encoding='utf-8'), f'{self.today.year}.1.1-dev\n')


if __name__ == "__main__":
    unittest.main()

