"""Validate the native source layout contract for engine-owned C++ sources.

The check is read-only.  It validates that every non-bootstrap header starts
with ``Common.h`` (with the exact bootstrap and standalone sample boundaries),
that an implementation starts with its case-exact same-directory header when
one exists and with ``Common.h`` otherwise, that the first include is separated
from following content by a blank line, and that every checked file ends with
exactly one final line ending and no trailing spaces.

Comments, ordinary C++ string literals and raw string literals are screened
lexically so include-looking text inside them is not treated as a directive.
Preprocessor conditions are not evaluated: the lexical first include wins.

With ``--comments`` the check also applies the native comment rules: a comment
block carries at most two lines of prose, no tracker IDs, dates, version
markers or author/history/benchmark bookkeeping, and only its concluding
sentence omits the final period.  License banners, commented-out code, tool
directives and value-layout field labels are exempt.  Comment findings accept
``--comment-baseline`` files (see ``finding_baseline.py``); without
``--comments`` the output is unchanged.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import sys

from finding_baseline import (BaselineError, Finding, apply_baseline, format_finding, load_baselines,
                              sort_findings, write_baseline)


_GENERATOR_INPUTS = frozenset({
    "Source/Common/GenericCode.template.cpp",
    "Source/Common/MetadataRegistration.template.cpp",
})
_NATIVE_SUFFIXES = frozenset({".h", ".cpp"})
_STANDALONE_SAMPLE_ROOT = "Examples/NativeExtensionSample/"
_STANDALONE_SAMPLE_HEADER = "SourceExt/NativeExtensionCore.h"
_STANDALONE_SAMPLE_TEST = "Tests/NativeExtensionCoreTest.cpp"
_INCLUDE_RE = re.compile(r"^[ \t]*#[ \t]*include\b(?P<rest>.*)\Z")
_INCLUDE_PREFIX_RE = re.compile(r"[ \t]*#[ \t]*include[ \t]*\Z")
_EOF_TAIL_RE = re.compile(r"(?P<eols>(?:\r\n|\n)+)\Z")
_EOL_RE = re.compile(r"\r\n|\n")
_RAW_LITERAL_RE = re.compile(
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^()\\\s]{0,16})\(')
_DRIVE_RE = re.compile(r"^[A-Za-z]:")
_STRING_TOKEN = "\x00"
_MAX_COMMENT_PROSE_LINES = 2
_CODE_TAIL_RE = re.compile(r"[;{}()\[\]=,>]\s*$")
_DISABLED_TAG_RE = re.compile(
    r"^(?:/\w*|@)\s*(?:Export\w+|EngineHook|Event|RemoteCall|Property|Setting|Enum|MigrationRule|ApiContract"
    r"|Component|RefType|FixedType|EntityHolder)\b")
_DIRECTIVE_RE = re.compile(
    r"(?:clang-format\s+(?:on|off)|NOLINT|ReSharper\s|cppcheck-suppress|IWYU\s+pragma|NOSONAR)")
_SEPARATOR_RE = re.compile(r"[-=*_#~+/.\s]*")
_LICENSE_RE = re.compile(r"SPDX-License-Identifier|\bcopyright\b|\blicensed under\b|\bMIT License\b",
                         re.IGNORECASE)
_FIELD_LABEL_RE = re.compile(r"([A-Za-z_]\w*):\s*\S.*")
_LAYOUT_RE = re.compile(r"\bLayout\s*=\s*([A-Za-z0-9_+\-]+)")
_LAYOUT_FIELD_RE = re.compile(r"-([A-Za-z_]\w*)")
_STANDARD_PREFIXES = (
    "AES", "ASTC", "BC", "BT", "CRC", "DXT", "ECMA", "ETC", "FNV", "IEC", "IEEE", "ISO", "MD", "RFC", "SHA",
    "UCS", "UTF", "X",
)
_MONTHS = ("January|February|March|April|May|June|July|August|September|October|November|December"
           "|Jan|Feb|Mar|Apr|Jun|Jul|Aug|Sep|Sept|Oct|Nov|Dec")
_BOOKKEEPING_PATTERNS = (
    ("tracker ID", re.compile(r"(?<![\w&#/])#\d{2,}\b")),
    ("tracker ID", re.compile(r"\b(?:issue|bug|ticket|pull request|merge request)\s*#?\d+\b|\bPR\s*#?\d+\b",
                              re.IGNORECASE)),
    ("tracker ID", re.compile(rf"\b(?!(?:{'|'.join(_STANDARD_PREFIXES)})-)[A-Z][A-Z0-9]{{1,15}}-\d+(?:-\d+)*\b")),
    ("date", re.compile(r"\b(?:19|20)\d{2}[-/.](?:0[1-9]|1[0-2])[-/.](?:0[1-9]|[12]\d|3[01])\b")),
    ("date", re.compile(r"\b(?:0?[1-9]|[12]\d|3[01])[./](?:0?[1-9]|1[0-2])[./](?:19|20)\d{2}\b")),
    ("date", re.compile(rf"\b(?:{_MONTHS})\.?\s+(?:\d{{1,2}},?\s+)?(?:19|20)\d{{2}}\b")),
    ("version marker", re.compile(
        r"\b(?:since|as of|until|added in|removed in|fixed in|changed in|introduced in|deprecated in)\s+"
        r"(?:v|version\s+)?\d+(?:\.\d+)+", re.IGNORECASE)),
    ("version marker", re.compile(r"\bv\d+\.\d+(?:\.\d+)*\b")),
    ("author note", re.compile(
        r"^(?:author|authors|written by|created by|modified by|changed by|fixed by|maintainer)\s*:|@author\b",
        re.IGNORECASE)),
    ("history note", re.compile(
        r"\b(?:was previously|used to be|formerly|no longer needed|kept for (?:history|reference)"
        r"|for historical reasons|(?:old|previous) (?:implementation|version|code))\b", re.IGNORECASE)),
    ("benchmark note", re.compile(
        r"\b\d+(?:\.\d+)?\s*(?:x|times|%)\s+(?:faster|slower)\b"
        r"|\b(?:benchmark(?:ed|s)?|measured)\b[^.;]*?\b\d+(?:\.\d+)?\s*(?:ms|us|ns|fps|x|%)(?!\w)",
        re.IGNORECASE)),
)


COMMENT_RULES = ("comment-length", "comment-bookkeeping", "comment-period")


def check_layout(root: Path, source_dir: str = "Source", shallow: bool = False) -> list[str]:
    """Validate the native source layout under *root* and return sorted diagnostics."""

    root_path, inventory, issues = _collect_inventory(root, source_dir, shallow)

    if inventory is None:
        return sorted(issues)

    inventory_set = set(inventory)
    for rel in inventory:
        text = _read_utf8(root_path / rel)

        if text is None:
            issues.add(f"{rel}: unable to read file as UTF-8")
            continue

        issues.update(_check_native_file(rel, text, inventory_set))

    return sorted(issues)


def check_comments(root: Path, source_dir: str = "Source",
                   shallow: bool = False) -> tuple[list[Finding], list[str]]:
    """Apply the native comment rules and return findings plus scope diagnostics."""

    root_path, inventory, issues = _collect_inventory(root, source_dir, shallow)

    if inventory is None:
        return [], sorted(issues)

    findings: list[Finding] = []
    for rel in inventory:
        text = _read_utf8(root_path / rel)

        if text is None:
            issues.add(f"{rel}: unable to read file as UTF-8")
            continue

        findings.extend(check_comment_text(rel, text))

    return sort_findings(findings), sorted(issues)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                        help="engine repository root to check")
    parser.add_argument("--source-dir", default="Source",
                        help="root-relative native source directory (default: Source)")
    parser.add_argument("--shallow", action="store_true",
                        help="check only direct children of the source directory")
    parser.add_argument("--comments", action="store_true",
                        help="also apply the native comment rules")
    parser.add_argument("--comment-baseline", action="append", type=Path, default=[],
                        help="accepted comment findings (rule, path, count, reason TSV); repeatable")
    parser.add_argument("--write-comment-baseline", type=Path,
                        help="write current comment finding counts with empty reasons instead of gating")
    parser.add_argument("--comments-report-only", action="store_true",
                        help="print comment findings without failing on them")
    args = parser.parse_args(argv)

    comment_options = (args.comment_baseline or args.write_comment_baseline is not None
                       or args.comments_report_only)

    if comment_options and not args.comments:
        parser.error("comment baseline and report options require --comments")

    issues = check_layout(args.root, source_dir=args.source_dir, shallow=args.shallow)
    for issue in issues:
        print(issue)

    if not args.comments:
        return 1 if issues else 0

    findings, scope_issues = check_comments(args.root, source_dir=args.source_dir, shallow=args.shallow)

    for issue in scope_issues:
        if issue not in issues:
            print(issue)

    failed_layout = bool(issues or scope_issues)

    if args.write_comment_baseline is not None:
        rows = write_baseline(args.write_comment_baseline, findings)
        print(f"wrote {rows} comment baseline row(s) to {args.write_comment_baseline}; "
              f"add a reason to every row", file=sys.stderr)
        return 1 if failed_layout else 0

    try:
        baseline = load_baselines(args.comment_baseline)
    except BaselineError as error:
        print(error)
        return 1

    remaining, stale = apply_baseline(findings, baseline)
    for finding in remaining:
        print(format_finding(finding))

    for diagnostic in stale:
        print(diagnostic)

    print(f"native comments: {len(findings)} finding(s), {len(remaining)} above the baseline, "
          f"{len(stale)} stale baseline row(s)", file=sys.stderr)

    if failed_layout:
        return 1

    if args.comments_report_only:
        return 0

    return 1 if remaining or stale else 0


def _collect_inventory(root: Path, source_dir: str,
                       shallow: bool) -> tuple[Path, list[str] | None, set[str]]:
    source_rel = _normalize_source_dir(source_dir)
    root_path = _resolved_root(root)

    if source_rel is None:
        return root_path, None, {
            "source_dir: source directory must be a non-empty root-relative "
            "path without '..' segments",
        }

    source_path = root_path if source_rel == "." else root_path.joinpath(*source_rel.split("/"))

    try:
        source_exists = source_path.exists()
        source_is_dir = source_path.is_dir()
    except (OSError, RuntimeError, ValueError):
        source_exists = False
        source_is_dir = False

    if not source_exists:
        return root_path, None, {f"{source_rel}: native source directory does not exist"}

    if not source_is_dir:
        return root_path, None, {f"{source_rel}: native source path is not a directory"}

    try:
        resolved_source = source_path.resolve()
    except (OSError, RuntimeError, ValueError):
        resolved_source = source_path

    if not _is_within(resolved_source, root_path):
        return root_path, None, {f"{source_rel}: source directory resolves outside the root"}

    files, issues = _scan_native_files(source_path, root_path, shallow)
    inventory = sorted(path for path in files if path not in _GENERATOR_INPUTS)

    if not inventory:
        issues.add(f"{source_rel}: native source inventory is empty")
        return root_path, None, issues

    return root_path, inventory, issues


def _normalize_source_dir(source_dir: str) -> str | None:
    if isinstance(source_dir, os.PathLike):
        source_dir = os.fspath(source_dir)

    if not isinstance(source_dir, str) or source_dir == "" or "\x00" in source_dir:
        return None

    text = source_dir.replace("\\", "/")

    if text.startswith("/") or _DRIVE_RE.match(text):
        return None

    parts: list[str] = []
    for part in text.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            return None
        parts.append(part)

    return "/".join(parts) if parts else "."


def _resolved_root(root: Path) -> Path:
    root_path = Path(root)

    try:
        return root_path.resolve()
    except (OSError, RuntimeError, ValueError):
        return root_path.absolute()


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _scan_native_files(source_path: Path, root_path: Path,
                       shallow: bool) -> tuple[set[str], set[str]]:
    files: set[str] = set()
    issues: set[str] = set()
    visited_directories = {_safe_resolve(source_path)}
    pending = [source_path]

    while pending:
        directory = pending.pop()

        try:
            with os.scandir(directory) as scanner:
                entries = sorted(scanner, key=lambda entry: entry.name)
        except (OSError, ValueError):
            issues.add(
                f"{_relative_path(directory, root_path)}: native source directory "
                f"cannot be scanned")
            continue

        for entry in entries:
            path = Path(entry.path)

            try:
                resolved = _safe_resolve(path)

                if not _is_within(resolved, root_path):
                    continue

                is_directory = (entry.is_dir(follow_symlinks=False)
                                or (entry.is_symlink() and resolved.is_dir()))
                is_file = (entry.is_file(follow_symlinks=False)
                           or (entry.is_symlink() and resolved.is_file()))

                if is_directory:
                    if shallow or resolved in visited_directories:
                        continue
                    visited_directories.add(resolved)
                    pending.append(path)
                    continue

                if is_file and path.suffix in _NATIVE_SUFFIXES:
                    files.add(_relative_path(path, root_path))
            except (OSError, RuntimeError, ValueError):
                issues.add(
                    f"{_relative_path(path, root_path)}: native source entry "
                    f"cannot be inspected")

    return files, issues


def _safe_resolve(path: Path) -> Path:
    try:
        return path.resolve()
    except (OSError, RuntimeError, ValueError):
        return path


def _relative_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.name


def _read_utf8(path: Path) -> str | None:
    try:
        return path.read_bytes().decode("utf-8-sig")
    except (OSError, UnicodeDecodeError, ValueError):
        return None


def _check_native_file(rel: str, text: str, inventory: set[str]) -> set[str]:
    issues: set[str] = set()
    analysis_text = text.replace("\r\n", "\n")
    includes = _real_includes(analysis_text)

    if rel.endswith(".h"):
        if _is_bootstrap_header(rel):
            for line, kind, target in includes:
                if _is_common_include(kind, target):
                    issues.add(
                        f"{rel}:{line}: bootstrap header must not include Common.h")
        else:
            _require_first_include(issues, rel, includes, "Common.h", analysis_text)
    else:
        header_rel = rel[:-4] + ".h"

        if header_rel in inventory:
            expected = _basename(header_rel)
        elif _is_standalone_sample(rel, _STANDALONE_SAMPLE_TEST):
            expected = "NativeExtensionCore.h"
        else:
            expected = "Common.h"

        _require_first_include(issues, rel, includes, expected, analysis_text)

    _check_eof(issues, rel, text)

    return issues


def _require_first_include(issues: set[str], rel: str,
                           includes: list[tuple[int, str, str]],
                           expected: str, text: str) -> None:
    if not includes:
        issues.add(f'{rel}: first include must be "{expected}"')
        return

    line, kind, target = includes[0]

    if kind != "quoted" or target != expected:
        issues.add(f'{rel}:{line}: first include must be "{expected}"')

    if _has_following_content(text, line) and not _has_blank_line_after(text, line):
        issues.add(_separation_issue(rel, line))


def _separation_issue(rel: str, line: int) -> str:
    return f"{rel}:{line}: blank line must follow the first include"


def _has_following_content(text: str, line: int) -> bool:
    lines = text.split("\n")

    return any(lines[index].strip() for index in range(line, len(lines)))


def _has_blank_line_after(text: str, line: int) -> bool:
    lines = text.split("\n")
    next_index = line

    return next_index < len(lines) and lines[next_index].strip() == ""


def _is_bootstrap_header(rel: str) -> bool:
    if rel in ("Source/Common/Common.h", "Source/Common/ImGuiExt/ImGuiConfig.h"):
        return True

    if _is_standalone_sample(rel, _STANDALONE_SAMPLE_HEADER):
        return True

    return _parent(rel) == "Source/Essentials"


def _is_standalone_sample(rel: str, sample_file: str) -> bool:
    # The sample is checked from the Engine root and from its own repository root.
    return rel == sample_file or rel == _STANDALONE_SAMPLE_ROOT + sample_file


def _is_common_include(kind: str, target: str) -> bool:
    if kind not in ("quoted", "angle"):
        return False

    normalized = target.replace("\\", "/")
    return normalized.rsplit("/", 1)[-1] == "Common.h"


def _basename(path: str) -> str:
    return path.rsplit("/", 1)[-1]


def _parent(path: str) -> str:
    if "/" not in path:
        return ""

    return path.rsplit("/", 1)[0]


def _real_includes(text: str) -> list[tuple[int, str, str]]:
    masked = _mask_source(text)
    result: list[tuple[int, str, str]] = []

    for line_number, line in enumerate(masked.split("\n"), 1):
        match = _INCLUDE_RE.match(line)

        if match is None:
            continue

        kind, target = _parse_include_target(match.group("rest"))
        result.append((line_number, kind, target))

    return result


def _parse_include_target(rest: str) -> tuple[str, str]:
    value = rest.strip()

    if not value:
        return "empty", ""

    if value.startswith('"'):
        match = re.match(r'"((?:\\.|[^"\\])*)"', value)

        if match is None:
            return "invalid", value

        return "quoted", match.group(1)

    if value.startswith("<"):
        end = value.find(">", 1)

        if end < 0:
            return "invalid", value

        return "angle", value[1:end]

    return "macro", value.split()[0]


def _mask_source(text: str) -> str:
    """Blank comments and string literals while preserving newlines and real include targets."""

    return scan_source(text)[0]


def scan_source(text: str) -> tuple[str, list[tuple[int, int]]]:
    """Return the masked text and the ``(start, end)`` offsets of every comment.

    Comments and string literals are blanked while newlines and real include
    targets are preserved, so offsets and line numbers match *text*.
    """

    masked = list(text)
    comments: list[tuple[int, int]] = []
    length = len(text)
    index = 0

    while index < length:
        char = text[index]

        if char == "\n":
            index += 1
            continue

        if text.startswith("//", index):
            end = text.find("\n", index)
            while end > 0 and text[end - 1] == "\\":
                end = text.find("\n", end + 1)
            end = length if end < 0 else end
            comments.append((index, end))
            _blank(masked, index, end)
            index = end
            continue

        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            comments.append((index, end))
            _blank(masked, index, end)
            index = end
            continue

        raw_end = _raw_literal_end(text, index) if char in "uULR" else None

        if raw_end is not None:
            _blank(masked, index, raw_end, _STRING_TOKEN)
            index = raw_end
            continue

        if char in ('"', "'"):
            preserve = _line_prefix_is_include_target(masked, index)
            end = _string_end(text, index, char)

            if not preserve:
                _blank(masked, index, end, _STRING_TOKEN)

            index = end
            continue

        index += 1

    return "".join(masked), comments


def _blank(chars: list[str], start: int, end: int, filler: str = " ") -> None:
    for position in range(start, end):
        if chars[position] != "\n":
            chars[position] = filler


def _raw_literal_end(text: str, index: int) -> int | None:
    if index > 0 and (text[index - 1].isalnum() or text[index - 1] == "_"):
        return None

    match = _RAW_LITERAL_RE.match(text, index)

    if match is None:
        return None

    closing = ")" + match.group("delimiter") + '"'
    end = text.find(closing, match.end())

    return len(text) if end < 0 else end + len(closing)


def _string_end(text: str, start: int, quote: str) -> int:
    index = start + 1
    length = len(text)

    while index < length:
        char = text[index]

        if char == "\\":
            index += 2 if index + 1 < length else 1
            continue

        if char == quote:
            return index + 1

        if char == "\n":
            return index

        index += 1

    return length


def _line_prefix_is_include_target(masked: list[str], index: int) -> bool:
    start = index

    while start > 0 and masked[start - 1] != "\n":
        start -= 1

    return _INCLUDE_PREFIX_RE.fullmatch("".join(masked[start:index])) is not None


def _check_eof(issues: set[str], rel: str, text: str) -> None:
    match = _EOF_TAIL_RE.search(text)

    if match is None or len(_EOL_RE.findall(match.group("eols"))) != 1:
        issues.add(_eof_issue(rel, text))
        return

    content = text[:match.start()]
    last_line = content.rsplit("\n", 1)[-1]

    if (content.endswith("\r") or not last_line or not last_line.strip()
            or last_line != last_line.rstrip()):
        issues.add(_eof_issue(rel, text))


def _eof_issue(rel: str, text: str) -> str:
    line = text.count("\n") + 1
    return (f"{rel}:{line}: file must end with exactly one line ending (LF or CRLF), "
            f"not an extra empty trailing line, and no trailing spaces")


def check_comment_text(rel: str, text: str) -> list[Finding]:
    """Apply the native comment rules to one file and return its findings."""

    analysis_text = text.replace("\r\n", "\n")
    masked, spans = scan_source(analysis_text)
    lines = analysis_text.split("\n")
    findings: list[Finding] = []

    for block in _comment_blocks(analysis_text, masked, spans):
        if _is_exempt_block(block):
            continue

        prose = _prose_lines(block, lines)

        if block.whole_line and len(prose) > _MAX_COMMENT_PROSE_LINES:
            findings.append(Finding(
                "comment-length", rel, block.line,
                f"comment has {len(prose)} lines of prose; keep at most {_MAX_COMMENT_PROSE_LINES} "
                f"and move the explanation to the owning document"))

        for offset, body in enumerate(block.bodies):
            marker = _bookkeeping_marker(body)

            if marker is not None:
                kind, value = marker
                findings.append(Finding(
                    "comment-bookkeeping", rel, block.line + offset,
                    f"comment carries a {kind} ({value}); keep history in version control "
                    f"and the owning document"))

        concluding = _concluding_line(block)

        if concluding is not None and _ends_with_period(block.bodies[concluding]):
            findings.append(Finding(
                "comment-period", rel, block.line + concluding,
                "concluding comment sentence must omit its final period"))

    return findings


class _CommentBlock:
    """Adjacent whole-line comments, one block comment, or one trailing comment."""

    def __init__(self, line: int, bodies: list[str], whole_line: bool, leading: bool) -> None:
        self.line = line
        self.bodies = bodies
        self.whole_line = whole_line
        self.leading = leading


def _comment_blocks(text: str, masked: str, spans: list[tuple[int, int]]) -> list[_CommentBlock]:
    blocks: list[_CommentBlock] = []
    current: _CommentBlock | None = None
    current_end_line = 0

    for start, end in spans:
        line = text.count("\n", 0, start) + 1
        raw = text[start:end]
        line_start = text.rfind("\n", 0, start) + 1
        line_end = text.find("\n", end)
        line_end = len(text) if line_end < 0 else line_end
        before_is_blank = not text[line_start:start].strip()
        after_is_blank = not masked[end:line_end].strip()
        leading = not masked[:start].strip() and not blocks and current is None

        if raw.startswith("///@"):
            # Export tags are metadata, not prose, and they end the comment block above them
            if current is not None:
                blocks.append(current)
                current = None
            continue

        if raw.startswith("//"):
            bodies = [part.strip() for part in raw[2:].split("\n")]
            whole_line = before_is_blank

            if (whole_line and current is not None and current.whole_line
                    and line == current_end_line + 1):
                current.bodies.extend(bodies)
                current_end_line = line + len(bodies) - 1
                continue

            if current is not None:
                blocks.append(current)

            current = _CommentBlock(line, bodies, whole_line, leading)
            current_end_line = line + len(bodies) - 1

            if not whole_line:
                blocks.append(current)
                current = None
            continue

        if current is not None:
            blocks.append(current)
            current = None

        # Inline block comments such as event argument names annotate code rather than explain it
        if not (before_is_blank and after_is_blank):
            continue

        bodies = [_block_comment_body(part) for part in raw[2:-2].split("\n")]

        while bodies and not bodies[0]:
            bodies.pop(0)
            line += 1

        while bodies and not bodies[-1]:
            bodies.pop()

        if bodies:
            blocks.append(_CommentBlock(line, bodies, True, leading))

    if current is not None:
        blocks.append(current)

    return blocks


def _block_comment_body(part: str) -> str:
    body = part.strip()

    if body.startswith("*"):
        body = body[1:].strip()

    return body


def _is_exempt_block(block: _CommentBlock) -> bool:
    joined = "\n".join(block.bodies)

    if _LICENSE_RE.search(joined) or (block.leading and "license" in joined.lower()):
        return True

    meaningful = [body for body in block.bodies if body]

    if meaningful and all(_DIRECTIVE_RE.match(_prose_text(body)) for body in meaningful):
        return True

    code_like = sum(1 for body in meaningful if _CODE_TAIL_RE.search(body) or _DISABLED_TAG_RE.match(body))

    return code_like * 2 > len(meaningful)


def _prose_lines(block: _CommentBlock, lines: list[str]) -> list[str]:
    layout_fields = _following_layout_fields(block, lines)
    prose: list[str] = []

    for body in block.bodies:
        text = _prose_text(body)

        if not text or _SEPARATOR_RE.fullmatch(text):
            continue

        label = _FIELD_LABEL_RE.fullmatch(text)

        if label is not None and label.group(1) in layout_fields:
            continue

        prose.append(text)

    return prose


def _following_layout_fields(block: _CommentBlock, lines: list[str]) -> set[str]:
    following = block.line - 1 + len(block.bodies)

    if following >= len(lines) or not lines[following].strip().startswith("///@ ExportValueType "):
        return set()

    layout = _LAYOUT_RE.search(lines[following])

    if layout is None:
        return set()

    return set(_LAYOUT_FIELD_RE.findall(layout.group(1)))


def _concluding_line(block: _CommentBlock) -> int | None:
    for index in range(len(block.bodies) - 1, -1, -1):
        text = _prose_text(block.bodies[index])

        if text and not _SEPARATOR_RE.fullmatch(text):
            return index

    return None


def _ends_with_period(body: str) -> bool:
    text = _prose_text(body)

    if not text.endswith(".") or text.endswith("..") or " " not in text:
        return False

    return _CODE_TAIL_RE.search(text[:-1]) is None


def _bookkeeping_marker(body: str) -> tuple[str, str] | None:
    text = _prose_text(body)

    for kind, pattern in _BOOKKEEPING_PATTERNS:
        match = pattern.search(text)

        if match is not None:
            return kind, match.group(0).strip()

    return None


def _prose_text(body: str) -> str:
    return body.lstrip("/!<").strip()


if __name__ == "__main__":
    raise SystemExit(main())
