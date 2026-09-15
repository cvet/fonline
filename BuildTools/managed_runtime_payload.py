#!/usr/bin/env python3
"""Build the target-platform managed class-library payload shipped in resource packs."""
from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
from pathlib import Path, PurePosixPath
from typing import Callable, Iterable, NamedTuple


MANIFEST_NAME = 'runtime.manifest'
CLASS_LIBRARY_DIRECTORY = PurePosixPath('lib/netcoreapp')
CORELIB_ASSEMBLY_NAME = 'System.Private.CoreLib'

# Table numbers and column layouts follow ECMA-335 partition II, chapter 22
TABLE_MODULE = 0x00
TABLE_TYPE_REF = 0x01
TABLE_TYPE_DEF = 0x02
TABLE_FIELD_PTR = 0x03
TABLE_FIELD = 0x04
TABLE_METHOD_PTR = 0x05
TABLE_METHOD_DEF = 0x06
TABLE_PARAM_PTR = 0x07
TABLE_PARAM = 0x08
TABLE_INTERFACE_IMPL = 0x09
TABLE_MEMBER_REF = 0x0A
TABLE_CONSTANT = 0x0B
TABLE_CUSTOM_ATTRIBUTE = 0x0C
TABLE_FIELD_MARSHAL = 0x0D
TABLE_DECL_SECURITY = 0x0E
TABLE_CLASS_LAYOUT = 0x0F
TABLE_FIELD_LAYOUT = 0x10
TABLE_STAND_ALONE_SIG = 0x11
TABLE_EVENT_MAP = 0x12
TABLE_EVENT_PTR = 0x13
TABLE_EVENT = 0x14
TABLE_PROPERTY_MAP = 0x15
TABLE_PROPERTY_PTR = 0x16
TABLE_PROPERTY = 0x17
TABLE_METHOD_SEMANTICS = 0x18
TABLE_METHOD_IMPL = 0x19
TABLE_MODULE_REF = 0x1A
TABLE_TYPE_SPEC = 0x1B
TABLE_IMPL_MAP = 0x1C
TABLE_FIELD_RVA = 0x1D
TABLE_ENC_LOG = 0x1E
TABLE_ENC_MAP = 0x1F
TABLE_ASSEMBLY = 0x20
TABLE_ASSEMBLY_PROCESSOR = 0x21
TABLE_ASSEMBLY_OS = 0x22
TABLE_ASSEMBLY_REF = 0x23
TABLE_FILE = 0x26
TABLE_EXPORTED_TYPE = 0x27
TABLE_MANIFEST_RESOURCE = 0x28
TABLE_GENERIC_PARAM = 0x2A
TABLE_METHOD_SPEC = 0x2B
TABLE_GENERIC_PARAM_CONSTRAINT = 0x2C
METADATA_TABLE_COUNT = 64


class ManagedAssemblyError(ValueError):
	"""A PE image that is not a readable managed assembly, or a reference that cannot be satisfied."""


class AssemblyIdentity(NamedTuple):
	name: str
	references: tuple[str, ...]


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


def select_payload(runtime_dir: Path, pack_assemblies: Iterable[AssemblyIdentity]) -> tuple[list[PurePosixPath], str]:
	"""Return the payload files a pack ships and the manifest listing them.

	A pack ships CoreLib plus the class libraries its own assemblies reach by reference: Mono opens a class
	library only when a reference names it, so the rest of the published runtime is never loaded.
	"""
	manifest_lines = read_manifest_lines(runtime_dir)

	def find_runtime_assembly(name: str) -> AssemblyIdentity | None:
		if name not in manifest_lines:
			return None
		assembly_path = runtime_dir / CLASS_LIBRARY_DIRECTORY / (name + '.dll')
		return read_assembly_identity_file(assembly_path)

	selected = sorted(select_runtime_assemblies(pack_assemblies, find_runtime_assembly))
	missing = [name for name in selected if name not in manifest_lines]
	if missing:
		raise ManagedAssemblyError(f'Managed runtime payload manifest lacks selected assemblies {missing}: {runtime_dir}')
	files = [CLASS_LIBRARY_DIRECTORY / (name + '.dll') for name in selected]
	return files, ''.join(manifest_lines[name] + '\n' for name in selected)


