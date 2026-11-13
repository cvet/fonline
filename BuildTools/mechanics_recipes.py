#!/usr/bin/env python3
"""Validate mechanics recipe pages against their manifest, example sources and the API model.

The recipe manifest (``recipes.json``, schema version 1) lists every recipe, its page, the example
files that prove it and the generated API symbols it uses. The checks keep the pages tied to them:

  manifest      structure, schema version, slugs, kinds, backend statuses and the smoke reference
  files         every listed path exists inside the example directory
  page          front matter order, recipe kind, fixed ``##`` order, pitfalls and backend status rows
  excerpt       angelscript, csharp, ini and json fences are contiguous slices of the entry's files
  symbol        manifest symbols exist in ``api.json`` and the page cites exactly those symbol ids
  event         no subscription to an event the Engine never fires unless the recipe fires it itself
  mirror        the Russian page keeps the front matter, ``##`` count and fenced blocks
  project-name  no embedding-project names in the pages, the section index or the example tree
  hub           the section index links every page and repeats each recipe's kind and statuses

A page outside the 150-300 line target is reported as a warning, not an error.
"""

from __future__ import annotations

import argparse
import io
import json
import posixpath
import re
import sys
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

import gameplay_test_runner


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "Examples/MechanicsSandbox/recipes.json"
DEFAULT_API_MODEL = "Docs/generated/api.json"
PAGES_DIRECTORY = "Docs/en/how-to/mechanics"
HUB_PAGE = f"{PAGES_DIRECTORY}/index.md"
HUB_FRONT_MATTER = (
    ("layout", "default"),
    ("title", "Build Game Mechanics"),
    ("locale", "en"),
    ("document_id", "mechanics-recipes"),
    ("permalink", "/Docs/en/how-to/mechanics/"),
)
RECIPE_KINDS = ("primitive", "pattern")
BACKEND_LABELS = {"angelscript": "AngelScript", "managed": "Managed C#"}
# A backend table row is recognized by these words in the row text
BACKEND_ROW_PATTERNS = {"angelscript": re.compile(r"angelscript", re.IGNORECASE), "managed": re.compile(r"managed|c#", re.IGNORECASE)}
BACKEND_STATUSES = ("runtime-verified", "compile-verified", "shape")
FILE_CATEGORIES = ("angelscript", "managed", "content", "maps", "tests")
CATEGORY_SUFFIXES = {"angelscript": (".fos",), "managed": (".cs",)}
MANIFEST_FIELDS = {"schema_version", "description", "sandbox", "recipes", "cross_cutting"}
RECIPE_FIELDS = {"slug", "title", "kind", "backends", "page", "files", "symbols", "builtins", "events_fired_by_recipe", "smoke"}
RECIPE_OPTIONAL_FIELDS = {"builtins"}
CROSS_CUTTING_FIELDS = {"slug", "title", "page", "files", "symbols", "builtins", "events_fired_by_recipe"}
CROSS_CUTTING_OPTIONAL_FIELDS = {"title", "builtins", "events_fired_by_recipe"}
SMOKE_FIELDS = {"manifest", "scenarios"}
SLUG_PATTERN = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
RECIPE_FRONT_MATTER_KEYS = ("layout", "title", "locale", "document_id", "permalink", "recipe_kind")
PAGE_FRONT_MATTER_KEYS = ("layout", "title", "locale", "document_id", "permalink")
RECIPE_SECTIONS = (
    "Source paths inspected",
    "Problem",
    "Engine primitives used",
    "Minimal example",
    "Step by step",
    "Authority and sync",
    "Persistence and migration",
    "Pitfalls and anti-patterns",
    "Test route",
    "Managed C#",
    "How a larger project extends it",
    "See also",
)
PITFALL_SECTION = "Pitfalls and anti-patterns"
MINIMUM_PITFALLS = 3
PAGE_LINE_TARGET = (150, 300)
# Fences in these languages must quote a file of the same entry; other languages are free
EXCERPT_SUFFIXES = {
    "angelscript": (".fos",),
    "csharp": (".cs",),
    "ini": (".fomain", ".fopro", ".fotxt", ".fomap", ".focfg", ".ini"),
    "json": (".json",),
}
VESTIGIAL_EVENT_PATTERN = re.compile(r"\bEngine does not fire it\b")
SUBSCRIPTION_PATTERN = re.compile(r"\b(?P<event>On[A-Za-z0-9_]+)\s*\.\s*Subscribe\s*\(")
SYMBOL_ID_PATTERN = re.compile(
    r"(?<![\w.#-])(?:script\.(?:entity|enum|enum-value|event|method|property|ref-field|ref-method|ref-type|value-field|value-type)"
    r"|setting\.(?:common|client|server)|migration\.[A-Za-z]+)\.[A-Za-z0-9_.#-]*[A-Za-z0-9_]"
)
# Short tokens are anchored at a word start so ordinary words such as "atlas" or "self_" stay clean
PROJECT_NAME_PATTERNS = (
    re.compile(r"last\s+frontier", re.IGNORECASE),
    re.compile(r"lastfrontier", re.IGNORECASE),
    re.compile(r"fonline-tla", re.IGNORECASE),
    re.compile(r"\btla::", re.IGNORECASE),
    re.compile(r"\btla\b", re.IGNORECASE),
    re.compile(r"\blf_", re.IGNORECASE),
)
SANDBOX_TEXT_SUFFIXES = {
    ".cmake", ".cs", ".csproj", ".focfg", ".fomain", ".fomap", ".fopro", ".fos", ".fotxt",
    ".ini", ".json", ".md", ".props", ".py", ".targets", ".txt", ".yaml", ".yml",
}
SANDBOX_SKIPPED_DIRECTORIES = {"__pycache__", ".cache", ".git", ".vs", "bin", "node_modules", "obj"}
FENCE_OPEN_PATTERN = re.compile(r"^(?P<indent> *)(?P<fence>`{3,}|~{3,})[ \t]*(?P<info>[^\s`]*)[^`]*$")
HEADING_PATTERN = re.compile(r"^(?P<level>#{1,6})[ \t]+(?P<text>.+?)[ \t]*$")
LIST_ITEM_PATTERN = re.compile(r"^(?:[-*+]|\d+[.)])[ \t]+\S")
LINK_PATTERN = re.compile(r"(?<!!)\[[^\]]*\]\((?P<target>[^)\s]+)\)")
TABLE_SEPARATOR_CELL = re.compile(r":?-+:?")


