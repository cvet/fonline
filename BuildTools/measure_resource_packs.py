#!/usr/bin/env python3
"""Measure the .fores resource pack format against ZIP over a real baked tree.

Writes every pack directory under the baked root in both formats, one at a time, and reports what each
costs: bytes shipped, the bytes a reader must pull to mount, entry count, the stored/deflate split, and
write time. Answers the shipped-size half of the plan's abort criterion; mount time and resident memory
need the engine's own readers and are not measured here.

Artifacts are deleted as soon as they are measured, so peak disk use is one pack in two formats.
"""

import argparse
import json
import multiprocessing
import os
import struct
import sys
import tempfile
import time
import zipfile
import zlib
from collections.abc import Sequence
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from package import RESOURCE_PACK_CODEC_DEFLATE, RESOURCE_PACK_ENTRY_SIZE, RESOURCE_PACK_HEADER_SIZE, write_resource_pack

ZIP_EOCD_SIGNATURE = b'PK\x05\x06'
ZIP_EOCD_MIN_SIZE = 22
ZIP_EOCD_SIZE_FIELD = 12


def collect_entries(pack_dir: Path) -> list[tuple[str, Path]]:
	entries: list[tuple[str, Path]] = []

	for path in sorted(pack_dir.rglob('*')):
		if path.is_file():
			entries.append((path.relative_to(pack_dir).as_posix(), path))

	return entries


def read_fores_shape(pack_path: Path) -> dict:
	"""Index cost and codec split, read back from the written file so the numbers come from the artifact."""
	with open(pack_path, 'rb') as handle:
		header = handle.read(RESOURCE_PACK_HEADER_SIZE)
		index_offset, index_stored_size, index_decoded_size = struct.unpack_from('<QQQ', header, 16)
		index_codec, entry_count = struct.unpack_from('<II', header, 40)

		handle.seek(index_offset)
		index = handle.read(index_stored_size)

	if index_codec == RESOURCE_PACK_CODEC_DEFLATE:
		index = zlib.decompress(index)

	assert len(index) == index_decoded_size, 'Index did not decode to the size the header declares'

	stored_count = 0
	stored_bytes = 0
	deflate_count = 0
	deflate_bytes = 0

	for entry in range(entry_count):
		stored_size, decoded_size = struct.unpack_from('<QQ', index, entry * 40 + 16)
		codec = struct.unpack_from('<I', index, entry * 40 + 32)[0]

		if codec == RESOURCE_PACK_CODEC_DEFLATE:
			deflate_count += 1
			deflate_bytes += decoded_size
		else:
			stored_count += 1
			stored_bytes += decoded_size

	return {
		'mountReadBytes': index_stored_size,
		'indexDecodedBytes': index_decoded_size,
		'indexCompressed': index_codec == RESOURCE_PACK_CODEC_DEFLATE,
		'storedCount': stored_count,
		'storedSourceBytes': stored_bytes,
		'deflateCount': deflate_count,
		'deflateSourceBytes': deflate_bytes,
	}


def read_fores_entries(pack_path: Path) -> dict[str, bytes]:
	"""Every entry decoded back out of the written pack, keyed by path, so the check reads the artifact."""
	with open(pack_path, 'rb') as handle:
		header = handle.read(RESOURCE_PACK_HEADER_SIZE)
		index_offset, index_stored_size, index_decoded_size = struct.unpack_from('<QQQ', header, 16)
		index_codec, entry_count = struct.unpack_from('<II', header, 40)

		handle.seek(index_offset)
		index = handle.read(index_stored_size)

		if index_codec == RESOURCE_PACK_CODEC_DEFLATE:
			index = zlib.decompress(index)

		assert len(index) == index_decoded_size, 'Index did not decode to the size the header declares'

		entries: dict[str, bytes] = {}

		for entry in range(entry_count):
			record = entry * RESOURCE_PACK_ENTRY_SIZE
			path_offset, path_length = struct.unpack_from('<II', index, record)
			data_offset, stored_size, decoded_size = struct.unpack_from('<QQQ', index, record + 8)
			codec = struct.unpack_from('<I', index, record + 32)[0]
			name = index[path_offset:path_offset + path_length].decode('utf-8')

			handle.seek(data_offset)
			blob = handle.read(stored_size)

			if codec == RESOURCE_PACK_CODEC_DEFLATE:
				blob = zlib.decompress(blob)

			assert len(blob) == decoded_size, 'Entry did not decode to the size the index declares: ' + name
			assert name not in entries, 'Pack holds the same path twice: ' + name
			entries[name] = blob

	return entries


