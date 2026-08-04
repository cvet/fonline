#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "Engine" / "BuildTools"))

import gameplay_test_runner  # noqa: E402


PACKAGE_ID = "Tutorial"
PACKAGE_CONFIG = "TutorialSmoke"
DEV_NAME = "FOMM"
NICE_NAME = "FOnline Minimal Multiplayer"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def payload_inventory(root: Path) -> list[dict[str, object]]:
    return [
        {
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": sha256(path),
        }
        for path in sorted(root.rglob("*"))
        if path.is_file()
    ]


def archive_members(path: Path) -> list[str]:
    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            return sorted(name for name in archive.namelist() if not name.endswith("/"))
    with tarfile.open(path, "r:gz") as archive:
        return sorted(member.name for member in archive.getmembers() if member.isfile())


def verify_archive(payload_root: Path, archive_path: Path) -> None:
    expected = sorted(
        path.relative_to(payload_root).as_posix()
        for path in payload_root.rglob("*")
        if path.is_file()
    )
    prefix = f"{payload_root.name}/"
    actual = sorted(
        name[len(prefix) :] if name.startswith(prefix) else name
        for name in archive_members(archive_path)
    )
    if actual != expected:
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        raise RuntimeError(f"archive inventory mismatch for {archive_path}: missing={missing}, extra={extra}")


def git_revision(engine_root: Path) -> str:
    result = subprocess.run(
        ["git", "-C", str(engine_root), "rev-parse", "HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        check=True,
    )
    return result.stdout.strip()


def verify(package_root: Path, engine_root: Path, platform: str) -> dict[str, object]:
    if platform not in {"Windows", "Linux"}:
        raise ValueError(f"unsupported tutorial package platform: {platform}")
    platform_suffix = "" if platform == "Windows" else "-Linux"
    archive_suffix = ".zip" if platform == "Windows" else ".tar.gz"
    executable_suffix = ".exe" if platform == "Windows" else ""
    client_root = package_root / f"{DEV_NAME}-Client-{PACKAGE_CONFIG}{platform_suffix}"
    server_root = package_root / f"{DEV_NAME}-Server-{PACKAGE_CONFIG}{platform_suffix}"
    client_archive = Path(str(client_root) + archive_suffix)
    server_archive = Path(str(server_root) + archive_suffix)

    required_paths = [client_root, server_root, client_archive, server_archive]
    missing = [str(path) for path in required_paths if not path.exists()]
    if missing:
        raise RuntimeError(f"missing package outputs: {', '.join(missing)}")

    client = client_root / f"{NICE_NAME}_Headless{executable_suffix}"
    server = server_root / f"{DEV_NAME}_ServerHeadless{executable_suffix}"
    for binary in (client, server):
        if not binary.is_file():
            raise RuntimeError(f"missing packaged role binary: {binary}")
        if platform == "Linux" and not os.access(binary, os.X_OK):
            raise RuntimeError(f"packaged Linux binary is not executable: {binary}")

    verify_archive(client_root, client_archive)
    verify_archive(server_root, server_archive)

    runtime_manifest = gameplay_test_runner.load_manifest(ROOT / "package-smoke.json")
    runtime_report = gameplay_test_runner.run_manifest(
        runtime_manifest,
        {
            "server": str(server),
            "server_root": str(server_root),
            "client": str(client),
            "client_root": str(client_root),
        },
    )
    runtime_report_path = package_root / "tutorial-package-runtime-report.json"
    gameplay_test_runner.write_report(runtime_report_path, runtime_report)
    if runtime_report["status"] != "passed":
        raise RuntimeError(f"packaged gameplay smoke failed; report: {runtime_report_path}")

    artifacts = [
        {"path": path.name, "size": path.stat().st_size, "sha256": sha256(path)}
        for path in (client_archive, server_archive)
    ]
    manifest = {
        "schema_version": 1,
        "package_id": PACKAGE_ID,
        "platform": platform,
        "engine_revision": git_revision(engine_root),
        "artifacts": artifacts,
        "payloads": {
            "client": payload_inventory(client_root),
            "server": payload_inventory(server_root),
        },
        "runtime_report": runtime_report,
        "role_presence": [client.name, server.name],
        "limitations": [
            "unsigned native archive fixture",
            "headless renderer and audio-disabled gameplay acceptance",
            "no installer, store, deployment, persistence, or rollback qualification",
        ],
    }
    manifest_path = package_root / "tutorial-packaging-manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"Tutorial package passed; evidence: {manifest_path}")
    return manifest


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Verify Minimal Multiplayer package archives and runtime behavior.")
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--platform", choices=("Windows", "Linux"), required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        verify(args.package_root.resolve(), args.engine_root.resolve(), args.platform)
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as error:
        print(f"[tutorial-package] {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
