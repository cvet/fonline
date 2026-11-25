"""Check authored AngelScript modules against the Engine module conventions.

The rules come from the Engine AngelScript style guide and the backend that
builds a module from every configured ``.fos`` file.  Each rule can be turned
off, or an optional rule turned on, from a JSON configuration, so a project
keeps exactly the rules its own style document adopts:

- ``namespace-matches-file``: the first top-level namespace equals the file stem;
- ``no-include``: no ``#include``; the backend already includes every module and
  cross-module calls name their owner as ``Namespace::Function()``;
- ``side-macro-value-test``: ``SERVER``, ``CLIENT`` and ``MAPPER`` are always
  defined as 0 or 1, so ``#ifdef`` and ``defined()`` do not isolate a side;
- ``preprocessor-balance``: every conditional block is closed;
- ``sort-directive``: the backend reads ``Sort N`` only from the first line and
  reads a non-integer value as 0;
- ``file-ending``: UTF-8 without BOM and exactly one line ending at EOF;
- ``commented-out-code`` and ``file-size``: optional quality ratchets, off by default.

Findings above a ``--baseline`` fail; counts may only go down.
"""

from __future__ import annotations

import argparse
from collections.abc import Sequence
from dataclasses import dataclass, field
import json
from pathlib import Path
import re
import sys

import angelscript_source
from check_script_bindings import SCRIPT_SUFFIXES, collect_files
from finding_baseline import (BaselineError, Finding, apply_baseline, format_finding, load_baselines,
                              sort_findings, write_baseline)


CONFIG_SCHEMA_VERSION = 1
SIDE_MACROS = ("SERVER", "CLIENT", "MAPPER")
DEFAULT_MAX_LINES = 1500

ENGINE_ROOT = Path(__file__).resolve().parents[1]

RULES = {
    "namespace-matches-file": (True, "the first top-level namespace equals the file stem"),
    "no-include": (True, "no #include directive; name the owning module as Namespace::Function()"),
    "side-macro-value-test": (True, "side macros are tested by value with #if, not #ifdef or defined()"),
    "preprocessor-balance": (True, "every #if/#ifdef/#ifndef has a matching #endif"),
    "sort-directive": (True, "Sort N appears only on the first line and carries an integer"),
    "file-ending": (True, "UTF-8 without BOM and exactly one line ending at EOF"),
    "commented-out-code": (False, "disabled code left in // comments"),
    "file-size": (False, "a module longer than max_lines lines"),
}
_RULE_OPTIONS = {
    "namespace-matches-file": {"enabled", "exceptions", "require_namespace"},
    "side-macro-value-test": {"enabled", "macros"},
    "file-size": {"enabled", "max_lines"},
}

_DIRECTIVE_RE = re.compile(r"^[ \t]*#[ \t]*(?P<name>[A-Za-z]+)\b(?P<rest>.*)$")
_NAMESPACE_RE = re.compile(r"\bnamespace\s+(?P<name>[A-Za-z_][\w:]*)\s*\{")
_SORT_LATER_RE = re.compile(r"^[ \t]*//[ \t]*Sort[ \t]+[+-]?\d+[ \t]*$")
_SORT_VALUE_RE = re.compile(r"[+-]?\d+")
_COMMENTED_CODE_RE = re.compile(
    r"^[ \t]*//[ \t]*(?:if|for|while|return|switch|else|do)\b[^\n]*[;{)]"
    r"|^[ \t]*//[ \t]*[A-Za-z_][\w:.]*[ \t]*\([^\n]*\)[ \t]*;")


@dataclass
class HygieneConfig:
    enabled: dict[str, bool] = field(default_factory=lambda: {rule: default for rule, (default, _) in RULES.items()})
    namespace_exceptions: dict[str, str] = field(default_factory=dict)
    require_namespace: bool = True
    side_macros: tuple[str, ...] = SIDE_MACROS
    max_lines: int = DEFAULT_MAX_LINES
    exclude: list[str] = field(default_factory=list)


