from __future__ import annotations

import argparse
import copy
import json
import re
import sys
from pathlib import Path
from typing import Any, Callable
from urllib.parse import quote

import docs_localization


SCHEMA_VERSION = 1
DEFAULT_CATALOG = "Docs/description-translations.ru.json"
DEFAULT_OUTPUT = "Docs/generated/description-translation-status.json"
GENERATED_BY = "BuildTools/docs_description_translations.py"
VALID_ENFORCEMENT = {"registered-translations-current", "complete"}
MODEL_PATHS = {
    "ai-control-protocol": "Docs/generated/ai-control-protocol.json",
    "api": "Docs/generated/api.json",
    "audio": "Docs/generated/audio.json",
    "cli": "Docs/generated/cli.json",
    "cmake": "Docs/generated/cmake.json",
    "effect-format": "Docs/generated/effect-format.json",
    "font-format": "Docs/generated/font-format.json",
    "gui-runtime": "Docs/generated/gui-runtime.json",
    "helper-cli": "Docs/generated/helper-cli.json",
    "image-format": "Docs/generated/image-format.json",
    "map-format": "Docs/generated/map-format.json",
    "model-format": "Docs/generated/model-format.json",
    "native-extension": "Docs/generated/native-extension.json",
    "package": "Docs/generated/package.json",
    "particle-format": "Docs/generated/particle-format.json",
    "prototype-format": "Docs/generated/prototype-format.json",
    "public-examples": "Docs/generated/public-examples.json",
    "support-matrix": "Docs/generated/support-matrix.json",
    "text-format": "Docs/generated/text-format.json",
    "video": "Docs/generated/video.json",
}
TRANSLATABLE_FIELDS = {
    "compiled_backends",
    "default_behavior",
    "description",
    "exit_gate",
    "limitations",
    "notes",
    "purpose",
    "qualification",
    "rationale",
    "requirement",
    "runtime_evidence",
    "summary",
    "support_note",
}
DOMAIN_STRING_FIELDS = {
    "audio": {"role"},
    "effect-format": {"behavior"},
    "font-format": {"role"},
    "helper-cli": {"invocation_owner"},
    "image-format": {"availability"},
    "map-format": {"applies_to", "cardinality", "default", "excluded_reason"},
    "model-format": {"runtime_effect"},
    "particle-format": {"default", "family"},
    "prototype-format": {"default", "excluded_reason", "resolves_to"},
    "support-matrix": {"compiler", "host", "platforms", "target"},
    "text-format": {"behavior", "missing_behavior"},
}
DOMAIN_LIST_FIELDS = {
    "ai-control-protocol": {"excluded", "included"},
    "audio": {"excluded", "included"},
    "cli": {"excluded", "included"},
    "cmake": {"excluded", "included", "option_override_precedence"},
    "effect-format": {"excluded", "included"},
    "font-format": {"excluded", "included"},
    "gui-runtime": {"excluded", "included"},
    "helper-cli": {"excluded", "included"},
    "image-format": {"excluded", "included"},
    "map-format": {"excluded", "included"},
    "model-format": {"excluded", "included", "requirements"},
    "particle-format": {"excluded", "included"},
    "native-extension": {"excluded", "included"},
    "prototype-format": {"excluded", "included"},
    "support-matrix": {"applications"},
    "text-format": {"excluded", "included"},
    "video": {"excluded", "included"},
}
IDENTITY_FIELDS = (
    "id",
    "stable_id",
    "name",
    "dest",
    "destination",
    "command",
    "path",
    "key",
    "field",
    "option",
    "token",
    "platform",
    "platforms",
    "type",
    "kind",
    "event",
    "method",
    "source",
    "value",
)
INLINE_CODE_PATTERN = re.compile(r"`[^`]+`")


class _DuplicateKeyError(ValueError):
    pass


def _reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise _DuplicateKeyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _load_json(path: Path) -> object:
    return json.loads(
        path.read_text(encoding="utf-8"),
        object_pairs_hook=_reject_duplicate_keys,
    )


def _is_scalar_identity(value: object) -> bool:
    return isinstance(value, (str, int)) and str(value).strip() != ""


