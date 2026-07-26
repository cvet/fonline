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

import docs_cli


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/ImageFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/image-format.json"
DEFAULT_OUTPUT_DIR = "Docs/generated/image-format"
GENERATED_BY = "BuildTools/docs_image_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-image-format-index", "Generated Image Format Reference"),
    ("formats.md", "generated-image-format-formats", "Image Source Formats"),
    ("fofrm.md", "generated-image-format-fofrm", "FOFRM Descriptor Reference"),
    ("options.md", "generated-image-format-options", "Legacy Image Filename Options"),
    ("baking.md", "generated-image-format-baking", "Image Baking Contract"),
    ("runtime.md", "generated-image-format-runtime", "Sprite Runtime Contract"),
    ("validation.md", "generated-image-format-validation", "Image Format Validation"),
)
OUTPUT_PATHS = tuple(
    f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
COLLECTION_KINDS = {
    "formats": "format",
    "descriptor_fields": "field",
    "filename_options": "option",
    "baking_rules": "baking",
    "runtime_rules": "runtime",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^image-format\."
    r"(format|field|option|baking|runtime|validation)\."
    r"[A-Za-z0-9][A-Za-z0-9.-]*$"
)
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}


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


def _source_refs(root: Path, value: object, label: str) -> list[dict[str, object]]:
    if not isinstance(value, list) or not value:
        raise ValueError(f"{label} must be a non-empty array")
    refs: list[dict[str, object]] = []
    for index, raw in enumerate(value):
        item_label = f"{label}[{index}]"
        if not isinstance(raw, dict):
            raise ValueError(f"{item_label} must be an object")
        path = _source_path(root, raw.get("path"), f"{item_label}.path")
        anchors = _string_list(raw.get("anchors"), f"{item_label}.anchors")
        source_text = (root / path).read_text(encoding="utf-8")
        for anchor in anchors:
            if anchor not in source_text:
                raise ValueError(f"{item_label} anchor is missing from {path}: {anchor}")
        refs.append({"path": path, "anchors": anchors})
    return refs


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "image-format":
        raise ValueError("scope.surface must be image-format")
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
    fields = (
        "image_baker",
        "image_baker_header",
        "default_sprites",
        "default_sprites_header",
        "sprite_manager",
        "sprite_manager_header",
        "texture_atlas",
        "texture_atlas_header",
        "sprite_resource",
        "sprite_resource_header",
        "sprite_meshing",
        "sprite_meshing_header",
        "string_utils_header",
    )
    result = {
        field: _source_path(root, raw.get(field), f"sources.{field}")
        for field in fields
    }
    tests = _string_list(raw.get("tests"), "sources.tests")
    result["tests"] = [
        _source_path(root, path, f"sources.tests[{index}]")
        for index, path in enumerate(tests)
    ]
    return result


def _quoted_values(text: str) -> list[str]:
    return re.findall(r'"([A-Za-z0-9_]+)"', text)


def _derive_baker_extensions(source_text: str) -> list[str]:
    extensions: list[str] = []
    for line in source_text.splitlines():
        if "AddLoader(" in line:
            extensions.extend(_quoted_values(line.rsplit(",", maxsplit=1)[-1]))
    if not extensions:
        raise ValueError("unable to derive ImageBaker extensions")
    return extensions


def _derive_runtime_extensions(header_text: str) -> list[str]:
    match = re.search(
        r"GetExtensions\(\) const -> vector<string> override \{ return \{([^}]*)\}; \}",
        header_text,
    )
    if match is None:
        raise ValueError("unable to derive DefaultSpriteFactory extensions")
    extensions = _quoted_values(match.group(1))
    if not extensions:
        raise ValueError("DefaultSpriteFactory extension list is empty")
    return extensions


def _derive_baker_identity(header_text: str) -> tuple[str, int]:
    name = re.search(r'NAME = "([^"]+)"', header_text)
    order = re.search(r"GetOrder\(\) const -> int32_t override \{ return (\d+); \}", header_text)
    if name is None or order is None:
        raise ValueError("unable to derive ImageBaker name/order")
    return name.group(1), int(order.group(1))