def read_manifest_lines(runtime_dir: Path) -> dict[str, str]:
	"""Map each class-library assembly name of a staged payload to its manifest line."""
	manifest_path = runtime_dir / MANIFEST_NAME
	lines: dict[str, str] = {}
	for line in manifest_path.read_text(encoding='utf-8').splitlines():
		line = line.strip()
		if not line:
			continue
		digest, separator, relative_name = line.partition('  ')
		relative_path = PurePosixPath(relative_name)
		if len(digest) != 64 or not separator or relative_path.parent != CLASS_LIBRARY_DIRECTORY or relative_path.suffix != '.dll':
			raise ManagedAssemblyError(f'Invalid managed runtime payload manifest line in {manifest_path}: {line}')
		if any(character not in '0123456789abcdef' for character in digest):
			raise ManagedAssemblyError(f'Invalid managed runtime payload digest in {manifest_path}: {digest}')
		if not (runtime_dir / relative_path).is_file():
			raise ManagedAssemblyError(f'Managed runtime payload file not found: {runtime_dir / relative_path}')
		if relative_path.stem in lines:
			raise ManagedAssemblyError(
				f'Duplicate managed runtime payload assembly {relative_path.stem} in {manifest_path}')
		lines[relative_path.stem] = line
	return lines


def select_runtime_assemblies(
	pack_assemblies: Iterable[AssemblyIdentity],
	find_runtime_assembly: Callable[[str], AssemblyIdentity | None],
) -> set[str]:
	"""Name every runtime class library the pack reaches by reference, requiring and including CoreLib."""
	pack_assemblies = list(pack_assemblies)
	pack_names = {assembly.name for assembly in pack_assemblies}
	pending = [(reference, assembly.name) for assembly in pack_assemblies for reference in assembly.references]
	corelib = find_runtime_assembly(CORELIB_ASSEMBLY_NAME)
	if corelib is None:
		raise ManagedAssemblyError(f'Managed runtime does not publish CoreLib {CORELIB_ASSEMBLY_NAME}')
	if corelib.name != CORELIB_ASSEMBLY_NAME:
		raise ManagedAssemblyError(
			f'Managed runtime CoreLib file {CORELIB_ASSEMBLY_NAME} defines another assembly: {corelib.name}')
	selected = {CORELIB_ASSEMBLY_NAME}
	pending.extend((reference, CORELIB_ASSEMBLY_NAME) for reference in corelib.references)
	while pending:
		reference, referrer = pending.pop()
		if reference in pack_names or reference in selected:
			continue
		runtime_assembly = find_runtime_assembly(reference)
		if runtime_assembly is None:
			# Compatibility facades such as mscorlib forward into out-of-band packages the runtime never
			# publishes, so only a reference the pack itself makes has to resolve
			if referrer in pack_names:
				raise ManagedAssemblyError(
					f'Managed assembly reference {reference} of {referrer} is satisfied neither by the pack nor by the runtime')
			continue
		if runtime_assembly.name != reference:
			raise ManagedAssemblyError(f'Managed runtime assembly file {reference} defines another assembly: {runtime_assembly.name}')
		selected.add(reference)
		pending.extend((nested_reference, reference) for nested_reference in runtime_assembly.references)
	return selected


def read_assembly_identity_file(path: Path) -> AssemblyIdentity:
	try:
		identity = read_assembly_identity(path.read_bytes())
		if identity.name != path.stem:
			raise ManagedAssemblyError(
				f'Managed assembly file {path.name} defines another assembly: {identity.name}')
		return identity
	except ManagedAssemblyError as error:
		raise ManagedAssemblyError(f'Cannot read managed assembly references of {path}: {error}') from error


