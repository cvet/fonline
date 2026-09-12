from __future__ import annotations

from pathlib import Path
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


def capture_emsdk_command(monkeypatch: pytest.MonkeyPatch, os_name: str) -> tuple[list[object], Path]:
	calls: list[tuple[list[object], Path]] = []
	emscripten_root = Path('/workspace/emsdk')

	def capture(command: list[object], cwd: Path) -> None:
		calls.append((command, cwd))

	with monkeypatch.context() as context:
		context.setattr(_buildtools.os, 'name', os_name)
		context.setattr(_buildtools, 'run', capture)
		_buildtools.run_emsdk_command(emscripten_root, 'install', '6.0.8')
	assert len(calls) == 1
	return calls[0]


def test_windows_emsdk_uses_the_current_python(monkeypatch: pytest.MonkeyPatch) -> None:
	monkeypatch.setattr(_buildtools.sys, 'executable', 'C:/Python313/python.exe')

	command, cwd = capture_emsdk_command(monkeypatch, 'nt')

	assert command == ['C:/Python313/python.exe', Path('/workspace/emsdk/emsdk.py'), 'install', '6.0.8']
	assert cwd == Path('/workspace/emsdk')


def test_posix_emsdk_keeps_the_native_wrapper(monkeypatch: pytest.MonkeyPatch) -> None:
	command, cwd = capture_emsdk_command(monkeypatch, 'posix')

	assert command == [Path('/workspace/emsdk/emsdk'), 'install', '6.0.8']
	assert cwd == Path('/workspace/emsdk')
