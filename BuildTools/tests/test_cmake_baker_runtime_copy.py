from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
APPLICATIONS_STAGE = (ENGINE / 'BuildTools/cmake/stages/Applications.cmake').read_text(encoding='utf-8')
BAKER_COPY = '$<TARGET_FILE:Probe_BakerLib>'
PROJECT_HEAD = r'''cmake_minimum_required(VERSION 3.22)
project(BakerRuntimeCopyProbe NONE)
include("@ENGINE@/BuildTools/Init.cmake")

# Record target commands instead of evaluating them, so configure needs no compiler
function(add_custom_command)
    cmake_parse_arguments(ARG "POST_BUILD;VERBATIM" "TARGET;COMMENT" "COMMAND" ${ARGN})
    if(ARG_TARGET)
        file(APPEND "${CMAKE_BINARY_DIR}/post-build.txt" "${ARG_TARGET}|${ARG_COMMAND}\n")
    endif()
endfunction()
function(target_link_options)
endfunction()
macro(AddExecutableApplication target source)
    add_custom_target(${target})
    file(APPEND "${CMAKE_BINARY_DIR}/targets.txt" "${target}\n")
endmacro()
macro(AddSharedApplication target source)
    add_custom_target(${target})
    file(APPEND "${CMAKE_BINARY_DIR}/targets.txt" "${target}\n")
endmacro()

set(FO_DEV_NAME Probe)
set(FO_ENGINE_ROOT Engine)
# The native resource-pack hash tool is the only host-compiled part of the stage
set(CMAKE_CROSSCOMPILING ON)
@OPTIONS@

@HOOKS@

# Stages without a part in the runtime copies are marked executed; Applications and Finalize run for real
set(FO_STAGES_EXECUTED Init ProjectOptions ThirdParty EngineSources Codegen CoreLibs)
BuildApplications()
list(APPEND FO_STAGES_EXECUTED ScriptsAndBaking Packages)
FinalizeProjectGeneration()
'''
ALL_ROLES = '''set(FO_BUILD_CLIENT ON)
set(FO_BUILD_SERVER ON)
set(FO_BUILD_MAPPER ON)
set(FO_BUILD_ASCOMPILER ON)
set(FO_BUILD_BAKER ON)
set(FO_MANAGED_SCRIPTING ON)
set(FO_BUILD_BAKER_LIB ON)
'''
PROJECT_RUNTIME_HOOK = '''add_custom_target(MyGamePlugin)
set(MYGAME_VENDOR_RUNTIME "${CMAKE_CURRENT_SOURCE_DIR}/vendor/runtime.bin")

function(_mygame_copy_runtimes)
    CopyRuntimeToTarget(${FO_DEV_NAME}_Client "${MYGAME_VENDOR_RUNTIME}")
    CopyTargetRuntimeToTarget(${FO_DEV_NAME}_Server MyGamePlugin)
endfunction()

AddStageHook(Finalize Pre _mygame_copy_runtimes)
'''
pytestmark = pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')


def prebaking_executables() -> set[str]:
    # Entry points whose InitApp call requests a startup prebake, mapped to the executables built from them
    applications = ENGINE / 'Source/Applications'
    prebaking_sources = {
        path.stem for path in applications.glob('*.cpp')
        if 'AppInitFlags::PrebakeResources' in path.read_text(encoding='utf-8')
    }
    declarations = re.findall(
        r'(AddExecutableApplication|AddSharedApplication)\(\s*\$\{FO_DEV_NAME\}_(\w+)\s+'
        r'"\$\{FO_ENGINE_ROOT\}/Source/Applications/(\w+)\.cpp"(.*?)\)',
        APPLICATIONS_STAGE, re.DOTALL)
    output_dirs: dict[str, set[str]] = {'AddExecutableApplication': set(), 'AddSharedApplication': set()}
    executables = set()
    for kind, name, source, arguments in declarations:
        if source not in prebaking_sources:
            continue
        output_dir = re.search(r'OUTPUT_DIR\s+(\S+)', arguments)
        assert output_dir is not None, name
        output_dirs[kind].add(output_dir.group(1))
        if kind == 'AddExecutableApplication':
            executables.add(name)
    # A prebaking runtime module resolves the baker beside its host executable, not beside itself
    assert output_dirs['AddSharedApplication'] <= output_dirs['AddExecutableApplication']
    assert {'Client', 'Server', 'ServerHeadless'} <= executables
    return executables


def configure(tmp_path: Path, options: str, hooks: str = '') -> subprocess.CompletedProcess[str]:
    source = tmp_path.resolve() / 'project'
    source.mkdir()
    (source / 'vendor').mkdir()
    (source / 'vendor/runtime.bin').write_bytes(b'runtime')
    (source / 'CMakeLists.txt').write_text(
        PROJECT_HEAD.replace('@ENGINE@', ENGINE.as_posix()).replace('@OPTIONS@', options).replace('@HOOKS@', hooks),
        encoding='utf-8')
    arguments = ['cmake', '-S', str(source), '-B', str(source / 'build')]
    if shutil.which('ninja') is not None:
        arguments += ['-G', 'Ninja']
    environment = {key: value for key, value in os.environ.items() if not key.startswith('FO_')}
    return subprocess.run(arguments, text=True, encoding='utf-8', capture_output=True, timeout=120, env=environment)


def recorded(tmp_path: Path, name: str) -> list[str]:
    path = tmp_path.resolve() / 'project/build' / name
    return path.read_text(encoding='utf-8').splitlines() if path.exists() else []


