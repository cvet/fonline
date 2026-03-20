#!/usr/bin/python3

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
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


def log(*parts: object) -> None:
	print('[BuildTools]', *parts, flush=True)


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


def run(cmd: Sequence[object], cwd: str | Path | None = None, env: Mapping[str, str] | None = None) -> None:
	log('Run:', ' '.join(str(part) for part in cmd))
	subprocess.check_call([str(part) for part in cmd], cwd=cwd, env=dict(env) if env is not None else None)


def run_bash(command: str, cwd: str | Path | None = None, env: Mapping[str, str] | None = None) -> None:
	log('Run:', command)
	subprocess.check_call(['bash', '-lc', command], cwd=cwd, env=dict(env) if env is not None else None)


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
	return ' '.join(args)


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
		emscripten_env = workspace / 'emsdk' / 'emsdk_env.sh'
		toolchain_file = '$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'
		cmake_args_str = join_shell_args(cmake_args)
		command = (
			f'source "{emscripten_env}" >/dev/null && '
			f'cmake -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE="{toolchain_file}" '
			f'-DFO_OUTPUT_PATH="{output}" {cmake_args_str} "{project_root}" && '
			f'cmake --build . --config {config} --parallel'
		)
		run_bash(command, cwd=build_dir)
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
		emscripten_env = workspace / 'emsdk' / 'emsdk_env.sh'
		toolchain_file = '$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'
		cmake_args_str = join_shell_args(cmake_args)
		command = (
			f'source "{emscripten_env}" >/dev/null && '
			f'cmake -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE="{toolchain_file}" {cmake_args_str} "{validation_root}" && '
			f'cmake --build . --config {config} --parallel'
		)
		run_bash(command, cwd=build_dir)
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
			shutil.rmtree(runtime_root)
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


def format_source(env: Mapping[str, str]) -> None:
	clang_format = discover_clang_format()
	source_root = Path(env['FO_ENGINE_ROOT']) / 'Source'
	files: list[Path] = []
	for pattern in FORMAT_PATTERNS:
		files.extend(sorted(source_root.glob(pattern)))
	if not files:
		log('No files matched format patterns')
		return

	total = len(files)
	for index, path in enumerate(files, start=1):
		rel_path = path.relative_to(source_root).as_posix().replace('/', '\\')
		print(f'Formatting [{index}/{total}] {rel_path}', flush=True)
		subprocess.check_call([clang_format, '-i', str(path)])


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

	return parser


def main() -> None:
	parser = create_parser()
	args = parser.parse_args()
	env = resolve_env()
	apply_env(env)

	if args.command == 'env':
		emit_shell_env(env, args.shell)
		if args.summary:
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
	raise SystemExit(f'Unsupported command: {args.command}')


if __name__ == '__main__':
	main()
