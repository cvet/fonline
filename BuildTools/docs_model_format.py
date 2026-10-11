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
DEFAULT_MANIFEST = "BuildTools/ModelFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/model-format.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/model-format"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/model-format"
LEGACY_OUTPUT_DIR = "Docs/generated/model-format"
GENERATED_BY = "BuildTools/docs_model_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-model-format-index", "Generated Model Format Reference"),
    ("syntax.md", "generated-model-format-syntax", "Model Description Syntax"),
    ("tokens.md", "generated-model-format-tokens", "Model Description Tokens"),
    ("composition.md", "generated-model-format-composition", "Model Composition"),
    ("assets.md", "generated-model-format-assets", "Model Assets And Limits"),
    ("animation.md", "generated-model-format-animation", "Model Animation Directives"),
    ("validation.md", "generated-model-format-validation", "Model Format Validation"),
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
OUTPUT_PATHS = CANONICAL_OUTPUT_PATHS + RUSSIAN_OUTPUT_PATHS + LEGACY_OUTPUT_PATHS
ENTRY_ID_PATTERN = re.compile(
    r"^model-format\.(limit|asset|token|rule)\.[A-Za-z0-9][A-Za-z0-9.-]*$"
)
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}
VALID_TOKEN_CATEGORIES = {
    "structure",
    "selector",
    "composition",
    "geometry",
    "transform",
    "material",
    "animation",
    "rendering",
}
PARSER_FUNCTION_START = "void ModelDescriptionParser::ParseToken"
PARSER_FUNCTION_END = "void ModelDescriptionParser::ApplyFloatValue"
PARSER_TOKEN_PATTERN = re.compile(r'token == "([^"]+)"')


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
    if not isinstance(raw, dict) or raw.get("surface") != "model-format":
        raise ValueError("scope.surface must be model-format")
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
        "model_info_baker",
        "model_mesh_baker",
        "client_runtime",
        "client_types",
        "rendering_limits",
        "project_interface",
        "baking_pipeline",
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
    if result.get("source_extension") != ".fo3d":
        raise ValueError("outputs.source_extension must be .fo3d")
    mesh_extensions = _string_list(
        result.get("mesh_extensions"), "outputs.mesh_extensions"
    )
    if mesh_extensions != [".fbx", ".obj"]:
        raise ValueError("outputs.mesh_extensions must be ['.fbx', '.obj']")
    if result.get("template_prefix") != "TEMPLATE_":
        raise ValueError("outputs.template_prefix must be TEMPLATE_")
    for field in (
        "baked_description",
        "animation_metadata",
        "runtime_side",
    ):
        _required_string(result.get(field), f"outputs.{field}")
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
        f"model-format.{kind}."
    ):
        raise ValueError(f"invalid {kind} id: {entry_id}")
    if entry_id in identities:
        raise ValueError(f"duplicate model format entry id: {entry_id}")
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
    options_seen: set[str] = set()
    for index, entry in enumerate(raw):
        label = f"compile_limits[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        _validate_entry_id(entry.get("id"), label, "limit", identities)
        option_name = _required_string(entry.get("option"), f"{label}.option")
        if option_name in options_seen:
            raise ValueError(f"duplicate compile limit option: {option_name}")
        options_seen.add(option_name)
        if option_name not in options:
            raise ValueError(f"compile limit option is missing: {option_name}")
        runtime_name = _required_string(
            entry.get("runtime_name"), f"{label}.runtime_name"
        )
        _required_string(entry.get("description"), f"{label}.description")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        enriched["default"] = options[option_name].get("default")
        enriched["value_kind"] = options[option_name].get("value_kind")
        enriched["category"] = options[option_name].get("category")
        if runtime_name == option_name:
            raise ValueError(f"{label}.runtime_name must differ from the CMake option")
        result.append(enriched)
    return result


