from __future__ import annotations

import argparse
import hashlib
import html
import json
import posixpath
import re
import sys
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/DocumentationDiagrams.json"
DEFAULT_CATALOG = "Docs/generated/diagrams.json"
DEFAULT_OUTPUT_DIR = "Docs/assets/diagrams"
GENERATED_BY = "BuildTools/docs_diagrams.py"
DIAGRAM_IDS = (
    "engine-game-boundary",
    "generated-content-pipeline",
    "documentation-delivery",
)
OUTPUT_PATHS = (
    DEFAULT_CATALOG,
    *(
        path
        for diagram_id in DIAGRAM_IDS
        for path in (
            f"{DEFAULT_OUTPUT_DIR}/{diagram_id}.svg",
            f"{DEFAULT_OUTPUT_DIR}/{diagram_id}-mobile.svg",
        )
    ),
)
ID_PATTERN = re.compile(r"^[a-z][a-z0-9-]*$")
ROLE_PALETTE = {
    "engine": ("#dff3e9", "#176b55"),
    "project": ("#fff1cf", "#8a5a00"),
    "process": ("#e7f0fa", "#2d5e91"),
    "artifact": ("#f1e9f6", "#6d497d"),
    "validation": ("#fbe6e2", "#a33a2b"),
    "delivery": ("#e4f3f2", "#287274"),
    "neutral": ("#edf1ee", "#59645d"),
}


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _number(value: object, label: str, *, minimum: float = 0) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        raise ValueError(f"{label} must be a number")
    result = float(value)
    if result < minimum:
        raise ValueError(f"{label} must be at least {minimum:g}")
    return result