def diff_against_source(entries: dict[str, bytes], source: Sequence[tuple[str, Path]], label: str) -> list[str]:
	"""Names and raw bytes, both directions - a format that loses a file is as wrong as one that corrupts it."""
	problems: list[str] = []
	expected = {name: path for name, path in source}

	for name in sorted(set(expected) - set(entries)):
		problems.append(f'{label}: missing {name}')

	for name in sorted(set(entries) - set(expected)):
		problems.append(f'{label}: unexpected {name}')

	for name in sorted(set(expected) & set(entries)):
		if entries[name] != expected[name].read_bytes():
			problems.append(f'{label}: bytes differ for {name}')

	return problems


def read_zip_directory_size(zip_path: Path) -> int:
	"""The central directory is what a zip reader pulls to build its file tree, so it is the mount cost."""
	size = zip_path.stat().st_size
	tail_size = min(size, 65536 + ZIP_EOCD_MIN_SIZE)

	with open(zip_path, 'rb') as handle:
		handle.seek(size - tail_size)
		tail = handle.read(tail_size)

	position = tail.rfind(ZIP_EOCD_SIGNATURE)
	assert position >= 0, 'Zip has no end-of-central-directory record'

	return struct.unpack_from('<I', tail, position + ZIP_EOCD_SIZE_FIELD)[0]


def measure_pack(job: tuple[str, str, str, int, int, bool]) -> dict:
	pack_name, pack_dir, work_dir, compress_level, min_gain_percent, verify = job
	entries = collect_entries(Path(pack_dir))

	if not entries:
		return {'pack': pack_name, 'skipped': 'no files'}

	source_bytes = sum(path.stat().st_size for _, path in entries)
	result = {'pack': pack_name, 'entryCount': len(entries), 'sourceBytes': source_bytes}
	problems: list[str] = []

	fores_path = Path(work_dir) / f'{pack_name}.fores'
	started = time.perf_counter()
	write_resource_pack(fores_path, entries, compress_level, min_gain_percent)
	result['foresWriteSeconds'] = round(time.perf_counter() - started, 3)
	result['foresBytes'] = fores_path.stat().st_size
	result.update(read_fores_shape(fores_path))

	if verify:
		problems.extend(diff_against_source(read_fores_entries(fores_path), entries, 'fores'))

	fores_path.unlink()

	zip_path = Path(work_dir) / f'{pack_name}.zip'
	started = time.perf_counter()

	with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=compress_level) as archive:
		for name, path in entries:
			archive.write(path, name)

	result['zipWriteSeconds'] = round(time.perf_counter() - started, 3)
	result['zipBytes'] = zip_path.stat().st_size
	result['zipMountReadBytes'] = read_zip_directory_size(zip_path)

	if verify:
		with zipfile.ZipFile(zip_path) as archive:
			zip_entries = {name: archive.read(name) for name in archive.namelist()}

		problems.extend(diff_against_source(zip_entries, entries, 'zip'))

	zip_path.unlink()
	result['problems'] = problems

	return result


