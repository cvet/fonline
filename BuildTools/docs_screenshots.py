from __future__ import annotations

import argparse
import datetime
import hashlib
import html
import json
import posixpath
import re
import struct
import sys
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/DocumentationScreenshots.json"
DEFAULT_CATALOG = "Docs/generated/screenshots.json"
DEFAULT_OUTPUT_DIR = "Docs/assets/screenshots"
GENERATED_BY = "BuildTools/docs_screenshots.py"
OUTPUT_PATHS = (DEFAULT_CATALOG,)
SCREENSHOT_IDS = ("mapper-particle-preview", "mapper-spark-editor")
ASSET_PATHS = tuple(
    f"{DEFAULT_OUTPUT_DIR}/{screenshot_id}.png"
    for screenshot_id in SCREENSHOT_IDS
)
MANIFEST_PATHS = (DEFAULT_CATALOG, *ASSET_PATHS)
ID_PATTERN = re.compile(r"^[a-z][a-z0-9-]*$")
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(value: object, label: str, *, minimum: int = 1) -> list[str]:
    if not isinstance(value, list) or len(value) < minimum:
        raise ValueError(f"{label} must contain at least {minimum} strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _positive_int(value: object, label: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        raise ValueError(f"{label} must be a positive integer")
    return value


def _png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 24 or data[:8] != PNG_SIGNATURE:
        raise ValueError(f"{path.as_posix()} is not a PNG image")
    chunk_size = struct.unpack(">I", data[8:12])[0]
    if data[12:16] != b"IHDR" or chunk_size != 13:
        raise ValueError(f"{path.as_posix()} has no canonical PNG IHDR")
    return struct.unpack(">II", data[16:24])


def _normalized_text_bytes(path: Path) -> bytes:
    text = path.read_text(encoding="utf-8")
    return text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")


def _validate_embedding(root: Path, screenshot: dict[str, Any]) -> None:
    owning_document = root / screenshot["owning_document"]
    if not owning_document.is_file():
        raise ValueError(
            f"{screenshot['id']} owning document does not exist: "
            f"{screenshot['owning_document']}"
        )
    image_path = posixpath.relpath(
        screenshot["path"],
        start=PurePosixPath(screenshot["owning_document"]).parent.as_posix(),
    )
    expected_image = (
        f'<img src="{image_path}" '
        f'alt="{html.escape(screenshot["alt"], quote=True)}" loading="lazy">'
    )
    expected_caption = (
        f'<figcaption>{html.escape(screenshot["caption"])}</figcaption>'
    )
    text = owning_document.read_text(encoding="utf-8")
    if expected_image not in text:
        raise ValueError(
            f"{screenshot['id']} owning document must embed the screenshot "
            "with its manifest alt text"
        )
    if expected_caption not in text:
        raise ValueError(
            f"{screenshot['id']} owning document must carry its manifest caption"
        )


def load_screenshots(
    root: Path,
    manifest_path: str = DEFAULT_MANIFEST,
    *,
    validate_embeddings: bool = True,
) -> dict[str, Any]:
    manifest_file = root / manifest_path
    raw = json.loads(manifest_file.read_text(encoding="utf-8"))
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"documentation screenshots schema_version must be {SCHEMA_VERSION}"
        )
    title = _required_string(raw.get("title"), "title")
    revision = _required_string(
        raw.get("engine_base_revision"), "engine_base_revision"
    )
    if not REVISION_PATTERN.fullmatch(revision):
        raise ValueError("engine_base_revision must be a full lowercase Git SHA")
    revision_policy = _required_string(
        raw.get("revision_policy"), "revision_policy"
    )
    values = raw.get("screenshots")
    if not isinstance(values, list) or not values:
        raise ValueError("screenshots must be a non-empty array")

    ids: set[str] = set()
    paths: set[str] = set()
    screenshots: list[dict[str, Any]] = []
    for index, value in enumerate(values):
        label = f"screenshots[{index}]"
        if not isinstance(value, dict):
            raise ValueError(f"{label} must be an object")
        screenshot = dict(value)
        screenshot_id = _required_string(screenshot.get("id"), f"{label}.id")
        if not ID_PATTERN.fullmatch(screenshot_id):
            raise ValueError(f"{label}.id must be lower-kebab-case")
        if screenshot_id in ids:
            raise ValueError(f"duplicate screenshot id: {screenshot_id}")
        ids.add(screenshot_id)
        screenshot["id"] = screenshot_id

        for field in (
            "title",
            "path",
            "owning_document",
            "alt",
            "caption",
            "captured_on",
            "license",
        ):
            screenshot[field] = _required_string(
                screenshot.get(field), f"{label}.{field}"
            )
        if len(screenshot["alt"]) < 80:
            raise ValueError(f"{label}.alt must describe the complete screenshot")
        if len(screenshot["caption"]) < 80:
            raise ValueError(f"{label}.caption must explain the screenshot")
        if screenshot["license"] != "MIT":
            raise ValueError(f"{label}.license must be MIT")
        try:
            datetime.date.fromisoformat(screenshot["captured_on"])
        except ValueError as exception:
            raise ValueError(
                f"{label}.captured_on must be an ISO date"
            ) from exception

        path = PurePosixPath(screenshot["path"])
        if (
            path.suffix.casefold() != ".png"
            or not path.is_relative_to(DEFAULT_OUTPUT_DIR)
        ):
            raise ValueError(
                f"{label}.path must be a PNG below {DEFAULT_OUTPUT_DIR}"
            )
        path_text = path.as_posix()
        if path_text in paths:
            raise ValueError(f"duplicate screenshot path: {path_text}")
        paths.add(path_text)
        image_path = root / path_text
        if not image_path.is_file():
            raise ValueError(f"{label}.path does not exist: {path_text}")

        screenshot["width"] = _positive_int(
            screenshot.get("width"), f"{label}.width"
        )
        screenshot["height"] = _positive_int(
            screenshot.get("height"), f"{label}.height"
        )
        actual_size = _png_size(image_path)
        expected_size = (screenshot["width"], screenshot["height"])
        if actual_size != expected_size:
            raise ValueError(
                f"{label} dimensions are {actual_size}, expected {expected_size}"
            )
        screenshot_sha = _required_string(
            screenshot.get("sha256"), f"{label}.sha256"
        )
        if not SHA256_PATTERN.fullmatch(screenshot_sha):
            raise ValueError(f"{label}.sha256 must be lowercase SHA-256")
        actual_sha = hashlib.sha256(image_path.read_bytes()).hexdigest()
        if actual_sha != screenshot_sha:
            raise ValueError(
                f"{label}.sha256 is stale: expected {screenshot_sha}, "
                f"actual {actual_sha}"
            )

        capture = screenshot.get("capture")
        if not isinstance(capture, dict):
            raise ValueError(f"{label}.capture must be an object")
        capture = dict(capture)
        for field in ("host", "renderer", "project", "viewport", "command"):
            capture[field] = _required_string(
                capture.get(field), f"{label}.capture.{field}"
            )
        capture["interaction_steps"] = _string_list(
            capture.get("interaction_steps"),
            f"{label}.capture.interaction_steps",
            minimum=3,
        )
        screenshot["capture"] = capture

        source_paths = _string_list(
            screenshot.get("source_paths"), f"{label}.source_paths", minimum=3
        )
        for source_path in source_paths:
            if not (root / source_path).is_file():
                raise ValueError(
                    f"{label}.source_paths entry does not exist: {source_path}"
                )
        screenshot["source_paths"] = source_paths
        screenshot["recapture_triggers"] = _string_list(
            screenshot.get("recapture_triggers"),
            f"{label}.recapture_triggers",
            minimum=3,
        )
        if validate_embeddings:
            _validate_embedding(root, screenshot)
        screenshots.append(screenshot)

    if tuple(screenshot["id"] for screenshot in screenshots) != SCREENSHOT_IDS:
        raise ValueError(
            f"screenshot ids must be ordered as {list(SCREENSHOT_IDS)}"
        )

    return {
        "schema_version": SCHEMA_VERSION,
        "title": title,
        "engine_base_revision": revision,
        "revision_policy": revision_policy,
        "screenshots": screenshots,
    }


