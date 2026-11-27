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
    ("binding.md", "generated-font-format-binding", "Font Binding Contract"),
    ("authoring.md", "generated-font-format-authoring", "Font Authoring Contract"),
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
    "binding_rules": "binding",
    "authoring_rules": "authoring",
    "layout_rules": "layout",
    "rendering_rules": "rendering",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^font-format\."
    r"(format|binding|authoring|layout|rendering|validation)\."
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
        "font_rasterizer",
        "font_rasterizer_header",
        "client_global_scripts",
        "client",
        "mapper",
        "settings",
        "raw_copy_baker",
        "resource_usage",
        "updater",
        "resource_defaults",
        "freetype_options",
        "project_interface",
        "converter",
        "converter_tests",
    )
    directory_fields = ("font_resources", "embedded_font_resources")
    result: dict[str, object] = {
        field: _source_path(root, raw.get(field), f"sources.{field}")
        for field in file_fields
    }
    result.update(
        {
            field: _source_directory(root, raw.get(field), f"sources.{field}")
            for field in directory_fields
        }
    )
    tests = _string_list(raw.get("tests"), "sources.tests")
    result["tests"] = [
        _source_path(root, path, f"sources.tests[{index}]")
        for index, path in enumerate(tests)
    ]
    return result


def _quoted_values(text: str) -> list[str]:
    return re.findall(r'"([A-Za-z0-9_]+)"', text)


def _read_source(root: Path, sources: dict[str, object], field: str) -> str:
    return (root / str(sources[field])).read_text(encoding="utf-8")


