#!/usr/bin/env python3
"""Audit first-party C++ for non-ASCII ordinary narrow literals.

The scanner is deliberately lexical rather than line-regex based: comments are
ignored, raw strings are understood, and UTF/wide prefixes are classified
separately. Ordinary narrow literals may contain only ASCII source characters
and ASCII-valued escapes; UTF-8 code units and non-ASCII universal escapes must
use an explicit UTF-prefixed literal or byte-oriented storage.
"""

from __future__ import annotations

import argparse
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc", ".inl"}
DEFAULT_EXCLUDES = (
    "Engine/ThirdParty/",
    "Engine/Build/",
    "SourceExt/SHA/",
    "SourceExt/curl/",
    "SourceExt/sentry-native/",
    "SourceExt/spine-cpp/",
    "SourceExt/steamworks/",
    "Tools/DialogEditor/",
    "Build/",
    "Workspace/",
)
LITERAL_PREFIXES = ("u8R", "uR", "UR", "LR", "R", "u8", "u", "U", "L")


@dataclass(frozen=True)
class Finding:
    path: Path
    line: int
    column: int
    kind: str
    preview: str
    rule: str

    def diagnostic(self) -> str:
        if self.rule == "non_ascii_narrow_literal":
            message = f"non-ASCII source text in ordinary narrow {self.kind} literal"
        elif self.rule == "non_ascii_narrow_escape":
            message = f"non-ASCII escape in ordinary narrow {self.kind} literal"
        else:
            message = "direct C++ pointer/reinterpret cast to char/char8_t storage; use a bounded text/byte or ABI adapter"

        return f"{self.path.as_posix()}:{self.line}:{self.column}: error: {message}: {self.preview} [{self.rule}]"


def _is_identifier_char(ch: str) -> bool:
    return ch == "_" or ch.isalnum()


def _line_column(text: str, offset: int) -> tuple[int, int]:
    line = text.count("\n", 0, offset) + 1
    previous_newline = text.rfind("\n", 0, offset)
    return line, offset - previous_newline


def _preview(value: str) -> str:
    compact = value.replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t")
    if len(compact) > 48:
        compact = compact[:45] + "..."
    return repr(compact)


def _literal_at(text: str, offset: int) -> tuple[str, str, int] | None:
    ch = text[offset]

    if ch in {'"', "'"}:
        return "", ch, offset

    if offset != 0 and _is_identifier_char(text[offset - 1]):
        return None

    for prefix in LITERAL_PREFIXES:
        quote_offset = offset + len(prefix)
        if not text.startswith(prefix, offset) or quote_offset >= len(text):
            continue

        quote = text[quote_offset]
        if quote == '"' or (quote == "'" and "R" not in prefix):
            return prefix, quote, quote_offset

    return None


def _scan_quoted_literal(text: str, content_start: int, quote: str) -> tuple[int, str]:
    pos = content_start

    while pos < len(text):
        ch = text[pos]

        if ch == "\\":
            pos = min(pos + 2, len(text))
            continue
        if ch == quote:
            return pos + 1, text[content_start:pos]

        pos += 1

    return len(text), text[content_start:]


def _scan_raw_literal(text: str, quote_offset: int) -> tuple[int, str] | None:
    delimiter_start = quote_offset + 1
    open_paren = text.find("(", delimiter_start, delimiter_start + 17)

    if open_paren == -1:
        return None

    delimiter = text[delimiter_start:open_paren]
    closing = ")" + delimiter + '"'
    close_offset = text.find(closing, open_paren + 1)

    if close_offset == -1:
        return len(text), text[open_paren + 1 :]

    return close_offset + len(closing), text[open_paren + 1 : close_offset]