def read_assembly_identity(image: bytes) -> AssemblyIdentity:
	"""Read the assembly's own name and the names in its AssemblyRef table from a PE image."""
	metadata_offset = find_metadata_root(image)
	streams = read_metadata_streams(image, metadata_offset)
	if '#~' not in streams or '#Strings' not in streams:
		raise ManagedAssemblyError('Managed assembly metadata has no compressed tables or strings stream')
	tables_offset, tables_size = streams['#~']
	heap_sizes = read_u8(image, tables_offset + 6)
	valid_tables = read_u32(image, tables_offset + 8) | (read_u32(image, tables_offset + 12) << 32)
	rows = [0] * METADATA_TABLE_COUNT
	rows_offset = tables_offset + 24
	for table in range(METADATA_TABLE_COUNT):
		if valid_tables & (1 << table):
			rows[table] = read_u32(image, rows_offset)
			rows_offset += 4
	if heap_sizes & 0x40:
		rows_offset += 4
	string_size = 4 if heap_sizes & 0x01 else 2
	guid_size = 4 if heap_sizes & 0x02 else 2
	blob_size = 4 if heap_sizes & 0x04 else 2
	row_sizes = make_table_row_sizes(rows, string_size, guid_size, blob_size)
	table_offsets = [rows_offset]
	for table in range(TABLE_ASSEMBLY_REF + 1):
		table_offsets.append(table_offsets[-1] + rows[table] * row_sizes[table])
	if table_offsets[TABLE_ASSEMBLY_REF + 1] > tables_offset + tables_size:
		raise ManagedAssemblyError('Managed assembly metadata tables overrun their stream')
	if rows[TABLE_ASSEMBLY] != 1:
		raise ManagedAssemblyError(f'Managed assembly image must define exactly one assembly, found {rows[TABLE_ASSEMBLY]}')

	def read_string(offset: int) -> str:
		return read_heap_string(image, streams['#Strings'], read_index(image, offset, string_size))

	# Assembly row: HashAlgId, four version parts, Flags, PublicKey blob, then Name
	name = read_string(table_offsets[TABLE_ASSEMBLY] + 16 + blob_size)
	# AssemblyRef row: four version parts, Flags, PublicKeyOrToken blob, then Name
	references = tuple(
		read_string(table_offsets[TABLE_ASSEMBLY_REF] + row * row_sizes[TABLE_ASSEMBLY_REF] + 12 + blob_size)
		for row in range(rows[TABLE_ASSEMBLY_REF])
	)
	return AssemblyIdentity(name, references)


def find_metadata_root(image: bytes) -> int:
	if read_u16(image, 0) != 0x5A4D:
		raise ManagedAssemblyError('Managed assembly image has no DOS header')
	pe_offset = read_u32(image, 0x3C)
	if read_u32(image, pe_offset) != 0x00004550:
		raise ManagedAssemblyError('Managed assembly image has no PE signature')
	section_count = read_u16(image, pe_offset + 6)
	optional_header_size = read_u16(image, pe_offset + 20)
	optional_header_offset = pe_offset + 24
	magic = read_u16(image, optional_header_offset)
	if magic == 0x10B:
		directory_count_offset = 92
	elif magic == 0x20B:
		directory_count_offset = 108
	else:
		raise ManagedAssemblyError(f'Managed assembly image has an unknown optional header {magic:#x}')
	clr_directory_offset = directory_count_offset + 4 + 14 * 8
	if read_u32(image, optional_header_offset + directory_count_offset) <= 14 or clr_directory_offset + 8 > optional_header_size:
		raise ManagedAssemblyError('Managed assembly image has no CLR runtime header directory')
	clr_rva = read_u32(image, optional_header_offset + clr_directory_offset)
	if clr_rva == 0:
		raise ManagedAssemblyError('Image is a native PE file, not a managed assembly')
	sections_offset = optional_header_offset + optional_header_size
	clr_offset = rva_to_offset(image, sections_offset, section_count, clr_rva)
	metadata_offset = rva_to_offset(image, sections_offset, section_count, read_u32(image, clr_offset + 8))
	if read_u32(image, metadata_offset) != 0x424A5342:
		raise ManagedAssemblyError('Managed assembly metadata root has no signature')
	return metadata_offset


def read_metadata_streams(image: bytes, metadata_offset: int) -> dict[str, tuple[int, int]]:
	version_length = read_u32(image, metadata_offset + 12)
	require_image_bytes(image, metadata_offset + 16, version_length)
	stream_count = read_u16(image, metadata_offset + 18 + version_length)
	header_offset = metadata_offset + 20 + version_length
	streams: dict[str, tuple[int, int]] = {}
	for _ in range(stream_count):
		stream_offset = metadata_offset + read_u32(image, header_offset)
		stream_size = read_u32(image, header_offset + 4)
		require_image_bytes(image, stream_offset, stream_size)
		name_end = header_offset + 8
		while read_u8(image, name_end) != 0:
			name_end += 1
		name = image[header_offset + 8:name_end]
		# The name field holds the terminator and is padded to a four-byte boundary
		header_offset += 8 + (len(name) + 4) // 4 * 4
		streams[name.decode('ascii', errors='replace')] = (stream_offset, stream_size)
	return streams


