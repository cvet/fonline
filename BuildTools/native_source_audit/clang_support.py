"""Shared clang-query/clang-tidy support for the AST-backed native source audits.

Clang tools are optional: callers raise ``ToolMissing`` when a tool or the compilation database is absent and
decide whether that is a skip or a failure.  Output paths are display paths relative to the audit root.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

from native_source_audit.common import AuditContext, relative_path, safe_resolve


QUERY_DIR = Path(__file__).resolve().parent / "queries"
SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx"}
EXCLUDED_PARTS = {"ThirdParty", "third_party", "Generated", "generated"}
MINIMUM_CLANG_MAJOR = 20
_WORD_BYTE = re.compile(rb"[A-Za-z0-9_]")
SUPPRESSION_MARKERS = {
    "fo-redundant-local-const": "FO_REDUNDANT_CONST_SUPPRESS:",
    "fo-use-after-move": "FO_USE_AFTER_MOVE_SUPPRESS:",
}


class ToolMissing(Exception):
    """A clang tool or the compilation database is unavailable."""


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
    """Map clang-reported paths to display paths inside the audit scopes.

    A path outside the root is still accepted when it contains ``/<scope>/``, so a compilation database
    produced from another checkout of the same tree maps onto this one.
    """

    def __init__(self, context: AuditContext) -> None:
        self.context = context
        self.root = context.root

    def map_path(self, raw_path: str | Path) -> tuple[str, Path] | None:
        path = _normalize_path(Path(raw_path))
        display = relative_path(path, self.root)

        if display is None:
            normalized = str(path).replace("\\", "/")
            lowered = normalized.lower()

            for scope in self.context.scopes:
                if scope == ".":
                    continue
                marker = "/" + scope.lower() + "/"
                marker_index = lowered.rfind(marker)
                if marker_index >= 0:
                    display = scope + "/" + normalized[marker_index + len(marker):]
                    break

        if display is None or not self._in_scope(display) or self.context.is_excluded(display):
            return None
        return display, self.root / display

    def location(self, raw_path: str, line: int, column: int) -> SourceLocation | None:
        mapped = self.map_path(raw_path)
        if mapped is None:
            return None
        display, absolute = mapped
        return SourceLocation(display, line, column, str(absolute))

    def canonical_for_database_file(self, path: Path) -> str | None:
        mapped = self.map_path(path)
        return mapped[0] if mapped is not None else None

    def _in_scope(self, display: str) -> bool:
        return any(scope == "." or display == scope or display.startswith(scope + "/") for scope in self.context.scopes)


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


def redundant_const_diagnostics(
    matches: Iterable[dict[str, NodeBinding]], mapper: SourcePathMapper
) -> list[Diagnostic]:
    diagnostics: dict[tuple[str, str, int, int, str], Diagnostic] = {}
    for match in matches:
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


def parse_redundant_const_output(output: str, mapper: SourcePathMapper) -> list[Diagnostic]:
    return redundant_const_diagnostics(parse_query_matches(output, mapper), mapper)


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


def _top_level_const_offsets(region: bytes) -> list[int]:
    """Offsets of the `const` tokens that sit outside every template argument and bracket group."""

    offsets: list[int] = []
    depth = 0
    index = 0
    while index < len(region):
        char = region[index : index + 1]
        if char in {b"<", b"(", b"[", b"{"}:
            depth += 1
        elif char in {b">", b")", b"]", b"}"}:
            # `->` is a member access, not a closing angle bracket
            if not (char == b">" and index > 0 and region[index - 1 : index] == b"-"):
                depth = max(depth - 1, 0)
        elif depth == 0 and region.startswith(b"const", index) and _is_token(region, index, len(b"const")):
            offsets.append(index)
            index += len(b"const")
            continue

        index += 1

    return offsets


def _is_token(region: bytes, start: int, length: int) -> bool:
    before = region[start - 1 : start] if start > 0 else b""
    after = region[start + length : start + length + 1]
    return not _WORD_BYTE.match(before) and not _WORD_BYTE.match(after)


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
    offsets = _top_level_const_offsets(content[declaration_start:declarator_start]) if declaration_start <= declarator_start else []
    if not offsets and node.name:
        line_start = content.rfind(b"\n", 0, declaration_start) + 1
        line_end = content.find(b"\n", declaration_start)
        if line_end < 0:
            line_end = len(content)
        encoded_name = node.name.encode("utf-8")
        name_match = re.search(rb"\b" + re.escape(encoded_name) + rb"\b", content[declaration_start:line_end])
        if name_match is not None:
            declarator_start = declaration_start + name_match.start()
            offsets = _top_level_const_offsets(content[line_start:declarator_start])
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
    raise ToolMissing(f"{' or '.join(names)} not found; install Clang {MINIMUM_CLANG_MAJOR}+ or set {environment_name}")


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
    if major < MINIMUM_CLANG_MAJOR:
        raise RuntimeError(f"{name} {major} is unsupported; the audits require Clang {MINIMUM_CLANG_MAJOR} or newer")
    return description


def _entry_priority(entry: dict) -> int:
    command_text = entry.get("command") or " ".join(entry.get("arguments", []))
    return 2 if "RelWithDebInfo" in command_text else 1 if "Release" in command_text else 0


def _entry_source_path(entry: dict) -> Path:
    raw_file = Path(entry["file"])
    if not raw_file.is_absolute():
        raw_file = Path(entry["directory"]) / raw_file
    return raw_file


def _read_compilation_database(database_dir: Path) -> list[dict]:
    database_file = database_dir / "compile_commands.json"
    if not database_file.is_file():
        raise RuntimeError(f"Compilation database not found: {database_file}")
    return json.loads(database_file.read_text(encoding="utf-8"))


def load_compilation_units(database_dir: Path, mapper: SourcePathMapper) -> list[CompilationUnit]:
    preferred: dict[str, tuple[int, CompilationUnit]] = {}
    for entry in _read_compilation_database(database_dir):
        raw_file = _entry_source_path(entry)
        canonical = mapper.canonical_for_database_file(raw_file)
        if canonical is None or raw_file.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        if any(part in EXCLUDED_PARTS for part in Path(canonical).parts):
            continue
        priority = _entry_priority(entry)
        unit = CompilationUnit(_normalize_path(raw_file), canonical)
        current = preferred.get(canonical)
        if current is None or priority > current[0]:
            preferred[canonical] = (priority, unit)
    return [item[1] for item in sorted(preferred.values(), key=lambda value: value[1].canonical_path)]


def write_analysis_database(database_dir: Path, units: Sequence[CompilationUnit], destination: Path) -> Path:
    selected: dict[Path, tuple[int, dict]] = {}
    wanted = {unit.database_path for unit in units}
    for entry in _read_compilation_database(database_dir):
        source_path = _normalize_path(_entry_source_path(entry))
        if source_path not in wanted:
            continue
        priority = _entry_priority(entry)
        current = selected.get(source_path)
        if current is None or priority > current[0]:
            selected[source_path] = (priority, entry)
    missing = sorted(str(path) for path in wanted - set(selected))
    if missing:
        raise RuntimeError("Compilation database lost commands for selected units\n" + "\n".join(missing))
    destination.mkdir(parents=True, exist_ok=True)
    payload = [item[1] for item in selected.values()]
    (destination / "compile_commands.json").write_text(json.dumps(payload), encoding="utf-8")
    return destination




def scope_source_pattern(context: AuditContext) -> str:
    """Default isExpansionInFileMatching regex: each scope with separators matched by any character.

    Metacharacters are bracketed instead of backslash-escaped because the pattern travels inside a
    clang-query string literal.
    """

    alternatives = []
    for scope in context.scopes:
        text = context.root.name if scope == "." else scope
        escaped = "".join(_regex_literal(char) for char in text)
        is_directory = scope == "." or (context.root / scope).is_dir()
        alternatives.append(escaped + ("." if is_directory else "$"))
    return alternatives[0] if len(alternatives) == 1 else "(" + "|".join(alternatives) + ")"


def _regex_literal(char: str) -> str:
    if char in "/\\[]":
        return "."
    if char in ".^$|()*+?{}":
        return f"[{char}]"
    return char


def build_query_file(template_names: Sequence[str], source_pattern: str) -> Path:
    template = "\n\n".join((QUERY_DIR / name).read_text(encoding="utf-8") for name in template_names)
    escaped_pattern = source_pattern.replace("\\", "\\\\").replace('"', '\\"')
    content = template.replace("@SOURCE_FILTER@", f'isExpansionInFileMatching("{escaped_pattern}")')
    with tempfile.NamedTemporaryFile(mode="w", suffix=".query", encoding="utf-8", newline="\n", delete=False) as temporary:
        temporary.write(content)
    return Path(temporary.name)


def resolve_compilation_database(context: AuditContext, explicit: Path | None) -> Path:
    configured = explicit if explicit is not None else context.config.get("compilation_database")
    if configured is None:
        raise ToolMissing("no compilation database; pass --compilation-database or set compilation_database in the configuration")
    database_dir = Path(configured)
    if not database_dir.is_absolute():
        database_dir = context.root / database_dir
    database_dir = safe_resolve(database_dir)
    if not (database_dir / "compile_commands.json").is_file():
        raise ToolMissing(f"compile_commands.json not found in {database_dir}")
    return database_dir


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
    return re.search(r"(?m)^.*(?:\(\d+,\d+\)|:\d+:\d+):\s+(?:fatal\s+)?error:\s", output) is not None


def tool_failure_message(tool: str, process: subprocess.CompletedProcess[str], output: str) -> str:
    analyzed = [argument for argument in process.args[1:] if Path(argument).suffix.lower() in SOURCE_EXTENSIONS]
    sources = "\n".join(f"  {path}" for path in analyzed)
    return f"{tool} failed ({process.returncode}) while analyzing:\n{sources}\n{output}"


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
                print(f"[native-source-audit] {stage}: {completed}/{len(commands)} batches", file=sys.stderr, flush=True)
            yield future.result()
            try:
                command = next(command_iterator)
            except StopIteration:
                continue
            futures.add(executor.submit(_run_process, command, timeout))



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
