from __future__ import annotations

from pathlib import Path
import os
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
pytestmark = pytest.mark.skipif(
    shutil.which('cmake') is None or shutil.which('git') is None,
    reason='CMake and Git are required',
)


def run(*args: str, cwd: Path, timeout: int = 60) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True, stderr=subprocess.STDOUT,
                                   timeout=timeout).strip()


@pytest.mark.parametrize('target', ('Probe_Baker', 'BakeResources', 'ForceBakeResources'))
@pytest.mark.parametrize('revision', ('unchanged', 'advanced', 'no-git'))
@pytest.mark.parametrize('directory', ('source', 'source with spaces'))
def test_markers_match_compiled_revision_after_head_advances(
    tmp_path: Path, target: str, revision: str, directory: str,
) -> None:
    source = tmp_path / directory
    source.mkdir()
    git_commit = ('git', '-c', 'user.name=Build Hash Test', '-c',
                  'user.email=build-hash@example.invalid', 'commit', '--allow-empty', '-m')
    if revision == 'no-git':
        configured_hash = 'f' * 40 + '-random'
    else:
        run('git', 'init', cwd=source)
        run(*git_commit, 'configured revision', cwd=source)
        configured_hash = run('git', 'rev-parse', 'HEAD', cwd=source)
    (source / 'Probe.fomain').write_text('', encoding='utf-8')
    (source / 'probe.cpp').write_text(r'''#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char**)
{
    std::cout << FO_BUILD_HASH;
    if (argc > 1) {
        std::filesystem::create_directories("Baking");
        std::ofstream("Baking/artifact.build-hash") << FO_BUILD_HASH;
    }
    return 0;
}
''', encoding='utf-8')
    engine_relative = os.path.relpath(ENGINE, source).replace('\\', '/')
    (source / 'CMakeLists.txt').write_text(r'''cmake_minimum_required(VERSION 3.22)
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
set(FO_DEV_NAME Probe)
set(FO_MAIN_CONFIG Probe.fomain)
set(FO_GIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(FO_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${FO_OUTPUT_PATH}")
add_custom_target(ForceCodeGeneration)
add_executable(Probe_Baker probe.cpp)
target_compile_features(Probe_Baker PRIVATE cxx_std_17)
target_compile_definitions(Probe_Baker PRIVATE "FO_BUILD_HASH=\"${FO_BUILD_HASH}\"")
set_target_properties(Probe_Baker PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FO_OUTPUT_PATH}")
''' + f'''set(FO_ENGINE_ROOT "{engine_relative}")
include("{ENGINE.as_posix()}/BuildTools/cmake/helpers/Build.cmake")
''' + r'''function(AddCommandTarget name)
    cmake_parse_arguments(ARG "COMMAND_ARGS" "" "" ${ARGN})
    add_custom_target(${name} ${ARG_UNPARSED_ARGUMENTS})
endfunction()
WriteBuildHash(Probe_Baker)
include("${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/stages/ScriptsAndBaking.cmake")
''', encoding='utf-8')
    build = source / 'build'
    run('cmake', '-S', str(source), '-B', str(build), '-DFO_BUILD_HASH=' + configured_hash,
        cwd=source)
    if revision == 'advanced':
        run(*git_commit, 'revision after configure', cwd=source)
        assert run('git', 'rev-parse', 'HEAD', cwd=source) != configured_hash
    run('cmake', '--build', str(build), '--config', 'Release', '--target', target,
        cwd=source, timeout=120)
    output = source / 'output'
    executable = next(output.rglob('Probe_Baker.exe' if os.name == 'nt' else 'Probe_Baker'))
    embedded_hash = run(str(executable), cwd=source, timeout=10)
    marker = next(output.rglob('Probe_Baker.build-hash'))
    assert embedded_hash == configured_hash
    assert marker.read_text().strip() == embedded_hash
    if target != 'Probe_Baker':
        assert (output / 'Baking/artifact.build-hash').read_text().strip() == embedded_hash
        assert (output / 'Baking/Resources.build-hash').read_text().strip() == embedded_hash
