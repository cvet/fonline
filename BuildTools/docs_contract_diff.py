from __future__ import annotations

import argparse
import copy
import hashlib
import html
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path
from typing import Any

import docs_api
import docs_api_diff
import docs_ai_control_protocol
import docs_audio
import docs_cli
import docs_cmake
import docs_effect_format
import docs_font_format
import docs_gui_runtime
import docs_helper_cli
import docs_image_format
import docs_map_format
import docs_model_format
import docs_native_extension
import docs_package
import docs_particle_format
import docs_prototype_format
import docs_text_format
import docs_video


SCHEMA_VERSION = 1
DOMAIN_ORDER = (
    "api",
    "cmake",
    "cli",
    "package",
    "helper-cli",
    "native-extension",
    "prototype-format",
    "map-format",
    "model-format",
    "text-format",
    "effect-format",
    "image-format",
    "particle-format",
    "font-format",
    "audio",
    "video",
    "gui-runtime",
    "ai-control-protocol",
)
DEFAULT_CURRENT_DIR = "Docs/generated"
DEFAULT_DISPOSITIONS = docs_api_diff.DEFAULT_DISPOSITIONS
DEFAULT_JSON_OUTPUT = "Workspace/contract-diff.json"
DEFAULT_MARKDOWN_OUTPUT = "Workspace/contract-diff.md"

MODEL_FILES = {
    "api": Path(docs_api.DEFAULT_OUTPUT).name,
    "cmake": Path(docs_cmake.DEFAULT_MODEL).name,
    "cli": Path(docs_cli.DEFAULT_MODEL).name,
    "package": Path(docs_package.DEFAULT_MODEL).name,
    "helper-cli": Path(docs_helper_cli.DEFAULT_MODEL).name,
    "native-extension": Path(docs_native_extension.DEFAULT_MODEL).name,
    "prototype-format": Path(docs_prototype_format.DEFAULT_MODEL).name,
    "map-format": Path(docs_map_format.DEFAULT_MODEL).name,
    "model-format": Path(docs_model_format.DEFAULT_MODEL).name,
    "text-format": Path(docs_text_format.DEFAULT_MODEL).name,
    "effect-format": Path(docs_effect_format.DEFAULT_MODEL).name,
    "image-format": Path(docs_image_format.DEFAULT_MODEL).name,
    "particle-format": Path(docs_particle_format.DEFAULT_MODEL).name,
    "font-format": Path(docs_font_format.DEFAULT_MODEL).name,
    "audio": Path(docs_audio.DEFAULT_MODEL).name,
    "video": Path(docs_video.DEFAULT_MODEL).name,
    "gui-runtime": Path(docs_gui_runtime.DEFAULT_MODEL).name,
    "ai-control-protocol": Path(docs_ai_control_protocol.DEFAULT_MODEL).name,
}
EXPECTED_SCHEMAS = {
    "api": docs_api.SCHEMA_VERSION,
    "cmake": docs_cmake.SCHEMA_VERSION,
    "cli": docs_cli.SCHEMA_VERSION,
    "package": docs_package.SCHEMA_VERSION,
    "helper-cli": docs_helper_cli.SCHEMA_VERSION,
    "native-extension": docs_native_extension.SCHEMA_VERSION,
    "prototype-format": docs_prototype_format.SCHEMA_VERSION,
    "map-format": docs_map_format.SCHEMA_VERSION,
    "model-format": docs_model_format.SCHEMA_VERSION,
    "text-format": docs_text_format.SCHEMA_VERSION,
    "effect-format": docs_effect_format.SCHEMA_VERSION,
    "image-format": docs_image_format.SCHEMA_VERSION,
    "particle-format": docs_particle_format.SCHEMA_VERSION,
    "font-format": docs_font_format.SCHEMA_VERSION,
    "audio": docs_audio.SCHEMA_VERSION,
    "video": docs_video.SCHEMA_VERSION,
    "gui-runtime": docs_gui_runtime.SCHEMA_VERSION,
    "ai-control-protocol": docs_ai_control_protocol.SCHEMA_VERSION,
}
SOURCE_IDENTITY_FIELDS = {
    "cmake": ("source_manifest",),
    "cli": ("source_parser",),
    "package": ("source_manifest", "source_parser"),
    "helper-cli": ("source_manifest",),
    "native-extension": ("source_manifest", "project_interface"),
    "prototype-format": (
        "source_manifest",
        "source_parser",
        "config_parser",
        "property_parser",
        "property_serializator",
        "api_model_generator",
    ),
    "map-format": (
        "source_manifest",
        "source_loader",
        "config_parser",
        "source_baker",
        "source_mapper",
        "api_model_generator",
    ),
    "model-format": ("source_manifest",),
    "text-format": ("source_manifest",),
    "effect-format": ("source_manifest",),
    "image-format": ("source_manifest",),
    "particle-format": ("source_manifest",),
    "font-format": ("source_manifest",),
    "audio": ("source_manifest",),
    "video": ("source_manifest",),
    "gui-runtime": ("source_manifest",),
    "ai-control-protocol": ("source_manifest",),
}
COLLECTIONS = {
    "cmake": ("options", "stages", "helpers"),
    "package": ("targets", "platforms", "packs", "payloads"),
}
DOC_FIELDS = {"description", "examples", "help_output", "notes", "summary", "support_note"}
POLICY_FIELDS = {"deprecated", "since", "stability"}
IGNORED_FIELDS = {
    "contract_digest",
    "enum_source",
    "generated_by",
    "source",
    "source_ref",
    "usage",
}
ENTRY_ID_PATTERN = {
    "cmake": re.compile(r"^cmake\."),
    "cli": re.compile(r"^cli\."),
    "package": re.compile(r"^package\."),
    "helper-cli": re.compile(r"^helper-cli\."),
    "native-extension": re.compile(r"^native-extension\."),
    "prototype-format": re.compile(r"^prototype-format\."),
    "map-format": re.compile(r"^map-format\."),
    "model-format": re.compile(r"^model-format\."),
    "text-format": re.compile(r"^text-format\."),
    "effect-format": re.compile(r"^effect-format\."),
    "image-format": re.compile(r"^image-format\."),
    "particle-format": re.compile(r"^particle-format\."),
    "font-format": re.compile(r"^font-format\."),
    "audio": re.compile(r"^audio\."),
    "video": re.compile(r"^video\."),
    "gui-runtime": re.compile(r"^gui-runtime\."),
    "ai-control-protocol": re.compile(r"^ai-control-protocol\."),
}


