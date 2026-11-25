#!/usr/bin/env python3

from __future__ import annotations

import argparse
import platform
import subprocess
import sys
from pathlib import Path


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Configure, build, bake and run the mechanics sandbox recipe checks for this host."
    )
    parser.add_argument(
        "--binary-dir",
        help="build tree to use instead of the preset's Build/<preset> directory",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    system = platform.system()
    if system == "Windows":
        preset = "windows"
    elif system == "Linux":
        preset = "linux"
    else:
        print(f"unsupported sandbox host: {system}", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parent
    if args.binary_dir:
        binary_dir = Path(args.binary_dir).resolve()
        commands = (
            ["cmake", "--preset", preset, "-B", str(binary_dir)],
            ["cmake", "--build", str(binary_dir), "--config", "Release", "--target", "RunMechanicsChecks"],
        )
    else:
        commands = (
            ["cmake", "--preset", preset],
            ["cmake", "--build", "--preset", f"{preset}-check"],
        )

    for command in commands:
        result = subprocess.run(command, cwd=root, check=False)
        if result.returncode != 0:
            return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