def _has_non_ascii_escape(content: str) -> bool:
    hex_digits = "0123456789abcdefABCDEF"
    octal_digits = "01234567"
    pos = 0

    while pos < len(content):
        if content[pos] != "\\" or pos + 1 >= len(content):
            pos += 1
            continue

        escape = content[pos + 1]

        if escape == "x":
            end = pos + 2

            while end < len(content) and content[end] in hex_digits:
                end += 1

            if end > pos + 2 and int(content[pos + 2 : end], 16) > 0x7F:
                return True

            pos = end
            continue

        if escape in octal_digits:
            end = pos + 2

            while end < min(pos + 4, len(content)) and content[end] in octal_digits:
                end += 1

            if int(content[pos + 1 : end], 8) > 0x7F:
                return True

            pos = end
            continue

        if escape in {"u", "U"}:
            digit_count = 4 if escape == "u" else 8
            end = pos + 2 + digit_count
            digits = content[pos + 2 : end]

            if len(digits) == digit_count and all(ch in hex_digits for ch in digits) and int(digits, 16) > 0x7F:
                return True

            pos = end
            continue

        pos += 2

    return False


def _named_cast_at(text: str, offset: int, keyword: str) -> tuple[int, str] | None:
    if not text.startswith(keyword, offset):
        return None
    if offset != 0 and _is_identifier_char(text[offset - 1]):
        return None

    target_start = offset + len(keyword)

    if target_start < len(text) and _is_identifier_char(text[target_start]):
        return None

    while target_start < len(text) and text[target_start].isspace():
        target_start += 1

    if target_start >= len(text) or text[target_start] != "<":
        return None

    depth = 1
    pos = target_start + 1

    while pos < len(text) and depth != 0:
        if text[pos] == "<":
            depth += 1
        elif text[pos] == ">":
            depth -= 1

        pos += 1

    if depth != 0:
        return None

    return pos, text[target_start + 1 : pos - 1]


def _is_character_storage_target(target: str) -> bool:
    words = "".join(ch if _is_identifier_char(ch) else " " for ch in target).split()
    return "char" in words or "char8_t" in words


def _c_style_character_pointer_cast_at(text: str, offset: int) -> tuple[int, str] | None:
    if text[offset] != "(" or (offset != 0 and _is_identifier_char(text[offset - 1])):
        return None

    close = text.find(")", offset + 1, min(offset + 96, len(text)))

    if close == -1:
        return None

    target = text[offset + 1 : close]
    words = "".join(ch if _is_identifier_char(ch) else " " for ch in target).split()
    allowed_words = {"char", "char8_t", "const", "volatile", "signed", "unsigned"}

    if "*" not in target or not _is_character_storage_target(target) or any(word not in allowed_words for word in words):
        return None

    symbols = "".join(ch for ch in target if not ch.isspace() and not _is_identifier_char(ch))

    if not symbols or any(ch != "*" for ch in symbols):
        return None

    following = close + 1

    while following < len(text) and text[following].isspace():
        following += 1

    if following >= len(text) or text[following] in ";,{)}":
        return None

    return close + 1, target


