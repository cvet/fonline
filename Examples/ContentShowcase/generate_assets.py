#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
import sys
import wave
from io import BytesIO
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def tga(width: int, height: int, pixel) -> bytes:
    header = struct.pack(
        "<BBBHHBHHHHBB",
        0,
        0,
        2,
        0,
        0,
        0,
        0,
        0,
        width,
        height,
        32,
        0x28,
    )
    body = bytearray()
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixel(x, y)
            body.extend((blue, green, red, alpha))
    return header + bytes(body)


def floor_tile() -> bytes:
    width, height = 128, 64
    center_x, center_y = (width - 1) / 2.0, (height - 1) / 2.0

    def pixel(x: int, y: int) -> tuple[int, int, int, int]:
        nx = abs(x - center_x) / 63.5
        ny = abs(y - center_y) / 31.5
        distance = nx + ny
        if distance > 1.0:
            return 0, 0, 0, 0
        edge = distance > 0.93
        seam = abs(((x + y * 2) % 24) - 12) < 1
        glow = max(0.0, 1.0 - distance)
        if edge:
            return 68, 118, 126, 255
        if seam:
            return 48, 82, 88, 255
        return int(27 + 13 * glow), int(43 + 18 * glow), int(48 + 20 * glow), 255

    return tga(width, height, pixel)


def panel() -> bytes:
    width, height = 180, 110

    def pixel(x: int, y: int) -> tuple[int, int, int, int]:
        if x < 3 or y < 3 or x >= width - 3 or y >= height - 3:
            return 0, 0, 0, 0
        border = x < 8 or y < 8 or x >= width - 8 or y >= height - 8
        if border:
            return 70, 181, 165, 255
        stripe = y in (28, 52, 76) and 18 <= x < width - 18
        marker = 18 <= x < 35 and 20 <= y < 88
        if stripe:
            return 92, 211, 189, 255
        if marker:
            return 232, 174, 76, 255
        return 18, 34, 39, 242

    return tga(width, height, pixel)


def beacon_frame(frame: int) -> bytes:
    width, height = 96, 128
    phase = frame / 6.0 * math.tau

    def pixel(x: int, y: int) -> tuple[int, int, int, int]:
        center_x = 47.5 + math.sin(phase) * 2.0
        glow_y = 31.0
        radius = math.hypot(x - center_x, y - glow_y)
        pulse = 19.0 + 4.0 * math.sin(phase)
        if radius < pulse:
            alpha = int(max(0.0, 1.0 - radius / pulse) * 210)
            return 238, 186, 74, alpha
        if 39 <= x <= 56 and 37 <= y <= 101:
            edge = x in (39, 40, 55, 56) or y in (37, 38, 100, 101)
            return (101, 201, 183, 255) if edge else (28, 58, 61, 255)
        if 27 <= x <= 68 and 99 <= y <= 110:
            return 64, 91, 91, 255
        if 35 <= x <= 60 and 52 <= y <= 58:
            lit = x < 35 + (frame + 1) * 4
            return (242, 181, 65, 255) if lit else (46, 83, 82, 255)
        return 0, 0, 0, 0

    return tga(width, height, pixel)


def spark() -> bytes:
    width = height = 32

    def pixel(x: int, y: int) -> tuple[int, int, int, int]:
        radius = math.hypot(x - 15.5, y - 15.5) / 15.5
        if radius >= 1.0:
            return 0, 0, 0, 0
        alpha = int((1.0 - radius) ** 2 * 255)
        return 104, 224, 202, alpha

    return tga(width, height, pixel)


def chime() -> bytes:
    sample_rate = 44_100
    duration = 0.45
    sample_count = int(sample_rate * duration)
    output = BytesIO()
    with wave.open(output, "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(sample_rate)
        samples = bytearray()
        for index in range(sample_count):
            time = index / sample_rate
            envelope = (1.0 - time / duration) ** 2
            value = (
                math.sin(math.tau * 523.25 * time)
                + 0.45 * math.sin(math.tau * 783.99 * time)
            )
            sample = int(max(-1.0, min(1.0, value * envelope * 0.22)) * 32767)
            samples.extend(struct.pack("<h", sample))
        audio.writeframes(bytes(samples))
    return output.getvalue()


def rendered_assets() -> dict[Path, bytes]:
    assets: dict[Path, bytes] = {
        Path("ShowcaseAssets/Showcase/Tiles/ShowcaseFloor.tga"): floor_tile(),
        Path("ShowcaseAssets/Showcase/Sprites/ShowcasePanel.tga"): panel(),
        Path("ShowcaseAssets/Showcase/Particles/ShowcaseSpark.tga"): spark(),
        Path("ShowcaseAssets/Showcase/Audio/ShowcaseChime.wav"): chime(),
    }
    for frame in range(6):
        assets[Path(f"ShowcaseAssets/Showcase/Sprites/Beacon_{frame}.tga")] = beacon_frame(frame)
    return assets


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate deterministic Content Showcase assets.")
    parser.add_argument("--check", action="store_true", help="Fail if generated assets are stale.")
    args = parser.parse_args(argv)

    stale: list[str] = []
    for relative_path, data in rendered_assets().items():
        path = ROOT / relative_path
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                stale.append(relative_path.as_posix())
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(f"Wrote {relative_path.as_posix()}")

    if stale:
        print(f"stale generated showcase assets: {', '.join(stale)}", file=sys.stderr)
        return 1
    if args.check:
        print(f"Content Showcase assets are current: {len(rendered_assets())} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
