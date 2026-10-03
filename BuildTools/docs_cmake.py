from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import quote

import docs_description_translations
import docs_localization


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/cmake/ProjectInterface.json"
DEFAULT_MODEL = "Docs/generated/cmake.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/cmake"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/cmake"
LEGACY_OUTPUT_DIR = "Docs/generated/cmake"
GENERATED_BY = "BuildTools/docs_cmake.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-cmake-index", "Generated CMake Project Interface"),
    ("options.md", "generated-cmake-options", "CMake Project Options"),
    ("stages.md", "generated-cmake-stages", "CMake Project Stages and Hooks"),
    ("helpers.md", "generated-cmake-helpers", "CMake Project Helpers"),
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
COMMAND_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
OPTION_PATTERN = re.compile(r"^FO_[A-Z0-9_]+$")
TOKEN_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(value: object, label: str, *, allow_empty: bool = False) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        raise ValueError(f"{label} must be {'an' if allow_empty else 'a non-empty'} array of strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _source_path(root: Path, value: object, label: str) -> str:
    source = _required_string(value, label)
    if "\\" in source:
        raise ValueError(f"{label} must use forward slashes")
    relative = PurePosixPath(source)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative path")
    if not root.joinpath(*relative.parts).is_file():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _validate_scope(raw_scope: object) -> dict[str, object]:
    if not isinstance(raw_scope, dict):
        raise ValueError("scope must be an object")
    if raw_scope.get("surface") != "cmake-project-interface":
        raise ValueError("scope.surface must be cmake-project-interface")
    stability = _required_string(raw_scope.get("stability"), "scope.stability")
    if stability not in {"experimental", "stable", "deprecated", "internal"}:
        raise ValueError(f"unsupported scope.stability: {stability}")
    since = raw_scope.get("since")
    if since is not None and (not isinstance(since, str) or not since.strip()):
        raise ValueError("scope.since must be null or a non-empty string")
    _required_string(raw_scope.get("support_note"), "scope.support_note")
    _string_list(raw_scope.get("included"), "scope.included")
    _string_list(raw_scope.get("excluded"), "scope.excluded")
    return dict(raw_scope)


def _validate_options(raw_options: object) -> list[dict[str, object]]:
    if not isinstance(raw_options, list) or not raw_options:
        raise ValueError("options must be a non-empty array")

    options: list[dict[str, object]] = []
    names: set[str] = set()
    for index, raw_option in enumerate(raw_options):
        label = f"options[{index}]"
        if not isinstance(raw_option, dict):
            raise ValueError(f"{label} must be an object")
        name = _required_string(raw_option.get("name"), f"{label}.name")
        if not OPTION_PATTERN.fullmatch(name):
            raise ValueError(f"{label}.name is not a public FO_ option: {name}")
        if name in names:
            raise ValueError(f"duplicate CMake project option: {name}")
        names.add(name)

        cache_type = _required_string(raw_option.get("cache_type"), f"{label}.cache_type")
        if cache_type not in {"BOOL", "STRING"}:
            raise ValueError(f"unsupported cache type for {name}: {cache_type}")
        value_kind = _required_string(raw_option.get("value_kind"), f"{label}.value_kind")
        category = _required_string(raw_option.get("category"), f"{label}.category")
        if not TOKEN_PATTERN.fullmatch(value_kind) or not TOKEN_PATTERN.fullmatch(category):
            raise ValueError(f"{name} value_kind and category must be lowercase tokens")
        required = raw_option.get("required")
        if not isinstance(required, bool):
            raise ValueError(f"{label}.required must be a boolean")
        default = raw_option.get("default")
        if required:
            if cache_type != "STRING" or default is not None:
                raise ValueError(f"required option {name} must be STRING with a null default")
        elif not isinstance(default, str):
            raise ValueError(f"optional option {name} must have a string default")
        if cache_type == "BOOL":
            if value_kind != "boolean" or required or default not in {"ON", "OFF"}:
                raise ValueError(f"boolean option {name} must be optional with an ON or OFF default")

        allowed_values = raw_option.get("allowed_values")
        if allowed_values is not None:
            values = _string_list(allowed_values, f"{label}.allowed_values")
            if default is not None and default not in values:
                raise ValueError(f"default for {name} is not present in allowed_values")
        _required_string(raw_option.get("description"), f"{label}.description")

        option = dict(raw_option)
        option["id"] = f"cmake.option.{name}"
        options.append(option)
    return options


def _validate_stages(root: Path, raw_stages: object) -> list[dict[str, object]]:
    if not isinstance(raw_stages, list) or not raw_stages:
        raise ValueError("stages must be a non-empty array")

    stages: list[dict[str, object]] = []
    names: set[str] = set()
    entrypoints: set[str] = set()
    for index, raw_stage in enumerate(raw_stages):
        label = f"stages[{index}]"
        if not isinstance(raw_stage, dict):
            raise ValueError(f"{label} must be an object")
        order = raw_stage.get("order")
        if not isinstance(order, int) or isinstance(order, bool) or order != index + 1:
            raise ValueError(f"{label}.order must be the contiguous value {index + 1}")
        name = _required_string(raw_stage.get("name"), f"{label}.name")
        entrypoint = _required_string(raw_stage.get("entrypoint"), f"{label}.entrypoint")
        if not COMMAND_PATTERN.fullmatch(name) or not COMMAND_PATTERN.fullmatch(entrypoint):
            raise ValueError(f"{label} name and entrypoint must be valid CMake command identifiers")
        if name in names:
            raise ValueError(f"duplicate CMake project stage: {name}")
        if entrypoint in entrypoints:
            raise ValueError(f"duplicate CMake project stage entrypoint: {entrypoint}")
        names.add(name)
        entrypoints.add(entrypoint)
        hooks = _string_list(raw_stage.get("hooks"), f"{label}.hooks")
        if any(hook not in {"Pre", "Post"} for hook in hooks):
            raise ValueError(f"unsupported hook point for stage {name}")
        _source_path(root, raw_stage.get("source"), f"{label}.source")
        _required_string(raw_stage.get("description"), f"{label}.description")

        stage = dict(raw_stage)
        stage["id"] = f"cmake.stage.{name}"
        stages.append(stage)
    return stages


def _validate_helpers(root: Path, raw_helpers: object) -> list[dict[str, object]]:
    if not isinstance(raw_helpers, list) or not raw_helpers:
        raise ValueError("helpers must be a non-empty array")

    helpers: list[dict[str, object]] = []
    names: set[str] = set()
    for index, raw_helper in enumerate(raw_helpers):
        label = f"helpers[{index}]"
        if not isinstance(raw_helper, dict):
            raise ValueError(f"{label} must be an object")
        name = _required_string(raw_helper.get("name"), f"{label}.name")
        if not COMMAND_PATTERN.fullmatch(name):
            raise ValueError(f"{label}.name must be a valid CMake command identifier")
        if name in names:
            raise ValueError(f"duplicate CMake project helper: {name}")
        names.add(name)
        kind = _required_string(raw_helper.get("kind"), f"{label}.kind")
        if kind not in {"function", "macro"}:
            raise ValueError(f"unsupported helper kind for {name}: {kind}")
        signature = _required_string(raw_helper.get("signature"), f"{label}.signature")
        if not signature.startswith(f"{name}("):
            raise ValueError(f"helper signature must start with {name}(")
        _source_path(root, raw_helper.get("source"), f"{label}.source")
        _required_string(raw_helper.get("description"), f"{label}.description")
        allowed_roles = raw_helper.get("allowed_roles")
        if allowed_roles is not None:
            _string_list(allowed_roles, f"{label}.allowed_roles")

        helper = dict(raw_helper)
        helper["id"] = f"cmake.helper.{name}"
        helpers.append(helper)
    return helpers


def generate_cmake_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise ValueError("CMake project interface manifest root must be an object")
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("Unsupported CMake project interface schema version")

    scope = _validate_scope(raw.get("scope"))
    precedence = _string_list(raw.get("option_override_precedence"), "option_override_precedence")
    options = _validate_options(raw.get("options"))
    stages = _validate_stages(root, raw.get("stages"))
    helpers = _validate_helpers(root, raw.get("helpers"))
    option_categories = Counter(str(option["category"]) for option in options)
    option_types = Counter(str(option["cache_type"]) for option in options)

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "scope": scope,
        "option_override_precedence": precedence,
        "summary": {
            "option_count": len(options),
            "required_option_count": sum(option["required"] is True for option in options),
            "optional_option_count": sum(option["required"] is False for option in options),
            "options_by_cache_type": dict(sorted(option_types.items())),
            "options_by_category": dict(sorted(option_categories.items())),
            "stage_count": len(stages),
            "helper_count": len(helpers),
        },
        "options": options,
        "stages": stages,
        "helpers": helpers,
    }