def render_outputs(root: Path) -> dict[str, str]:
    manifest = load_screenshots(root)
    records: list[dict[str, Any]] = []
    for screenshot in manifest["screenshots"]:
        source_hashes = {
            source_path: hashlib.sha256(
                _normalized_text_bytes(root / source_path)
            ).hexdigest()
            for source_path in screenshot["source_paths"]
        }
        record = dict(screenshot)
        record["source_sha256"] = source_hashes
        records.append(record)
    source_bytes = _normalized_text_bytes(root / DEFAULT_MANIFEST)
    catalog = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": DEFAULT_MANIFEST,
        "source_manifest_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "engine_base_revision": manifest["engine_base_revision"],
        "revision_policy": manifest["revision_policy"],
        "screenshot_count": len(records),
        "screenshots": records,
    }
    return {
        DEFAULT_CATALOG: json.dumps(catalog, ensure_ascii=False, indent=2) + "\n"
    }


def _write_or_check(root: Path, *, check: bool) -> int:
    stale: list[str] = []
    outputs = render_outputs(root)
    if tuple(outputs) != OUTPUT_PATHS:
        raise ValueError("rendered screenshot outputs do not match OUTPUT_PATHS")
    for relative_path, content in outputs.items():
        output = root / relative_path
        if check:
            if not output.is_file() or output.read_text(encoding="utf-8") != content:
                stale.append(relative_path)
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(content, encoding="utf-8")
    if stale:
        print(
            "Documentation screenshots are stale: " + ", ".join(stale),
            file=sys.stderr,
        )
        return 1
    if check:
        print("Documentation screenshots are current")
    return 0


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate versioned FOnline documentation screenshots."
    )
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        return _write_or_check(args.root.resolve(), check=args.check)
    except (OSError, json.JSONDecodeError, ValueError) as exception:
        print(
            f"Documentation screenshot validation failed: {exception}",
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
