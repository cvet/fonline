#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import os
import platform
import struct
import subprocess
import sys
import zlib
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parent
CAPTURE_SAMPLE_COUNT = 12
sys.path.insert(0, str(ROOT / "Engine" / "BuildTools"))

import gameplay_test_runner  # noqa: E402


def read_tga(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if len(data) < 18:
        raise ValueError("capture TGA is truncated")
    id_length, color_map_type, image_type = struct.unpack_from("<BBB", data, 0)
    width, height = struct.unpack_from("<HH", data, 12)
    depth = data[16]
    descriptor = data[17]
    if color_map_type != 0 or image_type != 2 or depth not in (24, 32) or width <= 0 or height <= 0:
        raise ValueError("capture must be an uncompressed 24-bit or 32-bit true-color TGA")
    bytes_per_pixel = depth // 8
    offset = 18 + id_length
    expected = width * height * bytes_per_pixel
    if offset + expected != len(data):
        raise ValueError("capture TGA payload size does not match its header")

    rows: list[bytes] = []
    top_origin = bool(descriptor & 0x20)
    for logical_y in range(height):
        source_y = logical_y if top_origin else height - logical_y - 1
        row_offset = offset + source_y * width * bytes_per_pixel
        row = bytearray()
        for x in range(width):
            pixel_offset = row_offset + x * bytes_per_pixel
            blue, green, red = data[pixel_offset : pixel_offset + 3]
            alpha = data[pixel_offset + 3] if bytes_per_pixel == 4 else 255
            row.extend((red, green, blue, alpha))
        rows.append(bytes(row))
    return width, height, b"".join(rows)


def png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    checksum = zlib.crc32(chunk_type)
    checksum = zlib.crc32(payload, checksum)
    return struct.pack(">I", len(payload)) + chunk_type + payload + struct.pack(">I", checksum & 0xFFFFFFFF)


def write_png(path: Path, width: int, height: int, rgba: bytes) -> None:
    stride = width * 4
    scanlines = b"".join(b"\x00" + rgba[y * stride : (y + 1) * stride] for y in range(height))
    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(scanlines, level=9))
        + png_chunk(b"IEND", b"")
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def verify_pixels(width: int, height: int, rgba: bytes) -> dict[str, int]:
    if (width, height) != (1280, 800):
        raise ValueError(f"capture size is {width}x{height}, expected 1280x800")
    pixels = [rgba[index : index + 4] for index in range(0, len(rgba), 4)]
    visible = sum(pixel[3] > 0 for pixel in pixels)
    bright = sum(max(pixel[:3]) >= 96 for pixel in pixels)
    unique = len({pixel for pixel in pixels[::97]})
    if visible < width * height * 0.95:
        raise ValueError("capture has too few visible pixels")
    if bright < width * height * 0.03:
        raise ValueError("capture is effectively blank or black")
    if unique < 48:
        raise ValueError("capture has insufficient sampled color diversity")

    def bright_region(left: int, top: int, right: int, bottom: int) -> int:
        count = 0
        for y in range(top, bottom):
            row = y * width * 4
            for x in range(left, right):
                offset = row + x * 4
                if max(rgba[offset : offset + 3]) >= 80:
                    count += 1
        return count

    regions = {
        "header_bright_pixels": bright_region(32, 24, 720, 112),
        "runtime_bright_pixels": bright_region(320, 150, 980, 570),
        "gallery_bright_pixels": bright_region(24, 540, 1220, 738),
        "footer_bright_pixels": bright_region(24, 750, 900, 792),
    }
    minimums = {
        "header_bright_pixels": 120,
        "runtime_bright_pixels": 1200,
        "gallery_bright_pixels": 4000,
        "footer_bright_pixels": 80,
    }
    failed = [name for name, minimum in minimums.items() if regions[name] < minimum]
    if failed:
        evidence = ", ".join(f"{name}={regions[name]}" for name in failed)
        raise ValueError(f"capture is missing required composed regions: {evidence}")
    return {
        "visible_pixels": visible,
        "bright_pixels": bright,
        "sampled_unique_colors": unique,
        **regions,
    }


