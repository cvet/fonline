"""Check local Markdown links, heading anchors and basic document structure.

Every selected document is decoded as strict UTF-8. Inline links, images, HTML
``href``/``src`` attributes and reference-style definitions outside front
matter, fenced code and inline code spans must resolve to an existing file or
directory under the root, or under an explicitly allowed target root. A
``#fragment`` on a Markdown target must name a GitHub-style heading slug
(repeated headings receive ``-1``, ``-2`` suffixes) or an explicit HTML
``id``/``name`` anchor. Unclosed fences and U+FFFD replacement characters are
reported; documents without exactly one H1 and reference-style links whose label
has no definition are reported on request. External URLs are never fetched.

This is an ATX-heading and inline-link subset of Markdown, not a renderer.
"""

from __future__ import annotations

import argparse
from collections.abc import Iterable, Iterator, Sequence
import io
import os
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit


DEFAULT_INCLUDES = ("**/*.md", "**/*.markdown")
MARKDOWN_SUFFIXES = frozenset({".md", ".markdown"})
ALWAYS_EXCLUDED_DIRECTORIES = frozenset({".git"})

INLINE_LINK_RE = re.compile(r"!?\[[^\]]*]\((?P<target>[^\r\n)]*)\)")
REFERENCE_LINK_RE = re.compile(r"^\s*\[(?P<label>[^\]]+)]:\s*(?P<target>\S+)")
REFERENCE_USE_RE = re.compile(r"(?<![\]\\])\[(?P<text>[^\]]+)]\[(?P<label>[^\]]*)]")
HTML_LINK_RE = re.compile(r"\b(?:href|src)=[\"'](?P<target>[^\"']+)[\"']", re.IGNORECASE)
FENCE_RE = re.compile(r"^\s*(?P<fence>`{3,}|~{3,})(?P<info>.*)$")
INLINE_CODE_RE = re.compile(r"(`+)(.+?)\1")
HTML_CODE_RE = re.compile(r"<code\b[^>]*>.*?</code>", re.IGNORECASE)
HEADING_RE = re.compile(r"^(?P<marks>#{1,6})\s+(?P<title>.+?)\s*#*\s*$")
EXPLICIT_ANCHOR_RE = re.compile(r"<(?:a|[^>]+)\s+(?:id|name)=[\"'](?P<anchor>[^\"']+)[\"']", re.IGNORECASE)
LINE_FRAGMENT_RE = re.compile(r"^L\d+(?:C\d+)?(?:-L\d+(?:C\d+)?)?$")
FRONT_MATTER_END_RE = re.compile(r"^(?:---|\.\.\.)\s*$")

_BYTE_ORDER_MARK = "\ufeff"
_REPLACEMENT_CHARACTER = "\ufffd"
_PROSE = "prose"
_FRONT_MATTER = "front-matter"
_FENCE_OPEN = "fence-open"
_FENCE_CONTENT = "fence-content"
_FENCE_CLOSE = "fence-close"


def iter_prose_lines(text: str, *, strip_inline_code: bool = True) -> Iterator[tuple[int, str]]:
    """Yield ``(line_number, line)`` for lines outside front matter and fenced code blocks."""

    for line_number, line, kind in _classify_lines(text):
        if kind == _PROSE:
            yield line_number, _strip_inline_code(line) if strip_inline_code else line


def unclosed_fence_line(text: str) -> int | None:
    """Return the line that opens a fenced code block which never closes, if any."""

    opened_at: int | None = None
    for line_number, _, kind in _classify_lines(text):
        if kind == _FENCE_OPEN:
            opened_at = line_number
        elif kind == _FENCE_CLOSE:
            opened_at = None
    return opened_at


def normalize_link_target(raw_target: str) -> str:
    """Strip an optional title and angle brackets from a raw link destination."""

    target = raw_target.strip()
    if target.startswith("<") and ">" in target:
        return target[1 : target.index(">")]

    title_separator = re.search(r"\s+[\"']", target)
    if title_separator:
        target = target[: title_separator.start()]
    return target


