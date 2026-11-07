"""Local-variable audit: redundant top-level ``const`` on locals (clang-query) and use-after-move (clang-tidy).

A finding may be suppressed in source with ``FO_REDUNDANT_CONST_SUPPRESS: <reason>`` or
``FO_USE_AFTER_MOVE_SUPPRESS: <reason>`` on the reported line or the line above it; the reason needs at least
eight characters.  Baseline rows are the alternative for findings that must not carry a source marker.
"""

from __future__ import annotations

import re
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

from native_source_audit import clang_support as clang
from native_source_audit.common import AuditContext


REDUNDANT_CONST_QUERY = 'redundant_local_const.query.in'


@dataclass
class AnalysisResult:
    units: list[clang.CompilationUnit]
    diagnostics: list[clang.Diagnostic]
    tool_versions: dict[str, str]


def run_redundant_const(
    clang_query: Path,
    database_dir: Path,
    units: Sequence[clang.CompilationUnit],
    mapper: clang.SourcePathMapper,
    source_pattern: str,
    timeout: int,
    batch_size: int,
    jobs: int,
) -> list[clang.Diagnostic]:
    diagnostics: dict[tuple[str, str, int, int, str], clang.Diagnostic] = {}
    query_file = clang.build_query_file([REDUNDANT_CONST_QUERY], source_pattern)

    try:
        commands = [
            [str(clang_query), '-p', str(database_dir), '-f', str(query_file), *(str(unit.database_path) for unit in units[start:start + batch_size])]
            for start in range(0, len(units), batch_size)
        ]

        for process in clang._iter_batched_processes(commands, timeout, jobs, 'redundant const'):
            output = process.stdout + process.stderr

            if process.returncode != 0 or 'Error parsing matcher' in output or 'Error while processing' in output or clang._has_compilation_error(output):
                raise RuntimeError(clang.tool_failure_message('clang-query', process, output))

            diagnostics.update((item.key, item) for item in clang.parse_redundant_const_output(output, mapper))
    finally:
        query_file.unlink(missing_ok=True)

    return list(diagnostics.values())


def _diagnostic_location_match(line: str) -> re.Match[str] | None:
    return re.match(r'^(.+):(\d+):(\d+):\s+(warning|error):\s+(.+?)\s+\[bugprone-use-after-move\]\s*$', line)


def run_use_after_move(
    clang_tidy: Path,
    database_dir: Path,
    units: Sequence[clang.CompilationUnit],
    mapper: clang.SourcePathMapper,
    timeout: int,
    batch_size: int,
    jobs: int,
) -> list[clang.Diagnostic]:
    diagnostics: dict[tuple[str, str, int, int, str], clang.Diagnostic] = {}
    commands = [
        [str(clang_tidy), '-p', str(database_dir), '-checks=-*,bugprone-use-after-move', '--quiet', *(str(unit.database_path) for unit in units[start:start + batch_size])]
        for start in range(0, len(units), batch_size)
    ]

    for process in clang._iter_batched_processes(commands, timeout, jobs, 'use-after-move'):
        output = process.stdout + process.stderr

        if process.returncode != 0:
            raise RuntimeError(clang.tool_failure_message('clang-tidy', process, output))

        last_diagnostic: clang.Diagnostic | None = None

        for line in output.splitlines():
            match = _diagnostic_location_match(line)

            if match is not None:
                location = mapper.location(match.group(1), int(match.group(2)), int(match.group(3)))

                if location is None:
                    last_diagnostic = None
                    continue

                message = match.group(5)
                variable_match = re.search(r"'([^']+)'\s+used after", message)
                variable = variable_match.group(1) if variable_match is not None else ''
                last_diagnostic = clang.Diagnostic(
                    code='fo-use-after-move',
                    severity='error',
                    message=f"use of consumed local variable '{variable}'" if variable else message,
                    location=location,
                    variable=variable,
                )
                diagnostics[last_diagnostic.key] = last_diagnostic
                continue

            note_match = re.match(r'^(.+):(\d+):(\d+):\s+note:\s+(.+)$', line)

            if last_diagnostic is not None and note_match is not None and 'move occurred here' in note_match.group(4):
                note_location = mapper.location(note_match.group(1), int(note_match.group(2)), int(note_match.group(3)))

                if note_location is not None:
                    last_diagnostic.notes.append(f'{note_location.path}:{note_location.line}:{note_location.column}: variable was consumed here')

    return list(diagnostics.values())


def select_units(context: AuditContext, database_dir: Path, unit_pattern: str | None) -> tuple[clang.SourcePathMapper, list[clang.CompilationUnit]]:
    mapper = clang.SourcePathMapper(context)
    units = clang.load_compilation_units(database_dir, mapper)

    if unit_pattern is not None:
        unit_filter = re.compile(unit_pattern)
        units = [unit for unit in units if unit_filter.search(unit.canonical_path)]

    if not units:
        raise RuntimeError(f'No compilation units inside the audit scopes in {database_dir / "compile_commands.json"}')

    return mapper, units


def analyze(
    context: AuditContext,
    *,
    database_dir: Path,
    clang_query: Path | None,
    clang_tidy: Path | None,
    source_pattern: str,
    timeout: int,
    batch_size: int,
    jobs: int,
    checks: str,
    unit_pattern: str | None = None,
) -> AnalysisResult:
    """Run the selected checks; the caller resolves the tools needed by *checks* beforehand."""

    mapper, units = select_units(context, database_dir, unit_pattern)
    diagnostics: list[clang.Diagnostic] = []
    versions: dict[str, str] = {}

    with tempfile.TemporaryDirectory(prefix='native_source_audit_database_') as temporary_database:
        tool_database = clang.write_analysis_database(database_dir, units, Path(temporary_database))

        if checks in {'all', 'redundant-local-const'}:
            assert clang_query is not None
            versions['clang-query'] = clang.require_supported_tool('clang-query', clang_query)
            diagnostics.extend(run_redundant_const(clang_query, tool_database, units, mapper, source_pattern, timeout, batch_size, jobs))

        if checks in {'all', 'use-after-move'}:
            assert clang_tidy is not None
            versions['clang-tidy'] = clang.require_supported_tool('clang-tidy', clang_tidy)
            diagnostics.extend(run_use_after_move(clang_tidy, tool_database, units, mapper, timeout, batch_size, jobs))

    clang.apply_suppressions(diagnostics)
    diagnostics = list({diagnostic.key: diagnostic for diagnostic in diagnostics}.values())
    diagnostics.sort(key=lambda item: (item.location.path, item.location.line, item.location.column, item.code))
    return AnalysisResult(units, diagnostics, versions)
