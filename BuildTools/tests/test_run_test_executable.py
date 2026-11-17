from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import time

import pytest


ENGINE = Path(__file__).resolve().parents[2]
CMAKE = shutil.which('cmake')
pytestmark = pytest.mark.skipif(CMAKE is None, reason='CMake is required')

ISOLATED_CASES = (
    'DataBaseInactivePanicCallbackCanRestore',
    'DataBaseInactivePanicCallbackCanAwaitFailedRead',
    'DataBaseInactivePanicDecisionSurvivesConcurrentStart',
    'DataBaseDirectRestoreDuringConcurrentOplogAppend',
    'DataBaseRestoreInsideAppliedBackendWritePreservesBothChanges',
    'DataBaseIndependentRecoveriesPreserveRecordAndJournal',
    'DataBaseRecoveryOwnerDoesNotBlockCommitHookCycle',
    'DataBaseRecoveryCommitHookWaitsForIndependentRecovery',
    'DataBaseRestoreKeepsAppendDuringLogCleanup',
    'DataBaseOldCleanupPreservesNewRecoveryProgress',
)


def test_isolated_cases_cover_hidden_database_tests() -> None:
    source = (ENGINE / 'Source/Tests/Test_DataBase.cpp').read_text(encoding='utf-8')
    hidden = re.findall(r'TEST_CASE\("([^"]+)",\s*"\[\.\]\[database\][^"]*"\)', source)
    assert len(hidden) == len(set(hidden))
    assert set(ISOLATED_CASES) == set(hidden)


def make_test_executable(root: Path, *, case: str = '', behavior: str = 'pass',
                         main_behavior: str = 'pass') -> Path:
    config = {'case': case, 'behavior': behavior, 'main_behavior': main_behavior}
    (root / 'config.json').write_text(json.dumps(config), encoding='utf-8')
    program = root / 'test_program.py'
    program.write_text(
        'import json, os, pathlib, sys, time\n'
        'root = pathlib.Path(__file__).resolve().parent\n'
        'config = json.loads((root / "config.json").read_text(encoding="utf-8"))\n'
        'case = sys.argv[1] if len(sys.argv) > 1 else ""\n'
        'with (root / "calls.jsonl").open("a", encoding="utf-8") as calls:\n'
        '    calls.write(json.dumps({"case": case, "pid": os.getpid()}) + "\\n")\n'
        'behavior = config["behavior"] if case and case == config["case"] else "pass"\n'
        'if not case:\n'
        '    if config["main_behavior"] == "fail":\n'
        '        print("main suite failed")\n'
        '        sys.exit(8)\n'
        '    print("test cases:    770 |    769 passed | 1 skipped")\n'
        '    print("assertions: 745228 | 745228 passed")\n'
        '    sys.exit(0)\n'
        'if behavior == "timeout":\n'
        '    print("isolated timeout marker", flush=True)\n'
        '    time.sleep(20)\n'
        'if behavior == "error":\n'
        '    print("isolated failure marker")\n'
        '    sys.exit(23)\n'
        'print(f\'Filters: "{case if behavior != "wrong_filter" else "another-case"}"\')\n'
        'if behavior == "missing":\n'
        '    print("missing summary marker")\n'
        'elif behavior == "no_tests":\n'
        '    print("No tests ran")\n'
        'elif behavior == "skipped":\n'
        '    print("test cases: 1 | 1 skipped")\n'
        'elif behavior == "multiple":\n'
        '    print("All tests passed (3 assertions in 2 test cases)")\n'
        'elif behavior == "zero_assertions":\n'
        '    print("All tests passed (0 assertions in 1 test case)")\n'
        'else:\n'
        '    print("All tests passed (3 assertions in 1 test case)")\n',
        encoding='utf-8',
    )

    if os.name == 'nt':
        executable = root / 'test executable.bat'
        executable.write_text(
            '@echo off\r\n'
            f'"{sys.executable}" "{program}" %*\r\n'
            'exit /b %ERRORLEVEL%\r\n', encoding='utf-8',
        )
    else:
        executable = root / 'test executable.sh'
        executable.write_text(
            f'#!/bin/sh\nexec {shlex.quote(sys.executable)} {shlex.quote(str(program))} "$@"\n',
            encoding='utf-8',
        )
        executable.chmod(0o755)

    return executable