def _is_translatable(domain: str, path: tuple[str, ...], field: str, value: object) -> bool:
    if isinstance(value, str) and value.strip():
        if field in TRANSLATABLE_FIELDS:
            return True
        if field in DOMAIN_STRING_FIELDS.get(domain, set()):
            return True
        if domain == "support-matrix" and path == ("policy",):
            return True
        if domain == "public-examples" and path == ("program", "owners"):
            return True
        if (
            domain == "audio"
            and path
            and (path[0] == "formats" or path[0].endswith("_rules"))
            and field == "name"
        ):
            return True
        if (
            domain == "video"
            and path
            and (path[0] == "formats" or path[0].endswith("_rules"))
            and field == "name"
        ):
            return True
        if (
            domain == "model-format"
            and path
            and path[0] in {"assets", "rules"}
            and field == "name"
        ):
            return True
        if (
            domain == "image-format"
            and path
            and path[0]
            in {
                "descriptor_fields",
                "baking_rules",
                "runtime_rules",
                "validation_rules",
            }
            and field == "name"
        ):
            return True
        if (
            domain == "effect-format"
            and path
            and path[0]
            in {
                "sections",
                "resources",
                "baking_rules",
                "runtime_rules",
                "validation_rules",
            }
            and field == "name"
        ):
            return True
        if (
            domain == "font-format"
            and path
            and path[0]
            in {
                "fofnt_fields",
                "bmfont_rules",
                "binding_rules",
                "layout_rules",
                "rendering_rules",
                "validation_rules",
            }
            and field == "name"
        ):
            return True
        if (
            domain == "gui-runtime"
            and path
            and path[0].endswith("_rules")
            and field == "name"
        ):
            return True
        if (
            domain == "helper-cli"
            and path
            and path[0] == "helpers"
            and len(path) == 2
            and field == "name"
        ):
            return True
        if (
            domain == "ai-control-protocol"
            and path
            and path[0]
            in {
                "wire_rules",
                "error_codes",
                "security_rules",
                "integration_rules",
                "validation_rules",
            }
            and field == "name"
        ):
            return True
        if (
            domain == "native-extension"
            and path
            and path[0] == "binding_rules"
            and field == "name"
        ):
            return True
        if (
            domain == "particle-format"
            and path
            and (
                path[0] == "object_families"
                or path[0]
                in {
                    "xml_rules",
                    "tooling_rules",
                    "runtime_rules",
                    "integration_rules",
                    "validation_rules",
                }
            )
            and field == "name"
        ):
            return True
        if (
            domain in {"map-format", "prototype-format"}
            and path
            and path[0] == "rules"
            and field == "name"
        ):
            return True
        if (
            domain == "text-format"
            and path
            and path[0].endswith("_rules")
            and field == "name"
        ):
            return True
    if isinstance(value, list) and field in DOMAIN_LIST_FIELDS.get(domain, set()):
        if not value or any(not isinstance(item, str) or not item.strip() for item in value):
            raise ValueError(
                f"translatable list field {domain}:{_locator((*path, field))} "
                "must contain non-empty strings"
            )
        return True
    return False


def _contains_translatable(domain: str, node: object, path: tuple[str, ...]) -> bool:
    if isinstance(node, dict):
        return any(
            _is_translatable(domain, path, str(field), value)
            or _contains_translatable(domain, value, (*path, str(field)))
            for field, value in node.items()
        )
    if isinstance(node, list):
        return any(_contains_translatable(domain, value, path) for value in node)
    return False


def _list_identity(domain: str, values: list[object], path: tuple[str, ...]) -> str:
    relevant = [
        value
        for value in values
        if _contains_translatable(domain, value, path)
    ]
    if not relevant:
        return ""
    if any(not isinstance(value, dict) for value in relevant):
        raise ValueError(
            f"translatable list {'/'.join(path)} must contain identifiable objects"
        )
    for field in IDENTITY_FIELDS:
        identities = [value.get(field) for value in relevant]
        if all(_is_scalar_identity(identity) for identity in identities):
            normalized = [str(identity) for identity in identities]
            if len(normalized) == len(set(normalized)):
                return field
    raise ValueError(
        f"translatable list {'/'.join(path)} has no unique stable identity field"
    )


def _locator(path: tuple[str, ...]) -> str:
    return "/" + "/".join(path)


