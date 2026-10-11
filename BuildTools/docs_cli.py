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
import docs_localization


SCHEMA_VERSION = 1
DEFAULT_SOURCE = "BuildTools/buildtools.py"
DEFAULT_MODEL = "Docs/generated/cli.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/buildtools"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/buildtools"
LEGACY_OUTPUT_DIR = "Docs/generated/cli"
GENERATED_BY = "BuildTools/docs_cli.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PROGRAM = "buildtools.py"
HELP_COLUMNS = 240
PAGE_DEFINITIONS = (
    ("index.md", "generated-cli-index", "Generated BuildTools CLI Reference"),
    ("commands.md", "generated-cli-commands", "BuildTools Commands"),
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


def _load_parser(root: Path, source_relative_path: str = DEFAULT_SOURCE) -> argparse.ArgumentParser:
    source_path = root / source_relative_path
    if not source_path.is_file():
        raise ValueError(f"BuildTools CLI parser source does not exist: {source_relative_path}")

    module_digest = hashlib.sha256(str(source_path.resolve()).encode("utf-8")).hexdigest()[:16]
    module_name = f"_fonline_docs_cli_source_{module_digest}"
    spec = importlib.util.spec_from_file_location(module_name, source_path)
    if spec is None or spec.loader is None:
        raise ValueError(f"Unable to load BuildTools CLI parser source: {source_relative_path}")
    module = importlib.util.module_from_spec(spec)
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

    if not isinstance(parser, argparse.ArgumentParser):
        raise ValueError("BuildTools create_parser() must return argparse.ArgumentParser")
    return parser


def _subparsers_action(parser: argparse.ArgumentParser) -> argparse._SubParsersAction[Any]:
    actions = [action for action in parser._actions if isinstance(action, argparse._SubParsersAction)]
    if len(actions) != 1:
        raise ValueError("BuildTools CLI parser must contain exactly one top-level subparser action")
    return actions[0]


def _serializable(value: object, label: str) -> object:
    if value is argparse.SUPPRESS:
        return None
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, (list, tuple)):
        return [_serializable(item, label) for item in value]
    raise ValueError(f"{label} is not documentation-serializable: {value!r}")


def _action_kind(action: argparse.Action) -> str:
    names = {
        "_StoreAction": "store",
        "_StoreConstAction": "store_const",
        "_StoreTrueAction": "store_true",
        "_StoreFalseAction": "store_false",
        "_AppendAction": "append",
        "_AppendConstAction": "append_const",
        "_CountAction": "count",
    }
    return names.get(type(action).__name__, type(action).__name__.removeprefix("_").removesuffix("Action"))


def _type_name(action: argparse.Action) -> str | None:
    if action.type is None:
        return None
    name = getattr(action.type, "__name__", None)
    return name if isinstance(name, str) and name else str(action.type)


def _is_required(action: argparse.Action) -> bool:
    if action.option_strings:
        return bool(action.required)
    return action.nargs not in (argparse.OPTIONAL, argparse.ZERO_OR_MORE)


def _argument_model(command_id: str, action: argparse.Action) -> dict[str, object]:
    if not action.dest or action.dest == argparse.SUPPRESS:
        raise ValueError(f"Argument in {command_id} must have a stable destination")
    choices = None if action.choices is None else list(action.choices)
    option_strings = list(action.option_strings)
    metavar = _serializable(action.metavar, f"{command_id}.{action.dest}.metavar")
    return {
        "id": f"{command_id}.argument.{action.dest}",
        "destination": action.dest,
        "kind": "option" if option_strings else "positional",
        "action": _action_kind(action),
        "option_strings": option_strings,
        "metavar": metavar,
        "required": _is_required(action),
        "nargs": 1 if action.nargs is None else _serializable(action.nargs, f"{command_id}.{action.dest}.nargs"),
        "choices": _serializable(choices, f"{command_id}.{action.dest}.choices"),
        "default": _serializable(action.default, f"{command_id}.{action.dest}.default"),
        "type": _type_name(action),
        "description": None if action.help in {None, argparse.SUPPRESS} else str(action.help),
    }


def _documented_actions(parser: argparse.ArgumentParser) -> list[argparse.Action]:
    return [
        action
        for action in parser._actions
        if not isinstance(action, (argparse._HelpAction, argparse._SubParsersAction))
        and action.help is not argparse.SUPPRESS
    ]


