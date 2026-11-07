"""Check native script-binding signatures and their nullability parity with AngelScript.

The native rules follow the Engine nullability contract: a script-facing
native signature spells a non-null handle ``ptr<T>`` and a nullable handle
``nptr<T>``, never a raw ``T*``.  Codegen rejects raw handle pointers during a
build; this check reports every raw declarator earlier, without metadata, in
Engine and project sources.  Script-facing surfaces are ``///@ ExportMethod``
and other ``FO_SCRIPT_API`` declarations (``///@ EngineHook`` declarations are
a separate native ABI), members listed by ``///@ ExportRefType ... Export =``,
``FO_ENTITY_EVENT`` payloads, and ``ScriptFunc``/``FindFunc``/``CheckFunc``
template arguments.  The removed ``FO_NULLABLE`` marker must not return, and a
``ScriptFunc`` stored from an exported callback parameter must spell the same
signature as that parameter.

With ``--scripts`` the check also compares AngelScript handlers with the
declarations they implement, argument by argument: subscribers of native
``///@ ExportEvent`` events and of ``///@ Event`` declarations, ``[[Event]]``
functions named after an event, and ``[[ServerRemoteCall]]`` /
``[[ClientRemoteCall]]`` implementations of ``///@ RemoteCall`` declarations.
The AngelScript compiler does not compare ``?`` markers when it matches a
handler, so a nullable argument could otherwise reach a non-null parameter.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable, Sequence
from dataclasses import dataclass
import fnmatch
from functools import partial
import os
from pathlib import Path
import re
import sys

import angelscript_source
from check_native_source_layout import scan_source
from finding_baseline import (BaselineError, Finding, apply_baseline, format_finding, load_baselines,
                              sort_findings, write_baseline)


ENGINE_ROOT = Path(__file__).resolve().parents[1]

RULES = {
    "raw-pointer": "raw T* in a script-facing native signature; use ptr<T> or nptr<T>",
    "obsolete-nullable-marker": "the removed FO_NULLABLE marker",
    "stored-callback-signature": "a stored ScriptFunc differs from the exported callback parameter",
    "event-handler-nullability": "an event handler disagrees with the event declaration about null",
    "remote-call-handler-nullability": "a remote-call handler disagrees with the declaration about null",
}
NATIVE_SUFFIXES = frozenset({".h", ".hpp", ".cpp", ".cc", ".cxx"})
SCRIPT_SUFFIXES = frozenset({".fos"})

_RAW_POINTER_RE = re.compile(
    r"(?<![\w:])(?:(?:const|volatile)\s+)*"
    r"(?P<type>[A-Za-z_]\w*(?:::\w+)*)\s*\*"
    r"(?=\s*(?:const\b|volatile\b|[A-Za-z_]\w*|[,>&)\[\];={}]|$))")
_SCRIPT_API_RE = re.compile(r"\bFO_SCRIPT_API\b")
_NULLABLE_MARKER_RE = re.compile(r"\bFO_NULLABLE\b")
_TEMPLATE_RE = re.compile(r"\b(?P<name>ScriptFunc|FindFunc|CheckFunc)\s*<")
_ENTITY_EVENT_RE = re.compile(r"\bFO_ENTITY_EVENT\s*\(")
_REF_TYPE_TAG_RE = re.compile(r"^[ \t]*///@[ \t]*ExportRefType\b(?P<flags>[^\n]*)", re.MULTILINE)
_CLASS_OPEN_RE = re.compile(r"\b(?:class|struct)\s+[A-Za-z_]\w*[^;{]*\{")
_DEFINE_LINE_RE = re.compile(r"[ \t]*#[ \t]*define\b")
_STORED_CALLBACK_RE = re.compile(r"\s*(?:>\s*)*(?:\(|[A-Za-z_]\w*\s*\{)\s*std::move\s*\(\s*(?P<name>[A-Za-z_]\w*)\s*\)")
_SCRIPT_EVENT_TAG_RE = re.compile(
    r"^[ \t]*///@[ \t]*Event[ \t]+(?P<target>\w+)[ \t]+(?P<entity>\w+)[ \t]+(?P<name>\w+)[ \t]*\((?P<args>[^)\n]*)\)",
    re.MULTILINE)
_REMOTE_CALL_TAG_RE = re.compile(
    r"^[ \t]*///@[ \t]*RemoteCall[ \t]+(?P<target>Server|Client)[ \t]+(?P<name>\w+)[ \t]*\((?P<args>[^)\n]*)\)",
    re.MULTILINE)
_SUBSCRIBE_RE = re.compile(
    r"\.\s*(?P<event>[A-Za-z_]\w*)\s*\.\s*Subscribe\s*\(\s*(?P<func>(?:[A-Za-z_]\w*\s*::\s*)*[A-Za-z_]\w*)\s*[,)]")
_DRIVE_RE = re.compile(r"^[A-Za-z]:")


@dataclass(frozen=True)
class Declaration:
    kind: str
    name: str
    target: str
    nullable: tuple[bool, ...]
    location: str


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="check_script_bindings.py",
        description="Check script-facing native signatures for ptr/nptr spelling and compare AngelScript "
                    "handlers with event and remote-call declarations")
    parser.add_argument("--root", type=Path,
                        help="repository root that scopes and reported paths are relative to "
                             "(default: the Engine root that contains this script)")
    parser.add_argument("--scope", action="append", default=[], metavar="DIR",
                        help="root-relative native source directory; repeatable (default: Source)")
    parser.add_argument("--scripts", action="append", default=[], metavar="DIR",
                        help="root-relative AngelScript directory whose handlers are compared; repeatable")
    parser.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                        help="fnmatch pattern for root-relative POSIX paths to skip; repeatable")
    parser.add_argument("--baseline", action="append", type=Path, default=[], metavar="TSV",
                        help="accepted findings (rule, path, count, reason TSV); repeatable")
    parser.add_argument("--write-baseline", type=Path, metavar="TSV",
                        help="write current finding counts with empty reasons instead of gating")
    parser.add_argument("--list-rules", action="store_true", help="print rule identifiers and exit")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)

    if args.list_rules:
        for rule, description in RULES.items():
            print(f"{rule}\t{description}")
        return 0

    root = (args.root or ENGINE_ROOT).resolve()
    scopes = args.scope or ["Source"]
    findings, errors = check_bindings(root, scopes, args.scripts, args.exclude)

    if errors:
        for error in errors:
            print(f"[ScriptBindings] {error}", file=sys.stderr)
        return 2

    if args.write_baseline is not None:
        rows = write_baseline(args.write_baseline, findings)
        print(f"[ScriptBindings] wrote {rows} baseline row(s) to {args.write_baseline}; add a reason to every row")
        return 0

    try:
        baseline = load_baselines(args.baseline)
    except BaselineError as error:
        print(f"[ScriptBindings] {error}", file=sys.stderr)
        return 2

    remaining, stale = apply_baseline(findings, baseline)
    for finding in remaining:
        print(format_finding(finding))

    for diagnostic in stale:
        print(diagnostic)

    print(f"[ScriptBindings] {len(findings)} finding(s), {len(remaining)} above the baseline, "
          f"{len(stale)} stale baseline row(s)")

    return 1 if remaining or stale else 0


def check_bindings(root: Path, scopes: Sequence[str], scripts: Sequence[str] = (),
                   excludes: Sequence[str] = ()) -> tuple[list[Finding], list[str]]:
    """Return binding findings for the native scopes and optional script directories."""

    native_files, errors = collect_files(root, scopes, NATIVE_SUFFIXES, excludes)
    script_files, script_errors = collect_files(root, scripts, SCRIPT_SUFFIXES, excludes)
    errors.extend(script_errors)

    if errors:
        return [], errors

    findings: list[Finding] = []
    declarations: list[Declaration] = []

    for rel in native_files:
        text = _read_text(root / rel, rel, errors)

        if text is None:
            continue

        file_findings, file_declarations = check_native_text(rel, text)
        findings.extend(file_findings)
        declarations.extend(file_declarations)

    if scripts:
        sources: dict[str, str] = {}

        for rel in script_files:
            text = _read_text(root / rel, rel, errors)

            if text is not None:
                sources[rel] = text

        findings.extend(check_script_parity(declarations, sources))

    return sort_findings(findings), errors


def collect_files(root: Path, directories: Sequence[str], suffixes: frozenset[str],
                  excludes: Sequence[str]) -> tuple[list[str], list[str]]:
    """Return sorted root-relative files below *directories* and scope errors."""

    files: set[str] = set()
    errors: list[str] = []

    for directory in directories:
        rel_dir = _normalize_relative(directory)

        if rel_dir is None:
            errors.append(f"{directory}: scope must be a root-relative path without '..' segments")
            continue

        base = root if rel_dir == "." else root.joinpath(*rel_dir.split("/"))

        if not base.is_dir():
            errors.append(f"{rel_dir}: scope directory does not exist")
            continue

        for current, children, names in os.walk(base):
            children.sort()
            current_path = Path(current)

            for name in sorted(names):
                path = current_path / name

                if path.suffix not in suffixes:
                    continue

                rel = path.relative_to(root).as_posix()

                if not any(fnmatch.fnmatchcase(rel, pattern) for pattern in excludes):
                    files.add(rel)

    return sorted(files), errors


def check_native_text(rel: str, text: str) -> tuple[list[Finding], list[Declaration]]:
    """Check one native file and return its findings and exported event declarations."""

    source = text.replace("\r\n", "\n")
    masked, spans = scan_source(source)
    comment_starts = {start for start, _ in spans}
    lines = source.split("\n")
    findings: list[Finding] = []
    reported: set[int] = set()

    def report_raw(fragment: str, base: int, surface: str) -> None:
        for match in _RAW_POINTER_RE.finditer(fragment):
            offset = base + match.start()

            if offset in reported:
                continue

            reported.add(offset)
            findings.append(Finding(
                "raw-pointer", rel, _line_of(source, offset),
                f"raw pointer '{match.group('type')}*' in {surface}; use ptr<T> for a non-null "
                f"or nptr<T> for a nullable script handle"))

    for match in _SCRIPT_API_RE.finditer(masked):
        if _is_define_line(masked, match.start()):
            continue

        owner = _previous_nonblank_line(lines, _line_of(source, match.start()))

        # Engine hooks use a separate native ABI that scripts never call
        if re.match(r"///@\s*EngineHook\b", owner):
            continue

        is_export = re.match(r"///@\s*ExportMethod\b", owner) is not None
        surface = "an ExportMethod signature" if is_export else "an FO_SCRIPT_API signature"
        signature_end = _signature_end(masked, match.end())
        report_raw(masked[match.end():signature_end], match.end(), surface)
        findings.extend(_check_stored_callbacks(rel, source, masked, match.end(), signature_end))

    for tag in _REF_TYPE_TAG_RE.finditer(source):
        if source.index("///@", tag.start()) not in comment_starts:
            continue

        _check_ref_type(source, masked, tag, report_raw)

    for match in _ENTITY_EVENT_RE.finditer(masked):
        if _is_define_line(masked, match.start()):
            continue

        close = _matching(masked, match.end() - 1, "(", ")")

        if close < 0:
            continue

        report_raw(masked[match.end():close], match.end(), "an FO_ENTITY_EVENT payload")

    for match in _TEMPLATE_RE.finditer(masked):
        if _is_define_line(masked, match.start()):
            continue

        close = _matching(masked, match.end() - 1, "<", ">")

        if close < 0:
            continue

        report_raw(masked[match.end():close], match.end(), f"{match.group('name')} template arguments")

    for match in _NULLABLE_MARKER_RE.finditer(masked):
        findings.append(Finding(
            "obsolete-nullable-marker", rel, _line_of(source, match.start()),
            "the FO_NULLABLE marker was removed; spell the handle ptr<T> or nptr<T>"))

    tags = _tag_declarations(rel, source, lambda offset: offset in comment_starts)

    return findings, _native_events(rel, source, masked, lines) + tags


def check_script_parity(native_declarations: Sequence[Declaration],
                        sources: dict[str, str]) -> list[Finding]:
    """Compare AngelScript handlers with event and remote-call declarations."""

    declarations = list(native_declarations)
    functions: list[tuple[str, angelscript_source.ScriptFunction]] = []
    subscriptions: list[tuple[str, int, str, str, str]] = []

    for rel, text in sorted(sources.items()):
        source = text.replace("\r\n", "\n")
        kinds = angelscript_source.classify(source)
        code = angelscript_source.mask(source, kinds)
        declarations.extend(_tag_declarations(rel, source, partial(_starts_line_comment, kinds)))
        functions.extend((rel, function) for function in angelscript_source.find_functions(code))
        matches = list(_SUBSCRIBE_RE.finditer(code))
        namespaces = angelscript_source.namespace_at(code, [match.start() for match in matches])

        for match in matches:
            func = re.sub(r"\s+", "", match.group("func"))
            subscriptions.append((rel, angelscript_source.line_of(code, match.start()), match.group("event"),
                                  func, namespaces[match.start()]))

    events: dict[str, list[Declaration]] = {}
    remote_calls: dict[tuple[str, str], list[Declaration]] = {}

    for declaration in declarations:
        if declaration.kind == "event":
            events.setdefault(declaration.name, []).append(declaration)
        else:
            remote_calls.setdefault((declaration.target, declaration.name), []).append(declaration)

    findings: list[Finding] = []
    checked: set[tuple[str, int, str]] = set()

    def compare_event(rel: str, function: angelscript_source.ScriptFunction, event: str) -> None:
        key = (rel, function.line, event)

        if key in checked:
            return

        checked.add(key)
        expected = _expected_event_bits(events[event], len(function.parameters))

        if expected is None:
            return

        bits, location = expected
        mismatches = _mismatches(function, bits)

        if mismatches:
            findings.append(Finding(
                "event-handler-nullability", rel, function.line,
                f"handler '{function.name}' for event {event} disagrees with the declaration at {location}: "
                + "; ".join(mismatches)))

    for rel, function in functions:
        if "Event" in function.attributes and function.name in events:
            compare_event(rel, function, function.name)

    for rel, _line, event, func, namespace in subscriptions:
        if event not in events:
            continue

        for function_rel, function in _resolve_functions(functions, func, namespace):
            compare_event(function_rel, function, event)

    for rel, function in functions:
        for attribute, target, implicit in (("ServerRemoteCall", "Server", 1), ("ClientRemoteCall", "Client", 0)):
            candidates = remote_calls.get((target, function.name))

            if attribute not in function.attributes or not candidates:
                continue

            variants = {candidate.nullable for candidate in candidates}

            if len(variants) != 1:
                continue

            bits = (False,) * implicit + candidates[0].nullable

            if len(bits) != len(function.parameters):
                continue

            mismatches = _mismatches(function, bits)

            if mismatches:
                findings.append(Finding(
                    "remote-call-handler-nullability", rel, function.line,
                    f"[[{attribute}]] '{function.name}' disagrees with the declaration at "
                    f"{candidates[0].location}: " + "; ".join(mismatches)))

    return findings


def _check_ref_type(source: str, masked: str, tag: re.Match[str],
                    report_raw: Callable[[str, int, str], None]) -> None:
    export = re.search(r"\bExport\s*=\s*(?P<names>.*)$", tag.group("flags"))

    if export is None:
        return

    names = set(re.findall(r"[A-Za-z_]\w*", export.group("names")))
    class_match = _CLASS_OPEN_RE.search(masked, tag.end())

    if not names or class_match is None:
        return

    open_brace = class_match.end() - 1
    close_brace = _matching(masked, open_brace, "{", "}")

    if close_brace < 0:
        return

    offset = open_brace + 1

    for line in masked[open_brace + 1:close_brace].splitlines(keepends=True):
        member = ""

        for method in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", line):
            if method.group(1) in names:
                member = method.group(1)
                break

        if not member:
            for field in sorted(names):
                if re.search(rf"\b{re.escape(field)}\b\s*(?:\{{|=|;)", line):
                    member = field
                    break

        if member:
            report_raw(line, offset, f"exported ExportRefType member '{member}'")

        offset += len(line)


def _check_stored_callbacks(rel: str, source: str, masked: str, start: int, signature_end: int) -> list[Finding]:
    open_paren = masked.find("(", start, signature_end)

    if open_paren < 0:
        return []

    parameters: dict[str, str] = {}

    for piece in angelscript_source.split_top_level(masked[open_paren + 1:signature_end - 1]):
        declaration = piece.strip()

        if not declaration.startswith("ScriptFunc<"):
            continue

        separator = declaration.rfind(" ")

        if separator > 0:
            parameters[declaration[separator + 1:]] = _normalize_template(declaration[:separator])

    if not parameters:
        return []

    body_open = masked.find("{", signature_end)
    semicolon = masked.find(";", signature_end)

    if body_open < 0 or (0 <= semicolon < body_open):
        return []

    body_close = _matching(masked, body_open, "{", "}")
    body_close = len(masked) if body_close < 0 else body_close
    findings: list[Finding] = []

    for match in _TEMPLATE_RE.finditer(masked, body_open, body_close):
        if match.group("name") != "ScriptFunc":
            continue

        close = _matching(masked, match.end() - 1, "<", ">")

        if close < 0:
            continue

        stored = _STORED_CALLBACK_RE.match(masked, close + 1)

        if stored is None or stored.group("name") not in parameters:
            continue

        stored_type = _normalize_template(masked[match.start():close + 1])
        exported_type = parameters[stored.group("name")]

        if stored_type != exported_type:
            findings.append(Finding(
                "stored-callback-signature", rel, _line_of(source, match.start()),
                f"stored {stored_type} differs from the exported parameter "
                f"{stored.group('name')} declared as {exported_type}"))

    return findings


def _native_events(rel: str, source: str, masked: str, lines: list[str]) -> list[Declaration]:
    declarations: list[Declaration] = []

    for match in _ENTITY_EVENT_RE.finditer(masked):
        if _is_define_line(masked, match.start()):
            continue

        line = _line_of(source, match.start())

        if not re.match(r"///@\s*ExportEvent\b", _previous_nonblank_line(lines, line)):
            continue

        close = _matching(masked, match.end() - 1, "(", ")")

        if close < 0:
            continue

        pieces = angelscript_source.split_top_level(masked[match.end():close])

        if not pieces or not pieces[0].strip():
            continue

        nullable = tuple(piece.strip().startswith("nptr<") for piece in pieces[1:] if piece.strip())
        declarations.append(Declaration("event", pieces[0].strip(), "", nullable, f"{rel}:{line}"))

    return declarations


def _tag_declarations(rel: str, source: str, is_comment_start: Callable[[int], bool]) -> list[Declaration]:
    """Read ``///@ Event`` and ``///@ RemoteCall`` tags that start a real line comment."""

    declarations: list[Declaration] = []

    for pattern, kind in ((_SCRIPT_EVENT_TAG_RE, "event"), (_REMOTE_CALL_TAG_RE, "remote-call")):
        for match in pattern.finditer(source):
            if not is_comment_start(source.index("///@", match.start())):
                continue

            parameters = angelscript_source.parse_parameters(match.group("args"))
            target = match.group("target") if kind == "remote-call" else ""
            declarations.append(Declaration(kind, match.group("name"), target,
                                            tuple(parameter.nullable for parameter in parameters),
                                            f"{rel}:{_line_of(source, match.start())}"))

    return declarations


