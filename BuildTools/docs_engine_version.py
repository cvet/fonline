from __future__ import annotations

import argparse
import datetime
import os
import re
import stat
import subprocess
import tempfile
from pathlib import Path

from engine_version import (CHANGELOG_PATHS, RELEASE_RE, changelog_releases, parse_engine_version,
                            read_engine_version, validate_changelog_texts,
                            validate_changelogs, validate_transition)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='Validate Engine update versions and bilingual migration notes.')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--check', action='store_true', help='Validate without writing files.')
    parser.add_argument('--write-next', action='store_true', help='Write the next master version after validating the fetched baseline and notes.')
    parser.add_argument('--branch', help='Publication target: master or release/YEAR.MAJOR.')
    parser.add_argument('--baseline-git-ref', help='Exact base revision; omitted for a source-only check.')
    parser.add_argument('--target-git-ref', help='Check a committed target instead of the working tree.')
    parser.add_argument('--history', action='store_true', help='Validate every incoming first-parent publication step.')
    return parser


def git(root: Path, *args: str, strip: bool = True) -> str:
    result = subprocess.run(['git', '-C', str(root), *args], capture_output=True, text=True, encoding='utf-8')
    if result.returncode:
        raise ValueError(f'Git version validation failed: {args}: {result.stderr.strip()}')
    return result.stdout.strip() if strip else result.stdout


def validate_committed_update(root: Path, baseline: str, target: str, branch: str) -> None:
    previous = git(root, 'show', f'{baseline}:VERSION', strip=False).removesuffix('\n').removesuffix('\r')
    current = git(root, 'show', f'{target}:VERSION', strip=False).removesuffix('\n').removesuffix('\r')
    date = datetime.datetime.fromisoformat(git(root, 'show', '-s', '--format=%cI', target)).astimezone(
        datetime.timezone.utc).date()
    validate_transition(previous, current, branch, date)
    texts = [git(root, 'show', f'{target}:{path}', strip=False) for path in CHANGELOG_PATHS]
    validate_changelog_texts(current, texts)
    if previous != '2022.1.0.wip':
        for path, text in zip(CHANGELOG_PATHS, texts, strict=True):
            old_text = git(root, 'show', f'{baseline}:{path}', strip=False)
            for old_version, _ in changelog_releases(previous, old_text):
                if old_version not in [version for version, _ in changelog_releases(current, text)]:
                    raise ValueError(f'{target}: missing preceding change notes {old_version} in {path}')
    if branch == 'master':
        notes_date = next(date_text for version, date_text in changelog_releases(current, texts[0]) if version == current)
        if notes_date != date.isoformat():
            raise ValueError(f'{target}: master change notes must carry its UTC commit date {date}')
    validate_promotion_paths(previous, current, git(root, 'diff', '--name-only', baseline, target).splitlines())


def validate_promotion_paths(previous: str, current: str, paths: list[str]) -> None:
    old, new = parse_engine_version(previous) if previous != '2022.1.0.wip' else None, parse_engine_version(current)
    if old and old.suffix == '-rc' and new.patch == old.patch and not new.suffix:
        allowed = ('VERSION', 'docs-manifest.json', 'llms.txt', 'llms-full.txt',
                   'README.md', 'README.ru.md', 'AGENTS.md', 'CLAUDE.md', 'PUBLIC_API.md')
        if any(path not in allowed and not path.startswith('Docs/') for path in paths):
            raise ValueError('rc promotion may update only VERSION and documentation; code changes require a patch increment')


def validate_history(root: Path, baseline: str, target: str, branch: str) -> int:
    baseline = git(root, 'rev-parse', '--verify', f'{baseline}^{{commit}}')
    target = git(root, 'rev-parse', '--verify', f'{target}^{{commit}}')
    chain = git(root, 'rev-list', '--first-parent', target).splitlines()
    if baseline not in chain:
        raise ValueError('Publication baseline must be on the target first-parent history')
    steps = list(reversed(chain[:chain.index(baseline)]))
    previous = baseline
    for step in steps:
        validate_committed_update(root, previous, step, branch)
        previous = step
    return len(steps)


def release_blocks(version: str, text: str) -> list[tuple[str, str, str]]:
    blocks = []
    for heading in RELEASE_RE.finditer(text):
        release, date = heading.groups()
        following = re.search(r'^## ', text[heading.end():], re.MULTILINE)
        end = heading.end() + following.start() if following else len(text)
        blocks.append((version if release == 'Current' else release, date, text[heading.end():end]))
    return blocks


def validate_existing_version(root: Path, baseline: str, previous: str, current: str, data: bytes) -> None:
    text = data.decode('utf-8').replace('\r\n', '\n')
    if text in (previous, previous + '\n', current, current + '\n'):
        return

    merge_head = git(root, 'rev-parse', '--verify', 'MERGE_HEAD^{commit}')
    local = git(root, 'show', ':2:VERSION', strip=False).removesuffix('\n').removesuffix('\r')
    incoming = git(root, 'show', ':3:VERSION', strip=False).removesuffix('\n').removesuffix('\r')
    head = git(root, 'show', 'HEAD:VERSION', strip=False).removesuffix('\n').removesuffix('\r')
    if local != '2022.1.0.wip':
        parse_engine_version(local)
    expected = f'<<<<<<< HEAD\n{local}\n=======\n{incoming}\n>>>>>>> origin/master\n'
    if merge_head != baseline or local != head or incoming != previous or text != expected:
        raise ValueError('VERSION is not the exact fetched baseline, next version or current merge conflict')


