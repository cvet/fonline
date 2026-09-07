#!/usr/bin/env python3
"""Bind native update payloads to the installed managed runtime companions."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def runtime_identity(runtime_dir: Path) -> str:
    if not runtime_dir.is_dir():
        raise ValueError(f'Managed runtime directory not found: {runtime_dir}')
    files = sorted(path for path in runtime_dir.rglob('*') if path.is_file())
    if not files:
        raise ValueError(f'Managed runtime directory is empty: {runtime_dir}')
    digest = hashlib.sha256()
    for path in files:
        digest.update(path.relative_to(runtime_dir).as_posix().encode('utf-8'))
        digest.update(b'\0')
        content_digest = hashlib.sha256()
        with path.open('rb') as stream:
            while chunk := stream.read(1024 * 1024):
                content_digest.update(chunk)
        digest.update(content_digest.digest())
    return digest.hexdigest()


def write_if_changed(path: Path, text: str) -> None:
    if path.is_file() and path.read_text(encoding='utf-8') == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.tmp')
    try:
        temporary.write_text(text, encoding='utf-8')
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-dir', type=Path, required=True)
    parser.add_argument('--header', type=Path, required=True)
    parser.add_argument('--identity-file', type=Path, required=True)
    args = parser.parse_args()
    identity = runtime_identity(args.runtime_dir)
    write_if_changed(args.header, '#pragma once\n\n#define FO_MANAGED_RUNTIME_ID "' + identity + '"\n')
    write_if_changed(args.identity_file, identity + '\n')


if __name__ == '__main__':
    main()