def _validate_assets(
    root: Path, raw: object, identities: set[str]
) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("assets must be a non-empty array")
    result: list[dict[str, object]] = []
    for index, entry in enumerate(raw):
        label = f"assets[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        _validate_entry_id(entry.get("id"), label, "asset", identities)
        _required_string(entry.get("name"), f"{label}.name")
        _string_list(entry.get("extensions"), f"{label}.extensions")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        _required_string(entry.get("description"), f"{label}.description")
        _string_list(entry.get("requirements"), f"{label}.requirements")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        result.append(enriched)
    return result


def _extract_parser_tokens(root: Path, source_path: str) -> list[str]:
    text = (root / source_path).read_text(encoding="utf-8")
    if PARSER_FUNCTION_START not in text or PARSER_FUNCTION_END not in text:
        raise ValueError("unable to isolate ModelDescriptionParser::ParseToken")
    body = text.split(PARSER_FUNCTION_START, 1)[1].split(PARSER_FUNCTION_END, 1)[0]
    result: list[str] = []
    for token in PARSER_TOKEN_PATTERN.findall(body):
        if token not in result:
            result.append(token)
    if not result:
        raise ValueError("no .fo3d parser tokens found")
    return result


def _validate_tokens(
    root: Path,
    raw: object,
    parser_tokens: list[str],
    identities: set[str],
) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("tokens must be a non-empty array")
    result: list[dict[str, object]] = []
    token_names: list[str] = []
    for index, entry in enumerate(raw):
        label = f"tokens[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        _validate_entry_id(entry.get("id"), label, "token", identities)
        names = _string_list(entry.get("names"), f"{label}.names")
        token_names.extend(names)
        category = _required_string(entry.get("category"), f"{label}.category")
        if category not in VALID_TOKEN_CATEGORIES:
            raise ValueError(f"unsupported {label}.category: {category}")
        _required_string(entry.get("syntax"), f"{label}.syntax")
        _required_string(entry.get("context"), f"{label}.context")
        if not isinstance(entry.get("repeatable"), bool):
            raise ValueError(f"{label}.repeatable must be a boolean")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        _required_string(entry.get("description"), f"{label}.description")
        _required_string(entry.get("runtime_effect"), f"{label}.runtime_effect")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        result.append(enriched)
    if len(token_names) != len(set(token_names)):
        duplicates = sorted(
            name for name, count in Counter(token_names).items() if count > 1
        )
        raise ValueError(f"duplicate documented parser tokens: {duplicates}")
    if set(token_names) != set(parser_tokens):
        missing = sorted(set(parser_tokens) - set(token_names))
        extra = sorted(set(token_names) - set(parser_tokens))
        raise ValueError(
            f"documented .fo3d tokens diverge from ParseToken: missing={missing}, extra={extra}"
        )
    return result


def _validate_rules(
    root: Path, raw: object, identities: set[str]
) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("rules must be a non-empty array")
    result: list[dict[str, object]] = []
    for index, entry in enumerate(raw):
        label = f"rules[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        _validate_entry_id(entry.get("id"), label, "rule", identities)
        for field in ("name", "requirement", "rationale"):
            _required_string(entry.get(field), f"{label}.{field}")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        result.append(enriched)
    return result


