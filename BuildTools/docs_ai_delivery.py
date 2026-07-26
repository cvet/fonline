from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path, PurePosixPath
from urllib.parse import quote


SCHEMA_VERSION = 1
VERSIONING_SCHEMA_VERSION = 1
LOCALIZATION_SCHEMA_VERSION = 1
GENERATED_BY = "BuildTools/docs_ai_delivery.py"
DEFAULT_MANIFEST = "Docs/documentation-manifest.json"
DEFAULT_LLMS_OUTPUT = "llms.txt"
DEFAULT_FULL_CONTEXT_OUTPUT = "llms-full.txt"
DEFAULT_PUBLIC_MANIFEST_OUTPUT = "docs-manifest.json"
OUTPUT_PATHS = (
    DEFAULT_LLMS_OUTPUT,
    DEFAULT_FULL_CONTEXT_OUTPUT,
    DEFAULT_PUBLIC_MANIFEST_OUTPUT,
)

SECTION_TITLES = {
    "tutorial": "Tutorials",
    "how-to": "How-to guides",
    "reference": "Reference",
    "explanation": "Explanations",
    "none": "Entry points",
}
SECTION_ORDER = ("tutorial", "how-to", "reference", "explanation", "none")


def _load_manifest(root: Path, manifest_relative_path: str) -> dict[str, object]:
    manifest_path = root / manifest_relative_path
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise ValueError(f"Unable to read documentation manifest: {exception}") from exception
    if not isinstance(manifest, dict):
        raise ValueError("Documentation manifest root must be an object")
    return manifest


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


def _validate_output_path(value: object, expected: str, label: str) -> str:
    if value != expected:
        raise ValueError(f"{label} must be {expected}")
    return expected


def _versioning_config(manifest: dict[str, object]) -> dict[str, object]:
    versioning = _require_object(manifest, "versioning", "documentation manifest versioning")
    if versioning.get("schema_version") != VERSIONING_SCHEMA_VERSION:
        raise ValueError(
            "documentation manifest versioning schema_version must be "
            f"{VERSIONING_SCHEMA_VERSION}"
        )

    current = _require_object(versioning, "current", "documentation versioning current")
    expected_current = {
        "channel": "current",
        "kind": "rolling-branch",
        "path_prefix": "",
        "support": "latest-development-revision",
    }
    for key, expected in expected_current.items():
        if current.get(key) != expected:
            raise ValueError(f"documentation versioning current {key} must be {expected}")
    _require_string(current, "label", "documentation versioning current label")
    _require_string(current, "source_ref", "documentation versioning current source_ref")

    releases = _require_object(versioning, "releases", "documentation versioning releases")
    expected_releases = {
        "status": "deferred",
        "source": "git-tag",
        "path_template": "/versions/{version}/",
        "requires_support_policy": True,
    }
    for key, expected in expected_releases.items():
        if releases.get(key) != expected:
            raise ValueError(f"documentation versioning releases {key} must be {expected}")

    history = _require_object(versioning, "history", "documentation versioning history")
    if history.get("mode") != "commit-addressable-ci-artifacts":
        raise ValueError(
            "documentation versioning history mode must be commit-addressable-ci-artifacts"
        )
    return versioning


