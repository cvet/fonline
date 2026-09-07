from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import re
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from urllib.parse import quote

import ai_control_client
import docs_cli
import docs_description_translations
import docs_localization


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/AiControlProtocol.json"
DEFAULT_MODEL = "Docs/generated/ai-control-protocol.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/ai-control-protocol"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/ai-control-protocol"
LEGACY_OUTPUT_DIR = "Docs/generated/ai-control-protocol"
GENERATED_BY = "BuildTools/docs_ai_control_protocol.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-ai-control-protocol-index", "Generated AiControl Protocol Reference"),
    ("wire.md", "generated-ai-control-protocol-wire", "AiControl Wire Contract"),
    ("methods.md", "generated-ai-control-protocol-methods", "AiControl Methods"),
    ("commands-events.md", "generated-ai-control-protocol-commands-events", "AiControl Commands and Events"),
    ("security.md", "generated-ai-control-protocol-security", "AiControl Security Boundary"),
    ("integration-validation.md", "generated-ai-control-protocol-integration-validation", "AiControl Integration and Validation"),
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
    "wire_rules": "wire",
    "methods": "method",
    "error_codes": "error",
    "command_fields": "command",
    "security_rules": "security",
    "integration_rules": "integration",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^ai-control-protocol\.(wire|method|error|command|security|integration|validation)\."
    r"[A-Za-z0-9][A-Za-z0-9.-]*$"
)
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(value: object, label: str, *, allow_empty: bool = False) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        raise ValueError(f"{label} must be a {'non-empty ' if not allow_empty else ''}array of strings")
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
        text = (root / path).read_text(encoding="utf-8", errors="replace")
        for anchor in anchors:
            if anchor not in text:
                raise ValueError(f"{item_label} anchor is missing from {path}: {anchor}")
        result.append({"path": path, "anchors": anchors})
    return result


def _load_sample_module(root: Path) -> object:
    path = root / "Examples/AiControlSample/ai_control_sample.py"
    spec = importlib.util.spec_from_file_location("docs_ai_control_sample", path)
    if spec is None or spec.loader is None:
        raise ValueError(f"unable to import protocol sample: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _derive_outputs(root: Path) -> dict[str, object]:
    sample = _load_sample_module(root)
    return {
        "protocol_version": ai_control_client.PROTOCOL_VERSION,
        "jsonrpc_version": ai_control_client.JSONRPC_VERSION,
        "default_host": ai_control_client.DEFAULT_HOST,
        "default_port": ai_control_client.DEFAULT_PORT,
        "max_line_bytes": ai_control_client.MAX_LINE_BYTES,
        "methods": list(ai_control_client.METHODS),
        "error_codes": {
            "parse": sample.ERROR_PARSE,
            "invalid_request": sample.ERROR_INVALID_REQUEST,
            "method_not_found": sample.ERROR_METHOD_NOT_FOUND,
            "invalid_params": sample.ERROR_INVALID_PARAMS,
            "unauthorized": sample.ERROR_UNAUTHORIZED,
            "queue_full": sample.ERROR_QUEUE_FULL,
        },
    }


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "ai-control-protocol":
        raise ValueError("scope.surface must be ai-control-protocol")
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


def _validate_entries(
    root: Path, collection: str, raw: object, identities: set[str]
) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError(f"{collection} must be a non-empty array")
    kind = COLLECTION_KINDS[collection]
    result: list[dict[str, object]] = []
    for index, entry in enumerate(raw):
        label = f"{collection}[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        identity = _required_string(entry.get("id"), f"{label}.id")
        if ENTRY_ID_PATTERN.fullmatch(identity) is None or not identity.startswith(
            f"ai-control-protocol.{kind}."
        ):
            raise ValueError(f"invalid {label}.id: {identity}")
        if identity in identities:
            raise ValueError(f"duplicate AiControl protocol entry id: {identity}")
        identities.add(identity)
        _required_string(entry.get("name"), f"{label}.name")
        stability = _required_string(entry.get("stability"), f"{label}.stability")
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        _required_string(entry.get("requirement"), f"{label}.requirement")
        _required_string(entry.get("rationale"), f"{label}.rationale")
        for field in ("params", "result", "value_type"):
            if field in entry:
                _required_string(entry.get(field), f"{label}.{field}")
        if "code" in entry and not isinstance(entry.get("code"), int):
            raise ValueError(f"{label}.code must be an integer")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(root, entry.get("source"), f"{label}.source")
        result.append(enriched)
    return result


def generate_ai_control_protocol_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read AiControl protocol manifest: {exception}") from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"AiControl protocol manifest schema_version must be {SCHEMA_VERSION}")
    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    if not isinstance(raw.get("sources"), dict):
        raise ValueError("sources must be an object")
    sources = {
        field: _source_path(root, raw["sources"].get(field), f"sources.{field}")
        for field in ("reference_client", "sample_server", "sample_smoke", "sample_readme")
    }
    expected_outputs = _derive_outputs(root)
    if raw.get("outputs") != expected_outputs:
        raise ValueError(f"outputs must match the live client and sample: {expected_outputs}")
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
    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": description,
        "scope": scope,
        "sources": sources,
        "outputs": expected_outputs,
        **collections,
        "summary": {
            "entry_count": sum(len(entries) for entries in collections.values()),
            **{f"{collection.removesuffix('s')}_count": len(entries) for collection, entries in collections.items()},
            "entries_by_stability": dict(sorted(stability_counts.items())),
        },
    }
    canonical = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
    return model


