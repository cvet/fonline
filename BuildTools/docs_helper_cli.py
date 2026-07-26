from __future__ import annotations

import argparse
import ast
import hashlib
import importlib.util
import json
import os
import re
import sys
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import quote

import docs_cli


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/HelperCliInterface.json"
DEFAULT_MODEL = "Docs/generated/helper-cli.json"
DEFAULT_OUTPUT_DIR = "Docs/generated/helper-cli"
GENERATED_BY = "BuildTools/docs_helper_cli.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-helper-cli-index", "Generated Helper CLI Reference"),
    ("commands.md", "generated-helper-cli-commands", "Helper Commands"),
)
OUTPUT_PATHS = tuple(f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS)
HELPER_ID_PATTERN = re.compile(r"^helper-cli\.[a-z0-9]+(?:-[a-z0-9]+)*$")
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


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "helper-cli":
        raise ValueError("scope.surface must be helper-cli")
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


def _has_top_level_parser_factory(path: Path) -> bool:
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    except (OSError, SyntaxError) as exception:
        raise ValueError(f"unable to inspect parser source {path}: {exception}") from exception
    return any(
        isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "create_parser"
        for node in tree.body
    )


def _discover_parser_sources(root: Path, discovery: dict[str, object]) -> set[str]:
    discovery_root = _required_string(discovery.get("root"), "discovery.root")
    relative = PurePosixPath(discovery_root)
    if "\\" in discovery_root or relative.is_absolute() or ".." in relative.parts:
        raise ValueError("discovery.root must be a repository-relative forward-slash path")
    discovery_path = root / discovery_root
    if not discovery_path.is_dir():
        raise ValueError("discovery.root must be a directory")
    excluded_directories = set(_string_list(discovery.get("excluded_directories"), "discovery.excluded_directories", allow_empty=True))
    excluded_prefixes = tuple(_string_list(discovery.get("excluded_name_prefixes"), "discovery.excluded_name_prefixes", allow_empty=True))

    sources: set[str] = set()
    for source_path in sorted(discovery_path.rglob("*.py")):
        relative_under_root = source_path.relative_to(discovery_path)
        if any(part in excluded_directories for part in relative_under_root.parts[:-1]):
            continue
        if source_path.name.startswith(excluded_prefixes):
            continue
        if _has_top_level_parser_factory(source_path):
            sources.add(source_path.relative_to(root).as_posix())
    return sources


def _load_manifest(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read helper CLI manifest {manifest_relative_path}: {exception}") from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"helper CLI manifest schema_version must be {SCHEMA_VERSION}")
    _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))

    discovery = raw.get("discovery")
    if not isinstance(discovery, dict):
        raise ValueError("discovery must be an object")
    excluded_raw = discovery.get("excluded_parser_sources")
    if not isinstance(excluded_raw, list):
        raise ValueError("discovery.excluded_parser_sources must be an array")
    excluded_sources: list[dict[str, str]] = []
    for index, entry in enumerate(excluded_raw):
        label = f"discovery.excluded_parser_sources[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        excluded_sources.append(
            {
                "source": _source_path(root, entry.get("source"), f"{label}.source"),
                "reason": _required_string(entry.get("reason"), f"{label}.reason"),
            }
        )

    helpers_raw = raw.get("helpers")
    if not isinstance(helpers_raw, list) or not helpers_raw:
        raise ValueError("helpers must be a non-empty array")
    helpers: list[dict[str, object]] = []
    helper_ids: set[str] = set()
    helper_sources: set[str] = set()
    for index, helper in enumerate(helpers_raw):
        label = f"helpers[{index}]"
        if not isinstance(helper, dict):
            raise ValueError(f"{label} must be an object")
        helper_id = _required_string(helper.get("id"), f"{label}.id")
        if not HELPER_ID_PATTERN.fullmatch(helper_id):
            raise ValueError(f"{label}.id is invalid: {helper_id}")
        if helper_id in helper_ids:
            raise ValueError(f"duplicate helper CLI id: {helper_id}")
        helper_ids.add(helper_id)
        source = _source_path(root, helper.get("source"), f"{label}.source")
        if source in helper_sources:
            raise ValueError(f"duplicate helper CLI source: {source}")
        helper_sources.add(source)
        normalized = {
            "id": helper_id,
            "name": _required_string(helper.get("name"), f"{label}.name"),
            "source": source,
            "factory": _required_string(helper.get("factory"), f"{label}.factory"),
            "program": _required_string(helper.get("program"), f"{label}.program"),
            "owner": _required_string(helper.get("owner"), f"{label}.owner"),
            "audiences": _string_list(helper.get("audiences"), f"{label}.audiences"),
            "invocation_owner": _required_string(helper.get("invocation_owner"), f"{label}.invocation_owner"),
            "description": _required_string(helper.get("description"), f"{label}.description"),
        }
        helpers.append(normalized)

    excluded_source_names = [entry["source"] for entry in excluded_sources]
    if len(excluded_source_names) != len(set(excluded_source_names)):
        raise ValueError("discovery.excluded_parser_sources must not contain duplicate sources")
    discovered = _discover_parser_sources(root, discovery)
    declared = helper_sources | set(excluded_source_names)
    if discovered != declared:
        missing = sorted(discovered - declared)
        stale = sorted(declared - discovered)
        details = []
        if missing:
            details.append("undocumented parser sources: " + ", ".join(missing))
        if stale:
            details.append("declared sources without create_parser(): " + ", ".join(stale))
        raise ValueError("helper CLI parser inventory mismatch; " + "; ".join(details))

    return {
        "description": raw["description"],
        "scope": scope,
        "discovery": {
            "root": discovery["root"],
            "excluded_directories": discovery["excluded_directories"],
            "excluded_name_prefixes": discovery["excluded_name_prefixes"],
            "excluded_parser_sources": excluded_sources,
        },
        "helpers": helpers,
    }