def _string_list(value: object, label: str, *, minimum: int = 1) -> list[str]:
    if not isinstance(value, list) or len(value) < minimum:
        raise ValueError(f"{label} must contain at least {minimum} strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _role(value: object, label: str) -> str:
    role = _required_string(value, label)
    if role not in ROLE_PALETTE:
        raise ValueError(f"{label} must be one of {sorted(ROLE_PALETTE)}")
    return role


def _box(
    value: object,
    label: str,
    *,
    canvas_width: float,
    canvas_height: float,
) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    box = dict(value)
    for field in ("x", "y"):
        box[field] = _number(box.get(field), f"{label}.{field}")
    for field in ("width", "height"):
        box[field] = _number(box.get(field), f"{label}.{field}", minimum=1)
    if box["x"] + box["width"] > canvas_width:
        raise ValueError(f"{label} exceeds the diagram width")
    if box["y"] + box["height"] > canvas_height:
        raise ValueError(f"{label} exceeds the diagram height")
    box["role"] = _role(box.get("role"), f"{label}.role")
    box["title"] = _required_string(box.get("title"), f"{label}.title")
    return box


def _boxes_overlap(first: dict[str, Any], second: dict[str, Any]) -> bool:
    return not (
        first["x"] + first["width"] <= second["x"]
        or second["x"] + second["width"] <= first["x"]
        or first["y"] + first["height"] <= second["y"]
        or second["y"] + second["height"] <= first["y"]
    )


def _validate_embedding(root: Path, diagram: dict[str, Any]) -> None:
    owning_document = root / str(diagram["owning_document"])
    if not owning_document.is_file():
        raise ValueError(
            f"{diagram['id']} owning document does not exist: "
            f"{diagram['owning_document']}"
        )
    document_directory = PurePosixPath(str(diagram["owning_document"])).parent
    image_path = PurePosixPath(
        posixpath.relpath(
            f"{DEFAULT_OUTPUT_DIR}/{diagram['id']}.svg",
            start=document_directory.as_posix(),
        )
    )
    mobile_image_path = PurePosixPath(
        posixpath.relpath(
            f"{DEFAULT_OUTPUT_DIR}/{diagram['id']}-mobile.svg",
            start=document_directory.as_posix(),
        )
    )
    expected_source = (
        f'<source media="(max-width: 700px)" '
        f'srcset="{mobile_image_path}">'
    )
    expected_image = (
        f'<img src="{image_path}" alt="{html.escape(str(diagram["alt"]), quote=True)}" '
        'loading="lazy">'
    )
    expected_caption = (
        f'<figcaption>{html.escape(str(diagram["caption"]))}</figcaption>'
    )
    text = owning_document.read_text(encoding="utf-8")
    if expected_source not in text:
        raise ValueError(
            f"{diagram['id']} owning document must embed the generated mobile SVG"
        )
    if expected_image not in text:
        raise ValueError(
            f"{diagram['id']} owning document must embed the generated SVG "
            "with its manifest alt text"
        )
    if expected_caption not in text:
        raise ValueError(
            f"{diagram['id']} owning document must carry its manifest caption"
        )


def load_diagrams(
    root: Path,
    manifest_path: str = DEFAULT_MANIFEST,
    *,
    validate_embeddings: bool = True,
) -> dict[str, Any]:
    manifest_file = root / manifest_path
    raw = json.loads(manifest_file.read_text(encoding="utf-8"))
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"documentation diagrams schema_version must be {SCHEMA_VERSION}"
        )
    title = _required_string(raw.get("title"), "title")
    values = raw.get("diagrams")
    if not isinstance(values, list) or not values:
        raise ValueError("diagrams must be a non-empty array")

    ids: list[str] = []
    diagrams: list[dict[str, Any]] = []
    for index, value in enumerate(values):
        label = f"diagrams[{index}]"
        if not isinstance(value, dict):
            raise ValueError(f"{label} must be an object")
        diagram = dict(value)
        diagram_id = _required_string(diagram.get("id"), f"{label}.id")
        if not ID_PATTERN.fullmatch(diagram_id):
            raise ValueError(f"{label}.id must be lower-kebab-case")
        if diagram_id in ids:
            raise ValueError(f"duplicate diagram id: {diagram_id}")
        ids.append(diagram_id)
        for field in ("title", "description", "owning_document", "alt", "caption"):
            diagram[field] = _required_string(
                diagram.get(field), f"{label}.{field}"
            )
        if len(diagram["alt"]) < 80:
            raise ValueError(f"{label}.alt must describe the complete diagram")
        if len(diagram["caption"]) < 80:
            raise ValueError(f"{label}.caption must explain the diagram meaning")
        width = _number(diagram.get("width"), f"{label}.width", minimum=640)
        height = _number(diagram.get("height"), f"{label}.height", minimum=360)
        diagram["width"] = int(width)
        diagram["height"] = int(height)
        source_paths = _string_list(
            diagram.get("source_paths"), f"{label}.source_paths", minimum=2
        )
        for source_path in source_paths:
            if not (root / source_path).is_file():
                raise ValueError(
                    f"{label}.source_paths entry does not exist: {source_path}"
                )
        diagram["source_paths"] = source_paths

        bands = diagram.get("bands")
        if not isinstance(bands, list) or not bands:
            raise ValueError(f"{label}.bands must be a non-empty array")
        diagram["bands"] = [
            _box(
                band,
                f"{label}.bands[{band_index}]",
                canvas_width=width,
                canvas_height=height,
            )
            for band_index, band in enumerate(bands)
        ]

        nodes = diagram.get("nodes")
        if not isinstance(nodes, list) or not nodes:
            raise ValueError(f"{label}.nodes must be a non-empty array")
        node_ids: set[str] = set()
        normalized_nodes: list[dict[str, Any]] = []
        for node_index, node_value in enumerate(nodes):
            node_label = f"{label}.nodes[{node_index}]"
            node = _box(
                node_value,
                node_label,
                canvas_width=width,
                canvas_height=height,
            )
            node_id = _required_string(node.get("id"), f"{node_label}.id")
            if not ID_PATTERN.fullmatch(node_id):
                raise ValueError(f"{node_label}.id must be lower-kebab-case")
            if node_id in node_ids:
                raise ValueError(f"{label} contains duplicate node id: {node_id}")
            node_ids.add(node_id)
            node["id"] = node_id
            node["lines"] = _string_list(
                node.get("lines"), f"{node_label}.lines", minimum=1
            )
            if len(node["lines"]) > 3:
                raise ValueError(f"{node_label}.lines supports at most 3 lines")
            for existing in normalized_nodes:
                if _boxes_overlap(existing, node):
                    raise ValueError(
                        f"{label} nodes overlap: {existing['id']} and {node_id}"
                    )
            normalized_nodes.append(node)
        diagram["nodes"] = normalized_nodes

        edges = diagram.get("edges")
        if not isinstance(edges, list) or not edges:
            raise ValueError(f"{label}.edges must be a non-empty array")
        normalized_edges: list[dict[str, str]] = []
        for edge_index, edge_value in enumerate(edges):
            edge_label = f"{label}.edges[{edge_index}]"
            if not isinstance(edge_value, dict):
                raise ValueError(f"{edge_label} must be an object")
            source = _required_string(edge_value.get("from"), f"{edge_label}.from")
            target = _required_string(edge_value.get("to"), f"{edge_label}.to")
            if source == target or source not in node_ids or target not in node_ids:
                raise ValueError(
                    f"{edge_label} must connect two distinct existing nodes"
                )
            normalized_edges.append({"from": source, "to": target})
        diagram["edges"] = normalized_edges
        if validate_embeddings:
            _validate_embedding(root, diagram)
        diagrams.append(diagram)

    if tuple(ids) != DIAGRAM_IDS:
        raise ValueError(
            "diagram ids must match the maintained output contract: "
            + ", ".join(DIAGRAM_IDS)
        )
    return {
        "schema_version": SCHEMA_VERSION,
        "title": title,
        "diagrams": diagrams,
    }


