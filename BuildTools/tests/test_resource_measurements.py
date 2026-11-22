from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

import pytest

BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))

import analyze_resource_corpus as corpus
import measure_resource_packs as measure
import package


def test_measurement_reads_every_version_two_record(tmp_path: Path) -> None:
    pack_dir = tmp_path / "Inputs"
    pack_dir.mkdir()
    content = {"A.txt": b"tiny", "B.bin": b"x" * 4096, "C.empty": b""}
    for name, data in content.items():
        (pack_dir / name).write_bytes(data)
    pack = tmp_path / "Pack.fores"
    package.write_resource_pack(pack, measure.collect_entries(pack_dir), 6, 5)
    shape = measure.read_fores_shape(pack)
    assert shape["storedCount"] == 2
    assert shape["storedSourceBytes"] == 4
    assert shape["deflateCount"] == 1
    assert shape["deflateSourceBytes"] == 4096
    assert shape["catalogStoredBytes"] == struct.unpack_from("<Q", pack.read_bytes(), 24)[0]
    assert measure.read_fores_entries(pack) == content
    result = measure.measure_pack(("Pack", str(pack_dir), str(tmp_path), 6, 5, True))
    assert result["problems"] == []
    assert result["zipCatalogStoredBytes"] > 0
    assert "Catalog .fores" in measure.format_report([result])


def test_merged_index_uses_configured_precedence_and_current_layout(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    for name in ("ZBase", "AOverride", "Empty", "Unselected"):
        (tmp_path / name).mkdir()
    (tmp_path / "ZBase/Shared.txt").write_bytes(b"base")
    (tmp_path / "AOverride/Shared.txt").write_bytes(b"override payload")
    (tmp_path / "Unselected/Unexpected").write_bytes(b"ignored")
    order = ["ZBase", "Empty", "AOverride"]
    jobs = corpus.collect_jobs(tmp_path, order, 6, 5)
    assert [job[2] for job in jobs] == ["ZBase", "AOverride"]
    files = [corpus.scan_file(job) for job in jobs]
    encoded: list[bytes] = []

    def capture(data: bytes, level: int, gain: int) -> tuple[int, bytes]:
        encoded.append(data)
        return 0, data

    monkeypatch.setattr(corpus, "encode_resource_pack_blob", capture)
    result = corpus.merged_index_size(list(reversed(files)), order, 6, 5)
    expected_size = 32 * 3 + 56 + sum(len(name) for name in order) + len("Shared.txt")
    assert result["rawBytes"] == expected_size
    assert result["entries"] == 1
    assert result["packs"] == 3
    assert result["packOrder"] == order
    assert result["estimated"]
    entry = struct.unpack_from("<IIIIQQQQII", encoded[-1], 32 * 3)
    assert entry[2] == 2
    assert entry[4] == 80
    assert entry[5:7] == (16, 16)
    assert entry[7] != 0
    assert entry[8:] == (0, 0)
    for ordinal, name in enumerate(order):
        offset, length, identity, patch_hash, patch_end = struct.unpack_from("<IIQQQ", encoded[-1], ordinal * 32)
        assert encoded[-1][offset:offset + length].decode() == name
        assert identity != 0
        assert (patch_hash, patch_end) == (0, 0)
    corpus.merged_index_size(files, list(reversed(order)), 6, 5)
    assert struct.unpack_from("<Q", encoded[-1], 32 * 3 + 32)[0] == 4


@pytest.mark.parametrize("script", ["analyze_resource_corpus.py", "measure_resource_packs.py"])
@pytest.mark.parametrize("option,value", [("--compress-level", "-1"), ("--compress-level", "10"), ("--min-gain-percent", "-1"), ("--min-gain-percent", "101")])
def test_invalid_compression_options_are_cli_errors(tmp_path: Path, script: str, option: str, value: str) -> None:
    result = subprocess.run([sys.executable, str(BUILDTOOLS / script), "--baked-root", str(tmp_path), "--packs", "Pack", option, value], capture_output=True, text=True)
    assert result.returncode == 2
    assert "invalid choice" in result.stderr
    assert "Traceback" not in result.stderr


def test_empty_corpus_files_have_a_valid_report(tmp_path: Path) -> None:
    (tmp_path / "Pack").mkdir()
    (tmp_path / "Pack/Empty").write_bytes(b"")
    result = subprocess.run([sys.executable, str(BUILDTOOLS / "analyze_resource_corpus.py"), "--baked-root", str(tmp_path), "--packs", "Pack", "--jobs", "1"], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert "0.0 %" in result.stdout


@pytest.mark.parametrize("packs", ["Pack,Pack", "Missing", "../outside"])
def test_invalid_pack_selection_is_a_cli_error(tmp_path: Path, packs: str) -> None:
    (tmp_path / "Pack").mkdir()
    result = subprocess.run([sys.executable, str(BUILDTOOLS / "analyze_resource_corpus.py"), "--baked-root", str(tmp_path), "--packs", packs], capture_output=True, text=True)
    assert result.returncode == 2
    assert "Traceback" not in result.stderr
