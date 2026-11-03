from __future__ import annotations

from pathlib import Path
import os
import re
import shutil
import subprocess
import sys

import pytest


ENGINE = Path(__file__).resolve().parents[2]
INIT = (ENGINE / 'BuildTools/cmake/stages/Init.cmake').read_text(encoding='utf-8')
INIT_HASH_REGION = INIT.split('# Evaluate build hash\n', 1)[1].split('\nSetValue(FO_CODEGEN_SCRIPT', 1)[0]
GIT_COMMIT = (
    'git', '-c', 'user.name=Build Hash Test', '-c',
    'user.email=build-hash@example.invalid', 'commit', '--allow-empty', '-m',
)
PROBE_SOURCE = r'''#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char**)
{
    std::cout << FO_BUILD_HASH;
    if (argc > 1) {
        std::filesystem::create_directories("Baking");
        std::ofstream("Baking/artifact.build-hash") << FO_BUILD_HASH;
        std::ofstream("Baking/Resources.build-hash") << FO_BUILD_HASH;
    }
    return 0;
}
'''
FIXTURE_CMAKE_HEAD = r'''cmake_minimum_required(VERSION 3.22)
project(BuildHashMarkerTest CXX)
macro(SetValue name)
    set(${name} ${ARGN})
endmacro()
macro(ParseArguments prefix options single multiple)
    cmake_parse_arguments(${prefix} "${options}" "${single}" "${multiple}" ${ARGN})
endmacro()
macro(GetTargetProperty variable target property)
    get_target_property(${variable} ${target} ${property})
endmacro()
macro(AddCustomCommand)
    add_custom_command(${ARGV})
endmacro()
macro(ExecuteProcess)
    execute_process(${ARGV})
endmacro()
macro(StatusMessage)
    message(STATUS ${ARGV})
endmacro()
macro(StringRandom result)
    string(RANDOM ${ARGN} ${result})
endmacro()
set(FO_DEV_NAME Probe)
set(FO_MAIN_CONFIG Probe.fomain)
set(FO_ENGINE_ROOT "Engine")
set(FO_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${FO_OUTPUT_PATH}")
add_custom_target(ForceCodeGeneration)
add_executable(Probe_Baker probe.cpp)
target_compile_features(Probe_Baker PRIVATE cxx_std_17)
set_target_properties(Probe_Baker PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FO_OUTPUT_PATH}")
include("${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/helpers/Build.cmake")
if(DEFINED FO_BUILD_HASH AND NOT "${FO_BUILD_HASH}" STREQUAL "")
    # Caller-supplied pin: keep it for the probe and all generated markers
else()
'''
FIXTURE_CMAKE_TAIL = r'''endif()
file(APPEND "${CMAKE_BINARY_DIR}/configure-log.txt" "${FO_BUILD_HASH}\n")
target_compile_definitions(Probe_Baker PRIVATE "FO_BUILD_HASH=\"${FO_BUILD_HASH}\"")
WriteBuildHash(Probe_Baker)
function(AddCommandTarget name)
    cmake_parse_arguments(ARG "COMMAND_ARGS" "" "" ${ARGN})
    add_custom_target(${name} ${ARG_UNPARSED_ARGUMENTS})
endfunction()
include("${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/stages/ScriptsAndBaking.cmake")
'''
pytestmark = pytest.mark.skipif(
    shutil.which('cmake') is None or shutil.which('git') is None,
    reason='CMake and Git are required',
)


def run(*args: str, cwd: Path, timeout: int = 60, env: dict[str, str] | None = None) -> str:
    environment = os.environ.copy()
    if env is not None:
        environment.update(env)
    return subprocess.check_output(args, cwd=cwd, text=True, stderr=subprocess.STDOUT,
                                   timeout=timeout, env=environment).strip()


def git_environment(path: Path) -> dict[str, str]:
    # Keep a fixture from discovering a repository above its own root
    return {'GIT_CEILING_DIRECTORIES': str(path.parent)}