def engine_revision() -> str:
    result = subprocess.run(
        ["git", "-C", str(ROOT / "Engine"), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    )
    revision = result.stdout.strip()
    if len(revision) != 40:
        raise ValueError("unable to resolve exact Engine revision")
    return revision


def source_digest() -> str:
    paths = [
        ROOT / "Content/ShowcaseContent.fopro",
        ROOT / "Maps/ShowcaseMap.fomap",
        ROOT / "Scripts/Showcase.fos",
        ROOT / "quality/performance-budget.json",
        *sorted((ROOT / "ShowcaseAssets/Showcase").rglob("*")),
    ]
    digest = hashlib.sha256()
    for path in paths:
        if not path.is_file():
            continue
        relative = path.relative_to(ROOT).as_posix().encode("utf-8")
        data = path.read_bytes()
        digest.update(struct.pack("<I", len(relative)))
        digest.update(relative)
        digest.update(struct.pack("<Q", len(data)))
        digest.update(data)
    return digest.hexdigest()


def capture_profile() -> tuple[str, str, str, str]:
    system = platform.system()
    if system == "Windows":
        return "windows-direct3d11", "ShowcaseCapture", "windows-direct3d11.png", "cmake --build --preset windows-capture"
    if system == "Linux":
        return "linux-opengl", "ShowcaseCaptureOpenGL", "linux-opengl.png", "cmake --build --preset linux-capture"
    raise ValueError(f"unsupported native capture host: {system}")


def update_contract(
    output: Path,
    statistics: dict[str, int],
    selected_source: str,
    samples_checked: int,
    profile_id: str,
    command: str,
) -> None:
    contract_path = ROOT / "captures/capture-contract.json"
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    for profile in contract["profiles"]:
        if profile["id"] != profile_id:
            continue
        ci_run_id = os.environ.get("GITHUB_RUN_ID")
        profile.update(
            {
                "status": "observed-ci" if ci_run_id else "observed-local",
                "captured_on": date.today().isoformat(),
                "engine_revision": engine_revision(),
                "source_sha256": source_digest(),
                "command": command,
                "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                "pixel_evidence": statistics,
                "selected_source": selected_source,
                "samples_checked": samples_checked,
            }
        )
        if ci_run_id:
            profile["ci"] = {
                "repository": os.environ.get("GITHUB_REPOSITORY", ""),
                "commit": os.environ.get("GITHUB_SHA", ""),
                "run_id": ci_run_id,
                "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", ""),
            }
        break
    else:
        raise ValueError(f"capture profile is missing from the contract: {profile_id}")
    contract_path.write_text(json.dumps(contract, indent=2) + "\n", encoding="utf-8", newline="\n")


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: capture_showcase.py <server> <client> <main-config>", file=sys.stderr)
        return 2
    server, client, config = (Path(value).resolve() for value in sys.argv[1:])
    try:
        profile_id, capture_subconfig, output_name, command = capture_profile()
    except ValueError as error:
        print(f"[content-showcase] capture validation failed: {error}", file=sys.stderr)
        return 2
    capture_tgas = [Path.cwd() / f"ShowcaseCapture_{index}.tga" for index in range(CAPTURE_SAMPLE_COUNT)]
    for capture_tga in capture_tgas:
        capture_tga.unlink(missing_ok=True)
    result = gameplay_test_runner.main(
        [
            "--manifest",
            str(ROOT / "showcase-capture.json"),
            "--value",
            f"server={server}",
            "--value",
            f"client={client}",
            "--value",
            f"config={config}",
            "--value",
            f"capture_subconfig={capture_subconfig}",
            "--report",
            str(ROOT / "Workspace/showcase-capture-report.json"),
        ]
    )
    if result != 0:
        return result
    try:
        best: tuple[int, Path, int, int, bytes, dict[str, int]] | None = None
        rejected: list[str] = []
        for capture_tga in capture_tgas:
            try:
                width, height, rgba = read_tga(capture_tga)
                statistics = verify_pixels(width, height, rgba)
            except (OSError, ValueError) as error:
                rejected.append(f"{capture_tga.name}: {error}")
                continue
            score = sum(value for name, value in statistics.items() if name.endswith("_bright_pixels"))
            candidate = (score, capture_tga, width, height, rgba, statistics)
            if best is None or score > best[0]:
                best = candidate
        if best is None:
            details = "; ".join(rejected)
            raise ValueError(f"none of the {CAPTURE_SAMPLE_COUNT} capture samples is complete: {details}")
        _, selected, width, height, rgba, statistics = best
        output = ROOT / "captures" / output_name
        write_png(output, width, height, rgba)
        update_contract(output, statistics, selected.name, len(capture_tgas), profile_id, command)
    except (OSError, ValueError, KeyError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print(f"[content-showcase] capture validation failed: {error}", file=sys.stderr)
        return 1
    print(f"[content-showcase] capture passed: {output} from {selected.name} ({statistics})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
