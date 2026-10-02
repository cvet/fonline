from __future__ import annotations

import os
from pathlib import Path
import shlex
import shutil
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


@pytest.mark.parametrize('cmake_limit,dotnet_limit,expected_nodes,expected_processors', [('2', None, 2, 2), ('2', '7', 2, 2), ('4', '2', 2, 2), (None, '3', 3, 3), ('1025', None, 1024, 1025), (None, '65535', 1024, 65535)])
def test_nested_runtime_honors_cpu_budget_in_actual_msbuild(tmp_path, monkeypatch, cmake_limit, dotnet_limit, expected_nodes, expected_processors):
    dotnet = shutil.which('dotnet')
    if dotnet is None:
        pytest.skip('dotnet SDK is required for actual MSBuild processor counts')
    runtime = tmp_path / 'runtime with spaces'
    runtime.mkdir()
    project = runtime / 'CpuBudget.proj'
    observed = runtime / 'processor-counts.txt'
    project.write_text(
        '<Project><Target Name="Build">'
        '<WriteLinesToFile File="processor-counts.txt" Overwrite="true" '
        'Lines="$(MSBuildNodeCount);$([System.Environment]::ProcessorCount)" />'
        '</Target></Project>\n', encoding='utf-8',
    )
    for name, value in [('CMAKE_BUILD_PARALLEL_LEVEL', cmake_limit), ('DOTNET_PROCESSOR_COUNT', dotnet_limit)]:
        if value is None:
            monkeypatch.delenv(name, raising=False)
        else:
            monkeypatch.setenv(name, value)
    monkeypatch.setenv('DOTNET_gcServer', '0')
    if os.name == 'nt':
        (runtime / 'build.cmd').write_text(
            f'@echo off\n"{dotnet}" msbuild "{project}" /m:6 /nologo /nr:false %*\n', encoding='utf-8',
        )
        monkeypatch.setattr(buildtools, 'resolve_visual_studio_2022_dev_cmd', lambda: None)
    else:
        script = runtime / 'build.sh'
        script.write_text(
            f'#!/bin/sh\nexec {shlex.quote(dotnet)} msbuild {shlex.quote(str(project))} /m:6 /nologo /nr:false "$@"\n',
            encoding='utf-8',
        )
        script.chmod(0o755)
    buildtools.run_runtime_build([], runtime, target_os='windows' if os.name == 'nt' else 'linux')
    assert [int(value) for value in observed.read_text().splitlines()] == [expected_nodes, expected_processors]
    assert os.environ.get('DOTNET_PROCESSOR_COUNT') == dotnet_limit


@pytest.mark.parametrize('cmake_limit,dotnet_limit,variable', [('0', None, 'CMAKE_BUILD_PARALLEL_LEVEL'), ('-1', None, 'CMAKE_BUILD_PARALLEL_LEVEL'), ('invalid', None, 'CMAKE_BUILD_PARALLEL_LEVEL'), ('65536', None, 'CMAKE_BUILD_PARALLEL_LEVEL'), ('2', '0', 'DOTNET_PROCESSOR_COUNT'), (None, '0', 'DOTNET_PROCESSOR_COUNT'), (None, 'invalid', 'DOTNET_PROCESSOR_COUNT')])
def test_invalid_runtime_cpu_budget_is_rejected_before_build(tmp_path, monkeypatch, cmake_limit, dotnet_limit, variable):
    if cmake_limit is None:
        monkeypatch.delenv('CMAKE_BUILD_PARALLEL_LEVEL', raising=False)
    else:
        monkeypatch.setenv('CMAKE_BUILD_PARALLEL_LEVEL', cmake_limit)
    if dotnet_limit is None:
        monkeypatch.delenv('DOTNET_PROCESSOR_COUNT', raising=False)
    else:
        monkeypatch.setenv('DOTNET_PROCESSOR_COUNT', dotnet_limit)
    with pytest.raises(SystemExit, match=variable + ' must be a decimal integer between 1 and 65535'):
        buildtools.run_runtime_build([], tmp_path, target_os='linux')
