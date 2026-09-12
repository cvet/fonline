from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(
    os.name == "nt" or not all(shutil.which(tool) for tool in ("cmake", "ninja", "cc", "ar")),
    reason="POSIX CMake, Ninja, a C compiler and archiver are required",
)
@pytest.mark.parametrize("generator", ["Ninja", "Ninja Multi-Config"])
def test_ninja_knows_the_managed_archive_producer_before_bootstrap(tmp_path: Path, generator: str) -> None:
    source = tmp_path / "project with spaces"
    engine = source / "Engine"
    (engine / "ThirdParty").mkdir(parents=True)
    (engine / "ThirdParty/dotnet-runtime").write_text("test-runtime\n")
    (engine / "BuildTools").mkdir()
    (engine / "BuildTools/buildtools.py").write_text(
        "import json, os, subprocess, sys\nfrom pathlib import Path\n"
        "workspace = Path(os.environ['FO_WORKSPACE'])\n"
        "_, target_os, arch, configuration = sys.argv[1:]\n"
        "triplet = f'{target_os}.{arch}.{configuration}'\n"
        "library = workspace / 'output/mono' / triplet / 'lib'\n"
        "library.mkdir(parents=True, exist_ok=True)\n"
        "source = workspace / 'runtime.c'\n"
        "source.write_text('int runtime_probe(void) { return 42; }\\n')\n"
        "obj = workspace / 'runtime.o'\n"
        f"subprocess.run([{shutil.which('cc')!r}, '-fPIC', '-c', str(source), '-o', str(obj)], check=True)\n"
        "names = ['monosgen-2.0', 'mono-component-debugger-stub-static', 'mono-component-diagnostics_tracing-stub-static',\n"
        "         'mono-component-hot_reload-stub-static', 'mono-component-marshal-ilgen-stub-static',\n"
        "         'System.Native', 'System.Globalization.Native', 'minipal']\n"
        "for name in names:\n"
        f"    subprocess.run([{shutil.which('ar')!r}, 'rcs', str(library / f'lib{{name}}.a'), str(obj)], check=True)\n"
        "(workspace / f'READY_test-runtime_{triplet}_mono_runtime_corelib_libs_native_nogl').touch()\n"
        "with (workspace / 'invocations.jsonl').open('a') as output:\n"
        "    output.write(json.dumps({'configuration': configuration, 'triplet': triplet}) + '\\n')\n"
    )
    stage = (BUILDTOOLS_DIR / "cmake/stages/ThirdParty.cmake").read_text()
    managed_stage = stage[stage.index("# Managed scripting runtime (Mono)"):]
    (source / "probe.c").write_text("extern int runtime_probe(void); int probe(void) { return runtime_probe(); }\n")
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\nproject(ManagedRuntimeByproducts C)\n"
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        "set(FO_MANAGED_SCRIPTING ON)\nset(FO_ENGINE_ROOT Engine)\nset(FO_MONO_OS linux)\nset(FO_MONO_ARCH x64)\n"
        'set(expr_DebugBuild "$<CONFIG:Debug>")\n'
        'include("${MANAGED_STAGE}")\n'
        "add_library(probe SHARED probe.c)\ntarget_link_libraries(probe PRIVATE ${FO_COMMON_SYSTEM_LIBS})\n"
        "add_dependencies(probe SetupManagedRuntime)\n"
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/archives-$<CONFIG>.txt" CONTENT "$<JOIN:${FO_MANAGED_RUNTIME_ARCHIVES},\n>")\n'
    )
    results = []
    for variant in ("before", "after"):
        stage_path = source / f"{variant}.cmake"
        stage_path.write_text(managed_stage.replace("        BYPRODUCTS ${FO_MANAGED_RUNTIME_ARCHIVES}\n", "") if variant == "before" else managed_stage)
        for configuration in ("Debug", "Release"):
            build = tmp_path / f"{variant}-{configuration}"
            configure_command = [
                shutil.which("cmake"), "-G", generator, "-S", str(source), "-B", str(build),
                f"-DMANAGED_STAGE={stage_path}", f"-DPython3_EXECUTABLE={sys.executable}",
            ]
            if generator == "Ninja":
                configure_command.append(f"-DCMAKE_BUILD_TYPE={configuration}")
            configure = subprocess.run(configure_command, capture_output=True, text=True)
            assert configure.returncode == 0, configure.stdout + configure.stderr
            assert "CMake Warning" not in configure.stdout + configure.stderr
            command = [shutil.which("cmake"), "--build", str(build), "--config", configuration, "--target", "probe"]
            result = subprocess.run(command, capture_output=True, text=True)
            invocation_file = build / "dotnet/invocations.jsonl"
            results.append({"variant": variant, "configuration": configuration, "returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
            if variant == "before":
                assert result.returncode != 0 and "no known rule to make it" in result.stderr, result.stdout + result.stderr
                assert "libSystem.Native.a" in result.stderr
                assert not invocation_file.exists()
                continue
            assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
            invocations = [json.loads(line) for line in invocation_file.read_text().splitlines()]
            assert invocations == [{"configuration": configuration, "triplet": f"linux.x64.{configuration}"}]
            archives = (build / f"archives-{configuration}.txt").read_text().splitlines()
            assert len(archives) == 8 and all(Path(path).is_file() for path in archives)
            repeat = subprocess.run(command, capture_output=True, text=True)
            assert repeat.returncode == 0 and not repeat.stderr, repeat.stdout + repeat.stderr
            assert [json.loads(line) for line in invocation_file.read_text().splitlines()] == invocations
    (tmp_path / "results.json").write_text(json.dumps(results, indent=2))
