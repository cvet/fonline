from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
INIT = (ENGINE / 'BuildTools/cmake/stages/Init.cmake').read_text(encoding='utf-8')
DECLARATIONS = INIT.split('# Options\n', 1)[1].split('# Quiet all non-error', 1)[0]
pytestmark = pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')


def configure(tmp_path: Path, *, wide: str, max_size: str) -> subprocess.CompletedProcess[str]:
    source = tmp_path / 'source'
    source.mkdir(exist_ok=True)
    (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.22)
project(MapSizeFixture LANGUAGES NONE)
include("{ENGINE.as_posix()}/BuildTools/Init.cmake")
set(FO_OUTPUT_PATH "${{CMAKE_CURRENT_BINARY_DIR}}/output")
{DECLARATIONS}
set(FO_FORCE_ENABLE_3D "")
include("{ENGINE.as_posix()}/BuildTools/cmake/stages/ProjectOptions.cmake")
StatusMessage("MapSizeFixture=${{FO_MAP_MAX_SIZE}}")
''', encoding='utf-8')
    environment = {name: value for name, value in os.environ.items() if name not in ('FO_MAP_WIDE_COORDS', 'FO_MAP_MAX_SIZE')}
    build_dir = tmp_path / f'build-{wide}-{max_size}'
    return subprocess.run(['cmake', '-S', str(source), '-B', str(build_dir), f'-DFO_MAP_WIDE_COORDS={wide}', f'-DFO_MAP_MAX_SIZE={max_size}'],
                          env=environment, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)


@pytest.mark.parametrize(('wide', 'max_size'), [('OFF', '200'), ('OFF', '4000'), ('OFF', '32767'), ('ON', '32768'), ('ON', '4194304')])
def test_map_max_size_within_the_coordinate_limit_configures(tmp_path: Path, wide: str, max_size: str) -> None:
    result = configure(tmp_path, wide=wide, max_size=max_size)
    output = result.stdout + result.stderr

    assert result.returncode == 0, output
    assert f'MapSizeFixture={max_size}' in output


@pytest.mark.parametrize(('wide', 'max_size', 'limit', 'reason'), [
    ('OFF', '32768', '32767', 'int16 map coordinate range'),
    ('ON', '4194305', '4194304', 'sprite draw-order key'),
    ('OFF', '199', '32767', 'int16 map coordinate range'),
    ('ON', 'big', '4194304', 'sprite draw-order key'),
])
def test_map_max_size_outside_the_coordinate_limit_is_rejected(tmp_path: Path, wide: str, max_size: str, limit: str, reason: str) -> None:
    result = configure(tmp_path, wide=wide, max_size=max_size)
    # CMake wraps long diagnostics, so compare the message with its whitespace collapsed
    output = ' '.join((result.stdout + result.stderr).split())

    assert result.returncode != 0, output
    assert f'FO_MAP_MAX_SIZE must be an integer from 200 to {limit}' in output
    assert reason in output
    assert f"got '{max_size}'" in output