def make_table_row_sizes(rows: list[int], string_size: int, guid_size: int, blob_size: int) -> list[int]:
	def simple_index(table: int) -> int:
		return 2 if rows[table] < 0x10000 else 4

	# A coded index widens once its largest table no longer fits beside the tag bits
	def coded_index(tables: Iterable[int], tag_bits: int) -> int:
		return 2 if max(rows[table] for table in tables) < (1 << (16 - tag_bits)) else 4

	type_def_or_ref = coded_index((TABLE_TYPE_DEF, TABLE_TYPE_REF, TABLE_TYPE_SPEC), 2)
	has_constant = coded_index((TABLE_FIELD, TABLE_PARAM, TABLE_PROPERTY), 2)
	has_custom_attribute = coded_index((
		TABLE_METHOD_DEF, TABLE_FIELD, TABLE_TYPE_REF, TABLE_TYPE_DEF, TABLE_PARAM, TABLE_INTERFACE_IMPL, TABLE_MEMBER_REF,
		TABLE_MODULE, TABLE_DECL_SECURITY, TABLE_PROPERTY, TABLE_EVENT, TABLE_STAND_ALONE_SIG, TABLE_MODULE_REF, TABLE_TYPE_SPEC,
		TABLE_ASSEMBLY, TABLE_ASSEMBLY_REF, TABLE_FILE, TABLE_EXPORTED_TYPE, TABLE_MANIFEST_RESOURCE, TABLE_GENERIC_PARAM,
		TABLE_GENERIC_PARAM_CONSTRAINT, TABLE_METHOD_SPEC), 5)
	has_field_marshal = coded_index((TABLE_FIELD, TABLE_PARAM), 1)
	has_decl_security = coded_index((TABLE_TYPE_DEF, TABLE_METHOD_DEF, TABLE_ASSEMBLY), 2)
	member_ref_parent = coded_index((TABLE_TYPE_DEF, TABLE_TYPE_REF, TABLE_MODULE_REF, TABLE_METHOD_DEF, TABLE_TYPE_SPEC), 3)
	has_semantics = coded_index((TABLE_EVENT, TABLE_PROPERTY), 1)
	method_def_or_ref = coded_index((TABLE_METHOD_DEF, TABLE_MEMBER_REF), 1)
	member_forwarded = coded_index((TABLE_FIELD, TABLE_METHOD_DEF), 1)
	custom_attribute_type = coded_index((TABLE_METHOD_DEF, TABLE_MEMBER_REF), 3)
	resolution_scope = coded_index((TABLE_MODULE, TABLE_MODULE_REF, TABLE_ASSEMBLY_REF, TABLE_TYPE_REF), 2)
	string, guid, blob = string_size, guid_size, blob_size

	sizes = [0] * (TABLE_ASSEMBLY_REF + 1)
	sizes[TABLE_MODULE] = 2 + string + guid * 3
	sizes[TABLE_TYPE_REF] = resolution_scope + string * 2
	sizes[TABLE_TYPE_DEF] = 4 + string * 2 + type_def_or_ref + simple_index(TABLE_FIELD) + simple_index(TABLE_METHOD_DEF)
	sizes[TABLE_FIELD_PTR] = simple_index(TABLE_FIELD)
	sizes[TABLE_FIELD] = 2 + string + blob
	sizes[TABLE_METHOD_PTR] = simple_index(TABLE_METHOD_DEF)
	sizes[TABLE_METHOD_DEF] = 4 + 2 + 2 + string + blob + simple_index(TABLE_PARAM)
	sizes[TABLE_PARAM_PTR] = simple_index(TABLE_PARAM)
	sizes[TABLE_PARAM] = 2 + 2 + string
	sizes[TABLE_INTERFACE_IMPL] = simple_index(TABLE_TYPE_DEF) + type_def_or_ref
	sizes[TABLE_MEMBER_REF] = member_ref_parent + string + blob
	sizes[TABLE_CONSTANT] = 2 + has_constant + blob
	sizes[TABLE_CUSTOM_ATTRIBUTE] = has_custom_attribute + custom_attribute_type + blob
	sizes[TABLE_FIELD_MARSHAL] = has_field_marshal + blob
	sizes[TABLE_DECL_SECURITY] = 2 + has_decl_security + blob
	sizes[TABLE_CLASS_LAYOUT] = 2 + 4 + simple_index(TABLE_TYPE_DEF)
	sizes[TABLE_FIELD_LAYOUT] = 4 + simple_index(TABLE_FIELD)
	sizes[TABLE_STAND_ALONE_SIG] = blob
	sizes[TABLE_EVENT_MAP] = simple_index(TABLE_TYPE_DEF) + simple_index(TABLE_EVENT)
	sizes[TABLE_EVENT_PTR] = simple_index(TABLE_EVENT)
	sizes[TABLE_EVENT] = 2 + string + type_def_or_ref
	sizes[TABLE_PROPERTY_MAP] = simple_index(TABLE_TYPE_DEF) + simple_index(TABLE_PROPERTY)
	sizes[TABLE_PROPERTY_PTR] = simple_index(TABLE_PROPERTY)
	sizes[TABLE_PROPERTY] = 2 + string + blob
	sizes[TABLE_METHOD_SEMANTICS] = 2 + simple_index(TABLE_METHOD_DEF) + has_semantics
	sizes[TABLE_METHOD_IMPL] = simple_index(TABLE_TYPE_DEF) + method_def_or_ref * 2
	sizes[TABLE_MODULE_REF] = string
	sizes[TABLE_TYPE_SPEC] = blob
	sizes[TABLE_IMPL_MAP] = 2 + member_forwarded + string + simple_index(TABLE_MODULE_REF)
	sizes[TABLE_FIELD_RVA] = 4 + simple_index(TABLE_FIELD)
	sizes[TABLE_ENC_LOG] = 4 + 4
	sizes[TABLE_ENC_MAP] = 4
	sizes[TABLE_ASSEMBLY] = 4 + 2 * 4 + 4 + blob + string * 2
	sizes[TABLE_ASSEMBLY_PROCESSOR] = 4
	sizes[TABLE_ASSEMBLY_OS] = 4 * 3
	sizes[TABLE_ASSEMBLY_REF] = 2 * 4 + 4 + blob + string * 2 + blob
	return sizes


