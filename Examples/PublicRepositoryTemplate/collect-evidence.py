#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
MAX_FILE_SIZE = 512 * 1024 * 1024
EVIDENCE_PATTERNS = (
    "Workspace/**/*.json",
    "Workspace/**/*.log",
    "Workspace/**/*.md",
    "Workspace/**/*.png",
    "Workspace/**/*.tga",
    "captures/*.json",
    "captures/*.png",
    "Build/**/Binaries/**/*",
    "Build/**/*packaging-manifest.json",
    "Build/**/*runtime-report.json",
    "Build/**/*.zip",
    "Build/**/*.tar.gz",
    "Build/**/*.wasm",
    "Build/**/*.data",
)


def git(*arguments: str, root: Path = ROOT) -> str:
    return subprocess.run(
        ["git", "-C", str(root), *arguments],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def selected_files(output: Path) -> list[Path]:
    selected = {
        ROOT / "example-repository.json",
        ROOT / "assets/provenance.json",
    }
    for pattern in EVIDENCE_PATTERNS:
        selected.update(path for path in ROOT.glob(pattern) if path.is_file())
    return sorted(path for path in selected if path != output and output not in path.parents)


def main() -> int:
    parser = argparse.ArgumentParser(description="Collect immutable FOnline example validation evidence.")
    parser.add_argument("--mode", choices=("pinned", "current"), required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "Evidence")
    args = parser.parse_args()

    metadata = json.loads((ROOT / "example-repository.json").read_text(encoding="utf-8"))
    pinned_revision = metadata["engine"]["revision"]
    tested_revision = git("rev-parse", "HEAD", root=ROOT / "Engine")
    if args.mode == "pinned" and tested_revision != pinned_revision:
        raise ValueError("pinned evidence Engine revision does not match example-repository.json")

    output = args.output.resolve()
    if output == ROOT or ROOT not in output.parents:
        raise ValueError("evidence output must be a child of the repository root")
    if output.exists():
        shutil.rmtree(output)
    files_root = output / "files"
    files_root.mkdir(parents=True)

    inventory: list[dict[str, object]] = []
    for source in selected_files(output):
        relative = source.relative_to(ROOT)
        size = source.stat().st_size
        if size > MAX_FILE_SIZE:
            raise ValueError(f"evidence file exceeds {MAX_FILE_SIZE} bytes: {relative.as_posix()}")
        destination = files_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        inventory.append(
            {
                "path": relative.as_posix(),
                "size": size,
                "sha256": sha256(source),
            }
        )

    report = {
        "schema_version": 1,
        "repository": metadata["repository"],
        "program_id": metadata["program_id"],
        "mode": args.mode,
        "source_commit": git("rev-parse", "HEAD"),
        "pinned_engine_revision": pinned_revision,
        "tested_engine_revision": tested_revision,
        "github": {
            "repository": os.environ.get("GITHUB_REPOSITORY", ""),
            "run_id": os.environ.get("GITHUB_RUN_ID", ""),
            "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", ""),
            "workflow": os.environ.get("GITHUB_WORKFLOW", ""),
            "job": os.environ.get("GITHUB_JOB", ""),
            "runner_os": os.environ.get("RUNNER_OS", ""),
        },
        "files": inventory,
    }
    (output / "example-evidence.json").write_text(
        json.dumps(report, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"Collected {len(inventory)} evidence files for {metadata['repository']} "
        f"against Engine {tested_revision}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