def _normalized_help(parser: argparse.ArgumentParser) -> str:
    with _stable_argparse_environment():
        return parser.format_help().replace("\r\n", "\n")


def _normalized_usage(parser: argparse.ArgumentParser) -> str:
    with _stable_argparse_environment():
        return parser.format_usage().replace("\r\n", "\n").strip()


def generate_cli_model(root: Path, source_relative_path: str = DEFAULT_SOURCE) -> dict[str, object]:
    parser = _load_parser(root, source_relative_path)
    subparsers = _subparsers_action(parser)
    choice_help = {
        action.dest: None if action.help is argparse.SUPPRESS else action.help
        for action in subparsers._choices_actions
    }

    commands: list[dict[str, object]] = []
    seen_parsers: set[int] = set()
    for name, command_parser in subparsers.choices.items():
        parser_identity = id(command_parser)
        if parser_identity in seen_parsers:
            raise ValueError("BuildTools CLI parser aliases require an explicit documentation policy")
        seen_parsers.add(parser_identity)
        if any(isinstance(action, argparse._SubParsersAction) for action in command_parser._actions):
            raise ValueError(f"Nested subcommands are not supported yet: {name}")

        command_id = f"cli.buildtools.command.{name}"
        commands.append(
            {
                "id": command_id,
                "name": name,
                "summary": choice_help.get(name),
                "description": command_parser.description,
                "usage": _normalized_usage(command_parser),
                "help_output": _normalized_help(command_parser),
                "arguments": [
                    _argument_model(command_id, action) for action in _documented_actions(command_parser)
                ],
            }
        )

    global_arguments = [
        _argument_model("cli.buildtools", action) for action in _documented_actions(parser)
    ]
    identities = [
        entry["id"]
        for command in commands
        for entry in [command, *command["arguments"]]
    ] + [argument["id"] for argument in global_arguments]
    if len(identities) != len(set(identities)):
        raise ValueError("BuildTools CLI model IDs must be unique")

    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_parser": source_relative_path,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "scope": {
            "surface": "buildtools-cli",
            "stability": "internal",
            "since": None,
            "support_note": "No versioned CLI support line is declared; pin an engine revision in automation.",
            "included": [
                "top-level commands and arguments exposed by BuildTools/buildtools.py create_parser()",
                "argparse defaults, choices, cardinality, descriptions, usage, and --help output",
            ],
            "excluded": [
                "helper-script command lines outside BuildTools/buildtools.py",
                "package.py declaration and payload contracts",
                "validation-target semantics and internal Python helpers",
            ],
        },
        "program": parser.prog,
        "description": parser.description,
        "usage": _normalized_usage(parser),
        "help_output": _normalized_help(parser),
        "global_arguments": global_arguments,
        "commands": commands,
        "summary": {
            "command_count": len(commands),
            "global_argument_count": len(global_arguments),
            "command_argument_count": sum(len(command["arguments"]) for command in commands),
        },
    }
    contract_content = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(contract_content.encode("utf-8")).hexdigest()
    return model


def render_cli_model(root: Path, source_relative_path: str = DEFAULT_SOURCE) -> str:
    return json.dumps(generate_cli_model(root, source_relative_path), ensure_ascii=False, indent=2) + "\n"


def _anchor(kind: str, identity: str) -> str:
    slug = "".join(character.lower() if character.isalnum() else "-" for character in identity).strip("-")
    slug = "-".join(part for part in slug.split("-") if part)
    digest = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:10]
    return f"{kind}-{slug}-{digest}"


def _text(value: object) -> str:
    if value is None or value == "":
        return "-"
    return html.escape(str(value), quote=False).replace("|", "&#124;").replace("{", "&#123;").replace("}", "&#125;")


def _code(value: object) -> str:
    return f"<code>{_text(value)}</code>"


def _source_link(model: dict[str, object]) -> str:
    source = str(model["source_parser"])
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(source)}"
    return f"[{source}]({url})"


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
        "> Generated reference. Do not edit this page directly. Update `BuildTools/buildtools.py`, then run "
        "`python BuildTools/docs_cli.py --write`.",
        "",
        "[Reference index](index.md) | [Commands](commands.md) | "
        "[Canonical JSON model](../../../generated/cli.json) | "
        "[Generation contract](../metadata/)",
        "",
    ]