def iter_links(text: str) -> Iterator[tuple[int, str]]:
    """Yield ``(line_number, target)`` for every link destination outside code."""

    for line_number, line in iter_prose_lines(text):
        for pattern in (INLINE_LINK_RE, HTML_LINK_RE):
            for match in pattern.finditer(line):
                yield line_number, normalize_link_target(match.group("target"))

        reference_match = REFERENCE_LINK_RE.match(line)
        if reference_match and not reference_match.group("label").startswith("^"):
            yield line_number, normalize_link_target(reference_match.group("target"))


def heading_slug(title: str) -> str:
    """Return the GitHub-style anchor slug of an ATX heading title, without a duplicate suffix."""

    title = re.sub(r"!?\[([^\]]+)]\([^)]*\)", r"\1", title)
    title = re.sub(r"<[^>]+>", "", title)
    title = title.replace("`", "").replace("*", "")
    title = title.strip().lower()
    title = re.sub(r"[^\w\- ]", "", title, flags=re.UNICODE)

    # Every remaining space becomes a hyphen, so "A / B" yields "a--b" exactly as GitHub and kramdown GFM do
    return title.replace(" ", "-")


def heading_anchors(text: str) -> set[str]:
    """Return every fragment a Markdown text answers to: heading slugs and explicit HTML anchors."""

    anchors: set[str] = set()
    slug_counts: dict[str, int] = {}

    for _, line in iter_prose_lines(text, strip_inline_code=False):
        for anchor_match in EXPLICIT_ANCHOR_RE.finditer(line):
            anchors.add(anchor_match.group("anchor"))

        heading_match = HEADING_RE.match(line)
        if not heading_match:
            continue

        base_slug = heading_slug(heading_match.group("title"))
        duplicate_index = slug_counts.get(base_slug, 0)
        slug_counts[base_slug] = duplicate_index + 1
        anchors.add(base_slug if duplicate_index == 0 else f"{base_slug}-{duplicate_index}")

    return anchors


def markdown_anchors(path: Path) -> set[str]:
    """Read a UTF-8 Markdown file and return its anchors; read and decoding errors propagate."""

    return heading_anchors(_read_markdown(path))


def is_external_target(target: str) -> bool:
    """Return whether a link destination names a URL scheme or a network path."""

    return bool(urlsplit(target).scheme) or target.startswith("//")


def resolve_local_target(root: Path, document: Path, target: str) -> tuple[Path, str]:
    """Resolve a local destination against its document; ``/`` paths are root-relative."""

    path_text, _, fragment = target.partition("#")
    path_text = unquote(path_text.split("?", maxsplit=1)[0])
    if path_text.startswith("/"):
        candidate = root / path_text.lstrip("/")
    elif path_text:
        candidate = document.parent / path_text
    else:
        candidate = document
    return candidate.resolve(), unquote(fragment)


def check_links(
    root: Path,
    documents: Iterable[str],
    *,
    target_roots: Sequence[Path] = (),
    skip_targets: Sequence[str] = (),
    allow_line_fragments: bool = False,
    require_reference_definitions: bool = False,
    root_label: str = "root",
) -> list[str]:
    """Check local link targets and anchors of root-relative *documents* and return diagnostics."""

    root = root.resolve()
    allowed_roots = [root, *(path.resolve() for path in target_roots)]
    skip_matchers = [_glob_regex(pattern, below=True) for pattern in skip_targets]
    errors: list[str] = []
    anchor_cache: dict[Path, set[str] | None] = {}

    for document_path in sorted(documents):
        document = root / document_path
        text, read_error = _try_read_markdown(document)
        if text is None:
            errors.append(f"{document_path}: {read_error}")
            continue

        if require_reference_definitions:
            errors.extend(_undefined_reference_labels(document_path, text))

        for line_number, target in iter_links(text):
            if not target or target.startswith("#"):
                candidate = document.resolve()
                fragment = unquote(target.removeprefix("#"))
            elif is_external_target(target):
                continue
            else:
                candidate, fragment = resolve_local_target(root, document, target)

            location = f"{document_path}:{line_number}"
            if not any(_is_within(candidate, allowed) for allowed in allowed_roots):
                errors.append(f"{location}: local link escapes the {root_label}: {target}")
                continue
            if _is_within(candidate, root) and _matches_any(candidate.relative_to(root).as_posix(), skip_matchers):
                continue
            if not candidate.exists():
                errors.append(f"{location}: local link target does not exist: {target}")
                continue

            if not fragment or not candidate.is_file() or candidate.suffix.lower() not in MARKDOWN_SUFFIXES:
                continue
            if allow_line_fragments and LINE_FRAGMENT_RE.match(fragment):
                continue

            if candidate not in anchor_cache:
                target_text, _ = _try_read_markdown(candidate)
                anchor_cache[candidate] = None if target_text is None else heading_anchors(target_text)

            anchors = anchor_cache[candidate]
            if anchors is None:
                errors.append(f"{location}: Markdown anchor target is not readable UTF-8: {target}")
            elif fragment not in anchors:
                errors.append(f"{location}: Markdown anchor does not exist: {target}")

    return errors