def _derive_raw_copy_extensions(settings_text: str) -> list[str]:
    match = re.search(
        r"SETTING\(vector<string>,\s*Baking,\s*RawCopyFileExtensions,"
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


def _derive_script_bind_overloads(script_text: str) -> list[str]:
    overloads = re.findall(
        r"FO_SCRIPT_API void Client_Game_BindFont\(ptr<ClientEngine> client,\s*([^)]*)\)",
        script_text,
    )
    if not overloads:
        raise ValueError("unable to derive the Game.BindFont overloads")
    return [" ".join(overload.split()) for overload in overloads]


def _derive_default_font(root: Path, sources: dict[str, object]) -> dict[str, object]:
    manager_text = _read_source(root, sources, "font_manager")
    defaults_text = _read_source(root, sources, "resource_defaults")
    body_match = re.search(
        r"void FontManager::BindDefaultFont\(bool skip_if_loaded\)\n\{(?P<body>.*?)\n\}\n",
        manager_text,
        re.DOTALL,
    )
    path_match = re.search(
        r'inline constexpr string_view Default\s*(?:=|\{)\s*"([^"]+)"', defaults_text
    )
    size_match = re.search(
        r"inline constexpr int32_t DefaultPixelSize\s*(?:=|\{)\s*([0-9]+)", defaults_text
    )
    if body_match is None or path_match is None or size_match is None:
        raise ValueError("unable to derive the engine default font binding")
    body = body_match.group("body")
    hinting = re.search(r"options\.Hinting = FontHinting::([A-Za-z0-9_]+);", body)
    outline = re.search(r"options\.OutlineWidth = ([0-9]+);", body)
    bind = re.search(
        r"BindTrueTypeFont\(FontType::Default,\s*\{string \{FontDefaults::Default\}\},\s*"
        r"FontDefaults::DefaultPixelSize,\s*options,\s*\{([0-9,\s]+)\},\s*"
        r"AtlasType::([A-Za-z0-9_]+)\)",
        body,
    )
    if hinting is None or outline is None or bind is None or "if (skip_if_loaded &&" not in body:
        raise ValueError("unable to derive the engine default font options")
    path = path_match.group(1)
    packs = [
        pack
        for pack in ("Embedded", "Core")
        if (root / "Resources" / pack / path).is_file()
    ]
    if len(packs) != 1:
        raise ValueError(f"the default font must ship in exactly one pack: {path}")
    callers = [
        name
        for name, field, call in (
            ("Client", "client", "FontMngr.BindDefaultFont(true)"),
            ("Mapper", "mapper", "FontMngr.BindDefaultFont(true)"),
            ("Updater", "updater", "_fontMngr.BindDefaultFont(true)"),
        )
        if call in _read_source(root, sources, field)
    ]
    return {
        "path": path,
        "pixel_size": int(size_match.group(1)),
        "pack": packs[0],
        "hinting": hinting.group(1),
        "outline_width": int(outline.group(1)),
        "preload_ranges": [int(value) for value in re.findall(r"[0-9]+", bind.group(1))],
        "atlas_type": bind.group(2),
        "bound_by": callers,
    }


def _find_option_entries(node: object) -> list[dict[str, object]]:
    found: list[dict[str, object]] = []
    if isinstance(node, dict):
        if "name" in node and "default" in node:
            found.append(node)
        for value in node.values():
            found.extend(_find_option_entries(value))
    elif isinstance(node, list):
        for value in node:
            found.extend(_find_option_entries(value))
    return found


def _derive_embedded_capacity(root: Path, sources: dict[str, object]) -> int:
    interface = json.loads(_read_source(root, sources, "project_interface"))
    options = [
        option
        for option in _find_option_entries(interface)
        if option.get("name") == "FO_EMBEDDED_DATA_CAPACITY"
    ]
    if len(options) != 1:
        raise ValueError("unable to derive the FO_EMBEDDED_DATA_CAPACITY default")
    return int(str(options[0]["default"]))


def _derive_colored_faces(root: Path, sources: dict[str, object]) -> dict[str, object]:
    options_text = _read_source(root, sources, "freetype_options")
    rasterizer_text = _read_source(root, sources, "font_rasterizer")
    manager_text = _read_source(root, sources, "font_manager")
    block = re.search(
        r"if \(impl->Colored\) \{(?P<body>.*?)\n    \}",
        rasterizer_text,
        re.DOTALL,
    )
    grey = re.search(r"TRUETYPE_GLYPH_GREY\s*\{([0-9]+)\}", manager_text)
    if block is None or grey is None:
        raise ValueError("unable to derive colored face rendering")
    load_flags = re.search(r"impl->LoadFlags = ([A-Z_| ]+);", block.group("body"))
    render_mode = re.search(r"impl->RenderMode = (FT_RENDER_MODE_[A-Z_]+);", block.group("body"))
    if load_flags is None or render_mode is None:
        raise ValueError("unable to derive colored face load flags")
    return {
        "freetype_color_layers": re.search(
            r"^#define TT_CONFIG_OPTION_COLOR_LAYERS\b", options_text, re.MULTILINE
        )
        is not None,
        "load_flags": load_flags.group(1).strip(),
        "render_mode": render_mode.group(1),
        "neutral_glyph_grey": int(grey.group(1)),
    }


def _derive_truetype_limits(manager_text: str) -> dict[str, int]:
    names = {
        "max_pixel_size": "TRUETYPE_MAX_PIXEL_SIZE",
        "max_outline_width": "TRUETYPE_MAX_OUTLINE_WIDTH",
        "max_preload_letters": "TRUETYPE_MAX_PRELOAD_LETTERS",
        "sheet_max_side": "TRUETYPE_SHEET_MAX_SIDE",
    }
    limits: dict[str, int] = {}
    for key, name in names.items():
        match = re.search(
            rf"static constexpr int32_t {name}\s*\{{(0x[0-9A-Fa-f]+|[0-9]+)\}}", manager_text
        )
        if match is None:
            raise ValueError(f"unable to derive {name}")
        limits[key] = int(match.group(1), 0)
    return limits


def _sfnt_tables(path: Path) -> set[str]:
    data = path.read_bytes()
    if len(data) < 12 or data[:4] not in (b"\x00\x01\x00\x00", b"true"):
        raise ValueError(f"not a TrueType face: {path}")
    table_count = int.from_bytes(data[4:6], "big")
    return {
        data[12 + 16 * index : 16 + 16 * index].decode("ascii")
        for index in range(table_count)
    }


def _derive_shipped_faces(
    root: Path, sources: dict[str, object]
) -> tuple[dict[str, list[str]], list[str]]:
    directories = {
        "Embedded": root / str(sources["embedded_font_resources"]),
        "Core": root / str(sources["font_resources"]),
    }
    shipped: dict[str, list[str]] = {}
    colored: list[str] = []
    for pack, directory in directories.items():
        faces = sorted(directory.glob("*.ttf"))
        shipped[pack] = [face.name for face in faces]
        for face in faces:
            if {"COLR", "CPAL"} <= _sfnt_tables(face):
                colored.append(face.name)
        stray = sorted(
            path.name for path in directory.iterdir() if path.suffix.lower() != ".ttf"
        )
        if stray:
            raise ValueError(
                f"the shipped {pack} font directory must hold only TrueType faces: "
                + ", ".join(stray)
            )
    if not shipped["Embedded"]:
        raise ValueError("the Embedded pack must ship the engine default font")
    return shipped, sorted(colored)


def _derive_converter(root: Path, sources: dict[str, object]) -> dict[str, object]:
    text = _read_source(root, sources, "converter")
    pixel = re.search(r"^PIXEL = ([0-9]+)$", text, re.MULTILINE)
    grey = re.search(r"^NEUTRAL_GREY = \(([0-9]+), [0-9]+, [0-9]+\)$", text, re.MULTILINE)
    limit = re.search(r"native <= 0 or native > ([0-9]+)", text)
    reader = re.search(
        r'read_bmfont\(path\) if path\.suffix\.lower\(\) == "\.([a-z]+)" else read_fofnt\(path\)',
        text,
    )
    if pixel is None or grey is None or limit is None or reader is None:
        raise ValueError("unable to derive the bitmap-font converter constants")
    modes = [mode for mode in ("--check",) if f'add_argument("{mode}"' in text]
    imported = {
        match
        for match in re.findall(r"^(?:import|from) ([A-Za-z0-9_]+)", text, re.MULTILINE)
        if match not in sys.stdlib_module_names
    }
    shipped, colored = _derive_shipped_faces(root, sources)
    return {
        "units_per_pixel": int(pixel.group(1)),
        "native_size_range": [1, int(limit.group(1))],
        "neutral_grey": int(grey.group(1)),
        "input_extensions": ["fofnt", reader.group(1)],
        "modes": modes,
        "python_packages": sorted(imported, key=str.lower),
        "shipped_face_count": sum(len(names) for names in shipped.values()),
        "colored_faces": colored,
    }


def _derive_outputs(root: Path, sources: dict[str, object]) -> dict[str, object]:
    manager_text = _read_source(root, sources, "font_manager")
    header_text = _read_source(root, sources, "font_manager_header")
    script_text = _read_source(root, sources, "client_global_scripts")
    settings_text = _read_source(root, sources, "settings")
    rasterizer_header_text = _read_source(root, sources, "font_rasterizer_header")
    raw_copy_baker_text = _read_source(root, sources, "raw_copy_baker")

    cache_match = re.search(
        r"CACHE_INVALIDATION_FRAME_COUNT\s*(?:=|\{)\s*([0-9]+)", manager_text
    )
    inline_match = re.search(r'InlineColorTagPrefix\s*(?:=|\{)\s*"([^"]+)"', header_text)
    atlas_matches = re.findall(
        r"BindTrueTypeFont\([^;]+AtlasType::([A-Za-z0-9_]+)", script_text
    )
    if cache_match is None or inline_match is None:
        raise ValueError("unable to derive the complete font runtime contract")
    if not atlas_matches or len(set(atlas_matches)) != 1:
        raise ValueError("font binding must use one atlas type")
    if "auto IsColored() const noexcept -> bool" not in rasterizer_header_text:
        raise ValueError("the rasterizer must report colored faces")

    removed_markers = (
        "BindFoFont",
        "BindBmfFont",
        "BuildFont",
        "BakeFontScale",
        "ResolveFontScale",
        ".fofnt",
        '".fnt"',
    )
    bitmap_loaders_removed = not any(
        marker in text
        for marker in removed_markers
        for text in (manager_text, header_text, script_text)
    )

    return {
        "runtime_extensions": ["ttf"],
        "raw_copy_extensions": _derive_raw_copy_extensions(settings_text),
        "raw_copy_passthrough": "_context->WriteData(file.GetPath(), file.GetData())"
        in raw_copy_baker_text,
        "bitmap_loaders_removed": bitmap_loaders_removed,
        "script_bind_overloads": _derive_script_bind_overloads(script_text),
        "default_font": _derive_default_font(root, sources),
        "embedded_data_capacity_default": _derive_embedded_capacity(root, sources),
        "colored_faces": _derive_colored_faces(root, sources),
        "truetype_limits": _derive_truetype_limits(manager_text),
        "converter": _derive_converter(root, sources),
        "shipped_faces": _derive_shipped_faces(root, sources)[0],
        "font_slots": _derive_enum(header_text, "FontType"),
        "font_flags": _derive_enum(header_text, "FontFlag"),
        "atlas_type": atlas_matches[0],
        "cache_invalidation_frames": int(cache_match.group(1)),
        "inline_color_prefix": inline_match.group(1),
        "runtime_side": "client",
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
        "[Index](index.md) | [Formats](formats.md) | [Binding](binding.md) | "
        "[Authoring](authoring.md) | [Layout](layout.md) | "
        "[Rendering](rendering.md) | [Validation](validation.md) | "
        "[Canonical JSON](../../../generated/font-format.json) | "
        "[Guide](../../how-to/content/font-format.md)",
        "",
    ]


def _page_header(filename: str) -> list[str]:
    for page_filename, document_id, title in PAGE_DEFINITIONS:
        if page_filename == filename:
            return _header(document_id, title)
    raise ValueError(f"unknown font format page: {filename}")


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
    lines = _page_header("index.md")
    scope = model["scope"]
    outputs = model["outputs"]
    summary = model["summary"]
    assert isinstance(scope, dict) and isinstance(outputs, dict) and isinstance(summary, dict)
    default_font = outputs["default_font"]
    colored = outputs["colored_faces"]
    assert isinstance(default_font, dict) and isinstance(colored, dict)
    lines.extend(
        [
            "This reference describes the Engine-owned TrueType text pipeline: face "
            "binding, the built-in default font, colored faces, the bitmap-font "
            "converter, text layout, rendering, and validation. Project font selection "
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
            ("Runtime faces", ", ".join(docs_cli._code(f".{item}") for item in outputs["runtime_extensions"])),
            (
                "Default font",
                f"{docs_cli._code(default_font['path'])}, "
                f"{docs_cli._code(default_font['pixel_size'])} px, "
                f"{docs_cli._code(default_font['pack'])}",
            ),
            ("Embedded data capacity default", docs_cli._code(outputs["embedded_data_capacity_default"])),
            ("Colored face load flags", docs_cli._code(colored["load_flags"])),
            ("Client atlas", docs_cli._code(outputs["atlas_type"])),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            ("[Formats](formats.md)", str(summary["format_count"]), "Runtime face formats, converter sources, and shipped faces."),
            ("[Binding](binding.md)", str(summary["binding_rule_count"]), "Face binding, the default font, slots, atlas, and Embedded capacity."),
            ("[Authoring](authoring.md)", str(summary["authoring_rule_count"]), "The bitmap-font converter, its output, and its staleness check."),
            ("[Layout](layout.md)", str(summary["layout_rule_count"]), "TextFormat, wrapping, alignment, measurement, and glyph fallback."),
            ("[Rendering](rendering.md)", str(summary["rendering_rule_count"]), "Glyph cells, colors, outlines, effects, color tags, and cache."),
            ("[Validation](validation.md)", str(summary["validation_rule_count"]), "Failure modes and executable validation gates."),
        ],
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_rules(model: dict[str, object], filename: str, collection: str) -> str:
    lines = _page_header(filename)
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, collection),
    )
    return "\n".join(lines)


