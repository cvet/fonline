from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
import struct
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1
DEFAULT_JSON_OUTPUT = "Docs/generated/project-remote-calls.json"
DEFAULT_MARKDOWN_OUTPUT = "Docs/generated/project-remote-calls.md"
SOURCE_PARSER = "Source/Tools/MetadataBaker.cpp"
VALID_TARGETS = {"Server": "server", "Client": "client"}
OPPOSITE_SIDE = {"server": "client", "client": "server"}


class MetadataDecodeError(ValueError):
    pass


class _MetadataReader:
    def __init__(self, data: bytes) -> None:
        self._data = memoryview(data)
        self._offset = 0

    @property
    def remaining(self) -> int:
        return len(self._data) - self._offset

    def _read_integer(self, format_string: str, label: str) -> int:
        size = struct.calcsize(format_string)
        if self.remaining < size:
            raise MetadataDecodeError(f"Truncated metadata while reading {label}")
        value = struct.unpack_from(format_string, self._data, self._offset)[0]
        self._offset += size
        return int(value)

    def read_uint16(self, label: str) -> int:
        return self._read_integer("<H", label)

    def read_uint32(self, label: str) -> int:
        return self._read_integer("<I", label)

    def read_text(self, size: int, label: str) -> str:
        if size > self.remaining:
            raise MetadataDecodeError(f"Truncated metadata while reading {label}: {size} bytes requested")
        raw = self._data[self._offset : self._offset + size].tobytes()
        self._offset += size
        try:
            return raw.decode("utf-8")
        except UnicodeDecodeError as exception:
            raise MetadataDecodeError(f"Invalid UTF-8 in {label}") from exception

    def verify_end(self) -> None:
        if self.remaining != 0:
            raise MetadataDecodeError(f"Metadata has {self.remaining} trailing bytes")


@dataclass(frozen=True)
class _RemoteCallEvidence:
    metadata_side: str
    direction: str
    input_id: str


@dataclass
class _RemoteCallRecord:
    target: str
    name: str
    subsystem_hint: str
    arguments: list[dict[str, object]]
    evidence: list[_RemoteCallEvidence]


def decode_metadata(data: bytes) -> dict[str, list[list[str]]]:
    reader = _MetadataReader(data)
    sections: dict[str, list[list[str]]] = {}
    section_count = reader.read_uint16("section count")

    for section_index in range(section_count):
        name_size = reader.read_uint16(f"section {section_index} name length")
        if name_size == 0:
            raise MetadataDecodeError(f"Metadata section {section_index} has an empty name")
        name = reader.read_text(name_size, f"section {section_index} name")
        if name in sections:
            raise MetadataDecodeError(f"Metadata contains duplicate section {name}")

        entry_count = reader.read_uint32(f"section {name} entry count")
        entries: list[list[str]] = []
        for entry_index in range(entry_count):
            part_count = reader.read_uint32(f"section {name} entry {entry_index} part count")
            parts: list[str] = []
            for part_index in range(part_count):
                part_size = reader.read_uint16(
                    f"section {name} entry {entry_index} part {part_index} length"
                )
                parts.append(
                    reader.read_text(part_size, f"section {name} entry {entry_index} part {part_index}")
                )
            entries.append(parts)
        sections[name] = entries

    reader.verify_end()
    return sections


def _metadata_target(sections: dict[str, list[list[str]]], path: Path) -> str:
    target_entries = sections.get("Target")
    if target_entries is None or len(target_entries) != 1 or len(target_entries[0]) != 1:
        raise MetadataDecodeError(f"Metadata input has no unambiguous Target section: {path}")
    target = target_entries[0][0]
    if target not in VALID_TARGETS:
        raise MetadataDecodeError(f"Remote-call docs require Server or Client metadata, got {target}: {path}")
    return VALID_TARGETS[target]


