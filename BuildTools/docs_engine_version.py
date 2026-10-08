from __future__ import annotations

import argparse
import datetime
import subprocess
from pathlib import Path

from engine_version import (CHANGELOG_PATHS, RELEASE_RE, parse_engine_version,
                            read_engine_version, validate_changelog_texts,
                            validate_changelogs, validate_transition)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='Validate Engine update versions and bilingual migration notes.')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--check', action='store_true', help='Validate without writing files.')
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
            for old_heading in RELEASE_RE.finditer(old_text):
                if old_heading[1] not in [version for version, _ in RELEASE_RE.findall(text)]:
                    raise ValueError(f'{target}: missing preceding change notes {old_heading[1]} in {path}')
    if branch == 'master':
        notes_date = next(date_text for version, date_text in RELEASE_RE.findall(texts[0]) if version == current)
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


def main() -> int:
    parser = create_parser()
    args = parser.parse_args()
    try:
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
