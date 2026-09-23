from __future__ import annotations

import argparse
import html
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path, PurePosixPath

import docs_ai_delivery
import docs_localization


SCHEMA_VERSION = 3
GENERATED_BY = "BuildTools/docs_site.py"
DEFAULT_MANIFEST = "Docs/documentation-manifest.json"
DEFAULT_NAVIGATION_OUTPUT = "_data/docs-site.json"
DEFAULT_SEARCH_OUTPUT = "assets/docs-search.json"
DEFAULT_RUSSIAN_SEARCH_OUTPUT = "assets/docs-search.ru.json"
DEFAULT_ROUTES_OUTPUT = "Docs/generated/document-routes.json"
OUTPUT_PATHS = (
    DEFAULT_NAVIGATION_OUTPUT,
    DEFAULT_SEARCH_OUTPUT,
    DEFAULT_RUSSIAN_SEARCH_OUTPUT,
    DEFAULT_ROUTES_OUTPUT,
)

HEADING_RE = re.compile(r"^(?P<marks>#{1,6})\s+(?P<title>.+?)\s*#*\s*$")
FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")
LINK_RE = re.compile(r"!?\[([^\]]*)\]\([^)]*\)")
HTML_RE = re.compile(r"<[^>]+>")
TOKEN_RE = re.compile(r"\w[\w_.:+/?<>-]*", re.UNICODE)
CAMEL_PART_RE = re.compile(r"[A-Z]+(?=[A-Z][a-z]|\d|$)|[A-Z]?[a-z]+|\d+")
VALID_ID_RE = re.compile(r"[a-z0-9][a-z0-9-]*")
URL_RE = re.compile(r"https?://\S+", re.IGNORECASE)
MARKDOWN_TARGET_RE = re.compile(r"\]\([^)]*\)")

STOP_WORDS = {
    "a",
    "an",
    "and",
    "are",
    "as",
    "at",
    "be",
    "by",
    "for",
    "from",
    "has",
    "in",
    "into",
    "is",
    "it",
    "of",
    "on",
    "or",
    "that",
    "the",
    "this",
    "to",
    "use",
    "with",
    "а",
    "без",
    "в",
    "во",
    "для",
    "же",
    "и",
    "из",
    "или",
    "как",
    "к",
    "на",
    "не",
    "но",
    "от",
    "по",
    "при",
    "с",
    "со",
    "что",
    "это",
}

MAX_DOCUMENT_FREQUENCY_RATIO = 0.60
QUERY_TOKEN_RE = re.compile(r"\w[\w_.:+/?<>-]*", re.UNICODE)
DIATAXIS_TITLES = {
    "en": {
        "none": "Overview",
        "tutorial": "Tutorial",
        "how-to": "How-to",
        "reference": "Reference",
        "explanation": "Explanation",
        "troubleshooting": "Troubleshooting",
    },
    "ru": {
        "none": "Обзор",
        "tutorial": "Руководство",
        "how-to": "Инструкция",
        "reference": "Справочник",
        "explanation": "Объяснение",
        "troubleshooting": "Устранение неполадок",
    },
}


