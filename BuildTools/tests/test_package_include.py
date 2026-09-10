from __future__ import annotations

import sys
import shutil
import subprocess
import zipfile
from pathlib import Path

import pytest


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
    archive_path = package_root / "LF-Dev.zip"
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
    archive_path = package_root / "LF-Dev.zip"

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


@pytest.mark.skipif(shutil.which("cmake") is None or shutil.which("ninja") is None, reason="CMake and Ninja are required")
def test_cmake_package_includes_reach_the_archive_written_by_the_packager(tmp_path: Path) -> None:
    output = tmp_path / "output"
    tools = ("FirstEditor", "SecondEditor", "ThirdEditor")
    for tool in tools:
        payload = output / "Binaries" / tool
        payload.mkdir(parents=True)
        (payload / "Editor.exe").write_bytes(tool.encode())

    wrapper = tmp_path / "fixture-engine" / "BuildTools" / "package.py"
    wrapper.parent.mkdir(parents=True)
    wrapper.write_text(
        "from pathlib import Path\nimport sys\n"
        f"sys.path.insert(0, {str(BUILDTOOLS_DIR)!r})\n"
        "import package\n"
        "if sys.argv[1] == 'include':\n"
        "    package.main()\n"
        "else:\n"
        "    args = package.parse_args()\n"
        "    packager = package.Packager.__new__(package.Packager)\n"
        "    packager.args = args\n"
        "    packager.pack_args = set(args.pack.split('+'))\n"
        "    packager.output_path = args.output\n"
        "    packager.target_output_path = str(Path(args.output) / 'Client')\n"
        "    packager.zip_compress_level = 6\n"
        "    Path(packager.target_output_path).mkdir(parents=True)\n"
        "    (Path(packager.target_output_path) / 'LF_Client.exe').write_bytes(b'client')\n"
        "    packager.finalize_output()\n",
        encoding="utf-8",
    )
    (tmp_path / "Test.fomain").write_text("Baking.ZipCompressLevel = 6\n", encoding="utf-8")
    cmake = '''cmake_minimum_required(VERSION 3.22)
project(PackageInclude NONE)
macro(StatusMessage)
endmacro()
macro(SetValue name)
    set(${name} ${ARGN})
endmacro()
macro(AddCommandTarget target)
    cmake_parse_arguments(ARG "" "WORKING_DIRECTORY;COMMENT" "COMMAND_ARGS" ${ARGN})
    add_custom_target(${target} ${ARG_COMMAND_ARGS}
        WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}" VERBATIM)
endmacro()
set(FO_PACKAGES Dev)
set(FO_ENGINE_ROOT fixture-engine)
set(FO_MAIN_CONFIG Test.fomain)
set(FO_BUILD_HASH test)
set(FO_DEV_NAME LF)
set(FO_NICE_NAME LastFrontier)
set(Package_Dev_Config Dev)
set(Package_Dev_Parts "Client,Linux,x64,SingleZip+Raw,Dev,")
'''
    cmake += f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
    cmake += f'set(FO_OUTPUT_PATH "{output.as_posix()}")\n'
    cmake += 'set(Package_Dev_IncludeSourceGlobs ' + " ".join(f'"Binaries/{tool}/*"' for tool in tools) + ')\n'
    cmake += 'set(Package_Dev_IncludeTargetPaths ' + " ".join(f'"Tools/{tool}"' for tool in tools) + ')\n'
    cmake += f'include("{(BUILDTOOLS_DIR / "cmake/helpers/Commands.cmake").as_posix()}")\n'
    cmake += f'include("{(BUILDTOOLS_DIR / "cmake/stages/Packages.cmake").as_posix()}")\n'
    (tmp_path / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
    build = tmp_path / "build"
    subprocess.run(["cmake", "-S", str(tmp_path), "-B", str(build), "-G", "Ninja"], capture_output=True, text=True, check=True)
    result = subprocess.run(["cmake", "--build", str(build), "--target", "MakePackage-Dev"], capture_output=True, text=True, check=True)

    package_root = output / "LF-Dev"
    with zipfile.ZipFile(package_root / "LF-Dev.zip") as archive:
        assert archive.read("LF_Client.exe") == b"client"
        for tool in tools:
            assert archive.read(f"Tools/{tool}/Editor.exe") == tool.encode()
            assert (package_root / "Tools" / tool / "Editor.exe").read_bytes() == tool.encode()
    assert "SingleZip not present" not in result.stdout
    assert not (output / "LF-Dev.zip").exists()