def render_ai_control_protocol_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return json.dumps(
        generate_ai_control_protocol_model(root, manifest_relative_path),
        ensure_ascii=False,
        indent=2,
    ) + "\n"


def _source_link(model: dict[str, object], source: str) -> str:
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(source)}"
    return f"[{source}]({url})"


def _source_links(model: dict[str, object], refs: list[dict[str, object]]) -> str:
    return ", ".join(_source_link(model, str(ref["path"])) for ref in refs)


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
        "`BuildTools/AiControlProtocol.json`, then run "
        "`python BuildTools/docs_ai_control_protocol.py --write`.",
        "",
        "[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | "
        "[Commands and events](commands-events.md) | [Security](security.md) | "
        "[Integration and validation](integration-validation.md) | "
        "[Canonical JSON](../../../generated/ai-control-protocol.json) | "
        "[Guide](../../how-to/ai-control-protocol.md)",
        "",
    ]


def _render_legacy_page(
    canonical_path: str,
    title: str,
    canonical_content: str,
) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/ai-control-protocol/{filename}"
    russian_path = f"../../ru/reference/ai-control-protocol/{filename}"
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


def _entry_anchor(entry: dict[str, object]) -> str:
    return (
        f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
        f'<code>{docs_cli._text(entry["id"])}</code>'
    )


