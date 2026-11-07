"""Scope, configuration, baseline and report support shared by the native source audits.

Paths are handled in three forms:

- a *display path* is the POSIX path relative to ``--root``; findings, reports and project baselines use it;
- a *policy key* names a file for the rules that single out Engine files: files under the Engine ``Source``
  directory use their Engine-relative path (``Source/...``), every other file is project code and uses
  ``PROJECT_KEY_PREFIX`` followed by its display path;
- the Engine baseline is written relative to the Engine root and translated to display paths on load.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path


# Not resolved: an Engine reached through a link stays inside the embedding project's root
ENGINE_ROOT = Path(os.path.abspath(__file__)).parents[2]
ENGINE_BASELINE_DIR = Path('BuildTools') / 'audit_baselines'
CONFIG_SCHEMA_VERSION = 1
CONFIG_KEYS = {'schema_version', 'scopes', 'exclude', 'baselines', 'compilation_database', 'smart_pointer'}
NATIVE_SOURCE_EXTS = frozenset({'.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl'})
PROJECT_KEY_PREFIX = '@project/'
BASELINE_HEADER = 'path\trule\tcode\treason'
BASELINE_ANY_CODE = '*'
STALE_BASELINE_RULE = 'StaleBaselineEntry'
SEVERITIES = ('error', 'warning', 'info')

EXIT_OK = 0
EXIT_FINDINGS = 1
EXIT_USAGE = 2


class AuditUsageError(Exception):
    """Invalid configuration, baseline or invocation; reported with exit code 2."""


@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    severity: str
    rule: str
    message: str
    code: str = ''


@dataclass(frozen=True)
class BaselineEntry:
    source: str
    source_line: int
    path: str
    rule: str
    code: str
    reason: str

    @property
    def is_directory(self) -> bool:
        return self.path.endswith('/')


@dataclass
class BaselineResult:
    findings: list[Finding]
    baselined: int
    stale: list[BaselineEntry]


@dataclass
class AuditContext:
    root: Path
    engine_root: Path
    scopes: list[str]
    excludes: list[str] = field(default_factory=list)
    baselines: list[Path] = field(default_factory=list)
    use_engine_baseline: bool = True
    config: dict = field(default_factory=dict)

    def display_path(self, path: Path) -> str:
        return display_path(path, self.root)

    def policy_key(self, path: Path) -> str:
        engine_relative = relative_path(path, self.engine_root / 'Source')

        if engine_relative is not None:
            return 'Source/' + engine_relative

        return PROJECT_KEY_PREFIX + self.display_path(path)

    def is_excluded(self, display: str) -> bool:
        """Exclude patterns are fnmatch globs on display paths; ``*`` also crosses ``/`` and ``dir/`` names a subtree."""

        for pattern in self.excludes:
            if pattern.endswith('/') and display.startswith(pattern):
                return True
            if fnmatch.fnmatchcase(display, pattern):
                return True

        return False

    def covers(self, display: str) -> bool:
        """Whether a baseline path lies completely inside the scanned scopes."""

        if self.is_excluded(display):
            return False

        target = display.rstrip('/')

        for scope in self.scopes:
            if scope == '.' or target == scope or target.startswith(scope + '/'):
                return True

        return False

    def iter_files(self, suffixes: frozenset[str] = NATIVE_SOURCE_EXTS) -> list[Path]:
        files: set[Path] = set()

        for scope in self.scopes:
            scope_path = self.root if scope == '.' else self.root / scope

            if scope_path.is_file():
                candidates = [scope_path]
            elif scope_path.is_dir():
                candidates = [item for item in scope_path.rglob('*') if item.is_file()]
            else:
                raise AuditUsageError(f'Scope does not exist: {scope}')

            for item in candidates:
                if item.suffix.lower() not in suffixes:
                    continue
                if self.is_excluded(self.display_path(item)):
                    continue

                files.add(item)

        return sorted(files, key=lambda item: self.display_path(item))


def safe_resolve(path: Path) -> Path:
    try:
        return path.resolve()
    except (OSError, RuntimeError, ValueError):
        return path.absolute()


def is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def absolute_path(path: Path) -> Path:
    return Path(os.path.abspath(path))


def relative_path(path: Path, root: Path) -> str | None:
    """POSIX path of *path* below *root*, comparing lexical absolute paths first and resolved paths second."""

    for candidate, parent in ((absolute_path(path), absolute_path(root)), (safe_resolve(path), safe_resolve(root))):
        if is_within(candidate, parent):
            relative = candidate.relative_to(parent).as_posix()
            return relative if relative else '.'

    return None


def display_path(path: Path, root: Path) -> str:
    relative = relative_path(path, root)
    return relative if relative is not None else absolute_path(path).as_posix()


def normalize_code(code: str) -> str:
    return ' '.join(code.split())


def normalize_scope(value: str, root: Path) -> str:
    text = value.replace('\\', '/')
    path = Path(text)
    relative = relative_path(path if path.is_absolute() else root / text, root)

    if relative is None or relative.split('/')[0] == '..':
        raise AuditUsageError(f'Scope is outside the root: {value}')

    return relative


def load_config(path: Path | None) -> dict:
    if path is None:
        return {}

    try:
        payload = json.loads(path.read_text(encoding='utf-8-sig'))
    except (OSError, ValueError) as ex:
        raise AuditUsageError(f'Unable to read audit configuration {path}: {ex}') from ex

    if not isinstance(payload, dict):
        raise AuditUsageError(f'{path}: configuration must be a JSON object')

    unknown = sorted(set(payload) - CONFIG_KEYS)

    if unknown:
        raise AuditUsageError(f'{path}: unknown configuration keys: {", ".join(unknown)}')
    if payload.get('schema_version') != CONFIG_SCHEMA_VERSION:
        raise AuditUsageError(f'{path}: schema_version must be {CONFIG_SCHEMA_VERSION}')

    for key in ('scopes', 'exclude'):
        if key in payload and not is_string_list(payload[key]):
            raise AuditUsageError(f'{path}: {key} must be a list of strings')

    baselines = payload.get('baselines', {})

    if not isinstance(baselines, dict) or not all(is_string_list(value) for value in baselines.values()):
        raise AuditUsageError(f'{path}: baselines must map an audit name to a list of baseline paths')
    if 'compilation_database' in payload and not isinstance(payload['compilation_database'], str):
        raise AuditUsageError(f'{path}: compilation_database must be a string')
    if 'smart_pointer' in payload and not isinstance(payload['smart_pointer'], dict):
        raise AuditUsageError(f'{path}: smart_pointer must be an object')

    return payload


def is_string_list(value: object) -> bool:
    return isinstance(value, list) and all(isinstance(item, str) and item for item in value)


def build_context(
    audit: str,
    *,
    root: Path | None,
    scopes: list[str],
    excludes: list[str],
    baselines: list[Path],
    config_path: Path | None,
    use_engine_baseline: bool,
    engine_root: Path | None = None,
) -> AuditContext:
    resolved_root = absolute_path(root if root is not None else Path.cwd())
    resolved_engine = absolute_path(engine_root if engine_root is not None else ENGINE_ROOT)
    config = load_config(None if config_path is None else relative_or_absolute(config_path, resolved_root))

    raw_scopes = scopes or config.get('scopes', [])

    if not raw_scopes:
        engine_source = relative_path(resolved_engine / 'Source', resolved_root)

        if engine_source is None:
            raise AuditUsageError('The Engine Source directory is outside --root; pass --scope explicitly')

        raw_scopes = [engine_source]

    normalized_scopes = sorted({normalize_scope(scope, resolved_root) for scope in raw_scopes})
    all_excludes = [pattern.replace('\\', '/') for pattern in [*config.get('exclude', []), *excludes]]
    config_baselines = [resolved_root / item for item in config.get('baselines', {}).get(audit, [])]
    all_baselines = [path if path.is_absolute() else resolved_root / path for path in [*config_baselines, *baselines]]

    return AuditContext(
        root=resolved_root,
        engine_root=resolved_engine,
        scopes=normalized_scopes,
        excludes=all_excludes,
        baselines=all_baselines,
        use_engine_baseline=use_engine_baseline,
        config=config,
    )


def engine_baseline_path(context: AuditContext, audit: str) -> Path:
    return context.engine_root / ENGINE_BASELINE_DIR / f'{audit}.tsv'


def read_baseline(path: Path, base: Path, context: AuditContext) -> list[BaselineEntry]:
    """Read one baseline file whose paths are relative to *base*; rows outside the root are dropped."""

    source = context.display_path(path)

    try:
        lines = path.read_text(encoding='utf-8-sig').splitlines()
    except (OSError, UnicodeDecodeError) as ex:
        raise AuditUsageError(f'Unable to read baseline {source}: {ex}') from ex

    if not lines or lines[0] != BASELINE_HEADER:
        raise AuditUsageError(f'{source}:1: expected TSV header: path<TAB>rule<TAB>code<TAB>reason')

    entries: list[BaselineEntry] = []
    seen: set[tuple[str, str, str]] = set()
    resolved_base = absolute_path(base)

    for line_no, line in enumerate(lines[1:], start=2):
        if not line.strip():
            continue

        parts = line.split('\t')

        if len(parts) != 4:
            raise AuditUsageError(f'{source}:{line_no}: expected four tab-separated columns')

        entry_path, rule, code, reason = (part.strip() for part in parts)

        if not entry_path or not rule or not code or not reason:
            raise AuditUsageError(f'{source}:{line_no}: every column, including the reason, must be non-empty')
        if '\\' in entry_path or entry_path.startswith('/') or ':' in entry_path or '..' in entry_path.split('/'):
            raise AuditUsageError(f'{source}:{line_no}: path must be a relative forward-slash path without ..')

        relative = relative_path(resolved_base / entry_path.rstrip('/'), context.root)

        if relative is None:
            continue

        display = relative + ('/' if entry_path.endswith('/') else '')
        normalized = BASELINE_ANY_CODE if code == BASELINE_ANY_CODE else normalize_code(code)
        key = (display, rule, normalized)

        if key in seen:
            raise AuditUsageError(f'{source}:{line_no}: duplicate baseline entry')

        seen.add(key)
        entries.append(BaselineEntry(source, line_no, display, rule, normalized, reason))

    return entries


def load_baselines(context: AuditContext, audit: str) -> list[BaselineEntry]:
    entries: list[BaselineEntry] = []

    if context.use_engine_baseline:
        engine_path = engine_baseline_path(context, audit)

        if engine_path.is_file():
            entries.extend(read_baseline(engine_path, context.engine_root, context))

    for path in context.baselines:
        if not path.is_file():
            raise AuditUsageError(f'Baseline file does not exist: {context.display_path(path)}')

        entries.extend(read_baseline(path, context.root, context))

    return entries


def entry_matches(entry: BaselineEntry, finding: Finding) -> bool:
    if entry.rule != finding.rule:
        return False
    if entry.is_directory:
        if not finding.path.startswith(entry.path):
            return False
    elif entry.path != finding.path:
        return False

    return entry.code == BASELINE_ANY_CODE or entry.code == normalize_code(finding.code)


def apply_baseline(findings: list[Finding], entries: list[BaselineEntry], context: AuditContext) -> BaselineResult:
    remaining: list[Finding] = []
    used: set[int] = set()
    baselined = 0

    for finding in findings:
        matched = [index for index, entry in enumerate(entries) if entry_matches(entry, finding)]

        if matched:
            used.update(matched)
            baselined += 1
            continue

        remaining.append(finding)

    stale = [entry for index, entry in enumerate(entries) if index not in used and context.covers(entry.path)]
    return BaselineResult(remaining, baselined, stale)


def stale_findings(stale: list[BaselineEntry]) -> list[Finding]:
    return [
        Finding(
            entry.source,
            entry.source_line,
            'error',
            STALE_BASELINE_RULE,
            f'Baseline entry no longer matches a finding; remove it: {entry.path}\t{entry.rule}\t{entry.code}',
        )
        for entry in stale
    ]


def should_fail(findings: list[Finding], fail_on: str) -> bool:
    if fail_on == 'none':
        return False
    if fail_on == 'warning':
        return any(finding.severity in {'error', 'warning'} for finding in findings)

    return any(finding.severity == 'error' for finding in findings)


def sort_findings(findings: list[Finding]) -> list[Finding]:
    return sorted(findings, key=lambda item: (item.path, item.line, item.rule, item.message))


def write_report(path: Path, findings: list[Finding]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open('w', encoding='utf-8', newline='\n') as file:
        file.write('path\tline\tseverity\trule\tcode\tmessage\n')

        for finding in findings:
            code = normalize_code(finding.code)
            file.write(f'{finding.path}\t{finding.line}\t{finding.severity}\t{finding.rule}\t{code}\t{finding.message}\n')


def baseline_rows(findings: list[Finding]) -> list[str]:
    """Candidate baseline rows with an empty reason column that a maintainer must fill in."""

    rows = {f'{finding.path}\t{finding.rule}\t{normalize_code(finding.code) or BASELINE_ANY_CODE}\t' for finding in findings}
    return [BASELINE_HEADER, *sorted(rows)]


def print_summary(name: str, files: int, result: BaselineResult, findings: list[Finding], limit: int) -> None:
    severity_counts = Counter(finding.severity for finding in findings)
    rule_counts = Counter(finding.rule for finding in findings)
    counts = ' '.join(f'{severity}={severity_counts[severity]}' for severity in SEVERITIES)

    print(f'{name}: audited {files} files, baselined {result.baselined} findings, stale baseline entries {len(result.stale)}')
    print(f'{name}: {counts}')

    for rule, count in sorted(rule_counts.items()):
        print(f'{name}: {rule}={count}')

    shown = findings if limit <= 0 else findings[:limit]

    for finding in shown:
        print(f'{finding.path}:{finding.line}: {finding.severity}: {finding.rule}: {finding.message}')

    if len(shown) < len(findings):
        print(f'{name}: {len(findings) - len(shown)} more findings; pass --report or --max-printed 0')


def relative_or_absolute(path: Path, root: Path) -> Path:
    return path if path.is_absolute() else root / path


def finish_audit(args: argparse.Namespace, context: AuditContext, name: str, files: int, findings: list[Finding], fail_on: str) -> tuple[int, list[Finding]]:
    """Apply baselines, print and write results, and choose the exit code shared by every audit."""

    entries = load_baselines(context, args.audit)
    result = apply_baseline(sort_findings(findings), entries, context)
    reported = sort_findings(result.findings + stale_findings(result.stale))

    if args.report is not None:
        write_report(relative_or_absolute(args.report, context.root), reported)

    if args.emit_baseline is not None:
        path = relative_or_absolute(args.emit_baseline, context.root)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('\n'.join(baseline_rows(result.findings)) + '\n', encoding='utf-8', newline='\n')

    print_summary(name, files, result, reported, args.max_printed)

    if result.stale or should_fail(result.findings, fail_on):
        return EXIT_FINDINGS, result.findings

    return EXIT_OK, result.findings
