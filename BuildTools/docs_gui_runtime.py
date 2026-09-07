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
DEFAULT_MANIFEST = "BuildTools/GuiRuntimeInterface.json"
DEFAULT_MODEL = "Docs/generated/gui-runtime.json"
DEFAULT_OUTPUT_DIR = "Docs/en/reference/gui-runtime"
RUSSIAN_OUTPUT_DIR = "Docs/ru/reference/gui-runtime"
LEGACY_OUTPUT_DIR = "Docs/generated/gui-runtime"
GENERATED_BY = "BuildTools/docs_gui_runtime.py"
REPOSITORY = "cvet/fonline"
SOURCE_REF = "master"
PAGE_DEFINITIONS = (
    ("index.md", "generated-gui-runtime-index", "Generated GUI Runtime Reference"),
    ("types.md", "generated-gui-runtime-types", "GUI Runtime Types"),
    ("screen-api.md", "generated-gui-runtime-screen-api", "GUI Screen API"),
    ("lifecycle.md", "generated-gui-runtime-lifecycle", "GUI Screen Lifecycle"),
    (
        "layout-rendering.md",
        "generated-gui-runtime-layout-rendering",
        "GUI Layout And Rendering",
    ),
    ("input.md", "generated-gui-runtime-input", "GUI Input Contract"),
    (
        "integration-validation.md",
        "generated-gui-runtime-integration-validation",
        "GUI Integration And Validation",
    ),
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
RULE_COLLECTIONS = (
    "lifecycle_rules",
    "layout_rules",
    "input_rules",
    "integration_rules",
    "validation_rules",
)
COLLECTION_KINDS = {
    "types": "type",
    "screen_api": "screen-api",
    "annotations": "annotation",
    "lifecycle_rules": "lifecycle",
    "layout_rules": "layout",
    "input_rules": "input",
    "integration_rules": "integration",
    "validation_rules": "validation",
}
ENTRY_ID_PATTERN = re.compile(
    r"^gui-runtime\.(type|screen-api|annotation|lifecycle|layout|input|"
    r"integration|validation)\.[A-Za-z0-9][A-Za-z0-9.-]*$"
)
VALID_STABILITY = {"stable", "experimental", "deprecated", "internal"}


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(
    value: object, label: str, *, allow_empty: bool = False
) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        qualifier = "an" if allow_empty else "a non-empty"
        raise ValueError(f"{label} must be {qualifier} array of strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _relative_path(value: object, label: str) -> tuple[str, PurePosixPath]:
    source = _required_string(value, label)
    relative = PurePosixPath(source)
    if "\\" in source or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(
            f"{label} must be a repository-relative forward-slash path"
        )
    return source, relative


def _source_path(root: Path, value: object, label: str) -> str:
    source, relative = _relative_path(value, label)
    if not root.joinpath(*relative.parts).is_file():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _source_directory(root: Path, value: object, label: str) -> str:
    source, relative = _relative_path(value, label)
    if not root.joinpath(*relative.parts).is_dir():
        raise ValueError(f"{label} does not exist: {source}")
    return source


def _source_refs(
    root: Path, value: object, label: str
) -> list[dict[str, object]]:
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
                raise ValueError(
                    f"{item_label} anchor is missing from {path}: {anchor}"
                )
        result.append({"path": path, "anchors": anchors})
    return result


def _validate_scope(raw: object) -> dict[str, object]:
    if not isinstance(raw, dict) or raw.get("surface") != "gui-runtime":
        raise ValueError("scope.surface must be gui-runtime")
    stability = _required_string(raw.get("stability"), "scope.stability")
    if stability not in VALID_STABILITY:
        raise ValueError(f"unsupported scope.stability: {stability}")
    since = raw.get("since")
    if since is not None and (
        not isinstance(since, str) or not since.strip()
    ):
        raise ValueError("scope.since must be null or a non-empty string")
    _required_string(raw.get("support_note"), "scope.support_note")
    _string_list(raw.get("included"), "scope.included")
    _string_list(raw.get("excluded"), "scope.excluded")
    return copy.deepcopy(raw)


def _validate_sources(root: Path, raw: object) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("sources must be an object")
    result = {
        field: _source_path(root, raw.get(field), f"sources.{field}")
        for field in ("gui_script", "input_script", "client_runtime", "tutorial")
    }
    result["native_test_directory"] = _source_directory(
        root,
        raw.get("native_test_directory"),
        "sources.native_test_directory",
    )
    return result


def _validate_notes(
    raw: object, label: str, expected_names: list[str]
) -> dict[str, str]:
    if not isinstance(raw, dict):
        raise ValueError(f"{label} must be an object")
    expected = set(expected_names)
    actual = set(raw)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise ValueError(f"{label} keys differ; missing={missing}, extra={extra}")
    return {
        name: _required_string(raw.get(name), f"{label}.{name}")
        for name in expected_names
    }


def _kebab(value: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1-\2", value)
    value = re.sub(r"[^A-Za-z0-9]+", "-", value)
    return value.strip("-").lower()


def _section(text: str, start: str, end: str) -> str:
    if start not in text or end not in text:
        raise ValueError(f"unable to locate GUI source section {start!r}")
    return text.split(start, 1)[1].split(end, 1)[0]


def _parse_comment_groups(section: str) -> list[dict[str, object]]:
    groups: list[dict[str, object]] = []
    current: dict[str, object] | None = None
    for line in section.splitlines():
        header = re.fullmatch(
            r"// ([A-Za-z_][A-Za-z0-9_]*)(?:\s*:\s*(.+))?", line
        )
        if header:
            base_chain = (
                [item.strip() for item in header.group(2).split(":")]
                if header.group(2)
                else []
            )
            current = {
                "name": header.group(1),
                "base_chain": base_chain,
                "members": [],
            }
            groups.append(current)
            continue
        member = re.fullmatch(r"//   (.+)", line)
        if member and current is not None:
            members = current["members"]
            assert isinstance(members, list)
            members.append(member.group(1))
    return groups


def _member_name(signature: str) -> str:
    declaration = signature.split(" - ", 1)[0]
    declaration = declaration.replace(" (overridable)", "")
    method = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", declaration)
    if method:
        return method.group(1)
    tokens = declaration.split()
    if not tokens:
        raise ValueError(f"unable to parse GUI API member: {signature}")
    return tokens[-1]


def _parse_gui_types(
    gui_text: str, notes: dict[str, str], gui_path: str
) -> list[dict[str, object]]:
    api_groups = _parse_comment_groups(
        _section(gui_text, "// API\n", "\nclass Object")
    )
    callback_groups = _parse_comment_groups(
        _section(gui_text, "// Callbacks\n", "// API\n")
    )
    callbacks_by_type = {
        str(group["name"]): list(group["members"])
        for group in callback_groups
    }
    classes = re.findall(
        r"^class\s+([A-Za-z_][A-Za-z0-9_]*)(?:\s*:\s*"
        r"([A-Za-z_][A-Za-z0-9_]*))?",
        gui_text,
        re.MULTILINE,
    )
    if [name for name, _ in classes] != [
        str(group["name"]) for group in api_groups
    ]:
        raise ValueError("GUI API comment type order differs from live classes")

    result: list[dict[str, object]] = []
    for (name, base), group in zip(classes, api_groups, strict=True):
        base_chain = list(group["base_chain"])
        documented_base = base_chain[0] if base_chain else ""
        if documented_base != base:
            raise ValueError(
                f"GUI API base for {name} is {documented_base!r}, "
                f"live class base is {base!r}"
            )
        members = list(group["members"])
        callbacks = callbacks_by_type.get(name, [])
        for declaration in [*members, *callbacks]:
            member_name = _member_name(str(declaration))
            if member_name not in gui_text:
                raise ValueError(
                    f"documented GUI member is missing from source: "
                    f"{name}.{member_name}"
                )
        result.append(
            {
                "id": f"gui-runtime.type.{_kebab(name)}",
                "name": name,
                "base": base or None,
                "members": members,
                "callbacks": callbacks,
                "stability": "experimental",
                "requirement": notes[name],
                "rationale": (
                    "This type and its documented members are declared by the "
                    "reusable Gui.fos object model."
                ),
                "source": [{"path": gui_path, "anchors": [f"class {name}"]}],
            }
        )
    return result


def _split_parameters(value: str) -> list[str]:
    if not value.strip():
        return []
    result: list[str] = []
    start = 0
    depth = 0
    for index, char in enumerate(value):
        if char in "<[(":
            depth += 1
        elif char in ">])":
            depth -= 1
        elif char == "," and depth == 0:
            result.append(value[start:index].strip())
            start = index + 1
    result.append(value[start:].strip())
    return result


def _parameter_type(parameter: str) -> str:
    declaration = parameter.split("=", 1)[0].strip()
    tokens = declaration.split()
    if len(tokens) < 2:
        return declaration
    return " ".join(tokens[:-1])


def _signature_suffix(parameters: str) -> str:
    parameter_types = [
        _parameter_type(item) for item in _split_parameters(parameters)
    ]
    return (
        ".".join(_kebab(item) for item in parameter_types)
        if parameter_types
        else "no-args"
    )


def _parse_screen_api(
    gui_text: str, notes: dict[str, str], gui_path: str
) -> list[dict[str, object]]:
    public_api = _section(
        gui_text, "// Public API\n", "// Engine callbacks\n"
    )
    pattern = re.compile(
        r"^(?!\s)([A-Za-z_][\w:<>,?\[\] ]*?)\s+"
        r"([A-Za-z_][A-Za-z0-9_]*)\(([^\n]*)\)$",
        re.MULTILINE,
    )
    matches = list(pattern.finditer(public_api))
    names = [match.group(2) for match in matches]
    if set(names) != set(notes):
        raise ValueError(
            "screen_api_notes must document every top-level public API name"
        )
    result: list[dict[str, object]] = []
    for match in matches:
        return_type = match.group(1).strip()
        name = match.group(2)
        parameters = match.group(3).strip()
        signature = f"{return_type} {name}({parameters})"
        result.append(
            {
                "id": (
                    f"gui-runtime.screen-api.{_kebab(name)}."
                    f"{_signature_suffix(parameters)}"
                ),
                "name": name,
                "signature": signature,
                "return_type": return_type,
                "parameters": _split_parameters(parameters),
                "stability": "experimental",
                "requirement": notes[name],
                "rationale": (
                    "This callable is declared in the Gui.fos Public API "
                    "section and implemented by the reusable screen runtime."
                ),
                "source": [{"path": gui_path, "anchors": [signature]}],
            }
        )
    ids = [str(entry["id"]) for entry in result]
    if len(ids) != len(set(ids)):
        raise ValueError("derived GUI screen API IDs are not unique")
    return result


def _parse_annotations(
    gui_text: str, notes: dict[str, str], gui_path: str
) -> tuple[list[dict[str, object]], dict[str, dict[str, int]]]:
    enum_matches = list(
        re.finditer(
            r"^///@ Enum ([A-Za-z_][A-Za-z0-9_]*) "
            r"([A-Za-z_][A-Za-z0-9_]*)(?:\s*=\s*([^\n]+))?",
            gui_text,
            re.MULTILINE,
        )
    )
    enum_values: dict[str, dict[str, int]] = {}
    enum_anchors: dict[str, list[str]] = {}
    next_values: dict[str, int] = {}
    for match in enum_matches:
        enum_name, value_name, explicit = match.groups()
        value = (
            int(explicit.strip(), 0)
            if explicit is not None
            else next_values.get(enum_name, 0)
        )
        enum_values.setdefault(enum_name, {})[value_name] = value
        enum_anchors.setdefault(enum_name, []).append(match.group(0))
        next_values[enum_name] = value + 1

    result: list[dict[str, object]] = []
    for enum_name, values in enum_values.items():
        result.append(
            {
                "id": f"gui-runtime.annotation.enum.{_kebab(enum_name)}",
                "name": enum_name,
                "kind": "enum",
                "values": values,
                "stability": "experimental",
                "requirement": notes[enum_name],
                "rationale": (
                    "The metadata annotation is compiled into the project "
                    "script enum surface."
                ),
                "source": [
                    {"path": gui_path, "anchors": enum_anchors[enum_name]}
                ],
            }
        )

    setting_pattern = re.compile(
        r"^///@ Setting (Client|Server|Common) "
        r"([A-Za-z_][A-Za-z0-9_:<>?]*) "
        r"([A-Za-z_][A-Za-z0-9_.]*)$",
        re.MULTILINE,
    )
    for match in setting_pattern.finditer(gui_text):
        side, value_type, name = match.groups()
        result.append(
            {
                "id": f"gui-runtime.annotation.setting.{_kebab(name)}",
                "name": name,
                "kind": "setting",
                "side": side,
                "value_type": value_type,
                "stability": "experimental",
                "requirement": notes[name],
                "rationale": (
                    "The setting annotation is part of the generated project "
                    "configuration contract."
                ),
                "source": [{"path": gui_path, "anchors": [match.group(0)]}],
            }
        )

    event_pattern = re.compile(
        r"^///@ Event (Client|Server|Common) "
        r"([A-Za-z_][A-Za-z0-9_]*) "
        r"([A-Za-z_][A-Za-z0-9_]*\([^\n]*\))$",
        re.MULTILINE,
    )
    for match in event_pattern.finditer(gui_text):
        side, owner, signature = match.groups()
        event_name = signature.split("(", 1)[0]
        qualified = f"{owner}.{event_name}"
        result.append(
            {
                "id": f"gui-runtime.annotation.event.{_kebab(qualified)}",
                "name": qualified,
                "kind": "event",
                "side": side,
                "signature": signature,
                "stability": "experimental",
                "requirement": notes[qualified],
                "rationale": (
                    "The event annotation is part of the generated client "
                    "script event surface."
                ),
                "source": [{"path": gui_path, "anchors": [match.group(0)]}],
            }
        )
    if set(notes) != {str(entry["name"]) for entry in result}:
        raise ValueError("annotation_notes differ from derived GUI annotations")
    return result, enum_values


def _derive_outputs(
    root: Path,
    sources: dict[str, object],
    types: list[dict[str, object]],
    screen_api: list[dict[str, object]],
    annotations: list[dict[str, object]],
    enum_values: dict[str, dict[str, int]],
) -> dict[str, object]:
    gui_text = (root / str(sources["gui_script"])).read_text(
        encoding="utf-8"
    )
    input_text = (root / str(sources["input_script"])).read_text(
        encoding="utf-8"
    )

    input_subscriptions = re.findall(
        r"Game\.(On[A-Za-z0-9_]+)\.Subscribe", input_text
    )
    repeat_block_match = re.search(
        r"if \(KeyPressed\[key\]\) \{(?P<body>.*?)\n    \}",
        input_text,
        re.DOTALL,
    )
    key_size_match = re.search(
        r"KeyPressed\.resize\((0x[0-9A-Fa-f]+|[0-9]+)\)", input_text
    )
    mouse_size_match = re.search(
        r"MousePressed\.resize\(([^)]+)\)", input_text
    )
    scroll_match = re.search(
        r"const int PanelScrollAnimationDurationMs = ([0-9]+);", gui_text
    )
    password_match = re.search(
        r"PasswordShowTime = Time::Milliseconds\(([0-9]+)\)", gui_text
    )
    repeat_initial = re.search(
        r"PressedObjectRepeatTime = Game\.FrameTime \+ "
        r"Time::Milliseconds\(([0-9]+)\)",
        gui_text,
    )
    repeat_interval = re.search(
        r"PressedObjectRepeatTime = tick \+ "
        r"Time::Milliseconds\(([0-9]+)\)",
        gui_text,
    )
    if None in (
        repeat_block_match,
        key_size_match,
        mouse_size_match,
        scroll_match,
        password_match,
        repeat_initial,
        repeat_interval,
    ):
        raise ValueError("unable to derive complete GUI timing/input outputs")
    assert repeat_block_match is not None
    repeatable_keys = re.findall(
        r"KeyCode::([A-Za-z_][A-Za-z0-9_]*)",
        repeat_block_match.group("body"),
    )
    integration_hooks = re.findall(
        r"^void ((?:EngineCallback|Callback)_[A-Za-z0-9_]+)\(",
        gui_text,
        re.MULTILINE,
    )
    external_initializer = re.search(
        r"(GuiScreens::InitializeScreens)\(\);", gui_text
    )
    if external_initializer is None:
        raise ValueError("GuiScreens::InitializeScreens hook is missing")

    engine_owned_roots = (root / "Source", root / "BuildTools", root / "Resources")
    formats: set[str] = set()
    for suffix in (".fogui", ".foguischeme"):
        if any(any(source_root.rglob(f"*{suffix}")) for source_root in engine_owned_roots):
            formats.add(suffix)

    test_root = root / str(sources["native_test_directory"])
    focused_tests = sorted(
        path.relative_to(root).as_posix()
        for path in test_root.glob("Test_*.cpp")
        if re.search(r"(GuiRuntime|CoreGui)", path.name, re.IGNORECASE)
    )
    return {
        "runtime_side": "client",
        "type_names": [str(entry["name"]) for entry in types],
        "type_count": len(types),
        "api_member_count": sum(
            len(entry["members"]) for entry in types
        ),
        "callback_signature_count": sum(
            len(entry["callbacks"]) for entry in types
        ),
        "screen_api_overload_count": len(screen_api),
        "annotation_count": len(annotations),
        "enum_values": enum_values,
        "settings": [
            str(entry["name"])
            for entry in annotations
            if entry["kind"] == "setting"
        ],
        "events": [
            str(entry["name"])
            for entry in annotations
            if entry["kind"] == "event"
        ],
        "input_subscriptions": [
            f"Game.{name}" for name in input_subscriptions
        ],
        "repeatable_keys": repeatable_keys,
        "integration_hooks": integration_hooks,
        "external_screen_initializer": external_initializer.group(1),
        "key_state_size": int(key_size_match.group(1), 0),
        "mouse_state_size_expression": mouse_size_match.group(1).strip(),
        "panel_scroll_animation_ms": int(scroll_match.group(1)),
        "password_reveal_ms": int(password_match.group(1)),
        "press_repeat_initial_ms": int(repeat_initial.group(1)),
        "press_repeat_interval_ms": int(repeat_interval.group(1)),
        "engine_authored_file_formats": sorted(formats),
        "focused_native_test_files": focused_tests,
    }


def _validate_outputs(
    raw: object, expected: dict[str, object]
) -> dict[str, object]:
    if not isinstance(raw, dict):
        raise ValueError("outputs must be an object")
    result = copy.deepcopy(raw)
    for field, value in expected.items():
        if result.get(field) != value:
            raise ValueError(
                f"outputs.{field} must match the live source: {value}"
            )
    return result


def _validate_rule_entries(
    root: Path,
    collection: str,
    raw: object,
    identities: set[str],
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
        if (
            ENTRY_ID_PATTERN.fullmatch(identity) is None
            or not identity.startswith(f"gui-runtime.{kind}.")
        ):
            raise ValueError(f"invalid {label}.id: {identity}")
        if identity in identities:
            raise ValueError(f"duplicate GUI runtime entry id: {identity}")
        identities.add(identity)
        _required_string(entry.get("name"), f"{label}.name")
        stability = _required_string(
            entry.get("stability"), f"{label}.stability"
        )
        if stability not in VALID_STABILITY:
            raise ValueError(f"unsupported {label}.stability: {stability}")
        _required_string(entry.get("requirement"), f"{label}.requirement")
        _required_string(entry.get("rationale"), f"{label}.rationale")
        enriched = copy.deepcopy(entry)
        enriched["source"] = _source_refs(
            root, entry.get("source"), f"{label}.source"
        )
        result.append(enriched)
    return result


def _validate_derived_entries(
    entries: list[dict[str, object]], identities: set[str]
) -> None:
    for entry in entries:
        identity = str(entry["id"])
        if ENTRY_ID_PATTERN.fullmatch(identity) is None:
            raise ValueError(f"invalid derived GUI runtime id: {identity}")
        if identity in identities:
            raise ValueError(f"duplicate GUI runtime entry id: {identity}")
        identities.add(identity)


def generate_gui_runtime_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(
            f"unable to read GUI runtime manifest "
            f"{manifest_relative_path}: {exception}"
        ) from exception
    if not isinstance(raw, dict) or raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"GUI runtime manifest schema_version must be {SCHEMA_VERSION}"
        )

    description = _required_string(raw.get("description"), "description")
    scope = _validate_scope(raw.get("scope"))
    sources = _validate_sources(root, raw.get("sources"))
    gui_text = (root / str(sources["gui_script"])).read_text(
        encoding="utf-8"
    )
    class_names = [
        match.group(1)
        for match in re.finditer(
            r"^class\s+([A-Za-z_][A-Za-z0-9_]*)",
            gui_text,
            re.MULTILINE,
        )
    ]
    type_notes = _validate_notes(raw.get("type_notes"), "type_notes", class_names)
    screen_names = [
        match.group(1)
        for match in re.finditer(
            r"^(?!\s)[A-Za-z_][\w:<>,?\[\] ]*?\s+"
            r"([A-Za-z_][A-Za-z0-9_]*)\([^\n]*\)$",
            _section(gui_text, "// Public API\n", "// Engine callbacks\n"),
            re.MULTILINE,
        )
    ]
    screen_api_notes = _validate_notes(
        raw.get("screen_api_notes"),
        "screen_api_notes",
        list(dict.fromkeys(screen_names)),
    )
    annotation_names = [
        *dict.fromkeys(
            re.findall(
                r"^///@ Enum ([A-Za-z_][A-Za-z0-9_]*) ",
                gui_text,
                re.MULTILINE,
            )
        ),
        *re.findall(
            r"^///@ Setting (?:Client|Server|Common) "
            r"[A-Za-z_][A-Za-z0-9_:<>?]* "
            r"([A-Za-z_][A-Za-z0-9_.]*)$",
            gui_text,
            re.MULTILINE,
        ),
        *[
            f"{owner}.{signature.split('(', 1)[0]}"
            for owner, signature in re.findall(
                r"^///@ Event (?:Client|Server|Common) "
                r"([A-Za-z_][A-Za-z0-9_]*) "
                r"([A-Za-z_][A-Za-z0-9_]*\([^\n]*\))$",
                gui_text,
                re.MULTILINE,
            )
        ],
    ]
    annotation_notes = _validate_notes(
        raw.get("annotation_notes"),
        "annotation_notes",
        annotation_names,
    )

    types = _parse_gui_types(
        gui_text, type_notes, str(sources["gui_script"])
    )
    screen_api = _parse_screen_api(
        gui_text, screen_api_notes, str(sources["gui_script"])
    )
    annotations, enum_values = _parse_annotations(
        gui_text, annotation_notes, str(sources["gui_script"])
    )
    outputs = _validate_outputs(
        raw.get("outputs"),
        _derive_outputs(
            root,
            sources,
            types,
            screen_api,
            annotations,
            enum_values,
        ),
    )

    identities: set[str] = set()
    for entries in (types, screen_api, annotations):
        _validate_derived_entries(entries, identities)
    rules = {
        collection: _validate_rule_entries(
            root, collection, raw.get(collection), identities
        )
        for collection in RULE_COLLECTIONS
    }
    collections: dict[str, list[dict[str, object]]] = {
        "types": types,
        "screen_api": screen_api,
        "annotations": annotations,
        **rules,
    }
    stability_counts = Counter(
        str(entry["stability"])
        for entries in collections.values()
        for entry in entries
    )
    summary_counts = {
        f"{collection.removesuffix('s')}_count": len(entries)
        for collection, entries in collections.items()
    }
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
        **collections,
        "summary": {
            "entry_count": sum(
                len(entries) for entries in collections.values()
            ),
            **summary_counts,
            "entries_by_stability": dict(sorted(stability_counts.items())),
        },
    }
    canonical = json.dumps(
        model, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    )
    model["contract_digest"] = hashlib.sha256(
        canonical.encode("utf-8")
    ).hexdigest()
    return model


def render_gui_runtime_model(
    root: Path, manifest_relative_path: str = DEFAULT_MANIFEST
) -> str:
    return (
        json.dumps(
            generate_gui_runtime_model(root, manifest_relative_path),
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
        "`BuildTools/GuiRuntimeInterface.json`, then run "
        "`python BuildTools/docs_gui_runtime.py --write`.",
        "",
        "[Index](index.md) | [Types](types.md) | "
        "[Screen API](screen-api.md) | [Lifecycle](lifecycle.md) | "
        "[Layout](layout-rendering.md) | [Input](input.md) | "
        "[Integration](integration-validation.md) | "
        "[Canonical JSON](../../../generated/gui-runtime.json) | "
        "[Guide](../../how-to/runtime/gui.md)",
        "",
    ]


def _entry_anchor(entry: dict[str, object]) -> str:
    return (
        f'<a id="{docs_cli._anchor("entry", str(entry["id"]))}"></a>'
        f'<code>{docs_cli._text(entry["id"])}</code>'
    )


def _render_legacy_page(
    canonical_path: str,
    title: str,
    canonical_content: str,
) -> str:
    filename = PurePosixPath(canonical_path).name
    english_path = f"../../en/reference/gui-runtime/{filename}"
    russian_path = f"../../ru/reference/gui-runtime/{filename}"
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
    return [
        (
            _entry_anchor(entry),
            docs_cli._text(entry["name"]),
            docs_cli._text(entry["requirement"]),
            docs_cli._text(entry["rationale"]),
            _source_ref_links(model, entry["source"]),
        )
        for entry in model[collection]
        if isinstance(entry, dict)
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
            "This reference describes the Engine-owned AngelScript GUI runtime. "
            "It is not a declarative GUI file-format specification: screen "
            "source formats, generators, catalogs, styles, and project hook "
            "implementations remain embedding-project concerns.",
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
            ("Runtime side", docs_cli._code(outputs["runtime_side"])),
            ("Runtime types", str(outputs["type_count"])),
            ("Documented type members", str(outputs["api_member_count"])),
            (
                "Callback signatures",
                str(outputs["callback_signature_count"]),
            ),
            (
                "Top-level API overloads",
                str(outputs["screen_api_overload_count"]),
            ),
            (
                "Engine declarative GUI formats",
                str(len(outputs["engine_authored_file_formats"])),
            ),
            (
                "Focused native runtime tests",
                str(len(outputs["focused_native_test_files"])),
            ),
        ],
    )
    docs_cli._table(
        lines,
        ("Reference", "Entries", "Purpose"),
        [
            (
                "[Types](types.md)",
                str(summary["type_count"]),
                "Object hierarchy, documented members, and callbacks.",
            ),
            (
                "[Screen API](screen-api.md)",
                str(summary["screen_api_count"]),
                "Registration, stack, focus, lookup, and drag/drop callables.",
            ),
            (
                "[Lifecycle](lifecycle.md)",
                str(summary["lifecycle_rule_count"]),
                "Creation, show/hide, cursor, and refresh behavior.",
            ),
            (
                "[Layout](layout-rendering.md)",
                str(summary["layout_rule_count"]),
                "Coordinates, docking, anchors, crop, frames, scroll, and grids.",
            ),
            (
                "[Input](input.md)",
                str(summary["input_rule_count"]),
                "Subscriptions, hit order, focus, repeat, drag, and loss.",
            ),
            (
                "[Integration](integration-validation.md)",
                str(
                    summary["integration_rule_count"]
                    + summary["validation_rule_count"]
                ),
                "Embedding-project ownership and validation gates.",
            ),
        ],
    )
    lines.extend(["## Boundary", "", "Included:", ""])
    lines.extend(f"- {item}" for item in scope["included"])
    lines.extend(["", "Excluded:", ""])
    lines.extend(f"- {item}" for item in scope["excluded"])
    lines.append("")
    return "\n".join(lines)


def _render_types(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[1][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Type", "Base", "Members", "Callbacks", "Role"),
        [
            (
                _entry_anchor(entry),
                docs_cli._code(entry["name"]),
                (
                    docs_cli._code(entry["base"])
                    if entry["base"] is not None
                    else "-"
                ),
                str(len(entry["members"])),
                str(len(entry["callbacks"])),
                docs_cli._text(entry["requirement"]),
            )
            for entry in model["types"]
            if isinstance(entry, dict)
        ],
    )
    for entry in model["types"]:
        assert isinstance(entry, dict)
        lines.extend(
            [
                f"## {entry['name']}",
                "",
                f"Base: {docs_cli._code(entry['base']) if entry['base'] else 'none'}",
                f"Source: {_source_ref_links(model, entry['source'])}",
                "",
                docs_cli._text(entry["requirement"]),
                "",
                "### Members",
                "",
            ]
        )
        members = entry["members"]
        assert isinstance(members, list)
        lines.extend(f"- {docs_cli._code(member)}" for member in members)
        callbacks = entry["callbacks"]
        assert isinstance(callbacks, list)
        lines.extend(["", "### Callbacks", ""])
        if callbacks:
            lines.extend(f"- {docs_cli._code(item)}" for item in callbacks)
        else:
            lines.append("- None declared by this type.")
        lines.append("")
    return "\n".join(lines)


def _render_screen_api(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[2][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Signature", "Contract", "Source"),
        [
            (
                _entry_anchor(entry),
                docs_cli._code(entry["signature"]),
                docs_cli._text(entry["requirement"]),
                _source_ref_links(model, entry["source"]),
            )
            for entry in model["screen_api"]
            if isinstance(entry, dict)
        ],
    )
    lines.extend(["## Metadata annotations", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Kind", "Name", "Contract", "Source"),
        [
            (
                _entry_anchor(entry),
                docs_cli._code(entry["kind"]),
                docs_cli._code(entry["name"]),
                docs_cli._text(entry["requirement"]),
                _source_ref_links(model, entry["source"]),
            )
            for entry in model["annotations"]
            if isinstance(entry, dict)
        ],
    )
    return "\n".join(lines)


def _render_rules(
    model: dict[str, object], page_index: int, collection: str
) -> str:
    lines = _header(*PAGE_DEFINITIONS[page_index][1:])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _rule_rows(model, collection),
    )
    return "\n".join(lines)


def _render_integration_validation(model: dict[str, object]) -> str:
    lines = _header(*PAGE_DEFINITIONS[6][1:])
    lines.extend(["## Integration rules", ""])
    docs_cli._table(
        lines,
        ("Stable ID", "Rule", "Requirement", "Why", "Source"),
        _rule_rows(model, "integration_rules"),
    )
    lines.extend(["## Validation rules", ""])
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
            "python BuildTools\\docs_gui_runtime.py --check",
            "python -m unittest BuildTools.tests.test_docs_gui_runtime",
            "python BuildTools\\docs_validate.py",
            "```",
            "",
            "These checks prove source/model/reference consistency. They do not "
            "replace compiling an embedding client or visibly testing layout, "
            "draw order, input, language, fonts, and teardown.",
            "",
        ]
    )
    return "\n".join(lines)


RUSSIAN_TITLES = {
    "Generated GUI Runtime Reference": "Сгенерированный справочник GUI Runtime",
    "GUI Runtime Types": "Типы GUI Runtime",
    "GUI Screen API": "API экранов GUI",
    "GUI Screen Lifecycle": "Жизненный цикл экранов GUI",
    "GUI Layout And Rendering": "Компоновка и отрисовка GUI",
    "GUI Input Contract": "Контракт ввода GUI",
    "GUI Integration And Validation": "Интеграция и проверка GUI",
}

RUSSIAN_REPLACEMENTS = {
    "> Generated reference. Do not edit directly. Update `BuildTools/GuiRuntimeInterface.json`, then run `python BuildTools/docs_gui_runtime.py --write`.":
        "> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/GuiRuntimeInterface.json`, затем выполните `python BuildTools/docs_gui_runtime.py --write`.",
    "[Index](index.md) | [Types](types.md) | [Screen API](screen-api.md) | [Lifecycle](lifecycle.md) | [Layout](layout-rendering.md) | [Input](input.md) | [Integration](integration-validation.md) | [Canonical JSON](../../../generated/gui-runtime.json) | [Guide](../../how-to/runtime/gui.md)":
        "[Индекс](index.md) | [Типы](types.md) | [API экранов](screen-api.md) | [Жизненный цикл](lifecycle.md) | [Компоновка](layout-rendering.md) | [Ввод](input.md) | [Интеграция](integration-validation.md) | [Канонический JSON](../../../generated/gui-runtime.json) | [Руководство](../../how-to/runtime/gui.md)",
    "This reference describes the Engine-owned AngelScript GUI runtime. It is not a declarative GUI file-format specification: screen source formats, generators, catalogs, styles, and project hook implementations remain embedding-project concerns.":
        "Этот справочник описывает принадлежащий Engine GUI runtime на AngelScript. Это не спецификация декларативного формата GUI: исходные форматы экранов, генераторы, каталоги, стили и реализации проектных hooks остаются ответственностью подключаемого проекта.",
    "## Contract status": "## Состояние контракта",
    "Stability": "Стабильность",
    "Support policy": "Политика поддержки",
    "Source manifest": "Исходный манифест",
    "Contract digest": "Дайджест контракта",
    "Runtime side": "Сторона runtime",
    "Runtime types": "Типы runtime",
    "Documented type members": "Документированные члены типов",
    "Callback signatures": "Сигнатуры callback-функций",
    "Top-level API overloads": "Перегрузки верхнеуровневого API",
    "Engine declarative GUI formats": "Декларативные GUI-форматы Engine",
    "Focused native runtime tests": "Целевые нативные тесты runtime",
    "Object hierarchy, documented members, and callbacks.":
        "Иерархия объектов, документированные члены и callbacks.",
    "Registration, stack, focus, lookup, and drag/drop callables.":
        "Функции регистрации, стека, фокуса, поиска и drag/drop.",
    "Creation, show/hide, cursor, and refresh behavior.":
        "Создание, show/hide, курсор и поведение обновления.",
    "Coordinates, docking, anchors, crop, frames, scroll, and grids.":
        "Координаты, docking, anchors, crop, рамки, прокрутка и grids.",
    "Subscriptions, hit order, focus, repeat, drag, and loss.":
        "Подписки, порядок hit, фокус, repeat, drag и потеря ввода.",
    "Embedding-project ownership and validation gates.":
        "Ответственность подключаемого проекта и gate проверки.",
    "## Boundary": "## Граница ответственности",
    "Included:": "Включено:",
    "Excluded:": "Исключено:",
    "Base: none": "Базовый тип: отсутствует",
    "Base: ": "Базовый тип: ",
    "Source: ": "Источник: ",
    "### Members": "### Члены",
    "### Callbacks": "### Callback-функции",
    "- None declared by this type.": "- Этот тип не объявляет callback-функций.",
    "## Metadata annotations": "## Аннотации метаданных",
    "## Integration rules": "## Правила интеграции",
    "## Validation rules": "## Правила проверки",
    "## Validation commands": "## Команды проверки",
    "These checks prove source/model/reference consistency. They do not replace compiling an embedding client or visibly testing layout, draw order, input, language, fonts, and teardown.":
        "Эти проверки доказывают согласованность исходников, модели и справочника. Они не заменяют компиляцию подключаемого клиента и видимую проверку компоновки, порядка отрисовки, ввода, языка, шрифтов и teardown.",
}

RUSSIAN_TABLE_HEADERS = {
    "| Field | Value |": "| Поле | Значение |",
    "| Reference | Entries | Purpose |": "| Справочник | Записи | Назначение |",
    "| Stable ID | Type | Base | Members | Callbacks | Role |":
        "| Стабильный ID | Тип | Базовый тип | Члены | Callbacks | Роль |",
    "| Stable ID | Signature | Contract | Source |":
        "| Стабильный ID | Сигнатура | Контракт | Источник |",
    "| Stable ID | Kind | Name | Contract | Source |":
        "| Стабильный ID | Вид | Имя | Контракт | Источник |",
    "| Stable ID | Rule | Requirement | Why | Source |":
        "| Стабильный ID | Правило | Требование | Причина | Источник |",
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
        raise ValueError("generated GUI runtime page has no front matter")
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
        raise ValueError("unsupported generated GUI runtime model")
    identities = [
        entry.get("id")
        for collection in COLLECTION_KINDS
        for entry in model.get(collection, [])
        if isinstance(entry, dict)
    ]
    if (
        any(
            not isinstance(identity, str) or not identity
            for identity in identities
        )
        or len(identities) != len(set(identities))
    ):
        raise ValueError(
            "every GUI runtime entry must have a unique non-empty ID"
        )
    canonical_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(model),
        f"{DEFAULT_OUTPUT_DIR}/types.md": _render_types(model),
        f"{DEFAULT_OUTPUT_DIR}/screen-api.md": _render_screen_api(model),
        f"{DEFAULT_OUTPUT_DIR}/lifecycle.md": _render_rules(
            model, 3, "lifecycle_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/layout-rendering.md": _render_rules(
            model, 4, "layout_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/input.md": _render_rules(
            model, 5, "input_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/integration-validation.md": (
            _render_integration_validation(model)
        ),
    }
    localized_model = model if russian_model is None else russian_model
    russian_base_pages = {
        f"{DEFAULT_OUTPUT_DIR}/index.md": _render_index(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/types.md": _render_types(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/screen-api.md": _render_screen_api(localized_model),
        f"{DEFAULT_OUTPUT_DIR}/lifecycle.md": _render_rules(
            localized_model, 3, "lifecycle_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/layout-rendering.md": _render_rules(
            localized_model, 4, "layout_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/input.md": _render_rules(
            localized_model, 5, "input_rules"
        ),
        f"{DEFAULT_OUTPUT_DIR}/integration-validation.md": (
            _render_integration_validation(localized_model)
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
        raise ValueError(
            "generated GUI runtime page set does not match OUTPUT_PATHS"
        )
    return {
        path: content.rstrip() + "\n"
        for path, content in sorted(pages.items())
    }


def render_reference_pages(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    model = generate_gui_runtime_model(root, manifest_relative_path)
    russian_model = docs_description_translations.apply_translations(
        root,
        "gui-runtime",
        model,
    )
    return generate_reference_pages(model, russian_model)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate the FOnline GUI runtime model and reference"
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
        model_content = render_gui_runtime_model(root, args.manifest)
        model = json.loads(model_content)
        russian_model = docs_description_translations.apply_translations(
            root,
            "gui-runtime",
            model,
        )
        pages = generate_reference_pages(model, russian_model)
    except (OSError, ImportError, json.JSONDecodeError, ValueError) as exception:
        print(
            f"Unable to generate GUI runtime documentation: {exception}",
            file=sys.stderr,
        )
        return 1
    outputs = {DEFAULT_MODEL: model_content, **pages}
    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        print(
            f"Wrote GUI runtime model and {len(pages)} reference pages"
        )
        return 0
    stale = [
        path
        for path, content in outputs.items()
        if not (root / path).is_file()
        or (root / path).read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Generated GUI runtime documentation is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_gui_runtime.py --write",
            file=sys.stderr,
        )
        return 1
    summary = model["summary"]
    print(
        "Generated GUI runtime documentation is current: "
        f"{summary['entry_count']} entries, "
        f"{summary['type_count']} types, "
        f"{summary['screen_api_count']} screen API overloads"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
