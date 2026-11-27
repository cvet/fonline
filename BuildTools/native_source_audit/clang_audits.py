"""Dispatch of the clang-based audits with the shared skip, baseline and exit-code contract."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

from native_source_audit import clang_support as clang
from native_source_audit import explicit_local_types, local_variables, raw_pointer_ast
from native_source_audit.common import (EXIT_OK, EXIT_USAGE, AuditContext, Finding, finish_audit, normalize_code,
                                        relative_or_absolute)


SMART_POINTER_AST = 'smart-pointer-ast'
LOCAL_VARIABLES = 'local-variables'
EXPLICIT_LOCAL_TYPES = 'explicit-local-types'
EXPLICIT_TYPE_RULE = 'ExplicitSimpleLocalType'

DISPLAY_NAMES = {
    SMART_POINTER_AST: 'SmartPointerAstAudit',
    LOCAL_VARIABLES: 'LocalVariableAudit',
    EXPLICIT_LOCAL_TYPES: 'ExplicitLocalTypesAudit',
}


def run(args: argparse.Namespace, context: AuditContext) -> int:
    name = DISPLAY_NAMES[args.audit]

    try:
        if args.audit == EXPLICIT_LOCAL_TYPES and args.apply_report is not None:
            return apply_reviewed_report(args, context, name)

        return dispatch(args, context, name)
    except clang.ToolMissing as ex:
        if args.require_tooling:
            print(f'error: {name}: {ex}', file=sys.stderr)
            return EXIT_USAGE

        print(f'{name}: skipped: {ex}')
        return EXIT_OK
    except (RuntimeError, OSError, subprocess.TimeoutExpired, json.JSONDecodeError) as ex:
        print(f'error: {name}: {ex}', file=sys.stderr)
        return EXIT_USAGE


def dispatch(args: argparse.Namespace, context: AuditContext, name: str) -> int:
    database_dir = clang.resolve_compilation_database(context, args.compilation_database)
    source_pattern = args.source_pattern or clang.scope_source_pattern(context)
    jobs = max(1, args.jobs)
    batch_size = max(1, args.batch_size)

    if args.audit == SMART_POINTER_AST:
        if not args.all and not args.diff_base:
            raise RuntimeError('pass --diff-base <revision> for added-line checks or --all for every match')

        clang_query = clang.discover_tool(args.clang_query, 'FO_CLANG_QUERY', ('clang-query-20', 'clang-query'))
        units, findings = raw_pointer_ast.collect(
            context, database_dir=database_dir, clang_query=clang_query, diff_base=args.diff_base, report_all=args.all,
            unit_pattern=args.unit_pattern, timeout=args.timeout, batch_size=batch_size, jobs=jobs,
        )
        exit_code, _remaining = finish_audit(args, context, name, units, findings, 'error')
        return exit_code

    if args.audit == LOCAL_VARIABLES:
        return run_local_variables(args, context, name, database_dir, source_pattern, jobs, batch_size)

    return run_explicit_local_types(args, context, name, database_dir, source_pattern, jobs, batch_size)


def run_local_variables(args: argparse.Namespace, context: AuditContext, name: str, database_dir: Path, source_pattern: str,
                        jobs: int, batch_size: int) -> int:
    if args.mode == 'apply' and args.checks != 'redundant-local-const':
        raise RuntimeError('--mode apply requires --checks redundant-local-const')

    clang_query = None
    clang_tidy = None

    if args.checks in {'all', 'redundant-local-const'}:
        clang_query = clang.discover_tool(args.clang_query, 'FO_CLANG_QUERY', ('clang-query-20', 'clang-query'))
    if args.checks in {'all', 'use-after-move'}:
        clang_tidy = clang.discover_tool(args.clang_tidy, 'FO_CLANG_TIDY', ('clang-tidy-20', 'clang-tidy'))

    result = local_variables.analyze(
        context, database_dir=database_dir, clang_query=clang_query, clang_tidy=clang_tidy, source_pattern=source_pattern,
        timeout=args.timeout, batch_size=batch_size, jobs=jobs, checks=args.checks, unit_pattern=args.unit_pattern,
    )

    if args.mode == 'apply':
        applied = clang.apply_redundant_const_fixes(result.diagnostics)
        print(f'{name}: applied redundant const fixes: {applied}')
        return EXIT_OK

    for diagnostic in result.diagnostics:
        if diagnostic.suppressed:
            clang.print_diagnostic(diagnostic)

    findings = [diagnostic_finding(diagnostic) for diagnostic in result.diagnostics if not diagnostic.suppressed]
    exit_code, _remaining = finish_audit(args, context, name, len(result.units), findings, 'warning' if args.mode == 'check' else 'none')
    return exit_code


def run_explicit_local_types(args: argparse.Namespace, context: AuditContext, name: str, database_dir: Path, source_pattern: str,
                             jobs: int, batch_size: int) -> int:
    if args.mode == 'apply' and args.also_redundant_local_const:
        raise RuntimeError('--also-redundant-local-const cannot be combined with --mode apply')

    clang_query = clang.discover_tool(args.clang_query, 'FO_CLANG_QUERY', ('clang-query-20', 'clang-query'))
    result = explicit_local_types.run_inventory(
        context, database_dir=database_dir, clang_query=clang_query, source_pattern=source_pattern, jobs=jobs,
        batch_size=batch_size, timeout=args.timeout, unit_pattern=args.unit_pattern,
        include_redundant_const=args.also_redundant_local_const,
    )
    payload = explicit_local_types.result_payload(result, args.mode)

    if args.mode == 'apply':
        payload['applied_replacements'] = explicit_local_types.apply_candidates(result, context.root)
        print(f'{name}: applied replacements: {payload["applied_replacements"]}')

    if args.json is not None:
        json_path = relative_or_absolute(args.json, context.root)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + '\n', encoding='utf-8', newline='\n')

    if args.mode == 'apply':
        return EXIT_OK

    findings = [candidate_finding(context, candidate) for candidate in result.candidates]
    findings.extend(diagnostic_finding(diagnostic) for diagnostic in result.redundant_const_diagnostics if not diagnostic.suppressed)
    exit_code, _remaining = finish_audit(args, context, name, len(result.units), findings, 'warning' if args.mode == 'check' else 'none')
    return exit_code


def apply_reviewed_report(args: argparse.Namespace, context: AuditContext, name: str) -> int:
    if args.mode != 'apply':
        raise RuntimeError('--apply-report requires --mode apply')

    candidates = explicit_local_types.load_report_candidates(relative_or_absolute(args.apply_report, context.root))
    applied = explicit_local_types.apply_candidate_list(candidates, context.root)
    print(f'{name}: applied replacements from reviewed report: {applied}')
    return EXIT_OK


def source_line(path: Path, line: int) -> str:
    try:
        lines = path.read_text(encoding='utf-8-sig', errors='replace').splitlines()
    except OSError:
        return ''

    return lines[line - 1] if 0 < line <= len(lines) else ''


def diagnostic_finding(diagnostic: clang.Diagnostic) -> Finding:
    location = diagnostic.location
    notes = ''.join(f'; {note}' for note in diagnostic.notes)
    return Finding(
        location.path,
        location.line,
        diagnostic.severity,
        diagnostic.code,
        f'{diagnostic.message} (column {location.column}){notes}',
        normalize_code(source_line(Path(location.absolute_path), location.line)),
    )


def candidate_finding(context: AuditContext, candidate: explicit_local_types.Candidate) -> Finding:
    return Finding(
        candidate.path,
        candidate.line,
        'error',
        EXPLICIT_TYPE_RULE,
        f'auto -> {candidate.replacement} (deduced {candidate.deduced_type}, column {candidate.column})',
        normalize_code(source_line(context.root / candidate.path, candidate.line)),
    )
