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
DEFAULT_MANIFEST = "BuildTools/PrototypeFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/prototype-format.json"
DEFAULT_OUTPUT_DIR = "Docs/generated/prototype-format"
GENERATED_BY = "BuildTools/docs_prototype_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-prototype-format-index", "Generated Prototype Format Reference"),
    ("syntax.md", "generated-prototype-format-syntax", "Prototype File Syntax"),
    ("properties.md", "generated-prototype-format-properties", "Built-in Prototype Properties"),
    ("validation.md", "generated-prototype-format-validation", "Prototype Validation Rules"),
)
OUTPUT_PATHS = tuple(f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS)
ENTRY_ID_PATTERN = re.compile(
    r"^prototype-format\.(section|directive|rule|entity|property)\.[A-Za-z0-9][A-Za-z0-9.-]*$"
)
SETTING_PATTERN = re.compile(
    r"FIXED_SETTING\(vector<string>,\s*Baking,\s*ProtoFileExtensions,\s*(.*?)\);"
)
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}
RUNTIME_SIDES = ("server", "client", "mapper")


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
    if not isinstance(raw, dict) or raw.get("surface") != "prototype-format":
        raise ValueError("scope.surface must be prototype-format")
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
        "source_parser",
        "config_parser",
        "property_parser",
        "property_serializator",
        "settings",
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


def _validate_file_selection(
    root: Path, raw: object, sources: dict[str, object]
) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("file_selection must be an object")
    result = copy.deepcopy(raw)
    if _required_string(result.get("setting"), "file_selection.setting") != "Baking.ProtoFileExtensions":
        raise ValueError("file_selection.setting must be Baking.ProtoFileExtensions")
    anchor = _required_string(result.get("setting_anchor"), "file_selection.setting_anchor")
    settings_text = (root / str(sources["settings"])).read_text(encoding="utf-8")
    if anchor not in settings_text:
        raise ValueError(f"file_selection.setting_anchor is missing from {sources['settings']}")
    match = SETTING_PATTERN.search(settings_text)
    if match is None:
        raise ValueError("unable to parse Baking.ProtoFileExtensions defaults")
    defaults = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', match.group(1))
    if not defaults:
        raise ValueError("Baking.ProtoFileExtensions must have at least one default")
    result["engine_defaults"] = defaults
    _required_string(result.get("output_pattern"), "file_selection.output_pattern")
    sides = _string_list(result.get("runtime_sides"), "file_selection.runtime_sides")
    if sides != list(RUNTIME_SIDES):
        raise ValueError(f"file_selection.runtime_sides must be {list(RUNTIME_SIDES)}")
    if result.get("nested_sections_skipped") is not True:
        raise ValueError("file_selection.nested_sections_skipped must be true")
    _required_string(result.get("description"), "file_selection.description")
    return result


def _validate_entries(root: Path, raw: object, collection: str) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError(f"{collection} must be a non-empty array")
    entries: list[dict[str, object]] = []
    identities: set[str] = set()
    expected_kind = collection.removesuffix("s").replace("section_form", "section")
    for index, entry in enumerate(raw):
        label = f"{collection}[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        entry_id = _required_string(entry.get("id"), f"{label}.id")
        if not ENTRY_ID_PATTERN.fullmatch(entry_id) or not entry_id.startswith(
            f"prototype-format.{expected_kind}."
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


def _prototype_entities(api_model: dict[str, object]) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for symbol in api_model["symbols"]:
        if symbol["kind"] != "entity" or not symbol["capabilities"]["prototypes"]:
            continue
        name = str(symbol["name"])
        result.append(
            {
                "id": f"prototype-format.entity.{name}",
                "name": name,
                "section": f"[Proto{name}]",
                "runtime_sides": list(symbol["runtime_sides"]),
                "capabilities": copy.deepcopy(symbol["capabilities"]),
                "stability": "internal",
                "source": copy.deepcopy(symbol["source"]),
            }
        )
    return sorted(result, key=lambda entry: str(entry["name"]))


def _prototype_properties(
    api_model: dict[str, object], entity_names: set[str]
) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for symbol in api_model["symbols"]:
        if symbol["kind"] != "property" or symbol["receiver"] not in entity_names:
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
                "id": f"prototype-format.property.{symbol['receiver']}.{symbol['name']}",
                "entity": symbol["receiver"],
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
    return sorted(result, key=lambda entry: (str(entry["entity"]), str(entry["name"])))


def generate_prototype_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read prototype format manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"prototype format manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    file_selection = _validate_file_selection(root, raw.get("file_selection"), sources)
    section_forms = _validate_entries(root, raw.get("section_forms"), "section_forms")
    directives = _validate_entries(root, raw.get("directives"), "directives")
    rules = _validate_entries(root, raw.get("rules"), "rules")

    api_model = docs_api.generate_api_model(root)
    entity_types = _prototype_entities(api_model)
    properties = _prototype_properties(api_model, {str(entry["name"]) for entry in entity_types})
    identities = [
        str(entry["id"])
        for entry in [*section_forms, *directives, *rules, *entity_types, *properties]
    ]
    if len(identities) != len(set(identities)):
        raise ValueError("prototype format model IDs must be unique")

    property_counts = Counter(str(entry["entity"]) for entry in properties)
    authorable_counts = Counter(
        str(entry["entity"]) for entry in properties if entry["authorable"]
    )
    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "source_parser": sources["source_parser"],
        "config_parser": sources["config_parser"],
        "property_parser": sources["property_parser"],
        "property_serializator": sources["property_serializator"],
        "api_model_generator": sources["api_model_generator"],
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": description,
        "scope": scope,
        "sources": sources,
        "file_selection": file_selection,
        "section_forms": section_forms,
        "directives": directives,
        "rules": rules,
        "entity_types": entity_types,
        "properties": properties,
        "summary": {
            "section_form_count": len(section_forms),
            "directive_count": len(directives),
            "rule_count": len(rules),
            "entity_type_count": len(entity_types),
            "property_count": len(properties),
            "authorable_property_count": sum(bool(entry["authorable"]) for entry in properties),
            "excluded_property_count": sum(not bool(entry["authorable"]) for entry in properties),
            "properties_by_entity": dict(sorted(property_counts.items())),
            "authorable_properties_by_entity": dict(sorted(authorable_counts.items())),
        },
    }
    contract_content = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(contract_content.encode("utf-8")).hexdigest()
    return model


