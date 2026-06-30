#!/usr/bin/python3

from __future__ import annotations

import argparse
import glob
import io
import json
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import tarfile
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable, Literal, Sequence

import buildtools
import foconfig


TARGET_CHOICES = ['Server', 'Client', 'Editor', 'Mapper', 'Baker']
PLATFORM_CHOICES = ['Windows', 'Linux', 'Android', 'macOS', 'iOS', 'Web']
PNG_FILE_SIGNATURE = b'\x89PNG\r\n\x1a\n'
ANDROID_ICON_DENSITY_DIRS = ('mipmap-mdpi', 'mipmap-hdpi', 'mipmap-xhdpi', 'mipmap-xxhdpi', 'mipmap-xxxhdpi')
INTERNAL_CONFIG_MARKER = b'###InternalConfig###1234'
INTERNAL_CONFIG_END_MARKER = b'###InternalConfigEnd###'
ANDROID_RELEASE_STORE_PASSWORD_ENV = 'FO_ANDROID_RELEASE_STORE_PASSWORD'
ANDROID_RELEASE_KEY_PASSWORD_ENV = 'FO_ANDROID_RELEASE_KEY_PASSWORD'
ANDROID_ARCH_ALIASES = {
	'arm': 'arm32',
	'arm32': 'arm32',
	'armeabi-v7a': 'arm32',
	'arm64': 'arm64',
	'arm64-v8a': 'arm64',
	'x86': 'x86',
}
ANDROID_ABI_BY_ARCH = {
	'arm32': 'armeabi-v7a',
	'arm64': 'arm64-v8a',
	'x86': 'x86',
}
ANDROID_ACTIVITY_CLASS = 'FOnlineActivity'
RUNTIME_COMPANION_EXTENSIONS = ('.dll', '.so', '.dylib')
PACKAGED_BUILD_NAME_MARKER = b'###NotPackaged###'
PACKAGED_BUILD_NAME_CAPACITY = 128

# Maps the (platform, arch-in-binary-entry-directory) pair used by the packager
# to the C++ binary target arch reported by GetCurrentBinaryUpdateTargetName()
# in Engine/Source/Common/Common.h. Most platforms match directly; Android
# binaries are staged with the Android ABI name (armeabi-v7a, arm64-v8a) while
# the C++ side reports the canonical arch (arm32, arm64). Keep this table in sync
# with GetCurrentBinaryUpdateTargetName so the server-side updater payload
# directory and the client request name agree per platform.
PACKAGER_TO_CXX_BINARY_TARGET_ARCH = {
	('Windows', 'win32'): 'win32',
	('Windows', 'win64'): 'win64',
	('Windows', 'arm64'): 'arm64',
	('Linux', 'x64'): 'x64',
	('Linux', 'arm64'): 'arm64',
	('Linux', 'x86'): 'x86',
	('Linux', 'arm'): 'arm',
	('Android', 'armeabi-v7a'): 'arm32',
	('Android', 'arm64-v8a'): 'arm64',
	('Android', 'x86'): 'x86',
	('macOS', 'arm64'): 'arm64',
	('macOS', 'x64'): 'x64',
	('iOS', 'arm64'): 'arm64',
	('iOS', 'simulator'): 'simulator',
	('Web', 'wasm'): 'wasm',
}


def parse_args() -> argparse.Namespace:
	parser = argparse.ArgumentParser(description='FOnline packager')
	parser.add_argument('-maincfg', dest='maincfg', required=True, help='Main config path')
	parser.add_argument('-buildhash', dest='buildhash', required=True, help='build hash')
	parser.add_argument('-devname', dest='devname', required=True, help='Dev game name')
	parser.add_argument('-nicename', dest='nicename', required=True, help='Representable game name')
	parser.add_argument('-target', dest='target', required=True, choices=TARGET_CHOICES, help='package target type')
	parser.add_argument('-platform', dest='platform', required=True, choices=PLATFORM_CHOICES, help='platform type')
	parser.add_argument('-arch', dest='arch', required=True, help='architectures to include (divided by +)')
	# Windows: win32 win64
	# Linux: x64
	# Android: arm32 arm64 x86
	# macOS: x64
	# iOS: arm64
	# Web: wasm
	parser.add_argument('-pack', dest='pack', required=True, help='package type')
	# Windows: Raw Zip Wix Headless Service
	# Linux: Raw Tar TarGz Zip AppImage
	# Android: Raw Apk
	# macOS: Raw Bundle Zip
	# iOS: Raw Bundle
	# Web: Raw Zip
	parser.add_argument('-config', dest='config', required=True, help='config name')
	parser.add_argument('-input', dest='input', required=True, action='append', default=[], help='input dir (from FO_OUTPUT_PATH)')
	parser.add_argument('-binary-output-postfix', dest='binary_output_postfix', default='', help='suffix appended to binary output dir names')
	parser.add_argument('-output', dest='output', required=True, help='output dir')
	parser.add_argument('-zip-compress-level', dest='zip_compress_level', type=int, choices=range(0, 10), help='override zip compression level')
	return parser.parse_args()


def log(*text: object) -> None:
	print('[Package]', *text, flush=True)


def patch_data(file_path: str | Path, mark: bytes, data: bytes, max_size: int) -> None:
	assert len(data) <= max_size, 'Data size is too big ' + str(len(data)) + ' but maximum is ' + str(max_size)
	with open(file_path, 'rb') as file:
		content = file.read()
	file_size = os.path.getsize(file_path)
	pos = content.find(mark)
	assert pos != -1
	padding = b'#' * (max_size - len(data))
	content = content[:pos] + data + padding + content[pos + max_size:]
	with open(file_path, 'wb') as file:
		file.write(content)
	assert file_size == os.path.getsize(file_path)


def patch_file(file_path: str | Path, text_from: str, text_to: str) -> None:
	with open(file_path, 'rb') as file:
		content = file.read()
	content = content.replace(text_from.encode('utf-8'), text_to.encode('utf-8'))
	with open(file_path, 'wb') as file:
		file.write(content)


def find_internal_config_capacity(content: bytes) -> int:
	pos = content.find(INTERNAL_CONFIG_MARKER)
	assert pos != -1, 'Internal config marker not found'
	end_pos = content.find(INTERNAL_CONFIG_END_MARKER, pos + len(INTERNAL_CONFIG_MARKER))
	assert end_pos != -1, 'Internal config end marker not found'
	return end_pos + len(INTERNAL_CONFIG_END_MARKER) - pos


def patch_pe_pdb_path(file_path: str | Path, new_pdb_name: str) -> bool:
	with open(file_path, 'rb') as file:
		content = bytearray(file.read())
	if content[:2] != b'MZ':
		return False
	pe_offset = struct.unpack_from('<I', content, 0x3C)[0]
	if content[pe_offset:pe_offset + 4] != b'PE\x00\x00':
		return False
	coff_offset = pe_offset + 4
	num_sections = struct.unpack_from('<H', content, coff_offset + 2)[0]
	opt_header_size = struct.unpack_from('<H', content, coff_offset + 16)[0]
	opt_header_offset = coff_offset + 20
	magic = struct.unpack_from('<H', content, opt_header_offset)[0]
	if magic == 0x10b:
		data_dir_offset = opt_header_offset + 96
	elif magic == 0x20b:
		data_dir_offset = opt_header_offset + 112
	else:
		return False
	debug_rva, debug_size = struct.unpack_from('<II', content, data_dir_offset + 6 * 8)
	if debug_rva == 0 or debug_size == 0:
		return False
	section_table_offset = opt_header_offset + opt_header_size
	debug_offset: int | None = None
	for i in range(num_sections):
		section_offset = section_table_offset + i * 40
		vsize, vaddr, _, raw_ptr = struct.unpack_from('<IIII', content, section_offset + 8)
		if vaddr <= debug_rva < vaddr + vsize:
			debug_offset = raw_ptr + (debug_rva - vaddr)
			break
	if debug_offset is None:
		return False
	new_path_bytes = new_pdb_name.encode('utf-8')
	IMAGE_DEBUG_TYPE_CODEVIEW = 2
	num_entries = debug_size // 28
	for i in range(num_entries):
		entry_offset = debug_offset + i * 28
		debug_type, raw_data_size, _, raw_data_ptr = struct.unpack_from('<IIII', content, entry_offset + 12)
		if debug_type != IMAGE_DEBUG_TYPE_CODEVIEW:
			continue
		if content[raw_data_ptr:raw_data_ptr + 4] != b'RSDS':
			continue
		path_start = raw_data_ptr + 4 + 16 + 4
		path_end = content.find(b'\x00', path_start, raw_data_ptr + raw_data_size)
		if path_end == -1:
			return False
		available = path_end - path_start
		if len(new_path_bytes) > available:
			return False
		content[path_start:path_end + 1] = new_path_bytes + b'\x00' * (available - len(new_path_bytes) + 1)
		with open(file_path, 'wb') as file:
			file.write(bytes(content))
		return True
	return False


