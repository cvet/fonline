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
DEFAULT_MANIFEST = "BuildTools/EffectFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/effect-format.json"
DEFAULT_OUTPUT_DIR = "Docs/generated/effect-format"
GENERATED_BY = "BuildTools/docs_effect_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-effect-format-index", "Generated Effect Format Reference"),
    ("syntax.md", "generated-effect-format-syntax", "Effect File Syntax"),
    ("render-state.md", "generated-effect-format-render-state", "Effect Render State"),
    ("resources.md", "generated-effect-format-resources", "Effect Shader Resources"),
    ("baking.md", "generated-effect-format-baking", "Effect Baking And Backends"),
    ("runtime.md", "generated-effect-format-runtime", "Effect Runtime And Script API"),
    ("validation.md", "generated-effect-format-validation", "Effect Format Validation"),
)
OUTPUT_PATHS = tuple(
    f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS
)
COLLECTION_KINDS = {
    "sections": "section",
    "effect_options": "option",
    "resources": "resource",
    "baking_rules": "baking",
    "runtime_rules": "runtime",
    "script_methods": "script",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^effect-format\."
    r"(limit|section|option|resource|baking|runtime|script|validation)\."
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
    result: list[dict[str, object]] = []
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
        result.append({"path": path, "anchors": anchors})
    return result


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "effect-format":
        raise ValueError("scope.surface must be effect-format")
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
        "effect_baker",
        "effect_baker_header",
        "render_effect",
        "render_effect_header",
        "effect_manager",
        "effect_manager_header",
        "client_runtime",
        "client_script_api",
        "project_interface",
        "cmake_codegen",
        "opengl_backend",
        "direct3d_backend",
        "vulkan_backend",
        "sdl_gpu_backend",
        "core_effect",
        "model_effect",
    ):
        result[field] = _source_path(root, raw.get(field), f"sources.{field}")
    tests = _string_list(raw.get("tests"), "sources.tests")
    result["tests"] = [
        _source_path(root, path, f"sources.tests[{index}]")
        for index, path in enumerate(tests)
    ]
    return result