def _localization_config(manifest: dict[str, object]) -> dict[str, object]:
    localization = _require_object(
        manifest,
        "localization",
        "documentation manifest localization",
    )
    if localization.get("schema_version") != LOCALIZATION_SCHEMA_VERSION:
        raise ValueError(
            "documentation manifest localization schema_version must be "
            f"{LOCALIZATION_SCHEMA_VERSION}"
        )
    if localization.get("canonical_locale") != "en":
        raise ValueError("documentation localization canonical_locale must be en")
    if localization.get("path_strategy") != "mirrored-relative-path":
        raise ValueError(
            "documentation localization path_strategy must be mirrored-relative-path"
        )
    if localization.get("translation_hash") != "normalized-sha256":
        raise ValueError(
            "documentation localization translation_hash must be normalized-sha256"
        )
    if localization.get("translation_pending") != "pre-production-only":
        raise ValueError(
            "documentation localization translation_pending must be pre-production-only"
        )

    locales = localization.get("locales")
    if not isinstance(locales, list) or len(locales) != 2:
        raise ValueError("documentation localization locales must contain en and ru")
    expected_locales = (
        ("en", "English", "Docs/en", "canonical-source-pending-migration"),
        ("ru", "Russian", "Docs/ru", "planned"),
    )
    for index, (locale_id, label, path_prefix, status) in enumerate(expected_locales):
        locale = locales[index]
        if not isinstance(locale, dict):
            raise ValueError(f"documentation localization locale {index} must be an object")
        expected = {
            "id": locale_id,
            "label": label,
            "path_prefix": path_prefix,
            "status": status,
        }
        for key, expected_value in expected.items():
            if locale.get(key) != expected_value:
                raise ValueError(
                    f"documentation localization locale {index} {key} must be {expected_value}"
                )

    entrypoint_targets = localization.get("entrypoint_targets")
    if not isinstance(entrypoint_targets, dict):
        raise ValueError("documentation localization entrypoint_targets must be an object")
    for document_id, targets in entrypoint_targets.items():
        if not isinstance(document_id, str) or not document_id:
            raise ValueError(
                "documentation localization entrypoint_targets keys must be document IDs"
            )
        if not isinstance(targets, dict):
            raise ValueError(
                f"documentation localization entrypoint target {document_id} must be an object"
            )
        for locale_id in ("en", "ru"):
            target = targets.get(locale_id)
            if not isinstance(target, str) or not target:
                raise ValueError(
                    f"documentation localization entrypoint target {document_id} "
                    f"must name {locale_id}"
                )
            path = PurePosixPath(target)
            if path.is_absolute() or ".." in path.parts or path.suffix.lower() != ".md":
                raise ValueError(
                    f"documentation localization entrypoint target {document_id} "
                    f"{locale_id} must be a repository Markdown path"
                )
    return localization


def _delivery_config(manifest: dict[str, object]) -> dict[str, object]:
    delivery = _require_object(manifest, "ai_delivery", "documentation manifest ai_delivery")
    if delivery.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"documentation manifest ai_delivery schema_version must be {SCHEMA_VERSION}")

    versioning = _versioning_config(manifest)
    localization = _localization_config(manifest)
    current_version = _require_object(
        versioning,
        "current",
        "documentation versioning current",
    )
    canonical_locale = _require_string(delivery, "canonical_locale", "ai_delivery canonical_locale")
    if canonical_locale != localization["canonical_locale"]:
        raise ValueError(
            "ai_delivery canonical_locale must match documentation localization"
        )
    source_ref = _require_string(delivery, "source_ref", "ai_delivery source_ref")
    if source_ref != current_version["source_ref"]:
        raise ValueError("ai_delivery source_ref must match documentation versioning current")

    llms = _require_object(delivery, "llms", "ai_delivery llms")
    _validate_output_path(llms.get("path"), DEFAULT_LLMS_OUTPUT, "ai_delivery llms path")
    start_document_ids = llms.get("start_document_ids")
    if (
        not isinstance(start_document_ids, list)
        or any(not isinstance(document_id, str) or not document_id for document_id in start_document_ids)
        or len(start_document_ids) != len(set(start_document_ids))
    ):
        raise ValueError("ai_delivery llms start_document_ids must be a unique string array")

    full_context = _require_object(delivery, "full_context", "ai_delivery full_context")
    _validate_output_path(
        full_context.get("path"),
        DEFAULT_FULL_CONTEXT_OUTPUT,
        "ai_delivery full_context path",
    )
    max_bytes = full_context.get("max_bytes")
    if not isinstance(max_bytes, int) or isinstance(max_bytes, bool) or max_bytes <= 0:
        raise ValueError("ai_delivery full_context max_bytes must be a positive integer")
    if full_context.get("generated_pages") != "indexes-only":
        raise ValueError("ai_delivery full_context generated_pages must be indexes-only")

    public_manifest = _require_object(delivery, "public_manifest", "ai_delivery public_manifest")
    _validate_output_path(
        public_manifest.get("path"),
        DEFAULT_PUBLIC_MANIFEST_OUTPUT,
        "ai_delivery public_manifest path",
    )
    return delivery


