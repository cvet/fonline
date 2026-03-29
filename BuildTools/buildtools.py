#!/usr/bin/python3

from __future__ import annotations

import argparse
import os
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Iterable, Mapping, Sequence


EnvMap = dict[str, str]
FlagMap = dict[str, int]
ValidationMap = dict[str, str]


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

BUILD_TARGETS = {
	'client': dict(FO_BUILD_CLIENT=1),
	'server': dict(FO_BUILD_SERVER=1),
	'editor': dict(FO_BUILD_EDITOR=1),
	'mapper': dict(FO_BUILD_MAPPER=1),
	'ascompiler': dict(FO_BUILD_ASCOMPILER=1),
	'baker': dict(FO_BUILD_BAKER=1),
	'unit-tests': dict(FO_UNIT_TESTS=1),
	'code-coverage': dict(FO_CODE_COVERAGE=1),
	'toolset': dict(FO_BUILD_ASCOMPILER=1, FO_BUILD_BAKER=1),
	'full': dict(
		FO_BUILD_CLIENT=1,
		FO_BUILD_SERVER=1,
		FO_BUILD_EDITOR=1,
		FO_BUILD_MAPPER=1,
		FO_BUILD_ASCOMPILER=1,
		FO_BUILD_BAKER=1,
	),
}

VALIDATION_TARGETS = {
	'linux-client': dict(platform='linux', target='client', config='Debug'),
	'linux-gcc-client': dict(platform='linux', target='client', config='Debug', compiler='gcc'),
	'android-arm-client': dict(platform='android', target='client', config='Debug'),
	'android-arm64-client': dict(platform='android-arm64', target='client', config='Debug'),
	'android-x86-client': dict(platform='android-x86', target='client', config='Debug'),
	'web-client': dict(platform='web', target='client', config='Debug'),
	'mac-client': dict(platform='mac', target='client', config='Debug'),
	'ios-client': dict(platform='ios', target='client', config='Debug'),
	'linux-server': dict(platform='linux', target='server', config='Debug'),
	'linux-gcc-server': dict(platform='linux', target='server', config='Debug', compiler='gcc'),
	'linux-editor': dict(platform='linux', target='editor', config='Debug'),
	'linux-gcc-editor': dict(platform='linux', target='editor', config='Debug', compiler='gcc'),
	'linux-mapper': dict(platform='linux', target='mapper', config='Debug'),
	'linux-gcc-mapper': dict(platform='linux', target='mapper', config='Debug', compiler='gcc'),
	'linux-ascompiler': dict(platform='linux', target='ascompiler', config='Debug'),
	'linux-gcc-ascompiler': dict(platform='linux', target='ascompiler', config='Debug', compiler='gcc'),
	'linux-baker': dict(platform='linux', target='baker', config='Debug'),
	'linux-gcc-baker': dict(platform='linux', target='baker', config='Debug', compiler='gcc'),
	'win32-client': dict(platform='win32', target='client', config='Debug'),
	'win64-client': dict(platform='win64', target='client', config='Debug'),
	'win64-clang-client': dict(platform='win64-clang', target='client', config='Debug'),
	'win64-server': dict(platform='win64', target='server', config='Debug'),
	'win64-clang-server': dict(platform='win64-clang', target='server', config='Debug'),
	'win64-editor': dict(platform='win64', target='editor', config='Debug'),
	'win64-mapper': dict(platform='win64', target='mapper', config='Debug'),
	'win64-ascompiler': dict(platform='win64', target='ascompiler', config='Debug'),
	'win64-clang-ascompiler': dict(platform='win64-clang', target='ascompiler', config='Debug'),
	'win64-baker': dict(platform='win64', target='baker', config='Debug'),
	'win64-clang-baker': dict(platform='win64-clang', target='baker', config='Debug'),
	'unit-tests': dict(platform='linux', target='unit-tests', config='Debug', run_target='RunUnitTests'),
	'code-coverage': dict(platform='linux', target='code-coverage', config='Debug', compiler='gcc', run_target='RunCodeCoverage'),
}

