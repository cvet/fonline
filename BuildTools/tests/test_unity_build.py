from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
INIT = (ENGINE / 'BuildTools/cmake/stages/Init.cmake').read_text(encoding='utf-8')
DECLARATIONS = INIT.split('# Options\n', 1)[1].split('# Quiet all non-error', 1)[0]
pytestmark = pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')


class UnityProject:
    def __init__(self, source: Path) -> None:
        self.source = source
        self.build_dir = source / 'build'
        core_sources = []
        for index in range(40):
            path = source / f'core_{index:02}.cpp'
            path.write_text(f'int core_fixture_{index:02}() {{ return {index}; }}\n', encoding='utf-8')
            core_sources.append(path.as_posix())

        app_sources = []
        for index in range(4):
            path = source / f'app_{index:02}.cpp'
            path.write_text(f'int app_fixture_{index:02}() {{ return {index}; }}\n', encoding='utf-8')
            app_sources.append(path.as_posix())

        (source / 'main.cpp').write_text('int main() { return 0; }\n', encoding='utf-8')
        (source / 'third_party.cpp').write_text('int third_party_fixture() { return 0; }\n', encoding='utf-8')
        core_source_list = '\n    '.join(f'"{path}"' for path in core_sources)
        app_source_list = ' '.join(f'"{path}"' for path in app_sources)
        (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.22)
project(UnityBuildFixture LANGUAGES CXX)
include("{ENGINE.as_posix()}/BuildTools/Init.cmake")
set(FO_OUTPUT_PATH "${{CMAKE_CURRENT_BINARY_DIR}}/output")
{DECLARATIONS}
set(FO_FORCE_ENABLE_3D "")
include("{ENGINE.as_posix()}/BuildTools/cmake/stages/ProjectOptions.cmake")
set(FO_GEN_DEPENDENCIES FixtureGeneration)
set(FO_MANAGED_SCRIPTING OFF)
set(expr_SanitizerConfigs 0)
AddCustomTarget(FixtureGeneration)
set(CORE_SOURCES
    {core_source_list})
AddCoreStaticLibrary(CoreFixture CORE_SOURCES)
set(THIRD_PARTY_SOURCES "${{CMAKE_CURRENT_SOURCE_DIR}}/third_party.cpp")
AddStaticThirdPartyLibrary(ThirdPartyFixture SOURCE_LIST THIRD_PARTY_SOURCES)
AddExecutableApplication(AppFixture "${{CMAKE_CURRENT_SOURCE_DIR}}/main.cpp"
    EXTRA_SOURCES {app_source_list}
    OUTPUT_DIR "${{CMAKE_CURRENT_BINARY_DIR}}/bin"
    WORKING_DIRECTORY "${{CMAKE_CURRENT_SOURCE_DIR}}"
    OUTPUT_NAME AppFixture
    TESTING_APP 0
    HEADLESS_APP 1
    LINK_LIBS CoreFixture ThirdPartyFixture)
StatusMessage("UnityFixtureWindows=${{FO_WINDOWS}}")
foreach(target CoreFixture AppFixture ThirdPartyFixture)
    GetTargetProperty(unity ${{target}} UNITY_BUILD)
    GetTargetProperty(mode ${{target}} UNITY_BUILD_MODE)
    GetTargetProperty(batch ${{target}} UNITY_BUILD_BATCH_SIZE)
    GetTargetProperty(defines ${{target}} COMPILE_DEFINITIONS)
    StatusMessage("UnityTarget=${{target}}|UNITY_BUILD=${{unity}}|UNITY_BUILD_MODE=${{mode}}|UNITY_BUILD_BATCH_SIZE=${{batch}}|COMPILE_DEFINITIONS=${{defines}}")