def _derive_sprite_resource_identity(header_text: str) -> tuple[int, int]:
    magic = re.search(r"SPRITE_RESOURCE_MAGIC\s*=\s*(\d+)", header_text)
    version = re.search(r"SPRITE_RESOURCE_VERSION\s*=\s*(\d+)", header_text)
    if magic is None or version is None:
        raise ValueError("unable to derive sprite resource magic/version")
    return int(magic.group(1)), int(version.group(1))


def _validate_outputs(
    raw: object,
    baker_extensions: list[str],
    runtime_extensions: list[str],
    baker_name: str,
    baker_order: int,
    container_magic: int,
    container_version: int,
) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    result = copy.deepcopy(raw)
    expected = {
        "baker_name": baker_name,
        "baker_order": baker_order,
        "accepted_extensions": baker_extensions,
        "default_runtime_extensions": runtime_extensions,
        "default_runtime_unsupported": sorted(set(baker_extensions) - set(runtime_extensions)),
        "container_magic": container_magic,
        "container_version": container_version,
        "pixel_format": "RGBA8",
        "direction_counts": ["1", "GameSettings::MAP_DIR_COUNT"],
        "runtime_side": "client",
        "source_option_separator": "$",
        "fofrm_effect_serialized": False,
    }
    for field, value in expected.items():
        if result.get(field) != value:
            raise ValueError(f"outputs.{field} must be {value}")
    return result


def _validate_entry_id(
    value: object, label: str, kind: str, identities: set[str]
) -> str:
    entry_id = _required_string(value, f"{label}.id")
    if not ENTRY_ID_PATTERN.fullmatch(entry_id) or not entry_id.startswith(
        f"image-format.{kind}."
    ):
        raise ValueError(f"invalid {kind} id: {entry_id}")
    if entry_id in identities:
        raise ValueError(f"duplicate image format entry id: {entry_id}")
    identities.add(entry_id)
    return entry_id


def _validate_entries(
    root: Path, collection: str, raw: object, identities: set[str]
) -> list[dict[str, object]]:
    kind = COLLECTION_KINDS[collection]
    if not isinstance(raw, list) or not raw:
        raise ValueError(f"{collection} must be a non-empty array")
    result: list[dict[str, object]] = []
    for index, entry in enumerate(raw):
        label = f"{collection}[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        _validate_entry_id(entry.get("id"), label, kind, identities)
        _required_string(entry.get("name"), f"{label}.name")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        _required_string(entry.get("requirement"), f"{label}.requirement")
        _required_string(entry.get("rationale"), f"{label}.rationale")
        for field in ("syntax", "default", "availability"):
            if field in entry:
                _required_string(entry.get(field), f"{label}.{field}")
        for field in ("values", "notes"):
            if field in entry:
                _string_list(entry.get(field), f"{label}.{field}", allow_empty=True)
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        result.append(enriched)
    return result


def generate_image_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read image format manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"image format manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    baker_source = (root / str(sources["image_baker"])).read_text(encoding="utf-8")
    baker_header = (root / str(sources["image_baker_header"])).read_text(encoding="utf-8")
    runtime_header = (root / str(sources["default_sprites_header"])).read_text(encoding="utf-8")
    sprite_resource_header = (root / str(sources["sprite_resource_header"])).read_text(encoding="utf-8")
    baker_extensions = _derive_baker_extensions(baker_source)
    runtime_extensions = _derive_runtime_extensions(runtime_header)
    baker_name, baker_order = _derive_baker_identity(baker_header)
    container_magic, container_version = _derive_sprite_resource_identity(sprite_resource_header)
    bake_collection = baker_source.split("auto ImageBaker::BakeCollection", maxsplit=1)[1].split(
        "auto ImageBaker::LoadAny", maxsplit=1
    )[0]
    if "EffectName" in bake_collection:
        raise ValueError("FOFRM EffectName is now serialized; update the image contract")
    outputs = _validate_outputs(
        raw.get("outputs"),
        baker_extensions,
        runtime_extensions,
        baker_name,
        baker_order,
        container_magic,
        container_version,
    )
    identities: set[str] = set()
    collections = {
        collection: _validate_entries(root, collection, raw.get(collection), identities)
        for collection in COLLECTION_KINDS
    }
    format_extensions = [str(entry.get("extension")) for entry in collections["formats"]]
    if format_extensions != baker_extensions:
        raise ValueError(
            "formats extensions must match the live ImageBaker loader order: "
            + ", ".join(baker_extensions)
        )
    stability_counts = Counter(
        str(entry["stability"])
        for entries in collections.values()
        for entry in entries
    )
    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": description,
        "scope": scope,
        "sources": sources,
        "outputs": outputs,
        **collections,
        "summary": {
            "entry_count": sum(len(entries) for entries in collections.values()),
            **{
                f"{collection.removesuffix('s')}_count": len(entries)
                for collection, entries in collections.items()
            },
            "baker_extension_count": len(baker_extensions),
            "runtime_extension_count": len(runtime_extensions),
            "entries_by_stability": dict(sorted(stability_counts.items())),
        },
    }
    canonical = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
    return model


