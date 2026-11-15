import datetime
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from engine_version import next_master_version
import generate_master_version


class GenerateMasterVersionTests(unittest.TestCase):
    def test_increments_the_published_minor(self):
        self.assertEqual(next_master_version('2026.1.118-dev', datetime.date(2026, 11, 10)), '2026.1.119-dev')

    def test_new_year_starts_the_first_line(self):
        self.assertEqual(next_master_version('2026.2.118-dev', datetime.date(2027, 1, 1)), '2027.1.0-dev')

    def test_rejects_release_and_future_baselines(self):
        with self.assertRaises(ValueError):
            next_master_version('2026.1.118.0', datetime.date(2026, 11, 10))
        with self.assertRaises(ValueError):
            next_master_version('2027.1.0-dev', datetime.date(2026, 11, 10))

    def test_wrong_merge_does_not_overwrite_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'VERSION'
            path.write_text('unchanged\n', encoding='utf-8')
            with patch.object(sys, 'argv', ['generate_master_version.py', '--root', directory]), \
                    patch.object(generate_master_version, 'git', side_effect=['master', 'published', 'different']):
                with self.assertRaises(SystemExit):
                    generate_master_version.main()
            self.assertEqual(path.read_text(encoding='utf-8'), 'unchanged\n')

    def test_matching_merge_generates_from_published_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'VERSION'
            path.write_text('conflict\n', encoding='utf-8')
            with patch.object(sys, 'argv', ['generate_master_version.py', '--root', directory]), \
                    patch.object(generate_master_version, 'git', side_effect=['master', 'published', 'published', 'unmerged', '2026.1.118-dev']):
                self.assertEqual(generate_master_version.main(), 0)
            expected = next_master_version('2026.1.118-dev', datetime.datetime.now(datetime.timezone.utc).date())
            self.assertEqual(path.read_text(encoding='utf-8'), expected + '\n')

    def test_advanced_published_tip_recomputes_resolved_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'VERSION'
            path.write_text('2026.1.119-dev\n', encoding='utf-8')
            values = ['master', 'published', 'old-tip', '', '', '2026.1.119-dev']
            with patch.object(sys, 'argv', ['generate_master_version.py', '--root', directory, '--advanced-tip']), \
                    patch.object(generate_master_version, 'git', side_effect=values):
                self.assertEqual(generate_master_version.main(), 0)
            expected = next_master_version('2026.1.119-dev', datetime.datetime.now(datetime.timezone.utc).date())
            self.assertEqual(path.read_text(encoding='utf-8'), expected + '\n')

    def test_advanced_tip_rejects_unexpected_existing_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'VERSION'
            path.write_text('2026.1.42-dev\n', encoding='utf-8')
            values = ['master', 'published', 'old-tip', '', '', '2026.1.119-dev']
            with patch.object(sys, 'argv', ['generate_master_version.py', '--root', directory, '--advanced-tip']), \
                    patch.object(generate_master_version, 'git', side_effect=values):
                with self.assertRaises(SystemExit):
                    generate_master_version.main()
            self.assertEqual(path.read_text(encoding='utf-8'), '2026.1.42-dev\n')

    def test_advanced_tip_rejects_unrelated_history(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'VERSION'
            path.write_text('2026.1.119-dev\n', encoding='utf-8')
            values = ['master', 'published', 'old-tip', subprocess.CalledProcessError(1, 'merge-base')]
            with patch.object(sys, 'argv', ['generate_master_version.py', '--root', directory, '--advanced-tip']), \
                    patch.object(generate_master_version, 'git', side_effect=values):
                with self.assertRaises(SystemExit):
                    generate_master_version.main()
            self.assertEqual(path.read_text(encoding='utf-8'), '2026.1.119-dev\n')

    def test_advanced_tip_rejects_unresolved_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'VERSION'
            path.write_text('conflict\n', encoding='utf-8')
            values = ['master', 'published', 'old-tip', '', 'unmerged']
            with patch.object(sys, 'argv', ['generate_master_version.py', '--root', directory, '--advanced-tip']), \
                    patch.object(generate_master_version, 'git', side_effect=values):
                with self.assertRaises(SystemExit):
                    generate_master_version.main()
            self.assertEqual(path.read_text(encoding='utf-8'), 'conflict\n')

    def test_advanced_tip_in_real_diverged_merge_and_second_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args, check=True):
                return subprocess.run(['git', '-C', directory, *args], check=check, capture_output=True, text=True, encoding='utf-8')

            git('init', '-b', 'master')
            git('config', 'user.name', 'Version Test')
            git('config', 'user.email', 'version-test@example.invalid')
            (root / 'VERSION').write_text('2026.1.118-dev\n', encoding='utf-8')
            (root / 'conflict').write_text('base\n', encoding='utf-8')
            git('add', 'VERSION', 'conflict')
            git('commit', '-m', 'base')
            git('checkout', '-b', 'incoming')
            (root / 'conflict').write_text('incoming\n', encoding='utf-8')
            git('commit', '-am', 'incoming')
            old_tip = git('rev-parse', 'HEAD').stdout.strip()
            (root / 'VERSION').write_text('2026.1.119-dev\n', encoding='utf-8')
            git('commit', '-am', 'published advance')
            git('update-ref', 'refs/remotes/origin/master', git('rev-parse', 'HEAD').stdout.strip())
            git('checkout', 'master')
            (root / 'conflict').write_text('local\n', encoding='utf-8')
            git('commit', '-am', 'local')
            self.assertNotEqual(git('merge', '--no-commit', old_tip, check=False).returncode, 0)
            (root / 'VERSION').write_text('2026.1.119-dev\n', encoding='utf-8')

            script = Path(generate_master_version.__file__).resolve()
            args = [sys.executable, str(script), '--root', directory, '--advanced-tip']
            result = subprocess.run(args, capture_output=True, text=True, encoding='utf-8')
            self.assertEqual(result.returncode, 0, result.stderr)
            expected = next_master_version('2026.1.119-dev', datetime.datetime.now(datetime.timezone.utc).date())
            self.assertEqual((root / 'VERSION').read_text(encoding='utf-8'), expected + '\n')
            repeated = subprocess.run(args, capture_output=True, text=True, encoding='utf-8')
            self.assertNotEqual(repeated.returncode, 0)
            self.assertEqual((root / 'VERSION').read_text(encoding='utf-8'), expected + '\n')

    def test_failed_version_replace_preserves_previous_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'VERSION'
            path.write_text('old\n', encoding='utf-8')
            with patch.object(generate_master_version.os, 'replace', side_effect=OSError('simulated failure')):
                with self.assertRaises(OSError):
                    generate_master_version.write_version(root, '2026.1.120-dev')
            self.assertEqual(path.read_text(encoding='utf-8'), 'old\n')
            self.assertEqual([entry.name for entry in root.iterdir()], ['VERSION'])

    def test_failed_version_stream_open_preserves_previous_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'VERSION'
            path.write_text('old\n', encoding='utf-8')
            with patch.object(generate_master_version.os, 'fdopen', side_effect=OSError('simulated failure')):
                with self.assertRaises(OSError):
                    generate_master_version.write_version(root, '2026.1.120-dev')
            self.assertEqual(path.read_text(encoding='utf-8'), 'old\n')
            self.assertEqual([entry.name for entry in root.iterdir()], ['VERSION'])


if __name__ == '__main__':
    unittest.main()
