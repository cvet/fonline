from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import quote

import codegen
import docs_cli


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/NativeExtensionInterface.json"
DEFAULT_PROJECT_INTERFACE = "BuildTools/cmake/ProjectInterface.json"
DEFAULT_MODEL = "Docs/generated/native-extension.json"
DEFAULT_OUTPUT_DIR = "Docs/generated/native-extension"
GENERATED_BY = "BuildTools/docs_native_extension.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-native-extension-index", "Generated Native Extension Interface"),
    ("roles.md", "generated-native-extension-roles", "Native Extension Roles"),
    ("hooks.md", "generated-native-extension-hooks", "Engine Hooks"),
    ("bindings.md", "generated-native-extension-bindings", "Native Binding Rules"),
)
OUTPUT_PATHS = tuple(f"{DEFAULT_OUTPUT_DIR}/{filename}" for filename, _, _ in PAGE_DEFINITIONS)
ENTRY_ID_PATTERN = re.compile(r"^native-extension\.(role|hook|binding)\.[A-Za-z0-9][A-Za-z0-9.-]*$")
ROLE_PATTERN = re.compile(r"^[A-Z][A-Z0-9_]*$")
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


def _relative_path(root: Path, value: object, label: str, *, must_exist: bool = True) -> str:
    path = _required_string(value, label)
    relative = PurePosixPath(path)
    if "\\" in path or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    if must_exist and not root.joinpath(*relative.parts).is_file():
        raise ValueError(f"{label} does not exist: {path}")
    return path


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "native-extension-interface":
        raise ValueError("scope.surface must be native-extension-interface")
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


def _validate_registration(root: Path, raw: object) -> dict[str, str]:
    if not isinstance(raw, dict):
        raise ValueError("registration must be an object")
    fields = (
        "cmake_helper",
        "source_stage",
        "required_before",
        "contribution_root_variable",
        "metadata_source_list",
        "codegen_parser",
        "generated_stub_output",
        "namespace_open_macro",
        "namespace_close_macro",
        "namespace_definition_macro",
        "export_macro",
    )
    result = {field: _required_string(raw.get(field), f"registration.{field}") for field in fields}
    _relative_path(root, result["codegen_parser"], "registration.codegen_parser")
    if result["cmake_helper"] != "AddEngineSources" or result["metadata_source_list"] != "FO_SOURCE_META_FILES":
        raise ValueError("registration must preserve AddEngineSources and FO_SOURCE_META_FILES ownership")
    return result


def _validate_roles(raw: object) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("roles must be a non-empty array")
    roles: list[dict[str, object]] = []
    ids: set[str] = set()
    names: set[str] = set()
    source_lists: set[str] = set()
    for index, role in enumerate(raw):
        label = f"roles[{index}]"
        if not isinstance(role, dict):
            raise ValueError(f"{label} must be an object")
        entry_id = _required_string(role.get("id"), f"{label}.id")
        name = _required_string(role.get("name"), f"{label}.name")
        source_list = _required_string(role.get("source_list"), f"{label}.source_list")
        if not ENTRY_ID_PATTERN.fullmatch(entry_id) or not entry_id.startswith("native-extension.role."):
            raise ValueError(f"invalid native extension role id: {entry_id}")
        if not ROLE_PATTERN.fullmatch(name):
            raise ValueError(f"invalid native extension role name: {name}")
        if entry_id in ids or name in names or source_list in source_lists:
            raise ValueError(f"duplicate native extension role identity: {name}")
        ids.add(entry_id)
        names.add(name)
        source_lists.add(source_list)
        _required_string(role.get("library_target"), f"{label}.library_target")
        _required_string(role.get("primary_header"), f"{label}.primary_header")
        _string_list(role.get("consumers"), f"{label}.consumers")
        script_targets = _string_list(role.get("script_targets"), f"{label}.script_targets", allow_empty=True)
        if any(target not in {value.lower() for value in codegen.EXPORT_TARGETS} for target in script_targets):
            raise ValueError(f"{label}.script_targets contains an unsupported codegen target")
        _required_string(role.get("description"), f"{label}.description")
        roles.append(copy.deepcopy(role))
    return roles