def render_cmake_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> str:
    return json.dumps(generate_cmake_model(root, manifest_relative_path), indent=2, ensure_ascii=True) + "\n"


def _text(value: object) -> str:
    result = html.escape(str(value), quote=True)
    result = result.replace("|", "&#124;").replace("{", "&#123;").replace("}", "&#125;")
    return result.replace("\r\n", "<br>").replace("\r", "<br>").replace("\n", "<br>")


def _code(value: object) -> str:
    return f"<code>{_text(value)}</code>"


def _anchor(prefix: str, identity: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", identity.lower()).strip("-")[:72] or "entry"
    digest = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:10]
    return f"{prefix}-{slug}-{digest}"


def _entry_id(entry: dict[str, object]) -> str:
    identity = str(entry["id"])
    return f'<a id="{_anchor("entry", identity)}"></a>{_code(identity)}'


def _source_link(model: dict[str, object], source: object) -> str:
    path = _required_string(source, "generated source path")
    repository = quote(str(model["repository"]), safe="/")
    source_ref = quote(str(model["source_ref"]), safe="")
    source_path = quote(path, safe="/")
    return f"[{_text(path)}](https://github.com/{repository}/blob/{source_ref}/{source_path})"


def _front_matter(document_id: str, title: str) -> list[str]:
    return [
        "---",
        f"title: {title}",
        f"document_id: {document_id}",
        "locale: en",
        "generated: true",
        "---",
        "",
    ]