def _require_object(parent: dict[str, object], key: str, label: str) -> dict[str, object]:
    value = parent.get(key)
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def _require_string(parent: dict[str, object], key: str, label: str) -> str:
    value = parent.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _site_config(manifest: dict[str, object]) -> dict[str, object]:
    docs_ai_delivery._versioning_config(manifest)
    docs_ai_delivery._localization_config(manifest)
    config = _require_object(manifest, "site_delivery", "documentation manifest site_delivery")
    if config.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"documentation manifest site_delivery schema_version must be {SCHEMA_VERSION}")
    if config.get("layout") != "default":
        raise ValueError("site_delivery layout must be default")
    if config.get("navigation_data_path") != DEFAULT_NAVIGATION_OUTPUT:
        raise ValueError(f"site_delivery navigation_data_path must be {DEFAULT_NAVIGATION_OUTPUT}")

    search = _require_object(config, "search", "site_delivery search")
    if search.get("path") != DEFAULT_SEARCH_OUTPUT:
        raise ValueError(f"site_delivery search path must be {DEFAULT_SEARCH_OUTPUT}")
    expected_locale_paths = {
        "en": DEFAULT_SEARCH_OUTPUT,
        "ru": DEFAULT_RUSSIAN_SEARCH_OUTPUT,
    }
    if search.get("locale_paths") != expected_locale_paths:
        raise ValueError(
            f"site_delivery search locale_paths must be {expected_locale_paths}"
        )
    max_bytes = search.get("max_bytes")
    if not isinstance(max_bytes, int) or isinstance(max_bytes, bool) or max_bytes <= 0:
        raise ValueError("site_delivery search max_bytes must be a positive integer")
    if search.get("minimum_query_length") != 2:
        raise ValueError("site_delivery search minimum_query_length must be 2")

    routing = _require_object(config, "routing", "site_delivery routing")
    expected_routing = {
        "path": DEFAULT_ROUTES_OUTPUT,
        "current_permalink_strategy": "source-markdown-path",
        "planned_permalink_strategy": "manifest-target-path",
        "legacy_route_strategy": "durable-markdown-pointer",
        "migration_status": "planned",
    }
    for key, expected in expected_routing.items():
        if routing.get(key) != expected:
            raise ValueError(f"site_delivery routing {key} must be {expected}")

    groups = config.get("navigation")
    if not isinstance(groups, list) or not groups:
        raise ValueError("site_delivery navigation must be a non-empty array")
    seen_group_ids: set[str] = set()
    for group_index, group_value in enumerate(groups):
        label = f"site_delivery navigation group {group_index}"
        if not isinstance(group_value, dict):
            raise ValueError(f"{label} must be an object")
        group_id = _require_string(group_value, "id", f"{label} id")
        if not VALID_ID_RE.fullmatch(group_id):
            raise ValueError(f"{label} id is invalid: {group_id}")
        if group_id in seen_group_ids:
            raise ValueError(f"site_delivery repeats navigation group id: {group_id}")
        seen_group_ids.add(group_id)
        _require_string(group_value, "title", f"{label} title")
        _require_string(group_value, "title_ru", f"{label} Russian title")
        document_ids = group_value.get("document_ids")
        if (
            not isinstance(document_ids, list)
            or any(not isinstance(document_id, str) or not document_id for document_id in document_ids)
        ):
            raise ValueError(f"{label} document_ids must be a string array")
    return config


def _site_path(source_path: str) -> str:
    if source_path == "README.md":
        return "/"
    path = PurePosixPath(source_path)
    if path.name == "index.md":
        return "/" + str(path.parent) + "/"
    return "/" + str(path.with_suffix(".html"))