class ContractDiffError(ValueError):
    pass


class BaselineUnavailable(ContractDiffError):
    pass


def _canonical_bytes(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def _digest(value: object) -> str:
    return hashlib.sha256(_canonical_bytes(value)).hexdigest()


def _read_json_text(text: str, label: str) -> dict[str, Any]:
    try:
        value = json.loads(text)
    except json.JSONDecodeError as exception:
        raise ContractDiffError(f"Invalid JSON in {label}: {exception}") from exception
    if not isinstance(value, dict):
        raise ContractDiffError(f"{label} must contain a JSON object")
    return value


def _require_object(value: object, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ContractDiffError(f"{label} must be an object")
    return value


def _require_array(value: object, label: str) -> list[Any]:
    if not isinstance(value, list):
        raise ContractDiffError(f"{label} must be an array")
    return value


def _flatten_entries(domain: str, model: dict[str, Any]) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []

    def append_entry(value: object, label: str, *, excluded: set[str] | None = None) -> None:
        entry = copy.deepcopy(_require_object(value, label))
        for field in excluded or set():
            entry.pop(field, None)
        entry_id = entry.get("id")
        if not isinstance(entry_id, str) or not entry_id:
            raise ContractDiffError(f"{label} has no stable id")
        if not ENTRY_ID_PATTERN[domain].match(entry_id):
            raise ContractDiffError(f"{label} has an invalid {domain} entry id: {entry_id}")
        entries.append(entry)

    if domain == "cmake":
        for collection in COLLECTIONS[domain]:
            for index, entry in enumerate(_require_array(model.get(collection), f"{domain}.{collection}")):
                append_entry(entry, f"{domain}.{collection}[{index}]")
    elif domain == "cli":
        for index, argument in enumerate(_require_array(model.get("global_arguments"), "cli.global_arguments")):
            append_entry(argument, f"cli.global_arguments[{index}]")
        for command_index, command_value in enumerate(_require_array(model.get("commands"), "cli.commands")):
            command = _require_object(command_value, f"cli.commands[{command_index}]")
            append_entry(command, f"cli.commands[{command_index}]", excluded={"arguments"})
            for argument_index, argument in enumerate(_require_array(command.get("arguments"), f"cli.commands[{command_index}].arguments")):
                append_entry(argument, f"cli.commands[{command_index}].arguments[{argument_index}]")
    elif domain == "package":
        declaration = _require_object(model.get("declaration"), "package.declaration")
        append_entry(declaration, "package.declaration", excluded={"clauses", "options"})
        for collection in ("clauses", "options"):
            for index, entry in enumerate(_require_array(declaration.get(collection), f"package.declaration.{collection}")):
                append_entry(entry, f"package.declaration.{collection}[{index}]")
        for collection in COLLECTIONS[domain]:
            for index, entry in enumerate(_require_array(model.get(collection), f"package.{collection}")):
                append_entry(entry, f"package.{collection}[{index}]")
        package_cli = _require_object(model.get("cli"), "package.cli")
        for index, argument in enumerate(_require_array(package_cli.get("arguments"), "package.cli.arguments")):
            append_entry(argument, f"package.cli.arguments[{index}]")
    elif domain == "helper-cli":
        for helper_index, helper_value in enumerate(_require_array(model.get("helpers"), "helper-cli.helpers")):
            helper = _require_object(helper_value, f"helper-cli.helpers[{helper_index}]")
            append_entry(
                helper,
                f"helper-cli.helpers[{helper_index}]",
                excluded={"global_arguments", "commands"},
            )
            for argument_index, argument in enumerate(
                _require_array(helper.get("global_arguments"), f"helper-cli.helpers[{helper_index}].global_arguments")
            ):
                append_entry(
                    argument,
                    f"helper-cli.helpers[{helper_index}].global_arguments[{argument_index}]",
                )
            for command_index, command_value in enumerate(
                _require_array(helper.get("commands"), f"helper-cli.helpers[{helper_index}].commands")
            ):
                command = _require_object(
                    command_value,
                    f"helper-cli.helpers[{helper_index}].commands[{command_index}]",
                )
                append_entry(
                    command,
                    f"helper-cli.helpers[{helper_index}].commands[{command_index}]",
                    excluded={"arguments"},
                )
                for argument_index, argument in enumerate(
                    _require_array(
                        command.get("arguments"),
                        f"helper-cli.helpers[{helper_index}].commands[{command_index}].arguments",
                    )
                ):
                    append_entry(
                        argument,
                        f"helper-cli.helpers[{helper_index}].commands[{command_index}].arguments[{argument_index}]",
                    )
    elif domain == "native-extension":
        for collection in ("roles", "hooks", "binding_rules"):
            for index, entry in enumerate(_require_array(model.get(collection), f"native-extension.{collection}")):
                append_entry(entry, f"native-extension.{collection}[{index}]")
    elif domain == "prototype-format":
        for collection in ("section_forms", "directives", "rules", "entity_types", "properties"):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"prototype-format.{collection}")
            ):
                append_entry(entry, f"prototype-format.{collection}[{index}]")
    elif domain == "map-format":
        for collection in ("sections", "directives", "ownerships", "rules", "properties"):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"map-format.{collection}")
            ):
                append_entry(entry, f"map-format.{collection}[{index}]")
    elif domain == "model-format":
        for collection in ("compile_limits", "assets", "tokens", "rules"):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"model-format.{collection}")
            ):
                append_entry(entry, f"model-format.{collection}[{index}]")
    elif domain == "text-format":
        for collection in (
            "syntax_rules",
            "language_rules",
            "proto_text_rules",
            "runtime_methods",
            "rendering_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"text-format.{collection}")
            ):
                append_entry(entry, f"text-format.{collection}[{index}]")
    elif domain == "effect-format":
        for collection in (
            "compile_limits",
            "sections",
            "effect_options",
            "resources",
            "baking_rules",
            "runtime_rules",
            "script_methods",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"effect-format.{collection}")
            ):
                append_entry(entry, f"effect-format.{collection}[{index}]")
    elif domain == "image-format":
        for collection in (
            "formats",
            "descriptor_fields",
            "filename_options",
            "baking_rules",
            "runtime_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"image-format.{collection}")
            ):
                append_entry(entry, f"image-format.{collection}[{index}]")
    elif domain == "particle-format":
        for collection in (
            "object_families",
            "objects",
            "xml_rules",
            "renderer_fields",
            "tooling_rules",
            "runtime_rules",
            "integration_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"particle-format.{collection}")
            ):
                append_entry(entry, f"particle-format.{collection}[{index}]")
    elif domain == "font-format":
        for collection in (
            "formats",
            "fofnt_fields",
            "bmfont_rules",
            "binding_rules",
            "layout_rules",
            "rendering_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"font-format.{collection}")
            ):
                append_entry(entry, f"font-format.{collection}[{index}]")
    elif domain == "audio":
        for collection in (
            "formats",
            "delivery_rules",
            "decoding_rules",
            "playback_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"audio.{collection}")
            ):
                append_entry(entry, f"audio.{collection}[{index}]")
    elif domain == "video":
        for collection in (
            "formats",
            "delivery_rules",
            "decoding_rules",
            "fullscreen_rules",
            "embedded_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"video.{collection}")
            ):
                append_entry(entry, f"video.{collection}[{index}]")
    elif domain == "gui-runtime":
        for collection in (
            "types",
            "screen_api",
            "annotations",
            "lifecycle_rules",
            "layout_rules",
            "input_rules",
            "integration_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"gui-runtime.{collection}")
            ):
                append_entry(entry, f"gui-runtime.{collection}[{index}]")
    elif domain == "ai-control-protocol":
        for collection in (
            "wire_rules",
            "methods",
            "error_codes",
            "command_fields",
            "security_rules",
            "integration_rules",
            "validation_rules",
        ):
            for index, entry in enumerate(
                _require_array(model.get(collection), f"ai-control-protocol.{collection}")
            ):
                append_entry(entry, f"ai-control-protocol.{collection}[{index}]")
    else:
        raise ContractDiffError(f"Unsupported generic contract domain: {domain}")

    seen: set[str] = set()
    for entry in entries:
        entry_id = str(entry["id"])
        if entry_id in seen:
            raise ContractDiffError(f"{domain} model contains duplicate entry ID: {entry_id}")
        seen.add(entry_id)
    return entries