class ConfigurationError(ValueError):
    pass


@dataclass(frozen=True)
class Finding:
    check: str
    path: str
    message: str
    severity: str = "error"

    def render(self) -> str:
        return f"{self.severity}[{self.check}] {self.path}: {self.message}"


@dataclass
class Entry:
    slug: str
    page: str
    title: str | None
    kind: str | None
    backends: dict[str, str]
    files: dict[str, list[str]]
    symbols: list[str]
    fired_events: list[str]
    smoke_manifest: str | None = None
    smoke_scenarios: list[str] = field(default_factory=list)
    cross_cutting: bool = False

    @property
    def label(self) -> str:
        return f"cross-cutting page {self.slug}" if self.cross_cutting else f"recipe {self.slug}"

    @property
    def listed_files(self) -> list[str]:
        return [path for paths in self.files.values() for path in paths]


@dataclass
class Fence:
    line: int
    language: str
    body: list[str]


@dataclass
class Document:
    lines: list[str]
    front_matter: list[tuple[str, str]] | None
    headings: list[tuple[int, int, str]]
    fences: list[Fence]
    prose: list[tuple[int, str]]
    unclosed_fence: int | None


@dataclass
class ApiModel:
    identifiers: set[str]
    events_by_name: dict[str, list[str]]
    vestigial_events: dict[str, list[str]]


class Report:
    def __init__(self) -> None:
        self.findings: list[Finding] = []

    def error(self, check: str, path: str, message: str) -> None:
        self.findings.append(Finding(check, path, message))

    def warning(self, check: str, path: str, message: str) -> None:
        self.findings.append(Finding(check, path, message, "warning"))


def validate(root: Path, manifest_path: Path | None = None, api_path: Path | None = None) -> list[Finding]:
    root = root.resolve()
    manifest_file = (manifest_path if manifest_path is not None else root / DEFAULT_MANIFEST).resolve()
    api_file = (api_path if api_path is not None else root / DEFAULT_API_MODEL).resolve()

    if not manifest_file.is_file():
        raise ConfigurationError(f"recipe manifest not found: {manifest_file}")

    api = load_api_model(api_file)
    report = Report()
    manifest_label = _display_path(root, manifest_file)
    parsed = _load_manifest(root, manifest_file, manifest_label, report)

    if parsed is not None:
        sandbox, entries = parsed
        for entry in entries:
            _check_entry(root, sandbox, entry, api, report)

        _check_hub(root, entries, report)
        _check_project_names(root, sandbox, entries, report)

    return report.findings


def validate_hub(root: Path) -> list[Finding]:
    report = Report()
    _check_hub(root.resolve(), None, report)
    return report.findings


def load_api_model(path: Path) -> ApiModel:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ConfigurationError(f"unable to read the API model {path}: {error}") from error

    symbols = raw.get("symbols") if isinstance(raw, dict) else None
    if not isinstance(symbols, list):
        raise ConfigurationError(f"API model has no symbols array: {path}")

    identifiers: set[str] = set()
    events_by_name: dict[str, list[str]] = {}
    vestigial: dict[str, list[str]] = {}

    for symbol in symbols:
        if not isinstance(symbol, dict) or not isinstance(symbol.get("id"), str):
            raise ConfigurationError(f"API model symbol without a string id: {path}")

        identifiers.add(symbol["id"])
        if isinstance(symbol.get("family_id"), str):
            identifiers.add(symbol["family_id"])

        if symbol.get("kind") == "event" and isinstance(symbol.get("name"), str):
            events_by_name.setdefault(symbol["name"], []).append(symbol["id"])
            if VESTIGIAL_EVENT_PATTERN.search(str(symbol.get("description", ""))):
                vestigial.setdefault(symbol["name"], []).append(symbol["id"])

    # A name is a trap only when every event declared under it is never fired
    vestigial_events = {name: ids for name, ids in vestigial.items() if len(ids) == len(events_by_name[name])}
    return ApiModel(identifiers, events_by_name, vestigial_events)