def _page_header(document_id: str, title: str) -> list[str]:
    lines = _front_matter(document_id, title)
    lines.extend(
        [
            f"# {title}",
            "",
            "> Generated reference. Do not edit this page directly. Update "
            "`BuildTools/cmake/ProjectInterface.json`, then run `python BuildTools/docs_cmake.py --write`.",
            "",
            "[Reference index](index.md) | "
            "[Canonical JSON model](../../../generated/cmake.json) | "
            "[Generation contract](../metadata/)",
            "",
        ]
    )
    return lines


def _render_legacy_page(canonical_path: str, title: str, canonical_content: str) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/cmake/{filename}"
    russian_path = f"../../ru/reference/cmake/{filename}"
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


def _table(lines: list[str], headers: tuple[str, ...], rows: list[tuple[str, ...]]) -> None:
    lines.append("| " + " | ".join(headers) + " |")
    lines.append("| " + " | ".join("---" for _ in headers) + " |")
    lines.extend("| " + " | ".join(row) + " |" for row in rows)
    lines.append("")


def _render_index(model: dict[str, object]) -> str:
    scope = model["scope"]
    summary = model["summary"]
    assert isinstance(scope, dict) and isinstance(summary, dict)
    lines = _page_header(*PAGE_DEFINITIONS[0][1:])
    lines.extend(
        [
            "This reference describes the project-facing CMake surface consumed by an embedding game repository. "
            "The manifest is a checked documentation model of the current CMake declarations; the implementation "
            "in `BuildTools/Init.cmake` and the stage/helper files remains authoritative at configure time.",
            "",
            "## Contract status",
            "",
        ]
    )
    _table(
        lines,
        ("Field", "Value"),
        [
            ("Stability", _code(scope["stability"])),
            ("Since", _code(scope["since"]) if scope.get("since") is not None else "Not declared"),
            ("Support policy", _text(scope["support_note"])),
            ("Source manifest", _source_link(model, model["source_manifest"])),
        ],
    )
    lines.extend(["## Coverage", ""])
    _table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            (
                "[Project options](options.md)",
                str(summary["option_count"]),
                "Required inputs, defaults, and override precedence.",
            ),
            (
                "[Stages and hooks](stages.md)",
                str(summary["stage_count"]),
                "Strict project-generation order and extension boundaries.",
            ),
            (
                "[Project helpers](helpers.md)",
                str(summary["helper_count"]),
                "Selected commands intended for embedding projects.",
            ),
        ],
    )
    lines.extend(["## Option override precedence", ""])
    precedence = model["option_override_precedence"]
    assert isinstance(precedence, list)
    lines.extend(f"{index}. {_text(value)}" for index, value in enumerate(precedence, start=1))
    lines.extend(
        [
            "",
            "The first defined source wins. Required options have no interface default and must be supplied by "
            "the project.",
            "",
        ]
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {_text(item)}" for item in scope["included"])
    lines.extend(["", "Excluded from this slice:", ""])
    lines.extend(f"- {_text(item)}" for item in scope["excluded"])
    lines.extend(
        [
            "",
            "Package declarations and payloads are documented by the separate "
            "[package interface reference](../packages/index.md). The main BuildTools command line is documented by "
            "the separate [parser-backed CLI reference](../buildtools/index.md).",
            "",
        ]
    )
    return "\n".join(lines)


def _render_options(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[1][1:])
    lines.extend(
        [
            "Options are declared during `StartProjectGeneration()` from the interface manifest. "
            "All values are CMake cache values after declaration.",
            "",
            "See the [reference index](index.md#option-override-precedence) for override order.",
            "",
        ]
    )
    rows: list[tuple[str, ...]] = []
    options = model["options"]
    assert isinstance(options, list)
    for option in options:
        assert isinstance(option, dict)
        default = "Required" if option["default"] is None else _code(option["default"])
        allowed_values = option.get("allowed_values")
        allowed = ", ".join(_code(value) for value in allowed_values) if isinstance(allowed_values, list) else "-"
        rows.append(
            (
                _entry_id(option),
                _code(option["name"]),
                f"{_code(option['cache_type'])} / {_code(option['value_kind'])}",
                "Yes" if option["required"] else "No",
                default,
                allowed,
                _code(option["category"]),
                _text(option["description"]),
            )
        )
    _table(
        lines,
        ("Stable ID", "Option", "Type / value", "Required", "Default", "Allowed", "Category", "Description"),
        rows,
    )
    return "\n".join(lines)


def _render_stages(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[2][1:])
    lines.extend(
        [
            "Embedding projects must call every entrypoint exactly once in the order below. Calling a stage twice, "
            "calling it out of order, or skipping a predecessor aborts CMake configure.",
            "",
            "Register hooks with `AddStageHook(<stage> <Pre|Post> <macro-name>)` before the target stage executes. "
            "Hooks run in registration order.",
            "",
        ]
    )
    rows: list[tuple[str, ...]] = []
    stages = model["stages"]
    assert isinstance(stages, list)
    for stage in stages:
        assert isinstance(stage, dict)
        hooks = ", ".join(_code(hook) for hook in stage["hooks"])
        rows.append(
            (
                str(stage["order"]),
                _entry_id(stage),
                _code(stage["name"]),
                _code(stage["entrypoint"]),
                hooks,
                _source_link(model, stage["source"]),
                _text(stage["description"]),
            )
        )
    _table(lines, ("Order", "Stable ID", "Stage", "Entrypoint", "Hooks", "Source", "Responsibility"), rows)
    return "\n".join(lines)


def _render_helpers(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[3][1:])
    lines.extend(
        [
            "These commands are the selected project-facing helper surface. Other commands under `BuildTools/cmake` "
            "remain internal unless they are added to the interface manifest.",
            "",
        ]
    )
    rows: list[tuple[str, ...]] = []
    helpers = model["helpers"]
    assert isinstance(helpers, list)
    for helper in helpers:
        assert isinstance(helper, dict)
        roles = helper.get("allowed_roles")
        allowed_roles = ", ".join(_code(role) for role in roles) if isinstance(roles, list) else "-"
        rows.append(
            (
                _entry_id(helper),
                _code(helper["signature"]),
                _code(helper["kind"]),
                allowed_roles,
                _source_link(model, helper["source"]),
                _text(helper["description"]),
            )
        )
    _table(lines, ("Stable ID", "Signature", "Kind", "Allowed roles", "Source", "Description"), rows)
    return "\n".join(lines)


RUSSIAN_TITLES = {
    "Generated CMake Project Interface": "Сгенерированный интерфейс проекта CMake",
    "CMake Project Options": "Параметры проекта CMake",
    "CMake Project Stages and Hooks": "Стадии и hooks проекта CMake",
    "CMake Project Helpers": "Проектные helper-команды CMake",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit this page directly. Update `BuildTools/cmake/ProjectInterface.json`, then run `python BuildTools/docs_cmake.py --write`.":
        "> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/cmake/ProjectInterface.json`, затем выполните `python BuildTools/docs_cmake.py --write`.",
    "[Reference index](index.md) | [Canonical JSON model](../../../generated/cmake.json) | [Generation contract](../metadata/)":
        "[Индекс справочника](index.md) | [Каноническая JSON-модель](../../../generated/cmake.json) | [Контракт генерации](../metadata/)",
    "This reference describes the project-facing CMake surface consumed by an embedding game repository. The manifest is a checked documentation model of the current CMake declarations; the implementation in `BuildTools/Init.cmake` and the stage/helper files remains authoritative at configure time.":
        "Этот справочник описывает доступную проекту поверхность CMake, которую использует подключающий игровой репозиторий. Manifest является проверяемой документационной моделью текущих объявлений CMake; во время конфигурации авторитетной остаётся реализация в `BuildTools/Init.cmake` и файлах стадий и helper-команд.",
    "## Contract status": "## Статус контракта",
    "Stability": "Стабильность",
    "Since": "Начиная с версии",
    "Not declared": "Не объявлено",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный manifest",
    "## Coverage": "## Покрытие",
    "[Project options](options.md)": "[Параметры проекта](options.md)",
    "Required inputs, defaults, and override precedence.": "Обязательные входы, значения по умолчанию и приоритет переопределений.",
    "[Stages and hooks](stages.md)": "[Стадии и hooks](stages.md)",
    "Strict project-generation order and extension boundaries.": "Строгий порядок генерации проекта и границы расширения.",
    "[Project helpers](helpers.md)": "[Проектные helper-команды](helpers.md)",
    "Selected commands intended for embedding projects.": "Выбранные команды, предназначенные для подключающих проектов.",
    "## Option override precedence": "## Приоритет переопределения параметров",
    "The first defined source wins. Required options have no interface default and must be supplied by the project.":
        "Побеждает первый определённый источник. Обязательные параметры не имеют значения интерфейса по умолчанию и должны быть заданы проектом.",
    "## Boundary": "## Граница",
    "Included:": "Включено:",
    "Excluded from this slice:": "Исключено из этого среза:",
    "Package declarations and payloads are documented by the separate [package interface reference](../packages/index.md). The main BuildTools command line is documented by the separate [parser-backed CLI reference](../buildtools/index.md).":
        "Объявления пакетов и payload описаны в отдельном [справочнике интерфейса пакетов](../packages/index.md). Основная командная строка BuildTools описана в отдельном [справочнике CLI на основе парсера](../buildtools/index.md).",
    "Options are declared during `StartProjectGeneration()` from the interface manifest. All values are CMake cache values after declaration.":
        "Параметры объявляются из manifest интерфейса во время `StartProjectGeneration()`. После объявления все значения являются значениями кеша CMake.",
    "See the [reference index](index.md#option-override-precedence) for override order.":
        "Порядок переопределения приведён в [индексе справочника](index.md#приоритет-переопределения-параметров).",
    "Embedding projects must call every entrypoint exactly once in the order below. Calling a stage twice, calling it out of order, or skipping a predecessor aborts CMake configure.":
        "Подключающие проекты должны вызвать каждую точку входа ровно один раз в указанном ниже порядке. Повторный вызов стадии, нарушение порядка или пропуск предшественника прерывают настройку CMake.",
    "Register hooks with `AddStageHook(<stage> <Pre|Post> <macro-name>)` before the target stage executes. Hooks run in registration order.":
        "Регистрируйте hooks через `AddStageHook(<stage> <Pre|Post> <macro-name>)` до выполнения целевой стадии. Hooks выполняются в порядке регистрации.",
    "These commands are the selected project-facing helper surface. Other commands under `BuildTools/cmake` remain internal unless they are added to the interface manifest.":
        "Эти команды образуют выбранную поверхность helper-команд для проекта. Остальные команды в `BuildTools/cmake` остаются внутренними, пока не будут добавлены в manifest интерфейса.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Option | Type / value | Required | Default | Allowed | Category | Description |":
        "| Стабильный ID | Параметр | Тип / значение | Обязателен | По умолчанию | Допустимые | Категория | Описание |",
    "| Order | Stable ID | Stage | Entrypoint | Hooks | Source | Responsibility |":
        "| Порядок | Стабильный ID | Стадия | Точка входа | Hooks | Источник | Ответственность |",
    "| Stable ID | Signature | Kind | Allowed roles | Source | Description |":
        "| Стабильный ID | Сигнатура | Вид | Допустимые роли | Источник | Описание |",
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
    content = content.replace(" | Yes | ", " | Да | ").replace(" | No | ", " | Нет | ")
    content = content.replace(" | Required | ", " | Обязателен | ")
    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError("generated CMake page has no front matter")
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
        raise ValueError("Unsupported generated CMake model")
    for key in ("options", "stages", "helpers"):
        entries = model.get(key)
        if not isinstance(entries, list) or any(not isinstance(entry, dict) for entry in entries):
            raise ValueError(f"CMake model {key} must be an array of objects")
    ids = [entry.get("id") for key in ("options", "stages", "helpers") for entry in model[key]]
    if any(not isinstance(identity, str) or not identity for identity in ids) or len(ids) != len(set(ids)):
        raise ValueError("Every CMake model entry must have a unique non-empty ID")

    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/options.md": _render_options(model),
        f"{DEFAULT_OUTPUT_DIR}/stages.md": _render_stages(model),
        f"{DEFAULT_OUTPUT_DIR}/helpers.md": _render_helpers(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/options.md": _render_options(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/stages.md": _render_stages(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/helpers.md": _render_helpers(localized_model),
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
            canonical_path, title, canonical_pages[canonical_path]
        )
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("Generated CMake reference page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def render_reference_pages(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, str]:
    model = generate_cmake_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(root, "cmake", model)
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline CMake project-interface model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write the generated JSON model and Markdown pages")
    mode.add_argument("--check", action="store_true", help="fail when generated CMake documentation is stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        model_content = render_cmake_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(root, "cmake", model)
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate CMake project-interface documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote CMake project-interface model and {len(pages)} reference pages")
        return 0

    stale_paths = [
        relative_path
        for relative_path, content in outputs.items()
        if not (root / relative_path).is_file() or (root / relative_path).read_text(encoding="utf-8") != content
    ]
    if stale_paths:
        print(
            "Generated CMake project-interface documentation is missing or stale: "
            + ", ".join(stale_paths)
            + "; run python BuildTools/docs_cmake.py --write",
            file=sys.stderr,
        )
        return 1

    print(f"Generated CMake project-interface documentation is current: {len(model['options'])} options, "
          f"{len(model['stages'])} stages, {len(model['helpers'])} helpers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
