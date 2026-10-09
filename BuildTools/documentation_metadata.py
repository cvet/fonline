from __future__ import annotations

import json
from pathlib import Path


PUBLIC_MANIFEST = "docs-manifest.json"
ROUTING_OUTPUT = PUBLIC_MANIFEST + "#/routing"
TRANSLATION_OUTPUT = PUBLIC_MANIFEST + "#/translation_status"


def _section(pointer: str) -> str:
    if pointer not in {ROUTING_OUTPUT, TRANSLATION_OUTPUT}:
        raise ValueError(f"Unknown shared documentation metadata section: {pointer}")
    return pointer.split("#/", 1)[1]


def read_section(root: Path, pointer: str) -> dict[str, object]:
    document = json.loads((root / PUBLIC_MANIFEST).read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ValueError("Shared documentation metadata must be an object")
    value = document.get(_section(pointer))
    if not isinstance(value, dict):
        raise ValueError(f"Missing documentation metadata section: {pointer}; regenerate its owner")
    return value


def write_section(root: Path, pointer: str, value: dict[str, object]) -> None:
    path = root / PUBLIC_MANIFEST
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

