#!/usr/bin/env python3
"""Clang-based checks for redundant local const and use-after-move."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Iterable, Sequence


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_ENGINE_ROOT = SCRIPT_DIR.parents[1]
QUERY_TEMPLATE = SCRIPT_DIR / "redundant_local_const.query.in"
SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx"}
EXCLUDED_PARTS = {"ThirdParty", "third_party", "Generated", "generated"}
SUPPRESSION_MARKERS = {
    "fo-redundant-local-const": "FO_REDUNDANT_CONST_SUPPRESS:",
    "fo-use-after-move": "FO_USE_AFTER_MOVE_SUPPRESS:",
}


@dataclass(frozen=True, order=True)
class SourceLocation:
    path: str
    line: int
    column: int
    absolute_path: str = field(compare=False)


@dataclass
class Diagnostic:
    code: str
    severity: str
    message: str
    location: SourceLocation
    variable: str = ""
    variable_type: str = ""
    fix_location: SourceLocation | None = None
    notes: list[str] = field(default_factory=list)
    suppressed: bool = False
    suppression_reason: str = ""

    @property
    def key(self) -> tuple[str, str, int, int, str]:
        return (self.code, self.location.path, self.location.line, self.location.column, self.variable)


@dataclass(frozen=True)
class CompilationUnit:
    database_path: Path
    canonical_path: str


@dataclass(frozen=True)
class NodeBinding:
    label: str
    kind: str
    ast_id: str
    first_line: str
    text: str
    location: SourceLocation | None
    declarator_location: SourceLocation | None
    name: str
    type_name: str


@dataclass
class AnalysisResult:
    units: list[CompilationUnit]
    diagnostics: list[Diagnostic]
    tool_versions: dict[str, str]


def _normalize_path(path: Path) -> Path:
    try:
        return path.resolve(strict=False)
    except OSError:
        return path.absolute()


class SourcePathMapper:
    def __init__(self, source_root: Path) -> None:
        self.source_root = _normalize_path(source_root)

    def map_path(self, raw_path: str | Path) -> tuple[str, Path] | None:
        path = _normalize_path(Path(raw_path))
        try:
            relative = path.relative_to(self.source_root)
            return self._display_path(relative), self.source_root / relative
        except ValueError:
            pass

        normalized = str(path).replace("\\", "/")
        if self.source_root.name == "Source":
            marker = "/Engine/Source/"
            marker_index = normalized.lower().rfind(marker.lower())
            if marker_index >= 0:
                relative = Path(*normalized[marker_index + len(marker) :].split("/"))
                return self._display_path(relative), self.source_root / relative
        return None

    def location(self, raw_path: str, line: int, column: int) -> SourceLocation | None:
        mapped = self.map_path(raw_path)
        if mapped is None:
            return None
        display, absolute = mapped
        return SourceLocation(display, line, column, str(absolute))

    def canonical_for_database_file(self, path: Path) -> str | None:
        mapped = self.map_path(path)
        return mapped[0] if mapped is not None else None

    def _display_path(self, relative: Path) -> str:
        if self.source_root.name == "Source":
            return (Path("Source") / relative).as_posix()
        return relative.as_posix()


def _source_location_from_node_line(line: str, mapper: SourcePathMapper) -> SourceLocation | None:
    match = re.search(r"<(.+?):(\d+):(\d+)(?:,|>)", line)
    if match is None:
        return None
    return mapper.location(match.group(1), int(match.group(2)), int(match.group(3)))


def _declarator_location_from_node_line(line: str, mapper: SourcePathMapper) -> SourceLocation | None:
    start = _source_location_from_node_line(line, mapper)
    if start is None:
        return None

    range_match = re.search(r"<(.+?)>\s+", line)
    if range_match is None:
        return start
    range_end = range_match.group(1).rsplit(",", maxsplit=1)[-1].strip()
    base = start
    absolute_end = re.fullmatch(r"(.+?):(\d+):(\d+)", range_end)
    relative_line_end = re.fullmatch(r"line:(\d+):(\d+)", range_end)
    relative_column_end = re.fullmatch(r"col:(\d+)", range_end)
    if absolute_end is not None:
        base = mapper.location(absolute_end.group(1), int(absolute_end.group(2)), int(absolute_end.group(3))) or base
    elif relative_line_end is not None:
        base = mapper.location(base.absolute_path, int(relative_line_end.group(1)), int(relative_line_end.group(2))) or base
    elif relative_column_end is not None:
        base = mapper.location(base.absolute_path, base.line, int(relative_column_end.group(1))) or base

    suffix = line[range_match.end() :]
    absolute_name = re.match(r"(.+?):(\d+):(\d+)\b", suffix)
    relative_line_name = re.match(r"line:(\d+):(\d+)\b", suffix)
    relative_column_name = re.match(r"col:(\d+)\b", suffix)
    if absolute_name is not None:
        return mapper.location(absolute_name.group(1), int(absolute_name.group(2)), int(absolute_name.group(3))) or base
    if relative_line_name is not None:
        return mapper.location(base.absolute_path, int(relative_line_name.group(1)), int(relative_line_name.group(2))) or base
    if relative_column_name is not None:
        return mapper.location(base.absolute_path, base.line, int(relative_column_name.group(1))) or base
    return base


def _decl_name_and_type(line: str) -> tuple[str, str]:
    matches = re.findall(r"\b(?:used\s+|referenced\s+)?([A-Za-z_]\w*)\s+'([^']*)'", line)
    return matches[-1] if matches else ("", "")


def _parse_node(label: str, lines: Sequence[str], mapper: SourcePathMapper) -> NodeBinding | None:
    if not lines:
        return None
    first_line = lines[0]
    identity = re.match(r"(?:[|`\- ]*)([A-Za-z][A-Za-z0-9_]*)\s+(0x[0-9a-fA-F]+)\b", first_line)
    if identity is None:
        return None
    kind, ast_id = identity.groups()
    name, type_name = _decl_name_and_type(first_line)
    return NodeBinding(
        label=label,
        kind=kind,
        ast_id=ast_id,
        first_line=first_line,
        text="\n".join(lines),
        location=_source_location_from_node_line(first_line, mapper),
        declarator_location=_declarator_location_from_node_line(first_line, mapper),
        name=name,
        type_name=type_name,
    )


def parse_query_matches(output: str, mapper: SourcePathMapper) -> list[dict[str, NodeBinding]]:
    matches: list[dict[str, NodeBinding]] = []
    current_match: dict[str, NodeBinding] | None = None
    current_label: str | None = None
    current_lines: list[str] = []

    def finish_binding() -> None:
        nonlocal current_label, current_lines
        if current_match is not None and current_label is not None:
            node = _parse_node(current_label, current_lines, mapper)
            if node is not None:
                current_match[current_label] = node
        current_label = None
        current_lines = []

    def finish_match() -> None:
        nonlocal current_match
        finish_binding()
        if current_match:
            matches.append(current_match)
        current_match = None

    for raw_line in output.splitlines():
        line = raw_line.rstrip()
        if line.startswith("Match #"):
            finish_match()
            current_match = {}
            continue
        binding_match = re.match(r'Binding for "([^"]+)":', line)
        if binding_match is not None:
            finish_binding()
            current_label = binding_match.group(1)
            continue
        if current_label is not None:
            if not line:
                finish_binding()
            else:
                current_lines.append(line)
    finish_match()
    return matches


def parse_redundant_const_output(output: str, mapper: SourcePathMapper) -> list[Diagnostic]:
    diagnostics: dict[tuple[str, str, int, int, str], Diagnostic] = {}
    for match in parse_query_matches(output, mapper):
        node = match.get("top_level_const_local")
        if node is None or node.kind != "VarDecl":
            continue
        location = node.declarator_location or node.location
        if location is None:
            continue
        diagnostic = Diagnostic(
            code="fo-redundant-local-const",
            severity="warning",
            message=f"top-level const is redundant on local variable '{node.name}'",
            location=location,
            variable=node.name,
            variable_type=node.type_name,
            fix_location=_find_top_level_const_location(node),
        )
        diagnostics[diagnostic.key] = diagnostic
    return list(diagnostics.values())


def _location_to_byte_offset(content: bytes, location: SourceLocation) -> int:
    if location.line < 1 or location.column < 1:
        raise RuntimeError(f"Invalid source location: {location.path}:{location.line}:{location.column}")
    line_start = 0
    for _ in range(location.line - 1):
        newline = content.find(b"\n", line_start)
        if newline < 0:
            raise RuntimeError(f"Source location is outside the file: {location.path}:{location.line}:{location.column}")
        line_start = newline + 1
    offset = line_start + location.column - 1
    if offset > len(content):
        raise RuntimeError(f"Source location is outside the file: {location.path}:{location.line}:{location.column}")
    return offset


def _byte_offset_to_location(content: bytes, offset: int, template: SourceLocation) -> SourceLocation:
    line = content.count(b"\n", 0, offset) + 1
    line_start = content.rfind(b"\n", 0, offset) + 1
    return SourceLocation(template.path, line, offset - line_start + 1, template.absolute_path)


def _find_top_level_const_offsets(content: bytes) -> list[int]:
    candidates: list[tuple[int, int]] = []
    angle_depth = 0
    paren_depth = 0
    bracket_depth = 0
    brace_depth = 0
    index = 0

    while index < len(content):
        if content.startswith(b"//", index):
            newline = content.find(b"\n", index + 2)
            index = len(content) if newline < 0 else newline + 1
            continue
        if content.startswith(b"/*", index):
            comment_end = content.find(b"*/", index + 2)
            index = len(content) if comment_end < 0 else comment_end + 2
            continue

        byte = content[index]
        if byte in {ord('"'), ord("'")}:
            quote = byte
            index += 1
            while index < len(content):
                if content[index] == ord("\\"):
                    index += 2
                    continue
                if content[index] == quote:
                    index += 1
                    break
                index += 1
            continue

        if (ord("a") <= byte <= ord("z")) or (ord("A") <= byte <= ord("Z")) or byte == ord("_"):
            token_start = index
            index += 1
            while index < len(content):
                token_byte = content[index]
                if not (
                    (ord("a") <= token_byte <= ord("z"))
                    or (ord("A") <= token_byte <= ord("Z"))
                    or (ord("0") <= token_byte <= ord("9"))
                    or token_byte == ord("_")
                ):
                    break
                index += 1
            if content[token_start:index] == b"const":
                candidates.append((angle_depth + paren_depth + bracket_depth + brace_depth, token_start))
            continue

        if byte == ord("<"):
            angle_depth += 1
        elif byte == ord(">"):
            angle_depth = max(0, angle_depth - 1)
        elif byte == ord("("):
            paren_depth += 1
        elif byte == ord(")"):
            paren_depth = max(0, paren_depth - 1)
        elif byte == ord("["):
            bracket_depth += 1
        elif byte == ord("]"):
            bracket_depth = max(0, bracket_depth - 1)
        elif byte == ord("{"):
            brace_depth += 1
        elif byte == ord("}"):
            brace_depth = max(0, brace_depth - 1)
        index += 1

    if not candidates:
        return []
    minimum_depth = min(depth for depth, _ in candidates)
    return [offset for depth, offset in candidates if depth == minimum_depth]


def _find_top_level_const_location(node: NodeBinding) -> SourceLocation | None:
    if node.location is None or node.declarator_location is None:
        return None
    if node.location.absolute_path != node.declarator_location.absolute_path:
        return None
    try:
        content = Path(node.location.absolute_path).read_bytes()
        declaration_start = _location_to_byte_offset(content, node.location)
        declarator_start = _location_to_byte_offset(content, node.declarator_location)
    except OSError:
        return None
    offsets = (
        _find_top_level_const_offsets(content[declaration_start:declarator_start])
        if declaration_start <= declarator_start
        else []
    )
    if not offsets and node.name:
        line_start = content.rfind(b"\n", 0, declaration_start) + 1
        line_end = content.find(b"\n", declaration_start)
        if line_end < 0:
            line_end = len(content)
        encoded_name = node.name.encode("utf-8")
        name_match = re.search(rb"\b" + re.escape(encoded_name) + rb"\b", content[declaration_start:line_end])
        if name_match is not None:
            declarator_start = declaration_start + name_match.start()
            offsets = _find_top_level_const_offsets(content[line_start:declarator_start])
            declaration_start = line_start
    if not offsets:
        return None
    offset = declaration_start + offsets[-1]
    return _byte_offset_to_location(content, offset, node.location)


def apply_redundant_const_fixes(diagnostics: Sequence[Diagnostic]) -> int:
    active = [
        diagnostic
        for diagnostic in diagnostics
        if diagnostic.code == "fo-redundant-local-const" and not diagnostic.suppressed
    ]
    missing = [diagnostic for diagnostic in active if diagnostic.fix_location is None]
    if missing:
        locations = ", ".join(
            f"{diagnostic.location.path}:{diagnostic.location.line}:{diagnostic.location.column}"
            for diagnostic in missing[:5]
        )
        raise RuntimeError(f"Unable to locate the top-level const token for {len(missing)} diagnostic(s): {locations}")

    edits_by_path: dict[str, set[int]] = {}
    for diagnostic in active:
        assert diagnostic.fix_location is not None
        content = Path(diagnostic.fix_location.absolute_path).read_bytes()
        offset = _location_to_byte_offset(content, diagnostic.fix_location)
        edits_by_path.setdefault(diagnostic.fix_location.absolute_path, set()).add(offset)

    applied = 0
    for raw_path, offsets in edits_by_path.items():
        path = Path(raw_path)
        content = path.read_bytes()
        for offset in sorted(offsets, reverse=True):
            if content[offset : offset + len(b"const")] != b"const":
                raise RuntimeError(f"Expected const token at {path}:{offset}")
            start = offset
            end = offset + len(b"const")
            if end < len(content) and content[end : end + 1] in {b" ", b"\t"}:
                end += 1
            elif start > 0 and content[start - 1 : start] in {b" ", b"\t"}:
                start -= 1
            content = content[:start] + content[end:]
            applied += 1
        path.write_bytes(content)
    return applied


def discover_tool(explicit: str | None, environment_name: str, names: Sequence[str]) -> Path:
    candidates: list[Path] = []
    configured = explicit or os.environ.get(environment_name)
    if configured:
        candidates.append(Path(configured))
    for name in names:
        found = shutil.which(name)
        if found:
            candidates.append(Path(found))
    if os.name == "nt":
        program_files = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        for name in names:
            candidates.append(program_files / "LLVM" / "bin" / f"{name}.exe")
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise RuntimeError(f"Unable to find {', '.join(names)}; set {environment_name} to the matching Clang tool")


def tool_version(tool: Path) -> tuple[int, str]:
    process = subprocess.run(
        [str(tool), "--version"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    output = (process.stdout + process.stderr).strip()
    match = re.search(r"(?:clang|LLVM) version\s+(\d+)", output, re.IGNORECASE)
    if match is None:
        raise RuntimeError(f"Unable to determine Clang version from {tool}: {output}")
    description = next(
        (line.strip() for line in output.splitlines() if re.search(r"(?:clang|LLVM) version", line, re.IGNORECASE)),
        output.splitlines()[0],
    )
    return int(match.group(1)), description


def require_supported_tool(name: str, tool: Path) -> str:
    major, description = tool_version(tool)
    if major < 20:
        raise RuntimeError(f"{name} {major} is unsupported; FOnline requires Clang 20 or newer")
    return description


def load_compilation_units(database_dir: Path, mapper: SourcePathMapper) -> list[CompilationUnit]:
    database_file = database_dir / "compile_commands.json"
    if not database_file.is_file():
        raise RuntimeError(f"Compilation database not found: {database_file}")
    payload = json.loads(database_file.read_text(encoding="utf-8"))
    preferred: dict[str, tuple[int, CompilationUnit]] = {}
    for entry in payload:
        raw_file = Path(entry["file"])
        if not raw_file.is_absolute():
            raw_file = Path(entry["directory"]) / raw_file
        canonical = mapper.canonical_for_database_file(raw_file)
        if canonical is None or raw_file.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        if any(part in EXCLUDED_PARTS for part in Path(canonical).parts):
            continue
        command_text = entry.get("command") or " ".join(entry.get("arguments", []))
        priority = 2 if "RelWithDebInfo" in command_text else 1 if "Release" in command_text else 0
        unit = CompilationUnit(_normalize_path(raw_file), canonical)
        current = preferred.get(canonical)
        if current is None or priority > current[0]:
            preferred[canonical] = (priority, unit)
    return [item[1] for item in sorted(preferred.values(), key=lambda value: value[1].canonical_path)]


def build_query_file(source_pattern: str) -> tempfile.NamedTemporaryFile:
    template = QUERY_TEMPLATE.read_text(encoding="utf-8")
    escaped_pattern = source_pattern.replace("\\", "\\\\").replace('"', '\\"')
    content = template.replace("@SOURCE_FILTER@", f'isExpansionInFileMatching("{escaped_pattern}")')
    temporary = tempfile.NamedTemporaryFile(mode="w", suffix=".query", encoding="utf-8", newline="\n", delete=False)
    temporary.write(content)
    temporary.close()
    return temporary


def _run_process(command: Sequence[str], timeout: int) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        check=False,
    )


def _has_compilation_error(output: str) -> bool:
    return re.search(r"(?m)^.*(?:\(\d+,\d+\)|:\d+:\d+):\s+error:\s", output) is not None


def _iter_batched_processes(
    commands: Sequence[Sequence[str]],
    timeout: int,
    jobs: int,
    stage: str,
) -> Iterable[subprocess.CompletedProcess[str]]:
    if not commands:
        return
    progress_interval = max(1, len(commands) // 20)
    command_iterator = iter(commands)
    worker_count = min(max(jobs, 1), len(commands))
    with ThreadPoolExecutor(max_workers=worker_count) as executor:
        futures = {executor.submit(_run_process, next(command_iterator), timeout) for _ in range(worker_count)}
        completed = 0
        while futures:
            future = next(as_completed(futures))
            futures.remove(future)
            completed += 1
            if len(commands) > 1 and (completed == 1 or completed == len(commands) or completed % progress_interval == 0):
                print(f"[local-variable-analysis] {stage}: {completed}/{len(commands)} batches", file=sys.stderr, flush=True)
            yield future.result()
            try:
                command = next(command_iterator)
            except StopIteration:
                continue
            futures.add(executor.submit(_run_process, command, timeout))


def run_redundant_const(
    clang_query: Path,
    database_dir: Path,
    units: Sequence[CompilationUnit],
    mapper: SourcePathMapper,
    source_pattern: str,
    timeout: int,
    batch_size: int,
    jobs: int,
) -> list[Diagnostic]:
    diagnostics: dict[tuple[str, str, int, int, str], Diagnostic] = {}
    query_file = build_query_file(source_pattern)
    try:
        commands = [
            [
                str(clang_query),
                "-p",
                str(database_dir),
                "-f",
                query_file.name,
                *(str(unit.database_path) for unit in units[start : start + batch_size]),
            ]
            for start in range(0, len(units), batch_size)
        ]
        for process in _iter_batched_processes(commands, timeout, jobs, "redundant const"):
            output = process.stdout + process.stderr
            if (
                process.returncode != 0
                or "Error parsing matcher" in output
                or "Error while processing" in output
                or _has_compilation_error(output)
            ):
                raise RuntimeError(f"clang-query failed ({process.returncode})\n{output}")
            diagnostics.update((item.key, item) for item in parse_redundant_const_output(output, mapper))
    finally:
        Path(query_file.name).unlink(missing_ok=True)
    return list(diagnostics.values())


def _diagnostic_location_match(line: str) -> re.Match[str] | None:
    return re.match(r"^(.+):(\d+):(\d+):\s+(warning|error):\s+(.+?)\s+\[bugprone-use-after-move\]\s*$", line)


def run_use_after_move(
    clang_tidy: Path,
    database_dir: Path,
    units: Sequence[CompilationUnit],
    mapper: SourcePathMapper,
    timeout: int,
    batch_size: int,
    jobs: int,
) -> list[Diagnostic]:
    diagnostics: dict[tuple[str, str, int, int, str], Diagnostic] = {}
    commands = [
        [
            str(clang_tidy),
            "-p",
            str(database_dir),
            "-checks=-*,bugprone-use-after-move",
            "--quiet",
            *(str(unit.database_path) for unit in units[start : start + batch_size]),
        ]
        for start in range(0, len(units), batch_size)
    ]
    for process in _iter_batched_processes(commands, timeout, jobs, "use-after-move"):
        output = process.stdout + process.stderr
        if process.returncode != 0:
            raise RuntimeError(f"clang-tidy failed ({process.returncode})\n{output}")
        last_diagnostic: Diagnostic | None = None
        for line in output.splitlines():
            match = _diagnostic_location_match(line)
            if match is not None:
                location = mapper.location(match.group(1), int(match.group(2)), int(match.group(3)))
                if location is None:
                    last_diagnostic = None
                    continue
                message = match.group(5)
                variable_match = re.search(r"'([^']+)'\s+used after", message)
                variable = variable_match.group(1) if variable_match is not None else ""
                last_diagnostic = Diagnostic(
                    code="fo-use-after-move",
                    severity="error",
                    message=f"use of consumed local variable '{variable}'" if variable else message,
                    location=location,
                    variable=variable,
                )
                diagnostics[last_diagnostic.key] = last_diagnostic
                continue
            note_match = re.match(r"^(.+):(\d+):(\d+):\s+note:\s+(.+)$", line)
            if last_diagnostic is not None and note_match is not None and "move occurred here" in note_match.group(4):
                note_location = mapper.location(note_match.group(1), int(note_match.group(2)), int(note_match.group(3)))
                if note_location is not None:
                    last_diagnostic.notes.append(
                        f"{note_location.path}:{note_location.line}:{note_location.column}: variable was consumed here"
                    )
    return list(diagnostics.values())


def apply_suppressions(diagnostics: list[Diagnostic]) -> list[Diagnostic]:
    cache: dict[str, list[str]] = {}
    invalid: list[Diagnostic] = []
    for diagnostic in diagnostics:
        marker = SUPPRESSION_MARKERS.get(diagnostic.code)
        if marker is None:
            continue
        lines = cache.get(diagnostic.location.absolute_path)
        if lines is None:
            try:
                lines = Path(diagnostic.location.absolute_path).read_text(
                    encoding="utf-8", errors="replace"
                ).splitlines()
            except OSError:
                lines = []
            cache[diagnostic.location.absolute_path] = lines
        for index in (diagnostic.location.line - 1, diagnostic.location.line - 2):
            if index < 0 or index >= len(lines):
                continue
            marker_index = lines[index].find(marker)
            if marker_index < 0:
                continue
            reason = lines[index][marker_index + len(marker) :].strip()
            if len(reason) < 8:
                invalid.append(
                    Diagnostic(
                        code="fo-invalid-suppression",
                        severity="error",
                        message=f"{marker} requires a concrete reason of at least 8 characters",
                        location=diagnostic.location,
                    )
                )
            else:
                diagnostic.suppressed = True
                diagnostic.suppression_reason = reason
            break
    diagnostics.extend(invalid)
    return diagnostics


def analyze(
    *,
    source_root: Path,
    database_dir: Path,
    clang_query_path: str | None,
    clang_tidy_path: str | None,
    source_pattern: str,
    timeout: int,
    batch_size: int,
    jobs: int,
    checks: str,
    unit_pattern: str | None = None,
    path_pattern: str | None = None,
) -> AnalysisResult:
    mapper = SourcePathMapper(source_root)
    units = load_compilation_units(database_dir, mapper)
    if unit_pattern is not None:
        unit_filter = re.compile(unit_pattern)
        units = [unit for unit in units if unit_filter.search(unit.canonical_path)]
    if not units:
        raise RuntimeError(f"No first-party source files found in {database_dir / 'compile_commands.json'}")

    diagnostics: list[Diagnostic] = []
    versions: dict[str, str] = {}
    if checks in {"all", "redundant-local-const"}:
        clang_query = discover_tool(clang_query_path, "FO_CLANG_QUERY", ("clang-query-20", "clang-query"))
        versions["clang-query"] = require_supported_tool("clang-query", clang_query)
        diagnostics.extend(
            run_redundant_const(
                clang_query, database_dir, units, mapper, source_pattern, timeout, batch_size, jobs
            )
        )
    if checks in {"all", "use-after-move"}:
        clang_tidy = discover_tool(clang_tidy_path, "FO_CLANG_TIDY", ("clang-tidy-20", "clang-tidy"))
        versions["clang-tidy"] = require_supported_tool("clang-tidy", clang_tidy)
        diagnostics.extend(run_use_after_move(clang_tidy, database_dir, units, mapper, timeout, batch_size, jobs))

    if path_pattern is not None:
        path_filter = re.compile(path_pattern)
        diagnostics = [diagnostic for diagnostic in diagnostics if path_filter.search(diagnostic.location.path)]

    apply_suppressions(diagnostics)
    diagnostics = list({diagnostic.key: diagnostic for diagnostic in diagnostics}.values())
    diagnostics.sort(key=lambda item: (item.location.path, item.location.line, item.location.column, item.code))
    return AnalysisResult(units, diagnostics, versions)


def result_payload(result: AnalysisResult, mode: str, checks: str, applied_fixes: int = 0) -> dict:
    active = [diagnostic for diagnostic in result.diagnostics if not diagnostic.suppressed]
    return {
        "mode": mode,
        "checks": checks,
        "tool_versions": result.tool_versions,
        "statistics": {
            "files_analyzed": len({unit.canonical_path for unit in result.units}),
            "redundant_local_const": sum(item.code == "fo-redundant-local-const" for item in active),
            "use_after_move": sum(item.code == "fo-use-after-move" for item in active),
            "suppressions": sum(item.suppressed for item in result.diagnostics),
            "active_errors": sum(item.severity == "error" for item in active),
            "active_warnings": sum(item.severity == "warning" for item in active),
            "applied_fixes": applied_fixes,
        },
        "diagnostics": [asdict(diagnostic) for diagnostic in result.diagnostics],
    }


def print_diagnostic(diagnostic: Diagnostic) -> None:
    suffix = " (suppressed)" if diagnostic.suppressed else ""
    print(
        f"{diagnostic.location.path}:{diagnostic.location.line}:{diagnostic.location.column}: "
        f"{diagnostic.severity}: {diagnostic.message} [{diagnostic.code}]{suffix}"
    )
    for note in diagnostic.notes:
        print(f"note: {note}")
    if diagnostic.code == "fo-redundant-local-const":
        print("help: remove only the local's top-level const; preserve pointee, referent, and element const")
    if diagnostic.suppressed:
        print(f"note: suppression reason: {diagnostic.suppression_reason}")


def print_summary(payload: dict) -> None:
    stats = payload["statistics"]
    print("Local variable analysis")
    print(f"  mode: {payload['mode']}")
    print(f"  checks: {payload['checks']}")
    print(f"  files analyzed: {stats['files_analyzed']}")
    print(f"  redundant top-level const locals: {stats['redundant_local_const']}")
    print(f"  use-after-move: {stats['use_after_move']}")
    print(f"  active diagnostics: {stats['active_errors']} error(s), {stats['active_warnings']} warning(s)")
    print(f"  suppressions: {stats['suppressions']}")
    if stats["applied_fixes"]:
        print(f"  applied redundant const fixes: {stats['applied_fixes']}")


def run_self_tests() -> int:
    suite = unittest.defaultTestLoader.discover(str(SCRIPT_DIR), pattern="test_*.py")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


def find_default_compilation_database(project_root: Path) -> Path | None:
    candidates = [
        project_root / "Build" / "LocalVariableAnalysis",
        project_root / "Build" / "Clang",
        project_root / "Build" / "Auto",
        project_root / "Workspace" / "validate-unit-tests",
    ]
    return next((candidate for candidate in candidates if (candidate / "compile_commands.json").is_file()), None)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("report-only", "apply", "full-enforcement"), default="report-only")
    parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--compilation-database", type=Path)
    parser.add_argument("--clang-query")
    parser.add_argument("--clang-tidy")
    parser.add_argument("--source-pattern", default="Engine.Source.")
    parser.add_argument("--unit-pattern", help="only analyze compilation units whose canonical path matches this regex")
    parser.add_argument("--path-pattern", help="only keep diagnostics whose canonical source path matches this regex")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--batch-size", type=int, default=4)
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 4))
    parser.add_argument("--fail-on", choices=("none", "error", "warning"))
    parser.add_argument(
        "--checks",
        choices=("all", "redundant-local-const", "use-after-move"),
        default="all",
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    if args.self_test:
        return run_self_tests()
    if args.mode == "apply" and args.checks != "redundant-local-const":
        print("error: --mode apply requires --checks redundant-local-const", file=sys.stderr)
        return 2

    engine_root = _normalize_path(args.engine_root)
    source_root = _normalize_path(args.source_root or engine_root / "Source")
    database_dir = args.compilation_database or find_default_compilation_database(engine_root.parent)
    if database_dir is None:
        print("error: no compile_commands.json found; pass --compilation-database <build-dir>", file=sys.stderr)
        return 2
    database_dir = _normalize_path(database_dir)
    fail_on = args.fail_on or ("warning" if args.mode == "full-enforcement" else "none")

    try:
        result = analyze(
            source_root=source_root,
            database_dir=database_dir,
            clang_query_path=args.clang_query,
            clang_tidy_path=args.clang_tidy,
            source_pattern=args.source_pattern,
            timeout=args.timeout,
            batch_size=max(args.batch_size, 1),
            jobs=max(args.jobs, 1),
            checks=args.checks,
            unit_pattern=args.unit_pattern,
            path_pattern=args.path_pattern,
        )
    except (OSError, RuntimeError, subprocess.TimeoutExpired, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    try:
        applied_fixes = apply_redundant_const_fixes(result.diagnostics) if args.mode == "apply" else 0
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    for diagnostic in result.diagnostics:
        print_diagnostic(diagnostic)
    payload = result_payload(result, args.mode, args.checks, applied_fixes)
    print_summary(payload)
    if args.json is not None:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    active = [diagnostic for diagnostic in result.diagnostics if not diagnostic.suppressed]
    if fail_on == "error" and any(diagnostic.severity == "error" for diagnostic in active):
        return 1
    if fail_on == "warning" and active:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