def _normalize_text(path: Path) -> str:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exception:
        raise ValueError(f"Unable to read documentation source {path.as_posix()}: {exception}") from exception
    return text.replace("\r\n", "\n").replace("\r", "\n")


def _sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _canonical_url(base_url: str, source_path: str) -> str:
    if source_path == "README.md":
        return base_url.rstrip("/") + "/"
    site_path = str(PurePosixPath(source_path).with_suffix(".html"))
    return base_url.rstrip("/") + "/" + quote(site_path, safe="/")


def _repository_urls(repository: str, source_ref: str, source_path: str) -> tuple[str, str]:
    encoded_repository = quote(repository, safe="/")
    encoded_ref = quote(source_ref, safe="")
    encoded_path = quote(source_path, safe="/")
    source_url = f"https://github.com/{encoded_repository}/blob/{encoded_ref}/{encoded_path}"
    raw_url = f"https://raw.githubusercontent.com/{encoded_repository}/{encoded_ref}/{encoded_path}"
    return source_url, raw_url


def _document_records(root: Path, manifest: dict[str, object]) -> list[dict[str, object]]:
    publishing = _require_object(manifest, "publishing", "documentation manifest publishing")
    base_url = _require_string(publishing, "production_url", "documentation publishing production_url")
    repository = _require_string(publishing, "repository", "documentation publishing repository")
    delivery = _delivery_config(manifest)
    source_ref = _require_string(delivery, "source_ref", "ai_delivery source_ref")
    canonical_locale = _require_string(delivery, "canonical_locale", "ai_delivery canonical_locale")
    documents = _require_object(manifest, "documents", "documentation manifest documents")

    records: list[dict[str, object]] = []
    seen_ids: set[str] = set()
    for source_path, document_value in documents.items():
        if not isinstance(source_path, str) or not isinstance(document_value, dict):
            raise ValueError("documentation manifest documents must map paths to objects")
        document_id = _require_string(document_value, "id", f"document {source_path} id")
        if document_id in seen_ids:
            raise ValueError(f"documentation manifest repeats document id: {document_id}")
        seen_ids.add(document_id)

        title = _require_string(document_value, "title", f"document {source_path} title")
        classification = _require_object(
            document_value,
            "classification",
            f"document {source_path} classification",
        )
        audiences = document_value.get("audiences")
        sources = document_value.get("sources")
        if not isinstance(audiences, list) or any(not isinstance(value, str) for value in audiences):
            raise ValueError(f"document {source_path} audiences must be a string array")
        if not isinstance(sources, list) or any(not isinstance(value, str) for value in sources):
            raise ValueError(f"document {source_path} sources must be a string array")

        path = root / source_path
        if not path.is_file():
            raise ValueError(f"documentation source is missing: {source_path}")
        text = _normalize_text(path)
        source_url, raw_url = _repository_urls(repository, source_ref, source_path)
        state = _require_string(document_value, "state", f"document {source_path} state")
        visibility = _require_string(
            classification,
            "visibility",
            f"document {source_path} visibility",
        )
        diataxis = _require_string(
            classification,
            "diataxis",
            f"document {source_path} Diataxis kind",
        )
        record = {
            "id": document_id,
            "path": source_path,
            "title": title,
            "locale": canonical_locale,
            "audiences": list(audiences),
            "diataxis": diataxis,
            "visibility": visibility,
            "human": classification.get("human"),
            "translation": classification.get("translation"),
            "owner": document_value.get("owner"),
            "state": state,
            "disposition": document_value.get("disposition"),
            "target": document_value.get("target"),
            "stability": "current-revision" if state == "current" else state,
            "canonical_url": _canonical_url(base_url, source_path),
            "source_url": source_url,
            "raw_url": raw_url,
            "source_paths": list(sources),
            "content_sha256": _sha256_text(text),
            "byte_size": len(text.encode("utf-8")),
            "content": text,
        }
        if "redirect_to" in document_value:
            record["redirect_to"] = document_value["redirect_to"]
        records.append(record)
    return records