def load_model_text(domain: str, text: str, label: str) -> dict[str, Any]:
    if domain == "api":
        try:
            return docs_api_diff.load_api_model_text(text, label)
        except docs_api_diff.ApiDiffError as exception:
            raise ContractDiffError(str(exception)) from exception
    if domain not in DOMAIN_ORDER:
        raise ContractDiffError(f"Unknown contract domain: {domain}")

    model = _read_json_text(text, label)
    expected_schema = EXPECTED_SCHEMAS[domain]
    if model.get("schema_version") != expected_schema:
        raise ContractDiffError(
            f"{label} {domain} schema version must be {expected_schema}, got {model.get('schema_version')}"
        )
    if not isinstance(model.get("repository"), str) or not model["repository"]:
        raise ContractDiffError(f"{label} has no repository")
    scope = _require_object(model.get("scope"), f"{label}.scope")
    if not isinstance(scope.get("surface"), str) or not scope["surface"]:
        raise ContractDiffError(f"{label} has no scope.surface")
    if scope.get("stability") not in docs_api_diff.VALID_STABILITIES:
        raise ContractDiffError(f"{label} has invalid scope.stability: {scope.get('stability')}")
    for field in SOURCE_IDENTITY_FIELDS[domain]:
        if not isinstance(model.get(field), str) or not model[field]:
            raise ContractDiffError(f"{label} has no {field}")
    _flatten_entries(domain, model)
    return model


def load_model(domain: str, path: Path, label: str) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exception:
        raise ContractDiffError(f"Unable to read {label} {domain} model {path}: {exception}") from exception
    return load_model_text(domain, text, label)


def _strip_fields(value: object, excluded: set[str]) -> object:
    if isinstance(value, dict):
        return {
            key: _strip_fields(item, excluded)
            for key, item in value.items()
            if key not in excluded
        }
    if isinstance(value, list):
        return [_strip_fields(item, excluded) for item in value]
    return value


def _entry_diff_view(entry: dict[str, Any]) -> dict[str, Any]:
    return _strip_fields(entry, IGNORED_FIELDS)  # type: ignore[return-value]


