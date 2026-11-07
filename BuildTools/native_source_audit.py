"""Audit native C++ sources against the Engine coding contracts.

Subcommands: ``allocator`` and ``smart-pointer`` are text audits that need only Python; ``smart-pointer-ast``,
``local-variables`` and ``explicit-local-types`` need Clang 20+ tools and a ``compile_commands.json`` and skip
with a reason when either is absent.  See Docs/en/how-to/project/native-source-audits.md.
"""

from __future__ import annotations

import argparse
import os
import sys

# Not resolved, so an Engine reached through a link keeps its path inside the embedding project
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from native_source_audit import cli  # noqa: E402


def create_parser() -> argparse.ArgumentParser:
    return cli.make_parser()


def main(argv: list[str] | None = None) -> int:
    return cli.run(argv)


if __name__ == '__main__':
    sys.exit(main())
