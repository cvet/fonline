from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest


SPEC = importlib.util.spec_from_file_location("probe_windows_file_io", Path(__file__).resolve().parents[1] / "probe_windows_file_io.py")
assert SPEC and SPEC.loader
PROBE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROBE)


@pytest.mark.parametrize(
    ("original", "expected"),
    [
        (r"C:\nested\file.pdb", r"\\?\C:\nested\file.pdb"),
        ("D:/nested/file.pdb", r"\\?\D:\nested\file.pdb"),
        (r"\\server\share\nested\file.pdb", r"\\?\UNC\server\share\nested\file.pdb"),
        (r"\\?\C:\nested\file.pdb", r"\\?\C:\nested\file.pdb"),
        (r"\\?\UNC\server\share\file.pdb", r"\\?\UNC\server\share\file.pdb"),
    ],
)
def test_extended_paths_preserve_drive_and_unc_forms(original: str, expected: str) -> None:
    assert PROBE.extended_path(original) == expected


@pytest.mark.parametrize("path", ["relative/file.pdb", r"C:relative.pdb", r"\rooted.pdb"])
def test_extended_path_rejects_nonabsolute_input(path: str) -> None:
    with pytest.raises(ValueError):
        PROBE.extended_path(path)


@pytest.mark.parametrize("length", [259, 260, 262, 320])
def test_fixture_reaches_exact_utf16_boundary(length: int) -> None:
    base = Path("C:/probe/Юникод/🧪")
    path = PROBE.make_case_path(base, length)
    assert PROBE.utf16_length(str(path)) == length
    assert path.name == "LastFrontier.dll.pdb"
    assert all(PROBE.utf16_length(part) < 255 for part in path.parts)


def test_fixture_rejects_overlong_root() -> None:
    with pytest.raises(ValueError, match="root is too long"):
        PROBE.make_case_path(Path("C:/" + "a" * 250), 259)


def test_engine_probe_compiles_exact_canonical_function_bodies() -> None:
    native, manifest = PROBE.engine_probe_source()
    canonical = PROBE.ENGINE_SOURCE.read_text(encoding="utf-8")
    assert len(manifest["extracted_functions"]) == 8
    for signature in PROBE.ENGINE_SIGNATURES:
        start = canonical.rindex(signature)
        end = canonical.index("\n}\n", start) + len("\n}\n")
        assert canonical[start:end] in native
    declaration = next(line for line in canonical.splitlines() if line.startswith("static auto fs_make_io_path(") and line.endswith(";"))
    assert declaration in native
    assert manifest["harness_aliases"]["string_view"] == "std::string_view"
    assert "Full engine linkage" in manifest["proof_scope"]
