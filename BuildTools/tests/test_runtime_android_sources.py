from __future__ import annotations

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
    path = tmp_path / "src/native/libs/System.Security.Cryptography.Native.Android/pal_ecc_import_export.c"
    path.parent.mkdir(parents=True)
    path.write_text('''
extern int printf(const char*, ...) __attribute__((format(printf, 1, 2)));
#define LOG_ERROR printf
typedef enum
{
    Unspecified = 0,
    PrimeShortWeierstrass = 1,
    PrimeTwistedEdwards = 2,
    PrimeMontgomery = 3,
    Characteristic2 = 4
} ECCurveType;
void report_curve(ECCurveType curveType)
{
    if (curveType != PrimeShortWeierstrass && curveType != Characteristic2)
    {
        LOG_ERROR("Unuspported curve type specified: %d", curveType);
    }
}
int main(void)
{
    for (int value = 0; value <= 4; value++)
        report_curve((ECCurveType)value);
    return 0;
}
''')
    return tmp_path


@pytest.mark.skipif(not (shutil.which("clang-20") or shutil.which("clang")), reason="Clang is required")
@pytest.mark.parametrize("target", ["armv7-none-linux-androideabi21", "aarch64-linux-android21", "i686-linux-android21"])
def test_android_curve_diagnostic_matches_variadic_argument(runtime: Path, target: str) -> None:
    compiler = shutil.which("clang-20") or shutil.which("clang")
    source = runtime / "src/native/libs/System.Security.Cryptography.Native.Android/pal_ecc_import_export.c"
    command = [compiler, "-target", target, "-fsyntax-only", "-Werror", "-Wformat-signedness", str(source)]
    before = subprocess.run(command, capture_output=True, text=True)
    assert before.returncode != 0 and "underlying type 'unsigned int'" in before.stderr, before.stdout + before.stderr

    buildtools.patch_runtime_android_sources(runtime)
    patched = source.read_bytes()
    buildtools.patch_runtime_android_sources(runtime)
    assert source.read_bytes() == patched

    after = subprocess.run(command, capture_output=True, text=True)
    assert after.returncode == 0 and not after.stderr, after.stdout + after.stderr


@pytest.mark.skipif(not shutil.which("cc"), reason="A host C compiler is required")
def test_android_curve_diagnostic_preserves_reported_values(runtime: Path) -> None:
    buildtools.patch_runtime_android_sources(runtime)
    source = runtime / "src/native/libs/System.Security.Cryptography.Native.Android/pal_ecc_import_export.c"
    program = runtime / "curve-format"
    result = subprocess.run([shutil.which("cc"), "-Werror", "-Wformat", str(source), "-o", str(program)], capture_output=True, text=True)
    assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
    output = subprocess.run([str(program)], capture_output=True, text=True)
    assert output.returncode == 0 and not output.stderr
    assert output.stdout == "".join(f"Unuspported curve type specified: {value}" for value in (0, 2, 3))


@pytest.mark.parametrize("contents", ["upstream source changed\n", 'LOG_ERROR("Unuspported curve type specified: %d", curveType);\n' * 2])
def test_android_runtime_patch_rejects_ambiguous_or_changed_source(runtime: Path, contents: str) -> None:
    source = runtime / "src/native/libs/System.Security.Cryptography.Native.Android/pal_ecc_import_export.c"
    source.write_text(contents)
    with pytest.raises(SystemExit, match="unique anchor not found"):
        buildtools.patch_runtime_android_sources(runtime)
    assert source.read_text() == contents
