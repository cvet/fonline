from __future__ import annotations

import json
from pathlib import Path


PUBLIC_MANIFEST = "docs-manifest.json"
ROUTING_OUTPUT = PUBLIC_MANIFEST + "#/routing"
TRANSLATION_OUTPUT = PUBLIC_MANIFEST + "#/translation_status"
BUILD_OUTPUT_DIR = "Workspace/Documentation"
SITE_OUTPUT_DIR = "Workspace/DocumentationSite"
SITE_CONFIG = "Docs/Site/_config.yml"
ROOT_ENDPOINTS = (PUBLIC_MANIFEST, "llms.txt", "llms-full.txt", "CNAME")
BUILD_SUPPORT_FILES = ("Gemfile", ".ruby-version", "CNAME")


def output_path(root: Path, relative_path: str) -> Path:
    path = relative_path.split("#/", 1)[0]
    if path in ROOT_ENDPOINTS or path in BUILD_SUPPORT_FILES:
        return root / BUILD_OUTPUT_DIR / path
    return root / path


def _section(pointer: str) -> str:
    if pointer not in {ROUTING_OUTPUT, TRANSLATION_OUTPUT}:
        raise ValueError(f"Unknown shared documentation metadata section: {pointer}")
    return pointer.split("#/", 1)[1]


def read_section(root: Path, pointer: str) -> dict[str, object]:
    document = json.loads(output_path(root, PUBLIC_MANIFEST).read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ValueError("Shared documentation metadata must be an object")
    value = document.get(_section(pointer))
    if not isinstance(value, dict):
        raise ValueError(f"Missing documentation metadata section: {pointer}; regenerate its owner")
    return value


def write_section(root: Path, pointer: str, value: dict[str, object]) -> None:
    path = output_path(root, PUBLIC_MANIFEST)
    path.parent.mkdir(parents=True, exist_ok=True)
    document = json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}
    if not isinstance(document, dict):
        raise ValueError("Shared documentation metadata must be an object")
    section = _section(pointer)
    if document.get(section) == value:
        return
    document[section] = value
    content = json.dumps(document, ensure_ascii=True, indent=2) + "\n"
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    temporary.replace(path)


def section_matches(root: Path, pointer: str, value: dict[str, object]) -> bool:
    try:
        return read_section(root, pointer) == value
    except (OSError, ValueError):
        return False

