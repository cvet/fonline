from __future__ import annotations

import functools
import random
import subprocess
import sys
from pathlib import Path

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import package  # noqa: E402
import resource_pack_hash  # noqa: E402
from test_package_parallel_resources import raw_fixture, resource_packager, source  # noqa: E402
from test_package_resource_pack import test_validation_rejects_bad_entries_even_with_valid_pack_checksums as reject_bad_entry  # noqa: E402


@pytest.fixture(scope="module")
def hash_library(tmp_path_factory: pytest.TempPathFactory) -> Path:
    build = tmp_path_factory.mktemp("resource-hash-build")
    for command in (
        ["cmake", "-S", str(BUILDTOOLS / "resource-pack-hash"), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
        ["cmake", "--build", str(build), "--config", "Release", "--parallel", "1"],
    ):
        result = subprocess.run(command, capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
    library = build / "bin" / resource_pack_hash.library_name()
    assert library.is_file()
    return library


@pytest.mark.parametrize("size", [0, 1, 2, 7, 8, 63, 64, 255, 256, 1023, 1024, 65535, 1048576])
@pytest.mark.parametrize("seed", [0, 1, package.FNV_OFFSET, (1 << 64) - 1])
def test_native_hash_matches_python_and_streamed_chunks(hash_library: Path, size: int, seed: int) -> None:
    hasher = resource_pack_hash.ResourcePackHasher(str(hash_library))
    data = random.Random(size).randbytes(size)
    expected = package.fnv1a_64(data, seed)
    assert hasher.hash_bytes(data, seed) == expected
    split = size // 2
    assert hasher.hash_bytes(data[split:], hasher.hash_bytes(data[:split], seed)) == expected


@pytest.mark.parametrize("buffer_type", [bytes, bytearray, memoryview])
def test_native_hash_uses_the_complete_buffer_including_nuls(hash_library: Path, buffer_type: type) -> None:
    data = b"foo\0bar\xff"
    hasher = resource_pack_hash.ResourcePackHasher(str(hash_library))
    assert hasher.hash_bytes(buffer_type(data)) == package.fnv1a_64(data)


def test_native_writer_matches_python_pack_bytes_and_rejects_corruption(hash_library: Path, tmp_path: Path) -> None:
    hasher = resource_pack_hash.ResourcePackHasher(str(hash_library))
    entries = []
    for name, data in [("empty", b""), ("stored", bytes(range(256))), ("compressed", b"\0" * 65536)]:
        path = tmp_path / name
        path.write_bytes(data)
        entries.append((name, path))
    python_pack = tmp_path / "python.fores"
    native_pack = tmp_path / "native.fores"
    package.write_resource_pack(python_pack, entries, 1, 5)
    package.write_resource_pack(native_pack, entries, 1, 5, hash_bytes=hasher.hash_bytes)
    assert native_pack.read_bytes() == python_pack.read_bytes()
    package.validate_resource_pack(native_pack, sorted(name for name, _ in entries), hash_bytes=hasher.hash_bytes)
    data = bytearray(native_pack.read_bytes())
    data[package.RESOURCE_PACK_HEADER_SIZE] ^= 1
    native_pack.write_bytes(data)
    with pytest.raises(AssertionError, match="pack checksum mismatch"):
        package.validate_resource_pack(native_pack, sorted(name for name, _ in entries), hash_bytes=hasher.hash_bytes)


@pytest.mark.parametrize("jobs", [1, 4])
def test_native_raw_cli_matches_python_output(hash_library: Path, tmp_path: Path, jobs: int, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv(package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, raising=False)
    arguments = raw_fixture(tmp_path)
    trees = []
    for backend in ("python", "native"):
        output = tmp_path / backend
        command = arguments + ["-output", str(output), "-resource-pack-jobs", str(jobs)]
        if backend == "native":
            command += ["-resource-pack-hash-library", str(hash_library)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
        if backend == "native":
            assert "Resource archive native hash" in result.stdout
        trees.append({path.relative_to(output).as_posix(): path.read_bytes() for path in output.rglob("*") if path.is_file()})
    assert trees[0] == trees[1]


def test_loaded_native_hasher_is_not_pickled_into_workers(hash_library: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv(package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, raising=False)
    packager = resource_packager(tmp_path)
    packager.resource_pack_hash_library = str(hash_library)
    packs = [(name, [source(tmp_path, name, "payload", name.encode() * 5000)], "Resources") for name in ("Alpha", "Beta")]
    packager.package_resource_pack(*packs[0])
    assert packager.resource_pack_hasher is not None
    packager.package_resource_packs(packs)
    for name, _, _ in packs:
        package.validate_resource_pack(tmp_path / f"output/Resources/{name}.fores", ["payload"])


def test_present_invalid_library_fails_instead_of_falling_back(tmp_path: Path) -> None:
    library = tmp_path / resource_pack_hash.library_name()
    library.write_bytes(b"invalid native library")
    with pytest.raises(OSError):
        resource_pack_hash.ResourcePackHasher(str(library))
    with pytest.raises(AssertionError, match="not found"):
        resource_pack_hash.discover_library([], str(tmp_path / "missing"))
    assert resource_pack_hash.discover_library([str(tmp_path)]) is None


def test_discovery_uses_input_priority_and_interpreter_width(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(resource_pack_hash.sys, "platform", "linux")
    monkeypatch.setattr(resource_pack_hash.platform, "machine", lambda: "x86_64")
    monkeypatch.setattr(resource_pack_hash.sys, "maxsize", (1 << 63) - 1)
    paths = []
    for name in ("first", "second"):
        library = tmp_path / name / "Binaries/BuildTools-Linux-x64/FOnlineResourcePackHash.so"
        library.parent.mkdir(parents=True)
        library.write_bytes(b"discovery fixture")
        paths.append(library)
    roots = [str(tmp_path / name) for name in ("first", "second")]
    assert resource_pack_hash.discover_library(roots) == str(paths[0].resolve())
    assert resource_pack_hash.discover_library(roots, str(paths[1])) == str(paths[1].resolve())
    monkeypatch.setattr(resource_pack_hash.sys, "maxsize", (1 << 31) - 1)
    assert resource_pack_hash.discover_library(roots) is None


@pytest.mark.parametrize("case", ["stored_size", "stored_hash", "deflate_invalid", "deflate_size", "deflate_hash", "deflate_truncated", "deflate_trailing", "path_backslash"])
def test_native_validator_rejects_malformed_payloads_with_valid_outer_hashes(hash_library: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, case: str) -> None:
    hasher = resource_pack_hash.ResourcePackHasher(str(hash_library))
    monkeypatch.setattr(package, "validate_resource_pack", functools.partial(package.validate_resource_pack, hash_bytes=hasher.hash_bytes))
    reject_bad_entry(tmp_path, case)


def test_host_library_does_not_inherit_engine_sanitizer_options(tmp_path: Path) -> None:
    parent = tmp_path / "parent"
    parent.mkdir()
    parent.joinpath("CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.22)\nproject(HashHost LANGUAGES CXX)\n'
        'if(MSVC)\nadd_compile_options(/fsanitize=address)\nelse()\n'
        'add_compile_options(-fsanitize=address)\nadd_link_options(-fsanitize=address)\nendif()\n'
        f'add_subdirectory("{(BUILDTOOLS / "resource-pack-hash").as_posix()}" hash)\n', encoding="utf-8")
    build = tmp_path / "build"
    for command in (
        ["cmake", "-S", str(parent), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
        ["cmake", "--build", str(build), "--config", "Release", "--parallel", "1"],
    ):
        result = subprocess.run(command, capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
    library = build / "hash/bin" / resource_pack_hash.library_name()
    result = subprocess.run([sys.executable, "-c",
                             "import sys; sys.path.insert(0, sys.argv[1]); from resource_pack_hash import ResourcePackHasher; "
                             "assert ResourcePackHasher(sys.argv[2]).hash_bytes(b'foobar') == 0x85944171f73967e8",
                             str(BUILDTOOLS), str(library)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
