from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from urllib.parse import quote

import docs_api
import docs_cli


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/MapFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/map-format.json"
DEFAULT_OUTPUT_DIR = "Docs/generated/map-format"
GENERATED_BY = "BuildTools/docs_map_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-map-format-index", "Generated Map Format Reference"),
    ("syntax.md", "generated-map-format-syntax", "Map File Syntax"),
    ("properties.md", "generated-map-format-properties", "Map Placement Properties"),
    ("baking.md", "generated-map-format-baking", "Map Baking And Runtime Loading"),
    ("validation.md", "generated-map-format-validation", "Map Validation Rules"),
)
OUTPUT_PATHS = tuple(f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS)
ENTRY_ID_PATTERN = re.compile(
    r"^map-format\.(section|directive|ownership|rule|property)\.[A-Za-z0-9][A-Za-z0-9.-]*$"
)
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}
RUNTIME_SIDES = ("server", "client", "mapper")
PROPERTY_RECEIVERS = ("Map", "Critter", "Item")


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(value: object, label: str, *, allow_empty: bool = False) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        qualifier = "an" if allow_empty else "a non-empty"
        raise ValueError(f"{label} must be {qualifier} array of strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _source_path(root: Path, value: object, label: str) -> str:
    source = _required_string(value, label)
    relative = PurePosixPath(source)
    if "\\" in source or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    if not root.joinpath(*relative.parts).is_file():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "map-format":
        raise ValueError("scope.surface must be map-format")
    stability = _required_string(raw.get("stability"), "scope.stability")
    if stability not in VALID_STABILITY:
        raise ValueError(f"unsupported scope.stability: {stability}")
    since = raw.get("since")
    if since is not None and (not isinstance(since, str) or not since.strip()):
        raise ValueError("scope.since must be null or a non-empty string")
    _required_string(raw.get("support_note"), "scope.support_note")
    _string_list(raw.get("included"), "scope.included")
    _string_list(raw.get("excluded"), "scope.excluded")
    return copy.deepcopy(raw)


def _validate_sources(root: Path, raw: object) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("sources must be an object")
    result: dict[str, object] = {}
    for field in (
        "source_loader",
        "config_parser",
        "source_baker",
        "prototype_baker",
        "text_baker",
        "source_mapper",
        "map_serializer",
        "server_loader",
        "client_loader",
        "api_model_generator",
        "api_model",
    ):
        result[field] = _source_path(root, raw.get(field), f"sources.{field}")
    tests = _string_list(raw.get("tests"), "sources.tests")
    result["tests"] = [
        _source_path(root, source, f"sources.tests[{index}]")
        for index, source in enumerate(tests)
    ]
    return result


def _validate_outputs(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    result = copy.deepcopy(raw)
    for field in ("source_selection", "conventional_extension", "server_pattern", "client_pattern", "description"):
        _required_string(result.get(field), f"outputs.{field}")
    sides = _string_list(result.get("runtime_sides"), "outputs.runtime_sides")
    if sides != list(RUNTIME_SIDES):
        raise ValueError(f"outputs.runtime_sides must be {list(RUNTIME_SIDES)}")
    return result


def _validate_entries(root: Path, raw: object, collection: str) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError(f"{collection} must be a non-empty array")
    expected_kind = collection.removesuffix("s")
    entries: list[dict[str, object]] = []
    identities: set[str] = set()
    for index, entry in enumerate(raw):
        label = f"{collection}[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        entry_id = _required_string(entry.get("id"), f"{label}.id")
        if not ENTRY_ID_PATTERN.fullmatch(entry_id) or not entry_id.startswith(
            f"map-format.{expected_kind}."
        ):
            raise ValueError(f"invalid {collection} id: {entry_id}")
        if entry_id in identities:
            raise ValueError(f"duplicate {collection} id: {entry_id}")
        identities.add(entry_id)
        _required_string(entry.get("name"), f"{label}.name")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        _required_string(entry.get("description"), f"{label}.description")
        source = entry.get("source")
        if not isinstance(source, dict):
            raise ValueError(f"{label}.source must be an object")
        source_path = _source_path(root, source.get("path"), f"{label}.source.path")
        anchors = _string_list(source.get("anchors"), f"{label}.source.anchors")
        source_text = (root / source_path).read_text(encoding="utf-8")
        for anchor in anchors:
            if anchor not in source_text:
                raise ValueError(f"{label} source anchor is missing from {source_path}: {anchor}")
        entries.append(copy.deepcopy(entry))
    return entries


def _map_properties(api_model: dict[str, object]) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for symbol in api_model["symbols"]:
        if symbol["kind"] != "property" or symbol["receiver"] not in PROPERTY_RECEIVERS:
            continue
        flags = list(symbol["flags"])
        is_virtual = "Virtual" in flags
        is_temporary = (
            symbol["mutability"] == "mutable" or "CoreProperty" in flags
        ) and not symbol["persistent"]
        excluded_reason = "virtual" if is_virtual else "temporary" if is_temporary else None
        runtime_sides = list(symbol["runtime_sides"])
        result.append(
            {
                "id": f"map-format.property.{symbol['receiver']}.{symbol['name']}",
                "receiver": symbol["receiver"],
                "name": symbol["name"],
                "type": symbol["type"],
                "nullable": symbol["nullable"],
                "runtime_sides": runtime_sides,
                "skipped_sides": [side for side in RUNTIME_SIDES if side not in runtime_sides],
                "authorable": excluded_reason is None,
                "excluded_reason": excluded_reason,
                "mutability": symbol["mutability"],
                "persistent": symbol["persistent"],
                "flags": flags,
                "stability": "internal",
                "description": symbol["description"],
                "source": copy.deepcopy(symbol["source"]),
            }
        )
    return sorted(result, key=lambda entry: (str(entry["receiver"]), str(entry["name"])))


def _attach_ownership_values(
    ownerships: list[dict[str, object]], api_model: dict[str, object]
) -> list[dict[str, object]]:
    enum_values = {
        str(symbol["name"]): symbol
        for symbol in api_model["symbols"]
        if symbol["kind"] == "enum-value" and symbol.get("receiver") == "ItemOwnership"
    }
    expected = {str(entry["name"]) for entry in ownerships}
    if enum_values and set(enum_values) != expected:
        raise ValueError(
            "ItemOwnership enum values do not match the map format manifest: "
            f"api={sorted(enum_values)}, manifest={sorted(expected)}"
        )
    result: list[dict[str, object]] = []
    for entry in ownerships:
        declared_value = entry.get("value")
        if not isinstance(declared_value, int):
            raise ValueError(f"ownership {entry['name']} must declare an integer value")
        enriched = copy.deepcopy(entry)
        if enum_values:
            value = enum_values[str(entry["name"])]
            if value["evaluated_value"] != declared_value:
                raise ValueError(
                    f"ItemOwnership.{entry['name']} value is {value['evaluated_value']}, "
                    f"manifest declares {declared_value}"
                )
            enriched["enum_source"] = copy.deepcopy(value["source"])
        else:
            enriched["enum_source"] = copy.deepcopy(entry["source"])
        result.append(enriched)
    if len({int(entry["value"]) for entry in result}) != len(result):
        raise ValueError("map ownership values must be unique")
    return sorted(result, key=lambda entry: int(entry["value"]))


def generate_map_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read map format manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"map format manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    outputs = _validate_outputs(raw.get("outputs"))
    sections = _validate_entries(root, raw.get("sections"), "sections")
    directives = _validate_entries(root, raw.get("directives"), "directives")
    ownerships = _validate_entries(root, raw.get("ownerships"), "ownerships")
    rules = _validate_entries(root, raw.get("rules"), "rules")

    api_model = docs_api.generate_api_model(root)
    ownerships = _attach_ownership_values(ownerships, api_model)
    properties = _map_properties(api_model)
    identities = [
        str(entry["id"])
        for entry in [*sections, *directives, *ownerships, *rules, *properties]
    ]
    if len(identities) != len(set(identities)):
        raise ValueError("map format model IDs must be unique")

    property_counts = Counter(str(entry["receiver"]) for entry in properties)
    authorable_counts = Counter(
        str(entry["receiver"]) for entry in properties if entry["authorable"]
    )
    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "source_loader": sources["source_loader"],
        "config_parser": sources["config_parser"],
        "source_baker": sources["source_baker"],
        "source_mapper": sources["source_mapper"],
        "api_model_generator": sources["api_model_generator"],
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": description,
        "scope": scope,
        "sources": sources,
        "outputs": outputs,
        "sections": sections,
        "directives": directives,
        "ownerships": ownerships,
        "rules": rules,
        "properties": properties,
        "summary": {
            "section_count": len(sections),
            "directive_count": len(directives),
            "ownership_count": len(ownerships),
            "supported_ownership_count": sum(bool(entry["supported"]) for entry in ownerships),
            "rule_count": len(rules),
            "property_count": len(properties),
            "authorable_property_count": sum(bool(entry["authorable"]) for entry in properties),
            "excluded_property_count": sum(not bool(entry["authorable"]) for entry in properties),
            "properties_by_receiver": dict(sorted(property_counts.items())),
            "authorable_properties_by_receiver": dict(sorted(authorable_counts.items())),
        },
    }
    contract_content = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(contract_content.encode("utf-8")).hexdigest()
    return model


def render_map_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return json.dumps(
        generate_map_format_model(root, manifest_relative_path),
        ensure_ascii=False,
        indent=2,
    ) + "\n"


def _source_link(model: dict[str, object], source: object) -> str:
    if not isinstance(source, dict) or not source.get("path"):
        return "-"
    path = str(source["path"])
    fragment = f"#L{source['line']}" if source.get("line") else ""
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(path)}{fragment}"
    return f"[{path}]({url})"