def _entry_contract_view(entry: dict[str, Any]) -> dict[str, Any]:
    return _strip_fields(entry, IGNORED_FIELDS | DOC_FIELDS)  # type: ignore[return-value]


def _source_identity(domain: str, model: dict[str, Any]) -> dict[str, Any]:
    return {field: model[field] for field in SOURCE_IDENTITY_FIELDS[domain]}


def _scope_shape(model: dict[str, Any]) -> dict[str, Any]:
    scope = model["scope"]
    return {
        field: copy.deepcopy(scope.get(field))
        for field in ("surface", "included", "excluded")
    }


def _scope_policy(model: dict[str, Any]) -> dict[str, Any]:
    scope = model["scope"]
    return {
        field: copy.deepcopy(scope.get(field))
        for field in ("stability", "since", "support_note")
    }


def _model_metadata(domain: str, model: dict[str, Any]) -> dict[str, Any]:
    if domain == "cmake":
        return {"option_override_precedence": copy.deepcopy(model.get("option_override_precedence"))}
    if domain == "cli":
        return {"program": model.get("program")}
    if domain == "package":
        declaration = _require_object(model.get("declaration"), "package.declaration")
        package_cli = _require_object(model.get("cli"), "package.cli")
        packager = _require_object(model.get("packager"), "package.packager")
        return {
            "program": package_cli.get("program"),
            "packager": _strip_fields(packager, IGNORED_FIELDS | DOC_FIELDS),
            "declaration_command": declaration.get("command"),
            "declaration_consumer": declaration.get("consumer"),
        }
    if domain == "helper-cli":
        return {"discovery": _strip_fields(model.get("discovery"), DOC_FIELDS)}
    if domain == "native-extension":
        return {"registration": _strip_fields(model.get("registration"), DOC_FIELDS)}
    if domain == "prototype-format":
        return {
            "file_selection": _strip_fields(
                model.get("file_selection"), IGNORED_FIELDS | DOC_FIELDS
            )
        }
    if domain == "map-format":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "model-format":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS),
            "removed_legacy": _strip_fields(
                model.get("removed_legacy"), IGNORED_FIELDS | DOC_FIELDS
            ),
        }
    if domain == "text-format":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS),
            "setting_defaults": _strip_fields(
                model.get("setting_defaults"), IGNORED_FIELDS | DOC_FIELDS
            ),
        }
    if domain == "effect-format":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "image-format":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "particle-format":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "font-format":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "audio":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "video":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "gui-runtime":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    if domain == "ai-control-protocol":
        return {
            "outputs": _strip_fields(model.get("outputs"), IGNORED_FIELDS | DOC_FIELDS)
        }
    raise ContractDiffError(f"Unsupported generic contract domain: {domain}")


def generic_model_digest(model: dict[str, Any]) -> str:
    return _digest(model)


def generic_contract_digest(domain: str, model: dict[str, Any]) -> str:
    entries = _flatten_entries(domain, model)
    contract = {
        "schema_version": model["schema_version"],
        "repository": model["repository"],
        "source_identity": _source_identity(domain, model),
        "scope": _strip_fields(model["scope"], DOC_FIELDS),
        "model_metadata": _model_metadata(domain, model),
        "entries": [
            _entry_contract_view(entry)
            for entry in sorted(entries, key=lambda item: str(item["id"]))
        ],
    }
    return _digest(contract)


def _changed_paths(before: object, after: object, prefix: str = "") -> list[str]:
    if isinstance(before, dict) and isinstance(after, dict):
        paths: list[str] = []
        for key in sorted(set(before) | set(after)):
            path = f"{prefix}.{key}" if prefix else key
            if key not in before or key not in after:
                paths.append(path)
            else:
                paths.extend(_changed_paths(before[key], after[key], path))
        return paths
    if isinstance(before, list) and isinstance(after, list):
        paths = []
        for index in range(max(len(before), len(after))):
            path = f"{prefix}[{index}]"
            if index >= len(before) or index >= len(after):
                paths.append(path)
            else:
                paths.extend(_changed_paths(before[index], after[index], path))
        return paths
    return [] if before == after else [prefix or "value"]


def _path_field(path: str) -> str:
    return re.sub(r"\[\d+\]$", "", path).rsplit(".", maxsplit=1)[-1]


def _change_id(domain: str, change_type: str, identity: object) -> str:
    return f"{domain}-change.{change_type}.{_digest(identity)[:16]}"


def _entry_kind(entry_id: str) -> str:
    parts = entry_id.split(".")
    return parts[1] if len(parts) > 1 else "entry"