def _public_current(records: list[dict[str, object]]) -> list[dict[str, object]]:
    return [record for record in records if record["visibility"] == "public" and record["state"] == "current"]


def _record_by_id(records: list[dict[str, object]]) -> dict[str, dict[str, object]]:
    return {str(record["id"]): record for record in records}


def _link_line(record: dict[str, object]) -> str:
    kind = str(record["diataxis"])
    kind_label = "entry point" if kind == "none" else kind
    audiences = ", ".join(str(value) for value in record["audiences"])
    return f"- [{record['title']}]({record['canonical_url']}): `{record['id']}`; {kind_label}; {audiences}."


def _machine_model_paths(manifest: dict[str, object]) -> list[tuple[str, str]]:
    generated_artifacts = _require_object(
        manifest,
        "generated_artifacts",
        "documentation manifest generated_artifacts",
    )
    paths: dict[str, str] = {}
    for artifact_id, artifact_value in generated_artifacts.items():
        if not isinstance(artifact_value, dict):
            continue
        for field in ("path", "model"):
            value = artifact_value.get(field)
            if isinstance(value, str) and value.endswith(".json"):
                paths[value] = artifact_id
        if artifact_id == "site_delivery":
            site_paths = artifact_value.get("paths")
            if isinstance(site_paths, list):
                for value in site_paths:
                    if not isinstance(value, str) or not value.endswith(".json"):
                        continue
                    name = PurePosixPath(value).stem
                    paths[value] = f"site-{name}"
    return [(paths[path], path) for path in sorted(paths)]


def render_llms(root: Path, manifest: dict[str, object], records: list[dict[str, object]]) -> str:
    publishing = _require_object(manifest, "publishing", "documentation manifest publishing")
    base_url = _require_string(publishing, "production_url", "documentation publishing production_url")
    delivery = _delivery_config(manifest)
    versioning = _versioning_config(manifest)
    current_version = _require_object(
        versioning,
        "current",
        "documentation versioning current",
    )
    llms = _require_object(delivery, "llms", "ai_delivery llms")
    start_ids = list(llms["start_document_ids"])

    eligible = _public_current(records)
    eligible_by_id = _record_by_id(eligible)
    if eligible and not start_ids:
        raise ValueError("ai_delivery llms start_document_ids must not be empty when public current documents exist")
    unknown_ids = [document_id for document_id in start_ids if document_id not in eligible_by_id]
    if unknown_ids:
        raise ValueError("ai_delivery llms start_document_ids are not public current documents: " + ", ".join(unknown_ids))

    lines = [
        "# FOnline Engine",
        "",
        "> Standalone FOnline engine documentation for game developers, engine contributors, tool authors, and AI agents.",
        "",
        f"Canonical site: {base_url}",
        f"Current documentation: `{current_version['source_ref']}` rolling branch.",
        f"Machine-readable document index: {base_url.rstrip('/')}/{DEFAULT_PUBLIC_MANIFEST_OUTPUT}",
        f"Bounded full-context bundle: {base_url.rstrip('/')}/{DEFAULT_FULL_CONTEXT_OUTPUT}",
        "",
        "## Start here",
        "",
    ]
    for document_id in start_ids:
        lines.append(_link_line(eligible_by_id[document_id]))

    consumed = set(start_ids)
    for section in SECTION_ORDER:
        section_records = sorted(
            (
                record
                for record in eligible
                if record["id"] not in consumed and record["diataxis"] == section
            ),
            key=lambda record: (str(record["title"]).casefold(), str(record["id"])),
        )
        if not section_records:
            continue
        lines.extend(["", f"## {SECTION_TITLES[section]}", ""])
        lines.extend(_link_line(record) for record in section_records)

    lines.extend(["", "## Machine-readable references", ""])
    for artifact_id, path in _machine_model_paths(manifest):
        url = base_url.rstrip("/") + "/" + quote(path, safe="/")
        lines.append(f"- [{artifact_id}]({url}): canonical generated JSON at `{path}`.")
    lines.append(
        f"- [public documentation manifest]({base_url.rstrip('/')}/{DEFAULT_PUBLIC_MANIFEST_OUTPUT}): "
        "public paths, stable IDs, provenance, URLs, and content hashes."
    )
    return "\n".join(lines) + "\n"


