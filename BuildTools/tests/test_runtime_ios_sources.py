from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import buildtools  # noqa: E402


FIXTURES = Path(__file__).with_name("fixtures") / "runtime_ios"
CLANG = shutil.which("clang-20") or shutil.which("clang")
IO_TYPES = '''
typedef __INT64_TYPE__ int64_t;
typedef __INT32_TYPE__ int32_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef __SIZE_TYPE__ size_t;
typedef long ssize_t;
typedef long long off_t;
typedef struct { void *Base; size_t Count; } IOVector;
struct iovec { void *iov_base; size_t iov_len; };
extern int errno;
#define EINTR 4
#define INT_MAX 2147483647
#define NULL ((void*)0)
#define assert(condition) ((void)(condition))
#define TARGET_APPLE 1
#define HAVE_PREADV 1
#define HAVE_PWRITEV 1
static int ToFileDescriptor(intptr_t fd) { return (int)fd; }
ssize_t preadv(int, const struct iovec*, int, off_t) __attribute__((availability(ios,introduced=14.0)));
ssize_t pwritev(int, const struct iovec*, int, off_t) __attribute__((availability(ios,introduced=14.0)));
ssize_t pread(int, void*, size_t, off_t);
ssize_t pwrite(int, const void*, size_t, off_t);
'''
HOST_TYPES = '''
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
typedef struct { void *Base; size_t Count; } IOVector;
static int ToFileDescriptor(intptr_t fd) { return (int)fd; }
#define HAVE_PREADV 1
#define HAVE_PWRITEV 1
#define Min(a,b) ((a) < (b) ? (a) : (b))
#define Error_SUCCESS 0
static int SystemNative_ConvertErrorPlatformToPal(int error) { return error; }
'''


@pytest.fixture
def runtime(tmp_path):
    root = tmp_path / "runtime"
    native = root / "src/native/libs/System.Native"
    native.mkdir(parents=True)
    for name in ("pal_io.c", "pal_networking.c"):
        shutil.copy2(FIXTURES / name, native / name)
    collation = root / "src/native/libs/System.Globalization.Native/pal_collation.m"
    collation.parent.mkdir()
    content = '''
#include <stdint.h>
#include <stdbool.h>
typedef enum { None=0, IgnoreCase=1, IgnoreNonSpace=2, IgnoreKanaType=8, IgnoreWidth=16, StringSort=536870912 } CompareOptions;
static bool IsComparisonOptionSupported(int32_t comparisonOptions) { return comparisonOptions >= 0; }
static int ConvertFromCompareOptionsToNSStringCompareOptions(int32_t comparisonOptions, bool literal) { return comparisonOptions + (int)literal; }
'''
    for index in range(4):
        content += f'int comparison_{index}(int32_t comparisonOptions)\n{{\n'
        content += '        if (!IsComparisonOptionSupported((CompareOptions)comparisonOptions)) return -1;\n'
        if index == 0:
            content += '        (void)ConvertFromCompareOptionsToNSStringCompareOptions((CompareOptions)comparisonOptions, false);\n'
        content += '        return ConvertFromCompareOptionsToNSStringCompareOptions((CompareOptions)comparisonOptions, true);\n}\n'
    content += '''
int comparison_options(int32_t options)
{
        if (!IsComparisonOptionSupported((CompareOptions)options)) return -1;
        return ConvertFromCompareOptionsToNSStringCompareOptions((CompareOptions)options, true);
}
'''
    collation.write_text(content)
    return root


def test_ios_patches_are_idempotent_and_reject_source_drift(runtime):
    buildtools.patch_runtime_ios_sources(runtime)
    files = {path: path.read_bytes() for path in runtime.rglob("*") if path.is_file()}
    buildtools.patch_runtime_ios_sources(runtime)
    assert all(path.read_bytes() == contents for path, contents in files.items())
    collation = runtime / "src/native/libs/System.Globalization.Native/pal_collation.m"
    collation.write_text("upstream changed\n")
    with pytest.raises(SystemExit, match="expected anchors not found"):
        buildtools.patch_runtime_ios_sources(runtime)


@pytest.mark.skipif(not CLANG, reason="Clang is required")
@pytest.mark.parametrize("target", ["arm64-apple-ios12.2", "x86_64-apple-ios12.2-simulator", "arm64-apple-ios14.0"])
def test_ios_vector_functions_require_runtime_availability_guards(runtime, target):
    source = runtime / "src/native/libs/System.Native/pal_io.c"
    command = [CLANG, "-target", target, "-ffreestanding", "-Werror", "-Wunguarded-availability-new", "-c"]
    for variant in ("before", "after"):
        if variant == "after":
            buildtools.patch_runtime_ios_sources(runtime)
        unit = runtime / f"io-{variant}.c"
        unit.write_text(IO_TYPES + source.read_text())
        result = subprocess.run([*command, str(unit), "-o", str(unit.with_suffix(".o"))], capture_output=True, text=True)
        if variant == "before" and "12.2" in target:
            assert result.returncode != 0 and "only available on iOS 14.0" in result.stderr, result.stderr
        else:
            assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
        if variant == "after" and "12.2" in target and (shutil.which("llvm-nm-20") or shutil.which("llvm-nm")):
            symbols = subprocess.run([shutil.which("llvm-nm-20") or shutil.which("llvm-nm"), "-m", str(unit.with_suffix(".o"))], capture_output=True, text=True)
            assert symbols.returncode == 0 and not symbols.stderr
            assert "weak external _preadv" in symbols.stdout and "weak external _pwritev" in symbols.stdout


