from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


HEADER_SIZE = _package.RESOURCE_PACK_HEADER_SIZE
ENTRY_SIZE = _package.RESOURCE_PACK_ENTRY_SIZE
CODEC_STORED = _package.RESOURCE_PACK_CODEC_STORED
CODEC_DEFLATE = _package.RESOURCE_PACK_CODEC_DEFLATE


def _write_pack(archive_path: Path, entries: list[tuple[str, Path]], level: int = 6, min_gain: int = 5) -> bytes:
    _package.write_resource_pack(archive_path, [(name, str(path)) for name, path in entries], level, min_gain)
    return archive_path.read_bytes()


def _parse(data: bytes) -> tuple[dict[str, object], list[dict[str, object]]]:
    """Read the pack back the way the engine reader does, so the test pins the layout rather than the writer."""
    magic, version_major, version_minor = struct.unpack_from("<IHH", data, 0)
    assert magic == _package.RESOURCE_PACK_MAGIC
    assert _package.fnv1a_64(data[:64]) == struct.unpack_from("<Q", data, 64)[0]
    assert _package.fnv1a_64(data[HEADER_SIZE:]) == struct.unpack_from("<Q", data, 8)[0]

    index_offset, index_stored_size, index_decoded_size = struct.unpack_from("<QQQ", data, 16)
    index_codec, entry_count = struct.unpack_from("<II", data, 40)
    data_offset, data_size = struct.unpack_from("<QQ", data, 48)

    assert data_offset == HEADER_SIZE
    assert index_offset == HEADER_SIZE + data_size
    assert index_offset + index_stored_size == len(data)

    stored_index = data[index_offset : index_offset + index_stored_size]
    index = zlib.decompress(stored_index) if index_codec == CODEC_DEFLATE else stored_index
    assert len(index) == index_decoded_size

    entries: list[dict[str, object]] = []

    for i in range(entry_count):
        path_offset, path_length, blob_offset, stored_size, decoded_size, codec, flags = struct.unpack_from("<IIQQQII", index, i * ENTRY_SIZE)
        assert flags == 0
        blob = data[blob_offset : blob_offset + stored_size]
        entries.append(
            {
                "path": index[path_offset : path_offset + path_length].decode("utf-8"),
                "codec": codec,
                "raw": zlib.decompress(blob) if codec == CODEC_DEFLATE else blob,
                "decoded_size": decoded_size,
            }
        )

    header = {"version_major": version_major, "version_minor": version_minor, "entry_count": entry_count}
    return header, entries


def _make_tree(tmp_path: Path) -> list[tuple[str, Path]]:
    base = tmp_path / "Pack"
    (base / "nested").mkdir(parents=True)

    compressible = base / "nested" / "Compressible.bin"
    compressible.write_bytes(b"A" * 4096)

    incompressible = base / "Incompressible.bin"
    state = 0x12345678
    noise = bytearray()

    for _ in range(4096):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        noise.append(state >> 24)

    incompressible.write_bytes(bytes(noise))

    tiny = base / "Tiny.txt"
    tiny.write_bytes(b"short")

    empty = base / "Empty.bin"
    empty.write_bytes(b"")

    return [
        ("Tiny.txt", tiny),
        ("nested/Compressible.bin", compressible),
        ("Incompressible.bin", incompressible),
        ("Empty.bin", empty),
    ]


def test_resource_pack_round_trips_every_entry(tmp_path: Path) -> None:
    entries = _make_tree(tmp_path)
    data = _write_pack(tmp_path / "Pack.fores", entries)
    header, parsed = _parse(data)

    assert header["version_major"] == _package.RESOURCE_PACK_VERSION_MAJOR
    assert header["entry_count"] == len(entries)

    by_path = {entry["path"]: entry for entry in parsed}
    assert sorted(by_path) == sorted(name for name, _ in entries)

    for name, path in entries:
        assert by_path[name]["raw"] == path.read_bytes()
        assert by_path[name]["decoded_size"] == path.stat().st_size


def test_resource_pack_sorts_entries_by_path(tmp_path: Path) -> None:
    _, parsed = _parse(_write_pack(tmp_path / "Pack.fores", _make_tree(tmp_path)))
    paths = [entry["path"] for entry in parsed]
    assert paths == sorted(paths)


def test_resource_pack_stores_what_deflate_cannot_shrink(tmp_path: Path) -> None:
    _, parsed = _parse(_write_pack(tmp_path / "Pack.fores", _make_tree(tmp_path)))
    by_path = {entry["path"]: entry for entry in parsed}

    assert by_path["nested/Compressible.bin"]["codec"] == CODEC_DEFLATE
    assert by_path["Incompressible.bin"]["codec"] == CODEC_STORED
    # Below the minimum size the writer never even tries, so a short file is stored whatever it holds
    assert by_path["Tiny.txt"]["codec"] == CODEC_STORED
    assert by_path["Empty.bin"]["codec"] == CODEC_STORED


def test_resource_pack_ignores_input_order(tmp_path: Path) -> None:
    entries = _make_tree(tmp_path)
    first = _write_pack(tmp_path / "First.fores", entries)
    second = _write_pack(tmp_path / "Second.fores", list(reversed(entries)))
    assert first == second


def test_resource_pack_refuses_a_duplicate_path(tmp_path: Path) -> None:
    entries = _make_tree(tmp_path)
    duplicate = [entries[0], entries[0]]

    with pytest.raises(AssertionError):
        _write_pack(tmp_path / "Duplicate.fores", duplicate)


def test_resource_pack_min_gain_keeps_a_weak_win_stored(tmp_path: Path) -> None:
    entries = [entry for entry in _make_tree(tmp_path) if entry[0] == "nested/Compressible.bin"]

    _, greedy = _parse(_write_pack(tmp_path / "Greedy.fores", entries, min_gain=0))
    _, strict = _parse(_write_pack(tmp_path / "Strict.fores", entries, min_gain=100))

    assert greedy[0]["codec"] == CODEC_DEFLATE
    assert strict[0]["codec"] == CODEC_STORED
