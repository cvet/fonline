from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


@pytest.fixture
def runtime(tmp_path: Path) -> Path:
    files = {
        "src/mono/CMakeLists.txt": "cmake_minimum_required(VERSION 3.20)\n",
        "src/mono/mono/mini/mini-generic-sharing.c": """
typedef short gint16;
typedef int GHashTable;
struct Signature { int hasthis; };
int argument_offset(struct Signature *sig)
{
\tgint16 pindex;
\tint args_start;
\tstatic GHashTable *cache;
    (void)cache;
    (void)sig;
    pindex = 1;
\targs_start = pindex;
\tif (sig->hasthis)
\t\targs_start ++;
#ifndef DISABLE_JIT
    return args_start;
#else
    return pindex;
#endif
}
""",
        "src/mono/mono/mini/mini-arm64.c": """
enum { OP_START = 0, OP_LAST = 3 };
static char opcode_simd_status[OP_LAST - OP_START];
int opcode_status(void)
{
#ifndef DISABLE_JIT
    return opcode_simd_status[0];
#else
    return 0;
#endif
}
""",
        "src/native/eventpipe/ep-session.c": """
enum { EP_ACTIVITY_ID_SIZE = 16 };
int buffer_sizes(void)
{
    const int max_static_io_capacity = 30;
    const int extension_activity_ids_max_len = 2 * (1 + EP_ACTIVITY_ID_SIZE);
    char events[max_static_io_capacity];
    char activity[extension_activity_ids_max_len];
    return sizeof(events) == 30 && sizeof(activity) == 34;
}
""",
        "src/native/libs/System.Native/pal_interfaceaddresses.c": """
#include <stdlib.h>
#include <stdint.h>
void allocate_buffer(size_t byteCount)
{
    uint8_t* buffer = malloc(byteCount);
    if (buffer != NULL) {
        free(buffer);
        buffer = malloc(byteCount);
    }
    free(buffer);
}
""",
        "src/native/libs/System.Native/pal_process.c": """
#include <stdlib.h>
#include <stdint.h>
#define Int32ToSizeT(value) ((size_t)(value))
void allocate_groups(int groupsLength)
{
    uint32_t *getGroupsBuffer;
    getGroupsBuffer = malloc(sizeof(uint32_t) * Int32ToSizeT(groupsLength));
    free(getGroupsBuffer);
}
""",
        "src/native/libs/System.Native/pal_signal.c": """
typedef enum { PosixSignalSIGTERM = 15 } PosixSignal;
void convert_signal(int signalCode, PosixSignal *posixSignal)
{
    *posixSignal = signalCode;
}
""",
        "src/native/libs/System.Net.Security.Native/pal_gssapi.c": """
#include <string.h>
char *find_slash(char *inputName, size_t inputNameLen)
{
    char* ptrSlash = memchr(inputName, '/', inputNameLen);
    return ptrSlash;
}
""",
        "src/native/libs/System.Native/pal_networking.c": """
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#define _POSIX_HOST_NAME_MAX 255
struct ifaddrs { int value; };
int network_cleanup(int fail)
{
    int result;
#if HAVE_GETIFADDRS
    struct ifaddrs* addrs = NULL;
#endif
    if (fail) { result = -1; goto cleanup; }
#if HAVE_GETIFADDRS
    char name[_POSIX_HOST_NAME_MAX];
    result = gethostname((char*)name, _POSIX_HOST_NAME_MAX);

    bool includeIPv4Loopback = true;
    bool includeIPv6Loopback = true;
    (void)includeIPv4Loopback;
    (void)includeIPv6Loopback;
#else
    result = 0;
#endif
cleanup:
#if HAVE_GETIFADDRS
    (void)addrs;
#endif
    return result;
}
""",
        "src/native/libs/configure.cmake": '''
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-Werror -Weverything")
check_c_source_compiles("
    #include <unistd.h>
    int main(void)
    {
        size_t namelen = 20;
        char name[20];
        int dummy = getdomainname(name, namelen);
        (void)dummy;
        return 0;
    }
" HAVE_GETDOMAINNAME_SIZET)
''',
    }
    for relative, content in files.items():
        path = tmp_path / "runtime" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    return tmp_path / "runtime"


def test_apple_source_patches_are_idempotent_and_reject_source_drift(runtime: Path) -> None:
    buildtools.patch_runtime_apple_sources(runtime)
    first = {path: path.read_bytes() for path in runtime.rglob("*") if path.is_file()}
    buildtools.patch_runtime_apple_sources(runtime)
    assert all(path.read_bytes() == content for path, content in first.items())
    path = runtime / "src/native/libs/configure.cmake"
    path.write_text("unknown signature probe\n")
    with pytest.raises(SystemExit, match="unique anchor not found"):
        buildtools.patch_runtime_apple_sources(runtime)


