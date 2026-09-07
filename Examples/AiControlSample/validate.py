from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    commands = (
        [sys.executable, "Examples/AiControlSample/run_protocol_smoke.py"],
        [sys.executable, "BuildTools/tests/test_ai_control_protocol.py"],
    )
    for command in commands:
        result = subprocess.run(command, cwd=ENGINE_ROOT, check=False)
        if result.returncode != 0:
            return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
