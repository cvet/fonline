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
DEFAULT_MANIFEST = "BuildTools/ParticleFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/particle-format.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/particle-format"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/particle-format"
LEGACY_OUTPUT_DIR = "Docs/generated/particle-format"
GENERATED_BY = "BuildTools/docs_particle_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-particle-format-index", "Generated Particle Format Reference"),
    ("xml.md", "generated-particle-format-xml", "Particle Source Rules"),
    ("objects.md", "generated-particle-format-objects", "Particle Formats and Backends"),
    ("renderer.md", "generated-particle-format-renderer", "Particle Rendering Contract"),
    ("tooling.md", "generated-particle-format-tooling", "Particle Tooling Contract"),
    ("runtime.md", "generated-particle-format-runtime", "Particle Runtime Contract"),
    ("integration.md", "generated-particle-format-integration", "Particle Integration Contract"),
    ("validation.md", "generated-particle-format-validation", "Particle Validation Contract"),
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
COLLECTION_KINDS = {
    "object_families": "family",
    "objects": "object",
    "xml_rules": "xml",
    "renderer_fields": "renderer",
    "tooling_rules": "tooling",
    "runtime_rules": "runtime",
    "integration_rules": "integration",
    "validation_rules": "validation",
}
MANIFEST_COLLECTIONS = tuple(COLLECTION_KINDS)
ENTRY_ID_PATTERN = re.compile(
    r"^particle-format\."
    r"(family|object|xml|renderer|tooling|runtime|integration|validation)\."
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
    if not isinstance(raw, dict) or raw.get("surface") != "particle-format":
        raise ValueError("scope.surface must be particle-format")
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


def _derive_runtime_extension(source_text: str, class_name: str) -> str:
    match = re.search(
        rf"{re.escape(class_name)}::GetExtensions\(\) const -> vector<string>"
        r".*?return \{\"([a-z0-9]+)\"\};",
        source_text,
        re.DOTALL,
    )
    if match is None:
        raise ValueError(f"unable to derive runtime extension for {class_name}")
    return match.group(1)


def _derive_bake_transforms(source_text: str) -> dict[str, str]:
    transforms: dict[str, str] = {}
    for source_extension, runtime_extension in re.findall(
        r'if \(ext == "(spark|efkproj)"\).*?change_file_extension\("(spk|efk)"\)',
        source_text,
        re.DOTALL,
    ):
        transforms.setdefault(source_extension, runtime_extension)
    if transforms != {"spark": "spk", "efkproj": "efk"}:
        raise ValueError(f"unexpected particle bake transforms: {transforms}")
    return transforms


def _validate_outputs(root: Path, raw: object) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    outputs = copy.deepcopy(raw)
    authored_extensions = _string_list(
        outputs.get("authored_extensions"), "outputs.authored_extensions"
    )
    runtime_extensions = _string_list(
        outputs.get("runtime_extensions"), "outputs.runtime_extensions"
    )
    backend_options = outputs.get("backend_options")
    bake_transforms = outputs.get("bake_transforms")
    if not isinstance(backend_options, dict) or not isinstance(bake_transforms, dict):
        raise ValueError("outputs backend_options and bake_transforms must be objects")

    init_text = (root / "BuildTools/cmake/stages/Init.cmake").read_text(encoding="utf-8")
    project_interface = json.loads(
        (root / "BuildTools/cmake/ProjectInterface.json").read_text(encoding="utf-8")
    )
    declared_options = {
        option.get("name"): option
        for option in project_interface.get("options", [])
        if isinstance(option, dict)
    }
    expected_options = {
        "spark": "FO_SPARK_PARTICLES",
        "effekseer": "FO_EFFEKSEER_PARTICLES",
    }
    for option in expected_options.values():
        declaration = declared_options.get(option)
        if (
            not isinstance(declaration, dict)
            or declaration.get("cache_type") != "BOOL"
            or declaration.get("default") != "OFF"
            or f"{option}=$<BOOL:${{{option}}}>" not in init_text
        ):
            raise ValueError(f"unable to derive enabled CMake particle option: {option}")

    spark_text = (root / "Source/Client/SparkExtension.cpp").read_text(encoding="utf-8")
    effekseer_text = (root / "Source/Client/EffekseerExtension.cpp").read_text(encoding="utf-8")
    expected_runtime = [
        _derive_runtime_extension(spark_text, "SparkParticleRuntimeBackend"),
        _derive_runtime_extension(effekseer_text, "EffekseerParticleRuntimeBackend"),
    ]
    baker_text = (root / "Source/Tools/ParticleBaker.cpp").read_text(encoding="utf-8")
    expected_transforms = _derive_bake_transforms(baker_text)

    if authored_extensions != list(expected_transforms):
        raise ValueError(
            f"outputs.authored_extensions must be {list(expected_transforms)}"
        )
    if runtime_extensions != expected_runtime:
        raise ValueError(f"outputs.runtime_extensions must be {expected_runtime}")
    if backend_options != expected_options:
        raise ValueError(f"outputs.backend_options must be {expected_options}")
    if bake_transforms != expected_transforms:
        raise ValueError(f"outputs.bake_transforms must be {expected_transforms}")
    if outputs.get("runtime_side") != "client":
        raise ValueError("outputs.runtime_side must be client")
    if outputs.get("authored_binaries_rejected") is not True:
        raise ValueError("outputs.authored_binaries_rejected must be true")
    _required_string(outputs.get("spark_runtime_magic"), "outputs.spark_runtime_magic")
    _required_string(
        outputs.get("effekseer_runtime_magic"), "outputs.effekseer_runtime_magic"
    )
    return outputs


def _validate_entries(
    root: Path, collection: str, kind: str, raw_entries: object
) -> list[dict[str, object]]:
    if not isinstance(raw_entries, list) or not raw_entries:
        raise ValueError(f"{collection} must be a non-empty array")
    result: list[dict[str, object]] = []
    for index, raw in enumerate(raw_entries):
        label = f"{collection}[{index}]"
        if not isinstance(raw, dict):
            raise ValueError(f"{label} must be an object")
        entry = copy.deepcopy(raw)
        identity = _required_string(entry.get("id"), f"{label}.id")
        if not ENTRY_ID_PATTERN.fullmatch(identity):
            raise ValueError(f"{label}.id is invalid: {identity}")
        if not identity.startswith(f"particle-format.{kind}."):
            raise ValueError(f"{label}.id must use particle-format.{kind} prefix")
        _required_string(entry.get("name"), f"{label}.name")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported stability for {identity}: {stability}")
        _required_string(entry.get("requirement"), f"{label}.requirement")
        _required_string(entry.get("rationale"), f"{label}.rationale")
        entry["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        if collection == "object_families":
            entry["members"] = _string_list(entry.get("members"), f"{label}.members")
        elif collection == "objects":
            is_resource_form = "format" in entry
            required_fields = (
                ("format", "role", "backend")
                if is_resource_form
                else ("class_name", "xml_tag", "family")
            )
            for field in required_fields:
                _required_string(entry.get(field), f"{label}.{field}")
            if not is_resource_form:
                descriptor_parents = entry.get("descriptor_parents")
                if descriptor_parents is not None:
                    _string_list(
                        descriptor_parents,
                        f"{label}.descriptor_parents",
                        allow_empty=True,
                    )
                declared_attributes = entry.get("declared_attributes")
                if declared_attributes is not None and not isinstance(
                    declared_attributes, list
                ):
                    raise ValueError(
                        f"{label}.declared_attributes must be an array"
                    )
        elif collection == "renderer_fields":
            _required_string(entry.get("default"), f"{label}.default")
            if "backend" in entry or "field" in entry:
                for field in ("backend", "field"):
                    _required_string(entry.get(field), f"{label}.{field}")
            else:
                for field in ("attribute", "attribute_type"):
                    _required_string(entry.get(field), f"{label}.{field}")
        result.append(entry)
    return result


def generate_particle_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise ValueError("particle format manifest root must be an object")
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("unsupported particle format schema version")

    _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _string_list(raw.get("sources"), "sources")
    sources = [
        _source_path(root, path, f"sources[{index}]")
        for index, path in enumerate(sources)
    ]
    outputs = _validate_outputs(root, raw.get("outputs"))
    collections = {
        collection: _validate_entries(root, collection, kind, raw.get(collection))
        for collection, kind in COLLECTION_KINDS.items()
    }
    identities = [
        entry["id"] for entries in collections.values() for entry in entries
    ]
    if len(identities) != len(set(identities)):
        raise ValueError("particle format entry IDs must be unique")

    stability_counts = Counter(
        str(entry["stability"])
        for entries in collections.values()
        for entry in entries
    )
    resource_forms = [
        entry for entry in collections["objects"] if "format" in entry
    ]
    graph_objects = [
        entry for entry in collections["objects"] if "xml_tag" in entry
    ]
    backends = [
        entry
        for entry in collections["object_families"]
        if any(str(member).startswith(".") for member in entry["members"])
    ]
    summary = {
        "entry_count": len(identities),
        "backend_count": len(backends),
        "format_count": len(resource_forms),
        "graph_object_count": len(graph_objects),
        "xml_rule_count": len(collections["xml_rules"]),
        "renderer_field_count": len(collections["renderer_fields"]),
        "tooling_rule_count": len(collections["tooling_rules"]),
        "runtime_rule_count": len(collections["runtime_rules"]),
        "integration_rule_count": len(collections["integration_rules"]),
        "validation_rule_count": len(collections["validation_rules"]),
        "stability_counts": dict(sorted(stability_counts.items())),
    }
    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": raw["description"],
        "scope": scope,
        "sources": sources,
        "outputs": outputs,
        **collections,
        "summary": summary,
    }
    digest_payload = json.dumps(
        model, ensure_ascii=True, sort_keys=True, separators=(",", ":")
    )
    model["contract_digest"] = hashlib.sha256(
        digest_payload.encode("utf-8")
    ).hexdigest()
    return model


def render_particle_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return (
        json.dumps(
            generate_particle_format_model(root, manifest_relative_path),
            ensure_ascii=True,
            indent=2,
        )
        + "\n"
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
        "> Generated reference. Do not edit this page directly. Update "
        "`BuildTools/ParticleFormatInterface.json`, then run "
        "`python BuildTools/docs_particle_format.py --write`.",
        "",
        "[Reference index](index.md) | [Source rules](xml.md) | "
        "[Formats and backends](objects.md) | [Rendering](renderer.md) | "
        "[Tooling](tooling.md) | [Runtime](runtime.md) | "
        "[Integration](integration.md) | [Validation](validation.md) | "
        "[Canonical JSON model](../../../generated/particle-format.json) | "
        "[Guide](../../how-to/content/particle-format.md) | "
        "[Authoring tools](../../how-to/tools/particle-authoring.md)",
        "",
    ]


def _render_legacy_page(
    canonical_path: str,
    title: str,
    canonical_content: str,
) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/particle-format/{filename}"
    russian_path = f"../../ru/reference/particle-format/{filename}"
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


def _source_links(model: dict[str, object], refs: object) -> str:
    assert isinstance(refs, list)
    links: list[str] = []
    for ref in refs:
        assert isinstance(ref, dict)
        path = str(ref["path"])
        url = (
            f"https://github.com/{model['repository']}/blob/"
            f"{model['source_ref']}/{quote(path)}"
        )
        links.append(f"[{docs_cli._text(path)}]({url})")
    return "<br>".join(links)


def _entry_id(entry: dict[str, object]) -> str:
    identity = str(entry["id"])
    return (
        f'<a id="{docs_cli._anchor("entry", identity)}"></a>'
        f"{docs_cli._code(identity)}"
    )


def _rule_rows(
    model: dict[str, object], collection: str
) -> list[tuple[str, str, str, str, str]]:
    return [
        (
            _entry_id(entry),
            docs_cli._text(entry["name"]),
            docs_cli._text(entry["requirement"]),
            docs_cli._text(entry["rationale"]),
            _source_links(model, entry["source"]),
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
    lines.extend(
        [
            "This reference describes the optional SPARK and Effekseer authoring, "
            "baking, runtime, Mapper, integration, and validation contract.",
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
            ("Source manifest", docs_cli._code(model["source_manifest"])),
            ("Contract digest", docs_cli._code(model["contract_digest"])),
            (
                "Authored extensions",
                ", ".join(docs_cli._code(value) for value in outputs["authored_extensions"]),
            ),
            (
                "Runtime extensions",
                ", ".join(docs_cli._code(value) for value in outputs["runtime_extensions"]),
            ),
            ("Runtime side", docs_cli._code(outputs["runtime_side"])),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            ("[Source rules](xml.md)", str(summary["xml_rule_count"]), "Authored XML and dependency boundaries."),
            ("[Formats and backends](objects.md)", str(summary["format_count"]), "Optional backends and source-to-runtime forms."),
            ("[Rendering](renderer.md)", str(summary["renderer_field_count"]), "Backend rendering routes and fields."),
            ("[Tooling](tooling.md)", str(summary["tooling_rule_count"]), "Mapper and standalone authoring workflows."),
            ("[Runtime](runtime.md)", str(summary["runtime_rule_count"]), "Composition, routing, seed, scale, and prewarm."),
            ("[Integration](integration.md)", str(summary["integration_rule_count"]), "Sprite, model, script, and project boundaries."),
            ("[Validation](validation.md)", str(summary["validation_rule_count"]), "Documentation, native, bake, and visible gates."),
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
        _rule_rows(model, collection),
    )
    return "\n".join(lines)


def _render_objects(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    backend_rows = []
    object_family_rows = []
    for entry in model["object_families"]:
        assert isinstance(entry, dict)
        row = (
            _entry_id(entry),
            docs_cli._text(entry["name"]),
            ", ".join(docs_cli._code(value) for value in entry["members"]),
            docs_cli._text(entry["requirement"]),
            _source_links(model, entry["source"]),
        )
        if any(str(member).startswith(".") for member in entry["members"]):
            backend_rows.append(row)
        else:
            object_family_rows.append(row)
    lines.extend(["## Backends", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Backend", "Formats", "Requirement", "Source"),
        backend_rows,
    )
    format_rows = []
    graph_object_rows = []
    for entry in model["objects"]:
        assert isinstance(entry, dict)
        if "format" in entry:
            format_rows.append(
                (
                    _entry_id(entry),
                    docs_cli._code(entry["format"]),
                    docs_cli._code(entry["backend"]),
                    docs_cli._code(entry["role"]),
                    docs_cli._text(entry["requirement"]),
                    _source_links(model, entry["source"]),
                )
            )
        else:
            graph_object_rows.append(
                (
                    _entry_id(entry),
                    docs_cli._code(entry["xml_tag"]),
                    docs_cli._text(entry["family"]),
                    docs_cli._text(entry["requirement"]),
                    _source_links(model, entry["source"]),
                )
            )
    lines.extend(["## Resource forms", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Format", "Backend", "Role", "Requirement", "Source"),
        format_rows,
    )
    lines.extend(["## SPARK object families", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Family", "Object tags", "Requirement", "Source"),
        object_family_rows,
    )
    lines.extend(["## SPARK object catalog", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "XML tag", "Family", "Requirement", "Source"),
        graph_object_rows,
    )
    return "\n".join(lines)


def _render_renderer(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    rows = []
    for entry in model["renderer_fields"]:
        assert isinstance(entry, dict)
        backend = entry.get("backend", "spark")
        field = entry.get("field", entry.get("attribute"))
        rows.append(
            (
                _entry_id(entry),
                docs_cli._code(backend),
                docs_cli._code(field),
                docs_cli._text(entry["default"]),
                docs_cli._text(entry["requirement"]),
                _source_links(model, entry["source"]),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Backend", "Field or route", "Default", "Behavior", "Source"),
        rows,
    )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[7][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Gate", "Requirement", "Why", "Source"),
        _rule_rows(model, "validation_rules"),
    )
    lines.extend(
        [
            "## Validation commands",
            "",
            "```powershell",
            "python BuildTools\\docs_particle_format.py --check",
            "python -m unittest BuildTools.tests.test_docs_particle_format",
            "cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests",
            "```",
            "",
            "An embedding project must also rebake its resources and visibly inspect "
            "every affected backend and integration route.",
            "",
        ]
    )
    return "\n".join(lines)


RUSSIAN_TITLES = {
    "Generated Particle Format Reference": "Сгенерированный справочник форматов частиц",
    "Particle Source Rules": "Правила исходников частиц",
    "Particle Formats and Backends": "Форматы и backend-ы частиц",
    "Particle Rendering Contract": "Контракт отрисовки частиц",
    "Particle Tooling Contract": "Контракт инструментов частиц",
    "Particle Runtime Contract": "Runtime-контракт частиц",
    "Particle Integration Contract": "Контракт интеграции частиц",
    "Particle Validation Contract": "Контракт проверки частиц",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit this page directly. Update `BuildTools/ParticleFormatInterface.json`, then run `python BuildTools/docs_particle_format.py --write`.":
        "> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/ParticleFormatInterface.json`, затем выполните `python BuildTools/docs_particle_format.py --write`.",
    "[Reference index](index.md) | [Source rules](xml.md) | [Formats and backends](objects.md) | [Rendering](renderer.md) | [Tooling](tooling.md) | [Runtime](runtime.md) | [Integration](integration.md) | [Validation](validation.md) | [Canonical JSON model](../../../generated/particle-format.json) | [Guide](../../how-to/content/particle-format.md) | [Authoring tools](../../how-to/tools/particle-authoring.md)":
        "[Индекс справочника](index.md) | [Правила исходников](xml.md) | [Форматы и backend-ы](objects.md) | [Отрисовка](renderer.md) | [Инструменты](tooling.md) | [Runtime](runtime.md) | [Интеграция](integration.md) | [Проверка](validation.md) | [Каноническая JSON-модель](../../../generated/particle-format.json) | [Руководство](../../how-to/content/particle-format.md) | [Инструменты авторинга](../../how-to/tools/particle-authoring.md)",
    "This reference describes the optional SPARK and Effekseer authoring, baking, runtime, Mapper, integration, and validation contract.":
        "Этот справочник описывает контракт необязательных backend-ов SPARK и Effekseer: авторинг, запекание, runtime, Mapper, интеграцию и проверку.",
    "## Contract status": "## Состояние контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный манифест",
    "Contract digest": "Дайджест контракта",
    "Authored extensions": "Авторские расширения",
    "Runtime extensions": "Расширения runtime",
    "Runtime side": "Сторона runtime",
    "Authored XML and dependency boundaries.":
        "Границы авторского XML и зависимостей.",
    "Optional backends and source-to-runtime forms.":
        "Необязательные backend-ы и формы source-to-runtime.",
    "Backend rendering routes and fields.":
        "Маршруты и поля отрисовки backend-ов.",
    "Mapper and standalone authoring workflows.":
        "Процессы авторинга в Mapper и отдельных инструментах.",
    "Composition, routing, seed, scale, and prewarm.":
        "Компоновка, маршрутизация, seed, масштаб и prewarm.",
    "Sprite, model, script, and project boundaries.":
        "Границы спрайтов, моделей, скриптов и проекта.",
    "Documentation, native, bake, and visible gates.":
        "Gate документации, native-кода, запекания и видимой проверки.",
    "## Boundary": "## Граница ответственности",
    "Included:": "Включено:",
    "Excluded:": "Исключено:",
    "## Backends": "## Backend-ы",
    "## Resource forms": "## Формы ресурсов",
    "## SPARK object families": "## Семейства объектов SPARK",
    "## SPARK object catalog": "## Каталог объектов SPARK",
    "## Validation commands": "## Команды проверки",
    "An embedding project must also rebake its resources and visibly inspect every affected backend and integration route.":
        "Подключаемый проект также должен повторно запечь ресурсы и визуально проверить каждый затронутый backend и маршрут интеграции.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Rule | Requirement | Why | Source |":
        "| Стабильный ID | Правило | Требование | Причина | Источник |",
    "| Stable ID | Backend | Formats | Requirement | Source |":
        "| Стабильный ID | Backend | Форматы | Требование | Источник |",
    "| Stable ID | Format | Backend | Role | Requirement | Source |":
        "| Стабильный ID | Формат | Backend | Роль | Требование | Источник |",
    "| Stable ID | Family | Object tags | Requirement | Source |":
        "| Стабильный ID | Семейство | Теги объектов | Требование | Источник |",
    "| Stable ID | XML tag | Family | Requirement | Source |":
        "| Стабильный ID | XML-тег | Семейство | Требование | Источник |",
    "| Stable ID | Backend | Field or route | Default | Behavior | Source |":
        "| Стабильный ID | Backend | Поле или маршрут | По умолчанию | Поведение | Источник |",
    "| Stable ID | Gate | Requirement | Why | Source |":
        "| Стабильный ID | Gate | Требование | Причина | Источник |",
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
        raise ValueError("generated particle format page has no front matter")
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
        raise ValueError("unsupported generated particle format model")
    identities = [
        entry.get("id")
        for collection in COLLECTION_KINDS
        for entry in model.get(collection, [])
        if isinstance(entry, dict)
    ]
    if (
        any(not isinstance(identity, str) or not identity for identity in identities)
        or len(identities) != len(set(identities))
    ):
        raise ValueError("every particle format entry must have a unique non-empty ID")
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/xml.md": _render_rules(model, 1, "xml_rules"),
        f"{DEFAULT_OUTPUT_DIR}/objects.md": _render_objects(model),
        f"{DEFAULT_OUTPUT_DIR}/renderer.md": _render_renderer(model),
        f"{DEFAULT_OUTPUT_DIR}/tooling.md": _render_rules(model, 4, "tooling_rules"),
        f"{DEFAULT_OUTPUT_DIR}/runtime.md": _render_rules(model, 5, "runtime_rules"),
        f"{DEFAULT_OUTPUT_DIR}/integration.md": _render_rules(model, 6, "integration_rules"),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/xml.md": _render_rules(
            localized_model, 1, "xml_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/objects.md": _render_objects(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/renderer.md": _render_renderer(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/tooling.md": _render_rules(
            localized_model, 4, "tooling_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/runtime.md": _render_rules(
            localized_model, 5, "runtime_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/integration.md": _render_rules(
            localized_model, 6, "integration_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(localized_model),
    }
    pages = dict(canonical_pages)
    for filename, document_id, title in PAGE_DEFINITIONS:
        canonical_path = f"{DEFAULT_OUTPUT_DIR}/{filename}"
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
        raise ValueError("generated particle format page set does not match OUTPUT_PATHS")
    return {
        path: content.rstrip() + "\n" for path, content in sorted(pages.items())
    }


def render_reference_pages(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    model = generate_particle_format_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "particle-format",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline particle format model and reference"
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_particle_format_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "particle-format",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(
            f"Unable to generate particle format documentation: {exception}",
            file=sys.stderr,
        )
        return 1
    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote particle format model and {len(pages)} reference pages")
        return 0
    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file()
        or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated particle format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_particle_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated particle format documentation is current: "
        f"{summary['entry_count']} entries, {summary['backend_count']} backends, "
        f"{summary['format_count']} formats"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
