#!/usr/bin/env python3
"""Describe a baked resource tree: what it is made of, and what the pack formats cost over it.

Answers the questions a format decision needs and a size total cannot: how files and bytes are distributed
by size, how much of the tree is duplicated within and across packs, where compression actually pays by
extension and by size, and how large the merged index over every pack comes out raw and deflated.

Reads the tree and writes nothing but its report.
"""

import argparse
import hashlib
import json
import multiprocessing
import os
import struct
import sys
import zlib
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from package import RESOURCE_PACK_HEADER_SIZE, RESOURCE_PACK_MIN_COMPRESSED_SIZE, encode_resource_pack_blob

# Buckets a resource actually falls into: sprites and configs at the small end, audio and models at the top
SIZE_BUCKETS = [1024, 4096, 16384, 65536, 262144, 1048576, 8388608]
RESOURCE_INDEX_PACK_SIZE = 16
RESOURCE_INDEX_ENTRY_SIZE = 40


def bucket_label(size: int) -> str:
	for limit in SIZE_BUCKETS:
		if size < limit:
			return f'<{human(limit)}'

	return f'>={human(SIZE_BUCKETS[-1])}'


def human(size: float) -> str:
	for unit in ('B', 'KB', 'MB', 'GB'):
		if size < 1024 or unit == 'GB':
			return f'{size:.0f} {unit}' if unit == 'B' else f'{size:.1f} {unit}'.replace('.0 ', ' ')

		size /= 1024

	return f'{size:.1f} GB'


def scan_file(job: tuple[str, str, str, int, int]) -> dict:
	path_str, rel_path, pack_name, compress_level, min_gain_percent = job
	path = Path(path_str)
	data = path.read_bytes()
	codec, blob = encode_resource_pack_blob(data, compress_level, min_gain_percent)

	return {
		'pack': pack_name,
		'path': rel_path,
		'name': path.name,
		'ext': path.suffix.lower().lstrip('.') or '(none)',
		'size': len(data),
		'storedSize': len(blob),
		'deflated': codec == 1,
		'digest': hashlib.blake2b(data, digest_size=16).hexdigest(),
	}


def collect_jobs(baked_root: Path, compress_level: int, min_gain_percent: int) -> list[tuple[str, str, str, int, int]]:
	jobs: list[tuple[str, str, str, int, int]] = []

	# A dot directory beside the packs is the baker's own working state, not content anything ships
	for pack_dir in sorted(p for p in baked_root.iterdir() if p.is_dir() and not p.name.startswith('.')):
		for path in sorted(pack_dir.rglob('*')):
			if path.is_file():
				jobs.append((str(path), path.relative_to(pack_dir).as_posix(), pack_dir.name, compress_level, min_gain_percent))

	return jobs


