#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "Engine" / "BuildTools"))

import gameplay_test_runner  # noqa: E402
import secure_channel_key  # noqa: E402


# Throwaway local secure-channel identity that the config reads beside itself; never a production key
CHANNEL_SECRET = Path("Workspace/ShowcaseChannel.key")
CHANNEL_PUBLIC = Path("Workspace/ShowcaseChannel.pub")


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: run_showcase_smoke.py <server> <client> <main-config>", file=sys.stderr)
        return 2
    server, client, config = (Path(value).resolve() for value in sys.argv[1:])
    secure_channel_key.ensure_key_pair(config.parent / CHANNEL_SECRET, config.parent / CHANNEL_PUBLIC)
    return gameplay_test_runner.main(
        [
            "--manifest",
            str(ROOT / "showcase-smoke.json"),
            "--value",
            f"server={server}",
            "--value",
            f"client={client}",
            "--value",
            f"config={config}",
            "--report",
            str(ROOT / "Workspace/showcase-smoke-report.json"),
        ]
    )


if __name__ == "__main__":
    raise SystemExit(main())