def _rule_rows(model: dict[str, object], collection: str) -> list[tuple[str, str, str, str, str]]:
    return [
        (
            _entry_anchor(entry),
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
    lines.extend([
        "This reference defines the Engine-owned, project-neutral envelope for "
        "opt-in AI observation and control bridges. It does not define a game "
        "schema, MCP namespace, listener inside the core runtime, or server-authority bypass.",
        "",
        "## Contract status",
        "",
    ])
    docs_cli._table(lines, ("Field", "Value"), [
        ("Stability", docs_cli._code(scope["stability"])),
        ("Support policy", docs_cli._text(scope["support_note"])),
        ("Protocol version", docs_cli._code(outputs["protocol_version"])),
        ("JSON-RPC marker", docs_cli._code(outputs["jsonrpc_version"])),
        ("Default endpoint", docs_cli._code(f"{outputs['default_host']}:{outputs['default_port']}")),
        ("Maximum JSON payload", docs_cli._code(outputs["max_line_bytes"])),
        ("Stable entries", str(summary["entry_count"])),
        ("Source manifest", _source_link(model, str(model["source_manifest"]))),
        ("Contract digest", docs_cli._code(model["contract_digest"])),
    ])
    docs_cli._table(lines, ("Reference", "Purpose"), [
        ("[Wire](wire.md)", "Framing, envelopes, ordering, and error codes."),
        ("[Methods](methods.md)", "The six transport methods and their results."),
        ("[Commands and events](commands-events.md)", "Common command fields and asynchronous completion."),
        ("[Security](security.md)", "Loopback, tokens, shipping builds, and authority."),
        ("[Integration and validation](integration-validation.md)", "Project ownership and executable evidence."),
    ])
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_wire(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    docs_cli._table(lines, ("Stable ID", "Rule", "Requirement", "Why", "Source"), _rule_rows(model, "wire_rules"))
    lines.extend(["## Error codes", ""])
    rows = []
    for entry in model["error_codes"]:
        assert isinstance(entry, dict)
        rows.append((_entry_anchor(entry), docs_cli._code(entry["code"]), docs_cli._text(entry["name"]), docs_cli._text(entry["requirement"]), _source_links(model, entry["source"])))
    docs_cli._table(lines, ("Stable ID", "Code", "Name", "Use", "Source"), rows)
    return "\n".join(lines)


def _render_methods(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    rows = []
    for entry in model["methods"]:
        assert isinstance(entry, dict)
        rows.append((_entry_anchor(entry), docs_cli._code(entry["name"]), docs_cli._code(entry["params"]), docs_cli._code(entry["result"]), docs_cli._text(entry["requirement"]), _source_links(model, entry["source"])))
    docs_cli._table(lines, ("Stable ID", "Method", "Params", "Result", "Contract", "Source"), rows)
    return "\n".join(lines)


def _render_commands(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    rows = []
    for entry in model["command_fields"]:
        assert isinstance(entry, dict)
        rows.append((_entry_anchor(entry), docs_cli._code(entry["name"]), docs_cli._code(entry["value_type"]), docs_cli._text(entry["requirement"]), _source_links(model, entry["source"])))
    docs_cli._table(lines, ("Stable ID", "Field", "Type", "Contract", "Source"), rows)
    lines.extend([
        "## Completion envelope",
        "",
        "An accepted command returns a `commandSeq`. The project later appends "
        "an event with `type=command_completed`, the same `commandSeq`, a boolean "
        "`success`, and a project-readable `message`. Acceptance never implies success.",
        "",
    ])
    return "\n".join(lines)


def _render_rules(model: dict[str, object], page_index: int, collections: tuple[str, ...]) -> str:
    lines = _header(*PAGE_DEFINITIONS[page_index][1:])
    titles = {
        "security_rules": "Security rules",
        "integration_rules": "Integration rules",
        "validation_rules": "Validation rules",
    }
    for collection in collections:
        lines.extend([f"## {titles[collection]}", ""])
        docs_cli._table(lines, ("Stable ID", "Rule", "Requirement", "Why", "Source"), _rule_rows(model, collection))
    if "validation_rules" in collections:
        lines.extend([
            "## Validation commands",
            "",
            "```powershell",
            "python Examples\\AiControlSample\\run_protocol_smoke.py",
            "python BuildTools\\tests\\test_ai_control_protocol.py",
            "python BuildTools\\docs_ai_control_protocol.py --check",
            "```",
            "",
        ])
    return "\n".join(lines)


RUSSIAN_TITLES = {
    "Generated AiControl Protocol Reference": "Сгенерированный справочник протокола AiControl",
    "AiControl Wire Contract": "Wire-контракт AiControl",
    "AiControl Methods": "Методы AiControl",
    "AiControl Commands and Events": "Команды и события AiControl",
    "AiControl Security Boundary": "Граница безопасности AiControl",
    "AiControl Integration and Validation": "Интеграция и проверка AiControl",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit directly. Update `BuildTools/AiControlProtocol.json`, then run `python BuildTools/docs_ai_control_protocol.py --write`.":
        "> Сгенерированный справочник. Не редактируйте его напрямую. Измените `BuildTools/AiControlProtocol.json`, затем запустите `python BuildTools/docs_ai_control_protocol.py --write`.",
    "[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | [Commands and events](commands-events.md) | [Security](security.md) | [Integration and validation](integration-validation.md) | [Canonical JSON](../../../generated/ai-control-protocol.json) | [Guide](../../how-to/ai-control-protocol.md)":
        "[Обзор](index.md) | [Wire-контракт](wire.md) | [Методы](methods.md) | [Команды и события](commands-events.md) | [Безопасность](security.md) | [Интеграция и проверка](integration-validation.md) | [Канонический JSON](../../../generated/ai-control-protocol.json) | [Руководство](../../how-to/ai-control-protocol.md)",
    "This reference defines the Engine-owned, project-neutral envelope for opt-in AI observation and control bridges. It does not define a game schema, MCP namespace, listener inside the core runtime, or server-authority bypass.":
        "Этот справочник определяет принадлежащий Engine и нейтральный к проекту конверт для опциональных bridges наблюдения и управления ИИ. Он не определяет игровую схему, пространство имён MCP, listener в основном runtime или обход server authority.",
    "## Contract status": "## Состояние контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Protocol version": "Версия протокола",
    "JSON-RPC marker": "Маркер JSON-RPC",
    "Default endpoint": "Endpoint по умолчанию",
    "Maximum JSON payload": "Максимальный JSON payload",
    "Stable entries": "Стабильные элементы",
    "Source manifest": "Манифест-источник",
    "Contract digest": "Digest контракта",
    "[Wire](wire.md)": "[Wire-контракт](wire.md)",
    "Framing, envelopes, ordering, and error codes.":
        "Framing, конверты, порядок и коды ошибок.",
    "[Methods](methods.md)": "[Методы](methods.md)",
    "The six transport methods and their results.":
        "Шесть транспортных методов и их результаты.",
    "[Commands and events](commands-events.md)":
        "[Команды и события](commands-events.md)",
    "Common command fields and asynchronous completion.":
        "Общие поля команд и асинхронное завершение.",
    "[Security](security.md)": "[Безопасность](security.md)",
    "Loopback, tokens, shipping builds, and authority.":
        "Loopback, tokens, shipping-сборки и authority.",
    "[Integration and validation](integration-validation.md)":
        "[Интеграция и проверка](integration-validation.md)",
    "Project ownership and executable evidence.":
        "Ответственность проекта и исполняемые свидетельства.",
    "## Boundary": "## Граница",
    "Included:": "Включено:",
    "Excluded:": "Исключено:",
    "## Error codes": "## Коды ошибок",
    "## Completion envelope": "## Конверт завершения",
    "An accepted command returns a `commandSeq`. The project later appends an event with `type=command_completed`, the same `commandSeq`, a boolean `success`, and a project-readable `message`. Acceptance never implies success.":
        "Принятая команда возвращает `commandSeq`. Позднее проект добавляет событие с `type=command_completed`, тем же `commandSeq`, логическим `success` и понятным проекту `message`. Принятие никогда не означает успех.",
    "## Security rules": "## Правила безопасности",
    "## Integration rules": "## Правила интеграции",
    "## Validation rules": "## Правила проверки",
    "## Validation commands": "## Команды проверки",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Purpose |": "| Справочник | Назначение |",
    "| Stable ID | Rule | Requirement | Why | Source |":
        "| Стабильный ID | Правило | Требование | Обоснование | Источник |",
    "| Stable ID | Code | Name | Use | Source |":
        "| Стабильный ID | Код | Имя | Применение | Источник |",
    "| Stable ID | Method | Params | Result | Contract | Source |":
        "| Стабильный ID | Метод | Параметры | Результат | Контракт | Источник |",
    "| Stable ID | Field | Type | Contract | Source |":
        "| Стабильный ID | Поле | Тип | Контракт | Источник |",
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
        raise ValueError("generated AiControl protocol page has no front matter")
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
        raise ValueError("unsupported generated AiControl protocol model")
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/wire.md": _render_wire(model),
        f"{DEFAULT_OUTPUT_DIR}/methods.md": _render_methods(model),
        f"{DEFAULT_OUTPUT_DIR}/commands-events.md": _render_commands(model),
        f"{DEFAULT_OUTPUT_DIR}/security.md": _render_rules(model, 4, ("security_rules",)),
        f"{DEFAULT_OUTPUT_DIR}/integration-validation.md": _render_rules(model, 5, ("integration_rules", "validation_rules")),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/wire.md": _render_wire(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/methods.md": _render_methods(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/commands-events.md": _render_commands(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/security.md": _render_rules(
            localized_model, 4, ("security_rules",)
        ),
        f"{DEFAULT_OUTPUT_DIR}/integration-validation.md": _render_rules(
            localized_model, 5, ("integration_rules", "validation_rules")
        ),
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
        raise ValueError("generated AiControl protocol page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def render_reference_pages(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    model = generate_ai_control_protocol_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "ai-control-protocol",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline AiControl protocol model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_ai_control_protocol_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "ai-control-protocol",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate AiControl protocol documentation: {exception}", file=sys.stderr)
        return 1
    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote AiControl protocol model and {len(pages)} reference pages")
        return 0
    stale = [
        path for path, content in outputs.items()
        if not (root / path).is_file() or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated AiControl protocol documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_ai_control_protocol.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(f"Generated AiControl protocol documentation is current: {summary['entry_count']} entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
