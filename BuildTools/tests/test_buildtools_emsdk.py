from __future__ import annotations

import io
from pathlib import Path
import shutil
import sys
import tarfile

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


def make_emscripten_tree(root: Path, environment_script: str = 'emsdk_env.sh') -> Path:
	emscripten_root = root / 'emsdk'
	(emscripten_root / 'upstream' / 'emscripten').mkdir(parents=True)
	(emscripten_root / '.emscripten').write_text('portable config\n')
	(emscripten_root / environment_script).write_text('environment\n')
	(emscripten_root / 'upstream' / 'emscripten' / 'emcc.py').write_text('compiler\n')
	return emscripten_root


def make_cached_emscripten_tree(tmp_path: Path) -> Path:
	source = tmp_path / 'source'
	emscripten_root = make_emscripten_tree(source)
	archive_path = tmp_path / 'source.tar.gz'
	with tarfile.open(archive_path, 'w:gz') as archive:
		archive.add(emscripten_root, arcname='emsdk')
	return archive_path


def test_emscripten_workspace_cache_key_separates_host_platforms(monkeypatch: pytest.MonkeyPatch) -> None:
	env = {'FO_EMSCRIPTEN_VERSION': '6.0.8'}
	monkeypatch.setattr(_buildtools.sys, 'platform', 'linux')
	monkeypatch.setattr(_buildtools.platform, 'machine', lambda: 'x86_64')

	assert _buildtools.build_emscripten_workspace_cache_name(env) == 'emscripten-6.0.8-linux-x86_64.tar.gz'

	monkeypatch.setattr(_buildtools.sys, 'platform', 'win32')
	monkeypatch.setattr(_buildtools.platform, 'machine', lambda: 'AMD64')
	assert _buildtools.build_emscripten_workspace_cache_name(env) == 'emscripten-6.0.8-win32-amd64.tar.gz'


def test_emscripten_workspace_cache_hit_skips_clone_and_install(
	tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
) -> None:
	workspace = tmp_path / 'workspace'
	workspace.mkdir()
	archive_source = make_cached_emscripten_tree(tmp_path)

	def fetch(_name: str, target: Path) -> bool:
		shutil.copy2(archive_source, target)
		return True

	monkeypatch.setattr(_buildtools, 'workspace_cache_fetch', fetch)
	monkeypatch.setattr(_buildtools, 'clone_git_repo', lambda *_args, **_kwargs: pytest.fail('unexpected clone'))
	monkeypatch.setattr(_buildtools, 'run_emsdk_command', lambda *_args: pytest.fail('unexpected install'))

	_buildtools.prepare_emscripten_workspace({
		'FO_WORKSPACE': str(workspace),
		'FO_EMSCRIPTEN_VERSION': '6.0.8',
	})

	assert _buildtools.is_emscripten_workspace_complete(workspace / 'emsdk')
	assert not list(workspace.glob('*.tar.gz'))


def test_workspace_cache_restore_promotes_only_the_expected_tree(tmp_path: Path) -> None:
	source = tmp_path / 'source'
	emscripten_root = make_emscripten_tree(source)
	(source / 'unrelated.txt').write_text('must not reach the workspace\n')
	archive_path = tmp_path / 'cache.tar.gz'
	with tarfile.open(archive_path, 'w:gz') as archive:
		archive.add(emscripten_root, arcname='emsdk')
		archive.add(source / 'unrelated.txt', arcname='unrelated.txt')
	workspace = tmp_path / 'workspace'

	assert _buildtools.restore_workspace_cache_tree(
		archive_path, workspace, 'emsdk', 'Emscripten SDK', _buildtools.is_emscripten_workspace_complete,
	)
	assert _buildtools.is_emscripten_workspace_complete(workspace / 'emsdk')
	assert not (workspace / 'unrelated.txt').exists()
	assert not list(workspace.glob('.emsdk-cache-*'))