FORMAT_PATTERNS = [
	'Applications/*.cpp',
	'Client/*.cpp', 'Client/*.h',
	'Common/*.cpp', 'Common/*.h',
	'Common/ImGuiExt/*.cpp', 'Common/ImGuiExt/*.h',
	'Essentials/*.cpp', 'Essentials/*.h',
	'Server/*.cpp', 'Server/*.h',
	'Tools/*.cpp', 'Tools/*.h',
	'Scripting/*.cpp',
	'Scripting/AngelScript/*.cpp', 'Scripting/AngelScript/*.h',
	'Scripting/AngelScript/CoreScripts/*.fos',
	'Frontend/*.cpp', 'Frontend/*.h',
	'Tests/*.cpp',
]
UTF8_BOM = b'\xef\xbb\xbf'

LINUX_PACKAGE_GROUPS = {
	'common-packages': (
		'7',
		['clang', 'clang-format', 'build-essential', 'git', 'cmake', 'python3', 'wget', 'unzip', 'binutils-dev'],
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
		'2',
		['android-sdk', 'openjdk-8-jdk', 'ant'],
	),
}


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


def resolve_workspace_emsdk_root(workspace: Path) -> str:
	emsdk_root = workspace / 'emsdk'
	return str(emsdk_root) if emsdk_root.is_dir() else ''


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

	if 'ANDROID_HOME' not in os.environ and Path('/usr/lib/android-sdk').is_dir():
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
		output = subprocess.check_output(
			[
				vswhere,
				'-latest',
				'-products',
				'*',
				'-requires',
				'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
				'-property',
				'installationPath',
			],
			text=True,
			stderr=subprocess.STDOUT,
		).strip()
		return bool(output)
	except Exception:
		return False


def check_host_tools(host_name: str) -> list[str]:
	issues: list[str] = []

	if host_name == 'linux':
		if not shutil.which('apt-get'):
			issues.append('Please use a Debian/Ubuntu host with apt-get available')
		if not is_running_as_root() and not shutil.which('sudo'):
			issues.append('Please install sudo or run the preparation script as root')
		if not shutil.which('cmake'):
			issues.append('Please install CMake')
		return issues

	if host_name == 'macos':
		try:
			subprocess.check_output(['xcode-select', '-p'], text=True, stderr=subprocess.STDOUT).strip()
		except Exception:
			issues.append('Please install Xcode from AppStore')

		if not resolve_macos_cmake():
			issues.append('Please install CMake from https://cmake.org')
		return issues

	if host_name == 'windows':
		if not has_windows_build_tools():
			issues.append('Please install Visual Studio with C++ Build Tools or Build Tools for Visual Studio')
		if not shutil.which('cmake'):
			issues.append('Please install CMake from https://cmake.org')
		if not (shutil.which('py') or shutil.which('python')):
			issues.append('Please install Python from https://www.python.org')
		return issues

	raise SystemExit(f'Unsupported host tools check: {host_name}')


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


def run_bash(command: str, cwd: str | Path | None = None, env: Mapping[str, str] | None = None) -> None:
	log('Run:', command)
	subprocess.check_call(['bash', '-lc', command], cwd=cwd, env=dict(env) if env is not None else None)


def run_windows_cmd(command: str, cwd: str | Path | None = None, env: Mapping[str, str] | None = None) -> None:
	log('Run:', command)
	subprocess.check_call(['cmd', '/d', '/c', command], cwd=cwd, env=dict(env) if env is not None else None)


def resolve_windows_build(platform_name: str) -> tuple[str, str | None]:
	if platform_name == 'win32':
		return 'Win32', None
	if platform_name == 'win64':
		return 'x64', None
	if platform_name == 'win32-clang':
		return 'Win32', 'ClangCL'
	if platform_name == 'win64-clang':
		return 'x64', 'ClangCL'
	raise SystemExit(f'Invalid Windows build platform: {platform_name}')


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