def _repository_markdown_path(value: object, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{label} must be a non-empty Markdown path")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or path.suffix.lower() != ".md":
        raise ValueError(f"{label} must be a repository Markdown path")
    return value


def _translation_source_paths(
    record: dict[str, object],
    localization: dict[str, object],
) -> dict[str, str]:
    if not (
        record["visibility"] == "public"
        and record["human"] is True
        and record["translation"] == "required"
    ):
        return {}

    document_id = str(record["id"])
    target = _repository_markdown_path(
        record["target"],
        f"documentation route {document_id} target",
    )
    if target.startswith("Docs/en/"):
        return {
            "en": target,
            "ru": "Docs/ru/" + target.removeprefix("Docs/en/"),
        }

    entrypoint_targets = _require_object(
        localization,
        "entrypoint_targets",
        "documentation localization entrypoint_targets",
    )
    targets = entrypoint_targets.get(document_id)
    if not isinstance(targets, dict):
        raise ValueError(
            f"documentation route {document_id} needs explicit en/ru entrypoint targets"
        )
    en_target = _repository_markdown_path(
        targets.get("en"),
        f"documentation route {document_id} English target",
    )
    ru_target = _repository_markdown_path(
        targets.get("ru"),
        f"documentation route {document_id} Russian target",
    )
    if en_target != target:
        raise ValueError(
            f"documentation route {document_id} English entrypoint target must match {target}"
        )
    if ru_target == en_target:
        raise ValueError(
            f"documentation route {document_id} Russian entrypoint target must differ from English"
        )
    return {"en": en_target, "ru": ru_target}


def _canonical_target_owners(
    records: list[dict[str, object]],
) -> dict[str, str]:
    grouped: dict[str, list[dict[str, object]]] = defaultdict(list)
    for record in records:
        if record["visibility"] != "public":
            continue
        target = _repository_markdown_path(
            record["target"],
            f"documentation route {record['id']} target",
        )
        grouped[target].append(record)

    owners: dict[str, str] = {}
    for target, target_records in grouped.items():
        candidates = [
            record for record in target_records if record["disposition"] != "replace"
        ]
        if len(target_records) == 1:
            owners[target] = str(target_records[0]["id"])
        elif len(candidates) == 1:
            owners[target] = str(candidates[0]["id"])
        else:
            record_ids = ", ".join(sorted(str(record["id"]) for record in target_records))
            raise ValueError(
                f"documentation target {target} needs exactly one non-replace owner: {record_ids}"
            )
    return owners


def render_routes(
    root: Path,
    manifest: dict[str, object],
    config: dict[str, object],
    records: list[dict[str, object]],
) -> str:
    publishing = _require_object(manifest, "publishing", "documentation manifest publishing")
    base_url = _require_string(
        publishing,
        "production_url",
        "documentation publishing production_url",
    )
    versioning = docs_ai_delivery._versioning_config(manifest)
    localization = docs_ai_delivery._localization_config(manifest)
    current_version = _require_object(
        versioning,
        "current",
        "documentation versioning current",
    )
    releases = _require_object(
        versioning,
        "releases",
        "documentation versioning releases",
    )
    routing = _require_object(config, "routing", "site_delivery routing")
    public_records = sorted(
        (record for record in records if record["visibility"] == "public"),
        key=lambda record: str(record["id"]),
    )
    records_by_id = {str(record["id"]): record for record in public_records}
    target_owners = _canonical_target_owners(public_records)

    entrypoint_targets = _require_object(
        localization,
        "entrypoint_targets",
        "documentation localization entrypoint_targets",
    )
    required_entrypoint_ids = {
        str(record["id"])
        for record in public_records
        if record["human"] is True
        and record["translation"] == "required"
        and not str(record["target"]).startswith("Docs/en/")
    }
    extra_entrypoint_ids = sorted(set(entrypoint_targets) - required_entrypoint_ids)
    if extra_entrypoint_ids:
        raise ValueError(
            "documentation localization has unused entrypoint targets: "
            + ", ".join(extra_entrypoint_ids)
        )

    seen_current_paths: dict[str, str] = {}
    planned_path_owners: dict[str, str] = {}
    locale_path_owners: dict[tuple[str, str], str] = {}
    rendered_routes = []
    legacy_redirects = []
    translation_owner_ids: set[str] = set()
    complete_translation_owner_ids: set[str] = set()

    for record in public_records:
        document_id = str(record["id"])
        source_path = str(record["path"])
        current_path = _site_path(source_path)
        previous_current_owner = seen_current_paths.get(current_path)
        if previous_current_owner is not None:
            raise ValueError(
                f"documentation current route {current_path} is shared by "
                f"{previous_current_owner} and {document_id}"
            )
        seen_current_paths[current_path] = document_id

        target_source_path = _repository_markdown_path(
            record["target"],
            f"documentation route {document_id} target",
        )
        canonical_document_id = target_owners[target_source_path]
        canonical_record = records_by_id[canonical_document_id]
        canonical_target_source_path = _repository_markdown_path(
            canonical_record["target"],
            f"documentation route {canonical_document_id} target",
        )
        if (
            record["state"] == "redirect"
            and record.get("redirect_to") != canonical_document_id
        ):
            raise ValueError(
                f"documentation redirect {document_id} must name canonical owner "
                f"{canonical_document_id}"
            )
        planned_path = _site_path(canonical_target_source_path)
        previous_planned_owner = planned_path_owners.get(planned_path)
        if (
            previous_planned_owner is not None
            and previous_planned_owner != canonical_document_id
        ):
            raise ValueError(
                f"documentation planned route {planned_path} is shared by canonical documents "
                f"{previous_planned_owner} and {canonical_document_id}"
            )
        planned_path_owners[planned_path] = canonical_document_id

        locale_source_paths = _translation_source_paths(record, localization)
        locale_routes = []
        for locale_id in ("en", "ru"):
            locale_source_path = locale_source_paths.get(locale_id)
            if locale_source_path is None:
                continue
            locale_path = _site_path(locale_source_path)
            locale_key = (locale_id, locale_path)
            previous_locale_owner = locale_path_owners.get(locale_key)
            if (
                previous_locale_owner is not None
                and previous_locale_owner != canonical_document_id
            ):
                raise ValueError(
                    f"documentation {locale_id} route {locale_path} is shared by canonical "
                    f"documents {previous_locale_owner} and {canonical_document_id}"
                )
            locale_path_owners[locale_key] = canonical_document_id
            available = (root / locale_source_path).is_file()
            locale_routes.append(
                {
                    "locale": locale_id,
                    "source_path": locale_source_path,
                    "path": locale_path,
                    "url": base_url.rstrip("/") + locale_path,
                    "availability": "available" if available else "missing",
                }
            )

        if locale_routes:
            translation_owner_ids.add(canonical_document_id)
            if all(route["availability"] == "available" for route in locale_routes):
                complete_translation_owner_ids.add(canonical_document_id)

        redirect_required = current_path != planned_path
        route = {
            "id": document_id,
            "canonical_document_id": canonical_document_id,
            "title": record["title"],
            "state": record["state"],
            "human": record["human"],
            "translation": record["translation"],
            "disposition": record["disposition"],
            "source_path": source_path,
            "current_path": current_path,
            "current_url": base_url.rstrip("/") + current_path,
            "planned_source_path": canonical_target_source_path,
            "planned_path": planned_path,
            "planned_url": base_url.rstrip("/") + planned_path,
            "migration_status": "planned" if redirect_required else "current",
            "redirect_required": redirect_required,
            "locale_routes": locale_routes,
        }
        if "redirect_to" in record:
            route["redirect_to"] = record["redirect_to"]
        rendered_routes.append(route)

        if redirect_required:
            legacy_redirects.append(
                {
                    "document_id": document_id,
                    "canonical_document_id": canonical_document_id,
                    "from": current_path,
                    "to": planned_path,
                    "strategy": routing["legacy_route_strategy"],
                    "status": "required-before-route-migration",
                }
            )

    current_version_output = {
        "channel": current_version["channel"],
        "kind": current_version["kind"],
        "label": current_version["label"],
        "value": current_version["source_ref"],
        "source_ref": current_version["source_ref"],
        "path_prefix": current_version["path_prefix"],
        "support": current_version["support"],
    }
    output = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "canonical_base_url": base_url,
        "version": current_version_output,
        "release_versions": dict(releases),
        "localization": {
            "canonical_locale": localization["canonical_locale"],
            "path_strategy": localization["path_strategy"],
            "translation_hash": localization["translation_hash"],
            "translation_pending": localization["translation_pending"],
            "locales": [dict(locale) for locale in localization["locales"]],
        },
        "routing": dict(routing),
        "route_count": len(rendered_routes),
        "redirect_count": len(legacy_redirects),
        "translation_target_count": len(translation_owner_ids),
        "translation_complete_count": len(complete_translation_owner_ids),
        "routes": rendered_routes,
        "legacy_redirects": legacy_redirects,
    }
    return json.dumps(output, indent=2, ensure_ascii=True) + "\n"


