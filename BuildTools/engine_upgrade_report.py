"""Write an Engine update record skeleton for an embedding project.

Run from the project root. The report records the three Engine identities (the
parent gitlink at HEAD, the index entry and the Engine checkout), audits the
incoming Engine range between the baseline and the target, lists the changelog
entries of the range with their migration sections oldest first, searches the
project sources for the names of a names file, and writes a Markdown record with
the facts filled in and every judgement left as ``<fill in>``.

The command only reads Git; it never fetches, checks out or stages anything.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys
from typing import NamedTuple

from engine_version import CHANGELOG_PATHS, VERSION_RE, ChangelogEntry, changelog_entries, changelog_section


FILL = '<fill in>'
MIGRATION_HEADING = 'Migration'
GENERATED_MODELS_DIR = 'Docs/generated'


class Identities(NamedTuple):
    project_head: str
    project_branch: str
    engine_path: str
    gitlink: str | None
    index_entries: list[str]
    checkout: str
    checkout_changes: int
    submodule_status: str


class Commit(NamedTuple):
    sha: str
    date: str
    subject: str


class RangeAudit(NamedTuple):
    old: str
    new: str
    old_version: str
    new_version: str
    is_ancestor: bool
    commits: list[Commit]
    publication_steps: list[tuple[Commit, str]]
    shortstat: str
    areas: list[tuple[str, int]]
    generated_models: list[str]


class NameMatches(NamedTuple):
    name: str
    locations: list[str]


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog='engine_upgrade_report.py',
        description='Write a Markdown Engine update record for an embedding project from read-only Git queries.')
    parser.add_argument('--project', type=Path, default='.',
                        help='embedding project work tree (default: current directory)')
    parser.add_argument('--engine', default='Engine', help='Engine checkout relative to --project (default: Engine)')
    parser.add_argument('--old', help='baseline Engine revision (default: the parent gitlink at HEAD)')
    parser.add_argument('--new', help='target Engine revision, for example a commit or a v<version> tag (default: the Engine checkout HEAD)')
    parser.add_argument('--changelog', default=CHANGELOG_PATHS[0],
                        help=f'changelog path inside the Engine repository, read at the target revision (default: {CHANGELOG_PATHS[0]})')
    parser.add_argument('--names', type=Path,
                        help='text file with one literal name per line to search in the project sources; blank lines are ignored')
    parser.add_argument('--search-path', action='append', metavar='PATHSPEC',
                        help='project pathspec to search for the names; repeatable (default: the whole project except the Engine checkout)')
    parser.add_argument('--max-locations', type=int, default=20,
                        help='locations listed per name in the record; the match count is always complete (default: 20)')
    parser.add_argument('--output', type=Path, help='write the record to this file instead of standard output')
    parser.add_argument('--force', action='store_true', help='replace an existing --output file')
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = create_parser()
    args = parser.parse_args(argv)
    if args.max_locations < 1:
        parser.error('--max-locations must be positive')
    if args.output is not None and args.output.exists() and not args.force:
        parser.error(f'{args.output} exists; pass --force to replace it')

    try:
        report, warnings, failed = build_report(args)
        if args.output is not None:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(report, encoding='utf-8', newline='\n')
        else:
            sys.stdout.flush()
            sys.stdout.buffer.write(report.encode('utf-8'))
            sys.stdout.buffer.flush()
    except (OSError, ValueError) as error:
        print(f'engine_upgrade_report: {error}', file=sys.stderr)
        return 1

    for warning in warnings:
        print(f'engine_upgrade_report: warning: {warning}', file=sys.stderr)
    if args.output is not None:
        print(f'engine_upgrade_report: wrote {args.output}', file=sys.stderr)
    return 1 if failed else 0


def build_report(args: argparse.Namespace) -> tuple[str, list[str], bool]:
    top = Path(git(args.project, 'rev-parse', '--show-toplevel')).resolve()
    engine_root = (args.project.resolve() / args.engine).resolve()
    identities = read_identities(top, engine_root)

    old = args.old or identities.gitlink
    if not old:
        raise ValueError(f'{identities.engine_path} has no gitlink at HEAD; pass --old with the baseline revision')
    new = args.new or identities.checkout
    audit = audit_range(engine_root, old, new)

    warnings: list[str] = []
    if not audit.is_ancestor:
        warnings.append(f'baseline {audit.old} is not an ancestor of target {audit.new}: this is a different history line, not an update')
    changelog_text = git(engine_root, 'show', f'{audit.new}:{args.changelog}', strip=False)
    entries = changelog_entries(audit.new_version, changelog_text)
    incoming = incoming_entries(entries, audit.old_version)
    if incoming is None:
        warnings.append(f'{audit.old_version} has no changelog entry at the target; review the complete commit range')
    unreleased = unreleased_text(changelog_text)

    names: list[NameMatches] | None = None
    names_file = None
    if args.names is not None:
        names_file = args.names
        names = search_names(top, identities.engine_path, read_names(args.names), args.search_path)

    report = render_report(identities, audit, incoming, unreleased, names, names_file, args.max_locations, args.changelog)
    return report, warnings, not audit.is_ancestor


def git(root: Path, *args: str, strip: bool = True, check: bool = True) -> str:
    result = subprocess.run(['git', '--no-optional-locks', '-C', str(root), *args],
                            capture_output=True, text=True, encoding='utf-8')
    if check and result.returncode:
        raise ValueError(f'git -C {root} {" ".join(args)} failed: {result.stderr.strip()}')
    return result.stdout.strip() if strip else result.stdout


def read_identities(top: Path, engine_root: Path) -> Identities:
    try:
        engine_path = engine_root.relative_to(top).as_posix()
    except ValueError as error:
        raise ValueError(f'Engine checkout {engine_root} is outside the project work tree {top}') from error

    head = git(top, 'rev-parse', '--verify', 'HEAD^{commit}')
    branch = git(top, 'symbolic-ref', '--quiet', '--short', 'HEAD', check=False) or 'detached'
    tree_entry = git(top, 'ls-tree', 'HEAD', '--', engine_path).split()
    gitlink = tree_entry[2] if len(tree_entry) >= 3 and tree_entry[0] == '160000' else None
    index_entries = [line.split()[1] + (f' (stage {line.split()[2]})' if line.split()[2] != '0' else '')
                     for line in git(top, 'ls-files', '--stage', '--', engine_path).splitlines()
                     if line.startswith('160000 ')]

    # Without its own .git, Git commands in the directory would silently answer for the project repository
    if not (engine_root / '.git').exists():
        raise ValueError(f'{engine_path} is not an initialized Git checkout; run git submodule update --init {engine_path} first')
    checkout = git(engine_root, 'rev-parse', '--verify', 'HEAD^{commit}')
    changes = len(git(engine_root, 'status', '--porcelain', '--untracked-files=no').splitlines())
    status = git(top, 'submodule', 'status', '--', engine_path) if gitlink else ''
    return Identities(head, branch, engine_path, gitlink, index_entries, checkout, changes, status)


def audit_range(engine_root: Path, old_ref: str, new_ref: str) -> RangeAudit:
    old = git(engine_root, 'rev-parse', '--verify', f'{old_ref}^{{commit}}')
    new = git(engine_root, 'rev-parse', '--verify', f'{new_ref}^{{commit}}')
    is_ancestor = subprocess.run(['git', '--no-optional-locks', '-C', str(engine_root), 'merge-base', '--is-ancestor', old, new],
                                 capture_output=True).returncode == 0

    commits = parse_commits(git(engine_root, 'log', '--topo-order', '--reverse', '--format=%H%x09%cs%x09%s', f'{old}..{new}'))
    steps = [(commit, read_version(engine_root, commit.sha))
             for commit in parse_commits(git(engine_root, 'log', '--first-parent', '--reverse',
                                             '--format=%H%x09%cs%x09%s', f'{old}..{new}', '--', 'VERSION'))]
    changed = git(engine_root, 'diff', '--name-only', '--no-renames', old, new).splitlines()
    areas: dict[str, int] = {}
    for path in changed:
        area = path.split('/', 1)[0] if '/' in path else '(root)'
        areas[area] = areas.get(area, 0) + 1
    generated = sorted(path for path in changed if path.startswith(GENERATED_MODELS_DIR + '/'))
    return RangeAudit(old, new, read_version(engine_root, old), read_version(engine_root, new), is_ancestor, commits,
                      steps, git(engine_root, 'diff', '--shortstat', old, new), sorted(areas.items()), generated)


def parse_commits(text: str) -> list[Commit]:
    return [Commit(*line.split('\t', 2)) for line in text.splitlines() if line]


def read_version(engine_root: Path, revision: str) -> str:
    text = git(engine_root, 'show', f'{revision}:VERSION', strip=False, check=False)
    version = text.removesuffix('\n').removesuffix('\r')
    return version if version and '\n' not in version else 'unknown'


def incoming_entries(entries: list[ChangelogEntry], old_version: str) -> list[ChangelogEntry] | None:
    versions = [entry.version for entry in entries]
    if old_version not in versions:
        return None
    return list(reversed(entries[:versions.index(old_version)]))


def unreleased_text(changelog: str) -> str:
    match = re.search(r'^## Unreleased[ \t]*\n(.*?)(?=^## |\Z)', changelog.replace('\r\n', '\n'), re.MULTILINE | re.DOTALL)
    return match[1].strip() if match else ''


def read_names(path: Path) -> list[str]:
    names = []
    for line in path.read_text(encoding='utf-8').splitlines():
        name = line.strip()
        if name and name not in names:
            names.append(name)
    return names


def search_names(top: Path, engine_path: str, names: list[str], pathspecs: list[str] | None) -> list[NameMatches]:
    if not names:
        return []
    command = ['grep', '-nIF', '--no-color', '-f', '-', '--', *(pathspecs or ['.']), f':(exclude){engine_path}']
    result = subprocess.run(['git', '--no-optional-locks', '-C', str(top), *command], input=''.join(name + '\n' for name in names),
                            capture_output=True, text=True, encoding='utf-8')
    # Exit status 1 is the negative result of the search, not a failure
    if result.returncode not in (0, 1):
        raise ValueError(f'git {" ".join(command)} failed: {result.stderr.strip()}')
    output = result.stdout
    locations: dict[str, list[str]] = {name: [] for name in names}
    for line in output.splitlines():
        path, _, rest = line.partition(':')
        number, _, content = rest.partition(':')
        for name in names:
            if name in content:
                locations[name].append(f'{path}:{number}')
    return [NameMatches(name, locations[name]) for name in names]


def code_spans(text: str) -> list[str]:
    # Single-token spans only: commands, revisions and Engine versions are not project names
    spans = []
    for span in re.findall(r'`([^`\s]+)`', text):
        if re.fullmatch(r'[0-9a-f]{7,40}', span) or VERSION_RE.fullmatch(span) or span in spans:
            continue
        spans.append(span)
    return spans


def cell(text: str) -> str:
    return text.replace('\\', '\\\\').replace('|', '\\|').replace('\n', ' ')


def quoted(text: str) -> list[str]:
    return [('> ' + line).rstrip() for line in text.strip().split('\n')]


def render_report(identities: Identities, audit: RangeAudit, incoming: list[ChangelogEntry] | None, unreleased: str,
                  names: list[NameMatches] | None, names_file: Path | None, max_locations: int, changelog: str) -> str:
    engine = identities.engine_path
    lines = [f'# Engine update record: {audit.old_version} to {audit.new_version}', '',
             'Generated by `engine_upgrade_report.py` from read-only Git queries. Replace every '
             f'`{FILL}` and keep the record with the update plan.', '',
             '## Goal', '', f'- Why the project moves the Engine pin now: {FILL}', f'- Exit criterion: {FILL}', '',
             '## Baseline and target', '',
             '| Identity | Revision | Engine VERSION |', '| --- | --- | --- |',
             f'| Project HEAD (branch `{identities.project_branch}`) | `{identities.project_head}` | - |',
             f'| Parent gitlink at HEAD | {code_or_none(identities.gitlink)} | {version_of(identities.gitlink, audit)} |',
             f'| Index entry | {", ".join(f"`{entry}`" for entry in identities.index_entries) or "none"} | - |',
             f'| Engine checkout | `{identities.checkout}`{checkout_state(identities)} | {version_of(identities.checkout, audit)} |',
             f'| Baseline (old) | `{audit.old}` | `{audit.old_version}` |',
             f'| Target (new) | `{audit.new}` | `{audit.new_version}` |', '']
    if identities.submodule_status:
        lines += [f'- `git submodule status -- {engine}`: `{identities.submodule_status}`']
    lines += [f'- Ancestry check (baseline is an ancestor of the target): {"passed" if audit.is_ancestor else "FAILED, a different history line"}',
              f'- Safety refs and stashes in both repositories: {FILL}',
              f'- Local changes preserved: {FILL}', '',
              '## Incoming range', '',
              f'- Commits: {len(audit.commits)} (`git -C {engine} log --oneline {short(audit.old)}..{short(audit.new)}`)',
              f'- Changed files: {audit.shortstat or "none"}',
              f'- Changed paths per top-level area: {", ".join(f"`{area}` {count}" for area, count in audit.areas) or "none"}', '',
              '### Publication steps', '']
    if audit.publication_steps:
        lines += ['| Commit | Date | VERSION | Subject |', '| --- | --- | --- | --- |']
        lines += [f'| `{short(commit.sha)}` | {commit.date} | `{version}` | {cell(commit.subject)} |'
                  for commit, version in audit.publication_steps]
    else:
        lines.append('No first-parent commit of the range changes `VERSION`.')
    lines += ['', '### Changed generated contract models', '']
    lines += [f'- `{path}`' for path in audit.generated_models] or ['None.']
    lines += ['', '### Commit notes', '', '| Commit | Subject | Owner area | Project consequence | Action |',
              '| --- | --- | --- | --- | --- |']
    lines += [f'| `{short(commit.sha)}` | {cell(commit.subject)} | {FILL} | {FILL} | {FILL} |' for commit in audit.commits]
    lines += ['', '## Changelog entries, oldest first', '']
    if incoming is None:
        lines += [f'`{audit.old_version}` has no entry in `{changelog}` at the target. It predates the maintained '
                  'log or belongs to another release line: review the complete commit range instead of a guessed range.', '']
    elif not incoming:
        lines += ['The target adds no dated changelog entry after the baseline.', '']
    for entry in incoming or []:
        migration = changelog_section(entry.body, MIGRATION_HEADING)
        lines += [f'### {entry.version} - {entry.date}', '', f'{MIGRATION_HEADING}:', '']
        lines += quoted(migration) if migration and migration.strip() else ['> No migration section.']
        lines += ['', f'- Applied: {FILL}', f'- Detection searches and their results: {FILL}', '']
    if unreleased:
        lines += ['### Unreleased text at the target', '', *quoted(unreleased), '']

    spans = code_spans('\n'.join(changelog_section(entry.body, MIGRATION_HEADING) or '' for entry in incoming or []))
    lines += ['## Names quoted in the incoming migration notes', '']
    if spans:
        lines += ['Candidates for the names file; keep the ones that name a symbol, setting, option, command or file form.', '']
        lines += [f'- `{span}`' for span in spans]
    else:
        lines.append('None.')

    lines += ['', '## Project consumer analysis', '']
    if names is None:
        lines += [f'Names file: {FILL} (run the report again with `--names <file>`)', '',
                  '| Name | Matches | Locations | Transformation | Positive check | Negative proof |',
                  '| --- | --- | --- | --- | --- | --- |', f'| {FILL} | | | | | |']
    else:
        lines += [f'Names file: `{names_file.as_posix() if names_file else ""}` ({len(names)} names), searched with '
                  f'`git grep -nIF` outside `{engine}`.', '',
                  '| Name | Matches | Locations | Transformation | Positive check | Negative proof |',
                  '| --- | --- | --- | --- | --- | --- |']
        for match in names:
            shown = ', '.join(f'`{location}`' for location in match.locations[:max_locations])
            if len(match.locations) > max_locations:
                shown += f', and {len(match.locations) - max_locations} more'
            negative = FILL if not match.locations else '-'
            lines.append(f'| `{cell(match.name)}` | {len(match.locations)} | {shown or "none"} | {FILL} | {FILL} | {negative} |')

    lines += ['', '## Generated contract report', '',
              f'- Command: `python3 {engine}/BuildTools/docs_contract_diff.py --root {engine} --baseline-git-ref {audit.old} --write`',
              f'- Status, changed entries and domains without a baseline: {FILL}', '',
              '## Steps', '', f'1. {FILL} - boundary: data, bake, protocol, CMake or none',
              f'   Acceptance: {FILL}', '',
              '## Verification record', '']
    lines += [f'- {item}: {FILL}' for item in (
        'Configure and compile per host and target', 'Script compile per backend',
        'Forced bake and side metadata comparison', 'Focused tests, smoke and visible-client checks',
        'Save upgrade and restore rehearsal', 'Compatibility, updater and package checks',
        'Documentation and translation checks')]
    lines += ['', '## Qualification limits', '',
              f'- Platforms, devices and rollout steps not observed: {FILL}', f'- Owner decisions still open: {FILL}', '',
              '## Completion record', '',
              f'- Project old/new: `{identities.project_head}` / {FILL}',
              f'- Engine old/new: `{audit.old}` (`{audit.old_version}`) / `{audit.new}` (`{audit.new_version}`)',
              f'- Incoming Engine commits audited: {len(audit.commits)}']
    lines += [f'- {item}: {FILL}' for item in (
        'Generated contract report', 'Required dispositions', 'Configuration changes', 'Content/resource migrations',
        'Save migration and restore evidence', 'Network/updater/ABI decision', 'Validated host/target matrix',
        'Visible/device checks', 'Documentation and translation status', 'Known residual risks',
        'Safety refs retained until')]
    return '\n'.join(lines) + '\n'


def short(sha: str) -> str:
    return sha[:10]


def code_or_none(sha: str | None) -> str:
    return f'`{sha}`' if sha else 'none'


def checkout_state(identities: Identities) -> str:
    return ' (clean)' if not identities.checkout_changes else f' ({identities.checkout_changes} changed tracked files, -dirty)'


def version_of(sha: str | None, audit: RangeAudit) -> str:
    if sha == audit.old:
        return f'`{audit.old_version}`'
    if sha == audit.new:
        return f'`{audit.new_version}`'
    return '-'


if __name__ == '__main__':
    raise SystemExit(main())
