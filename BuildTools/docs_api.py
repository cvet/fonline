from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import urlsplit

import codegen


DEFAULT_OUTPUT = "Docs/generated/api.json"
SCHEMA_VERSION = 2
SOURCE_SUFFIXES = {".cpp", ".h", ".inc"}
RUNTIME_SIDES = {
    "Common": ["server", "client", "mapper"],
    "Server": ["server"],
    "Client": ["client", "mapper"],
    "Mapper": ["mapper"],
}
NON_DOCUMENTATION_COMMENT_PREFIXES = (
    "ReSharper disable",
    "NOLINT",
)
API_CONTRACT_SCOPE_SELECTOR = "scope:native-codegen"


def collect_engine_metadata_inputs(root: Path) -> list[Path]:
    source_root = root / "Source"
    return sorted(
        path
        for path in source_root.rglob("*")
        if path.is_file() and path.suffix in SOURCE_SUFFIXES and "Tests" not in path.relative_to(source_root).parts
    )


def _parse_engine_metadata(root: Path) -> tuple[codegen.CodeGenTagStore, codegen.CodeGenTagSourceStore]:
    metadata_inputs = collect_engine_metadata_inputs(root)
    codegen.reset_metadata_parse_state()

    with tempfile.TemporaryDirectory() as error_output_dir:
        codegen.args = argparse.Namespace(
            meta=[str(path) for path in metadata_inputs],
            verbose=False,
            genoutput=error_output_dir,
        )
        codegen.parse_meta_files()
        codegen.parse_all_tags()

    return codegen.codegen_tags, codegen.codegen_tag_sources


def _entries(
    tags: codegen.CodeGenTagStore,
    sources: codegen.CodeGenTagSourceStore,
    tag_name: str,
) -> list[tuple[Any, codegen.SourceLocation | None]]:
    tag_values = list(tags[tag_name])  # type: ignore[literal-required]
    tag_sources = sources[tag_name]
    if len(tag_values) != len(tag_sources):
        raise ValueError(f"Codegen tag/source count mismatch for {tag_name}")
    return list(zip(tag_values, tag_sources, strict=True))


def _source(root: Path, location: codegen.SourceLocation | None) -> dict[str, object] | None:
    if location is None:
        return None

    source_path = Path(location.abs_path).resolve()
    try:
        relative_path = source_path.relative_to(root)
    except ValueError as exception:
        raise ValueError(f"API source path escapes the engine root: {source_path}") from exception

    return {"path": relative_path.as_posix(), "line": location.line}


def _description(comment: list[str]) -> str:
    lines = [
        line
        for line in comment
        if not line.strip().startswith(NON_DOCUMENTATION_COMMENT_PREFIXES)
    ]
    return "\n".join(lines).strip()


def _script_type(meta_type: str, receiver: str = "SELF_ENTITY", nullable: bool = False) -> str:
    type_name = codegen.meta_type_to_unified_type(meta_type, self_entity=receiver)
    return type_name + ("?" if nullable else "")


def _argument(arg: codegen.MethodArg, receiver: str) -> dict[str, object]:
    return {
        "name": arg.name,
        "type": _script_type(arg.arg_type, receiver, arg.nullable),
        "nullable": arg.nullable,
        "by_reference": arg.arg_type.endswith(".ref"),
        "default": arg.default_value,
    }


def _argument_signature(arg: dict[str, object]) -> str:
    result = f"{arg['type']} {arg['name']}"
    if arg["default"] is not None:
        result += f" = {arg['default']}"
    return result