def _load_parser(root: Path, helper: dict[str, object]) -> argparse.ArgumentParser:
    source = str(helper["source"])
    source_path = root / source
    module_digest = hashlib.sha256(str(source_path.resolve()).encode("utf-8")).hexdigest()[:16]
    module_name = f"_fonline_docs_helper_cli_{module_digest}"
    spec = importlib.util.spec_from_file_location(module_name, source_path)
    if spec is None or spec.loader is None:
        raise ValueError(f"unable to load helper CLI parser source: {source}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    source_directory = str(source_path.parent)
    sys.path.insert(0, source_directory)
    try:
        with docs_cli._stable_argparse_environment():
            spec.loader.exec_module(module)
            factory = getattr(module, str(helper["factory"]), None)
            if not callable(factory):
                raise ValueError(f"{source} must expose callable {helper['factory']}()")
            parser = factory()
    finally:
        if sys.path[0] == source_directory:
            sys.path.pop(0)
        else:
            sys.path.remove(source_directory)
        sys.modules.pop(module_name, None)

    if not isinstance(parser, argparse.ArgumentParser):
        raise ValueError(f"{source} {helper['factory']}() must return argparse.ArgumentParser")
    if parser.prog != helper["program"]:
        raise ValueError(f"{source} parser.prog must be {helper['program']!r}, got {parser.prog!r}")
    return parser


def _parser_model(helper: dict[str, object], parser: argparse.ArgumentParser) -> dict[str, object]:
    helper_id = str(helper["id"])
    subparser_actions = [
        action for action in parser._actions if isinstance(action, argparse._SubParsersAction)
    ]
    if len(subparser_actions) > 1:
        raise ValueError(f"{helper_id} parser must not contain multiple top-level subparser actions")

    commands: list[dict[str, object]] = []
    if subparser_actions:
        subparsers = subparser_actions[0]
        choice_help = {
            action.dest: None if action.help is argparse.SUPPRESS else action.help
            for action in subparsers._choices_actions
        }
        seen_parsers: set[int] = set()
        for name, command_parser in subparsers.choices.items():
            if id(command_parser) in seen_parsers:
                raise ValueError(f"{helper_id} parser aliases require an explicit documentation policy")
            seen_parsers.add(id(command_parser))
            if any(isinstance(action, argparse._SubParsersAction) for action in command_parser._actions):
                raise ValueError(f"nested helper CLI subcommands are not supported: {helper_id}.{name}")
            command_id = f"{helper_id}.command.{name}"
            commands.append(
                {
                    "id": command_id,
                    "name": name,
                    "summary": choice_help.get(name),
                    "description": command_parser.description,
                    "usage": docs_cli._normalized_usage(command_parser),
                    "help_output": docs_cli._normalized_help(command_parser),
                    "arguments": [
                        docs_cli._argument_model(command_id, action)
                        for action in docs_cli._documented_actions(command_parser)
                    ],
                }
            )

    global_arguments = [
        docs_cli._argument_model(helper_id, action)
        for action in docs_cli._documented_actions(parser)
    ]
    result = dict(helper)
    result.update(
        {
            "parser_description": parser.description,
            "usage": docs_cli._normalized_usage(parser),
            "help_output": docs_cli._normalized_help(parser),
            "global_arguments": global_arguments,
            "commands": commands,
        }
    )
    return result


def generate_helper_cli_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, object]:
    manifest = _load_manifest(root, manifest_relative_path)
    helpers = [
        _parser_model(helper, _load_parser(root, helper))
        for helper in manifest["helpers"]
        if isinstance(helper, dict)
    ]
    identities = [
        entry["id"]
        for helper in helpers
        for entry in [
            helper,
            *helper["global_arguments"],
            *[
                nested
                for command in helper["commands"]
                for nested in [command, *command["arguments"]]
            ],
        ]
    ]
    if len(identities) != len(set(identities)):
        raise ValueError("helper CLI model IDs must be unique")

    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": manifest["description"],
        "scope": manifest["scope"],
        "discovery": manifest["discovery"],
        "helpers": helpers,
        "summary": {
            "helper_count": len(helpers),
            "command_count": sum(len(helper["commands"]) for helper in helpers),
            "global_argument_count": sum(len(helper["global_arguments"]) for helper in helpers),
            "command_argument_count": sum(
                len(command["arguments"])
                for helper in helpers
                for command in helper["commands"]
            ),
        },
    }
    contract_content = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(contract_content.encode("utf-8")).hexdigest()
    return model