def _svg_text(
    x: float,
    y: float,
    value: str,
    *,
    css_class: str,
    anchor: str = "middle",
) -> str:
    return (
        f'<text class="{css_class}" x="{x:g}" y="{y:g}" '
        f'text-anchor="{anchor}">{html.escape(value)}</text>'
    )


def _edge_points(
    source: dict[str, Any], target: dict[str, Any]
) -> tuple[float, float, float, float]:
    source_center_x = source["x"] + source["width"] / 2
    source_center_y = source["y"] + source["height"] / 2
    target_center_x = target["x"] + target["width"] / 2
    target_center_y = target["y"] + target["height"] / 2
    horizontal = abs(target_center_x - source_center_x) >= abs(
        target_center_y - source_center_y
    )
    if horizontal:
        if target_center_x >= source_center_x:
            return (
                source["x"] + source["width"],
                source_center_y,
                target["x"],
                target_center_y,
            )
        return (
            source["x"],
            source_center_y,
            target["x"] + target["width"],
            target_center_y,
        )
    if target_center_y >= source_center_y:
        return (
            source_center_x,
            source["y"] + source["height"],
            target_center_x,
            target["y"],
        )
    return (
        source_center_x,
        source["y"],
        target_center_x,
        target["y"] + target["height"],
    )