def _render_legacy_page(canonical_path: str, title: str, canonical_content: str) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/buildtools/{filename}"
    russian_path = f"../../ru/reference/buildtools/{filename}"
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
    "Generated BuildTools CLI Reference": "Сгенерированный справочник CLI BuildTools",
    "BuildTools Commands": "Команды BuildTools",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit this page directly. Update `BuildTools/buildtools.py`, then run `python BuildTools/docs_cli.py --write`.":
        "> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/buildtools.py`, затем выполните `python BuildTools/docs_cli.py --write`.",
    "[Reference index](index.md) | [Commands](commands.md) | [Canonical JSON model](../../../generated/cli.json) | [Generation contract](../metadata/)":
        "[Индекс справочника](index.md) | [Команды](commands.md) | [Каноническая JSON-модель](../../../generated/cli.json) | [Контракт генерации](../metadata/)",
    "This reference is generated from the same `argparse.ArgumentParser` used by the executable BuildTools entry point. Parser changes therefore make the committed model and pages stale.":
        "Этот справочник создаётся из того же `argparse.ArgumentParser`, который использует исполняемая точка входа BuildTools. Поэтому изменение парсера делает устаревшими зафиксированную модель и страницы.",
    "## Contract status": "## Статус контракта",
    "Stability": "Стабильность",
    "Since": "Начиная с версии",
    "Not declared": "Не объявлено",
    "Support policy": "Политика поддержки",
    "Source parser": "Исходный парсер",
    "Contract digest": "Дайджест контракта",
    "## Coverage": "## Покрытие",
    "[Commands](commands.md)": "[Команды](commands.md)",
    "Global arguments": "Глобальные аргументы",
    "Arguments accepted before a command.": "Аргументы, принимаемые перед командой.",
    "## Top-level help": "## Справка верхнего уровня",
    "## Boundary": "## Граница",
    "Included:": "Включено:",
    "Excluded from this slice:": "Исключено из этого среза:",
    "Excluded surfaces remain implementation details until an owning parser-backed contract and compatibility policy are published.":
        "Исключённые поверхности остаются деталями реализации, пока для них не будут опубликованы владеющий контракт на основе парсера и политика совместимости.",
    "Run commands from the engine repository root with `python BuildTools/buildtools.py <command>`. The exact usage and help blocks below are emitted by the executable parser.":
        "Запускайте команды из корня репозитория движка через `python BuildTools/buildtools.py <command>`. Точные блоки usage и справки ниже созданы исполняемым парсером.",
    "No parser description is declared.": "Описание в парсере не объявлено.",
    "Stable ID:": "Стабильный ID:",
    "No command-specific arguments.": "У команды нет собственных аргументов.",
    "### Exact `--help` output": "### Точный вывод `--help`",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |":
        "| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |",
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
    content = re.sub(
        r"Commands with (\d+) command-specific arguments\.",
        r"Команды с \1 собственными аргументами.",
        content,
    )
    content = content.replace(" | yes | ", " | да | ").replace(" | no | ", " | нет | ")
    front_matter_end = content.find("\n---\n", 4)
    if front_matter_end < 0:
        raise ValueError("Generated BuildTools CLI page has no front matter")
    insert_at = front_matter_end + len("\n---\n")
    marker = docs_localization.translation_metadata_line(
        document_id,
        canonical_path,
        docs_localization.normalized_sha256(english_content),
    )
    return content[:insert_at] + "\n" + marker + "\n" + content[insert_at:]


def _render_index(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    summary = model["summary"]
    assert isinstance(scope, dict) and isinstance(summary, dict)
    lines.extend(
        [
            "This reference is generated from the same `argparse.ArgumentParser` used by the executable "
            "BuildTools entry point. Parser changes therefore make the committed model and pages stale.",
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
            ("Since", "Not declared" if scope["since"] is None else _code(scope["since"])),
            ("Support policy", _text(scope["support_note"])),
            ("Source parser", _source_link(model)),
            ("Contract digest", _code(model["contract_digest"])),
        ],
    )
    lines.extend(["## Coverage", ""])
    _table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            (
                "[Commands](commands.md)",
                str(summary["command_count"]),
                f"Commands with {summary['command_argument_count']} command-specific arguments.",
            ),
            ("Global arguments", str(summary["global_argument_count"]), "Arguments accepted before a command."),
        ],
    )
    lines.extend(["## Top-level help", "", "```text", str(model["help_output"]).rstrip(), "```", ""])
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded from this slice:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.extend(
        [
            "",
            "Excluded surfaces remain implementation details until an owning parser-backed contract and "
            "compatibility policy are published.",
            "",
        ]
    )
    return "\n".join(lines)


