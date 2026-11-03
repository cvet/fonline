from __future__ import annotations

from pathlib import Path
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
pytestmark = pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')


def run(*args: str, cwd: Path, timeout: int = 60) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True, stderr=subprocess.STDOUT,
                                   timeout=timeout).strip()


def test_baking_targets_invoke_compiled_baker_without_hash_side_channel(tmp_path: Path) -> None:
    source = tmp_path / 'source'
    source.mkdir()
    (source / 'Probe.fomain').write_text('', encoding='utf-8')
    stage = source / 'Engine/BuildTools/cmake/stages/ScriptsAndBaking.cmake'
    stage.parent.mkdir(parents=True)
    shutil.copy2(ENGINE / 'BuildTools/cmake/stages/ScriptsAndBaking.cmake', stage)
    (source / 'CMakeLists.txt').write_text(r'''cmake_minimum_required(VERSION 3.22)
project(BakingCommandOwnerTest NONE)
set(COMMAND_RECORD_DIR "${CMAKE_BINARY_DIR}/commands")
file(MAKE_DIRECTORY "${COMMAND_RECORD_DIR}")
macro(SetValue name)
    set(${name} ${ARGN})
endmacro()
macro(ParseArguments prefix options single multiple)
    cmake_parse_arguments(${prefix} "${options}" "${single}" "${multiple}" ${ARGN})
endmacro()
function(AbortMessage)
    message(FATAL_ERROR ${ARGV})
endfunction()
function(AddCommandTarget name)
    cmake_parse_arguments(RECORD "" "WORKING_DIRECTORY;COMMENT" "COMMAND_ARGS;DEPENDS;SOURCES" ${ARGN})
    file(WRITE "${COMMAND_RECORD_DIR}/${name}.commands.txt" "${RECORD_COMMAND_ARGS}")
    file(WRITE "${COMMAND_RECORD_DIR}/${name}.depends.txt" "${RECORD_DEPENDS}")
    file(WRITE "${COMMAND_RECORD_DIR}/${name}.working-directory.txt" "${RECORD_WORKING_DIRECTORY}")
    file(WRITE "${COMMAND_RECORD_DIR}/${name}.comment.txt" "${RECORD_COMMENT}")
endfunction()
set(FO_DEV_NAME Probe)
set(FO_MAIN_CONFIG Probe.fomain)
set(FO_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/output")
include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/BuildTools/cmake/stages/ScriptsAndBaking.cmake")
''', encoding='utf-8')
    build = source / 'build'
    run('cmake', '-S', str(source), '-B', str(build), cwd=source)
    records = build / 'commands'
    expected = {
        'BakeResources': [
            '-ApplySubConfig', 'NONE', '-Baking.ForceBaking', 'False', 'VERBATIM',
        ],
        'ForceBakeResources': [
            '-ApplySubConfig', 'NONE', '-Baking.ForceBaking', 'True', 'VERBATIM',
        ],
    }
    for target, command_tail in expected.items():
        commands = (records / f'{target}.commands.txt').read_text(encoding='utf-8').strip().split(';')
        assert commands[:3] == ['COMMAND', 'Probe_Baker', '-ApplyConfig']
        assert Path(commands[3]) == source / 'Probe.fomain'
        assert commands[4:] == command_tail
        assert commands.count('COMMAND') == 1
        assert commands.count('Probe_Baker') == 1
        assert commands.count('-Baking.ForceBaking') == 1
        assert not any('writebuildhash' in token.lower() for token in commands)
        assert not any(token.upper().startswith(('-DBUILD_HASH', '-DHASH_FILE')) for token in commands)
        assert (records / f'{target}.depends.txt').read_text(encoding='utf-8').strip() == 'ForceCodeGeneration'
        working_directory = (records / f'{target}.working-directory.txt').read_text(encoding='utf-8').strip()
        assert Path(working_directory) == source / 'output'
        assert (records / f'{target}.comment.txt').read_text(encoding='utf-8').strip() == 'Bake resources'
