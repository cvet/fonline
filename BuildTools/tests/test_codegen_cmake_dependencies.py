from __future__ import annotations

from pathlib import Path
import shutil
import subprocess

import pytest


@pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')
def test_reconfigured_codegen_arguments_invalidate_outputs(tmp_path: Path) -> None:
    stage = Path(__file__).resolve().parents[1] / 'cmake/stages/Codegen.cmake'
    fake_generator = tmp_path / 'generator.py'
    fake_generator.write_text('''from pathlib import Path
import sys
args = Path(sys.argv[1][1:]).read_text().splitlines()
output = Path(args[args.index('-genoutput') + 1])
output.mkdir(exist_ok=True)
for name in ('EngineConfig.gen.h', 'EmbeddedResources.gen.inc', 'InternalConfig.gen.inc',
             'GenericCode-Common.gen.cpp',
             *(f'MetadataRegistration-{side}{stub}.gen.cpp' for side in ('Server','Client','Mapper') for stub in ('','Stub'))):
    (output / name).write_text('\\n'.join(args))
with (output / 'invocations').open('a') as stream:
    stream.write('run\\n')
''')
    cmake = '''cmake_minimum_required(VERSION 3.22)
project(CodegenDependencyTest NONE)
macro(SetValue name)
    set(${name} ${ARGN})
endmacro()
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
macro(FileAppend)
    file(APPEND ${ARGV})
endmacro()
macro(AddCustomCommand)
    add_custom_command(${ARGV})
endmacro()
function(AddCommandTarget name)
    cmake_parse_arguments(ARG "COMMAND_ARGS" "" "" ${ARGN})
    add_custom_target(${name} ${ARG_UNPARSED_ARGUMENTS})
endfunction()
set(FO_GEOMETRY HEXAGONAL)
set(FO_MAIN_CONFIG Test.fomain)
set(FO_DEV_NAME Test)
set(FO_NICE_NAME Test)
set(FO_EMBEDDED_DATA_CAPACITY 100)
'''
    cmake += f'set(FO_CODEGEN_SCRIPT "{fake_generator.as_posix()}")\ninclude("{stage.as_posix()}")\n'
    (tmp_path / 'CMakeLists.txt').write_text(cmake)
    build = tmp_path / 'build'

    def configure_and_build(revision: str, capacity: int) -> None:
        subprocess.run(['cmake', '-S', str(tmp_path), '-B', str(build),
                        '-DFO_BUILD_HASH=' + revision, '-DFO_STRING_INLINE_CAPACITY=' + str(capacity)],
                       check=True, capture_output=True, text=True)
        subprocess.run(['cmake', '--build', str(build), '--target', 'CodeGeneration'],
                       check=True, capture_output=True, text=True)

    configure_and_build('first-revision', 31)
    header = build / 'GeneratedSource/EngineConfig.gen.h'
    assert 'first-revision' in header.read_text()
    configure_and_build('second-revision', 63)
    assert 'second-revision' in header.read_text()
    assert 'FO_STRING_INLINE_CAPACITY=63' in header.read_text()
    args_mtime = (build / 'codegen-args.txt').stat().st_mtime_ns
    configure_and_build('second-revision', 63)
    assert (build / 'codegen-args.txt').stat().st_mtime_ns == args_mtime
    assert (build / 'GeneratedSource/invocations').read_text().splitlines() == ['run', 'run']
