from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
CMAKE = shutil.which('cmake')
pytestmark = pytest.mark.skipif(CMAKE is None, reason='CMake is required')


def test_catch2_summary_with_skipped_tests_is_accepted(tmp_path: Path) -> None:
    if os.name == 'nt':
        executable = tmp_path / 'success.bat'
        executable.write_text(
            '@echo off\r\n'
            'echo test cases:    770 ^|    769 passed ^| 1 skipped\r\n'
            'echo assertions: 745228 ^| 745228 passed\r\n',
            encoding='utf-8',
        )
    else:
        executable = tmp_path / 'success.sh'
        executable.write_text(
            '#!/bin/sh\n'
            "printf '%s\\n' 'test cases:    770 |    769 passed | 1 skipped'\n"
            "printf '%s\\n' 'assertions: 745228 | 745228 passed'\n",
            encoding='utf-8',
        )
        executable.chmod(0o755)

    result = subprocess.run(
        [
            CMAKE,
            f'-DTEST_EXECUTABLE={executable.as_posix()}',
            f'-DTEST_WORKING_DIRECTORY={tmp_path.as_posix()}',
            f'-DTEST_LOG={(tmp_path / "test.log").as_posix()}',
            '-P',
            (ENGINE / 'BuildTools/cmake/RunTestExecutable.cmake').as_posix(),
        ],
        capture_output=True,
        text=True,
        encoding='utf-8',
        errors='replace',
        timeout=30,
    )

    output = result.stdout + result.stderr
    assert result.returncode == 0, output
    assert 'test cases:    770 |    769 passed | 1 skipped' in output
