#!/usr/bin/python3

from __future__ import annotations

import argparse
import glob
import io
import os
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


TARGET_CHOICES = ['Server', 'Client', 'Single', 'Editor', 'Mapper', 'Baker']
PLATFORM_CHOICES = ['Windows', 'Linux', 'Android', 'macOS', 'iOS', 'Web']
PNG_FILE_SIGNATURE = b'\x89PNG\r\n\x1a\n'
ANDROID_ICON_DENSITY_DIRS = ('mipmap-mdpi', 'mipmap-hdpi', 'mipmap-xhdpi', 'mipmap-xxhdpi', 'mipmap-xxxhdpi')
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
	parser.add_argument('-output', dest='output', required=True, help='output dir')
	parser.add_argument('-zip-compress-level', dest='zip_compress_level', type=int, choices=range(0, 10), help='override zip compression level')
	return parser.parse_args()


def log(*text: object) -> None:
	print('[Package]', *text, flush=True)


def patch_data(file_path: str | Path, mark: bytes, data: bytes, max_size: int) -> None:
	assert len(data) <= max_size, 'Data size is to big ' + str(len(data)) + ' but maximum is ' + str(max_size)
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
		return entry

	def build_output_variant_suffix(self, variant: BinaryVariant, is_windows: bool) -> str:
		return variant.output_suffix(is_windows)

	def resolve_binary_input_dir(self, arch: str, variant: BinaryVariant, bin_name: str) -> str:
		return self.get_input(os.path.join('Binaries', self.build_binary_entry(arch, variant)), bin_name)

	def copy_optional_pdb(self, bin_path: str, input_name: str, output_name: str) -> None:
		pdb_path = os.path.join(bin_path, input_name + '.pdb')
		if os.path.isfile(pdb_path):
			log('PDB file included')
			shutil.copy(pdb_path, os.path.join(self.target_output_path, output_name + '.pdb'))
		else:
			log('PDB file NOT included')

	def package_platform_binary(self, bin_path: str, input_name: str, output_name: str, output_ext: str, additional_config_data: str | None = None) -> str:
		output_file_path = os.path.join(self.target_output_path, output_name + output_ext)
		shutil.copy(os.path.join(bin_path, input_name + output_ext), output_file_path)
		self.patch_packaged_binary(output_file_path, additional_config_data)
		return output_file_path

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
		files = [file_path for file_path in glob.glob(pattern, recursive=True) if self.filter_resource_file(target, file_path)]
		assert files, 'No files in pack ' + pack_name
		return files

	def write_files_zip(self, archive_path: str, base_path: str, files: Sequence[str]) -> None:
		with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=self.zip_compress_level) as archive:
			for file_path in files:
				archive.write(file_path, os.path.relpath(file_path, base_path))

	def make_embedded_pack(self, files: Sequence[str], base_path: str) -> bytes:
		embedded_buffer = io.BytesIO()
		with zipfile.ZipFile(embedded_buffer, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=self.zip_compress_level) as archive:
			for file_path in files:
				archive.write(file_path, os.path.relpath(file_path, base_path))
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
		assert self.baking_path, 'Baking path is not initialized'
		config_suffix = 'client' if self.args.target == 'Client' else 'server'
		config_name = (self.args.config if self.args.config else '(Root)') + '.fomain-' + config_suffix
		config_path = os.path.join(self.baking_path, 'Configs', config_name)
		log('Config', config_name)
		assert os.path.isfile(config_path), 'Config file not found'
		with open(config_path, 'r', encoding='utf-8-sig') as file:
			self.config_data = file.read().encode()
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
		patch_data(file_path, b'###InternalConfig###1234', result_data, 10048)

	def patch_packaged_mark(self, file_path: str) -> None:
		patch_data(file_path, b'###NOT_PACKAGED###', b'###XXXXXXXXXXXX###', 18)

	def patch_packaged_binary(self, file_path: str, additional_config_data: str | None = None) -> None:
		if self.has_pack('NoRes'):
			return
		self.patch_embedded(file_path)
		self.patch_config(file_path, additional_config_data)
		self.patch_packaged_mark(file_path)

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
		for arch in self.iter_arches():
			for variant in self.iter_windows_variants():
				is_lib = self.has_pack('Lib')
				bin_name = self.args.devname + '_' + self.args.target + variant.role + ('Lib' if is_lib else '')
				log('Setup', arch, bin_name, variant.log_name())

				bin_out_name = (bin_name if self.args.target != 'Client' else self.args.nicename) + self.build_output_variant_suffix(variant, is_windows=True)
				bin_path = self.resolve_binary_input_dir(arch, variant, bin_name)
				bin_ext = '.dll' if is_lib else '.exe'
				log('Binary input', bin_path)

				self.package_platform_binary(bin_path, bin_name, bin_out_name, bin_ext, 'ForceOpenGL=1' if variant.graphics == 'OGL' else None)
				self.copy_optional_pdb(bin_path, bin_name, bin_out_name)

	def package_linux(self) -> None:
		for arch in self.iter_arches():
			for variant in self.iter_linux_variants():
				bin_name = self.args.devname + '_' + self.args.target + variant.role
				bin_out_name = (bin_name if self.args.target != 'Client' else self.args.nicename) + self.build_output_variant_suffix(variant, is_windows=False)
				log('Setup', arch, bin_name, variant.log_name())
				bin_path = self.resolve_binary_input_dir(arch, variant, bin_name)
				log('Binary input', bin_path)

				output_file_path = self.package_platform_binary(bin_path, bin_name, bin_out_name, '')

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
		self.patch_packaged_mark(wasm_output_path)

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

		emsdk_root = buildtools.resolve_env().get('EMSDK', '')
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
			self.patch_packaged_binary(dst_so)

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
		patch_file(app_build_gradle, '$RELEASE_STORE_PASSWORD$', escape_groovy_string(release_store_password))
		patch_file(app_build_gradle, '$RELEASE_KEY_ALIAS$', escape_groovy_string(release_key_alias))
		patch_file(app_build_gradle, '$RELEASE_KEY_PASSWORD$', escape_groovy_string(release_key_password))
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
		android_home = android_env.get('ANDROID_HOME', '') or android_env.get('ANDROID_SDK_ROOT', '')
		android_ndk_root = android_env.get('ANDROID_NDK_ROOT', '')

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

			build_task = 'assembleDebug' if self.has_pack('Debug') else 'assembleRelease'
			result = subprocess.call([gradlew, build_task], cwd=self.target_output_path, env=gradle_env)
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

	def finalize_output(self) -> None:
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

		if not self.has_pack('Raw'):
			shutil.rmtree(self.target_output_path, True)

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