def check_structure(
    root: Path,
    documents: Iterable[str],
    *,
    require_single_h1: bool = False,
    h1_exempt: Sequence[str] = (),
) -> list[str]:
    """Check UTF-8 validity, fence balance and optionally the H1 count of root-relative *documents*."""

    root = root.resolve()
    exempt_matchers = [_glob_regex(pattern) for pattern in h1_exempt]
    errors: list[str] = []

    for document_path in sorted(documents):
        text, read_error = _try_read_markdown(root / document_path)
        if text is None:
            errors.append(f"{document_path}: {read_error}")
            continue

        replacement_line = next(
            (number for number, line in enumerate(text.splitlines(), start=1) if _REPLACEMENT_CHARACTER in line),
            None,
        )
        if replacement_line is not None:
            errors.append(f"{document_path}:{replacement_line}: document contains the U+FFFD replacement character")

        unclosed = unclosed_fence_line(text)
        if unclosed is not None:
            errors.append(f"{document_path}:{unclosed}: fenced code block is never closed")

        if require_single_h1 and not _matches_any(document_path, exempt_matchers):
            h1_count = sum(1 for _, line in iter_prose_lines(text) if _heading_level(line) == 1)
            if h1_count != 1:
                errors.append(f"{document_path}: expected exactly one H1 heading, found {h1_count}")

    return errors


def collect_documents(
    root: Path,
    includes: Sequence[str] = DEFAULT_INCLUDES,
    excludes: Sequence[str] = (),
) -> list[str]:
    """Return sorted root-relative POSIX paths of files matching *includes* and no *excludes*.

    Patterns are root-relative globs: ``*`` and ``?`` stay inside one path segment
    and ``**`` spans segments. An exclude also removes everything below a matching
    directory. ``.git`` directories and symbolic-link directories are never walked.
    """

    root = root.resolve()
    include_matchers = [_glob_regex(pattern) for pattern in includes]
    exclude_matchers = [_glob_regex(pattern, below=True) for pattern in excludes]
    documents: list[str] = []

    for directory, directory_names, file_names in os.walk(root):
        relative_directory = Path(directory).relative_to(root).as_posix()
        prefix = "" if relative_directory == "." else relative_directory + "/"
        directory_names[:] = [
            name
            for name in directory_names
            if name not in ALWAYS_EXCLUDED_DIRECTORIES and not _matches_any(prefix + name, exclude_matchers)
        ]

        for name in file_names:
            relative_path = prefix + name
            if _matches_any(relative_path, include_matchers) and not _matches_any(relative_path, exclude_matchers):
                documents.append(relative_path)

    return sorted(documents)


