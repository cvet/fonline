#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import wave
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def fail(message: str) -> None:
    raise ValueError(message)


def load_json(relative_path: str) -> dict[str, object]:
    value = json.loads((ROOT / relative_path).read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("schema_version") != 1:
        fail(f"invalid schema_version in {relative_path}")
    return value


def tga_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 18 or data[2] != 2 or data[16] != 32:
        fail(f"expected uncompressed 32-bit TGA: {path.relative_to(ROOT).as_posix()}")
    width, height = struct.unpack_from("<HH", data, 12)
    if width <= 0 or height <= 0 or len(data) != 18 + width * height * 4:
        fail(f"invalid TGA payload: {path.relative_to(ROOT).as_posix()}")
    if not any(data[index] for index in range(21, len(data), 4)):
        fail(f"TGA has no visible pixels: {path.relative_to(ROOT).as_posix()}")
    return width, height


def verify_provenance(budget: dict[str, object]) -> None:
    provenance = load_json("assets/provenance.json")
    assets = provenance.get("assets")
    if not isinstance(assets, list) or not assets:
        fail("asset provenance must contain assets")
    source_budget = budget["source"]
    assert isinstance(source_budget, dict)
    if len(assets) > int(source_budget["max_provenance_assets"]):
        fail("provenance asset count exceeds budget")

    total_bytes = 0
    paths: set[str] = set()
    for asset in assets:
        if not isinstance(asset, dict):
            fail("asset provenance entry is not an object")
        relative_path = str(asset.get("path", ""))
        if not relative_path or relative_path in paths:
            fail(f"invalid or duplicate provenance path: {relative_path}")
        paths.add(relative_path)
        path = ROOT / relative_path
        if not path.is_file():
            fail(f"provenance asset is missing: {relative_path}")
        if asset.get("license") != "CC0-1.0" or asset.get("source") != "project-original":
            fail(f"showcase asset is not declared project-original CC0: {relative_path}")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != asset.get("sha256"):
            fail(f"provenance digest mismatch: {relative_path}")
        size = path.stat().st_size
        if size > int(source_budget["max_single_asset_bytes"]):
            fail(f"single asset exceeds budget: {relative_path}")
        total_bytes += size
    if total_bytes > int(source_budget["max_total_asset_bytes"]):
        fail("total asset bytes exceed budget")


def verify_sources(budget: dict[str, object]) -> None:
    result = subprocess.run(
        [sys.executable, str(ROOT / "generate_assets.py"), "--check"],
        cwd=ROOT,
        check=False,
    )
    if result.returncode != 0:
        fail("generated assets are stale")
    result = subprocess.run(
        [sys.executable, str(ROOT / "generate_config.py"), "--check"],
        cwd=ROOT,
        check=False,
    )
    if result.returncode != 0:
        fail("generated config is stale")

    expected_tga_sizes = {
        "ShowcaseAssets/Showcase/Tiles/ShowcaseFloor.tga": (128, 64),
        "ShowcaseAssets/Showcase/Sprites/ShowcasePanel.tga": (180, 110),
        "ShowcaseAssets/Showcase/Particles/ShowcaseSpark.tga": (32, 32),
        **{
            f"ShowcaseAssets/Showcase/Sprites/Beacon_{frame}.tga": (96, 128)
            for frame in range(6)
        },
    }
    for relative_path, expected_size in expected_tga_sizes.items():
        if tga_size(ROOT / relative_path) != expected_size:
            fail(f"unexpected TGA dimensions: {relative_path}")

    audio_path = ROOT / "ShowcaseAssets/Showcase/Audio/ShowcaseChime.wav"
    with wave.open(str(audio_path), "rb") as audio:
        if audio.getnchannels() != 1 or audio.getsampwidth() != 2 or audio.getframerate() != 44_100:
            fail("showcase chime format changed")
        duration_ms = audio.getnframes() * 1000 // audio.getframerate()
    source_budget = budget["source"]
    assert isinstance(source_budget, dict)
    if duration_ms > int(source_budget["max_audio_duration_ms"]):
        fail("showcase chime exceeds duration budget")

    descriptor = (ROOT / "ShowcaseAssets/Showcase/Sprites/Beacon.fofrm").read_text(encoding="utf-8")
    if "count = 6" not in descriptor or "fps = 8" not in descriptor:
        fail("beacon FOFRM timing changed")
    if len(re.findall(r"^next_[xy]_\d+\s*=", descriptor, re.MULTILINE)) != 12:
        fail("beacon FOFRM must retain complete NextX/NextY metadata")

    particle = (ROOT / "ShowcaseAssets/Showcase/Particles/Showcase.spark").read_text(encoding="utf-8")
    capacity_match = re.search(r'id="capacity" value="(\d+)"', particle)
    if capacity_match is None or int(capacity_match.group(1)) > int(source_budget["max_particle_capacity"]):
        fail("particle capacity is missing or exceeds budget")
    for required_reference in (
        "Showcase/Effects/ShowcaseParticle.fofx",
        "ShowcaseSpark.tga",
    ):
        if required_reference not in particle:
            fail(f"particle source lost reference: {required_reference}")

    map_source = (ROOT / "Maps/ShowcaseMap.fomap").read_text(encoding="utf-8")
    item_count = len(re.findall(r"^\[ShowcaseMap/Item\]$", map_source, re.MULTILINE))
    if item_count != int(source_budget["max_static_map_items"]):
        fail("showcase map item inventory no longer matches its explicit budget")

    capture = load_json("captures/capture-contract.json")
    profiles = capture.get("profiles")
    if not isinstance(profiles, list) or {profile.get("id") for profile in profiles if isinstance(profile, dict)} != {
        "windows-direct3d11",
        "linux-opengl",
        "web-webgl2",
    }:
        fail("capture contract must define the complete backend matrix")
    for profile in profiles:
        assert isinstance(profile, dict)
        if profile.get("status") != "observed-local":
            continue
        capture_path = ROOT / str(profile.get("output", ""))
        if not capture_path.is_file():
            fail(f"observed capture is missing: {capture_path.relative_to(ROOT).as_posix()}")
        if hashlib.sha256(capture_path.read_bytes()).hexdigest() != profile.get("sha256"):
            fail(f"observed capture digest mismatch: {capture_path.relative_to(ROOT).as_posix()}")
        if not re.fullmatch(r"[0-9a-f]{40}", str(profile.get("engine_revision", ""))):
            fail(f"observed capture has no exact Engine revision: {profile.get('id')}")
        if not re.fullmatch(r"[0-9a-f]{64}", str(profile.get("source_sha256", ""))):
            fail(f"observed capture has no source digest: {profile.get('id')}")
        if not isinstance(profile.get("command"), str) or not str(profile["command"]).strip():
            fail(f"observed capture has no reproduction command: {profile.get('id')}")

        from capture_showcase import verify_pixels
        from capture_showcase_web import read_png

        width, height, rgba = read_png(capture_path)
        if (width, height) != (profile.get("width"), profile.get("height")):
            fail(f"observed capture dimensions do not match the contract: {profile.get('id')}")
        if verify_pixels(width, height, rgba) != profile.get("pixel_evidence"):
            fail(f"observed capture pixel evidence is stale: {profile.get('id')}")

        if profile.get("id") == "web-webgl2":
            if not re.fullmatch(r"[0-9a-f]{64}", str(profile.get("package_sha256", ""))):
                fail("observed Web capture has no package digest")
            for field in ("browser_version", "renderer", "webgl_version", "shading_language_version"):
                if not isinstance(profile.get(field), str) or not str(profile[field]).strip():
                    fail(f"observed Web capture has no {field}")

    verify_provenance(budget)


def verify_web(client: Path) -> None:
    if not client.is_file() or client.stat().st_size == 0:
        fail(f"Web client output is missing or empty: {client}")
    siblings = list(client.parent.glob(f"{client.stem}.*"))
    if not any(path.suffix in {".wasm", ".js", ".html"} and path.stat().st_size > 0 for path in siblings):
        fail(f"Web client has no nonempty wasm/js/html artifact beside {client}")
    print(f"Content Showcase Web output verified: {client}")


def verify_web_package(package_root: Path) -> None:
    contract = load_json("showcase-web-package.json")
    package_dir = package_root / str(contract["payload_directory"])
    if not package_dir.is_dir():
        fail(f"Web package payload directory is missing: {package_dir}")

    required_files = contract.get("required_files")
    if not isinstance(required_files, list) or not required_files:
        fail("Web package contract has no required_files")
    actual_files = {
        path.relative_to(package_dir).as_posix()
        for path in package_dir.rglob("*")
        if path.is_file()
    }
    missing = sorted(set(map(str, required_files)) - actual_files)
    if missing:
        fail(f"Web package is missing required files: {', '.join(missing)}")
    empty = sorted(relative for relative in required_files if (package_dir / str(relative)).stat().st_size == 0)
    if empty:
        fail(f"Web package contains empty required files: {', '.join(map(str, empty))}")

    wasm = (package_dir / "FOCS_Client.wasm").read_bytes()
    if not wasm.startswith(b"\x00asm"):
        fail("packaged FOCS_Client.wasm has no WebAssembly magic")
    resources_data = package_dir / "Resources.data"
    minimum_resource_bytes = int(contract["minimum_resources_data_bytes"])
    if resources_data.stat().st_size < minimum_resource_bytes:
        fail("packaged Resources.data is below the explicit minimum size")

    index = (package_dir / "index.html").read_text(encoding="utf-8")
    for marker in ("Resources.js", "FOCS_Client.js", "FOnline Content Showcase"):
        if marker not in index:
            fail(f"packaged index.html lost marker: {marker}")
    resources_js = (package_dir / "Resources.js").read_text(encoding="utf-8")
    if "Resources.data" not in resources_js or "LZ4" not in resources_js:
        fail("Resources.js does not describe the LZ4 preload payload")

    archive = package_root / f"{contract['payload_directory']}.zip"
    if not archive.is_file() or archive.stat().st_size == 0:
        fail(f"Web package archive is missing: {archive}")
    with zipfile.ZipFile(archive) as package_zip:
        archive_files = {name.rstrip("/") for name in package_zip.namelist() if not name.endswith("/")}
        expected_archive_files = actual_files
        if archive_files != expected_archive_files:
            fail("Web package archive inventory differs from the Raw payload")
        bad_member = package_zip.testzip()
        if bad_member is not None:
            fail(f"Web package archive has a corrupt member: {bad_member}")

    print(
        "Content Showcase Web package verified: "
        f"{len(actual_files)} files, Resources.data={resources_data.stat().st_size} bytes"
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate Content Showcase assets, budgets, and build outputs.")
    parser.add_argument("--mode", choices=("source", "web", "web-package"), default="source")
    parser.add_argument("--client", type=Path)
    parser.add_argument("--package-root", type=Path)
    args = parser.parse_args(argv)
    try:
        budget = load_json("quality/performance-budget.json")
        verify_sources(budget)
        if args.mode == "web":
            if args.client is None:
                fail("--client is required in web mode")
            verify_web(args.client.resolve())
        elif args.mode == "web-package":
            if args.package_root is None:
                fail("--package-root is required in web-package mode")
            verify_web_package(args.package_root.resolve())
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"Content Showcase validation failed: {error}", file=sys.stderr)
        return 1
    print("Content Showcase source and performance contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