def _make_entry_change(
    domain: str,
    domain_stability: str,
    change_type: str,
    before: dict[str, Any] | None,
    after: dict[str, Any] | None,
) -> dict[str, Any]:
    entry = before if before is not None else after
    if entry is None:
        raise ContractDiffError("Contract entry change has no before or after value")
    entry_id = str(entry["id"])

    if change_type == "added":
        classification = "additive"
        changed_fields: list[str] = []
        reasons = ["new contract entry"]
        disposition_required = False
    elif change_type == "removed":
        classification = "breaking"
        changed_fields = []
        reasons = ["contract entry removed"]
        disposition_required = domain_stability in docs_api_diff.PUBLIC_STABILITIES
    else:
        if before is None or after is None:
            raise ContractDiffError("Modified contract entry must have before and after values")
        changed_fields = _changed_paths(before, after)
        shape_fields = [
            field
            for field in changed_fields
            if _path_field(field) not in DOC_FIELDS | POLICY_FIELDS
        ]
        policy_fields = [
            field for field in changed_fields if _path_field(field) in POLICY_FIELDS
        ]
        baseline_stability = str(before.get("stability", domain_stability))
        current_stability = str(after.get("stability", domain_stability))
        withdrawal = docs_api_diff.stability_withdrawal(baseline_stability, current_stability)
        if shape_fields or withdrawal:
            classification = "breaking"
            reasons = []
            if shape_fields:
                reasons.append("contract shape changed: " + ", ".join(shape_fields))
            if withdrawal:
                reasons.append(f"stability withdrawn: {baseline_stability} -> {current_stability}")
            disposition_required = baseline_stability in docs_api_diff.PUBLIC_STABILITIES
        elif policy_fields:
            classification = "policy"
            reasons = ["contract policy metadata changed"]
            disposition_required = False
        else:
            classification = "documentation"
            reasons = ["documentation metadata changed"]
            disposition_required = False

    identity = {
        "domain": domain,
        "change_type": change_type,
        "entry_id": entry_id,
        "before": before,
        "after": after,
    }
    return {
        "change_id": _change_id(domain, change_type, identity),
        "domain": domain,
        "change_type": change_type,
        "classification": classification,
        "entry_id": entry_id,
        "entry_kind": _entry_kind(entry_id),
        "baseline_stability": domain_stability if before is not None else None,
        "current_stability": domain_stability if after is not None else None,
        "changed_fields": changed_fields,
        "reasons": reasons,
        "disposition_required": disposition_required,
        "before": before,
        "after": after,
    }


def _make_model_change(
    domain: str,
    domain_stability: str,
    change_type: str,
    before: object,
    after: object,
    reason: str,
    *,
    classification: str = "breaking",
    disposition_required: bool = True,
    current_stability: str | None = None,
) -> dict[str, Any]:
    identity = {
        "domain": domain,
        "change_type": change_type,
        "before": before,
        "after": after,
    }
    return {
        "change_id": _change_id(domain, change_type, identity),
        "domain": domain,
        "change_type": change_type,
        "classification": classification,
        "entry_id": None,
        "entry_kind": f"{domain}-model",
        "baseline_stability": domain_stability,
        "current_stability": current_stability or domain_stability,
        "changed_fields": [change_type.replace("-", "_")],
        "reasons": [reason],
        "disposition_required": disposition_required,
        "before": before,
        "after": after,
    }


def _disposition_index(dispositions: dict[str, Any]) -> dict[tuple[str, str, str], dict[str, Any]]:
    return {
        (
            str(entry["change_id"]),
            str(entry["baseline_contract_sha256"]),
            str(entry["current_contract_sha256"]),
        ): entry
        for entry in dispositions["entries"]
    }


def _missing_disposition_template(
    domain: str,
    change: dict[str, Any],
    baseline_contract_sha256: str,
    current_contract_sha256: str,
) -> dict[str, str]:
    return {
        "domain": domain,
        "change_id": str(change["change_id"]),
        "baseline_contract_sha256": baseline_contract_sha256,
        "current_contract_sha256": current_contract_sha256,
        "classification": "breaking",
        "rationale": "Describe why this change is intentional.",
        "migration": "Link migration guidance or explain why it is not required.",
        "release_note": "Link the release note or record its planned owner.",
        "compatibility": "Record migration/version/coordinated-release impact.",
        "owner": "Owning domain or maintainer.",
    }


