from pathlib import Path
import os
import shutil
import subprocess
import sys
import time

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(os.name != 'nt' or not all(shutil.which(tool) for tool in ('cmake', 'cl', 'lib')),
                    reason='Windows Visual Studio compiler environment is required')
@pytest.mark.parametrize('changed_input', ['archive', 'marker'])
def test_visual_studio_relinks_after_published_runtime_archive_changes(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, changed_input: str) -> None:
    monkeypatch.delenv('FO_MANAGED_RUNTIME_PREBUILT', raising=False)
    version = os.environ.get('VisualStudioVersion', '')
    generator = {'18.0': 'Visual Studio 18 2026', '17.0': 'Visual Studio 17 2022'}.get(version)
    if generator is None:
        pytest.skip('A supported Visual Studio developer environment is required')
    source = tmp_path / 'source with spaces'
    engine = source / 'Engine'
    (engine / 'ThirdParty').mkdir(parents=True)
    (engine / 'ThirdParty/dotnet-runtime').write_text('test-runtime\n', encoding='utf-8')
    (engine / 'BuildTools').mkdir()
    (engine / 'BuildTools/buildtools.py').write_text(
        'import os, subprocess, sys\nfrom pathlib import Path\n'
        "workspace = Path(os.environ['FO_WORKSPACE'])\n"
        '_, target_os, arch, configuration = sys.argv[1:]\n'
        "triplet = f'{target_os}.{arch}.{configuration}'\n"
        "library = workspace / 'output/mono' / triplet / 'lib'\n"
        'library.mkdir(parents=True, exist_ok=True)\n'
        "source = workspace / 'runtime.c'\n"
        "source.write_text('int runtime_probe(void) { return 42; }\\n')\n"
        "obj = workspace / 'runtime.obj'\n"
        "subprocess.run(['cl', '/nologo', '/c', str(source), '/Fo' + str(obj)], check=True)\n"
        "names = ['monosgen-2.0', 'mono-component-debugger-stub-static', 'mono-component-diagnostics_tracing-stub-static',\n"
        "         'mono-component-hot_reload-stub-static', 'mono-component-marshal-ilgen-stub-static', 'minipal']\n"
        'for name in names:\n'
        "    subprocess.run(['lib', '/nologo', '/OUT:' + str(library / f'{name}.lib'), str(obj)], check=True)\n"
        "suffix = '_mono_runtime_corelib_libs_native_sfx_nogl_overridable_allocators_embedded_debug_info_isa_fallback_suspend_retry_win7_stack_bounds'\n"
        "(workspace / f'READY_test-runtime_{triplet}{suffix}').touch()\n", encoding='utf-8')
    stage = (BUILDTOOLS_DIR / 'cmake/stages/ThirdParty.cmake').read_text(encoding='utf-8')
    (source / 'managed.cmake').write_text(stage[stage.index('# Managed scripting runtime (Mono)'):], encoding='utf-8')
    (source / 'main.c').write_text('extern int runtime_probe(void); int main(void) { return runtime_probe() != 42; }\n', encoding='utf-8')
    (source / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.22)\nproject(ManagedRuntimeVisualStudio C)\n'
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        'set(FO_MANAGED_SCRIPTING ON)\nset(FO_WINDOWS ON)\nset(FO_ENGINE_ROOT Engine)\n'
        'set(FO_MONO_OS windows)\nset(FO_MONO_ARCH x64)\nset(expr_DebugBuild "$<CONFIG:Debug>")\n'
        'include("${CMAKE_CURRENT_SOURCE_DIR}/managed.cmake")\n'
        'add_executable(probe main.c)\ntarget_link_libraries(probe PRIVATE ${FO_COMMON_SYSTEM_LIBS})\n'
        'add_dependencies(probe SetupManagedRuntime)\nTrackManagedRuntimeLinkInputs(probe)\n'
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/probe-$<CONFIG>.txt" CONTENT "$<TARGET_FILE:probe>")\n',
        encoding='utf-8')
    build = tmp_path / 'build with spaces'
    configure = subprocess.run(['cmake', '-G', generator, '-S', str(source), '-B', str(build),
                                f'-DPython3_EXECUTABLE={Path(sys.executable).as_posix()}'], capture_output=True, text=True)
    assert configure.returncode == 0, configure.stdout + configure.stderr
    command = ['cmake', '--build', str(build), '--config', 'Release', '--target', 'probe']
    initial = subprocess.run(command, capture_output=True, text=True)
    assert initial.returncode == 0, initial.stdout + initial.stderr
    probe = Path((build / 'probe-Release.txt').read_text())
    original_mtime = probe.stat().st_mtime_ns
    unchanged = subprocess.run(command, capture_output=True, text=True)
    assert unchanged.returncode == 0, unchanged.stdout + unchanged.stderr
    assert probe.stat().st_mtime_ns == original_mtime
    archive = build / 'dotnet/output/mono/windows.x64.Release/lib/monosgen-2.0.lib'
    assert archive.is_file()
    markers = list((build / 'dotnet').glob('READY_test-runtime_windows.x64.Release_*'))
    assert len(markers) == 1
    time.sleep(2.1)
    os.utime(archive if changed_input == 'archive' else markers[0], None)
    relink = subprocess.run(command, capture_output=True, text=True)
    assert relink.returncode == 0, relink.stdout + relink.stderr
    assert probe.stat().st_mtime_ns > original_mtime, relink.stdout + relink.stderr