def check_documents(
    root: Path,
    includes: Sequence[str] = DEFAULT_INCLUDES,
    excludes: Sequence[str] = (),
    *,
    target_roots: Sequence[Path] = (),
    skip_targets: Sequence[str] = (),
    allow_line_fragments: bool = False,
    require_reference_definitions: bool = False,
    require_single_h1: bool = False,
    h1_exempt: Sequence[str] = (),
) -> tuple[list[str], int]:
    """Collect documents, run structure and link checks, and return ``(diagnostics, document_count)``."""

    documents = collect_documents(root, includes, excludes)
    structure_errors = check_structure(root, documents, require_single_h1=require_single_h1, h1_exempt=h1_exempt)
    link_errors = check_links(
        root,
        documents,
        target_roots=target_roots,
        skip_targets=skip_targets,
        allow_line_fragments=allow_line_fragments,
        require_reference_definitions=require_reference_definitions,
    )

    # Both passes report an unreadable document with the same text; keep one copy
    unique_errors = list(dict.fromkeys(structure_errors + link_errors))
    return sorted(unique_errors, key=_diagnostic_sort_key), len(documents)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="markdown_links.py",
        description="Check local Markdown links, heading anchors, UTF-8 and fenced-code balance under one root",
    )
    parser.add_argument("--root", type=Path, default=".",
                        help="repository root that bounds link targets (default: current directory)")
    parser.add_argument("--include", action="append", default=None, metavar="GLOB",
                        help="root-relative document glob; repeatable (default: **/*.md and **/*.markdown)")
    parser.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                        help="root-relative file or directory glob removed from the document set; repeatable")
    parser.add_argument("--target-root", action="append", default=[], type=Path, metavar="DIR",
                        help="extra directory, relative to --root, that links may point into without its documents "
                             "being checked; repeatable")
    parser.add_argument("--skip-target", action="append", default=[], metavar="GLOB",
                        help="root-relative glob of link targets that are not verified, such as a submodule that is "
                             "not checked out; repeatable")
    parser.add_argument("--allow-line-fragments", action="store_true",
                        help="accept #L10 and #L10-L20 line fragments on Markdown targets")
    parser.add_argument("--require-reference-definitions", action="store_true",
                        help="report [text][label] links whose label has no definition in the same document")
    parser.add_argument("--require-single-h1", action="store_true",
                        help="report documents that do not contain exactly one H1 heading")
    parser.add_argument("--h1-exempt", action="append", default=[], metavar="GLOB",
                        help="root-relative glob of documents exempt from --require-single-h1; repeatable")
    parser.add_argument("--list", action="store_true",
                        help="print the selected documents instead of checking them")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    _use_utf8_output()

    root = args.root.resolve()
    if not root.is_dir():
        print(f"markdown_links: root is not a directory: {root}", file=sys.stderr)
        return 2

    target_roots = [(root / path).resolve() for path in args.target_root]
    missing_roots = [path for path in target_roots if not path.is_dir()]
    for path in missing_roots:
        print(f"markdown_links: target root is not a directory: {path}", file=sys.stderr)
    if missing_roots:
        return 2

    includes = args.include if args.include is not None else list(DEFAULT_INCLUDES)
    if args.list:
        for document in collect_documents(root, includes, args.exclude):
            print(document)
        return 0

    errors, document_count = check_documents(
        root,
        includes,
        args.exclude,
        target_roots=target_roots,
        skip_targets=args.skip_target,
        allow_line_fragments=args.allow_line_fragments,
        require_reference_definitions=args.require_reference_definitions,
        require_single_h1=args.require_single_h1,
        h1_exempt=args.h1_exempt,
    )
    if document_count == 0:
        print("markdown_links: no documents match the include and exclude patterns", file=sys.stderr)
        return 2

    for error in errors:
        print(error)

    print(f"markdown_links: checked {document_count} document(s), {len(errors)} finding(s)")
    return 1 if errors else 0


def _use_utf8_output() -> None:
    # Redirected output would otherwise use the ANSI code page on Windows and fail on non-Latin paths
    for stream in (sys.stdout, sys.stderr):
        if isinstance(stream, io.TextIOWrapper):
            stream.reconfigure(encoding="utf-8")


