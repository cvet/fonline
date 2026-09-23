from __future__ import annotations

import argparse
import hashlib
import html
import importlib.util
import json
import os
import re
import sys
from contextlib import contextmanager
from pathlib import Path, PurePosixPath
from typing import Any, Iterator
from urllib.parse import quote

import docs_description_translations
import docs_cli
import docs_localization


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/PackageInterface.json"
DEFAULT_SOURCE = "BuildTools/package.py"
DEFAULT_MODEL = "Docs/generated/package.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/packages"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/packages"
LEGACY_OUTPUT_DIR = "Docs/generated/package"
GENERATED_BY = "BuildTools/docs_package.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PROGRAM = "package.py"
HELP_COLUMNS = docs_cli.HELP_COLUMNS
PAGE_DEFINITIONS = (
    ("index.md", "generated-package-index", "Generated Package Interface"),
    ("declaration.md", "generated-package-declaration", "Package Declaration Grammar"),
    ("matrix.md", "generated-package-matrix", "Package Targets, Platforms, and Packs"),
    ("payloads.md", "generated-package-payloads", "Package Payloads and Artifacts"),
    ("cli.md", "generated-package-cli", "Packager Command Line"),
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
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}
VALID_STATUS = {"implemented", "placeholder", "unsupported"}


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
    relative = PurePosixPath(source)
    if "\\" in source or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    if not root.joinpath(*relative.parts).is_file():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "package-interface":
        raise ValueError("scope.surface must be package-interface")
    stability = _required_string(raw.get("stability"), "scope.stability")
    if stability not in VALID_STABILITY:
        raise ValueError(f"unsupported scope.stability: {stability}")
    since = raw.get("since")
    if since is not None and (not isinstance(since, str) or not since.strip()):
        raise ValueError("scope.since must be null or a non-empty string")
    _required_string(raw.get("support_note"), "scope.support_note")
    _string_list(raw.get("included"), "scope.included")
    _string_list(raw.get("excluded"), "scope.excluded")
    return dict(raw)


def _validate_declaration(root: Path, raw: object) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("declaration must be an object")
    command = _required_string(raw.get("command"), "declaration.command")
    _source_path(root, raw.get("source"), "declaration.source")
    _source_path(root, raw.get("consumer"), "declaration.consumer")
    _required_string(raw.get("description"), "declaration.description")
    clauses = raw.get("clauses")
    if not isinstance(clauses, list) or not clauses:
        raise ValueError("declaration.clauses must be a non-empty array")
    clause_names: set[str] = set()
    normalized_clauses: list[dict[str, object]] = []
    for index, clause in enumerate(clauses):
        label = f"declaration.clauses[{index}]"
        if not isinstance(clause, dict):
            raise ValueError(f"{label} must be an object")
        name = _required_string(clause.get("name"), f"{label}.name")
        if name in clause_names:
            raise ValueError(f"duplicate package declaration clause: {name}")
        clause_names.add(name)
        if not isinstance(clause.get("required"), bool) or not isinstance(clause.get("repeatable"), bool):
            raise ValueError(f"{label} required/repeatable must be booleans")
        arguments = clause.get("arguments")
        if not isinstance(arguments, list) or not arguments:
            raise ValueError(f"{label}.arguments must be a non-empty array")
        argument_names: set[str] = set()
        for argument_index, argument in enumerate(arguments):
            argument_label = f"{label}.arguments[{argument_index}]"
            if not isinstance(argument, dict):
                raise ValueError(f"{argument_label} must be an object")
            argument_name = _required_string(argument.get("name"), f"{argument_label}.name")
            if argument_name in argument_names:
                raise ValueError(f"duplicate argument in package clause {name}: {argument_name}")
            argument_names.add(argument_name)
            _required_string(argument.get("value_kind"), f"{argument_label}.value_kind")
            _required_string(argument.get("description"), f"{argument_label}.description")
        _required_string(clause.get("description"), f"{label}.description")
        normalized = dict(clause)
        normalized["id"] = f"package.declaration.{command}.clause.{name}"
        normalized_clauses.append(normalized)
    if clause_names != {"CONFIG", "BINARY"}:
        raise ValueError("package declaration clauses must be CONFIG and BINARY")

    options = raw.get("options")
    if not isinstance(options, list) or not options:
        raise ValueError("declaration.options must be a non-empty array")
    option_names: set[str] = set()
    normalized_options: list[dict[str, object]] = []
    for index, option in enumerate(options):
        label = f"declaration.options[{index}]"
        if not isinstance(option, dict):
            raise ValueError(f"{label} must be an object")
        name = _required_string(option.get("name"), f"{label}.name")
        if name in option_names:
            raise ValueError(f"duplicate package option: {name}")
        option_names.add(name)
        _required_string(option.get("value_kind"), f"{label}.value_kind")
        _required_string(option.get("default_source"), f"{label}.default_source")
        _required_string(option.get("description"), f"{label}.description")
        normalized = dict(option)
        normalized["id"] = f"package.binary-option.{name}"
        normalized_options.append(normalized)

    result = dict(raw)
    result["id"] = f"package.declaration.{command}"
    result["clauses"] = normalized_clauses
    result["options"] = normalized_options
    return result


