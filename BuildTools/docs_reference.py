from __future__ import annotations

import argparse
import hashlib
import html
import json
import posixpath
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any
from urllib.parse import quote, urlsplit

import docs_api


DEFAULT_MODEL = docs_api.DEFAULT_OUTPUT
DEFAULT_OUTPUT_DIR = "Docs/generated/api"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-api-index", "Generated API Reference"),
    ("methods.md", "generated-api-methods", "Native Script Methods"),
    ("properties.md", "generated-api-properties", "Entity Properties"),
    ("events.md", "generated-api-events", "Engine Events"),
    ("types.md", "generated-api-types", "Script Types"),
    ("settings.md", "generated-api-settings", "Engine Settings"),
    ("migrations.md", "generated-api-migrations", "Migration Rules"),
)
OUTPUT_PATHS = tuple(f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS)
PAGE_LINKS = {
    "method": "methods.md",
    "property": "properties.md",
    "event": "events.md",
    "entity": "types.md",
    "enum": "types.md",
    "enum-value": "types.md",
    "value-type": "types.md",
    "value-field": "types.md",
    "ref-type": "types.md",
    "ref-field": "types.md",
    "ref-method": "types.md",
    "setting": "settings.md",
    "migration-rule": "migrations.md",
}
TYPE_KINDS = {
    "entity",
    "enum",
    "enum-value",
    "value-type",
    "value-field",
    "ref-type",
    "ref-field",
    "ref-method",
}

Symbol = dict[str, Any]


def _text(value: object) -> str:
    result = html.escape(str(value), quote=True)
    result = result.replace("|", "&#124;").replace("{", "&#123;").replace("}", "&#125;")
    return result.replace("\r\n", "<br>").replace("\r", "<br>").replace("\n", "<br>")


def _code(value: object) -> str:
    return f"<code>{_text(value)}</code>"


def _anchor(prefix: str, identity: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", identity.lower()).strip("-")[:72] or "entry"
    digest = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:10]
    return f"{prefix}-{slug}-{digest}"


def _symbol_anchor(symbol: Symbol) -> str:
    return _anchor("symbol", str(symbol["id"]))


def _symbol_id_cell(symbol: Symbol) -> str:
    return f'<a id="{_symbol_anchor(symbol)}"></a>{_code(symbol["id"])}'


def _description(symbol: Symbol) -> str:
    description = str(symbol.get("description", "")).strip()
    return _text(description) if description else "-"


def _runtime_sides(symbol: Symbol) -> str:
    sides = symbol.get("runtime_sides", [])
    return ", ".join(_text(side) for side in sides) if isinstance(sides, list) and sides else "-"


def _flags(symbol: Symbol) -> str:
    flags = symbol.get("flags", [])
    return ", ".join(_code(flag) for flag in flags) if isinstance(flags, list) and flags else "-"


def _location_link(model: dict[str, Any], source: object) -> str:
    if not isinstance(source, dict):
        return "-"

    path = source.get("path")
    line = source.get("line")
    if not isinstance(path, str) or not isinstance(line, int):
        return "-"

    scope = model.get("scope", {})
    repository = scope.get("repository", "cvet/fonline") if isinstance(scope, dict) else "cvet/fonline"
    url = f"https://github.com/{quote(str(repository), safe='/')}/blob/{SOURCE_REF}/{quote(path, safe='/')}#L{line}"
    return f"[{_text(path)}:{line}]({url})"


def _source_link(model: dict[str, Any], symbol: Symbol) -> str:
    return _location_link(model, symbol.get("source"))


def _example_link(example: str) -> str:
    parsed = urlsplit(example)
    if parsed.scheme:
        return f"[{_text(example)}]({example})"

    path_text, separator, fragment = example.partition("#")
    relative_path = posixpath.relpath(path_text, DEFAULT_OUTPUT_DIR)
    target = quote(relative_path, safe="/")
    if separator:
        target += "#" + quote(fragment)
    return f"[{_text(example)}]({target})"


