"""AST companion of the smart-pointer audit: raw pointer declarations found by clang-query.

The normal mode is diff based: only matches on lines added since ``--diff-base`` become findings, so the existing
reviewed ABI inventory does not turn into one-time failures.  A real ABI boundary can carry a
``FO_RAW_POINTER_ALLOWLIST`` or ``NOLINT(raw-pointer)`` marker on the line or the line above it.
"""

from __future__ import annotations

import re
import subprocess
import tempfile
from pathlib import Path

from native_source_audit import clang_support as clang
from native_source_audit.common import AuditContext, AuditUsageError, Finding, normalize_code


RULE = 'NewRawPointerDecl'
ALLOW_MARKERS = (
    'FO_RAW_POINTER_ALLOWLIST',
    'NOLINT(raw-pointer)',
)
DIAG_RE = re.compile(r'^(?P<path>.+?):(?P<line>\d+):(?P<col>\d+):\s+(?P<kind>note|warning|error):')
HUNK_RE = re.compile(r'^@@ -\d+(?:,\d+)? \+(?P<start>\d+)(?:,(?P<count>\d+))? @@')
QUERY = '''
set output diag
match decl(
    anyOf(
        varDecl(hasType(pointerType())).bind("rawPointerDecl"),
        fieldDecl(hasType(pointerType())).bind("rawPointerDecl"),
        parmVarDecl(hasType(pointerType())).bind("rawPointerDecl"),
        functionDecl(returns(pointerType())).bind("rawPointerDecl")
    ),
    unless(isExpansionInSystemHeader())
)
'''
MESSAGE = 'New raw pointer declaration found by clang-query; use ptr/nptr/unique/refcount spelling or add a reviewed FO_RAW_POINTER_ALLOWLIST marker at an ABI boundary.'


def collect_added_lines(context: AuditContext, diff_base: str, displays: list[str]) -> dict[str, set[int]]:
    """Added line numbers per display path; every scope must belong to the root's own git repository."""

    if not displays:
        return {}

    root_top = git_toplevel(context.root)

    for scope in context.scopes:
        scope_path = context.root / scope
        scope_top = git_toplevel(scope_path if scope_path.is_dir() else scope_path.parent)

        if scope_top != root_top:
            raise AuditUsageError(f'--diff-base compares one git repository; scope {scope} belongs to {scope_top}, run the audit from that repository')

    command = ['git', 'diff', '--relative', '--unified=0', '--no-ext-diff', '--no-color', diff_base, '--', *displays]
    result = subprocess.run(command, cwd=context.root, text=True, encoding='utf-8', errors='replace', capture_output=True, check=False)

    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip() or f'git diff failed with exit code {result.returncode}')

    added_lines: dict[str, set[int]] = {}
    current_file = ''
    current_line = 0

    for line in result.stdout.splitlines():
        if line.startswith('+++ '):
            target = line[len('+++ '):]
            current_file = target[len('b/'):] if target.startswith('b/') else ''
            current_line = 0
            added_lines.setdefault(current_file, set())
            continue

        match = HUNK_RE.match(line)

        if match is not None:
            current_line = int(match.group('start'))
            continue

        if not current_file or current_line == 0:
            continue

        if line.startswith('+') and not line.startswith('+++'):
            added_lines.setdefault(current_file, set()).add(current_line)
            current_line += 1
        elif line.startswith('-') and not line.startswith('---'):
            continue
        else:
            current_line += 1

    return added_lines


def git_toplevel(directory: Path) -> str:
    result = subprocess.run(['git', 'rev-parse', '--show-toplevel'], cwd=directory, text=True, encoding='utf-8', capture_output=True, check=False)

    if result.returncode != 0:
        raise AuditUsageError(f'--diff-base requires a git work tree: {directory}')

    return Path(result.stdout.strip()).resolve().as_posix()


def has_allow_marker(lines: list[str], line_no: int) -> bool:
    for index in (line_no - 1, line_no - 2):
        if 0 <= index < len(lines) and any(marker in lines[index] for marker in ALLOW_MARKERS):
            return True

    return False


def run_clang_query(clang_query: Path, database_dir: Path, units: list[clang.CompilationUnit], mapper: clang.SourcePathMapper,
                    timeout: int, batch_size: int, jobs: int) -> list[tuple[str, int, int]]:
    with tempfile.NamedTemporaryFile('w', encoding='utf-8', suffix='.query', newline='\n', delete=False) as query_file:
        query_file.write(QUERY)
        query_path = Path(query_file.name)

    matches: list[tuple[str, int, int]] = []
    seen: set[tuple[str, int, int]] = set()

    try:
        commands = [
            [str(clang_query), '-p', str(database_dir), '-f', str(query_path), *(str(unit.database_path) for unit in units[start:start + batch_size])]
            for start in range(0, len(units), batch_size)
        ]

        for process in clang._iter_batched_processes(commands, timeout, jobs, 'raw pointer declarations'):
            output = process.stdout + process.stderr

            if process.returncode != 0 or 'Error parsing matcher' in output:
                raise RuntimeError(clang.tool_failure_message('clang-query', process, output))

            for line in output.splitlines():
                match = DIAG_RE.match(line)

                if match is None:
                    continue

                location = mapper.location(match.group('path'), int(match.group('line')), int(match.group('col')))

                if location is None:
                    continue

                key = (location.path, location.line, location.column)

                if key not in seen:
                    seen.add(key)
                    matches.append(key)
    finally:
        query_path.unlink(missing_ok=True)

    return sorted(matches)


def collect(context: AuditContext, *, database_dir: Path, clang_query: Path, diff_base: str | None, report_all: bool,
            unit_pattern: str | None, timeout: int, batch_size: int, jobs: int) -> tuple[int, list[Finding]]:
    clang.require_supported_tool('clang-query', clang_query)
    mapper = clang.SourcePathMapper(context)
    units = clang.load_compilation_units(database_dir, mapper)

    if unit_pattern is not None:
        unit_filter = re.compile(unit_pattern)
        units = [unit for unit in units if unit_filter.search(unit.canonical_path)]

    added_lines = collect_added_lines(context, diff_base, [unit.canonical_path for unit in units]) if diff_base and not report_all else {}
    line_cache: dict[str, list[str]] = {}
    findings: list[Finding] = []

    with tempfile.TemporaryDirectory(prefix='native_source_audit_database_') as temporary_database:
        tool_database = clang.write_analysis_database(database_dir, units, Path(temporary_database)) if units else database_dir
        matches = run_clang_query(clang_query, tool_database, units, mapper, timeout, batch_size, jobs) if units else []

    for display, line, column in matches:
        if not report_all and line not in added_lines.get(display, set()):
            continue

        if display not in line_cache:
            try:
                line_cache[display] = (context.root / display).read_text(encoding='utf-8-sig', errors='replace').splitlines()
            except OSError:
                line_cache[display] = []

        lines = line_cache[display]

        if has_allow_marker(lines, line):
            continue

        code = lines[line - 1] if 0 < line <= len(lines) else ''
        findings.append(Finding(display, line, 'error', RULE, f'{MESSAGE} (column {column})', normalize_code(code)))

    return len(units), findings