def _classify_lines(text: str) -> Iterator[tuple[int, str, str]]:
    lines = text.splitlines()
    front_matter_end = _front_matter_end(lines)
    fence_marker: str | None = None
    fence_length = 0

    for line_number, line in enumerate(lines, start=1):
        if line_number <= front_matter_end:
            yield line_number, line, _FRONT_MATTER
            continue

        fence = FENCE_RE.match(line)
        if fence_marker is None:
            # A backtick run followed by another backtick on the same line is inline code, not a fence
            if fence and not (fence.group("fence")[0] == "`" and "`" in fence.group("info")):
                fence_marker = fence.group("fence")[0]
                fence_length = len(fence.group("fence"))
                yield line_number, line, _FENCE_OPEN
            else:
                yield line_number, line, _PROSE
            continue

        if (fence and fence.group("fence")[0] == fence_marker and len(fence.group("fence")) >= fence_length
                and not fence.group("info").strip()):
            fence_marker = None
            yield line_number, line, _FENCE_CLOSE
        else:
            yield line_number, line, _FENCE_CONTENT


def _front_matter_end(lines: list[str]) -> int:
    if not lines or lines[0].rstrip() != "---":
        return 0

    for index in range(1, len(lines)):
        if FRONT_MATTER_END_RE.match(lines[index]):
            return index + 1
    return 0


def _strip_inline_code(line: str) -> str:
    return INLINE_CODE_RE.sub(lambda match: " " * len(match.group(0)), line)


def _heading_level(line: str) -> int:
    match = HEADING_RE.match(line)
    return len(match.group("marks")) if match else 0


def _undefined_reference_labels(document_path: str, text: str) -> list[str]:
    definitions: set[str] = set()
    uses: list[tuple[int, str]] = []

    for line_number, line in iter_prose_lines(text):
        definition = REFERENCE_LINK_RE.match(line)
        if definition:
            definitions.add(_reference_label(definition.group("label")))
            continue

        for match in REFERENCE_USE_RE.finditer(HTML_CODE_RE.sub("", line)):
            uses.append((line_number, match.group("label") or match.group("text")))

    return [
        f"{document_path}:{line_number}: reference link label is not defined: [{label}]"
        for line_number, label in uses
        if _reference_label(label) not in definitions
    ]


def _reference_label(label: str) -> str:
    return " ".join(label.split()).casefold()


def _read_markdown(path: Path) -> str:
    return path.read_bytes().decode("utf-8").removeprefix(_BYTE_ORDER_MARK)


def _try_read_markdown(path: Path) -> tuple[str | None, str]:
    try:
        return _read_markdown(path), ""
    except UnicodeDecodeError as error:
        return None, f"document is not valid UTF-8 ({error.reason} at byte {error.start})"
    except OSError as error:
        return None, f"document cannot be read ({error.strerror or error})"


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _glob_regex(pattern: str, *, below: bool = False) -> re.Pattern[str]:
    text = pattern.replace("\\", "/").strip("/")
    parts: list[str] = []
    index = 0

    while index < len(text):
        if text.startswith("**/", index):
            parts.append("(?:.*/)?")
            index += 3
        elif text.startswith("**", index):
            parts.append(".*")
            index += 2
        elif text[index] == "*":
            parts.append("[^/]*")
            index += 1
        elif text[index] == "?":
            parts.append("[^/]")
            index += 1
        else:
            parts.append(re.escape(text[index]))
            index += 1

    # An exclude or skip pattern that names a directory also covers everything below it
    suffix = r"(?:/.*)?\Z" if below else r"\Z"
    return re.compile("".join(parts) + suffix)


def _matches_any(relative_path: str, matchers: Sequence[re.Pattern[str]]) -> bool:
    return any(matcher.match(relative_path) for matcher in matchers)


def _diagnostic_sort_key(error: str) -> tuple[str, int, str]:
    path, _, rest = error.partition(":")
    line_text, _, message = rest.partition(":")
    return (path, int(line_text), message) if line_text.isdigit() else (path, 0, rest)


if __name__ == "__main__":
    raise SystemExit(main())
