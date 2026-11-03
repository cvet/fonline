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
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re


_GENERATOR_INPUTS = frozenset({
    "Source/Common/GenericCode.template.cpp",
    "Source/Common/MetadataRegistration.template.cpp",
})
_NATIVE_SUFFIXES = frozenset({".h", ".cpp"})
_STANDALONE_SAMPLE_HEADER = "Examples/NativeExtensionSample/SourceExt/NativeExtensionCore.h"
_STANDALONE_SAMPLE_TEST = "Examples/NativeExtensionSample/Tests/NativeExtensionCoreTest.cpp"
_INCLUDE_RE = re.compile(r"^[ \t]*#[ \t]*include\b(?P<rest>.*)\Z")
_INCLUDE_PREFIX_RE = re.compile(r"[ \t]*#[ \t]*include[ \t]*\Z")
_EOF_TAIL_RE = re.compile(r"(?P<eols>(?:\r\n|\n)+)\Z")
_EOL_RE = re.compile(r"\r\n|\n")
_RAW_LITERAL_RE = re.compile(
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^()\\\s]{0,16})\(')
_DRIVE_RE = re.compile(r"^[A-Za-z]:")
_STRING_TOKEN = "\x00"


def check_layout(root: Path, source_dir: str = "Source", shallow: bool = False) -> list[str]:
    """Validate the native source layout under *root* and return sorted diagnostics."""

    source_rel = _normalize_source_dir(source_dir)
    if source_rel is None:
        return [
            "source_dir: source directory must be a non-empty root-relative "
            "path without '..' segments",
        ]

    root_path = _resolved_root(root)
    source_path = root_path if source_rel == "." else root_path.joinpath(*source_rel.split("/"))

    try:
        source_exists = source_path.exists()
        source_is_dir = source_path.is_dir()
    except (OSError, RuntimeError, ValueError):
        source_exists = False
        source_is_dir = False

    if not source_exists:
        return [f"{source_rel}: native source directory does not exist"]

    if not source_is_dir:
        return [f"{source_rel}: native source path is not a directory"]

    try:
        resolved_source = source_path.resolve()
    except (OSError, RuntimeError, ValueError):
        resolved_source = source_path

    if not _is_within(resolved_source, root_path):
        return [f"{source_rel}: source directory resolves outside the root"]

    files, issues = _scan_native_files(source_path, root_path, shallow)
    inventory = sorted(path for path in files if path not in _GENERATOR_INPUTS)

    if not inventory:
        issues.add(f"{source_rel}: native source inventory is empty")
        return sorted(issues)

    inventory_set = set(inventory)
    for rel in inventory:
        text = _read_utf8(root_path / rel)

        if text is None:
            issues.add(f"{rel}: unable to read file as UTF-8")
            continue

        issues.update(_check_native_file(rel, text, inventory_set))

    return sorted(issues)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                        help="engine repository root to check")
    parser.add_argument("--source-dir", default="Source",
                        help="root-relative native source directory (default: Source)")
    parser.add_argument("--shallow", action="store_true",
                        help="check only direct children of the source directory")
    args = parser.parse_args(argv)

    issues = check_layout(args.root, source_dir=args.source_dir, shallow=args.shallow)
    for issue in issues:
        print(issue)

    return 1 if issues else 0


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
        elif rel == _STANDALONE_SAMPLE_TEST:
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
    if rel in ("Source/Common/Common.h",
               "Source/Common/ImGuiExt/ImGuiConfig.h",
               _STANDALONE_SAMPLE_HEADER):
        return True

    return _parent(rel) == "Source/Essentials"


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

    masked = list(text)
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
            _blank(masked, index, end)
            index = end
            continue

        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
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

    return "".join(masked)


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


if __name__ == "__main__":
    raise SystemExit(main())