def _is_generated_detail(record: dict[str, object]) -> bool:
    path = str(record["path"])
    return path.startswith("Docs/generated/") and not path.endswith("/index.md")


def render_full_context(
    manifest: dict[str, object],
    records: list[dict[str, object]],
) -> str:
    publishing = _require_object(manifest, "publishing", "documentation manifest publishing")
    base_url = _require_string(publishing, "production_url", "documentation publishing production_url")
    delivery = _delivery_config(manifest)
    versioning = _versioning_config(manifest)
    current_version = _require_object(
        versioning,
        "current",
        "documentation versioning current",
    )
    llms = _require_object(delivery, "llms", "ai_delivery llms")
    full_context = _require_object(delivery, "full_context", "ai_delivery full_context")
    start_ids = list(llms["start_document_ids"])

    selected = [record for record in _public_current(records) if not _is_generated_detail(record)]
    selected_by_id = _record_by_id(selected)
    ordered = [selected_by_id[document_id] for document_id in start_ids if document_id in selected_by_id]
    consumed = {str(record["id"]) for record in ordered}
    ordered.extend(sorted((record for record in selected if record["id"] not in consumed), key=lambda record: str(record["path"])))

    lines = [
        "# FOnline Engine bounded documentation context",
        "",
        "This generated bundle contains public current Markdown owned by the standalone engine.",
        "Generated reference detail pages are represented by their indexes; complete JSON models remain linked from llms.txt.",
        f"Canonical site: {base_url}",
        f"Documentation version: {current_version['source_ref']} ({current_version['kind']})",
        f"Documents: {len(ordered)}",
        "",
    ]
    for record in ordered:
        lines.extend(
            [
                f"===== BEGIN DOCUMENT {record['id']} =====",
                f"Source: {record['path']}",
                f"Canonical URL: {record['canonical_url']}",
                f"Content SHA-256: {record['content_sha256']}",
                "",
                str(record["content"]).rstrip("\n"),
                "",
                f"===== END DOCUMENT {record['id']} =====",
                "",
            ]
        )
    rendered = "\n".join(lines).rstrip("\n") + "\n"
    byte_size = len(rendered.encode("utf-8"))
    max_bytes = int(full_context["max_bytes"])
    if byte_size > max_bytes:
        raise ValueError(f"generated full-context bundle is {byte_size} bytes; limit is {max_bytes}")
    return rendered


def _artifact_record(artifact_id: str, path: str, content: str, base_url: str) -> dict[str, object]:
    return {
        "id": artifact_id,
        "path": path,
        "canonical_url": base_url.rstrip("/") + "/" + quote(path, safe="/"),
        "content_sha256": _sha256_text(content),
        "byte_size": len(content.encode("utf-8")),
    }