def _validate_named_entries(raw: object, label: str, id_prefix: str) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError(f"{label} must be a non-empty array")
    names: set[str] = set()
    entries: list[dict[str, object]] = []
    for index, entry in enumerate(raw):
        entry_label = f"{label}[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{entry_label} must be an object")
        name = _required_string(entry.get("name"), f"{entry_label}.name")
        if name in names:
            raise ValueError(f"duplicate {label} name: {name}")
        names.add(name)
        _required_string(entry.get("description"), f"{entry_label}.description")
        normalized = dict(entry)
        normalized["id"] = f"{id_prefix}.{name}"
        entries.append(normalized)
    return entries


@contextmanager
def _stable_argparse_environment() -> Iterator[None]:
    previous_argv0 = sys.argv[0]
    previous_columns = os.environ.get("COLUMNS")
    sys.argv[0] = PROGRAM
    os.environ["COLUMNS"] = str(HELP_COLUMNS)
    try:
        yield
    finally:
        sys.argv[0] = previous_argv0
        if previous_columns is None:
            os.environ.pop("COLUMNS", None)
        else:
            os.environ["COLUMNS"] = previous_columns


def _load_parser(root: Path, source_relative_path: str) -> argparse.ArgumentParser:
    source_path = root / source_relative_path
    if not source_path.is_file():
        raise ValueError(f"package parser source does not exist: {source_relative_path}")
    module_name = "_fonline_docs_package_" + hashlib.sha256(str(source_path.resolve()).encode()).hexdigest()[:16]
    spec = importlib.util.spec_from_file_location(module_name, source_path)
    if spec is None or spec.loader is None:
        raise ValueError(f"unable to load package parser source: {source_relative_path}")
    module = importlib.util.module_from_spec(spec)
    buildtools_path = str(source_path.parent)
    inserted_path = buildtools_path not in sys.path
    if inserted_path:
        sys.path.insert(0, buildtools_path)
    sys.modules[module_name] = module
    try:
        with _stable_argparse_environment():
            spec.loader.exec_module(module)
            factory = getattr(module, "create_parser", None)
            if not callable(factory):
                raise ValueError(f"{source_relative_path} must expose create_parser()")
            parser = factory()
    finally:
        sys.modules.pop(module_name, None)
        if inserted_path:
            sys.path.remove(buildtools_path)
    if not isinstance(parser, argparse.ArgumentParser):
        raise ValueError("package create_parser() must return argparse.ArgumentParser")
    return parser


def _serializable(value: object, label: str) -> object:
    if value is argparse.SUPPRESS:
        return None
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, (list, tuple, range)):
        return [_serializable(item, label) for item in value]
    raise ValueError(f"{label} is not documentation-serializable: {value!r}")


def _action_kind(action: argparse.Action) -> str:
    names = {
        "_StoreAction": "store",
        "_StoreTrueAction": "store_true",
        "_AppendAction": "append",
    }
    return names.get(type(action).__name__, type(action).__name__.removeprefix("_").removesuffix("Action"))