def rva_to_offset(image: bytes, sections_offset: int, section_count: int, rva: int) -> int:
	for section in range(section_count):
		header_offset = sections_offset + section * 40
		virtual_size = read_u32(image, header_offset + 8)
		virtual_address = read_u32(image, header_offset + 12)
		raw_data_size = read_u32(image, header_offset + 16)
		raw_data_offset = read_u32(image, header_offset + 20)
		if virtual_address <= rva < virtual_address + max(virtual_size, raw_data_size):
			offset = raw_data_offset + rva - virtual_address
			if offset >= len(image):
				raise ManagedAssemblyError(f'Managed assembly RVA {rva:#x} points past the image')
			return offset
	raise ManagedAssemblyError(f'Managed assembly RVA {rva:#x} belongs to no section')


def read_heap_string(image: bytes, strings_stream: tuple[int, int], index: int) -> str:
	heap_offset, heap_size = strings_stream
	if index >= heap_size:
		raise ManagedAssemblyError(f'Managed assembly string index {index} is out of its heap')
	end = image.find(b'\0', heap_offset + index, heap_offset + heap_size)
	if end < 0:
		raise ManagedAssemblyError(f'Managed assembly string {index} runs past its heap')
	return image[heap_offset + index:end].decode('utf-8')


def read_index(image: bytes, offset: int, size: int) -> int:
	return read_u32(image, offset) if size == 4 else read_u16(image, offset)


def read_u8(image: bytes, offset: int) -> int:
	require_image_bytes(image, offset, 1)
	return image[offset]


def read_u16(image: bytes, offset: int) -> int:
	require_image_bytes(image, offset, 2)
	return struct.unpack_from('<H', image, offset)[0]


def read_u32(image: bytes, offset: int) -> int:
	require_image_bytes(image, offset, 4)
	return struct.unpack_from('<I', image, offset)[0]


def require_image_bytes(image: bytes, offset: int, size: int) -> None:
	if offset < 0 or offset + size > len(image):
		raise ManagedAssemblyError(f'Managed assembly image is truncated at {offset:#x}+{size}')


def main() -> None:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--runtime-dir', type=Path, required=True)
	parser.add_argument('--output-dir', type=Path, required=True)
	parser.add_argument('--stamp', type=Path, required=True)
	args = parser.parse_args()
	stage_payload(args.runtime_dir, args.output_dir, args.stamp)


if __name__ == '__main__':
	main()