def scan_text(text: str, path: Path) -> list[Finding]:
    findings: list[Finding] = []
    pos = 0

    while pos < len(text):
        if text.startswith("//", pos):
            newline = text.find("\n", pos + 2)
            pos = len(text) if newline == -1 else newline + 1
            continue

        if text.startswith("/*", pos):
            close = text.find("*/", pos + 2)
            pos = len(text) if close == -1 else close + 2
            continue

        for cast_keyword in ("reinterpret_cast", "static_cast"):
            cast = _named_cast_at(text, pos, cast_keyword)

            if cast is None:
                continue

            cast_end, target = cast

            if _is_character_storage_target(target) and (cast_keyword == "reinterpret_cast" or "*" in target):
                line, column = _line_column(text, pos)
                cast_preview = _preview(f"{cast_keyword}<{target.strip()}>")
                findings.append(
                    Finding(
                        path=path,
                        line=line,
                        column=column,
                        kind="cast",
                        preview=cast_preview,
                        rule="unsafe_character_storage_cast",
                    )
                )

            pos = cast_end
            break
        else:
            cast = None

        if cast is not None:
            continue

        c_style_cast = _c_style_character_pointer_cast_at(text, pos)

        if c_style_cast is not None:
            cast_end, target = c_style_cast
            line, column = _line_column(text, pos)
            findings.append(
                Finding(
                    path=path,
                    line=line,
                    column=column,
                    kind="cast",
                    preview=_preview(f"({target.strip()})"),
                    rule="unsafe_character_storage_cast",
                )
            )
            pos = cast_end
            continue

        literal = _literal_at(text, pos)

        if literal is None:
            pos += 1
            continue

        prefix, quote, quote_offset = literal
        literal_start = pos

        if "R" in prefix:
            raw_result = _scan_raw_literal(text, quote_offset)

            if raw_result is None:
                pos = quote_offset + 1
                continue

            end, content = raw_result
        else:
            end, content = _scan_quoted_literal(text, quote_offset + 1, quote)

        is_ordinary_narrow = prefix == "" or (prefix == "R" and quote == '"')

        if is_ordinary_narrow and any(ord(ch) > 0x7F for ch in content):
            line, column = _line_column(text, literal_start)
            kind = "string" if quote == '"' else "character"
            findings.append(Finding(path=path, line=line, column=column, kind=kind, preview=_preview(content), rule="non_ascii_narrow_literal"))

        if is_ordinary_narrow and "R" not in prefix and _has_non_ascii_escape(content):
            line, column = _line_column(text, literal_start)
            kind = "string" if quote == '"' else "character"
            findings.append(Finding(path=path, line=line, column=column, kind=kind, preview=_preview(content), rule="non_ascii_narrow_escape"))

        pos = max(end, pos + 1)

    return findings


def mask_non_code(text: str) -> str:
    masked = list(text)

    def blank(start: int, end: int) -> None:
        for index in range(start, end):
            if masked[index] not in {"\r", "\n"}:
                masked[index] = " "

    pos = 0

    while pos < len(text):
        if text.startswith("//", pos):
            newline = text.find("\n", pos + 2)
            end = len(text) if newline == -1 else newline
            blank(pos, end)
            pos = end
            continue

        if text.startswith("/*", pos):
            close = text.find("*/", pos + 2)
            end = len(text) if close == -1 else close + 2
            blank(pos, end)
            pos = end
            continue

        literal = _literal_at(text, pos)

        if literal is None:
            pos += 1
            continue

        prefix, quote, quote_offset = literal

        if "R" in prefix:
            raw_result = _scan_raw_literal(text, quote_offset)

            if raw_result is None:
                pos = quote_offset + 1
                continue

            end, _ = raw_result
        else:
            end, _ = _scan_quoted_literal(text, quote_offset + 1, quote)

        blank(pos, end)
        pos = end

    return "".join(masked)


def _is_excluded(relative_path: str, excludes: Iterable[str]) -> bool:
    normalized = relative_path.replace("\\", "/")

    for raw_prefix in excludes:
        prefix = raw_prefix.replace("\\", "/").rstrip("/")

        if normalized == prefix or normalized.startswith(prefix + "/"):
            return True

    return False