def _walk_entries(
    domain: str,
    node: object,
    path: tuple[str, ...],
    visit: Callable[[str, object, Callable[[object], None]], None],
) -> None:
    if isinstance(node, dict):
        for raw_field, value in node.items():
            field = str(raw_field)
            next_path = (*path, field)
            if _is_translatable(domain, path, field, value):
                source = copy.deepcopy(value)

                def replace(replacement: object, *, owner: dict[str, object] = node, key: str = field) -> None:
                    owner[key] = replacement

                visit(_locator(next_path), source, replace)
            elif isinstance(value, (dict, list)):
                _walk_entries(domain, value, next_path, visit)
        return

    if isinstance(node, list):
        identity_field = _list_identity(domain, node, path)
        if not identity_field:
            return
        for value in node:
            if not _contains_translatable(domain, value, path):
                continue
            assert isinstance(value, dict)
            identity = quote(str(value[identity_field]), safe="")
            _walk_entries(
                domain,
                value,
                (*path, f"@{identity_field}={identity}"),
                visit,
            )


def inventory_model(domain: str, model: dict[str, Any]) -> dict[str, object]:
    if domain not in MODEL_PATHS:
        raise ValueError(f"unknown generated-description domain: {domain}")
    entries: dict[str, object] = {}

    def collect(locator: str, source: object, replace: Callable[[object], None]) -> None:
        del replace
        if locator in entries:
            raise ValueError(f"duplicate generated-description locator: {domain}:{locator}")
        entries[locator] = source

    _walk_entries(domain, model, (), collect)
    return dict(sorted(entries.items()))


def _validate_catalog_header(raw: object) -> dict[str, Any]:
    if not isinstance(raw, dict):
        raise ValueError("generated-description translation catalog must be an object")
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"generated-description translation catalog schema_version must be {SCHEMA_VERSION}"
        )
    if raw.get("source_locale") != "en" or raw.get("target_locale") != "ru":
        raise ValueError("generated-description translation catalog must map en to ru")
    enforcement = raw.get("enforcement")
    if enforcement not in VALID_ENFORCEMENT:
        raise ValueError(
            "generated-description translation catalog enforcement must be one of "
            + ", ".join(sorted(VALID_ENFORCEMENT))
        )
    domains = raw.get("domains")
    if not isinstance(domains, dict):
        raise ValueError("generated-description translation catalog domains must be an object")
    unknown_domains = sorted(set(domains) - set(MODEL_PATHS))
    if unknown_domains:
        raise ValueError(
            "generated-description translation catalog has unknown domains: "
            + ", ".join(unknown_domains)
        )
    return raw


def load_catalog(root: Path, catalog_path: str = DEFAULT_CATALOG) -> dict[str, Any]:
    return _validate_catalog_header(_load_json(root / catalog_path))


def _domain_catalog(catalog: dict[str, Any], domain: str) -> dict[str, dict[str, object]]:
    raw_domain = catalog["domains"].get(domain, {"entries": {}})
    if not isinstance(raw_domain, dict) or set(raw_domain) != {"entries"}:
        raise ValueError(f"generated-description catalog domain {domain} must contain only entries")
    entries = raw_domain["entries"]
    if not isinstance(entries, dict):
        raise ValueError(f"generated-description catalog domain {domain} entries must be an object")
    normalized: dict[str, dict[str, object]] = {}
    for locator, raw_entry in entries.items():
        if not isinstance(locator, str) or not locator.startswith("/"):
            raise ValueError(f"generated-description catalog domain {domain} has invalid locator")
        if not isinstance(raw_entry, dict):
            raise ValueError(f"generated-description translation {domain}:{locator} must be an object")
        allowed_fields = {"source_sha256", "translation", "preserve_source"}
        unknown_fields = set(raw_entry) - allowed_fields
        if unknown_fields:
            raise ValueError(
                f"generated-description translation {domain}:{locator} has unknown fields: "
                + ", ".join(sorted(unknown_fields))
            )
        source_hash = raw_entry.get("source_sha256")
        translation = raw_entry.get("translation")
        preserve_source = raw_entry.get("preserve_source", False)
        if not isinstance(source_hash, str) or not re.fullmatch(r"[0-9a-f]{64}", source_hash):
            raise ValueError(
                f"generated-description translation {domain}:{locator} has invalid source_sha256"
            )
        translation_is_string = isinstance(translation, str) and bool(translation.strip())
        translation_is_list = (
            isinstance(translation, list)
            and bool(translation)
            and all(isinstance(item, str) and item.strip() for item in translation)
        )
        if not translation_is_string and not translation_is_list:
            raise ValueError(
                f"generated-description translation {domain}:{locator} must have a non-empty "
                "string or string-array translation"
            )
        if not isinstance(preserve_source, bool):
            raise ValueError(
                f"generated-description translation {domain}:{locator} preserve_source must be boolean"
            )
        normalized[locator] = {
            "source_sha256": source_hash,
            "translation": translation,
            "preserve_source": preserve_source,
        }
    return normalized