def generate_generic_domain_diff(
    domain: str,
    baseline: dict[str, Any],
    current: dict[str, Any],
    dispositions: dict[str, Any],
    *,
    baseline_label: str = "baseline",
    current_label: str = "current",
) -> dict[str, Any]:
    docs_api_diff.validate_dispositions(dispositions)
    baseline_entries_list = _flatten_entries(domain, baseline)
    current_entries_list = _flatten_entries(domain, current)
    baseline_stability = str(baseline["scope"]["stability"])
    current_stability = str(current["scope"]["stability"])
    baseline_model_sha256 = generic_model_digest(baseline)
    current_model_sha256 = generic_model_digest(current)
    baseline_contract_sha256 = generic_contract_digest(domain, baseline)
    current_contract_sha256 = generic_contract_digest(domain, current)
    changes: list[dict[str, Any]] = []

    if _source_identity(domain, baseline) != _source_identity(domain, current):
        changes.append(
            _make_model_change(
                domain,
                baseline_stability,
                "source-contract",
                _source_identity(domain, baseline),
                _source_identity(domain, current),
                "canonical model source changed",
            )
        )
    if baseline.get("repository") != current.get("repository") or _scope_shape(baseline) != _scope_shape(current):
        changes.append(
            _make_model_change(
                domain,
                baseline_stability,
                "model-scope",
                {"repository": baseline.get("repository"), "scope": _scope_shape(baseline)},
                {"repository": current.get("repository"), "scope": _scope_shape(current)},
                "canonical model repository or declared scope changed",
            )
        )
    if _model_metadata(domain, baseline) != _model_metadata(domain, current):
        changes.append(
            _make_model_change(
                domain,
                baseline_stability,
                "model-contract",
                _model_metadata(domain, baseline),
                _model_metadata(domain, current),
                "canonical model-level contract changed",
            )
        )

    baseline_policy = _scope_policy(baseline)
    current_policy = _scope_policy(current)
    if baseline_policy != current_policy:
        withdrawal = docs_api_diff.stability_withdrawal(baseline_stability, current_stability)
        changes.append(
            _make_model_change(
                domain,
                baseline_stability,
                "domain-policy",
                baseline_policy,
                current_policy,
                (
                    f"domain stability withdrawn: {baseline_stability} -> {current_stability}"
                    if withdrawal
                    else "domain policy metadata changed"
                ),
                classification="breaking" if withdrawal else "policy",
                disposition_required=withdrawal and baseline_stability in docs_api_diff.PUBLIC_STABILITIES,
                current_stability=current_stability,
            )
        )

    baseline_entries = {
        str(entry["id"]): _entry_diff_view(entry) for entry in baseline_entries_list
    }
    current_entries = {
        str(entry["id"]): _entry_diff_view(entry) for entry in current_entries_list
    }
    for entry_id in sorted(set(baseline_entries) | set(current_entries)):
        before = baseline_entries.get(entry_id)
        after = current_entries.get(entry_id)
        if before is None:
            changes.append(_make_entry_change(domain, current_stability, "added", None, after))
        elif after is None:
            changes.append(_make_entry_change(domain, baseline_stability, "removed", before, None))
        elif before != after:
            changes.append(_make_entry_change(domain, baseline_stability, "modified", before, after))

    changes.sort(
        key=lambda change: (
            str(change["entry_id"] or ""),
            str(change["change_type"]),
            str(change["change_id"]),
        )
    )
    disposition_index = _disposition_index(dispositions)
    missing_dispositions: list[dict[str, str]] = []
    satisfied_disposition_count = 0
    for change in changes:
        if not change["disposition_required"]:
            change["disposition"] = None
            change["disposition_status"] = "not-required"
            continue
        key = (
            str(change["change_id"]),
            baseline_contract_sha256,
            current_contract_sha256,
        )
        disposition = disposition_index.get(key)
        if disposition is None:
            change["disposition"] = None
            change["disposition_status"] = "missing"
            missing_dispositions.append(
                _missing_disposition_template(
                    domain, change, baseline_contract_sha256, current_contract_sha256
                )
            )
        else:
            change["disposition"] = disposition
            change["disposition_status"] = "satisfied"
            satisfied_disposition_count += 1

    classifications = Counter(str(change["classification"]) for change in changes)
    change_types = Counter(str(change["change_type"]) for change in changes)
    required_disposition_count = sum(bool(change["disposition_required"]) for change in changes)
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_contract_diff.py",
        "domain": domain,
        "status": "blocked" if missing_dispositions else "pass",
        "baseline": {
            "label": baseline_label,
            "model_sha256": baseline_model_sha256,
            "contract_sha256": baseline_contract_sha256,
            "entry_count": len(baseline_entries),
            "stability": baseline_stability,
        },
        "current": {
            "label": current_label,
            "model_sha256": current_model_sha256,
            "contract_sha256": current_contract_sha256,
            "entry_count": len(current_entries),
            "stability": current_stability,
        },
        "summary": {
            "change_count": len(changes),
            "changes_by_type": dict(sorted(change_types.items())),
            "changes_by_classification": dict(sorted(classifications.items())),
            "required_disposition_count": required_disposition_count,
            "satisfied_disposition_count": satisfied_disposition_count,
            "missing_disposition_count": len(missing_dispositions),
        },
        "changes": changes,
        "missing_dispositions": missing_dispositions,
    }


def generate_domain_bootstrap(
    domain: str,
    current: dict[str, Any],
    current_label: str,
    baseline_label: str,
    reason: str,
) -> dict[str, Any]:
    if domain == "api":
        report = docs_api_diff.generate_bootstrap_report(current, current_label, baseline_label, reason)
        report["domain"] = domain
        return report
    entries = _flatten_entries(domain, current)
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_contract_diff.py",
        "domain": domain,
        "status": "bootstrap",
        "baseline": {"label": baseline_label, "unavailable_reason": reason},
        "current": {
            "label": current_label,
            "model_sha256": generic_model_digest(current),
            "contract_sha256": generic_contract_digest(domain, current),
            "entry_count": len(entries),
            "stability": current["scope"]["stability"],
        },
        "summary": {
            "change_count": 0,
            "changes_by_type": {},
            "changes_by_classification": {},
            "required_disposition_count": 0,
            "satisfied_disposition_count": 0,
            "missing_disposition_count": 0,
        },
        "changes": [],
        "missing_dispositions": [],
    }


def generate_contract_diff(
    baseline_models: dict[str, dict[str, Any]],
    current_models: dict[str, dict[str, Any]],
    dispositions: dict[str, Any],
    *,
    baseline_label: str = "baseline",
    current_label: str = "current",
) -> dict[str, Any]:
    docs_api_diff.validate_dispositions(dispositions)
    domain_reports: dict[str, dict[str, Any]] = {}
    for domain in DOMAIN_ORDER:
        if domain not in baseline_models or domain not in current_models:
            raise ContractDiffError(f"Missing {domain} model for aggregate comparison")
        if domain == "api":
            report = docs_api_diff.generate_api_diff(
                baseline_models[domain],
                current_models[domain],
                dispositions,
                baseline_label=f"{baseline_label}/{MODEL_FILES[domain]}",
                current_label=f"{current_label}/{MODEL_FILES[domain]}",
            )
            report["domain"] = domain
        else:
            report = generate_generic_domain_diff(
                domain,
                baseline_models[domain],
                current_models[domain],
                dispositions,
                baseline_label=f"{baseline_label}/{MODEL_FILES[domain]}",
                current_label=f"{current_label}/{MODEL_FILES[domain]}",
            )
        domain_reports[domain] = report
    return _aggregate_reports(domain_reports, baseline_label, current_label)


