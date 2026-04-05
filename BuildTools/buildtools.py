#!/usr/bin/python3

from __future__ import annotations

import argparse
import os
import re
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Callable, Iterable, Mapping, NotRequired, Sequence, TypedDict


EnvMap = dict[str, str]
FlagMap = dict[str, int]


class ValidationTarget(TypedDict):
	platform: str
	target: str
	config: str
	compiler: NotRequired[str]
	run_target: NotRequired[str]


def make_flag_map(*enabled_flag_names: str) -> FlagMap:
	return {flag_name: 1 for flag_name in enabled_flag_names}


def make_validation_target(
	platform_name: str,
	target_name: str,
	config_name: str,
	compiler_name: str | None = None,
	run_target_name: str | None = None,
) -> ValidationTarget:
	validation_target: ValidationTarget = {
		'platform': platform_name,
		'target': target_name,
		'config': config_name,
	}
	if compiler_name is not None:
		validation_target['compiler'] = compiler_name
	if run_target_name is not None:
		validation_target['run_target'] = run_target_name
	return validation_target


def make_validation_target_set(
	name_prefix: str,
	platform_name: str,
	target_names: Sequence[str],
	config_name: str = 'Debug',
	compiler_name: str | None = None,
) -> dict[str, ValidationTarget]:
	return {
		f'{name_prefix}-{target_name}': make_validation_target(
			platform_name,
			target_name,
			config_name,
			compiler_name=compiler_name,
		)
		for target_name in target_names
	}


FLAG_NAMES = [
	'FO_BUILD_CLIENT',
	'FO_BUILD_SERVER',
	'FO_BUILD_EDITOR',
	'FO_BUILD_MAPPER',
	'FO_BUILD_ASCOMPILER',
	'FO_BUILD_BAKER',
	'FO_UNIT_TESTS',
	'FO_CODE_COVERAGE',
]

COMMON_VALIDATION_TARGET_NAMES = ('client', 'server', 'editor', 'mapper', 'ascompiler', 'baker')
WIN64_CLANG_VALIDATION_TARGET_NAMES = ('client', 'server', 'ascompiler', 'baker')

SINGLE_FLAG_BUILD_TARGETS = {
	'client': 'FO_BUILD_CLIENT',
	'server': 'FO_BUILD_SERVER',
	'editor': 'FO_BUILD_EDITOR',
	'mapper': 'FO_BUILD_MAPPER',
	'ascompiler': 'FO_BUILD_ASCOMPILER',
	'baker': 'FO_BUILD_BAKER',
	'unit-tests': 'FO_UNIT_TESTS',
	'code-coverage': 'FO_CODE_COVERAGE',
}

SINGLE_CLIENT_VALIDATION_PLATFORMS = {
	'android-arm32': 'android-arm32',
	'android-arm64': 'android-arm64',
	'android-x86': 'android-x86',
	'web': 'web',
	'mac': 'mac',
	'ios': 'ios',
	'win32': 'win32',
}

BUILD_TARGETS: dict[str, FlagMap] = {
	**{target_name: make_flag_map(flag_name) for target_name, flag_name in SINGLE_FLAG_BUILD_TARGETS.items()},
	'toolset': make_flag_map('FO_BUILD_ASCOMPILER', 'FO_BUILD_BAKER'),
	'full': make_flag_map(
		'FO_BUILD_CLIENT',
		'FO_BUILD_SERVER',
		'FO_BUILD_EDITOR',
		'FO_BUILD_MAPPER',
		'FO_BUILD_ASCOMPILER',
		'FO_BUILD_BAKER',
	),
}

VALIDATION_TARGETS: dict[str, ValidationTarget] = {
	**make_validation_target_set('linux', 'linux', COMMON_VALIDATION_TARGET_NAMES),
	**make_validation_target_set('linux-gcc', 'linux', COMMON_VALIDATION_TARGET_NAMES, compiler_name='gcc'),
	**{
		f'{name_prefix}-client': make_validation_target(platform_name, 'client', 'Debug')
		for name_prefix, platform_name in SINGLE_CLIENT_VALIDATION_PLATFORMS.items()
	},
	**make_validation_target_set('win64', 'win64', COMMON_VALIDATION_TARGET_NAMES),
	**make_validation_target_set('win64-clang', 'win64-clang', WIN64_CLANG_VALIDATION_TARGET_NAMES),
	'unit-tests': make_validation_target('linux', 'unit-tests', 'Debug', run_target_name='RunUnitTests'),
	'code-coverage': make_validation_target('linux', 'code-coverage', 'Debug', compiler_name='gcc', run_target_name='RunCodeCoverage'),
}

ANDROID_PLATFORMS = ('android-arm32', 'android-arm64', 'android-x86')
APPLE_PLATFORMS = ('mac', 'ios')

ANDROID_ABI_BY_PLATFORM = {
	'android-arm32': 'armeabi-v7a',
	'android-arm64': 'arm64-v8a',
	'android-x86': 'x86',
}

WINDOWS_BUILD_BY_PLATFORM = {
	'win32': ('Win32', None),
	'win64': ('x64', None),
	'win32-clang': ('Win32', 'ClangCL'),
	'win64-clang': ('x64', 'ClangCL'),
}

FORMAT_PATTERNS = [
	'**/*.cpp',
	'**/*.h',
	'**/*.fos',
]
UTF8_BOM = b'\xef\xbb\xbf'
CLANG_FORMAT_VERSION_RE = re.compile(r'clang-format version (\d+)(?:\.|\b)')

LINUX_PACKAGE_GROUPS = {
	'common-packages': (
		'8',
		['clang', 'clang-format-20', 'build-essential', 'git', 'cmake', 'python3', 'wget', 'unzip', 'binutils-dev'],
	),
	'linux-packages': (
		'6',
		['libc++-dev', 'libc++abi-dev', 'libx11-dev', 'libxcursor-dev', 'libxrandr-dev', 'libxss-dev', 'libjack-dev', 'libpulse-dev', 'libasound-dev', 'freeglut3-dev', 'libssl-dev', 'libevent-dev', 'libxi-dev', 'libzstd-dev'],
	),
	'web-packages': (
		'2',
		['nodejs', 'default-jre'],
	),
	'android-packages': (
		'3',
		['openjdk-17-jdk'],
	),
}

ANDROID_REQUIRED_SDK_PACKAGES = (
	'platform-tools',
	'build-tools;34.0.0',
	'platforms;android-35',
)


def log(*parts: object) -> None:
	print('[BuildTools]', *parts, flush=True)


class TerminalProgress:
	def __init__(self, prefix: str) -> None:
		self._prefix = prefix
		self._last_len = 0

	def update(self, message: str) -> None:
		line = f'[{self._prefix}] {message}'
		padding = ' ' * max(0, self._last_len - len(line))
		print('\r' + line + padding, end='', flush=True)
		self._last_len = len(line)

	def clear(self) -> None:
		if self._last_len == 0:
			return

		print('\r' + ' ' * self._last_len + '\r', end='', flush=True)
		self._last_len = 0

	def finish(self, message: str) -> None:
		self.clear()
		print(f'[{self._prefix}] {message}', flush=True)


def read_first_line(path: Path) -> str:
	if not path.is_file():
		return ''
	with path.open('r', encoding='utf-8-sig') as file:
		return file.readline().strip()


def resolve_buildtools_path(env: Mapping[str, str], *parts: str) -> Path:
	return Path(env['FO_ENGINE_ROOT']) / 'BuildTools' / Path(*parts)


def resolve_buildtools_cmake_path(env: Mapping[str, str], file_name: str) -> Path:
	return resolve_buildtools_path(env, 'cmake', file_name)