class ConfigError(Exception):
    """The hygiene configuration violates its schema."""


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="check_angelscript_hygiene.py",
        description="Check authored AngelScript modules for namespace, include, side-macro, sort-directive "
                    "and file-ending conventions with an optional count-ratchet baseline")
    parser.add_argument("--root", type=Path,
                        help="repository root that script directories and reported paths are relative to "
                             "(default: the Engine root that contains this script)")
    parser.add_argument("--scripts", action="append", default=[], metavar="DIR",
                        help="root-relative directory searched recursively for .fos files; repeatable "
                             "(default: Examples)")
    parser.add_argument("--config", type=Path, metavar="JSON",
                        help="rule switches, rule options and excludes (schema_version 1)")
    parser.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                        help="fnmatch pattern for root-relative POSIX paths to skip; repeatable")
    parser.add_argument("--baseline", action="append", type=Path, default=[], metavar="TSV",
                        help="accepted findings (rule, path, count, reason TSV); repeatable")
    parser.add_argument("--write-baseline", type=Path, metavar="TSV",
                        help="write current finding counts with empty reasons instead of gating")
    parser.add_argument("--list-rules", action="store_true", help="print rule identifiers, defaults and exit")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)

    if args.list_rules:
        for rule, (default, description) in RULES.items():
            print(f"{rule}\t{'on' if default else 'off'}\t{description}")
        return 0

    try:
        config = load_config(args.config) if args.config is not None else HygieneConfig()
    except ConfigError as error:
        print(f"[AngelScriptHygiene] {error}", file=sys.stderr)
        return 2

    root = (args.root or ENGINE_ROOT).resolve()
    findings, errors = check_hygiene(root, args.scripts or ["Examples"], config, args.exclude)

    if errors:
        for error in errors:
            print(f"[AngelScriptHygiene] {error}", file=sys.stderr)
        return 2

    if args.write_baseline is not None:
        rows = write_baseline(args.write_baseline, findings)
        print(f"[AngelScriptHygiene] wrote {rows} baseline row(s) to {args.write_baseline}; "
              f"add a reason to every row")
        return 0

    try:
        baseline = load_baselines(args.baseline)
    except BaselineError as error:
        print(f"[AngelScriptHygiene] {error}", file=sys.stderr)
        return 2

    remaining, stale = apply_baseline(findings, baseline)
    for finding in remaining:
        print(format_finding(finding))

    for diagnostic in stale:
        print(diagnostic)

    print(f"[AngelScriptHygiene] {len(findings)} finding(s), {len(remaining)} above the baseline, "
          f"{len(stale)} stale baseline row(s)")

    return 1 if remaining or stale else 0


def load_config(path: Path) -> HygieneConfig:
    """Read a hygiene configuration; unknown rules, options and keys are errors."""

    try:
        data = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ConfigError(f"{path}: unable to read configuration: {error}") from error

    if not isinstance(data, dict):
        raise ConfigError(f"{path}: configuration must be a JSON object")

    unknown = set(data) - {"schema_version", "exclude", "rules"}

    if unknown:
        raise ConfigError(f"{path}: unknown configuration keys: {', '.join(sorted(unknown))}")

    if data.get("schema_version") != CONFIG_SCHEMA_VERSION:
        raise ConfigError(f"{path}: schema_version must be {CONFIG_SCHEMA_VERSION}")

    config = HygieneConfig()
    config.exclude = _string_list(data.get("exclude", []), f"{path}: exclude")
    rules = data.get("rules", {})

    if not isinstance(rules, dict):
        raise ConfigError(f"{path}: rules must be an object keyed by rule identifier")

    for rule, options in rules.items():
        if rule not in RULES:
            raise ConfigError(f"{path}: unknown rule {rule}; use --list-rules")

        if not isinstance(options, dict):
            raise ConfigError(f"{path}: rules.{rule} must be an object")

        unknown_options = set(options) - _RULE_OPTIONS.get(rule, {"enabled"})

        if unknown_options:
            raise ConfigError(f"{path}: unknown options for {rule}: {', '.join(sorted(unknown_options))}")

        if "enabled" in options:
            if not isinstance(options["enabled"], bool):
                raise ConfigError(f"{path}: rules.{rule}.enabled must be true or false")

            config.enabled[rule] = options["enabled"]

    _read_rule_options(path, rules, config)

    return config


def check_hygiene(root: Path, scripts: Sequence[str], config: HygieneConfig,
                  excludes: Sequence[str] = ()) -> tuple[list[Finding], list[str]]:
    """Return hygiene findings for every ``.fos`` file below *scripts*."""

    files, errors = collect_files(root, scripts, SCRIPT_SUFFIXES, [*config.exclude, *excludes])

    if errors:
        return [], errors

    findings: list[Finding] = []

    for rel in files:
        try:
            raw = (root / rel).read_bytes()
        except OSError as error:
            errors.append(f"{rel}: unable to read file: {error}")
            continue

        findings.extend(check_script(rel, raw, config))

    return sort_findings(findings), errors


