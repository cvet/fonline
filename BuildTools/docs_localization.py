from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "Docs/documentation-manifest.json"
DEFAULT_GLOSSARY = "Docs/translation-glossary.json"
DEFAULT_OUTPUT = "Docs/generated/translation-status.json"
GENERATED_BY = "BuildTools/docs_localization.py"
TRANSLATION_METADATA_RE = re.compile(
    r"^\s*<!--\s*docs-translation:\s*(?P<json>\{[^\r\n]+\})\s*-->\s*$",
    re.MULTILINE,
)
FENCE_RE = re.compile(
    r"^(?P<indent> {0,3})(?P<fence>`{3,}|~{3,})[^\r\n]*\r?\n"
    r"(?P<body>.*?)"
    r"^(?P=indent)(?P=fence)\s*$",
    re.MULTILINE | re.DOTALL,
)
LINK_RE = re.compile(r"(?<!!)\[[^\]]+]\((?P<target>[^)\r\n]+)\)")
VALID_GLOSSARY_POLICIES = {"preserve", "translate", "contextual"}


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _repository_path(value: object, label: str) -> str:
    text = _required_string(value, label)
    path = PurePosixPath(text)
    if path.is_absolute() or ".." in path.parts or "\\" in text:
        raise ValueError(f"{label} must be a repository-relative forward-slash path")
    return text