def parse_markdown(text: str) -> Document:
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    if lines and lines[-1] == "":
        lines.pop()

    front_matter: list[tuple[str, str]] | None = None
    index = 0

    if lines and lines[0] == "---":
        for end in range(1, len(lines)):
            if lines[end] == "---":
                front_matter = [_front_matter_pair(line) for line in lines[1:end]]
                index = end + 1
                break

    headings: list[tuple[int, int, str]] = []
    fences: list[Fence] = []
    prose: list[tuple[int, str]] = []
    unclosed_fence: int | None = None

    while index < len(lines):
        line = lines[index]
        opening = FENCE_OPEN_PATTERN.match(line)

        if opening is None:
            heading = HEADING_PATTERN.match(line)
            if heading is not None:
                headings.append((index + 1, len(heading.group("level")), heading.group("text")))
            prose.append((index + 1, line))
            index += 1
            continue

        # A fence inside a list item is indented; its content lines carry the same indentation
        marker = opening.group("fence")
        indent = len(opening.group("indent"))
        closing = re.compile(rf"^ *{re.escape(marker[0])}{{{len(marker)},}}[ \t]*$")
        start = index + 1
        body: list[str] = []
        index += 1

        while index < len(lines) and closing.match(lines[index]) is None:
            body.append(_remove_indent(lines[index], indent))
            index += 1

        if index >= len(lines):
            unclosed_fence = start

        fences.append(Fence(start, opening.group("info").lower(), body))
        index += 1

    return Document(lines, front_matter, headings, fences, prose, unclosed_fence)


def _check_entry(root: Path, sandbox: str, entry: Entry, api: ApiModel, report: Report) -> None:
    files_present = _check_files(root, sandbox, entry, report)
    _check_smoke(root, sandbox, entry, report)

    page_path = root / entry.page
    if not page_path.is_file():
        report.error("page", entry.page, f"page for {entry.label} does not exist")
        return

    document = _read_document(root, page_path, report)
    if document is None:
        return

    _check_page(entry, document, report)

    if files_present:
        _check_excerpts(root, entry, document, report)
        _check_events(root, entry, api, report)

    _check_symbols(entry, document, api, report)
    _check_mirror(root, entry, document, report)