def _aggregate_reports(
    domain_reports: dict[str, dict[str, Any]], baseline_label: str, current_label: str
) -> dict[str, Any]:
    statuses = {str(report["status"]) for report in domain_reports.values()}
    status = "blocked" if "blocked" in statuses else "bootstrap" if "bootstrap" in statuses else "pass"
    changes = [
        change
        for domain in DOMAIN_ORDER
        for change in domain_reports[domain]["changes"]
    ]
    missing_dispositions = [
        disposition
        for domain in DOMAIN_ORDER
        for disposition in domain_reports[domain]["missing_dispositions"]
    ]
    classifications = Counter(str(change["classification"]) for change in changes)
    change_types = Counter(str(change["change_type"]) for change in changes)
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_contract_diff.py",
        "status": status,
        "baseline": {"label": baseline_label},
        "current": {"label": current_label},
        "summary": {
            "domain_count": len(domain_reports),
            "change_count": len(changes),
            "changes_by_type": dict(sorted(change_types.items())),
            "changes_by_classification": dict(sorted(classifications.items())),
            "required_disposition_count": sum(
                int(report["summary"]["required_disposition_count"])
                for report in domain_reports.values()
            ),
            "satisfied_disposition_count": sum(
                int(report["summary"]["satisfied_disposition_count"])
                for report in domain_reports.values()
            ),
            "missing_disposition_count": len(missing_dispositions),
            "domains": {
                domain: {
                    "status": domain_reports[domain]["status"],
                    "change_count": domain_reports[domain]["summary"]["change_count"],
                    "required_disposition_count": domain_reports[domain]["summary"]["required_disposition_count"],
                    "missing_disposition_count": domain_reports[domain]["summary"]["missing_disposition_count"],
                }
                for domain in DOMAIN_ORDER
            },
        },
        "domains": domain_reports,
        "changes": changes,
        "missing_dispositions": missing_dispositions,
    }


def render_contract_diff(report: dict[str, Any]) -> str:
    return json.dumps(report, indent=2, ensure_ascii=True) + "\n"


def _text(value: object) -> str:
    result = html.escape(str(value), quote=True)
    result = result.replace("|", "&#124;").replace("{", "&#123;").replace("}", "&#125;")
    return result.replace("\r\n", "<br>").replace("\r", "<br>").replace("\n", "<br>")


def _code(value: object) -> str:
    return f"<code>{_text(value)}</code>"


def render_contract_diff_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    lines = [
        "# FOnline Contract Diff",
        "",
        f"Status: **{_text(report['status'])}**",
        "",
        f"Baseline: {_code(report['baseline']['label'])}",
        "",
        f"Current: {_code(report['current']['label'])}",
        "",
        f"Changes: **{summary['change_count']}**; required dispositions: "
        f"**{summary['required_disposition_count']}**; missing: "
        f"**{summary['missing_disposition_count']}**.",
        "",
        "| Domain | Status | Changes | Required dispositions | Missing |",
        "| --- | --- | ---: | ---: | ---: |",
    ]
    for domain in DOMAIN_ORDER:
        domain_summary = summary["domains"][domain]
        lines.append(
            f"| {_code(domain)} | {_text(domain_summary['status'])} | "
            f"{domain_summary['change_count']} | {domain_summary['required_disposition_count']} | "
            f"{domain_summary['missing_disposition_count']} |"
        )

    lines.extend(
        [
            "",
            "| Domain | Classification | Change | Entry | Stability | Fields | Disposition | Change ID |",
            "| --- | --- | --- | --- | --- | --- | --- | --- |",
        ]
    )
    for change in report["changes"]:
        entry = change.get("symbol_id") or change.get("entry_id") or f"{change['domain']} model"
        stability = f"{change.get('baseline_stability')} -> {change.get('current_stability')}"
        fields = ", ".join(change["changed_fields"]) or "-"
        lines.append(
            f"| {_code(change['domain'])} | {_text(change['classification'])} | "
            f"{_text(change['change_type'])} | {_code(entry)} | {_text(stability)} | "
            f"{_text(fields)} | {_text(change['disposition_status'])} | "
            f"{_code(change['change_id'])} |"
        )
    if not report["changes"]:
        lines.append("| - | - | - | - | - | - | - | - |")
    lines.append("")

    bootstrap_domains = [
        domain for domain in DOMAIN_ORDER if report["domains"][domain]["status"] == "bootstrap"
    ]
    if bootstrap_domains:
        lines.extend(
            [
                "## Bootstrap domains",
                "",
                "These domains have no comparable model at the selected baseline: "
                + ", ".join(f"`{domain}`" for domain in bootstrap_domains)
                + ".",
                "",
            ]
        )

    if report["missing_dispositions"]:
        lines.extend(
            [
                "## Missing dispositions",
                "",
                "Add these entries to `Docs/contract-change-dispositions.json`, replacing every "
                "placeholder with an owner-reviewed disposition:",
                "",
                "```json",
                json.dumps(report["missing_dispositions"], indent=2, ensure_ascii=True),
                "```",
                "",
            ]
        )
    return "\n".join(lines).rstrip() + "\n"


