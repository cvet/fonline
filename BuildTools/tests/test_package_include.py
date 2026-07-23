from __future__ import annotations

import sys
import zipfile
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


def test_package_include_copies_and_replaces_target_tree_and_single_zip(tmp_path: Path) -> None:
    input_root = tmp_path / "output"
    payload = input_root / "Binaries" / "ExternalTool"
    (payload / "resources").mkdir(parents=True)
    (payload / "Tool.exe").write_bytes(b"tool")
    (payload / ".tool-config").write_bytes(b"hidden-config")
    (payload / "resources" / "config.json").write_bytes(b"config")

    package_root = input_root / "LF-Dev"
    package_root.mkdir()
    (package_root / "LF_Client.exe").write_bytes(b"client")
    archive_path = input_root / "LF-Dev.zip"
    with zipfile.ZipFile(archive_path, "w") as archive:
        archive.write(package_root / "LF_Client.exe", "LF_Client.exe")

    _package.include_package_files(
        input_root,
        "Binaries/ExternalTool/*",
        package_root,
        "Tools/External",
        archive_path,
        6,
    )

    output = package_root / "Tools" / "External"
    assert (output / "Tool.exe").read_bytes() == b"tool"
    assert (output / ".tool-config").read_bytes() == b"hidden-config"
    assert (output / "resources" / "config.json").read_bytes() == b"config"

    (payload / "Tool.exe").write_bytes(b"updated-tool")
    (payload / "resources" / "config.json").unlink()
    _package.include_package_files(
        input_root,
        "Binaries/ExternalTool/*",
        package_root,
        "Tools/External",
        archive_path,
        6,
    )

    assert (output / "Tool.exe").read_bytes() == b"updated-tool"
    assert not (output / "resources" / "config.json").exists()
    with zipfile.ZipFile(archive_path, "r") as archive:
        names = archive.namelist()
        assert names.count("Tools/External/Tool.exe") == 1
        assert archive.read("Tools/External/Tool.exe") == b"updated-tool"
        assert archive.read("Tools/External/.tool-config") == b"hidden-config"
        assert archive.read("LF_Client.exe") == b"client"
        assert "Tools/External/resources/config.json" not in names


def test_package_include_keeps_files_in_package_root_without_single_zip(tmp_path: Path) -> None:
    input_root = tmp_path / "output"
    payload = input_root / "Binaries" / "ExternalTool"
    payload.mkdir(parents=True)
    (payload / "Tool.exe").write_bytes(b"tool")

    package_root = input_root / "LF-Dev"
    package_root.mkdir()
    archive_path = input_root / "LF-Dev.zip"

    _package.include_package_files(
        input_root,
        "Binaries/ExternalTool/*",
        package_root,
        "Tools/External",
        archive_path,
        6,
    )

    assert (package_root / "Tools" / "External" / "Tool.exe").read_bytes() == b"tool"
    assert not archive_path.exists()