def render_svg(diagram: dict[str, Any]) -> str:
    width = diagram["width"]
    height = diagram["height"]
    title_id = f"{diagram['id']}-title"
    description_id = f"{diagram['id']}-description"
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
            f'height="{height}" viewBox="0 0 {width} {height}" role="img" '
            f'aria-labelledby="{title_id} {description_id}">'
        ),
        f"  <title id=\"{title_id}\">{html.escape(diagram['title'])}</title>",
        (
            f"  <desc id=\"{description_id}\">"
            f"{html.escape(diagram['description'])}</desc>"
        ),
        "  <defs>",
        '    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" '
        'markerWidth="7" markerHeight="7" orient="auto-start-reverse">',
        '      <path d="M 0 0 L 10 5 L 0 10 z" fill="#42534a"/>',
        "    </marker>",
        "    <style>",
        "      text { font-family: Inter, Segoe UI, Arial, sans-serif; fill: #17211c; }",
        "      .diagram-title { font-size: 28px; font-weight: 700; }",
        "      .diagram-subtitle { font-size: 15px; fill: #4c5b53; }",
        "      .band-title { font-size: 16px; font-weight: 700; }",
        "      .node-title { font-size: 16px; font-weight: 700; }",
        "      .node-line { font-size: 13px; fill: #35433b; }",
        "      .source-note { font-size: 12px; fill: #58675f; }",
        "      .edge { fill: none; stroke: #42534a; stroke-width: 2; marker-end: url(#arrow); }",
        "    </style>",
        "  </defs>",
        f'  <rect width="{width}" height="{height}" rx="6" fill="#f8faf8" stroke="#c9d2cc"/>',
        "  "
        + _svg_text(
            width / 2,
            47,
            diagram["title"],
            css_class="diagram-title",
        ),
        "  "
        + _svg_text(
            width / 2,
            76,
            diagram["description"],
            css_class="diagram-subtitle",
        ),
    ]

    for band in diagram["bands"]:
        fill, stroke = ROLE_PALETTE[band["role"]]
        lines.extend(
            [
                (
                    f'  <rect x="{band["x"]:g}" y="{band["y"]:g}" '
                    f'width="{band["width"]:g}" height="{band["height"]:g}" '
                    f'rx="6" fill="{fill}" fill-opacity="0.38" '
                    f'stroke="{stroke}" stroke-opacity="0.58"/>'
                ),
                "  "
                + _svg_text(
                    band["x"] + band["width"] / 2,
                    band["y"] + 34,
                    band["title"],
                    css_class="band-title",
                ),
            ]
        )

    nodes_by_id = {node["id"]: node for node in diagram["nodes"]}
    for edge in diagram["edges"]:
        x1, y1, x2, y2 = _edge_points(
            nodes_by_id[edge["from"]], nodes_by_id[edge["to"]]
        )
        offset = max(24.0, abs(x2 - x1) * 0.34)
        if abs(x2 - x1) >= abs(y2 - y1):
            path = (
                f"M {x1:g} {y1:g} C {x1 + (offset if x2 >= x1 else -offset):g} "
                f"{y1:g}, {x2 - (offset if x2 >= x1 else -offset):g} {y2:g}, "
                f"{x2:g} {y2:g}"
            )
        else:
            vertical_offset = max(24.0, abs(y2 - y1) * 0.34)
            direction = vertical_offset if y2 >= y1 else -vertical_offset
            path = (
                f"M {x1:g} {y1:g} C {x1:g} {y1 + direction:g}, "
                f"{x2:g} {y2 - direction:g}, {x2:g} {y2:g}"
            )
        lines.append(f'  <path class="edge" d="{path}"/>')

    for node in diagram["nodes"]:
        fill, stroke = ROLE_PALETTE[node["role"]]
        lines.append(
            (
                f'  <rect x="{node["x"]:g}" y="{node["y"]:g}" '
                f'width="{node["width"]:g}" height="{node["height"]:g}" '
                f'rx="6" fill="{fill}" stroke="{stroke}" stroke-width="2"/>'
            )
        )
        center_x = node["x"] + node["width"] / 2
        title_y = node["y"] + 31
        lines.append(
            "  "
            + _svg_text(
                center_x,
                title_y,
                node["title"],
                css_class="node-title",
            )
        )
        for line_index, value in enumerate(node["lines"]):
            lines.append(
                "  "
                + _svg_text(
                    center_x,
                    title_y + 26 + line_index * 20,
                    value,
                    css_class="node-line",
                )
            )

    provenance = (
        f"Source-backed diagram: {len(diagram['source_paths'])} checked paths; "
        f"full provenance in {DEFAULT_CATALOG}"
    )
    lines.extend(
        [
            "  "
            + _svg_text(
                width / 2,
                height - 28,
                provenance,
                css_class="source-note",
            ),
            "</svg>",
            "",
        ]
    )
    return "\n".join(lines)


def _wrap_words(value: str, maximum: int) -> list[str]:
    words = value.split()
    lines: list[str] = []
    current = ""
    for word in words:
        candidate = f"{current} {word}".strip()
        if current and len(candidate) > maximum:
            lines.append(current)
            current = word
        else:
            current = candidate
    if current:
        lines.append(current)
    return lines