def _parse_remote_call_entry(
    entry: list[str], metadata_side: str, input_id: str, path: Path
) -> _RemoteCallRecord:
    if len(entry) < 3 or (len(entry) - 3) % 3 != 0:
        raise MetadataDecodeError(f"Malformed RemoteCall entry in {path}: expected header plus argument triples")

    name, subsystem_hint, direction = entry[:3]
    if not name or not subsystem_hint:
        raise MetadataDecodeError(f"Malformed RemoteCall entry in {path}: name and subsystem hint are required")
    if "/" in subsystem_hint or "\\" in subsystem_hint:
        raise MetadataDecodeError(f"RemoteCall subsystem hint must be a file name: {subsystem_hint}")
    if direction not in {"In", "Out"}:
        raise MetadataDecodeError(f"Malformed RemoteCall direction in {path}: {direction}")

    target = metadata_side if direction == "In" else OPPOSITE_SIDE[metadata_side]
    arguments: list[dict[str, object]] = []
    argument_names: set[str] = set()
    for index in range(3, len(entry), 3):
        type_name, nullable_marker, argument_name = entry[index : index + 3]
        if not type_name or not argument_name:
            raise MetadataDecodeError(f"Malformed RemoteCall argument in {path}: type and name are required")
        if nullable_marker not in {"", "?"}:
            raise MetadataDecodeError(
                f"Malformed RemoteCall nullability marker in {path}: {nullable_marker}"
            )
        if argument_name in argument_names:
            raise MetadataDecodeError(f"Duplicate RemoteCall argument name in {path}: {argument_name}")
        argument_names.add(argument_name)
        arguments.append(
            {
                "name": argument_name,
                "type": type_name + nullable_marker,
                "nullable": nullable_marker == "?",
            }
        )

    return _RemoteCallRecord(
        target=target,
        name=name,
        subsystem_hint=subsystem_hint,
        arguments=arguments,
        evidence=[
            _RemoteCallEvidence(
                metadata_side=metadata_side,
                direction=direction.lower(),
                input_id=input_id,
            )
        ],
    )