def _starts_line_comment(kinds: str, offset: int) -> bool:
    return (kinds[offset] == angelscript_source.LINE_COMMENT
            and (offset == 0 or kinds[offset - 1] != angelscript_source.LINE_COMMENT))


def _expected_event_bits(candidates: list[Declaration], count: int) -> tuple[tuple[bool, ...], str] | None:
    expected: set[tuple[bool, ...]] = set()
    location = ""

    for candidate in candidates:
        if len(candidate.nullable) == count:
            expected.add(candidate.nullable)
            location = location or candidate.location
        elif len(candidate.nullable) + 1 == count:
            # Entity events pass the non-null owning entity before the declared arguments
            expected.add((False,) + candidate.nullable)
            location = location or candidate.location

    if len(expected) != 1:
        return None

    return expected.pop(), location


def _mismatches(function: angelscript_source.ScriptFunction, bits: tuple[bool, ...]) -> list[str]:
    result: list[str] = []

    for index, (parameter, declared) in enumerate(zip(function.parameters, bits), 1):
        if parameter.nullable != declared:
            name = f" '{parameter.name}'" if parameter.name else ""
            result.append(
                f"argument {index}{name} is declared {'nullable' if declared else 'non-null'} "
                f"but the handler takes {'nullable' if parameter.nullable else 'non-null'} {parameter.type}")

    return result