def render_mobile_svg(diagram: dict[str, Any]) -> tuple[str, int, int]:
    width = 480
    title_lines = _wrap_words(str(diagram["title"]), 36)
    if len(title_lines) > 2:
        raise ValueError(f"{diagram['id']} title is too long for the mobile diagram")
    title_id = f"{diagram['id']}-mobile-title"
    description_id = f"{diagram['id']}-mobile-description"
    nodes_by_id = {node["id"]: node for node in diagram["nodes"]}
    used_nodes: set[str] = set()
    groups: list[tuple[str, str, list[dict[str, Any]]]] = []
    for band in diagram["bands"]:
        band_nodes = [
            node
            for node in diagram["nodes"]
            if (
                band["x"] <= node["x"] + node["width"] / 2
                <= band["x"] + band["width"]
                and band["y"] <= node["y"] + node["height"] / 2
                <= band["y"] + band["height"]
            )
        ]
        band_nodes.sort(key=lambda node: (node["y"], node["x"]))
        used_nodes.update(str(node["id"]) for node in band_nodes)
        groups.append((str(band["title"]), str(band["role"]), band_nodes))
    ungrouped = [
        node for node in diagram["nodes"] if str(node["id"]) not in used_nodes
    ]
    if ungrouped:
        groups.append(("Shared integration contract", "neutral", ungrouped))

    group_heights = [62 + len(nodes) * 104 for _, _, nodes in groups]
    connection_lines: list[str] = []
    for edge in diagram["edges"]:
        relation = (
            f"{nodes_by_id[edge['from']]['title']} to "
            f"{nodes_by_id[edge['to']]['title']}"
        )
        wrapped = _wrap_words(relation, 46)
        if len(wrapped) > 2:
            raise ValueError(
                f"{diagram['id']} mobile connection label is too long: {relation}"
            )
        connection_lines.extend(wrapped)
        connection_lines.append("")
    if connection_lines and not connection_lines[-1]:
        connection_lines.pop()

    header_height = 42 + len(title_lines) * 29
    groups_height = sum(group_heights) + max(0, len(groups) - 1) * 16
    connections_height = 66 + len(connection_lines) * 22
    height = header_height + groups_height + connections_height + 70
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
            f'height="{height}" viewBox="0 0 {width} {height}" role="img" '
            f'aria-labelledby="{title_id} {description_id}">'
        ),
        f'  <title id="{title_id}">{html.escape(diagram["title"])}</title>',
        (
            f'  <desc id="{description_id}">'
            f'{html.escape(diagram["description"])} Mobile reading order groups '
            "the same source-owned nodes and lists their connections.</desc>"
        ),
        "  <style>",
        "    text { font-family: Inter, Segoe UI, Arial, sans-serif; fill: #17211c; }",
        "    .diagram-title { font-size: 24px; font-weight: 700; }",
        "    .band-title { font-size: 18px; font-weight: 700; }",
        "    .node-title { font-size: 17px; font-weight: 700; }",
        "    .node-line { font-size: 15px; fill: #35433b; }",
        "    .connection { font-size: 15px; fill: #35433b; }",
        "    .source-note { font-size: 13px; fill: #58675f; }",
        "  </style>",
        f'  <rect width="{width}" height="{height}" rx="6" fill="#f8faf8" stroke="#c9d2cc"/>',
    ]
    y = 38
    for title_line in title_lines:
        lines.append(
            "  "
            + _svg_text(
                width / 2,
                y,
                title_line,
                css_class="diagram-title",
            )
        )
        y += 29
    y = header_height

    for group_index, (group_title, role, nodes) in enumerate(groups):
        fill, stroke = ROLE_PALETTE[role]
        group_height = group_heights[group_index]
        lines.append(
            (
                f'  <rect x="18" y="{y}" width="{width - 36}" '
                f'height="{group_height}" rx="6" fill="{fill}" '
                f'fill-opacity="0.38" stroke="{stroke}" stroke-opacity="0.58"/>'
            )
        )
        lines.append(
            "  "
            + _svg_text(
                width / 2,
                y + 34,
                group_title,
                css_class="band-title",
            )
        )
        node_y = y + 54
        for node in nodes:
            node_fill, node_stroke = ROLE_PALETTE[node["role"]]
            lines.append(
                (
                    f'  <rect x="38" y="{node_y}" width="{width - 76}" '
                    f'height="88" rx="6" fill="{node_fill}" '
                    f'stroke="{node_stroke}" stroke-width="2"/>'
                )
            )
            lines.append(
                "  "
                + _svg_text(
                    width / 2,
                    node_y + 29,
                    node["title"],
                    css_class="node-title",
                )
            )
            for line_index, value in enumerate(node["lines"]):
                lines.append(
                    "  "
                    + _svg_text(
                        width / 2,
                        node_y + 53 + line_index * 18,
                        value,
                        css_class="node-line",
                    )
                )
            node_y += 104
        y += group_height + 16

    lines.append(
        "  "
        + _svg_text(
            width / 2,
            y + 28,
            "Connections",
            css_class="band-title",
        )
    )
    connection_y = y + 56
    for value in connection_lines:
        if value:
            lines.append(
                "  "
                + _svg_text(
                    width / 2,
                    connection_y,
                    value,
                    css_class="connection",
                )
            )
        connection_y += 22
    lines.extend(
        [
            "  "
            + _svg_text(
                width / 2,
                height - 28,
                f"Full provenance and exact hashes: {DEFAULT_CATALOG}",
                css_class="source-note",
            ),
            "</svg>",
            "",
        ]
    )
    return "\n".join(lines), width, height