def _validate_removed_legacy(raw: object, parser_tokens: list[str]) -> list[dict[str, str]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("removed_legacy must be a non-empty array")
    result: list[dict[str, str]] = []
    names: set[str] = set()
    for index, entry in enumerate(raw):
        label = f"removed_legacy[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        name = _required_string(entry.get("name"), f"{label}.name")
        if name in names:
            raise ValueError(f"duplicate removed legacy token: {name}")
        names.add(name)
        if name in parser_tokens:
            raise ValueError(f"removed legacy token is still accepted by ParseToken: {name}")
        result.append(
            {
                "name": name,
                "replacement": _required_string(
                    entry.get("replacement"), f"{label}.replacement"
                ),
                "description": _required_string(
                    entry.get("description"), f"{label}.description"
                ),
            }
        )
    return result


def generate_model_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read model format manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"model format manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    outputs = _validate_outputs(raw.get("outputs"))
    options = _load_project_options(root, str(sources["project_interface"]))
    identities: set[str] = set()
    compile_limits = _validate_limits(
        root, raw.get("compile_limits"), options, identities
    )
    assets = _validate_assets(root, raw.get("assets"), identities)
    parser_tokens = _extract_parser_tokens(root, str(sources["model_info_baker"]))
    tokens = _validate_tokens(root, raw.get("tokens"), parser_tokens, identities)
    rules = _validate_rules(root, raw.get("rules"), identities)
    removed_legacy = _validate_removed_legacy(raw.get("removed_legacy"), parser_tokens)
    token_categories = Counter(str(entry["category"]) for entry in tokens)
    stability_counts = Counter(str(entry["stability"]) for entry in tokens)

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
        "assets": assets,
        "tokens": tokens,
        "parser_tokens": parser_tokens,
        "rules": rules,
        "removed_legacy": removed_legacy,
        "summary": {
            "token_group_count": len(tokens),
            "parser_token_count": len(parser_tokens),
            "asset_count": len(assets),
            "limit_count": len(compile_limits),
            "rule_count": len(rules),
            "removed_legacy_count": len(removed_legacy),
            "tokens_by_category": dict(sorted(token_categories.items())),
            "tokens_by_stability": dict(sorted(stability_counts.items())),
        },
    }
    contract_content = json.dumps(
        model, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    )
    model["contract_digest"] = hashlib.sha256(
        contract_content.encode("utf-8")
    ).hexdigest()
    return model


def render_model_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return (
        json.dumps(
            generate_model_format_model(root, manifest_relative_path),
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
        "`BuildTools/ModelFormatInterface.json`, then run "
        "`python BuildTools/docs_model_format.py --write`.",
        "",
        "[Index](index.md) | [Syntax](syntax.md) | [Tokens](tokens.md) | "
        "[Composition](composition.md) | [Assets](assets.md) | "
        "[Animation](animation.md) | [Validation](validation.md) | "
        "[Canonical JSON](../../../generated/model-format.json) | "
        "[Guide](../../how-to/content/model-format.md)",
        "",
    ]


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
            "This reference describes the reusable Engine-owned `.fo3d` language and "
            "the model assets it composes. Concrete game models and layer meanings "
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
            (
                "Mesh inputs",
                ", ".join(docs_cli._code(item) for item in outputs["mesh_extensions"]),
            ),
            ("Runtime side", docs_cli._code(outputs["runtime_side"])),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            (
                "[Tokens](tokens.md)",
                f"{summary['token_group_count']} groups / "
                f"{summary['parser_token_count']} spellings",
                "Every accepted current parser token.",
            ),
            (
                "[Assets](assets.md)",
                str(summary["asset_count"]),
                "Mesh, description, texture, effect, and particle inputs.",
            ),
            (
                "[Validation](validation.md)",
                str(summary["rule_count"]),
                "Authoring, baking, runtime, and legacy rules.",
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
            "The parser is stateful and whitespace-tokenized. A compact line is legal, "
            "but directive order determines the current layer link and mesh selector.",
            "",
            "## Minimal concrete description",
            "",
            "```text",
            "Model Body.fbx",
            "",
            "Anim CritterStateAnim.Unarmed CritterActionAnim.Idle ModelFile Idle",
            "",
            "Layer 1",
            "Value 1",
            "Attach Hat.fbx Link Head",
            "```",
            "",
            "## Include template",
            "",
            "```text",
            "# TEMPLATE_Humanoid.fo3d",
            "Model %mesh%",
            "Scale* %scale%",
            "```",
            "",
            "```text",
            "# Human.fo3d",
            "Include TEMPLATE_Humanoid.fo3d mesh Human.fbx scale 0.9",
            "```",
            "",
            "## Syntax rules",
            "",
        ]
    )
    syntax_rule_ids = {
        "model-format.rule.lexical-syntax",
        "model-format.rule.multiple-directives",
        "model-format.rule.selector-order",
        "model-format.rule.template-files",
        "model-format.rule.include-replacements",
        "model-format.rule.relative-paths",
        "model-format.rule.zero-identity",
        "model-format.rule.layer-zero",
    }
    rows = []
    for rule in model["rules"]:
        assert isinstance(rule, dict)
        if rule["id"] not in syntax_rule_ids:
            continue
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(rule["id"]))}"></a>'
                f'<code>{docs_cli._text(rule["id"])}</code>',
                docs_cli._text(rule["name"]),
                docs_cli._text(rule["requirement"]),
                docs_cli._text(rule["rationale"]),
            )
        )
    docs_cli._table(lines, ("Stable ID", "Rule", "Requirement", "Why"), rows)
    return "\n".join(lines)


