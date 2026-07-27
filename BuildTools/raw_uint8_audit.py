#!/usr/bin/env python3
"""Audit first-party C++ uint8_t containers against a reviewed numeric/ABI baseline.

Arbitrary storage belongs in byte containers. Numeric 0...255 sequences and
fixed third-party/script ABI containers remain uint8_t, but every such source
declaration is pinned by path, count, and a hash of its normalized source line.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from text_type_audit import DEFAULT_EXCLUDES, iter_source_files, mask_non_code


RAW_UINT8_PATTERN = re.compile(
    r"\b(?:(?:std::)?(?:vector|readonly_vector|span|const_span|array|deque|list|set|unordered_set|unique_ptr)\s*<\s*(?:const\s+)?uint8_t\b|uint8_t\s+[A-Za-z_]\w*\s*\[)"
)
DEFAULT_ALLOWLIST = Path(__file__).with_name("raw_uint8_allowlist.tsv")


@dataclass(frozen=True)
class Occurrence:
    path: str
    line: int
    column: int
    preview: str

    def diagnostic(self) -> str:
        return f"{self.path}:{self.line}:{self.column}: error: uint8_t container is not in the reviewed numeric/ABI baseline: {self.preview} [raw_uint8_container]"


@dataclass(frozen=True)
class BaselineEntry:
    count: int
    digest: str
    reason: str


def _canonical_path(path: Path) -> str:
    parts = path.as_posix().split("/")
    return "/".join(parts[1:]) if parts and parts[0] == "Engine" else "/".join(parts)


def _line_column(text: str, offset: int) -> tuple[int, int]:
    line = text.count("\n", 0, offset) + 1
    previous_newline = text.rfind("\n", 0, offset)
    return line, offset - previous_newline


def _line_preview(text: str, offset: int) -> str:
    line_start = text.rfind("\n", 0, offset) + 1
    line_end = text.find("\n", offset)

    if line_end == -1:
        line_end = len(text)

    return " ".join(text[line_start:line_end].strip().split())


def scan_text(text: str, path: Path) -> list[Occurrence]:
    masked = mask_non_code(text)
    canonical_path = _canonical_path(path)
    occurrences: list[Occurrence] = []

    for match in RAW_UINT8_PATTERN.finditer(masked):
        line, column = _line_column(text, match.start())
        occurrences.append(Occurrence(path=canonical_path, line=line, column=column, preview=_line_preview(text, match.start())))

    return occurrences


def _digest(occurrences: Iterable[Occurrence]) -> str:
    payload = "\n".join(sorted(occurrence.preview for occurrence in occurrences))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def load_allowlist(path: Path) -> tuple[dict[str, BaselineEntry], list[str]]:
    entries: dict[str, BaselineEntry] = {}
    errors: list[str] = []

    if not path.is_file():
        return {}, [f"{path.as_posix()}:1:1: error: raw uint8_t allowlist not found [raw_uint8_allowlist_missing]"]

    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not raw_line or raw_line.startswith("#"):
            continue

        columns = raw_line.split("\t")

        if len(columns) != 4:
            errors.append(f"{path.as_posix()}:{line_number}:1: error: expected four tab-separated columns [raw_uint8_allowlist_format]")
            continue

        source_path, count_text, digest, reason = columns

        try:
            count = int(count_text)
        except ValueError:
            errors.append(f"{path.as_posix()}:{line_number}:1: error: invalid occurrence count {count_text!r} [raw_uint8_allowlist_format]")
            continue

        if source_path in entries:
            errors.append(f"{path.as_posix()}:{line_number}:1: error: duplicate source path {source_path!r} [raw_uint8_allowlist_duplicate]")
            continue
        if count <= 0 or not re.fullmatch(r"[0-9a-f]{64}", digest) or not reason:
            errors.append(f"{path.as_posix()}:{line_number}:1: error: invalid count, sha256, or reason [raw_uint8_allowlist_format]")
            continue

        entries[source_path] = BaselineEntry(count=count, digest=digest, reason=reason)

    return entries, errors


def collect(root: Path, scopes: Iterable[str], excludes: Iterable[str]) -> tuple[list[Occurrence], set[str], int, list[str]]:
    occurrences: list[Occurrence] = []
    scanned_paths: set[str] = set()
    errors: list[str] = []
    file_count = 0

    for path, relative_path in iter_source_files(root, scopes, excludes):
        file_count += 1
        canonical_path = _canonical_path(relative_path)
        scanned_paths.add(canonical_path)

        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as ex:
            errors.append(f"{relative_path.as_posix()}:1:1: error: source file is not valid UTF-8: {ex} [invalid_source_utf8]")
            continue

        occurrences.extend(scan_text(text, relative_path))

    return occurrences, scanned_paths, file_count, errors


def validate_baseline(
    occurrences: list[Occurrence], scanned_paths: set[str], baseline: dict[str, BaselineEntry], scope_prefixes: Iterable[str] = ()
) -> tuple[list[Occurrence], list[str]]:
    by_path: dict[str, list[Occurrence]] = {}
    canonical_prefixes = tuple(prefix.rstrip("/") for prefix in scope_prefixes)

    for occurrence in occurrences:
        by_path.setdefault(occurrence.path, []).append(occurrence)

    findings: list[Occurrence] = []
    errors: list[str] = []

    for path, path_occurrences in sorted(by_path.items()):
        expected = baseline.get(path)

        if expected is None:
            findings.extend(path_occurrences)
            continue

        actual_count = len(path_occurrences)
        actual_digest = _digest(path_occurrences)

        if actual_count != expected.count or actual_digest != expected.digest:
            errors.append(
                f"{path}:1:1: error: reviewed uint8_t baseline changed; expected count/hash {expected.count}/{expected.digest}, got {actual_count}/{actual_digest} [raw_uint8_baseline_changed]"
            )

    for path in sorted(baseline):
        path_is_in_scope = path in scanned_paths or any(path == prefix or path.startswith(f"{prefix}/") for prefix in canonical_prefixes)

        if path_is_in_scope and path not in by_path:
            errors.append(f"{path}:1:1: error: stale raw uint8_t allowlist entry [raw_uint8_allowlist_stale]")

    return findings, errors


def dump_baseline(occurrences: list[Occurrence]) -> None:
    by_path: dict[str, list[Occurrence]] = {}

    for occurrence in occurrences:
        by_path.setdefault(occurrence.path, []).append(occurrence)

    print("# path\tcount\tsha256(normalized declarations)\treview reason")

    for path, path_occurrences in sorted(by_path.items()):
        print(f"{path}\t{len(path_occurrences)}\t{_digest(path_occurrences)}\tREVIEW")


def _run_self_test() -> None:
    sample = r'''
// vector<uint8_t> ignored_comment;
auto ignored_literal = "span<uint8_t>";
uint8_t scalar = 1;
vector<uint8_t> values;
span<const uint8_t> view;
const_span<uint8_t> const_view;
array<uint8_t, 4> channels;
std::unique_ptr<uint8_t[]> external;
uint8_t raw_array[8];
readonly_vector<uint8_t> script_view;
'''
    occurrences = scan_text(sample, Path("Engine/Source/Sample.cpp"))
    assert len(occurrences) == 7
    assert {occurrence.line for occurrence in occurrences} == {5, 6, 7, 8, 9, 10, 11}
    digest = _digest(occurrences)
    baseline = {"Source/Sample.cpp": BaselineEntry(count=7, digest=digest, reason="self-test")}
    findings, errors = validate_baseline(occurrences, {"Source/Sample.cpp"}, baseline)
    assert not findings
    assert not errors

    changed_baseline = {"Source/Sample.cpp": BaselineEntry(count=5, digest=digest, reason="self-test")}
    _, changed_errors = validate_baseline(occurrences, {"Source/Sample.cpp"}, changed_baseline)
    assert len(changed_errors) == 1

    deleted_baseline = {"Source/Deleted.cpp": BaselineEntry(count=1, digest="0" * 64, reason="self-test")}
    _, deleted_errors = validate_baseline([], set(), deleted_baseline, {"Source"})
    assert len(deleted_errors) == 1


def _default_scopes(root: Path) -> list[str]:
    if (root / "Engine/Source").is_dir():
        return ["Engine/Source", "SourceExt"]
    if (root / "Source").is_dir():
        return ["Source"]
    return []


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="Repository or embedding-project root")
    parser.add_argument("--scope", action="append", default=[], help="Source directory relative to --root; repeatable")
    parser.add_argument("--exclude", action="append", default=[], help="Excluded path prefix relative to --root; repeatable")
    parser.add_argument("--allowlist", type=Path, default=DEFAULT_ALLOWLIST, help="Reviewed uint8_t baseline TSV")
    parser.add_argument("--dump-baseline", action="store_true", help="Print the current baseline without validating the allowlist")
    parser.add_argument("--self-test", action="store_true", help="Run the scanner's built-in regression tests")
    args = parser.parse_args(argv)

    if args.self_test:
        _run_self_test()
        print("Raw uint8_t audit self-test passed")
        return 0

    root = args.root.resolve()
    scopes = args.scope or _default_scopes(root)

    if not scopes:
        parser.error("no source scopes found; pass --scope")

    occurrences, scanned_paths, file_count, errors = collect(root, scopes, (*DEFAULT_EXCLUDES, *args.exclude))

    if args.dump_baseline:
        for error in errors:
            print(error)
        dump_baseline(occurrences)
        return 1 if errors else 0

    baseline, allowlist_errors = load_allowlist(args.allowlist)
    scope_prefixes = {_canonical_path(Path(scope)) for scope in scopes}
    findings, baseline_errors = validate_baseline(occurrences, scanned_paths, baseline, scope_prefixes)

    for diagnostic in (*errors, *allowlist_errors, *baseline_errors):
        print(diagnostic)
    for finding in findings:
        print(finding.diagnostic())

    error_count = len(errors) + len(allowlist_errors) + len(baseline_errors)

    if error_count or findings:
        print(f"Raw uint8_t audit failed: {file_count} files, {len(findings)} unreviewed declaration(s), {error_count} audit error(s)")
        return 1

    print(f"Raw uint8_t audit passed: {file_count} files, {len(occurrences)} reviewed numeric/ABI declaration(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