def _identity_hash(identity: object) -> str:
    canonical = json.dumps(identity, ensure_ascii=True, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()[:16]


def _symbol_id(family_id: str, identity: object, family_count: int) -> str:
    return family_id if family_count == 1 else f"{family_id}#{_identity_hash(identity)}"


def _base_symbol(
    root: Path,
    *,
    symbol_id: str,
    family_id: str,
    kind: str,
    name: str,
    runtime_sides: list[str],
    signature: str,
    comment: list[str],
    source: codegen.SourceLocation | None,
    receiver: str | None = None,
    flags: list[str] | None = None,
) -> dict[str, object]:
    return {
        "id": symbol_id,
        "family_id": family_id,
        "kind": kind,
        "name": name,
        "runtime_sides": runtime_sides,
        "receiver": receiver,
        "signature": signature,
        "description": _description(comment),
        "flags": list(flags or []),
        "stability": "internal",
        "since": None,
        "deprecated": None,
        "examples": [],
        "source": _source(root, source),
        "contract": {
            "explicit": False,
            "selector": None,
            "source": None,
            "notes": "",
        },
    }


def _method_receivers(tag: codegen.ExportMethodTag) -> list[str]:
    if tag.entity != "Entity":
        return [tag.entity]
    if "TimeEventRelated" in tag.flags:
        return [
            entity
            for entity in codegen.game_entities
            if codegen.game_entities_info[entity].has_time_events
        ]
    return list(codegen.game_entities)


def _append_enum_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportEnumTag, codegen.SourceLocation | None]],
) -> None:
    for tag, source in entries:
        family_id = f"script.enum.{tag.group_name}"
        symbol = _base_symbol(
            root,
            symbol_id=family_id,
            family_id=family_id,
            kind="enum",
            name=tag.group_name,
            runtime_sides=RUNTIME_SIDES["Common"],
            signature=f"enum {tag.group_name} : {tag.underlying_type}",
            comment=tag.comment,
            source=source,
            flags=tag.flags,
        )
        symbol["underlying_type"] = tag.underlying_type
        symbol["generated"] = source is None
        symbols.append(symbol)

        for key_value in tag.key_values:
            value_family_id = f"script.enum-value.{tag.group_name}.{key_value.key}"
            value = codegen.require_enum_value_text(key_value)
            value_doc = tag.value_docs.get(key_value.key)
            value_symbol = _base_symbol(
                root,
                symbol_id=value_family_id,
                family_id=value_family_id,
                kind="enum-value",
                name=key_value.key,
                runtime_sides=RUNTIME_SIDES["Common"],
                signature=f"{tag.group_name}.{key_value.key} = {value}",
                comment=value_doc.comment if value_doc is not None else key_value.comment,
                source=value_doc.source if value_doc is not None else source,
                receiver=tag.group_name,
            )
            value_symbol["parent_id"] = family_id
            value_symbol["value"] = value
            value_symbol["evaluated_value"] = int(value, 0)
            value_symbol["generated"] = source is None
            symbols.append(value_symbol)


def _append_value_type_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportValueTypeTag, codegen.SourceLocation | None]],
) -> None:
    for tag, source in entries:
        family_id = f"script.value-type.{tag.name}"
        layout = codegen.get_value_type_layout(tag)
        symbol = _base_symbol(
            root,
            symbol_id=family_id,
            family_id=family_id,
            kind="value-type",
            name=tag.name,
            runtime_sides=RUNTIME_SIDES["Common"],
            signature=f"value type {tag.name}",
            comment=tag.comment,
            source=source,
            flags=tag.flags,
        )
        symbol["native_type"] = tag.native_type
        symbols.append(symbol)

        for field_type, field_name in layout:
            field_family_id = f"script.value-field.{tag.name}.{field_name}"
            field_doc = tag.field_docs.get(field_name)
            field_symbol = _base_symbol(
                root,
                symbol_id=field_family_id,
                family_id=field_family_id,
                kind="value-field",
                name=field_name,
                runtime_sides=RUNTIME_SIDES["Common"],
                signature=f"{field_type} {tag.name}.{field_name}",
                comment=field_doc.comment if field_doc is not None else [],
                source=field_doc.source if field_doc is not None else source,
                receiver=tag.name,
            )
            field_symbol["parent_id"] = family_id
            field_symbol["type"] = field_type
            field_symbol["mutability"] = "value"
            symbols.append(field_symbol)


def _append_entity_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportEntityTag, codegen.SourceLocation | None]],
) -> None:
    for tag, source in entries:
        family_id = f"script.entity.{tag.name}"
        symbol = _base_symbol(
            root,
            symbol_id=family_id,
            family_id=family_id,
            kind="entity",
            name=tag.name,
            runtime_sides=RUNTIME_SIDES["Common"],
            signature=f"entity {tag.name}",
            comment=tag.comment,
            source=source,
            flags=tag.flags,
        )
        symbol["server_class"] = tag.server_class_name
        symbol["client_class"] = tag.client_class_name
        symbol["capabilities"] = {
            "global": "Global" in tag.flags,
            "prototypes": "HasProtos" in tag.flags,
            "statics": "HasStatics" in tag.flags,
            "abstract": "HasAbstract" in tag.flags,
            "time_events": "HasTimeEvents" in tag.flags,
        }
        symbols.append(symbol)


