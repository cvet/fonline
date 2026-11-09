"""Count-ratchet baselines shared by the read-only source checks.

A baseline is a tab-separated file with the exact header ``rule``, ``path``,
``count`` and ``reason``.  Each row accepts up to ``count`` findings of one rule
in one root-relative file and must state why they remain.  A file that holds
more findings than its row allows fails, and so does a row that allows more
than remain: counts may only go down, and every reduction is recorded by
lowering or deleting the row.  Several baseline files may be combined, for
example an Engine file and a project file beside it; a key may appear in only
one of them.  Lines starting with ``#`` and blank lines are ignored.
"""

from __future__ import annotations

from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path


HEADER = ("rule", "path", "count", "reason")


@dataclass(frozen=True)
class Finding:
    rule: str
    path: str
    line: int
    message: str


@dataclass(frozen=True)
class BaselineRow:
    rule: str
    path: str
    count: int
    reason: str
    source: str


class BaselineError(Exception):
    """A baseline file is unreadable or violates the format contract."""


def format_finding(finding: Finding) -> str:
    return f"{finding.path}:{finding.line}: [{finding.rule}] {finding.message}"


def sort_findings(findings: Iterable[Finding]) -> list[Finding]:
    return sorted(findings, key=lambda item: (item.path, item.line, item.rule, item.message))


def load_baselines(paths: Sequence[Path]) -> dict[tuple[str, str], BaselineRow]:
    """Read every baseline file and return its rows keyed by ``(rule, path)``."""

    rows: dict[tuple[str, str], BaselineRow] = {}

    for path in paths:
        for row in _read_baseline(Path(path)):
            key = (row.rule, row.path)
            previous = rows.get(key)

            if previous is not None:
                raise BaselineError(
                    f"{row.source}: duplicate baseline key {row.rule} {row.path}, "
                    f"first declared at {previous.source}")

            rows[key] = row

    return rows


def apply_baseline(findings: Iterable[Finding],
                   rows: dict[tuple[str, str], BaselineRow]) -> tuple[list[Finding], list[str]]:
    """Return findings above their baseline and diagnostics for stale rows."""

    grouped: dict[tuple[str, str], list[Finding]] = {}

    for finding in findings:
        grouped.setdefault((finding.rule, finding.path), []).append(finding)

    remaining: list[Finding] = []
    stale: list[str] = []

    for key, group in sorted(grouped.items()):
        row = rows.get(key)
        allowed = row.count if row is not None else 0

        if len(group) > allowed:
            remaining.extend(group)

    for key, row in sorted(rows.items()):
        found = len(grouped.get(key, ()))

        if found == 0:
            stale.append(f"{row.source}: baseline row {row.rule} {row.path} matches no finding; delete it")
        elif found < row.count:
            stale.append(
                f"{row.source}: baseline row {row.rule} {row.path} allows {row.count} "
                f"but {found} remain; lower the count to {found}")

    return sort_findings(remaining), stale


def write_baseline(path: Path, findings: Iterable[Finding]) -> int:
    """Write the current counts with empty reasons and return the row count.

    The written file is a draft: loading rejects empty reasons, so every row
    must be justified before the baseline is accepted.
    """

    counts: dict[tuple[str, str], int] = {}

    for finding in findings:
        key = (finding.rule, finding.path)
        counts[key] = counts.get(key, 0) + 1

    lines = ["\t".join(HEADER)]
    lines.extend(f"{rule}\t{rel}\t{count}\t" for (rule, rel), count in sorted(counts.items()))

    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    return len(counts)


def _read_baseline(path: Path) -> list[BaselineRow]:
    try:
        text = path.read_bytes().decode("utf-8-sig")
    except (OSError, UnicodeDecodeError) as error:
        raise BaselineError(f"{path}: unable to read baseline as UTF-8: {error}") from error

    rows: list[BaselineRow] = []
    header_seen = False

    for number, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.rstrip("\r")
        source = f"{path.as_posix()}:{number}"

        if not line.strip() or line.startswith("#"):
            continue

        columns = line.split("\t")

        if not header_seen:
            if tuple(columns) != HEADER:
                raise BaselineError(f"{source}: baseline header must be {' / '.join(HEADER)} separated by tabs")

            header_seen = True
            continue

        if len(columns) != len(HEADER):
            raise BaselineError(f"{source}: baseline row must have {len(HEADER)} tab-separated columns")

        rule, rel, count_text, reason = (column.strip() for column in columns)

        if not rule or not rel:
            raise BaselineError(f"{source}: baseline row needs a rule and a path")

        if "\\" in rel or any(char in rel for char in "*?[") or rel.startswith("/") or ".." in rel.split("/"):
            raise BaselineError(f"{source}: baseline path must be an exact root-relative POSIX path: {rel}")

        if not count_text.isdigit() or int(count_text) < 1:
            raise BaselineError(f"{source}: baseline count must be a positive integer: {count_text}")

        if not reason:
            raise BaselineError(f"{source}: baseline row {rule} {rel} needs a written reason")

        rows.append(BaselineRow(rule, rel, int(count_text), reason, source))

    if not header_seen:
        raise BaselineError(f"{path.as_posix()}: baseline header is missing")

    return rows
