#!/usr/bin/env python3
"""Readelf-based ELF metadata reading and closed-bundle validation for portable packages."""
from __future__ import annotations

import hashlib
import os
import re
import struct
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

ELF_MAGIC = b'\x7fELF'
ELFCLASS32 = 1
ELFCLASS64 = 2
ELFDATA2LSB = 1
ELFDATA2MSB = 2
ET_EXEC = 2
ET_DYN = 3
READELF_TIMEOUT_SECONDS = 30

INTERPRETER_PATTERN = re.compile(r'\[Requesting program interpreter: ([^\]]+)\]')
NEEDED_PATTERN = re.compile(r'\(NEEDED\)\s+Shared library: \[([^\]]+)\]')
SONAME_PATTERN = re.compile(r'\(SONAME\)\s+Library soname: \[([^\]]+)\]')
RPATH_PATTERN = re.compile(r'\(RPATH\)\s+Library rpath: \[([^\]]*)\]')
SECTION_PATTERN = re.compile(r'^Version (definition|needs|symbols) section')
FILE_PATTERN = re.compile(r'\bFile:\s+(\S+)')
NAME_PATTERN = re.compile(r'\bName:\s+(\S+)')
FLAGS_PATTERN = re.compile(r'\bFlags:\s+(\S+)')


class ElfError(ValueError):
	"""ELF metadata or a bundle closure a portable package cannot ship."""


@dataclass(frozen=True)
class ElfInfo:
	path: Path
	bits: int
	machine: int
	interpreter: str | None
	needed: tuple[str, ...]
	soname: str | None
	required_versions: Mapping[str, frozenset[str]]
	provided_versions: frozenset[str]
	rpath: tuple[str, ...] = ()
	has_runpath: bool = False


def inspect_elf(path: Path, *, readelf: str = 'readelf') -> ElfInfo:
	"""Read one ELF file's header fields and readelf metadata into an immutable ElfInfo."""
	bits, machine = read_header(path)
	stdout = run_readelf(path, readelf)
	interpreter, needed, soname, required_versions, provided_versions = parse_readelf_output(stdout)
	rpath = RPATH_PATTERN.search(stdout)
	return ElfInfo(path, bits, machine, interpreter, needed, soname, required_versions, provided_versions,
		tuple(rpath.group(1).split(':')) if rpath else (), '(RUNPATH)' in stdout)


def validate_elf_closure(root: Path, *, readelf: str = 'readelf') -> list[ElfInfo]:
	"""Inspect every ELF under root and prove the bundle closes: one architecture, every NEEDED and
	required version satisfied by bundled providers, and the interpreter bundled rather than hosted."""
	if not root.is_dir():
		if root.exists():
			raise ElfError(f'ELF bundle root is not a directory: {root}')
		raise FileNotFoundError(root)

	root_resolved = root.resolve()
	elf_paths, alias_names = collect_elf_files(root_resolved)
	if not elf_paths:
		raise ElfError(f'No ELF files found under bundle root: {root}')

	infos = [inspect_elf(path, readelf=readelf) for path in elf_paths]
	require_single_architecture(infos)

	for info in infos:
		require_bundled_interpreter(info, root_resolved, set(elf_paths))
		require_private_rpath(info, root_resolved)

	providers = collect_providers(infos, alias_names)
	for name, candidates in providers.items():
		if len(candidates) > 1:
			paths = ', '.join(sorted(str(candidate.path) for candidate in candidates.values()))
			raise ElfError(f'Bundled provider {name} is ambiguous between different ELF files: {paths}')
	for info in infos:
		require_closed_dependencies(info, providers)

	return infos


def require_private_rpath(info: ElfInfo, root: Path) -> None:
	"""Old DT_RPATH precedes --library-path; only paths confined to the relocatable bundle are safe."""
	if info.has_runpath:
		return
	for entry in info.rpath:
		origin = next((token for token in ('$ORIGIN', '${ORIGIN}')
			if entry == token or entry.startswith(token + '/')), None)
		if origin is None or '$' in entry[len(origin):] or not (
			info.path.parent / entry[len(origin):].lstrip('/')).resolve().is_relative_to(root):
			raise ElfError(f'ELF file {info.path} has non-private DT_RPATH entry {entry!r}; use DT_RUNPATH or a bundled $ORIGIN path')