def _append_ref_type_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportRefTypeTag, codegen.SourceLocation | None]],
) -> None:
    method_families = Counter(
        f"script.ref-method.{tag.target.lower()}.{tag.name}.{method.name}"
        for tag, _ in entries
        for method in tag.methods
    )

    for tag, source in entries:
        runtime_sides = RUNTIME_SIDES[tag.target]
        family_id = f"script.ref-type.{tag.target.lower()}.{tag.name}"
        symbol = _base_symbol(
            root,
            symbol_id=family_id,
            family_id=family_id,
            kind="ref-type",
            name=tag.name,
            runtime_sides=runtime_sides,
            signature=f"ref type {tag.name}",
            comment=tag.comment,
            source=source,
            flags=tag.flags,
        )
        symbol["declared_target"] = tag.target.lower()
        symbols.append(symbol)

        for field in tag.fields:
            field_family_id = f"script.ref-field.{tag.target.lower()}.{tag.name}.{field.name}"
            field_symbol = _base_symbol(
                root,
                symbol_id=field_family_id,
                family_id=field_family_id,
                kind="ref-field",
                name=field.name,
                runtime_sides=runtime_sides,
                signature=f"{_script_type(field.field_type, tag.name)} {tag.name}.{field.name}",
                comment=field.comment,
                source=source,
                receiver=tag.name,
            )
            field_symbol["parent_id"] = family_id
            field_symbol["type"] = _script_type(field.field_type, tag.name)
            field_symbol["mutability"] = "mutable"
            symbols.append(field_symbol)

        for method in tag.methods:
            method_family_id = f"script.ref-method.{tag.target.lower()}.{tag.name}.{method.name}"
            arguments = [_argument(arg, tag.name) for arg in method.args]
            return_type = _script_type(method.ret, tag.name)
            signature = (
                f"{return_type} {tag.name}.{method.name}("
                + ", ".join(_argument_signature(arg) for arg in arguments)
                + ")"
            )
            identity = {"return_type": return_type, "arguments": arguments}
            method_id = _symbol_id(method_family_id, identity, method_families[method_family_id])
            method_symbol = _base_symbol(
                root,
                symbol_id=method_id,
                family_id=method_family_id,
                kind="ref-method",
                name=method.name,
                runtime_sides=runtime_sides,
                signature=signature,
                comment=method.comment,
                source=source,
                receiver=tag.name,
            )
            method_symbol["parent_id"] = family_id
            method_symbol["return"] = {"type": return_type, "nullable": False}
            method_symbol["arguments"] = arguments
            symbols.append(method_symbol)


def _append_property_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportPropertyTag, codegen.SourceLocation | None]],
) -> None:
    for tag, source in entries:
        family_id = f"script.property.{tag.entity}.{tag.name}"
        nullable = "Nullable" in tag.flags
        property_type = _script_type(tag.property_type, tag.entity, nullable)
        symbol = _base_symbol(
            root,
            symbol_id=family_id,
            family_id=family_id,
            kind="property",
            name=tag.name,
            runtime_sides=RUNTIME_SIDES[tag.access],
            signature=f"{property_type} {tag.entity}.{tag.name}",
            comment=tag.comment,
            source=source,
            receiver=tag.entity,
            flags=tag.flags,
        )
        symbol["access"] = tag.access.lower()
        symbol["type"] = property_type
        symbol["nullable"] = nullable
        symbol["mutability"] = "mutable" if "Mutable" in tag.flags else "read-only"
        symbol["persistent"] = "Persistent" in tag.flags
        symbols.append(symbol)