def baker_copy_consumers(tmp_path: Path) -> list[str]:
    consumers = []
    for line in recorded(tmp_path, 'post-build.txt'):
        target, command = line.split('|', 1)
        if BAKER_COPY in command.split(';'):
            assert command.split(';')[1:3] == ['-E', 'copy_if_different'], line
            assert f'$<TARGET_FILE_DIR:{target}>' in command.split(';'), line
            consumers.append(target)
    return consumers


@pytest.mark.parametrize(('platform', 'options'), (
    pytest.param('windows', 'set(FO_WINDOWS ON)\n' + ALL_ROLES, id='windows'),
    pytest.param('linux', 'set(FO_LINUX ON)\n' + ALL_ROLES, id='linux'),
))
def test_stage_copies_baker_next_to_exactly_the_prebaking_executables(tmp_path: Path, platform: str, options: str) -> None:
    result = configure(tmp_path, options)
    assert result.returncode == 0, result.stdout + result.stderr

    targets = set(recorded(tmp_path, 'targets.txt'))
    expected = {f'Probe_{name}' for name in prebaking_executables()} & targets
    consumers = baker_copy_consumers(tmp_path)
    assert 'Probe_BakerLib' in targets
    assert sorted(consumers) == sorted(expected)
    assert {'Probe_Client', 'Probe_ClientHeadless', 'Probe_Server', 'Probe_ServerHeadless'} <= expected
    assert ('Probe_ServerService' in expected) == (platform == 'windows')
    assert ('Probe_ServerDaemon' in expected) == (platform == 'linux')
    for tool in ('Mapper', 'AnimationViewer', 'ParticleViewer', 'ASCompiler', 'ManagedScriptBaker', 'Baker', 'BakerLib'):
        assert f'Probe_{tool}' in targets
        assert f'Probe_{tool}' not in consumers


@pytest.mark.parametrize('options', (
    'set(FO_WINDOWS ON)\nset(FO_BUILD_CLIENT ON)\nset(FO_BUILD_SERVER ON)\n',
    'set(FO_WEB ON)\nset(FO_BUILD_CLIENT ON)\nset(FO_BUILD_BAKER ON)\n',
    'set(FO_ANDROID ON)\nset(FO_BUILD_LIBRARY ON)\nset(FO_BUILD_CLIENT ON)\nset(FO_BUILD_BAKER ON)\n',
), ids=('no-baker', 'web', 'library-client'))
def test_stage_adds_no_copy_without_a_loadable_baker_or_executable_host(tmp_path: Path, options: str) -> None:
    result = configure(tmp_path, options)
    assert result.returncode == 0, result.stdout + result.stderr

    targets = set(recorded(tmp_path, 'targets.txt'))
    assert 'Probe_Client' in targets
    assert ('Probe_BakerLib' in targets) == ('FO_BUILD_LIBRARY' in options)
    assert baker_copy_consumers(tmp_path) == []


def test_project_finalize_hook_adds_its_own_runtime_copies(tmp_path: Path) -> None:
    result = configure(tmp_path, 'set(FO_WINDOWS ON)\n' + ALL_ROLES, PROJECT_RUNTIME_HOOK)
    assert result.returncode == 0, result.stdout + result.stderr

    commands = [line.split('|', 1) for line in recorded(tmp_path, 'post-build.txt')]
    vendor_runtime = (tmp_path.resolve() / 'project/vendor/runtime.bin').as_posix()
    assert any(target == 'Probe_Client' and vendor_runtime in command.split(';') for target, command in commands)
    assert any(target == 'Probe_Server' and '$<TARGET_FILE:MyGamePlugin>' in command.split(';')
               for target, command in commands)
    assert 'Probe_Server' in baker_copy_consumers(tmp_path)


def test_project_side_baker_copy_fails_configure_explicitly(tmp_path: Path) -> None:
    hooks = '''function(_mygame_post_target_setup)
    CopyTargetRuntimeToTarget(${FO_DEV_NAME}_Server ${FO_DEV_NAME}_BakerLib)
endfunction()

AddStageHook(Finalize Pre _mygame_post_target_setup)
'''
    result = configure(tmp_path, 'set(FO_WINDOWS ON)\n' + ALL_ROLES, hooks)
    output = re.sub(r'\s+', ' ', result.stdout + result.stderr)
    assert result.returncode != 0
    assert ('CopyTargetRuntimeToTarget: the Applications stage already copies Probe_BakerLib next to '
            'Probe_Server; remove the project-side copy') in output


def test_repeated_project_runtime_pair_fails_configure_explicitly(tmp_path: Path) -> None:
    hooks = PROJECT_RUNTIME_HOOK + '''
function(_mygame_copy_again)
    CopyTargetRuntimeToTarget(${FO_DEV_NAME}_Server MyGamePlugin)
endfunction()

AddStageHook(Finalize Pre _mygame_copy_again)
'''
    result = configure(tmp_path, 'set(FO_WINDOWS ON)\n' + ALL_ROLES, hooks)
    output = re.sub(r'\s+', ' ', result.stdout + result.stderr)
    assert result.returncode != 0
    assert ('CopyTargetRuntimeToTarget: MyGamePlugin is already copied next to Probe_Server; '
            'remove the duplicate call') in output


def test_documented_project_hook_is_the_configured_one() -> None:
    hook = PROJECT_RUNTIME_HOOK.split('\n\n', 1)[1]
    for locale in ('en', 'ru'):
        page = (ENGINE / f'Docs/{locale}/how-to/build/embedding-project.md').read_text(encoding='utf-8')
        assert '```cmake\n' + hook + '```' in page.replace('\r\n', '\n'), locale
