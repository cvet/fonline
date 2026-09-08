"""Collect real LLVM profiles from unit and quick-exit integration executables."""

from __future__ import annotations

import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILD_TOOLS = Path(__file__).resolve().parents[1]
TOOL = BUILD_TOOLS / "codecoverage.py"
QUICK_EXIT = BUILD_TOOLS / "cmake/helpers/CoverageQuickExit.c"


def run(command, *, env=None, check=True):
    result = subprocess.run(command, env=env, text=True, capture_output=True, timeout=45)
    if check:
        assert result.returncode == 0, result.stdout + result.stderr
        assert not result.stderr.strip(), result.stderr
    return result


def load_collector():
    spec = importlib.util.spec_from_file_location("codecoverage_under_test", TOOL)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def compiled(tmp_path):
    clang = shutil.which("clang-20")
    clangxx = shutil.which("clang++-20")
    if not all((clang, clangxx, shutil.which("llvm-cov-20"), shutil.which("llvm-profdata-20"))):
        pytest.skip("Clang/LLVM 20 required for the real profile fixture")
    root = tmp_path / "workspace with spaces"
    common = root / "Engine/Source/Common/Shared.cpp"
    bridge = root / "Engine/Source/Scripting/Managed/Bridge.cpp"
    common.parent.mkdir(parents=True)
    bridge.parent.mkdir(parents=True)
    common.write_text("""int shared(int value)
{
    if (value < 0)
        return -value;
    return value + 10;
}
""")
    bridge.write_text("""int bridge(int code)
{
    if (code == 42)
        return 7;
    return -1;
}
int untested_bridge(int value)
{
    return value * 2;
}
""")
    unit_source = root / "unit.cpp"
    unit_source.write_text("int shared(int); int main() { return shared(1) == 11 ? 0 : 1; }\n")
    server_source = root / "server.cpp"
    server_source.write_text("""#include <cstdlib>
int shared(int); int bridge(int);
int main() { std::quick_exit(shared(-3) == 3 && bridge(42) == 7 ? 0 : 2); }
""")
    flags = ["-O0", "-g", "-fprofile-instr-generate", "-fcoverage-mapping", "-Wall", "-Wextra", "-Werror"]
    common_object = root / "common.o"
    quick_object = root / "quick.o"
    run([clangxx, *flags, "-c", str(common), "-o", str(common_object)])
    run([clang, *flags, "-DFO_CODE_COVERAGE_LLVM=1", "-DFO_CODE_COVERAGE_GCC=0", "-c",
         str(QUICK_EXIT), "-o", str(quick_object)])
    unit = root / "unit"
    server = root / "server"
    plain_server = root / "server-without-flush"
    run([clangxx, *flags, str(unit_source), str(common_object), "-o", str(unit)])
    for binary, objects in ((server, [quick_object]), (plain_server, [])):
        run([clangxx, *flags, str(server_source), str(common_object), str(bridge),
             *map(str, objects), "-o", str(binary)])
    output = root / "coverage"
    args = ["--workspace-root", str(root), "--build-dir", str(root), "--binary", str(unit),
            "--backend", "llvm", "--output-dir", str(output)]
    run([sys.executable, str(TOOL), "full", *args])
    return root, unit, server, plain_server, output, args


def raw_hashes(output):
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (output / "raw").glob("*.profraw")}