def resolve_validation_project_source(env: Mapping[str, str]) -> Path:
	return resolve_buildtools_path(env, 'validation-project')


def resolve_workspace_path(env: Mapping[str, str], *parts: str) -> Path:
	return Path(env['FO_WORKSPACE']) / Path(*parts)


def resolve_toolset_build_dir(env: Mapping[str, str]) -> Path:
	build_dir_name = 'build-win64-toolset' if os.name == 'nt' else 'build-linux-toolset'
	return resolve_workspace_path(env, build_dir_name)


def resolve_web_debug_root(env: Mapping[str, str]) -> Path:
	return resolve_workspace_path(env, 'web-debug')


def resolve_web_package_dir(env: Mapping[str, str], devname: str, config: str) -> Path:
	return resolve_web_debug_root(env) / f'{devname}-Client-{config}-Web'


def resolve_configure_build_dir(env: Mapping[str, str], platform_name: str, target_name: str, config: str) -> Path:
	return resolve_workspace_path(env, f'build-{platform_name}-{target_name}-{config}')


def resolve_validation_project_workspace(env: Mapping[str, str]) -> Path:
	return resolve_workspace_path(env, 'validation-project')


def resolve_validation_build_dir(env: Mapping[str, str], name: str) -> Path:
	return resolve_workspace_path(env, f'validate-{name}')


def resolve_workspace_emsdk_root(workspace: Path) -> str:
	emsdk_root = workspace / 'emsdk'
	return str(emsdk_root) if emsdk_root.is_dir() else ''


def resolve_workspace_android_sdk_root(workspace: Path) -> str:
	android_sdk_root = workspace / 'android-sdk'
	return str(android_sdk_root) if android_sdk_root.is_dir() else ''


def resolve_android_sdk_host_tag() -> str:
	if os.name == 'nt':
		return 'win'
	if sys.platform == 'darwin':
		return 'mac'
	return 'linux'


def resolve_emscripten_toolchain(env: Mapping[str, str]) -> Path:
	return Path(env['EMSDK']) / 'upstream' / 'emscripten' / 'cmake' / 'Modules' / 'Platform' / 'Emscripten.cmake'


def resolve_apple_cmake() -> str:
	return shutil.which('cmake') or '/Applications/CMake.app/Contents/bin/cmake'


def make_cmake_build_cmd(config: str, target_name: str | None = None, cmake_bin: str = 'cmake') -> list[str]:
	command = [cmake_bin, '--build', '.', '--config', config]
	if target_name is not None:
		command.extend(['--target', target_name])
	command.append('--parallel')
	return command


def run_cmake_build(build_dir: Path, config: str, env: Mapping[str, str] | None = None) -> None:
	run(make_cmake_build_cmd(config), cwd=build_dir, env=env)


def run_cmake_target(build_dir: Path, config: str, target_name: str, env: Mapping[str, str] | None = None) -> None:
	run(make_cmake_build_cmd(config, target_name=target_name), cwd=build_dir, env=env)


def run_emsdk_cmake_build(workspace: Path, build_dir: Path, config: str) -> None:
	run_in_emsdk_env(make_cmake_build_cmd(config), workspace, cwd=build_dir)


def stringify_args(*args: object) -> list[str]:
	return [str(arg) for arg in args]


def to_cmake_path(path: str | Path) -> str:
	return str(path).replace('\\', '/')


def make_unix_makefiles_configure_cmd(*args: object) -> list[str]:
	return ['cmake', '-G', 'Unix Makefiles', *stringify_args(*args)]


def make_emsdk_configure_cmd(toolchain_file: Path, *args: object) -> list[str]:
	configure_cmd = ['cmake', '-DCMAKE_TOOLCHAIN_FILE=' + to_cmake_path(toolchain_file), *stringify_args(*args)]
	if os.name == 'nt':
		configure_cmd[1:1] = ['-G', 'Ninja Multi-Config', f'-DCMAKE_MAKE_PROGRAM={discover_windows_ninja()}']
	else:
		configure_cmd[1:1] = ['-G', 'Unix Makefiles']
	return configure_cmd


def make_android_configure_cmd(toolchain_file: str, toolchain_settings: Sequence[str], *args: object) -> list[str]:
	return make_unix_makefiles_configure_cmd(f'-DCMAKE_TOOLCHAIN_FILE={to_cmake_path(toolchain_file)}', *toolchain_settings, *args)


def make_xcode_configure_cmd(cmake_bin: str, *args: object) -> list[str]:
	return [cmake_bin, '-G', 'Xcode', *stringify_args(*args)]


def make_windows_configure_cmd(platform_name: str, *args: object) -> list[str]:
	arch, toolset = resolve_windows_build(platform_name)
	configure_cmd = ['cmake', '-A', arch]
	if toolset:
		configure_cmd += ['-T', toolset]
	configure_cmd += stringify_args(*args)
	return configure_cmd


def resolve_env() -> EnvMap:
	script_dir = Path(__file__).resolve().parent
	project_root = Path(os.environ.get('FO_PROJECT_ROOT') or Path.cwd()).resolve()
	engine_root = Path(os.environ.get('FO_ENGINE_ROOT') or (script_dir / '..')).resolve()
	workspace = Path(os.environ.get('FO_WORKSPACE') or (Path.cwd() / 'Workspace')).resolve()
	output = Path(os.environ.get('FO_OUTPUT') or (workspace / 'output')).resolve()
	third_party = engine_root / 'ThirdParty'

	env: EnvMap = {
		'FO_PROJECT_ROOT': str(project_root),
		'FO_ENGINE_ROOT': str(engine_root),
		'FO_WORKSPACE': str(workspace),
		'FO_OUTPUT': str(output),
		'EMSDK': resolve_workspace_emsdk_root(workspace),
		'EMSCRIPTEN_VERSION': read_first_line(third_party / 'emscripten'),
		'ANDROID_NDK_VERSION': read_first_line(third_party / 'android-ndk'),
		'ANDROID_SDK_VERSION': read_first_line(third_party / 'android-sdk'),
		'ANDROID_NATIVE_API_LEVEL_NUMBER': read_first_line(third_party / 'android-api'),
		'FO_DOTNET_RUNTIME': read_first_line(third_party / 'dotnet-runtime'),
		'FO_IOS_SDK': read_first_line(third_party / 'iOS-sdk'),
	}

	workspace_android_sdk = resolve_workspace_android_sdk_root(workspace)
	if workspace_android_sdk:
		env['ANDROID_HOME'] = workspace_android_sdk
		env['ANDROID_SDK_ROOT'] = workspace_android_sdk
	elif 'ANDROID_HOME' not in os.environ and Path('/usr/lib/android-sdk').is_dir():
		env['ANDROID_HOME'] = '/usr/lib/android-sdk'
		env['ANDROID_SDK_ROOT'] = '/usr/lib/android-sdk'
	else:
		env['ANDROID_HOME'] = os.environ.get('ANDROID_HOME', '')
		env['ANDROID_SDK_ROOT'] = os.environ.get('ANDROID_SDK_ROOT', '')

	ndk_root = workspace / 'android-ndk'
	dotnet_runtime_root = workspace / 'dotnet' / 'runtime'
	env['ANDROID_NDK_ROOT'] = str(ndk_root) if ndk_root.is_dir() else os.environ.get('ANDROID_NDK_ROOT', '')
	env['FO_DOTNET_RUNTIME_ROOT'] = os.environ.get('FO_DOTNET_RUNTIME_ROOT') or (str(dotnet_runtime_root) if dotnet_runtime_root.is_dir() else '')
	return env


