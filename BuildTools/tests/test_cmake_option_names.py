from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
INIT = (ENGINE / 'BuildTools/cmake/stages/Init.cmake').read_text(encoding='utf-8')
# Retired-input refusals, every declaration and the name validation that follows them
INIT_OPTIONS = INIT.split('# Quiet all non-error', 1)[0]
REQUIRED = '''SetOption(FO_MAIN_CONFIG "Fixture.fomain")
SetOption(FO_DEV_NAME "FIXTURE")
SetOption(FO_NICE_NAME "Fixture")
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)
SetOption(FO_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/output")
'''
pytestmark = pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')


def configure(tmp_path: Path, project_lines: str, *, after: str = '', args: tuple[str, ...] = (),
              env: dict[str, str] | None = None) -> tuple[int, str]:
    source = tmp_path / 'source'
    source.mkdir(exist_ok=True)
    (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.22)
project(OptionNameFixture LANGUAGES NONE)
include("{ENGINE.as_posix()}/BuildTools/Init.cmake")
{REQUIRED}{project_lines}
{INIT_OPTIONS}
{after}
StatusMessage("OptionNameFixture=${{FO_ENABLE_3D}}|${{FO_DATABASE_SQLITE}}|${{MYGAME_FEATURE}}|${{MYGAME_OTHER}}")
''', encoding='utf-8')
    environment = {name: value for name, value in os.environ.items() if not name.startswith('FO_')}
    environment.update(env or {})
    result = subprocess.run(['cmake', '-S', str(source), '-B', str(tmp_path / 'build'), *args],
                            env=environment, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
    # CMake wraps long diagnostics, so compare messages with their whitespace collapsed
    return result.returncode, ' '.join((result.stdout + result.stderr).split())


def test_declared_and_project_prefixed_names_configure(tmp_path: Path) -> None:
    code, output = configure(tmp_path, '''SetOption(FO_ENABLE_3D ON)
SetOptionValues(FO_DATABASE_SQLITE ON FO_IMAGE_FRM ON)
SetOption(MYGAME_FEATURE ON)
SetOptionValues(MYGAME_OTHER OFF)''')

    assert code == 0, output
    assert 'OptionNameFixture=ON|ON|ON|OFF' in output


def test_environment_override_and_undeclared_environment_names_keep_working(tmp_path: Path) -> None:
    code, output = configure(tmp_path, 'SetOption(FO_ENABLE_3D OFF)',
                             env={'FO_ENABLE_3D': 'YES', 'FO_NOT_AN_OPTION': '1'})

    assert code == 0, output
    assert 'FO_ENABLE_3D = ON (env)' in output
    assert 'OptionNameFixture=ON|' in output


@pytest.mark.parametrize('command', ('SetOption(FO_ENABLE_3DD ON)', 'SetOptionValues(FO_ENABLE_3D ON FO_ENABLE_3DD ON)'))
def test_undeclared_fo_name_fails_naming_the_option(tmp_path: Path, command: str) -> None:
    code, output = configure(tmp_path, command)
    caller = command.split('(', 1)[0]

    assert code != 0, output
    assert (f'{caller}: FO_ENABLE_3DD is not a declared option. Check the name against the CMake option reference; '
            'project variables need a prefix other than FO_') in output
    assert 'OptionNameFixture=' not in output


def test_name_set_after_the_declarations_is_checked_immediately(tmp_path: Path) -> None:
    code, output = configure(tmp_path, '', after='SetOption(FO_LATE_TYPO ON)')

    assert code != 0, output
    assert 'SetOption: FO_LATE_TYPO is not a declared option.' in output


@pytest.mark.parametrize(('name', 'guidance'), [
    ('FO_MONO_SCRIPTING', 'Use FO_MANAGED_SCRIPTING for the Managed C# backend'),
    ('FO_BUILD_EDITOR', 'Use FO_BUILD_MAPPER for the Mapper, AnimationViewer and ParticleViewer; no generic Editor target exists'),
    ('FO_INTERNAL_CONFIG_CAPACITY', 'The Engine owns the internal config capacity; delete the setting'),
    ('FO_DISABLE_UNQLITE', 'Enable the SQLite database provider with FO_DATABASE_SQLITE when the project needs it'),
])
@pytest.mark.parametrize('caller', ('SetOption', 'SetOptionValues'))
def test_retired_name_fails_with_its_replacement(tmp_path: Path, name: str, guidance: str, caller: str) -> None:
    code, output = configure(tmp_path, f'{caller}({name} ON)')

    assert code != 0, output
    assert f'{caller}: {name} was removed. {guidance}' in output
    assert 'Start project generation' not in output


@pytest.mark.parametrize(('name', 'project_lines', 'args', 'env'), [
    ('FO_DISABLE_MONGO', 'SetOption(FO_DISABLE_MONGO ON)', (), None),
    ('FO_DISABLE_SQLITE', 'SetOptionValues(FO_DISABLE_SQLITE ON)', (), None),
    ('FO_DISABLE_SQLITE', '', ('-DFO_DISABLE_SQLITE=ON',), None),
    ('FO_DISABLE_MONGO', '', (), {'FO_DISABLE_MONGO': 'ON'}),
])
def test_retired_database_options_keep_their_refusal(
    tmp_path: Path, name: str, project_lines: str, args: tuple[str, ...], env: dict[str, str] | None,
) -> None:
    code, output = configure(tmp_path, project_lines, args=args, env=env)

    assert code != 0, output
    assert (f'{name} was removed. Select positive FO_DATABASE_MONGO/FO_DATABASE_SQLITE options with SetOptionValues; '
            'clear the retired cache entry') in output
    assert 'is not a declared option' not in output


def test_messages_keep_semicolons_and_concatenate_arguments(tmp_path: Path) -> None:
    code, output = configure(tmp_path, '', after='''StatusMessage("status a; b" " c")
AbortMessage("abort a; b" " c")''')

    assert code != 0, output
    assert '-- status a; b c' in output
    assert 'abort a; b c' in output