def _argument_model(action: argparse.Action) -> dict[str, object]:
    choices = None if action.choices is None else list(action.choices)
    return {
        "id": f"package.cli.argument.{action.dest}",
        "destination": action.dest,
        "action": _action_kind(action),
        "option_strings": list(action.option_strings),
        "required": docs_cli._is_required(action),
        "nargs": 1 if action.nargs is None else _serializable(action.nargs, f"{action.dest}.nargs"),
        "choices": _serializable(choices, f"{action.dest}.choices"),
        "default": _serializable(action.default, f"{action.dest}.default"),
        "type": getattr(action.type, "__name__", None) if action.type is not None else None,
        "description": None if action.help in {None, argparse.SUPPRESS} else str(action.help),
    }


def _parser_model(parser: argparse.ArgumentParser) -> dict[str, object]:
    arguments = [
        _argument_model(action)
        for action in parser._actions
        if not isinstance(action, argparse._HelpAction) and action.help is not argparse.SUPPRESS
    ]
    with _stable_argparse_environment():
        usage = parser.format_usage().replace("\r\n", "\n").strip()
        help_output = parser.format_help().replace("\r\n", "\n")
    return {
        "program": parser.prog,
        "description": parser.description,
        "usage": usage,
        "help_output": help_output,
        "arguments": arguments,
    }


def generate_package_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("unsupported package interface schema version")

    scope = _validate_scope(raw.get("scope"))
    declaration = _validate_declaration(root, raw.get("declaration"))
    packager = raw.get("packager")
    if not isinstance(packager, dict):
        raise ValueError("packager must be an object")
    source_parser = _source_path(root, packager.get("source"), "packager.source")
    _required_string(packager.get("entrypoint"), "packager.entrypoint")
    _required_string(packager.get("description"), "packager.description")

    targets = _validate_named_entries(raw.get("targets"), "targets", "package.target")
    target_names = {str(entry["name"]) for entry in targets}
    for index, target in enumerate(targets):
        if target.get("resource_mode") not in {"server-and-client", "client", "none"}:
            raise ValueError(f"targets[{index}].resource_mode is invalid")
        _string_list(target.get("required_packs"), f"targets[{index}].required_packs", allow_empty=True)

    platforms = _validate_named_entries(raw.get("platforms"), "platforms", "package.platform")
    platform_names = {str(entry["name"]) for entry in platforms}
    for index, platform in enumerate(platforms):
        if platform.get("status") not in VALID_STATUS:
            raise ValueError(f"platforms[{index}].status is invalid")
        _string_list(platform.get("architectures"), f"platforms[{index}].architectures")
        names = _string_list(platform.get("targets"), f"platforms[{index}].targets")
        if not set(names) <= target_names:
            raise ValueError(f"platforms[{index}].targets contains an unknown target")

    packs = _validate_named_entries(raw.get("packs"), "packs", "package.pack")
    pack_names = {str(entry["name"]) for entry in packs}
    for index, pack in enumerate(packs):
        if pack.get("status") not in VALID_STATUS:
            raise ValueError(f"packs[{index}].status is invalid")
        _required_string(pack.get("category"), f"packs[{index}].category")
        if not isinstance(pack.get("artifact"), bool):
            raise ValueError(f"packs[{index}].artifact must be a boolean")
        allowed_platforms = _string_list(pack.get("platforms"), f"packs[{index}].platforms")
        allowed_targets = _string_list(pack.get("targets"), f"packs[{index}].targets")
        if not set(allowed_platforms) <= platform_names or not set(allowed_targets) <= target_names:
            raise ValueError(f"packs[{index}] references an unknown platform or target")
        if pack["artifact"]:
            _required_string(pack.get("output"), f"packs[{index}].output")
    for index, target in enumerate(targets):
        if not set(target["required_packs"]) <= pack_names:
            raise ValueError(f"targets[{index}].required_packs contains an unknown pack")
    if not any(pack["artifact"] and pack["status"] == "implemented" for pack in packs):
        raise ValueError("package interface must contain an implemented artifact pack")

    payloads = _validate_named_entries(raw.get("payloads"), "payloads", "package.payload")
    if {entry["name"] for entry in payloads} != platform_names:
        raise ValueError("payload names must match platform names")
    for index, payload in enumerate(payloads):
        if payload.get("status") not in VALID_STATUS:
            raise ValueError(f"payloads[{index}].status is invalid")

    parser = _load_parser(root, source_parser)
    cli = _parser_model(parser)
    argument_by_destination = {argument["destination"]: argument for argument in cli["arguments"]}
    if argument_by_destination.get("target", {}).get("choices") != [entry["name"] for entry in targets]:
        raise ValueError("package parser target choices do not match PackageInterface.json")
    if argument_by_destination.get("platform", {}).get("choices") != [entry["name"] for entry in platforms]:
        raise ValueError("package parser platform choices do not match PackageInterface.json")
    if "pack" not in argument_by_destination:
        raise ValueError("package parser must expose the pack argument")

    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "source_parser": source_parser,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "scope": scope,
        "declaration": declaration,
        "packager": dict(packager),
        "targets": targets,
        "platforms": platforms,
        "packs": packs,
        "payloads": payloads,
        "cli": cli,
        "summary": {
            "clause_count": len(declaration["clauses"]),
            "option_count": len(declaration["options"]),
            "target_count": len(targets),
            "platform_count": len(platforms),
            "implemented_platform_count": sum(platform["status"] == "implemented" for platform in platforms),
            "pack_count": len(packs),
            "implemented_pack_count": sum(pack["status"] == "implemented" for pack in packs),
            "artifact_pack_count": sum(pack["artifact"] and pack["status"] == "implemented" for pack in packs),
            "cli_argument_count": len(cli["arguments"]),
        },
    }
    digest_input = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(digest_input.encode("utf-8")).hexdigest()
    return model


