from __future__ import annotations

import json
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

import pytest


ENGINE = Path(__file__).resolve().parents[2]
INIT = (ENGINE / 'BuildTools/cmake/stages/Init.cmake').read_text(encoding='utf-8')
INIT_HASH_REGION = INIT.split('# Evaluate build hash\n', 1)[1].split('\nSetValue(FO_CODEGEN_SCRIPT', 1)[0]
ENGINE_METADATA_REGION = (
    ENGINE / 'BuildTools/cmake/stages/EngineSources.cmake'
).read_text(encoding='utf-8').split('AppendList(FO_SOURCE_META_FILES\n', 1)[1].split(')\n', 1)[0]
ENGINE_METADATA_FILES = re.findall(r'"\$\{FO_ENGINE_ROOT\}/([^"]+)"', ENGINE_METADATA_REGION)
GIT_COMMIT = (
    'git', '-c', 'user.name=Build Hash Test', '-c',
    'user.email=build-hash@example.invalid', 'commit', '--allow-empty', '-m',
)
PROBE_SOURCE = r'''#include "EngineConfig.gen.h"
#include <filesystem>
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
function(AddCommandTarget name)
    cmake_parse_arguments(ARG "COMMAND_ARGS" "" "" ${ARGN})
    add_custom_target(${name} ${ARG_UNPARSED_ARGUMENTS})
endfunction()
macro(AppendList name)
    list(APPEND ${name} ${ARGN})
endmacro()
macro(IncludeFile name)
    include(${name})
endmacro()
macro(RequirePackage)
    find_package(${ARGV})
endmacro()
macro(FileWrite)
    file(WRITE ${ARGV})
endmacro()
set(FO_NICE_NAME Probe)
set(FO_GEOMETRY HEXAGONAL)
set(FO_MAP_HEX_WIDTH 32)
set(FO_MAP_HEX_HEIGHT 16)
set(FO_MAP_CAMERA_ANGLE 25.6589)
set(FO_EFFECT_SCRIPT_VALUES 16)
set(FO_EFFECT_MAX_PASSES 6)
set(FO_MODEL_LAYERS_COUNT 30)
set(FO_MODEL_MAX_TEXTURES 8)
set(FO_MODEL_MAX_BONES 54)
set(FO_MODEL_BONES_PER_VERTEX 4)
set(FO_STRING_INLINE_CAPACITY 31)
set(FO_EMBEDDED_DATA_CAPACITY 200000)
set(FO_CODEGEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Engine/BuildTools/codegen.py")
set(FO_SOURCE_META_FILES
''' + ENGINE_METADATA_REGION + r''')
include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/BuildTools/cmake/helpers/Options.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/BuildTools/cmake/stages/Codegen.cmake")
target_include_directories(Probe_Baker PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")
add_dependencies(Probe_Baker ${FO_GEN_DEPENDENCIES})
WriteBuildHash(Probe_Baker)
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
    return subprocess.check_output(args, cwd=cwd, text=True, encoding='utf-8', stderr=subprocess.STDOUT,
                                   timeout=timeout, env=environment).strip()


def git_environment(path: Path) -> dict[str, str]:
    # Keep a fixture from discovering a repository above its own root
    return {'GIT_CEILING_DIRECTORIES': str(path.parent), 'PYTHONDONTWRITEBYTECODE': '1'}


def write_fixture(source: Path) -> None:
    # Stage an Engine archive; revision tests can give it an independent repository
    fixture_engine = source / 'Engine'
    for relative in (
        'BuildTools/cmake/helpers/Build.cmake',
        'BuildTools/cmake/helpers/EngineVersion.cmake',
        'BuildTools/cmake/helpers/WriteBuildHash.cmake',
        'BuildTools/cmake/helpers/Options.cmake',
        'BuildTools/cmake/helpers/EnsureCodegenOutputs.cmake.in',
        'BuildTools/cmake/stages/Codegen.cmake',
        'BuildTools/cmake/stages/ScriptsAndBaking.cmake',
        'BuildTools/codegen.py',
        'BuildTools/engine_version.py',
        'VERSION',
        *ENGINE_METADATA_FILES,
    ):
        destination = fixture_engine / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ENGINE / relative, destination)
    (source / 'Probe.fomain').write_text('', encoding='utf-8')
    (source / 'probe.cpp').write_text(PROBE_SOURCE, encoding='utf-8')
    (source / 'CMakeLists.txt').write_text(
        FIXTURE_CMAKE_HEAD + INIT_HASH_REGION + FIXTURE_CMAKE_TAIL, encoding='utf-8')


def configure(source: Path, environment: dict[str, str], *, pinned: str | None = None,
              generator: str | None = None) -> Path:
    build = source / 'build'
    arguments = ['cmake']
    if generator is not None:
        arguments.extend(['-G', generator])
        if generator.startswith('Visual Studio '):
            arguments.extend(['-A', 'x64'])
    arguments.extend(['-S', str(source), '-B', str(build)])
    if pinned is not None:
        arguments.append('-DFO_BUILD_HASH=' + pinned)
    run(*arguments, cwd=source, env=environment)
    return build


def build_target(source: Path, build: Path, target: str, environment: dict[str, str],
                 *, generator: str | None = None) -> str:
    arguments = ['cmake', '--build', str(build), '--config', 'Release', '--target', target]
    if generator is not None and generator.startswith('Visual Studio '):
        arguments.extend(['--', '/nologo', '/verbosity:normal'])
    return run(*arguments, cwd=source, timeout=120, env=environment)


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


def visual_studio_generator() -> str:
    if os.name != 'nt':
        pytest.skip('Visual Studio regression requires Windows')
    installer = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'))
    vswhere = installer / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        pytest.skip('vswhere.exe is required to detect an installed Visual Studio')
    installation_versions = [
        version.strip() for version in run(
            str(vswhere), '-prerelease', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '-property', 'installationVersion', cwd=ENGINE).splitlines()
        if version.strip()
    ]
    if not installation_versions:
        pytest.skip('No supported Visual Studio C++ toolchain is installed')
    capabilities = json.loads(run('cmake', '-E', 'capabilities', cwd=ENGINE))
    generator_names = [generator['name'] for generator in capabilities['generators']]
    for installation_version in installation_versions:
        major = installation_version.split('.', 1)[0]
        generators = [
            name for name in generator_names
            if re.fullmatch(rf'Visual Studio {major} \d{{4}}', name)
        ]
        if generators:
            return generators[0]
    pytest.skip('CMake has no generator for an installed Visual Studio C++ toolchain')


def assert_no_directory_additional_inputs(build: Path) -> None:
    for project_name in ('ZERO_CHECK.vcxproj', 'Probe_Baker.vcxproj'):
        project = build / project_name
        assert project.is_file(), project_name
        nodes = [
            element for element in ET.parse(project).getroot().iter()
            if element.tag.rsplit('}', 1)[-1] == 'AdditionalInputs'
        ]
        assert nodes, f'{project_name} has no AdditionalInputs nodes'
        for node in nodes:
            for value in (node.text or '').split(';'):
                value = value.strip().strip('"')
                if not value or value.startswith('%('):
                    continue
                dependency = Path(value.replace('\\', '/'))
                if not dependency.is_absolute():
                    dependency = project.parent / dependency
                assert not dependency.is_dir(), (
                    f'{project_name} has directory AdditionalInput: {value}')


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


@pytest.mark.skipif(os.name != 'nt', reason='Visual Studio regression requires Windows')
@pytest.mark.parametrize('linked', (False, True), ids=('normal', 'linked-worktree'))
@pytest.mark.parametrize('target', ('Probe_Baker', 'BakeResources'))
def test_visual_studio_packed_ref_transitions_use_file_only_dependencies(
    tmp_path: Path, linked: bool, target: str,
) -> None:
    generator = visual_studio_generator()
    if linked:
        main = tmp_path / 'main'
        main.mkdir()
        main_environment = git_environment(main)
        run('git', 'init', cwd=main, env=main_environment)
        run(*GIT_COMMIT, 'linked baseline', cwd=main, env=main_environment)
        source = tmp_path / 'wt x'
        run('git', 'worktree', 'add', '-b', 'linked', str(source), cwd=main, env=main_environment)
        assert (source / '.git').is_file()
    else:
        source = tmp_path / 'repo x'
        source.mkdir()
        environment = git_environment(source)
        run('git', 'init', cwd=source, env=environment)
        run(*GIT_COMMIT, 'normal baseline', cwd=source, env=environment)
        assert (source / '.git').is_dir()
    environment = git_environment(source)
    write_fixture(source)
    branch = run('git', 'symbolic-ref', '--short', 'HEAD', cwd=source, env=environment)
    run('git', 'pack-refs', '--all', '--prune', cwd=source, env=environment)
    packed_ref = Path(run('git', 'rev-parse', '--git-path', f'refs/heads/{branch}', cwd=source,
                          env=environment))
    if not packed_ref.is_absolute():
        packed_ref = (source / packed_ref).resolve()
    assert not packed_ref.exists()
    build = configure(source, environment, generator=generator)
    assert f'CMAKE_GENERATOR:INTERNAL={generator}' in (
        build / 'CMakeCache.txt').read_text(encoding='utf-8').splitlines()
    expected_hashes = [run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)]
    for stage in range(3):
        if stage:
            run(*GIT_COMMIT, f'loose-ref transition {stage}', cwd=source, env=environment)
            assert packed_ref.is_file()
            expected_hashes.append(run('git', 'rev-parse', 'HEAD', cwd=source, env=environment))
        output = build_target(source, build, target, environment, generator=generator)
        assert 'MSB8064' not in output
        compiled = compiled_hash(source, environment)
        assert compiled == expected_hashes[-1]
        assert_markers(source, target, compiled)
        assert configure_hashes(build) == expected_hashes
        assert_no_directory_additional_inputs(build)


@pytest.mark.parametrize('file_input', (False, True), ids=('literal', 'file'))
@pytest.mark.parametrize('value,literal_value', (('0', '0'), (' padded hash ', ' padded hash'),
                                               ('custom;hash with spaces', 'custom;hash with spaces'),
                                               ('hash-\u03bb', 'hash-\u03bb')))
def test_build_hash_writer_preserves_literal_and_file_values(
    tmp_path: Path, file_input: bool, value: str, literal_value: str,
) -> None:
    output = tmp_path / 'output.hash'
    arguments = ['cmake', '-DHASH_FILE=' + str(output)]
    if file_input:
        source = tmp_path / 'input.hash'
        source.write_bytes(value.encode('utf-8'))
        arguments.append('-DBUILD_HASH_INPUT_FILE=' + str(source))
    else:
        arguments.append('-DBUILD_HASH=' + value)
    arguments.extend(['-P', str(ENGINE / 'BuildTools/cmake/helpers/WriteBuildHash.cmake')])
    run(*arguments, cwd=tmp_path)
    assert output.read_bytes() == (value if file_input else literal_value).encode('utf-8')


@pytest.mark.parametrize('invalid', ('missing', 'empty-literal', 'missing-file', 'empty-file',
                                   'directory', 'both'))
def test_build_hash_writer_rejects_invalid_input_before_output_changes(
    tmp_path: Path, invalid: str,
) -> None:
    output = tmp_path / 'output.hash'
    output.write_bytes(b'previous marker')
    source = tmp_path / 'input.hash'
    arguments = ['cmake', '-DHASH_FILE=' + str(output)]
    if invalid == 'empty-literal':
        arguments.append('-DBUILD_HASH=')
    elif invalid in ('missing-file', 'empty-file', 'directory', 'both'):
        if invalid == 'empty-file':
            source.write_bytes(b'')
        elif invalid == 'directory':
            source.mkdir()
        elif invalid == 'both':
            source.write_bytes(b'file hash')
            arguments.append('-DBUILD_HASH=literal hash')
        arguments.append('-DBUILD_HASH_INPUT_FILE=' + str(source))
    arguments.extend(['-P', str(ENGINE / 'BuildTools/cmake/helpers/WriteBuildHash.cmake')])
    result = subprocess.run(arguments, cwd=tmp_path, capture_output=True, text=True,
                            encoding='utf-8', timeout=30)
    assert result.returncode != 0
    assert output.read_bytes() == b'previous marker'


@pytest.mark.parametrize('linked', (False, True), ids=('normal', 'linked-worktree'))
def test_read_engine_version_keeps_tracking_engine_git_head(tmp_path: Path, linked: bool) -> None:
    source = tmp_path / 'src'
    source.mkdir()
    environment = git_environment(source)
    write_fixture(source)
    run('git', 'init', cwd=source, env=environment)
    run(*GIT_COMMIT, 'constant project revision', cwd=source, env=environment)
    project_hash = run('git', 'rev-parse', 'HEAD', cwd=source, env=environment)
    engine = source / 'Engine'
    if linked:
        main = tmp_path / 'base'
        shutil.copytree(engine, main, ignore=shutil.ignore_patterns('__pycache__'))
        main_environment = git_environment(main)
        run('git', 'init', '-b', 'main', cwd=main, env=main_environment)
        run('git', 'add', '.', cwd=main, env=main_environment)
        run(*GIT_COMMIT, 'engine baseline', cwd=main, env=main_environment)
        engine = source / 'eng x'
        run('git', 'worktree', 'add', '-b', 'feature', str(engine), cwd=main,
            env=main_environment)
        cmake = source / 'CMakeLists.txt'
        content = cmake.read_text(encoding='utf-8')
        content = content.replace('set(FO_ENGINE_ROOT "Engine")', 'set(FO_ENGINE_ROOT "eng x")')
        cmake.write_text(content.replace('/Engine/', '/eng x/'), encoding='utf-8')
        assert (engine / '.git').is_file()
    else:
        main = engine
        main_environment = git_environment(main)
        run('git', 'init', '-b', 'feature', cwd=engine, env=main_environment)
        run('git', 'add', '.', cwd=engine, env=main_environment)
        run(*GIT_COMMIT, 'engine baseline', cwd=engine, env=main_environment)
        assert (engine / '.git').is_dir()
    engine_environment = git_environment(engine)
    probe = source / 'probe.cpp'
    probe.write_text(PROBE_SOURCE.replace('std::cout << FO_BUILD_HASH;',
                                         'std::cout << FO_ENGINE_REVISION;'), encoding='utf-8')
    generator = visual_studio_generator() if os.name == 'nt' else None
    build = configure(source, environment, generator=generator)
    version = (engine / 'VERSION').read_bytes()
    for stage in ('initial', 'packed', 'first-loose', 'subsequent'):
        if stage == 'packed':
            run('git', 'pack-refs', '--all', '--prune', cwd=main, env=main_environment)
        elif stage in ('first-loose', 'subsequent'):
            run(*GIT_COMMIT, stage, cwd=engine, env=engine_environment)
        expected = run(sys.executable, '-c',
                       'import sys; from pathlib import Path; sys.path.insert(0, sys.argv[1]); '
                       'from engine_version import read_engine_revision; '
                       'print(read_engine_revision(Path(sys.argv[2])))',
                       str(engine / 'BuildTools'), str(engine), cwd=source, env=environment)
        assert not run('git', 'status', '--porcelain', '--untracked-files=no', cwd=engine,
                       env=engine_environment)
        output = build_target(source, build, 'Probe_Baker', environment, generator=generator)
        assert 'MSB8064' not in output
        assert compiled_hash(source, environment) == expected
        assert_markers(source, 'Probe_Baker', project_hash)
        assert (engine / 'VERSION').read_bytes() == version
        assert run('git', 'rev-parse', 'HEAD', cwd=source, env=environment) == project_hash
