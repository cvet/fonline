from __future__ import annotations

import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


@pytest.fixture(scope="module")
def process_start_time(tmp_path_factory):
    if os.name != "nt":
        pytest.skip("Windows process-handle regression")
    compiler = shutil.which("clang++")
    if compiler is None:
        pytest.skip("clang++ is required for the Windows process probe")

    source = (Path(__file__).resolve().parents[2] / "Source/Essentials/WinApi.cpp").read_text(encoding="utf-8")
    functions = []
    for signature in (
        "static auto file_time_to_ticks(FILETIME time) noexcept -> uint64_t",
        "auto winapi::get_running_process_start_time(uint32_t pid) noexcept -> optional<uint64_t>",
    ):
        begin = source.index(signature + "\n{")
        end = source.index("\n}\n", begin) + len("\n}\n")
        functions.append(source[begin:end])

    root = tmp_path_factory.mktemp("process_identity")
    probe = root / "probe.cpp"
    library = root / "probe.dll"
    probe.write_text(
        "#include <Windows.h>\n#include <cstdint>\n#include <optional>\n"
        "using std::optional;\n#define FO_TRACE_ZONE(category)\n"
        "namespace winapi { auto get_running_process_start_time(uint32_t) noexcept -> optional<uint64_t>; }\n"
        + "\n".join(functions)
        + '\nextern "C" __declspec(dllexport) uint64_t probe(uint32_t pid) {\n'
        "    return winapi::get_running_process_start_time(pid).value_or(0);\n}\n",
        encoding="utf-8",
    )
    result = subprocess.run(
        [compiler, "-std=c++20", "-shared", "-Wall", "-Wextra", "-Werror", str(probe), "-o", str(library)],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    dll = ctypes.CDLL(str(library))
    dll.probe.argtypes = [ctypes.c_uint32]
    dll.probe.restype = ctypes.c_uint64
    return dll.probe


def test_current_process_has_stable_identity(process_start_time):
    start = process_start_time(os.getpid())
    assert start != 0
    assert process_start_time(os.getpid()) == start
    assert process_start_time(0) == 0


def test_another_live_process_has_identity(process_start_time):
    with subprocess.Popen(
        [sys.executable, "-c", "print('ready', flush=True); input()"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    ) as child:
        try:
            assert child.stdout.readline().strip() == "ready"
            start = process_start_time(child.pid)
            assert start != 0
            assert process_start_time(child.pid) == start
        finally:
            child.communicate("\n", timeout=30)


@pytest.mark.parametrize("exit_code", [0, 259])
def test_exited_process_is_not_running_with_retained_handle(process_start_time, exit_code):
    # Popen keeps the process handle alive after wait(), so OpenProcess can still find the exited process
    with subprocess.Popen([sys.executable, "-c", f"raise SystemExit({exit_code})"]) as child:
        assert child.wait(timeout=30) == exit_code
        assert process_start_time(child.pid) == 0
