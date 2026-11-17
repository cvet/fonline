from pathlib import Path
import os
import shutil
import subprocess
import time

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(not shutil.which('cmake') or not shutil.which('ninja'), reason='CMake and Ninja are required')
@pytest.mark.parametrize('generator', ['Ninja', 'Ninja Multi-Config'])
@pytest.mark.parametrize('changed_input', ['runtime.a', 'runtime.ready'])
def test_application_relinks_after_a_managed_archive_changes(tmp_path: Path, generator: str, changed_input: str) -> None:
    source = tmp_path / 'source with spaces'
    source.mkdir()
    (source / 'main.c').write_text('int main(void) { return 0; }\n', encoding='utf-8')
    (source / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.22)\n'
        'project(ManagedRuntimeLinkProbe C)\n'
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        'set(FO_MANAGED_SCRIPTING ON)\n'
        'set(FO_MANAGED_RUNTIME_ARCHIVES "${CMAKE_BINARY_DIR}/runtime.a")\n'
        'set(FO_DOTNET_DIR "${CMAKE_BINARY_DIR}")\n'
        'set(FO_MONO_READY_MARKER "runtime.ready")\n'
        'file(WRITE "${FO_MANAGED_RUNTIME_ARCHIVES}" "fixture")\n'
        'file(WRITE "${FO_DOTNET_DIR}/${FO_MONO_READY_MARKER}" "ready")\n'
        'add_executable(probe main.c)\n'
        'TrackManagedRuntimeLinkInputs(probe)\n'
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/probe-$<CONFIG>.txt" CONTENT "$<TARGET_FILE:probe>")\n',
        encoding='utf-8',
    )
    build = tmp_path / 'build with spaces'
    configure = subprocess.run(['cmake', '-G', generator, '-S', str(source), '-B', str(build),
                                '-DCMAKE_BUILD_TYPE=Release'], capture_output=True, text=True)
    assert configure.returncode == 0, configure.stdout + configure.stderr
    command = ['cmake', '--build', str(build), '--config', 'Release', '--target', 'probe']
    initial = subprocess.run(command, capture_output=True, text=True)
    assert initial.returncode == 0, initial.stdout + initial.stderr
    probe = Path((build / 'probe-Release.txt').read_text())
    original_mtime = probe.stat().st_mtime_ns
    unchanged = subprocess.run(command, capture_output=True, text=True)
    assert unchanged.returncode == 0, unchanged.stdout + unchanged.stderr
    assert probe.stat().st_mtime_ns == original_mtime, unchanged.stdout + unchanged.stderr
    time.sleep(2.1)
    os.utime(build / changed_input, None)
    relink = subprocess.run(command, capture_output=True, text=True)
    assert relink.returncode == 0, relink.stdout + relink.stderr
    assert probe.stat().st_mtime_ns > original_mtime, relink.stdout + relink.stderr