def write_fixture(source: Path) -> None:
    # The staged Engine has no .git: only the project's own HEAD may trigger reconfiguration
    fixture_engine = source / 'Engine'
    for relative in (
        'BuildTools/cmake/helpers/Build.cmake',
        'BuildTools/cmake/helpers/EngineVersion.cmake',
        'BuildTools/cmake/helpers/WriteBuildHash.cmake',
        'BuildTools/cmake/stages/ScriptsAndBaking.cmake',
    ):
        destination = fixture_engine / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ENGINE / relative, destination)
    (source / 'Probe.fomain').write_text('', encoding='utf-8')
    (source / 'probe.cpp').write_text(PROBE_SOURCE, encoding='utf-8')
    (source / 'CMakeLists.txt').write_text(
        FIXTURE_CMAKE_HEAD + INIT_HASH_REGION + FIXTURE_CMAKE_TAIL, encoding='utf-8')


def configure(source: Path, environment: dict[str, str], *, pinned: str | None = None) -> Path:
    build = source / 'build'
    arguments = ['cmake', '-S', str(source), '-B', str(build)]
    if pinned is not None:
        arguments.append('-DFO_BUILD_HASH=' + pinned)
    run(*arguments, cwd=source, env=environment)
    return build


def build_target(source: Path, build: Path, target: str, environment: dict[str, str]) -> None:
    run('cmake', '--build', str(build), '--config', 'Release', '--target', target,
        cwd=source, timeout=120, env=environment)


def configure_hashes(build: Path) -> list[str]:
    return (build / 'configure-log.txt').read_text(encoding='utf-8').splitlines()


def compiled_hash(source: Path, environment: dict[str, str]) -> str:
    output = source / 'output'
    executable = next(output.rglob('Probe_Baker.exe' if os.name == 'nt' else 'Probe_Baker'))
    return run(str(executable), cwd=source, timeout=10, env=environment)


def assert_markers(source: Path, target: str, expected: str) -> None:
    output = source / 'output'
    marker = next(output.rglob('Probe_Baker.build-hash')).read_text(encoding='utf-8').strip()
    assert marker == expected
    if target != 'Probe_Baker':
        assert (output / 'Baking/artifact.build-hash').read_text(encoding='utf-8').strip() == expected
        assert (output / 'Baking/Resources.build-hash').read_text(encoding='utf-8').strip() == expected


@pytest.mark.parametrize('target', ('Probe_Baker', 'BakeResources', 'ForceBakeResources'))
@pytest.mark.parametrize('revision', ('unchanged', 'advanced', 'pinned', 'no-git'))
@pytest.mark.parametrize('directory', ('source',))
def test_markers_match_compiled_revision_after_head_advances(
    tmp_path: Path, target: str, revision: str, directory: str,
) -> None:
    source = tmp_path / directory
    source.mkdir()
    environment = git_environment(source)
    configured_hash = None
    if revision != 'no-git':
        run('git', 'init', cwd=source, env=environment)
        run(*GIT_COMMIT, 'configured revision', cwd=source, env=environment)
        configured_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    write_fixture(source)
    pinned_hash = None
    if revision == 'pinned':
        pinned_hash = 'b' * 40
        assert pinned_hash != configured_hash
    build = configure(source, environment, pinned=pinned_hash)
    expected_hash = configured_hash
    expected_configures = [configured_hash]
    if revision == 'advanced':
        run(*GIT_COMMIT, 'revision after configure', cwd=source, env=environment)
        expected_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
        assert expected_hash != configured_hash
        expected_configures.append(expected_hash)
    elif revision == 'pinned':
        run(*GIT_COMMIT, 'revision after pin', cwd=source, env=environment)
        expected_hash = pinned_hash
        expected_configures = [pinned_hash]
    build_target(source, build, target, environment)
    compiled = compiled_hash(source, environment)
    if revision == 'no-git':
        assert re.fullmatch(r'[0-9a-f]{40}-random', compiled)
        assert configure_hashes(build) == [compiled]
    else:
        assert compiled == expected_hash
        assert configure_hashes(build) == expected_configures
    assert_markers(source, target, compiled)