def discover_fomain(project_root: Path) -> Path:
	"""Find the single *.fomain file in the project root."""
	candidates = list(project_root.glob('*.fomain'))
	assert len(candidates) == 1, f'Expected exactly one .fomain file in {project_root}, found {len(candidates)}'
	return candidates[0]


def apply_env(env: Mapping[str, str]) -> None:
	for key, value in env.items():
		if value:
			os.environ[key] = value


def print_env_summary(env: Mapping[str, str]) -> None:
	log('Setup environment')
	for key in [
		'FO_PROJECT_ROOT',
		'FO_ENGINE_ROOT',
		'FO_WORKSPACE',
		'FO_OUTPUT',
		'EMSDK',
		'EMSCRIPTEN_VERSION',
		'ANDROID_HOME',
		'ANDROID_SDK_ROOT',
		'ANDROID_NDK_VERSION',
		'ANDROID_SDK_VERSION',
		'ANDROID_NATIVE_API_LEVEL_NUMBER',
		'ANDROID_NDK_ROOT',
		'FO_DOTNET_RUNTIME',
		'FO_DOTNET_RUNTIME_ROOT',
		'FO_IOS_SDK',
	]:
		log('-', f'{key}={env.get(key, "")}')


def emit_shell_env(env: Mapping[str, str], shell_name: str) -> None:
	for key, value in env.items():
		value = value.replace('\n', ' ')
		if shell_name == 'bash':
			escaped = value.replace('\\', '\\\\').replace('"', '\\"')
			print(f'export {key}="{escaped}"')
		elif shell_name == 'cmd':
			escaped = value.replace('"', '""')
			print(f'set "{key}={escaped}"')
		else:
			print(f'{key}={value}')


def resolve_macos_cmake() -> str:
	path = shutil.which('cmake')
	if path:
		return path

	bundle_path = Path('/Applications/CMake.app/Contents/bin/cmake')
	return str(bundle_path) if bundle_path.is_file() else ''


def resolve_vswhere() -> str:
	roots = [
		Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')),
		Path(os.environ.get('ProgramFiles', r'C:\Program Files')),
	]
	for root in roots:
		candidate = root / 'Microsoft Visual Studio' / 'Installer' / 'vswhere.exe'
		if candidate.is_file():
			return str(candidate)
	return ''


def has_windows_build_tools() -> bool:
	vswhere = resolve_vswhere()
	if not vswhere:
		return False
	try:
		output = run_capture_text([
			vswhere,
			'-latest',
			'-products',
			'*',
			'-requires',
			'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
			'-property',
			'installationPath',
		])
		return bool(output)
	except Exception:
		return False


def check_linux_host_tools() -> list[str]:
	issues: list[str] = []
	if not shutil.which('apt-get'):
		issues.append('Please use a Debian/Ubuntu host with apt-get available')
	if not is_running_as_root() and not shutil.which('sudo'):
		issues.append('Please install sudo or run the preparation script as root')
	if not shutil.which('cmake'):
		issues.append('Please install CMake')
	return issues


def check_macos_host_tools() -> list[str]:
	issues: list[str] = []
	try:
		run_capture_text(['xcode-select', '-p'])
	except Exception:
		issues.append('Please install Xcode from AppStore')

	if not resolve_macos_cmake():
		issues.append('Please install CMake from https://cmake.org')
	return issues


def check_windows_host_tools() -> list[str]:
	issues: list[str] = []
	if not has_windows_build_tools():
		issues.append('Please install Visual Studio with C++ Build Tools or Build Tools for Visual Studio')
	if not shutil.which('cmake'):
		issues.append('Please install CMake from https://cmake.org')
	if not (shutil.which('py') or shutil.which('python')):
		issues.append('Please install Python from https://www.python.org')
	return issues


HOST_TOOL_CHECKERS: dict[str, Callable[[], list[str]]] = {
	'linux': check_linux_host_tools,
	'macos': check_macos_host_tools,
	'windows': check_windows_host_tools,
}


def check_host_tools(host_name: str) -> list[str]:
	host_checker = HOST_TOOL_CHECKERS.get(host_name)
	if host_checker is None:
		raise SystemExit(f'Unsupported host tools check: {host_name}')
	return host_checker()


def build_flag_args(target_name: str, config: str | None = None) -> list[str]:
	if target_name not in BUILD_TARGETS:
		raise SystemExit(f'Unknown build target: {target_name}')
	flags = {name: 0 for name in FLAG_NAMES}
	flags.update(BUILD_TARGETS[target_name])
	args = [f'-D{name}={value}' for name, value in flags.items()]
	if config:
		args.append(f'-DCMAKE_BUILD_TYPE={config}')
	return args


def ensure_dir(path: str | Path) -> None:
	Path(path).mkdir(parents=True, exist_ok=True)


def is_running_as_root() -> bool:
	geteuid = getattr(os, 'geteuid', None)
	return bool(geteuid and geteuid() == 0)


def remove_tree(path: str | Path) -> None:
	def handle_remove_readonly(func: object, target: str, excinfo: object) -> None:
		_ = func, excinfo
		os.chmod(target, stat.S_IWRITE)
		os.unlink(target)

	shutil.rmtree(path, onexc=handle_remove_readonly)


def remove_path_if_exists(path: Path) -> None:
	if path.is_dir() and not path.is_symlink():
		remove_tree(path)
	elif path.exists() or path.is_symlink():
		path.unlink()


def ensure_empty_dir(path: Path) -> None:
	remove_path_if_exists(path)
	ensure_dir(path)


def copy_directory(source_path: str | Path, target_path: str | Path, dirs_exist_ok: bool = False) -> None:
	shutil.copytree(source_path, target_path, dirs_exist_ok=dirs_exist_ok)


def download_file(url: str, target_path: Path, label: str) -> None:
	log(f'Download {label}:', url)
	urllib.request.urlretrieve(url, target_path)


def clone_git_repo(target_path: Path, repo_url: str, branch_name: str | None = None, depth: int | None = None) -> None:
	command: list[object] = ['git', 'clone', repo_url]
	if depth is not None:
		command.extend(['--depth', str(depth)])
	if branch_name is not None:
		command.extend(['--branch', branch_name])
	command.append(str(target_path))
	run(command)


def resolve_runtime_build_script() -> str:
	return 'build.cmd' if os.name == 'nt' else './build.sh'


def extract_zip_with_permissions(archive_path: Path, output_dir: Path) -> None:
	with zipfile.ZipFile(archive_path) as archive:
		for member in archive.infolist():
			mode = member.external_attr >> 16
			extracted_path = output_dir / member.filename

			if member.is_dir():
				extracted_path.mkdir(parents=True, exist_ok=True)
				if mode:
					extracted_path.chmod(mode)
				continue

			extracted_path.parent.mkdir(parents=True, exist_ok=True)

			if stat.S_ISLNK(mode) and hasattr(os, 'symlink'):
				if extracted_path.exists() or extracted_path.is_symlink():
					extracted_path.unlink()
				link_target = archive.read(member).decode('utf-8')
				os.symlink(link_target, extracted_path)
				continue

			with archive.open(member) as source, extracted_path.open('wb') as destination:
				shutil.copyfileobj(source, destination)

			if mode:
				extracted_path.chmod(mode)


def run(cmd: Sequence[object], cwd: str | Path | None = None, env: Mapping[str, str] | None = None) -> None:
	log('Run:', ' '.join(str(part) for part in cmd))
	subprocess.check_call([str(part) for part in cmd], cwd=cwd, env=dict(env) if env is not None else None)