def render_helper_cli_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> str:
    return json.dumps(generate_helper_cli_model(root, manifest_relative_path), ensure_ascii=False, indent=2) + "\n"


def _source_link(model: dict[str, object], source: str) -> str:
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(source)}"
    return f"[{source}]({url})"


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
        "> Generated reference. Do not edit this page directly. Update `BuildTools/HelperCliInterface.json` "
        "or the owning executable parser, then run `python BuildTools/docs_helper_cli.py --write`.",
        "",
        "[Reference index](index.md) | [Commands](commands.md) | "
        "[Canonical JSON model](../helper-cli.json) | [Generation contract](../../GeneratedApiAndMetadata.md)",
        "",
    ]


def _render_index(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    summary = model["summary"]
    assert isinstance(scope, dict) and isinstance(summary, dict)
    lines.extend(
        [
            "This reference is generated from the `argparse.ArgumentParser` objects used by executable "
            "engine helper scripts. The manifest owns purpose and audience; source parsers own executable syntax.",
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
            ("Since", "Not declared" if scope["since"] is None else docs_cli._code(scope["since"])),
            ("Support policy", docs_cli._text(scope["support_note"])),
            ("Source manifest", _source_link(model, str(model["source_manifest"]))),
            ("Contract digest", docs_cli._code(model["contract_digest"])),
        ],
    )
    lines.extend(["## Inventory", ""])
    rows: list[tuple[str, ...]] = []
    for helper in model["helpers"]:
        assert isinstance(helper, dict)
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", str(helper["id"]))}"></a><code>{docs_cli._text(helper["id"])}</code>',
                f"[{docs_cli._text(helper['name'])}](commands.md#{docs_cli._anchor('entry', str(helper['id']))})",
                docs_cli._code(helper["owner"]),
                docs_cli._text(helper["invocation_owner"]),
                _source_link(model, str(helper["source"])),
                f"{len(helper['commands'])} / {len(helper['global_arguments'])}",
            )
        )
    docs_cli._table(lines, ("Stable ID", "Helper", "Owner", "Invocation owner", "Parser source", "Commands / global args"), rows)
    lines.extend(
        [
            "## Coverage",
            "",
            f"The model contains {summary['helper_count']} helpers, {summary['command_count']} subcommands, "
            f"{summary['global_argument_count']} global arguments, and {summary['command_argument_count']} "
            "subcommand arguments.",
            "",
            "Included:",
            "",
        ]
    )
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_arguments(lines: list[str], arguments: list[dict[str, object]]) -> None:
    if not arguments:
        lines.extend(["No arguments at this level.", ""])
        return
    rows: list[tuple[str, ...]] = []
    for argument in arguments:
        argument_id = str(argument["id"])
        rows.append(
            (
                f'<a id="{docs_cli._anchor("entry", argument_id)}"></a><code>{docs_cli._text(argument_id)}</code>',
                docs_cli._argument_name(argument),
                docs_cli._code(argument["kind"]),
                "yes" if argument["required"] else "no",
                docs_cli._render_value(argument["nargs"]),
                docs_cli._render_value(argument["choices"]),
                docs_cli._render_value(argument["default"]),
                docs_cli._text(argument["description"] or "Not documented in the parser."),
            )
        )
    docs_cli._table(
        lines,
        ("Stable ID", "Argument", "Kind", "Required", "Values", "Choices", "Default", "Description"),
        rows,
    )