def _render_formats(model: dict[str, object]) -> str:
    lines = _page_header("formats.md")
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
    converter = outputs["converter"]
    assert isinstance(converter, dict)
    lines.extend(["## Shipped faces", ""])
    docs_cli._table(
        lines,
        ("Pack", "Faces"),
        [
            (docs_cli._code(pack), ", ".join(docs_cli._code(name) for name in names))
            for pack, names in outputs["shipped_faces"].items()
        ],
    )
    lines.extend(["## Colored shipped faces", ""])
    docs_cli._table(
        lines,
        ("Tables", "Faces"),
        [
            (
                f"{docs_cli._code('COLR')}, {docs_cli._code('CPAL')}",
                ", ".join(docs_cli._code(name) for name in converter["colored_faces"]),
            )
        ],
    )
    return "\n".join(lines)


def _render_authoring(model: dict[str, object]) -> str:
    lines = _page_header("authoring.md")
    outputs = model["outputs"]
    assert isinstance(outputs, dict)
    converter = outputs["converter"]
    assert isinstance(converter, dict)
    lines.extend(["## Converter facts", ""])
    docs_cli._table(
        lines,
        ("Field", "Value"),
        [
            ("Source formats", ", ".join(docs_cli._code(f".{item}") for item in converter["input_extensions"])),
            ("Font units per bitmap pixel", docs_cli._code(converter["units_per_pixel"])),
            (
                "Native size range",
                f"{converter['native_size_range'][0]}..{converter['native_size_range'][1]} px",
            ),
            ("Neutral glyph grey", docs_cli._code(converter["neutral_grey"])),
            ("Shipped faces", docs_cli._code(converter["shipped_face_count"])),
            ("Python packages", ", ".join(docs_cli._code(item) for item in converter["python_packages"])),
        ],
    )
    lines.extend(["## Authoring rules", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, "authoring_rules"),
    )
    lines.extend(
        [
            "## Converter commands",
            "",
            "```powershell",
            "python BuildTools\\bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf --family \"Name\"",
            "python BuildTools\\bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf --family \"Name\" --check",
            "```",
            "",
            AUTHORING_COMMANDS_NOTE,
            "",
        ]
    )
    return "\n".join(lines)


