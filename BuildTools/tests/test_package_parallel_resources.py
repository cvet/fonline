from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import package  # noqa: E402


@pytest.fixture(autouse=True)
def isolated_archive_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv(package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, raising=False)


def resource_packager(tmp_path: Path) -> package.Packager:
    packager = package.Packager.__new__(package.Packager)
    packager.baking_path = str(tmp_path / "Baking")
    packager.target_output_path = str(tmp_path / "output")
    packager.resource_pack_compress_level = 6
    packager.resource_pack_min_compress_gain = 5
    packager.resource_pack_jobs = 4
    packager.resource_archive_paths = {}
    (tmp_path / "output/Resources").mkdir(parents=True)
    return packager


def source(tmp_path: Path, pack: str, name: str, content: bytes) -> str:
    path = tmp_path / "Baking" / pack / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return str(path)


def test_parallel_colliding_destinations_keep_order_and_current_cache_key(tmp_path: Path) -> None:
    packager = resource_packager(tmp_path)
    server = source(tmp_path, "Shared", "server", b"server")
    client = source(tmp_path, "Shared", "client", b"client")
    other = source(tmp_path, "Other", "payload", b"other")
    packs = [("Shared", [server], "Resources"), ("Other", [other], "Resources"),
             ("Shared", [client], "./Resources")]

    packager.package_resource_packs(packs)

    archive = tmp_path / "output/Resources/Shared.fores"
    package.validate_resource_pack(archive, ["client"])
    expected = tmp_path / "expected.fores"
    package.write_resource_pack(expected, [("client", client)], 6, 5)
    assert archive.read_bytes() == expected.read_bytes()
    old_key = packager.resource_archive_cache_key([("server", server)])
    new_key = packager.resource_archive_cache_key([("client", client)])
    assert old_key not in packager.resource_archive_paths
    assert Path(packager.resource_archive_paths[new_key]).resolve() == archive.resolve()
    assert len(packager.resource_archive_paths) == 2


def test_name_reuse_and_destination_collisions_preserve_transitive_order(tmp_path: Path) -> None:
    packager = resource_packager(tmp_path)
    (tmp_path / "output/Resources/Nested").mkdir()
    server = source(tmp_path, "Shared", "server", b"server")
    older = source(tmp_path, "Nested/Shared", "older", b"older")
    client = source(tmp_path, "Shared", "client", b"client")
    other = source(tmp_path, "Other", "payload", b"other")

    packager.package_resource_packs([
        ("Shared", [server], "Resources"), ("Nested/Shared", [older], "Resources"),
        ("Shared", [client], "Resources/Nested"), ("Other", [other], "Resources")])

    package.validate_resource_pack(tmp_path / "output/Resources/Shared.fores", ["server"])
    package.validate_resource_pack(tmp_path / "output/Resources/Nested/Shared.fores", ["client"])
    assert packager.resource_archive_cache_key([("older", older)]) not in packager.resource_archive_paths
    assert len(packager.resource_archive_paths) == 3


