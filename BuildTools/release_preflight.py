"""Validate a project release before publication and prepare its assets.

The preflight reads the project version file, validates it with a version
scheme of ``project_version.py``, derives the tag, rejects a tag that already
exists or a version that does not exceed the newest release tag, and decides
between publish and dry-run mode: publish only when the checked ref equals the
release ref. Optionally it splits assets larger than a limit into
``<name>.partNN`` pieces, verifies them against the original digest, and writes
one SHA-256 manifest of the final assets. It reads Git only and never creates
tags or publishes anything.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
from typing import NamedTuple

import artifact_checksums
import project_version


_SIZE_UNITS = {'': 1, 'K': 1024, 'M': 1024 ** 2, 'G': 1024 ** 3}
MAX_PARTS = 100
COPY_CHUNK_SIZE = 1024 * 1024


class Asset(NamedTuple):
    name: str
    size: int
    digest: str


class PreflightResult(NamedTuple):
    publish: bool
    version: str
    tag: str
    previous_tag: str
    assets: list[Asset]
    warnings: list[str]


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog='release_preflight.py',
        description='Validate the version, tag and assets of a project release and decide between publish and dry-run.')
    parser.add_argument('--root', type=Path, default='.',
                        help='directory inside the project work tree (default: current directory)')
    parser.add_argument('--config', type=Path,
                        help='project_version.py JSON config; its file, scheme and limits apply here')
    parser.add_argument('--file', help='version file relative to --root (default: VERSION)')
    parser.add_argument('--scheme', help='version scheme, as for project_version.py (default: patch-per-commit)')
    parser.add_argument('--limit', action='append', default=[], metavar='COMPONENT=N',
                        help='scheme component limit, as for project_version.py; repeatable')
    parser.add_argument('--tag-prefix', default='v', help='release tag prefix before the version (default: v)')
    parser.add_argument('--remote', help='also look for the tag on this Git remote, for example origin')
    parser.add_argument('--ref', help='ref being released, for example refs/heads/main (default: the branch checked out at HEAD)')
    parser.add_argument('--release-ref', help='only this ref publishes; every other ref is a dry run, and without it every run is a dry run')
    parser.add_argument('--dry-run', action='store_true', help='force dry-run mode even on the release ref')
    parser.add_argument('--engine', metavar='PATH',
                        help='also require the committed gitlink of this Engine submodule to be reachable from a remote-tracking branch of the submodule')
    parser.add_argument('--assets-dir', type=Path, help='directory whose root-level files are the release assets')
    parser.add_argument('--asset-pattern', action='append', metavar='GLOB',
                        help='file-name pattern of the assets in --assets-dir; repeatable (default: every file)')
    parser.add_argument('--max-asset-bytes', type=parse_size, metavar='SIZE',
                        help='split each larger asset into <name>.partNN pieces of at most SIZE bytes; K, M and G suffixes are powers of 1024')
    parser.add_argument('--manifest', metavar='FILENAME', help='write a SHA-256 manifest of the final assets into --assets-dir')
    parser.add_argument('--outputs-file', type=Path,
                        help='append version, tag, previous_tag, publish and assets as key=value lines, for example to $GITHUB_OUTPUT')
    parser.add_argument('--summary-file', type=Path,
                        help='append a Markdown summary, for example to $GITHUB_STEP_SUMMARY')
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = create_parser()
    args = parser.parse_args(argv)
    if args.manifest and not args.assets_dir:
        parser.error('--manifest requires --assets-dir')
    if args.max_asset_bytes is not None and not args.assets_dir:
        parser.error('--max-asset-bytes requires --assets-dir')
    try:
        result = run_preflight(args)
        if args.outputs_file is not None:
            append_text(args.outputs_file, outputs_text(result))
        if args.summary_file is not None:
            append_text(args.summary_file, summary_text(result))
    except (OSError, ValueError) as error:
        print(f'release_preflight: {error}', file=sys.stderr)
        return 1

    for warning in result.warnings:
        print(f'release_preflight: warning: {warning}', file=sys.stderr)
    print(f'mode={"publish" if result.publish else "dry-run"}')
    print(f'version={result.version}')
    print(f'tag={result.tag}')
    print(f'previous_tag={result.previous_tag}')
    for asset in result.assets:
        print(f'asset={asset.digest}  {asset.size}  {asset.name}')
    return 0


def parse_size(text: str) -> int:
    match = re.fullmatch(r'([1-9][0-9]*)([KMG]?)', text)
    if match is None:
        raise argparse.ArgumentTypeError(f'expected a positive integer with an optional K, M or G suffix: {text!r}')
    return int(match[1]) * _SIZE_UNITS[match[2]]


def run_preflight(args: argparse.Namespace) -> PreflightResult:
    settings = {'file': project_version.DEFAULT_FILE, 'scheme': project_version.DEFAULT_SCHEME, 'limits': {}}
    if args.config is not None:
        config = project_version.read_config(args.config)
        settings.update({key: config[key] for key in settings if key in config})
    if args.file is not None:
        settings['file'] = args.file
    if args.scheme is not None:
        settings['scheme'] = args.scheme
    if args.limit:
        settings['limits'] = {**settings['limits'], **dict(project_version.parse_limit(value) for value in args.limit)}
    scheme = project_version.load_scheme(settings['scheme'], settings['limits'], args.root)

    top = Path(git(args.root, 'rev-parse', '--show-toplevel')).resolve()
    if git(top, 'rev-parse', '--is-shallow-repository') != 'false':
        raise ValueError('The repository is shallow; fetch the complete history and tags before a release')
    dirty = git(top, 'status', '--porcelain', '--untracked-files=no')
    if dirty:
        raise ValueError('Tracked files differ from HEAD; release only a committed revision:\n' + dirty)

    version_path = (args.root / settings['file']).resolve()
    version_text = project_version.normalize_text(version_path.read_bytes().decode('utf-8'), settings['file'])
    try:
        version_value = tuple(scheme.parse(version_text))
    except ValueError as error:
        raise ValueError(f'{settings["file"]} holds {version_text!r}, which is invalid: {error}') from error
    tag = args.tag_prefix + version_text
    git(top, 'check-ref-format', f'refs/tags/{tag}')

    ref = normalize_ref(args.ref) if args.ref else git_optional(top, 'symbolic-ref', '-q', 'HEAD')
    release_ref = normalize_ref(args.release_ref) if args.release_ref else None
    publish = release_ref is not None and ref == release_ref and not args.dry_run

    problems = []
    if tag_exists(top, tag, args.remote):
        problems.append(f'release tag {tag} already exists; advance the version before publishing')
    previous_tag, previous_value = newest_release_tag(top, args.tag_prefix, scheme, tag)
    if previous_value is not None and version_value <= previous_value:
        problems.append(f'version {version_text} does not exceed the newest release tag {previous_tag}')
    if args.engine:
        problems += engine_pin_problems(top, (args.root / args.engine).resolve())
    if publish and problems:
        raise ValueError('; '.join(problems))

    assets: list[Asset] = []
    if args.assets_dir is not None:
        assets = prepare_assets(args.assets_dir, args.asset_pattern, args.max_asset_bytes, args.manifest)
    return PreflightResult(publish, version_text, tag, previous_tag, assets, problems)


def git(root: Path, *args: str) -> str:
    result = subprocess.run(['git', '--no-optional-locks', '-C', str(root), *args],
                            capture_output=True, text=True, encoding='utf-8')
    if result.returncode:
        raise ValueError(f'git {" ".join(args)} failed: {result.stderr.strip()}')
    return result.stdout.strip()


def git_optional(root: Path, *args: str) -> str:
    result = subprocess.run(['git', '--no-optional-locks', '-C', str(root), *args],
                            capture_output=True, text=True, encoding='utf-8')
    if result.returncode not in (0, 1):
        raise ValueError(f'git {" ".join(args)} failed: {result.stderr.strip()}')
    return result.stdout.strip() if result.returncode == 0 else ''


def normalize_ref(ref: str) -> str:
    return ref if ref.startswith('refs/') else f'refs/heads/{ref}'


def tag_exists(top: Path, tag: str, remote: str | None) -> bool:
    if git_optional(top, 'rev-parse', '--quiet', '--verify', f'refs/tags/{tag}'):
        return True
    return bool(remote) and bool(git(top, 'ls-remote', '--tags', remote, f'refs/tags/{tag}'))


def newest_release_tag(top: Path, prefix: str, scheme: project_version.VersionScheme,
                       current_tag: str) -> tuple[str, tuple[int, ...] | None]:
    newest: tuple[tuple[int, ...], str] | None = None
    for tag in git(top, 'tag', '--list', f'{prefix}*').splitlines():
        if tag == current_tag:
            continue
        try:
            value = tuple(scheme.parse(tag[len(prefix):]))
        except ValueError:
            # A tag outside the scheme belongs to another tag family, not to this release line
            continue
        if newest is None or value > newest[0]:
            newest = (value, tag)
    return (newest[1], newest[0]) if newest else ('', None)


def engine_pin_problems(top: Path, engine_path: Path) -> list[str]:
    try:
        relative = engine_path.relative_to(top).as_posix()
    except ValueError as error:
        raise ValueError(f'Engine path {engine_path} is outside the work tree {top}') from error
    entry = git(top, 'ls-tree', 'HEAD', '--', relative).split()
    if len(entry) < 3 or entry[0] != '160000':
        return [f'{relative} is not a submodule gitlink at HEAD']

    # The clean-tree check already rejects a checkout or local edits that differ from the gitlink
    gitlink = entry[2]
    if not git(engine_path, 'branch', '--remotes', '--contains', gitlink):
        return [f'{relative} gitlink {gitlink} is not reachable from any remote-tracking branch']
    return []


def prepare_assets(directory: Path, patterns: list[str] | None, max_bytes: int | None, manifest: str | None) -> list[Asset]:
    root = artifact_checksums.resolve_root(directory)
    manifest_path = artifact_checksums.manifest_destination(root, manifest)
    # Directories are not assets; links stay in the selection so that the checks below reject them
    names = sorted(path.name for path in root.iterdir()
                   if path != manifest_path and (path.is_symlink() or not path.is_dir())
                   and artifact_checksums.is_candidate(path.name, None)
                   and (not patterns or any(path.match(pattern) for pattern in patterns)))
    if not names:
        raise ValueError(f'No release assets found in {root}')
    for name in names:
        path = artifact_checksums.checked_artifact(root, name)
        if path.stat().st_size == 0:
            raise ValueError(f'Release asset is empty: {name}')

    final_names: list[str] = []
    for name in names:
        path = root / name
        if max_bytes is not None and path.stat().st_size > max_bytes:
            final_names += split_asset(path, max_bytes)
        else:
            final_names.append(name)

    if manifest_path is not None:
        artifact_checksums.write_checksums(root, final_names, suffixes=None, manifest=manifest, sidecars=False)
    assets = []
    for name in sorted(final_names):
        path = root / name
        assets.append(Asset(name, path.stat().st_size, artifact_checksums.hash_artifact(path)[0]))
    return assets


def split_asset(path: Path, max_bytes: int) -> list[str]:
    """Split into <name>.partNN pieces, verify their concatenation, then remove the original."""

    size = path.stat().st_size
    count = (size + max_bytes - 1) // max_bytes
    if count > MAX_PARTS:
        raise ValueError(f'{path.name} needs {count} parts of {max_bytes} bytes; at most {MAX_PARTS} are supported')
    names = [f'{path.name}.part{index:02d}' for index in range(count)]
    for name in names:
        if (path.parent / name).exists() or (path.parent / name).is_symlink():
            raise ValueError(f'Part {name} already exists; remove stale parts before splitting {path.name}')

    original = hashlib.sha256()
    written: list[Path] = []
    try:
        with path.open('rb') as source:
            identity = artifact_checksums.file_identity(os.fstat(source.fileno()))
            for name in names:
                with tempfile.NamedTemporaryFile(mode='wb', dir=path.parent, prefix=f'.{name}.', suffix='.tmp',
                                                 delete=False) as output:
                    temporary = Path(output.name)
                    written.append(temporary)
                    remaining = max_bytes
                    while remaining and (chunk := source.read(min(COPY_CHUNK_SIZE, remaining))):
                        original.update(chunk)
                        output.write(chunk)
                        remaining -= len(chunk)
                    output.flush()
                    os.fsync(output.fileno())
                os.chmod(temporary, 0o644)
            if source.read(1) or artifact_checksums.file_identity(os.fstat(source.fileno())) != identity:
                raise OSError(f'Asset changed while splitting: {path}')

        combined = hashlib.sha256()
        total = 0
        for temporary in written:
            with temporary.open('rb') as part:
                while chunk := part.read(COPY_CHUNK_SIZE):
                    combined.update(chunk)
                    total += len(chunk)
        if total != size or combined.hexdigest() != original.hexdigest():
            raise OSError(f'Split parts of {path.name} do not reassemble to the original bytes')

        for temporary, name in zip(written, names, strict=True):
            os.replace(temporary, path.parent / name)
        written = []
        path.unlink()
    finally:
        for temporary in written:
            temporary.unlink(missing_ok=True)
    return names


def outputs_text(result: PreflightResult) -> str:
    lines = [f'version={result.version}', f'tag={result.tag}', f'previous_tag={result.previous_tag}',
             f'publish={"true" if result.publish else "false"}',
             f'assets={" ".join(asset.name for asset in result.assets)}']
    return ''.join(line + '\n' for line in lines)


def summary_text(result: PreflightResult) -> str:
    mode = 'publish' if result.publish else 'dry-run'
    lines = [f'### Release {mode}', '', '| Field | Value |', '| --- | --- |',
             f'| Version | `{result.version}` |', f'| Tag | `{result.tag}` |',
             f'| Previous tag | `{result.previous_tag or "none"}` |', '']
    if result.assets:
        lines += ['| Asset | Bytes | SHA-256 |', '| --- | --- | --- |']
        lines += [f'| `{asset.name}` | {asset.size} | `{asset.digest}` |' for asset in result.assets]
        lines.append('')
    lines += [f'Warning: {warning}' for warning in result.warnings]
    if not result.publish:
        lines.append('Dry run: nothing is tagged or published by this run.')
    return '\n'.join(lines) + '\n'


def append_text(path: Path, text: str) -> None:
    if path.is_symlink() or (path.exists() and not stat.S_ISREG(path.stat().st_mode)):
        raise ValueError(f'Output destination is not a regular file: {path}')
    with path.open('a', encoding='utf-8', newline='\n') as output:
        output.write(text)


if __name__ == '__main__':
    raise SystemExit(main())
