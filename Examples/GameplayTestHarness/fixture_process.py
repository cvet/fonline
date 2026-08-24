#!/usr/bin/env python3

from __future__ import annotations

import argparse
import time
from collections.abc import Sequence


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Deterministic child process for gameplay runner tests")
    parser.add_argument("--ready")
    parser.add_argument("--marker", action="append", default=[])
    parser.add_argument("--delay-ms", type=int, default=0)
    parser.add_argument("--exit-code", type=int, default=0)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    if args.ready:
        print(args.ready, flush=True)
    if args.delay_ms > 0:
        time.sleep(args.delay_ms / 1000)
    for marker in args.marker:
        print(marker, flush=True)
    return args.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
