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
import docs_description_translations
import docs_localization


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/FontFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/font-format.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/font-format"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/font-format"
LEGACY_OUTPUT_DIR = "Docs/generated/font-format"
GENERATED_BY = "BuildTools/docs_font_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-font-format-index", "Generated Font Format Reference"),
    ("formats.md", "generated-font-format-formats", "Font Resource Formats"),
    ("fofnt.md", "generated-font-format-fofnt", "FOFNT Field Reference"),
    ("bmfont.md", "generated-font-format-bmfont", "Binary BMFont Contract"),
    ("binding.md", "generated-font-format-binding", "Font Binding Contract"),
    ("layout.md", "generated-font-format-layout", "Text Layout Contract"),
    ("rendering.md", "generated-font-format-rendering", "Font Rendering Contract"),
    ("validation.md", "generated-font-format-validation", "Font Validation Contract"),
)
CANONICAL_OUTPUT_PATHS = tuple(
    f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
RUSSIAN_OUTPUT_PATHS = tuple(
    f"{RUSSIAN_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
LEGACY_OUTPUT_PATHS = tuple(
    f"{LEGACY_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
OUTPUT_PATHS = (*CANONICAL_OUTPUT_PATHS, *RUSSIAN_OUTPUT_PATHS, *LEGACY_OUTPUT_PATHS)
COLLECTION_KINDS = {
    "formats": "format",
    "fofnt_fields": "fofnt",
    "bmfont_rules": "bmfont",
    "binding_rules": "binding",
    "layout_rules": "layout",
    "rendering_rules": "rendering",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^font-format\."
    r"(format|fofnt|bmfont|binding|layout|rendering|validation)\."
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


def _relative_path(value: object, label: str) -> tuple[str, PurePosixPath]:
    source = _required_string(value, label)
    relative = PurePosixPath(source)
    if "\\" in source or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    return source, relative


def _source_path(root: Path, value: object, label: str) -> str:
    source, relative = _relative_path(value, label)
    if not root.joinpath(*relative.parts).is_file():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _source_directory(root: Path, value: object, label: str) -> str:
    source, relative = _relative_path(value, label)
    if not root.joinpath(*relative.parts).is_dir():
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
        source_text = (root / path).read_text(encoding="utf-8", errors="replace")
        for anchor in anchors:
            if anchor not in source_text:
                raise ValueError(f"{item_label} anchor is missing from {path}: {anchor}")
        refs.append({"path": path, "anchors": anchors})
    return refs


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "font-format":
        raise ValueError("scope.surface must be font-format")
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
    file_fields = (
        "font_manager",
        "font_manager_header",
        "client_global_scripts",
        "settings",
        "raw_copy_baker",
        "updater",
        "file_reader_header",
    )
    result: dict[str, object] = {
        field: _source_path(root, raw.get(field), f"sources.{field}")
        for field in file_fields
    }
    result["font_resources"] = _source_directory(
        root, raw.get("font_resources"), "sources.font_resources"
    )
    tests = _string_list(raw.get("tests"), "sources.tests")
    result["tests"] = [
        _source_path(root, path, f"sources.tests[{index}]")
        for index, path in enumerate(tests)
    ]
    return result


def _quoted_values(text: str) -> list[str]:
    return re.findall(r'"([A-Za-z0-9_]+)"', text)


def _derive_raw_copy_extensions(settings_text: str) -> list[str]:
    match = re.search(
        r"FIXED_SETTING\(vector<string>,\s*Baking,\s*RawCopyFileExtensions,"
        r"(?P<values>.*?)\);",
        settings_text,
        re.DOTALL,
    )
    if match is None:
        raise ValueError("unable to derive Baking.RawCopyFileExtensions")
    values = _quoted_values(match.group("values"))
    if not values:
        raise ValueError("Baking.RawCopyFileExtensions is empty")
    return values


def _derive_enum(header_text: str, name: str) -> list[dict[str, object]]:
    match = re.search(
        rf"enum class {re.escape(name)}\s*:[^{{]+\{{(?P<body>.*?)\}};",
        header_text,
        re.DOTALL,
    )
    if match is None:
        raise ValueError(f"unable to derive {name}")
    entries: list[dict[str, object]] = []
    for raw_line in match.group("body").splitlines():
        line_match = re.match(
            r"\s*([A-Za-z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+),?\s*(?://\s*(.*))?$",
            raw_line,
        )
        if line_match is None:
            continue
        entries.append(
            {
                "name": line_match.group(1),
                "value": int(line_match.group(2), 0),
                "description": (line_match.group(3) or "No additional behavior").strip(),
            }
        )
    if not entries:
        raise ValueError(f"{name} has no derivable entries")
    return entries


def _derive_fofnt_keys(manager_text: str) -> list[str]:
    body = manager_text.split("void FontManager::BindFoFont", maxsplit=1)[1].split(
        "void FontManager::BindBmfFont", maxsplit=1
    )[0]
    keys: list[str] = []
    for key in re.findall(r'key\s*(?:==|!=)\s*"([A-Za-z0-9]+)"', body):
        if key not in keys:
            keys.append(key)
    if not keys or keys[0] != "Version":
        raise ValueError("unable to derive ordered FOFNT keys")
    return keys


def _derive_runtime_extensions(script_text: str) -> list[str]:
    body = script_text.split("FO_SCRIPT_API void Client_Game_BindFont", maxsplit=1)[1].split(
        "///@ ExportMethod", maxsplit=1
    )[0]
    values = re.findall(r'fontFname\.ends_with\("\.([A-Za-z0-9]+)"\)', body)
    if not values:
        raise ValueError("unable to derive Game.BindFont extension dispatch")
    return values


def _derive_bmfont_contract(manager_text: str) -> dict[str, object]:
    body = manager_text.split("void FontManager::BindBmfFont", maxsplit=1)[1].split(
        "void FontManager::FormatText", maxsplit=1
    )[0]
    signature = re.search(
        r"make_signature\('B',\s*'M',\s*'F',\s*([0-9]+)\)", body
    )
    padding = re.search(r"GetBEUInt32\(\)\s*!=\s*(0x[0-9A-Fa-f]+)u", body)
    page_count = re.search(r"GetLEUInt16\(\)\s*!=\s*([0-9]+)", body)
    record_size = re.search(r"GetLEUInt32\(\)\s*/\s*([0-9]+)", body)
    if None in (signature, padding, page_count, record_size):
        raise ValueError("unable to derive binary BMFont constants")
    signed_fields = []
    unsigned_fields = []
    for variable, field_name in (("ox", "xoffset"), ("oy", "yoffset"), ("xa", "xadvance")):
        if re.search(rf"int16_t {variable}\s*=\s*reader\.GetLEInt16\(\);", body):
            signed_fields.append(field_name)
        elif re.search(rf"uint16_t {variable}\s*=\s*reader\.GetLEUInt16\(\);", body):
            unsigned_fields.append(field_name)
        else:
            raise ValueError(f"unable to derive the BMFont {field_name} integer decoding")
    return {
        "signature": "BMF",
        "version": int(signature.group(1)),
        "padding_word": padding.group(1).lower(),
        "page_count": int(page_count.group(1)),
        "char_record_size": int(record_size.group(1)),
        "signed_fields": signed_fields,
        "unsigned_fields": unsigned_fields,
    }


def _derive_outputs(root: Path, sources: dict[str, object]) -> dict[str, object]:
    manager_text = (root / str(sources["font_manager"])).read_text(encoding="utf-8")
    header_text = (root / str(sources["font_manager_header"])).read_text(encoding="utf-8")
    script_text = (root / str(sources["client_global_scripts"])).read_text(encoding="utf-8")
    settings_text = (root / str(sources["settings"])).read_text(encoding="utf-8")
    updater_text = (root / str(sources["updater"])).read_text(encoding="utf-8")

    max_version_match = re.search(r"if \(version > ([0-9]+)\)", manager_text)
    scale_match = re.search(
        r"std::isfinite\(scale\) && scale > ([-0-9.]+)f && scale <= ([-0-9.]+)f",
        manager_text,
    )
    cache_match = re.search(
        r"CACHE_INVALIDATION_FRAME_COUNT\s*=\s*([0-9]+)", manager_text
    )
    scale_default_match = re.search(
        r"Client_Game_BindFont\([^\n]+defaultScale\s*=\s*([-0-9.]+)f", script_text
    )
    atlas_matches = re.findall(
        r"Bind(?:FoFont|BmfFont)\([^;]+AtlasType::([A-Za-z0-9_]+)", script_text
    )
    updater_match = re.search(r'BindFoFont\([^,]+,\s*"([^"]+)"', updater_text)
    inline_match = re.search(r'InlineColorTagPrefix\s*=\s*"([^"]+)"', header_text)
    if None in (
        max_version_match,
        scale_match,
        cache_match,
        scale_default_match,
        updater_match,
        inline_match,
    ):
        raise ValueError("unable to derive the complete font runtime contract")
    if not atlas_matches or len(set(atlas_matches)) != 1:
        raise ValueError("font extension dispatch must use one atlas type")

    font_resource_root = root / str(sources["font_resources"])
    bundled = {
        extension: sorted(path.name for path in font_resource_root.glob(f"*.{extension}"))
        for extension in ("fofnt", "fnt", "bmfc")
    }
    if not bundled["fofnt"] or not bundled["fnt"]:
        raise ValueError("bundled font resources must exercise both runtime formats")

    return {
        "runtime_extensions": _derive_runtime_extensions(script_text),
        "authoring_sidecar_extensions": ["bmfc"],
        "raw_copy_extensions": _derive_raw_copy_extensions(settings_text),
        "raw_copy_passthrough": "_context->WriteData(file.GetPath(), file.GetData())" in (root / str(sources["raw_copy_baker"])).read_text(encoding="utf-8"),
        "fofnt_max_version": int(max_version_match.group(1)),
        "fofnt_keys": _derive_fofnt_keys(manager_text),
        "bmfont": _derive_bmfont_contract(manager_text),
        "font_slots": _derive_enum(header_text, "FontType"),
        "font_flags": _derive_enum(header_text, "FontFlag"),
        "default_scale": float(scale_default_match.group(1)),
        "scale_range": {
            "minimum_exclusive": float(scale_match.group(1)),
            "maximum_inclusive": float(scale_match.group(2)),
        },
        "atlas_type": atlas_matches[0],
        "cache_invalidation_frames": int(cache_match.group(1)),
        "inline_color_prefix": inline_match.group(1),
        "updater_default_font": updater_match.group(1),
        "runtime_side": "client",
        "bundled_descriptors": bundled,
    }


def _validate_entry_id(value: object, label: str, kind: str, identities: set[str]) -> str:
    identity = _required_string(value, f"{label}.id")
    if ENTRY_ID_PATTERN.fullmatch(identity) is None or not identity.startswith(
        f"font-format.{kind}."
    ):
        raise ValueError(f"invalid {label}.id: {identity}")
    if identity in identities:
        raise ValueError(f"duplicate font format entry id: {identity}")
    identities.add(identity)
    return identity


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
        for field in ("extension", "role", "syntax", "default", "availability"):
            if field in entry:
                _required_string(entry.get(field), f"{label}.{field}")
        for field in ("values", "notes"):
            if field in entry:
                _string_list(entry.get(field), f"{label}.{field}", allow_empty=True)
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        result.append(enriched)
    return result


def _validate_outputs(raw: object, expected: dict[str, object]) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    result = copy.deepcopy(raw)
    for field, value in expected.items():
        if result.get(field) != value:
            raise ValueError(f"outputs.{field} must match the live source: {value}")
    return result


def generate_font_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read font format manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"font format manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    outputs = _validate_outputs(raw.get("outputs"), _derive_outputs(root, sources))
    identities: set[str] = set()
    collections = {
        collection: _validate_entries(root, collection, raw.get(collection), identities)
        for collection in COLLECTION_KINDS
    }
    stability_counts = Counter(
        str(entry["stability"])
        for entries in collections.values()
        for entry in entries
    )
    collection_counts = {
        f"{collection.removesuffix('s')}_count": len(entries)
        for collection, entries in collections.items()
    }
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
            **collection_counts,
            "entries_by_stability": dict(sorted(stability_counts.items())),
        },
    }
    canonical = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
    return model


def render_font_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return json.dumps(
        generate_font_format_model(root, manifest_relative_path),
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
        "`BuildTools/FontFormatInterface.json`, then run "
        "`python BuildTools/docs_font_format.py --write`.",
        "",
        "[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | "
        "[BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | "
        "[Rendering](rendering.md) | [Validation](validation.md) | "
        "[Canonical JSON](../../../generated/font-format.json) | "
        "[Guide](../../how-to/content/font-format.md)",
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
    bmfont = outputs["bmfont"]
    assert isinstance(bmfont, dict)
    lines.extend(
        [
            "This reference describes Engine-owned font descriptors, client binding, "
            "text layout, rendering, scaling, and validation. Project font selection "
            "and typography policy remain outside this contract.",
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
            ("Source manifest", _source_link(model, str(model["source_manifest"]))),
            ("Contract digest", docs_cli._code(model["contract_digest"])),
            ("Runtime descriptors", ", ".join(docs_cli._code(f".{item}") for item in outputs["runtime_extensions"])),
            ("FOFNT maximum version", docs_cli._code(outputs["fofnt_max_version"])),
            ("BMFont binary version", docs_cli._code(bmfont["version"])),
            ("Client atlas", docs_cli._code(outputs["atlas_type"])),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            ("[Formats](formats.md)", str(summary["format_count"]), "Descriptor roles and supported resource suffixes."),
            ("[FOFNT](fofnt.md)", str(summary["fofnt_field_count"]), "Text descriptor keys and glyph metrics."),
            ("[BMFont](bmfont.md)", str(summary["bmfont_rule_count"]), "Accepted binary-v3 blocks and metric transformations."),
            ("[Binding](binding.md)", str(summary["binding_rule_count"]), "Raw-copy, slot, atlas, scale, and startup behavior."),
            ("[Layout](layout.md)", str(summary["layout_rule_count"]), "TextFormat, wrapping, alignment, measurement, and glyph fallback."),
            ("[Rendering](rendering.md)", str(summary["rendering_rule_count"]), "Texture preparation, borders, effects, color tags, and cache."),
            ("[Validation](validation.md)", str(summary["validation_rule_count"]), "Failure modes and executable validation gates."),
        ],
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_rules(model: dict[str, object], page_index: int, collection: str) -> str:
    lines = _header(*PAGE_DEFINITIONS[page_index][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, collection),
    )
    return "\n".join(lines)


def _render_formats(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    rows = []
    for entry in model["formats"]:
        assert isinstance(entry, dict)
        rows.append(
            (
                _entry_anchor(entry),
                docs_cli._code(entry["extension"]),
                docs_cli._text(entry["role"]),
                docs_cli._text(entry["requirement"]),
                _source_ref_links(model, entry["source"]),
            )
        )
    docs_cli._table(lines, ("Stable ID", "Suffix", "Role", "Contract", "Source"), rows)
    outputs = model["outputs"]
    assert isinstance(outputs, dict)
    lines.extend(["## Bundled descriptors", ""])
    docs_cli._table(
        lines,
        ("Suffix", "Files"),
        [
            (docs_cli._code(f".{extension}"), ", ".join(docs_cli._code(name) for name in names))
            for extension, names in outputs["bundled_descriptors"].items()
        ],
    )
    return "\n".join(lines)


def _render_fofnt(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    rows = []
    for entry in model["fofnt_fields"]:
        assert isinstance(entry, dict)
        rows.append(
            (
                _entry_anchor(entry),
                docs_cli._text(entry["name"]),
                docs_cli._code(entry.get("syntax", "See requirement")),
                docs_cli._text(entry["requirement"]),
                _source_ref_links(model, entry["source"]),
            )
        )
    docs_cli._table(lines, ("Stable ID", "Key", "Syntax", "Behavior", "Source"), rows)
    return "\n".join(lines)


def _render_layout(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[5][1:])
    outputs = model["outputs"]
    assert isinstance(outputs, dict)
    lines.extend(["## FontFlag values", ""])
    docs_cli._table(
        lines,
        ("Name", "Value", "Source behavior"),
        [
            (
                docs_cli._code(flag["name"]),
                docs_cli._code(f"0x{flag['value']:04X}"),
                docs_cli._text(flag["description"]),
            )
            for flag in outputs["font_flags"]
        ],
    )
    lines.extend(["## Layout rules", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, "layout_rules"),
    )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[7][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, "validation_rules"),
    )
    lines.extend(
        [
            "## Validation commands",
            "",
            "```powershell",
            "python BuildTools\\docs_font_format.py --check",
            "python -m unittest BuildTools.tests.test_docs_font_format",
            "cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests",
            "```",
            "",
            "An embedding project must also bake the descriptor and image together, "
            "run its text-measurement regression, and inspect representative regular, "
            "bordered, scaled, wrapped, and localized text in a visible client.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_legacy_page(
    canonical_path: str,
    title: str,
    canonical_content: str,
) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/font-format/{filename}"
    russian_path = f"../../ru/reference/font-format/{filename}"
    lines = [
        f"# {title}",
        "",
        "> Legacy route.",
        "",
        "The canonical generated reference moved to locale-specific paths.",
        "",
        f"[English]({english_path}) | [Russian]({russian_path})",
        "",
    ]
    for line in canonical_content.splitlines():
        heading = re.fullmatch(r"(#{2,3}) (.+)", line)
        if heading:
            lines.extend(
                [
                    f"{heading.group(1)} {heading.group(2)}",
                    "",
                    f"Continue with the [canonical reference]({english_path}).",
                    "",
                ]
            )
        for anchor in re.findall(r'<a id="([^"]+)"></a>', line):
            lines.extend(
                [
                    f'<a id="{anchor}"></a>',
                    f"- [`{anchor}`]({english_path}#{anchor})",
                    "",
                ]
            )
    return "\n".join(lines).rstrip() + "\n"


RUSSIAN_TITLES = {
    "Generated Font Format Reference": "Сгенерированный справочник форматов шрифтов",
    "Font Resource Formats": "Форматы ресурсов шрифтов",
    "FOFNT Field Reference": "Справочник полей FOFNT",
    "Binary BMFont Contract": "Контракт бинарного BMFont",
    "Font Binding Contract": "Контракт привязки шрифтов",
    "Text Layout Contract": "Контракт компоновки текста",
    "Font Rendering Contract": "Контракт отрисовки шрифтов",
    "Font Validation Contract": "Контракт проверки шрифтов",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.":
        "> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.",
    "[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)":
        "[Индекс](index.md) | [Форматы](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Привязка](binding.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)",
    "This reference describes Engine-owned font descriptors, client binding, text layout, rendering, scaling, and validation. Project font selection and typography policy remain outside this contract.":
        "Этот справочник описывает принадлежащие Engine дескрипторы шрифтов, клиентскую привязку, компоновку и отрисовку текста, масштабирование и проверку. Выбор шрифтов и типографическая политика проекта остаются вне контракта.",
    "## Contract status": "## Состояние контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный манифест",
    "Contract digest": "Дайджест контракта",
    "Runtime descriptors": "Runtime-дескрипторы",
    "FOFNT maximum version": "Максимальная версия FOFNT",
    "BMFont binary version": "Версия binary BMFont",
    "Client atlas": "Клиентский атлас",
    "Descriptor roles and supported resource suffixes.":
        "Роли дескрипторов и поддерживаемые суффиксы ресурсов.",
    "Text descriptor keys and glyph metrics.":
        "Ключи текстового дескриптора и метрики глифов.",
    "Accepted binary-v3 blocks and metric transformations.":
        "Принимаемые блоки binary v3 и преобразования метрик.",
    "Raw-copy, slot, atlas, scale, and startup behavior.":
        "Raw-copy, слоты, атлас, масштаб и поведение при запуске.",
    "TextFormat, wrapping, alignment, measurement, and glyph fallback.":
        "TextFormat, перенос, выравнивание, измерение и fallback глифов.",
    "Texture preparation, borders, effects, color tags, and cache.":
        "Подготовка текстуры, рамки, эффекты, теги цвета и кеш.",
    "Failure modes and executable validation gates.":
        "Режимы отказа и исполняемые gate проверки.",
    "## Boundary": "## Граница ответственности",
    "Included:": "Включено:",
    "Excluded:": "Исключено:",
    "## Bundled descriptors": "## Поставляемые дескрипторы",
    "See requirement": "См. требование",
    "## FontFlag values": "## Значения FontFlag",
    "## Layout rules": "## Правила компоновки",
    "## Validation commands": "## Команды проверки",
    "An embedding project must also bake the descriptor and image together, run its text-measurement regression, and inspect representative regular, bordered, scaled, wrapped, and localized text in a visible client.":
        "Подключаемый проект также должен совместно запечь дескриптор и изображение, запустить регрессию измерения текста и проверить в видимом клиенте репрезентативный обычный, обведённый, масштабированный, перенесённый и локализованный текст.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Suffix | Role | Contract | Source |":
        "| Стабильный ID | Суффикс | Роль | Контракт | Источник |",
    "| Suffix | Files |": "| Суффикс | Файлы |",
    "| Stable ID | Key | Syntax | Behavior | Source |":
        "| Стабильный ID | Ключ | Синтаксис | Поведение | Источник |",
    "| Stable ID | Rule | Requirement | Why | Source |":
        "| Стабильный ID | Правило | Требование | Причина | Источник |",
    "| Name | Value | Source behavior |":
        "| Имя | Значение | Поведение исходного кода |",
}


def _render_russian_page(
    document_id: str,
    canonical_path: str,
    english_content: str,
    russian_base_content: str,
) -> str:
    content = russian_base_content.replace("locale: en", "locale: ru", 1)
    for english, russian in sorted(
        {**RUSSIAN_TITLES, **RUSSIAN_REPLACEMENTS}.items(),
        key=lambda item: -len(item[0]),
    ):
        content = content.replace(english, russian)
    for english, russian in RUSSIAN_TABLE_HEADERS.items():
        content = content.replace(english, russian)
    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError("generated font format page has no front matter")
    insert_at = front_matter_end + len("\n---\n")
    marker = docs_localization.translation_metadata_line(
        document_id,
        canonical_path,
        docs_localization.normalized_sha256(english_content),
    )
    return content[:insert_at] + "\n" + marker + "\n" + content[insert_at:]


def generate_reference_pages(
    model: dict[str, object],
    russian_model: dict[str, object] | None = None,
) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("unsupported generated font format model")
    identities = [
        entry.get("id")
        for collection in COLLECTION_KINDS
        for entry in model.get(collection, [])
        if isinstance(entry, dict)
    ]
    if any(not isinstance(identity, str) or not identity for identity in identities) or len(identities) != len(set(identities)):
        raise ValueError("every font format entry must have a unique non-empty ID")
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/formats.md": _render_formats(model),
        f"{DEFAULT_OUTPUT_DIR}/fofnt.md": _render_fofnt(model),
        f"{DEFAULT_OUTPUT_DIR}/bmfont.md": _render_rules(model, 3, "bmfont_rules"),
        f"{DEFAULT_OUTPUT_DIR}/binding.md": _render_rules(model, 4, "binding_rules"),
        f"{DEFAULT_OUTPUT_DIR}/layout.md": _render_layout(model),
        f"{DEFAULT_OUTPUT_DIR}/rendering.md": _render_rules(model, 6, "rendering_rules"),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/formats.md": _render_formats(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/fofnt.md": _render_fofnt(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/bmfont.md": _render_rules(
            localized_model, 3, "bmfont_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/binding.md": _render_rules(
            localized_model, 4, "binding_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/layout.md": _render_layout(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/rendering.md": _render_rules(
            localized_model, 6, "rendering_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(localized_model),
    }
    pages = dict(canonical_pages)
    for filename, document_id, title in PAGE_DEFINITIONS:
        canonical_path = f"{DEFAULT_OUTPUT_DIR}/{filename}"
        russian_path = f"{RUSSIAN_OUTPUT_DIR}/{filename}"
        legacy_path = f"{LEGACY_OUTPUT_DIR}/{filename}"
        pages[russian_path] = _render_russian_page(
            document_id,
            canonical_path,
            canonical_pages[canonical_path],
            russian_base_pages[canonical_path],
        )
        pages[legacy_path] = _render_legacy_page(
            canonical_path,
            title,
            canonical_pages[canonical_path],
        )
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated font format page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def render_reference_pages(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    model = generate_font_format_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "font-format",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline font format model and reference"
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_font_format_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "font-format",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate font format documentation: {exception}", file=sys.stderr)
        return 1
    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote font format model and {len(pages)} reference pages")
        return 0
    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file()
        or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated font format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_font_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated font format documentation is current: "
        f"{summary['entry_count']} entries, {summary['fofnt_field_count']} FOFNT fields, "
        f"{summary['bmfont_rule_count']} BMFont rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