def test_multi_object_preserves_unit_and_integration_lines(compiled):
    root, unit, server, _, output, args = compiled
    collector = load_collector()
    lcov = output / "raw/coverage.lcov"
    before = collector.parse_lcov(lcov, root)
    common = "Engine/Source/Common/Shared.cpp"
    bridge = "Engine/Source/Scripting/Managed/Bridge.cpp"
    assert before[common].hits_by_line[4] == 0
    assert bridge not in before
    env = os.environ.copy()
    env["LLVM_PROFILE_FILE"] = str(output / "raw/integration-%m-%p.profraw")
    run([str(server)], env=env)
    profiles = raw_hashes(output)
    assert len(profiles) == 2
    assert all(p.stat().st_size > 0 for p in (output / "raw").glob("*.profraw"))

    missing_object = run([sys.executable, str(TOOL), "report", *args], check=False)
    assert missing_object.returncode != 0, "A profile from an omitted binary must fail instead of dropping its mapping"
    assert "Binary ID" in missing_object.stderr or "binary ID" in missing_object.stderr
    run([sys.executable, str(TOOL), "report", *args, "--object", str(server)])
    merged = collector.parse_lcov(lcov, root)
    assert raw_hashes(output) == profiles, "Report must not reset either run's profiles"
    assert merged[common].total_lines == before[common].total_lines
    assert merged[common].hits_by_line[4] > 0
    assert merged[common].hits_by_line[5] > 0
    assert merged[bridge].hits_by_line[4] > 0
    assert merged[bridge].hits_by_line[5] == 0
    assert merged[bridge].hits_by_line[9] == 0
    assert merged[bridge].covered_lines < merged[bridge].total_lines

    run([sys.executable, str(TOOL), "report", *args,
         "--object", str(server), "--object", str(unit), "--object", str(server)])
    repeated = collector.parse_lcov(lcov, root)
    assert {p: c.hits_by_line for p, c in repeated.items()} == {p: c.hits_by_line for p, c in merged.items()}


def test_quick_exit_requires_flush_and_propagates_write_failure(compiled):
    root, _, server, plain_server, _, _ = compiled
    env = os.environ.copy()
    env["LLVM_PROFILE_FILE"] = str(root / "plain.profraw")
    run([str(plain_server)], env=env)
    assert (root / "plain.profraw").stat().st_size == 0
    env["LLVM_PROFILE_FILE"] = str(root / "flushed.profraw")
    run([str(server)], env=env)
    assert (root / "flushed.profraw").stat().st_size > 0
    blocker = root / "not-a-directory"
    blocker.write_text("blocked")
    env["LLVM_PROFILE_FILE"] = str(blocker / "failed.profraw")
    result = run([str(server)], env=env, check=False)
    assert result.returncode != 0
    assert "Profile Error" in result.stderr


@pytest.mark.parametrize("problem", ["missing-object", "invalid-object", "invalid-profile", "wrong-backend", "wrong-command"])
def test_report_rejects_incomplete_inputs(compiled, problem):
    root, _, _, _, output, args = compiled
    command = "report"
    extra = []
    if problem in ("missing-object", "invalid-object"):
        bad_object = root / "bad-object"
        if problem == "invalid-object":
            bad_object.write_text("not a coverage executable")
        extra = ["--object", str(bad_object)]
    elif problem == "invalid-profile":
        (output / "raw/broken.profraw").write_text("invalid profile")
    elif problem == "wrong-backend":
        args = ["gcc" if value == "llvm" else value for value in args]
        extra = ["--object", str(root / "server")]
    else:
        command = "run"
        extra = ["--object", str(root / "server")]
    before = raw_hashes(output)
    result = run([sys.executable, str(TOOL), command, *args, *extra], check=False)
    assert result.returncode != 0, result.stdout + result.stderr
    assert raw_hashes(output) == before