def _append_method_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportMethodTag, codegen.SourceLocation | None]],
) -> None:
    family_counts = Counter(
        f"script.method.{tag.target.lower()}.{tag.entity}.{tag.name}" for tag, _ in entries
    )

    for tag, source in entries:
        family_id = f"script.method.{tag.target.lower()}.{tag.entity}.{tag.name}"
        arguments = [_argument(arg, tag.entity) for arg in tag.args]
        return_type = _script_type(tag.ret, tag.entity, tag.ret_nullable)
        signature = (
            f"{return_type} {tag.entity}.{tag.name}("
            + ", ".join(_argument_signature(arg) for arg in arguments)
            + ")"
        )
        identity = {"return_type": return_type, "arguments": arguments}
        symbol_id = _symbol_id(family_id, identity, family_counts[family_id])
        symbol = _base_symbol(
            root,
            symbol_id=symbol_id,
            family_id=family_id,
            kind="method",
            name=tag.name,
            runtime_sides=RUNTIME_SIDES[tag.target],
            signature=signature,
            comment=tag.comment,
            source=source,
            receiver=tag.entity,
            flags=tag.flags,
        )
        symbol["declared_target"] = tag.target.lower()
        symbol["receivers"] = _method_receivers(tag)
        symbol["return"] = {"type": return_type, "nullable": tag.ret_nullable}
        symbol["arguments"] = arguments
        symbols.append(symbol)


def _append_event_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportEventTag, codegen.SourceLocation | None]],
) -> None:
    family_counts = Counter(
        f"script.event.{tag.target.lower()}.{tag.entity}.{tag.name}" for tag, _ in entries
    )

    for tag, source in entries:
        family_id = f"script.event.{tag.target.lower()}.{tag.entity}.{tag.name}"
        arguments = [_argument(arg, tag.entity) for arg in tag.args]
        signature = (
            f"event {tag.entity}.{tag.name}("
            + ", ".join(_argument_signature(arg) for arg in arguments)
            + ")"
        )
        identity = {"arguments": arguments}
        symbol_id = _symbol_id(family_id, identity, family_counts[family_id])
        symbol = _base_symbol(
            root,
            symbol_id=symbol_id,
            family_id=family_id,
            kind="event",
            name=tag.name,
            runtime_sides=RUNTIME_SIDES[tag.target],
            signature=signature,
            comment=tag.comment,
            source=source,
            receiver=tag.entity,
            flags=tag.flags,
        )
        symbol["declared_target"] = tag.target.lower()
        symbol["arguments"] = arguments
        symbols.append(symbol)


def _setting_secret_tokens(entries: list[tuple[codegen.ExportSettingsTag, codegen.SourceLocation | None]]) -> list[str]:
    for tag, _ in entries:
        for setting in tag.settings:
            if setting.name == "Common.SecretSettingTokens":
                return [token.lower() for token in setting.init_values]
    return []


def _append_setting_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ExportSettingsTag, codegen.SourceLocation | None]],
) -> None:
    secret_tokens = _setting_secret_tokens(entries)
    for tag, source in entries:
        for setting in tag.settings:
            family_id = f"setting.{tag.target.lower()}.{setting.name}"
            setting_type = _script_type(setting.value_type)
            default_text = ", ".join(setting.init_values) if setting.init_values else None
            signature = f"{setting_type} {setting.name}"
            if default_text is not None:
                signature += f" = {default_text}"
            symbol = _base_symbol(
                root,
                symbol_id=family_id,
                family_id=family_id,
                kind="setting",
                name=setting.name,
                runtime_sides=RUNTIME_SIDES[tag.target],
                signature=signature,
                comment=setting.comment,
                source=source,
                flags=tag.flags,
            )
            symbol["declared_target"] = tag.target.lower()
            symbol["group"] = tag.group_name
            symbol["type"] = setting_type
            symbol["setting_kind"] = "fixed" if setting.kind == "fix" else "variable"
            symbol["mutability"] = "startup-fixed" if setting.kind == "fix" else "runtime-variable"
            symbol["default_values"] = list(setting.init_values)
            symbol["command_line_redacted_by_default"] = any(
                token in setting.name.lower() for token in secret_tokens
            )
            symbol["redaction_rule"] = "name matches a default Common.SecretSettingTokens entry"
            symbols.append(symbol)