def check_script(rel: str, raw: bytes, config: HygieneConfig) -> list[Finding]:
    """Apply the enabled rules to one script file."""

    findings: list[Finding] = []

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        if config.enabled["file-ending"]:
            findings.append(Finding("file-ending", rel, 1, "file is not valid UTF-8"))
        return findings

    text = text.removeprefix("\ufeff").replace("\r\n", "\n")
    kinds = angelscript_source.classify(text)
    code = angelscript_source.mask(text, kinds)
    lines = text.split("\n")
    code_lines = code.split("\n")

    if config.enabled["namespace-matches-file"]:
        findings.extend(_check_namespace(rel, code, config))

    if config.enabled["no-include"]:
        for number, line in enumerate(code_lines, 1):
            directive = _DIRECTIVE_RE.match(line)

            if directive is not None and directive.group("name") == "include":
                findings.append(Finding(
                    "no-include", rel, number,
                    "#include in a module; the backend already builds every configured file into one module, "
                    "so call the owner as Namespace::Function()"))

    if config.enabled["side-macro-value-test"]:
        findings.extend(_check_side_macros(rel, code_lines, config.side_macros))

    if config.enabled["preprocessor-balance"]:
        findings.extend(_check_balance(rel, code_lines))

    if config.enabled["sort-directive"]:
        findings.extend(_check_sort(rel, lines))

    if config.enabled["file-ending"]:
        findings.extend(_check_file_ending(rel, raw))

    if config.enabled["commented-out-code"]:
        comments = angelscript_source.mask(text, kinds, angelscript_source.LINE_COMMENT + angelscript_source.BLOCK_COMMENT)

        for number, line in enumerate(comments.split("\n"), 1):
            if "///@" in line or "////" in line:
                continue

            if _COMMENTED_CODE_RE.match(line):
                findings.append(Finding("commented-out-code", rel, number,
                                        "commented-out code; delete it, version control keeps the history"))

    if config.enabled["file-size"]:
        count = len(lines) - 1 if text.endswith("\n") else len(lines)

        if count > config.max_lines:
            findings.append(Finding("file-size", rel, 1,
                                    f"{count} lines exceed the {config.max_lines}-line limit; split the module "
                                    f"by responsibility"))

    return findings


def _check_namespace(rel: str, code: str, config: HygieneConfig) -> list[Finding]:
    stem = rel.rsplit("/", 1)[-1].rsplit(".", 1)[0]
    expected = config.namespace_exceptions.get(rel, stem)
    depth = 0
    position = 0

    for match in _NAMESPACE_RE.finditer(code):
        depth += code.count("{", position, match.start()) - code.count("}", position, match.start())
        position = match.start()

        if depth != 0:
            continue

        declared = match.group("name")

        if declared == expected:
            return []

        return [Finding("namespace-matches-file", rel, angelscript_source.line_of(code, match.start()),
                        f"namespace '{declared}' must match the file stem '{expected}'")]

    if config.require_namespace:
        return [Finding("namespace-matches-file", rel, 1, f"module declares no namespace; use namespace {expected}")]

    return []


def _check_side_macros(rel: str, code_lines: list[str], macros: Sequence[str]) -> list[Finding]:
    findings: list[Finding] = []
    alternatives = "|".join(re.escape(macro) for macro in macros)
    defined_re = re.compile(rf"\bdefined\s*\(?\s*({alternatives})\b")
    name_re = re.compile(rf"\s*({alternatives})\b")

    for number, line in enumerate(code_lines, 1):
        directive = _DIRECTIVE_RE.match(line)

        if directive is None:
            continue

        name = directive.group("name")
        rest = directive.group("rest")
        macro = None

        if name in ("ifdef", "ifndef"):
            match = name_re.match(rest)
            macro = match.group(1) if match is not None else None
        elif name in ("if", "elif"):
            match = defined_re.search(rest)
            macro = match.group(1) if match is not None else None

        if macro is not None:
            findings.append(Finding(
                "side-macro-value-test", rel, number,
                f"{macro} is defined as 0 or 1 in every side module, so this test is always the same; "
                f"use #if {macro} or #if !{macro}"))

    return findings