def test_workspace_cache_restore_rejects_a_path_escape_and_falls_back(tmp_path: Path) -> None:
	archive_path = tmp_path / 'cache.tar.gz'
	with tarfile.open(archive_path, 'w:gz') as archive:
		payload = b'outside\n'
		member = tarfile.TarInfo('../outside.txt')
		member.size = len(payload)
		archive.addfile(member, io.BytesIO(payload))
	workspace = tmp_path / 'workspace'

	assert not _buildtools.restore_workspace_cache_tree(
		archive_path, workspace, 'emsdk', 'Emscripten SDK', _buildtools.is_emscripten_workspace_complete,
	)
	assert not (tmp_path / 'outside.txt').exists()
	assert not archive_path.exists()
	assert not list(workspace.glob('.emsdk-cache-*'))


def test_workspace_tree_is_not_packed_without_a_configured_cache(
	tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
) -> None:
	source = make_emscripten_tree(tmp_path)
	monkeypatch.delenv(_buildtools.WORKSPACE_CACHE_VAR, raising=False)
	monkeypatch.setattr(_buildtools.tarfile, 'open', lambda *_args, **_kwargs: pytest.fail('unexpected pack'))

	_buildtools.workspace_cache_store_tree('emscripten.tar.gz', tmp_path / 'cache.tar.gz', source, 'SDK')


def test_workspace_tree_uses_fast_gzip_for_critical_path_cache_fill(
	tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
) -> None:
	source = make_emscripten_tree(tmp_path)
	archive_path = tmp_path / 'cache.tar.gz'
	calls: list[tuple[str, int | None]] = []
	real_open = tarfile.open

	def capture_open(name: Path, mode: str, **kwargs: int) -> tarfile.TarFile:
		calls.append((mode, kwargs.get('compresslevel')))
		return real_open(name, mode, **kwargs)

	monkeypatch.setattr(_buildtools.tarfile, 'open', capture_open)
	monkeypatch.setattr(_buildtools, 'workspace_cache_store', lambda *_args: None)
	monkeypatch.setenv(_buildtools.WORKSPACE_CACHE_VAR, 'https://ci.example/cache/workspaces')

	_buildtools.workspace_cache_store_tree('emscripten.tar.gz', archive_path, source, 'SDK')

	assert calls == [('w:gz', _buildtools.WORKSPACE_CACHE_GZIP_LEVEL)]
	assert _buildtools.WORKSPACE_CACHE_GZIP_LEVEL == 1
	assert not archive_path.exists()


@pytest.mark.parametrize('cache_payload', [b'not a tar archive', None])
def test_emscripten_workspace_cache_miss_or_corruption_builds_and_stores(
	tmp_path: Path, monkeypatch: pytest.MonkeyPatch, cache_payload: bytes | None,
) -> None:
	workspace = tmp_path / 'workspace'
	workspace.mkdir()
	commands: list[tuple[str, ...]] = []
	stored: list[tuple[str, bool]] = []

	def fetch(_name: str, target: Path) -> bool:
		if cache_payload is None:
			return False
		target.write_bytes(cache_payload)
		return True

	def clone(target: Path, _url: str) -> None:
		target.mkdir(parents=True)

	def emsdk(emscripten_root: Path, *args: str) -> None:
		commands.append(args)
		if args[0] == 'activate':
			make_emscripten_tree(workspace)

	def store(name: str, source: Path) -> None:
		stored.append((name, source.is_file()))

	monkeypatch.setattr(_buildtools, 'workspace_cache_fetch', fetch)
	monkeypatch.setattr(_buildtools, 'workspace_cache_store', store)
	monkeypatch.setattr(_buildtools, 'clone_git_repo', clone)
	monkeypatch.setattr(_buildtools, 'run_emsdk_command', emsdk)
	monkeypatch.setenv(_buildtools.WORKSPACE_CACHE_VAR, 'https://ci.example/cache/workspaces')

	_buildtools.prepare_emscripten_workspace({
		'FO_WORKSPACE': str(workspace),
		'FO_EMSCRIPTEN_VERSION': '6.0.8',
	})

	assert [command[0] for command in commands] == ['list', 'install', 'activate']
	assert stored and stored[0][0].startswith('emscripten-6.0.8-') and stored[0][1]
	assert _buildtools.is_emscripten_workspace_complete(workspace / 'emsdk')
	assert not list(workspace.glob('*.tar.gz'))