def run_runner(root: Path, executable: Path, *, timeout: int | None = None) -> subprocess.CompletedProcess[str]:
    arguments = [
        CMAKE,
        f'-DTEST_EXECUTABLE={executable.as_posix()}',
        f'-DTEST_WORKING_DIRECTORY={root.as_posix()}',
        f'-DTEST_LOG={(root / "test.log").as_posix()}',
    ]

    if timeout is not None:
        arguments.append(f'-DTEST_ISOLATED_TIMEOUT_SECONDS={timeout}')

    arguments.extend(['-P', (ENGINE / 'BuildTools/cmake/RunTestExecutable.cmake').as_posix()])
    return subprocess.run(arguments, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)


def calls(root: Path) -> list[dict[str, object]]:
    return [json.loads(line) for line in (root / 'calls.jsonl').read_text(encoding='utf-8').splitlines()]


def process_is_running(pid: int) -> bool:
    if os.name == 'nt':
        import ctypes
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel32.OpenProcess.restype = wintypes.HANDLE
        kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel32.GetExitCodeProcess.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel32.CloseHandle.restype = wintypes.BOOL
        handle = kernel32.OpenProcess(0x1000, False, pid)

        if not handle:
            return ctypes.get_last_error() != 87

        try:
            code = wintypes.DWORD()
            if not kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
                raise ctypes.WinError(ctypes.get_last_error())
            return code.value == 259
        finally:
            kernel32.CloseHandle(handle)

    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def test_runner_accepts_skipped_main_suite_and_runs_each_isolated_case_in_a_separate_process(tmp_path: Path) -> None:
    executable = make_test_executable(tmp_path)
    result = run_runner(tmp_path, executable)
    output = result.stdout + result.stderr

    assert result.returncode == 0, output
    assert 'test cases:    770 |    769 passed | 1 skipped' in output
    executed = calls(tmp_path)
    assert [call['case'] for call in executed] == ['', *ISOLATED_CASES]
    assert all(int(call['pid']) > 0 for call in executed)

    for case in ISOLATED_CASES:
        assert case in output
        assert any(case in path.read_text(encoding='utf-8') for path in tmp_path.glob('test.log*'))


@pytest.mark.parametrize('case', ISOLATED_CASES)
def test_runner_fails_when_an_isolated_case_fails(tmp_path: Path, case: str) -> None:
    executable = make_test_executable(tmp_path, case=case, behavior='error')
    result = run_runner(tmp_path, executable)
    output = result.stdout + result.stderr

    assert result.returncode != 0
    assert case in output
    assert 'isolated failure marker' in output


@pytest.mark.parametrize('behavior', ['missing', 'no_tests', 'skipped', 'multiple', 'zero_assertions', 'wrong_filter'])
def test_runner_rejects_missing_skipped_or_non_unique_isolated_results(tmp_path: Path, behavior: str) -> None:
    case = ISOLATED_CASES[0]
    executable = make_test_executable(tmp_path, case=case, behavior=behavior)
    result = run_runner(tmp_path, executable)
    output = result.stdout + result.stderr

    assert result.returncode != 0
    assert case in output


def test_runner_times_out_an_isolated_child_and_retains_its_output(tmp_path: Path) -> None:
    case = ISOLATED_CASES[0]
    executable = make_test_executable(tmp_path, case=case, behavior='timeout')
    result = run_runner(tmp_path, executable, timeout=1)
    output = result.stdout + result.stderr

    assert result.returncode != 0
    assert case in output
    assert 'isolated timeout marker' in output
    assert 'timeout' in output.lower()
    child_pid = int(calls(tmp_path)[1]['pid'])
    deadline = time.monotonic() + 5

    while process_is_running(child_pid) and time.monotonic() < deadline:
        time.sleep(0.05)

    assert not process_is_running(child_pid), f'timed-out test child {child_pid} is still running'


@pytest.mark.parametrize('timeout', [0, 46, -1])
def test_runner_rejects_timeout_that_disables_or_extends_the_gate(tmp_path: Path, timeout: int) -> None:
    executable = make_test_executable(tmp_path)
    result = run_runner(tmp_path, executable, timeout=timeout)

    assert result.returncode != 0
    assert 'TEST_ISOLATED_TIMEOUT_SECONDS' in result.stdout + result.stderr


def test_runner_preserves_main_suite_failure(tmp_path: Path) -> None:
    executable = make_test_executable(tmp_path, main_behavior='fail')
    result = run_runner(tmp_path, executable)
    output = result.stdout + result.stderr

    assert result.returncode != 0
    assert 'main suite failed' in output
