"""Native EOF formatting must agree with the source-layout gate."""

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import buildtools


@pytest.mark.parametrize("eol", ["\n", "\r\n"])
@pytest.mark.parametrize("suffix", [".h", ".cpp", ".fos"])
def test_formatter_normalizes_one_final_eol(tmp_path: Path, monkeypatch, eol: str, suffix: str) -> None:
    path = tmp_path / ("Unit" + suffix)
    path.write_bytes(("int Value;" + eol * 4).encode("utf-8"))
    monkeypatch.setattr(buildtools, "run_capture_text", lambda *args, **kwargs: "int Value;\n")

    assert buildtools.format_files("clang-format-20", tmp_path, ["*" + suffix]) == 1
    expected = ("int Value;" + eol).encode("utf-8")
    assert path.read_bytes() == expected
    assert buildtools.format_files("clang-format-20", tmp_path, ["*" + suffix]) == 0