def render_package_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> str:
    return json.dumps(generate_package_model(root, manifest_relative_path), ensure_ascii=False, indent=2) + "\n"


def _anchor(identity: str) -> str:
    slug = "-".join(part for part in "".join(c.lower() if c.isalnum() else "-" for c in identity).split("-") if part)
    return f"entry-{slug}-{hashlib.sha256(identity.encode()).hexdigest()[:10]}"


def _text(value: object) -> str:
    if value is None or value == "":
        return "-"
    return html.escape(str(value), quote=False).replace("|", "&#124;").replace("{", "&#123;").replace("}", "&#125;")


def _code(value: object) -> str:
    return f"<code>{_text(value)}</code>"


def _list_code(values: object) -> str:
    return ", ".join(_code(value) for value in values) if isinstance(values, list) and values else "-"


def _entry_id(entry: dict[str, object]) -> str:
    identity = str(entry["id"])
    return f'<a id="{_anchor(identity)}"></a><code>{_text(identity)}</code>'


def _source_link(model: dict[str, object], source: object) -> str:
    path = str(source)
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(path)}"
    return f"[{path}]({url})"


def _table(lines: list[str], headers: tuple[str, ...], rows: list[tuple[str, ...]]) -> None:
    lines.append("| " + " | ".join(headers) + " |")
    lines.append("| " + " | ".join("---" for _ in headers) + " |")
    lines.extend("| " + " | ".join(row) + " |" for row in rows)
    lines.append("")


def _page_header(document_id: str, title: str) -> list[str]:
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
        "> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or "
        "`BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.",
        "",
        "[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | "
        "[Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../../../generated/package.json)",
        "",
    ]