def _validate_hooks(root: Path, raw: object, role_names: set[str]) -> list[dict[str, object]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("hooks must be a non-empty array")
    hooks: list[dict[str, object]] = []
    ids: set[str] = set()
    names: set[str] = set()
    for index, hook in enumerate(raw):
        label = f"hooks[{index}]"
        if not isinstance(hook, dict):
            raise ValueError(f"{label} must be an object")
        entry_id = _required_string(hook.get("id"), f"{label}.id")
        name = _required_string(hook.get("name"), f"{label}.name")
        role = _required_string(hook.get("role"), f"{label}.role")
        if not ENTRY_ID_PATTERN.fullmatch(entry_id) or not entry_id.startswith("native-extension.hook."):
            raise ValueError(f"invalid engine hook id: {entry_id}")
        if entry_id in ids or name in names:
            raise ValueError(f"duplicate engine hook identity: {name}")
        if role not in role_names:
            raise ValueError(f"engine hook {name} has unknown role {role}")
        ids.add(entry_id)
        names.add(name)
        signature = _required_string(hook.get("signature"), f"{label}.signature")
        if name not in signature:
            raise ValueError(f"engine hook signature does not contain {name}")
        call_sites = _string_list(hook.get("call_sites"), f"{label}.call_sites")
        for call_site_index, call_site in enumerate(call_sites):
            source = _relative_path(root, call_site, f"{label}.call_sites[{call_site_index}]")
            if name not in (root / source).read_text(encoding="utf-8"):
                raise ValueError(f"engine hook call site does not reference {name}: {source}")
        _string_list(hook.get("stub_declarations"), f"{label}.stub_declarations", allow_empty=True)
        stub_definition = _required_string(hook.get("stub_definition"), f"{label}.stub_definition")
        if name not in stub_definition:
            raise ValueError(f"engine hook stub does not define {name}")
        _required_string(hook.get("default_behavior"), f"{label}.default_behavior")
        if not isinstance(hook.get("compatibility_hashed"), bool):
            raise ValueError(f"{label}.compatibility_hashed must be boolean")
        _required_string(hook.get("description"), f"{label}.description")
        hooks.append(copy.deepcopy(hook))
    return hooks


def _validate_binding_rules(raw: object) -> list[dict[str, str]]:
    if not isinstance(raw, list) or not raw:
        raise ValueError("binding_rules must be a non-empty array")
    rules: list[dict[str, str]] = []
    ids: set[str] = set()
    for index, rule in enumerate(raw):
        label = f"binding_rules[{index}]"
        if not isinstance(rule, dict):
            raise ValueError(f"{label} must be an object")
        entry_id = _required_string(rule.get("id"), f"{label}.id")
        if not ENTRY_ID_PATTERN.fullmatch(entry_id) or not entry_id.startswith("native-extension.binding."):
            raise ValueError(f"invalid native binding rule id: {entry_id}")
        if entry_id in ids:
            raise ValueError(f"duplicate native binding rule id: {entry_id}")
        ids.add(entry_id)
        rules.append(
            {
                "id": entry_id,
                "name": _required_string(rule.get("name"), f"{label}.name"),
                "requirement": _required_string(rule.get("requirement"), f"{label}.requirement"),
                "rationale": _required_string(rule.get("rationale"), f"{label}.rationale"),
            }
        )
    return rules


def _project_roles(root: Path, project_interface_path: str = DEFAULT_PROJECT_INTERFACE) -> list[str]:
    interface = json.loads((root / project_interface_path).read_text(encoding="utf-8"))
    helper = next(
        (entry for entry in interface.get("helpers", []) if entry.get("name") == "AddEngineSources"),
        None,
    )
    if not isinstance(helper, dict):
        raise ValueError("CMake project interface must declare AddEngineSources")
    return _string_list(helper.get("allowed_roles"), "AddEngineSources.allowed_roles")


def generate_native_extension_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read native extension manifest {manifest_relative_path}: {exception}") from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"native extension manifest schema_version must be {SCHEMA_VERSION}")

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    registration = _validate_registration(root, raw.get("registration"))
    roles = _validate_roles(raw.get("roles"))
    role_names = [str(role["name"]) for role in roles]
    hooks = _validate_hooks(root, raw.get("hooks"), set(role_names))
    binding_rules = _validate_binding_rules(raw.get("binding_rules"))

    project_roles = _project_roles(root)
    if project_roles != role_names:
        raise ValueError(f"native extension roles differ from ProjectInterface.json: {role_names} != {project_roles}")
    if Path(codegen.NATIVE_EXTENSION_INTERFACE_PATH).resolve() == manifest_path.resolve():
        runtime_hooks = [dict(hook) for hook in codegen.ENGINE_HOOK_DEFINITIONS]
        if runtime_hooks != hooks:
            raise ValueError("native extension hooks differ from codegen runtime definitions")
        if list(codegen.ENGINE_HOOK_NAMES) != [hook["name"] for hook in hooks]:
            raise ValueError("native extension hook order differs from codegen runtime")

    identities = [entry["id"] for entry in [*roles, *hooks, *binding_rules]]
    if len(identities) != len(set(identities)):
        raise ValueError("native extension model IDs must be unique")
    hooks_by_role = Counter(str(hook["role"]) for hook in hooks)

    model: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_relative_path,
        "project_interface": DEFAULT_PROJECT_INTERFACE,
        "repository": REPOSITORY,
        "source_ref": SOURCE_REF,
        "description": description,
        "scope": scope,
        "registration": registration,
        "roles": roles,
        "hooks": hooks,
        "binding_rules": binding_rules,
        "summary": {
            "role_count": len(roles),
            "hook_count": len(hooks),
            "binding_rule_count": len(binding_rules),
            "compatibility_hashed_hook_count": sum(bool(hook["compatibility_hashed"]) for hook in hooks),
            "hooks_by_role": dict(sorted(hooks_by_role.items())),
        },
    }
    content = json.dumps(model, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    model["contract_digest"] = hashlib.sha256(content.encode("utf-8")).hexdigest()
    return model


def render_native_extension_model(root: Path, manifest_relative_path: str = DEFAULT_MANIFEST) -> str:
    return json.dumps(generate_native_extension_model(root, manifest_relative_path), ensure_ascii=False, indent=2) + "\n"


def _source_link(model: dict[str, object], source: str) -> str:
    url = f"https://github.com/{model['repository']}/blob/{model['source_ref']}/{quote(source)}"
    return f"[{source}]({url})"


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
        "> Generated reference. Do not edit directly. Update `BuildTools/NativeExtensionInterface.json`, "
        "then run `python BuildTools/docs_native_extension.py --write`.",
        "",
        "[Index](index.md) | [Roles](roles.md) | [Hooks](hooks.md) | [Bindings](bindings.md) | "
        "[Canonical JSON](../native-extension.json) | [Guide](../../NativeExtensions.md)",
        "",
    ]


