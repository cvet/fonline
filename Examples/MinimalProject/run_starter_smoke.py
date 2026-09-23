#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import struct
import sys
from pathlib import Path


REQUIRED_MARKERS = (
    "starter_native_extension_value=42",
    "starter_server_started",
    "starter_smoke_passed",
)
REQUIRED_REMOTE_CALL_IDS = {
    "script.remote-call.client.StarterNotice",
    "script.remote-call.server.StarterPing",
}
TIMEOUT_SECONDS = 60


def write_process_output(output: str) -> None:
    encoding = sys.stdout.encoding or "utf-8"
    safe_output = output.encode(encoding, errors="replace").decode(encoding)
    sys.stdout.write(safe_output)


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


def verify_remote_call_metadata(metadata_paths: list[Path]) -> bool:
    calls: dict[str, list[tuple[str, tuple[str, ...]]]] = {}
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
    except (OSError, UnicodeDecodeError, ValueError, struct.error) as error:
        print(f"[starter-smoke] unable to verify baked remote-call metadata: {error}", file=sys.stderr)
        return False

    invalid_ids = []
    for call_id in sorted(REQUIRED_REMOTE_CALL_IDS):
        evidence = calls.get(call_id, [])
        sides = {side for side, _ in evidence}
        contracts = {contract for _, contract in evidence}
        if sides != {"server", "client"} or len(evidence) != 2 or len(contracts) != 1:
            invalid_ids.append(call_id)
    if invalid_ids:
        print(
            f"[starter-smoke] missing or inconsistent baked remote-call contracts: {', '.join(invalid_ids)}",
            file=sys.stderr,
        )
        return False

    print("[starter-smoke] baked remote-call metadata verified")
    return True


def main() -> int:
    if len(sys.argv) != 5:
        print(
            "usage: run_starter_smoke.py <server-executable> <main-config> "
            "<server-metadata> <client-metadata>",
            file=sys.stderr,
        )
        return 2

    server = Path(sys.argv[1]).resolve()
    config = Path(sys.argv[2]).resolve()
    metadata_paths = [Path(value).resolve() for value in sys.argv[3:]]
    command = [str(server), "-ApplyConfig", str(config), "-ApplySubConfig", "StarterSmoke"]

    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        write_process_output(output)
        print(f"[starter-smoke] server timed out after {TIMEOUT_SECONDS} seconds", file=sys.stderr)
        return 1

    write_process_output(result.stdout)

    if result.returncode != 0:
        print(f"[starter-smoke] server exited with code {result.returncode}", file=sys.stderr)
        return result.returncode

    missing = [marker for marker in REQUIRED_MARKERS if marker not in result.stdout]
    if missing:
        print(f"[starter-smoke] missing required markers: {', '.join(missing)}", file=sys.stderr)
        return 1

    print("[starter-smoke] required lifecycle markers found")
    if not verify_remote_call_metadata(metadata_paths):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