def run_capture_text(
	cmd: Sequence[object],
	cwd: str | Path | None = None,
	env: Mapping[str, str] | None = None,
	*,
	log_command: bool = True,
) -> str:
	if log_command:
		log('Run:', ' '.join(str(part) for part in cmd))
	return subprocess.check_output(
		[str(part) for part in cmd],
		cwd=cwd,
		env=dict(env) if env is not None else None,
		text=True,
		encoding='utf-8',
		stderr=subprocess.STDOUT,
	).strip()


def run_bash(command: str, cwd: str | Path | None = None, env: Mapping[str, str] | None = None) -> None:
	log('Run:', command)
	subprocess.check_call(['bash', '-lc', command], cwd=cwd, env=dict(env) if env is not None else None)


def resolve_windows_build(platform_name: str) -> tuple[str, str | None]:
	build = WINDOWS_BUILD_BY_PLATFORM.get(platform_name)
	if build is None:
		raise SystemExit(f'Invalid Windows build platform: {platform_name}')
	return build


def join_shell_args(args: Iterable[str]) -> str:
	return ' '.join(shlex.quote(arg) for arg in args)


def join_windows_args(args: Iterable[str]) -> str:
	return subprocess.list2cmdline([str(arg) for arg in args])


def discover_windows_ninja() -> str:
	path = shutil.which('ninja')
	if path:
		return path

	program_files_roots = [
		Path(os.environ.get('ProgramFiles', r'C:\Program Files')),
		Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')),
	]

	for root in program_files_roots:
		for candidate in root.glob('Microsoft Visual Studio/*/*/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'):
			if candidate.is_file():
				return str(candidate)

	raise SystemExit('ninja not found; install Ninja or Visual Studio CMake tools')


def run_in_emsdk_env(command: Sequence[str], workspace: Path, cwd: str | Path | None = None, env: Mapping[str, str] | None = None) -> None:
	emscripten_root = workspace / 'emsdk'
	if not emscripten_root.is_dir():
		raise SystemExit(f'Workspace EMSDK is not prepared: {emscripten_root}')

	if os.name == 'nt':
		emscripten_env = emscripten_root / 'emsdk_env.bat'
		if not emscripten_env.is_file():
			raise SystemExit(f'Emscripten environment script not found: {emscripten_env}')
		with tempfile.NamedTemporaryFile('w', suffix='.cmd', delete=False, encoding='utf-8') as script_file:
			script_file.write('@echo off\n')
			script_file.write(f'call "{emscripten_env}" >nul\n')
			script_file.write('if errorlevel 1 exit /b %errorlevel%\n')
			script_file.write(f'{join_windows_args(command)}\n')
			temp_script = Path(script_file.name)
		try:
			run(['cmd', '/d', '/c', str(temp_script)], cwd=cwd, env=env)
		finally:
			temp_script.unlink(missing_ok=True)
	else:
		emscripten_env = emscripten_root / 'emsdk_env.sh'
		if not emscripten_env.is_file():
			raise SystemExit(f'Emscripten environment script not found: {emscripten_env}')
		full_command = f'source "{emscripten_env}" >/dev/null && {join_shell_args(str(part) for part in command)}'
		run_bash(full_command, cwd=cwd, env=env)


def build_toolset_version() -> str:
	return f'v1-{os.name}'


def build_emscripten_version(env: Mapping[str, str]) -> str:
	version = env.get('EMSCRIPTEN_VERSION', '')
	if not version:
		raise SystemExit('EMSCRIPTEN_VERSION is not configured')
	return version


def build_android_ndk_version(env: Mapping[str, str]) -> str:
	version = env.get('ANDROID_NDK_VERSION', '')
	if not version:
		raise SystemExit('ANDROID_NDK_VERSION is not configured')
	return version


def build_android_sdk_version(env: Mapping[str, str]) -> str:
	version = env.get('ANDROID_SDK_VERSION', '')
	if not version:
		raise SystemExit('ANDROID_SDK_VERSION is not configured')
	return version


def build_android_sdk_archive_name(env: Mapping[str, str]) -> str:
	version = build_android_sdk_version(env)
	return f'commandlinetools-{resolve_android_sdk_host_tag()}-{version}_latest.zip'


def build_dotnet_version(env: Mapping[str, str]) -> str:
	version = env.get('FO_DOTNET_RUNTIME', '')
	if not version:
		raise SystemExit('FO_DOTNET_RUNTIME is not configured')
	return version


def workspace_version_marker(workspace: Path, part_name: str) -> Path:
	return workspace / f'{part_name}-version.txt'


def is_workspace_part_ready(workspace: Path, part_name: str, version: str) -> bool:
	return read_first_line(workspace_version_marker(workspace, part_name)) == version


def mark_workspace_part_ready(workspace: Path, part_name: str, version: str) -> None:
	workspace_version_marker(workspace, part_name).write_text(version + '\n', encoding='utf-8')


def ensure_workspace_part(
	workspace: Path,
	part_name: str,
	version: str,
	check_only: bool,
	action: Callable[[Mapping[str, str]], None],
	env: Mapping[str, str],
	) -> None:
	if is_workspace_part_ready(workspace, part_name, version):
		log(f'Workspace part {part_name} is ready ({version})')
		return

	if check_only:
		raise SystemExit(f'Workspace part {part_name} is not ready ({version})')

	log(f'Prepare workspace part {part_name} ({version})')
	action(env)
	mark_workspace_part_ready(workspace, part_name, version)


def run_marker_step(marker_path: Path, action_name: str, action: Callable[[], None]) -> None:
	if marker_path.exists():
		return
	log(action_name)
	action()
	marker_path.touch()


def reset_marker(marker_path: Path) -> None:
	remove_path_if_exists(marker_path)


def ensure_directory_link(link_path: Path, target_path: str | Path) -> None:
	if link_path.exists():
		return
	if os.name == 'nt':
		run(['cmd', '/c', 'mklink', '/d', str(link_path), str(target_path)])
	else:
		link_path.symlink_to(Path(target_path), target_is_directory=True)


def upload_codecov(build_dir: Path, token: str) -> None:
	codecov_path = build_dir / 'codecov'
	reset_marker(codecov_path)
	run(['curl', '-Os', 'https://uploader.codecov.io/latest/linux/codecov'], cwd=build_dir)
	codecov_path.chmod(codecov_path.stat().st_mode | 0o111)
	run([str(codecov_path), '-t', token, '--gcov'], cwd=build_dir)


def install_linux_packages(group_name: str, workspace: Path, check_only: bool) -> None:
	version, packages = LINUX_PACKAGE_GROUPS[group_name]
	if is_workspace_part_ready(workspace, group_name, version):
		log(f'Workspace part {group_name} is ready ({version})')
		return

	if check_only:
		raise SystemExit(f'Workspace part {group_name} is not ready ({version})')

	package_manager = ['apt-get', '-qq', '-y', '-o', 'DPkg::Lock::Timeout=-1']
	if not is_running_as_root():
		package_manager.insert(0, 'sudo')
	log(f'Install Linux package group {group_name}')
	run([*package_manager, 'update'])
	for package_name in packages:
		log('Install', package_name)
		run([*package_manager, 'install', package_name])
	mark_workspace_part_ready(workspace, group_name, version)


def reset_build_dir(build_dir: Path) -> None:
	if build_dir.exists():
		remove_tree(build_dir)
	ensure_dir(build_dir)


def make_toolset_cmake_args(output_path: str, config_name: str | None = None) -> list[str]:
	return [f'-DFO_OUTPUT_PATH={to_cmake_path(output_path)}', *build_flag_args('toolset', config=config_name)]