def read_header(path: Path) -> tuple[int, int]:
	"""Read bits and machine from the first 64 bytes of the file, never from readelf's machine name."""
	with path.open('rb') as stream:
		header = stream.read(64)

	if len(header) < 64:
		raise ElfError(f'ELF file {path} is truncated to {len(header)} header bytes')
	if header[:4] != ELF_MAGIC:
		raise ElfError(f'File does not start with the ELF magic: {path}')

	elf_class = header[4]
	if elf_class == ELFCLASS32:
		bits = 32
	elif elf_class == ELFCLASS64:
		bits = 64
	else:
		raise ElfError(f'ELF file {path} has unknown class {elf_class}')

	elf_data = header[5]
	if elf_data == ELFDATA2LSB:
		endian = '<'
	elif elf_data == ELFDATA2MSB:
		endian = '>'
	else:
		raise ElfError(f'ELF file {path} has unknown data encoding {elf_data}')

	elf_type, machine = struct.unpack_from(endian + 'HH', header, 16)
	if elf_type not in (ET_EXEC, ET_DYN):
		raise ElfError(f'ELF file {path} has type {elf_type}, expected ET_EXEC or ET_DYN')

	return bits, machine


def run_readelf(path: Path, readelf: str) -> str:
	"""Run one readelf invocation and reject any tool failure: nonzero exit, stderr, or a timeout."""
	command = [readelf, '--wide', '--program-headers', '--dynamic', '--version-info', str(path)]
	env = dict(os.environ)
	env['LC_ALL'] = 'C'

	try:
		process = subprocess.run(
			command,
			capture_output=True,
			text=True,
			encoding='utf-8',
			errors='replace',
			timeout=READELF_TIMEOUT_SECONDS,
			env=env,
		)
	except subprocess.TimeoutExpired as error:
		raise ElfError(f'readelf timed out after {READELF_TIMEOUT_SECONDS} seconds on {path}') from error

	if process.returncode != 0:
		raise ElfError(f'readelf exited with code {process.returncode} for {path}: {process.stderr.strip()}')
	if process.stderr:
		raise ElfError(f'readelf wrote diagnostics for {path}: {process.stderr.strip()}')

	return process.stdout


def parse_readelf_output(
	stdout: str,
) -> tuple[str | None, tuple[str, ...], str | None, dict[str, frozenset[str]], frozenset[str]]:
	"""Split readelf output into interpreter, NEEDED, SONAME, required and provided versions."""
	interpreter = None
	needed: list[str] = []
	soname = None
	required: dict[str, set[str]] = {}
	provided: set[str] = set()
	section = None
	current_file = None

	for line in stdout.splitlines():
		interpreter_match = INTERPRETER_PATTERN.search(line)
		if interpreter_match is not None:
			interpreter = interpreter_match.group(1)

		needed_match = NEEDED_PATTERN.search(line)
		if needed_match is not None:
			needed.append(needed_match.group(1))

		soname_match = SONAME_PATTERN.search(line)
		if soname_match is not None:
			soname = soname_match.group(1)

		section_match = SECTION_PATTERN.match(line)
		if section_match is not None:
			section = section_match.group(1)
			current_file = None
			continue

		# 'symbols' rows are usage references and are never counted as provided definitions
		if section == 'definition':
			name_match = NAME_PATTERN.search(line)
			flags_match = FLAGS_PATTERN.search(line)
			is_base = flags_match is not None and 'BASE' in flags_match.group(1)
			if name_match is not None and not is_base:
				provided.add(name_match.group(1))
		elif section == 'needs':
			file_match = FILE_PATTERN.search(line)
			if file_match is not None:
				current_file = file_match.group(1)
				continue

			name_match = NAME_PATTERN.search(line)
			if name_match is not None and current_file is not None:
				required.setdefault(current_file, set()).add(name_match.group(1))

	return (
		interpreter,
		tuple(needed),
		soname,
		{name: frozenset(versions) for name, versions in required.items()},
		frozenset(provided),
	)


