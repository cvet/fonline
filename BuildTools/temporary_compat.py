#!/usr/bin/python3

"""Fails once code marked as temporary compatibility has outlived its date.

Native code is marked with FO_TEMPORARY_COMPAT(Id, "YYYY-MM-DD"); managed code with [TemporaryCompat("Id", "YYYY-MM-DD")].
Every place of one compatibility repeats the same id and date, the last day the code may exist. Before the date nothing
is reported; from the day after the check fails and lists every place to delete. Without arguments it scans the engine
Source tree; an embedding project passes its own directories, the engine's among them.
"""

from __future__ import annotations

import argparse
import datetime
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ENGINE_SOURCE = Path(__file__).resolve().parents[1] / "Source"
EXTENSIONS = {".h", ".hpp", ".cpp", ".inl", ".cs"}

# A date further ahead would let a marker keep its code for good, which is what the marker exists to prevent
MAX_HORIZON_DAYS = 366

NATIVE_RE = re.compile(r"\bFO_TEMPORARY_COMPAT\s*\(")
MANAGED_RE = re.compile(r"\bTemporaryCompat(?:Attribute)?\s*\(")
NATIVE_ARGS_RE = re.compile(r'\(\s*(?P<id>[A-Za-z]\w*)\s*,\s*"(?P<until>[^"\n]*)"\s*\)')
MANAGED_ARGS_RE = re.compile(r'\(\s*"(?P<id>[A-Za-z]\w*)"\s*,\s*"(?P<until>[^"\n]*)"\s*\)')
DATE_RE = re.compile(r"\d{4}-\d{2}-\d{2}")


@dataclass(frozen=True)
class Marker:
    path: str
    line: int
    id: str
    until: datetime.date


@dataclass(frozen=True)
class Issue:
    path: str
    line: int
    message: str


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dirs", nargs="*", type=Path, default=[ENGINE_SOURCE], help="directories to scan (default: the engine Source tree)")
    parser.add_argument("--today", type=datetime.date.fromisoformat, default=datetime.date.today())
    parser.add_argument("--report", action="store_true", help="list every marked compatibility and its date")
    args = parser.parse_args()

    markers, issues = collect(args.dirs)
    issues += check(markers, args.today)
    groups = group_by_id(markers)

    if args.report:
        for compat_id, group in groups.items():
            days_left = (group[0].until - args.today).days
            print(f"{compat_id}: until {group[0].until.isoformat()} ({days_left} day(s) left)")

            for marker in group:
                print(f"  {marker.path}:{marker.line}")

    for issue in issues:
        print(f"error: {issue.path}:{issue.line}: {issue.message}", file=sys.stderr)

    print(f"temporary compat check: {len(markers)} marker(s) in {len(groups)} compatibilit(ies), {len(issues)} error(s)")
    return 1 if issues else 0


def collect(dirs: list[Path]) -> tuple[list[Marker], list[Issue]]:
    markers: list[Marker] = []
    issues: list[Issue] = []

    for scan_dir in dirs:
        for path in sorted(scan_dir.rglob("*")):
            if path.suffix not in EXTENSIONS or not path.is_file():
                continue

            text = path.read_text(encoding="utf-8", errors="replace")

            # Only a file naming a marker is parsed, so large vendored trees cost a substring search
            if "FO_TEMPORARY_COMPAT" not in text and "TemporaryCompat" not in text:
                continue

            found, problems = find_markers(display_path(path), text)
            markers += found
            issues += problems

    return markers, issues


def check(markers: list[Marker], today: datetime.date) -> list[Issue]:
    issues: list[Issue] = []

    for compat_id, group in group_by_id(markers).items():
        dates = sorted({marker.until for marker in group})
        first = group[0]

        if len(dates) != 1:
            places = ", ".join(f"{marker.until.isoformat()} at {marker.path}:{marker.line}" for marker in group)
            issues.append(Issue(first.path, first.line, f"temporary compatibility {compat_id} carries different dates ({places}); every place of one compatibility states the same date"))
            continue

        until = dates[0]

        if today > until:
            places = "".join(f"\n  {marker.path}:{marker.line}" for marker in group)
            issues.append(Issue(first.path, first.line, f"temporary compatibility {compat_id} expired after {until.isoformat()}: delete the code at every place below, or move the date in a reviewed change{places}"))
        elif (until - today).days > MAX_HORIZON_DAYS:
            issues.append(Issue(first.path, first.line, f"temporary compatibility {compat_id} is dated {until.isoformat()}, more than {MAX_HORIZON_DAYS} days ahead; set a nearer date and move it again if still needed"))

    return issues


def find_markers(rel_path: str, text: str) -> tuple[list[Marker], list[Issue]]:
    code = mask_comments(text)
    markers: list[Marker] = []
    issues: list[Issue] = []

    for pattern, args_re, native in ((NATIVE_RE, NATIVE_ARGS_RE, True), (MANAGED_RE, MANAGED_ARGS_RE, False)):
        for match in pattern.finditer(code):
            line_start = code.rfind("\n", 0, match.start()) + 1

            # The macro definition and the attribute constructor are not markers; an attribute follows '[' or ','
            if native and code[line_start:match.start()].lstrip().startswith("#"):
                continue
            if not native and not code[:match.start()].rstrip().endswith(("[", ",")):
                continue

            line = code.count("\n", 0, match.start()) + 1
            args = args_re.match(code, match.end() - 1)
            spelling = 'FO_TEMPORARY_COMPAT(Id, "YYYY-MM-DD")' if native else '[TemporaryCompat("Id", "YYYY-MM-DD")]'

            if args is None:
                issues.append(Issue(rel_path, line, f"temporary compatibility marker is not written as {spelling}"))
                continue

            until = parse_date(args.group("until"))

            if until is None:
                issues.append(Issue(rel_path, line, f"temporary compatibility {args.group('id')} has no valid date: {args.group('until')!r}"))
                continue

            markers.append(Marker(rel_path, line, args.group("id"), until))

    return markers, issues


def parse_date(text: str) -> datetime.date | None:
    if not DATE_RE.fullmatch(text):
        return None

    try:
        return datetime.date.fromisoformat(text)
    except ValueError:
        return None


def display_path(path: Path) -> str:
    try:
        return Path(os.path.relpath(path)).as_posix()
    except ValueError:
        return path.as_posix()


def group_by_id(markers: list[Marker]) -> dict[str, list[Marker]]:
    groups: dict[str, list[Marker]] = {}

    for marker in sorted(markers, key=lambda marker: (marker.id, marker.path, marker.line)):
        groups.setdefault(marker.id, []).append(marker)

    return groups


def mask_comments(text: str) -> str:
    """Blanks out comments, keeping string literals, every offset and every newline in place."""

    chars = list(text)
    index = 0
    state = "code"

    while index < len(chars):
        char = chars[index]
        next_char = chars[index + 1] if index + 1 < len(chars) else ""

        if state == "code":
            if char == "/" and next_char == "/":
                state = "line_comment"
                continue
            if char == "/" and next_char == "*":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "block_comment"
                continue
            if char in ('"', "'"):
                state = char
        elif state == "line_comment":
            if char == "\n":
                state = "code"
            else:
                chars[index] = " "
        elif state == "block_comment":
            if char == "*" and next_char == "/":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "code"
                continue
            if char != "\n":
                chars[index] = " "
        else:
            if char == "\\":
                index += 2
                continue
            if char == state or char == "\n":
                state = "code"

        index += 1

    return "".join(chars)


if __name__ == "__main__":
    raise SystemExit(main())
