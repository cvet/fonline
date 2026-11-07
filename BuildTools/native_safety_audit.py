"""Audit Engine and project native sources against the entity-cover and exception-safety contracts.

The Engine scope is always audited from the Engine checkout that contains this
script and is compared with the Engine baselines in ``BuildTools/audit_baselines``.
A game project adds its native directories with ``--scope``, its reviewed rules
and vendored exclusions with ``--config``, and its own rows with ``--baseline``.
Exit codes: 0 pass, 1 audit findings, 2 invalid command line or configuration.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from native_safety_audit import exception_safety, sync_scope
from native_safety_audit.scope import ConfigError, create_context


ACTIONS = ("audit", "report", "update")
ACTION_OPTIONS = {
    "audit": ("fail_on",),
    "report": ("level", "area", "file_filter", "function_filter"),
    "update": ("add_new", "prune_removed", "migrate_signatures", "accept", "accept_all_changed"),
}


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="native_safety_audit.py",
        description="Audit Engine and project native sources against the entity-cover and exception-safety contracts.",
    )
    commands = parser.add_subparsers(dest="command", required=True, metavar="command")

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--root", type=Path, default=None,
                        help="repository root for --scope, --config and --baseline paths (default: current directory)")
    common.add_argument("--engine-root", type=Path, default=None,
                        help="Engine checkout to audit (default: the checkout that contains this script)")
    common.add_argument("--scope", action="append", default=[], metavar="DIR",
                        help="root-relative project native directory audited with the Engine rules (repeatable)")
    common.add_argument("--config", type=Path, default=None, metavar="FILE",
                        help="project JSON configuration with vendored exclusions and reviewed project rules")
    common.add_argument("--baseline", type=Path, default=None, metavar="FILE",
                        help="project baseline TSV for findings in the --scope directories")

    sync = commands.add_parser("sync-scope", parents=[common],
                               help="reject native code that obtains, replaces or releases entity cover for script exports")
    sync.add_argument("--json", action="store_true", help="print the summary and diagnostics as JSON")

    exception = commands.add_parser("exception-safety", parents=[common],
                                    help="check per-function exception-safety classifications against the baselines")
    exception.add_argument("action", nargs="?", choices=ACTIONS, default="audit",
                           help="audit (default) fails on drift; report summarizes the baselines; update maintains them")
    exception.add_argument("--fail-on", choices=("error", "warning"), default=None,
                           help="audit: also fail on warnings (pending None adjudication, review notes)")
    exception.add_argument("--level", choices=exception_safety.LEVELS, help="report: list rows with this level")
    exception.add_argument("--area", help="report: list rows of this source area (second path component)")
    exception.add_argument("--file", dest="file_filter", help="report: list rows whose file contains this text")
    exception.add_argument("--function", dest="function_filter", help="report: list rows whose function contains this text")
    exception.add_argument("--add-new", action="store_true", help="update: add unclassified rows for new functions")
    exception.add_argument("--prune-removed", action="store_true", help="update: remove rows whose function vanished")
    exception.add_argument("--migrate-signatures", action="store_true",
                           help="update: carry classifications across signature-only changes with identical body hashes")
    exception.add_argument("--accept", action="append", default=[], metavar="FILE::FUNCTION",
                           help="update: refresh the body hash of a re-derived function (repeatable)")
    exception.add_argument("--accept-all-changed", action="store_true", help="update: refresh every changed body hash")

    return parser


def main(argv: list[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    parser = create_parser()
    args = parser.parse_args(argv)

    if args.command == "exception-safety":
        for action, options in ACTION_OPTIONS.items():
            used = [option for option in options if getattr(args, option) not in (None, False, [])]

            if action != args.action and used:
                parser.error(f"exception-safety {args.action}: --{used[0].replace('_filter', '').replace('_', '-')} belongs to the {action} action")

    try:
        context = create_context(args.root, args.engine_root, args.scope, args.config)

        if args.command == "sync-scope":
            return sync_scope.run(context, args.baseline, args.json)

        units, scope_errors = exception_safety.create_units(context, args.baseline)
    except ConfigError as exception:
        print(f"error: {exception}", file=sys.stderr)
        return 2

    if args.action == "audit":
        return exception_safety.run_audit(units, scope_errors, args.fail_on or "error")

    if args.action == "report":
        return exception_safety.run_report(units, args.level, args.area, args.file_filter, args.function_filter)

    return exception_safety.run_update(units, scope_errors, args.add_new, args.prune_removed,
                                       args.migrate_signatures, args.accept, args.accept_all_changed)


if __name__ == "__main__":
    raise SystemExit(main())