def collect_elf_files(root: Path) -> tuple[list[Path], dict[Path, set[str]]]:
	"""Walk the bundle deterministically, reject escaping or broken links, and collect every ELF
	regular file by magic regardless of its filename, keyed by resolved path with alias names kept."""
	elf_paths: set[Path] = set()
	alias_names: dict[Path, set[str]] = {}

	for current, dirnames, filenames in os.walk(root, followlinks=False):
		dirnames.sort()
		filenames.sort()
		current_path = Path(current)

		for name in dirnames + filenames:
			path = current_path / name
			resolved = path.resolve()

			if not is_within(resolved, root):
				raise ElfError(f'Resolved path {resolved} of {path} escapes the bundle root {root}')
			if path.is_symlink() and not resolved.exists():
				raise ElfError(f'Symlink {path} is broken: {resolved}')
			if not resolved.is_file():
				continue
			if not has_elf_magic(resolved):
				continue

			elf_paths.add(resolved)
			alias_names.setdefault(resolved, set()).add(path.name)

	return sorted(elf_paths), alias_names


def is_within(path: Path, root: Path) -> bool:
	try:
		path.relative_to(root)
	except ValueError:
		return False
	return True


def has_elf_magic(path: Path) -> bool:
	with path.open('rb') as stream:
		return stream.read(4) == ELF_MAGIC


def require_single_architecture(infos: list[ElfInfo]) -> None:
	first = infos[0]

	for info in infos[1:]:
		if info.bits != first.bits or info.machine != first.machine:
			raise ElfError(
				f'Bundled ELF architectures differ: {first.path} is {first.bits}-bit machine {first.machine}, '
				f'{info.path} is {info.bits}-bit machine {info.machine}')


def require_bundled_interpreter(info: ElfInfo, root: Path, bundled: set[Path]) -> None:
	if info.interpreter is None:
		return

	stripped = info.interpreter.lstrip('/')
	if not stripped:
		raise ElfError(f'ELF file {info.path} requests an empty program interpreter path')

	if (root / stripped).resolve() not in bundled:
		raise ElfError(
			f'ELF file {info.path} requests program interpreter {info.interpreter}, '
			f'which is not bundled under {root}')


def collect_providers(infos: list[ElfInfo], alias_names: dict[Path, set[str]]) -> dict[str, dict[str, ElfInfo]]:
	"""Index every provider by bundle-facing names and a content digest, so identical names carrying
	different bytes stay separate candidates instead of one silently winning."""
	providers: dict[str, dict[str, ElfInfo]] = {}

	for info in infos:
		digest = file_sha256(info.path)
		names = set(alias_names.get(info.path, ()))

		for name in sorted(names):
			providers.setdefault(name, {})[digest] = info

	return providers


def require_closed_dependencies(info: ElfInfo, providers: dict[str, dict[str, ElfInfo]]) -> None:
	for needed in info.needed:
		candidates = providers.get(needed, {})
		if not candidates:
			raise ElfError(f'ELF file {info.path} needs {needed}, which no bundled file provides')

		if len(candidates) > 1:
			paths = ', '.join(sorted(str(candidate.path) for candidate in candidates.values()))
			raise ElfError(f'ELF file {info.path} needs {needed}, which is ambiguous in the bundle: {paths}')

		provider = next(iter(candidates.values()))
		missing = info.required_versions.get(needed, frozenset()) - provider.provided_versions
		if missing:
			raise ElfError(
				f'ELF file {info.path} needs {needed} versions {sorted(missing)}, '
				f'which {provider.path} does not provide (it provides {sorted(provider.provided_versions)})')


def file_sha256(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open('rb') as stream:
		while chunk := stream.read(1024 * 1024):
			digest.update(chunk)
	return digest.hexdigest()
