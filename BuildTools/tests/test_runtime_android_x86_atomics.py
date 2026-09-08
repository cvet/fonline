from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


# The upstream anchors retain their exact spelling; the runtime probe executes
# the definitions inserted by the production patch, including its CAS loop.
UPSTREAM = """#if !defined (BROKEN_64BIT_ATOMICS_INTRINSIC)
static inline gint64 mono_atomic_cas_i64(volatile gint64 *dest, gint64 exch, gint64 comp)
{
    return __sync_val_compare_and_swap (dest, comp, exch);
}
#endif
static inline gint64 mono_atomic_xchg_i64(volatile gint64 *val, gint64 new_val)
{
\tgint64 old_val;
\tdo {
\t\told_val = *val;
\t} while (mono_atomic_cas_i64 (val, new_val, old_val) != old_val);
\treturn old_val;
}
static inline void mono_atomic_store_i64(volatile gint64 *dst, gint64 val)
{
    mono_atomic_xchg_i64 (dst, val);
}
"""


@pytest.fixture
def runtime(tmp_path: Path) -> Path:
    path = tmp_path / "src/mono/mono/utils/atomic.h"
    path.parent.mkdir(parents=True)
    path.write_text(UPSTREAM, encoding="utf-8")
    return tmp_path


@pytest.fixture
def clang() -> str:
    compiler = next((shutil.which(name) for name in ("clang-20", "clang", "clang-18") if shutil.which(name)), None)
    if compiler is None:
        pytest.skip("Clang is required for the Android x86 code-generation probe")
    return compiler


def patched_header(runtime: Path) -> str:
    buildtools.patch_runtime_android_x86_atomics(runtime)
    return (runtime / "src/mono/mono/utils/atomic.h").read_text(encoding="utf-8")


def test_patch_is_idempotent(runtime: Path) -> None:
    assert patched_header(runtime) == patched_header(runtime)


@pytest.mark.parametrize("mutation", ["missing", "duplicate", "exchange"])
def test_unknown_source_fails_before_writing(runtime: Path, mutation: str) -> None:
    path = runtime / "src/mono/mono/utils/atomic.h"
    content = UPSTREAM
    if mutation == "missing":
        content = content.replace("#if !defined (BROKEN_64BIT_ATOMICS_INTRINSIC)", "#if 1")
    elif mutation == "duplicate":
        content += "#if !defined (BROKEN_64BIT_ATOMICS_INTRINSIC)\n#endif\n"
    else:
        content = content.replace("old_val = *val;", "old_val = *val + 1;")
    path.write_text(content, encoding="utf-8")
    with pytest.raises(SystemExit, match="unique anchor"):
        buildtools.patch_runtime_android_x86_atomics(runtime)
    assert path.read_text(encoding="utf-8") == content


@pytest.mark.parametrize("optimization", ["-O0", "-O2", "-O3"])
def test_android_x86_codegen_preserves_pic_and_avoids_library_locks(runtime: Path, clang: str, optimization: str) -> None:
    header = runtime / "canonical-i64.h"
    header.write_text(patched_header(runtime), encoding="utf-8")
    source = BUILDTOOLS_DIR / "tests/fixtures/android_x86_atomics_probe.c"
    output = runtime / "probe.s"
    result = subprocess.run(
        [clang, "--target=i686-linux-android21", "-std=gnu11", optimization, "-fPIC", "-Wall", "-Wextra", "-Werror", "-I", str(runtime), "-S", str(source), "-o", str(output)],
        capture_output=True, text=True, check=False,
    )
    assert result.returncode == 0, result.stderr
    assert not result.stderr
    assembly = output.read_text(encoding="utf-8")
    assert "cmpxchg8b" in assembly
    assert "lock" in assembly
    assert "__atomic_" not in assembly
    assert "__sync_" not in assembly


def test_other_targets_keep_the_upstream_implementation(runtime: Path, clang: str) -> None:
    before = runtime / "before.h"
    after = runtime / "after.h"
    before.write_text(UPSTREAM, encoding="utf-8")
    after.write_text(patched_header(runtime), encoding="utf-8")
    for macros in ([], ["-DHOST_ANDROID", "-DHOST_AMD64"], ["-DHOST_X86"]):
        outputs = [subprocess.check_output([clang, "-E", "-P", *macros, str(path)], text=True) for path in (before, after)]
        assert outputs[0].split() == outputs[1].split()


def test_real_i386_concurrent_operations_on_four_and_eight_byte_alignment(runtime: Path, clang: str) -> None:
    libc = Path("/usr/lib32/libc.so.6")
    if sys.platform != "linux" or not libc.is_file() or not Path("/lib/ld-linux.so.2").is_file():
        pytest.skip("The execution probe requires Linux i386 loader and libc")
    (runtime / "canonical-i64.h").write_text(patched_header(runtime), encoding="utf-8")
    source = BUILDTOOLS_DIR / "tests/fixtures/android_x86_atomics_probe.c"
    executable = runtime / "atomic-probe"
    result = subprocess.run(
        [clang, "-m32", "-std=gnu11", "-O2", "-fPIC", "-Wall", "-Wextra", "-Werror", "-nostdlib", "-I", str(runtime), str(source), str(libc), "-o", str(executable)],
        capture_output=True, text=True, check=False,
    )
    assert result.returncode == 0, result.stderr
    assert not result.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30, check=False)
    assert result.returncode == 0, f"status={result.returncode}\n{result.stdout}\n{result.stderr}"
    assert result.stdout.startswith("PASS: eight atomic APIs; 4/8 alignment;")
    assert not result.stderr
