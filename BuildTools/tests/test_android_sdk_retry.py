from __future__ import annotations

from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


def test_run_with_retry_succeeds_after_called_process_error(monkeypatch: pytest.MonkeyPatch) -> None:
	calls = {'n': 0}
	retries: list[int] = []

	def fake_run(cmd: object, cwd: object = None, env: object = None) -> None:
		_ = cmd, cwd, env
		calls['n'] += 1
		if calls['n'] < 3:
			raise subprocess.CalledProcessError(1, ['sdkmanager'])

	monkeypatch.setattr(_buildtools, 'run', fake_run)
	monkeypatch.setattr(_buildtools, 'DOWNLOAD_RETRY_DELAY_SEC', 0)
	monkeypatch.setattr(_buildtools.time, 'sleep', lambda _delay: None)

	_buildtools.run_with_retry(['sdkmanager'], label='Android SDK packages', on_retry=lambda: retries.append(1))

	assert calls['n'] == 3
	assert retries == [1, 1]


def test_run_with_retry_raises_after_exhausting_attempts(monkeypatch: pytest.MonkeyPatch) -> None:
	def fake_run(cmd: object, cwd: object = None, env: object = None) -> None:
		_ = cwd, env
		raise subprocess.CalledProcessError(1, cmd)  # type: ignore[arg-type]

	monkeypatch.setattr(_buildtools, 'run', fake_run)
	monkeypatch.setattr(_buildtools, 'DOWNLOAD_RETRY_COUNT', 2)
	monkeypatch.setattr(_buildtools, 'DOWNLOAD_RETRY_DELAY_SEC', 0)
	monkeypatch.setattr(_buildtools.time, 'sleep', lambda _delay: None)

	with pytest.raises(subprocess.CalledProcessError):
		_buildtools.run_with_retry(['sdkmanager'], label='Android SDK packages')


def test_clone_retries_a_reset_connection_from_a_clean_target(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
	target = tmp_path / 'runtime'
	calls: list[bool] = []

	def fake_run(cmd: object, cwd: object = None, env: object = None) -> None:
		_ = cmd, cwd, env
		calls.append(target.exists())
		if len(calls) == 1:
			(target / '.git').mkdir(parents=True)
			raise subprocess.CalledProcessError(128, ['git', 'clone'])

	monkeypatch.setattr(_buildtools, 'run', fake_run)
	monkeypatch.setattr(_buildtools, 'DOWNLOAD_RETRY_DELAY_SEC', 0)
	monkeypatch.setattr(_buildtools.time, 'sleep', lambda _delay: None)

	_buildtools.clone_git_repo(target, 'https://github.com/dotnet/runtime.git', branch_name='v10.0.11', depth=1)

	assert calls == [False, False]