def render_image_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return json.dumps(
        generate_image_format_model(root, manifest_relative_path),
        ensure_ascii=False,
        indent=2,
    ) + "\n"


def _source_link(model: dict[str, object], source: str) -> str:
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(source)}"
    return f"[{source}]({url})"


def _source_ref_links(model: dict[str, object], refs: list[dict[str, object]]) -> str:
    return ", ".join(
        _source_link(model, str(ref["path"]))
        for ref in refs
        if isinstance(ref, dict)
    )


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
        "`BuildTools/ImageFormatInterface.json`, then run "
        "`python BuildTools/docs_image_format.py --write`.",
        "",
        "[Index](index.md) | [Formats](formats.md) | [FOFRM](fofrm.md) | "
        "[Options](options.md) | [Baking](baking.md) | [Runtime](runtime.md) | "
        "[Validation](validation.md) | [Canonical JSON](../image-format.json) | "
        "[Guide](../../ImageFormat.md)",
        "",
    ]


def _entry_anchor(entry: dict[str, object]) -> str:
    return (
        f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
        f'<code>{docs_cli._text(entry["id"])}</code>'
    )


def _entry_rows(
    model: dict[str, object], collection: str
) -> list[tuple[str, str, str, str, str]]:
    return [
        (
            _entry_anchor(entry),
            docs_cli._text(entry["name"]),
            docs_cli._text(entry["requirement"]),
            docs_cli._text(entry["rationale"]),
            _source_ref_links(model, entry["source"]),
        )
        for entry in model[collection]
        if isinstance(entry, dict)
    ]


