from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]


def _tool(name: str) -> str | None:
    return shutil.which(f"{name}-20") or shutil.which(name)


@pytest.mark.skipif(
    not shutil.which("cmake") or not all(_tool(name) for name in ("clang", "ld64.lld", "llvm-ar")),
    reason="CMake and LLVM Mach-O tools are required",
)
@pytest.mark.parametrize(
    ("architecture", "target", "platform", "runtime_os"),
    [
        ("arm64", "arm64-apple-macos12.0", "macos", "osx"),
        ("x86_64", "x86_64-apple-macos12.0", "macos", "osx"),
        ("arm64", "arm64-apple-ios12.2", "ios", "ios"),
        ("x86_64", "x86_64-apple-ios12.2-simulator", "ios-simulator", "iossimulator"),
    ],
)
def test_managed_headless_apple_link_has_its_own_runtime_dependencies(
    tmp_path: Path, architecture: str, target: str, platform: str, runtime_os: str,
) -> None:
    clang, linker, archiver = (_tool(name) for name in ("clang", "ld64.lld", "llvm-ar"))
    source = tmp_path / "project with spaces"
    build = tmp_path / "build with spaces"
    source.mkdir()
    sdk = tmp_path / "SDK with spaces"
    (sdk / "usr/lib").mkdir(parents=True)
    commands = []

    def run(command: list[str]) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(command, capture_output=True, text=True)
        commands.append({"command": command, "returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
        return result

    def object_file(name: str, body: str) -> Path:
        path = source / f"{name}.c"
        path.write_text(body + "\n")
        obj = path.with_suffix(".o")
        result = run([clang, "-target", target, "-ffreestanding", "-fno-stack-protector", "-Werror", "-c", str(path), "-o", str(obj)])
        assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
        return obj

    # These SDK symbol fixtures leave actual archive extraction and Apple dependency resolution to ld64
    system_libraries = {
        "usr/lib/libSystem.dylib": 'void binder(void) __asm__("dyld_stub_binder"); void binder(void) { }',
        "usr/lib/libobjc.dylib": "void* objc_getClass(const char* name) { return (void*)name; }",
        "System/Library/Frameworks/Foundation.framework/Foundation": "void* NSTemporaryDirectory(void) { return (void*)0; }",
        "System/Library/Frameworks/CoreFoundation.framework/CoreFoundation": "long CFDictionaryGetCount(void* value) { return value != (void*)0; }",
    }
    for index, (relative, body) in enumerate(system_libraries.items()):
        output = sdk / relative
        output.parent.mkdir(parents=True, exist_ok=True)
        obj = object_file(f"sdk_{index}", body)
        result = run([
            linker, "-dylib", "-arch", architecture, "-platform_version", platform, "12.0" if platform == "macos" else "12.2", "26.0",
            "-install_name", "/" + relative, "-o", str(output), str(obj),
        ])
        assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr

    runtime_arch = "x64" if architecture == "x86_64" else architecture
    runtime = build / f"dotnet/output/mono/{runtime_os}.{runtime_arch}.Release"
    (runtime / "lib").mkdir(parents=True)
    (runtime / "include/mono-2.0").mkdir(parents=True)
    archives = {
        "monosgen-2.0": 'extern void* objc_getClass(const char*); void* mono_probe(void) { return objc_getClass("NSObject"); }',
        "System.Native": "extern void* NSTemporaryDirectory(void); void* native_probe(void) { return NSTemporaryDirectory(); }",
        "System.Globalization.Native": "extern long CFDictionaryGetCount(void*); long globalization_probe(void) { return CFDictionaryGetCount((void*)0); }",
        "mono-component-debugger-stub-static": "int debugger_probe;",
        "mono-component-diagnostics_tracing-stub-static": "int tracing_probe;",
        "mono-component-hot_reload-stub-static": "int hot_reload_probe;",
        "mono-component-marshal-ilgen-stub-static": "int marshal_probe;",
        "minipal": "int minipal_probe;",
    }
    for index, (name, body) in enumerate(archives.items()):
        obj = object_file(f"runtime_{index}", body)
        result = run([archiver, "rcs", str(runtime / f"lib/lib{name}.a"), str(obj)])
        assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr

    engine = source / "Engine"
    (engine / "ThirdParty").mkdir(parents=True)
    (engine / "ThirdParty/dotnet-runtime").write_text("test-runtime\n")
    stage = (BUILDTOOLS_DIR / "cmake/stages/ThirdParty.cmake").read_text()
    (source / "managed.cmake").write_text(stage[stage.index("# Managed scripting runtime (Mono)"):])
    (source / "client.c").write_text(
        "extern void* mono_probe(void);\nextern void* native_probe(void);\nextern long globalization_probe(void);\n"
        "long client_probe(void) { return (mono_probe() != (void*)0) + (native_probe() != (void*)0) + globalization_probe(); }\n"
    )
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\nproject(ManagedAppleLink C)\n"
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        "set(FO_MANAGED_SCRIPTING ON)\nset(FO_HEADLESS_ONLY ON)\nset(FO_ENGINE_ROOT Engine)\n"
        f"set(FO_MONO_OS {runtime_os})\nset(FO_MONO_ARCH {runtime_arch})\nset(expr_DebugBuild 0)\n"
        f"set({'FO_MAC' if runtime_os == 'osx' else 'FO_IOS'} ON)\n"
        'include("${CMAKE_CURRENT_SOURCE_DIR}/managed.cmake")\n'
        'get_property(directories DIRECTORY PROPERTY LINK_DIRECTORIES)\n'
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/link-directories.txt" CONTENT "${directories}")\n'
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/runtime-libraries.txt" CONTENT "$<JOIN:${FO_COMMON_SYSTEM_LIBS},\n>")\n'
        "add_library(ManagedHeadless SHARED client.c)\n"
        "target_link_libraries(ManagedHeadless PRIVATE ${FO_COMMON_SYSTEM_LIBS})\n"
    )
    configure = run([
        shutil.which("cmake"), "-G", "Unix Makefiles", "-S", str(source), "-B", str(build),
        "-DCMAKE_SYSTEM_NAME=Darwin", f"-DCMAKE_C_COMPILER={clang}", f"-DCMAKE_C_COMPILER_TARGET={target}",
        "-DCMAKE_C_COMPILER_WORKS=TRUE",
        f"-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld={linker} -mlinker-version=1053.12",
        f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_BUILD_TYPE=Release", f"-DPython3_EXECUTABLE={sys.executable}",
    ])
    assert configure.returncode == 0, configure.stdout + configure.stderr
    assert "CMake Warning" not in configure.stdout + configure.stderr
    result = run([shutil.which("cmake"), "--build", str(build), "--target", "ManagedHeadless", "--verbose"])
    (tmp_path / "commands.json").write_text(json.dumps(commands, indent=2))
    assert result.returncode == 0, result.stdout + result.stderr
    assert "warning:" not in result.stdout + result.stderr, result.stdout + result.stderr
    assert (build / "libManagedHeadless.dylib").is_file()
    assert not (build / "link-directories.txt").read_text()
    libraries = (build / "runtime-libraries.txt").read_text().splitlines()
    assert {str(runtime / f"lib/lib{name}.a") for name in archives}.issubset(libraries)
    assert not (runtime / "lib/Release").exists()