def _resolve_functions(functions: list[tuple[str, angelscript_source.ScriptFunction]], reference: str,
                       namespace: str) -> list[tuple[str, angelscript_source.ScriptFunction]]:
    qualifier, _, name = reference.rpartition("::")
    named = [(rel, function) for rel, function in functions if function.name == name]

    if qualifier:
        return [(rel, function) for rel, function in named
                if function.namespace == qualifier or function.namespace.endswith("::" + qualifier)]

    local = [(rel, function) for rel, function in named if function.namespace == namespace]

    return local or [(rel, function) for rel, function in named if function.namespace == ""]


def _signature_end(masked: str, start: int) -> int:
    open_paren = masked.find("(", start)
    stop = masked.find(";", start)
    brace = masked.find("{", start)

    for limit in (stop, brace):
        if 0 <= limit < open_paren:
            return limit

    if open_paren < 0:
        end = masked.find("\n", start)
        return len(masked) if end < 0 else end

    close = _matching(masked, open_paren, "(", ")")

    return len(masked) if close < 0 else close + 1


def _matching(text: str, start: int, opening: str, closing: str) -> int:
    depth = 0

    for index in range(start, len(text)):
        char = text[index]

        if char == opening:
            depth += 1
        elif char == closing:
            depth -= 1

            if depth == 0:
                return index

    return -1


def _normalize_template(text: str) -> str:
    return re.sub(r"\s+", "", text)


def _is_define_line(masked: str, offset: int) -> bool:
    line_start = masked.rfind("\n", 0, offset) + 1

    return _DEFINE_LINE_RE.match(masked, line_start) is not None


def _previous_nonblank_line(lines: list[str], line: int) -> str:
    for index in range(line - 2, -1, -1):
        text = lines[index].strip()

        if text:
            return text

    return ""


def _line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _read_text(path: Path, rel: str, errors: list[str]) -> str | None:
    try:
        return path.read_bytes().decode("utf-8-sig")
    except (OSError, UnicodeDecodeError) as error:
        errors.append(f"{rel}: unable to read file as UTF-8: {error}")
        return None


def _normalize_relative(value: str) -> str | None:
    text = os.fspath(value).replace("\\", "/")

    if not text or "\x00" in text or text.startswith("/") or _DRIVE_RE.match(text):
        return None

    parts = [part for part in text.split("/") if part not in ("", ".")]

    if ".." in parts:
        return None

    return "/".join(parts) if parts else "."


if __name__ == "__main__":
    raise SystemExit(main())