def _check_balance(rel: str, code_lines: list[str]) -> list[Finding]:
    findings: list[Finding] = []
    stack: list[int] = []

    for number, line in enumerate(code_lines, 1):
        directive = _DIRECTIVE_RE.match(line)

        if directive is None:
            continue

        name = directive.group("name")

        if name in ("if", "ifdef", "ifndef"):
            stack.append(number)
        elif name in ("elif", "else") and not stack:
            findings.append(Finding("preprocessor-balance", rel, number, f"#{name} without an open #if"))
        elif name == "endif":
            if stack:
                stack.pop()
            else:
                findings.append(Finding("preprocessor-balance", rel, number, "#endif without an open #if"))

    for number in stack:
        findings.append(Finding("preprocessor-balance", rel, number, "conditional block is not closed by #endif"))

    return findings


def _check_sort(rel: str, lines: list[str]) -> list[Finding]:
    findings: list[Finding] = []
    first = lines[0] if lines else ""
    position = first.find("Sort ")

    if position >= 0:
        value = first[position + len("Sort "):].strip().split(" ", 1)[0]

        if not _SORT_VALUE_RE.fullmatch(value):
            findings.append(Finding(
                "sort-directive", rel, 1,
                f"the first line contains 'Sort ' followed by '{value}', which the backend reads as sort value 0"))

    for number, line in enumerate(lines[1:], 2):
        if _SORT_LATER_RE.match(line):
            findings.append(Finding(
                "sort-directive", rel, number,
                "the backend reads Sort N only from the first line; move it there or delete it"))

    return findings


def _check_file_ending(rel: str, raw: bytes) -> list[Finding]:
    findings: list[Finding] = []

    if raw.startswith(b"\xef\xbb\xbf"):
        findings.append(Finding("file-ending", rel, 1, "file starts with a UTF-8 BOM"))

    body = raw.rstrip(b"\r\n")
    trailer = raw[len(body):]
    line = raw.count(b"\n") + 1

    if not body:
        findings.append(Finding("file-ending", rel, 1, "file is empty"))
    elif trailer not in (b"\n", b"\r\n"):
        message = ("file does not end with a line ending" if not trailer
                   else "file ends with extra empty lines; keep exactly one line ending")
        findings.append(Finding("file-ending", rel, line, message))

    return findings


def _read_rule_options(path: Path, rules: dict, config: HygieneConfig) -> None:
    namespace = rules.get("namespace-matches-file", {})

    if "require_namespace" in namespace:
        if not isinstance(namespace["require_namespace"], bool):
            raise ConfigError(f"{path}: namespace-matches-file.require_namespace must be true or false")

        config.require_namespace = namespace["require_namespace"]

    for index, exception in enumerate(namespace.get("exceptions", [])):
        label = f"{path}: namespace-matches-file.exceptions[{index}]"

        if not isinstance(exception, dict) or set(exception) != {"path", "namespace", "reason"}:
            raise ConfigError(f"{label} must have exactly path, namespace and reason")

        if not all(isinstance(exception[key], str) and exception[key].strip() for key in exception):
            raise ConfigError(f"{label} needs a non-empty path, namespace and reason")

        if exception["path"] in config.namespace_exceptions:
            raise ConfigError(f"{label} repeats the path {exception['path']}")

        config.namespace_exceptions[exception["path"]] = exception["namespace"]

    side = rules.get("side-macro-value-test", {})

    if "macros" in side:
        macros = _string_list(side["macros"], f"{path}: side-macro-value-test.macros")

        if not macros or not all(re.fullmatch(r"[A-Za-z_]\w*", macro) for macro in macros):
            raise ConfigError(f"{path}: side-macro-value-test.macros must list macro identifiers")

        config.side_macros = tuple(macros)

    size = rules.get("file-size", {})

    if "max_lines" in size:
        if not isinstance(size["max_lines"], int) or isinstance(size["max_lines"], bool) or size["max_lines"] < 1:
            raise ConfigError(f"{path}: file-size.max_lines must be a positive integer")

        config.max_lines = size["max_lines"]


def _string_list(value: object, label: str) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) and item for item in value):
        raise ConfigError(f"{label} must be a list of non-empty strings")

    return list(value)


if __name__ == "__main__":
    raise SystemExit(main())