def _is_public_human_current(record: dict[str, object]) -> bool:
    return (
        record["visibility"] == "public"
        and record["state"] == "current"
        and record["human"] is True
    )


def _is_generated_detail(record: dict[str, object]) -> bool:
    path = str(record["path"])
    generated_path = path.startswith("Docs/generated/")
    migrated_generated_path = (
        path.startswith("Docs/en/reference/")
        and str(record["id"]).startswith("generated-")
    )
    return (generated_path or migrated_generated_path) and not path.endswith(
        "/index.md"
    )


def _markdown_body(text: str) -> str:
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    if not normalized.startswith("---\n"):
        return normalized
    end = normalized.find("\n---\n", 4)
    return normalized[end + 5 :] if end >= 0 else normalized


def _current_translations(
    root: Path,
    localization_status: dict[str, object],
    records: list[dict[str, object]],
) -> dict[str, dict[str, object]]:
    records_by_id = {str(record["id"]): record for record in records}
    translations: dict[str, dict[str, object]] = {}
    raw_documents = localization_status.get("documents")
    if not isinstance(raw_documents, list):
        raise ValueError("localization status documents must be an array")
    for raw_document in raw_documents:
        if not isinstance(raw_document, dict) or raw_document.get("status") != "current":
            continue
        document_id = str(raw_document.get("id", ""))
        canonical = records_by_id.get(document_id)
        if canonical is None:
            raise ValueError(
                f"current translation has no canonical document: {document_id}"
            )
        russian_path = str(raw_document.get("russian_path", ""))
        russian_file = root / russian_path
        if not russian_file.is_file():
            raise ValueError(
                f"current translation source is missing: {russian_path}"
            )
        content = russian_file.read_text(encoding="utf-8")
        headings = _headings(_markdown_body(content))
        title = next(
            (str(heading["title"]) for heading in headings if heading["level"] == 1),
            str(raw_document.get("title", canonical["title"])),
        )
        english_path = str(raw_document.get("english_path", ""))
        if not (root / english_path).is_file():
            english_path = str(raw_document.get("source_path", canonical["path"]))
        translations[document_id] = {
            "id": document_id,
            "title": title,
            "content": content,
            "path": russian_path,
            "english_path": english_path,
            "russian_path": russian_path,
            "diataxis": canonical["diataxis"],
        }
    return translations