def _render_layout(model: dict[str, object]) -> str:
    lines = _page_header("layout.md")
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
    lines = _page_header("validation.md")
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
            "python -m unittest BuildTools.tests.test_bitmap_font_to_ttf",
            "cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests",
            "```",
            "",
            VALIDATION_COMMANDS_NOTE,
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


AUTHORING_COMMANDS_NOTE = (
    "Converted faces are generated data: edit the bitmap source or the converter, "
    "regenerate the face, and never edit a face by hand. The `--check` mode fails "
    "when OUTPUT differs from a fresh conversion. The original bitmap sources of the "
    "faces the Engine ships are kept outside it, in the `fonline-historical-stuff` "
    "repository (folder `FontSources`)."
)
VALIDATION_COMMANDS_NOTE = (
    "An embedding project must also bake the faces, run its text-measurement "
    "regression, and inspect representative regular, bordered, colored, wrapped, "
    "and localized text in a visible client."
)

RUSSIAN_TITLES = {
    "Generated Font Format Reference": "Сгенерированный справочник форматов шрифтов",
    "Font Resource Formats": "Форматы ресурсов шрифтов",
    "Font Binding Contract": "Контракт привязки шрифтов",
    "Font Authoring Contract": "Контракт авторинга шрифтов",
    "Text Layout Contract": "Контракт компоновки текста",
    "Font Rendering Contract": "Контракт отрисовки шрифтов",
    "Font Validation Contract": "Контракт проверки шрифтов",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.":
        "> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.",
    "[Index](index.md) | [Formats](formats.md) | [Binding](binding.md) | [Authoring](authoring.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)":
        "[Индекс](index.md) | [Форматы](formats.md) | [Привязка](binding.md) | [Авторинг](authoring.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)",
    "This reference describes the Engine-owned TrueType text pipeline: face binding, the built-in default font, colored faces, the bitmap-font converter, text layout, rendering, and validation. Project font selection and typography policy remain outside this contract.":
        "Этот справочник описывает принадлежащий Engine конвейер TrueType-текста: привязку лиц, встроенный шрифт по умолчанию, цветные лица, конвертер bitmap-шрифтов, компоновку текста, отрисовку и проверку. Выбор шрифтов и типографическая политика проекта остаются вне контракта.",
    "## Contract status": "## Состояние контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный манифест",
    "Contract digest": "Дайджест контракта",
    "Runtime faces": "Runtime-лица",
    "Default font": "Шрифт по умолчанию",
    "Embedded data capacity default": "Ёмкость встроенных данных по умолчанию",
    "Colored face load flags": "Флаги загрузки цветного лица",
    "Client atlas": "Клиентский атлас",
    "| [Formats](formats.md) |": "| [Форматы](formats.md) |",
    "| [Binding](binding.md) |": "| [Привязка](binding.md) |",
    "| [Authoring](authoring.md) |": "| [Авторинг](authoring.md) |",
    "| [Layout](layout.md) |": "| [Компоновка](layout.md) |",
    "| [Rendering](rendering.md) |": "| [Отрисовка](rendering.md) |",
    "| [Validation](validation.md) |": "| [Проверка](validation.md) |",
    "Runtime face formats, converter sources, and shipped faces.":
        "Runtime-форматы лиц, источники конвертера и поставляемые лица.",
    "Face binding, the default font, slots, atlas, and Embedded capacity.":
        "Привязка лиц, шрифт по умолчанию, слоты, атлас и ёмкость Embedded.",
    "The bitmap-font converter, its output, and its staleness check.":
        "Конвертер bitmap-шрифтов, его результат и проверка актуальности.",
    "TextFormat, wrapping, alignment, measurement, and glyph fallback.":
        "TextFormat, перенос, выравнивание, измерение и fallback глифов.",
    "Glyph cells, colors, outlines, effects, color tags, and cache.":
        "Ячейки глифов, цвета, обводка, эффекты, теги цвета и кеш.",
    "Failure modes and executable validation gates.":
        "Режимы отказа и исполняемые gate проверки.",
    "## Boundary": "## Граница ответственности",
    "Included:": "Включено:",
    "Excluded:": "Исключено:",
    "## Shipped faces": "## Поставляемые лица",
    "## Colored shipped faces": "## Поставляемые цветные лица",
    "## Converter facts": "## Параметры конвертера",
    "Source formats": "Исходные форматы",
    "Font units per bitmap pixel": "Единиц шрифта на пиксель bitmap",
    "Native size range": "Диапазон родного размера",
    "Neutral glyph grey": "Нейтральный серый глифа",
    "Shipped faces": "Поставляемых лиц",
    "Python packages": "Пакеты Python",
    "## Authoring rules": "## Правила авторинга",
    "## Converter commands": "## Команды конвертера",
    AUTHORING_COMMANDS_NOTE:
        "Преобразованные лица являются генерируемыми данными: правьте bitmap-источник или конвертер, пересоздавайте лицо и никогда не редактируйте его вручную. Режим `--check` завершается ошибкой, если OUTPUT отличается от свежего преобразования. Исходные bitmap-источники лиц, которые поставляет Engine, хранятся вне него, в репозитории `fonline-historical-stuff` (папка `FontSources`).",
    "See requirement": "См. требование",
    "## FontFlag values": "## Значения FontFlag",
    "## Layout rules": "## Правила компоновки",
    "## Validation commands": "## Команды проверки",
    VALIDATION_COMMANDS_NOTE:
        "Подключаемый проект также должен запечь лица, запустить регрессию измерения текста и проверить в видимом клиенте репрезентативный обычный, обведённый, цветной, перенесённый и локализованный текст.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Suffix | Role | Contract | Source |":
        "| Стабильный ID | Суффикс | Роль | Контракт | Источник |",
    "| Pack | Faces |": "| Пакет | Лица |",
    "| Tables | Faces |": "| Таблицы | Лица |",
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


def _canonical_pages(model: dict[str, object]) -> dict[str, str]:
    return {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/formats.md": _render_formats(model),
        f"{DEFAULT_OUTPUT_DIR}/binding.md": _render_rules(model, "binding.md", "binding_rules"),
        f"{DEFAULT_OUTPUT_DIR}/authoring.md": _render_authoring(model),
        f"{DEFAULT_OUTPUT_DIR}/layout.md": _render_layout(model),
        f"{DEFAULT_OUTPUT_DIR}/rendering.md": _render_rules(model, "rendering.md", "rendering_rules"),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }


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
    canonical_pages = _canonical_pages(model)
    russian_base_pages = _canonical_pages(model if russian_model is None else russian_model)
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
        f"{summary['entry_count']} entries, {summary['binding_rule_count']} binding rules, "
        f"{summary['authoring_rule_count']} authoring rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