def _load_manifest(root: Path, manifest_file: Path, label: str, report: Report) -> tuple[str, list[Entry]] | None:
    try:
        raw = json.loads(manifest_file.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        report.error("manifest", label, f"manifest is not readable JSON: {error}")
        return None

    if not isinstance(raw, dict):
        report.error("manifest", label, "manifest root must be an object")
        return None

    unknown = sorted(set(raw) - MANIFEST_FIELDS)
    if unknown:
        report.error("manifest", label, f"unknown field(s): {', '.join(unknown)}")

    schema = raw.get("schema_version")
    if isinstance(schema, bool) or schema != SCHEMA_VERSION:
        report.error("manifest", label, f"schema_version must be {SCHEMA_VERSION}, found {schema!r}")
        return None

    if "description" in raw and (not isinstance(raw["description"], str) or not raw["description"]):
        report.error("manifest", label, "description must be a non-empty string")

    sandbox = raw.get("sandbox")
    if not _is_relative_path(sandbox):
        report.error("manifest", label, "sandbox must be a repository-relative POSIX path")
        return None

    expected_sandbox = _display_path(root, manifest_file.parent)
    if sandbox != expected_sandbox:
        report.error("manifest", label, f"sandbox must name the manifest directory {expected_sandbox}, found {sandbox}")
    if not (root / sandbox).is_dir():
        report.error("files", label, f"sandbox directory does not exist: {sandbox}")

    recipes = raw.get("recipes")
    if not isinstance(recipes, list) or not recipes:
        report.error("manifest", label, "recipes must be a non-empty array")
        return None

    cross_cutting = raw.get("cross_cutting", [])
    if not isinstance(cross_cutting, list):
        report.error("manifest", label, "cross_cutting must be an array")
        cross_cutting = []

    entries: list[Entry] = []
    for index, value in enumerate(recipes):
        entry = _parse_entry(value, f"recipes[{index}]", False, label, report)
        if entry is not None:
            entries.append(entry)
    for index, value in enumerate(cross_cutting):
        entry = _parse_entry(value, f"cross_cutting[{index}]", True, label, report)
        if entry is not None:
            entries.append(entry)

    seen_slugs: set[str] = set()
    seen_pages: set[str] = set()
    for entry in entries:
        if entry.slug in seen_slugs:
            report.error("manifest", label, f"duplicate slug: {entry.slug}")
        if entry.page in seen_pages:
            report.error("manifest", label, f"duplicate page: {entry.page}")
        seen_slugs.add(entry.slug)
        seen_pages.add(entry.page)

    return sandbox, entries


def _parse_entry(value: object, where: str, cross_cutting: bool, label: str, report: Report) -> Entry | None:
    if not isinstance(value, dict):
        report.error("manifest", label, f"{where} must be an object")
        return None

    allowed = CROSS_CUTTING_FIELDS if cross_cutting else RECIPE_FIELDS
    optional = CROSS_CUTTING_OPTIONAL_FIELDS if cross_cutting else RECIPE_OPTIONAL_FIELDS
    errors_before = len(report.findings)

    unknown = sorted(set(value) - allowed)
    if unknown:
        report.error("manifest", label, f"{where} has unknown field(s): {', '.join(unknown)}")
    missing = sorted(allowed - optional - set(value))
    if missing:
        report.error("manifest", label, f"{where} is missing field(s): {', '.join(missing)}")
        return None

    slug = value["slug"]
    if not isinstance(slug, str) or SLUG_PATTERN.fullmatch(slug) is None:
        report.error("manifest", label, f"{where}.slug must be a lowercase kebab-case string")
        return None
    where = f"{where} ({slug})"

    title = value.get("title")
    if (title is not None or not cross_cutting) and (not isinstance(title, str) or not title.strip()):
        report.error("manifest", label, f"{where}.title must be a non-empty string")

    kind: str | None = None
    backends: dict[str, str] = {}
    if not cross_cutting:
        kind = value["kind"]
        if kind not in RECIPE_KINDS:
            report.error("manifest", label, f"{where}.kind must be one of {', '.join(RECIPE_KINDS)}, found {kind!r}")
        backends = _parse_backends(value["backends"], where, label, report)

    page = value["page"]
    expected_page = f"{PAGES_DIRECTORY}/{slug}.md"
    if page != expected_page:
        report.error("manifest", label, f"{where}.page must be {expected_page}, found {page!r}")

    files = _parse_files(value["files"], where, cross_cutting, backends, label, report)
    symbols = _string_list(value["symbols"], f"{where}.symbols", label, report, allow_empty=False)
    _string_list(value.get("builtins", []), f"{where}.builtins", label, report, allow_empty=True)
    fired_events = _string_list(value.get("events_fired_by_recipe", []), f"{where}.events_fired_by_recipe", label, report, allow_empty=True)

    smoke_manifest: str | None = None
    smoke_scenarios: list[str] = []
    if not cross_cutting:
        smoke = value["smoke"]
        if not isinstance(smoke, dict) or set(smoke) != SMOKE_FIELDS:
            report.error("manifest", label, f"{where}.smoke must be an object with exactly manifest and scenarios")
        else:
            smoke_manifest = smoke["manifest"]
            if not _is_relative_path(smoke_manifest):
                report.error("manifest", label, f"{where}.smoke.manifest must be a repository-relative POSIX path")
                smoke_manifest = None
            smoke_scenarios = _string_list(smoke["scenarios"], f"{where}.smoke.scenarios", label, report, allow_empty=False)

    if len(report.findings) != errors_before:
        return None

    return Entry(slug, page, title, kind, backends, files, symbols, fired_events, smoke_manifest, smoke_scenarios, cross_cutting)


def _parse_backends(value: object, where: str, label: str, report: Report) -> dict[str, str]:
    if not isinstance(value, dict) or set(value) != set(BACKEND_LABELS):
        report.error("manifest", label, f"{where}.backends must name exactly {', '.join(BACKEND_LABELS)}")
        return {}

    for backend, status in value.items():
        if status not in BACKEND_STATUSES:
            report.error("manifest", label, f"{where}.backends.{backend} must be one of {', '.join(BACKEND_STATUSES)}, found {status!r}")

    return dict(value)


def _parse_files(value: object, where: str, cross_cutting: bool, backends: dict[str, str], label: str, report: Report) -> dict[str, list[str]]:
    if not isinstance(value, dict) or not value:
        report.error("manifest", label, f"{where}.files must be a non-empty object")
        return {}

    unknown = sorted(set(value) - set(FILE_CATEGORIES))
    if unknown:
        report.error("manifest", label, f"{where}.files has unknown categories: {', '.join(unknown)}; use {', '.join(FILE_CATEGORIES)}")

    required = [] if cross_cutting else [backend for backend in BACKEND_LABELS if backend in backends]
    for category in required:
        if category not in value:
            report.error("manifest", label, f"{where}.files.{category} is required")

    files: dict[str, list[str]] = {}
    seen: set[str] = set()

    for category, paths in value.items():
        paths = _string_list(paths, f"{where}.files.{category}", label, report, allow_empty=False)
        for path in paths:
            if not _is_relative_path(path):
                report.error("manifest", label, f"{where}.files.{category} entry is not a repository-relative POSIX path: {path!r}")
            elif category in CATEGORY_SUFFIXES and not path.endswith(CATEGORY_SUFFIXES[category]):
                report.error("manifest", label, f"{where}.files.{category} entry must end with {', '.join(CATEGORY_SUFFIXES[category])}: {path}")
            if path in seen:
                report.error("manifest", label, f"{where}.files lists {path} twice")
            seen.add(path)
        files[category] = paths

    return files


def _string_list(value: object, where: str, label: str, report: Report, *, allow_empty: bool) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        report.error("manifest", label, f"{where} must be an array of non-empty strings")
        return []
    if not value and not allow_empty:
        report.error("manifest", label, f"{where} must not be empty")
    if len(value) != len(set(value)):
        report.error("manifest", label, f"{where} contains duplicates")
    return list(value)


def _check_files(root: Path, sandbox: str, entry: Entry, report: Report) -> bool:
    present = True

    for path in entry.listed_files:
        if not _is_inside(path, sandbox):
            report.error("files", path, f"file listed for {entry.label} is outside the example directory {sandbox}")
            present = False
        elif not (root / path).is_file():
            report.error("files", path, f"file listed for {entry.label} does not exist")
            present = False

    return present


def _check_smoke(root: Path, sandbox: str, entry: Entry, report: Report) -> None:
    if entry.smoke_manifest is None:
        return

    path = entry.smoke_manifest
    if not _is_inside(path, sandbox):
        report.error("files", path, f"smoke manifest of {entry.label} is outside the example directory {sandbox}")
        return
    if not (root / path).is_file():
        report.error("files", path, f"smoke manifest of {entry.label} does not exist")
        return

    try:
        smoke = gameplay_test_runner.load_manifest(root / path)
    except gameplay_test_runner.ManifestError as error:
        report.error("manifest", path, f"smoke manifest of {entry.label} is invalid: {error}")
        return

    scenario_ids = {scenario["id"] for scenario in smoke["scenarios"]}
    for scenario in entry.smoke_scenarios:
        if scenario not in scenario_ids:
            report.error("manifest", path, f"{entry.label} names smoke scenario {scenario!r} that the manifest does not define")


def _check_page(entry: Entry, document: Document, report: Report) -> None:
    page = entry.page
    expected_keys = PAGE_FRONT_MATTER_KEYS if entry.cross_cutting else RECIPE_FRONT_MATTER_KEYS
    document_id = f"mechanics-{entry.slug}" if entry.cross_cutting else f"recipe-{entry.slug}"
    expected_values = {
        "layout": "default",
        "locale": "en",
        "document_id": document_id,
        "permalink": "/" + page[: -len(".md")] + ".html",
        "recipe_kind": entry.kind,
    }

    if document.unclosed_fence is not None:
        report.error("page", page, f"fenced block opened at line {document.unclosed_fence} is not closed")

    front_matter = document.front_matter or []
    keys = tuple(key for key, _ in front_matter)
    if keys != expected_keys:
        report.error("page", page, f"front matter keys must be exactly {', '.join(expected_keys)} in this order, found {', '.join(keys) or 'none'}")
    values = dict(front_matter)

    for key, expected in expected_values.items():
        if key in expected_keys and key in values and values[key] != expected:
            report.error("page", page, f"front matter {key} must be {expected!r}, found {values[key]!r}")

    title = values.get("title", "")
    if entry.title is not None and title.casefold() != entry.title.casefold():
        report.error("page", page, f"front matter title {title!r} does not match the manifest title {entry.title!r}")

    first_h1 = next((text for _, level, text in document.headings if level == 1), None)
    if first_h1 is None or first_h1.casefold() != title.casefold():
        report.error("page", page, f"the first heading must be '# {title}'")

    sections = [text for _, level, text in document.headings if level == 2]
    if entry.cross_cutting:
        if not sections or sections[0] != RECIPE_SECTIONS[0]:
            report.error("page", page, f"the first ## section must be '{RECIPE_SECTIONS[0]}'")
    else:
        if sections != list(RECIPE_SECTIONS):
            report.error("page", page, f"## sections must be exactly, in order: {' | '.join(RECIPE_SECTIONS)}; found: {' | '.join(sections) or 'none'}")

        pitfalls = _section_list_items(document, PITFALL_SECTION)
        if pitfalls is not None and pitfalls < MINIMUM_PITFALLS:
            report.error("page", page, f"'## {PITFALL_SECTION}' needs at least {MINIMUM_PITFALLS} top-level list items, found {pitfalls}")

        _check_status_rows(entry, document, report)

    low, high = PAGE_LINE_TARGET
    if not low <= len(document.lines) <= high:
        report.warning("page", page, f"page has {len(document.lines)} lines; the target is {low}-{high}")


def _check_status_rows(entry: Entry, document: Document, report: Report) -> None:
    rows = [line for _, line in document.prose if line.lstrip().startswith("|")]

    for backend, status in entry.backends.items():
        pattern = BACKEND_ROW_PATTERNS[backend]
        backend_rows = [row for row in rows if pattern.search(row)]
        if not any(f"`{status}`" in row for row in backend_rows):
            report.error("page", entry.page, f"no table row shows {BACKEND_LABELS[backend]} with status `{status}` from the manifest")

        for row in backend_rows:
            shown = [candidate for candidate in BACKEND_STATUSES if f"`{candidate}`" in row]
            if len(shown) == 1 and shown[0] != status:
                report.error("page", entry.page, f"a table row shows {BACKEND_LABELS[backend]} as `{shown[0]}`; the manifest says `{status}`")


def _check_excerpts(root: Path, entry: Entry, document: Document, report: Report) -> None:
    for fence in document.fences:
        suffixes = EXCERPT_SUFFIXES.get(fence.language)
        if suffixes is None:
            continue

        where = f"fenced {fence.language} block at line {fence.line}"
        snippet = [line.rstrip() for line in fence.body]
        if not any(snippet):
            report.error("excerpt", entry.page, f"{where} is empty")
            continue

        candidates = [path for path in entry.listed_files if path.endswith(suffixes)]
        if not candidates:
            report.error("excerpt", entry.page, f"{where} has no {'/'.join(suffixes)} file listed for {entry.label} to quote")
            continue

        matches = [(*_longest_prefix_match(_source_lines(root / path), snippet), path) for path in candidates]
        matched, start, path = max(matches, key=lambda match: match[0])

        if matched == len(snippet):
            continue

        if matched == 0:
            report.error("excerpt", entry.page, f"{where} is not a contiguous excerpt of a file listed for {entry.label}; its first line occurs in none of them")
        else:
            report.error("excerpt", entry.page, f"{where} is not a contiguous excerpt of a file listed for {entry.label}; "
                         f"the longest match is {matched} of {len(snippet)} line(s) from {path}:{start + 1}, "
                         f"first differing line: {snippet[matched]!r}")


def _check_symbols(entry: Entry, document: Document, api: ApiModel, report: Report) -> None:
    text = "\n".join(line for _, line in document.prose)

    for symbol in entry.symbols:
        if symbol not in api.identifiers:
            report.error("symbol", entry.page, f"manifest symbol {symbol} of {entry.label} is not an id or family_id in the API model")
        if symbol not in text:
            report.error("symbol", entry.page, f"manifest symbol {symbol} of {entry.label} is not cited by the page")

    listed = set(entry.symbols)
    for line_number, line in document.prose:
        for match in SYMBOL_ID_PATTERN.finditer(line):
            if match.group(0) not in listed:
                report.error("symbol", entry.page, f"line {line_number} cites {match.group(0)}, which is not in the manifest symbols of {entry.label}")


def _check_events(root: Path, entry: Entry, api: ApiModel, report: Report) -> None:
    texts = {path: _source_text(root / path) for path in entry.listed_files}

    for fired in entry.fired_events:
        if fired.startswith("script.event.") and fired not in api.identifiers:
            report.error("event", entry.page, f"events_fired_by_recipe entry {fired} of {entry.label} is not an event in the API model")
        name = fired.rsplit(".", 1)[-1].split("#", 1)[0]
        fire_site = re.compile(rf"\b{re.escape(name)}\s*\.\s*Fire\s*\(")
        if not any(fire_site.search(text) for text in texts.values()):
            report.error("event", entry.page, f"{entry.label} lists {fired} in events_fired_by_recipe, but none of its files fires {name}")

    fired_names = {fired.rsplit(".", 1)[-1].split("#", 1)[0] for fired in entry.fired_events}
    for path, text in texts.items():
        if not path.endswith((".fos", ".cs")):
            continue
        for line_number, line in enumerate(text.split("\n"), start=1):
            for match in SUBSCRIPTION_PATTERN.finditer(line):
                name = match.group("event")
                if name in api.vestigial_events and name not in fired_names:
                    ids = ", ".join(api.vestigial_events[name])
                    report.error("event", f"{path}:{line_number}", f"{entry.label} subscribes to {name} ({ids}), which the Engine never fires; "
                                 "fire it in the recipe and list it in events_fired_by_recipe, or use an event the Engine fires")


def _check_mirror(root: Path, entry: Entry, document: Document, report: Report) -> None:
    mirror = _russian_path(entry.page)
    mirror_path = root / mirror

    if not mirror_path.is_file():
        report.error("mirror", mirror, f"Russian mirror of {entry.page} does not exist")
        return

    translated = _read_document(root, mirror_path, report)
    if translated is None:
        return

    _check_mirror_document(document, translated, entry.page, mirror, report)


def _check_mirror_document(document: Document, translated: Document, page: str, mirror: str, report: Report) -> None:
    source_matter = document.front_matter or []
    mirror_matter = translated.front_matter or []

    if [key for key, _ in mirror_matter] != [key for key, _ in source_matter]:
        report.error("mirror", mirror, "front matter keys differ from the English page")
    else:
        for (key, source_value), (_, mirror_value) in zip(source_matter, mirror_matter):
            expected = {"locale": "ru", "permalink": source_value.replace("/Docs/en/", "/Docs/ru/", 1), "title": None}.get(key, source_value)
            if expected is not None and mirror_value != expected:
                report.error("mirror", mirror, f"front matter {key} must be {expected!r}, found {mirror_value!r}")

    source_sections = sum(1 for _, level, _ in document.headings if level == 2)
    mirror_sections = sum(1 for _, level, _ in translated.headings if level == 2)
    if source_sections != mirror_sections:
        report.error("mirror", mirror, f"has {mirror_sections} ## headings; {page} has {source_sections}")

    source_fences = [(fence.language, fence.body) for fence in document.fences]
    mirror_fences = [(fence.language, fence.body) for fence in translated.fences]
    if source_fences != mirror_fences:
        index = next((i for i, (a, b) in enumerate(zip(source_fences, mirror_fences)) if a != b), min(len(source_fences), len(mirror_fences)))
        report.error("mirror", mirror, f"fenced blocks differ from {page} (English {len(source_fences)}, Russian {len(mirror_fences)}, first difference at block {index + 1})")


def _check_hub(root: Path, entries: list[Entry] | None, report: Report) -> None:
    hub_path = root / HUB_PAGE
    if not hub_path.is_file():
        report.error("hub", HUB_PAGE, "the mechanics section index does not exist")
        return

    document = _read_document(root, hub_path, report)
    if document is None:
        return

    if document.front_matter != list(HUB_FRONT_MATTER):
        expected = ", ".join(f"{key}: {value}" for key, value in HUB_FRONT_MATTER)
        report.error("hub", HUB_PAGE, f"front matter must be exactly {expected}")

    sections = [text for _, level, text in document.headings if level == 2]
    if not sections or sections[0] != RECIPE_SECTIONS[0]:
        report.error("hub", HUB_PAGE, f"the first ## section must be '{RECIPE_SECTIONS[0]}'")

    mirror = _russian_path(HUB_PAGE)
    translated = _read_document(root, root / mirror, report) if (root / mirror).is_file() else None
    if translated is None:
        report.error("mirror", mirror, f"Russian mirror of {HUB_PAGE} does not exist")
    else:
        _check_mirror_document(document, translated, HUB_PAGE, mirror, report)

    if entries is None:
        return

    for entry in entries:
        _check_hub_entry(HUB_PAGE, document, entry.page, entry, True, report)
        if translated is not None:
            _check_hub_entry(mirror, translated, _russian_path(entry.page), entry, False, report)


def _check_hub_entry(hub: str, document: Document, page: str, entry: Entry, by_column: bool, report: Report) -> None:
    check = "hub" if hub == HUB_PAGE else "mirror"
    linking = [(line_number, line) for line_number, line in document.prose if page in _link_targets(hub, line)]

    if not linking:
        report.error(check, hub, f"does not link {page}")
        return
    if entry.cross_cutting:
        return

    rows = [line for _, line in linking if line.lstrip().startswith("|")]
    if not rows:
        report.error(check, hub, f"has no table row linking {page}")
        return

    expected = {"kind": entry.kind, **entry.backends}
    if not by_column:
        for name, value in expected.items():
            if not any(f"`{value}`" in row for row in rows):
                report.error(check, hub, f"the table row for {entry.slug} does not show {name} `{value}`")
        return

    columns = _hub_columns(document)
    if columns is None:
        report.error(check, hub, "has no recipe table with Kind, AngelScript and Managed C# columns")
        return

    for row in rows:
        cells = _table_cells(row)
        for name, value in expected.items():
            index = columns[name]
            shown = cells[index] if index < len(cells) else ""
            if f"`{value}`" not in shown:
                report.error(check, hub, f"the table row for {entry.slug} shows {name} {shown or 'nothing'}; the manifest says `{value}`")


def _hub_columns(document: Document) -> dict[str, int] | None:
    for _, line in document.prose:
        if not line.lstrip().startswith("|"):
            continue
        cells = [cell.casefold() for cell in _table_cells(line)]
        columns = {}
        for name, word in (("kind", "kind"), ("angelscript", "angelscript"), ("managed", "managed")):
            matches = [index for index, cell in enumerate(cells) if word in cell]
            if len(matches) == 1:
                columns[name] = matches[0]
        if len(columns) == 3:
            return columns
    return None


def _check_project_names(root: Path, sandbox: str, entries: list[Entry], report: Report) -> None:
    paths: list[str] = []

    for page in [HUB_PAGE] + [entry.page for entry in entries]:
        paths.extend([page, _russian_path(page)])

    sandbox_root = root / sandbox
    if sandbox_root.is_dir():
        for path in sorted(sandbox_root.rglob("*")):
            relative = path.relative_to(sandbox_root).parts
            skipped = any(part.casefold() in SANDBOX_SKIPPED_DIRECTORIES or part.casefold().startswith("build") for part in relative[:-1])
            if path.is_file() and not skipped and path.suffix.lower() in SANDBOX_TEXT_SUFFIXES:
                paths.append(_display_path(root, path))

    for path in dict.fromkeys(paths):
        if not (root / path).is_file():
            continue
        for line_number, line in enumerate(_source_text(root / path).split("\n"), start=1):
            for pattern in PROJECT_NAME_PATTERNS:
                match = pattern.search(line)
                if match is not None:
                    report.error("project-name", f"{path}:{line_number}", f"names an embedding project ({match.group(0)!r}); Engine recipes stay project-neutral")
                    break


def _section_list_items(document: Document, section: str) -> int | None:
    starts = [line for line, level, text in document.headings if level == 2 and text == section]
    if not starts:
        return None

    following = [line for line, level, _ in document.headings if level <= 2 and line > starts[0]]
    end = following[0] if following else len(document.lines) + 1
    return sum(1 for line_number, line in document.prose if starts[0] < line_number < end and LIST_ITEM_PATTERN.match(line))


def _longest_prefix_match(lines: list[str], snippet: list[str]) -> tuple[int, int]:
    best = (0, 0)

    for start in range(len(lines)):
        matched = 0
        while matched < len(snippet) and start + matched < len(lines) and lines[start + matched] == snippet[matched]:
            matched += 1
        if matched > best[0]:
            best = (matched, start)
            if matched == len(snippet):
                break

    return best


def _link_targets(document_path: str, line: str) -> set[str]:
    base = posixpath.dirname(document_path)
    targets = set()

    for match in LINK_PATTERN.finditer(line):
        target = match.group("target").split("#", 1)[0]
        if target and "://" not in target:
            targets.add(posixpath.normpath(posixpath.join(base, target)))

    return targets


def _table_cells(row: str) -> list[str]:
    cells = [cell.strip() for cell in row.strip().strip("|").split("|")]
    return [] if all(TABLE_SEPARATOR_CELL.fullmatch(cell) for cell in cells) else cells


def _read_document(root: Path, path: Path, report: Report) -> Document | None:
    try:
        return parse_markdown(path.read_text(encoding="utf-8"))
    except UnicodeDecodeError as error:
        report.error("page", _display_path(root, path), f"is not UTF-8: {error}")
        return None


def _source_text(path: Path) -> str:
    return path.read_bytes().decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")


def _source_lines(path: Path) -> list[str]:
    return [line.rstrip() for line in _source_text(path).split("\n")]


def _front_matter_pair(line: str) -> tuple[str, str]:
    key, separator, value = line.partition(":")
    return (key.strip(), value.strip()) if separator else (line.strip(), "")


def _remove_indent(line: str, indent: int) -> str:
    removable = len(line) - len(line.lstrip(" "))
    return line[min(indent, removable):]


def _russian_path(page: str) -> str:
    return "Docs/ru/" + page[len("Docs/en/"):] if page.startswith("Docs/en/") else page


def _is_relative_path(value: object) -> bool:
    if not isinstance(value, str) or not value or "\\" in value or value.startswith("/") or re.match(r"^[A-Za-z]:", value):
        return False
    return all(part not in ("", ".", "..") for part in value.split("/"))


def _is_inside(path: str, directory: str) -> bool:
    return PurePosixPath(path).parts[: len(PurePosixPath(directory).parts)] == PurePosixPath(directory).parts


def _display_path(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="mechanics_recipes.py",
        description="Check mechanics recipe pages against their recipe manifest, example files and the generated API model",
    )
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--check", action="store_true",
                        help="validate the manifest, pages, Russian mirrors, section index and example files")
    action.add_argument("--list", action="store_true",
                        help="print the recipes and cross-cutting pages from the manifest without validating them")
    parser.add_argument("--root", type=Path, default=None,
                        help="Engine checkout to check (default: the checkout that contains this script)")
    parser.add_argument("--manifest", type=Path, default=None,
                        help=f"recipe manifest (default: {DEFAULT_MANIFEST} under --root)")
    parser.add_argument("--api-model", type=Path, default=None,
                        help=f"generated API model (default: {DEFAULT_API_MODEL} under --root)")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    _use_utf8_output()

    root = (args.root if args.root is not None else Path(__file__).resolve().parents[1]).resolve()
    if not root.is_dir():
        print(f"mechanics_recipes: root is not a directory: {root}", file=sys.stderr)
        return 2

    if args.list:
        return _list_entries(root, args.manifest)

    try:
        findings = validate(root, args.manifest, args.api_model)
    except ConfigurationError as error:
        print(f"mechanics_recipes: {error}", file=sys.stderr)
        return 2

    for finding in findings:
        print(finding.render())

    errors = sum(1 for finding in findings if finding.severity == "error")
    warnings = len(findings) - errors
    print(f"mechanics_recipes: {errors} error(s), {warnings} warning(s)")
    return 1 if errors else 0


def _list_entries(root: Path, manifest_path: Path | None) -> int:
    manifest_file = (manifest_path if manifest_path is not None else root / DEFAULT_MANIFEST).resolve()
    report = Report()
    parsed = _load_manifest(root, manifest_file, _display_path(root, manifest_file), report) if manifest_file.is_file() else None

    if parsed is None:
        for finding in report.findings:
            print(finding.render(), file=sys.stderr)
        print(f"mechanics_recipes: no readable recipe manifest at {manifest_file}", file=sys.stderr)
        return 2

    for entry in parsed[1]:
        if entry.cross_cutting:
            print(f"{entry.slug}\tcross-cutting\t{entry.page}")
        else:
            statuses = " ".join(f"{backend}={status}" for backend, status in entry.backends.items())
            print(f"{entry.slug}\t{entry.kind}\t{statuses}\t{entry.page}")
    return 0


def _use_utf8_output() -> None:
    # Redirected output would otherwise use the ANSI code page on Windows and fail on non-Latin paths
    for stream in (sys.stdout, sys.stderr):
        if isinstance(stream, io.TextIOWrapper):
            stream.reconfigure(encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
