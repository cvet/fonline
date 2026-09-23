#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import os
import socket
import struct
import subprocess
import sys
import time
import zlib
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def wait_for_port(port: int, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.25):
                return
        except OSError:
            time.sleep(0.1)
    raise TimeoutError(f"port {port} did not become ready")


def wait_for_marker(path: Path, marker: str, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        text = path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""
        if marker in text:
            return text
        time.sleep(0.1)
    raise TimeoutError(f"marker {marker!r} did not appear in {path}")


def stop_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=10)


def read_png(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("capture is not a PNG")
    offset = 8
    width = height = color_type = 0
    compressed = bytearray()
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError("capture PNG is truncated")
        length = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if chunk_type == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if depth != 8 or color_type not in (2, 6) or compression or filtering or interlace:
                raise ValueError("capture PNG uses an unsupported pixel format")
        elif chunk_type == b"IDAT":
            compressed.extend(payload)
        elif chunk_type == b"IEND":
            break
    channels = 3 if color_type == 2 else 4
    stride = width * channels
    decoded = zlib.decompress(bytes(compressed))
    if len(decoded) != height * (stride + 1):
        raise ValueError("capture PNG scanline size is invalid")

    def paeth(left: int, above: int, upper_left: int) -> int:
        prediction = left + above - upper_left
        left_distance = abs(prediction - left)
        above_distance = abs(prediction - above)
        upper_left_distance = abs(prediction - upper_left)
        if left_distance <= above_distance and left_distance <= upper_left_distance:
            return left
        return above if above_distance <= upper_left_distance else upper_left

    rows: list[bytearray] = []
    cursor = 0
    for _ in range(height):
        filter_type = decoded[cursor]
        cursor += 1
        source = decoded[cursor : cursor + stride]
        cursor += stride
        row = bytearray(stride)
        above = rows[-1] if rows else bytearray(stride)
        for index, value in enumerate(source):
            left = row[index - channels] if index >= channels else 0
            upper = above[index]
            upper_left = above[index - channels] if index >= channels else 0
            if filter_type == 0:
                result = value
            elif filter_type == 1:
                result = value + left
            elif filter_type == 2:
                result = value + upper
            elif filter_type == 3:
                result = value + ((left + upper) // 2)
            elif filter_type == 4:
                result = value + paeth(left, upper, upper_left)
            else:
                raise ValueError(f"capture PNG uses unknown filter {filter_type}")
            row[index] = result & 0xFF
        rows.append(row)

    rgba = bytearray()
    for row in rows:
        for index in range(0, len(row), channels):
            rgba.extend(row[index : index + 3])
            rgba.append(row[index + 3] if channels == 4 else 255)
    return width, height, bytes(rgba)


def update_capture_contract(
    output: Path,
    report: dict[str, object],
    package_dir: Path,
) -> None:
    sys.path.insert(0, str(ROOT))
    from capture_showcase import engine_revision, source_digest

    contract_path = ROOT / "captures/capture-contract.json"
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    package_digest = hashlib.sha256()
    for package_file in sorted(path for path in package_dir.rglob("*") if path.is_file()):
        relative = package_file.relative_to(package_dir).as_posix().encode("utf-8")
        data = package_file.read_bytes()
        package_digest.update(len(relative).to_bytes(4, "little"))
        package_digest.update(relative)
        package_digest.update(len(data).to_bytes(8, "little"))
        package_digest.update(data)
    for profile in contract["profiles"]:
        if profile["id"] != "web-webgl2":
            continue
        profile.update(
            {
                "status": "observed-local",
                "captured_on": date.today().isoformat(),
                "engine_revision": engine_revision(),
                "source_sha256": source_digest(),
                "command": "python validate.py --web-runtime",
                "package_sha256": package_digest.hexdigest(),
                "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                "pixel_evidence": report["pixel_evidence"],
                "browser_version": report["browser_version"],
                "renderer": report["renderer"],
                "webgl_version": report["version"],
                "shading_language_version": report["shading_language_version"],
            }
        )
        break
    contract_path.write_text(json.dumps(contract, indent=2) + "\n", encoding="utf-8", newline="\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run and capture the packaged Content Showcase Web client.")
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--package-dir", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--playwright-root", type=Path, default=ROOT / "WebTests")
    parser.add_argument("--output", type=Path, default=ROOT / "Workspace/web-webgl2.png")
    parser.add_argument(
        "--report",
        type=Path,
        default=ROOT / "Workspace/showcase-web-runtime-report.json",
    )
    parser.add_argument(
        "--update-contract",
        action="store_true",
        help="Replace the checked capture and update captures/capture-contract.json.",
    )
    args = parser.parse_args(argv)

    contract = json.loads((ROOT / "showcase-web-runtime.json").read_text(encoding="utf-8"))
    package_dir = args.package_dir.resolve()
    server = args.server.resolve()
    config = args.config.resolve()
    playwright_root = args.playwright_root.resolve()
    output = args.output.resolve()
    browser_report = args.report.resolve()
    checked_capture = (ROOT / "captures/web-webgl2.png").resolve()
    if args.update_contract and output != checked_capture:
        print("--update-contract requires --output captures/web-webgl2.png", file=sys.stderr)
        return 2
    for path, description in (
        (server, "native server"),
        (package_dir / "index.html", "Web package"),
        (config, "showcase config"),
        (playwright_root / "node_modules/playwright/package.json", "Playwright installation"),
    ):
        if not path.is_file():
            print(f"Content Showcase Web runtime failed: missing {description}: {path}", file=sys.stderr)
            return 2
    output_root = package_dir.parents[1]
    browser_report.parent.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    server_log = output_root / f"{server.stem}.log"
    web_log = browser_report.parent / "showcase-web-server.log"
    for path in (server_log, web_log, browser_report):
        path.unlink(missing_ok=True)

    creation_flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    with web_log.open("wb") as web_stream:
        server_process = subprocess.Popen(
            [str(server), "-ApplyConfig", str(config), "-ApplySubConfig", "ShowcaseRelease"],
            cwd=output_root,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
            creationflags=creation_flags,
        )
        web_process: subprocess.Popen[bytes] | None = None
        try:
            wait_for_marker(server_log, "showcase_server_started", 45)
            web_process = subprocess.Popen(
                [sys.executable, "web-server.py", "--port", str(contract["http_port"])],
                cwd=package_dir,
                stdout=web_stream,
                stderr=subprocess.STDOUT,
                creationflags=creation_flags,
            )
            wait_for_port(int(contract["http_port"]), 20)
            url = (
                f"http://127.0.0.1:{contract['http_port']}/"
                f"?ClientNetwork.WebSocketHost=127.0.0.1"
                f"&Network.WebSocketPort={contract['websocket_port']}"
            )
            result = subprocess.run(
                [
                    "node",
                    str(ROOT / "capture_showcase_web.mjs"),
                    url,
                    str(output),
                    str(browser_report),
                    str(playwright_root),
                    str(ROOT / "showcase-web-runtime.json"),
                ],
                cwd=ROOT,
                check=False,
                timeout=int(contract["timeout_seconds"]) + 30,
            )
            if result.returncode != 0:
                return result.returncode
            server_text = wait_for_marker(server_log, "showcase_server_world_ready", 10)
            missing_server = [marker for marker in contract["required_server_markers"] if marker not in server_text]
            forbidden_server = [marker for marker in contract["forbidden_markers"] if marker in server_text]
            if missing_server or forbidden_server:
                print(
                    f"Content Showcase Web server evidence failed: missing={missing_server}, forbidden={forbidden_server}",
                    file=sys.stderr,
                )
                return 1
            report = json.loads(browser_report.read_text(encoding="utf-8"))
            from capture_showcase import verify_pixels

            width, height, rgba = read_png(output)
            report["pixel_evidence"] = verify_pixels(width, height, rgba)
            report["server_markers"] = {
                marker: marker in server_text for marker in contract["required_server_markers"]
            }
            browser_report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            if args.update_contract:
                update_capture_contract(output, report, package_dir)
            print(f"Content Showcase Web runtime passed: {output}")
            return 0
        except (OSError, TimeoutError, subprocess.TimeoutExpired, KeyError, ValueError, json.JSONDecodeError) as error:
            print(f"Content Showcase Web runtime failed: {error}", file=sys.stderr)
            return 1
        finally:
            if web_process is not None:
                stop_process(web_process)
            stop_process(server_process)


if __name__ == "__main__":
    raise SystemExit(main())