@pytest.mark.skipif(not CLANG or os.name == "nt", reason="Clang and POSIX headers are required")
def test_ios_collation_and_sendfile_compile_without_signedness_or_cleanup_warnings(runtime):
    collation = runtime / "src/native/libs/System.Globalization.Native/pal_collation.m"
    sendfile = runtime / "src/native/libs/System.Native/pal_networking.c"
    for variant in ("before", "after"):
        if variant == "after":
            buildtools.patch_runtime_ios_sources(runtime)
        for name, content, language, flags in (
            ("collation", collation.read_text(), "objective-c", ["-Wsign-conversion"]),
            ("sendfile", HOST_TYPES + sendfile.read_text(), "c++", []),
        ):
            unit = runtime / f"{name}-{variant}.c"
            unit.write_text(content)
            result = subprocess.run([CLANG, "-x", language, "-Werror", *flags, "-c", str(unit), "-o", str(unit.with_suffix(".o"))], capture_output=True, text=True)
            if variant == "before":
                assert result.returncode != 0, result.stdout + result.stderr
                assert ("changes signedness" if name == "collation" else "jump") in result.stderr
            else:
                assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr


@pytest.mark.skipif(not CLANG or os.name == "nt", reason="Clang and POSIX vector I/O are required")
@pytest.mark.parametrize("available", [0, 1])
def test_ios_vector_native_and_scalar_paths_preserve_offsets_short_reads_and_retry_eintr(runtime, available):
    buildtools.patch_runtime_ios_sources(runtime)
    body = (runtime / "src/native/libs/System.Native/pal_io.c").read_text()
    wrappers = f'''
static int feature_available = {available};
static int vector_calls = 0, scalar_calls = 0, interrupt_next = 1, limit_write = 0;
static ssize_t read_vector(int fd, const struct iovec *v, int count, off_t offset) {{ vector_calls++; if (interrupt_next) {{ interrupt_next=0; errno=EINTR; return -1; }} return preadv(fd,v,count,offset); }}
static ssize_t write_vector(int fd, const struct iovec *v, int count, off_t offset) {{ vector_calls++; if (interrupt_next) {{ interrupt_next=0; errno=EINTR; return -1; }} if (limit_write) return pwrite(fd,v[0].iov_base,1,offset); return pwritev(fd,v,count,offset); }}
static ssize_t read_scalar(int fd, void *v, size_t count, off_t offset) {{ scalar_calls++; if (interrupt_next) {{ interrupt_next=0; errno=EINTR; return -1; }} return pread(fd,v,count,offset); }}
static ssize_t write_scalar(int fd, const void *v, size_t count, off_t offset) {{ scalar_calls++; if (interrupt_next) {{ interrupt_next=0; errno=EINTR; return -1; }} if (limit_write) count=1; return pwrite(fd,v,count,offset); }}
#define TARGET_APPLE 1
#define __builtin_available(...) feature_available
#define preadv read_vector
#define pwritev write_vector
#define pread read_scalar
#define pwrite write_scalar
'''
    driver = '''
int main(void)
{
    FILE *file = tmpfile(); assert(file);
    int fd = fileno(file);
    IOVector writes[] = {{(void*)"ab", 2}, {(void*)"cde", 3}};
    assert(SystemNative_PWriteV(fd, writes, 2, 3) == 5);
    assert(lseek(fd, 0, SEEK_CUR) == 0);
    char first[2] = {0}, second[5] = {0};
    IOVector reads[] = {{first, 2}, {second, 5}};
    interrupt_next = 1;
    assert(SystemNative_PReadV(fd, reads, 2, 3) == 5);
    assert(memcmp(first, "ab", 2) == 0 && memcmp(second, "cde", 3) == 0);
    assert(lseek(fd, 0, SEEK_CUR) == 0);
    assert(SystemNative_PReadV(-1, reads, 2, 3) == -1 && errno == EBADF);
    assert(SystemNative_PWriteV(-1, writes, 2, 3) == -1 && errno == EBADF);
    limit_write = 1;
    assert(SystemNative_PWriteV(fd, writes, 2, 10) == 1);
    assert(lseek(fd, 0, SEEK_CUR) == 0);
    assert(feature_available ? (vector_calls > 0 && scalar_calls == 0) : (scalar_calls > 0 && vector_calls == 0));
    fclose(file);
    return 0;
}
'''
    unit = runtime / "io-runtime.c"
    unit.write_text(HOST_TYPES + wrappers + body + driver)
    program = runtime / "io-runtime"
    result = subprocess.run([CLANG, "-Werror", str(unit), "-o", str(program)], capture_output=True, text=True)
    assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
    result = subprocess.run([str(program)], capture_output=True, text=True)
    assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
