"""Regression tests for native_source_audit: scopes, configuration, baselines, exit codes and the allocator audit.

Fixtures live under tmp_path.  The Engine HEAD tests run the clang-free audits over the real Engine ``Source``
directory with the Engine-owned baselines, so a new finding or a stale baseline row fails here.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS.parent
sys.path.insert(0, str(BUILDTOOLS))
from native_source_audit import allocator, cli  # noqa: E402
from native_source_audit.common import (BASELINE_HEADER, AuditContext, AuditUsageError, Finding,  # noqa: E402
                                        apply_baseline, build_context, load_config, read_baseline)


ENTRY = BUILDTOOLS / 'native_source_audit.py'
BASELINE_DIR = BUILDTOOLS / 'audit_baselines'


def write(root: Path, relative: str, text: str) -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8')
    return path


def project(tmp_path: Path) -> Path:
    """An embedding-project layout with a synthetic Engine at Engine/ and a project extension directory."""

    root = tmp_path.resolve()
    write(root, 'Engine/Source/Common/Clean.cpp', 'vector<int> values {};\n')
    write(root, 'Engine/Source/Essentials/Containers.h', 'template<typename T> using vector = std::vector<T, safe_allocator<T>>;\n')
    write(root, 'Engine/BuildTools/audit_baselines/allocator.tsv', BASELINE_HEADER + '\n')
    write(root, 'SourceExt/Bridge.cpp', '// std::vector in a comment\nauto text = "std::string";\nstd::string name {};\n')
    return root


def run_cli(root: Path, *args: str) -> int:
    return cli.run([*args[:1], '--root', str(root), '--engine-root', str(root / 'Engine'), *args[1:]])


# Allocator rules


@pytest.mark.parametrize(('code', 'expected'), [
    ('auto a = 1; // std::vector<int>', []),
    ('/* std::string */ int x;', []),
    ('const char* s = "std::vector";', []),
    ('std::vector<int> values;', ['StdContainerType']),
    ('std::string_view sv;', []),
    ('std::wstring_view sv;', []),
    ('std::format_to(out, "{}", v);', []),
    ('std::vformat_to(out, f, a);', []),
    ('auto s = std::format("{}", v);', ['StdAllocatingCall']),
    ('auto s = std::to_string(v);', ['StdAllocatingCall']),
    ('void* p = malloc(10);', ['RawCAllocation']),
    ('free(p);', ['RawCAllocation']),
    ('MemFree(p);', []),
    ('safe_alloc::free_raw(p);', []),
    ('rpfree(p);', []),
    ('allocator.free(p);', []),
    ('_aligned_free(p);', ['RawCAllocation']),
])
def test_allocator_rules(code: str, expected: list[str]) -> None:
    assert [finding.rule for finding in allocator.scan_text('A.cpp', code + '\n')] == expected


def test_multiline_block_comment_keeps_line_numbers() -> None:
    findings = allocator.scan_text('A.cpp', '/* std::vector\n   std::map */\nstd::set<int> s;\n')

    assert [(finding.line, finding.rule) for finding in findings] == [(3, 'StdContainerType')]


def test_allocator_owner_files_are_skipped_only_inside_the_engine(tmp_path: Path) -> None:
    root = project(tmp_path)
    write(root, 'SourceExt/Essentials/Containers.h', 'std::vector<int> v;\n')
    context = AuditContext(root=root, engine_root=root / 'Engine', scopes=['Engine/Source', 'SourceExt'])

    findings = allocator.collect(context, context.iter_files())

    assert sorted((finding.path, finding.line) for finding in findings) == [('SourceExt/Bridge.cpp', 3), ('SourceExt/Essentials/Containers.h', 1)]


# Baselines


def baseline_context(root: Path, scopes: list[str]) -> AuditContext:
    return AuditContext(root=root, engine_root=root / 'Engine', scopes=scopes)


def finding(path: str, rule: str = 'StdContainerType', code: str = 'std::string name {};', line: int = 3) -> Finding:
    return Finding(path, line, 'error', rule, 'message', code)


def test_baseline_matches_code_independently_of_line_and_whitespace(tmp_path: Path) -> None:
    root = project(tmp_path)
    baseline = write(root, 'audit/allocator.tsv', BASELINE_HEADER + '\nSourceExt/Bridge.cpp\tStdContainerType\tstd::string   name {};\tForeign signature.\n')
    context = baseline_context(root, ['SourceExt'])
    entries = read_baseline(baseline, root, context)

    result = apply_baseline([finding('SourceExt/Bridge.cpp', line=99)], entries, context)

    assert result.findings == [] and result.baselined == 1 and result.stale == []


def test_baseline_wildcard_and_directory_rows_are_rule_scoped(tmp_path: Path) -> None:
    root = project(tmp_path)
    baseline = write(root, 'audit/allocator.tsv', BASELINE_HEADER + '\nSourceExt/\tStdContainerType\t*\tJSON boundary.\n')
    context = baseline_context(root, ['SourceExt'])
    entries = read_baseline(baseline, root, context)

    result = apply_baseline([finding('SourceExt/Deep/A.cpp'), finding('SourceExt/A.cpp', rule='RawCAllocation', code='free(p);')], entries, context)

    assert [item.rule for item in result.findings] == ['RawCAllocation']


def test_stale_row_inside_scope_is_reported_and_outside_scope_is_not(tmp_path: Path) -> None:
    root = project(tmp_path)
    baseline = write(root, 'audit/allocator.tsv', BASELINE_HEADER + '\nSourceExt/Gone.cpp\tStdContainerType\t*\tRemoved.\nOther/A.cpp\tStdContainerType\t*\tOut of scope.\n')
    context = baseline_context(root, ['SourceExt'])

    result = apply_baseline([], read_baseline(baseline, root, context), context)

    assert [entry.path for entry in result.stale] == ['SourceExt/Gone.cpp']


@pytest.mark.parametrize('text', [
    'path\trule\tcode\n',
    BASELINE_HEADER + '\nA.cpp\tRule\tcode\n',
    BASELINE_HEADER + '\nA.cpp\tRule\tcode\t \n',
    BASELINE_HEADER + '\n/abs/A.cpp\tRule\tcode\treason\n',
    BASELINE_HEADER + '\n../A.cpp\tRule\tcode\treason\n',
    BASELINE_HEADER + '\nA.cpp\tRule\tcode\treason\nA.cpp\tRule\tcode\treason again\n',
])
def test_malformed_baselines_are_rejected(tmp_path: Path, text: str) -> None:
    root = project(tmp_path)
    baseline = write(root, 'audit/allocator.tsv', text)

    with pytest.raises(AuditUsageError):
        read_baseline(baseline, root, baseline_context(root, ['SourceExt']))


def test_engine_baseline_rows_are_translated_into_the_project_root(tmp_path: Path) -> None:
    root = project(tmp_path)
    write(root, 'Engine/Source/Tools/Report.cpp', 'std::string text {};\n')
    write(root, 'Engine/BuildTools/audit_baselines/allocator.tsv', BASELINE_HEADER + '\nSource/Tools/Report.cpp\tStdContainerType\tstd::string text {};\tLibrary takes std::string.\n')

    assert run_cli(root, 'allocator', '--scope', 'Engine/Source') == 0
    assert run_cli(root, 'allocator', '--scope', 'Engine/Source', '--no-engine-baseline') == 1


def test_project_baseline_sits_next_to_the_engine_baseline(tmp_path: Path) -> None:
    root = project(tmp_path)
    write(root, 'Tools/NativeAudits/allocator.tsv', BASELINE_HEADER + '\nSourceExt/Bridge.cpp\tStdContainerType\t*\tnlohmann::json boundary.\n')

    assert run_cli(root, 'allocator', '--scope', 'Engine/Source', '--scope', 'SourceExt') == 1
    assert run_cli(root, 'allocator', '--scope', 'Engine/Source', '--scope', 'SourceExt', '--baseline', 'Tools/NativeAudits/allocator.tsv') == 0


def test_stale_project_baseline_row_fails(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    root = project(tmp_path)
    write(root, 'Tools/NativeAudits/allocator.tsv', BASELINE_HEADER + '\nSourceExt/Bridge.cpp\tStdContainerType\t*\tJSON.\nSourceExt/Bridge.cpp\tRawCAllocation\t*\tNo longer used.\n')

    assert run_cli(root, 'allocator', '--scope', 'SourceExt', '--baseline', 'Tools/NativeAudits/allocator.tsv') == 1
    assert 'StaleBaselineEntry' in capsys.readouterr().out


def test_missing_baseline_file_is_a_usage_error(tmp_path: Path) -> None:
    root = project(tmp_path)

    assert run_cli(root, 'allocator', '--scope', 'SourceExt', '--baseline', 'missing.tsv') == 2


# Configuration and CLI


def test_configuration_supplies_scopes_excludes_and_baselines(tmp_path: Path) -> None:
    root = project(tmp_path)
    write(root, 'SourceExt/vendor/lib.cpp', 'void* p = malloc(1);\n')
    write(root, 'Tools/NativeAudits/allocator.tsv', BASELINE_HEADER + '\nSourceExt/Bridge.cpp\tStdContainerType\t*\tJSON.\n')
    write(root, 'Tools/NativeAudits/config.json', json.dumps({
        'schema_version': 1,
        'scopes': ['Engine/Source', 'SourceExt'],
        'exclude': ['SourceExt/vendor/'],
        'baselines': {'allocator': ['Tools/NativeAudits/allocator.tsv']},
    }))

    assert run_cli(root, 'allocator', '--config', 'Tools/NativeAudits/config.json') == 0
    assert run_cli(root, 'allocator', '--config', str(root / 'Tools/NativeAudits/config.json')) == 0
    assert run_cli(root, 'allocator', '--config', str(root / 'Tools/NativeAudits/config.json'), '--scope', 'SourceExt/vendor') == 0


def test_exclude_glob_crosses_directories(tmp_path: Path) -> None:
    root = project(tmp_path)
    write(root, 'SourceExt/third_party/x/lib.cpp', 'void* p = malloc(1);\n')
    context = build_context('allocator', root=root, scopes=['SourceExt'], excludes=['*/third_party/*', 'SourceExt/Bridge.cpp'],
                            baselines=[], config_path=None, use_engine_baseline=False, engine_root=root / 'Engine')

    assert context.iter_files() == []


@pytest.mark.parametrize('payload', [
    {'scopes': ['Source']},
    {'schema_version': 2},
    {'schema_version': 1, 'unknown': True},
    {'schema_version': 1, 'scopes': 'Source'},
    {'schema_version': 1, 'baselines': {'allocator': 'a.tsv'}},
    [],
])
def test_invalid_configuration_is_rejected(tmp_path: Path, payload: object) -> None:
    path = write(tmp_path.resolve(), 'config.json', json.dumps(payload))

    with pytest.raises(AuditUsageError):
        load_config(path)


def test_scope_outside_root_and_missing_scope_are_usage_errors(tmp_path: Path) -> None:
    root = project(tmp_path)

    assert run_cli(root, 'allocator', '--scope', '../elsewhere') == 2
    assert run_cli(root, 'allocator', '--scope', 'Missing') == 2


def test_default_scope_is_the_engine_source_directory(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    root = project(tmp_path)

    assert run_cli(root, 'allocator') == 0
    assert 'audited 2 files' in capsys.readouterr().out


def test_report_and_emitted_baseline(tmp_path: Path) -> None:
    root = project(tmp_path)

    assert run_cli(root, 'allocator', '--scope', 'SourceExt', '--report', 'out/report.tsv', '--emit-baseline', 'out/candidate.tsv') == 1

    report = (root / 'out/report.tsv').read_text(encoding='utf-8').splitlines()
    assert report[0] == 'path\tline\tseverity\trule\tcode\tmessage'
    assert report[1].startswith('SourceExt/Bridge.cpp\t3\terror\tStdContainerType\tstd::string name {};\t')
    candidate = (root / 'out/candidate.tsv').read_text(encoding='utf-8')
    assert candidate == BASELINE_HEADER + '\nSourceExt/Bridge.cpp\tStdContainerType\tstd::string name {};\t\n'
    with pytest.raises(AuditUsageError):
        read_baseline(root / 'out/candidate.tsv', root, baseline_context(root, ['SourceExt']))


def test_fail_on_none_reports_without_failing(tmp_path: Path) -> None:
    root = project(tmp_path)

    assert run_cli(root, 'allocator', '--scope', 'SourceExt', '--fail-on', 'none') == 0


def test_entry_script_runs_as_a_program(tmp_path: Path) -> None:
    root = project(tmp_path)
    result = subprocess.run([sys.executable, str(ENTRY), 'allocator', '--root', str(root), '--engine-root', str(root / 'Engine'), '--scope', 'SourceExt'],
                            capture_output=True, text=True, encoding='utf-8', check=False)

    assert result.returncode == 1
    assert 'SourceExt/Bridge.cpp:3: error: StdContainerType' in result.stdout


def test_parser_program_and_subcommands() -> None:
    parser = cli.make_parser()
    subcommands = next(action for action in parser._actions if action.dest == 'audit')

    assert parser.prog == 'native_source_audit.py'
    assert list(subcommands.choices) == ['allocator', 'smart-pointer', 'smart-pointer-ast', 'local-variables', 'explicit-local-types']


# Engine-owned baselines and Engine HEAD


@pytest.mark.parametrize('audit', ['allocator', 'smart-pointer'])
def test_engine_baseline_file_format(audit: str) -> None:
    lines = (BASELINE_DIR / f'{audit}.tsv').read_text(encoding='utf-8').splitlines()

    assert lines[0] == BASELINE_HEADER
    for line in lines[1:]:
        path, rule, code, reason = line.split('\t')
        assert (ENGINE_ROOT / path.rstrip('/')).exists(), path
        assert rule and code and len(reason) >= 20, line


@pytest.mark.parametrize('audit', ['allocator', 'smart-pointer'])
def test_engine_source_passes_with_the_engine_baseline(audit: str, capsys: pytest.CaptureFixture[str]) -> None:
    exit_code = cli.run([audit, '--root', str(ENGINE_ROOT), '--max-printed', '0'])
    output = capsys.readouterr().out

    assert exit_code == 0, output
    assert 'stale baseline entries 0' in output


def test_engine_allocator_findings_need_the_baseline() -> None:
    assert cli.run(['allocator', '--root', str(ENGINE_ROOT), '--no-engine-baseline', '--max-printed', '1']) == 1