def render_prototype_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return json.dumps(
        generate_prototype_format_model(root, manifest_relative_path),
        ensure_ascii=False,
        indent=2,
    ) + "\n"


def _source_link(model: dict[str, object], source: object) -> str:
    if not isinstance(source, dict) or not source.get("path"):
        return "-"
    path = str(source["path"])
    fragment = f"#L{source['line']}" if source.get("line") else ""
    url = (
        f"https://github.com/{model['repository']}/blob/{model['source_ref']}/"
        f"{quote(path)}{fragment}"
    )
    return f"[{path}]({url})"


def _path_link(model: dict[str, object], path: object) -> str:
    source = str(path)
    url = (
        f"https://github.com/{model['repository']}/blob/{model['source_ref']}/"
        f"{quote(source)}"
    )
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
        "`BuildTools/PrototypeFormatInterface.json` or the owning engine metadata, then run "
        "`python BuildTools/docs_prototype_format.py --write`.",
        "",
        "[Index](index.md) | [Syntax](syntax.md) | [Properties](properties.md) | "
        "[Validation](validation.md) | [Canonical JSON](../prototype-format.json) | "
        "[Authoring guide](../../PrototypeFormat.md)",
        "",
    ]


def _render_index(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    summary = model["summary"]
    lines.extend(
        [
            "This reference describes the engine-owned prototype grammar and the built-in metadata "
            "available to any embedding project at this engine revision.",
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
            (
                "[Syntax](syntax.md)",
                str(summary["section_form_count"] + summary["directive_count"]),
                "Discovery, sections, identity, and inheritance.",
            ),
            (
                "[Properties](properties.md)",
                str(summary["property_count"]),
                "Built-in HasProtos types and engine-owned property keys.",
            ),
            (
                "[Validation](validation.md)",
                str(summary["rule_count"]),
                "Source-backed bake and migration requirements.",
            ),
        ],
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.extend(
        [
            "",
            "Embedding projects must generate or document their additional entity declarations, "
            "`FixedType` metadata, properties, extensions, and content IDs separately.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_syntax(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    selection = model["file_selection"]
    defaults = ", ".join(docs_cli._code(item) for item in selection["engine_defaults"])
    sides = ", ".join(docs_cli._code(item) for item in selection["runtime_sides"])
    lines.extend(
        [
            f"`{selection['setting']}` selects input files. Engine defaults: {defaults}. "
            "An embedding project may add extensions without changing how sections are parsed.",
            "",
            f"Each pack emits `{selection['output_pattern']}` for {sides}. Every top-level "
            "`[ProtoMap]` anchor contributes a Map prototype; nested map-placement sections are skipped.",
            "",
            "## Section forms",
            "",
        ]
    )
    rows = []
    for entry in model["section_forms"]:
        entry_id = str(entry["id"])
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", entry_id)}"></a>'
                f'<code>{docs_cli._text(entry_id)}</code>',
                docs_cli._code(entry["syntax"]),
                docs_cli._text(entry["resolves_to"]),
                docs_cli._text(entry["description"]),
            )
        )
    docs_cli._table(lines, ("Stable ID", "Syntax", "Resolution", "Meaning"), rows)
    lines.extend(["## Control directives", ""])
    rows = []
    for entry in model["directives"]:
        entry_id = str(entry["id"])
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", entry_id)}"></a>'
                f'<code>{docs_cli._text(entry_id)}</code>',
                docs_cli._code(entry["syntax"]),
                docs_cli._text(entry["default"]),
                docs_cli._text(entry["description"]),
            )
        )
    docs_cli._table(lines, ("Stable ID", "Syntax", "Default", "Meaning"), rows)
    lines.extend(
        [
            "## Minimal example",
            "",
            "```ini",
            "[ProtoItem]",
            "$Name = BaseItem",
            "",
            "[ProtoItem]",
            "$Name = DerivedItem",
            "$Parent = BaseItem",
            "```",
            "",
            "The section selects the type. The extension and directory do not. Values use the shared "
            "configuration parser, including `#` comments, backslash continuation, and "
            "`key += value` append syntax.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_properties(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    summary = model["summary"]
    lines.extend(
        [
            f"The engine declares {summary['entity_type_count']} built-in `HasProtos` entity "
            f"types and {summary['property_count']} properties for them. "
            f"{summary['authorable_property_count']} properties can be loaded from prototype text "
            "at this revision.",
            "",
            "A property marked `no` is virtual or temporary and fails when authored on a "
            "side where it exists. A server-only key is skipped in client/mapper output, and a "
            "client-only key is skipped in server output.",
            "",
            "## Built-in entity types",
            "",
        ]
    )
    entity_rows = []
    for entry in model["entity_types"]:
        entry_id = str(entry["id"])
        entity_rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", entry_id)}"></a>'
                f'<code>{docs_cli._text(entry_id)}</code>',
                docs_cli._code(entry["name"]),
                docs_cli._code(entry["section"]),
                ", ".join(docs_cli._code(side) for side in entry["runtime_sides"]),
                str(summary["authorable_properties_by_entity"].get(entry["name"], 0)),
                _source_link(model, entry["source"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Type", "Section", "Sides", "Authorable properties", "Source"),
        entity_rows,
    )
    for entity in model["entity_types"]:
        entity_name = str(entity["name"])
        lines.extend([f"## `{entity_name}` properties", ""])
        rows = []
        for entry in model["properties"]:
            if entry["entity"] != entity_name:
                continue
            entry_id = str(entry["id"])
            authored = "yes" if entry["authorable"] else f"no ({entry['excluded_reason']})"
            rows.append(
                (
                    f'<a id="{docs_cli._anchor("entry", entry_id)}"></a>'
                    f'<code>{docs_cli._text(entry["name"])}</code>',
                    docs_cli._code(entry["type"]),
                    authored,
                    ", ".join(docs_cli._code(side) for side in entry["runtime_sides"]),
                    ", ".join(docs_cli._code(flag) for flag in entry["flags"]) or "-",
                    _source_link(model, entry["source"]),
                )
            )
        docs_cli._table(
            lines,
            ("Property", "Type", "Authorable", "Sides", "Flags", "Source"),
            rows,
        )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    lines.extend(
        [
            "These rules are enforced by the parser, metadata registrators, property serializer, or "
            "the side-specific prototype bake. Stable IDs let CI track contract changes.",
            "",
        ]
    )
    rows = []
    for entry in model["rules"]:
        entry_id = str(entry["id"])
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", entry_id)}"></a>'
                f'<code>{docs_cli._text(entry_id)}</code>',
                docs_cli._text(entry["name"]),
                docs_cli._text(entry["requirement"]),
                docs_cli._text(entry["description"]),
                _path_link(model, entry["source"]["path"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Notes", "Authority"),
        rows,
    )
    lines.extend(
        [
            "## Current diagnostic limitation",
            "",
            "Parent graphs must be acyclic. The current baker recursively expands parents without a "
            "dedicated cycle diagnostic, so embedding projects should validate cycles before baking "
            "and keep inheritance chains shallow. This is a documented implementation limitation, "
            "not a supported cyclic behavior.",
            "",
        ]
    )
    return "\n".join(lines)


def generate_reference_pages(model: dict[str, object]) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("unsupported generated prototype format model")
    identities = [
        entry.get("id")
        for key in ("section_forms", "directives", "rules", "entity_types", "properties")
        for entry in model.get(key, [])
        if isinstance(entry, dict)
    ]
    if (
        any(not isinstance(identity, str) or not identity for identity in identities)
        or len(identities) != len(set(identities))
    ):
        raise ValueError("every prototype format entry must have a unique non-empty ID")
    pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/syntax.md": _render_syntax(model),
        f"{DEFAULT_OUTPUT_DIR}/properties.md": _render_properties(model),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated prototype format page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline prototype format model and reference"
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_prototype_format_model(root, args.manifest)
        model = json.loads(model_content)
        pages = generate_reference_pages(model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate prototype format documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote prototype format model and {len(pages)} reference pages")
        return 0

    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file() or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated prototype format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_prototype_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated prototype format documentation is current: "
        f"{summary['entity_type_count']} entity types, "
        f"{summary['authorable_property_count']} authorable properties, "
        f"{summary['rule_count']} rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
