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
DEFAULT_MANIFEST = "BuildTools/TextFormatInterface.json"
DEFAULT_MODEL = "Docs/generated/text-format.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/text-format"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/text-format"
LEGACY_OUTPUT_DIR = "Docs/generated/text-format"
GENERATED_BY = "BuildTools/docs_text_format.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-text-format-index", "Generated Text And Localization Reference"),
    ("syntax.md", "generated-text-format-syntax", "Text Pack Syntax"),
    ("languages.md", "generated-text-format-languages", "Languages And Normalization"),
    ("proto-text.md", "generated-text-format-proto-text", "Prototype Text"),
    ("runtime.md", "generated-text-format-runtime", "Runtime Text API"),
    ("validation.md", "generated-text-format-validation", "Text Format Validation"),
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
    "syntax_rules": "syntax",
    "language_rules": "language",
    "proto_text_rules": "proto",
    "runtime_methods": "runtime",
    "rendering_rules": "rendering",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^text-format\.(syntax|language|proto|runtime|rendering|validation)\."
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
    if not isinstance(raw, dict) or raw.get("surface") != "text-format":
        raise ValueError("scope.surface must be text-format")
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
        "text_pack_header",
        "text_pack",
        "text_baker",
        "proto_text_baker",
        "client_runtime",
        "client_script_api",
        "server_runtime",
        "server_script_api",
        "common_script_api",
        "script_types",
        "settings",
        "renderer",
        "renderer_header",
    ):
        result[field] = _source_path(root, raw.get(field), f"sources.{field}")
    tests = _string_list(raw.get("tests"), "sources.tests")
    result["tests"] = [
        _source_path(root, path, f"sources.tests[{index}]")
        for index, path in enumerate(tests)
    ]
    return result


def _extract_setting_defaults(root: Path, settings_path: str) -> dict[str, object]:
    text = (root / settings_path).read_text(encoding="utf-8")
    bake_match = re.search(
        r'FIXED_SETTING\(vector<string>, Baking, BakeLanguages,\s*(?P<defaults>[^;]+)\);',
        text,
    )
    client_match = re.search(
        r'VARIABLE_SETTING\(string, Client, Language,\s*"(?P<default>[^"]+)"\);',
        text,
    )
    if bake_match is None or client_match is None:
        raise ValueError("unable to derive text language defaults from Settings.inc")
    bake_defaults = re.findall(r'"([^"]+)"', bake_match.group("defaults"))
    if not bake_defaults:
        raise ValueError("Baking.BakeLanguages must declare at least one default")
    return {
        "Baking.BakeLanguages": bake_defaults,
        "Client.Language": client_match.group("default"),
    }


def _extract_proto_pack_names(root: Path, proto_text_baker_path: str) -> list[str]:
    text = (root / proto_text_baker_path).read_text(encoding="utf-8")
    names: list[str] = []
    for name in re.findall(r'empty_lang_pack\.try_emplace\("([^"]+)"', text):
        if name not in names:
            names.append(name)
    if not names:
        raise ValueError("unable to derive generated prototype text-pack names")
    return names