def _navigation_groups(
    config: dict[str, object],
    records: list[dict[str, object]],
    translations: dict[str, dict[str, object]],
) -> tuple[list[dict[str, object]], dict[str, tuple[str, str]]]:
    eligible = {
        str(record["id"]): record
        for record in records
        if _is_public_human_current(record) and not _is_generated_detail(record)
    }
    groups = config["navigation"]
    assert isinstance(groups, list)
    seen_document_ids: set[str] = set()
    rendered_groups: list[dict[str, object]] = []
    document_groups: dict[str, tuple[str, str]] = {}

    for group_value in groups:
        assert isinstance(group_value, dict)
        group_id = str(group_value["id"])
        group_title = str(group_value["title"])
        items = []
        for document_id_value in group_value["document_ids"]:
            document_id = str(document_id_value)
            if document_id in seen_document_ids:
                raise ValueError(f"site_delivery repeats navigation document id: {document_id}")
            if document_id not in eligible:
                raise ValueError(
                    "site_delivery navigation document is not public current human top-level documentation: "
                    + document_id
                )
            seen_document_ids.add(document_id)
            record = eligible[document_id]
            item = {
                "id": document_id,
                "title": record["title"],
                "path": record["path"],
                "url": _site_path(str(record["path"])),
                "diataxis": record["diataxis"],
            }
            translation = translations.get(document_id)
            if translation is not None:
                item["locales"] = {
                    "en": {
                        "title": record["title"],
                        "path": translation["english_path"],
                        "url": _site_path(str(translation["english_path"])),
                    },
                    "ru": {
                        "title": translation["title"],
                        "path": translation["russian_path"],
                        "url": _site_path(str(translation["russian_path"])),
                    },
                }
            items.append(item)
            document_groups[document_id] = (group_id, group_title)
        rendered_groups.append(
            {
                "id": group_id,
                "title": group_title,
                "titles": {
                    "en": group_title,
                    "ru": str(group_value["title_ru"]),
                },
                "items": items,
            }
        )

    missing_ids = sorted(set(eligible) - seen_document_ids)
    if missing_ids:
        raise ValueError(
            "site_delivery navigation omits public current human top-level documents: "
            + ", ".join(missing_ids)
        )
    return rendered_groups, document_groups


def _heading_slug(title: str) -> str:
    title = LINK_RE.sub(r"\1", title)
    title = HTML_RE.sub("", title)
    title = title.replace("`", "").replace("*", "")
    title = title.strip().lower()
    title = re.sub(r"[^\w\- ]", "", title, flags=re.UNICODE)
    return re.sub(r"\s+", "-", title)


def _headings(text: str) -> list[dict[str, object]]:
    headings: list[dict[str, object]] = []
    slug_counts: dict[str, int] = {}
    active_fence: str | None = None
    for line in text.splitlines():
        fence = FENCE_RE.match(line)
        if fence:
            marker = fence.group(1)[0]
            active_fence = marker if active_fence is None else None if active_fence == marker else active_fence
            continue
        if active_fence is not None:
            continue
        match = HEADING_RE.match(line)
        if not match:
            continue
        title = _plain_text(match.group("title"))
        if not title:
            continue
        base_slug = _heading_slug(match.group("title"))
        duplicate_index = slug_counts.get(base_slug, 0)
        slug_counts[base_slug] = duplicate_index + 1
        headings.append(
            {
                "title": title,
                "level": len(match.group("marks")),
                "anchor": base_slug if duplicate_index == 0 else f"{base_slug}-{duplicate_index}",
            }
        )
    return headings


def _plain_text(value: str) -> str:
    value = LINK_RE.sub(r"\1", value)
    value = HTML_RE.sub(" ", value)
    value = html.unescape(value)
    value = value.replace("`", "").replace("*", "").replace("_", " ")
    value = re.sub(r"\s+", " ", value)
    return value.strip(" #>|-")