def test_parallel_cache_unavailable_state_reaches_the_parent(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    packager = resource_packager(tmp_path)
    helper = tmp_path / "unavailable.py"
    helper.write_text("from pathlib import Path\n"
                      "with Path(__file__).with_suffix('.calls').open('a') as log:\n"
                      "    log.write('unavailable\\n')\nraise SystemExit(3)\n", encoding="utf-8")
    monkeypatch.setenv(package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, str(helper))
    packager.resource_pack_jobs = 2
    packs = [(name, [source(tmp_path, name, "payload", name.encode())], "Resources")
             for name in ("Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta")]

    packager.package_resource_packs(packs)

    assert packager.resource_archive_cache_unavailable
    assert len(helper.with_suffix('.calls').read_text().splitlines()) <= 2
    for name, _, _ in packs:
        package.validate_resource_pack(tmp_path / f"output/Resources/{name}.fores", ["payload"])
    assert packager.run_resource_archive_cache_helper("restore", "unused", "unused") is None


def raw_fixture(tmp_path: Path) -> list[str]:
    config = tmp_path / "Game.fomain"
    text = """Baking.BakeOutput = Baking
Baking.ServerResources = ServerResources
Baking.ClientResources = Resources
Baking.PlatformBinaries = PlatformBinaries
Baking.ResourcePackCompressLevel = 6
Baking.ResourcePackMinCompressGain = 5
Baking.BundleCompressLevel = 6
[ResourcePack]
Name = Embedded
[ResourcePack]
Name = Alpha
[ResourcePack]
Name = Beta
"""
    config.write_text(text, encoding="utf-8")
    for name in ("Embedded", "Alpha", "Beta"):
        source(tmp_path, name, "payload", (name.encode() * 3000) + bytes(range(256)))
    baked = tmp_path / "Baking"
    (baked / "Resources.build-hash").write_text("fixture", encoding="utf-8")
    (baked / "Configs").mkdir()
    for target in ("server", "client"):
        (baked / f"Configs/LocalTest.fomain-{target}").write_text(text, encoding="utf-8")
    binaries = tmp_path / "Binaries/Server-Linux-x64"
    binaries.mkdir(parents=True)
    (binaries / "Game_Server.build-hash").write_text("fixture", encoding="utf-8")
    (binaries / "Game_Server").write_bytes(
        package.make_embedded_marker(10000) + package.INTERNAL_CONFIG_MARKER + b"\0" * 10000 +
        package.INTERNAL_CONFIG_END_MARKER + package.PACKAGED_BUILD_NAME_MARKER + b"\0" * 128)
    return [sys.executable, str(BUILDTOOLS_DIR / "package.py"), "-maincfg", str(config),
            "-buildhash", "fixture", "-devname", "Game", "-nicename", "Game", "-target", "Server",
            "-platform", "Linux", "-arch", "x64", "-pack", "Raw", "-config", "LocalTest",
            "-input", str(tmp_path)]


@pytest.mark.parametrize("warm_cache", [False, True])
def test_raw_cli_parallel_package_matches_serial_package(tmp_path: Path, warm_cache: bool) -> None:
    arguments = raw_fixture(tmp_path)
    env = os.environ.copy()
    env.pop(package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV, None)
    if warm_cache:
        cache = tmp_path / "cache"
        cache.mkdir()
        helper = tmp_path / "cache.py"
        helper.write_text("""import os, shutil, sys
from pathlib import Path
action = sys.argv[1]
archive = Path(sys.argv[sys.argv.index('--archive') + 1])
cached = Path(__file__).parent / 'cache' / sys.argv[sys.argv.index('--key') + 1]
if action == 'restore':
    if not cached.is_file():
        raise SystemExit(2)
    shutil.copyfile(cached, archive)
elif action == 'store':
    temporary = cached.with_suffix('.' + str(os.getpid()))
    shutil.copyfile(archive, temporary)
    os.replace(temporary, cached)
""", encoding="utf-8")
        env[package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV] = str(helper)
    trees = []
    for jobs in (1, 4):
        output = tmp_path / f"output-{jobs}"
        result = subprocess.run(arguments + ["-output", str(output), "-resource-pack-jobs", str(jobs)],
                                capture_output=True, text=True, env=env)
        assert result.returncode == 0, result.stdout + result.stderr
        assert result.stdout.count("Resource archive local hit") == 2
        if warm_cache and jobs == 4:
            assert result.stdout.count("Resource archive cache hit") == 2
        trees.append({path.relative_to(output).as_posix(): path.read_bytes()
                      for path in output.rglob("*") if path.is_file()})
    assert trees[0] == trees[1]
    archives = list((tmp_path / "output-4").rglob("*.fores"))
    assert len(archives) == 4
    for archive in archives:
        package.validate_resource_pack(archive, ["payload"])


def test_parallel_poisoned_cache_fails_and_workers_finish_before_cleanup(tmp_path: Path) -> None:
    arguments = raw_fixture(tmp_path)
    helper = tmp_path / "cache.py"
    helper.write_text("""import sys, time
from pathlib import Path
action = sys.argv[1]
archive = Path(sys.argv[sys.argv.index('--archive') + 1])
trace = Path(__file__).parent / (archive.parent.name + '-' + sys.argv[sys.argv.index('--key') + 1] + '.trace')
with trace.open('a') as log:
    log.write(action + '\\n')
if action == 'restore':
    if archive.name == 'Alpha.fores':
        archive.write_bytes(b'corrupt')
        raise SystemExit(0)
    time.sleep(0.2)
    raise SystemExit(2)
""", encoding="utf-8")
    output = tmp_path / "failed"
    result = subprocess.run(arguments + ["-output", str(output)], capture_output=True, text=True,
                            env={**os.environ, package.RESOURCE_PACK_JOBS_ENV: "4",
                                 package.RESOURCE_ARCHIVE_CACHE_HELPER_ENV: str(helper)})
    assert result.returncode != 0
    assert "Resource pack validation failed" in result.stderr
    assert not (output / "Game-Server-LocalTest-Linux").exists()
    traces = [path.read_text().splitlines() for path in tmp_path.glob("*.trace")]
    assert ["restore"] in traces
    assert ["restore", "store"] in traces


@pytest.mark.parametrize("value", ["0", "-1", "invalid"])
def test_cli_rejects_invalid_worker_limit_before_creating_output(tmp_path: Path, value: str) -> None:
    arguments = raw_fixture(tmp_path)
    output = tmp_path / "invalid"
    result = subprocess.run(arguments + ["-output", str(output)], capture_output=True, text=True,
                            env={**os.environ, package.RESOURCE_PACK_JOBS_ENV: value})
    assert result.returncode != 0
    assert "positive integer" in result.stderr
    assert not output.exists()