def test_markers_track_head_in_source_with_spaces(tmp_path: Path) -> None:
    source = tmp_path / 'source with spaces'
    source.mkdir()
    environment = git_environment(source)
    run('git', 'init', cwd=source, env=environment)
    run(*GIT_COMMIT, 'configured revision', cwd=source, env=environment)
    configured_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    write_fixture(source)
    build = configure(source, environment)
    run(*GIT_COMMIT, 'revision after configure', cwd=source, env=environment)
    advanced_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    assert advanced_hash != configured_hash
    build_target(source, build, 'BakeResources', environment)
    compiled = compiled_hash(source, environment)
    assert compiled == advanced_hash
    assert configure_hashes(build) == [configured_hash, advanced_hash]
    assert_markers(source, 'BakeResources', compiled)


def test_head_switch_without_branch_advance_reconfigures(tmp_path: Path) -> None:
    source = tmp_path / 'source'
    source.mkdir()
    environment = git_environment(source)
    run('git', 'init', cwd=source, env=environment)
    run(*GIT_COMMIT, 'configured revision', cwd=source, env=environment)
    configured_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    configured_branch = run('git', 'symbolic-ref', '--short', 'HEAD', cwd=source, env=environment)
    run('git', 'checkout', '-b', 'other', cwd=source, env=environment)
    run(*GIT_COMMIT, 'other branch revision', cwd=source, env=environment)
    other_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    assert other_hash != configured_hash
    run('git', 'checkout', configured_branch, cwd=source, env=environment)
    assert run('git', 'rev-parse', 'HEAD', cwd=source, env=environment) == configured_hash
    write_fixture(source)
    build = configure(source, environment)
    run('git', 'checkout', 'other', cwd=source, env=environment)
    assert run('git', 'rev-parse', 'HEAD', cwd=source, env=environment) == other_hash
    build_target(source, build, 'Probe_Baker', environment)
    assert configure_hashes(build) == [configured_hash, other_hash]
    assert compiled_hash(source, environment) == other_hash
    assert_markers(source, 'Probe_Baker', other_hash)


def test_packed_branch_ref_transitions_reconfigure(tmp_path: Path) -> None:
    source = tmp_path / 'source'
    source.mkdir()
    environment = git_environment(source)
    run('git', 'init', cwd=source, env=environment)
    run(*GIT_COMMIT, 'configured revision', cwd=source, env=environment)
    configured_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    write_fixture(source)
    build = configure(source, environment)
    build_target(source, build, 'Probe_Baker', environment)
    assert configure_hashes(build) == [configured_hash]
    assert compiled_hash(source, environment) == configured_hash
    run('git', 'pack-refs', '--all', '--prune', cwd=source, env=environment)
    build_target(source, build, 'Probe_Baker', environment)
    assert configure_hashes(build) == [configured_hash, configured_hash]
    assert compiled_hash(source, environment) == configured_hash
    run(*GIT_COMMIT, 'revision after packed baseline', cwd=source, env=environment)
    advanced_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    assert advanced_hash != configured_hash
    build_target(source, build, 'Probe_Baker', environment)
    assert configure_hashes(build) == [configured_hash, configured_hash, advanced_hash]
    assert compiled_hash(source, environment) == advanced_hash
    assert_markers(source, 'Probe_Baker', advanced_hash)
    run('git', 'pack-refs', '--all', '--prune', cwd=source, env=environment)
    build_target(source, build, 'Probe_Baker', environment)
    assert configure_hashes(build) == [configured_hash, configured_hash, advanced_hash, advanced_hash]
    assert compiled_hash(source, environment) == advanced_hash
    assert_markers(source, 'Probe_Baker', advanced_hash)


