#!/usr/bin/env python3

from __future__ import annotations

import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "Engine" / "BuildTools"))

import gameplay_test_runner  # noqa: E402


REQUIRED_REMOTE_CALL_IDS = {
    "script.remote-call.server.EnterWorld",
    "script.remote-call.server.CollectSupply",
    "script.remote-call.server.FinishSmoke",
    "script.remote-call.client.WorldReady",
    "script.remote-call.client.SupplyCollected",
    "script.remote-call.client.SmokeComplete",
}


def decode_metadata(data: bytes) -> dict[str, list[list[str]]]:
    view = memoryview(data)
    offset = 0

    def read(format_string: str, label: str) -> int:
        nonlocal offset
        size = struct.calcsize(format_string)
        if offset + size > len(view):
            raise ValueError(f"truncated metadata while reading {label}")
        value = struct.unpack_from(format_string, view, offset)[0]
        offset += size
        return int(value)

    def read_text(size: int, label: str) -> str:
        nonlocal offset
        if offset + size > len(view):
            raise ValueError(f"truncated metadata while reading {label}")
        raw = view[offset : offset + size].tobytes()
        offset += size
        return raw.decode("utf-8")

    sections: dict[str, list[list[str]]] = {}
    for section_index in range(read("<H", "section count")):
        name = read_text(read("<H", f"section {section_index} name length"), f"section {section_index} name")
        if not name or name in sections:
            raise ValueError(f"invalid or duplicate metadata section: {name!r}")
        entries: list[list[str]] = []
        for entry_index in range(read("<I", f"section {name} entry count")):
            parts = []
            for part_index in range(read("<I", f"section {name} entry {entry_index} part count")):
                part_size = read("<H", f"section {name} entry {entry_index} part {part_index} length")
                parts.append(read_text(part_size, f"section {name} entry {entry_index} part {part_index}"))
            entries.append(parts)
        sections[name] = entries
    if offset != len(view):
        raise ValueError(f"metadata has {len(view) - offset} trailing bytes")
    return sections


def verify_metadata(metadata_paths: list[Path]) -> bool:
    calls: dict[str, list[tuple[str, tuple[str, ...]]]] = {}
    property_sides: set[str] = set()
    try:
        for path in metadata_paths:
            sections = decode_metadata(path.read_bytes())
            target_entries = sections.get("Target")
            if target_entries not in ([["Server"]], [["Client"]]):
                raise ValueError(f"metadata has no unambiguous Server/Client target: {path}")
            metadata_side = target_entries[0][0].lower()
            for entry in sections.get("RemoteCall", []):
                if len(entry) < 3 or (len(entry) - 3) % 3 != 0 or entry[2] not in {"In", "Out"}:
                    raise ValueError(f"malformed RemoteCall entry: {path}")
                target = metadata_side if entry[2] == "In" else ("client" if metadata_side == "server" else "server")
                call_id = f"script.remote-call.{target}.{entry[0]}"
                contract = (target, entry[0], entry[1], *entry[3:])
                calls.setdefault(call_id, []).append((metadata_side, contract))
            if any("SuppliesCollected" in entry for entry in sections.get("Property", [])):
                property_sides.add(metadata_side)
    except (OSError, UnicodeDecodeError, ValueError, struct.error) as error:
        print(f"[tutorial-smoke] unable to verify baked metadata: {error}", file=sys.stderr)
        return False

    invalid_ids = []
    for call_id in sorted(REQUIRED_REMOTE_CALL_IDS):
        evidence = calls.get(call_id, [])
        sides = {side for side, _ in evidence}
        contracts = {contract for _, contract in evidence}
        if sides != {"server", "client"} or len(evidence) != 2 or len(contracts) != 1:
            invalid_ids.append(call_id)
    if invalid_ids:
        print(f"[tutorial-smoke] missing or inconsistent remote calls: {', '.join(invalid_ids)}", file=sys.stderr)
        return False
    if property_sides != {"server", "client"}:
        print("[tutorial-smoke] SuppliesCollected property is not present on both metadata sides", file=sys.stderr)
        return False

    print("[tutorial-smoke] remote calls and replicated persistent property verified")
    return True


def main() -> int:
    if len(sys.argv) != 6:
        print(
            "usage: run_tutorial_smoke.py <server-executable> <client-executable> <main-config> "
            "<server-metadata> <client-metadata>",
            file=sys.stderr,
        )
        return 2

    server = Path(sys.argv[1]).resolve()
    client = Path(sys.argv[2]).resolve()
    config = Path(sys.argv[3]).resolve()
    metadata_paths = [Path(value).resolve() for value in sys.argv[4:]]

    if not verify_metadata(metadata_paths):
        return 1
    return gameplay_test_runner.main(
        [
            "--manifest",
            str(ROOT / "tutorial-smoke.json"),
            "--value",
            f"server={server}",
            "--value",
            f"client={client}",
            "--value",
            f"config={config}",
            "--report",
            str(ROOT / "Workspace/tutorial-smoke-report.json"),
        ]
    )


if __name__ == "__main__":
    raise SystemExit(main())
