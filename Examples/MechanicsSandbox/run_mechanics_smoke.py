#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BUILDTOOLS_ROOT = ROOT.parents[1] / "BuildTools"
if not BUILDTOOLS_ROOT.is_dir():
    BUILDTOOLS_ROOT = ROOT / "Engine" / "BuildTools"
if not BUILDTOOLS_ROOT.is_dir():
    raise RuntimeError("unable to locate Engine BuildTools")
sys.path.insert(0, str(BUILDTOOLS_ROOT))

import gameplay_test_runner  # noqa: E402


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run the mechanics sandbox recipe scenarios against baked resources in the current directory."
    )
    parser.add_argument("server", help="headless server executable")
    parser.add_argument("client", help="headless client executable")
    parser.add_argument("config", help="sandbox main config (.fomain)")
    parser.add_argument(
        "--restart-db",
        default="MechanicsRestartDb",
        help="directory for the JSON databases of the restart scenarios; it is deleted before the run",
    )
    parser.add_argument("--report", default="mechanics-smoke-report.json", help="JSON report path")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    restart_db = Path(args.restart_db).resolve()
    working_dir = Path.cwd().resolve()
    if restart_db == working_dir or restart_db in working_dir.parents:
        print(f"[mechanics-smoke] refusing to clear {restart_db}: it contains the working directory", file=sys.stderr)
        return 2

    # Each restart pair needs an empty database for its first phase
    if restart_db.exists():
        shutil.rmtree(restart_db)
    restart_db.mkdir(parents=True)

    return gameplay_test_runner.main(
        [
            "--manifest",
            str(ROOT / "mechanics-smoke.json"),
            "--value",
            f"server={Path(args.server).resolve()}",
            "--value",
            f"client={Path(args.client).resolve()}",
            "--value",
            f"config={Path(args.config).resolve()}",
            "--value",
            f"restart_db={restart_db.as_posix()}",
            "--report",
            str(Path(args.report).resolve()),
        ]
    )


if __name__ == "__main__":
    raise SystemExit(main())