def write_next_master_version(root: Path, baseline: str, date: datetime.date) -> str:
    if not re.fullmatch(r'(?:[0-9a-f]{40}|[0-9a-f]{64})', baseline):
        raise ValueError('The fetched master baseline must be an exact full commit id')

    fetched = git(root, 'rev-parse', '--verify', 'refs/remotes/origin/master^{commit}')
    resolved = git(root, 'rev-parse', '--verify', f'{baseline}^{{commit}}')
    if baseline != fetched or resolved != baseline:
        raise ValueError('The baseline is not the current fetched origin/master commit')

    previous = git(root, 'show', f'{baseline}:VERSION', strip=False).removesuffix('\n').removesuffix('\r')
    if previous == '2022.1.0.wip':
        current = f'{date.year}.1.1-dev'
    else:
        old = parse_engine_version(previous)
        if old.suffix != '-dev' or old.year > date.year:
            raise ValueError('The fetched baseline is not a usable master version')
        current = f'{date.year}.1.0-dev' if old.year < date.year else f'{old.year}.{old.major}.{old.minor + 1}-dev'
    validate_transition(previous, current, 'master', date)

    texts = [(root / path).read_text(encoding='utf-8') for path in CHANGELOG_PATHS]
    validate_changelog_texts(current, texts)
    if previous != '2022.1.0.wip':
        old_texts = [git(root, 'show', f'{baseline}:{path}', strip=False) for path in CHANGELOG_PATHS]
        validate_changelog_texts(previous, old_texts)
        for path, old_text, text in zip(CHANGELOG_PATHS, old_texts, texts, strict=True):
            old_blocks = release_blocks(previous, old_text)
            new_blocks = iter(release_blocks(current, text))
            for old_block in old_blocks:
                for new_block in new_blocks:
                    if new_block[0] == old_block[0]:
                        if new_block != old_block:
                            raise ValueError(f'Preceding change notes changed in {path}: {old_block[0]}')
                        break
                else:
                    raise ValueError(f'Missing or reordered preceding change notes in {path}: {old_block[0]}')
    for path, text in zip(CHANGELOG_PATHS, texts, strict=True):
        current_releases = changelog_releases(current, text)
        if next(note_date for version, note_date in current_releases if version == current) != date.isoformat():
            raise ValueError(f'Current change notes must carry the UTC date in {path}')

    version_path = root / 'VERSION'
    if version_path.is_symlink() or not version_path.is_file():
        raise ValueError('VERSION must be an existing regular file')
    old_bytes = version_path.read_bytes()
    old_mode = stat.S_IMODE(version_path.stat().st_mode)
    validate_existing_version(root, baseline, previous, current, old_bytes)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(mode='wb', dir=root, prefix='.engine-version-', delete=False) as output:
            temporary_path = Path(output.name)
            output.write((current + '\n').encode('utf-8'))
        os.chmod(temporary_path, old_mode)
        if git(root, 'rev-parse', '--verify', 'refs/remotes/origin/master^{commit}') != baseline:
            raise ValueError('The fetched origin/master changed during version preparation')
        if [(root / path).read_text(encoding='utf-8') for path in CHANGELOG_PATHS] != texts:
            raise ValueError('The change notes changed during version preparation')
        if version_path.is_symlink() or version_path.read_bytes() != old_bytes or stat.S_IMODE(version_path.stat().st_mode) != old_mode:
            raise ValueError('VERSION changed during version preparation')
        os.replace(temporary_path, version_path)
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()
    return current


def main() -> int:
    parser = create_parser()
    args = parser.parse_args()
    try:
        if args.write_next:
            if args.check or args.branch != 'master' or not args.baseline_git_ref or args.target_git_ref or args.history:
                raise ValueError('--write-next requires master and an exact baseline, without --check, --target-git-ref or --history')
            current = write_next_master_version(args.root, args.baseline_git_ref, datetime.datetime.now(datetime.timezone.utc).date())
            print(f'Engine next version written: {current}')
            return 0
        if args.target_git_ref and not args.baseline_git_ref:
            raise ValueError('--target-git-ref requires --baseline-git-ref')
        if args.history and (not args.baseline_git_ref or not args.target_git_ref or not args.branch):
            raise ValueError('--history requires --baseline-git-ref, --target-git-ref and --branch')
        if args.baseline_git_ref:
            if not args.branch:
                raise ValueError('Update comparison requires --branch')
            if args.target_git_ref:
                if args.history:
                    count = validate_history(args.root, args.baseline_git_ref, args.target_git_ref, args.branch)
                else:
                    validate_committed_update(args.root, args.baseline_git_ref, args.target_git_ref, args.branch)
                    count = 1
                print(f'Engine update validation passed: {count} publication step(s)')
                return 0
            previous = git(args.root, 'show', f'{args.baseline_git_ref}:VERSION', strip=False).removesuffix('\n').removesuffix('\r')
            current = read_engine_version(args.root)
            validate_transition(previous, current, args.branch, datetime.datetime.now(datetime.timezone.utc).date())
            paths = git(args.root, 'diff', '--name-only', args.baseline_git_ref).splitlines()
            paths += git(args.root, 'ls-files', '--others', '--exclude-standard').splitlines()
            validate_promotion_paths(previous, current, paths)
        elif args.branch:
            version = parse_engine_version(read_engine_version(args.root))
            if args.branch == 'master' and (version.suffix != '-dev' or version.year != datetime.datetime.now(datetime.timezone.utc).year):
                raise ValueError('master requires a development version in the current UTC year')
            if args.branch != 'master' and (version.patch is None or args.branch != f'release/{version.year}.{version.major}'):
                raise ValueError('Release version must match release/YEAR.MAJOR')
        validate_changelogs(args.root)
        print(f'Engine version and migration notes passed: {read_engine_version(args.root)}')
        return 0
    except (ValueError, OSError) as exception:
        parser.exit(1, f'Engine update validation failed: {exception}\n')


if __name__ == '__main__':
    raise SystemExit(main())