def _append_migration_symbols(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.MigrationRuleTag, codegen.SourceLocation | None]],
) -> None:
    for tag, source in entries:
        rule_kind, scope, old_name, replacement = tag.args
        family_id = f"migration.{rule_kind}.{scope}.{old_name}"
        symbol = _base_symbol(
            root,
            symbol_id=family_id,
            family_id=family_id,
            kind="migration-rule",
            name=old_name,
            runtime_sides=RUNTIME_SIDES["Common"],
            signature=" ".join(tag.args),
            comment=tag.comment,
            source=source,
        )
        symbol["rule_kind"] = rule_kind
        symbol["scope"] = scope
        symbol["replacement"] = replacement
        symbols.append(symbol)


def _resolve_contract_selector(
    selector: str,
    symbols_by_id: dict[str, dict[str, object]],
    symbols_by_family: dict[str, list[dict[str, object]]],
) -> list[dict[str, object]]:
    exact_symbol = symbols_by_id.get(selector)
    if exact_symbol is not None:
        return [exact_symbol]
    return symbols_by_family.get(selector, [])


def _validate_contract_example(root: Path, example: str) -> None:
    parsed = urlsplit(example)
    if parsed.scheme:
        if parsed.scheme not in {"http", "https"} or not parsed.netloc:
            raise ValueError(f"API contract example must use HTTP(S): {example}")
        return

    path_text, _, _ = example.partition("#")
    path = PurePosixPath(path_text)
    if not path_text or path.is_absolute() or ".." in path.parts:
        raise ValueError(f"API contract example must stay inside the engine root: {example}")
    if not (root / path_text).is_file():
        raise ValueError(f"API contract example does not exist: {example}")


def _apply_api_contracts(
    root: Path,
    symbols: list[dict[str, object]],
    entries: list[tuple[codegen.ApiContractTag, codegen.SourceLocation | None]],
) -> dict[str, object] | None:
    symbols_by_id = {str(symbol["id"]): symbol for symbol in symbols}
    symbols_by_family: dict[str, list[dict[str, object]]] = {}
    for symbol in symbols:
        symbols_by_family.setdefault(str(symbol["family_id"]), []).append(symbol)

    scope_entries = [(tag, source) for tag, source in entries if tag.selector == API_CONTRACT_SCOPE_SELECTOR]
    if len(scope_entries) > 1:
        raise ValueError("Multiple native-codegen scope contracts are not allowed")

    scope_contract: dict[str, object] | None = None
    if scope_entries:
        scope_tag, scope_source = scope_entries[0]
        symbol_ids = sorted(symbols_by_id)
        inventory_sha256 = hashlib.sha256("\n".join(symbol_ids).encode("utf-8")).hexdigest()
        if scope_tag.symbol_count != len(symbol_ids):
            raise ValueError(
                "Native-codegen scope contract SymbolCount is stale: "
                f"declared {scope_tag.symbol_count}, generated {len(symbol_ids)}"
            )
        if scope_tag.inventory_sha256 != inventory_sha256:
            raise ValueError(
                "Native-codegen scope contract InventorySha256 is stale: "
                f"declared {scope_tag.inventory_sha256}, generated {inventory_sha256}"
            )
        for example in scope_tag.examples:
            _validate_contract_example(root, example)
        contract_source = _source(root, scope_source)
        if contract_source is None:
            raise ValueError("Native-codegen scope contract has no source provenance")
        scope_contract = {
            "selector": scope_tag.selector,
            "stability": scope_tag.stability,
            "since": scope_tag.since,
            "symbol_count": scope_tag.symbol_count,
            "inventory_sha256": scope_tag.inventory_sha256,
            "examples": list(scope_tag.examples),
            "source": contract_source,
            "notes": _description(scope_tag.comment),
        }
        for symbol in symbols:
            symbol["stability"] = scope_tag.stability
            symbol["since"] = scope_tag.since
            symbol["deprecated"] = None
            symbol["examples"] = []
            symbol["contract"] = {
                "explicit": True,
                "selector": scope_tag.selector,
                "source": contract_source,
                "notes": "",
            }

    classified_symbol_ids: set[str] = set()
    for tag, source in entries:
        if tag.selector == API_CONTRACT_SCOPE_SELECTOR:
            continue
        selected_symbols = _resolve_contract_selector(tag.selector, symbols_by_id, symbols_by_family)
        if not selected_symbols:
            raise ValueError(f"API contract selector does not match a generated symbol or family: {tag.selector}")

        selected_ids = {str(symbol["id"]) for symbol in selected_symbols}
        duplicates = sorted(selected_ids & classified_symbol_ids)
        if duplicates:
            raise ValueError(f"API contract selectors overlap for symbols: {', '.join(duplicates)}")

        replacement_symbols: list[dict[str, object]] = []
        if tag.replacement is not None:
            replacement_symbols = _resolve_contract_selector(tag.replacement, symbols_by_id, symbols_by_family)
            if not replacement_symbols:
                raise ValueError(f"Deprecated API replacement does not match a symbol or family: {tag.replacement}")
            replacement_ids = {str(symbol["id"]) for symbol in replacement_symbols}
            if selected_ids & replacement_ids:
                raise ValueError(f"Deprecated API replacement resolves to the deprecated selector: {tag.selector}")

        for example in tag.examples:
            _validate_contract_example(root, example)

        contract_source = _source(root, source)
        if contract_source is None:
            raise ValueError(f"API contract has no source provenance: {tag.selector}")

        for symbol in selected_symbols:
            symbol["stability"] = tag.stability
            symbol["since"] = tag.since
            symbol["deprecated"] = (
                {
                    "since": tag.deprecated_since,
                    "replacement": tag.replacement,
                    "removal": tag.removal,
                }
                if tag.stability == "deprecated"
                else None
            )
            symbol["examples"] = list(tag.examples)
            symbol["contract"] = {
                "explicit": True,
                "selector": tag.selector,
                "source": contract_source,
                "notes": _description(tag.comment),
            }

        classified_symbol_ids.update(selected_ids)

    return scope_contract