def _resolve_inside_root(root: Path, value: str, label: str) -> Path:
    path = (root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError as exception:
        raise ContractDiffError(f"{label} escapes the engine root: {value}") from exception
    return path


def _verify_git_ref(root: Path, ref: str) -> None:
    if not ref or set(ref) == {"0"}:
        raise BaselineUnavailable("event has no previous revision")
    result = subprocess.run(
        ["git", "cat-file", "-e", f"{ref}^{{commit}}"],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or "unknown revision"
        raise ContractDiffError(f"Contract baseline git revision is unavailable: {ref}: {detail}")


def _load_git_model(root: Path, ref: str, domain: str, model_path: str) -> dict[str, Any]:
    result = subprocess.run(
        ["git", "show", f"{ref}:{model_path}"],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="strict",
        check=False,
    )
    if result.returncode != 0:
        raise BaselineUnavailable(f"{model_path} does not exist at baseline revision {ref}")
    return load_model_text(domain, result.stdout, f"git {ref}:{model_path}")


def _load_directory_models(root: Path, directory: str, label: str) -> dict[str, dict[str, Any]]:
    directory_path = _resolve_inside_root(root, directory, label)
    models: dict[str, dict[str, Any]] = {}
    for domain in DOMAIN_ORDER:
        path = directory_path / MODEL_FILES[domain]
        models[domain] = load_model(domain, path, f"{label} {domain}")
    return models


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Compare canonical FOnline generated contract models"
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    baseline = parser.add_mutually_exclusive_group(required=True)
    baseline.add_argument("--baseline-dir")
    baseline.add_argument("--baseline-git-ref")
    parser.add_argument("--current-dir", default=DEFAULT_CURRENT_DIR)
    parser.add_argument("--dispositions", default=DEFAULT_DISPOSITIONS)
    parser.add_argument("--json-output", default=DEFAULT_JSON_OUTPUT)
    parser.add_argument("--markdown-output", default=DEFAULT_MARKDOWN_OUTPUT)
    parser.add_argument("--allow-missing-baseline", action="store_true")
    parser.add_argument("--enforce", action="store_true")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        current_models = _load_directory_models(root, args.current_dir, "Current model directory")
        dispositions_path = _resolve_inside_root(root, args.dispositions, "Contract change dispositions")
        dispositions = docs_api_diff.load_dispositions(dispositions_path)
        current_label = _resolve_inside_root(root, args.current_dir, "Current model directory").relative_to(root).as_posix()

        if args.baseline_dir:
            baseline_label = _resolve_inside_root(root, args.baseline_dir, "Baseline model directory").relative_to(root).as_posix()
            try:
                baseline_models = _load_directory_models(root, args.baseline_dir, "Baseline model directory")
            except ContractDiffError as exception:
                if not args.allow_missing_baseline:
                    raise
                raise ContractDiffError(
                    "Directory baselines must contain every configured domain model; "
                    "use a Git baseline for per-domain bootstrap"
                ) from exception
            report = generate_contract_diff(
                baseline_models,
                current_models,
                dispositions,
                baseline_label=baseline_label,
                current_label=current_label,
            )
        else:
            baseline_label = f"git:{args.baseline_git_ref}:Docs/generated"
            domain_reports: dict[str, dict[str, Any]] = {}
            try:
                _verify_git_ref(root, args.baseline_git_ref)
            except BaselineUnavailable as exception:
                if not args.allow_missing_baseline:
                    raise
                for domain in DOMAIN_ORDER:
                    model_path = f"Docs/generated/{MODEL_FILES[domain]}"
                    domain_reports[domain] = generate_domain_bootstrap(
                        domain,
                        current_models[domain],
                        f"{current_label}/{MODEL_FILES[domain]}",
                        f"git:{args.baseline_git_ref}:{model_path}",
                        str(exception),
                    )
            else:
                for domain in DOMAIN_ORDER:
                    model_path = f"Docs/generated/{MODEL_FILES[domain]}"
                    domain_current_label = f"{current_label}/{MODEL_FILES[domain]}"
                    domain_baseline_label = f"git:{args.baseline_git_ref}:{model_path}"
                    try:
                        baseline_model = _load_git_model(
                            root, args.baseline_git_ref, domain, model_path
                        )
                    except BaselineUnavailable as exception:
                        if not args.allow_missing_baseline:
                            raise
                        domain_reports[domain] = generate_domain_bootstrap(
                            domain,
                            current_models[domain],
                            domain_current_label,
                            domain_baseline_label,
                            str(exception),
                        )
                        continue

                    if domain == "api":
                        domain_report = docs_api_diff.generate_api_diff(
                            baseline_model,
                            current_models[domain],
                            dispositions,
                            baseline_label=domain_baseline_label,
                            current_label=domain_current_label,
                        )
                        domain_report["domain"] = domain
                    else:
                        domain_report = generate_generic_domain_diff(
                            domain,
                            baseline_model,
                            current_models[domain],
                            dispositions,
                            baseline_label=domain_baseline_label,
                            current_label=domain_current_label,
                        )
                    domain_reports[domain] = domain_report
            report = _aggregate_reports(domain_reports, baseline_label, current_label)

        if args.write:
            json_output = _resolve_inside_root(root, args.json_output, "Contract diff JSON output")
            markdown_output = _resolve_inside_root(root, args.markdown_output, "Contract diff Markdown output")
            json_output.parent.mkdir(parents=True, exist_ok=True)
            markdown_output.parent.mkdir(parents=True, exist_ok=True)
            json_output.write_text(render_contract_diff(report), encoding="utf-8", newline="\n")
            markdown_output.write_text(
                render_contract_diff_markdown(report), encoding="utf-8", newline="\n"
            )
    except (BaselineUnavailable, ContractDiffError, docs_api_diff.ApiDiffError, OSError, UnicodeError) as exception:
        print(f"Unable to evaluate contract diff: {exception}", file=sys.stderr)
        return 1

    summary = report["summary"]
    print(
        f"Contract diff {report['status']}: {summary['change_count']} changes across "
        f"{summary['domain_count']} domains, {summary['required_disposition_count']} required "
        f"dispositions, {summary['missing_disposition_count']} missing"
    )
    if args.enforce and report["status"] == "blocked":
        report_hint = (
            "inspect Workspace/contract-diff.md"
            if args.write
            else "rerun with --write to produce Workspace/contract-diff.md"
        )
        print(
            "Public contract breaking changes require entries in "
            f"Docs/contract-change-dispositions.json; {report_hint}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