def escape_groovy_string(value: str) -> str:
	return value.replace('\\', '\\\\').replace("'", "\\'").replace('\r', '\\r').replace('\n', '\\n')


def is_png_data(data: bytes) -> bool:
	return data.startswith(PNG_FILE_SIGNATURE)


def normalize_android_arch(arch: str) -> str:
	canonical_arch = ANDROID_ARCH_ALIASES.get(arch)
	assert canonical_arch is not None, 'Unknown Android architecture ' + arch
	return canonical_arch


def resolve_android_abi(arch: str) -> str:
	return ANDROID_ABI_BY_ARCH[normalize_android_arch(arch)]


def make_zip(name: str | Path, path: str | Path, compress_level: int, mode: Literal['w', 'a'] = 'w') -> None:
	with zipfile.ZipFile(name, mode, zipfile.ZIP_DEFLATED, compresslevel=compress_level) as archive:
		for root, _, files in os.walk(path):
			for file_name in files:
				archive.write(os.path.join(root, file_name), os.path.join(os.path.relpath(root, path), file_name))


def make_tar(name: str | Path, path: str | Path, mode: Literal['w', 'w:gz']) -> None:
	def filter_member(tar_info: tarfile.TarInfo) -> tarfile.TarInfo:
		return tar_info

	with tarfile.open(name, mode) as archive:
		archive.add(path, arcname=os.path.basename(os.fspath(path)), filter=filter_member)


def make_embedded_marker(size: int) -> bytearray:
	return bytearray([(index + 42) % 200 for index in range(size)])


@dataclass
class BinaryVariant:
	role: str = ''
	profiling: str = ''
	graphics: str = ''

	def log_name(self) -> str:
		return '+'.join(part for part in [self.role, self.profiling, self.graphics] if part)

	def output_suffix(self, is_windows: bool) -> str:
		suffix = ''
		if self.profiling:
			suffix += '_Profiling'
		if is_windows and self.graphics == 'OGL':
			suffix += '_OpenGL'
		return suffix


