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
from typing import Iterable, Literal, Sequence

import foconfig


TARGET_CHOICES = ['Server', 'Client', 'Single', 'Editor', 'Mapper', 'Baker']
PLATFORM_CHOICES = ['Windows', 'Linux', 'Android', 'macOS', 'iOS', 'Web']


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
	# Android: arm arm64 x86
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
	parser.add_argument('-angelscript', dest='angelscript', action='store_true', help='attach angelscript scripts')
	parser.add_argument('-mono', dest='mono', action='store_true', help='attach mono scripts')
	parser.add_argument('-input', dest='input', required=True, action='append', default=[], help='input dir (from FO_OUTPUT_PATH)')
	parser.add_argument('-output', dest='output', required=True, help='output dir')
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


def make_zip(name: str | Path, path: str | Path, compress_level: int, mode: Literal['w', 'a'] = 'w') -> None:
	archive = zipfile.ZipFile(name, mode, zipfile.ZIP_DEFLATED, compresslevel=compress_level)
	for root, _, files in os.walk(path):
		for file_name in files:
			archive.write(os.path.join(root, file_name), os.path.join(os.path.relpath(root, path), file_name))
	archive.close()


def make_tar(name: str | Path, path: str | Path, mode: Literal['w', 'w:gz']) -> None:
	def filter_member(tar_info: tarfile.TarInfo) -> tarfile.TarInfo:
		return tar_info

	archive = tarfile.open(name, mode)
	current_dir = os.getcwd()
	new_dir, folder = os.path.split(path)
	os.chdir(new_dir)
	archive.add(folder, filter=filter_member)
	os.chdir(current_dir)
	archive.close()


