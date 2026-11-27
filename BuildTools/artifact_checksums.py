"""Write or verify SHA-256 checksums of finished release packages.

The default selection is every root-level file of the package directory whose
name ends with a release-archive suffix (.zip, .tar, .tar.gz, .msi or .apk,
compared case-insensitively). Each selected file receives a sibling
``<name>.sha256`` sidecar of one line, ``<lowercase hex>  <leaf name>``; an
optional manifest at the directory root lists every selected file by its
root-relative path in the same format, so ``sha256sum --check`` accepts both.

Every selected input is hashed before any output is staged, outputs are written
to temporary files and renamed into place, and a file that changes while it is
hashed is rejected. Package writers must have stopped before the command runs.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import stat
import sys
import tempfile
from typing import NamedTuple


DEFAULT_SUFFIXES = ('.zip', '.tar', '.tar.gz', '.msi', '.apk')
SIDECAR_SUFFIX = '.sha256'
HASH_CHUNK_SIZE = 1024 * 1024
_UNSAFE_CHARACTERS = frozenset('\\\r\n\0<>:"|?*')


class ArtifactDigest(NamedTuple):
    path: Path
    relative: str
    digest: str
    identity: tuple[int, ...]


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog='artifact_checksums.py',
        description='Write or verify SHA-256 sidecars and an optional manifest for finished release packages.')
    parser.add_argument('directory', type=Path, help='finished package directory, before publication')
    parser.add_argument('--artifact', action='append', metavar='RELATIVE_PATH',
                        help='select this file, relative to the directory, instead of scanning the directory root; repeatable')
    parser.add_argument('--suffix', action='append', metavar='SUFFIX',
                        help='case-insensitive file-name suffix to select; repeatable; replaces the default .zip, .tar, .tar.gz, .msi and .apk')
    parser.add_argument('--manifest', metavar='FILENAME',
                        help='also write (or verify) one manifest of every selected file at the directory root, for example SHA256SUMS')
    parser.add_argument('--no-sidecars', action='store_true',
                        help='write or verify only the manifest; requires --manifest')
    parser.add_argument('--verify', action='store_true',
                        help='compare existing sidecars and manifest with the current file bytes instead of writing them')
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = create_parser()
    args = parser.parse_args(argv)
    if args.no_sidecars and not args.manifest:
        parser.error('--no-sidecars requires --manifest')

    try:
        suffixes = normalize_suffixes(args.suffix) if args.suffix else DEFAULT_SUFFIXES
        if args.verify:
            count = verify_checksums(args.directory, args.artifact, suffixes=suffixes, manifest=args.manifest,
                                     sidecars=not args.no_sidecars)
            print(f'artifact_checksums: verified {count} artifact(s) in {args.directory}')
        else:
            count = write_checksums(args.directory, args.artifact, suffixes=suffixes, manifest=args.manifest,
                                    sidecars=not args.no_sidecars)
            print(f'artifact_checksums: wrote checksums of {count} artifact(s) in {args.directory}')
    except (OSError, ValueError) as error:
        print(f'artifact_checksums: {error}', file=sys.stderr)
        return 1
    return 0


def normalize_suffixes(values: list[str]) -> tuple[str, ...]:
    suffixes = []
    for value in values:
        suffix = value.lower()
        if not suffix.startswith('.') or len(suffix) < 2 or any(character in _UNSAFE_CHARACTERS or character == '/' for character in suffix):
            raise ValueError(f'Invalid artifact suffix: {value!r}')
        suffixes.append(suffix)
    return tuple(sorted(set(suffixes)))


def write_checksums(directory: Path, selected: list[str] | None = None, *, suffixes: tuple[str, ...] | None = DEFAULT_SUFFIXES,
                    manifest: str | None = None, sidecars: bool = True) -> int:
    """Hash the selected artifacts and atomically write their sidecars and manifest; return the artifact count."""

    root = resolve_root(directory)
    manifest_path = manifest_destination(root, manifest)
    artifacts = select_artifacts(root, selected, suffixes, manifest_path)

    # A read failure of any package leaves the previous set of sums untouched
    records = [ArtifactDigest(artifact, relative_name(root, artifact), *hash_artifact(artifact)) for artifact in artifacts]
    for record in records:
        if file_identity(record.path.stat()) != record.identity:
            raise OSError(f'Artifact changed after hashing: {record.path}')

    outputs: list[tuple[Path, str]] = []
    if sidecars:
        outputs += [(sidecar_path(record.path), f'{record.digest}  {record.path.name}\n') for record in records]
    if manifest_path is not None:
        outputs.append((manifest_path, manifest_text(records)))
    for destination, _ in outputs:
        validate_destination(destination)

    write_atomically(outputs)
    return len(records)


def verify_checksums(directory: Path, selected: list[str] | None = None, *, suffixes: tuple[str, ...] | None = DEFAULT_SUFFIXES,
                     manifest: str | None = None, sidecars: bool = True) -> int:
    """Compare existing sidecars and manifest entries with the current bytes; return the artifact count."""

    root = resolve_root(directory)
    manifest_path = manifest_destination(root, manifest)
    artifacts = select_artifacts(root, selected, suffixes, manifest_path)
    digests = {artifact: hash_artifact(artifact)[0] for artifact in artifacts}

    problems = []
    if sidecars:
        for artifact, digest in digests.items():
            expected = f'{digest}  {artifact.name}\n'
            sidecar = sidecar_path(artifact)
            if not sidecar.is_file():
                problems.append(f'missing sidecar {relative_name(root, sidecar)}')
            elif sidecar.read_bytes().decode('utf-8') != expected:
                problems.append(f'sidecar does not match {relative_name(root, artifact)}')

    if manifest_path is not None:
        if not manifest_path.is_file():
            problems.append(f'missing manifest {manifest_path.name}')
        else:
            listed = parse_manifest(manifest_path.read_bytes().decode('utf-8'))
            for name in sorted(listed):
                artifact = root / Path(validate_relative_name(name))
                if artifact not in digests:
                    digests[artifact] = hash_artifact(checked_artifact(root, name))[0]
                if digests[artifact] != listed[name]:
                    problems.append(f'manifest entry does not match {name}')
            for artifact in artifacts:
                if relative_name(root, artifact) not in listed:
                    problems.append(f'manifest does not list {relative_name(root, artifact)}')

    if problems:
        raise ValueError('Checksum verification failed: ' + '; '.join(problems))
    return len(artifacts)


def resolve_root(directory: Path) -> Path:
    root = directory.expanduser().resolve(strict=True)
    if not root.is_dir():
        raise ValueError(f'Package path is not a directory: {root}')
    return root


def manifest_destination(root: Path, manifest: str | None) -> Path | None:
    if manifest is None:
        return None
    if not manifest or manifest in ('.', '..') or '/' in manifest or any(character in _UNSAFE_CHARACTERS for character in manifest):
        raise ValueError(f'Manifest must be a plain file name: {manifest!r}')
    return root / manifest


def select_artifacts(root: Path, selected: list[str] | None, suffixes: tuple[str, ...] | None,
                     manifest_path: Path | None = None) -> list[Path]:
    """Return the sorted artifact paths; None suffixes accept any regular file that is not a checksum output."""

    if selected is None:
        names = [path.name for path in root.iterdir()
                 if not path.is_dir() and is_candidate(path.name, suffixes) and path != manifest_path]
    else:
        names = selected
    artifacts: set[Path] = set()
    for name in names:
        validate_relative_name(name)
        if not is_candidate(PurePosixPath(name).name, suffixes):
            raise ValueError(f'Not a selected release artifact: {name!r}')
        artifact = checked_artifact(root, name)
        if artifact == manifest_path:
            raise ValueError(f'The manifest cannot be an artifact: {name!r}')
        validate_destination(sidecar_path(artifact))
        artifacts.add(artifact)
    if not artifacts:
        described = ', '.join(suffixes) if suffixes is not None else 'any suffix'
        raise ValueError(f'No release artifacts ({described}) found in {root}')
    return sorted(artifacts)


def is_candidate(name: str, suffixes: tuple[str, ...] | None) -> bool:
    lowered = name.lower()
    if lowered.endswith(SIDECAR_SUFFIX) or (name.startswith('.') and lowered.endswith('.tmp')):
        return False
    return suffixes is None or any(lowered.endswith(suffix) and len(lowered) > len(suffix) for suffix in suffixes)


def validate_relative_name(name: str) -> str:
    relative = Path(name)
    if (not name or relative.is_absolute() or PurePosixPath(name).is_absolute() or PureWindowsPath(name).drive
            or '..' in relative.parts or any(character in _UNSAFE_CHARACTERS for character in name)):
        raise ValueError(f'Unsafe artifact path: {name!r}')
    return name


def checked_artifact(root: Path, name: str) -> Path:
    artifact = root / Path(validate_relative_name(name))
    for path in (artifact, *artifact.parents):
        if path == root:
            break
        if path.is_symlink() or path.is_junction():
            raise ValueError(f'Artifact path must not contain links: {name!r}')
    artifact.resolve(strict=True).relative_to(root)
    if not stat.S_ISREG(artifact.stat().st_mode):
        raise ValueError(f'Artifact is not a regular file: {name!r}')
    return artifact


def sidecar_path(artifact: Path) -> Path:
    return artifact.with_name(artifact.name + SIDECAR_SUFFIX)


def relative_name(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def validate_destination(path: Path) -> None:
    if path.is_symlink() or (path.exists() and not stat.S_ISREG(path.stat().st_mode)):
        raise ValueError(f'Checksum destination is not a regular file: {path}')


def manifest_text(records: list[ArtifactDigest]) -> str:
    return ''.join(f'{record.digest}  {record.relative}\n' for record in sorted(records, key=lambda record: record.relative))


def parse_manifest(text: str) -> dict[str, str]:
    entries: dict[str, str] = {}
    for number, line in enumerate(text.split('\n')[:-1] if text.endswith('\n') else text.split('\n'), start=1):
        digest, separator, name = line.partition('  ')
        if not separator or len(digest) != 64 or any(character not in '0123456789abcdef' for character in digest) or not name:
            raise ValueError(f'Malformed manifest line {number}: {line!r}')
        if name in entries:
            raise ValueError(f'Duplicate manifest entry: {name!r}')
        entries[name] = digest
    return entries


def write_atomically(outputs: list[tuple[Path, str]]) -> None:
    staged: list[tuple[Path, Path]] = []
    try:
        for destination, content in outputs:
            with tempfile.NamedTemporaryFile(mode='wb', dir=destination.parent, prefix=f'.{destination.name}.',
                                             suffix='.tmp', delete=False) as output:
                temporary = Path(output.name)
                staged.append((temporary, destination))
                output.write(content.encode('utf-8'))
                output.flush()
                os.fsync(output.fileno())
            os.chmod(temporary, 0o644)

        # Files are closed before the rename, which Windows requires
        for temporary, destination in staged:
            os.replace(temporary, destination)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def hash_artifact(path: Path) -> tuple[str, tuple[int, ...]]:
    digest = hashlib.sha256()
    with path.open('rb') as source:
        identity = file_identity(os.fstat(source.fileno()))
        while chunk := source.read(HASH_CHUNK_SIZE):
            digest.update(chunk)
        if file_identity(os.fstat(source.fileno())) != identity:
            raise OSError(f'Artifact changed while hashing: {path}')
    if file_identity(path.stat()) != identity:
        raise OSError(f'Artifact replaced while hashing: {path}')
    return digest.hexdigest(), identity


def file_identity(metadata: os.stat_result) -> tuple[int, ...]:
    return (metadata.st_dev, metadata.st_ino, metadata.st_size, metadata.st_mtime_ns)


if __name__ == '__main__':
    raise SystemExit(main())
