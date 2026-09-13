from __future__ import annotations

import json
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(not shutil.which("cmake") or not shutil.which("ninja") or not shutil.which("ar"), reason="CMake, Ninja and ar are required")
def test_mongoc_archive_preserves_protocol_checks_and_client_operations(tmp_path: Path) -> None:
    (tmp_path / "probe.c").write_text(
        '#include <mongoc/mongoc.h>\n'
        'int main(void) {\n'
        '    mongoc_init();\n'
        '    mongoc_uri_t *uri = mongoc_uri_new("mongodb://localhost:27017/probe");\n'
        '    if (!uri) return 1;\n'
        '    mongoc_client_t *client = mongoc_client_new_from_uri(uri);\n'
        '    if (!client) return 2;\n'
        '    bson_t document; bson_init(&document);\n'
        '    if (!BSON_APPEND_INT32(&document, "probe", 37)) return 3;\n'
        '    bson_iter_t iter;\n'
        '    if (!bson_iter_init_find(&iter, &document, "probe") || bson_iter_int32(&iter) != 37) return 4;\n'
        '    bson_destroy(&document); mongoc_client_destroy(client); mongoc_uri_destroy(uri);\n'
        '    mongoc_cleanup(); return 0;\n}\n', encoding="utf-8",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.22)\nproject(MongocArchiveInputs C)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        'foreach(option ENABLE_SHARED ENABLE_TESTS ENABLE_EXAMPLES ENABLE_UNINSTALL ENABLE_SSL ENABLE_SASL ENABLE_SRV ENABLE_MONGODB_AWS_AUTH ENABLE_CLIENT_SIDE_ENCRYPTION ENABLE_SNAPPY ENABLE_ZSTD ENABLE_ZLIB)\n'
        '    set(${option} OFF CACHE STRING "" FORCE)\nendforeach()\n'
        'set(ENABLE_STATIC BUILD_ONLY CACHE STRING "" FORCE)\n'
        f'add_subdirectory("{(ENGINE_ROOT / "ThirdParty/mongo-c-driver").as_posix()}" mongo)\n'
        'get_target_property(sources mongoc_static SOURCES)\n'
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/archive.txt" CONTENT "$<TARGET_FILE:mongoc_static>")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/sources.txt" "${sources}")\n'
        'add_executable(mongoc_probe probe.c)\ntarget_link_libraries(mongoc_probe PRIVATE mongoc::static)\n', encoding="utf-8",
    )
    build = tmp_path / "build"
    configured = subprocess.run(["cmake", "-S", str(tmp_path), "-B", str(build), "-G", "Ninja"], capture_output=True, text=True)
    assert configured.returncode == 0, configured.stdout + configured.stderr
    assert "CMake Warning" not in configured.stdout + configured.stderr
    sources = {Path(path).name for path in (build / "sources.txt").read_text().split(";")}
    for name in ("mongoc-crypt.c", "mongoc-crypto.c", "mongoc-scram.c", "mongoc-ssl.c", "mongoc-stream-tls.c", "mongoc-flags.c", "mongoc-opcode.c"):
        assert name not in sources
    assert "mongoc-client.c" in sources
    commands = json.loads((build / "compile_commands.json").read_text())
    assert not any(Path(command["file"]).name in ("mongoc-flags.c", "mongoc-opcode.c") for command in commands)
    check = next(command for command in commands if Path(command["file"]).name == "mongoc-rpc.c")

    compiled = subprocess.run(["cmake", "--build", str(build), "--target", "mongoc_probe", "--parallel", "2"], capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    assert "warning:" not in (compiled.stdout + compiled.stderr).lower()
    members = subprocess.check_output(["ar", "t", (build / "archive.txt").read_text()], text=True).splitlines()
    assert not any(member.startswith(("mongoc-flags.", "mongoc-opcode.", "mongoc-crypt.", "mongoc-ssl.")) for member in members)
    assert any(member.startswith("mongoc-client.") for member in members)
    executable = build / ("mongoc_probe.exe" if (build / "mongoc_probe.exe").exists() else "mongoc_probe")
    ran = subprocess.run([str(executable)], capture_output=True, text=True)
    assert ran.returncode == 0, ran.stdout + ran.stderr

    if "cl.exe" not in check["command"].lower():
        for source, declarations, mismatch in (
            ("mongoc-rpc.c", '#include <mongoc/mongoc-flags.h>\n#include <mongoc/mongoc-compression-private.h>\n#include <mongoc/mongoc-flags-private.h>\n', "MONGOC_MSG_NONE 32"),
            ("mongoc-cluster.c", '#include <mongoc/mongoc-opcode.h>\n', "MONGOC_OPCODE_MSG 0"),
        ):
            poison = tmp_path / (source + "-mismatched.h")
            poison.write_text(declarations + '#include <mongoc/mcd-rpc.h>\n#include <bson/bson.h>\n#define ' + mismatch + '\n', encoding="utf-8")
            check = next(command for command in commands if Path(command["file"]).name == source)
            arguments = shlex.split(check["command"])
            arguments[arguments.index("-o") + 1] = str(tmp_path / "must-not-compile.o")
            arguments += ["-include", str(poison)]
            invalid = subprocess.run(arguments, cwd=check["directory"], capture_output=True, text=True)
            assert invalid.returncode != 0
            assert "BSON_STATIC_ASSERT" in invalid.stdout + invalid.stderr
            assert "negative" in (invalid.stdout + invalid.stderr).lower() or "static assertion" in (invalid.stdout + invalid.stderr).lower()


@pytest.mark.skipif(not shutil.which("cmake"), reason="CMake is required")
@pytest.mark.parametrize("backend,system", [("OFF", "Darwin"), ("OpenSSL", "Linux"), ("SecureChannel", "Windows"), ("SecureTransport", "Darwin"), ("OFF", "Android")])
def test_mongoc_source_selection_keeps_enabled_platform_implementations(tmp_path: Path, backend: str, system: str) -> None:
    stage = (ENGINE_ROOT / "ThirdParty/mongo-c-driver/src/libmongoc/CMakeLists.txt").read_text()
    selection = stage[stage.index("set (MONGOC_SOURCES\n"):stage.index("\nif (MONGOC_ENABLE_SASL)")]
    (tmp_path / "select.cmake").write_text(
        f'set(CMAKE_SYSTEM_NAME {system})\n'
        f'set(MONGOC_ENABLE_SSL {"OFF" if backend == "OFF" else "ON"})\n'
        'set(MONGOC_ENABLE_CRYPTO ${MONGOC_ENABLE_SSL})\n'
        f'set(MONGOC_ENABLE_SSL_OPENSSL {"ON" if backend == "OpenSSL" else "OFF"})\n'
        f'set(MONGOC_ENABLE_SSL_SECURE_CHANNEL {"ON" if backend == "SecureChannel" else "OFF"})\n'
        f'set(MONGOC_ENABLE_SSL_SECURE_TRANSPORT {"ON" if backend == "SecureTransport" else "OFF"})\n'
        'set(MONGOC_ENABLE_CRYPTO_LIBCRYPTO ${MONGOC_ENABLE_SSL_OPENSSL})\n'
        'set(MONGOC_ENABLE_CRYPTO_CNG ${MONGOC_ENABLE_SSL_SECURE_CHANNEL})\n'
        'set(MONGOC_ENABLE_CRYPTO_COMMON_CRYPTO ${MONGOC_ENABLE_SSL_SECURE_TRANSPORT})\n'
        'set(MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION ON)\n' + selection +
        '\nfile(WRITE "sources.txt" "${MONGOC_SOURCES}")\n'
    )
    result = subprocess.run(["cmake", "-P", str(tmp_path / "select.cmake")], cwd=tmp_path, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    sources = [Path(path).name for path in (tmp_path / "sources.txt").read_text().split(";")]
    assert "mongoc-client.c" in sources
    assert "mongoc-crypt.c" in sources
    assert ("mongoc-linux-distro-scanner.c" in sources) == (system in ("Linux", "Android"))
    assert ("mongoc-scram.c" in sources) == (backend != "OFF")
    for selected, name in (("OpenSSL", "openssl"), ("SecureChannel", "secure-channel"), ("SecureTransport", "secure-transport")):
        assert ("mongoc-" + name + ".c" in sources) == (backend == selected)
        assert ("mongoc-stream-tls-" + name + ".c" in sources) == (backend == selected)
    assert sources.count("mongoc-crypto-openssl.c") == (1 if backend == "OpenSSL" else 0)
    assert sources.count("mongoc-rand-openssl.c") == (1 if backend == "OpenSSL" else 0)
