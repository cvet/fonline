#!/usr/bin/env python3
"""Build the target-platform managed class-library payload shipped in resource packs."""
from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
from pathlib import Path


MANIFEST_NAME = 'runtime.manifest'


def is_managed_pe_assembly(path: Path) -> bool:
	"""Return whether a PE file declares a CLR runtime header."""
	try:
		data = path.read_bytes()
		if len(data) < 0x40 or data[:2] != b'MZ':
			return False
		pe_offset = struct.unpack_from('<I', data, 0x3C)[0]
		if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b'PE\0\0':
			return False
		optional_header_offset = pe_offset + 24
		optional_header_size = struct.unpack_from('<H', data, pe_offset + 20)[0]
		if optional_header_offset + optional_header_size > len(data):
			return False
		magic = struct.unpack_from('<H', data, optional_header_offset)[0]
		if magic == 0x10B:
			number_of_directories_offset = optional_header_offset + 92
			data_directories_offset = optional_header_offset + 96
		elif magic == 0x20B:
			number_of_directories_offset = optional_header_offset + 108
			data_directories_offset = optional_header_offset + 112
		else:
			return False
		if number_of_directories_offset + 4 > optional_header_offset + optional_header_size:
			return False
		number_of_directories = struct.unpack_from('<I', data, number_of_directories_offset)[0]
		clr_directory_offset = data_directories_offset + 14 * 8
		if number_of_directories <= 14 or clr_directory_offset + 8 > optional_header_offset + optional_header_size:
			return False
		clr_rva, clr_size = struct.unpack_from('<II', data, clr_directory_offset)
		return clr_rva != 0 and clr_size != 0
	except (OSError, struct.error):
		return False


def collect_payload_assemblies(runtime_dir: Path) -> list[Path]:
	netcoreapp_dir = runtime_dir / 'lib' / 'netcoreapp'
	if not netcoreapp_dir.is_dir():
		raise ValueError(f'Managed class-library directory not found: {netcoreapp_dir}')
	assemblies = sorted(
		(path for path in netcoreapp_dir.glob('*.dll') if path.is_file() and is_managed_pe_assembly(path)),
		key=lambda path: path.name,
	)
	if not assemblies:
		raise ValueError(f'No managed class-library assemblies found: {netcoreapp_dir}')
	if not any(path.name == 'System.Private.CoreLib.dll' for path in assemblies):
		raise ValueError(f'Managed System.Private.CoreLib.dll not found: {netcoreapp_dir}')
	return assemblies


def file_sha256(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open('rb') as stream:
		while chunk := stream.read(1024 * 1024):
			digest.update(chunk)
	return digest.hexdigest()


def stage_payload(runtime_dir: Path, output_dir: Path, stamp_path: Path) -> list[Path]:
	stamp_path.unlink(missing_ok=True)
	assemblies = collect_payload_assemblies(runtime_dir)
	if output_dir.exists():
		shutil.rmtree(output_dir)
	output_netcoreapp_dir = output_dir / 'lib' / 'netcoreapp'
	output_netcoreapp_dir.mkdir(parents=True)
	manifest_lines = []
	staged_paths = []
	for assembly in assemblies:
		relative_path = Path('lib') / 'netcoreapp' / assembly.name
		target = output_dir / relative_path
		shutil.copy2(assembly, target)
		manifest_lines.append(f'{file_sha256(target)}  {relative_path.as_posix()}')
		staged_paths.append(target)
	manifest_path = output_dir / MANIFEST_NAME
	manifest_path.write_text('\n'.join(manifest_lines) + '\n', encoding='utf-8', newline='\n')
	staged_paths.append(manifest_path)
	stamp_path.parent.mkdir(parents=True, exist_ok=True)
	stamp_path.write_text(file_sha256(manifest_path) + '\n', encoding='utf-8', newline='\n')
	return staged_paths


def main() -> None:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--runtime-dir', type=Path, required=True)
	parser.add_argument('--output-dir', type=Path, required=True)
	parser.add_argument('--stamp', type=Path, required=True)
	args = parser.parse_args()
	stage_payload(args.runtime_dir, args.output_dir, args.stamp)


if __name__ == '__main__':
	main()