def merged_index_size(files: list[dict], compress_level: int, min_gain_percent: int) -> dict:
	"""The .foindex over every pack, last pack winning a shared path.

	The bytes are laid out for real - records carrying the offsets and sizes the entries would actually hold -
	because a stand-in buffer of zeros deflates to nothing and would report an index far smaller than one.
	"""
	pack_names = sorted({f['pack'] for f in files})
	pack_index = {name: i for i, name in enumerate(pack_names)}
	merged: dict[str, dict] = {}
	data_offset: dict[str, int] = {name: RESOURCE_PACK_HEADER_SIZE for name in pack_names}

	for f in files:
		f = dict(f, dataOffset=data_offset[f['pack']])
		data_offset[f['pack']] += f['storedSize']
		merged[f['path']] = f

	pool = bytearray()
	pack_records = bytearray()

	for name in pack_names:
		encoded = name.encode('utf-8')
		pack_records += struct.pack('<IIQ', RESOURCE_INDEX_PACK_SIZE * len(pack_names) + RESOURCE_INDEX_ENTRY_SIZE * len(merged) + len(pool), len(encoded), 0)
		pool += encoded

	entry_records = bytearray()

	for path_name in sorted(merged):
		f = merged[path_name]
		encoded = path_name.encode('utf-8')
		entry_records += struct.pack(
			'<IIIIQQQ',
			RESOURCE_INDEX_PACK_SIZE * len(pack_names) + RESOURCE_INDEX_ENTRY_SIZE * len(merged) + len(pool),
			len(encoded),
			pack_index[f['pack']],
			1 if f['deflated'] else 0,
			f['dataOffset'],
			f['storedSize'],
			f['size'],
		)
		pool += encoded

	index = bytes(pack_records + entry_records + pool)
	codec, blob = encode_resource_pack_blob(index, compress_level, min_gain_percent)

	return {
		'entries': len(merged),
		'packs': len(pack_names),
		'rawBytes': len(index),
		'storedBytes': len(blob),
		'deflated': codec == 1,
	}


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument('--baked-root', required=True, help='directory holding one subdirectory per pack')
	parser.add_argument('--out', help='write the full report as JSON to this path')
	parser.add_argument('--compress-level', type=int, default=6, help='zlib level to measure the gain at')
	parser.add_argument('--min-gain-percent', type=int, default=5, help='the store-instead-of-deflate threshold')
	parser.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 4) // 2), help='files scanned in parallel')
	parser.add_argument('--top', type=int, default=15, help='how many largest files and extensions to list')
	args = parser.parse_args()

	baked_root = Path(args.baked_root)

	if not baked_root.is_dir():
		print(f'error: no baked root at {baked_root}', file=sys.stderr)
		return 1

	jobs = collect_jobs(baked_root, args.compress_level, args.min_gain_percent)

	if not jobs:
		print('error: no files to analyze', file=sys.stderr)
		return 1

	if args.jobs > 1:
		with multiprocessing.Pool(args.jobs) as pool:
			files = pool.map(scan_file, jobs, chunksize=64)
	else:
		files = [scan_file(job) for job in jobs]

	total_size = sum(f['size'] for f in files)
	total_stored = sum(f['storedSize'] for f in files)

	buckets: dict[str, dict] = defaultdict(lambda: {'files': 0, 'bytes': 0, 'stored': 0, 'deflated': 0})

	for f in files:
		bucket = buckets[bucket_label(f['size'])]
		bucket['files'] += 1
		bucket['bytes'] += f['size']
		bucket['stored'] += f['storedSize']
		bucket['deflated'] += 1 if f['deflated'] else 0

	extensions: dict[str, dict] = defaultdict(lambda: {'files': 0, 'bytes': 0, 'stored': 0})

	for f in files:
		ext = extensions[f['ext']]
		ext['files'] += 1
		ext['bytes'] += f['size']
		ext['stored'] += f['storedSize']

	by_digest: dict[str, list[dict]] = defaultdict(list)

	for f in files:
		by_digest[f['digest']].append(f)

	duplicate_groups = [group for group in by_digest.values() if len(group) > 1]
	duplicate_files = sum(len(group) - 1 for group in duplicate_groups)
	duplicate_bytes = sum(group[0]['size'] * (len(group) - 1) for group in duplicate_groups)
	cross_pack_groups = [group for group in duplicate_groups if len({f['pack'] for f in group}) > 1]

	index = merged_index_size(files, args.compress_level, args.min_gain_percent)

	ordered_buckets = [f'<{human(limit)}' for limit in SIZE_BUCKETS] + [f'>={human(SIZE_BUCKETS[-1])}']

	print(f'Files {len(files)}, {human(total_size)} raw, {human(total_stored)} stored ({total_stored * 100 / total_size:.1f} %)')
	print()
	print('| Size bucket | Files | Bytes | Stored | Deflated |')
	print('|-------------|------:|------:|-------:|---------:|')

	for label in ordered_buckets:
		if label in buckets:
			b = buckets[label]
			print(f'| {label} | {b["files"]} | {human(b["bytes"])} | {human(b["stored"])} | {b["deflated"] * 100 // b["files"]} % |')

	print()
	print('| Extension | Files | Bytes | Stored | Gain |')
	print('|-----------|------:|------:|-------:|-----:|')

	for ext, e in sorted(extensions.items(), key=lambda kv: -kv[1]['bytes'])[:args.top]:
		gain = (1 - e['stored'] / e['bytes']) * 100 if e['bytes'] else 0
		print(f'| {ext} | {e["files"]} | {human(e["bytes"])} | {human(e["stored"])} | {gain:.0f} % |')

	print()
	print(f'Duplicates: {duplicate_files} redundant file(s), {human(duplicate_bytes)}, in {len(duplicate_groups)} group(s); {len(cross_pack_groups)} group(s) span packs')
	print(f'Merged index: {index["entries"]} entries over {index["packs"]} packs, {human(index["rawBytes"])} raw, {human(index["storedBytes"])} stored, deflated {index["deflated"]}')
	print()
	print('| Largest file | Pack | Bytes | Stored |')
	print('|--------------|------|------:|-------:|')

	for f in sorted(files, key=lambda f: -f['size'])[:args.top]:
		print(f'| {f["name"]} | {f["pack"]} | {human(f["size"])} | {human(f["storedSize"])} |')

	if args.out:
		report = {
			'totals': {'files': len(files), 'rawBytes': total_size, 'storedBytes': total_stored},
			'buckets': {label: buckets[label] for label in ordered_buckets if label in buckets},
			'extensions': extensions,
			'duplicates': {'files': duplicate_files, 'bytes': duplicate_bytes, 'groups': len(duplicate_groups), 'crossPackGroups': len(cross_pack_groups)},
			'mergedIndex': index,
			'minCompressedSize': RESOURCE_PACK_MIN_COMPRESSED_SIZE,
		}
		Path(args.out).write_text(json.dumps(report, indent=2), encoding='utf-8')
		print(f'\nwritten: {args.out}')

	return 0


if __name__ == '__main__':
	sys.exit(main())