def _normalized_text_bytes(path: Path) -> bytes:
    text = path.read_text(encoding="utf-8")
    return text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")


def render_outputs(root: Path) -> dict[str, str]:
    manifest = load_diagrams(root)
    svg_outputs: dict[str, str] = {}
    catalog_diagrams: list[dict[str, Any]] = []
    for diagram in manifest["diagrams"]:
        output_path = f"{DEFAULT_OUTPUT_DIR}/{diagram['id']}.svg"
        mobile_output_path = (
            f"{DEFAULT_OUTPUT_DIR}/{diagram['id']}-mobile.svg"
        )
        svg = render_svg(diagram)
        mobile_svg, mobile_width, mobile_height = render_mobile_svg(diagram)
        svg_outputs[output_path] = svg
        svg_outputs[mobile_output_path] = mobile_svg
        catalog_diagrams.append(
            {
                "id": diagram["id"],
                "title": diagram["title"],
                "description": diagram["description"],
                "owning_document": diagram["owning_document"],
                "alt": diagram["alt"],
                "caption": diagram["caption"],
                "source_paths": diagram["source_paths"],
                "variants": [
                    {
                        "id": "desktop",
                        "path": output_path,
                        "width": diagram["width"],
                        "height": diagram["height"],
                        "sha256": hashlib.sha256(
                            svg.encode("utf-8")
                        ).hexdigest(),
                    },
                    {
                        "id": "mobile",
                        "path": mobile_output_path,
                        "width": mobile_width,
                        "height": mobile_height,
                        "sha256": hashlib.sha256(
                            mobile_svg.encode("utf-8")
                        ).hexdigest(),
                    },
                ],
            }
        )
    source_bytes = _normalized_text_bytes(root / DEFAULT_MANIFEST)
    catalog = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": DEFAULT_MANIFEST,
        "source_manifest_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "diagram_count": len(catalog_diagrams),
        "diagrams": catalog_diagrams,
    }
    return {
        DEFAULT_CATALOG: json.dumps(catalog, ensure_ascii=False, indent=2) + "\n",
        **svg_outputs,
    }


def _write_or_check(root: Path, *, check: bool) -> int:
    stale: list[str] = []
    outputs = render_outputs(root)
    if tuple(outputs) != OUTPUT_PATHS:
        raise ValueError("rendered diagram outputs do not match OUTPUT_PATHS")
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
            "Documentation diagrams are stale: " + ", ".join(stale),
            file=sys.stderr,
        )
        return 1
    if check:
        print(
            f"Documentation diagrams are current: {len(DIAGRAM_IDS)} diagrams, "
            f"{len(outputs) - 1} SVG variants"
        )
    return 0


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate source-owned accessible FOnline documentation diagrams."
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
        print(f"Documentation diagram generation failed: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
