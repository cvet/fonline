from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
INTERFACE = json.loads((ENGINE / 'BuildTools/cmake/ProjectInterface.json').read_text(encoding='utf-8'))
INIT = (ENGINE / 'BuildTools/cmake/stages/Init.cmake').read_text(encoding='utf-8')
DECLARATIONS = INIT.split('# Options\n', 1)[1].split('# Quiet all non-error', 1)[0]
pytestmark = pytest.mark.skipif(shutil.which('cmake') is None, reason='CMake is required')


def run_script(tmp_path: Path, body: str, *, args: tuple[str, ...] = (), env: dict[str, str] | None = None) -> list[str]:
    script = tmp_path / 'probe.cmake'
    script.write_text(f'''cmake_minimum_required(VERSION 3.22)
include("{ENGINE.as_posix()}/BuildTools/cmake/helpers/Commands.cmake")
include("{ENGINE.as_posix()}/BuildTools/cmake/helpers/Build.cmake")
include("{ENGINE.as_posix()}/BuildTools/cmake/helpers/Options.cmake")
{body}
''', encoding='utf-8')
    clean_env = {key: value for key, value in os.environ.items() if not key.startswith('FO_')}
    clean_env.update(env or {})
    result = subprocess.run(['cmake', *args, '-P', str(script)], text=True, encoding='utf-8',
                            capture_output=True, timeout=60, env=clean_env)
    assert result.returncode == 0, result.stdout + result.stderr
    return re.findall(r'Hash=([a-f0-9]{64})', result.stdout + result.stderr)


@pytest.mark.parametrize('option', INTERFACE['options'], ids=lambda entry: entry['name'])
def test_each_declared_option_has_its_documented_baking_effect(tmp_path: Path, option: dict[str, object]) -> None:
    name = option['name']
    if option['cache_type'] == 'BOOL':
        changed = 'ON' if option['default'] == 'OFF' else 'OFF'
    elif option['value_kind'] == 'enum':
        changed = option['allowed_values'][-1]
    elif option['value_kind'] == 'integer':
        changed = str(int(option['default'] or '1') + 8)
    else:
        changed = 'changed-value'
    hashes = run_script(tmp_path, DECLARATIONS + f'''
ComputeBakingConfigHash(before)
set({name} "{changed}")
ComputeBakingConfigHash(after)
StatusMessage("Hash=${{before}}")
StatusMessage("Hash=${{after}}")
''')
    assert len(hashes) == 2
    assert (hashes[0] != hashes[1]) == option['affects_baking']


@pytest.mark.parametrize('value', ('1', 'ON', 'TRUE', 'YES', 'on', 'true', '0', 'OFF', 'FALSE', 'NO', 'off', 'false'))
def test_boolean_cache_and_environment_spellings_have_the_same_hash(tmp_path: Path, value: str) -> None:
    body = '''DeclareBoolOption(FO_TEST_FEATURE "feature" OFF BAKING)
ComputeBakingConfigHash(hash)
StatusMessage("Hash=${hash}")
'''
    canonical_value = 'ON' if value.upper() in {'1', 'ON', 'TRUE', 'YES'} else 'OFF'
    canonical = run_script(tmp_path, body, args=(f'-DFO_TEST_FEATURE={canonical_value}',))
    assert len(canonical) == 1
    assert run_script(tmp_path, body, args=(f'-DFO_TEST_FEATURE={value}',)) == canonical
    assert run_script(tmp_path, body, env={'FO_TEST_FEATURE': value}) == canonical


def test_declaration_order_duplicates_and_inherited_scope_do_not_change_hash(tmp_path: Path) -> None:
    body = '''DeclareValueOption(FO_TEST_VALUE "value" "a;b\nc" BAKING)
DeclareBoolOption(FO_TEST_FEATURE "feature" ON BAKING)
ComputeBakingConfigHash(first)
set_property(GLOBAL PROPERTY _FO_DECLARED_OPTIONS "")
DeclareBoolOption(FO_TEST_FEATURE "feature" ON BAKING)
DeclareValueOption(FO_TEST_VALUE "value" "a;b\nc" BAKING)
DeclareBoolOption(FO_TEST_FEATURE "feature" ON BAKING)
function(nested)
    ComputeBakingConfigHash(second)
    StatusMessage("Hash=${second}")
endfunction()
StatusMessage("Hash=${first}")
nested()
'''
    hashes = run_script(tmp_path, body)
    assert len(hashes) == 2 and hashes[0] == hashes[1]


def test_final_override_and_new_project_option_are_included_automatically(tmp_path: Path) -> None:
    hashes = run_script(tmp_path, '''DeclareValueOptions(BAKING FO_TEST_LIMIT "limit" 4)
ComputeBakingConfigHash(first)
set(FO_TEST_LIMIT 8)
ComputeBakingConfigHash(second)
DeclareBoolOption(GAME_CUSTOM_IMPORTER "game importer" ON BAKING)
ComputeBakingConfigHash(third)
StatusMessage("Hash=${first}")
StatusMessage("Hash=${second}")
StatusMessage("Hash=${third}")
''')
    assert len(hashes) == 3 and len(set(hashes)) == 3


def test_environment_override_wins_over_cache_and_project_default(tmp_path: Path) -> None:
    body = '''SetOptionValues(FO_TEST_FEATURE OFF)
DeclareBoolOption(FO_TEST_FEATURE "feature" OFF BAKING)
ComputeBakingConfigHash(hash)
StatusMessage("Hash=${hash}")
'''
    expected = run_script(tmp_path, body, args=('-DFO_TEST_FEATURE=ON',))
    assert run_script(tmp_path, body, args=('-DFO_TEST_FEATURE=OFF',), env={'FO_TEST_FEATURE': 'YES'}) == expected


def test_string_boundaries_are_unambiguous(tmp_path: Path) -> None:
    body = '''DeclareValueOption(FO_TEST_A "a" "a;b" BAKING)
DeclareValueOption(FO_TEST_B "b" "c" BAKING)
ComputeBakingConfigHash(first)
set(FO_TEST_A "a")
set(FO_TEST_B "b;c")
ComputeBakingConfigHash(second)
StatusMessage("Hash=${first}")
StatusMessage("Hash=${second}")
'''
    hashes = run_script(tmp_path, body)
    assert len(hashes) == 2 and hashes[0] != hashes[1]
