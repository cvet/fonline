from pathlib import Path
import os
import shutil
import subprocess

import pytest


RUNNER = Path(__file__).resolve().parents[1] / 'cmake/RunTestExecutable.cmake'
SKIPPED_SUMMARY = (
    'test cases:     856 |     855 passed | 1 skipped\n'
    'assertions: 1739404 | 1739404 passed\n\n'
)


@pytest.mark.parametrize(('output', 'exit_code', 'accepted'), [
    ('All tests passed (1 assertion in 1 test case)\n', 0, True),
    (SKIPPED_SUMMARY, 0, True),
    ('test cases:    770 |    769 passed | 1 skipped\nassertions: 745228 | 745228 passed\n', 0, True),
    (SKIPPED_SUMMARY.replace('\n', '\r\n'), 0, True),
    ('test cases: 2 | 1 passed | 1 failed\nassertions: 2 | 1 passed | 1 failed\n', 0, False),
    ('test cases: 2 | 2 skipped\nassertions: - none -\n', 0, False),
    (SKIPPED_SUMMARY.replace('855 passed', '854 passed'), 0, False),
    (SKIPPED_SUMMARY.replace('1739404 passed', '1739403 passed'), 0, False),
    ('No tests ran\n', 0, False),
    ('', 0, False),
    ('All tests passed (1 assertion in 1 test case)\n', 42, False),
])
def test_runner_requires_successful_exit_and_complete_catch_summary(
    tmp_path: Path, output: str, exit_code: int, accepted: bool,
) -> None:
    cmake = shutil.which('cmake')
    assert cmake is not None
    output_file = tmp_path / 'stdout.txt'
    output_file.write_bytes(output.encode('utf-8'))

    if os.name == 'nt':
        executable = tmp_path / 'producer.cmd'
        executable.write_text(f'@echo off\ntype stdout.txt\nexit /b {exit_code}\n', encoding='utf-8')
    else:
        executable = tmp_path / 'producer.sh'
        executable.write_text(f'#!/bin/sh\ncat stdout.txt\nexit {exit_code}\n', encoding='utf-8')
        executable.chmod(0o700)

    log = tmp_path / 'runner.log'
    result = subprocess.run([
        cmake,
        '-DTEST_EXECUTABLE=' + executable.as_posix(),
        '-DTEST_WORKING_DIRECTORY=' + tmp_path.as_posix(),
        '-DTEST_LOG=' + log.as_posix(),
        '-P', str(RUNNER),
    ], capture_output=True, text=True, timeout=30)

    assert (result.returncode == 0) is accepted, result.stdout + result.stderr
    assert log.read_bytes() == output_file.read_bytes()
