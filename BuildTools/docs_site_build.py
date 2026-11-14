from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

import docs_prepare
import documentation_metadata


def site_directory(root: Path, relative_path: str) -> Path:
    destination = (root / relative_path).resolve()
    workspace = (root / "Workspace").resolve()
    if destination == workspace or not destination.is_relative_to(workspace):
        raise ValueError("Documentation site output must be a directory below Workspace")
    return destination


def export_endpoints(root: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    for name in documentation_metadata.ROOT_ENDPOINTS:
        shutil.copyfile(documentation_metadata.output_path(root, name), destination / name)


def build(root: Path, destination: Path, *, external: bool = False) -> None:
    docs_prepare.prepare(root, external=external)
    environment = dict(os.environ)
    environment["JEKYLL_ENV"] = "production"
    gemfile = documentation_metadata.output_path(root, "Gemfile")
    environment["BUNDLE_GEMFILE"] = str(gemfile)
    bundle = shutil.which("bundle") or "bundle"
    subprocess.run(
        [bundle, "exec", "jekyll", "build", "--source", str(root),
         "--config", str(root / documentation_metadata.SITE_CONFIG),
         "--destination", str(destination), "--trace"],
        cwd=gemfile.parent, env=environment, check=True,
    )
    export_endpoints(root, destination)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Build and export the FOnline documentation site")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--site-dir", default=documentation_metadata.SITE_OUTPUT_DIR)
    parser.add_argument("--external", action="store_true", help="also require real shell parsers for snippets")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        build(root, site_directory(root, args.site_dir), external=args.external)
    except (OSError, ValueError, subprocess.CalledProcessError) as exception:
        print(f"Unable to build documentation site: {exception}", file=sys.stderr)
        return 1
    print(f"Documentation site built: {args.site_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