def _argument_name(argument: dict[str, object]) -> str:
    option_strings = argument["option_strings"]
    assert isinstance(option_strings, list)
    if option_strings:
        return ", ".join(_code(option) for option in option_strings)
    return _code(argument["metavar"] or argument["destination"])


def _render_value(value: object) -> str:
    if value is None:
        return "-"
    if isinstance(value, list):
        return ", ".join(_code(item) for item in value) if value else "-"
    if isinstance(value, bool):
        return _code(str(value).lower())
    return _code(value)


def _render_commands(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[1][1:])
    lines.extend(
        [
            "Run commands from the engine repository root with `python BuildTools/buildtools.py <command>`. "
            "The exact usage and help blocks below are emitted by the executable parser.",
            "",
        ]
    )
    commands = model["commands"]
    assert isinstance(commands, list)
    for command in commands:
        assert isinstance(command, dict)
        command_id = str(command["id"])
        lines.extend(
            [
                f'<a id="{_anchor("entry", command_id)}"></a>',
                f"## `{command['name']}`",
                "",
                str(command["summary"] or command["description"] or "No parser description is declared."),
                "",
                f"Stable ID: `{command_id}`",
                "",
                "```text",
                str(command["usage"]).rstrip(),
                "```",
                "",
            ]
        )
        arguments = command["arguments"]
        assert isinstance(arguments, list)
        if arguments:
            rows: list[tuple[str, ...]] = []
            for argument in arguments:
                assert isinstance(argument, dict)
                argument_id = str(argument["id"])
                choices = argument["choices"]
                rows.append(
                    (
                        f'<a id="{_anchor("entry", argument_id)}"></a><code>{_text(argument_id)}</code>',
                        _argument_name(argument),
                        _code(argument["kind"]),
                        "yes" if argument["required"] else "no",
                        _render_value(argument["nargs"]),
                        _render_value(choices),
                        _render_value(argument["default"]),
                        _text(argument["description"] or "Not documented in the parser."),
                    )
                )
            _table(
                lines,
                ("Stable ID", "Argument", "Kind", "Required", "Values", "Choices", "Default", "Description"),
                rows,
            )
        else:
            lines.extend(["No command-specific arguments.", ""])
        lines.extend(["### Exact `--help` output", "", "```text", str(command["help_output"]).rstrip(), "```", ""])
    return "\n".join(lines)


def generate_reference_pages(
    model: dict[str, object],
    russian_model: dict[str, object] | None = None,
) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("Unsupported generated BuildTools CLI model")
    commands = model.get("commands")
    if not isinstance(commands, list) or any(not isinstance(command, dict) for command in commands):
        raise ValueError("BuildTools CLI model commands must be an array of objects")
    identities = [
        entry.get("id")
        for command in commands
        for entry in [command, *command.get("arguments", [])]
        if isinstance(entry, dict)
    ]
    if any(not isinstance(identity, str) or not identity for identity in identities) or len(identities) != len(set(identities)):
        raise ValueError("Every BuildTools CLI command and argument must have a unique non-empty ID")

    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/commands.md": _render_commands(model),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/commands.md": _render_commands(localized_model),
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
        raise ValueError("Generated BuildTools CLI reference page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def render_reference_pages(root: Path, source_relative_path: str = DEFAULT_SOURCE) -> dict[str, str]:
    model = generate_cli_model(root, source_relative_path)
    russian_model = docs_description_translations.apply_translations(root, "cli", model)
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline BuildTools CLI model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--source", default=DEFAULT_SOURCE)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write the generated JSON model and Markdown pages")
    mode.add_argument("--check", action="store_true", help="fail when generated CLI documentation is stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        model_content = render_cli_model(root, args.source)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(root, "cli", model)
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate BuildTools CLI documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote BuildTools CLI model and {len(pages)} reference pages")
        return 0

    stale_paths = [
        relative_path
        for relative_path, content in outputs.items()
        if not (root / relative_path).is_file() or (root / relative_path).read_text(encoding="utf-8") != content
    ]
    if stale_paths:
        print(
            "Generated BuildTools CLI documentation is missing or stale: "
            + ", ".join(stale_paths)
            + "; run python BuildTools/docs_cli.py --write",
            file=sys.stderr,
        )
        return 1

    summary = model["summary"]
    print(
        f"Generated BuildTools CLI documentation is current: {summary['command_count']} commands, "
        f"{summary['command_argument_count']} command arguments"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