def prepare_toolset_workspace(env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	output = env['FO_OUTPUT']
	project_root = env['FO_PROJECT_ROOT']

	if os.name == 'nt':
		build_dir = workspace / 'build-win64-toolset'
		if build_dir.exists():
			remove_tree(build_dir)
		ensure_dir(build_dir)
		run([
			'cmake',
			'-G',
			'Visual Studio 17 2022',
			'-A',
			'x64',
			f'-DFO_OUTPUT_PATH={output}',
			'-DFO_BUILD_BAKER=1',
			'-DFO_BUILD_ASCOMPILER=1',
			'-DFO_UNIT_TESTS=0',
			project_root,
		], cwd=build_dir)
		return

	build_dir = workspace / 'build-linux-toolset'
	if build_dir.exists():
		remove_tree(build_dir)
	ensure_dir(build_dir)
	build_env = os.environ.copy()
	build_env['CC'] = '/usr/bin/clang'
	build_env['CXX'] = '/usr/bin/clang++'
	run([
		'cmake',
		'-G',
		'Unix Makefiles',
		f'-DFO_OUTPUT_PATH={output}',
		'-DCMAKE_BUILD_TYPE=Release',
		'-DFO_BUILD_CLIENT=0',
		'-DFO_BUILD_SERVER=0',
		'-DFO_BUILD_EDITOR=0',
		'-DFO_BUILD_MAPPER=0',
		'-DFO_BUILD_ASCOMPILER=1',
		'-DFO_BUILD_BAKER=1',
		'-DFO_UNIT_TESTS=0',
		'-DFO_CODE_COVERAGE=0',
		project_root,
	], cwd=build_dir, env=build_env)


def run_emsdk_command(emsdk_root: Path, *args: str) -> None:
	if os.name == 'nt':
		command = ['cmd', '/d', '/s', '/c', str(emsdk_root / 'emsdk.bat'), *args]
		log('Run:', ' '.join(str(part) for part in command))
		subprocess.check_call(command, cwd=emsdk_root)
	else:
		run([emsdk_root / 'emsdk', *args], cwd=emsdk_root)


def prepare_emscripten_workspace(env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	emsdk_root = workspace / 'emsdk'
	if emsdk_root.exists():
		remove_tree(emsdk_root)

	run(['git', 'clone', 'https://github.com/emscripten-core/emsdk.git', str(emsdk_root)])
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

	if archive_path.exists():
		archive_path.unlink()
	if ndk_source_dir.exists():
		remove_tree(ndk_source_dir)
	if ndk_target_dir.exists():
		remove_tree(ndk_target_dir)

	url = f'https://dl.google.com/android/repository/{archive_name}'
	log('Download Android NDK:', url)
	urllib.request.urlretrieve(url, archive_path)
	log('Unpack Android NDK:', archive_path)
	extract_zip_with_permissions(archive_path, workspace)
	shutil.move(str(ndk_source_dir), str(ndk_target_dir))
	archive_path.unlink(missing_ok=True)


def prepare_dotnet_workspace(env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	dotnet_root = workspace / 'dotnet'
	if dotnet_root.exists():
		remove_tree(dotnet_root)
	dotnet_root.mkdir(parents=True, exist_ok=True)
	run([
		'git',
		'clone',
		'https://github.com/dotnet/runtime.git',
		'--depth',
		'1',
		'--branch',
		build_dotnet_version(env),
		str(dotnet_root / 'runtime'),
	])


def prepare_workspace(parts: Sequence[str], check_only: bool, env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	output = Path(env['FO_OUTPUT'])
	ensure_dir(workspace)
	ensure_dir(output)

	part_versions = {
		'toolset': build_toolset_version(),
		'emscripten': build_emscripten_version(env),
		'android-ndk': build_android_ndk_version(env),
		'dotnet': build_dotnet_version(env),
	}
	part_actions = {
		'toolset': prepare_toolset_workspace,
		'emscripten': prepare_emscripten_workspace,
		'android-ndk': prepare_android_ndk_workspace,
		'dotnet': prepare_dotnet_workspace,
	}

	for part_name in parts:
		version = part_versions[part_name]
		if is_workspace_part_ready(workspace, part_name, version):
			log(f'Workspace part {part_name} is ready ({version})')
			continue

		if check_only:
			raise SystemExit(f'Workspace part {part_name} is not ready ({version})')

		log(f'Prepare workspace part {part_name} ({version})')
		part_actions[part_name](env)
		mark_workspace_part_ready(workspace, part_name, version)


def has_feature(features: Sequence[str], *names: str) -> bool:
	return any(feature in names for feature in features)


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

	if host_name == 'linux':
		selected = list(features) if features else ['all']
		if has_feature(selected, 'packages', 'all'):
			install_linux_packages('common-packages', workspace, check_only)
			if has_feature(selected, 'linux', 'all'):
				install_linux_packages('linux-packages', workspace, check_only)
			if has_feature(selected, 'web', 'all'):
				install_linux_packages('web-packages', workspace, check_only)
			if has_feature(selected, 'android', 'android-arm64', 'android-x86', 'all'):
				install_linux_packages('android-packages', workspace, check_only)

		if has_feature(selected, 'toolset', 'all'):
			prepare_workspace(['toolset'], check_only, env)
		if has_feature(selected, 'web', 'all'):
			prepare_workspace(['emscripten'], check_only, env)
		if has_feature(selected, 'android', 'android-arm64', 'android-x86', 'all'):
			prepare_workspace(['android-ndk'], check_only, env)
		if has_feature(selected, 'dotnet', 'all'):
			prepare_workspace(['dotnet'], check_only, env)
		return

	if host_name == 'windows':
		selected = list(features) if features else ['toolset']
		parts: list[str] = []
		if has_feature(selected, 'toolset', 'all'):
			parts.append('toolset')
		if has_feature(selected, 'web', 'all'):
			parts.append('emscripten')
		if parts:
			prepare_workspace(parts, check_only, env)
		return

	if host_name == 'macos':
		return

	raise SystemExit(f'Unsupported host workspace preparation: {host_name}')


def resolve_build_hash(env: Mapping[str, str]) -> str:
	project_root = Path(env['FO_PROJECT_ROOT'])
	result = subprocess.run(
		['git', 'rev-parse', 'HEAD'],
		cwd=project_root,
		capture_output=True,
		text=True,
	)
	assert result.returncode == 0, f'Failed to get git hash: {result.stderr.strip()}'
	build_hash = result.stdout.strip()
	assert build_hash, 'git rev-parse HEAD returned empty hash'
	return build_hash


def package_web_debug(env: Mapping[str, str]) -> None:
	project_root = Path(env['FO_PROJECT_ROOT'])
	output_root_input = Path(env['FO_OUTPUT'])
	output_root = Path(env['FO_WORKSPACE']) / 'web-debug'
	package_script = Path(env['FO_ENGINE_ROOT']) / 'BuildTools' / 'package.py'
	build_hash = resolve_build_hash(env)
	input_roots = [output_root_input, project_root]

	command = [
		sys.executable,
		str(package_script),
		'-maincfg',
		str(project_root / 'LastFrontier.fomain'),
		'-buildhash',
		build_hash,
		'-devname',
		'LF',
		'-nicename',
		'LastFrontier',
		'-target',
		'Client',
		'-platform',
		'Web',
		'-arch',
		'wasm',
		'-pack',
		'Raw+WebServer',
		'-config',
		'RemoteSceneLaunch',
		'-zip-compress-level',
		'1',
		'-output',
		str(output_root),
	]
	for input_root in input_roots:
		command.extend(['-input', str(input_root)])

	run(command)


def web_package_dir(env: Mapping[str, str]) -> Path:
	return Path(env['FO_WORKSPACE']) / 'web-debug' / 'LF-Client-RemoteSceneLaunch-Web'


def resolve_android_abi(platform_name: str) -> str:
	if platform_name == 'android':
		return 'armeabi-v7a'
	if platform_name == 'android-arm64':
		return 'arm64-v8a'
	if platform_name == 'android-x86':
		return 'x86'
	raise SystemExit(f'Invalid Android platform: {platform_name}')


def configure_build(platform_name: str, target_name: str, config: str, env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	output = env['FO_OUTPUT']
	project_root = env['FO_PROJECT_ROOT']
	build_dir = workspace / f'build-{platform_name}-{target_name}-{config}'
	ensure_dir(workspace)
	ensure_dir(output)
	ensure_dir(build_dir)
	ready_path = build_dir / 'READY'
	if ready_path.exists():
		ready_path.unlink()

	cmake_args = build_flag_args(target_name, config=config)

	if platform_name == 'linux':
		build_env = os.environ.copy()
		build_env['CC'] = '/usr/bin/clang'
		build_env['CXX'] = '/usr/bin/clang++'
		run(['cmake', '-G', 'Unix Makefiles', f'-DFO_OUTPUT_PATH={output}', *cmake_args, project_root], cwd=build_dir, env=build_env)
		run(['cmake', '--build', '.', '--config', config, '--parallel'], cwd=build_dir, env=build_env)
	elif platform_name == 'web':
		toolchain_file = Path(env['EMSDK']) / 'upstream' / 'emscripten' / 'cmake' / 'Modules' / 'Platform' / 'Emscripten.cmake'
		configure_cmd = [
			'cmake',
			'-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain_file),
			f'-DFO_OUTPUT_PATH={output}',
			*cmake_args,
		]
		if os.name == 'nt':
			configure_cmd[1:1] = ['-G', 'Ninja Multi-Config', f'-DCMAKE_MAKE_PROGRAM={discover_windows_ninja()}']
		else:
			configure_cmd[1:1] = ['-G', 'Unix Makefiles']
		configure_cmd.append(project_root)
		run_in_emsdk_env(configure_cmd, workspace, cwd=build_dir)
		run_in_emsdk_env(['cmake', '--build', '.', '--config', config, '--parallel'], workspace, cwd=build_dir)
	elif platform_name in ('android', 'android-arm64', 'android-x86'):
		android_abi = resolve_android_abi(platform_name)
		toolchain_file = f'{env["ANDROID_NDK_ROOT"]}/build/cmake/android.toolchain.cmake'
		toolchain_settings = [
			f'-DANDROID_ABI={android_abi}',
			f'-DANDROID_PLATFORM=android-{env["ANDROID_NATIVE_API_LEVEL_NUMBER"]}',
			'-DANDROID_STL=c++_static',
		]
		run(['cmake', '-G', 'Unix Makefiles', f'-DCMAKE_TOOLCHAIN_FILE={toolchain_file}', *toolchain_settings, f'-DFO_OUTPUT_PATH={output}', *cmake_args, project_root], cwd=build_dir)
		run(['cmake', '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
	elif platform_name in ('mac', 'ios'):
		cmake_bin = shutil.which('cmake') or '/Applications/CMake.app/Contents/bin/cmake'
		if platform_name == 'mac':
			run([cmake_bin, '-G', 'Xcode', f'-DFO_OUTPUT_PATH={output}', *build_flag_args(target_name), project_root], cwd=build_dir)
			run([cmake_bin, '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
		else:
			toolchain_settings = [
				'-DPLATFORM=SIMULATOR64',
				f'-DDEPLOYMENT_TARGET={env["FO_IOS_SDK"]}',
				'-DENABLE_BITCODE=0',
				'-DENABLE_ARC=0',
				'-DENABLE_VISIBILITY=0',
				'-DENABLE_STRICT_TRY_COMPILE=0',
			]
			run([cmake_bin, '-G', 'Xcode', f'-DCMAKE_TOOLCHAIN_FILE={Path(env["FO_ENGINE_ROOT"]) / "BuildTools" / "ios.toolchain.cmake"}', *toolchain_settings, f'-DFO_OUTPUT_PATH={output}', *build_flag_args(target_name), project_root], cwd=build_dir)
			run([cmake_bin, '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
	elif platform_name.startswith('win'):
		arch, toolset = resolve_windows_build(platform_name)
		configure_cmd = ['cmake', '-A', arch]
		if toolset:
			configure_cmd += ['-T', toolset]
		configure_cmd += [f'-DFO_OUTPUT_PATH={output}', *build_flag_args(target_name), project_root]
		run(configure_cmd, cwd=build_dir)
		run(['cmake', '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
	else:
		raise SystemExit(f'Invalid build platform: {platform_name}')

	ready_path.touch()


def prepare_validation_project(env: Mapping[str, str]) -> Path:
	workspace = Path(env['FO_WORKSPACE'])
	validation_root = workspace / 'validation-project'
	source_root = Path(env['FO_ENGINE_ROOT']) / 'BuildTools' / 'validation-project'
	ensure_dir(workspace)
	shutil.copytree(source_root, validation_root, dirs_exist_ok=True)
	engine_link = validation_root / 'Engine'
	if not engine_link.exists():
		if os.name == 'nt':
			run(['cmd', '/c', 'mklink', '/d', str(engine_link), env['FO_ENGINE_ROOT']])
		else:
			engine_link.symlink_to(Path(env['FO_ENGINE_ROOT']), target_is_directory=True)
	return validation_root


def run_validation(name: str, env: Mapping[str, str]) -> None:
	if name not in VALIDATION_TARGETS:
		raise SystemExit(f'Invalid validation target: {name}')
	validation: ValidationMap = VALIDATION_TARGETS[name]
	validation_root = prepare_validation_project(env)
	platform_name = validation['platform']
	target_name = validation['target']
	config = validation['config']
	workspace = Path(env['FO_WORKSPACE'])
	build_dir = workspace / f'validate-{name}'
	ensure_dir(build_dir)
	cmake_args = build_flag_args(target_name, config=config)

	if platform_name == 'linux':
		build_env = os.environ.copy()
		if validation.get('compiler') == 'gcc':
			build_env['CC'] = '/usr/bin/gcc'
			build_env['CXX'] = '/usr/bin/g++'
		else:
			build_env['CC'] = '/usr/bin/clang'
			build_env['CXX'] = '/usr/bin/clang++'
		run(['cmake', '-G', 'Unix Makefiles', *cmake_args, str(validation_root)], cwd=build_dir, env=build_env)
		run(['cmake', '--build', '.', '--config', config, '--parallel'], cwd=build_dir, env=build_env)
	elif platform_name == 'web':
		toolchain_file = Path(env['EMSDK']) / 'upstream' / 'emscripten' / 'cmake' / 'Modules' / 'Platform' / 'Emscripten.cmake'
		configure_cmd = ['cmake', '-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain_file), *cmake_args]
		if os.name == 'nt':
			configure_cmd[1:1] = ['-G', 'Ninja Multi-Config', f'-DCMAKE_MAKE_PROGRAM={discover_windows_ninja()}']
		else:
			configure_cmd[1:1] = ['-G', 'Unix Makefiles']
		configure_cmd.append(str(validation_root))
		run_in_emsdk_env(configure_cmd, workspace, cwd=build_dir)
		run_in_emsdk_env(['cmake', '--build', '.', '--config', config, '--parallel'], workspace, cwd=build_dir)
	elif platform_name in ('android', 'android-arm64', 'android-x86'):
		android_abi = resolve_android_abi(platform_name)
		toolchain_file = f'{env["ANDROID_NDK_ROOT"]}/build/cmake/android.toolchain.cmake'
		toolchain_settings = [
			f'-DANDROID_ABI={android_abi}',
			f'-DANDROID_PLATFORM=android-{env["ANDROID_NATIVE_API_LEVEL_NUMBER"]}',
			'-DANDROID_STL=c++_static',
		]
		run(['cmake', '-G', 'Unix Makefiles', f'-DCMAKE_TOOLCHAIN_FILE={toolchain_file}', *toolchain_settings, *cmake_args, str(validation_root)], cwd=build_dir)
		run(['cmake', '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
	elif platform_name in ('mac', 'ios'):
		cmake_bin = shutil.which('cmake') or '/Applications/CMake.app/Contents/bin/cmake'
		if platform_name == 'mac':
			run([cmake_bin, '-G', 'Xcode', *build_flag_args(target_name), str(validation_root)], cwd=build_dir)
			run([cmake_bin, '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
		else:
			toolchain_settings = [
				'-DPLATFORM=SIMULATOR64',
				f'-DDEPLOYMENT_TARGET={env["FO_IOS_SDK"]}',
				'-DENABLE_BITCODE=0',
				'-DENABLE_ARC=0',
				'-DENABLE_VISIBILITY=0',
				'-DENABLE_STRICT_TRY_COMPILE=0',
			]
			run([cmake_bin, '-G', 'Xcode', f'-DCMAKE_TOOLCHAIN_FILE={Path(env["FO_ENGINE_ROOT"]) / "BuildTools" / "ios.toolchain.cmake"}', *toolchain_settings, *build_flag_args(target_name), str(validation_root)], cwd=build_dir)
			run([cmake_bin, '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
	elif platform_name.startswith('win'):
		arch, toolset = resolve_windows_build(platform_name)
		configure_cmd = ['cmake', '-A', arch]
		if toolset:
			configure_cmd += ['-T', toolset]
		configure_cmd += [*build_flag_args(target_name), str(validation_root)]
		run(configure_cmd, cwd=build_dir)
		run(['cmake', '--build', '.', '--config', config, '--parallel'], cwd=build_dir)
	else:
		raise SystemExit(f'Unsupported validation platform: {platform_name}')

	run_target = validation.get('run_target')
	if run_target:
		log('Run', run_target)
		run(['cmake', '--build', '.', '--config', config, '--target', run_target, '--parallel'], cwd=build_dir)

		if name == 'code-coverage' and os.environ.get('CODECOV_TOKEN'):
			codecov_path = build_dir / 'codecov'
			if codecov_path.exists():
				codecov_path.unlink()
			run(['curl', '-Os', 'https://uploader.codecov.io/latest/linux/codecov'], cwd=build_dir)
			codecov_path.chmod(codecov_path.stat().st_mode | 0o111)
			run([str(codecov_path), '-t', os.environ['CODECOV_TOKEN'], '--gcov'], cwd=build_dir)


def setup_mono(os_name: str, arch: str, config: str, env: Mapping[str, str]) -> None:
	triplet = f'{os_name}.{arch}.{config}'
	workspace = Path(env['FO_WORKSPACE'])
	runtime_root = workspace / 'runtime'
	clone_marker = workspace / 'CLONED'
	built_marker = workspace / f'BUILT_{triplet}'
	ready_marker = workspace / f'READY_{triplet}'

	if not clone_marker.exists():
		if runtime_root.exists():
			log('Remove previous repository')
			remove_tree(runtime_root)
		dotnet_runtime_root = env.get('FO_DOTNET_RUNTIME_ROOT', '')
		if dotnet_runtime_root:
			log('Copy runtime')
			shutil.copytree(dotnet_runtime_root, runtime_root)
		else:
			log('Clone runtime')
			run(['git', 'clone', 'https://github.com/dotnet/runtime.git', '--depth', '1', '--branch', env['FO_DOTNET_RUNTIME'], str(runtime_root)])
		clone_marker.touch()

	if not built_marker.exists():
		log('Build runtime')
		build_script = 'build.cmd' if os.name == 'nt' else './build.sh'
		run([build_script, '-os', os_name, '-arch', arch, '-c', config, '-subset', 'mono.runtime'], cwd=runtime_root)
		built_marker.touch()

	if not ready_marker.exists():
		output_dir = workspace / 'output' / 'mono' / triplet
		input_dir = runtime_root / 'artifacts' / 'obj' / 'mono' / triplet / 'out'
		if not input_dir.is_dir():
			raise SystemExit(f'Files not found: {input_dir}')
		log('Copy from', input_dir, 'to', output_dir)
		ensure_dir(output_dir)
		shutil.copytree(input_dir, output_dir, dirs_exist_ok=True)
		ready_marker.touch()

	log(f'Runtime {triplet} is ready!')



def discover_clang_format() -> str:
	for executable in ('clang-format-20', 'clang-format'):
		path = shutil.which(executable)
		if path:
			return path
	raise SystemExit('clang-format not found')


def read_text_strip_bom(path: Path) -> tuple[str, bool]:
	data = path.read_bytes()
	has_bom = data.startswith(UTF8_BOM)
	return data.decode('utf-8-sig'), has_bom


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
		formatted = strip_text_bom(subprocess.check_output([clang_format, str(path)], text=True, encoding='utf-8'))
		if original == formatted and not has_bom:
			continue

		changed += 1
		write_text_utf8(path, formatted)

	progress.finish(f'Completed, changed {changed} file(s)')
	return changed


def format_source(env: Mapping[str, str]) -> None:
	clang_format = discover_clang_format()
	format_files(clang_format, Path(env['FO_ENGINE_ROOT']) / 'Source', FORMAT_PATTERNS)


def build_toolset(target: str, env: Mapping[str, str]) -> None:
	workspace = Path(env['FO_WORKSPACE'])
	build_dir = workspace / ('build-win64-toolset' if os.name == 'nt' else 'build-linux-toolset')
	if not build_dir.is_dir():
		raise SystemExit(f'Toolset build directory not found: {build_dir}')
	run(['cmake', '--build', '.', '--config', 'Release', '--target', target, '--parallel'], cwd=build_dir)


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
	prepare_parser.add_argument('parts', nargs='+', choices=['toolset', 'emscripten', 'android-ndk', 'dotnet'])
	prepare_parser.add_argument('--check', action='store_true')

	package_web_parser = subparsers.add_parser('package-web-debug', help='package the local web debug client')
	package_web_parser.set_defaults(no_args=True)

	host_check_parser = subparsers.add_parser('host-check', help='check host prerequisites')
	host_check_parser.add_argument('host', choices=['linux', 'macos', 'windows'])

	prepare_host_parser = subparsers.add_parser('prepare-host-workspace', help='prepare host workspace and prerequisites')
	prepare_host_parser.add_argument('host', choices=['linux', 'windows', 'macos'])
	prepare_host_parser.add_argument('features', nargs='*', choices=['packages', 'linux', 'web', 'android', 'android-arm64', 'android-x86', 'toolset', 'dotnet', 'all'])
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
		package_web_debug(env)
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