def _render_index(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    summary = model["summary"]
    assert isinstance(scope, dict) and isinstance(summary, dict)
    lines.extend([
        "This reference joins the CMake package declaration with the runtime-consumed packager contract. "
        "It describes engine capabilities, not an embedding project's release matrix.",
        "This generated package interface is the entry point for `DefinePackage` grammar, `package.py` "
        "targets, platforms, pack tokens, and payload effects.",
        "",
        "## Cross-contract decision",
        "",
        "Use this cross-contract sequence:",
        "",
        "1. State the package boundary explicitly: pin an exact Engine revision and use `BuildTools/PackageInterface.json` as the `internal` package contract; do not replace this statement with a generic revision-pinned heading.",
        "2. Treat accepted package.py dimensions and payload effects as capability only. Embedding-project package matrices, release policy, secret provisioning, deployment topology, and installer signing credentials remain excluded and project-owned.",
        "3. Read support separately: `build_gated`, `smoke_gated`, `source_capable`, and `not_in_public_matrix` are distinct states.",
        "4. For each example, report source status, remote visibility/state, observed required checks, exact Engine pin, update-delivery policy, and matching Contract digest together. Only a `published` source with a `public` / `published` remote and `passing` observed checks is publication evidence; a private, reserved, source-staged, planned, or unobserved repository is not.",
        "",
        "## Contract status",
        "",
    ])
    _table(lines, ("Field", "Value"), [
        ("Stability", _code(scope["stability"])),
        ("Since", "Not declared" if scope["since"] is None else _code(scope["since"])),
        ("Support policy", _text(scope["support_note"])),
        ("Manifest", _source_link(model, model["source_manifest"])),
        ("Packager", _source_link(model, model["source_parser"])),
        ("Contract digest", _code(model["contract_digest"])),
    ])
    lines.extend(["## Coverage", ""])
    _table(lines, ("Reference", "Entries", "Purpose"), [
        ("[Declaration](declaration.md)", str(summary["clause_count"]), "CMake clauses and per-binary modifiers."),
        ("[Targets/platforms/packs](matrix.md)", f"{summary['target_count']} / {summary['platform_count']} / {summary['pack_count']}", "Accepted runtime dimensions and support status."),
        ("[Payloads and artifacts](payloads.md)", str(summary["artifact_pack_count"]), "Implemented output-producing pack tokens."),
        ("[Packager CLI](cli.md)", str(summary["cli_argument_count"]), "Exact internal package.py invocation contract."),
    ])
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded from this slice:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


RUSSIAN_TITLES = {
    "Generated Package Interface": "Сгенерированный интерфейс пакетов",
    "Package Declaration Grammar": "Грамматика объявления пакетов",
    "Package Targets, Platforms, and Packs": "Цели, платформы и наборы пакетов",
    "Package Payloads and Artifacts": "Содержимое пакетов и артефакты",
    "Packager Command Line": "Командная строка упаковщика",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or `BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.":
        "> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/PackageInterface.json` или `BuildTools/package.py`, затем выполните `python BuildTools/docs_package.py --write`.",
    "[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | [Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../../../generated/package.json)":
        "[Индекс](index.md) | [Объявление](declaration.md) | [Матрица](matrix.md) | [Содержимое](payloads.md) | [CLI](cli.md) | [Канонический JSON](../../../generated/package.json)",
    "This reference joins the CMake package declaration with the runtime-consumed packager contract. It describes engine capabilities, not an embedding project's release matrix.":
        "Этот справочник связывает объявление пакета CMake с контрактом упаковщика, который используется во время выполнения. Он описывает возможности движка, а не матрицу релиза встраивающего проекта.",
    "This generated package interface is the entry point for `DefinePackage` grammar, `package.py` targets, platforms, pack tokens, and payload effects.":
        "Этот сгенерированный интерфейс пакетов является точкой входа для грамматики `DefinePackage`, целей `package.py`, платформ, pack tokens и эффектов payload.",
    "## Cross-contract decision": "## Решение между контрактами",
    "Use this cross-contract sequence:": "Используйте следующую cross-contract последовательность:",
    "1. State the package boundary explicitly: pin an exact Engine revision and use `BuildTools/PackageInterface.json` as the `internal` package contract; do not replace this statement with a generic revision-pinned heading.":
        "1. Формулируйте package boundary явно: фиксируйте точную ревизию Engine и используйте `BuildTools/PackageInterface.json` как `internal` package contract; не заменяйте это утверждение общим revision-pinned заголовком.",
    "2. Treat accepted package.py dimensions and payload effects as capability only. Embedding-project package matrices, release policy, secret provisioning, deployment topology, and installer signing credentials remain excluded and project-owned.":
        "2. Считайте допустимые dimensions package.py и payload effects только capability. Package matrices встраивающего проекта, release policy, secret provisioning, deployment topology и credentials подписи установщика остаются исключёнными и принадлежат проекту.",
    "3. Read support separately: `build_gated`, `smoke_gated`, `source_capable`, and `not_in_public_matrix` are distinct states.":
        "3. Рассматривайте support отдельно: `build_gated`, `smoke_gated`, `source_capable` и `not_in_public_matrix` являются разными состояниями.",
    "4. For each example, report source status, remote visibility/state, observed required checks, exact Engine pin, update-delivery policy, and matching Contract digest together. Only a `published` source with a `public` / `published` remote and `passing` observed checks is publication evidence; a private, reserved, source-staged, planned, or unobserved repository is not.":
        "4. Для каждого примера сообщайте вместе source status, visibility/state remote, наблюдённые required checks, точный Engine pin, политику update delivery и соответствующий Contract digest. Свидетельством публикации является только source со статусом `published`, remote `public` / `published` и наблюдёнными checks `passing`; private, reserved, source-staged, planned или unobserved repository им не является.",
    "## Contract status": "## Статус контракта",
    "## Coverage": "## Покрытие",
    "## Boundary": "## Граница ответственности",
    "## Per-binary modifiers": "## Модификаторы отдельного бинарного файла",
    "## Targets": "## Цели",
    "## Platforms": "## Платформы",
    "## Pack tokens": "## Токены наборов",
    "## Platform payloads": "## Содержимое для платформ",
    "## Output-producing packs": "## Наборы, создающие выходные артефакты",
    "Included:": "Включено:",
    "Excluded from this slice:": "Не входит в этот раздел:",
    "`DefinePackage` requires `CONFIG`. Each `BINARY` becomes one `package.py` invocation. `POSTFIX` is optional and belongs only to the immediately preceding `BINARY`; it has no package-wide fallback.":
        "`DefinePackage` требует `CONFIG`. Каждое предложение `BINARY` превращается в отдельный вызов `package.py`. `POSTFIX` необязателен и относится только к непосредственно предшествующему `BINARY`; общего значения для всего пакета нет.",
    "The packager stages one target payload, applies binary/resource transformations, emits artifact packs in a fixed finalization order, and removes the staged directory unless `Raw` retains it.":
        "Упаковщик подготавливает содержимое одной цели, применяет преобразования бинарных файлов и ресурсов, выпускает артефакты в фиксированном порядке завершения и удаляет промежуточный каталог, если токен `Raw` не требует его сохранить.",
    "A package invocation must select at least one implemented output-producing pack. Modifier-only lists and unknown, duplicate, placeholder, unsupported-platform, or target-incompatible tokens fail before output staging.":
        "Вызов упаковщика должен выбрать хотя бы один реализованный набор, создающий выходной артефакт. Списки только из модификаторов, а также неизвестные, повторяющиеся, резервные, неподдерживаемые платформой или несовместимые с целью токены отклоняются до подготовки выходных данных.",
    "CMake normally invokes this internal CLI once for each `BINARY` clause. Direct callers must provide the same build hash, config, input, and output context.":
        "Обычно CMake вызывает этот внутренний CLI один раз для каждого предложения `BINARY`. Прямые вызовы должны передавать тот же хеш сборки, конфигурацию и контекст входных и выходных данных.",
    "Not declared": "Не объявлено",
    "Not documented in the parser.": "Не документировано в parser.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Clause | Arguments | Required | Repeatable | Purpose |":
        "| Стабильный ID | Предложение | Аргументы | Обязательно | Повторяемо | Назначение |",
    "| Stable ID | Modifier | Value | Default | Purpose |":
        "| Стабильный ID | Модификатор | Значение | По умолчанию | Назначение |",
    "| Stable ID | Target | Resources | Required packs | Purpose |":
        "| Стабильный ID | Цель | Ресурсы | Обязательные наборы | Назначение |",
    "| Stable ID | Platform | Status | Architectures | Targets | Payload |":
        "| Стабильный ID | Платформа | Статус | Архитектуры | Цели | Содержимое |",
    "| Stable ID | Pack | Category | Status | Platforms | Targets | Effect |":
        "| Стабильный ID | Набор | Категория | Статус | Платформы | Цели | Эффект |",
    "| Stable ID | Platform | Status | Payload |":
        "| Стабильный ID | Платформа | Статус | Содержимое |",
    "| Stable ID | Pack | Status | Output | Effect |":
        "| Стабильный ID | Набор | Статус | Выход | Эффект |",
    "| Stable ID | Argument | Required | Action | Choices | Default | Description |":
        "| Стабильный ID | Аргумент | Обязательно | Действие | Варианты | По умолчанию | Описание |",
}


def _render_russian_page(
    filename: str,
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
    content = content.replace(" | yes |", " | да |")
    content = content.replace(" | no |", " | нет |")

    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError(f"generated package page has no front matter: {filename}")
    insert_at = front_matter_end + len("\n---\n")
    marker = docs_localization.translation_metadata_line(
        document_id,
        canonical_path,
        docs_localization.normalized_sha256(english_content),
    )
    return content[:insert_at] + "\n" + marker + "\n" + content[insert_at:]


def _render_legacy_page(
    canonical_path: str,
    title: str,
    canonical_content: str,
) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/packages/{filename}"
    russian_path = f"../../ru/reference/packages/{filename}"
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


def _render_declaration(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[1][1:])
    declaration = model["declaration"]
    assert isinstance(declaration, dict)
    lines.extend([
        str(declaration["description"]),
        "",
        f"Stable ID: `{declaration['id']}`",
        "",
        f"Source: {_source_link(model, declaration['source'])}; consumer: {_source_link(model, declaration['consumer'])}.",
        "",
        "```cmake",
        "DefinePackage(<name>",
        "    CONFIG <config>",
        "    BINARY <target> <platform> <arch[+arch...]> <pack[+pack...]> [POSTFIX <value>]",
        "    [BINARY ...]",
        ")",
        "```",
        "",
    ])
    rows = []
    for clause in declaration["clauses"]:
        arguments = " ".join(f"<{argument['name']}>" for argument in clause["arguments"])
        rows.append((_entry_id(clause), _code(clause["name"]), _code(arguments), "yes" if clause["required"] else "no", "yes" if clause["repeatable"] else "no", _text(clause["description"])))
    _table(lines, ("Stable ID", "Clause", "Arguments", "Required", "Repeatable", "Purpose"), rows)
    lines.extend(["## Per-binary modifiers", ""])
    option_rows = [(_entry_id(option), _code(option["name"]), _code(option["value_kind"]), _code(option["default_source"]), _text(option["description"])) for option in declaration["options"]]
    _table(lines, ("Stable ID", "Modifier", "Value", "Default", "Purpose"), option_rows)
    lines.extend([
        "`DefinePackage` requires `CONFIG`. Each `BINARY` becomes one `package.py` invocation. `POSTFIX` is "
        "optional and belongs only to the immediately preceding `BINARY`; it has no package-wide fallback.",
        "",
    ])
    return "\n".join(lines)


def _render_matrix(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[2][1:])
    lines.extend(["## Targets", ""])
    _table(lines, ("Stable ID", "Target", "Resources", "Required packs", "Purpose"), [
        (_entry_id(entry), _code(entry["name"]), _code(entry["resource_mode"]), _list_code(entry["required_packs"]), _text(entry["description"])) for entry in model["targets"]
    ])
    lines.extend(["## Platforms", ""])
    _table(lines, ("Stable ID", "Platform", "Status", "Architectures", "Targets", "Payload"), [
        (_entry_id(entry), _code(entry["name"]), _code(entry["status"]), _list_code(entry["architectures"]), _list_code(entry["targets"]), _text(entry["description"])) for entry in model["platforms"]
    ])
    lines.extend(["## Pack tokens", ""])
    _table(lines, ("Stable ID", "Pack", "Category", "Status", "Platforms", "Targets", "Effect"), [
        (_entry_id(entry), _code(entry["name"]), _code(entry["category"]), _code(entry["status"]), _list_code(entry["platforms"]), _list_code(entry["targets"]), _text(entry["description"])) for entry in model["packs"]
    ])
    return "\n".join(lines)


def _render_payloads(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[3][1:])
    lines.extend([
        "The packager stages one target payload, applies binary/resource transformations, emits artifact packs in "
        "a fixed finalization order, and removes the staged directory unless `Raw` retains it.",
        "",
        "## Platform payloads",
        "",
    ])
    _table(lines, ("Stable ID", "Platform", "Status", "Payload"), [
        (_entry_id(entry), _code(entry["name"]), _code(entry["status"]), _text(entry["description"])) for entry in model["payloads"]
    ])
    lines.extend(["## Output-producing packs", ""])
    artifact_packs = [entry for entry in model["packs"] if entry["artifact"]]
    _table(lines, ("Stable ID", "Pack", "Status", "Output", "Effect"), [
        (_entry_id(entry), _code(entry["name"]), _code(entry["status"]), _code(entry["output"]), _text(entry["description"])) for entry in artifact_packs
    ])
    lines.extend([
        "A package invocation must select at least one implemented output-producing pack. Modifier-only lists and "
        "unknown, duplicate, placeholder, unsupported-platform, or target-incompatible tokens fail before output staging.",
        "",
    ])
    return "\n".join(lines)


def _render_cli(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[4][1:])
    cli = model["cli"]
    assert isinstance(cli, dict)
    lines.extend([
        "CMake normally invokes this internal CLI once for each `BINARY` clause. Direct callers must provide the "
        "same build hash, config, input, and output context.",
        "",
        "```text",
        str(cli["help_output"]).rstrip(),
        "```",
        "",
    ])
    rows = []
    for argument in cli["arguments"]:
        names = ", ".join(_code(name) for name in argument["option_strings"])
        choices = _list_code(argument["choices"])
        default_value = argument["default"]
        if isinstance(default_value, bool):
            default = _code(str(default_value).lower())
        elif default_value is None or default_value == []:
            default = "-"
        else:
            default = _code(default_value)
        rows.append((_entry_id(argument), names, "yes" if argument["required"] else "no", _code(argument["action"]), choices, default, _text(argument["description"] or "Not documented in the parser.")))
    _table(lines, ("Stable ID", "Argument", "Required", "Action", "Choices", "Default", "Description"), rows)
    return "\n".join(lines)


def generate_reference_pages(
    model: dict[str, object],
    russian_model: dict[str, object] | None = None,
) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("unsupported generated package model")
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/declaration.md": _render_declaration(model),
        f"{DEFAULT_OUTPUT_DIR}/matrix.md": _render_matrix(model),
        f"{DEFAULT_OUTPUT_DIR}/payloads.md": _render_payloads(model),
        f"{DEFAULT_OUTPUT_DIR}/cli.md": _render_cli(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/declaration.md": _render_declaration(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/matrix.md": _render_matrix(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/payloads.md": _render_payloads(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/cli.md": _render_cli(localized_model),
    }
    pages = dict(canonical_pages)
    definitions = {
        filename: (document_id, title)
        for filename, document_id, title in PAGE_DEFINITIONS
    }
    for canonical_path, content in canonical_pages.items():
        filename = PurePosixPath(canonical_path).name
        document_id, title = definitions[filename]
        pages[f"{RUSSIAN_OUTPUT_DIR}/{filename}"] = _render_russian_page(
            filename,
            document_id,
            canonical_path,
            content,
            russian_base_pages[canonical_path],
        )
        pages[f"{LEGACY_OUTPUT_DIR}/{filename}"] = _render_legacy_page(
            canonical_path,
            title,
            content,
        )
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated package reference page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def render_reference_pages(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, str]:
    model = generate_package_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "package",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline package interface model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        model_content = render_package_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "package",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate package interface documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote package interface model and {len(pages)} reference pages")
        return 0

    stale = [path for path, content in outputs.items() if not (root / path).is_file() or (root / path).read_text(encoding="utf-8") != content]
    if stale:
        print("Generated package interface documentation is missing or stale: " + ", ".join(stale) + "; run python BuildTools/docs_package.py --write", file=sys.stderr)
        return 1
    summary = model["summary"]
    print(f"Generated package interface documentation is current: {summary['target_count']} targets, {summary['platform_count']} platforms, {summary['pack_count']} packs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