def test_linked_worktree_tracks_head_with_git_file(tmp_path: Path) -> None:
    main = tmp_path / 'main'
    main.mkdir()
    main_environment = git_environment(main)
    run('git', 'init', cwd=main, env=main_environment)
    run(*GIT_COMMIT, 'main baseline', cwd=main, env=main_environment)
    configured_hash = run('git', 'rev-parse', 'HEAD', cwd=main, env=main_environment)
    worktree = tmp_path / 'worktree with spaces'
    run('git', 'worktree', 'add', '-b', 'linked', str(worktree), cwd=main, env=main_environment)
    assert (worktree / '.git').is_file()
    environment = git_environment(worktree)
    write_fixture(worktree)
    build = configure(worktree, environment)
    run(*GIT_COMMIT, 'linked revision after configure', cwd=worktree, env=environment)
    advanced_hash = run('git', 'rev-parse', 'HEAD', cwd=worktree, env=environment)
    assert advanced_hash != configured_hash
    build_target(worktree, build, 'BakeResources', environment)
    compiled = compiled_hash(worktree, environment)
    assert compiled == advanced_hash
    assert configure_hashes(build) == [configured_hash, advanced_hash]
    assert_markers(worktree, 'BakeResources', compiled)


def test_read_engine_version_keeps_tracking_engine_git_head(tmp_path: Path) -> None:
    environment = {'GIT_CEILING_DIRECTORIES': str(tmp_path)}
    engine = tmp_path / 'Engine'
    for relative in (
        'BuildTools/cmake/helpers/EngineVersion.cmake',
        'BuildTools/engine_version.py',
    ):
        destination = engine / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ENGINE / relative, destination)
    (engine / 'VERSION').write_text('2026.1.67-dev\n', encoding='utf-8')
    run('git', 'init', cwd=engine, env=environment)
    run('git', 'add', '-A', cwd=engine, env=environment)
    run(*GIT_COMMIT, 'engine baseline', cwd=engine, env=environment)
    baseline = run('git', 'rev-parse', 'HEAD', cwd=engine, env=environment)
    source = tmp_path / 'source'
    source.mkdir()
    (source / 'check.cpp').write_text('int main() { return 0; }\n', encoding='utf-8')
    (source / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.22)\n'
        'project(EngineVersionTracking CXX)\n'
        f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
        f'include("{engine.as_posix()}/BuildTools/cmake/helpers/EngineVersion.cmake")\n'
        f'ReadEngineVersion("{engine.as_posix()}" engineVersion gitDependencies)\n'
        + r'''file(APPEND "${CMAKE_BINARY_DIR}/configure-log.txt" "${engineVersion}\n")''' + '\n'
        + r'''file(APPEND "${CMAKE_BINARY_DIR}/git-dependencies.txt" "${gitDependencies}\n")''' + '\n'
        + 'add_executable(check check.cpp)\n',
        encoding='utf-8')
    build = source / 'build'
    run('cmake', '-S', str(source), '-B', str(build), cwd=source, env=environment)
    configure_log = build / 'configure-log.txt'
    assert configure_log.read_text(encoding='utf-8').splitlines() == ['2026.1.67-dev']
    dependencies = (build / 'git-dependencies.txt').read_text(encoding='utf-8').splitlines()[0].split(';')
    assert dependencies
    assert all(Path(dependency).is_file() for dependency in dependencies)
    assert any(Path(dependency).name == 'HEAD' for dependency in dependencies)
    run(*GIT_COMMIT, 'engine revision after configure', cwd=engine, env=environment)
    assert run('git', 'rev-parse', 'HEAD', cwd=engine, env=environment) != baseline
    run('cmake', '--build', str(build), '--config', 'Release', '--target', 'check',
        cwd=source, timeout=60, env=environment)
    assert configure_log.read_text(encoding='utf-8').splitlines() == ['2026.1.67-dev', '2026.1.67-dev']