def _path_link(model: dict[str, object], path: object) -> str:
    source = str(path)
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(source)}"
    return f"[{source}]({url})"


def _header(document_id: str, title: str) -> list[str]:
    return [
        "---",
        f"title: {title}",
        f"document_id: {document_id}",
        "locale: en",
        "generated: true",
        "---",
        "",
        f"# {title}",
        "",
        "> Generated reference. Do not edit directly. Update "
        "`BuildTools/MapFormatInterface.json` or the owning engine metadata, then run "
        "`python BuildTools/docs_map_format.py --write`.",
        "",
        "[Index](index.md) | [Syntax](syntax.md) | [Properties](properties.md) | "
        "[Baking](baking.md) | [Validation](validation.md) | "
        "[Canonical JSON](../map-format.json) | [Authoring guide](../../MapFormat.md)",
        "",
    ]


def _render_index(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    summary = model["summary"]
    lines.extend(
        [
            "This reference describes the reusable engine contract for authored `.fomap` files, "
            "their side-specific bake products, and initial runtime materialization.",
            "",
            "## Contract status",
            "",
        ]
    )
    docs_cli._table(
        lines,
        ("Field", "Value"),
        [
            ("Stability", docs_cli._code(scope["stability"])),
            ("Support policy", docs_cli._text(scope["support_note"])),
            ("Source manifest", _path_link(model, model["source_manifest"])),
            ("Contract digest", docs_cli._code(model["contract_digest"])),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            ("[Syntax](syntax.md)", str(summary["section_count"] + summary["directive_count"]), "Sections and control directives."),
            ("[Properties](properties.md)", str(summary["property_count"]), "Engine-owned Map, Critter, and Item properties."),
            ("[Baking](baking.md)", str(summary["ownership_count"]), "Ownership and server/client materialization."),
            ("[Validation](validation.md)", str(summary["rule_count"]), "Source-backed requirements and limitations."),
        ],
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.extend(["", "Embedding projects must document their added metadata, prototypes, map catalog, and level-design conventions separately.", ""])
    return "\n".join(lines)


def _render_syntax(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    lines.extend(
        [
            "A map container is a configured prototype file with one or more `[ProtoMap]` anchors. "
            "Each anchor owns nested `[$Name/Critter]` and `[$Name/Item]` sections; an explicit map id may replace `$Name`.",
            "",
            "## Section forms",
            "",
        ]
    )
    rows = []
    for entry in model["sections"]:
        entry_id = str(entry["id"])
        rows.append((f'<a id="{docs_cli._anchor("entry", entry_id)}"></a><code>{docs_cli._text(entry_id)}</code>', docs_cli._code(entry["syntax"]), docs_cli._code(entry["receiver"]), docs_cli._text(entry["cardinality"]), docs_cli._text(entry["description"])))
    docs_cli._table(lines, ("Stable ID", "Syntax", "Receiver", "Cardinality", "Meaning"), rows)
    lines.extend(["## Control directives", ""])
    rows = []
    for entry in model["directives"]:
        entry_id = str(entry["id"])
        rows.append((f'<a id="{docs_cli._anchor("entry", entry_id)}"></a><code>{docs_cli._text(entry["name"])}</code>', docs_cli._text(entry["applies_to"]), docs_cli._code(entry["syntax"]), "yes" if entry["required"] else "no", docs_cli._text(entry["default"]), docs_cli._text(entry["description"])))
    docs_cli._table(lines, ("Directive", "Sections", "Syntax", "Required", "Default", "Meaning"), rows)
    lines.extend(
        [
            "## Minimal map",
            "",
            "```ini",
            "[ProtoMap]",
            "$Name = SmallRoom",
            "Size = 80 80",
            "WorkHex = 40 40",
            "",
            "[$Name/Critter]",
            "$Id = 1",
            "$Proto = Guard",
            "Hex = 38 40",
            "Dir = 3",
            "",
            "[$Name/Item]",
            "$Id = 2",
            "$Proto = MetalDoor",
            "Hex = 42 40",
            "```",
            "",
            "Within one selected map, placement order is not semantic: the loader processes all critters, then all items. Keep explicit unique ids for stable ownership references and reviewable diffs.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_properties(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    summary = model["summary"]
    lines.extend(
        [
            f"The current engine metadata exposes {summary['property_count']} properties across `Map`, `Critter`, and `Item`; {summary['authorable_property_count']} can be loaded from authored map text at this revision.",
            "",
            "A `no` entry is virtual or temporary. Side-specific properties are serialized only where their metadata permits them. Project-defined properties are additional to this generated catalog.",
            "",
        ]
    )
    for receiver in PROPERTY_RECEIVERS:
        count = summary["properties_by_receiver"].get(receiver, 0)
        authorable = summary["authorable_properties_by_receiver"].get(receiver, 0)
        lines.extend([f"## `{receiver}` properties", "", f"{authorable} of {count} properties are authorable in map text at this revision.", ""])
        rows = []
        for entry in model["properties"]:
            if entry["receiver"] != receiver:
                continue
            entry_id = str(entry["id"])
            authored = "yes" if entry["authorable"] else f"no ({entry['excluded_reason']})"
            rows.append((f'<a id="{docs_cli._anchor("entry", entry_id)}"></a><code>{docs_cli._text(entry["name"])}</code>', docs_cli._code(entry["type"]), authored, ", ".join(docs_cli._code(side) for side in entry["runtime_sides"]), ", ".join(docs_cli._code(flag) for flag in entry["flags"]) or "-", _source_link(model, entry["source"])))
        docs_cli._table(lines, ("Property", "Type", "Authorable", "Sides", "Flags", "Source"), rows)
    return "\n".join(lines)


def _render_baking(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    outputs = model["outputs"]
    lines.extend(
        [
            f"Map containers are selected by `{outputs['source_selection']}`; `{outputs['conventional_extension']}` is a project convention, not an engine requirement. Each declared map emits `{outputs['server_pattern']}` and `{outputs['client_pattern']}` as a coupled resource pair.",
            "",
            "The server payload contains placed critters and all items. The client payload contains visible static items; hidden static items contribute required string hashes but no client item record.",
            "",
            "## Item ownership",
            "",
        ]
    )
    rows = []
    for entry in model["ownerships"]:
        entry_id = str(entry["id"])
        rows.append((f'<a id="{docs_cli._anchor("entry", entry_id)}"></a><code>{docs_cli._text(entry["name"])}</code>', str(entry["value"]), "yes" if entry["supported"] else "no", docs_cli._code(entry["owner_property"]), docs_cli._text(entry["description"]), _source_link(model, entry["enum_source"])))
    docs_cli._table(lines, ("Ownership", "Value", "Map-supported", "Reference/position", "Meaning", "Enum source"), rows)
    lines.extend(
        [
            "## Runtime split",
            "",
            "- Static `MapHex` items become immutable grid entries and may block movement or shooting, expose triggers, and occupy multihex cells.",
            "- Non-static `MapHex` items and placed critters are generated for each map instance; their authored ids are remapped to runtime ids.",
            "- `CritterInventory` and `ItemContainer` records are attached after their direct owners are generated. A missing owner mapping skips the child.",
            "- The client reconstructs only the visible static map layer from the client binary; dynamic entities arrive through normal runtime synchronization.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[4][1:])
    lines.extend(["These stable rule IDs let documentation CI classify format changes against an earlier engine revision.", ""])
    rows = []
    for entry in model["rules"]:
        entry_id = str(entry["id"])
        rows.append((f'<a id="{docs_cli._anchor("entry", entry_id)}"></a><code>{docs_cli._text(entry_id)}</code>', docs_cli._text(entry["name"]), docs_cli._text(entry["requirement"]), docs_cli._text(entry["description"]), _path_link(model, entry["source"]["path"])))
    docs_cli._table(lines, ("Stable ID", "Rule", "Requirement", "Notes", "Authority"), rows)
    lines.extend(
        [
            "## Authoring validation sequence",
            "",
            "1. Declare every map with `[ProtoMap]`; name every anchor in a multi-map container.",
            "2. Address placements with `[$Name/Critter]` / `[$Name/Item]` after their anchor or use an explicit declared map id.",
            "3. Assign explicit unique positive placement ids within each map and resolve every `$Proto` and ownership reference.",
            "4. Validate receiver properties, resources, map bounds, and static-item ownership.",
            "5. Bake both side outputs and treat warnings followed by an aggregate map error as a failed build.",
            "6. After mapper save, inspect the edited map's normalization and verify that sibling map blocks stayed unchanged.",
            "",
        ]
    )
    return "\n".join(lines)


def generate_reference_pages(model: dict[str, object]) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("unsupported generated map format model")
    identities = [
        entry.get("id")
        for key in ("sections", "directives", "ownerships", "rules", "properties")
        for entry in model.get(key, [])
        if isinstance(entry, dict)
    ]
    if any(not isinstance(identity, str) or not identity for identity in identities) or len(identities) != len(set(identities)):
        raise ValueError("every map format entry must have a unique non-empty ID")
    pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/syntax.md": _render_syntax(model),
        f"{DEFAULT_OUTPUT_DIR}/properties.md": _render_properties(model),
        f"{DEFAULT_OUTPUT_DIR}/baking.md": _render_baking(model),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated map format page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline map format model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_map_format_model(root, args.manifest)
        model = json.loads(model_content)
        pages = generate_reference_pages(model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate map format documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote map format model and {len(pages)} reference pages")
        return 0

    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file() or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated map format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_map_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated map format documentation is current: "
        f"{summary['section_count']} sections, {summary['directive_count']} directives, "
        f"{summary['ownership_count']} ownership modes, {summary['rule_count']} rules, "
        f"{summary['property_count']} properties"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