def render_public_manifest(
    root: Path,
    manifest: dict[str, object],
    records: list[dict[str, object]],
    llms_content: str,
    full_context_content: str,
) -> str:
    publishing = _require_object(manifest, "publishing", "documentation manifest publishing")
    base_url = _require_string(publishing, "production_url", "documentation publishing production_url")
    repository = _require_string(publishing, "repository", "documentation publishing repository")
    delivery = _delivery_config(manifest)
    versioning = _versioning_config(manifest)
    localization = _localization_config(manifest)
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
    source_ref = _require_string(current_version, "source_ref", "documentation versioning source_ref")
    canonical_locale = _require_string(
        localization,
        "canonical_locale",
        "documentation localization canonical_locale",
    )

    public_records = []
    for record in sorted(
        (record for record in records if record["visibility"] == "public"),
        key=lambda record: str(record["id"]),
    ):
        public_records.append({key: value for key, value in record.items() if key != "content"})

    artifacts = [
        _artifact_record("llms-index", DEFAULT_LLMS_OUTPUT, llms_content, base_url),
        _artifact_record(
            "llms-full-context",
            DEFAULT_FULL_CONTEXT_OUTPUT,
            full_context_content,
            base_url,
        ),
    ]
    for artifact_id, path in _machine_model_paths(manifest):
        artifact_path = root / path
        if not artifact_path.is_file():
            raise ValueError(f"declared machine-readable documentation artifact is missing: {path}")
        artifacts.append(_artifact_record(artifact_id, path, _normalize_text(artifact_path), base_url))

    public_manifest = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "canonical_locale": canonical_locale,
        "canonical_base_url": base_url,
        "repository": repository,
        "source_ref": source_ref,
        "version": {
            "channel": current_version["channel"],
            "kind": current_version["kind"],
            "label": current_version["label"],
            "value": source_ref,
            "source_ref": source_ref,
            "path_prefix": current_version["path_prefix"],
            "support": current_version["support"],
        },
        "release_versions": dict(releases),
        "localization": {
            "canonical_locale": canonical_locale,
            "path_strategy": localization["path_strategy"],
            "translation_hash": localization["translation_hash"],
            "translation_pending": localization["translation_pending"],
            "locales": [dict(locale) for locale in localization["locales"]],
        },
        "document_count": len(public_records),
        "documents": public_records,
        "artifacts": artifacts,
    }
    return json.dumps(public_manifest, indent=2, ensure_ascii=True) + "\n"


def render_outputs(
    root: Path,
    manifest_relative_path: str = DEFAULT_MANIFEST,
) -> dict[str, str]:
    manifest = _load_manifest(root, manifest_relative_path)
    records = _document_records(root, manifest)
    llms_content = render_llms(root, manifest, records)
    full_context_content = render_full_context(manifest, records)
    public_manifest_content = render_public_manifest(
        root,
        manifest,
        records,
        llms_content,
        full_context_content,
    )
    return {
        DEFAULT_LLMS_OUTPUT: llms_content,
        DEFAULT_FULL_CONTEXT_OUTPUT: full_context_content,
        DEFAULT_PUBLIC_MANIFEST_OUTPUT: public_manifest_content,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate AI-facing FOnline documentation artifacts")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="write generated AI-delivery artifacts")
    mode.add_argument("--check", action="store_true", help="fail when generated AI-delivery artifacts are stale")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        outputs = render_outputs(root, args.manifest)
    except ValueError as exception:
        print(f"Unable to generate AI documentation delivery: {exception}", file=sys.stderr)
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
            "Generated AI documentation artifacts are missing or stale: "
            + ", ".join(stale)
            + "; run python BuildTools/docs_ai_delivery.py --write",
            file=sys.stderr,
        )
        return 1

    full_context_size = len(outputs[DEFAULT_FULL_CONTEXT_OUTPUT].encode("utf-8"))
    public_manifest = json.loads(outputs[DEFAULT_PUBLIC_MANIFEST_OUTPUT])
    print(
        "AI documentation delivery is current: "
        f"{public_manifest['document_count']} public documents, "
        f"{full_context_size} full-context bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