def _display_input_path(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return path.name


def _argument_signature(argument: dict[str, object]) -> str:
    return f"{argument['type']} {argument['name']}"


def _handler_signature(record: _RemoteCallRecord) -> str:
    namespace = Path(record.subsystem_hint).stem
    arguments = ", ".join(_argument_signature(argument) for argument in record.arguments)
    if record.target == "server":
        arguments = "Player player" + (f", {arguments}" if arguments else "")
    return f"void {namespace}::{record.name}({arguments})"


def _caller_surfaces(record: _RemoteCallRecord) -> list[str]:
    arguments = ", ".join(_argument_signature(argument) for argument in record.arguments)
    if record.target == "server":
        return [f"Player.ServerCall.{record.name}({arguments})"]
    return [
        f"Player.ClientCall.{record.name}({arguments})",
        f"Critter.PlayerClientCall.{record.name}({arguments})",
    ]


def _record_identity(record: _RemoteCallRecord) -> tuple[object, ...]:
    return (
        record.target,
        record.name,
        record.subsystem_hint,
        tuple((argument["name"], argument["type"], argument["nullable"]) for argument in record.arguments),
    )


def generate_remote_call_model(
    root: Path, metadata_paths: list[Path], *, require_paired: bool = True
) -> dict[str, object]:
    root = root.resolve()
    resolved_paths = [
        path.resolve() if path.is_absolute() else (root / path).resolve()
        for path in metadata_paths
    ]
    if not resolved_paths:
        raise ValueError("At least one baked metadata input is required")
    if len(resolved_paths) != len(set(resolved_paths)):
        raise ValueError("Duplicate baked metadata input path")

    inputs: list[dict[str, object]] = []
    records_by_key: dict[tuple[str, str], _RemoteCallRecord] = {}
    for path in resolved_paths:
        data = path.read_bytes()
        sections = decode_metadata(data)
        metadata_side = _metadata_target(sections, path)
        digest = hashlib.sha256(data).hexdigest()
        input_id = f"{metadata_side}:{path.name}:{digest[:12]}"
        inputs.append(
            {
                "id": input_id,
                "side": metadata_side,
                "path": _display_input_path(root, path),
                "sha256": digest,
            }
        )

        for entry in sections.get("RemoteCall", []):
            record = _parse_remote_call_entry(entry, metadata_side, input_id, path)
            key = (record.target, record.name)
            existing = records_by_key.get(key)
            if existing is None:
                records_by_key[key] = record
                continue
            if _record_identity(existing) != _record_identity(record):
                raise MetadataDecodeError(
                    f"RemoteCall metadata differs between baked inputs: {record.target}.{record.name}"
                )
            if any(evidence.metadata_side == metadata_side for evidence in existing.evidence):
                raise MetadataDecodeError(
                    f"RemoteCall metadata is duplicated on {metadata_side} side: {record.target}.{record.name}"
                )
            existing.evidence.extend(record.evidence)

    symbols: list[dict[str, object]] = []
    for record in sorted(records_by_key.values(), key=lambda item: (item.target, item.name)):
        evidence_sides = {evidence.metadata_side for evidence in record.evidence}
        if require_paired and evidence_sides != {"server", "client"}:
            raise MetadataDecodeError(
                f"RemoteCall metadata is not paired across server/client outputs: {record.target}.{record.name}"
            )

        symbol_id = f"script.remote-call.{record.target}.{record.name}"
        symbols.append(
            {
                "id": symbol_id,
                "family_id": symbol_id,
                "kind": "remote-call",
                "name": record.name,
                "target": record.target,
                "runtime_sides": ["server", "client"],
                "signature": "remote "
                + record.target
                + " "
                + record.name
                + "("
                + ", ".join(_argument_signature(argument) for argument in record.arguments)
                + ")",
                "arguments": record.arguments,
                "subsystem_hint": record.subsystem_hint,
                "handler_attribute": "ServerRemoteCall"
                if record.target == "server"
                else "ClientRemoteCall",
                "handler_signature": _handler_signature(record),
                "caller_surfaces": _caller_surfaces(record),
                "source": None,
                "source_hint": record.subsystem_hint,
                "support_status": "project-owned",
                "evidence": [
                    {
                        "metadata_side": evidence.metadata_side,
                        "direction": evidence.direction,
                        "input_id": evidence.input_id,
                    }
                    for evidence in sorted(record.evidence, key=lambda item: item.metadata_side)
                ],
            }
        )

    symbol_ids = [str(symbol["id"]) for symbol in symbols]
    if len(symbol_ids) != len(set(symbol_ids)):
        raise MetadataDecodeError("Generated RemoteCall symbol IDs are not unique")

    target_counts = Counter(str(symbol["target"]) for symbol in symbols)
    paired_count = sum(
        {str(evidence["metadata_side"]) for evidence in symbol["evidence"]} == {"server", "client"}
        for symbol in symbols
    )
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_metadata.py",
        "source_parser": SOURCE_PARSER,
        "scope": {
            "surface": "project-baked-remote-calls",
            "ownership": "embedding-project",
            "input_contract": "MetadataBaker .fometa Server/Client outputs",
        },
        "summary": {
            "remote_call_count": len(symbols),
            "remote_calls_by_target": dict(sorted(target_counts.items())),
            "paired_remote_call_count": paired_count,
            "metadata_input_count": len(inputs),
        },
        "metadata_inputs": sorted(inputs, key=lambda item: str(item["id"])),
        "symbols": symbols,
    }


def render_remote_call_model(
    root: Path, metadata_paths: list[Path], *, require_paired: bool = True
) -> str:
    model = generate_remote_call_model(root, metadata_paths, require_paired=require_paired)
    return json.dumps(model, indent=2, ensure_ascii=True) + "\n"


def _text(value: object) -> str:
    result = html.escape(str(value), quote=True)
    result = result.replace("|", "&#124;").replace("{", "&#123;").replace("}", "&#125;")
    return result.replace("\r\n", "<br>").replace("\r", "<br>").replace("\n", "<br>")


def _code(value: object) -> str:
    return f"<code>{_text(value)}</code>"


