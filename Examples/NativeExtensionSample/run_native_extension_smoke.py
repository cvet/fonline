#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


REQUIRED_MARKERS = (
    "native_extension_hook_initialized",
    "native_extension_value=42",
    "native_extension_smoke_passed",
)
TIMEOUT_SECONDS = 60


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: run_native_extension_smoke.py <server-executable> <main-config>", file=sys.stderr)
        return 2

    server = Path(sys.argv[1]).resolve()
    config = Path(sys.argv[2]).resolve()
    command = [str(server), "-ApplyConfig", str(config), "-ApplySubConfig", "NativeExtensionSmoke"]

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
        sys.stdout.write(output)
        print(f"[native-extension-smoke] timed out after {TIMEOUT_SECONDS} seconds", file=sys.stderr)
        return 1

    sys.stdout.write(result.stdout)
    if result.returncode != 0:
        print(f"[native-extension-smoke] server exited with code {result.returncode}", file=sys.stderr)
        return result.returncode

    missing = [marker for marker in REQUIRED_MARKERS if marker not in result.stdout]
    if missing:
        print(f"[native-extension-smoke] missing required markers: {', '.join(missing)}", file=sys.stderr)
        return 1

    print("[native-extension-smoke] lifecycle hook, script export, and shutdown verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
