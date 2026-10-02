from __future__ import annotations

from pathlib import Path
import shutil
import subprocess

import pytest


pytestmark = pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')
GENERATORS = ('Unix Makefiles', 'Ninja')
OUTPUTS = ('EngineConfig.gen.h', 'EmbeddedResources.gen.inc', 'InternalConfig.gen.inc',
           'GenericCode-Common.gen.cpp',
           *(f'MetadataRegistration-{side}{stub}.gen.cpp'
             for side in ('Server', 'Client', 'Mapper') for stub in ('', 'Stub')))


class CodegenProject:
    def __init__(self, source: Path, generator: str | None) -> None:
        if generator is not None and shutil.which('ninja' if generator == 'Ninja' else 'make') is None:
            pytest.skip(f'{generator} is required')
        self.source = source
        self.build_dir = source / 'build'
        self.generator = generator
        self.metadata = source / 'metadata.txt'
        self.metadata.write_text('first metadata')
        fake_generator = source / 'generator.py'
        fake_generator.write_text('''from pathlib import Path
import sys
args = Path(sys.argv[1][1:]).read_text().splitlines()
output = Path(args[args.index('-genoutput') + 1])
output.mkdir(exist_ok=True)
if (output / 'reject-generation').exists():
    sys.exit(42)
content = '\\n'.join(args) + '\\n' + Path(args[args.index('-meta') + 1]).read_text()
for name in ('EngineConfig.gen.h', 'EmbeddedResources.gen.inc', 'InternalConfig.gen.inc',
             'GenericCode-Common.gen.cpp',
             *(f'MetadataRegistration-{side}{stub}.gen.cpp' for side in ('Server','Client','Mapper') for stub in ('','Stub'))):
    path = output / name
    if not path.exists() or path.read_text() != content:
        path.write_text(content)
with (output / 'invocations').open('a') as stream:
    stream.write('run\\n')
''')
        stage = Path(__file__).resolve().parents[1] / 'cmake/stages/Codegen.cmake'
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
        cmake += f'''set(FO_CODEGEN_SCRIPT "{fake_generator.as_posix()}")
set(FO_SOURCE_META_FILES "{self.metadata.as_posix()}")
include("{stage.as_posix()}")
add_custom_command(OUTPUT "${{CMAKE_CURRENT_BINARY_DIR}}/consumer-receipt"
    COMMAND "${{CMAKE_COMMAND}}" -E copy
            "${{CMAKE_CURRENT_BINARY_DIR}}/GeneratedSource/EngineConfig.gen.h"
            "${{CMAKE_CURRENT_BINARY_DIR}}/consumer-receipt"
    DEPENDS "${{CMAKE_CURRENT_BINARY_DIR}}/GeneratedSource/EngineConfig.gen.h")
add_custom_target(Consumer DEPENDS "${{CMAKE_CURRENT_BINARY_DIR}}/consumer-receipt")
add_dependencies(Consumer CodeGeneration)
'''
        (source / 'CMakeLists.txt').write_text(cmake)

    def configure(self, revision: str = 'first-revision', capacity: int = 31) -> None:
        generator_args = ['-G', self.generator] if self.generator else []
        subprocess.run(['cmake', *generator_args, '-S', str(self.source), '-B', str(self.build_dir),
                        '-DFO_BUILD_HASH=' + revision, '-DFO_STRING_INLINE_CAPACITY=' + str(capacity)],
                       check=True, capture_output=True, text=True)

    def build(self, target: str = 'Consumer', *, check: bool = True) -> subprocess.CompletedProcess[str]:
        return subprocess.run(['cmake', '--build', str(self.build_dir), '--target', target],
                              check=check, capture_output=True, text=True)

    def output(self, name: str) -> Path:
        return self.build_dir / 'GeneratedSource' / name

    def invocations(self) -> int:
        return len(self.output('invocations').read_text().splitlines())

    def mtimes(self) -> dict[str, int]:
        return {name: self.output(name).stat().st_mtime_ns for name in OUTPUTS}


