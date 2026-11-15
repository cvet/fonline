from __future__ import annotations

import argparse
import datetime
import os
import subprocess
import tempfile
from pathlib import Path

from engine_version import next_master_version


def git(root: Path, *args: str) -> str:
    result = subprocess.run(['git', '-C', str(root), *args], check=True, capture_output=True, text=True, encoding='utf-8')
    return result.stdout.strip()


def write_version(root: Path, version: str) -> None:
    descriptor, name = tempfile.mkstemp(prefix='.VERSION-', dir=root)
    temporary = Path(name)
    try:
        try:
            stream = os.fdopen(descriptor, 'w', encoding='utf-8', newline='\n')
        except BaseException:
            os.close(descriptor)
            raise
        with stream:
            stream.write(version + '\n')
        os.chmod(temporary, (root / 'VERSION').stat().st_mode)
        os.replace(temporary, root / 'VERSION')
    finally:
        temporary.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description='Generate the next master version for an active merge with origin/master.')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--advanced-tip', action='store_true', help='Recompute a resolved version when origin/master advanced during the active merge.')
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        if git(root, 'branch', '--show-current') != 'master':
            raise ValueError('The current branch must be master')
        published = git(root, 'rev-parse', '--verify', 'origin/master^{commit}')
        merge_target = git(root, 'rev-parse', '--verify', 'MERGE_HEAD^{commit}')
        if args.advanced_tip:
            if merge_target == published:
                raise ValueError('The published tip has not advanced since the active merge began')
            git(root, 'merge-base', '--is-ancestor', merge_target, published)
            if git(root, 'ls-files', '--unmerged', '--', 'VERSION'):
                raise ValueError('VERSION must already be resolved for an advanced tip')
        else:
            if merge_target != published:
                raise ValueError('The active merge must target the current origin/master')
            if not git(root, 'ls-files', '--unmerged', '--', 'VERSION'):
                raise ValueError('VERSION must be unresolved in the active merge')
        previous = git(root, 'show', f'{published}:VERSION')
        if args.advanced_tip and (root / 'VERSION').read_text(encoding='utf-8').strip() != previous:
            raise ValueError('Resolved VERSION must match the advanced published version')
        version = next_master_version(previous, datetime.datetime.now(datetime.timezone.utc).date())
        write_version(root, version)
        print(f'Generated Engine VERSION {version} from {published}')
        return 0
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Engine version generation failed: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