@dataclass
class Packager:
	args: argparse.Namespace
	fomain: foconfig.ConfigParser
	pack_args: set[str] = field(init=False)
	output_path: str = field(init=False)
	build_tools_path: str = field(init=False)
	server_res_dir: str = field(init=False)
	client_res_dir: str = field(init=False)
	platform_binaries_dir: str = field(init=False)
	zip_compress_level: int = field(init=False)
	target_output_path: str = field(init=False)
	baking_path: str | None = field(init=False, default=None)
	embedded_data: bytes = field(init=False, default=b'')
	config_data: bytes = field(init=False, default=b'')

	def __post_init__(self) -> None:
		self.pack_args = set(self.args.pack.split('+'))
		self.output_path = os.path.realpath(self.args.output if self.args.output else os.getcwd()).rstrip('\\/')
		self.build_tools_path = os.path.dirname(os.path.realpath(__file__))
		self.server_res_dir = self.fomain.mainSection().getStr('Baking.ServerResources')
		self.client_res_dir = self.fomain.mainSection().getStr('Baking.ClientResources')
		self.platform_binaries_dir = self.fomain.mainSection().getStr('Baking.PlatformBinaries')
		self.zip_compress_level = self.args.zip_compress_level if self.args.zip_compress_level is not None else self.fomain.mainSection().getInt('Baking.ZipCompressLevel')
		self.target_output_path = self.build_target_output_path()

	def has_pack(self, name: str) -> bool:
		return name in self.pack_args

	def build_target_output_path(self) -> str:
		path = os.path.join(self.output_path, self.args.devname + '-' + self.args.target)
		if self.args.target != self.args.config:
			path += '-' + self.args.config
		if self.args.platform != 'Windows':
			path += '-' + self.args.platform
		return path

	def iter_arches(self) -> list[str]:
		arches = self.args.arch.split('+')
		if self.args.platform != 'Android':
			return arches

		normalized_arches: list[str] = []
		for arch in arches:
			canonical_arch = normalize_android_arch(arch)
			if canonical_arch not in normalized_arches:
				normalized_arches.append(canonical_arch)
		return normalized_arches

	def build_binary_entry(self, arch: str, variant: BinaryVariant) -> str:
		entry_arch = resolve_android_abi(arch) if self.args.platform == 'Android' else arch
		entry = self.args.target + '-' + self.args.platform + '-' + entry_arch
		if variant.profiling == 'TotalProfiling':
			entry += '-Profiling_Total'
		elif variant.profiling == 'OnDemandProfiling':
			entry += '-Profiling_OnDemand'
		if self.has_pack('Debug'):
			entry += '-Debug'
		if self.args.binary_output_postfix:
			entry += '-' + self.args.binary_output_postfix
		return entry

	def build_output_variant_suffix(self, variant: BinaryVariant, is_windows: bool) -> str:
		return variant.output_suffix(is_windows)

	def build_client_runtime_input_name(self, variant: BinaryVariant | None = None) -> str:
		role = variant.role if variant is not None else ''
		return self.args.devname + '_ClientLib' + role

	def build_client_runtime_alias_name(self, variant: BinaryVariant) -> str:
		return self.args.devname + '_Client' + variant.role

	def get_runtime_library_ext_for_platform(self, platform: str) -> str:
		if platform == 'Windows':
			return '.dll'
		if platform in ('Linux', 'Android'):
			return '.so'
		if platform in ('macOS', 'iOS'):
			return '.dylib'
		return ''

	def build_runtime_update_target_name(self, binary_entry_name: str) -> str | None:
		if not binary_entry_name.startswith('Client-'):
			return None
		after_client = binary_entry_name[len('Client-'):]
		# Optional trailing suffixes (-Profiling_Total/-OnDemand, -Debug, -{binary_output_postfix})
		# follow the platform/arch prefix, so pick the longest matching known (platform, arch).
		best_platform: str | None = None
		best_cxx_arch: str | None = None
		best_prefix_len = -1
		for (platform, arch_in_entry), cxx_arch in PACKAGER_TO_CXX_BINARY_TARGET_ARCH.items():
			prefix = platform + '-' + arch_in_entry
			if after_client == prefix or after_client.startswith(prefix + '-'):
				if len(prefix) > best_prefix_len:
					best_platform = platform
					best_cxx_arch = cxx_arch
					best_prefix_len = len(prefix)
		if best_platform is None or best_cxx_arch is None:
			return None
		return best_platform + '-' + best_cxx_arch

	@staticmethod
	def extract_binary_entry_postfix(binary_entry_name: str) -> str | None:
		# Mirror of build_binary_entry(): {target}-{platform}-{arch}[-Profiling_X][-Debug][-{binary_output_postfix}].
		# Returns the FO_BINARY_OUTPUT_POSTFIX segment (empty if absent), or None when the entry
		# doesn't match any known platform/arch. The server-side runtime payload packager uses
		# this to tag each PlatformBinaries/{target}/{name}.{ext} payload with its variant's
		# postfix so multiple FO_BINARY_OUTPUT_POSTFIX builds (e.g. Steam vs non-Steam) can
		# coexist under one binary_target_name and a client picks its own by PACKAGED_BUILD_NAME.
		if not binary_entry_name.startswith('Client-'):
			return None
		after_client = binary_entry_name[len('Client-'):]
		best_prefix_len = -1
		for (platform, arch_in_entry), _ in PACKAGER_TO_CXX_BINARY_TARGET_ARCH.items():
			prefix = platform + '-' + arch_in_entry
			if after_client == prefix or after_client.startswith(prefix + '-'):
				if len(prefix) > best_prefix_len:
					best_prefix_len = len(prefix)
		if best_prefix_len < 0:
			return None
		remainder = after_client[best_prefix_len:]
		for opt in ('-Profiling_Total', '-Profiling_OnDemand'):
			if remainder.startswith(opt):
				remainder = remainder[len(opt):]
				break
		if remainder.startswith('-Debug'):
			remainder = remainder[len('-Debug'):]
		if not remainder:
			return ''
		assert remainder.startswith('-'), 'Unexpected binary entry layout: ' + binary_entry_name
		return remainder[1:]

	def resolve_binary_input_dir(self, arch: str, variant: BinaryVariant, bin_name: str) -> str:
		return self.get_input(os.path.join('Binaries', self.build_binary_entry(arch, variant)), bin_name)

	def copy_pdb(self, bin_path: str, input_name: str, output_name: str) -> None:
		pdb_path = os.path.join(bin_path, input_name + '.pdb')
		assert os.path.isfile(pdb_path), 'PDB file not found: ' + pdb_path
		log('PDB file included')
		shutil.copy(pdb_path, os.path.join(self.target_output_path, output_name + '.pdb'))

	def copy_runtime_pdb(self, bin_path: str, input_name: str, dll_output_path: str) -> None:
		pdb_path = os.path.join(bin_path, input_name + '.pdb')
		assert os.path.isfile(pdb_path), 'Runtime PDB file not found: ' + pdb_path
		pdb_out_name = os.path.basename(dll_output_path) + '.pdb'
		pdb_out_path = os.path.join(os.path.dirname(dll_output_path), pdb_out_name)
		log('Runtime PDB file included', pdb_out_path)
		shutil.copy(pdb_path, pdb_out_path)
		assert patch_pe_pdb_path(dll_output_path, pdb_out_name), 'Runtime DLL RSDS not patched (no CodeView entry or path too short): ' + dll_output_path

	def copy_runtime_companions(self, bin_path: str, primary_name: str, primary_ext: str, excluded_names: set[str] | None = None) -> None:
		primary_file_name = primary_name + primary_ext
		excluded_names = excluded_names or set()
		for entry_name in sorted(os.listdir(bin_path)):
			entry_path = os.path.join(bin_path, entry_name)
			if not os.path.isfile(entry_path):
				continue
			if entry_name == primary_file_name:
				continue
			if entry_name in excluded_names:
				continue
			if os.path.splitext(entry_name)[1].lower() not in RUNTIME_COMPANION_EXTENSIONS:
				continue

			log('Runtime companion included', entry_name)
			shutil.copy(entry_path, os.path.join(self.target_output_path, entry_name))

	def package_platform_binary(self, bin_path: str, input_name: str, output_name: str, output_ext: str, additional_config_data: str | None = None, excluded_companions: set[str] | None = None) -> str:
		output_file_path = os.path.join(self.target_output_path, output_name + output_ext)
		shutil.copy(os.path.join(bin_path, input_name + output_ext), output_file_path)
		self.patch_packaged_binary(output_file_path, output_name, additional_config_data)
		self.copy_runtime_companions(bin_path, input_name, output_ext, excluded_companions)
		return output_file_path

	def package_all_client_runtime_update_payloads(self) -> None:
		copied_payloads: set[tuple[str, str]] = set()
		client_embedded_data = self.make_embedded_data_for_target('Client')
		_, client_config_data = self.read_config_data('Client')

		for input_dir in self.args.input:
			binaries_root = os.path.join(os.path.abspath(input_dir), 'Binaries')
			if not os.path.isdir(binaries_root):
				continue

			for entry_name in os.listdir(binaries_root):
				entry_path = os.path.join(binaries_root, entry_name)
				if not os.path.isdir(entry_path) or not entry_name.startswith('Client-'):
					continue

				request_target_name = self.build_runtime_update_target_name(entry_name)
				if request_target_name is None:
					continue

				entry_postfix = self.extract_binary_entry_postfix(entry_name)
				if entry_postfix is None:
					continue

				parts = request_target_name.split('-', 1)
				if len(parts) != 2:
					continue

				platform = parts[0]
				runtime_ext = self.get_runtime_library_ext_for_platform(platform)
				if not runtime_ext:
					continue

				default_runtime_variant = BinaryVariant()
				headless_runtime_variant = BinaryVariant(role='Headless')

				build_hash_path = os.path.join(entry_path, self.build_client_runtime_input_name(default_runtime_variant) + '.build-hash')
				if not os.path.isfile(build_hash_path):
					continue

				with open(build_hash_path, 'r', encoding='utf-8-sig') as file:
					build_hash = file.read().strip()
				if build_hash != self.args.buildhash:
					continue

				suffix = ''
				if '-Profiling_' in entry_name:
					suffix = '_Profiling'

				# binary_output_postfix is appended to the staged payload name so two
				# binary entries that map to the same request_target_name (e.g.
				# Client-Linux-x64 vs Client-Linux-x64-Steam) do not collide. Each
				# client variant patches its own PACKAGED_BUILD_NAME to match the
				# resulting suffixed payload, so updater's remap_runtime_name picks
				# the right file.
				postfix_suffix = '_' + entry_postfix if entry_postfix else ''

				variant_specs: list[tuple[str, str | None, BinaryVariant]] = []
				variant_specs.append((self.args.nicename + suffix + postfix_suffix, None, default_runtime_variant))
				if platform == 'Windows':
					variant_specs.append((self.args.nicename + suffix + '_OpenGL' + postfix_suffix, 'ForceOpenGL=1', default_runtime_variant))
				headless_runtime_path = os.path.join(entry_path, self.build_client_runtime_input_name(headless_runtime_variant) + runtime_ext)
				if os.path.isfile(headless_runtime_path):
					variant_specs.append((self.args.nicename + suffix + '_Headless' + postfix_suffix, None, headless_runtime_variant))

				for output_name, variant_config_data, runtime_variant in variant_specs:
					payload_key = (request_target_name, output_name)
					if payload_key in copied_payloads:
						continue

					runtime_input_name = self.build_client_runtime_input_name(runtime_variant)
					runtime_input_path = os.path.join(entry_path, runtime_input_name + runtime_ext)
					if not os.path.isfile(runtime_input_path):
						continue

					payload_dir = os.path.join(self.target_output_path, self.platform_binaries_dir, request_target_name)
					os.makedirs(payload_dir, exist_ok=True)
					output_path = os.path.join(payload_dir, output_name + runtime_ext)
					log('Client runtime update payload', output_path)
					shutil.copy(runtime_input_path, output_path)

					old_embedded_data = self.embedded_data
					old_config_data = self.config_data
					try:
						self.embedded_data = client_embedded_data
						self.config_data = client_config_data
						self.patch_packaged_binary(output_path, output_name, variant_config_data)
					finally:
						self.embedded_data = old_embedded_data
						self.config_data = old_config_data

					if platform == 'Windows':
						pdb_input_path = os.path.join(entry_path, runtime_input_name + '.pdb')
						assert os.path.isfile(pdb_input_path), 'Client runtime update payload PDB not found: ' + pdb_input_path
						pdb_out_name = os.path.basename(output_path) + '.pdb'
						pdb_out_path = os.path.join(payload_dir, pdb_out_name)
						log('Client runtime update payload PDB', pdb_out_path)
						shutil.copy(pdb_input_path, pdb_out_path)
						assert patch_pe_pdb_path(output_path, pdb_out_name), 'Client runtime update payload RSDS not patched: ' + output_path

						# Stage the host executable's PDB (`<name>.pdb`) so a client that lost it can
						# re-download it. The client fetches it only when its local copy is missing and
						# never overwrites a present one (see Updater.cpp): an up-to-date host recovers a
						# matching PDB, while an older host (whose PDB is build-specific) is never clobbered.
						host_pdb_input = os.path.join(entry_path, self.build_client_runtime_alias_name(runtime_variant) + '.pdb')
						if os.path.isfile(host_pdb_input):
							host_pdb_out = os.path.join(payload_dir, output_name + '.pdb')
							log('Client host PDB included', host_pdb_out)
							shutil.copy(host_pdb_input, host_pdb_out)

					copied_payloads.add(payload_key)

	def merge_additional_config_data(self, *entries: str | None) -> str | None:
		lines = [entry for entry in entries if entry]
		return '\n'.join(lines) if lines else None

	def select_platform_packager(self) -> Callable[[], None]:
		if self.args.platform == 'Windows':
			return self.package_windows
		if self.args.platform == 'Linux':
			return self.package_linux
		if self.args.platform == 'Web':
			return self.package_web
		if self.args.platform == 'Android':
			return self.package_android
		if self.args.platform == 'macOS':
			return self.package_macos
		if self.args.platform == 'iOS':
			return self.package_ios
		raise AssertionError('Unknown build target')

	def prepare_output(self) -> None:
		log('Output to', self.target_output_path)
		if os.path.isdir(self.target_output_path):
			for entry_name in os.listdir(self.target_output_path):
				entry_path = os.path.join(self.target_output_path, entry_name)
				if os.path.isdir(entry_path) and not os.path.islink(entry_path):
					shutil.rmtree(entry_path, True)
				else:
					try:
						os.remove(entry_path)
					except FileNotFoundError:
						pass
		else:
			shutil.rmtree(self.target_output_path, True)
			if os.path.isfile(self.target_output_path):
				os.remove(self.target_output_path)
		if os.path.isfile(self.target_output_path + '.zip'):
			os.remove(self.target_output_path + '.zip')
		os.makedirs(self.target_output_path, exist_ok=True)

	def cleanup_output(self) -> None:
		if self.target_output_path:
			shutil.rmtree(self.target_output_path, True)

	def get_input(self, subdir: str, input_type: str) -> str:
		for input_dir in self.args.input:
			abs_dir = os.path.join(os.path.abspath(input_dir), subdir)
			if os.path.isdir(abs_dir):
				build_hash_path = os.path.join(abs_dir, input_type + '.build-hash')
				if not os.path.isfile(build_hash_path):
					continue
				with open(build_hash_path, 'r', encoding='utf-8-sig') as file:
					build_hash = file.read().strip()
				assert build_hash == self.args.buildhash, 'Build hash file ' + build_hash_path + ' has wrong hash'
				return abs_dir
		raise AssertionError('Input dir ' + subdir + ' not found for ' + input_type)

	def get_target_resource_packs(self, target: str) -> list[str]:
		resource_entries: list[str] = []
		for res_pack in self.fomain.getSections('ResourcePack'):
			server_only = res_pack.getBool('ServerOnly', False)
			client_only = res_pack.getBool('ClientOnly', False)
			mapper_only = res_pack.getBool('MapperOnly', False)
			if target == 'Server' and not client_only and not mapper_only:
				resource_entries.append(res_pack.getStr('Name'))
			if target == 'Client' and not server_only and not mapper_only:
				resource_entries.append(res_pack.getStr('Name'))
		return resource_entries

	def filter_resource_file(self, target: str, file_path: str) -> bool:
		if not os.path.isfile(file_path):
			return False
		if target == 'Server' and (file_path.endswith('-client') or file_path.endswith('-mapper')):
			return False
		if target == 'Client' and (file_path.endswith('-server') or file_path.endswith('-mapper')):
			return False
		if target == 'Mapper' and file_path.endswith('-server'):
			return False
		return True

	def collect_resource_files(self, pack_name: str, target: str) -> list[str]:
		assert self.baking_path, 'Baking path is not initialized'
		pattern = os.path.join(self.baking_path, pack_name, '**')
		files = sorted(file_path for file_path in glob.glob(pattern, recursive=True) if self.filter_resource_file(target, file_path))
		assert files, 'No files in pack ' + pack_name
		return files

	def make_embedded_data_for_target(self, target: str) -> bytes:
		assert self.baking_path, 'Baking path is not initialized'
		for pack_name in self.get_target_resource_packs(target):
			if pack_name == 'Embedded':
				files = self.collect_resource_files(pack_name, target)
				return self.make_embedded_pack(files, os.path.join(self.baking_path, pack_name))
		raise AssertionError('Embedded resource pack not found for ' + target)

	def read_config_data(self, target: str) -> tuple[str, bytes]:
		assert self.baking_path, 'Baking path is not initialized'
		config_suffix = 'client' if target == 'Client' else 'server'
		config_name = (self.args.config if self.args.config else '(Root)') + '.fomain-' + config_suffix
		config_path = os.path.join(self.baking_path, 'Configs', config_name)
		assert os.path.isfile(config_path), 'Config file not found'
		with open(config_path, 'r', encoding='utf-8-sig') as file:
			return config_name, file.read().encode()

	def write_files_zip(self, archive_path: str, base_path: str, files: Sequence[str]) -> None:
		with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=self.zip_compress_level) as archive:
			zip_entries = sorted((os.path.relpath(file_path, base_path).replace(os.sep, '/'), file_path) for file_path in files)
			for arcname, file_path in zip_entries:
				self.write_stable_zip_entry(archive, file_path, arcname)

	def write_stable_zip_entry(self, archive: zipfile.ZipFile, file_path: str, arcname: str) -> None:
		info = zipfile.ZipInfo(filename=arcname, date_time=(1980, 1, 1, 0, 0, 0))
		info.create_system = 3
		info.compress_type = zipfile.ZIP_DEFLATED
		info.external_attr = 0o644 << 16
		with open(file_path, 'rb') as src, archive.open(info, 'w') as dst:
			shutil.copyfileobj(src, dst)

	def make_embedded_pack(self, files: Sequence[str], base_path: str) -> bytes:
		embedded_buffer = io.BytesIO()
		with zipfile.ZipFile(embedded_buffer, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=self.zip_compress_level) as archive:
			for file_path in files:
				arcname = os.path.relpath(file_path, base_path).replace(os.sep, '/')
				self.write_stable_zip_entry(archive, file_path, arcname)
		data = embedded_buffer.getvalue()
		return struct.pack('I', len(data)) + data

	def ensure_resource_dirs(self) -> None:
		if self.args.target == 'Server':
			os.makedirs(os.path.join(self.target_output_path, self.server_res_dir), exist_ok=True)
			os.makedirs(os.path.join(self.target_output_path, self.client_res_dir), exist_ok=True)
		elif self.args.target == 'Client':
			os.makedirs(os.path.join(self.target_output_path, self.client_res_dir), exist_ok=True)

	def package_resource_pack(self, pack_name: str, files: Sequence[str], base_res_name: str) -> None:
		archive_path = os.path.join(self.target_output_path, base_res_name, pack_name + '.zip')
		assert self.baking_path, 'Baking path is not initialized'
		base_path = os.path.join(self.baking_path, pack_name)
		log('Make pack', pack_name, '=>', pack_name + '.zip', '(' + str(len(files)) + ')')
		self.write_files_zip(archive_path, base_path, files)

	def load_config_data(self) -> None:
		config_name, self.config_data = self.read_config_data(self.args.target)
		log('Config', config_name)
		log('Embedded data length', len(self.embedded_data))
		log('Embedded config length', len(self.config_data))

	def prepare_resources(self) -> None:
		bake_output = self.fomain.mainSection().getStr('Baking.BakeOutput')
		self.baking_path = self.get_input(bake_output, 'Resources')
		log('Baking input', self.baking_path)

		self.ensure_resource_dirs()

		for pack_name in self.get_target_resource_packs(self.args.target):
			files = self.collect_resource_files(pack_name, self.args.target)
			if pack_name == 'Embedded':
				log('Make pack', pack_name, '=>', 'embed to executable', '(' + str(len(files)) + ')')
				self.embedded_data = self.make_embedded_pack(files, os.path.join(self.baking_path, pack_name))
			else:
				base_res_name = self.server_res_dir if self.args.target == 'Server' else self.client_res_dir
				self.package_resource_pack(pack_name, files, base_res_name)

		if self.args.target == 'Server':
			for pack_name in self.get_target_resource_packs('Client'):
				if pack_name == 'Embedded':
					continue
				files = self.collect_resource_files(pack_name, 'Client')
				log('Make client pack', pack_name, '=>', pack_name + '.zip', '(' + str(len(files)) + ')')
				self.write_files_zip(
					os.path.join(self.target_output_path, self.client_res_dir, pack_name + '.zip'),
					os.path.join(self.baking_path, pack_name),
					files,
				)

		self.load_config_data()

	def patch_embedded(self, file_path: str) -> None:
		assert self.embedded_data, 'Embedded data is not prepared'
		with open(file_path, 'rb') as file:
			content = file.read()
		pos = content.find(make_embedded_marker(10000))
		assert pos != -1, 'Space for embedded data not found'
		embedded_capacity = 0
		while content[pos:pos + 5000] == make_embedded_marker(embedded_capacity + 5000)[embedded_capacity:embedded_capacity + 5000]:
			embedded_capacity += 5000
			pos += 5000
		assert embedded_capacity >= 10000, embedded_capacity
		patch_data(file_path, make_embedded_marker(embedded_capacity), self.embedded_data, embedded_capacity)

	def patch_config(self, file_path: str, additional_config_data: str | None = None) -> None:
		assert self.config_data, 'Embedded config is not prepared'
		result_data = self.config_data + (('\n' + additional_config_data).encode() if additional_config_data else b'')
		with open(file_path, 'rb') as file:
			content = file.read()
		patch_data(file_path, INTERNAL_CONFIG_MARKER, result_data, find_internal_config_capacity(content))

	def patch_packaged_build_name(self, file_path: str, build_name: str) -> None:
		name_bytes = build_name.encode('utf-8')
		assert len(name_bytes) + 1 <= PACKAGED_BUILD_NAME_CAPACITY, f'Packaged build name too long ({len(name_bytes)} bytes, cap {PACKAGED_BUILD_NAME_CAPACITY - 1}): {build_name}'
		patch_data(file_path, PACKAGED_BUILD_NAME_MARKER, name_bytes + b'\x00' * (PACKAGED_BUILD_NAME_CAPACITY - len(name_bytes)), PACKAGED_BUILD_NAME_CAPACITY)

	def patch_packaged_binary(self, file_path: str, build_name: str, additional_config_data: str | None = None) -> None:
		if self.has_pack('NoRes'):
			return
		self.patch_embedded(file_path)
		self.patch_config(file_path, additional_config_data)
		self.patch_packaged_build_name(file_path, build_name)

	def iter_windows_variants(self) -> list[BinaryVariant]:
		bin_roles = ['']
		if self.has_pack('Headless'):
			bin_roles.append('Headless')
		if self.has_pack('Service'):
			bin_roles.append('Service')

		profiling_variants = ['']
		if self.has_pack('TotalProfiling'):
			profiling_variants.append('TotalProfiling')
		if self.has_pack('OnDemandProfiling'):
			profiling_variants.append('OnDemandProfiling')

		graphics_variants = ['']
		if self.has_pack('OGL'):
			graphics_variants.append('OGL')

		return [
			BinaryVariant(bin_role, profiling_variant, graphics_variant)
			for bin_role in bin_roles
			for profiling_variant in profiling_variants
			for graphics_variant in graphics_variants
		]

	def iter_linux_variants(self) -> list[BinaryVariant]:
		bin_roles = ['']
		if self.has_pack('Headless'):
			bin_roles.append('Headless')
		if self.has_pack('Daemon'):
			bin_roles.append('Daemon')

		profiling_variants = ['']
		if self.has_pack('TotalProfiling'):
			profiling_variants.append('TotalProfiling')
		if self.has_pack('OnDemandProfiling'):
			profiling_variants.append('OnDemandProfiling')

		return [
			BinaryVariant(bin_role, profiling_variant)
			for bin_role in bin_roles
			for profiling_variant in profiling_variants
		]

	def package_windows(self) -> None:
		if self.args.target == 'Server' and not self.has_pack('NoRes'):
			self.package_all_client_runtime_update_payloads()

		# Mirror of the suffix appended to server-side payloads in
		# package_all_client_runtime_update_payloads: tagging the client output
		# name keeps PACKAGED_BUILD_NAME aligned with what the server stages
		# under PlatformBinaries/<target>/<name>.dll for this variant.
		client_postfix_suffix = '_' + self.args.binary_output_postfix if self.args.binary_output_postfix else ''

		for arch in self.iter_arches():
			for variant in self.iter_windows_variants():
				is_lib = self.has_pack('Lib')
				bin_name = self.args.devname + '_' + self.args.target + variant.role + ('Lib' if is_lib else '')
				log('Setup', arch, bin_name, variant.log_name())

				bin_out_name = bin_name + self.build_output_variant_suffix(variant, is_windows=True) if self.args.target != 'Client' else self.args.nicename + ('_' + variant.role if variant.role else '') + self.build_output_variant_suffix(variant, is_windows=True) + client_postfix_suffix
				bin_path = self.resolve_binary_input_dir(arch, variant, bin_name)
				bin_ext = '.dll' if is_lib else '.exe'
				log('Binary input', bin_path)

				additional_config_data = 'ForceOpenGL=1' if variant.graphics == 'OGL' else None
				excluded_companions: set[str] = set()
				if self.args.target == 'Client':
					excluded_companions.add(self.build_client_runtime_alias_name(variant) + '.dll')

				if self.args.target == 'Client' and not is_lib:
					runtime_input_name = self.build_client_runtime_input_name(variant)
					runtime_alias_name = self.build_client_runtime_alias_name(variant)
					runtime_out_name = bin_out_name
					runtime_dll_path = self.package_platform_binary(bin_path, runtime_input_name, runtime_out_name, '.dll', additional_config_data, excluded_companions={runtime_alias_name + '.dll'})
					self.copy_runtime_pdb(bin_path, runtime_input_name, runtime_dll_path)
					excluded_companions.add(runtime_input_name + '.dll')

				main_binary_path = self.package_platform_binary(bin_path, bin_name, bin_out_name, bin_ext, additional_config_data, excluded_companions)
				self.copy_pdb(bin_path, bin_name, bin_out_name)
				if bin_ext == '.exe':
					# Point the frozen host exe at its sibling `<name>.pdb` (renamed from the
					# build's `<bin_name>.pdb`). Otherwise the exe keeps the build-machine
					# CodeView path and only resolves symbols via a debugger's module-adjacent
					# basename heuristic.
					assert patch_pe_pdb_path(main_binary_path, bin_out_name + '.pdb'), 'Host exe RSDS not patched: ' + main_binary_path

	def package_linux(self) -> None:
		if self.args.target == 'Server' and not self.has_pack('NoRes'):
			self.package_all_client_runtime_update_payloads()

		all_linux_variants = self.iter_linux_variants()
		cross_variant_excluded: set[str] = set()
		if self.args.target == 'Client':
			for other_variant in all_linux_variants:
				cross_variant_excluded.add(self.build_client_runtime_alias_name(other_variant) + '.so')
				cross_variant_excluded.add(self.build_client_runtime_input_name(other_variant) + '.so')

		# Mirrors the postfix tagging in package_all_client_runtime_update_payloads
		# so the Linux client's PACKAGED_BUILD_NAME matches the server-staged payload
		# for this binary_output_postfix variant.
		client_postfix_suffix = '_' + self.args.binary_output_postfix if self.args.target == 'Client' and self.args.binary_output_postfix else ''

		for arch in self.iter_arches():
			for variant in all_linux_variants:
				bin_name = self.args.devname + '_' + self.args.target + variant.role
				if self.args.target == 'Client':
					bin_out_name_base = self.args.nicename + ('_' + variant.role if variant.role else '')
				else:
					bin_out_name_base = bin_name
				bin_out_name = bin_out_name_base + self.build_output_variant_suffix(variant, is_windows=False) + client_postfix_suffix
				log('Setup', arch, bin_name, variant.log_name())
				bin_path = self.resolve_binary_input_dir(arch, variant, bin_name)
				log('Binary input', bin_path)

				additional_config_data = None
				excluded_companions: set[str] = set(cross_variant_excluded)

				if self.args.target == 'Client':
					runtime_input_name = self.build_client_runtime_input_name(variant)
					runtime_alias_name = self.build_client_runtime_alias_name(variant)
					runtime_out_name = bin_out_name
					runtime_excluded = set(cross_variant_excluded) | {runtime_alias_name + '.so'}
					self.package_platform_binary(bin_path, runtime_input_name, runtime_out_name, '.so', additional_config_data, excluded_companions=runtime_excluded)
					excluded_companions.add(runtime_input_name + '.so')

				output_file_path = self.package_platform_binary(bin_path, bin_name, bin_out_name, '', additional_config_data, excluded_companions)

				st = os.stat(output_file_path)
				os.chmod(output_file_path, st.st_mode | stat.S_IEXEC)

		if self.has_pack('AppImage'):
			pass

	def package_web(self) -> None:
		assert self.args.arch == 'wasm'
		assert not self.has_pack('NoRes'), 'Web package requires resources'

		bin_name = self.args.devname + '_' + self.args.target
		bin_out_name = bin_name
		log('Setup', bin_name)
		bin_entry = self.args.target + '-' + self.args.platform + '-' + self.args.arch + ('-Debug' if self.has_pack('Debug') else '')
		bin_path = self.get_input(os.path.join('Binaries', bin_entry), bin_name)
		log('Binary input', bin_path)

		shutil.copy(os.path.join(bin_path, bin_name + '.js'), os.path.join(self.target_output_path, bin_out_name + '.js'))
		wasm_output_path = os.path.join(self.target_output_path, bin_out_name + '.wasm')
		shutil.copy(os.path.join(bin_path, bin_name + '.wasm'), wasm_output_path)

		self.patch_embedded(wasm_output_path)
		self.patch_config(wasm_output_path)
		self.patch_packaged_build_name(wasm_output_path, bin_out_name)

		web_loading_image = self.fomain.mainSection().getStr('Web.LoadingImage', '')
		web_background_color = self.fomain.mainSection().getStr('Web.BackgroundColor', 'rgb(0, 0, 0)')
		if not web_background_color:
			web_background_color = 'rgb(0, 0, 0)'
		web_loading_image_out = ''
		web_loading_image_style = 'display:none;'
		if web_loading_image:
			web_loading_image_path = (Path(self.args.maincfg).resolve().parent / web_loading_image).resolve()
			assert web_loading_image_path.is_file(), f'Web loading image not found: {web_loading_image_path}'
			web_loading_image_out = 'web-loading-image' + web_loading_image_path.suffix.lower()
			web_loading_image_style = ''
			shutil.copy(web_loading_image_path, os.path.join(self.target_output_path, web_loading_image_out))

		index_path = os.path.join(self.target_output_path, 'index.html')
		shutil.copy(os.path.join(self.build_tools_path, 'web', 'default-index.html'), index_path)

		if self.has_pack('WebServer'):
			shutil.copy(os.path.join(self.build_tools_path, 'web', 'simple-web-server.py'), os.path.join(self.target_output_path, 'web-server.py'))

		emsdk_root = buildtools.resolve_env().get('FO_EMSDK', '')
		assert emsdk_root, 'Workspace EMSDK is not prepared'
		file_packager_path = os.path.join(emsdk_root, 'upstream', 'emscripten', 'tools', 'file_packager.py')
		assert os.path.isfile(file_packager_path), 'No emscripten tools/file_packager.py found'

		packager_args = [
			sys.executable if sys.executable else 'python3',
			file_packager_path,
			os.path.join(self.target_output_path, 'Resources.data').replace('\\', '/'),
			'--preload',
			os.path.join(self.target_output_path, self.client_res_dir).replace('\\', '/') + '@' + self.client_res_dir,
			'--js-output=' + os.path.join(self.target_output_path, 'Resources.js').replace('\\', '/'),
			'--lz4',
		]
		log('Call emscripten packager:')
		for arg in packager_args:
			log('-', arg)

		result = subprocess.call(packager_args)
		assert result == 0, 'Emscripten tools/file_packager.py failed'

		shutil.rmtree(os.path.join(self.target_output_path, self.client_res_dir), True)

		patch_file(index_path, '$TITLE$', self.args.nicename)
		patch_file(index_path, '$LOADING$', self.args.nicename)
		patch_file(index_path, '$BACKGROUND_COLOR$', web_background_color)
		patch_file(index_path, '$LOADING_IMAGE$', web_loading_image_out if web_loading_image_out else 'data:,')
		patch_file(index_path, '$LOADING_IMAGE_STYLE$', web_loading_image_style)
		patch_file(index_path, '$RESOURCESJS$', 'Resources.js')
		patch_file(index_path, '$MAINJS$', bin_out_name + '.js')

	def resolve_config_relative_path(self, config_path: str) -> str:
		if os.path.isabs(config_path):
			return config_path
		return os.path.normpath(os.path.join(os.path.dirname(os.path.realpath(self.args.maincfg)), config_path))

	def resolve_optional_config_relative_path(self, config_path: str) -> str:
		return self.resolve_config_relative_path(config_path) if config_path else ''

	def build_android_java_package_path(self, package_name: str) -> str:
		return os.path.join(*package_name.split('.'))

	def patch_android_activity(self, package_name: str) -> None:
		template_activity_path = os.path.join(self.target_output_path, 'app', 'src', 'main', 'java-template', ANDROID_ACTIVITY_CLASS + '.java')
		assert os.path.isfile(template_activity_path), 'Android activity template not found: ' + template_activity_path

		output_java_dir = os.path.join(self.target_output_path, 'app', 'src', 'main', 'java', self.build_android_java_package_path(package_name))
		os.makedirs(output_java_dir, exist_ok=True)

		activity_path = os.path.join(output_java_dir, ANDROID_ACTIVITY_CLASS + '.java')
		shutil.copy(template_activity_path, activity_path)
		patch_file(activity_path, '$PACKAGE$', package_name)
		patch_file(activity_path, '$CONFIG$', self.args.config)

		shutil.rmtree(os.path.join(self.target_output_path, 'app', 'src', 'main', 'java-template'), True)
		log('Android activity', activity_path)

	def try_read_android_icon_png(self, icon_path: str) -> bytes | None:
		with open(icon_path, 'rb') as file:
			icon_data = file.read()

		return icon_data if is_png_data(icon_data) else None

	def patch_android_icon(self) -> None:
		configured_icon = self.fomain.mainSection().getStr('Android.Icon', 'Engine/Resources/Radiation.png')
		icon_path = self.resolve_config_relative_path(configured_icon)
		assert os.path.isfile(icon_path), 'Android icon file not found: ' + icon_path

		icon_png_data = self.try_read_android_icon_png(icon_path)
		assert icon_png_data, 'Android.Icon must point to a PNG file: ' + icon_path

		res_dir = os.path.join(self.target_output_path, 'app', 'src', 'main', 'res')

		for density_dir in ANDROID_ICON_DENSITY_DIRS:
			icon_output_dir = os.path.join(res_dir, density_dir)
			os.makedirs(icon_output_dir, exist_ok=True)
			with open(os.path.join(icon_output_dir, 'ic_launcher.png'), 'wb') as file:
				file.write(icon_png_data)

		log('Android icon', icon_path)

	def package_android(self) -> None:
		assert not self.has_pack('NoRes'), 'Android package requires resources'

		bin_name = self.args.devname + '_' + self.args.target
		log('Setup', bin_name)

		# Copy android-project template to output
		android_template = os.path.join(self.build_tools_path, 'android-project')
		shutil.copytree(android_template, self.target_output_path, dirs_exist_ok=True)

		# Copy native libraries for each ABI
		for arch in self.iter_arches():
			android_abi = resolve_android_abi(arch)
			variant = BinaryVariant()
			bin_path = self.resolve_binary_input_dir(arch, variant, bin_name)
			log('Binary input', arch, bin_path)

			jni_libs_dir = os.path.join(self.target_output_path, 'app', 'libs', android_abi)
			os.makedirs(jni_libs_dir, exist_ok=True)

			# CMake outputs libLF_Client.so, rename to libmain.so for SDLActivity
			src_so = os.path.join(bin_path, 'lib' + bin_name + '.so')
			dst_so = os.path.join(jni_libs_dir, 'libmain.so')
			shutil.copy(src_so, dst_so)
			log('Native library', src_so, '=>', dst_so)

			# Patch the binary (embedded data, config, packaged mark)
			self.patch_packaged_binary(dst_so, bin_name)

		# Move baked resources into assets directory
		assets_dir = os.path.join(self.target_output_path, 'app', 'src', 'main', 'assets')
		os.makedirs(assets_dir, exist_ok=True)
		client_res_source = os.path.join(self.target_output_path, self.client_res_dir)
		if os.path.isdir(client_res_source):
			assets_res_dir = os.path.join(assets_dir, self.client_res_dir)
			shutil.move(client_res_source, assets_res_dir)
			log('Resources moved to', assets_res_dir)

		# Read Android config from fomain
		package_name = self.fomain.mainSection().getStr('Android.PackageName', 'com.fonline.app')
		version_code = self.fomain.mainSection().getStr('Android.VersionCode', '1')
		version_name = self.args.buildhash[:8] if self.args.buildhash else '1.0'
		min_sdk = self.fomain.mainSection().getStr('Android.MinSdk', '23')
		target_sdk = self.fomain.mainSection().getStr('Android.TargetSdk', '35')
		compile_sdk = self.fomain.mainSection().getStr('Android.CompileSdk', '35')
		screen_orientation = self.fomain.mainSection().getStr('Android.ScreenOrientation', 'landscape')
		release_store_file = self.resolve_optional_config_relative_path(self.fomain.mainSection().getStr('Android.Keystore', ''))
		release_store_password = self.fomain.mainSection().getStr('Android.KeystorePassword', '')
		release_key_alias = self.fomain.mainSection().getStr('Android.KeyAlias', '')
		release_key_password = self.fomain.mainSection().getStr('Android.KeyPassword', '')

		has_release_signing = any((release_store_file, release_store_password, release_key_alias, release_key_password))
		if has_release_signing:
			assert all((release_store_file, release_store_password, release_key_alias, release_key_password)), 'Android release signing requires Android.Keystore, Android.KeystorePassword, Android.KeyAlias, and Android.KeyPassword'
			assert os.path.isfile(release_store_file), 'Android keystore file not found: ' + release_store_file
			release_store_file = Path(release_store_file).as_posix()

		abi_filters = ', '.join("'" + resolve_android_abi(arch) + "'" for arch in self.iter_arches())

		# Patch template placeholders in build.gradle
		app_build_gradle = os.path.join(self.target_output_path, 'app', 'build.gradle')
		patch_file(app_build_gradle, '$PACKAGE$', package_name)
		patch_file(app_build_gradle, '$COMPILE_SDK$', compile_sdk)
		patch_file(app_build_gradle, '$MIN_SDK$', min_sdk)
		patch_file(app_build_gradle, '$TARGET_SDK$', target_sdk)
		patch_file(app_build_gradle, '$VERSION_CODE$', version_code)
		patch_file(app_build_gradle, '$VERSION_NAME$', version_name)
		patch_file(app_build_gradle, '$ABI_FILTERS$', abi_filters)
		patch_file(app_build_gradle, '$RELEASE_STORE_FILE$', escape_groovy_string(release_store_file))
		patch_file(app_build_gradle, '$RELEASE_KEY_ALIAS$', escape_groovy_string(release_key_alias))
		patch_file(os.path.join(self.target_output_path, 'app', 'proguard-rules.pro'), '$PACKAGE$', package_name)
		self.patch_android_activity(package_name)

		# Patch AndroidManifest.xml
		manifest_path = os.path.join(self.target_output_path, 'app', 'src', 'main', 'AndroidManifest.xml')
		patch_file(manifest_path, '$VERSION_CODE$', version_code)
		patch_file(manifest_path, '$VERSION_NAME$', version_name)
		patch_file(manifest_path, '$APP_NAME$', self.args.nicename)
		patch_file(manifest_path, '$SCREEN_ORIENTATION$', screen_orientation)

		# Patch strings.xml
		strings_path = os.path.join(self.target_output_path, 'app', 'src', 'main', 'res', 'values', 'strings.xml')
		patch_file(strings_path, '$APP_NAME$', self.args.nicename)
		self.patch_android_icon()

		android_env = buildtools.resolve_env()
		android_home = android_env.get('FO_ANDROID_HOME', '') or android_env.get('FO_ANDROID_SDK_ROOT', '')
		android_ndk_root = android_env.get('FO_ANDROID_NDK_ROOT', '')

		if android_home:
			local_properties_path = os.path.join(self.target_output_path, 'local.properties')
			with open(local_properties_path, 'w', encoding='utf-8', newline='\n') as file:
				file.write('sdk.dir=' + Path(android_home).as_posix() + '\n')
				if android_ndk_root:
					file.write('ndk.dir=' + Path(android_ndk_root).as_posix() + '\n')
			log('Android local.properties', local_properties_path)

		# Build APK if requested
		if self.has_pack('Apk'):
			log('Building APK...')
			gradlew = os.path.join(self.target_output_path, 'gradlew')
			st = os.stat(gradlew)
			os.chmod(gradlew, st.st_mode | stat.S_IEXEC)

			gradle_env = os.environ.copy()
			if android_home:
				gradle_env['ANDROID_HOME'] = android_home
				gradle_env['ANDROID_SDK_ROOT'] = android_home
			if release_store_password:
				gradle_env[ANDROID_RELEASE_STORE_PASSWORD_ENV] = release_store_password
			if release_key_password:
				gradle_env[ANDROID_RELEASE_KEY_PASSWORD_ENV] = release_key_password

			gradle_user_home = os.path.join(
				os.path.dirname(self.output_path),
				'.gradle-user-home',
				os.path.basename(self.target_output_path),
			)
			os.makedirs(gradle_user_home, exist_ok=True)
			gradle_env['GRADLE_USER_HOME'] = gradle_user_home
			log('Android Gradle user home', gradle_user_home)

			build_task = 'assembleDebug' if self.has_pack('Debug') else 'assembleRelease'
			result = subprocess.call([gradlew, '--no-daemon', build_task], cwd=self.target_output_path, env=gradle_env)
			assert result == 0, 'Gradle build failed'

			build_type = 'debug' if self.has_pack('Debug') else 'release'
			apk_pattern = os.path.join(self.target_output_path, 'app', 'build', 'outputs', 'apk', build_type, '*.apk')
			apk_files = sorted(glob.glob(apk_pattern))
			assert apk_files, 'No APK file found after Gradle build'

			preferred_apk_files = [apk_file for apk_file in apk_files if not apk_file.endswith('-unsigned.apk')]
			selected_apk_file = preferred_apk_files[0] if preferred_apk_files else apk_files[0]
			assert self.has_pack('Debug') or not selected_apk_file.endswith('-unsigned.apk'), 'Release APK is unsigned'

			apk_output = os.path.join(self.output_path, os.path.basename(self.target_output_path) + '.apk')
			shutil.copy(selected_apk_file, apk_output)
			log('APK output', apk_output)

	def package_macos(self) -> None:
		assert False, 'macOS packaging is not supported in this repository state'

	def package_ios(self) -> None:
		assert False, 'iOS packaging is not supported in this repository state'

	def sign_windows_binaries(self) -> None:
		# Optional release-time code signing of the staged Windows PE artifacts: launcher exes, runtime DLLs, and
		# the client-runtime update payloads (the downloaded-and-executed DLL is what makes signing matter for
		# antivirus reputation — see the H1 finding in the project's client-AV audit). Runs before any
		# archiving/installer step so every downstream artifact (Zip/Wix/Raw) carries the signature, and after
		# all binary patching so the signature covers the final bytes. Tool-agnostic by design:
		# Windows.CodeSigningHook is an owner-provided executable script called once per PE as `<hook> <abs-path>`;
		# the script owns the tool (osslsigncode / signtool / Azure Trusted Signing / SSL.com eSigner), the
		# certificate, the timestamp URL and any secrets (kept out of the repo and the main config). Empty hook =
		# unsigned (today's behavior). A signing failure is fatal so a release that asked to be signed never ships
		# unsigned.
		if self.args.platform != 'Windows':
			return

		hook = self.resolve_optional_config_relative_path(self.fomain.mainSection().getStr('Windows.CodeSigningHook', ''))
		if not hook:
			log('Code signing: skipped (Windows.CodeSigningHook not set)')
			return

		assert os.path.isfile(hook), 'Windows.CodeSigningHook script not found: ' + hook

		binaries = sorted({
			str(path)
			for pattern in ('*.exe', '*.dll')
			for path in Path(self.target_output_path).rglob(pattern)
		})
		if not binaries:
			log('Code signing: no .exe/.dll found under', self.target_output_path)
			return

		log('Code signing', len(binaries), 'Windows binaries via', hook)
		for binary in binaries:
			result = subprocess.call([hook, binary])
			assert result == 0, 'Windows.CodeSigningHook failed (exit ' + str(result) + ') for: ' + binary
		log('Code signing: done')

	def finalize_output(self) -> None:
		self.sign_windows_binaries()

		if self.has_pack('Zip'):
			log('Create zipped archive')
			make_zip(self.target_output_path + '.zip', self.target_output_path, self.zip_compress_level)

		if self.has_pack('SingleZip'):
			log('Add to single zip archive')
			single_zip_path = os.path.join(self.output_path, os.path.basename(self.output_path) + '.zip')
			make_zip(single_zip_path, self.target_output_path, self.zip_compress_level, 'a')

		if self.has_pack('Tar'):
			log('Create tar archive')
			make_tar(self.target_output_path + '.tar', self.target_output_path, 'w')

		if self.has_pack('TarGz'):
			log('Create tar.gz archive')
			make_tar(self.target_output_path + '.tar.gz', self.target_output_path, 'w:gz')

		if self.has_pack('Root'):
			shutil.copytree(self.target_output_path, self.output_path, dirs_exist_ok=True)

		if self.has_pack('Wix'):
			self.make_wix_installer()

		if not self.has_pack('Raw'):
			shutil.rmtree(self.target_output_path, True)

	def resolve_game_version(self) -> str:
		# Resolve Common.GameVersion to a concrete value. The main config commonly points it at a file
		# (e.g. `Common.GameVersion = $FILE{VERSION}`); foconfig keeps directives verbatim, so resolve the
		# `$FILE{...}` indirection here relative to the main config directory.
		raw = self.fomain.mainSection().getStr('Common.GameVersion', '0.0.0').strip()
		file_match = re.match(r'^\$FILE\{(.+)\}$', raw)
		if file_match:
			version_path = self.resolve_config_relative_path(file_match.group(1).strip())
			assert os.path.isfile(version_path), 'Common.GameVersion $FILE not found: ' + version_path
			with open(version_path, 'r', encoding='utf-8-sig') as version_file:
				raw = version_file.read().strip()
		return raw

	def ensure_msi_toolset(self) -> None:
		# The MSI is required when the Wix pack is requested, so verify the toolset up front and fail with a
		# clear message instead of a cryptic subprocess error. The host OS decides the toolset: WiX
		# (candle/light) on Windows, GNOME wixl elsewhere — matching msicreator/createmsi.py. On
		# Debian/Ubuntu wixl ships in its own "wixl" apt package (the "msitools" package carries only
		# msiinfo/msibuild/msidiff/msiextract and does NOT include wixl).
		if os.name == 'nt':
			missing = [tool for tool in ('candle', 'light') if shutil.which(tool) is None]
			assert not missing, 'Wix pack requires the WiX Toolset (' + ', '.join(missing) + ' not found on PATH)'
		else:
			assert shutil.which('wixl') is not None, 'Wix pack requires the "wixl" toolset on PATH (install the "wixl" package, e.g. apt-get install wixl)'

	def make_wix_installer(self) -> None:
		# Build a Windows MSI from the just-staged client payload (self.target_output_path) and register
		# the deep-link URI scheme so site login works without the client editing the registry at
		# runtime (installer-time registration is the AV-friendly path; the runtime self-register in
		# SourceExt/DeepLink.cpp stays as the fallback for the portable/zip/Steam builds). The MSI is a
		# required artifact: a missing toolset or a generator/build error fails the package. All
		# game-specific values come from the project config, so the engine packager stays game-agnostic.
		assert self.args.platform == 'Windows' and self.args.target == 'Client', 'Wix pack is only valid for the Windows Client target'

		self.ensure_msi_toolset()

		scheme = self.fomain.mainSection().getStr('Auth.UriScheme', '').strip()
		assert scheme, 'Wix pack requires Auth.UriScheme to register the deep-link URI scheme'

		game_name = self.fomain.mainSection().getStr('Common.GameName', self.args.nicename).strip() or self.args.nicename

		upgrade_code = self.fomain.mainSection().getStr('Windows.MsiUpgradeCode', '').strip()
		assert re.match(r'^[0-9A-Fa-f]{8}-([0-9A-Fa-f]{4}-){3}[0-9A-Fa-f]{12}$', upgrade_code), 'Wix pack requires Windows.MsiUpgradeCode to be a stable GUID (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)'
		upgrade_code = upgrade_code.upper()

		version = self.resolve_game_version()
		assert re.match(r'^\d+(\.\d+){1,3}$', version), 'Wix pack requires a numeric Common.GameVersion (x.y.z[.w]), got: ' + version

		icon_path = self.resolve_optional_config_relative_path(self.fomain.mainSection().getStr('Windows.Icon', ''))
		if icon_path:
			assert os.path.isfile(icon_path), 'Windows.Icon file not found: ' + icon_path

		exe_name = self.args.nicename + '.exe'
		command_value = '"[INSTALLDIR]%s" --DeepLinkUri "%%1"' % exe_name
		registry_entries = [
			{'root': 'HKCU', 'key': 'Software\\Classes\\%s' % scheme, 'action': 'createAndRemoveOnUninstall',
			 'name': '', 'type': 'string', 'value': 'URL:%s Protocol' % scheme, 'key_path': 'yes'},
			{'root': 'HKCU', 'key': 'Software\\Classes\\%s' % scheme, 'action': 'createAndRemoveOnUninstall',
			 'name': 'URL Protocol', 'type': 'string', 'value': '', 'key_path': 'no'},
			{'root': 'HKCU', 'key': 'Software\\Classes\\%s\\shell\\open\\command' % scheme, 'action': 'createAndRemoveOnUninstall',
			 'name': '', 'type': 'string', 'value': command_value, 'key_path': 'no'},
		]

		staged_dir = os.path.basename(self.target_output_path)
		work_dir = os.path.dirname(self.target_output_path)
		config: dict[str, object] = {
			'product_name': game_name,
			'manufacturer': game_name,
			'name': game_name,
			'name_base': self.args.nicename,
			'version': version,
			'comments': game_name + ' game client',
			'installdir': self.args.nicename,
			'license_file': '',
			'upgrade_guid': upgrade_code,
			'major_upgrade': {'AllowSameVersionUpgrades': 'yes', 'DowngradeErrorMessage': 'A newer version is already installed.'},
			'arch': 64,
			'registry_entries': registry_entries,
			'startmenu_shortcut': exe_name,
			'desktop_shortcut': exe_name,
			'parts': [{'id': 'MainProgram', 'title': game_name, 'description': 'Game client', 'staged_dir': staged_dir}],
		}
		if icon_path:
			config['addremove_icon'] = icon_path

		config_path = os.path.join(work_dir, self.args.nicename + '.wix.json')
		with open(config_path, 'w', encoding='utf-8') as config_file:
			json.dump(config, config_file)

		# Drop the installed-build marker into the staged payload so the MSI-installed client uses
		# the per-user writable data dir (cache/logs/self-update overlay) instead of the read-only
		# install dir. Added only for the MSI and removed afterwards, so the sibling Raw/Zip
		# portable artifacts (already finalized earlier in finalize_output) stay portable.
		marker_path = os.path.join(self.target_output_path, 'INSTALLED')
		try:
			with open(marker_path, 'w', encoding='utf-8') as marker_file:
				marker_file.write('installed\n')

			createmsi = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'msicreator', 'createmsi.py')
			log('Wix: building MSI installer', config_path)
			# createmsi.py requires a bare json filename (no path segment) and resolves it plus the staged
			# payload relative to its working directory, so invoke it with the basename and cwd=work_dir.
			subprocess.run([sys.executable, createmsi, os.path.basename(config_path)], cwd=work_dir, check=True)
			log('Wix: MSI built (registers %s:// URI scheme, Start Menu + Desktop shortcuts, installs writable-data marker)' % scheme)
		finally:
			if os.path.exists(marker_path):
				os.remove(marker_path)

	def run(self) -> None:
		log(f'Make {self.args.target} ({self.args.config}) for {self.args.platform}')
		self.prepare_output()
		try:
			if not self.has_pack('NoRes'):
				self.prepare_resources()

			self.select_platform_packager()()

			self.finalize_output()
			log('Complete!')
		except Exception:
			self.cleanup_output()
			raise


def main() -> None:
	args = parse_args()
	fomain = foconfig.ConfigParser()
	fomain.loadFromFile(args.maincfg)
	Packager(args, fomain).run()


if __name__ == '__main__':
	main()