def _summary(text: str, max_characters: int = 240) -> str:
    paragraph: list[str] = []
    active_fence: str | None = None
    for line in text.splitlines():
        fence = FENCE_RE.match(line)
        if fence:
            marker = fence.group(1)[0]
            active_fence = marker if active_fence is None else None if active_fence == marker else active_fence
            continue
        if active_fence is not None:
            continue
        stripped = line.strip()
        if not stripped:
            if paragraph:
                break
            continue
        if (
            HEADING_RE.match(line)
            or stripped.startswith((">", "- ", "* ", "+ ", "|", "![", "[!["))
            or re.match(r"\d+\.\s", stripped)
        ):
            continue
        plain = _plain_text(stripped)
        if plain:
            paragraph.append(plain)
    summary = " ".join(paragraph)
    if len(summary) <= max_characters:
        return summary
    shortened = summary[: max_characters - 1].rsplit(" ", maxsplit=1)[0]
    return shortened.rstrip(".,;:") + "..."


def _token_variants(value: str) -> list[str]:
    variants: list[str] = []
    for match in TOKEN_RE.finditer(value):
        raw = match.group(0)
        normalized = raw.casefold().strip("./:-?<>+")
        if (
            2 <= len(normalized) <= 64
            and normalized not in STOP_WORDS
            and not normalized.isdigit()
            and "/" not in raw
        ):
            variants.append(normalized)
        for component in re.split(r"[_.:+/?<>-]+", raw):
            for part in CAMEL_PART_RE.findall(component):
                normalized_part = part.casefold()
                if (
                    len(normalized_part) >= 2
                    and normalized_part not in STOP_WORDS
                    and not normalized_part.isdigit()
                ):
                    variants.append(normalized_part)
    return variants


def _search_scores(record: dict[str, object], headings: list[dict[str, object]]) -> Counter[str]:
    scores: Counter[str] = Counter()
    searchable_content = URL_RE.sub(" ", str(record["content"]))
    searchable_content = MARKDOWN_TARGET_RE.sub("]", searchable_content)
    body_counts = Counter(_token_variants(searchable_content))
    for token, count in body_counts.items():
        scores[token] += min(count, 4)
    for token in _token_variants(str(record["id"]) + " " + str(record["path"])):
        scores[token] += 6
    for heading in headings:
        for token in _token_variants(str(heading["title"])):
            scores[token] += 7
    for token in _token_variants(str(record["title"])):
        scores[token] += 14
    return scores