def _normalize_text(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def normalized_sha256(text: str) -> str:
    return hashlib.sha256(_normalize_text(text).encode("utf-8")).hexdigest()


def _load_manifest(root: Path, manifest_path: str) -> dict[str, object]:
    value = json.loads((root / manifest_path).read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("documentation manifest must be an object")
    return value


def _load_glossary(root: Path, glossary_path: str) -> dict[str, object]:
    value = json.loads((root / glossary_path).read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("translation glossary must be an object")
    if value.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"translation glossary schema_version must be {SCHEMA_VERSION}"
        )
    if value.get("source_locale") != "en" or value.get("target_locale") != "ru":
        raise ValueError("translation glossary must map en to ru")
    terms = value.get("terms")
    if not isinstance(terms, list) or not terms:
        raise ValueError("translation glossary terms must be a non-empty array")
    seen: set[str] = set()
    for index, entry in enumerate(terms):
        label = f"translation glossary terms[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{label} must be an object")
        term = _required_string(entry.get("term"), f"{label}.term")
        if term in seen:
            raise ValueError(f"duplicate translation glossary term: {term}")
        seen.add(term)
        _required_string(entry.get("russian"), f"{label}.russian")
        policy = _required_string(entry.get("policy"), f"{label}.policy")
        if policy not in VALID_GLOSSARY_POLICIES:
            raise ValueError(f"{label}.policy is unsupported: {policy}")
        _required_string(entry.get("note"), f"{label}.note")
    return value


def _translation_targets(
    document_id: str,
    document: dict[str, object],
    entrypoint_targets: dict[str, object],
) -> tuple[str, str]:
    explicit = entrypoint_targets.get(document_id)
    if explicit is not None:
        if not isinstance(explicit, dict):
            raise ValueError(
                f"localization entrypoint target {document_id} must be an object"
            )
        return (
            _repository_path(explicit.get("en"), f"{document_id} English target"),
            _repository_path(explicit.get("ru"), f"{document_id} Russian target"),
        )

    english = _repository_path(
        document.get("target"), f"document {document_id} target"
    )
    prefix = "Docs/en/"
    if not english.startswith(prefix):
        raise ValueError(
            f"translation-required document {document_id} must target {prefix} "
            "or declare explicit entrypoint targets"
        )
    return english, "Docs/ru/" + english.removeprefix(prefix)


def _translation_metadata(text: str, path: str) -> dict[str, object]:
    match = TRANSLATION_METADATA_RE.search(text)
    if match is None:
        raise ValueError(f"Russian translation is missing docs-translation metadata: {path}")
    try:
        value = json.loads(match.group("json"))
    except json.JSONDecodeError as exception:
        raise ValueError(
            f"Russian translation has invalid docs-translation metadata: {path}: "
            f"{exception}"
        ) from exception
    if not isinstance(value, dict):
        raise ValueError(f"Russian translation metadata must be an object: {path}")
    return value


def translation_metadata_line(
    document_id: str, source_path: str, source_sha256: str
) -> str:
    value = {
        "document_id": document_id,
        "locale": "ru",
        "source_path": source_path,
        "source_sha256": source_sha256,
    }
    return (
        "<!-- docs-translation: "
        + json.dumps(value, ensure_ascii=False, separators=(",", ":"))
        + " -->"
    )


def _fenced_bodies(text: str) -> list[str]:
    return [
        _normalize_text(match.group("body")).rstrip("\n")
        for match in FENCE_RE.finditer(_normalize_text(text))
    ]


def _resolve_link(path: str, target: str) -> str | None:
    target = target.strip()
    if target.startswith("<") and ">" in target:
        target = target[1 : target.index(">")]
    target = target.split(maxsplit=1)[0]
    parsed = urlsplit(target)
    if parsed.scheme or target.startswith("//") or not parsed.path:
        return None
    raw_path = unquote(parsed.path)
    if raw_path.startswith("/"):
        result = PurePosixPath(raw_path.lstrip("/"))
    else:
        result = PurePosixPath(path).parent / raw_path
    normalized: list[str] = []
    for part in result.parts:
        if part in {"", "."}:
            continue
        if part == "..":
            if normalized:
                normalized.pop()
            continue
        normalized.append(part)
    return PurePosixPath(*normalized).as_posix()


def _validate_language_preserving_links(
    root: Path,
    records: list[dict[str, object]],
) -> None:
    localized_lookup: dict[str, str] = {}
    for record in records:
        if record["status"] != "current":
            continue
        russian_path = str(record["russian_path"])
        localized_lookup[str(record["source_path"])] = russian_path
        localized_lookup[str(record["english_path"])] = russian_path

    for record in records:
        if record["status"] != "current":
            continue
        russian_path = str(record["russian_path"])
        text = (root / russian_path).read_text(encoding="utf-8")
        for match in LINK_RE.finditer(text):
            resolved = _resolve_link(russian_path, match.group("target"))
            expected = localized_lookup.get(resolved or "")
            if expected is not None and resolved != expected:
                raise ValueError(
                    f"Russian translation {russian_path} links to English "
                    f"{resolved} although Russian counterpart {expected} exists"
                )


def generate_localization_status(
    root: Path,
    manifest_path: str = DEFAULT_MANIFEST,
    glossary_path: str = DEFAULT_GLOSSARY,
    *,
    enforce_complete: bool = False,
) -> dict[str, object]:
    manifest = _load_manifest(root, manifest_path)
    glossary = _load_glossary(root, glossary_path)
    localization = manifest.get("localization")
    if not isinstance(localization, dict):
        raise ValueError("documentation manifest localization must be an object")
    if localization.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"documentation localization schema_version must be {SCHEMA_VERSION}"
        )
    if localization.get("canonical_locale") != "en":
        raise ValueError("documentation canonical locale must be en")
    if localization.get("path_strategy") != "mirrored-relative-path":
        raise ValueError("documentation localization path strategy must be mirrored")
    if localization.get("translation_hash") != "normalized-sha256":
        raise ValueError("documentation translation hash must be normalized-sha256")
    if localization.get("glossary") != glossary_path:
        raise ValueError(f"documentation localization glossary must be {glossary_path}")
    if localization.get("status_output") != DEFAULT_OUTPUT:
        raise ValueError(
            f"documentation localization status_output must be {DEFAULT_OUTPUT}"
        )
    enforcement = localization.get("enforcement")
    if enforcement not in {"existing-translations-current", "complete"}:
        raise ValueError("documentation localization enforcement is unsupported")
    enforce_complete = enforce_complete or enforcement == "complete"

    entrypoint_targets = localization.get("entrypoint_targets")
    if not isinstance(entrypoint_targets, dict):
        raise ValueError("documentation localization entrypoint_targets must be an object")
    documents = manifest.get("documents")
    if not isinstance(documents, dict):
        raise ValueError("documentation manifest documents must be an object")

    records: list[dict[str, object]] = []
    ids: set[str] = set()
    for source_path, raw_document in documents.items():
        if not isinstance(source_path, str) or not isinstance(raw_document, dict):
            raise ValueError("documentation manifest documents must map paths to objects")
        classification = raw_document.get("classification")
        if not isinstance(classification, dict):
            raise ValueError(f"document {source_path} classification must be an object")
        if (
            raw_document.get("state") != "current"
            or classification.get("human") is not True
            or classification.get("translation") != "required"
        ):
            continue

        document_id = _required_string(
            raw_document.get("id"), f"document {source_path} id"
        )
        if document_id in ids:
            raise ValueError(f"duplicate translated document id: {document_id}")
        ids.add(document_id)
        source_path = _repository_path(source_path, f"document {document_id} source")
        source_file = root / source_path
        if not source_file.is_file():
            raise ValueError(f"canonical English source is missing: {source_path}")
        english_path, russian_path = _translation_targets(
            document_id, raw_document, entrypoint_targets
        )
        source_text = source_file.read_text(encoding="utf-8")
        source_hash = normalized_sha256(source_text)
        russian_file = root / russian_path
        status = "missing"
        if russian_file.is_file():
            russian_text = russian_file.read_text(encoding="utf-8")
            metadata = _translation_metadata(russian_text, russian_path)
            expected_metadata = {
                "document_id": document_id,
                "locale": "ru",
                "source_path": source_path,
                "source_sha256": source_hash,
            }
            if metadata != expected_metadata:
                raise ValueError(
                    f"Russian translation metadata is stale or mismatched: "
                    f"{russian_path}; expected "
                    + json.dumps(expected_metadata, ensure_ascii=False)
                )
            if _fenced_bodies(source_text) != _fenced_bodies(russian_text):
                raise ValueError(
                    f"Russian translation changed, removed, or reordered fenced "
                    f"code blocks: {russian_path}"
                )
            status = "current"
        records.append(
            {
                "id": document_id,
                "title": raw_document.get("title"),
                "owner": raw_document.get("owner"),
                "diataxis": classification.get("diataxis"),
                "source_path": source_path,
                "english_path": english_path,
                "english_path_state": (
                    "migrated" if (root / english_path).is_file() else "planned"
                ),
                "russian_path": russian_path,
                "source_sha256": source_hash,
                "source_bytes": len(_normalize_text(source_text).encode("utf-8")),
                "status": status,
            }
        )

    records.sort(key=lambda item: (str(item["english_path"]), str(item["id"])))
    _validate_language_preserving_links(root, records)
    missing = [str(record["id"]) for record in records if record["status"] == "missing"]
    if enforce_complete and missing:
        raise ValueError(
            "Russian translation coverage is incomplete: " + ", ".join(missing)
        )

    current_count = len(records) - len(missing)
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "canonical_locale": "en",
        "target_locale": "ru",
        "path_strategy": "mirrored-relative-path",
        "translation_hash": "normalized-sha256",
        "enforcement": enforcement,
        "glossary": {
            "path": glossary_path,
            "term_count": len(glossary["terms"]),
        },
        "summary": {
            "required_document_count": len(records),
            "current_translation_count": current_count,
            "missing_translation_count": len(missing),
            "coverage_percent": (
                round(current_count * 100.0 / len(records), 2) if records else 100.0
            ),
            "complete": not missing,
        },
        "documents": records,
    }