def iter_source_files(root: Path, scopes: Iterable[str], excludes: Iterable[str]) -> Iterable[tuple[Path, Path]]:
    for scope_name in scopes:
        scope = root / scope_name

        if not scope.exists():
            continue

        for path in sorted(scope.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
                continue

            relative_path = path.relative_to(root)

            if not _is_excluded(relative_path.as_posix(), excludes):
                yield path, relative_path


def audit(root: Path, scopes: Iterable[str], excludes: Iterable[str]) -> tuple[list[Finding], int, list[str]]:
    findings: list[Finding] = []
    errors: list[str] = []
    file_count = 0

    for path, relative_path in iter_source_files(root, scopes, excludes):
        file_count += 1

        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as ex:
            errors.append(f"{relative_path.as_posix()}:1:1: error: source file is not valid UTF-8: {ex} [invalid_source_utf8]")
            continue

        findings.extend(scan_text(text, relative_path))

    return findings, file_count, errors


def _run_self_test() -> None:
    sample = r'''
// "comment Привет"
/* '\xD0' */
auto utf8 = u8"Привет";
auto utf16 = u"Привет";
auto utf32 = U"Привет";
auto wide = L"Привет";
auto utf8_raw = u8R"tag(Привет)tag";
auto escaped_ascii = "\x7F";
auto bad_string = "Привет";
auto bad_raw = R"tag(мир)tag";
auto bad_char = 'é';
auto adjacent = u8"ok" "данные";
auto explicit_escaped = u8"\xD0\x9F";
auto bad_hex = "\xD0\x9F";
auto bad_octal = "\200";
auto bad_universal = "\u00e9";
auto bad_cast = reinterpret_cast<const char*>(bytes);
auto bad_static_pointer_cast = static_cast<char*>(storage);
auto bad_c_style_cast = (const char*)storage;
auto good_cast = reinterpret_cast<const byte*>(bytes);
'''
    findings = scan_text(sample, Path("sample.cpp"))
    assert [(finding.line, finding.kind, finding.rule) for finding in findings] == [
        (10, "string", "non_ascii_narrow_literal"),
        (11, "string", "non_ascii_narrow_literal"),
        (12, "character", "non_ascii_narrow_literal"),
        (13, "string", "non_ascii_narrow_literal"),
        (15, "string", "non_ascii_narrow_escape"),
        (16, "string", "non_ascii_narrow_escape"),
        (17, "string", "non_ascii_narrow_escape"),
        (18, "cast", "unsafe_character_storage_cast"),
        (19, "cast", "unsafe_character_storage_cast"),
        (20, "cast", "unsafe_character_storage_cast"),
    ]
    masked = mask_non_code(sample)
    assert "comment Привет" not in masked
    assert "bad_string" in masked
    assert '"Привет"' not in masked

    with tempfile.TemporaryDirectory(prefix="text_type_audit_") as temp_dir:
        root = Path(temp_dir)
        (root / "Engine/Source").mkdir(parents=True)
        (root / "Engine/ThirdParty").mkdir(parents=True)
        (root / "Engine/Source/Good.cpp").write_text('auto text = u8"текст";\n', encoding="utf-8")
        (root / "Engine/Source/Bad.cpp").write_text('auto text = "текст";\n', encoding="utf-8")
        (root / "Engine/ThirdParty/Vendored.cpp").write_text('auto text = "текст";\n', encoding="utf-8")
        audited, file_count, errors = audit(root, ["Engine"], DEFAULT_EXCLUDES)
        assert file_count == 2
        assert not errors
        assert len(audited) == 1
        assert audited[0].path.as_posix() == "Engine/Source/Bad.cpp"


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
    parser.add_argument("--self-test", action="store_true", help="Run the scanner's built-in regression tests")
    args = parser.parse_args(argv)

    if args.self_test:
        _run_self_test()
        print("Text type audit self-test passed")
        return 0

    root = args.root.resolve()
    scopes = args.scope or _default_scopes(root)

    if not scopes:
        parser.error("no source scopes found; pass --scope")

    findings, file_count, errors = audit(root, scopes, (*DEFAULT_EXCLUDES, *args.exclude))

    for diagnostic in errors:
        print(diagnostic)
    for finding in findings:
        print(finding.diagnostic())

    if errors or findings:
        print(f"Text type audit failed: {file_count} files, {len(findings)} finding(s), {len(errors)} source error(s)")
        return 1

    print(f"Text type audit passed: {file_count} files, no text-type findings")
    return 0


if __name__ == "__main__":
    sys.exit(main())
