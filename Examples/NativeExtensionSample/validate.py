#!/usr/bin/env python3

from __future__ import annotations

import platform
import subprocess
import sys
from pathlib import Path


def main() -> int:
    system = platform.system()
    if system == "Windows":
        preset = "windows"
    elif system == "Linux":
        preset = "linux"
    else:
        print(f"unsupported native extension sample host: {system}", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parent
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
