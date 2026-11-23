"""Native EOF formatting must agree with the source-layout gate."""

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import buildtools


def test_format_source_skips_build_outputs(tmp_path: Path, monkeypatch) -> None:
    source = tmp_path / "Source"
    authored = source / "Scripting/Managed/Analyzers/Analyzer.cs"
    outputs = [authored.parent / name for name in (
        "obj/Debug/FOnline.Analyzers.AssemblyInfo.cs",
        "bin/Debug/Nested/Generated.cs",
    )]
    for path in [authored, *outputs]:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"class  Example {}\n")

    formatted_paths = []

    def format_file(command, **kwargs):
        formatted_paths.append(Path(command[1]))
        return "class Example {}\n"

    monkeypatch.setattr(buildtools, "discover_clang_format", lambda: "clang-format-20")
    monkeypatch.setattr(buildtools, "run_capture_text", format_file)
    buildtools.format_source({"FO_ENGINE_ROOT": str(tmp_path)})

    assert formatted_paths == [authored]
    assert authored.read_bytes() == b"class Example {}\n"
    assert all(path.read_bytes() == b"class  Example {}\n" for path in outputs)


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
