from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
CLANG_CL = shutil.which("clang-cl-20") or shutil.which("clang-cl")
LLVM_AR = shutil.which("llvm-lib-20") or shutil.which("llvm-lib")
LLVM_READOBJ = shutil.which("llvm-readobj-20") or shutil.which("llvm-readobj")


@pytest.mark.skipif(
    not all((shutil.which("cmake"), shutil.which("ninja"), CLANG_CL, LLVM_AR, LLVM_READOBJ)),
    reason="CMake, Ninja and LLVM COFF tools are required",
)
@pytest.mark.parametrize(
    ("role", "managed", "configuration", "arch", "runtime_flag", "runtime_library"),
    [
        ("SERVER", True, "Release", "x86_64", "/MT", "libcmt.lib"),
        ("SERVER", True, "Debug", "x86_64", "/MTd", "libcmtd.lib"),
        ("SERVER", True, "Debug_Profiling_Total", "x86_64", "/MTd", "libcmtd.lib"),
        ("MAPPER", True, "Release", "x86_64", "/MT", "libcmt.lib"),
        ("SERVER", True, "Release", "i686", "/MT", "libcmt.lib"),
        ("CLIENT", False, "Release", "x86_64", "/MT", "libcmt.lib"),
        ("SERVER", False, "Release", "x86_64", "/MD", "msvcrt.lib"),
        ("SERVER", False, "Debug", "x86_64", "/MDd", "msvcrtd.lib"),
    ],
)
def test_windows_hosts_select_one_matching_crt(
    tmp_path: Path, role: str, managed: bool, configuration: str, arch: str,
    runtime_flag: str, runtime_library: str,
) -> None:
    (tmp_path / "probe.cpp").write_text("int runtime_contract_probe() { return 42; }\n", encoding="utf-8")
    vendor = tmp_path / "vendor"
    vendor.mkdir()
    (vendor / "CMakeLists.txt").write_text(
        'add_library(vendor_probe OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/../probe.cpp")\n', encoding="utf-8",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(WindowsRuntimeContract CXX)\n"
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        f"set(FO_MANAGED_SCRIPTING {'ON' if managed else 'OFF'})\n"
        f"set(FO_BUILD_{role} ON)\n"
        "set(FO_ENGINE_ROOT Engine)\n"
        "set(FO_MAIN_CONFIG Test.fomain)\n"
        "set(FO_DEV_NAME Test)\n"
        "set(FO_NICE_NAME Test)\n"
        "set(FO_GEOMETRY HEXAGONAL)\n"
        "set(FO_MAP_HEX_WIDTH 32)\n"
        "set(FO_MAP_HEX_HEIGHT 16)\n"
        "set(FO_MAP_CAMERA_ANGLE 30)\n"
        "set(FO_APP_ICON test.ico)\n"
        'set(FO_OUTPUT_PATH "${CMAKE_BINARY_DIR}/output")\n'
        "StartProjectGeneration()\n"
        "add_library(crt_probe OBJECT probe.cpp)\n"
        "add_subdirectory(vendor)\n"
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/effective-options-$<COMPILE_LANGUAGE>.txt"\n'
        '    CONTENT "$<TARGET_GENEX_EVAL:crt_probe,$<TARGET_PROPERTY:crt_probe,COMPILE_OPTIONS>>" TARGET crt_probe)\n',
        encoding="utf-8",
    )
    build = tmp_path / "build"
    configure = subprocess.run(
        [
            "cmake", "-S", str(tmp_path), "-B", str(build), "-G", "Ninja",
            "-DCMAKE_SYSTEM_NAME=Windows", f"-DCMAKE_SYSTEM_PROCESSOR={arch}",
            f"-DCMAKE_CXX_COMPILER={CLANG_CL}", f"-DCMAKE_AR={LLVM_AR}",
            f"-DCMAKE_CXX_COMPILER_TARGET={arch}-pc-windows-msvc",
            "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY", f"-DCMAKE_BUILD_TYPE={configuration}",
        ],
        capture_output=True, text=True,
    )
    assert configure.returncode == 0, configure.stdout + configure.stderr
    assert not re.search(r"\bwarning\b", configure.stdout + configure.stderr, re.IGNORECASE)
    options = (build / "effective-options-CXX.txt").read_text().split(";")
    assert [option for option in options if re.fullmatch(r"/M[DT]d?", option)] == [runtime_flag]

    compile_result = subprocess.run(["cmake", "--build", str(build)], capture_output=True, text=True)
    assert compile_result.returncode == 0, compile_result.stdout + compile_result.stderr
    assert not re.search(r"\bwarning\b", compile_result.stdout + compile_result.stderr, re.IGNORECASE)
    for relative in ("CMakeFiles/crt_probe.dir/probe.cpp.obj", "vendor/CMakeFiles/vendor_probe.dir/__/probe.cpp.obj"):
        directives = subprocess.run(
            [str(LLVM_READOBJ), "--coff-directives", str(build / relative)], capture_output=True, text=True,
        )
        assert directives.returncode == 0, directives.stdout + directives.stderr
        libraries = re.findall(r"/DEFAULTLIB:([^\s]+)", directives.stdout, re.IGNORECASE)
        assert {library.lower() for library in libraries} == {runtime_library, "oldnames.lib"}, directives.stdout