def render_localization_status(
    root: Path,
    manifest_path: str = DEFAULT_MANIFEST,
    glossary_path: str = DEFAULT_GLOSSARY,
) -> str:
    return (
        json.dumps(
            generate_localization_status(root, manifest_path, glossary_path),
            ensure_ascii=False,
            indent=2,
        )
        + "\n"
    )


def _write_or_check(root: Path, *, check: bool, enforce_complete: bool) -> int:
    model = generate_localization_status(root, enforce_complete=enforce_complete)
    content = json.dumps(model, ensure_ascii=False, indent=2) + "\n"
    output = root / DEFAULT_OUTPUT
    if check:
        if not output.is_file() or output.read_text(encoding="utf-8") != content:
            print(
                "Translation status is stale; run "
                "python BuildTools/docs_localization.py --write",
                file=sys.stderr,
            )
            return 1
        summary = model["summary"]
        print(
            "Translation status is current: "
            f"{summary['current_translation_count']}/"
            f"{summary['required_document_count']} current"
        )
        return 0
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(content, encoding="utf-8")
    return 0


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate and validate English/Russian documentation parity."
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--enforce-complete",
        action="store_true",
        help="Fail when any translation-required document lacks a Russian counterpart.",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        return _write_or_check(
            args.root.resolve(),
            check=args.check,
            enforce_complete=args.enforce_complete,
        )
    except (OSError, json.JSONDecodeError, ValueError) as exception:
        print(f"Documentation localization validation failed: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