@pytest.mark.parametrize('generator', (None, *GENERATORS))
def test_reconfigured_codegen_arguments_invalidate_outputs(tmp_path: Path, generator: str | None) -> None:
    project = CodegenProject(tmp_path, generator)
    project.configure()
    project.build()
    header = project.output('EngineConfig.gen.h')
    assert 'first-revision' in header.read_text()
    project.configure('second-revision', 63)
    project.build()
    assert 'second-revision' in header.read_text()
    assert 'FO_STRING_INLINE_CAPACITY=63' in header.read_text()
    assert (project.build_dir / 'consumer-receipt').read_text() == header.read_text()
    args_mtime = (project.build_dir / 'codegen-args.txt').stat().st_mtime_ns
    project.configure('second-revision', 63)
    project.build()
    assert (project.build_dir / 'codegen-args.txt').stat().st_mtime_ns == args_mtime
    assert project.invocations() == 2


@pytest.mark.parametrize('generator', GENERATORS)
def test_forced_identical_codegen_does_not_rebuild_consumers(tmp_path: Path, generator: str) -> None:
    project = CodegenProject(tmp_path, generator)
    project.configure()
    project.build()
    mtimes = project.mtimes()
    receipt = project.build_dir / 'consumer-receipt'
    receipt_mtime = receipt.stat().st_mtime_ns
    project.build('ForceCodeGeneration')
    assert project.invocations() == 2
    project.build()
    project.build()
    assert project.invocations() == 2
    assert project.mtimes() == mtimes
    assert receipt.stat().st_mtime_ns == receipt_mtime


@pytest.mark.parametrize('generator', GENERATORS)
def test_unchanged_metadata_refresh_does_not_touch_outputs(tmp_path: Path, generator: str) -> None:
    project = CodegenProject(tmp_path, generator)
    project.configure()
    project.build()
    mtimes = project.mtimes()
    receipt = project.build_dir / 'consumer-receipt'
    receipt_mtime = receipt.stat().st_mtime_ns
    project.metadata.touch()
    project.build()
    project.build()
    assert project.invocations() == 2
    assert project.mtimes() == mtimes
    assert receipt.stat().st_mtime_ns == receipt_mtime
    project.metadata.write_text('changed metadata')
    project.build()
    assert project.invocations() == 3
    assert 'changed metadata' in receipt.read_text()


@pytest.mark.parametrize('generator', GENERATORS)
@pytest.mark.parametrize('missing', ('EngineConfig.gen.h', 'MetadataRegistration-Server.gen.cpp'))
def test_missing_codegen_output_is_restored(tmp_path: Path, generator: str, missing: str) -> None:
    project = CodegenProject(tmp_path, generator)
    project.configure()
    project.build()
    mtimes = project.mtimes()
    content = project.output(missing).read_bytes()
    project.output(missing).unlink()
    project.build()
    project.build()
    assert project.invocations() == 2
    assert project.output(missing).read_bytes() == content
    assert {name: mtime for name, mtime in project.mtimes().items() if name != missing} == {
        name: mtime for name, mtime in mtimes.items() if name != missing}


@pytest.mark.parametrize('generator', GENERATORS)
def test_missing_output_repair_propagates_generator_failure(tmp_path: Path, generator: str) -> None:
    project = CodegenProject(tmp_path, generator)
    project.configure()
    project.build()
    receipt = project.build_dir / 'consumer-receipt'
    receipt_mtime = receipt.stat().st_mtime_ns
    project.output('reject-generation').touch()
    project.output('EngineConfig.gen.h').unlink()
    result = project.build(check=False)
    assert result.returncode != 0
    assert not project.output('EngineConfig.gen.h').exists()
    assert receipt.stat().st_mtime_ns == receipt_mtime