def _render_commands(model: dict[str, object]) -> str:
    lines = _page_header(*PAGE_DEFINITIONS[1][1:])
    lines.extend(
        [
            "Commands are shown with exact parser-generated usage and help at a fixed 80-column width. "
            "Invoke a helper from the engine repository root unless its invocation owner sets another working directory.",
            "",
        ]
    )
    for helper in model["helpers"]:
        assert isinstance(helper, dict)
        helper_id = str(helper["id"])
        lines.extend(
            [
                f'<a id="{docs_cli._anchor("entry", helper_id)}"></a>',
                f"## {helper['name']}",
                "",
                str(helper["description"]),
                "",
                f"Stable ID: `{helper_id}`  ",
                f"Program: `{helper['program']}`  ",
                f"Owner: `{helper['owner']}`  ",
                f"Audience: {', '.join(f'`{audience}`' for audience in helper['audiences'])}  ",
                f"Invocation owner: {helper['invocation_owner']}  ",
                f"Parser source: {_source_link(model, str(helper['source']))}",
                "",
                "### Top-level arguments",
                "",
            ]
        )
        _render_arguments(lines, helper["global_arguments"])
        lines.extend(["### Exact top-level `--help` output", "", "```text", str(helper["help_output"]).rstrip(), "```", ""])
        for command in helper["commands"]:
            command_id = str(command["id"])
            lines.extend(
                [
                    f'<a id="{docs_cli._anchor("entry", command_id)}"></a>',
                    f"### `{command['name']}`",
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
            _render_arguments(lines, command["arguments"])
            lines.extend(["#### Exact `--help` output", "", "```text", str(command["help_output"]).rstrip(), "```", ""])
    return "\n".join(lines)


def generate_reference_pages(model: dict[str, object]) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("unsupported generated helper CLI model")
    helpers = model.get("helpers")
    if not isinstance(helpers, list) or any(not isinstance(helper, dict) for helper in helpers):
        raise ValueError("helper CLI model helpers must be an array of objects")
    identities = [
        entry.get("id")
        for helper in helpers
        for entry in [
            helper,
            *helper.get("global_arguments", []),
            *[
                nested
                for command in helper.get("commands", [])
                if isinstance(command, dict)
                for nested in [command, *command.get("arguments", [])]
            ],
        ]
        if isinstance(entry, dict)
    ]
    if any(not isinstance(identity, str) or not identity for identity in identities) or len(identities) != len(set(identities)):
        raise ValueError("every helper CLI entry must have a unique non-empty ID")

    pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/commands.md": _render_commands(model),
    }
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated helper CLI reference page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline helper CLI model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write the generated JSON model and Markdown pages")
    mode.add_argument("--check", action="store_true", help="fail when generated helper CLI documentation is stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        model_content = render_helper_cli_model(root, args.manifest)
        model = json.loads(model_content)
        pages = generate_reference_pages(model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate helper CLI documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote helper CLI model and {len(pages)} reference pages")
        return 0

    stale_paths = [
        relative_path
        for relative_path, content in outputs.items()
        if not (root / relative_path).is_file() or (root / relative_path).read_text(encoding="utf-8") != content
    ]
    if stale_paths:
        print(
            "Generated helper CLI documentation is missing or stale: "
            + ", ".join(stale_paths)
            + "; run python BuildTools/docs_helper_cli.py --write",
            file=sys.stderr,
        )
        return 1

    summary = model["summary"]
    print(
        f"Generated helper CLI documentation is current: {summary['helper_count']} helpers, "
        f"{summary['command_count']} subcommands, "
        f"{summary['global_argument_count'] + summary['command_argument_count']} arguments"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
