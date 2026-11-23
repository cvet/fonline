from pathlib import Path
import shutil
import subprocess

import pytest


@pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is required")
def test_common_header_classification_requires_a_literal_dot(tmp_path: Path) -> None:
    buildtools = Path(__file__).resolve().parents[1]
    for name in ("common.h", "common.cpp", "lookalikeh", "client.h"):
        (tmp_path / name).write_text("")
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(ContributedHeaderClassification NONE)\n"
        f'include("{(buildtools / "Init.cmake").as_posix()}")\n'
        'set(FO_CONTRIBUTION_DIR "${CMAKE_CURRENT_SOURCE_DIR}")\n'
        "AddEngineSource(COMMON common.h common.cpp lookalikeh)\n"
        "AddEngineSource(CLIENT client.h)\n"
        'file(WRITE "${CMAKE_BINARY_DIR}/headers.txt" "${FO_ADDED_COMMON_HEADERS}")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/common.txt" "${FO_COMMON_SOURCE}")\n',
        encoding="utf-8",
    )
    build = tmp_path / "build"
    result = subprocess.run(
        ["cmake", "-S", str(tmp_path), "-B", str(build), "-Werror=dev"],
        capture_output=True, text=True,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    assert (build / "headers.txt").read_text().split(";") == [(tmp_path / "common.h").as_posix()]
    assert (build / "common.txt").read_text().split(";") == [
        (tmp_path / name).as_posix() for name in ("common.h", "common.cpp", "lookalikeh")
    ]