def _validate_outputs(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    result = copy.deepcopy(raw)
    expected = {
        "source_extension": ".fofx",
        "metadata_suffix": ".fofx-<pass>-info",
        "runtime_side": "client",
        "source_copy": True,
        "stages": ["vert", "frag"],
        "stage_flavors": [
            "spv",
            "spv_sdl",
            "glsl",
            "glsl_es",
            "hlsl",
            "msl_mac",
            "msl_ios",
        ],
    }
    for field, value in expected.items():
        if result.get(field) != value:
            raise ValueError(f"outputs.{field} must be {value}")
    native = result.get("native_descriptor_sets")
    if native != {"uniform_buffers": 0, "samplers": 1}:
        raise ValueError("outputs.native_descriptor_sets must map UBOs to 0 and samplers to 1")
    sdl = result.get("sdl_gpu_descriptor_sets")
    if sdl != {
        "vertex_samplers": 0,
        "vertex_uniform_buffers": 1,
        "fragment_samplers": 2,
        "fragment_uniform_buffers": 3,
    }:
        raise ValueError("outputs.sdl_gpu_descriptor_sets has unexpected values")
    limits = result.get("sdl_gpu_stage_limits")
    if limits != {"samplers": 16, "uniform_buffers": 4}:
        raise ValueError("outputs.sdl_gpu_stage_limits has unexpected values")
    return result


def _load_project_options(root: Path, path: str) -> dict[str, dict[str, object]]:
    try:
        raw = json.loads((root / path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read project interface {path}: {exception}") from exception
    if not isinstance(raw, dict) or not isinstance(raw.get("options"), list):
        raise ValueError(f"project interface {path} has no options array")
    result: dict[str, dict[str, object]] = {}
    for option in raw["options"]:
        if not isinstance(option, dict):
            raise ValueError(f"project interface {path} contains a non-object option")
        name = _required_string(option.get("name"), "project option name")
        if name in result:
            raise ValueError(f"project interface contains duplicate option {name}")
        result[name] = copy.deepcopy(option)
    return result


def _validate_entry_id(
    value: object, label: str, kind: str, identities: set[str]
) -> str:
    entry_id = _required_string(value, f"{label}.id")
    if not ENTRY_ID_PATTERN.fullmatch(entry_id) or not entry_id.startswith(
        f"effect-format.{kind}."
    ):
        raise ValueError(f"invalid {kind} id: {entry_id}")
    if entry_id in identities:
        raise ValueError(f"duplicate effect format entry id: {entry_id}")
    identities.add(entry_id)
    return entry_id


def _validate_limits(
    root: Path,
    raw: object,
    options: dict[str, dict[str, object]],
    identities: set[str],
) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("compile_limits must be a non-empty array")
    result: list[dict[str, object]] = []
    seen_options: set[str] = set()
    for index, entry in enumerate(raw):
        label = f"compile_limits[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        _validate_entry_id(entry.get("id"), label, "limit", identities)
        option = _required_string(entry.get("option"), f"{label}.option")
        if option in seen_options:
            raise ValueError(f"duplicate effect compile limit option: {option}")
        seen_options.add(option)
        if option not in options:
            raise ValueError(f"effect compile limit option is missing: {option}")
        _required_string(entry.get("runtime_name"), f"{label}.runtime_name")
        _required_string(entry.get("description"), f"{label}.description")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        enriched["default"] = options[option].get("default")
        enriched["value_kind"] = options[option].get("value_kind")
        enriched["category"] = options[option].get("category")
        result.append(enriched)
    return result


def _validate_entries(
    root: Path,
    collection: str,
    raw: object,
    identities: set[str],
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
        for field in ("syntax", "default", "availability", "signature", "behavior"):
            if field in entry:
                _required_string(entry.get(field), f"{label}.{field}")
        for field in ("values", "notes", "sides"):
            if field in entry:
                _string_list(entry.get(field), f"{label}.{field}", allow_empty=True)
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        result.append(enriched)
    return result


def generate_effect_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read effect format manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"effect format manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    outputs = _validate_outputs(raw.get("outputs"))
    options = _load_project_options(root, str(sources["project_interface"]))
    identities: set[str] = set()
    compile_limits = _validate_limits(
        root, raw.get("compile_limits"), options, identities
    )
    collections = {
        collection: _validate_entries(
            root, collection, raw.get(collection), identities
        )
        for collection in COLLECTION_KINDS
    }
    stability_counts = Counter(
        str(entry["stability"])
        for collection in collections.values()
        for entry in collection
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
        "compile_limits": compile_limits,
        **collections,
        "summary": {
            "entry_count": len(compile_limits)
            + sum(len(collection) for collection in collections.values()),
            "compile_limit_count": len(compile_limits),
            **{
                f"{collection.removesuffix('s')}_count": len(entries)
                for collection, entries in collections.items()
            },
            "entries_by_stability": dict(sorted(stability_counts.items())),
        },
    }
    contract_content = json.dumps(
        model, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    )
    model["contract_digest"] = hashlib.sha256(
        contract_content.encode("utf-8")
    ).hexdigest()
    return model


def render_effect_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return (
        json.dumps(
            generate_effect_format_model(root, manifest_relative_path),
            ensure_ascii=False,
            indent=2,
        )
        + "\n"
    )


def _source_link(model: dict[str, object], source: str) -> str:
    url = (
        f"https://github.com/{model['repository']}/blob/"
        f"{model['source_ref']}/{quote(source)}"
    )
    return f"[{source}]({url})"


def _source_ref_links(
    model: dict[str, object], refs: list[dict[str, object]]
) -> str:
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
        "`BuildTools/EffectFormatInterface.json`, then run "
        "`python BuildTools/docs_effect_format.py --write`.",
        "",
        "[Index](index.md) | [Syntax](syntax.md) | "
        "[Render state](render-state.md) | [Resources](resources.md) | "
        "[Baking](baking.md) | [Runtime](runtime.md) | "
        "[Validation](validation.md) | [Canonical JSON](../effect-format.json) | "
        "[Guide](../../EffectFormat.md)",
        "",
    ]


def _entry_rows(
    model: dict[str, object], collection: str
) -> list[tuple[str, str, str, str, str]]:
    rows = []
    for entry in model[collection]:
        assert isinstance(entry, dict)
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
                f'<code>{docs_cli._text(entry["id"])}</code>',
                docs_cli._text(entry["name"]),
                docs_cli._text(entry["requirement"]),
                docs_cli._text(entry["rationale"]),
                _source_ref_links(model, entry["source"]),
            )
        )
    return rows


def _render_index(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    outputs = model["outputs"]
    summary = model["summary"]
    assert isinstance(scope, dict)
    assert isinstance(outputs, dict)
    assert isinstance(summary, dict)
    lines.extend(
        [
            "This reference describes the reusable Engine-owned `.fofx` authoring, "
            "baking, renderer-resource, runtime-loading, and script-control contract. "
            "Concrete shader catalogs, visual policy, and ScriptValue slot meanings "
            "remain project-owned.",
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
            ("Source extension", docs_cli._code(outputs["source_extension"])),
            ("Runtime side", docs_cli._code(outputs["runtime_side"])),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            (
                "[Syntax](syntax.md)",
                str(summary["section_count"]),
                "Required and optional sections plus pass fallback.",
            ),
            (
                "[Render state](render-state.md)",
                str(summary["effect_option_count"]),
                "Pass count, blending, depth, shader version, and shadow state.",
            ),
            (
                "[Resources](resources.md)",
                f"{summary['resource_count']} resources / "
                f"{summary['compile_limit_count']} limits",
                "Vertex layouts, samplers, built-in uniform buffers, and bindings.",
            ),
            (
                "[Baking](baking.md)",
                str(summary["baking_rule_count"]),
                "Compiler environment, reflection, output flavors, and SDL remapping.",
            ),
            (
                "[Runtime](runtime.md)",
                f"{summary['runtime_rule_count']} rules / "
                f"{summary['script_method_count']} methods",
                "Effect selection, caching, ScriptValue persistence, and updates.",
            ),
        ],
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_syntax(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    lines.extend(
        [
            "A minimal one-pass effect contains the required config section and one "
            "vertex/fragment shader pair:",
            "",
            "```ini",
            "[Effect]",
            "",
            "[VertexShader]",
            "layout(set = 0, binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };",
            "layout(location = 0) in vec3 InPosition;",
            "void main(void) { gl_Position = ProjMatrix * vec4(InPosition, 1.0); }",
            "",
            "[FragmentShader]",
            "layout(location = 0) out vec4 FragColor;",
            "void main(void) { FragColor = vec4(1.0); }",
            "```",
            "",
        ]
    )
    docs_cli._table(
        lines,
        ("Stable ID", "Section", "Requirement", "Why", "Source"),
        _entry_rows(model, "sections"),
    )
    return "\n".join(lines)


def _render_render_state(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    rows = []
    for entry in model["effect_options"]:
        assert isinstance(entry, dict)
        accepted = ", ".join(docs_cli._code(value) for value in entry.get("values", []))
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
                f'<code>{docs_cli._text(entry["id"])}</code>',
                docs_cli._code(entry.get("syntax", entry["name"])),
                docs_cli._code(entry.get("default", "none")),
                accepted or "project-defined integer",
                docs_cli._text(entry["requirement"]),
                _source_ref_links(model, entry["source"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Key", "Default", "Accepted", "Behavior", "Source"),
        rows,
    )
    lines.extend(
        [
            "Pass-specific keys use the exact `_PassN` suffix and inherit the "
            "unsuffixed value when absent.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_resources(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    lines.extend(["## Compile-time limits", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "CMake option", "Runtime name", "Default", "Meaning", "Source"),
        [
            (
                f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
                f'<code>{docs_cli._text(entry["id"])}</code>',
                docs_cli._code(entry["option"]),
                docs_cli._code(entry["runtime_name"]),
                docs_cli._code(entry["default"]),
                docs_cli._text(entry["description"]),
                _source_ref_links(model, entry["source"]),
            )
            for entry in model["compile_limits"]
        ],
    )
    lines.extend(["## Shader resources and inputs", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Resource", "Requirement", "Why", "Source"),
        _entry_rows(model, "resources"),
    )
    return "\n".join(lines)


def _render_baking(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[4][1:])
    outputs = model["outputs"]
    assert isinstance(outputs, dict)
    lines.extend(
        [
            "Every pass produces one metadata file and seven flavors per shader "
            "stage. The original `.fofx` source is copied to baked resources because "
            "the runtime still reads `[Effect]` state from it.",
            "",
            "Stage flavors: "
            + ", ".join(docs_cli._code(item) for item in outputs["stage_flavors"]),
            "",
        ]
    )
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, "baking_rules"),
    )
    return "\n".join(lines)


def _render_runtime(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[5][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _entry_rows(model, "runtime_rules"),
    )
    lines.extend(["## Script methods", ""])
    rows = []
    for entry in model["script_methods"]:
        assert isinstance(entry, dict)
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
                f'<code>{docs_cli._text(entry["id"])}</code>',
                docs_cli._code(entry["signature"]),
                ", ".join(docs_cli._code(side) for side in entry.get("sides", [])),
                docs_cli._text(entry.get("behavior", entry["requirement"])),
                docs_cli._text(entry["requirement"]),
                _source_ref_links(model, entry["source"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Signature", "Sides", "Behavior", "Errors", "Source"),
        rows,
    )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[6][1:])
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
            "python BuildTools\\docs_effect_format.py --check",
            "python -m unittest BuildTools.tests.test_docs_effect_format",
            "cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests",
            "```",
            "",
            "Finish in an embedding project with its resource bake and visible checks "
            "on every renderer/backend profile that the project supports.",
            "",
        ]
    )
    return "\n".join(lines)


def generate_reference_pages(model: dict[str, object]) -> dict[str, str]:
    if (
        model.get("schema_version") != SCHEMA_VERSION
        or model.get("generated_by") != GENERATED_BY
    ):
        raise ValueError("unsupported generated effect format model")
    identities = [
        entry.get("id")
        for collection in ("compile_limits", *COLLECTION_KINDS)
        for entry in model.get(collection, [])
        if isinstance(entry, dict)
    ]
    if (
        any(not isinstance(identity, str) or not identity for identity in identities)
        or len(identities) != len(set(identities))
    ):
        raise ValueError("every effect format entry must have a unique non-empty ID")
    pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/syntax.md": _render_syntax(model),
        f"{DEFAULT_OUTPUT_DIR}/render-state.md": _render_render_state(model),
        f"{DEFAULT_OUTPUT_DIR}/resources.md": _render_resources(model),
        f"{DEFAULT_OUTPUT_DIR}/baking.md": _render_baking(model),
        f"{DEFAULT_OUTPUT_DIR}/runtime.md": _render_runtime(model),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated effect format page set does not match OUTPUT_PATHS")
    return {
        path: content.rstrip() + "\n" for path, content in sorted(pages.items())
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline effect format model and reference"
    )
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_effect_format_model(root, args.manifest)
        model = json.loads(model_content)
        pages = generate_reference_pages(model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(
            f"Unable to generate effect format documentation: {exception}",
            file=sys.stderr,
        )
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote effect format model and {len(pages)} reference pages")
        return 0

    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file()
        or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated effect format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_effect_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated effect format documentation is current: "
        f"{summary['entry_count']} entries, "
        f"{summary['resource_count']} resources, "
        f"{summary['validation_rule_count']} validation rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