def _contract(model: dict[str, Any], symbol: Symbol) -> str:
    stability = str(symbol.get("stability", "internal"))
    contract = symbol.get("contract")
    explicit = isinstance(contract, dict) and contract.get("explicit") is True
    parts = [f"{_code(stability)} ({'explicit' if explicit else 'default'})"]

    since = symbol.get("since")
    if since is not None:
        parts.append(f"since {_code(since)}")

    deprecated = symbol.get("deprecated")
    if isinstance(deprecated, dict):
        parts.append(f"deprecated since {_code(deprecated.get('since', ''))}")
        parts.append(f"replacement {_code(deprecated.get('replacement', ''))}")
        parts.append(f"removal {_code(deprecated.get('removal', ''))}")

    examples = symbol.get("examples")
    if isinstance(examples, list) and examples:
        parts.append("examples: " + ", ".join(_example_link(str(example)) for example in examples))

    if explicit and isinstance(contract, dict):
        parts.append("contract source: " + _location_link(model, contract.get("source")))
        notes = str(contract.get("notes", "")).strip()
        if notes:
            parts.append(_text(notes))

    return "<br>".join(parts)


def _front_matter(document_id: str, title: str) -> list[str]:
    return [
        "---",
        f"title: {title}",
        f"document_id: {document_id}",
        "locale: en",
        "generated: true",
        "---",
        "",
    ]


def _page_header(document_id: str, title: str) -> list[str]:
    lines = _front_matter(document_id, title)
    lines.extend(
        [
            f"# {title}",
            "",
            "> Generated reference. Do not edit this page directly. Update engine metadata, regenerate "
            "`Docs/generated/api.json`, then run `python BuildTools/docs_reference.py --write`.",
            "",
            "A dash in the description column means that the owning source metadata has no documentation comment. "
            "Every contract cell identifies an explicit source classification or the default `internal` policy; "
            "script reachability alone does not make a symbol public or stable.",
            "",
            "[Reference index](index.md) | [Canonical JSON model](../api.json) | "
            "[Generation contract](../../GeneratedApiAndMetadata.md)",
            "",
        ]
    )
    return lines


def _symbols(model: dict[str, Any], *kinds: str) -> list[Symbol]:
    raw_symbols = model.get("symbols")
    if not isinstance(raw_symbols, list):
        raise ValueError("API model symbols must be an array")

    selected = [symbol for symbol in raw_symbols if isinstance(symbol, dict) and symbol.get("kind") in kinds]
    return sorted(selected, key=lambda symbol: (str(symbol.get("name", "")), str(symbol.get("id", ""))))


def _grouped(symbols: list[Symbol], key_fields: tuple[str, ...]) -> dict[tuple[str, ...], list[Symbol]]:
    groups: dict[tuple[str, ...], list[Symbol]] = defaultdict(list)
    for symbol in symbols:
        key = tuple(str(symbol.get(field) or "global") for field in key_fields)
        groups[key].append(symbol)
    return dict(sorted(groups.items()))


def _table(lines: list[str], headers: tuple[str, ...], rows: list[tuple[str, ...]]) -> None:
    lines.append("| " + " | ".join(headers) + " |")
    lines.append("| " + " | ".join("---" for _ in headers) + " |")
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")