def _render_index(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[0][1:])
    scope = model["scope"]
    summary = model["summary"]
    assert isinstance(scope, dict) and isinstance(summary, dict)
    lines.extend([
        "This interface describes engine-owned composition and hook contracts for project-native C++ sources. "
        "It does not document any particular game's extension implementation.",
        "",
        "## Contract status",
        "",
    ])
    docs_cli._table(lines, ("Field", "Value"), [
        ("Stability", docs_cli._code(scope["stability"])),
        ("Support policy", docs_cli._text(scope["support_note"])),
        ("Source manifest", _source_link(model, str(model["source_manifest"]))),
        ("Contract digest", docs_cli._code(model["contract_digest"])),
    ])
    docs_cli._table(lines, ("Reference", "Entries", "Purpose"), [
        ("[Roles](roles.md)", str(summary["role_count"]), "CMake source routing, libraries, headers, and consumers."),
        ("[Hooks](hooks.md)", str(summary["hook_count"]), "Optional declarations, call sites, defaults, and compatibility state."),
        ("[Bindings](bindings.md)", str(summary["binding_rule_count"]), "Registration, namespace, pointer, and dependency rules."),
    ])
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_roles(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    lines.extend([
        "`AddEngineSources` accepts only these roles. Every resolved source also enters `FO_SOURCE_META_FILES` "
        "before code generation.",
        "",
    ])
    rows: list[tuple[str, ...]] = []
    for role in model["roles"]:
        assert isinstance(role, dict)
        entry_id = str(role["id"])
        rows.append((
            f'<a id="{docs_cli._anchor("entry", entry_id)}"></a><code>{docs_cli._text(entry_id)}</code>',
            docs_cli._code(role["name"]),
            docs_cli._code(role["source_list"]),
            docs_cli._code(role["library_target"]),
            docs_cli._code(role["primary_header"]),
            ", ".join(docs_cli._code(item) for item in role["consumers"]),
            ", ".join(docs_cli._code(item) for item in role["script_targets"]) or "-",
            docs_cli._text(role["description"]),
        ))
    docs_cli._table(lines, ("Stable ID", "Role", "Source list", "Library", "Primary header", "Consumers", "Script targets", "Purpose"), rows)
    lines.extend([
        "## Registration shape",
        "",
        "```cmake",
        "AddEngineSources(",
        "    COMMON SourceExt/CommonExtension.cpp",
        "    SERVER SourceExt/ServerExtension.cpp",
        "    CLIENT SourceExt/ClientExtension.cpp)",
        "RegisterEngineSources()",
        "```",
        "",
        "Paths and globs are resolved relative to the embedding-project contribution root. Unknown roles are configure errors.",
        "",
    ])
    return "\n".join(lines)


def _render_hooks(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    lines.extend([
        "A project implements a hook by declaring it as metadata in a source registered under the owning role. "
        "Codegen omits that hook's fallback when it sees the declaration.",
        "",
    ])
    for hook in model["hooks"]:
        assert isinstance(hook, dict)
        entry_id = str(hook["id"])
        lines.extend([
            f'<a id="{docs_cli._anchor("entry", entry_id)}"></a>',
            f"## `{hook['name']}`",
            "",
            str(hook["description"]),
            "",
            f"Stable ID: `{entry_id}`  ",
            f"Role: `{hook['role']}`  ",
            f"Compatibility-hashed presence: `{'yes' if hook['compatibility_hashed'] else 'no'}`  ",
            "Call sites: " + ", ".join(_source_link(model, str(path)) for path in hook["call_sites"]),
            "",
            "```cpp",
            "FO_BEGIN_NAMESPACE",
            "///@ EngineHook",
            f"FO_SCRIPT_API {hook['signature']};",
            "FO_END_NAMESPACE",
            "```",
            "",
            f"Default: {hook['default_behavior']}",
            "",
        ])
    return "\n".join(lines)


def _render_bindings(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[3][1:])
    lines.extend([
        "These rules are the reusable boundary. Project dependency setup, native state, settings, and packaging remain project-owned.",
        "",
    ])
    rows = []
    for rule in model["binding_rules"]:
        assert isinstance(rule, dict)
        entry_id = str(rule["id"])
        rows.append((
            f'<a id="{docs_cli._anchor("entry", entry_id)}"></a><code>{docs_cli._text(entry_id)}</code>',
            docs_cli._text(rule["name"]),
            docs_cli._text(rule["requirement"]),
            docs_cli._text(rule["rationale"]),
        ))
    docs_cli._table(lines, ("Stable ID", "Rule", "Requirement", "Why"), rows)
    lines.extend([
        "## Minimal exported method",
        "",
        "```cpp",
        '#include "Common.h"',
        '#include "Server.h"',
        "",
        "FO_USING_NAMESPACE();",
        "FO_BEGIN_NAMESPACE",
        "///@ ExportMethod",
        "FO_SCRIPT_API int32_t Server_Game_ProjectValue(ptr<ServerEngine> server);",
        "FO_END_NAMESPACE",
        "",
        "int32_t FO_NAMESPACE Server_Game_ProjectValue(ptr<ServerEngine> server)",
        "{",
        "    ignore_unused(server);",
        "    return 1;",
        "}",
        "```",
        "",
        "The exported body intentionally has no stack-trace entry macro. Ordinary non-exported native functions keep the normal engine stack-trace convention.",
        "",
    ])
    return "\n".join(lines)


def generate_reference_pages(model: dict[str, object]) -> dict[str, str]:
    if model.get("schema_version") != SCHEMA_VERSION or model.get("generated_by") != GENERATED_BY:
        raise ValueError("unsupported generated native extension model")
    identities = [
        entry.get("id")
        for key in ("roles", "hooks", "binding_rules")
        for entry in model.get(key, [])
        if isinstance(entry, dict)
    ]
    if any(not isinstance(identity, str) or not identity for identity in identities) or len(identities) != len(set(identities)):
        raise ValueError("every native extension entry must have a unique non-empty ID")
    pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/roles.md": _render_roles(model),
        f"{DEFAULT_OUTPUT_DIR}/hooks.md": _render_hooks(model),
        f"{DEFAULT_OUTPUT_DIR}/bindings.md": _render_bindings(model),
    }
    if set(pages) != set(OUTPUT_PATHS):
        raise ValueError("generated native extension page set does not match OUTPUT_PATHS")
    return {path: content.rstrip() + "\n" for path, content in sorted(pages.items())}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the FOnline native extension model and reference")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        model_content = render_native_extension_model(root, args.manifest)
        model = json.loads(model_content)
        pages = generate_reference_pages(model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(f"Unable to generate native extension documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote native extension model and {len(pages)} reference pages")
        return 0

    stale = [
        path for path, content in outputs.items()
        if not (root / path).is_file() or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated native extension documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_native_extension.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        f"Generated native extension documentation is current: {summary['role_count']} roles, "
        f"{summary['hook_count']} hooks, {summary['binding_rule_count']} binding rules"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