def generate_api_model(root: Path) -> dict[str, object]:
    root = root.resolve()
    tags, sources = _parse_engine_metadata(root)
    symbols: list[dict[str, object]] = []

    enum_entries = _entries(tags, sources, "ExportEnum")
    value_type_entries = _entries(tags, sources, "ExportValueType")
    entity_entries = _entries(tags, sources, "ExportEntity")
    ref_type_entries = _entries(tags, sources, "ExportRefType")
    property_entries = _entries(tags, sources, "ExportProperty")
    method_entries = _entries(tags, sources, "ExportMethod")
    event_entries = _entries(tags, sources, "ExportEvent")
    setting_entries = _entries(tags, sources, "ExportSettings")
    migration_entries = _entries(tags, sources, "MigrationRule")
    contract_entries = _entries(tags, sources, "ApiContract")
    scope_contracts = [tag for tag, _ in contract_entries if tag.selector == API_CONTRACT_SCOPE_SELECTOR]
    default_stability = scope_contracts[0].stability if scope_contracts else "internal"

    _append_enum_symbols(root, symbols, enum_entries)
    _append_value_type_symbols(root, symbols, value_type_entries)
    _append_entity_symbols(root, symbols, entity_entries)
    _append_ref_type_symbols(root, symbols, ref_type_entries)
    _append_property_symbols(root, symbols, property_entries)
    _append_method_symbols(root, symbols, method_entries)
    _append_event_symbols(root, symbols, event_entries)
    _append_setting_symbols(root, symbols, setting_entries)
    _append_migration_symbols(root, symbols, migration_entries)
    scope_contract = _apply_api_contracts(root, symbols, contract_entries)

    symbols.sort(key=lambda symbol: str(symbol["id"]))
    symbol_ids = [str(symbol["id"]) for symbol in symbols]
    if len(symbol_ids) != len(set(symbol_ids)):
        duplicates = sorted(symbol_id for symbol_id, count in Counter(symbol_ids).items() if count > 1)
        raise ValueError(f"Duplicate generated API symbol IDs: {', '.join(duplicates)}")

    kind_counts = Counter(str(symbol["kind"]) for symbol in symbols)
    stability_counts = Counter(str(symbol["stability"]) for symbol in symbols)
    explicit_contract_symbols = [
        symbol for symbol in symbols if isinstance(symbol.get("contract"), dict) and symbol["contract"]["explicit"]
    ]
    explicit_stability_counts = Counter(str(symbol["stability"]) for symbol in explicit_contract_symbols)
    described_symbol_count = sum(bool(symbol["description"]) for symbol in symbols)
    symbols_without_source = sum(symbol["source"] is None for symbol in symbols)
    declaration_source_files = {
        str(symbol["source"]["path"])
        for symbol in symbols
        if isinstance(symbol.get("source"), dict)
    }
    contract_source_files = {
        str(symbol["contract"]["source"]["path"])
        for symbol in explicit_contract_symbols
        if isinstance(symbol["contract"].get("source"), dict)
    }
    metadata_source_files = sorted(declaration_source_files | contract_source_files)

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_api.py",
        "source_parser": "BuildTools/codegen.py",
        "scope": {
            "repository": "cvet/fonline",
            "surface": "engine-native-codegen",
            "default_stability": default_stability,
            "contract": scope_contract,
            "included": [
                "native script enums, value types, reference types, entities, properties, methods, and events",
                "engine settings parsed from ExportSettings",
                "native migration rules",
                "source-authored API contracts parsed from ApiContract",
            ],
            "excluded": [
                "project-authored script metadata, including remote calls",
                "CMake options and stage helpers",
                "BuildTools command-line interfaces",
                "package layouts and native extension ABI details",
            ],
        },
        "parser_contract": {
            "export_targets": [target.lower() for target in codegen.EXPORT_TARGETS],
            "registration_targets": [target.lower() for target in codegen.REGISTRATION_TARGETS],
            "engine_hook_names": list(codegen.ENGINE_HOOK_NAMES),
            "migration_rule_kinds": list(codegen.MIGRATION_RULE_KINDS),
            "api_stability_labels": list(codegen.API_STABILITY_LABELS),
            "api_contract_scope_selector": API_CONTRACT_SCOPE_SELECTOR,
        },
        "summary": {
            "symbol_count": len(symbols),
            "symbols_by_kind": dict(sorted(kind_counts.items())),
            "symbols_by_stability": dict(sorted(stability_counts.items())),
            "explicit_contract_declaration_count": len(contract_entries),
            "explicit_contract_symbol_count": len(explicit_contract_symbols),
            "default_contract_symbol_count": len(symbols) - len(explicit_contract_symbols),
            "explicit_contracts_by_stability": dict(sorted(explicit_stability_counts.items())),
            "described_symbol_count": described_symbol_count,
            "missing_description_count": len(symbols) - described_symbol_count,
            "symbols_without_source_count": symbols_without_source,
            "metadata_source_file_count": len(metadata_source_files),
            "contract_source_file_count": len(contract_source_files),
        },
        "metadata_source_files": metadata_source_files,
        "symbols": symbols,
    }