endforeach()
''', encoding='utf-8')

    def configure(self, *, unity: str = 'ON', batch_size: str | None = None) -> subprocess.CompletedProcess[str]:
        args = ['cmake', '-S', str(self.source), '-B', str(self.build_dir),
                '-DCMAKE_UNITY_BUILD=OFF', f'-DFO_UNITY_BUILD={unity}']
        if batch_size is not None:
            args.append(f'-DFO_UNITY_BUILD_BATCH_SIZE={batch_size}')
        return subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)

    def build(self) -> subprocess.CompletedProcess[str]:
        return subprocess.run(['cmake', '--build', str(self.build_dir), '--config', 'Release',
                               '--target', 'AppFixture', '--parallel', '1'],
                              capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)

    def unity_includes(self, target: str) -> list[int]:
        files = sorted(path for path in self.build_dir.rglob('unity_*.cxx') if f'{target}.dir' in path.as_posix())
        return [sum(line.startswith('#include') for line in path.read_text(encoding='utf-8').splitlines())
                for path in files]


def configured_project(tmp_path: Path, *, unity: str) -> tuple[UnityProject, str]:
    project = UnityProject(tmp_path)
    result = project.configure(unity=unity)
    output = result.stdout + result.stderr
    assert result.returncode == 0, output
    return project, output


def compile_definitions(output: str, target: str) -> list[str]:
    match = re.search(rf'UnityTarget={target}\|[^\r\n]*\|COMPILE_DEFINITIONS=([^\r\n]*)', output)
    assert match is not None
    return match.group(1).split(';')


def test_unity_build_batches_first_party_targets_only(tmp_path: Path) -> None:
    project, output = configured_project(tmp_path, unity='ON')
    assert 'UnityTarget=CoreFixture|UNITY_BUILD=ON|UNITY_BUILD_MODE=BATCH|UNITY_BUILD_BATCH_SIZE=32' in output
    assert 'UnityTarget=AppFixture|UNITY_BUILD=ON|UNITY_BUILD_MODE=BATCH|UNITY_BUILD_BATCH_SIZE=32' in output
    assert project.unity_includes('CoreFixture') == [32, 8]
    assert project.unity_includes('AppFixture') == [5]
    assert project.unity_includes('ThirdPartyFixture') == []
    assert ('NOMINMAX' in compile_definitions(output, 'CoreFixture')) == ('UnityFixtureWindows=ON' in output)
    assert ('NOMINMAX' in compile_definitions(output, 'AppFixture')) == ('UnityFixtureWindows=ON' in output)
    assert 'NOMINMAX' not in compile_definitions(output, 'ThirdPartyFixture')
    result = project.build()
    assert result.returncode == 0, result.stdout + result.stderr


def test_unity_build_off_preserves_individual_compilation(tmp_path: Path) -> None:
    project, output = configured_project(tmp_path, unity='OFF')
    assert 'UnityTarget=CoreFixture|UNITY_BUILD=OFF|UNITY_BUILD_MODE=BATCH|UNITY_BUILD_BATCH_SIZE=32' in output
    assert 'UnityTarget=AppFixture|UNITY_BUILD=OFF|UNITY_BUILD_MODE=BATCH|UNITY_BUILD_BATCH_SIZE=32' in output
    assert project.unity_includes('CoreFixture') == []
    assert project.unity_includes('AppFixture') == []
    assert project.unity_includes('ThirdPartyFixture') == []
    assert 'NOMINMAX' not in compile_definitions(output, 'CoreFixture')
    assert 'NOMINMAX' not in compile_definitions(output, 'AppFixture')
    assert 'NOMINMAX' not in compile_definitions(output, 'ThirdPartyFixture')
    result = project.build()
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize('value', ('0', '-1', 'abc'))
def test_unity_build_rejects_invalid_batch_size(tmp_path: Path, value: str) -> None:
    project = UnityProject(tmp_path)
    result = project.configure(batch_size=value)
    output = result.stdout + result.stderr
    assert result.returncode != 0
    assert re.search(r'FO_UNITY_BUILD_BATCH_SIZE.*positive integer', output)


def test_rendering_backend_file_local_function_names_are_unique() -> None:
    frontend = ENGINE / 'Source/Frontend'
    backend_files = [
        frontend / 'Rendering-Direct3D.cpp',
        frontend / 'Rendering-OpenGL.cpp',
        frontend / 'Rendering-SDLGpu.cpp',
        frontend / 'Rendering-Vulkan.cpp',
    ]
    function_pattern = re.compile(
        r'^static\s+(?:auto|void|bool|int32_t|uint32_t|size_t|float32_t)\s+([A-Za-z_]\w*)\s*\(',
        re.MULTILINE,
    )
    owners: dict[str, set[str]] = {}
    for path in backend_files:
        for name in function_pattern.findall(path.read_text(encoding='utf-8')):
            owners.setdefault(name, set()).add(path.name)

    duplicates = {name: sorted(paths) for name, paths in owners.items() if len(paths) > 1}
    assert duplicates == {}


def test_validate_workflow_has_dedicated_unity_job() -> None:
    workflow = (ENGINE / '.github/workflows/validate.yml').read_text(encoding='utf-8')
    match = re.search(r'^  unity:\n(?P<body>.*?)(?=^  [a-z][a-z0-9-]*:\n|\Z)', workflow, re.MULTILINE | re.DOTALL)
    assert match is not None
    body = match.group('body')
    assert 'FO_UNITY_BUILD: ON' in body
    assert "FO_UNITY_BUILD_BATCH_SIZE: '32'" in body
    assert 'python3 -m pytest -q BuildTools/tests/test_unity_build.py' in body
    assert 'BuildTools/validate.sh unit-tests' in body
    assert "inputs.job == 'unity'" in body
    assert re.search(r'^          - unity$', workflow, re.MULTILINE)

    default_match = re.search(
        r'^  unit-tests:\n(?P<body>.*?)(?=^  [a-z][a-z0-9-]*:\n|\Z)',
        workflow,
        re.MULTILINE | re.DOTALL,
    )
    assert default_match is not None
    assert 'FO_UNITY_BUILD' not in default_match.group('body')