def _anchor(symbol_id: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", symbol_id.lower()).strip("-")[:72] or "remote-call"
    digest = hashlib.sha256(symbol_id.encode("utf-8")).hexdigest()[:10]
    return f"symbol-{slug}-{digest}"


def render_remote_call_markdown(model: dict[str, Any]) -> str:
    if model.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("Unsupported project remote-call model schema version")
    symbols = model.get("symbols")
    if not isinstance(symbols, list) or any(not isinstance(symbol, dict) for symbol in symbols):
        raise ValueError("Project remote-call model symbols must be an array of objects")

    lines = [
        "---",
        "title: Project Remote Calls",
        "document_id: project-remote-calls",
        "locale: en",
        "generated: true",
        "---",
        "",
        "# Project Remote Calls",
        "",
        "> Generated from baked MetadataBaker output. Do not edit this page directly and do not treat "
        "project-owned calls as FOnline engine compatibility promises.",
        "",
        f"This page contains **{len(symbols)}** paired remote-call contracts.",
        "",
        "`Source hint` is the file name retained by runtime metadata for namespace binding. The baked format "
        "does not preserve a repository-relative declaration path or line number.",
        "",
        "## Metadata inputs",
        "",
        "| Side | Input | SHA-256 |",
        "| --- | --- | --- |",
    ]
    for metadata_input in model.get("metadata_inputs", []):
        lines.append(
            f"| {_text(metadata_input['side'])} | {_code(metadata_input['path'])} | "
            f"{_code(metadata_input['sha256'])} |"
        )
    lines.append("")

    for target in ("server", "client"):
        target_symbols = [symbol for symbol in symbols if symbol.get("target") == target]
        lines.extend(
            [
                f"## {target.title()} target",
                "",
                "| Declaration | Symbol ID | Caller surfaces | Handler | Source hint | Evidence |",
                "| --- | --- | --- | --- | --- | --- |",
            ]
        )
        for symbol in target_symbols:
            evidence = ", ".join(
                f"{item['metadata_side']}/{item['direction']}" for item in symbol["evidence"]
            )
            callers = "<br>".join(_code(caller) for caller in symbol["caller_surfaces"])
            handler = f"{_code('[[' + symbol['handler_attribute'] + ']]')}<br>{_code(symbol['handler_signature'])}"
            lines.append(
                f"| {_code(symbol['signature'])} | <a id=\"{_anchor(str(symbol['id']))}\"></a>"
                f"{_code(symbol['id'])} | {callers} | {handler} | {_code(symbol['source_hint'])} | "
                f"{_text(evidence)} |"
            )
        if not target_symbols:
            lines.append("| - | - | - | - | - | - |")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def _resolve_output(root: Path, value: str) -> Path:
    output = (root / value).resolve()
    try:
        output.relative_to(root)
    except ValueError as exception:
        raise ValueError(f"Generated documentation output escapes the project root: {value}") from exception
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate project remote-call documentation from MetadataBaker .fometa outputs"
    )
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--metadata", type=Path, action="append", required=True)
    parser.add_argument("--json-output", default=DEFAULT_JSON_OUTPUT)
    parser.add_argument("--markdown-output", default=DEFAULT_MARKDOWN_OUTPUT)
    parser.add_argument("--allow-unpaired", action="store_true")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        model = generate_remote_call_model(root, args.metadata, require_paired=not args.allow_unpaired)
        json_content = json.dumps(model, indent=2, ensure_ascii=True) + "\n"
        markdown_content = render_remote_call_markdown(model)
        json_output = _resolve_output(root, args.json_output)
        markdown_output = _resolve_output(root, args.markdown_output)
    except (OSError, MetadataDecodeError, ValueError) as exception:
        print(f"Unable to generate project remote-call documentation: {exception}", file=sys.stderr)
        return 1

    outputs = {json_output: json_content, markdown_output: markdown_content}
    if args.write:
        for output, content in outputs.items():
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(content, encoding="utf-8", newline="\n")
        print(f"Wrote project remote-call JSON and Markdown for {len(model['symbols'])} calls")
        return 0

    stale = [
        output.relative_to(root).as_posix()
        for output, content in outputs.items()
        if not output.is_file() or output.read_text(encoding="utf-8") != content
    ]
    if stale:
        print(
            "Project remote-call documentation is missing or stale: "
            + ", ".join(stale)
            + "; rerun BuildTools/docs_metadata.py --write with the same metadata inputs",
            file=sys.stderr,
        )
        return 1

    print(f"Project remote-call documentation is current: {len(model['symbols'])} calls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