def _render_index(model: dict[str, Any]) -> str:
    lines = _front_matter("generated-api-index", "Generated API Reference")
    lines.extend(
        [
            "# Generated API Reference",
            "",
            "> Generated reference. Do not edit these pages directly. The canonical input is "
            "[`Docs/generated/api.json`](../api.json), produced from the engine's native codegen metadata parser.",
            "",
            "This reference describes the declarations in the model's `engine-native-codegen` scope. "
            "It is searchable, source-linked input for developers and AI agents, but it is not yet the complete "
            "stable public API contract.",
            "",
        ]
    )

    counts: dict[str, int] = defaultdict(int)
    for symbol in _symbols(model, *PAGE_LINKS):
        counts[str(symbol["kind"])] += 1

    page_rows = [
        ("[Native script methods](methods.md)", str(counts["method"]), "Native methods exported to scripts."),
        ("[Entity properties](properties.md)", str(counts["property"]), "Generated entity property contracts."),
        ("[Engine events](events.md)", str(counts["event"]), "Server, client, common, and mapper events."),
        (
            "[Script types](types.md)",
            str(sum(counts[kind] for kind in TYPE_KINDS)),
            "Entities, enums, value types, reference types, fields, and methods.",
        ),
        ("[Engine settings](settings.md)", str(counts["setting"]), "Fixed and runtime-variable engine settings."),
        (
            "[Migration rules](migrations.md)",
            str(counts["migration-rule"]),
            "Native metadata migration declarations.",
        ),
    ]
    _table(lines, ("Reference", "Symbols", "Coverage"), page_rows)

    summary = model.get("summary", {})
    if not isinstance(summary, dict):
        summary = {}
    lines.extend(["## Model quality", ""])
    _table(
        lines,
        ("Signal", "Count"),
        [
            ("Addressable symbols", str(summary.get("symbol_count", sum(counts.values())))),
            ("Symbols with descriptions", str(summary.get("described_symbol_count", 0))),
            ("Symbols missing descriptions", str(summary.get("missing_description_count", 0))),
            ("Symbols without source provenance", str(summary.get("symbols_without_source_count", 0))),
            ("Metadata source files", str(summary.get("metadata_source_file_count", 0))),
            ("Explicit contract declarations", str(summary.get("explicit_contract_declaration_count", 0))),
            ("Explicitly classified symbols", str(summary.get("explicit_contract_symbol_count", 0))),
            ("Default-internal symbols", str(summary.get("default_contract_symbol_count", 0))),
        ],
    )

    stability_counts = summary.get("symbols_by_stability", {})
    if isinstance(stability_counts, dict):
        lines.extend(["## Stability labels", ""])
        _table(
            lines,
            ("Label", "Symbols"),
            [(_code(label), str(count)) for label, count in sorted(stability_counts.items())],
        )

    scope = model.get("scope", {})
    if not isinstance(scope, dict):
        scope = {}
    lines.extend(["## Scope", "", "Included:", ""])
    for item in scope.get("included", []):
        lines.append(f"- {_text(item)}")
    lines.extend(["", "Excluded from the current model:", ""])
    for item in scope.get("excluded", []):
        lines.append(f"- {_text(item)}")
    lines.extend(
        [
            "",
            "Project-facing CMake declarations are intentionally outside this model; use the separate "
            "[generated CMake project-interface reference](../cmake/index.md).",
            "",
            "The main BuildTools command line is also outside this model; use the separate "
            "[parser-backed CLI reference](../cli/index.md).",
            "",
            "Package declarations and payloads use their own runtime-consumed contract; use the separate "
            "[package interface reference](../package/index.md).",
            "",
            "Source links follow the repository's `master` branch. The path and line stored in the canonical JSON "
            "are the generated provenance record; revision-pinned links remain part of the publication roadmap.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_methods(model: dict[str, Any]) -> str:
    lines = _page_header("generated-api-methods", "Native Script Methods")
    methods = _symbols(model, "method")
    lines.extend([f"This page contains **{len(methods)}** native methods exported to scripts.", ""])

    for (target, receiver), group in _grouped(methods, ("declared_target", "receiver")).items():
        lines.extend(
            [
                f'<a id="{_anchor("group", f"method:{target}:{receiver}")}"></a>',
                f"## {_code(receiver)} ({_text(target)})",
                "",
            ]
        )
        rows: list[tuple[str, ...]] = []
        for symbol in group:
            receivers = symbol.get("receivers", [])
            applies_to = ", ".join(_code(receiver_name) for receiver_name in receivers) if receivers else "-"
            rows.append(
                (
                    _code(symbol["signature"]),
                    _symbol_id_cell(symbol),
                    applies_to,
                    _runtime_sides(symbol),
                    _contract(model, symbol),
                    _flags(symbol),
                    _source_link(model, symbol),
                    _description(symbol),
                )
            )
        _table(
            lines,
            ("Signature", "Symbol ID", "Applies to", "Runtime", "Contract", "Flags", "Source", "Description"),
            rows,
        )
    return "\n".join(lines)


def _render_properties(model: dict[str, Any]) -> str:
    lines = _page_header("generated-api-properties", "Entity Properties")
    properties = _symbols(model, "property")
    lines.extend([f"This page contains **{len(properties)}** generated entity properties.", ""])

    for (receiver,), group in _grouped(properties, ("receiver",)).items():
        lines.extend(
            [
                f'<a id="{_anchor("group", f"property:{receiver}")}"></a>',
                f"## {_code(receiver)}",
                "",
            ]
        )
        rows: list[tuple[str, ...]] = []
        for symbol in group:
            contract = [str(symbol.get("mutability", "unknown"))]
            if symbol.get("nullable") is True:
                contract.append("nullable")
            if symbol.get("persistent") is True:
                contract.append("persistent")
            rows.append(
                (
                    _code(symbol["signature"]),
                    _symbol_id_cell(symbol),
                    _text(symbol.get("access", "unknown")),
                    _text(", ".join(contract)),
                    _runtime_sides(symbol),
                    _contract(model, symbol),
                    _flags(symbol),
                    _source_link(model, symbol),
                    _description(symbol),
                )
            )
        _table(
            lines,
            (
                "Signature",
                "Symbol ID",
                "Access",
                "Property contract",
                "Runtime",
                "API contract",
                "Flags",
                "Source",
                "Description",
            ),
            rows,
        )
    return "\n".join(lines)


def _render_events(model: dict[str, Any]) -> str:
    lines = _page_header("generated-api-events", "Engine Events")
    events = _symbols(model, "event")
    lines.extend([f"This page contains **{len(events)}** exported engine events.", ""])

    for (target, receiver), group in _grouped(events, ("declared_target", "receiver")).items():
        lines.extend(
            [
                f'<a id="{_anchor("group", f"event:{target}:{receiver}")}"></a>',
                f"## {_code(receiver)} ({_text(target)})",
                "",
            ]
        )
        rows = [
            (
                _code(symbol["signature"]),
                _symbol_id_cell(symbol),
                _runtime_sides(symbol),
                _contract(model, symbol),
                _flags(symbol),
                _source_link(model, symbol),
                _description(symbol),
            )
            for symbol in group
        ]
        _table(lines, ("Signature", "Symbol ID", "Runtime", "Contract", "Flags", "Source", "Description"), rows)
    return "\n".join(lines)


def _type_summary(lines: list[str], model: dict[str, Any], symbol: Symbol) -> None:
    lines.extend(
        [
            f'<a id="{_symbol_anchor(symbol)}"></a>',
            f"### {_code(symbol['name'])}",
            "",
            f"{_code(symbol['signature'])}  ",
            f"Symbol ID: {_code(symbol['id'])}  ",
            f"Runtime: {_runtime_sides(symbol)}  ",
            f"Contract: {_contract(model, symbol)}  ",
            f"Flags: {_flags(symbol)}  ",
            f"Source: {_source_link(model, symbol)}",
            "",
            _description(symbol),
            "",
        ]
    )


def _members_by_parent(symbols: list[Symbol]) -> dict[str, list[Symbol]]:
    members: dict[str, list[Symbol]] = defaultdict(list)
    for symbol in symbols:
        members[str(symbol.get("parent_id", ""))].append(symbol)
    return members


def _render_types(model: dict[str, Any]) -> str:
    lines = _page_header("generated-api-types", "Script Types")
    type_symbols = _symbols(model, *TYPE_KINDS)
    lines.extend([f"This page contains **{len(type_symbols)}** type and member symbols.", ""])

    entities = _symbols(model, "entity")
    lines.extend(["## Entities", ""])
    entity_rows: list[tuple[str, ...]] = []
    for symbol in entities:
        capabilities = symbol.get("capabilities", {})
        enabled_capabilities = (
            [name.replace("_", "-") for name, enabled in capabilities.items() if enabled]
            if isinstance(capabilities, dict)
            else []
        )
        entity_rows.append(
            (
                _code(symbol["signature"]),
                _symbol_id_cell(symbol),
                _text(", ".join(enabled_capabilities)) if enabled_capabilities else "-",
                _runtime_sides(symbol),
                _contract(model, symbol),
                _source_link(model, symbol),
                _description(symbol),
            )
        )
    _table(lines, ("Entity", "Symbol ID", "Capabilities", "Runtime", "Contract", "Source", "Description"), entity_rows)

    enum_values = _members_by_parent(_symbols(model, "enum-value"))
    lines.extend(["## Enums", ""])
    for symbol in _symbols(model, "enum"):
        _type_summary(lines, model, symbol)
        rows = [
            (
                _code(value["name"]),
                _code(value.get("value", "")),
                _text(value.get("evaluated_value", "")),
                _symbol_id_cell(value),
                _contract(model, value),
                _source_link(model, value),
                _description(value),
            )
            for value in enum_values.pop(str(symbol["id"]), [])
        ]
        _table(lines, ("Value", "Declared", "Numeric", "Symbol ID", "Contract", "Source", "Description"), rows)

    value_fields = _members_by_parent(_symbols(model, "value-field"))
    lines.extend(["## Value Types", ""])
    for symbol in _symbols(model, "value-type"):
        _type_summary(lines, model, symbol)
        rows = [
            (
                _code(field["signature"]),
                _symbol_id_cell(field),
                _text(field.get("mutability", "value")),
                _contract(model, field),
                _source_link(model, field),
                _description(field),
            )
            for field in value_fields.pop(str(symbol["id"]), [])
        ]
        _table(lines, ("Field", "Symbol ID", "Value contract", "API contract", "Source", "Description"), rows)

    ref_members = _members_by_parent(_symbols(model, "ref-field", "ref-method"))
    lines.extend(["## Reference Types", ""])
    for symbol in _symbols(model, "ref-type"):
        _type_summary(lines, model, symbol)
        rows = [
            (
                _text(member["kind"]),
                _code(member["signature"]),
                _symbol_id_cell(member),
                _text(member.get("mutability", "callable")),
                _contract(model, member),
                _source_link(model, member),
                _description(member),
            )
            for member in ref_members.pop(str(symbol["id"]), [])
        ]
        _table(
            lines,
            ("Kind", "Member", "Symbol ID", "Member contract", "API contract", "Source", "Description"),
            rows,
        )

    orphan_members = [
        member
        for groups in (enum_values, value_fields, ref_members)
        for group in groups.values()
        for member in group
    ]
    if orphan_members:
        lines.extend(["## Unattached Type Members", ""])
        rows = [
            (
                _text(member["kind"]),
                _code(member["signature"]),
                _symbol_id_cell(member),
                _contract(model, member),
                _source_link(model, member),
                _description(member),
            )
            for member in sorted(orphan_members, key=lambda item: str(item["id"]))
        ]
        _table(lines, ("Kind", "Member", "Symbol ID", "Contract", "Source", "Description"), rows)

    return "\n".join(lines)


def _render_settings(model: dict[str, Any]) -> str:
    lines = _page_header("generated-api-settings", "Engine Settings")
    settings = _symbols(model, "setting")
    lines.extend(
        [
            f"This page contains **{len(settings)}** settings parsed from native `ExportSettings` metadata.",
            "",
            "`CLI redaction default` reports whether the setting name matches the default "
            "`Common.SecretSettingTokens` command-line masking policy. It is not a semantic credential or "
            "sensitivity classification.",
            "",
        ]
    )

    for (target, group_name), group in _grouped(settings, ("declared_target", "group")).items():
        lines.extend(
            [
                f'<a id="{_anchor("group", f"setting:{target}:{group_name}")}"></a>',
                f"## {_code(group_name)} ({_text(target)})",
                "",
            ]
        )
        rows = [
            (
                _code(symbol["signature"]),
                _symbol_id_cell(symbol),
                _text(symbol.get("mutability", "unknown")),
                _runtime_sides(symbol),
                "yes" if symbol.get("command_line_redacted_by_default") is True else "no",
                _contract(model, symbol),
                _source_link(model, symbol),
                _description(symbol),
            )
            for symbol in group
        ]
        _table(
            lines,
            (
                "Setting",
                "Symbol ID",
                "Mutability",
                "Runtime",
                "CLI redaction default",
                "Contract",
                "Source",
                "Description",
            ),
            rows,
        )
    return "\n".join(lines)


def _render_migrations(model: dict[str, Any]) -> str:
    lines = _page_header("generated-api-migrations", "Migration Rules")
    migrations = _symbols(model, "migration-rule")
    lines.extend([f"This page contains **{len(migrations)}** native metadata migration rules.", ""])

    for (rule_kind,), group in _grouped(migrations, ("rule_kind",)).items():
        lines.extend(
            [
                f'<a id="{_anchor("group", f"migration:{rule_kind}")}"></a>',
                f"## {_code(rule_kind)}",
                "",
            ]
        )
        rows = [
            (
                _code(symbol.get("scope", "")),
                _code(symbol["name"]),
                _code(symbol.get("replacement", "")),
                _symbol_id_cell(symbol),
                _contract(model, symbol),
                _source_link(model, symbol),
                _description(symbol),
            )
            for symbol in group
        ]
        _table(
            lines,
            ("Scope", "Previous name", "Replacement", "Symbol ID", "Contract", "Source", "Description"),
            rows,
        )
    return "\n".join(lines)


def generate_reference_pages(model: dict[str, Any]) -> dict[str, str]:
    if model.get("schema_version") != docs_api.SCHEMA_VERSION:
        raise ValueError("Unsupported API model schema version")
    if model.get("generated_by") != "BuildTools/docs_api.py":
        raise ValueError("API reference input must be generated by BuildTools/docs_api.py")

    raw_symbols = model.get("symbols")
    if not isinstance(raw_symbols, list) or any(not isinstance(symbol, dict) for symbol in raw_symbols):
        raise ValueError("API model symbols must be an array of objects")
    unknown_kinds = sorted({str(symbol.get("kind")) for symbol in raw_symbols if symbol.get("kind") not in PAGE_LINKS})
    if unknown_kinds:
        raise ValueError(f"Unsupported API symbol kinds: {', '.join(unknown_kinds)}")
    symbol_ids = [symbol.get("id") for symbol in raw_symbols]
    if any(not isinstance(symbol_id, str) or not symbol_id for symbol_id in symbol_ids):
        raise ValueError("Every API symbol must have a non-empty string ID")
    if len(symbol_ids) != len(set(symbol_ids)):
        raise ValueError("API model contains duplicate symbol IDs")

    pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/methods.md": _render_methods(model),
        f"{DEFAULT_OUTPUT_DIR}/properties.md": _render_properties(model),
        f"{DEFAULT_OUTPUT_DIR}/events.md": _render_events(model),
        f"{DEFAULT_OUTPUT_DIR}/types.md": _render_types(model),
        f"{DEFAULT_OUTPUT_DIR}/settings.md": _render_settings(model),
        f"{DEFAULT_OUTPUT_DIR}/migrations.md": _render_migrations(model),
    }
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("Generated API reference page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def render_reference_pages(root: Path, model_relative_path: str = DEFAULT_MODEL) -> dict[str, str]:
    model_path = root / model_relative_path
    model = json.loads(model_path.read_text(encoding="utf-8"))
    if not isinstance(model, dict):
        raise ValueError("API model root must be an object")
    return generate_reference_pages(model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate Markdown reference pages from the FOnline API model")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--model", default=DEFAULT_MODEL)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write all generated Markdown reference pages")
    mode.add_argument("--check", action="store_true", help="fail when committed Markdown reference pages are stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        pages = render_reference_pages(root, args.model)
    except (OSError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate API reference pages: {exception}", file=sys.stderr)
        return 1

    if args.write:
        for relative_path, content in pages.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote {len(pages)} generated API reference pages")
        return 0

    stale_paths = []
    for relative_path, content in pages.items():
        output_path = root / relative_path
        if not output_path.is_file() or output_path.read_text(encoding="utf-8") != content:
            stale_paths.append(relative_path)
    if stale_paths:
        print(
            "Generated API reference pages are missing or stale: "
            + ", ".join(stale_paths)
            + "; run python BuildTools/docs_reference.py --write",
            file=sys.stderr,
        )
        return 1

    print(f"Generated API reference pages are current: {len(pages)} pages")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
