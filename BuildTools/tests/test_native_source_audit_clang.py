"""Tests for the clang-based native source audits.

Parsing, mapping and skip behaviour run everywhere.  Integration tests compile small fixtures with the local
Clang 20+ tools and skip with an explicit reason when clang-query, clang-tidy or clang++ is unavailable.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
from native_source_audit import clang_support, cli, explicit_local_types, local_variables  # noqa: E402
from native_source_audit.common import AuditContext  # noqa: E402


LOCAL_VARIABLE_FIXTURE = r'''
#include <utility>

struct Value
{
    Value();
    Value(Value&&);
};

void Consume(Value);
void Read(const Value&);

void Validate()
{
    const int redundant = 1;

    int value = 1;
    const int* pointee_const = &value;
    int* const const_pointer = &value;
    const std::pair<const int*, int> nested_const = {&value, 1};
    const int const_array[] = {1, 2};
    constexpr int constexpr_value = 1;

    const int intentional_const = 1; // FO_REDUNDANT_CONST_SUPPRESS: const selects a deliberate API contract

    Value moved;
    Consume(std::move(moved));
    Read(moved);

    Value inspected_after_move;
    Consume(std::move(inspected_after_move));
    Read(inspected_after_move); // FO_USE_AFTER_MOVE_SUPPRESS: test verifies the moved-from object contract

    (void)redundant;
    (void)pointee_const;
    (void)const_pointer;
    (void)const_array;
    (void)constexpr_value;
    (void)intentional_const;
}
'''

EXPLICIT_TYPES_FIXTURE = r'''
using int16_t = short;
using int32_t = int;
using float32_t = float;
using float64_t = double;

namespace qualified
{
enum namespaced_kind
{
    namespaced_value,
};

struct simple_record
{
};

auto GetRecord() -> simple_record;
}

namespace fo
{
struct engine_type
{
};

auto GetEngineType() -> engine_type;

void EngineScope()
{
    auto engine_local = GetEngineType();
}
}

enum simple_kind
{
    simple_value,
};

struct CamelCase
{
};

struct pair_data
{
    int first;
    int second;
};

struct owner
{
    struct nested_type
    {
    };

    static auto GetNested() -> nested_type;
};

template<typename T>
struct container
{
};

template<int Size>
struct matrix
{
};

using mat44 = matrix<4>;
using other_mat = matrix<4>;

using count_t = int;

auto GetSimpleKind() -> simple_kind;
auto GetAliasCount() -> count_t;
auto Translate(const mat44&) -> matrix<4>;
auto SelectMatrix(const mat44&, const other_mat&) -> matrix<4>;
auto BuildMatrix() -> matrix<4>;

template<typename T>
void Instantiated(T value)
{
    auto inconsistent = value;
}

void ValidateCases()
{
    auto integer = 1;
    auto short_integer = static_cast<short>(1);
    auto floating = 1.0F;
    auto double_floating = 1.0;
    auto platform_long = static_cast<long>(1);
    auto kind = GetSimpleKind();
    auto camel = CamelCase {};
    auto templated = container<int> {};
    auto namespaced = qualified::namespaced_value;
    auto alias = GetAliasCount();
    auto alias_backed_matrix = Translate(mat44 {});
    auto ambiguous_matrix_alias = SelectMatrix(mat44 {}, other_mat {});
    auto unspelled_matrix_alias = BuildMatrix();
    auto namespace_record = qualified::GetRecord();
    auto nested = owner::GetNested();
    auto outside_fo = fo::GetEngineType();

    int values[2] {};
    auto decayed = values;
    auto* pointer = &integer;
    const int const_integer = 2;
    auto& const_reference = const_integer;
    const auto& authored_const_reference = integer;
    const auto top_level_const_pointer = &integer;
    auto [first, second] = pair_data {};

    Instantiated(1);
    Instantiated(1.0F);
}
'''


def find_tool(names: tuple[str, ...], environment: str) -> Path | None:
    """A Clang tool new enough for the audits, or None so the dependent tests skip."""

    try:
        tool = clang_support.discover_tool(None, environment, names)
        major, _description = clang_support.tool_version(tool)
    except (clang_support.ToolMissing, RuntimeError, OSError):
        return None

    return tool if major >= clang_support.MINIMUM_CLANG_MAJOR else None


def find_compiler(clang_query: Path) -> Path | None:
    for name in ('clang++-20', 'clang++'):
        sibling = clang_query.parent / (f'{name}.exe' if os.name == 'nt' else name)
        if sibling.is_file():
            return sibling
        found = shutil.which(name)
        if found:
            return Path(found)
    return None


@pytest.fixture(scope='module')
def clang_query() -> Path:
    tool = find_tool(('clang-query-20', 'clang-query'), 'FO_CLANG_QUERY')
    if tool is None:
        pytest.skip('clang-query 20+ is not installed; set FO_CLANG_QUERY to run the AST audit tests')
    return tool


@pytest.fixture(scope='module')
def clang_tidy() -> Path:
    tool = find_tool(('clang-tidy-20', 'clang-tidy'), 'FO_CLANG_TIDY')
    if tool is None:
        pytest.skip('clang-tidy 20+ is not installed; set FO_CLANG_TIDY to run the use-after-move tests')
    return tool


@pytest.fixture(scope='module')
def compiler(clang_query: Path) -> Path:
    found = find_compiler(clang_query)
    if found is None:
        pytest.skip('clang++ matching clang-query is not installed')
    return found


def make_project(tmp_path: Path, compiler: Path, sources: dict[str, str], scopes: list[str]) -> AuditContext:
    root = tmp_path.resolve()
    database = []
    for relative, text in sources.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        database.append({'directory': str(root), 'command': f'"{compiler}" -std=c++20 -c "{path}"', 'file': str(path)})
    (root / 'compile_commands.json').write_text(json.dumps(database), encoding='utf-8')
    return AuditContext(root=root, engine_root=root / 'Engine', scopes=scopes)


# Tool-independent behaviour


def test_missing_tool_raises_tool_missing(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv('FO_TEST_MISSING_TOOL', raising=False)

    with pytest.raises(clang_support.ToolMissing):
        clang_support.discover_tool(None, 'FO_TEST_MISSING_TOOL', ('fo-definitely-missing-clang-tool',))


@pytest.mark.parametrize('audit', ['local-variables', 'explicit-local-types', 'smart-pointer-ast'])
def test_missing_compilation_database_skips_or_fails_on_request(tmp_path: Path, audit: str, capsys: pytest.CaptureFixture[str]) -> None:
    root = tmp_path.resolve()
    (root / 'Native').mkdir()
    extra = ['--all'] if audit == 'smart-pointer-ast' else []

    assert cli.run([audit, '--root', str(root), '--scope', 'Native', *extra]) == 0
    assert 'skipped: no compilation database' in capsys.readouterr().out
    assert cli.run([audit, '--root', str(root), '--scope', 'Native', '--require-tooling', *extra]) == 2


def test_scope_source_pattern_matches_either_separator(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    (root / 'Engine/Source').mkdir(parents=True)
    (root / 'SourceExt').mkdir()
    (root / 'SourceExt/One.cpp').write_text('', encoding='utf-8')

    assert clang_support.scope_source_pattern(AuditContext(root=root, engine_root=root, scopes=['Engine/Source', 'SourceExt'])) == '(Engine.Source.|SourceExt.)'
    assert clang_support.scope_source_pattern(AuditContext(root=root, engine_root=root, scopes=['SourceExt/One.cpp'])) == 'SourceExt.One[.]cpp$'


def test_mapper_maps_scoped_paths_and_rejects_others(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    context = AuditContext(root=root, engine_root=root / 'Engine', scopes=['Engine/Source'], excludes=['Engine/Source/Vendor/'])
    mapper = clang_support.SourcePathMapper(context)

    assert mapper.map_path(root / 'Engine/Source/Common/A.cpp')[0] == 'Engine/Source/Common/A.cpp'
    assert mapper.map_path('/elsewhere/checkout/Engine/Source/Common/A.cpp')[0] == 'Engine/Source/Common/A.cpp'
    assert mapper.map_path(root / 'Engine/Source/Vendor/A.cpp') is None
    assert mapper.map_path(root / 'Other/A.cpp') is None


def test_multiline_ast_range_uses_declarator_location(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    source = root / 'fixture.cpp'
    source.write_text('const int value = 1;\n', encoding='utf-8')
    mapper = clang_support.SourcePathMapper(AuditContext(root=root, engine_root=root, scopes=['.']))

    location = clang_support._declarator_location_from_node_line(f"VarDecl 0x1 <{source}:1:1, line:3:2> col:11 value 'const int'", mapper)

    assert location is not None
    assert (location.path, location.line, location.column) == ('fixture.cpp', 1, 11)


def test_const_location_falls_back_when_ast_uses_declaration_start_for_name(tmp_path: Path) -> None:
    source = tmp_path.resolve() / 'source.cpp'
    source.write_text('    const bool value = true;\n', encoding='utf-8')
    start = clang_support.SourceLocation('source.cpp', 1, 5, str(source))
    node = clang_support.NodeBinding('top_level_const_local', 'VarDecl', '0x1', '', '', start, start, 'value', 'const bool')

    location = clang_support._find_top_level_const_location(node)

    assert location is not None and (location.line, location.column) == (1, 5)


def test_compilation_errors_are_detected_in_clang_output() -> None:
    assert clang_support._has_compilation_error('source.cpp:1:2: error: invalid declaration\n')
    assert clang_support._has_compilation_error('source.cpp(1,2): error: invalid declaration\n')
    assert not clang_support._has_compilation_error('source.cpp:1:2: warning: ignored\n')


def test_invalid_suppression_does_not_hide_diagnostic(tmp_path: Path) -> None:
    source = tmp_path.resolve() / 'source.cpp'
    source.write_text('const int value = 1; // FO_REDUNDANT_CONST_SUPPRESS: no\n', encoding='utf-8')
    diagnostic = clang_support.Diagnostic('fo-redundant-local-const', 'warning', 'test', clang_support.SourceLocation('source.cpp', 1, 11, str(source)), variable='value')

    diagnostics = clang_support.apply_suppressions([diagnostic])

    assert not diagnostic.suppressed
    assert any(item.code == 'fo-invalid-suppression' for item in diagnostics)


def test_analysis_database_keeps_one_preferred_command_per_unit(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    database = []
    for name in ('first', 'second'):
        source = root / f'{name}.cpp'
        source.write_text('void F() {}\n', encoding='utf-8')
        for target, config in (('Client', 'Debug'), ('Server', 'Release'), ('Mapper', 'RelWithDebInfo'), ('Baker', 'Debug')):
            database.append({'directory': str(root), 'command': f'clang++ -c "{source}" -o {config}/{target}/{name}.o', 'file': str(source)})
    (root / 'compile_commands.json').write_text(json.dumps(database), encoding='utf-8')
    mapper = clang_support.SourcePathMapper(AuditContext(root=root, engine_root=root, scopes=['.']))

    units = clang_support.load_compilation_units(root, mapper)
    clang_support.write_analysis_database(root, units, root / 'pruned')
    pruned = json.loads((root / 'pruned' / 'compile_commands.json').read_text(encoding='utf-8'))

    assert [unit.canonical_path for unit in units] == ['first.cpp', 'second.cpp']
    assert len(pruned) == 2 and all('RelWithDebInfo' in entry['command'] for entry in pruned)


def test_type_shape_accepts_only_one_simple_base_name() -> None:
    assert explicit_local_types.parse_type_shape('const simple_type *').base == 'simple_type'
    assert explicit_local_types.parse_type_shape('CamelCase') is None
    assert explicit_local_types.parse_type_shape('std::string') is None
    assert explicit_local_types.parse_type_shape('container<int>') is None
    assert explicit_local_types.parse_type_shape('unsigned int') is None


def test_apply_report_requires_apply_mode(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    (root / 'Native').mkdir()
    (root / 'report.json').write_text(json.dumps({'candidates': []}), encoding='utf-8')

    assert cli.run(['explicit-local-types', '--root', str(root), '--scope', 'Native', '--apply-report', 'report.json']) == 2
    assert cli.run(['explicit-local-types', '--root', str(root), '--scope', 'Native', '--mode', 'apply', '--apply-report', 'report.json']) == 0


# Integration with real Clang tools


def test_redundant_const_and_use_after_move(tmp_path: Path, clang_query: Path, clang_tidy: Path, compiler: Path) -> None:
    context = make_project(tmp_path, compiler, {'Native/fixture.cpp': LOCAL_VARIABLE_FIXTURE, 'Native/vendor/fixture.cpp': 'void VendorCase() { const int vendored = 1; }\n'}, ['Native'])
    context.excludes = ['Native/vendor/']

    result = local_variables.analyze(context, database_dir=context.root, clang_query=clang_query, clang_tidy=clang_tidy,
                                     source_pattern=clang_support.scope_source_pattern(context), timeout=120, batch_size=1, jobs=1, checks='all')

    assert [unit.canonical_path for unit in result.units] == ['Native/fixture.cpp']
    by_variable = {diagnostic.variable: diagnostic for diagnostic in result.diagnostics}
    assert by_variable['redundant'].code == 'fo-redundant-local-const'
    assert by_variable['const_pointer'].code == 'fo-redundant-local-const'
    assert by_variable['moved'].code == 'fo-use-after-move'
    assert by_variable['intentional_const'].suppressed
    assert by_variable['inspected_after_move'].suppressed
    for absent in ('pointee_const', 'const_array', 'constexpr_value', 'vendored'):
        assert absent not in by_variable


def test_local_variables_cli_fails_then_passes_with_a_project_baseline(tmp_path: Path, clang_query: Path, clang_tidy: Path, compiler: Path) -> None:
    context = make_project(tmp_path, compiler, {'Native/fixture.cpp': LOCAL_VARIABLE_FIXTURE}, ['Native'])
    root = context.root
    common = ['local-variables', '--root', str(root), '--scope', 'Native', '--compilation-database', '.', '--jobs', '1', '--batch-size', '1']

    assert cli.run(common) == 1
    assert cli.run([*common, '--mode', 'report']) == 0
    assert cli.run([*common, '--emit-baseline', 'candidate.tsv']) == 1
    rows = (root / 'candidate.tsv').read_text(encoding='utf-8').splitlines()
    (root / 'baseline.tsv').write_text('\n'.join([rows[0], *(row + 'Fixture keeps the finding on purpose.' for row in rows[1:])]) + '\n', encoding='utf-8')
    assert cli.run([*common, '--baseline', 'baseline.tsv']) == 0


def test_apply_removes_only_top_level_const(tmp_path: Path, clang_query: Path, compiler: Path) -> None:
    context = make_project(tmp_path, compiler, {'Native/fixture.cpp': LOCAL_VARIABLE_FIXTURE}, ['Native'])

    result = local_variables.analyze(context, database_dir=context.root, clang_query=clang_query, clang_tidy=None,
                                     source_pattern=clang_support.scope_source_pattern(context), timeout=120, batch_size=1, jobs=1,
                                     checks='redundant-local-const')
    applied = clang_support.apply_redundant_const_fixes(result.diagnostics)
    rewritten = (context.root / 'Native/fixture.cpp').read_text(encoding='utf-8')

    assert applied == 3
    for expected in ('int redundant = 1;', 'int* const_pointer = &value;', 'std::pair<const int*, int> nested_const = {&value, 1};',
                     'const int* pointee_const = &value;', 'const int const_array[] = {1, 2};', 'constexpr int constexpr_value = 1;',
                     'const int intentional_const = 1;'):
        assert expected in rewritten


def test_missing_header_rejects_partial_analysis(tmp_path: Path, clang_query: Path, compiler: Path) -> None:
    context = make_project(tmp_path, compiler, {'Native/fixture.cpp': '#include "missing_analysis_context.hpp"\nvoid inspect() { const int value = 1; }\n'}, ['Native'])

    with pytest.raises(RuntimeError, match='fatal error:.*file not found'):
        local_variables.analyze(context, database_dir=context.root, clang_query=clang_query, clang_tidy=None,
                                source_pattern=clang_support.scope_source_pattern(context), timeout=120, batch_size=1, jobs=1,
                                checks='redundant-local-const')


def test_only_exact_simple_types_are_rewritten(tmp_path: Path, clang_query: Path, compiler: Path) -> None:
    context = make_project(tmp_path, compiler, {'Engine/Source/fixture.cpp': EXPLICIT_TYPES_FIXTURE, 'Engine/Source/vendor/fixture.cpp': 'void VendorCase() { auto vendored = 1; }\n'}, ['Engine/Source'])

    result = explicit_local_types.run_inventory(context, database_dir=context.root, clang_query=clang_query,
                                                source_pattern=clang_support.scope_source_pattern(context), jobs=1, batch_size=1, timeout=120,
                                                unit_pattern=r'^Engine/Source/fixture\.cpp$', include_redundant_const=True)

    assert [unit.canonical_path for unit in result.units] == ['Engine/Source/fixture.cpp']
    replacements = {candidate.declarations: candidate.replacement for candidate in result.candidates}
    assert replacements[('integer',)] == 'int32_t'
    assert replacements[('short_integer',)] == 'int16_t'
    assert replacements[('floating',)] == 'float32_t'
    assert replacements[('double_floating',)] == 'float64_t'
    assert replacements[('kind',)] == 'simple_kind'
    assert replacements[('pointer',)] == 'int32_t'
    assert replacements[('const_reference',)] == 'const int32_t'
    assert replacements[('authored_const_reference',)] == 'int32_t'
    assert replacements[('alias',)] == 'count_t'
    assert replacements[('alias_backed_matrix',)] == 'mat44'
    assert replacements[('engine_local',)] == 'engine_type'
    for excluded in ('inconsistent', 'vendored', 'camel', 'ambiguous_matrix_alias', 'unspelled_matrix_alias', 'platform_long', 'templated',
                     'namespaced', 'decayed', 'namespace_record', 'nested', 'outside_fo', 'top_level_const_pointer', 'first', 'second'):
        assert (excluded,) not in replacements
    redundant = {diagnostic.variable for diagnostic in result.redundant_const_diagnostics if not diagnostic.suppressed}
    assert {'const_integer', 'top_level_const_pointer'} <= redundant

    applied = explicit_local_types.apply_candidates(result, context.root)
    rewritten = (context.root / 'Engine/Source/fixture.cpp').read_text(encoding='utf-8')

    assert applied == 11
    for expected in ('int32_t integer = 1;', 'int16_t short_integer = static_cast<short>(1);', 'float32_t floating = 1.0F;',
                     'float64_t double_floating = 1.0;', 'auto platform_long = static_cast<long>(1);', 'simple_kind kind = GetSimpleKind();',
                     'int32_t* pointer = &integer;', 'const int32_t& const_reference = const_integer;',
                     'const int32_t& authored_const_reference = integer;', 'count_t alias = GetAliasCount();',
                     'mat44 alias_backed_matrix = Translate(mat44 {});', 'auto ambiguous_matrix_alias = SelectMatrix(mat44 {}, other_mat {});',
                     'auto unspelled_matrix_alias = BuildMatrix();', 'auto inconsistent = value;', 'auto decayed = values;',
                     'const auto top_level_const_pointer = &integer;'):
        assert expected in rewritten


def test_explicit_local_types_cli_check_and_clean_control(tmp_path: Path, clang_query: Path, compiler: Path) -> None:
    context = make_project(tmp_path, compiler, {'Native/bad.cpp': 'void F() { auto value = 1; (void)value; }\n',
                                                'Native/good.cpp': 'void G() { int value = 1; (void)value; }\n'}, ['Native'])
    common = ['explicit-local-types', '--root', str(context.root), '--compilation-database', '.', '--jobs', '1', '--batch-size', '1']

    assert cli.run([*common, '--scope', 'Native/bad.cpp']) == 1
    assert cli.run([*common, '--scope', 'Native/good.cpp']) == 0


def test_smart_pointer_ast_reports_raw_declarations_and_honours_markers(tmp_path: Path, clang_query: Path, compiler: Path, capsys: pytest.CaptureFixture[str]) -> None:
    source = 'int* Raw(int* input);\n// FO_RAW_POINTER_ALLOWLIST: C callback signature\nvoid Callback(void* user_data);\nint Clean(int value);\n'
    context = make_project(tmp_path, compiler, {'Native/raw.cpp': source}, ['Native'])
    common = ['smart-pointer-ast', '--root', str(context.root), '--scope', 'Native', '--compilation-database', '.', '--jobs', '1', '--batch-size', '1']

    assert cli.run([*common, '--all']) == 1
    output = capsys.readouterr().out
    assert 'Native/raw.cpp:1: error: NewRawPointerDecl' in output
    assert 'Native/raw.cpp:3:' not in output and 'Native/raw.cpp:4:' not in output


def test_smart_pointer_ast_diff_mode_checks_only_added_lines(tmp_path: Path, clang_query: Path, compiler: Path) -> None:
    if shutil.which('git') is None:
        pytest.skip('git is not installed')
    context = make_project(tmp_path, compiler, {'Native/raw.cpp': 'int* Existing();\n'}, ['Native'])
    root = context.root
    for command in (['init', '-q'], ['add', 'Native/raw.cpp'], ['-c', 'user.name=t', '-c', 'user.email=t@t', 'commit', '-q', '-m', 'base']):
        subprocess.run(['git', *command], cwd=root, check=True, capture_output=True)
    common = ['smart-pointer-ast', '--root', str(root), '--scope', 'Native', '--compilation-database', '.', '--jobs', '1', '--batch-size', '1', '--diff-base', 'HEAD']

    assert cli.run(common) == 0
    (root / 'Native/raw.cpp').write_text('int* Existing();\nint* Added();\n', encoding='utf-8')
    assert cli.run(common) == 1
