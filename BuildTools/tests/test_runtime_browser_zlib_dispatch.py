from __future__ import annotations

from pathlib import Path
import json
import re
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


@pytest.fixture
def runtime(tmp_path: Path) -> Path:
    external = tmp_path / "src/native/external"
    external.mkdir(parents=True)
    (external / "zlib-ng.cmake").write_text(
        'include(FetchContent)\n'
        'FetchContent_Declare(fetchzlibng SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/zlib-ng")\n'
        'FetchContent_MakeAvailable(fetchzlibng)\n', encoding="utf-8"
    )
    return tmp_path


@pytest.mark.skipif(not shutil.which("cmake"), reason="CMake is required")
@pytest.mark.parametrize("browser", [False, True])
def test_only_browser_dispatch_source_loses_optimization(runtime: Path, browser: bool) -> None:
    external = runtime / "src/native/external"
    zlib = external / "zlib-ng"
    zlib.mkdir()
    (zlib / "CMakeLists.txt").write_text(
        'add_library(zlib STATIC IMPORTED GLOBAL)\n'
        'set_source_files_properties(functable.c compress.c PROPERTIES COMPILE_OPTIONS -O3)\n',
        encoding="utf-8",
    )
    modules = runtime / "modules"
    modules.mkdir()
    (modules / "FetchContent.cmake").write_text(
        'function(FetchContent_Declare)\nendfunction()\n'
        'function(FetchContent_MakeAvailable)\n'
        '  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/zlib-ng" zlib-build)\n'
        'endfunction()\n', encoding="utf-8",
    )
    (runtime / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.22)\nproject(ZlibPatch NONE)\n'
        f'set(CLR_CMAKE_TARGET_BROWSER {"ON" if browser else "OFF"})\n'
        'list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules")\n'
        'include(src/native/external/zlib-ng.cmake)\n'
        'foreach(name functable compress)\n'
        '  get_property(options SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/native/external/zlib-ng/${name}.c"\n'
        '    TARGET_DIRECTORY zlib PROPERTY COMPILE_OPTIONS)\n'
        '  file(WRITE "${CMAKE_BINARY_DIR}/${name}.txt" "${options}")\n'
        'endforeach()\n', encoding="utf-8",
    )

    buildtools.patch_runtime_browser_zlib_dispatch(runtime)
    patched = (external / "zlib-ng.cmake").read_bytes()
    buildtools.patch_runtime_browser_zlib_dispatch(runtime)
    assert (external / "zlib-ng.cmake").read_bytes() == patched

    build = runtime / "build"
    result = subprocess.run(
        [shutil.which("cmake"), "-S", str(runtime), "-B", str(build)],
        capture_output=True, text=True, timeout=30,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert (build / "functable.txt").read_text() == ("-O3;-O0" if browser else "-O3")
    assert (build / "compress.txt").read_text() == "-O3"


@pytest.mark.parametrize("contents", ["upstream source changed\n", "FetchContent_MakeAvailable(fetchzlibng)\n" * 2])
def test_patch_rejects_missing_or_ambiguous_anchor_without_writing(runtime: Path, contents: str) -> None:
    source = runtime / "src/native/external/zlib-ng.cmake"
    source.write_text(contents, encoding="utf-8")

    with pytest.raises(SystemExit, match="unique anchor not found"):
        buildtools.patch_runtime_browser_zlib_dispatch(runtime)

    assert source.read_text(encoding="utf-8") == contents


@pytest.mark.skipif(not all(shutil.which(tool) for tool in ("cmake", "ninja", "clang", "llvm-ar", "llvm-objdump")),
                    reason="CMake, Ninja and LLVM with WebAssembly support are required")
def test_patched_cmake_compiles_dispatch_with_valid_call_relocations(runtime: Path, record_testsuite_property) -> None:
    compiler = shutil.which("clang")
    version = subprocess.run([compiler, "--version"], capture_output=True, text=True, check=True)
    record_testsuite_property("wasm_dispatch_compiler", version.stdout.strip())
    external = runtime / "src/native/external"
    zlib = external / "zlib-ng"
    zlib.mkdir()
    shutil.copyfile(BUILDTOOLS_DIR / "tests/fixtures/wasm_pic_dispatch.c", zlib / "functable.c")
    (zlib / "compress.c").write_text("int optimized_implementation(void) { return 42; }\n", encoding="utf-8")
    (zlib / "CMakeLists.txt").write_text(
        "add_library(zlib STATIC functable.c compress.c)\n", encoding="utf-8"
    )
    (runtime / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\nproject(ZlibCompilerProbe C)\n"
        "set(CLR_CMAKE_TARGET_BROWSER ON)\nadd_compile_options(-O3 -fPIC)\n"
        "include(src/native/external/zlib-ng.cmake)\n", encoding="utf-8"
    )
    buildtools.patch_runtime_browser_zlib_dispatch(runtime)
    build = runtime / "build"
    configured = subprocess.run(
        [shutil.which("cmake"), "-S", str(runtime), "-B", str(build), "-G", "Ninja",
         f"-DCMAKE_C_COMPILER={compiler}", f"-DCMAKE_AR={shutil.which('llvm-ar')}",
         "-DCMAKE_SYSTEM_NAME=Generic", "-DCMAKE_C_COMPILER_TARGET=wasm32-unknown-emscripten",
         "-DCMAKE_C_COMPILER_FORCED=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
        capture_output=True, text=True, timeout=30,
    )
    assert configured.returncode == 0, configured.stdout + configured.stderr
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    for entry in commands:
        name = Path(entry["file"]).name
        options = re.findall(r"(?<!\S)-O[0-3szg](?!\S)", entry["command"])
        assert options[-1] == ("-O0" if name == "functable.c" else "-O3"), entry
        record_testsuite_property(name + "_command", entry["command"])

    compiled = subprocess.run([shutil.which("cmake"), "--build", str(build), "--target", "zlib"],
                              capture_output=True, text=True, timeout=30)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    objects = [path for path in build.rglob("functable.c.*") if path.suffix in (".o", ".obj")]
    assert len(objects) == 1, objects
    obj = objects[0]
    dumped = subprocess.run([shutil.which("llvm-objdump"), "-dr", str(obj)],
                            capture_output=True, text=True, timeout=30)
    assert dumped.returncode == 0, dumped.stdout + dumped.stderr
    lines = dumped.stdout.splitlines()
    assert any("call_indirect" in line or "R_WASM_FUNCTION_INDEX_LEB" in line for line in lines), dumped.stdout
    assert not any("call" in lines[index - 1] and "R_WASM_GLOBAL_INDEX_LEB" in line
                   for index, line in enumerate(lines) if index > 0), dumped.stdout
