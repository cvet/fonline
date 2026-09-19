from __future__ import annotations

import shutil
import struct
import sys
import zlib
from pathlib import Path
from types import SimpleNamespace

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


HEADER_SIZE = _package.RESOURCE_PACK_HEADER_SIZE
ENTRY_SIZE = _package.RESOURCE_PACK_ENTRY_SIZE
CODEC_STORED = _package.RESOURCE_PACK_CODEC_STORED
CODEC_DEFLATE = _package.RESOURCE_PACK_CODEC_DEFLATE


def test_android_activity_resolves_the_configured_asset_directory(tmp_path: Path) -> None:
    packager = _package.Packager.__new__(_package.Packager)
    packager.target_output_path = str(tmp_path)
    packager.client_res_dir = "Content/Resource Packs"
    packager.args = SimpleNamespace(config="LocalTest")
    template = tmp_path / "app/src/main/java-template"
    template.mkdir(parents=True)
    shutil.copy(BUILDTOOLS_DIR / "android-project/app/src/main/java-template/FOnlineActivity.java", template)
    packager.patch_android_activity("com.fonline.test")
    activity = (tmp_path / "app/src/main/java/com/fonline/test/FOnlineActivity.java").read_text()
    assert 'getApplicationInfo().sourceDir + "!/assets/" + "Content/Resource Packs"' in activity
    assert "getAssets().open" not in activity
    assert "$RESOURCE_DIRECTORY$" not in activity


def _write_pack(archive_path: Path, entries: list[tuple[str, Path]], level: int = 6, min_gain: int = 5) -> bytes:
    _package.write_resource_pack(archive_path, [(name, str(path)) for name, path in entries], level, min_gain)
    return archive_path.read_bytes()


def _parse(data: bytes) -> tuple[dict[str, object], list[dict[str, object]]]:
    """Read the pack back the way the engine reader does, so the test pins the layout rather than the writer."""
    magic, version_major, version_minor = struct.unpack_from("<IHH", data, 0)
    assert magic == _package.RESOURCE_PACK_MAGIC
    assert _package.fnv1a_64(data[:72]) == struct.unpack_from("<Q", data, 72)[0]
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
        path_offset, path_length, blob_offset, stored_size, decoded_size, codec, flags, file_hash = struct.unpack_from("<IIQQQIIQ", index, i * ENTRY_SIZE)
        assert flags == 0
        blob = data[blob_offset : blob_offset + stored_size]
        decoded = zlib.decompress(blob) if codec == CODEC_DEFLATE else blob
        assert _package.fnv1a_64(decoded) == file_hash
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
    _package.validate_resource_pack(tmp_path / "Pack.fores", sorted(name for name, _ in entries))

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


def test_content_hash_is_independent_of_compression(tmp_path: Path) -> None:
    entries = _make_tree(tmp_path)
    stored = _write_pack(tmp_path / "Stored.fores", entries, min_gain=100)
    compressed = _write_pack(tmp_path / "Compressed.fores", entries)
    assert struct.unpack_from("<Q", stored, 8) != struct.unpack_from("<Q", compressed, 8)
    assert struct.unpack_from("<Q", stored, 64) == struct.unpack_from("<Q", compressed, 64)
    _, parsed = _parse(compressed)
    logical = bytearray(struct.pack("<I", len(parsed)))
    for entry in parsed:
        name = entry["path"].encode("utf-8")
        logical.extend(struct.pack("<IQQ", len(name), len(entry["raw"]), _package.fnv1a_64(entry["raw"])))
        logical.extend(name)
    assert _package.fnv1a_64(logical) == struct.unpack_from("<Q", stored, 64)[0]


@pytest.mark.parametrize("name", ["/root", "../escape", "a/../b", "a/./b", "a//b", "a/", "C:drive", "nul\0name"])
def test_resource_pack_rejects_noncanonical_paths(tmp_path: Path, name: str) -> None:
    source = tmp_path / "source"
    source.write_bytes(b"content")
    with pytest.raises(AssertionError):
        _write_pack(tmp_path / "Invalid.fores", [(name, source)])


def _pack_with_payload(payload: bytes, codec: int, decoded_size: int, file_hash: int, name: bytes = b"File.bin") -> bytes:
    index = struct.pack("<IIQQQIIQ", ENTRY_SIZE, len(name), HEADER_SIZE, len(payload), decoded_size, codec, 0, file_hash) + name
    content_hash = _package.fnv1a_64(struct.pack("<IIQQ", 1, len(name), decoded_size, file_hash) + name)
    header = bytearray(HEADER_SIZE)
    struct.pack_into("<IHHQ", header, 0, _package.RESOURCE_PACK_MAGIC, 2, 0, _package.fnv1a_64(payload + index))
    struct.pack_into("<QQQIIQQQ", header, 16, HEADER_SIZE + len(payload), len(index), len(index), CODEC_STORED, 1, HEADER_SIZE, len(payload), content_hash)
    struct.pack_into("<Q", header, 72, _package.fnv1a_64(header[:72]))
    return bytes(header) + payload + index


@pytest.mark.parametrize("case", ["stored_size", "stored_hash", "deflate_invalid", "deflate_size", "deflate_hash", "deflate_truncated", "deflate_trailing", "path_backslash"])
def test_validation_rejects_bad_entries_even_with_valid_pack_checksums(tmp_path: Path, case: str) -> None:
    raw = b"Resource content" * 32
    payload = zlib.compress(raw)
    codec = CODEC_DEFLATE
    decoded_size = len(raw)
    file_hash = _package.fnv1a_64(raw)
    name = b"File.bin"
    if case.startswith("stored"):
        codec, payload = CODEC_STORED, raw
    if case.endswith("size"):
        decoded_size -= 1
    elif case.endswith("hash"):
        file_hash ^= 1
    elif case == "deflate_invalid":
        payload = b"invalid zlib stream"
    elif case == "deflate_truncated":
        payload = payload[:-1]
    elif case == "deflate_trailing":
        payload += b"junk"
    elif case == "path_backslash":
        name = b"Sub\\File.bin"
    archive = tmp_path / "Invalid.fores"
    archive.write_bytes(_pack_with_payload(payload, codec, decoded_size, file_hash, name))
    with pytest.raises(AssertionError, match="Resource pack validation failed"):
        _package.validate_resource_pack(archive, [name.decode()])


@pytest.mark.parametrize("raw", [b"", b"x", b"abc" * 1000])
@pytest.mark.parametrize("codec", [CODEC_STORED, CODEC_DEFLATE])
def test_payload_validation_streams_across_small_chunks(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, raw: bytes, codec: int) -> None:
    monkeypatch.setattr(_package, "RESOURCE_ARCHIVE_HASH_CHUNK_BYTES", 17)
    payload = zlib.compress(raw) if codec == CODEC_DEFLATE else raw
    archive = tmp_path / "Streamed.fores"
    archive.write_bytes(_pack_with_payload(payload, codec, len(raw), _package.fnv1a_64(raw)))
    _package.validate_resource_pack(archive, ["File.bin"])