def prepare_toolset_workspace(env: Mapping[str, str]) -> None:
	output = env['FO_OUTPUT']
	project_root = env['FO_PROJECT_ROOT']
	build_dir = resolve_toolset_build_dir(env)
	reset_build_dir(build_dir)

	if os.name == 'nt':
		run([
			'cmake',
			'-G',
			'Visual Studio 17 2022',
			'-A',
			'x64',
			*make_toolset_cmake_args(output),
			to_cmake_path(project_root),
		], cwd=build_dir)
		return

	build_env = make_linux_build_env()
	configure_cmd = make_unix_makefiles_configure_cmd(*make_toolset_cmake_args(output, config_name='Release'), project_root)
	run(configure_cmd, cwd=build_dir, env=build_env)


def run_emsdk_command(emsdk_root: Path, *args: str) -> None:
	if os.name == 'nt':
		command = ['cmd', '/d', '/s', '/c', str(emsdk_root / 'emsdk.bat'), *args]
		run(command, cwd=emsdk_root)
	else:
		run([emsdk_root / 'emsdk', *args], cwd=emsdk_root)


def prepare_emscripten_workspace(env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	emsdk_root = workspace / 'emsdk'
	remove_path_if_exists(emsdk_root)

	clone_git_repo(emsdk_root, 'https://github.com/emscripten-core/emsdk.git')
	run_emsdk_command(emsdk_root, 'list')
	version = build_emscripten_version(env)
	run_emsdk_command(emsdk_root, 'install', '--build=Release', '--shallow', version)
	run_emsdk_command(emsdk_root, 'activate', '--build=Release', version)


def prepare_android_ndk_workspace(env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	version = build_android_ndk_version(env)
	archive_name = f'{version}-linux.zip'
	archive_path = workspace / archive_name
	ndk_source_dir = workspace / version
	ndk_target_dir = workspace / 'android-ndk'

	remove_path_if_exists(archive_path)
	remove_path_if_exists(ndk_source_dir)
	remove_path_if_exists(ndk_target_dir)

	url = f'https://dl.google.com/android/repository/{archive_name}'
	download_file(url, archive_path, 'Android NDK')
	log('Unpack Android NDK:', archive_path)
	extract_zip_with_permissions(archive_path, workspace)
	shutil.move(str(ndk_source_dir), str(ndk_target_dir))
	remove_path_if_exists(archive_path)


def prepare_android_sdk_workspace(env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	archive_name = build_android_sdk_archive_name(env)
	archive_path = workspace / archive_name
	android_sdk_root = workspace / 'android-sdk'
	extract_root = workspace / 'android-sdk-extract'
	cmdline_tools_root = android_sdk_root / 'cmdline-tools'
	cmdline_tools_latest = cmdline_tools_root / 'latest'

	remove_path_if_exists(archive_path)
	remove_path_if_exists(extract_root)
	remove_path_if_exists(android_sdk_root)
	ensure_dir(cmdline_tools_root)

	url = f'https://dl.google.com/android/repository/{archive_name}'
	download_file(url, archive_path, 'Android SDK Command-line Tools')
	log('Unpack Android SDK Command-line Tools:', archive_path)
	extract_zip_with_permissions(archive_path, extract_root)

	shutil.move(str(extract_root / 'cmdline-tools'), str(cmdline_tools_latest))
	remove_path_if_exists(extract_root)
	remove_path_if_exists(archive_path)

	sdkmanager = cmdline_tools_latest / 'bin' / 'sdkmanager'
	if not sdkmanager.is_file():
		raise SystemExit(f'sdkmanager not found after extraction: {sdkmanager}')

	sdk_env = os.environ.copy()
	sdk_env['ANDROID_HOME'] = str(android_sdk_root)
	sdk_env['ANDROID_SDK_ROOT'] = str(android_sdk_root)

	run_bash(
		f'yes | {shlex.quote(str(sdkmanager))} --sdk_root={shlex.quote(str(android_sdk_root))} --licenses >/dev/null',
		env=sdk_env,
	)
	run(
		[
			sdkmanager,
			f'--sdk_root={android_sdk_root}',
			*ANDROID_REQUIRED_SDK_PACKAGES,
		],
		env=sdk_env,
	)


def prepare_dotnet_workspace(env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	dotnet_root = workspace / 'dotnet'
	ensure_empty_dir(dotnet_root)
	clone_git_repo(dotnet_root / 'runtime', 'https://github.com/dotnet/runtime.git', branch_name=build_dotnet_version(env), depth=1)


def prepare_workspace(parts: Sequence[str], check_only: bool, env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	output = Path(env['FO_OUTPUT'])
	ensure_dir(workspace)
	ensure_dir(output)

	part_versions = {
		'toolset': build_toolset_version(),
		'emscripten': build_emscripten_version(env),
		'android-sdk': build_android_sdk_version(env),
		'android-ndk': build_android_ndk_version(env),
		'dotnet': build_dotnet_version(env),
	}
	part_actions = {
		'toolset': prepare_toolset_workspace,
		'emscripten': prepare_emscripten_workspace,
		'android-sdk': prepare_android_sdk_workspace,
		'android-ndk': prepare_android_ndk_workspace,
		'dotnet': prepare_dotnet_workspace,
	}

	for part_name in parts:
		ensure_workspace_part(workspace, part_name, part_versions[part_name], check_only, part_actions[part_name], env)


def has_feature(features: Sequence[str], *names: str) -> bool:
	return any(feature in names for feature in features)


HOST_DEFAULT_FEATURES = {
	'linux': ['all'],
	'windows': ['toolset'],
	'macos': [],
}

LINUX_FEATURE_PACKAGE_GROUPS = {
	'linux': ['linux-packages'],
	'web': ['web-packages'],
	'android-arm32': ['android-packages'],
	'android-arm64': ['android-packages'],
	'android-x86': ['android-packages'],
	'all': ['linux-packages', 'web-packages', 'android-packages'],
}

HOST_FEATURE_WORKSPACE_PARTS = {
	'linux': {
		'toolset': ['toolset'],
		'web': ['emscripten'],
		'android-arm32': ['android-sdk', 'android-ndk'],
		'android-arm64': ['android-sdk', 'android-ndk'],
		'android-x86': ['android-sdk', 'android-ndk'],
		'dotnet': ['dotnet'],
		'all': ['toolset', 'emscripten', 'android-sdk', 'android-ndk', 'dotnet'],
	},
	'windows': {
		'toolset': ['toolset'],
		'web': ['emscripten'],
		'all': ['toolset', 'emscripten'],
	},
	'macos': {},
}


def resolve_selected_features(host_name: str, features: Sequence[str]) -> list[str]:
	selected = list(features)
	if selected:
		return selected
	default_features = HOST_DEFAULT_FEATURES.get(host_name)
	if default_features is None:
		raise SystemExit(f'Unsupported host workspace preparation: {host_name}')
	return list(default_features)


def extend_unique(values: list[str], new_values: Sequence[str]) -> None:
	for value in new_values:
		if value not in values:
			values.append(value)


def resolve_feature_values(selected: Sequence[str], feature_map: Mapping[str, Sequence[str]]) -> list[str]:
	resolved: list[str] = []
	for feature_name, values in feature_map.items():
		if has_feature(selected, feature_name):
			extend_unique(resolved, values)
	return resolved


def prepare_linux_host_workspace(selected: Sequence[str], workspace: Path, check_only: bool, env: Mapping[str, str]) -> None:
	if has_feature(selected, 'packages', 'all'):
		install_linux_packages('common-packages', workspace, check_only)
		for package_group in resolve_feature_values(selected, LINUX_FEATURE_PACKAGE_GROUPS):
			install_linux_packages(package_group, workspace, check_only)

	parts = resolve_feature_values(selected, HOST_FEATURE_WORKSPACE_PARTS['linux'])
	if parts:
		prepare_workspace(parts, check_only, env)


def prepare_windows_host_workspace(selected: Sequence[str], workspace: Path, check_only: bool, env: Mapping[str, str]) -> None:
	_ = workspace
	parts = resolve_feature_values(selected, HOST_FEATURE_WORKSPACE_PARTS['windows'])
	if parts:
		prepare_workspace(parts, check_only, env)


def prepare_macos_host_workspace(selected: Sequence[str], workspace: Path, check_only: bool, env: Mapping[str, str]) -> None:
	_ = selected, workspace, check_only, env


HOST_WORKSPACE_PREPARERS: dict[str, Callable[[Sequence[str], Path, bool, Mapping[str, str]], None]] = {
	'linux': prepare_linux_host_workspace,
	'windows': prepare_windows_host_workspace,
	'macos': prepare_macos_host_workspace,
}


def prepare_host_workspace(host_name: str, features: Sequence[str], check_only: bool, env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	output = Path(env['FO_OUTPUT'])
	ensure_dir(workspace)
	ensure_dir(output)

	issues = check_host_tools(host_name)
	if issues:
		for issue in issues:
			print(issue, flush=True)
		raise SystemExit(1)

	selected = resolve_selected_features(host_name, features)
	host_preparer = HOST_WORKSPACE_PREPARERS.get(host_name)
	if host_preparer is None:
		raise SystemExit(f'Unsupported host workspace preparation: {host_name}')
	host_preparer(selected, workspace, check_only, env)


def resolve_build_hash(env: Mapping[str, str]) -> str:
	project_root = Path(env['FO_PROJECT_ROOT'])
	build_hash = run_capture_text(['git', 'rev-parse', 'HEAD'], cwd=project_root)
	assert build_hash, 'git rev-parse HEAD returned empty hash'
	return build_hash


def package_web_debug(env: Mapping[str, str], devname: str, configs: Sequence[str]) -> None:
	for config in configs:
		_package_web_debug_config(env, devname, config)


def _package_web_debug_config(env: Mapping[str, str], devname: str, config: str) -> None:
	import foconfig

	project_root = Path(env['FO_PROJECT_ROOT'])
	output_root_input = Path(env['FO_OUTPUT'])
	output_root = resolve_web_debug_root(env)
	package_script = resolve_buildtools_path(env, 'package.py')
	build_hash = resolve_build_hash(env)
	input_roots = [output_root_input, project_root]

	fomain_path = discover_fomain(project_root)
	fomain = foconfig.ConfigParser()
	fomain.loadFromFile(fomain_path)
	nicename = fomain.mainSection().getStr('Common.GameName')

	command = [
		sys.executable,
		str(package_script),
		'-maincfg',
		str(fomain_path),
		'-buildhash',
		build_hash,
		'-devname',
		devname,
		'-nicename',
		nicename,
		'-target',
		'Client',
		'-platform',
		'Web',
		'-arch',
		'wasm',
		'-pack',
		'Raw+WebServer',
		'-config',
		config,
		'-zip-compress-level',
		'1',
		'-output',
		str(output_root),
	]
	for input_root in input_roots:
		command.extend(['-input', str(input_root)])

	run(command)


def web_package_dir(env: Mapping[str, str], devname: str, config: str) -> Path:
	return resolve_web_package_dir(env, devname, config)


def resolve_android_debug_root(env: Mapping[str, str]) -> Path:
	return resolve_workspace_path(env, 'android-debug')


def resolve_android_package_dir(env: Mapping[str, str], devname: str, config: str) -> Path:
	return resolve_android_debug_root(env) / f'{devname}-Client-{config}-Android'


def package_android_debug(env: Mapping[str, str], devname: str, platform_name: str, configs: Sequence[str]) -> None:
	for config in configs:
		_package_android_debug_config(env, devname, platform_name, config)


def _package_android_debug_config(env: Mapping[str, str], devname: str, platform_name: str, config: str) -> None:
	import foconfig

	project_root = Path(env['FO_PROJECT_ROOT'])
	output_root_input = Path(env['FO_OUTPUT'])
	output_root = resolve_android_debug_root(env)
	package_script = resolve_buildtools_path(env, 'package.py')
	build_hash = resolve_build_hash(env)
	input_roots = [output_root_input, project_root]

	fomain_path = discover_fomain(project_root)
	fomain = foconfig.ConfigParser()
	fomain.loadFromFile(fomain_path)
	nicename = fomain.mainSection().getStr('Common.GameName')
	android_abi = resolve_android_abi(platform_name)

	command = [
		sys.executable,
		str(package_script),
		'-maincfg',
		str(fomain_path),
		'-buildhash',
		build_hash,
		'-devname',
		devname,
		'-nicename',
		nicename,
		'-target',
		'Client',
		'-platform',
		'Android',
		'-arch',
		android_abi,
		'-pack',
		'Raw',
		'-config',
		config,
		'-zip-compress-level',
		'1',
		'-output',
		str(output_root),
	]
	for input_root in input_roots:
		command.extend(['-input', str(input_root)])

	run(command)


def resolve_android_abi(platform_name: str) -> str:
	android_abi = ANDROID_ABI_BY_PLATFORM.get(platform_name)
	if android_abi is None:
		raise SystemExit(f'Invalid Android platform: {platform_name}')
	return android_abi


def make_linux_build_env(compiler_name: str = 'clang') -> EnvMap:
	build_env = os.environ.copy()
	if compiler_name == 'gcc':
		build_env['CC'] = '/usr/bin/gcc'
		build_env['CXX'] = '/usr/bin/g++'
	else:
		build_env['CC'] = '/usr/bin/clang'
		build_env['CXX'] = '/usr/bin/clang++'
	return build_env


def make_android_toolchain_settings(platform_name: str, env: Mapping[str, str]) -> list[str]:
	android_abi = resolve_android_abi(platform_name)
	return [
		f'-DANDROID_ABI={android_abi}',
		f'-DANDROID_PLATFORM=android-{env["ANDROID_NATIVE_API_LEVEL_NUMBER"]}',
		'-DANDROID_STL=c++_static',
	]


def make_ios_toolchain_settings(env: Mapping[str, str]) -> list[str]:
	return [
		'-DPLATFORM=SIMULATOR64',
		f'-DDEPLOYMENT_TARGET={env["FO_IOS_SDK"]}',
		'-DENABLE_BITCODE=0',
		'-DENABLE_ARC=0',
		'-DENABLE_VISIBILITY=0',
		'-DENABLE_STRICT_TRY_COMPILE=0',
	]


def run_apple_cmake_build(cmake_bin: str, build_dir: Path, config: str) -> None:
	run(make_cmake_build_cmd(config, cmake_bin=cmake_bin), cwd=build_dir)


def make_platform_build_flag_args(platform_name: str, target_name: str, config: str) -> list[str]:
	if platform_name in ('linux', 'web', *ANDROID_PLATFORMS):
		return build_flag_args(target_name, config=config)
	return build_flag_args(target_name)


def make_platform_configure_env(platform_name: str, compiler_name: str) -> EnvMap | None:
	if platform_name == 'linux':
		return make_linux_build_env(compiler_name)
	return None


def resolve_platform_configure_runner(platform_name: str) -> str:
	return 'emsdk' if platform_name == 'web' else 'direct'


def resolve_platform_build_runner(platform_name: str) -> str:
	if platform_name == 'web':
		return 'emsdk'
	if platform_name in APPLE_PLATFORMS:
		return 'apple'
	return 'direct'


def make_platform_configure_cmd(
	platform_name: str,
	source_path: str,
	configure_args: Sequence[str],
	env: Mapping[str, str],
) -> list[str]:
	if platform_name == 'linux':
		return make_unix_makefiles_configure_cmd(*configure_args, to_cmake_path(source_path))
	if platform_name == 'web':
		toolchain_file = resolve_emscripten_toolchain(env)
		configure_cmd = make_emsdk_configure_cmd(toolchain_file, *configure_args)
		configure_cmd.append(to_cmake_path(source_path))
		return configure_cmd
	if platform_name in ANDROID_PLATFORMS:
		toolchain_file = to_cmake_path(f'{env["ANDROID_NDK_ROOT"]}/build/cmake/android.toolchain.cmake')
		toolchain_settings = make_android_toolchain_settings(platform_name, env)
		return make_android_configure_cmd(toolchain_file, toolchain_settings, *configure_args, to_cmake_path(source_path))
	if platform_name in APPLE_PLATFORMS:
		cmake_bin = resolve_apple_cmake()
		if platform_name == 'mac':
			return make_xcode_configure_cmd(cmake_bin, *configure_args, to_cmake_path(source_path))
		toolchain_settings = make_ios_toolchain_settings(env)
		ios_toolchain = resolve_buildtools_cmake_path(env, 'ios.toolchain.cmake')
		return make_xcode_configure_cmd(cmake_bin, f'-DCMAKE_TOOLCHAIN_FILE={to_cmake_path(ios_toolchain)}', *toolchain_settings, *configure_args, to_cmake_path(source_path))
	if platform_name.startswith('win'):
		return make_windows_configure_cmd(platform_name, *configure_args, to_cmake_path(source_path))
	raise SystemExit(f'Unsupported platform: {platform_name}')


def run_platform_build_step(
	platform_name: str,
	build_dir: Path,
	config: str,
	env: Mapping[str, str],
	build_env: Mapping[str, str] | None,
) -> None:
	build_runner = resolve_platform_build_runner(platform_name)
	if build_runner == 'emsdk':
		workspace = Path(env['FO_WORKSPACE'])
		run_emsdk_cmake_build(workspace, build_dir, config)
	elif build_runner == 'apple':
		cmake_bin = resolve_apple_cmake()
		run_apple_cmake_build(cmake_bin, build_dir, config)
	else:
		run_cmake_build(build_dir, config, env=build_env)


def run_platform_configure_step(
	platform_name: str,
	configure_cmd: Sequence[str],
	build_dir: Path,
	env: Mapping[str, str],
	build_env: Mapping[str, str] | None,
) -> None:
	configure_runner = resolve_platform_configure_runner(platform_name)
	if configure_runner == 'emsdk':
		workspace = Path(env['FO_WORKSPACE'])
		run_in_emsdk_env(configure_cmd, workspace, cwd=build_dir)
	else:
		run(configure_cmd, cwd=build_dir, env=build_env)


def run_platform_configure_build(
	platform_name: str,
	target_name: str,
	source_path: str,
	build_dir: Path,
	config: str,
	env: Mapping[str, str],
	extra_cmake_args: Sequence[str] = (),
	compiler_name: str = 'clang',
) -> None:
	cmake_args = make_platform_build_flag_args(platform_name, target_name, config)
	configure_args = [*extra_cmake_args, *cmake_args]
	build_env = make_platform_configure_env(platform_name, compiler_name)
	configure_cmd = make_platform_configure_cmd(platform_name, source_path, configure_args, env)
	run_platform_configure_step(platform_name, configure_cmd, build_dir, env, build_env)

	run_platform_build_step(platform_name, build_dir, config, env, build_env)


def configure_build(platform_name: str, target_name: str, config: str, env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	output = env['FO_OUTPUT']
	project_root = env['FO_PROJECT_ROOT']
	build_dir = resolve_configure_build_dir(env, platform_name, target_name, config)
	ensure_dir(workspace)
	ensure_dir(output)
	ensure_dir(build_dir)
	ready_path = build_dir / 'READY'
	reset_marker(ready_path)

	run_platform_configure_build(
		platform_name,
		target_name,
		project_root,
		build_dir,
		config,
		env,
		extra_cmake_args=[f'-DFO_OUTPUT_PATH={to_cmake_path(output)}'],
	)

	ready_path.touch()


def prepare_validation_project(env: Mapping[str, str]) -> Path:
	workspace = Path(env['FO_WORKSPACE'])
	validation_root = resolve_validation_project_workspace(env)
	source_root = resolve_validation_project_source(env)
	ensure_dir(workspace)
	copy_directory(source_root, validation_root, dirs_exist_ok=True)
	engine_link = validation_root / 'Engine'
	ensure_directory_link(engine_link, env['FO_ENGINE_ROOT'])
	return validation_root


def run_validation(name: str, env: Mapping[str, str]) -> None:
	if name not in VALIDATION_TARGETS:
		raise SystemExit(f'Invalid validation target: {name}')
	validation = VALIDATION_TARGETS[name]
	validation_root = prepare_validation_project(env)
	platform_name = validation['platform']
	target_name = validation['target']
	config = validation['config']
	build_dir = resolve_validation_build_dir(env, name)
	ensure_dir(build_dir)

	run_platform_configure_build(
		platform_name,
		target_name,
		str(validation_root),
		build_dir,
		config,
		env,
		compiler_name=validation.get('compiler', 'clang'),
	)

	run_target = validation.get('run_target')
	if run_target:
		log('Run', run_target)
		run_cmake_target(build_dir, config, run_target)

		if name == 'code-coverage' and os.environ.get('CODECOV_TOKEN'):
			upload_codecov(build_dir, os.environ['CODECOV_TOKEN'])


def setup_mono(os_name: str, arch: str, config: str, env: Mapping[str, str]) -> None:
	triplet = f'{os_name}.{arch}.{config}'
	workspace = Path(env['FO_WORKSPACE'])
	runtime_root = workspace / 'runtime'
	clone_marker = workspace / 'CLONED'
	built_marker = workspace / f'BUILT_{triplet}'
	ready_marker = workspace / f'READY_{triplet}'

	def clone_runtime() -> None:
		if runtime_root.exists():
			log('Remove previous repository')
			remove_path_if_exists(runtime_root)
		dotnet_runtime_root = env.get('FO_DOTNET_RUNTIME_ROOT', '')
		if dotnet_runtime_root:
			log('Copy runtime')
			copy_directory(dotnet_runtime_root, runtime_root)
		else:
			log('Clone runtime')
			clone_git_repo(runtime_root, 'https://github.com/dotnet/runtime.git', branch_name=env['FO_DOTNET_RUNTIME'], depth=1)

	run_marker_step(clone_marker, 'Prepare runtime source', clone_runtime)

	def build_runtime() -> None:
		build_script = resolve_runtime_build_script()
		run([build_script, '-os', os_name, '-arch', arch, '-c', config, '-subset', 'mono.runtime'], cwd=runtime_root)

	run_marker_step(built_marker, 'Build runtime', build_runtime)

	def publish_runtime() -> None:
		output_dir = workspace / 'output' / 'mono' / triplet
		input_dir = runtime_root / 'artifacts' / 'obj' / 'mono' / triplet / 'out'
		if not input_dir.is_dir():
			raise SystemExit(f'Files not found: {input_dir}')
		log('Copy from', input_dir, 'to', output_dir)
		copy_directory(input_dir, output_dir, dirs_exist_ok=True)

	run_marker_step(ready_marker, f'Publish runtime {triplet}', publish_runtime)

	log(f'Runtime {triplet} is ready!')



def discover_clang_format() -> str:
	for executable in ('clang-format-20', 'clang-format'):
		path = shutil.which(executable)
		if not path:
			continue

		try:
			version_output = subprocess.check_output([path, '--version'], text=True, encoding='utf-8', errors='replace')
		except (OSError, subprocess.CalledProcessError):
			continue

		match = CLANG_FORMAT_VERSION_RE.search(version_output)
		if match is not None and int(match.group(1)) == 20:
			return path

	raise SystemExit('clang-format version 20 not found in system PATH')


def read_text_strip_bom(path: Path) -> tuple[str, bool]:
	data = path.read_bytes()
	has_bom = data.startswith(UTF8_BOM)
	return data.decode('utf-8-sig'), has_bom


def detect_line_ending(content: str) -> str:
	return '\r\n' if '\r\n' in content else '\n'


def normalize_line_endings(content: str, line_ending: str) -> str:
	return content.replace('\r\n', '\n').replace('\r', '\n').replace('\n', line_ending)


def normalize_for_comparison(content: str) -> str:
	return content.replace('\r\n', '\n').replace('\r', '\n')


def differs_beyond_line_endings(original: str, formatted: str) -> bool:
	return normalize_for_comparison(original) != normalize_for_comparison(formatted)


def ensure_trailing_newline(content: str, line_ending: str) -> str:
	return content.rstrip('\r\n') + line_ending


def write_text_utf8(path: Path, content: str) -> None:
	path.write_bytes(content.encode('utf-8'))


def strip_text_bom(content: str) -> str:
	return content[1:] if content.startswith('\ufeff') else content


def format_files(clang_format: str, root: Path, patterns: Sequence[str]) -> int:
	files: list[Path] = []
	for pattern in patterns:
		files.extend(sorted(root.glob(pattern)))
	if not files:
		log('No files matched format patterns in', root)
		return 0

	changed = 0
	seen: set[Path] = set()
	unique_files: list[Path] = []
	for path in files:
		if path in seen:
			continue
		seen.add(path)
		unique_files.append(path)

	progress = TerminalProgress('BuildTools')
	total = len(unique_files)

	for index, path in enumerate(unique_files, start=1):
		rel_path = path.relative_to(root).as_posix()
		progress.update(f'Formatting {index}/{total}: {rel_path}')

		original, has_bom = read_text_strip_bom(path)
		formatted = strip_text_bom(run_capture_text([clang_format, str(path)], log_command=False))
		formatted = ensure_trailing_newline(normalize_line_endings(formatted, detect_line_ending(original)), detect_line_ending(original))
		if not differs_beyond_line_endings(original, formatted) and not has_bom:
			continue

		changed += 1
		write_text_utf8(path, formatted)
		progress.clear()
		log('Formatted:', rel_path)

	progress.finish(f'Completed, changed {changed} file(s)')
	return changed


def format_source(env: Mapping[str, str]) -> None:
	clang_format = discover_clang_format()
	format_files(clang_format, Path(env['FO_ENGINE_ROOT']) / 'Source', FORMAT_PATTERNS)


def build_toolset(target: str, env: Mapping[str, str]) -> None:
	build_dir = resolve_toolset_build_dir(env)
	if not build_dir.is_dir():
		raise SystemExit(f'Toolset build directory not found: {build_dir}')
	run_cmake_target(build_dir, 'Release', target)


def create_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description='Shared BuildTools helpers')
	subparsers = parser.add_subparsers(dest='command', required=True)

	env_parser = subparsers.add_parser('env', help='resolve BuildTools environment')
	env_parser.add_argument('--shell', choices=['bash', 'cmd', 'plain'], default='plain')
	env_parser.add_argument('--summary', action='store_true')
	env_parser.add_argument('--summary-only', action='store_true')

	build_parser = subparsers.add_parser('build', help='configure and build a target')
	build_parser.add_argument('platform')
	build_parser.add_argument('target')
	build_parser.add_argument('config', nargs='?', default='Release')

	validate_parser = subparsers.add_parser('validate', help='configure and validate a scenario')
	validate_parser.add_argument('name')

	mono_parser = subparsers.add_parser('setup-mono', help='prepare mono runtime')
	mono_parser.add_argument('os_name')
	mono_parser.add_argument('arch')
	mono_parser.add_argument('config')

	format_parser = subparsers.add_parser('format-source', help='format engine source files')
	format_parser.set_defaults(no_args=True)

	toolset_parser = subparsers.add_parser('toolset', help='build an existing toolset target')
	toolset_parser.add_argument('target')

	prepare_parser = subparsers.add_parser('prepare-workspace', help='prepare shared workspace parts')
	prepare_parser.add_argument('parts', nargs='+', choices=['toolset', 'emscripten', 'android-sdk', 'android-ndk', 'dotnet'])
	prepare_parser.add_argument('--check', action='store_true')

	package_web_parser = subparsers.add_parser('package-web-debug', help='package the local web debug client')
	package_web_parser.add_argument('devname', help='short project name for binary/directory naming (e.g. LF)')
	package_web_parser.add_argument('configs', nargs='+', help='config names to package (e.g. RemoteSceneLaunch LocalTest)')

	package_android_parser = subparsers.add_parser('package-android-debug', help='package the local android debug client')
	package_android_parser.add_argument('devname', help='short project name for binary/directory naming (e.g. LF)')
	package_android_parser.add_argument('platform', choices=ANDROID_PLATFORMS, help='Android target platform (e.g. android-arm64)')
	package_android_parser.add_argument('configs', nargs='+', help='config names to package (e.g. LocalTest)')

	host_check_parser = subparsers.add_parser('host-check', help='check host prerequisites')
	host_check_parser.add_argument('host', choices=['linux', 'macos', 'windows'])

	prepare_host_parser = subparsers.add_parser('prepare-host-workspace', help='prepare host workspace and prerequisites')
	prepare_host_parser.add_argument('host', choices=['linux', 'windows', 'macos'])
	prepare_host_parser.add_argument('features', nargs='*', choices=['packages', 'linux', 'web', 'android-arm32', 'android-arm64', 'android-x86', 'toolset', 'dotnet', 'all'])
	prepare_host_parser.add_argument('--check', action='store_true')

	return parser


def main() -> None:
	parser = create_parser()
	args = parser.parse_args()
	env = resolve_env()
	apply_env(env)

	if args.command == 'env':
		if not args.summary_only:
			emit_shell_env(env, args.shell)
		if args.summary or args.summary_only:
			print_env_summary(env)
		return

	if args.command != 'format-source':
		print_env_summary(env)

	if args.command == 'build':
		configure_build(args.platform, args.target, args.config, env)
		return
	if args.command == 'validate':
		run_validation(args.name, env)
		return
	if args.command == 'setup-mono':
		setup_mono(args.os_name, args.arch, args.config, env)
		return
	if args.command == 'format-source':
		format_source(env)
		return
	if args.command == 'toolset':
		build_toolset(args.target, env)
		return
	if args.command == 'prepare-workspace':
		prepare_workspace(args.parts, args.check, env)
		return
	if args.command == 'package-web-debug':
		package_web_debug(env, args.devname, args.configs)
		return
	if args.command == 'package-android-debug':
		package_android_debug(env, args.devname, args.platform, args.configs)
		return
	if args.command == 'host-check':
		issues = check_host_tools(args.host)
		if issues:
			for issue in issues:
				print(issue, flush=True)
			raise SystemExit(1)
		print('Host is ready!', flush=True)
		return
	if args.command == 'prepare-host-workspace':
		prepare_host_workspace(args.host, args.features, args.check, env)
		print('Workspace is ready!', flush=True)
		return
	raise SystemExit(f'Unsupported command: {args.command}')


if __name__ == '__main__':
	main()