@pytest.mark.parametrize("platform", ["LINUX", "MAC", "IOS", "ANDROID", "WEB"])
def test_cmake_coverage_companions_reuse_libraries_and_scope_quick_exit(tmp_path, platform):
    if not all(shutil.which(name) for name in ("cmake", "ninja", "clang-20", "clang++-20")):
        pytest.skip("CMake/Ninja and Clang 20 required")
    root = tmp_path / "project"
    source = root / "Engine/Source/Applications"
    source.mkdir(parents=True)
    helper = root / "Engine/BuildTools/cmake/helpers/CoverageQuickExit.c"
    helper.parent.mkdir(parents=True)
    shutil.copy2(QUICK_EXIT, helper)
    (root / "core.cpp").write_text("int core() { return 7; }\n")
    for name in ("TestingApp", "ServerHeadlessApp", "BakerApp", "ManagedScriptBakerApp"):
        body = "return core() == 7 ? 0 : 1;" if name == "TestingApp" else "std::quick_exit(core() == 7 ? 0 : 1);"
        (source / f"{name}.cpp").write_text(f"#include <cstdlib>\nint core(); int main() {{ {body} }}\n")
    cmake = r'''
cmake_minimum_required(VERSION 3.22)
project(CoverageCompanions C CXX)
add_compile_options(-O0 -fprofile-instr-generate -fcoverage-mapping -Wall -Wextra -Werror)
add_link_options(-fprofile-instr-generate)
add_library(Core STATIC core.cpp)
foreach(coreLib IN ITEMS ServerLib ClientLib BakerLib MapperLib AppHeadless)
    add_library(${coreLib} INTERFACE)
    target_link_libraries(${coreLib} INTERFACE Core)
endforeach()
macro(StatusMessage)
endmacro()
macro(SetValue name)
    set(${name} ${ARGN})
endmacro()
function(AddCommandTarget)
endfunction()
function(AddExecutableApplication target source)
    cmake_parse_arguments(APP "WIN32;WRITE_BUILD_HASH" "OUTPUT_DIR;WORKING_DIRECTORY;OUTPUT_NAME;TESTING_APP;HEADLESS_APP" "LINK_LIBS;DEPENDS;EXTRA_SOURCES" ${ARGN})
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE ${APP_LINK_LIBS})
endfunction()
macro(TargetCompileDefinitions target)
    target_compile_definitions(${target} ${ARGN})
endmacro()
set(FO_DEV_NAME Probe)
set(FO_ENGINE_ROOT Engine)
set(FO_CODE_COVERAGE ON)
set(FO_MANAGED_SCRIPTING ON)
set(FO_BUILD_BAKER_LIB ON)
set(FO_@PLATFORM@ ON)
include("@STAGE@")
foreach(name IN ITEMS Probe_CodeCoverage Probe_ServerHeadless Probe_Baker Probe_ManagedScriptBaker)
    get_target_property(sources ${name} SOURCES)
    file(WRITE "${CMAKE_BINARY_DIR}/${name}.sources" "${sources}")
endforeach()
if(TARGET Probe_BakerLib OR TARGET Probe_Server)
    message(FATAL_ERROR "Coverage companions must not enable plugin or windowed targets")
endif()
'''
    stage = BUILD_TOOLS / "cmake/stages/Applications.cmake"
    (root / "CMakeLists.txt").write_text(cmake.replace("@PLATFORM@", platform).replace("@STAGE@", str(stage)))
    build = root / "build"
    run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
         "-DCMAKE_C_COMPILER=clang-20", "-DCMAKE_CXX_COMPILER=clang++-20"])
    run(["cmake", "--build", str(build), "--parallel", "2"])
    for name in ("Probe_ServerHeadless", "Probe_Baker", "Probe_ManagedScriptBaker"):
        sources = (build / f"{name}.sources").read_text()
        assert ("CoverageQuickExit.c" in sources) == (platform == "LINUX")
        if platform == "LINUX":
            env = os.environ.copy()
            env["LLVM_PROFILE_FILE"] = str(build / f"{name}-%p.profraw")
            run([str(build / name)], env=env)
            assert all(p.stat().st_size > 0 for p in build.glob(f"{name}-*.profraw"))
            assert len(list(build.glob(f"{name}-*.profraw"))) == 1
    assert "CoverageQuickExit.c" not in (build / "Probe_CodeCoverage.sources").read_text()
    assert len(list(build.glob("CMakeFiles/Core.dir/*.o"))) == 1


def test_gcc_quick_exit_dumps_native_counters(tmp_path):
    if not shutil.which("gcc") or not shutil.which("g++"):
        pytest.skip("GCC required")
    main = tmp_path / "main.cpp"
    main.write_text("#include <cstdlib>\nint main() { std::quick_exit(0); }\n")
    hook = tmp_path / "hook.o"
    run(["gcc", "--coverage", "-Wall", "-Wextra", "-Werror", "-DFO_CODE_COVERAGE_LLVM=0",
         "-DFO_CODE_COVERAGE_GCC=1", "-c", str(QUICK_EXIT), "-o", str(hook)])
    binary = tmp_path / "app"
    run(["g++", "--coverage", str(main), str(hook), "-o", str(binary)])
    run([str(binary)])
    assert len(list(tmp_path.glob("*.gcda"))) == 2
    assert all(p.stat().st_size > 0 for p in tmp_path.glob("*.gcda"))
