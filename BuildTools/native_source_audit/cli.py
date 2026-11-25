"""Command-line implementation behind ``BuildTools/native_source_audit.py``."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from native_source_audit import allocator, clang_audits, smart_pointer
from native_source_audit.common import (EXIT_USAGE, AuditContext, AuditUsageError, build_context, finish_audit,
                                        relative_or_absolute)


PROGRAM = 'native_source_audit.py'

DESCRIPTION = (
    'Audit native C++ sources against the Engine coding contracts. Each audit scans --scope paths below --root '
    '(default: the Engine Source directory), applies the Engine-owned baseline plus project baselines, and fails '
    'on remaining findings or on stale baseline rows. Exit codes: 0 passed or skipped, 1 findings or stale '
    'baseline rows, 2 invalid invocation, configuration, baseline or tool failure.'
)


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog=PROGRAM, description=DESCRIPTION)
    commands = parser.add_subparsers(dest='audit', required=True, metavar='AUDIT')

    allocator_parser = commands.add_parser(
        allocator.NAME,
        help='Report standard containers, allocating std:: helpers and C allocation outside the memory system.',
        description='Report standard containers, allocating std:: helpers and bare C allocation calls that bypass the Engine memory system.',
    )
    add_common_arguments(allocator_parser)
    add_text_arguments(allocator_parser)

    pointer_parser = commands.add_parser(
        smart_pointer.NAME,
        help='Check the ptr/nptr/unique/refcount smart-pointer vocabulary and raw-pointer boundaries.',
        description='Check the smart-pointer vocabulary, nullable checks and raw-pointer boundary shapes with text rules.',
    )
    add_common_arguments(pointer_parser)
    add_text_arguments(pointer_parser)
    pointer_parser.add_argument('--inventory', action='store_true', help='Print non-failing inventory counters for pointer vocabulary and raw-pointer markers.')
    pointer_parser.add_argument('--inventory-output', type=Path, help='Write per-file inventory counters as TSV.')
    pointer_parser.add_argument('--inventory-details-output', type=Path, help='Write line-level inventory markers as TSV.')
    pointer_parser.add_argument('--summary-output', type=Path, help='Write a JSON summary of findings and inventory counters.')
    pointer_parser.add_argument('--jobs', type=int, default=0, help='Worker processes; 0 uses the CPU count; output order does not depend on it (default: 0).')

    ast_parser = commands.add_parser(
        clang_audits.SMART_POINTER_AST,
        help='Find raw pointer declarations on lines added since a git revision with clang-query.',
        description='Run clang-query over a compilation database and report raw pointer declarations, fields, parameters and returns.',
    )
    add_common_arguments(ast_parser)
    add_clang_arguments(ast_parser)
    ast_parser.add_argument('--diff-base', help='Report only matches on lines added since this git revision.')
    ast_parser.add_argument('--all', action='store_true', help='Report every match in the scopes instead of added lines only.')

    local_parser = commands.add_parser(
        clang_audits.LOCAL_VARIABLES,
        help='Find redundant top-level const locals (clang-query) and use-after-move (clang-tidy).',
        description='Run clang-query and clang-tidy over a compilation database to find redundant top-level local const and use-after-move.',
    )
    add_common_arguments(local_parser)
    add_clang_arguments(local_parser)
    local_parser.add_argument('--checks', choices=('all', 'redundant-local-const', 'use-after-move'), default='all', help='Checks to run (default: all).')
    local_parser.add_argument('--mode', choices=('check', 'report', 'apply'), default='check',
                              help='check fails on active findings; report never fails; apply removes redundant const (requires --checks redundant-local-const).')

    explicit_parser = commands.add_parser(
        clang_audits.EXPLICIT_LOCAL_TYPES,
        help='Find auto locals whose deduced type is a simple explicit type (clang-query).',
        description='Run clang-query over a compilation database and report auto locals that must spell their simple type explicitly.',
    )
    add_common_arguments(explicit_parser)
    add_clang_arguments(explicit_parser)
    explicit_parser.add_argument('--mode', choices=('check', 'report', 'apply'), default='check',
                                 help='check fails on candidates; report never fails; apply rewrites the eligible auto tokens.')
    explicit_parser.add_argument('--also-redundant-local-const', action='store_true', help='Run the redundant-local-const matcher in the same AST pass.')
    explicit_parser.add_argument('--json', type=Path, help='Write the full candidate report as JSON.')
    explicit_parser.add_argument('--apply-report', type=Path, help='Apply a previously reviewed JSON report without running clang-query (requires --mode apply).')

    return parser


def add_common_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument('--root', type=Path, help='Root that scopes, excludes, reports and project baselines are relative to (default: current directory).')
    parser.add_argument('--scope', action='append', default=[], help='File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory).')
    parser.add_argument('--exclude', action='append', default=[], help='fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable.')
    parser.add_argument('--config', type=Path, help='JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root.')
    parser.add_argument('--baseline', action='append', type=Path, default=[], help='Additional project baseline TSV with root-relative paths; repeatable.')
    parser.add_argument('--no-engine-baseline', action='store_true', help='Do not apply BuildTools/audit_baselines/<audit>.tsv from the Engine.')
    parser.add_argument('--report', type=Path, help='Write remaining findings and stale baseline rows as TSV.')
    parser.add_argument('--emit-baseline', type=Path, help='Write candidate baseline rows (empty reason column) for the remaining findings.')
    parser.add_argument('--max-printed', type=int, default=50, help='Maximum findings printed to stdout; 0 prints all (default: 50).')
    parser.add_argument('--engine-root', type=Path, help='Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool).')


def add_text_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument('--fail-on', choices=('error', 'warning', 'none'), default='error', help='Minimum severity that fails the command (default: error).')


def add_clang_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument('--compilation-database', type=Path, help='Directory containing compile_commands.json (default: config compilation_database).')
    parser.add_argument('--clang-query', help='clang-query executable (default: FO_CLANG_QUERY, then PATH).')
    parser.add_argument('--clang-tidy', help='clang-tidy executable for use-after-move (default: FO_CLANG_TIDY, then PATH).')
    parser.add_argument('--require-tooling', action='store_true', help='Fail with exit code 2 instead of skipping when clang tools or the compilation database are missing.')
    parser.add_argument('--source-pattern', help='clang-query isExpansionInFileMatching regex (default: derived from the scopes).')
    parser.add_argument('--unit-pattern', help='Only analyze compilation units whose display path matches this regex.')
    parser.add_argument('--jobs', type=int, default=4, help='Parallel clang processes (default: 4).')
    parser.add_argument('--batch-size', type=int, default=2, help='Translation units per clang process (default: 2).')
    parser.add_argument('--timeout', type=int, default=900, help='Seconds allowed per clang process (default: 900).')


def run(argv: list[str] | None = None) -> int:
    parser = make_parser()
    args = parser.parse_args(argv)

    try:
        context = build_context(
            args.audit,
            root=args.root,
            scopes=args.scope,
            excludes=args.exclude,
            baselines=args.baseline,
            config_path=args.config,
            use_engine_baseline=not args.no_engine_baseline,
            engine_root=args.engine_root,
        )

        if args.audit == allocator.NAME:
            return run_allocator(args, context)
        if args.audit == smart_pointer.NAME:
            return run_smart_pointer(args, context)

        return clang_audits.run(args, context)
    except AuditUsageError as ex:
        print(f'error: {ex}', file=sys.stderr)
        return EXIT_USAGE


def run_allocator(args: argparse.Namespace, context: AuditContext) -> int:
    files = context.iter_files()
    findings = allocator.collect(context, files)
    exit_code, _remaining = finish_audit(args, context, 'AllocatorAudit', len(files), findings, args.fail_on)
    return exit_code


def run_smart_pointer(args: argparse.Namespace, context: AuditContext) -> int:
    options = smart_pointer.options_from_config(context)
    files = context.iter_files()
    result = smart_pointer.collect(context, files, options, args.jobs if args.jobs > 0 else os.cpu_count() or 1)

    if args.inventory:
        totals: dict[str, int] = {}

        for row in result.rows:
            totals[row.metric] = totals.get(row.metric, 0) + row.count

        for metric, count in sorted(totals.items()):
            print(f'SmartPointerInventory: {metric}={count}')

    if args.inventory_output is not None:
        smart_pointer.write_inventory(relative_or_absolute(args.inventory_output, context.root), result.rows)
    if args.inventory_details_output is not None:
        smart_pointer.write_inventory_details(relative_or_absolute(args.inventory_details_output, context.root), result.details)

    exit_code, remaining = finish_audit(args, context, 'SmartPointerAudit', len(files), result.findings, args.fail_on)

    if args.summary_output is not None:
        smart_pointer.write_summary(relative_or_absolute(args.summary_output, context.root), len(files), remaining, result.rows)

    return exit_code