def _render_tokens(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    lines.extend(
        [
            "The manifest token set is compared directly with "
            "`ModelDescriptionParser::ParseToken`. A new or removed parser spelling "
            "makes generation fail until this table is reconciled.",
            "",
        ]
    )
    rows = []
    for entry in model["tokens"]:
        assert isinstance(entry, dict)
        names = ", ".join(docs_cli._code(name) for name in entry["names"])
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
                f'<code>{docs_cli._text(entry["id"])}</code>',
                names,
                docs_cli._code(entry["category"]),
                docs_cli._code(entry["syntax"]),
                docs_cli._text(entry["context"]),
                docs_cli._code(entry["stability"]),
                docs_cli._text(entry["description"]),
                docs_cli._text(entry["runtime_effect"]),
            )
        )
    docs_cli._table(
        lines,
        (
            "Stable ID",
            "Token spellings",
            "Category",
            "Syntax",
            "Context",
            "Stability",
            "Authoring contract",
            "Runtime effect",
        ),
        rows,
    )
    return "\n".join(lines)


def _render_composition(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    lines.extend(
        [
            "Runtime composition starts from the default Root data, then activates "
            "links whose `Layer` and `Value` match the current model-layer array.",
            "",
            "## Layer composition flow",
            "",
            "1. Copy the project-provided layer array.",
            "2. Apply exact `AnimLayerValue` overrides for the requested animation.",
            "3. Apply default Root transforms, materials, effects, disables, and cuts.",
            "4. Activate matching layer Root entries and child/particle attachments.",
            "5. Remove children and particles whose links are no longer active.",
            "6. Regenerate combined meshes when composition, materials, effects, or cuts changed.",
            "",
            "## Composition directives",
            "",
        ]
    )
    categories = {"composition", "geometry", "transform", "material", "rendering"}
    rows = []
    for entry in model["tokens"]:
        assert isinstance(entry, dict)
        if entry["category"] not in categories:
            continue
        rows.append(
            (
                ", ".join(docs_cli._code(name) for name in entry["names"]),
                docs_cli._code(entry["context"]),
                docs_cli._text(entry["description"]),
                docs_cli._text(entry["runtime_effect"]),
                _source_ref_links(model, entry["source"]),
            )
        )
    docs_cli._table(
        lines,
        ("Directive", "Context", "Authoring contract", "Runtime effect", "Source"),
        rows,
    )
    lines.extend(
        [
            "## Attachment choice",
            "",
            "- Use `Attach child.fo3d` when the child needs its own model description, "
            "layers, materials, cuts, or animation declarations.",
            "- Use `Attach child.fbx` or `Attach child.obj` for a direct baked hierarchy.",
            "- Add `Link Bone` to place the complete child under one parent bone.",
            "- Omit `Link` only when parent and child intentionally share same-named "
            "bones and the child should follow the parent skeleton.",
            "- Use `AttachParticles ... Link Bone`; the runtime requires a target bone.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_assets(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[4][1:])
    lines.extend(
        [
            "`ModelMeshBaker` bakes mesh sources before `ModelInfoBaker` validates "
            "and serializes concrete `.fo3d` descriptions.",
            "",
            "## Asset inputs",
            "",
        ]
    )
    rows = []
    for asset in model["assets"]:
        assert isinstance(asset, dict)
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(asset["id"]))}"></a>'
                f'<code>{docs_cli._text(asset["id"])}</code>',
                docs_cli._text(asset["name"]),
                ", ".join(docs_cli._code(ext) for ext in asset["extensions"]),
                docs_cli._text(asset["description"]),
                "<br>".join(docs_cli._text(item) for item in asset["requirements"]),
                _source_ref_links(model, asset["source"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Asset", "Extensions", "Purpose", "Requirements", "Source"),
        rows,
    )
    lines.extend(["## Compile-time limits", ""])
    limit_rows = []
    for limit in model["compile_limits"]:
        assert isinstance(limit, dict)
        limit_rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(limit["id"]))}"></a>'
                f'<code>{docs_cli._text(limit["id"])}</code>',
                docs_cli._code(limit["option"]),
                docs_cli._code(limit["runtime_name"]),
                docs_cli._code(limit["default"]),
                docs_cli._text(limit["description"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Project option", "Runtime constant", "Default", "Meaning"),
        limit_rows,
    )
    lines.extend(
        [
            "The defaults above come from the generated project-interface contract. "
            "A project may override them, but client binaries, baked resources, model "
            "layer properties, shaders, and packaged content must agree.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_animation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[5][1:])
    lines.extend(
        [
            "This page lists `.fo3d` directives that participate in animation "
            "selection or movement-pose composition. Effective durations, alias "
            "materialization, and script lookup are documented in "
            "[Model Animation](../../how-to/content/model-animation.md).",
            "",
        ]
    )
    rows = []
    for entry in model["tokens"]:
        assert isinstance(entry, dict)
        if entry["category"] != "animation":
            continue
        rows.append(
            (
                ", ".join(docs_cli._code(name) for name in entry["names"]),
                docs_cli._code(entry["syntax"]),
                docs_cli._text(entry["description"]),
                docs_cli._text(entry["runtime_effect"]),
            )
        )
    docs_cli._table(
        lines,
        ("Directive", "Syntax", "Authoring contract", "Runtime effect"),
        rows,
    )
    lines.extend(
        [
            "## Separation from 2D root motion",
            "",
            "These directives drive 3D skeletal clips and model composition. "
            "`NextX` / `NextY` sprite-frame offsets and movement-projected frame "
            "selection belong to [Sprite Root Motion](../../how-to/content/sprite-root-motion.md).",
            "",
        ]
    )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[6][1:])
    lines.extend(["## Contract rules", ""])
    rows = []
    for rule in model["rules"]:
        assert isinstance(rule, dict)
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(rule["id"]))}"></a>'
                f'<code>{docs_cli._text(rule["id"])}</code>',
                docs_cli._text(rule["name"]),
                docs_cli._text(rule["requirement"]),
                docs_cli._text(rule["rationale"]),
                _source_ref_links(model, rule["source"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        rows,
    )
    lines.extend(["## Removed legacy spellings", ""])
    legacy_rows = []
    for entry in model["removed_legacy"]:
        assert isinstance(entry, dict)
        legacy_rows.append(
            (
                docs_cli._code(entry["name"]),
                docs_cli._code(entry["replacement"]),
                docs_cli._text(entry["description"]),
            )
        )
    docs_cli._table(lines, ("Removed token", "Replacement", "Current contract"), legacy_rows)
    lines.extend(
        [
            "The accepted compatibility spelling `Subset` is listed separately in "
            "[Tokens](tokens.md) as deprecated because it consumes an argument but "
            "does not select a mesh.",
            "",
            "## Validation commands",
            "",
            "```powershell",
            "python BuildTools\\docs_model_format.py --check",
            "python -m unittest BuildTools.tests.test_docs_model_format",
            ".\\Binaries\\Tests-Windows-win64\\LF_UnitTests.exe \"ModelBaker*\"",
            "cmake --build Build\\Auto --config RelWithDebInfo --target BakeResources",
            "```",
            "",
            "Finish with a visible client scene that exercises every authored layer "
            "combination, attachment, material override, cut, animation, draw size, "
            "and interaction bound used by the project.",
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
    english_path = f"../../en/reference/model-format/{filename}"
    russian_path = f"../../ru/reference/model-format/{filename}"
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
    "Generated Model Format Reference": "Справочник формата моделей",
    "Model Description Syntax": "Синтаксис описания моделей",
    "Model Description Tokens": "Токены описания моделей",
    "Model Composition": "Композиция моделей",
    "Model Assets And Limits": "Ресурсы и лимиты моделей",
    "Model Animation Directives": "Директивы анимации моделей",
    "Model Format Validation": "Валидация формата моделей",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit directly. Update `BuildTools/ModelFormatInterface.json`, then run `python BuildTools/docs_model_format.py --write`.":
        "> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/ModelFormatInterface.json`, затем запустите `python BuildTools/docs_model_format.py --write`.",
    "[Index](index.md) | [Syntax](syntax.md) | [Tokens](tokens.md) | [Composition](composition.md) | [Assets](assets.md) | [Animation](animation.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/model-format.json) | [Guide](../../how-to/content/model-format.md)":
        "[Индекс](index.md) | [Синтаксис](syntax.md) | [Токены](tokens.md) | [Композиция](composition.md) | [Ресурсы](assets.md) | [Анимация](animation.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/model-format.json) | [Руководство](../../how-to/content/model-format.md)",
    "This reference describes the reusable Engine-owned `.fo3d` language and the model assets it composes. Concrete game models and layer meanings remain project-owned.":
        "Справочник описывает переиспользуемый язык `.fo3d`, принадлежащий движку, и собираемые им ресурсы моделей. Конкретные игровые модели и семантика слоёв принадлежат проекту.",
    "## Contract status": "## Состояние контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный манифест",
    "Contract digest": "Дайджест контракта",
    "Source extension": "Расширение исходника",
    "Mesh inputs": "Входные меши",
    "Runtime side": "Runtime-сторона",
    "[Tokens](tokens.md)": "[Токены](tokens.md)",
    " groups / ": " группы / ",
    " spellings": " написаний",
    "Every accepted current parser token.": "Все принимаемые текущим parser токены.",
    "[Assets](assets.md)": "[Ресурсы](assets.md)",
    "Mesh, description, texture, effect, and particle inputs.":
        "Входы мешей, описаний, текстур, эффектов и частиц.",
    "[Validation](validation.md)": "[Валидация](validation.md)",
    "Authoring, baking, runtime, and legacy rules.":
        "Правила авторинга, запекания, runtime и legacy.",
    "## Boundary": "## Граница",
    "Included:": "Включено:",
    "Excluded:": "Не включено:",
    "The parser is stateful and whitespace-tokenized. A compact line is legal, but directive order determines the current layer link and mesh selector.":
        "Parser хранит состояние и разделяет вход по whitespace. Компактная строка допустима, но порядок directives определяет текущий layer link и selector mesh.",
    "## Minimal concrete description": "## Минимальное конкретное описание",
    "## Include template": "## Include-шаблон",
    "## Syntax rules": "## Правила синтаксиса",
    "The manifest token set is compared directly with `ModelDescriptionParser::ParseToken`. A new or removed parser spelling makes generation fail until this table is reconciled.":
        "Набор токенов манифеста напрямую сравнивается с `ModelDescriptionParser::ParseToken`. Добавление или удаление parser spelling останавливает генерацию, пока таблица не будет согласована.",
    "Runtime composition starts from the default Root data, then activates links whose `Layer` and `Value` match the current model-layer array.":
        "Runtime-композиция начинается с default `Root`, затем активирует links, у которых `Layer` и `Value` совпадают с текущим model-layer array.",
    "## Layer composition flow": "## Процесс композиции слоёв",
    "1. Copy the project-provided layer array.":
        "1. Скопировать предоставленный проектом layer array.",
    "2. Apply exact `AnimLayerValue` overrides for the requested animation.":
        "2. Применить точные overrides `AnimLayerValue` для запрошенной animation.",
    "3. Apply default Root transforms, materials, effects, disables, and cuts.":
        "3. Применить default `Root`: transforms, materials, effects, disables и cuts.",
    "4. Activate matching layer Root entries and child/particle attachments.":
        "4. Активировать совпавшие layer `Root`, child attachments и particle attachments.",
    "5. Remove children and particles whose links are no longer active.":
        "5. Удалить children и particles, links которых перестали быть активны.",
    "6. Regenerate combined meshes when composition, materials, effects, or cuts changed.":
        "6. Перегенерировать combined meshes при изменении composition, materials, effects или cuts.",
    "## Composition directives": "## Директивы композиции",
    "## Attachment choice": "## Выбор attachment",
    "- Use `Attach child.fo3d` when the child needs its own model description, layers, materials, cuts, or animation declarations.":
        "- Используйте `Attach child.fo3d`, если child нужны собственные description, layers, materials, cuts или animations.",
    "- Use `Attach child.fbx` or `Attach child.obj` for a direct baked hierarchy.":
        "- Используйте `Attach child.fbx` или `Attach child.obj` для прямой baked hierarchy.",
    "- Add `Link Bone` to place the complete child under one parent bone.":
        "- Добавьте `Link Bone`, чтобы поместить весь child под одной parent bone.",
    "- Omit `Link` only when parent and child intentionally share same-named bones and the child should follow the parent skeleton.":
        "- Не задавайте `Link` только когда parent и child намеренно имеют одноимённые bones и child должен следовать parent skeleton.",
    "- Use `AttachParticles ... Link Bone`; the runtime requires a target bone.":
        "- Для particles используйте `AttachParticles ... Link Bone`; runtime требует target bone.",
    "`ModelMeshBaker` bakes mesh sources before `ModelInfoBaker` validates and serializes concrete `.fo3d` descriptions.":
        "`ModelMeshBaker` запекает исходники мешей до того, как `ModelInfoBaker` проверяет и сериализует конкретные `.fo3d`.",
    "## Asset inputs": "## Входные ресурсы",
    "## Compile-time limits": "## Compile-time лимиты",
    "The defaults above come from the generated project-interface contract. A project may override them, but client binaries, baked resources, model layer properties, shaders, and packaged content must agree.":
        "Defaults взяты из generated project-interface contract. Проект может их переопределить, но client binaries, baked resources, model layer properties, shaders и packaged content должны использовать одинаковые значения.",
    "This page lists `.fo3d` directives that participate in animation selection or movement-pose composition. Effective durations, alias materialization, and script lookup are documented in [Model Animation](../../how-to/content/model-animation.md).":
        "Здесь перечислены directives `.fo3d`, участвующие в выборе анимации и композиции позы движения. Effective durations, materialization aliases и script lookup описаны в [Model Animation](../../how-to/content/model-animation.md).",
    "## Separation from 2D root motion": "## Отделение от 2D root motion",
    "These directives drive 3D skeletal clips and model composition. `NextX` / `NextY` sprite-frame offsets and movement-projected frame selection belong to [Sprite Root Motion](../../how-to/content/sprite-root-motion.md).":
        "Эти directives управляют 3D skeletal clips и model composition. Offsets `NextX` / `NextY` и выбор кадров по направлению движения принадлежат [Sprite Root Motion](../../how-to/content/sprite-root-motion.md).",
    "## Contract rules": "## Правила контракта",
    "## Removed legacy spellings": "## Удалённые legacy-написания",
    "The accepted compatibility spelling `Subset` is listed separately in [Tokens](tokens.md) as deprecated because it consumes an argument but does not select a mesh.":
        "Compatibility spelling `Subset` отдельно помечен как deprecated в [токенах](tokens.md): он забирает argument, но не выбирает mesh.",
    "## Validation commands": "## Команды проверки",
    "Finish with a visible client scene that exercises every authored layer combination, attachment, material override, cut, animation, draw size, and interaction bound used by the project.":
        "Завершите видимой client scene, которая покрывает все используемые проектом сочетания слоёв, attachments, material overrides, cuts, animations, draw size и interaction bounds.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |":
        "| Справочник | Записей | Назначение |",
    "| Stable ID | Rule | Requirement | Why |":
        "| Стабильный ID | Правило | Требование | Причина |",
    "| Stable ID | Token spellings | Category | Syntax | Context | Stability | Authoring contract | Runtime effect |":
        "| Стабильный ID | Написания token | Категория | Синтаксис | Контекст | Стабильность | Контракт авторинга | Runtime-эффект |",
    "| Directive | Context | Authoring contract | Runtime effect | Source |":
        "| Directive | Контекст | Контракт авторинга | Runtime-эффект | Источник |",
    "| Stable ID | Asset | Extensions | Purpose | Requirements | Source |":
        "| Стабильный ID | Ресурс | Расширения | Назначение | Требования | Источник |",
    "| Stable ID | Project option | Runtime constant | Default | Meaning |":
        "| Стабильный ID | Опция проекта | Runtime-константа | По умолчанию | Смысл |",
    "| Directive | Syntax | Authoring contract | Runtime effect |":
        "| Directive | Синтаксис | Контракт авторинга | Runtime-эффект |",
    "| Stable ID | Rule | Requirement | Why | Source |":
        "| Стабильный ID | Правило | Требование | Причина | Источник |",
    "| Removed token | Replacement | Current contract |":
        "| Удалённый token | Замена | Текущий контракт |",
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
        raise ValueError("generated model format page has no front matter")
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
    if (
        model.get("schema_version") != SCHEMA_VERSION
        or model.get("generated_by") != GENERATED_BY
    ):
        raise ValueError("unsupported generated model format model")
    identities = [
        entry.get("id")
        for key in ("compile_limits", "assets", "tokens", "rules")
        for entry in model.get(key, [])
        if isinstance(entry, dict)
    ]
    if (
        any(not isinstance(identity, str) or not identity for identity in identities)
        or len(identities) != len(set(identities))
    ):
        raise ValueError("every model format entry must have a unique non-empty ID")
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/syntax.md": _render_syntax(model),
        f"{DEFAULT_OUTPUT_DIR}/tokens.md": _render_tokens(model),
        f"{DEFAULT_OUTPUT_DIR}/composition.md": _render_composition(model),
        f"{DEFAULT_OUTPUT_DIR}/assets.md": _render_assets(model),
        f"{DEFAULT_OUTPUT_DIR}/animation.md": _render_animation(model),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/syntax.md": _render_syntax(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/tokens.md": _render_tokens(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/composition.md": _render_composition(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/assets.md": _render_assets(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/animation.md": _render_animation(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(localized_model),
    }
    pages = dict(canonical_pages)
    for (filename, document_id, title), canonical_path in zip(
        PAGE_DEFINITIONS, CANONICAL_OUTPUT_PATHS, strict=True
    ):
        pages[f"{RUSSIAN_OUTPUT_DIR}/{filename}"] = _render_russian_page(
            document_id,
            canonical_path,
            canonical_pages[canonical_path],
            russian_base_pages[canonical_path],
        )
        pages[f"{LEGACY_OUTPUT_DIR}/{filename}"] = _render_legacy_page(
            canonical_path,
            title,
            canonical_pages[canonical_path],
        )
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated model format page set does not match OUTPUT_PATHS")
    return {
        path: content.rstrip() + "\n" for path, content in sorted(pages.items())
    }


def render_reference_pages(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    model = generate_model_format_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "model-format",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline model format model and reference"
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
        model_content = render_model_format_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "model-format",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(
            f"Unable to generate model format documentation: {exception}",
            file=sys.stderr,
        )
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote model format model and {len(pages)} reference pages")
        return 0

    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file()
        or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated model format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_model_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated model format documentation is current: "
        f"{summary['token_group_count']} token groups, "
        f"{summary['parser_token_count']} parser spellings, "
        f"{summary['asset_count']} assets, {summary['rule_count']} rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
