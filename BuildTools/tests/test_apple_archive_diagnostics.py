from __future__ import annotations

import json
import re
import shlex
from pathlib import Path
import shutil
import subprocess

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ARCHIVE_OPTION = "-no_warning_for_no_symbols"


def _tool(name: str) -> str | None:
    return shutil.which(f"{name}-20") or shutil.which(name)


def _configure_flags(tmp_path: Path, system: str, generator: str = "Xcode") -> dict[str, str]:
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(AppleArchiveDiagnostics NONE)\n"
        f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")\n'
        f"set(CMAKE_SYSTEM_NAME {system})\n"
        + ("set(CMAKE_GENERATOR Xcode)\n" if generator == "Xcode" else "")
        +
        "set(CMAKE_SYSTEM_PROCESSOR x86_64)\n"
        "set(CMAKE_CXX_COMPILER_ID Clang)\n"
        "set(CMAKE_CXX_COMPILER_VERSION 20.0)\n"
        "set(CMAKE_SIZEOF_VOID_P 8)\n"
        "set(PLATFORM SIMULATOR64)\n"
        "set(FO_HEADLESS_ONLY ON)\n"
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
        'set(CMAKE_STATIC_LINKER_FLAGS "-existing-archive-option")\n'
        "StartProjectGeneration()\n"
        'foreach(name CMAKE_STATIC_LINKER_FLAGS CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS)\n'
        '  file(APPEND "${CMAKE_BINARY_DIR}/flags.txt" "${name}=${${name}}\\n")\n'
        'endforeach()\n',
        encoding="utf-8",
    )
    result = subprocess.run(
        ["cmake", "-G", "Unix Makefiles" if generator == "Xcode" else generator, "-S", str(tmp_path), "-B", str(tmp_path / "build")],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "CMake Warning" not in result.stdout + result.stderr
    return dict(line.split("=", 1) for line in (tmp_path / "build" / "flags.txt").read_text().splitlines())


@pytest.mark.skipif(not shutil.which("cmake"), reason="CMake is required")
@pytest.mark.parametrize("system", ["Darwin", "iOS", "Linux"])
@pytest.mark.parametrize("generator", ["Xcode", "Ninja", "Unix Makefiles"])
def test_archive_option_is_apple_only_and_never_a_compiler_or_dynamic_linker_flag(tmp_path: Path, system: str, generator: str) -> None:
    flags = _configure_flags(tmp_path, system, generator)

    expected = ["-existing-archive-option"]
    if system != "Linux" and generator == "Xcode":
        expected.append(ARCHIVE_OPTION)
    assert flags.pop("CMAKE_STATIC_LINKER_FLAGS").split() == expected
    assert all(ARCHIVE_OPTION not in value for value in flags.values())


@pytest.mark.skipif(
    not all(_tool(name) for name in ("clang", "llvm-libtool-darwin", "llvm-nm", "llvm-ar")) or not shutil.which("cmake"),
    reason="CMake and LLVM Mach-O tools are required",
)
@pytest.mark.parametrize("archive_route", ["xcode", "ios-c", "ios-cxx"])
def test_apple_archive_keeps_empty_members_and_real_symbols_without_hiding_invalid_inputs(tmp_path: Path, archive_route: str) -> None:
    flags = _configure_flags(tmp_path, "iOS")["CMAKE_STATIC_LINKER_FLAGS"].split()
    flags.remove("-existing-archive-option")
    clang = _tool("clang")
    libtool = _tool("llvm-libtool-darwin")
    (tmp_path / "empty.c").write_text("typedef int module_anchor;\n")
    (tmp_path / "real.c").write_text("int apple_archive_probe(void) { return 37; }\n")
    for name in ("empty", "real"):
        result = subprocess.run(
            [clang, "-target", "x86_64-apple-ios26.0-simulator", "-c", str(tmp_path / f"{name}.c"), "-o", str(tmp_path / f"{name}.o")],
            capture_output=True, text=True,
        )
        assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr

    def archive(name: str, options: list[str], inputs: list[str], actual_rule: bool = False) -> subprocess.CompletedProcess[str]:
        command = [libtool, "-static", "-warnings_as_errors", *options, "-o", str(tmp_path / name), *inputs]
        if actual_rule and archive_route != "xcode":
            language = archive_route.removeprefix("ios-").upper()
            toolchain = (BUILDTOOLS_DIR / "cmake/toolchains/ios.toolchain.cmake").read_text()
            match = re.search(rf'set\(CMAKE_{language}_CREATE_STATIC_LIBRARY\s+"([^"\n]+)"\)', toolchain)
            assert match is not None
            tokens = shlex.split(match.group(1).replace("${BUILD_LIBTOOL}", shlex.quote(libtool)))
            substitutions = {"<TARGET>": [str(tmp_path / name)], "<LINK_FLAGS>": ["-warnings_as_errors"], "<OBJECTS>": inputs}
            command = [arg for token in tokens for arg in substitutions.get(token, [token])]
        return subprocess.run(command, capture_output=True, text=True)

    objects = [str(tmp_path / name) for name in ("empty.o", "real.o")]
    negative = archive("negative.a", [], objects)
    assert negative.returncode != 0 and "has no symbols" in negative.stderr
    positive = archive("positive.a", flags, objects, actual_rule=True)
    assert positive.returncode == 0 and not positive.stderr, positive.stdout + positive.stderr
    symbols = subprocess.run([_tool("llvm-nm"), str(tmp_path / "positive.a")], capture_output=True, text=True)
    assert symbols.returncode == 0 and "T _apple_archive_probe" in symbols.stdout
    members = subprocess.run([_tool("llvm-ar"), "t", str(tmp_path / "positive.a")], capture_output=True, text=True)
    assert members.returncode == 0 and members.stdout.splitlines() == ["empty.o", "real.o"]

    (tmp_path / "malformed.o").write_text("This is not a Mach-O object\n")
    invalid_results = {}
    for name in ("missing.o", "malformed.o"):
        result = archive("invalid.a", flags, [str(tmp_path / name)], actual_rule=True)
        assert result.returncode != 0 and "error:" in result.stderr, result.stdout + result.stderr
        invalid_results[name] = {"returncode": result.returncode, "stderr": result.stderr}
    (tmp_path / "diagnostics.json").write_text(json.dumps({
        "negative": {"returncode": negative.returncode, "stderr": negative.stderr},
        "positive": {"returncode": positive.returncode, "stderr": positive.stderr},
        "symbols": symbols.stdout, "members": members.stdout, "invalid_inputs": invalid_results,
    }, indent=2))