def make_embedded_marker(size: int) -> bytearray:
	return bytearray([(index + 42) % 200 for index in range(size)])


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
		self.server_res_dir = self.fomain.mainSection().getStr('ServerResources')
		self.client_res_dir = self.fomain.mainSection().getStr('ClientResources')
		self.zip_compress_level = self.fomain.mainSection().getInt('ZipCompressLevel')
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
		return self.args.arch.split('+')

	def prepare_output(self) -> None:
		log('Output to', self.target_output_path)
		shutil.rmtree(self.target_output_path, True)
		if os.path.isfile(self.target_output_path + '.zip'):
			os.remove(self.target_output_path + '.zip')
		os.makedirs(self.target_output_path)

	def cleanup_output(self) -> None:
		if self.target_output_path:
			shutil.rmtree(self.target_output_path, True)

	def get_input(self, subdir: str, input_type: str) -> str:
		for input_dir in self.args.input:
			abs_dir = os.path.join(os.path.abspath(input_dir), subdir)
			if os.path.isdir(abs_dir):
				build_hash_path = os.path.join(abs_dir, input_type + '.build-hash')
				assert os.path.isfile(build_hash_path), 'Build hash file ' + build_hash_path + ' not found'
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
		bake_output = self.fomain.mainSection().getStr('BakeOutput')
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

	def iter_windows_bin_types(self) -> list[str]:
		bin_types = ['']
		if self.has_pack('Headless'):
			bin_types.append('Headless')
		if self.has_pack('Service'):
			bin_types.append('Service')
		if self.has_pack('TotalProfiling'):
			bin_types.append('TotalProfiling')
		if self.has_pack('OnDemandProfiling'):
			bin_types.append('OnDemandProfiling')
		if self.has_pack('OGL'):
			bin_types.append('OGL')
		return bin_types

	def iter_linux_bin_types(self) -> list[str]:
		bin_types = ['']
		if self.has_pack('Headless'):
			bin_types.append('Headless')
		if self.has_pack('Daemon'):
			bin_types.append('Daemon')
		if self.has_pack('TotalProfiling'):
			bin_types.append('TotalProfiling')
		if self.has_pack('OnDemandProfiling'):
			bin_types.append('OnDemandProfiling')
		return bin_types

	def package_windows(self) -> None:
		for arch in self.iter_arches():
			for bin_type in self.iter_windows_bin_types():
				is_lib = self.has_pack('Lib')
				bin_name = self.args.devname + '_' + self.args.target + (bin_type if bin_type in ['Headless', 'Service'] else '') + ('Lib' if is_lib else '')
				log('Setup', arch, bin_name, bin_type)

				bin_out_name = (bin_name if self.args.target != 'Client' else self.args.nicename) + \
					('_Profiling' if bin_type in ['TotalProfiling', 'OnDemandProfiling'] else '') + \
					('_OpenGL' if bin_type == 'OGL' else '')
				bin_entry = self.args.target + '-' + self.args.platform + '-' + arch + \
					('-Profiling_Total' if bin_type == 'TotalProfiling' else '') + \
					('-Profiling_OnDemand' if bin_type == 'OnDemandProfiling' else '') + \
					('-Debug' if self.has_pack('Debug') else '')
				bin_path = self.get_input(os.path.join('Binaries', bin_entry), bin_name)
				bin_ext = '.dll' if is_lib else '.exe'
				log('Binary input', bin_path)

				output_file_path = os.path.join(self.target_output_path, bin_out_name + bin_ext)
				shutil.copy(os.path.join(bin_path, bin_name + bin_ext), output_file_path)

				pdb_path = os.path.join(bin_path, bin_name + '.pdb')
				if os.path.isfile(pdb_path):
					log('PDB file included')
					shutil.copy(pdb_path, os.path.join(self.target_output_path, bin_out_name + '.pdb'))
				else:
					log('PDB file NOT included')

				self.patch_packaged_binary(output_file_path, 'ForceOpenGL=1' if bin_type == 'OGL' else None)

	def package_linux(self) -> None:
		for arch in self.iter_arches():
			for bin_type in self.iter_linux_bin_types():
				bin_name = self.args.devname + '_' + self.args.target + (bin_type if bin_type in ['Headless', 'Daemon'] else '')
				bin_out_name = (bin_name if self.args.target != 'Client' else self.args.nicename) + \
					('_Profiling' if bin_type in ['TotalProfiling', 'OnDemandProfiling'] else '')
				log('Setup', arch, bin_name, bin_type)
				bin_entry = self.args.target + '-' + self.args.platform + '-' + arch + \
					('-Profiling_Total' if bin_type == 'TotalProfiling' else '') + \
					('-Profiling_OnDemand' if bin_type == 'OnDemandProfiling' else '') + \
					('-Debug' if self.has_pack('Debug') else '')
				bin_path = self.get_input(os.path.join('Binaries', bin_entry), bin_name)
				log('Binary input', bin_path)

				output_file_path = os.path.join(self.target_output_path, bin_out_name)
				shutil.copy(os.path.join(bin_path, bin_name), output_file_path)
				self.patch_packaged_binary(output_file_path)

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

		self.patch_packaged_binary(wasm_output_path)

		index_path = os.path.join(self.target_output_path, 'index.html')
		shutil.copy(os.path.join(self.build_tools_path, 'web', 'default-index.html'), index_path)

		if self.has_pack('WebServer'):
			shutil.copy(os.path.join(self.build_tools_path, 'web', 'simple-web-server.py'), os.path.join(self.target_output_path, 'web-server.py'))

		assert 'EMSDK' in os.environ and os.environ['EMSDK'], 'No EMSDK provided'
		file_packager_path = os.path.join(os.environ['EMSDK'], 'upstream', 'emscripten', 'tools', 'file_packager.py')
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
		patch_file(index_path, '$RESOURCESJS$', 'Resources.js')
		patch_file(index_path, '$MAINJS$', bin_out_name + '.js')

	def package_android(self) -> None:
		assert False, 'Android packaging is not supported in this repository state'

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

			if self.args.platform == 'Windows':
				self.package_windows()
			elif self.args.platform == 'Linux':
				self.package_linux()
			elif self.args.platform == 'Web':
				self.package_web()
			elif self.args.platform == 'Android':
				self.package_android()
			elif self.args.platform == 'macOS':
				self.package_macos()
			elif self.args.platform == 'iOS':
				self.package_ios()
			else:
				assert False, 'Unknown build target'

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