def _validate_outputs(
    root: Path, raw: object, sources: dict[str, object]
) -> tuple[dict[str, object], dict[str, object]]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    result = copy.deepcopy(raw)
    expected = {
        "source_extension": ".fotxt",
        "baked_extension": ".fotxt-bin",
        "source_filename": "<TextPack>.<Language>.fotxt",
        "baked_filename": "<ResourcePack>.<TextPack>.<Language>.fotxt-bin",
        "raw_entry": "{Key1}{Key2}{Text}",
        "raw_key3": "empty",
    }
    for field, value in expected.items():
        if result.get(field) != value:
            raise ValueError(f"outputs.{field} must be {value}")
    prototype_packs = _string_list(
        result.get("prototype_packs"), "outputs.prototype_packs"
    )
    derived_packs = _extract_proto_pack_names(
        root, str(sources["proto_text_baker"])
    )
    if prototype_packs != derived_packs:
        raise ValueError(
            "outputs.prototype_packs diverges from ProtoTextBaker: "
            f"documented={prototype_packs}, source={derived_packs}"
        )
    settings = _extract_setting_defaults(root, str(sources["settings"]))
    return result, settings


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
        entry_id = _required_string(entry.get("id"), f"{label}.id")
        if (
            not ENTRY_ID_PATTERN.fullmatch(entry_id)
            or not entry_id.startswith(f"text-format.{kind}.")
        ):
            raise ValueError(f"invalid {kind} id: {entry_id}")
        if entry_id in identities:
            raise ValueError(f"duplicate text format entry id: {entry_id}")
        identities.add(entry_id)
        _required_string(entry.get("name"), f"{label}.name")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        if collection == "runtime_methods":
            _required_string(entry.get("signature"), f"{label}.signature")
            _string_list(entry.get("sides"), f"{label}.sides")
            _required_string(entry.get("behavior"), f"{label}.behavior")
            _required_string(
                entry.get("missing_behavior"), f"{label}.missing_behavior"
            )
        else:
            _required_string(entry.get("requirement"), f"{label}.requirement")
            _required_string(entry.get("rationale"), f"{label}.rationale")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(
            root, entry.get("source"), f"{label}.source"
        )
        result.append(enriched)
    return result