def _source_text(value: object) -> str:
    if isinstance(value, str):
        return value
    if isinstance(value, list) and all(isinstance(item, str) for item in value):
        return json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    raise ValueError("generated-description source must be a string or string array")


def _inline_code(value: object) -> list[str]:
    if isinstance(value, str):
        return INLINE_CODE_PATTERN.findall(value)
    if isinstance(value, list):
        return [token for item in value for token in INLINE_CODE_PATTERN.findall(str(item))]
    return []


def _validate_translation(
    domain: str,
    locator: str,
    source: object,
    entry: dict[str, object],
) -> object:
    expected_hash = docs_localization.normalized_sha256(_source_text(source))
    if entry["source_sha256"] != expected_hash:
        raise ValueError(
            f"stale generated-description translation {domain}:{locator}; "
            f"expected source_sha256 {expected_hash}"
        )
    translation = entry["translation"]
    if isinstance(source, str) != isinstance(translation, str):
        raise ValueError(
            f"generated-description translation {domain}:{locator} must preserve source value type"
        )
    if isinstance(source, list):
        if not isinstance(translation, list) or len(source) != len(translation):
            raise ValueError(
                f"generated-description translation {domain}:{locator} must preserve list length"
            )
    preserve_source = bool(entry["preserve_source"])
    if translation == source and not preserve_source:
        raise ValueError(
            f"generated-description translation {domain}:{locator} repeats source without preserve_source"
        )
    if translation != source and preserve_source:
        raise ValueError(
            f"generated-description translation {domain}:{locator} sets preserve_source for changed text"
        )
    if _inline_code(source) != _inline_code(translation):
        raise ValueError(
            f"generated-description translation {domain}:{locator} must preserve inline code spans"
        )
    return translation


def _build_translation_memory(
    domain: str,
    inventory: dict[str, object],
    translations: dict[str, dict[str, object]],
) -> tuple[dict[str, object], dict[tuple[str, str], tuple[object, str]]]:
    direct: dict[str, object] = {}
    candidates: dict[tuple[str, str], tuple[object, str]] = {}
    ambiguous: set[tuple[str, str]] = set()
    for locator, entry in translations.items():
        source = inventory.get(locator)
        if source is None:
            continue
        translated = _validate_translation(domain, locator, source, entry)
        direct[locator] = translated
        source_text = _source_text(source)
        key = (docs_localization.normalized_sha256(source_text), source_text)
        previous = candidates.get(key)
        if previous is None:
            candidates[key] = (translated, locator)
        elif previous[0] != translated:
            ambiguous.add(key)
    return direct, {key: value for key, value in candidates.items() if key not in ambiguous}


def _translation_memory_key(source: object) -> tuple[str, str]:
    source_text = _source_text(source)
    return docs_localization.normalized_sha256(source_text), source_text


def apply_translations(
    root: Path,
    domain: str,
    model: dict[str, Any],
    catalog_path: str = DEFAULT_CATALOG,
) -> dict[str, Any]:
    catalog = load_catalog(root, catalog_path)
    translations = _domain_catalog(catalog, domain)
    inventory = inventory_model(domain, model)
    unknown = sorted(set(translations) - set(inventory))
    if unknown:
        raise ValueError(
            f"generated-description catalog domain {domain} has unknown locators: "
            + ", ".join(unknown)
        )
    direct, memory = _build_translation_memory(domain, inventory, translations)
    translated = copy.deepcopy(model)

    def apply(locator: str, source: object, replace: Callable[[object], None]) -> None:
        if locator in direct:
            replace(direct[locator])
            return
        reused = memory.get(_translation_memory_key(source))
        if reused is not None:
            replace(reused[0])

    _walk_entries(domain, translated, (), apply)
    return translated


