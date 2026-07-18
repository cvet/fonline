#!/usr/bin/env python3
"""Audit and replace eligible local auto declarations with simple explicit types."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
import unittest
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Sequence


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_ENGINE_ROOT = SCRIPT_DIR.parents[1]
QUERY_TEMPLATE = SCRIPT_DIR / "explicit_local_types.query.in"
LOCAL_VARIABLE_VALIDATOR_DIR = SCRIPT_DIR.parent / "LocalVariableValidator"
sys.path.insert(0, str(LOCAL_VARIABLE_VALIDATOR_DIR))
import local_variable_validator as clang_support  # noqa: E402


SNAKE_CASE_TYPE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$")
TYPE_TOKEN = re.compile(r"[A-Za-z_]\w*|&&|[&*]")
TYPE_QUALIFIERS = {"const", "volatile", "restrict", "__restrict", "__restrict__"}
ALLOWED_TOP_LEVEL_IMPLICIT_CASTS = {"LValueToRValue"}
BUILTIN_SIMPLE_TYPES = {
    "bool",
    "char",
    "char8_t",
    "char16_t",
    "char32_t",
    "double",
    "float",
    "int",
    "short",
    "signed",
    "void",
    "wchar_t",
}
ALLOWED_IMPORTED_SIMPLE_TYPES = {"string_view"}
PREFERRED_PRIMITIVE_SPELLINGS = {
    "double": "float64_t",
    "float": "float32_t",
    "int": "int32_t",
    "short": "int16_t",
}
UNSIZED_NUMERIC_PRIMITIVES = {"long", "signed", "unsigned"}


@dataclass(frozen=True)
class TypeShape:
    base: str
    pointer_depth: int
    is_reference: bool
    base_qualifiers: frozenset[str]


@dataclass(frozen=True)
class Observation:
    path: str
    name: str
    declaration_line: int
    declaration_column: int
    deduced_type: str
    initializer_type: str
    token_line: int | None
    token_column: int | None
    replacement: str
    reason: str

    @property
    def token_key(self) -> tuple[str, int, int] | None:
        if self.token_line is None or self.token_column is None:
            return None
        return (self.path, self.token_line, self.token_column)


@dataclass(frozen=True)
class Candidate:
    path: str
    line: int
    column: int
    replacement: str
    deduced_type: str
    declarations: tuple[str, ...]


@dataclass
class AuditResult:
    units: list[clang_support.CompilationUnit]
    candidates: list[Candidate]
    observations: list[Observation]
    exclusion_counts: Counter[str]
    type_counts: Counter[str]
    tool_version: str


def parse_type_shape(type_name: str) -> TypeShape | None:
    if not type_name or any(marker in type_name for marker in ("::", "<", ">", "(", ")", "[", "]", ",", "...")):
        return None
    tokens = TYPE_TOKEN.findall(type_name)
    residue = TYPE_TOKEN.sub("", type_name)
    if residue.strip():
        return None
    identifiers = [token for token in tokens if re.fullmatch(r"[A-Za-z_]\w*", token) and token not in TYPE_QUALIFIERS]
    if len(identifiers) != 1:
        return None
    base = identifiers[0]
    if base == "auto" or SNAKE_CASE_TYPE.fullmatch(base) is None:
        return None
    reference_tokens = [token for token in tokens if token in {"&", "&&"}]
    if len(reference_tokens) > 1:
        return None
    declarator_index = next((index for index, token in enumerate(tokens) if token in {"*", "&", "&&"}), len(tokens))
    base_qualifiers = frozenset(token for token in tokens[:declarator_index] if token in TYPE_QUALIFIERS)
    return TypeShape(base, sum(token == "*" for token in tokens), bool(reference_tokens), base_qualifiers)


def _first_initializer_line(node: clang_support.NodeBinding) -> tuple[str, str] | None:
    for line in node.text.splitlines()[1:]:
        child = re.match(r"^(?:\|-|`-)([A-Za-z][A-Za-z0-9_]*)\b", line)
        if child is None or child.group(1).endswith("Attr"):
            continue
        quoted = re.findall(r"'([^']*)'", line)
        if not quoted:
            continue
        return line, quoted[0]
    return None


def _canonical_ast_type(line: str) -> str:
    quoted = re.findall(r"'([^']*)'", line)
    return quoted[-1] if quoted else ""


def _authored_node_text(
    node: clang_support.NodeBinding,
    source_cache: dict[str, str],
) -> str:
    start = node.location
    if start is None:
        return ""
    text = source_cache.get(start.absolute_path)
    if text is None:
        try:
            text = Path(start.absolute_path).read_bytes().decode("utf-8")
        except (OSError, UnicodeError):
            return ""
        source_cache[start.absolute_path] = text

    range_match = re.search(r"<(.+?)>\s+", node.first_line)
    if range_match is None:
        return ""
    range_end = range_match.group(1).rsplit(",", maxsplit=1)[-1].strip()
    end_line = start.line
    end_column = start.column
    absolute_end = re.fullmatch(r"(.+?):(\d+):(\d+)", range_end)
    relative_line_end = re.fullmatch(r"line:(\d+):(\d+)", range_end)
    relative_column_end = re.fullmatch(r"col:(\d+)", range_end)
    if absolute_end is not None:
        if Path(absolute_end.group(1)).resolve(strict=False) != Path(start.absolute_path).resolve(strict=False):
            return ""
        end_line = int(absolute_end.group(2))
        end_column = int(absolute_end.group(3))
    elif relative_line_end is not None:
        end_line = int(relative_line_end.group(1))
        end_column = int(relative_line_end.group(2))
    elif relative_column_end is not None:
        end_column = int(relative_column_end.group(1))
    else:
        return ""

    starts = _line_starts(text)
    start_offset = _offset(starts, start.line, start.column, len(text))
    end_offset = _offset(starts, end_line, end_column, len(text))
    if start_offset is None or end_offset is None or start_offset > end_offset:
        return ""
    return text[start_offset : end_offset + 1]


def _unique_simple_initializer_alias(
    node: clang_support.NodeBinding,
    source_cache: dict[str, str],
) -> tuple[str, str] | None:
    canonical_type = _canonical_ast_type(node.first_line)
    if not canonical_type:
        return None

    authored_text = _authored_node_text(node, source_cache)
    if not authored_text:
        return None
    aliases: set[str] = set()
    for line in node.text.splitlines()[1:]:
        for alias, desugared in re.findall(r"'([^']*)':'([^']*)'", line):
            if desugared != canonical_type:
                continue
            shape = parse_type_shape(alias)
            if (
                shape is not None
                and shape.pointer_depth == 0
                and not shape.is_reference
                and not shape.base_qualifiers
                and re.search(rf"\b{re.escape(shape.base)}\b", authored_text)
            ):
                aliases.add(shape.base)
    if len(aliases) != 1:
        return None
    return next(iter(aliases)), canonical_type


def _line_starts(text: str) -> list[int]:
    starts = [0]
    for match in re.finditer("\n", text):
        starts.append(match.end())
    return starts


def _offset(starts: Sequence[int], line: int, column: int, text_length: int) -> int | None:
    if line < 1 or line > len(starts):
        return None
    value = starts[line - 1] + column - 1
    return value if 0 <= value <= text_length else None


def _token_location(text: str, offset: int) -> tuple[int, int]:
    line = text.count("\n", 0, offset) + 1
    previous_newline = text.rfind("\n", 0, offset)
    return line, offset - previous_newline


def _find_auto_token(
    node: clang_support.NodeBinding,
    source_cache: dict[str, str],
) -> tuple[int, int, str, str] | None:
    start = node.location
    declaration = node.declarator_location
    if start is None or declaration is None or start.absolute_path != declaration.absolute_path:
        return None
    text = source_cache.get(start.absolute_path)
    if text is None:
        try:
            text = Path(start.absolute_path).read_bytes().decode("utf-8")
        except (OSError, UnicodeError):
            return None
        source_cache[start.absolute_path] = text
    starts = _line_starts(text)
    start_offset = _offset(starts, start.line, start.column, len(text))
    declaration_offset = _offset(starts, declaration.line, declaration.column, len(text))
    if start_offset is None or declaration_offset is None or start_offset >= declaration_offset:
        return None

    prefix = text[start_offset:declaration_offset]
    matches = list(re.finditer(r"\bauto\b", prefix))
    if len(matches) != 1:
        line_start = starts[declaration.line - 1]
        fallback = text[line_start:declaration_offset]
        fallback_matches = list(re.finditer(r"\bauto\b", fallback))
        if len(fallback_matches) != 1 or ";" in fallback[: fallback_matches[0].start()]:
            return None
        start_offset = line_start
        prefix = fallback
        matches = fallback_matches

    match = matches[0]
    token_offset = start_offset + match.start()
    before = prefix[: match.start()]
    after = prefix[match.end() :]
    if re.search(r"decltype\s*\($", before):
        return None
    line, column = _token_location(text, token_offset)
    return line, column, before, after


def _explicit_type_spelling(shape: TypeShape) -> str:
    qualifiers = [
        qualifier
        for qualifier in ("const", "volatile", "restrict", "__restrict", "__restrict__")
        if qualifier in shape.base_qualifiers
    ]
    return " ".join([*qualifiers, _preferred_base(shape.base)]) + "*" * shape.pointer_depth


def _preferred_base(base: str) -> str:
    return PREFERRED_PRIMITIVE_SPELLINGS.get(base, base)


def _authored_base_qualifiers(before: str, after: str) -> frozenset[str]:
    declarator = re.search(r"[*&]", after)
    base_region = before + " " + after[: declarator.start() if declarator is not None else len(after)]
    return frozenset(re.findall(r"\b(?:const|volatile|restrict|__restrict|__restrict__)\b", base_region))


def _has_qualified_type_origin(node: clang_support.NodeBinding, shape: TypeShape, has_alias_spelling: bool) -> bool:
    if has_alias_spelling or shape.base in BUILTIN_SIMPLE_TYPES:
        return False
    qualified_base = re.compile(rf"\b(?:[A-Za-z_]\w*::)+{re.escape(shape.base)}\b")
    return qualified_base.search(node.text) is not None


def classify_node(
    node: clang_support.NodeBinding,
    source_cache: dict[str, str],
    forced_qualified_origin: bool = False,
) -> Observation | None:
    declaration = node.declarator_location or node.location
    if declaration is None:
        return None
    common = {
        "path": declaration.path,
        "name": node.name or "<unnamed>",
        "declaration_line": declaration.line,
        "declaration_column": declaration.column,
        "deduced_type": node.type_name,
    }
    token = _find_auto_token(node, source_cache)
    token_line = token[0] if token is not None else None
    token_column = token[1] if token is not None else None

    def observation(reason: str, initializer_type: str = "", replacement: str = "") -> Observation:
        return Observation(
            **common,
            initializer_type=initializer_type,
            token_line=token_line,
            token_column=token_column,
            replacement=replacement,
            reason=reason,
        )

    if token is None:
        return observation("auto-token-not-rewritable")
    if node.kind != "VarDecl":
        return observation("unsupported-declaration-kind")
    has_alias_spelling = re.search(r"'[^']*':'[^']*'", node.first_line) is not None
    initializer_alias = _unique_simple_initializer_alias(node, source_cache)
    declared_shape = parse_type_shape(node.type_name)
    if declared_shape is None and initializer_alias is not None:
        declared_shape = parse_type_shape(initializer_alias[0])
        has_alias_spelling = True
    if declared_shape is None:
        return observation("non-simple-deduced-type")
    if declared_shape.base in UNSIZED_NUMERIC_PRIMITIVES:
        return observation("unsized-numeric-primitive")
    if (
        forced_qualified_origin and declared_shape.base not in ALLOWED_IMPORTED_SIMPLE_TYPES
    ) or _has_qualified_type_origin(node, declared_shape, has_alias_spelling):
        return observation("qualified-or-nested-type-origin")

    initializer = _first_initializer_line(node)
    if initializer is None:
        return observation("initializer-not-resolved")
    initializer_line, initializer_type = initializer
    if initializer_alias is not None:
        initializer_type = _canonical_ast_type(initializer_line)
        if initializer_type != initializer_alias[1]:
            return observation("initializer-type-mismatch", initializer_type)
    else:
        initializer_shape = parse_type_shape(initializer_type)
        if initializer_shape is None:
            return observation("non-simple-initializer-type", initializer_type)
        if (
            declared_shape.base != initializer_shape.base
            or declared_shape.pointer_depth != initializer_shape.pointer_depth
        ):
            return observation("initializer-type-mismatch", initializer_type)
    if "ImplicitCastExpr" in initializer_line:
        cast = re.search(r"\s<([A-Za-z][A-Za-z0-9_]*)>\s*$", initializer_line)
        cast_kind = cast.group(1) if cast is not None else "unknown"
        if cast_kind not in ALLOWED_TOP_LEVEL_IMPLICIT_CASTS:
            return observation(f"implicit-conversion:{cast_kind}", initializer_type)

    assert token_line is not None and token_column is not None
    _, _, before, after = token
    has_authored_declarator = any(char in after for char in "*&")
    has_authored_cv = re.search(r"\b(?:const|volatile)\b", before + " " + after) is not None
    has_constexpr = re.search(r"\bconstexpr\b", before + " " + after) is not None
    if has_authored_cv and not has_authored_declarator and declared_shape.pointer_depth:
        return observation("top-level-cv-with-deduced-pointer", initializer_type)
    if has_authored_declarator:
        authored_qualifiers = _authored_base_qualifiers(before, after)
        missing_qualifiers = declared_shape.base_qualifiers - authored_qualifiers
        ordered_qualifiers = [
            qualifier
            for qualifier in ("const", "volatile", "restrict", "__restrict", "__restrict__")
            if qualifier in missing_qualifiers
        ]
        replacement = " ".join([*ordered_qualifiers, _preferred_base(declared_shape.base)])
    elif has_authored_cv:
        replacement = _preferred_base(declared_shape.base)
    elif has_constexpr:
        replacement = _preferred_base(declared_shape.base) if not declared_shape.pointer_depth else _explicit_type_spelling(declared_shape)
    else:
        replacement = _explicit_type_spelling(declared_shape)
    return observation("", initializer_type, replacement)


def _build_query_file(source_pattern: str) -> tempfile.NamedTemporaryFile:
    template = QUERY_TEMPLATE.read_text(encoding="utf-8")
    escaped = source_pattern.replace("\\", "\\\\").replace('"', '\\"')
    content = template.replace("@SOURCE_FILTER@", f'isExpansionInFileMatching("{escaped}")')
    temporary = tempfile.NamedTemporaryFile(mode="w", suffix=".query", encoding="utf-8", newline="\n", delete=False)
    temporary.write(content)
    temporary.close()
    return temporary


def _query_version(tool: Path) -> str:
    process = clang_support._run_process([str(tool), "--version"], 30)
    text = (process.stdout + process.stderr).strip()
    if process.returncode != 0:
        raise RuntimeError(f"Unable to query clang-query version\n{text}")
    major = re.search(r"version\s+(\d+)", text)
    if major is None or int(major.group(1)) < 20:
        raise RuntimeError(f"clang-query 20+ is required, got: {text}")
    return text.splitlines()[0]


def run_inventory(
    *,
    engine_root: Path,
    database_dir: Path,
    source_pattern: str,
    jobs: int,
    batch_size: int,
    timeout: int,
    clang_query_path: str | None = None,
    source_root: Path | None = None,
    unit_pattern: str | None = None,
    path_pattern: str | None = None,
) -> AuditResult:
    source_root = (source_root or engine_root / "Source").resolve()
    mapper = clang_support.SourcePathMapper(source_root)
    clang_query = clang_support.discover_tool(clang_query_path, "FO_CLANG_QUERY", ("clang-query-20", "clang-query"))
    version = _query_version(clang_query)
    units = clang_support.load_compilation_units(database_dir, mapper)
    if unit_pattern is not None:
        unit_filter = re.compile(unit_pattern)
        units = [unit for unit in units if unit_filter.search(unit.canonical_path)]
    if not units:
        raise RuntimeError(f"No first-party source files found in {database_dir / 'compile_commands.json'}")

    query_file = _build_query_file(source_pattern)
    path_filter = re.compile(path_pattern) if path_pattern is not None else None
    parsed_nodes: dict[tuple[str, int, int, str, str, str], clang_support.NodeBinding] = {}
    nested_type_declarations: set[tuple[str, int, int, str]] = set()
    try:
        commands: list[list[str]] = []
        for start in range(0, len(units), batch_size):
            batch = units[start : start + batch_size]
            commands.append([str(clang_query), "-p", str(database_dir), "-f", query_file.name, *(str(unit.database_path) for unit in batch)])
        for process in clang_support._iter_batched_processes(commands, timeout, jobs, "explicit-type AST inventory"):
            output = process.stdout + process.stderr
            if process.returncode != 0 or "Error parsing matcher" in output or clang_support._has_compilation_error(output):
                raise RuntimeError(f"clang-query failed ({process.returncode})\n{output}")
            for match in clang_support.parse_query_matches(output, mapper):
                nested_node = match.get("nested_type_local")
                if (
                    nested_node is not None
                    and nested_node.declarator_location is not None
                    and (path_filter is None or path_filter.search(nested_node.declarator_location.path))
                ):
                    nested_type_declarations.add(
                        (
                            nested_node.declarator_location.path,
                            nested_node.declarator_location.line,
                            nested_node.declarator_location.column,
                            nested_node.name,
                        )
                    )
                node = match.get("auto_local")
                if node is None or node.declarator_location is None:
                    continue
                if path_filter is not None and path_filter.search(node.declarator_location.path) is None:
                    continue
                key = (
                    node.declarator_location.path,
                    node.declarator_location.line,
                    node.declarator_location.column,
                    node.name,
                    node.type_name,
                    node.text,
                )
                parsed_nodes[key] = node
    finally:
        Path(query_file.name).unlink(missing_ok=True)

    source_cache: dict[str, str] = {}
    observations = []
    for node in parsed_nodes.values():
        assert node.declarator_location is not None
        declaration_key = (
            node.declarator_location.path,
            node.declarator_location.line,
            node.declarator_location.column,
            node.name,
        )
        item = classify_node(node, source_cache, declaration_key in nested_type_declarations)
        if item is not None:
            observations.append(item)
    token_groups: dict[tuple[str, int, int], list[Observation]] = defaultdict(list)
    orphaned: list[Observation] = []
    for observation in observations:
        if observation.token_key is None:
            orphaned.append(observation)
        else:
            token_groups[observation.token_key].append(observation)

    candidates: list[Candidate] = []
    exclusion_counts: Counter[str] = Counter(observation.reason for observation in orphaned)
    type_counts: Counter[str] = Counter()
    for token_key, group in sorted(token_groups.items()):
        reasons = sorted({observation.reason for observation in group if observation.reason})
        replacements = {observation.replacement for observation in group if observation.replacement}
        if reasons:
            exclusion_counts["not-universally-eligible:" + ",".join(reasons)] += 1
            continue
        if len(replacements) != 1:
            exclusion_counts["inconsistent-instantiations"] += 1
            continue
        replacement = next(iter(replacements))
        deduced_types = sorted({observation.deduced_type for observation in group})
        declarations = tuple(sorted({observation.name for observation in group}))
        candidates.append(Candidate(token_key[0], token_key[1], token_key[2], replacement, " | ".join(deduced_types), declarations))
        type_counts[replacement] += 1

    return AuditResult(units, candidates, observations, exclusion_counts, type_counts, version)


def apply_candidate_list(candidates: Sequence[Candidate], source_root: Path) -> int:
    by_path: dict[str, list[Candidate]] = defaultdict(list)
    for candidate in candidates:
        by_path[candidate.path].append(candidate)
    changed = 0
    resolved_root = source_root.resolve()
    for canonical_path, candidates in sorted(by_path.items()):
        path = (resolved_root / Path(canonical_path)).resolve()
        try:
            path.relative_to(resolved_root)
        except ValueError as error:
            raise RuntimeError(f"Candidate path escapes source root: {canonical_path}") from error
        text = path.read_bytes().decode("utf-8")
        starts = _line_starts(text)
        edits: list[tuple[int, str]] = []
        for candidate in candidates:
            offset = _offset(starts, candidate.line, candidate.column, len(text))
            if offset is None or text[offset : offset + 4] != "auto":
                raise RuntimeError(f"Source changed before rewrite: {canonical_path}:{candidate.line}:{candidate.column}")
            edits.append((offset, candidate.replacement))
        for offset, replacement in sorted(edits, reverse=True):
            text = text[:offset] + replacement + text[offset + 4 :]
        path.write_bytes(text.encode("utf-8"))
        changed += len(edits)
    return changed


def apply_candidates(result: AuditResult, source_root: Path) -> int:
    return apply_candidate_list(result.candidates, source_root)


def load_report_candidates(report_path: Path) -> list[Candidate]:
    payload = json.loads(report_path.read_text(encoding="utf-8"))
    raw_candidates = payload.get("candidates")
    if not isinstance(raw_candidates, list):
        raise RuntimeError(f"Invalid explicit-type report: {report_path}")
    candidates: list[Candidate] = []
    for raw in raw_candidates:
        if not isinstance(raw, dict):
            raise RuntimeError(f"Invalid candidate entry in {report_path}")
        candidates.append(
            Candidate(
                path=str(raw["path"]),
                line=int(raw["line"]),
                column=int(raw["column"]),
                replacement=str(raw["replacement"]),
                deduced_type=str(raw["deduced_type"]),
                declarations=tuple(str(name) for name in raw["declarations"]),
            )
        )
    return candidates


def result_payload(result: AuditResult, mode: str) -> dict:
    return {
        "mode": mode,
        "tool_version": result.tool_version,
        "statistics": {
            "files_analyzed": len({unit.canonical_path for unit in result.units}),
            "auto_observations": len(result.observations),
            "eligible_replacements": len(result.candidates),
            "excluded_observations": sum(result.exclusion_counts.values()),
        },
        "eligible_types": dict(result.type_counts.most_common()),
        "exclusions": dict(result.exclusion_counts.most_common()),
        "candidates": [asdict(candidate) for candidate in result.candidates],
    }


def print_summary(payload: dict) -> None:
    stats = payload["statistics"]
    print("Explicit simple local type analysis")
    print(f"  mode: {payload['mode']}")
    print(f"  files analyzed: {stats['files_analyzed']}")
    print(f"  auto observations: {stats['auto_observations']}")
    print(f"  eligible replacements: {stats['eligible_replacements']}")
    print(f"  excluded observations/groups: {stats['excluded_observations']}")


def find_default_compilation_database(project_root: Path) -> Path | None:
    for candidate in (
        project_root / "Build" / "LocalVariableAnalysis",
        project_root / "Build" / "Clang",
        project_root / "Build" / "Auto",
        project_root / "Workspace" / "validate-unit-tests",
    ):
        if (candidate / "compile_commands.json").is_file():
            return candidate
    return None


def run_self_tests() -> int:
    suite = unittest.defaultTestLoader.discover(str(SCRIPT_DIR), pattern="test_*.py")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("report-only", "check", "apply"), default="report-only")
    parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--compilation-database", type=Path)
    parser.add_argument("--source-pattern", default="Engine.Source.")
    parser.add_argument("--unit-pattern", help="only analyze compilation units whose canonical path matches this regex")
    parser.add_argument("--path-pattern", help="only keep AST observations whose canonical source path matches this regex")
    parser.add_argument("--clang-query")
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--batch-size", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--apply-report", type=Path, help="apply a previously reviewed JSON report without rerunning Clang")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    if args.self_test:
        return run_self_tests()
    engine_root = args.engine_root.resolve()
    source_root = (args.source_root or engine_root / "Source").resolve()
    rewrite_root = source_root.parent if source_root.name == "Source" else source_root
    project_root = engine_root.parent
    if args.apply_report is not None:
        if args.mode != "apply":
            raise RuntimeError("--apply-report requires --mode apply")
        candidates = load_report_candidates(args.apply_report.resolve())
        applied = apply_candidate_list(candidates, rewrite_root)
        print("Explicit simple local type analysis")
        print("  mode: apply-reviewed-report")
        print(f"  applied replacements: {applied}")
        return 0
    database_dir = args.compilation_database or find_default_compilation_database(project_root)
    if database_dir is None:
        raise RuntimeError("Compilation database not found; configure the local analysis preset first")
    result = run_inventory(
        engine_root=engine_root,
        database_dir=Path(database_dir).resolve(),
        source_pattern=args.source_pattern,
        jobs=max(1, args.jobs),
        batch_size=max(1, args.batch_size),
        timeout=args.timeout,
        clang_query_path=args.clang_query,
        source_root=source_root,
        unit_pattern=args.unit_pattern,
        path_pattern=args.path_pattern,
    )
    payload = result_payload(result, args.mode)
    if args.mode == "apply":
        payload["applied_replacements"] = apply_candidates(result, rewrite_root)
    if args.json is not None:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print_summary(payload)
    if args.mode == "apply":
        print(f"  applied replacements: {payload['applied_replacements']}")
    return 1 if args.mode == "check" and result.candidates else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