def render_api_model(root: Path) -> str:
    return json.dumps(generate_api_model(root), indent=2, ensure_ascii=True) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate source-backed FOnline API documentation model")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write the generated API model")
    mode.add_argument("--check", action="store_true", help="fail when the committed API model is stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    output_path = root / args.output
    model = generate_api_model(root)
    generated = json.dumps(model, indent=2, ensure_ascii=True) + "\n"

    if args.write:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(generated, encoding="utf-8", newline="\n")
        print(f"Wrote {output_path.relative_to(root).as_posix()}")
        return 0

    if not output_path.is_file():
        print(f"Generated API model is missing: {args.output}", file=sys.stderr)
        return 1
    if output_path.read_text(encoding="utf-8") != generated:
        print("Generated API model is stale: run python BuildTools/docs_api.py --write", file=sys.stderr)
        return 1

    summary = model["summary"]
    counts = summary["symbols_by_kind"]
    explicit_contract_count = summary.get("explicit_contract_symbol_count", 0)
    contract_symbol_label = "symbol" if explicit_contract_count == 1 else "symbols"
    print(
        "Generated API model is current: "
        f"{counts.get('method', 0)} methods, "
        f"{counts.get('property', 0)} properties, "
        f"{counts.get('event', 0)} events, "
        f"{counts.get('setting', 0)} settings, "
        f"{explicit_contract_count} explicitly classified {contract_symbol_label}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
