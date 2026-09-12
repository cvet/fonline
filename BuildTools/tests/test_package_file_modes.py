from __future__ import annotations

import json
import os
import sys
import tarfile
import zipfile
from pathlib import Path
from types import SimpleNamespace

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


def test_linux_packager_records_raw_executable_mode_on_windows_too(tmp_path: Path) -> None:
    package_root = tmp_path / "LF-Prod"
    target_root = package_root / "LF-Server-PublicGame-Linux"
    target_root.mkdir(parents=True)
    packager = _package.Packager.__new__(_package.Packager)
    packager.args = SimpleNamespace(
        target="Server",
        platform="Linux",
        arch="x64",
        devname="LF",
        nicename="LastFrontier",
        config="PublicGame",
        binary_output_postfix="",
    )
    packager.pack_args = {"NoRes", "Raw"}
    packager.output_path = str(package_root)
    packager.target_output_path = str(target_root)
    packager.logical_file_modes = {}
    packager.resolve_binary_input_dir = lambda *_args: str(tmp_path / "input")

    def fake_package_binary(_input: str, _input_name: str, output_name: str, _ext: str,
                            _config: str | None, _excluded: set[str]) -> str:
        output = target_root / output_name
        output.write_bytes(b"linux executable")
        return str(output)

    packager.package_platform_binary = fake_package_binary

    packager.package_linux()
    packager.persist_logical_file_modes()

    assert packager.logical_file_modes == {"LF_Server": 0o755}
    assert _package.read_package_mode_manifest(package_root) == {
        "LF-Server-PublicGame-Linux/LF_Server": 0o755,
    }


def test_zip_uses_logical_mode_independently_of_host_filesystem(tmp_path: Path) -> None:
    payload = tmp_path / "payload"
    payload.mkdir()
    executable = payload / "LF_Server"
    executable.write_bytes(b"linux executable")
    os.chmod(executable, 0o644)

    archive_path = tmp_path / "server.zip"
    _package.make_zip(archive_path, payload, 6, mode_overrides={"LF_Server": 0o755})

    with zipfile.ZipFile(archive_path) as archive:
        info = archive.getinfo("LF_Server")
        assert info.create_system == 3
        assert (info.external_attr >> 16) & 0o777 == 0o755


def test_tar_uses_logical_mode_independently_of_host_filesystem(tmp_path: Path) -> None:
    payload = tmp_path / "LF-Server-Linux"
    payload.mkdir()
    executable = payload / "LF_Server"
    executable.write_bytes(b"linux executable")
    os.chmod(executable, 0o644)

    archive_path = tmp_path / "server.tar"
    _package.make_tar(archive_path, payload, "w", {"LF_Server": 0o755})

    with tarfile.open(archive_path) as archive:
        assert archive.getmember("LF-Server-Linux/LF_Server").mode == 0o755


def test_package_mode_manifest_merges_raw_package_parts(tmp_path: Path) -> None:
    package_root = tmp_path / "LF-Prod"
    first_target = package_root / "LF-Client"
    second_target = package_root / "LF-Server-Linux"
    first_target.mkdir(parents=True)
    second_target.mkdir()
    (first_target / "client-helper").write_bytes(b"client")
    (second_target / "LF_Server").write_bytes(b"server")

    _package.write_package_mode_manifest(package_root, {"LF-Client/client-helper": 0o755})
    modes = _package.read_package_mode_manifest(package_root)
    modes["LF-Server-Linux/LF_Server"] = 0o755
    _package.write_package_mode_manifest(package_root, modes)

    assert _package.read_package_mode_manifest(package_root) == {
        "LF-Client/client-helper": 0o755,
        "LF-Server-Linux/LF_Server": 0o755,
    }
    manifest = json.loads((package_root / _package.PACKAGE_MODE_MANIFEST).read_text(encoding="utf-8"))
    assert manifest == {
        "version": 1,
        "files": {
            "LF-Client/client-helper": "755",
            "LF-Server-Linux/LF_Server": "755",
        },
    }


@pytest.mark.parametrize("relative_path", ["../outside", "/absolute", "C:/windows", "not\\posix"])
def test_package_mode_manifest_rejects_unsafe_paths(tmp_path: Path, relative_path: str) -> None:
    package_root = tmp_path / "LF-Prod"
    package_root.mkdir()

    with pytest.raises(AssertionError, match="Package mode path"):
        _package.write_package_mode_manifest(package_root, {relative_path: 0o755})
