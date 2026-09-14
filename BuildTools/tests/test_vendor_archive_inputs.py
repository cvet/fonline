from __future__ import annotations

from pathlib import Path
import json
import shutil
import subprocess

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]


def _configure(source: Path, build: Path) -> None:
    result = subprocess.run(
        ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja"], capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "CMake Warning" not in result.stdout + result.stderr


@pytest.mark.skipif(not shutil.which("cmake") or not shutil.which("ninja"), reason="CMake and Ninja are required")
@pytest.mark.parametrize("assembly", [False, True])
def test_libressl_archive_inputs_keep_aes_and_tls_functional(tmp_path: Path, assembly: bool) -> None:
    (tmp_path / "probe.c").write_text(
        "#include <string.h>\n#include <openssl/aes.h>\n#include <tls.h>\n"
        "int main(void) {\n"
        "    unsigned char key_bytes[16] = {0};\n"
        "    unsigned char plain[16] = {0};\n"
        "    unsigned char encrypted[16], decrypted[16];\n"
        "    unsigned char expected[16] = {0x66,0xe9,0x4b,0xd4,0xef,0x8a,0x2c,0x3b,0x88,0x4c,0xfa,0x59,0xca,0x34,0x2b,0x2e};\n"
        "    AES_KEY key;\n"
        "    if (AES_set_encrypt_key(key_bytes, 128, &key) != 0) return 1;\n"
        "    AES_encrypt(plain, encrypted, &key);\n"
        "    if (memcmp(encrypted, expected, sizeof(expected)) != 0) return 2;\n"
        "    if (AES_set_decrypt_key(key_bytes, 128, &key) != 0) return 3;\n"
        "    AES_decrypt(encrypted, decrypted, &key);\n"
        "    if (memcmp(decrypted, plain, sizeof(plain)) != 0) return 4;\n"
        "    if (tls_init() != 0) return 5;\n"
        "    struct tls *client = tls_client();\n"
        "    if (client == 0) return 6;\n"
        "    tls_free(client);\n"
        "    return 0;\n}\n",
        encoding="utf-8",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(LibreSSLArchiveInputs C)\n"
        "set(BUILD_SHARED_LIBS OFF)\nset(LIBRESSL_APPS OFF)\nset(LIBRESSL_TESTS OFF)\n"
        "set(LIBRESSL_SKIP_INSTALL ON)\nset(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
        f"set(ENABLE_ASM {'ON' if assembly else 'OFF'} CACHE BOOL \"\" FORCE)\n"
        f'add_subdirectory("{(ENGINE_ROOT / "ThirdParty/LibreSSL").as_posix()}" LibreSSL)\n'
        "get_target_property(crypto_sources crypto_obj SOURCES)\n"
        "get_target_property(tls_compat_type tls_compat_obj TYPE)\n"
        'file(WRITE "${CMAKE_BINARY_DIR}/sources.txt" "${crypto_sources}\\n${tls_compat_type}")\n'
        "add_executable(archive_probe probe.c)\n"
        'target_include_directories(archive_probe PRIVATE "${CMAKE_BINARY_DIR}/include")\n'
        "target_link_libraries(archive_probe PRIVATE tls)\n",
        encoding="utf-8",
    )
    build = tmp_path / "build"
    _configure(tmp_path, build)
    sources, tls_compat_type = (build / "sources.txt").read_text().split("\n")
    crypto_sources = sources.split(";")
    assert ("aes/aes_core.c" in crypto_sources) == ("aes/aes_amd64.c" not in crypto_sources)
    if not assembly:
        assert "aes/aes_core.c" in crypto_sources
    commands = json.loads((build / "compile_commands.json").read_text())
    assert not any(Path(command["file"]).name == "empty.c" for command in commands)
    if tls_compat_type != "INTERFACE_LIBRARY":
        assert tls_compat_type == "OBJECT_LIBRARY"
        assert any(Path(command["file"]).name == "pread.c" for command in json.loads((build / "compile_commands.json").read_text()))

    result = subprocess.run(
        ["cmake", "--build", str(build), "--target", "archive_probe", "--parallel", "2"], capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "warning:" not in (result.stdout + result.stderr).lower()
    executable = build / ("archive_probe.exe" if (build / "archive_probe.exe").exists() else "archive_probe")
    result = subprocess.run([str(executable)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.skipif(not shutil.which("cmake") or not shutil.which("ninja"), reason="CMake and Ninja are required")
@pytest.mark.parametrize("optimizer", [False, True])
def test_glslang_spirv_tools_bridge_tracks_optimizer(tmp_path: Path, optimizer: bool) -> None:
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(SpirvArchiveInputs CXX)\n"
        "function(glslang_only_export_explicit_symbols target)\nendfunction()\n"
        "add_library(glslang INTERFACE)\nadd_library(SPIRV-Tools-opt INTERFACE)\n"
        f"set(ENABLE_OPT {'ON' if optimizer else 'OFF'})\n"
        f'add_subdirectory("{(ENGINE_ROOT / "ThirdParty/glslang/SPIRV").as_posix()}" SPIRV)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/sources.txt" "${SPIRV_SOURCES}")\n',
        encoding="utf-8",
    )
    build = tmp_path / "build"
    _configure(tmp_path, build)
    sources = {Path(source).name for source in (build / "sources.txt").read_text().split(";")}
    assert ("SpvTools.cpp" in sources) == optimizer
    assert {"GlslangToSpv.cpp", "SpvBuilder.cpp", "spirv_c_interface.cpp"} <= sources