def _render_index(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    outputs = model["outputs"]
    summary = model["summary"]
    assert isinstance(scope, dict) and isinstance(outputs, dict) and isinstance(summary, dict)
    lines.extend([
        "This reference describes the Engine-owned image import, FOFRM composition, "
        "baked sprite, client loading, atlas, cache, and validation contract. Project "
        "asset catalogs and visual acceptance remain project-owned.",
        "",
        "## Contract status",
        "",
    ])
    docs_cli._table(lines, ("Field", "Value"), [
        ("Stability", docs_cli._code(scope["stability"])),
        ("Support policy", docs_cli._text(scope["support_note"])),
        ("Source manifest", _source_link(model, str(model["source_manifest"]))),
        ("Contract digest", docs_cli._code(model["contract_digest"])),
        ("Baker", f"{docs_cli._code(outputs['baker_name'])}, order {outputs['baker_order']}"),
        ("Baked pixels", docs_cli._code(outputs["pixel_format"])),
        ("Runtime side", docs_cli._code(outputs["runtime_side"])),
    ])
    docs_cli._table(lines, ("Reference", "Entries", "Purpose"), [
        ("[Formats](formats.md)", str(summary["format_count"]), "Accepted source formats and their current import behavior."),
        ("[FOFRM](fofrm.md)", str(summary["descriptor_field_count"]), "Descriptor fields, aliases, directions, flattening, and timing."),
        ("[Options](options.md)", str(summary["filename_option_count"]), "ART, SPR, and BAM filename selectors."),
        ("[Baking](baking.md)", str(summary["baking_rule_count"]), "Discovery, output naming, container records, and failures."),
        ("[Runtime](runtime.md)", str(summary["runtime_rule_count"]), "Factory coverage, sprite sheets, atlas upload, and caches."),
        ("[Validation](validation.md)", str(summary["validation_rule_count"]), "Source constraints and executable checks."),
    ])
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_formats(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    rows = []
    for entry in model["formats"]:
        assert isinstance(entry, dict)
        rows.append((
            _entry_anchor(entry),
            docs_cli._code(entry["extension"]),
            docs_cli._text(entry.get("availability", "baker and default runtime")),
            docs_cli._text(entry["requirement"]),
            _source_ref_links(model, entry["source"]),
        ))
    docs_cli._table(lines, ("Stable ID", "Extension", "Availability", "Import contract", "Source"), rows)
    return "\n".join(lines)


def _render_fofrm(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    lines.extend([
        "FOFRM is the authored composition layer for static images, animations, "
        "direction sheets, and imported legacy sources. References are relative to "
        "the descriptor directory.",
        "",
    ])
    docs_cli._table(lines, ("Stable ID", "Field/rule", "Requirement", "Why", "Source"), _entry_rows(model, "descriptor_fields"))
    return "\n".join(lines)


def _render_options(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    rows = []
    for entry in model["filename_options"]:
        assert isinstance(entry, dict)
        rows.append((
            _entry_anchor(entry),
            docs_cli._code(entry["syntax"]),
            docs_cli._text(entry["requirement"]),
            docs_cli._text(entry["rationale"]),
            _source_ref_links(model, entry["source"]),
        ))
    docs_cli._table(lines, ("Stable ID", "Syntax", "Behavior", "Why", "Source"), rows)
    return "\n".join(lines)


def _render_rules(model: dict[str, object], page_index: int, collection: str) -> str:
    lines = _header(*PAGE_DEFINITIONS[page_index][1:])
    docs_cli._table(lines, ("Stable ID", "Rule", "Requirement", "Why", "Source"), _entry_rows(model, collection))
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[6][1:])
    docs_cli._table(lines, ("Stable ID", "Rule", "Requirement", "Why", "Source"), _entry_rows(model, "validation_rules"))
    lines.extend([
        "## Validation commands",
        "",
        "```powershell",
        "python BuildTools\\docs_image_format.py --check",
        "python -m unittest BuildTools.tests.test_docs_image_format",
        "cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests",
        "```",
        "",
        "An embedding project must also rebake affected resources and inspect every "
        "changed animation, direction, alpha edge, hit mask, and supported client profile.",
        "",
    ])
    return "\n".join(lines)


def generate_reference_pages(model: dict[str, object]) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("unsupported generated image format model")
    identities = [
        entry.get("id")
        for collection in COLLECTION_KINDS
        for entry in model.get(collection, [])
        if isinstance(entry, dict)
    ]
    if any(not isinstance(identity, str) or not identity for identity in identities) or len(identities) != len(set(identities)):
        raise ValueError("every image format entry must have a unique non-empty ID")
    pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/formats.md": _render_formats(model),
        f"{DEFAULT_OUTPUT_DIR}/fofrm.md": _render_fofrm(model),
        f"{DEFAULT_OUTPUT_DIR}/options.md": _render_options(model),
        f"{DEFAULT_OUTPUT_DIR}/baking.md": _render_rules(model, 4, "baking_rules"),
        f"{DEFAULT_OUTPUT_DIR}/runtime.md": _render_rules(model, 5, "runtime_rules"),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated image format page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline image format model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_image_format_model(root, args.manifest)
        model = json.loads(model_content)
        pages = generate_reference_pages(model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate image format documentation: {exception}", file=sys.stderr)
        return 1
    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote image format model and {len(pages)} reference pages")
        return 0
    stale = [
        path for path, content in outputs.items()
        if not (root / path).is_file() or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated image format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_image_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated image format documentation is current: "
        f"{summary['entry_count']} entries, {summary['format_count']} source formats, "
        f"{summary['runtime_extension_count']} default runtime extensions"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