def search_documents(
    index: dict[str, object],
    query: str,
    *,
    limit: int = 12,
) -> list[dict[str, object]]:
    raw_tokens = QUERY_TOKEN_RE.findall(query.casefold())
    tokens: list[str] = []
    for raw in raw_tokens:
        token = raw.strip("./:+?<>-")
        if len(token) >= 2 and token not in tokens:
            tokens.append(token)
    if not tokens:
        return []

    documents = index.get("documents")
    terms = index.get("terms")
    if not isinstance(documents, list) or not isinstance(terms, dict):
        raise ValueError("documentation search index must contain documents and terms")

    scores: dict[int, float] = {}
    matches: Counter[int] = Counter()
    all_terms = sorted(str(term) for term in terms)

    def collect_postings(
        token_scores: dict[int, float],
        postings: object,
        multiplier: float,
    ) -> None:
        if not isinstance(postings, list):
            raise ValueError("documentation search postings must be arrays")
        for posting in postings:
            if (
                not isinstance(posting, list)
                or len(posting) != 2
                or not isinstance(posting[0], int)
                or not isinstance(posting[1], (int, float))
                or not 0 <= posting[0] < len(documents)
            ):
                raise ValueError("documentation search posting is malformed")
            document_index = posting[0]
            token_scores[document_index] = max(
                token_scores.get(document_index, 0.0),
                posting[1] * multiplier,
            )

    effective_token_count = 0
    for token in tokens:
        token_scores: dict[int, float] = {}
        exact = terms.get(token)
        if exact is not None:
            collect_postings(token_scores, exact, 1.0)
        elif len(token) >= 3:
            prefix_matches = 0
            for term in all_terms:
                if term.startswith(token):
                    collect_postings(token_scores, terms[term], 0.55)
                    prefix_matches += 1
                    if prefix_matches == 32:
                        break
        if not token_scores:
            continue
        effective_token_count += 1
        for document_index, token_score in token_scores.items():
            scores[document_index] = scores.get(document_index, 0.0) + token_score
            matches[document_index] += 1

    normalized_query = query.strip().casefold()
    results: list[dict[str, object]] = []
    minimum_matches = max(1, (effective_token_count * 3 + 4) // 5)
    for document_index, score in scores.items():
        document = documents[document_index]
        if not isinstance(document, dict):
            raise ValueError("documentation search document is malformed")
        title = str(document.get("title", ""))
        document_id = str(document.get("id", ""))
        if normalized_query in title.casefold():
            score += 80
        if document_id.casefold() == normalized_query:
            score += 120
        if matches[document_index] >= minimum_matches:
            results.append(
                {
                    "document": document,
                    "score": score,
                    "matches": matches[document_index],
                }
            )

    results.sort(
        key=lambda result: (
            -int(result["matches"]),
            -float(result["score"]),
            str(result["document"].get("title", "")).casefold(),
        )
    )
    return results[:limit]


def render_navigation(
    manifest: dict[str, object],
    config: dict[str, object],
    records: list[dict[str, object]],
    translations: dict[str, dict[str, object]],
) -> tuple[str, dict[str, tuple[str, str]]]:
    publishing = _require_object(manifest, "publishing", "documentation manifest publishing")
    versioning = docs_ai_delivery._versioning_config(manifest)
    localization = docs_ai_delivery._localization_config(manifest)
    current_version = _require_object(
        versioning,
        "current",
        "documentation versioning current",
    )
    routing = _require_object(config, "routing", "site_delivery routing")
    groups, document_groups = _navigation_groups(config, records, translations)
    records_by_id = {str(record["id"]): record for record in records}
    locale_pairs = []
    for document_id, translation in sorted(translations.items()):
        canonical = records_by_id[document_id]
        locale_pairs.append(
            {
                "document_id": document_id,
                "en": {
                    "title": canonical["title"],
                    "path": translation["english_path"],
                    "url": _site_path(str(translation["english_path"])),
                },
                "ru": {
                    "title": translation["title"],
                    "path": translation["russian_path"],
                    "url": _site_path(str(translation["russian_path"])),
                },
            }
        )
    output = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "title": _require_string(publishing, "title", "documentation publishing title"),
        "description": _require_string(
            publishing,
            "site_description",
            "documentation publishing site_description",
        ),
        "canonical_base_url": _require_string(
            publishing,
            "production_url",
            "documentation publishing production_url",
        ),
        "repository": _require_string(
            publishing,
            "repository",
            "documentation publishing repository",
        ),
        "source_ref": current_version["source_ref"],
        "version": {
            "channel": current_version["channel"],
            "kind": current_version["kind"],
            "label": current_version["label"],
            "value": current_version["source_ref"],
            "source_ref": current_version["source_ref"],
            "path_prefix": current_version["path_prefix"],
            "support": current_version["support"],
        },
        "canonical_locale": localization["canonical_locale"],
        "locales": [dict(locale) for locale in localization["locales"]],
        "locale_pair_count": len(locale_pairs),
        "locale_pairs": locale_pairs,
        "routes_path": routing["path"],
        "navigation_item_count": sum(len(group["items"]) for group in groups),
        "navigation": groups,
    }
    return json.dumps(output, indent=2, ensure_ascii=True) + "\n", document_groups