def generate_text_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read text format manifest {manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"text format manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    outputs, setting_defaults = _validate_outputs(root, raw.get("outputs"), sources)
    identities: set[str] = set()
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
        "setting_defaults": setting_defaults,
        **collections,
        "summary": {
            "entry_count": sum(len(collection) for collection in collections.values()),
            "syntax_rule_count": len(collections["syntax_rules"]),
            "language_rule_count": len(collections["language_rules"]),
            "proto_text_rule_count": len(collections["proto_text_rules"]),
            "runtime_method_count": len(collections["runtime_methods"]),
            "rendering_rule_count": len(collections["rendering_rules"]),
            "validation_rule_count": len(collections["validation_rules"]),
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


def render_text_format_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return (
        json.dumps(
            generate_text_format_model(root, manifest_relative_path),
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
        "`BuildTools/TextFormatInterface.json`, then run "
        "`python BuildTools/docs_text_format.py --write`.",
        "",
        "[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | "
        "[Prototype text](proto-text.md) | [Runtime](runtime.md) | "
        "[Validation](validation.md) | "
        "[Canonical JSON](../../../generated/text-format.json) | "
        "[Guide](../../how-to/content/text-and-localization.md)",
        "",
    ]


def _render_legacy_page(
    canonical_path: str,
    title: str,
    canonical_content: str,
) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/text-format/{filename}"
    russian_path = f"../../ru/reference/text-format/{filename}"
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


def _rule_rows(
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
            "This reference describes the reusable Engine-owned text-pack, language, "
            "prototype-text, runtime lookup, and inline color contract. Concrete game "
            "pack catalogs and formatting lexems remain project-owned.",
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
            ("Source filename", docs_cli._code(outputs["source_filename"])),
            ("Baked filename", docs_cli._code(outputs["baked_filename"])),
            ("Raw entry", docs_cli._code(outputs["raw_entry"])),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            (
                "[Syntax](syntax.md)",
                str(summary["syntax_rule_count"]),
                "Raw brace fields, key identity, multiline text, and variants.",
            ),
            (
                "[Languages](languages.md)",
                str(summary["language_rule_count"]),
                "Filename selection, defaults, rebakes, and normalization.",
            ),
            (
                "[Prototype text](proto-text.md)",
                str(summary["proto_text_rule_count"]),
                "$Text grammar, inheritance, pack routing, and decoding.",
            ),
            (
                "[Runtime](runtime.md)",
                f"{summary['runtime_method_count']} methods / "
                f"{summary['rendering_rule_count']} rendering rules",
                "Script lookup, language switching, server boundary, and color tags.",
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
            "A raw text file supplies the collection through its filename. Each "
            "parsed logical entry supplies Key1, Key2, and Text:",
            "",
            "```text",
            "{Welcome}{}{Welcome to the wasteland.}",
            "{QuestName}{Short}{A difficult choice}",
            "{LongMessage}{}{First line",
            "Second line}",
            "```",
            "",
            "Raw `.fotxt` does not author Key3. Use prototype `$Text` fields when a "
            "prototype-owned key needs both Key2 and Key3.",
            "",
        ]
    )
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _rule_rows(model, "syntax_rules"),
    )
    return "\n".join(lines)


def _render_languages(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    settings = model["setting_defaults"]
    assert isinstance(settings, dict)
    lines.extend(
        [
            "Language fallback is materialized by the bakers. Runtime lookup reads "
            "the selected binary language pack and does not consult the base pack.",
            "",
            "## Engine defaults",
            "",
        ]
    )
    docs_cli._table(
        lines,
        ("Setting", "Source default", "Meaning"),
        [
            (
                "`Baking.BakeLanguages`",
                ", ".join(
                    docs_cli._code(item)
                    for item in settings["Baking.BakeLanguages"]
                ),
                "Ordered output languages; the first is the normalization base.",
            ),
            (
                "`Client.Language`",
                docs_cli._code(settings["Client.Language"]),
                "Initial current-language pack loaded by the client.",
            ),
        ],
    )
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _rule_rows(model, "language_rules"),
    )
    return "\n".join(lines)


def _render_proto_text(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    outputs = model["outputs"]
    assert isinstance(outputs, dict)
    lines.extend(
        [
            "Prototype-localized text is authored inside any valid prototype section:",
            "",
            "```ini",
            "[ProtoItem]",
            "$Name = LaserRifle",
            "$Text engl Name = Laser rifle",
            "$Text engl Desc Short = Compact description",
            "$Text russ Name = Localized name",
            "```",
            "",
            "The complete key is the generated pack, prototype id, optional Key2, "
            "and optional Key3. Omitting the language selects the first configured "
            "BakeLanguages entry.",
            "",
            "## Generated packs",
            "",
            ", ".join(docs_cli._code(name) for name in outputs["prototype_packs"]),
            "",
        ]
    )
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _rule_rows(model, "proto_text_rules"),
    )
    return "\n".join(lines)


def _render_runtime(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[4][1:])
    lines.extend(
        [
            "The generated API reference owns the exact exported signatures. This "
            "page explains selection, missing-data behavior, and side availability.",
            "",
            "## Script methods",
            "",
        ]
    )
    rows = []
    for entry in model["runtime_methods"]:
        assert isinstance(entry, dict)
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
                f'<code>{docs_cli._text(entry["id"])}</code>',
                docs_cli._code(entry["signature"]),
                ", ".join(docs_cli._code(side) for side in entry["sides"]),
                docs_cli._text(entry["behavior"]),
                docs_cli._text(entry["missing_behavior"]),
                _source_ref_links(model, entry["source"]),
            )
        )
    docs_cli._table(
        lines,
        (
            "Stable ID",
            "Signature",
            "Sides",
            "Behavior",
            "Missing or invalid input",
            "Source",
        ),
        rows,
    )
    lines.extend(["## Renderer-owned inline tags", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _rule_rows(model, "rendering_rules"),
    )
    lines.extend(
        [
            "The Engine does not interpret game lexems such as player-name, gender, "
            "argument, nested-text, or random-choice tags. An embedding project that "
            "adds them owns their grammar, tests, diagnostics, and ordering relative "
            "to renderer color tags.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[5][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _rule_rows(model, "validation_rules"),
    )
    lines.extend(
        [
            "## Validation commands",
            "",
            "```powershell",
            "python BuildTools\\docs_text_format.py --check",
            "python -m unittest BuildTools.tests.test_docs_text_format",
            "cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests",
            "```",
            "",
            "In an embedding project, finish with its resource bake, localization "
            "guards, and a visible client check for language switching and formatted "
            "text that the project itself owns.",
            "",
        ]
    )
    return "\n".join(lines)


RUSSIAN_TITLES = {
    "Generated Text And Localization Reference": "Справочник текста и локализации",
    "Text Pack Syntax": "Синтаксис текстового пакета",
    "Languages And Normalization": "Языки и нормализация",
    "Prototype Text": "Текст прототипов",
    "Runtime Text API": "Runtime API текста",
    "Text Format Validation": "Проверка формата текста",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit directly. Update `BuildTools/TextFormatInterface.json`, then run `python BuildTools/docs_text_format.py --write`.":
        "> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/TextFormatInterface.json`, затем выполните `python BuildTools/docs_text_format.py --write`.",
    "[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | [Prototype text](proto-text.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/text-format.json) | [Guide](../../how-to/content/text-and-localization.md)":
        "[Индекс](index.md) | [Синтаксис](syntax.md) | [Языки](languages.md) | [Текст прототипов](proto-text.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/text-format.json) | [Руководство](../../how-to/content/text-and-localization.md)",
    "This reference describes the reusable Engine-owned text-pack, language, prototype-text, runtime lookup, and inline color contract. Concrete game pack catalogs and formatting lexems remain project-owned.":
        "Этот справочник описывает переиспользуемый контракт движка для текстовых пакетов, языков, текста прототипов, runtime lookup и встроенных цветов. Конкретные каталоги игровых пакетов и formatter-ы lexem принадлежат проекту.",
    "## Contract status": "## Состояние контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный manifest",
    "Contract digest": "Дайджест контракта",
    "Source filename": "Имя исходного файла",
    "Baked filename": "Имя запечённого файла",
    "Raw entry": "Исходная запись",
    "[Syntax](syntax.md)": "[Синтаксис](syntax.md)",
    "Raw brace fields, key identity, multiline text, and variants.": "Исходные поля в фигурных скобках, идентичность ключей, многострочный текст и варианты.",
    "[Languages](languages.md)": "[Языки](languages.md)",
    "Filename selection, defaults, rebakes, and normalization.": "Выбор имени файла, значения по умолчанию, повторное запекание и нормализация.",
    "[Prototype text](proto-text.md)": "[Текст прототипов](proto-text.md)",
    "$Text grammar, inheritance, pack routing, and decoding.": "Грамматика `$Text`, наследование, маршрутизация пакетов и декодирование.",
    "Script lookup, language switching, server boundary, and color tags.": "Script lookup, переключение языка, граница сервера и цветовые теги.",
    "## Boundary": "## Граница",
    "Included:": "Включено:",
    "Excluded:": "Исключено:",
    "A raw text file supplies the collection through its filename. Each parsed logical entry supplies Key1, Key2, and Text:":
        "Исходный текстовый файл задаёт коллекцию своим именем. Каждая разобранная логическая запись задаёт Key1, Key2 и Text:",
    "Raw `.fotxt` does not author Key3. Use prototype `$Text` fields when a prototype-owned key needs both Key2 and Key3.":
        "Исходный `.fotxt` не задаёт Key3. Используйте поля `$Text` прототипа, когда принадлежащему прототипу ключу нужны одновременно Key2 и Key3.",
    "Language fallback is materialized by the bakers. Runtime lookup reads the selected binary language pack and does not consult the base pack.":
        "Языковой fallback материализуется baker-ами. Runtime lookup читает выбранный бинарный языковой пакет и не обращается к базовому пакету.",
    "## Engine defaults": "## Значения движка по умолчанию",
    "Ordered output languages; the first is the normalization base.": "Упорядоченные выходные языки; первый служит базой нормализации.",
    "Initial current-language pack loaded by the client.": "Начальный пакет текущего языка, загружаемый клиентом.",
    "Prototype-localized text is authored inside any valid prototype section:":
        "Локализованный текст прототипа задаётся внутри любой допустимой секции прототипа:",
    "The complete key is the generated pack, prototype id, optional Key2, and optional Key3. Omitting the language selects the first configured BakeLanguages entry.":
        "Полный ключ состоит из сгенерированного пакета, id прототипа, необязательного Key2 и необязательного Key3. Если язык опущен, выбирается первый настроенный элемент `BakeLanguages`.",
    "## Generated packs": "## Генерируемые пакеты",
    "The generated API reference owns the exact exported signatures. This page explains selection, missing-data behavior, and side availability.":
        "Точными экспортируемыми сигнатурами владеет сгенерированный справочник API. Эта страница описывает выбор, поведение при отсутствии данных и доступность по сторонам.",
    "## Script methods": "## Script-методы",
    "## Renderer-owned inline tags": "## Встроенные теги рендерера",
    "The Engine does not interpret game lexems such as player-name, gender, argument, nested-text, or random-choice tags. An embedding project that adds them owns their grammar, tests, diagnostics, and ordering relative to renderer color tags.":
        "Движок не интерпретирует игровые lexem наподобие тегов имени игрока, пола, аргумента, вложенного текста или случайного выбора. Подключаемый проект, добавляющий их, владеет их грамматикой, тестами, диагностикой и порядком относительно цветовых тегов рендерера.",
    "## Validation commands": "## Команды проверки",
    "In an embedding project, finish with its resource bake, localization guards, and a visible client check for language switching and formatted text that the project itself owns.":
        "В подключаемом проекте завершите проверку запеканием ресурсов, проектными проверками локализации и видимой проверкой переключения языка и форматированного текста в клиенте.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Rule | Requirement | Why | Source |":
        "| Стабильный ID | Правило | Требование | Причина | Источник |",
    "| Setting | Source default | Meaning |": "| Настройка | Исходное значение по умолчанию | Смысл |",
    "| Stable ID | Signature | Sides | Behavior | Missing or invalid input | Source |":
        "| Стабильный ID | Сигнатура | Стороны | Поведение | Отсутствующие или некорректные данные | Источник |",
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
    content = content.replace(" methods / ", " методов / ").replace(
        " rendering rules", " правил рендеринга"
    )
    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError("generated text format page has no front matter")
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
        raise ValueError("unsupported generated text format model")
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
        raise ValueError("every text format entry must have a unique non-empty ID")
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/syntax.md": _render_syntax(model),
        f"{DEFAULT_OUTPUT_DIR}/languages.md": _render_languages(model),
        f"{DEFAULT_OUTPUT_DIR}/proto-text.md": _render_proto_text(model),
        f"{DEFAULT_OUTPUT_DIR}/runtime.md": _render_runtime(model),
        f"{DEFAULT_OUTPUT_DIR}/validation.md": _render_validation(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/syntax.md": _render_syntax(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/languages.md": _render_languages(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/proto-text.md": _render_proto_text(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/runtime.md": _render_runtime(localized_model),
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
        raise ValueError("generated text format page set does not match OUTPUT_PATHS")
    return {
        path: content.rstrip() + "\n" for path, content in sorted(pages.items())
    }


def render_reference_pages(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    model = generate_text_format_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "text-format",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline text format model and reference"
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
        model_content = render_text_format_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "text-format",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(
            f"Unable to generate text format documentation: {exception}",
            file=sys.stderr,
        )
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote text format model and {len(pages)} reference pages")
        return 0

    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file()
        or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated text format documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_text_format.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated text format documentation is current: "
        f"{summary['entry_count']} entries, "
        f"{summary['runtime_method_count']} runtime methods, "
        f"{summary['validation_rule_count']} validation rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