@pytest.mark.skipif(not (shutil.which("clang-20") or shutil.which("clang")), reason="Clang is required")
@pytest.mark.parametrize("disable_jit", [False, True])
def test_apple_native_source_corrections_compile_without_masking_diagnostics(runtime: Path, disable_jit: bool) -> None:
    clang = shutil.which("clang-20") or shutil.which("clang")
    original = {}
    for source in runtime.rglob("*.c"):
        original[source] = source.read_bytes()
    buildtools.patch_runtime_apple_sources(runtime)
    results = []
    for source in original:
        is_pal = "/libs/" in source.as_posix()
        command = [clang, "-x", "c++" if is_pal else "c", "-Wall", "-Wextra", "-Werror", "-Werror=vla", "-DHAVE_GETIFADDRS=1"]
        if disable_jit:
            command.append("-DDISABLE_JIT")
        previous = source.with_name("before_" + source.name)
        previous.write_bytes(original[source])
        negative = subprocess.run([*command, "-c", str(previous), "-o", str(previous.with_suffix(".o"))], capture_output=True, text=True)
        must_fail = is_pal or source.name == "ep-session.c" or disable_jit
        assert (negative.returncode != 0) == must_fail, negative.stdout + negative.stderr
        result = subprocess.run([*command, "-c", str(source), "-o", str(source.with_suffix(".o"))], capture_output=True, text=True)
        results.append({"source": str(source.relative_to(runtime)), "returncode": result.returncode, "stderr": result.stderr, "before_returncode": negative.returncode, "before_stderr": negative.stderr})
        assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
    (runtime / "compile-results.json").write_text(json.dumps(results, indent=2))


@pytest.mark.skipif(not shutil.which("cmake") or not (shutil.which("clang-20") or shutil.which("clang")), reason="CMake and Clang are required")
@pytest.mark.parametrize("parameter_type", ["int", "size_t"])
@pytest.mark.parametrize("flags", [
    "-Werror -Weverything", "-Werror",
    "-Werror -Weverything -Wno-error=incompatible-function-pointer-types -Wno-error=incompatible-function-pointer-types-strict",
    "-w -Wno-error=incompatible-function-pointer-types",
])
@pytest.mark.parametrize("legacy_patch", [False, True])
def test_domain_name_probe_checks_the_function_type_independently_of_conversion_diagnostics(runtime: Path, parameter_type: str, flags: str, legacy_patch: bool) -> None:
    probe = runtime / "src/native/libs/configure.cmake"
    previous_probe = (
        "        // (FOnline Patch) Compare the parameter type; a constant length can hide narrowing\n"
        "        int (*getdomainname_sizet)(char*, size_t) = getdomainname;\n"
        "        int dummy = getdomainname_sizet(name, namelen);"
    )
    text = probe.read_text().replace("-Werror -Weverything", flags)
    if legacy_patch:
        text = text.replace("        int dummy = getdomainname(name, namelen);", previous_probe)
    probe.write_text(text)
    source = runtime.parent / "sdk signature"
    source.mkdir()
    (source / "unistd.h").write_text(f"#include <stddef.h>\nint getdomainname(char*, {parameter_type});\n")
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\nproject(DomainNameSignature C)\n"
        f'set(CMAKE_REQUIRED_INCLUDES "{source.as_posix()}")\n'
        f'include("{(runtime / "src/native/libs/configure.cmake").as_posix()}")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/result.txt" "${HAVE_GETDOMAINNAME_SIZET}")\n'
    )
    results = {}
    for variant in ("before", "after"):
        if variant == "after":
            buildtools.patch_runtime_apple_sources(runtime)
            first_patch = probe.read_bytes()
            buildtools.patch_runtime_apple_sources(runtime)
            assert probe.read_bytes() == first_patch
        build = source / "build"
        result = subprocess.run([
            shutil.which("cmake"), "-S", str(source), "-B", str(build),
            f"-DCMAKE_C_COMPILER={shutil.which('clang-20') or shutil.which('clang')}",
        ], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "CMake Warning" not in result.stdout + result.stderr
        results[variant] = (build / "result.txt").read_text()
    accepted_before = (
        parameter_type == "size_t" or flags.startswith("-w ")
        or (not legacy_patch and flags == "-Werror")
        or (legacy_patch and "-Wno-error=incompatible-function-pointer-types" in flags)
    )
    assert results["before"] == ("1" if accepted_before else "")
    assert results["after"] == ("1" if parameter_type == "size_t" else "")