def render_search(
    manifest: dict[str, object],
    config: dict[str, object],
    records: list[dict[str, object]],
    document_groups: dict[str, tuple[str, str]],
    translations: dict[str, dict[str, object]],
    locale: str,
) -> str:
    publishing = _require_object(manifest, "publishing", "documentation manifest publishing")
    if locale == "en":
        eligible = sorted(
            (record for record in records if _is_public_human_current(record)),
            key=lambda record: (str(record["title"]).casefold(), str(record["id"])),
        )
    elif locale == "ru":
        eligible = sorted(
            translations.values(),
            key=lambda record: (str(record["title"]).casefold(), str(record["id"])),
        )
    else:
        raise ValueError(f"unsupported documentation search locale: {locale}")
    group_titles = {
        str(group["id"]): str(
            group["title_ru"] if locale == "ru" else group["title"]
        )
        for group in config["navigation"]
        if isinstance(group, dict)
    }
    documents = []
    postings: dict[str, list[list[int]]] = defaultdict(list)
    for document_index, record in enumerate(eligible):
        content = _markdown_body(str(record["content"]))
        document_headings = _headings(content)
        group_id, group_title = document_groups.get(
            str(record["id"]),
            (str(record["diataxis"]), str(record["diataxis"]).replace("-", " ").title()),
        )
        group_title = group_titles.get(group_id, group_title)
        diataxis = str(record["diataxis"])
        documents.append(
            {
                "id": record["id"],
                "document_id": record["id"],
                "locale": locale,
                "title": record["title"],
                "url": _site_path(str(record["path"])),
                "path": record["path"],
                "section_id": group_id,
                "section_title": group_title,
                "diataxis": diataxis,
                "diataxis_title": DIATAXIS_TITLES[locale].get(
                    diataxis, diataxis.replace("-", " ").title()
                ),
                "summary": _summary(content),
            }
        )
        searchable_record = dict(record)
        searchable_record["content"] = content
        for token, score in sorted(
            _search_scores(searchable_record, document_headings).items()
        ):
            postings[token].append([document_index, score])

    search = _require_object(config, "search", "site_delivery search")
    postings = {
        token: token_postings
        for token, token_postings in postings.items()
        if len(documents) <= 2
        or len(token_postings) / len(documents) <= MAX_DOCUMENT_FREQUENCY_RATIO
    }
    output = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "canonical_base_url": _require_string(
            publishing,
            "production_url",
            "documentation publishing production_url",
        ),
        "locale": locale,
        "minimum_query_length": search["minimum_query_length"],
        "maximum_document_frequency_ratio": MAX_DOCUMENT_FREQUENCY_RATIO,
        "document_count": len(documents),
        "documents": documents,
        "terms": {token: postings[token] for token in sorted(postings)},
    }
    rendered = json.dumps(output, ensure_ascii=False, separators=(",", ":")) + "\n"
    max_bytes = int(search["max_bytes"])
    byte_size = len(rendered.encode("utf-8"))
    if byte_size > max_bytes:
        raise ValueError(f"generated documentation search index is {byte_size} bytes; limit is {max_bytes}")
    return rendered


def render_outputs(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    manifest = docs_ai_delivery._load_manifest(root, manifest_relative_path)
    config = _site_config(manifest)
    records = docs_ai_delivery._document_records(root, manifest)
    localization = docs_ai_delivery._localization_config(manifest)
    localization_status = docs_localization.generate_localization_status(
        root,
        manifest_relative_path,
        str(localization["glossary"]),
    )
    translations = _current_translations(root, localization_status, records)
    navigation, document_groups = render_navigation(
        manifest, config, records, translations
    )
    english_search = render_search(
        manifest, config, records, document_groups, translations, "en"
    )
    russian_search = render_search(
        manifest, config, records, document_groups, translations, "ru"
    )
    routes = render_routes(root, manifest, config, records)
    return {
        DEFAULT_NAVIGATION_OUTPUT: navigation,
        DEFAULT_SEARCH_OUTPUT: english_search,
        DEFAULT_RUSSIAN_SEARCH_OUTPUT: russian_search,
        DEFAULT_ROUTES_OUTPUT: routes,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate FOnline documentation site navigation and search data")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write generated documentation site data")
    mode.add_argument("--check", action="store_true", help="fail when generated site data is stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        outputs = render_outputs(root, args.manifest)
    except ValueError as exception:
        print(f"Unable to generate documentation site data: {exception}", file=sys.stderr)
        return 1

    if args.write:
        for relative_path, content in outputs.items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
            print(f"Wrote {relative_path}")
        return 0

    stale = []
    for relative_path, content in outputs.items():
        output_path = root / relative_path
        if not output_path.is_file() or output_path.read_text(encoding="utf-8") != content:
            stale.append(relative_path)
    if stale:
        print(
            "Generated documentation site data is missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_site.py --write",
            file=sys.stderr,
        )
        return 1

    navigation = json.loads(outputs[DEFAULT_NAVIGATION_OUTPUT])
    english_search = json.loads(outputs[DEFAULT_SEARCH_OUTPUT])
    russian_search = json.loads(outputs[DEFAULT_RUSSIAN_SEARCH_OUTPUT])
    routes = json.loads(outputs[DEFAULT_ROUTES_OUTPUT])
    english_search_size = len(outputs[DEFAULT_SEARCH_OUTPUT].encode("utf-8"))
    russian_search_size = len(
        outputs[DEFAULT_RUSSIAN_SEARCH_OUTPUT].encode("utf-8")
    )
    print(
        "Documentation site data is current: "
        f"{navigation['navigation_item_count']} navigation items, "
        f"{english_search['document_count']} English and "
        f"{russian_search['document_count']} Russian searchable documents, "
        f"{routes['route_count']} public routes, {routes['redirect_count']} planned redirects, "
        f"{english_search_size}/{russian_search_size} search bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