def generate_status(
    root: Path,
    catalog_path: str = DEFAULT_CATALOG,
    *,
    enforce_complete: bool = False,
) -> dict[str, Any]:
    catalog = load_catalog(root, catalog_path)
    domain_statuses: dict[str, object] = {}
    total_count = 0
    translated_count = 0
    missing_count = 0
    for domain, model_path in sorted(MODEL_PATHS.items()):
        model_raw = _load_json(root / model_path)
        if not isinstance(model_raw, dict):
            raise ValueError(f"generated-description model must be an object: {model_path}")
        inventory = inventory_model(domain, model_raw)
        translations = _domain_catalog(catalog, domain)
        unknown = sorted(set(translations) - set(inventory))
        if unknown:
            raise ValueError(
                f"generated-description catalog domain {domain} has unknown locators: "
                + ", ".join(unknown)
            )
        direct, memory = _build_translation_memory(domain, inventory, translations)
        records: list[dict[str, object]] = []
        current_count = 0
        for locator, source in inventory.items():
            status = "missing"
            translation_source_locator: str | None = None
            if locator in direct:
                status = "current"
                current_count += 1
            else:
                reused = memory.get(_translation_memory_key(source))
                if reused is not None:
                    status = "current"
                    translation_source_locator = reused[1]
                    current_count += 1
            record: dict[str, object] = {
                "locator": locator,
                "source": source,
                "source_sha256": docs_localization.normalized_sha256(_source_text(source)),
                "status": status,
            }
            if translation_source_locator is not None:
                record["translation_source_locator"] = translation_source_locator
            records.append(record)
        domain_missing = len(records) - current_count
        total_count += len(records)
        translated_count += current_count
        missing_count += domain_missing
        domain_statuses[domain] = {
            "source_model": model_path,
            "entry_count": len(records),
            "current_count": current_count,
            "missing_count": domain_missing,
            "complete": domain_missing == 0,
            "entries": records,
        }

    complete = missing_count == 0
    if (enforce_complete or catalog["enforcement"] == "complete") and not complete:
        raise ValueError(
            f"generated-description translations are incomplete: {missing_count} of {total_count} missing"
        )
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_catalog": catalog_path,
        "source_locale": "en",
        "target_locale": "ru",
        "enforcement": catalog["enforcement"],
        "summary": {
            "domain_count": len(MODEL_PATHS),
            "entry_count": total_count,
            "current_count": translated_count,
            "missing_count": missing_count,
            "complete": complete,
        },
        "domains": domain_statuses,
    }


def render_status(
    root: Path,
    catalog_path: str = DEFAULT_CATALOG,
    *,
    enforce_complete: bool = False,
) -> str:
    return json.dumps(
        generate_status(root, catalog_path, enforce_complete=enforce_complete),
        ensure_ascii=False,
        indent=2,
    ) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate Russian translations of prose embedded in generated documentation models."
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--catalog", default=DEFAULT_CATALOG)
    parser.add_argument("--enforce-complete", action="store_true")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        content = render_status(
            root,
            args.catalog,
            enforce_complete=args.enforce_complete,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exception:
        print(
            f"Unable to validate generated-description translations: {exception}",
            file=sys.stderr,
        )
        return 1

    output = root / DEFAULT_OUTPUT
    if args.write:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(content, encoding="utf-8", newline="\n")
        status = json.loads(content)
        summary = status["summary"]
        print(
            "Wrote generated-description translation status: "
            f"{summary['current_count']}/{summary['entry_count']} current"
        )
        return 0

    if not output.is_file() or output.read_text(encoding="utf-8") != content:
        print(
            "Generated-description translation status is missing or stale; "
            "run python BuildTools/docs_description_translations.py --write",
            file=sys.stderr,
        )
        return 1
    status = json.loads(content)
    summary = status["summary"]
    print(
        "Generated-description translations are current: "
        f"{summary['current_count']}/{summary['entry_count']} translated, "
        f"{summary['missing_count']} pending"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