def format_report(results: list[dict]) -> str:
	measured = [r for r in results if 'skipped' not in r]
	lines = [
		'| Pack | Files | Source | .fores | .zip | Delta | Mount read .fores | Mount read .zip | Stored |',
		'|------|------:|-------:|-------:|-----:|------:|------------------:|----------------:|-------:|',
	]

	for row in sorted(measured, key=lambda r: -r['sourceBytes']):
		delta = row['foresBytes'] - row['zipBytes']
		stored_share = row['storedCount'] * 100 // row['entryCount'] if row['entryCount'] else 0
		lines.append(
			'| {} | {} | {} | {} | {} | {}{} | {} | {} | {} % |'.format(
				row['pack'],
				row['entryCount'],
				mib(row['sourceBytes']),
				mib(row['foresBytes']),
				mib(row['zipBytes']),
				'+' if delta > 0 else '',
				mib(delta),
				kib(row['mountReadBytes']),
				kib(row['zipMountReadBytes']),
				stored_share,
			)
		)

	totals = {key: sum(row[key] for row in measured) for key in ('sourceBytes', 'foresBytes', 'zipBytes', 'mountReadBytes', 'zipMountReadBytes', 'entryCount')}
	delta = totals['foresBytes'] - totals['zipBytes']
	lines.append(
		'| **Total** | **{}** | **{}** | **{}** | **{}** | **{}{}** | **{}** | **{}** | |'.format(
			totals['entryCount'],
			mib(totals['sourceBytes']),
			mib(totals['foresBytes']),
			mib(totals['zipBytes']),
			'+' if delta > 0 else '',
			mib(delta),
			kib(totals['mountReadBytes']),
			kib(totals['zipMountReadBytes']),
		)
	)

	return '\n'.join(lines)


def mib(value: int) -> str:
	return f'{value / (1024 * 1024):.1f} MB'


def kib(value: int) -> str:
	return f'{value / 1024:.0f} KB'


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument('--baked-root', required=True, help='directory holding one subdirectory per pack')
	parser.add_argument('--out', help='write the measurements as JSON to this path')
	parser.add_argument('--packs', help='comma-separated pack names, default every subdirectory')
	parser.add_argument('--compress-level', type=int, default=6, help='zlib level for both formats')
	parser.add_argument('--min-gain-percent', type=int, default=5, help='the .fores store-instead-of-deflate threshold')
	parser.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 4) // 2), help='packs measured in parallel')
	parser.add_argument('--verify', action='store_true', help='read every entry back out of both artifacts and diff names and raw bytes against the source')
	args = parser.parse_args()

	baked_root = Path(args.baked_root)

	if not baked_root.is_dir():
		print(f'error: no baked root at {baked_root}', file=sys.stderr)
		return 1

	wanted = set(args.packs.split(',')) if args.packs else None
	pack_dirs = [path for path in sorted(baked_root.iterdir()) if path.is_dir() and (wanted is None or path.name in wanted)]

	if not pack_dirs:
		print('error: no pack directories to measure', file=sys.stderr)
		return 1

	with tempfile.TemporaryDirectory(prefix='fores_measure_') as work_dir:
		jobs = [(path.name, str(path), work_dir, args.compress_level, args.min_gain_percent, args.verify) for path in pack_dirs]

		if args.jobs > 1:
			with multiprocessing.Pool(args.jobs) as pool:
				results = pool.map(measure_pack, jobs)
		else:
			results = [measure_pack(job) for job in jobs]

	report = format_report(results)
	print(report)

	problems = [problem for row in results for problem in row.get('problems', [])]

	if args.verify:
		print()

		for problem in problems[:50]:
			print(problem)

		print('verify: {} entr(ies) differed'.format(len(problems)) if problems else 'verify: every entry round-trips, names and bytes')

	if args.out:
		Path(args.out).write_text(json.dumps({'compressLevel': args.compress_level, 'minGainPercent': args.min_gain_percent, 'packs': results}, indent=2), encoding='utf-8')
		print(f'\nwritten: {args.out}')

	return 1 if args.verify and problems else 0


if __name__ == '__main__':
	sys.exit(main())
