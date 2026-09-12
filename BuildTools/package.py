#!/usr/bin/python3

from __future__ import annotations

import argparse
import glob
import hashlib
import io
import json
import os
import re
import shlex
import shutil
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import IO, Callable, Iterable, Literal, Mapping, Sequence

import buildtools
import foconfig


TARGET_CHOICES = ['Server', 'Client', 'Mapper', 'Baker', 'AnimationViewer', 'ParticleViewer']
PLATFORM_CHOICES = ['Windows', 'Linux', 'Android', 'macOS', 'iOS', 'Web']
# Mirrors CanSelfUpdateNativeModules() in Source/Client/Updater.cpp: only these clients fetch native
# modules from the server. Every platform still receives its target-specific managed class libraries
# as an ordinary resource pack; this list controls only native client modules
SELF_UPDATING_CLIENT_PLATFORMS = ('Windows', 'Linux', 'macOS')
PNG_FILE_SIGNATURE = b'\x89PNG\r\n\x1a\n'
ANDROID_ICON_DENSITY_DIRS = ('mipmap-mdpi', 'mipmap-hdpi', 'mipmap-xhdpi', 'mipmap-xxhdpi', 'mipmap-xxxhdpi')
INTERNAL_CONFIG_MARKER = b'###InternalConfig###1234'
INTERNAL_CONFIG_END_MARKER = b'###InternalConfigEnd###'
ANDROID_RELEASE_STORE_PASSWORD_ENV = 'FO_ANDROID_RELEASE_STORE_PASSWORD'
ANDROID_RELEASE_KEY_PASSWORD_ENV = 'FO_ANDROID_RELEASE_KEY_PASSWORD'
ANDROID_MANIFEST_METADATA_CONFIG_PREFIX = 'Android.ManifestMetaData.'
ANDROID_GRADLE_MAVEN_REPOSITORY_CONFIG_PREFIX = 'Android.GradleMavenRepository.'
ANDROID_GRADLE_DEPENDENCY_CONFIG_PREFIX = 'Android.GradleDependency.'
ANDROID_JAVA_SOURCE_CONFIG_PREFIX = 'Android.JavaSource.'
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
MANAGED_RUNTIME_DIRECTORY = 'ManagedRuntime'
MANAGED_RUNTIME_MANIFEST = 'runtime.manifest'
MANAGED_CORELIB_RELATIVE_PATH = os.path.join('lib', 'netcoreapp', 'System.Private.CoreLib.dll')
PACKAGED_BUILD_NAME_MARKER = b'###NotPackaged###'
PACKAGED_BUILD_NAME_CAPACITY = 128
WEB_ASSET_BUNDLE_LIMIT = 256 * 1024 * 1024
PACKAGE_MODE_MANIFEST = '.lf-package-modes.json'
PACKAGE_MODE_MANIFEST_VERSION = 1
PACKAGE_FILE_MODES = frozenset({0o644, 0o755})
RESOURCE_ARCHIVE_CACHE_FORMAT = 1
RESOURCE_ARCHIVE_CACHE_HELPER_ENV = 'FO_RESOURCE_ARCHIVE_CACHE_HELPER'
RESOURCE_ARCHIVE_CACHE_MISS = 2
RESOURCE_ARCHIVE_CACHE_UNAVAILABLE = 3
RESOURCE_ARCHIVE_HASH_CHUNK_BYTES = 1024 * 1024

# Maps the (platform, arch-in-binary-entry-directory) pair used by the packager
# to the C++ binary target arch reported by GetCurrentBinaryUpdateTargetName()
# in Engine/Source/Common/Common.h. Most platforms match directly; Android
# binaries are staged with the Android ABI name (armeabi-v7a, arm64-v8a) while
# the C++ side reports the canonical arch (arm32, arm64). Keep this table in sync
# with GetCurrentBinaryUpdateTargetName so the server-side updater payload
# directory and the client request name agree per platform
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
	parser.add_argument('-expect-client-runtime', dest='expect_client_runtime', action='append', default=[],
		help='Client variant whose runtime payload this server package must distribute, as Platform:arch[:postfix]. Repeatable')
	# Windows: win32 win64 win32-win7 win64-win7
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


def parse_include_args(arguments: Sequence[str]) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description='Include external files in an assembled FOnline package')
	parser.add_argument('-maincfg', dest='maincfg', required=True, help='Main config path')
	parser.add_argument('-input', dest='input', required=True, type=Path, help='root used to resolve the source glob')
	parser.add_argument('-source', dest='source', required=True, help='source path glob relative to the input root')
	parser.add_argument('-output', dest='output', required=True, type=Path, help='assembled package root')
	parser.add_argument('-target', dest='target', required=True, help='target directory relative to the package root')
	parser.add_argument('-singlezip', dest='singlezip', required=True, type=Path, help='package SingleZip path; updated only when it exists')
	return parser.parse_args(arguments)


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
		vsize, vaddr, _, raw_data_ptr = struct.unpack_from('<IIII', content, section_offset + 8)
		if vaddr <= debug_rva < vaddr + vsize:
			debug_offset = raw_data_ptr + (debug_rva - vaddr)
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


def escape_android_manifest_attribute(value: str) -> str:
	return value.replace('&', '&amp;').replace('"', '&quot;').replace("'", '&apos;').replace('<', '&lt;').replace('>', '&gt;')


def build_android_manifest_meta_data(config: foconfig.ConfigSection) -> str:
	entries: list[str] = []

	for key, value in sorted(config.content.items()):
		if not key.startswith(ANDROID_MANIFEST_METADATA_CONFIG_PREFIX):
			continue

		name = key[len(ANDROID_MANIFEST_METADATA_CONFIG_PREFIX):].strip()
		assert name, 'Android.ManifestMetaData.* key must include a manifest meta-data name'
		assert value, 'Android.ManifestMetaData.' + name + ' must not be empty'

		entries.append(
			'        <meta-data\n'
			'            android:name="' + escape_android_manifest_attribute(name) + '"\n'
			'            android:value="' + escape_android_manifest_attribute(value) + '" />'
		)

	return '\n'.join(entries)


def build_android_gradle_maven_repositories(config: foconfig.ConfigSection) -> str:
	entries: list[str] = []

	for key, value in sorted(config.content.items()):
		if not key.startswith(ANDROID_GRADLE_MAVEN_REPOSITORY_CONFIG_PREFIX):
			continue

		name = key[len(ANDROID_GRADLE_MAVEN_REPOSITORY_CONFIG_PREFIX):].strip()
		assert name, 'Android.GradleMavenRepository.* key must include a repository name'
		if not value:
			continue

		entries.append("        maven { url = uri('" + escape_groovy_string(value) + "') }")

	return '\n'.join(entries)


def build_android_gradle_dependencies(config: foconfig.ConfigSection) -> str:
	entries: list[str] = []

	for key, value in sorted(config.content.items()):
		if not key.startswith(ANDROID_GRADLE_DEPENDENCY_CONFIG_PREFIX):
			continue

		name = key[len(ANDROID_GRADLE_DEPENDENCY_CONFIG_PREFIX):].strip()
		assert name, 'Android.GradleDependency.* key must include a dependency name'
		if not value:
			continue

		entries.append('    ' + value)

	return '\n'.join(entries)


def read_android_ndk_revision(android_ndk_root: str) -> str:
	if not android_ndk_root:
		return ''

	source_properties = Path(android_ndk_root) / 'source.properties'
	try:
		lines = source_properties.read_text(encoding='utf-8').splitlines()
	except OSError:
		return ''

	for line in lines:
		key, separator, value = line.partition('=')
		if separator and key.strip() == 'Pkg.Revision':
			return value.strip()

	return ''


def load_config_from_data(config_data: bytes) -> foconfig.ConfigParser:
	config = foconfig.ConfigParser()
	config.loadFromLines(config_data.decode('utf-8-sig').splitlines())
	return config


def is_png_data(data: bytes) -> bool:
	return data.startswith(PNG_FILE_SIGNATURE)


def normalize_android_arch(arch: str) -> str:
	canonical_arch = ANDROID_ARCH_ALIASES.get(arch)
	assert canonical_arch is not None, 'Unknown Android architecture ' + arch
	return canonical_arch


def resolve_android_abi(arch: str) -> str:
	return ANDROID_ABI_BY_ARCH[normalize_android_arch(arch)]


def zip_entry_matches_file(
	archive: zipfile.ZipFile,
	archive_info: zipfile.ZipInfo,
	file_path: str,
	expected_mode: int | None = None,
) -> bool:
	if archive_info.file_size != os.path.getsize(file_path):
		return False
	if expected_mode is not None and (archive_info.external_attr >> 16) & 0o777 != expected_mode:
		return False

	with archive.open(archive_info) as archive_file, open(file_path, 'rb') as source_file:
		while True:
			archive_chunk = archive_file.read(1024 * 1024)
			source_chunk = source_file.read(1024 * 1024)
			if archive_chunk != source_chunk:
				return False
			if not archive_chunk:
				return True


def validate_resource_zip(archive_source: str | Path | IO[bytes], expected_entries: Sequence[str], description: str | None = None) -> None:
	archive_name = description if description is not None else str(archive_source)
	try:
		with zipfile.ZipFile(archive_source, 'r') as archive:
			actual_entries = [info.filename for info in archive.infolist()]
			assert actual_entries == list(expected_entries), f'Resource pack entry list mismatch: {archive_name}'

			for info in archive.infolist():
				try:
					with archive.open(info) as entry:
						while entry.read(1024 * 1024):
							pass
				except Exception as error:
					raise AssertionError(f'Resource pack validation failed for {archive_name}: {info.filename}: {error}') from error
	except AssertionError:
		raise
	except Exception as error:
		raise AssertionError(f'Resource pack validation failed for {archive_name}: {error}') from error


def make_zip(
	name: str | Path,
	path: str | Path,
	compress_level: int,
	mode: Literal['w', 'a'] = 'w',
	mode_overrides: Mapping[str, int] | None = None,
) -> None:
	mode_overrides = mode_overrides or {}
	with zipfile.ZipFile(name, mode, zipfile.ZIP_DEFLATED, compresslevel=compress_level) as archive:
		existing_entries = {entry.filename: entry for entry in archive.infolist()}

		for root, _, files in os.walk(path):
			for file_name in files:
				file_path = os.path.join(root, file_name)
				archive_name = os.path.relpath(file_path, path).replace(os.sep, '/')
				logical_mode = mode_overrides.get(archive_name)
				existing_entry = existing_entries.get(archive_name)
				if existing_entry is not None:
					assert zip_entry_matches_file(archive, existing_entry, file_path, logical_mode), 'Conflicting zip entry while merging package parts: ' + archive_name
					continue

				if logical_mode is None:
					archive.write(file_path, archive_name)
				else:
					assert logical_mode in PACKAGE_FILE_MODES, 'Unsupported package file mode: ' + format(logical_mode, '03o')
					info = zipfile.ZipInfo.from_file(file_path, archive_name)
					info.create_system = 3
					info.compress_type = zipfile.ZIP_DEFLATED
					info.external_attr = logical_mode << 16
					with open(file_path, 'rb') as source, archive.open(info, 'w') as destination:
						shutil.copyfileobj(source, destination)
				existing_entries[archive_name] = archive.getinfo(archive_name)


def resolve_safe_relative_path(root: Path, relative_path: str, description: str) -> Path:
	path = Path(relative_path)
	assert not path.is_absolute(), f'{description} must be relative: {relative_path}'
	assert '..' not in path.parts, f'{description} must not escape its root: {relative_path}'
	resolved_root = root.resolve()
	resolved_path = (resolved_root / path).resolve()
	assert resolved_path == resolved_root or resolved_root in resolved_path.parents, f'{description} escapes its root: {relative_path}'
	return resolved_path


def validate_package_mode_path(relative_path: str) -> None:
	assert relative_path and '\\' not in relative_path, f'Package mode path must use POSIX separators: {relative_path!r}'
	posix_path = PurePosixPath(relative_path)
	windows_path = PureWindowsPath(relative_path)
	assert not posix_path.is_absolute() and not windows_path.drive, f'Package mode path must be relative: {relative_path}'
	assert relative_path == posix_path.as_posix(), f'Package mode path is not normalized: {relative_path}'
	assert all(part not in ('', '.', '..') for part in posix_path.parts), f'Package mode path must not escape its root: {relative_path}'


def read_package_mode_manifest(package_root: Path) -> dict[str, int]:
	manifest_path = package_root / PACKAGE_MODE_MANIFEST
	if not manifest_path.is_file():
		return {}

	try:
		manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
	except (OSError, json.JSONDecodeError) as error:
		raise AssertionError(f'Invalid package mode manifest {manifest_path}: {error}') from error

	assert isinstance(manifest, dict), f'Package mode manifest must be an object: {manifest_path}'
	assert manifest.get('version') == PACKAGE_MODE_MANIFEST_VERSION, f'Unsupported package mode manifest version: {manifest.get("version")!r}'
	assert set(manifest) == {'version', 'files'}, f'Package mode manifest has unknown fields: {manifest_path}'
	files = manifest.get('files')
	assert isinstance(files, dict), f'Package mode manifest files must be an object: {manifest_path}'

	modes: dict[str, int] = {}
	for relative_path, encoded_mode in files.items():
		assert isinstance(relative_path, str), f'Package mode path must be a string: {relative_path!r}'
		validate_package_mode_path(relative_path)
		assert isinstance(encoded_mode, str) and re.fullmatch(r'[0-7]{3}', encoded_mode), f'Invalid package mode for {relative_path}: {encoded_mode!r}'
		logical_mode = int(encoded_mode, 8)
		assert logical_mode in PACKAGE_FILE_MODES, f'Unsupported package mode for {relative_path}: {encoded_mode}'
		file_path = resolve_safe_relative_path(package_root, relative_path, 'Package mode path')
		assert file_path.is_file(), f'Package mode path is not a file: {relative_path}'
		modes[relative_path] = logical_mode
	return modes


def write_package_mode_manifest(package_root: Path, modes: Mapping[str, int]) -> None:
	manifest_path = package_root / PACKAGE_MODE_MANIFEST
	if not modes:
		manifest_path.unlink(missing_ok=True)
		return

	for relative_path, logical_mode in modes.items():
		validate_package_mode_path(relative_path)
		assert logical_mode in PACKAGE_FILE_MODES, f'Unsupported package mode for {relative_path}: {logical_mode!r}'
		assert resolve_safe_relative_path(package_root, relative_path, 'Package mode path').is_file(), f'Package mode path is not a file: {relative_path}'

	payload = {
		'version': PACKAGE_MODE_MANIFEST_VERSION,
		'files': {relative_path: format(logical_mode, '03o') for relative_path, logical_mode in sorted(modes.items())},
	}
	with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=package_root, prefix=PACKAGE_MODE_MANIFEST + '.', suffix='.tmp', delete=False) as output:
		temporary_path = Path(output.name)
		json.dump(payload, output, indent=2)
		output.write('\n')
	try:
		os.replace(temporary_path, manifest_path)
	finally:
		temporary_path.unlink(missing_ok=True)


def iter_package_include_files(target_root: Path) -> list[Path]:
	return sorted(path for path in target_root.rglob('*') if path.is_file())


def make_package_include_zip_info(archive_name: str, file_path: Path) -> zipfile.ZipInfo:
	info = zipfile.ZipInfo(filename=archive_name, date_time=(1980, 1, 1, 0, 0, 0))
	info.create_system = 3
	info.compress_type = zipfile.ZIP_DEFLATED
	info.external_attr = stat.S_IMODE(file_path.stat().st_mode) << 16
	return info


def write_package_include_entries(
	archive: zipfile.ZipFile,
	package_root: Path,
	target_root: Path,
) -> None:
	for file_path in iter_package_include_files(target_root):
		archive_name = file_path.relative_to(package_root).as_posix()
		info = make_package_include_zip_info(archive_name, file_path)
		with file_path.open('rb') as source, archive.open(info, 'w') as destination:
			shutil.copyfileobj(source, destination)


def update_package_include_single_zip(
	archive_path: Path,
	package_root: Path,
	target_root: Path,
	compress_level: int,
) -> None:
	if not archive_path.is_file():
		log('SingleZip not present; included files remain in package root only', archive_path)
		return

	target_archive_path = target_root.relative_to(package_root).as_posix()
	target_archive_prefix = target_archive_path.rstrip('/') + '/'
	with zipfile.ZipFile(archive_path, 'r') as archive:
		has_previous_entries = any(
			info.filename == target_archive_path or info.filename.startswith(target_archive_prefix)
			for info in archive.infolist()
		)

	if not has_previous_entries:
		with zipfile.ZipFile(archive_path, 'a', zipfile.ZIP_DEFLATED, compresslevel=compress_level) as archive:
			write_package_include_entries(archive, package_root, target_root)
		return

	file_descriptor, temporary_name = tempfile.mkstemp(prefix=archive_path.name + '.', suffix='.tmp', dir=archive_path.parent)
	os.close(file_descriptor)
	temporary_path = Path(temporary_name)
	try:
		with zipfile.ZipFile(archive_path, 'r') as source_archive, zipfile.ZipFile(
			temporary_path,
			'w',
			zipfile.ZIP_DEFLATED,
			compresslevel=compress_level,
		) as target_archive:
			target_archive.comment = source_archive.comment
			for info in source_archive.infolist():
				if info.filename == target_archive_path or info.filename.startswith(target_archive_prefix):
					continue
				target_archive.writestr(info, source_archive.read(info))
			write_package_include_entries(target_archive, package_root, target_root)
		os.replace(temporary_path, archive_path)
	finally:
		if temporary_path.exists():
			temporary_path.unlink()


def include_package_files(
	input_root: Path,
	source_glob: str,
	package_root: Path,
	target_path: str,
	single_zip_path: Path,
	compress_level: int,
) -> None:
	input_root = input_root.resolve()
	package_root = package_root.resolve()
	single_zip_path = single_zip_path.resolve()
	assert input_root.is_dir(), f'Package include input root not found: {input_root}'
	assert package_root.is_dir(), f'Assembled package root not found: {package_root}'
	assert source_glob and not Path(source_glob).is_absolute(), f'Package include source glob must be relative: {source_glob}'
	assert '..' not in Path(source_glob).parts, f'Package include source glob must not escape its root: {source_glob}'
	assert target_path not in ('', '.'), 'Package include target path must name a package subdirectory'
	target_root = resolve_safe_relative_path(package_root, target_path, 'Package include target path')

	matches = sorted(
		(path for path in input_root.glob(source_glob) if path.is_file() or path.is_dir()),
		key=lambda path: path.as_posix(),
	)
	assert matches, f'Package include source glob matched no files: {source_glob}'
	for source_path in matches:
		resolved_source_path = source_path.resolve()
		assert resolved_source_path == input_root or input_root in resolved_source_path.parents, f'Package include source escapes input root: {source_path}'
		assert (
			resolved_source_path != package_root
			and resolved_source_path not in package_root.parents
			and package_root not in resolved_source_path.parents
		), f'Package include source overlaps package output: {source_path}'

	if target_root.exists():
		shutil.rmtree(target_root)
	target_root.mkdir(parents=True)

	for source_path in matches:
		destination_path = target_root / source_path.name
		if source_path.is_dir():
			shutil.copytree(source_path, destination_path, dirs_exist_ok=True)
		elif source_path.is_file():
			destination_path.parent.mkdir(parents=True, exist_ok=True)
			shutil.copy2(source_path, destination_path)

	log('Include', source_glob, '=>', target_root)
	update_package_include_single_zip(single_zip_path, package_root, target_root, compress_level)


def package_web_resources(
	output_path: Path,
	file_packager_path: Path,
	preload_files: Sequence[tuple[Path, str]],
	max_bundle_size: int = WEB_ASSET_BUNDLE_LIMIT,
) -> None:
	assert 0 < max_bundle_size <= WEB_ASSET_BUNDLE_LIMIT, 'Invalid Web asset bundle limit'
	assert preload_files, 'Web package requires preloaded files'
	bundles: list[list[tuple[Path, str]]] = [[]]
	bundle_size = 0
	seen_paths: set[str] = set()
	for source_path, virtual_path in sorted(preload_files, key=lambda entry: entry[1]):
		assert virtual_path not in seen_paths, f'Duplicate Web asset path: {virtual_path}'
		seen_paths.add(virtual_path)
		file_size = source_path.stat().st_size
		assert file_size <= max_bundle_size, f'Web asset exceeds bundle limit: {virtual_path} ({file_size} bytes)'
		if bundles[-1] and bundle_size + file_size > max_bundle_size:
			bundles.append([])
			bundle_size = 0
		bundles[-1].append((source_path, virtual_path))
		bundle_size += file_size

	with tempfile.TemporaryDirectory(prefix='web-preload-', dir=output_path) as temporary_path:
		loader_path = Path(temporary_path) / 'Resources.js'
		with loader_path.open('w', encoding='utf-8', newline='\n') as loader:
			for index, files in enumerate(bundles):
				bundle_name = f'Resources-{index}'
				bundle_loader_path = Path(temporary_path) / (bundle_name + '.js')
				arguments = [(output_path / (bundle_name + '.data')).as_posix(), '--preload']
				arguments.extend(source.as_posix().replace('@', '@@') + '@' + target.replace('@', '@@') for source, target in files)
				# Init.cmake guarantees FORCE_FILESYSTEM; --quiet acknowledges only that standalone reminder
				arguments.extend(['--js-output=' + bundle_loader_path.as_posix(), '--lz4', '--quiet'])
				response_path = Path(temporary_path) / (bundle_name + '.rsp.utf-8')
				response_path.write_text(shlex.join(arguments), encoding='utf-8')
				log('Package Web asset bundle', bundle_name, f'({len(files)} files, {sum(source.stat().st_size for source, _ in files)} bytes)')
				result = subprocess.call(
					[sys.executable or 'python3', str(file_packager_path), '@' + str(response_path)],
					env={**os.environ, 'EM_FILE_PACKAGER_MAX_CHUNK_SIZE_MB': str(WEB_ASSET_BUNDLE_LIMIT // (1024 * 1024))},
				)
				assert result == 0, f'Emscripten tools/file_packager.py failed for {bundle_name}: {result}'
				loader.write(bundle_loader_path.read_text(encoding='utf-8'))
				loader.write('\n')
		loader_path.replace(output_path / 'Resources.js')


def make_tar(
	name: str | Path,
	path: str | Path,
	mode: Literal['w', 'w:gz'],
	mode_overrides: Mapping[str, int] | None = None,
) -> None:
	mode_overrides = mode_overrides or {}
	archive_root = os.path.basename(os.fspath(path)).replace(os.sep, '/')

	def filter_member(tar_info: tarfile.TarInfo) -> tarfile.TarInfo:
		relative_name = tar_info.name.replace('\\', '/')
		if relative_name.startswith(archive_root + '/'):
			relative_name = relative_name[len(archive_root) + 1:]
		logical_mode = mode_overrides.get(relative_name)
		if logical_mode is not None:
			assert logical_mode in PACKAGE_FILE_MODES, 'Unsupported package file mode: ' + format(logical_mode, '03o')
			tar_info.mode = logical_mode
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
	target_config: foconfig.ConfigParser | None = field(init=False, default=None)
	logical_file_modes: dict[str, int] = field(init=False, default_factory=dict)
	resource_archive_paths: dict[str, str] = field(init=False, default_factory=dict)

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
		if self.args.platform == 'Windows':
			if self.args.binary_output_postfix and self.args.binary_output_postfix != self.args.config:
				path += '-' + self.args.binary_output_postfix
		else:
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
		if self.args.platform == 'Android':
			entry_arch = resolve_android_abi(arch)
		elif self.args.platform == 'Windows':
			entry_arch = buildtools.resolve_windows_binary_arch(arch)
		else:
			entry_arch = arch
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

	def build_client_runtime_companion_names(self, runtime_ext: str) -> set[str]:
		# Client and ClientHeadless share one binary output directory. Package jobs can
		# therefore see a sibling runtime left by another target/job even when that
		# variant is not requested by the current pack. Treat all engine-owned client
		# runtime input/alias names as application binaries, not dependency DLLs/DSOs;
		# the selected variant is copied explicitly under its packaged output name
		runtime_variants = (BinaryVariant(), BinaryVariant(role='Headless'))
		return {
			name + runtime_ext
			for variant in runtime_variants
			for name in (self.build_client_runtime_input_name(variant), self.build_client_runtime_alias_name(variant))
		}

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
		# follow the platform/arch prefix, so pick the longest matching known (platform, arch)
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
		# coexist under one binary_target_name and a client picks its own by PACKAGED_BUILD_NAME
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
			if remainder == opt or remainder.startswith(opt + '-'):
				remainder = remainder[len(opt):]
				break
		if remainder == '-Debug' or remainder.startswith('-Debug-'):
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
		copied_native_payloads: set[tuple[str, str]] = set()
		copied_resource_payloads: set[tuple[str, str]] = set()
		resource_payload_sources: dict[tuple[str, str], tuple[tuple[int, str], str]] = {}
		# A variant that never reaches PlatformBinaries leaves its players with 'update the client
		# manually' and nothing to act on, so every skip states its reason and the declared variants
		# are verified before the package is called done
		skipped_entries: list[str] = []
		client_embedded_data = self.make_embedded_data_for_target('Client')
		_, client_config_data = self.read_config_data('Client')
		managed_runtime_pack = self.find_client_managed_runtime_pack()

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

				default_runtime_variant = BinaryVariant()
				headless_runtime_variant = BinaryVariant(role='Headless')

				build_hash_path = os.path.join(entry_path, self.args.devname + '_Client.build-hash')
				if not os.path.isfile(build_hash_path):
					build_hash_path = os.path.join(entry_path, self.build_client_runtime_input_name(default_runtime_variant) + '.build-hash')
				if not os.path.isfile(build_hash_path):
					skipped_entries.append(entry_name + ': no build hash file at ' + build_hash_path)
					log('Client platform update payload skipped', entry_name, 'no build hash file')
					continue

				with open(build_hash_path, 'r', encoding='utf-8-sig') as file:
					build_hash = file.read().strip()
				if build_hash != self.args.buildhash:
					skipped_entries.append(entry_name + ': built from ' + build_hash + ', package is ' + self.args.buildhash)
					log('Client platform update payload skipped', entry_name, 'build hash', build_hash, '!= package build hash', self.args.buildhash)
					continue

				if managed_runtime_pack is not None:
					payload_key = (request_target_name, managed_runtime_pack)
					runtime_dir = os.path.join(entry_path, MANAGED_RUNTIME_DIRECTORY)
					self.read_managed_runtime_identity(runtime_dir)
					# One updater path serves all variants, whose independent equivalent CoreLib builds may differ.
					# Prefer the least-qualified entry
					source_priority = (len(entry_name), entry_name)
					previous_source = resource_payload_sources.get(payload_key)
					if previous_source is None or source_priority < previous_source[0]:
						resource_payload_sources[payload_key] = (source_priority, runtime_dir)

				parts = request_target_name.split('-', 1)
				if len(parts) != 2:
					continue

				platform = parts[0]
				if platform not in SELF_UPDATING_CLIENT_PLATFORMS:
					continue

				runtime_ext = self.get_runtime_library_ext_for_platform(platform)
				if not runtime_ext:
					continue

				suffix = ''
				variant_entry_name = entry_name[:-(len(entry_postfix) + 1)] if entry_postfix else entry_name
				if variant_entry_name.endswith(('-Profiling_Total', '-Profiling_OnDemand', '-Profiling_Total-Debug', '-Profiling_OnDemand-Debug')):
					suffix = '_Profiling'

				# binary_output_postfix is appended to the staged payload name so two
				# binary entries that map to the same request_target_name (e.g.
				# Client-Linux-x64 vs Client-Linux-x64-Steam) do not collide. Each
				# client variant patches its own PACKAGED_BUILD_NAME to match the
				# resulting suffixed payload, so updater's remap_runtime_name picks
				# the right file
				postfix_suffix = '_' + entry_postfix if entry_postfix else ''

				variant_specs: list[tuple[str, str | None, BinaryVariant]] = []
				variant_specs.append((self.args.nicename + suffix + postfix_suffix, None, default_runtime_variant))
				if platform == 'Windows':
					variant_specs.append((self.args.nicename + suffix + '_OpenGL' + postfix_suffix, 'ForceOpenGL=1', default_runtime_variant))
				headless_runtime_path = os.path.join(entry_path, self.build_client_runtime_input_name(headless_runtime_variant) + runtime_ext)
				if os.path.isfile(headless_runtime_path):
					variant_specs.append((self.args.nicename + suffix + '_Headless' + postfix_suffix, None, headless_runtime_variant))

				for output_name, variant_config_data, runtime_variant in variant_specs:
					runtime_input_name = self.build_client_runtime_input_name(runtime_variant)
					runtime_input_path = os.path.join(entry_path, runtime_input_name + runtime_ext)
					if not os.path.isfile(runtime_input_path):
						skipped_entries.append(entry_name + ': no runtime library at ' + runtime_input_path)
						log('Client runtime update payload skipped', entry_name, 'no runtime library', runtime_input_path)
						continue

					build_hash_path = Path(entry_path) / (runtime_input_name + '.build-hash')
					if not build_hash_path.is_file() or build_hash_path.read_text(encoding='utf-8-sig').strip() != self.args.buildhash:
						continue

					payload_target_name = request_target_name
					payload_key = (payload_target_name, output_name)
					if payload_key in copied_native_payloads:
						continue

					payload_dir = os.path.join(self.target_output_path, self.platform_binaries_dir, payload_target_name)
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
						# matching PDB, while an older host (whose PDB is build-specific) is never clobbered
						host_pdb_input = os.path.join(entry_path, self.build_client_runtime_alias_name(runtime_variant) + '.pdb')
						if os.path.isfile(host_pdb_input):
							host_pdb_out = os.path.join(payload_dir, output_name + '.pdb')
							log('Client host PDB included', host_pdb_out)
							shutil.copy(host_pdb_input, host_pdb_out)

					copied_native_payloads.add(payload_key)

		for (request_target_name, pack_name), (_, runtime_dir) in sorted(resource_payload_sources.items()):
			payload_dir = os.path.join(self.target_output_path, self.platform_binaries_dir, request_target_name)
			os.makedirs(payload_dir, exist_ok=True)
			output_path = os.path.join(payload_dir, pack_name + '.zip')
			log('Client managed resource payload', output_path)
			self.write_client_resource_pack_with_runtime(output_path, pack_name, runtime_dir)
			copied_resource_payloads.add((request_target_name, pack_name))

		self.verify_expected_client_runtime_payloads(
			copied_native_payloads, copied_resource_payloads, managed_runtime_pack, skipped_entries)

	@staticmethod
	def staged_payload_satisfies(copied_payloads: set[tuple[str, str]], target_name: str, output_name: str) -> bool:
		return (target_name, output_name) in copied_payloads

	def verify_expected_client_runtime_payloads(
		self,
		copied_native_payloads: set[tuple[str, str]],
		copied_resource_payloads: set[tuple[str, str]],
		managed_runtime_pack: str | None,
		skipped_entries: list[str],
	) -> None:
		# The server package declares which client variants it distributes. Without this check a variant
		# that was not built, or was built from another commit, is dropped in silence and the first
		# report comes from a player told to update the client by hand
		for expectation in getattr(self.args, 'expect_client_runtime', ()) or ():
			parts = expectation.split(':')
			assert len(parts) in (2, 3), 'Expected client runtime must be Platform:arch[:postfix], got: ' + expectation
			platform, arch = parts[0], parts[1]
			postfix = parts[2] if len(parts) == 3 else ''

			if platform == 'Android':
				entry_arch = resolve_android_abi(arch)
			elif platform == 'Windows':
				entry_arch = buildtools.resolve_windows_binary_arch(arch)
			else:
				entry_arch = arch
			binary_entry = 'Client-' + platform + '-' + entry_arch + ('-' + postfix if postfix else '')
			target_name = self.build_runtime_update_target_name(binary_entry)
			assert target_name is not None, 'Expected client runtime names an unknown platform/arch: ' + expectation

			if managed_runtime_pack is not None and (target_name, managed_runtime_pack) not in copied_resource_payloads:
				reasons = self.describe_missing_client_payloads(copied_resource_payloads, skipped_entries)
				raise AssertionError(
					'Client managed resource payload missing from the server package: expected '
					+ managed_runtime_pack + '.zip under PlatformBinaries/' + target_name
					+ ' (from ' + binary_entry + '). This client would receive managed class libraries for another platform. Skipped entries: '
					+ reasons)

			if platform not in SELF_UPDATING_CLIENT_PLATFORMS:
				continue

			output_name = self.args.nicename + ('_' + postfix if postfix else '')
			if self.staged_payload_satisfies(copied_native_payloads, target_name, output_name):
				continue

			reasons = self.describe_missing_client_payloads(copied_native_payloads, skipped_entries)
			raise AssertionError(
				'Client runtime payload missing from the server package: expected ' + output_name + ' under PlatformBinaries/' + target_name
				+ ' (from ' + binary_entry + '). Clients of this variant would be told to update manually. Skipped entries: ' + reasons)

	@staticmethod
	def describe_missing_client_payloads(copied_payloads: set[tuple[str, str]], skipped_entries: list[str]) -> str:
		if skipped_entries:
			return '; '.join(skipped_entries)
		if copied_payloads:
			return 'staged ' + ', '.join(sorted(target + '/' + name for target, name in copied_payloads))
		return 'no client binaries directory was found for it'

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

	def record_logical_file_mode(self, file_path: str | Path, logical_mode: int) -> None:
		assert logical_mode in PACKAGE_FILE_MODES, 'Unsupported package file mode: ' + format(logical_mode, '03o')
		relative_path = Path(file_path).resolve().relative_to(Path(self.target_output_path).resolve()).as_posix()
		validate_package_mode_path(relative_path)
		if not hasattr(self, 'logical_file_modes'):
			self.logical_file_modes = {}
		self.logical_file_modes[relative_path] = logical_mode

	def persist_logical_file_modes(self) -> None:
		package_root = Path(self.output_path)
		target_root = Path(self.target_output_path)
		target_prefix = target_root.resolve().relative_to(package_root.resolve()).as_posix().rstrip('/') + '/'
		all_modes = {
			path: mode
			for path, mode in read_package_mode_manifest(package_root).items()
			if not path.startswith(target_prefix)
		}

		logical_file_modes = getattr(self, 'logical_file_modes', {})
		if self.has_pack('Raw'):
			all_modes.update({target_prefix + path: mode for path, mode in logical_file_modes.items()})
		if self.has_pack('Root'):
			all_modes.update(logical_file_modes)

		write_package_mode_manifest(package_root, all_modes)

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
		zip_entries = sorted((os.path.relpath(file_path, base_path).replace(os.sep, '/'), file_path) for file_path in files)
		self.write_zip_entries(archive_path, zip_entries)

	def resource_archive_cache_key(self, zip_entries: Sequence[tuple[str, str]]) -> str:
		digest = hashlib.sha256()
		digest.update(struct.pack('<II', RESOURCE_ARCHIVE_CACHE_FORMAT, self.zip_compress_level))

		for arcname, file_path in zip_entries:
			name = arcname.encode('utf-8')
			digest.update(struct.pack('<QQ', len(name), os.path.getsize(file_path)))
			digest.update(name)

			with open(file_path, 'rb') as source:
				for chunk in iter(lambda: source.read(RESOURCE_ARCHIVE_HASH_CHUNK_BYTES), b''):
					digest.update(chunk)

		return digest.hexdigest()

	def run_resource_archive_cache_helper(self, action: str, key: str, archive_path: str) -> int | None:
		if getattr(self, 'resource_archive_cache_unavailable', False):
			return None

		helper = os.environ.get(RESOURCE_ARCHIVE_CACHE_HELPER_ENV)

		if not helper:
			return None

		assert os.path.isfile(helper), RESOURCE_ARCHIVE_CACHE_HELPER_ENV + ' is not a file: ' + helper
		status = subprocess.run(
			[sys.executable, helper, action, '--key', key, '--archive', archive_path], check=False).returncode

		if status == RESOURCE_ARCHIVE_CACHE_UNAVAILABLE:
			self.resource_archive_cache_unavailable = True

		return status

	def restore_resource_archive(self, archive_path: str, key: str, entry_names: Sequence[str]) -> bool:
		local_archives = getattr(self, 'resource_archive_paths', {})
		local_path = local_archives.get(key)

		if local_path is not None and os.path.isfile(local_path):
			if os.path.realpath(local_path) != os.path.realpath(archive_path):
				shutil.copy2(local_path, archive_path)

			validate_resource_zip(archive_path, entry_names)
			log('Resource archive local hit', key)
			return True

		status = self.run_resource_archive_cache_helper('restore', key, archive_path)

		if status is None or status in (RESOURCE_ARCHIVE_CACHE_MISS, RESOURCE_ARCHIVE_CACHE_UNAVAILABLE):
			return False

		assert status == 0, 'Resource archive cache restore failed with exit code ' + str(status)
		validate_resource_zip(archive_path, entry_names)
		log('Resource archive cache hit', key)
		return True

	def remember_resource_archive(self, archive_path: str, key: str) -> None:
		if not hasattr(self, 'resource_archive_paths'):
			self.resource_archive_paths = {}

		archive_identity = os.path.realpath(archive_path)
		self.resource_archive_paths = {
			cached_key: cached_path
			for cached_key, cached_path in self.resource_archive_paths.items()
			if os.path.realpath(cached_path) != archive_identity
		}
		self.resource_archive_paths[key] = archive_path

	def write_zip_entries(self, archive_path: str, zip_entries: Sequence[tuple[str, str]]) -> None:
		zip_entries = sorted(zip_entries)
		entry_names = [arcname for arcname, _ in zip_entries]
		assert len(entry_names) == len(set(entry_names)), 'Duplicate resource zip entry in ' + archive_path
		cache_key = self.resource_archive_cache_key(zip_entries)

		if self.restore_resource_archive(archive_path, cache_key, entry_names):
			self.remember_resource_archive(archive_path, cache_key)
			return

		try:
			with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=self.zip_compress_level) as archive:
				for arcname, file_path in zip_entries:
					self.write_stable_zip_entry(archive, file_path, arcname)

			validate_resource_zip(archive_path, entry_names)
		except Exception:
			self.run_resource_archive_cache_helper('release', cache_key, archive_path)
			raise

		status = self.run_resource_archive_cache_helper('store', cache_key, archive_path)
		assert status in (None, 0), 'Resource archive cache store failed with exit code ' + str(status)
		self.remember_resource_archive(archive_path, cache_key)

	def find_client_managed_runtime_pack(self) -> str | None:
		assert self.baking_path, 'Baking path is not initialized'
		managed_packs = [
			pack_name
			for pack_name in self.get_target_resource_packs('Client')
			if os.path.isdir(os.path.join(self.baking_path, pack_name, MANAGED_RUNTIME_DIRECTORY))
		]
		assert len(managed_packs) <= 1, 'Managed runtime payload must belong to exactly one client resource pack'
		if not managed_packs:
			return None

		self.read_managed_runtime_identity(os.path.join(self.baking_path, managed_packs[0], MANAGED_RUNTIME_DIRECTORY))
		return managed_packs[0]

	@staticmethod
	def read_managed_runtime_identity(runtime_dir: str) -> bytes:
		manifest_path = os.path.join(runtime_dir, MANAGED_RUNTIME_MANIFEST)
		corelib_path = os.path.join(runtime_dir, MANAGED_CORELIB_RELATIVE_PATH)
		assert os.path.isfile(manifest_path), 'Managed runtime manifest not found: ' + manifest_path
		assert os.path.isfile(corelib_path), 'Managed System.Private.CoreLib.dll not found: ' + corelib_path
		with open(manifest_path, 'rb') as manifest_file:
			identity = manifest_file.read()
		assert identity, 'Managed runtime manifest is empty: ' + manifest_path
		return identity

	def write_client_resource_pack_with_runtime(self, archive_path: str, pack_name: str, runtime_dir: str) -> None:
		assert self.baking_path, 'Baking path is not initialized'
		self.read_managed_runtime_identity(runtime_dir)

		pack_base = os.path.join(self.baking_path, pack_name)
		baked_runtime_base = os.path.realpath(os.path.join(pack_base, MANAGED_RUNTIME_DIRECTORY))
		zip_entries = [
			(os.path.relpath(file_path, pack_base).replace(os.sep, '/'), file_path)
			for file_path in self.collect_resource_files(pack_name, 'Client')
			if os.path.commonpath((baked_runtime_base, os.path.realpath(file_path))) != baked_runtime_base
		]

		runtime_files = sorted(
			file_path
			for file_path in glob.glob(os.path.join(runtime_dir, '**'), recursive=True)
			if os.path.isfile(file_path)
		)
		assert runtime_files, 'Managed runtime payload is empty: ' + runtime_dir
		zip_entries.extend(
			(
				MANAGED_RUNTIME_DIRECTORY + '/' + os.path.relpath(file_path, runtime_dir).replace(os.sep, '/'),
				file_path,
			)
			for file_path in runtime_files
		)
		self.write_zip_entries(archive_path, zip_entries)

	def package_client_managed_runtime_resources(self) -> None:
		managed_runtime_pack = self.find_client_managed_runtime_pack()
		if managed_runtime_pack is None:
			return

		packaged_identity: bytes | None = None
		packaged_runtime_dir: str | None = None
		for arch in self.iter_arches():
			binary_entry = self.build_binary_entry(arch, BinaryVariant())
			bin_path = self.get_input(os.path.join('Binaries', binary_entry), self.args.devname + '_Client')
			runtime_dir = os.path.join(bin_path, MANAGED_RUNTIME_DIRECTORY)
			runtime_identity = self.read_managed_runtime_identity(runtime_dir)
			assert packaged_identity is None or packaged_identity == runtime_identity, (
				'Client package architectures carry different managed runtime payloads')
			packaged_identity = runtime_identity
			packaged_runtime_dir = runtime_dir

		assert packaged_runtime_dir is not None, 'Client package has no managed runtime source architecture'
		archive_path = os.path.join(self.target_output_path, self.client_res_dir, managed_runtime_pack + '.zip')
		log('Replace baked managed runtime with client platform payload', archive_path)
		self.write_client_resource_pack_with_runtime(archive_path, managed_runtime_pack, packaged_runtime_dir)

	def write_stable_zip_entry(self, archive: zipfile.ZipFile, file_path: str, arcname: str) -> None:
		info = zipfile.ZipInfo(filename=arcname, date_time=(1980, 1, 1, 0, 0, 0))
		info.create_system = 3
		info.compress_type = zipfile.ZIP_DEFLATED
		info.external_attr = 0o644 << 16
		with open(file_path, 'rb') as src, archive.open(info, 'w') as dst:
			shutil.copyfileobj(src, dst)

	def make_embedded_pack(self, files: Sequence[str], base_path: str) -> bytes:
		embedded_buffer = io.BytesIO()
		zip_entries = [os.path.relpath(file_path, base_path).replace(os.sep, '/') for file_path in files]
		with zipfile.ZipFile(embedded_buffer, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=self.zip_compress_level) as archive:
			for arcname, file_path in zip(zip_entries, files):
				self.write_stable_zip_entry(archive, file_path, arcname)
		data = embedded_buffer.getvalue()
		validate_resource_zip(io.BytesIO(data), zip_entries, 'embedded resource pack')
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
		self.target_config = load_config_from_data(self.config_data)
		log('Config', config_name)
		log('Embedded data length', len(self.embedded_data))
		log('Embedded config length', len(self.config_data))

	def get_effective_config_section(self) -> foconfig.ConfigSection:
		return self.target_config.mainSection() if self.target_config else self.fomain.mainSection()

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

		client_runtime_companions = self.build_client_runtime_companion_names('.dll') if self.args.target == 'Client' else set()

		for arch in self.iter_arches():
			# Mirror of the suffix appended to server-side payloads in
			# package_all_client_runtime_update_payloads: tagging the client output
			# name keeps PACKAGED_BUILD_NAME aligned with what the server stages
			# under PlatformBinaries/<target>/<name>.dll for this variant
			binary_output_postfix = self.args.binary_output_postfix
			client_postfix_suffix = '_' + binary_output_postfix if binary_output_postfix else ''
			for variant in self.iter_windows_variants():
				is_lib = self.has_pack('Lib')
				bin_name = self.args.devname + '_' + self.args.target + variant.role + ('Lib' if is_lib else '')
				log('Setup', arch, bin_name, variant.log_name())

				bin_out_name = bin_name + self.build_output_variant_suffix(variant, is_windows=True) if self.args.target != 'Client' else self.args.nicename + ('_' + variant.role if variant.role else '') + self.build_output_variant_suffix(variant, is_windows=True) + client_postfix_suffix
				bin_path = self.resolve_binary_input_dir(arch, variant, bin_name)
				bin_ext = '.dll' if is_lib else '.exe'
				log('Binary input', bin_path)

				additional_config_data = 'ForceOpenGL=1' if variant.graphics == 'OGL' else None
				excluded_companions = set(client_runtime_companions)

				if self.args.target == 'Client' and not is_lib:
					runtime_input_name = self.build_client_runtime_input_name(variant)
					runtime_out_name = bin_out_name
					runtime_dll_path = self.package_platform_binary(bin_path, runtime_input_name, runtime_out_name, '.dll', additional_config_data, excluded_companions=excluded_companions)
					self.copy_runtime_pdb(bin_path, runtime_input_name, runtime_dll_path)

				main_binary_path = self.package_platform_binary(bin_path, bin_name, bin_out_name, bin_ext, additional_config_data, excluded_companions)
				self.copy_pdb(bin_path, bin_name, bin_out_name)
				if bin_ext == '.exe':
					# Point the frozen host exe at its sibling `<name>.pdb` (renamed from the
					# build's `<bin_name>.pdb`). Otherwise the exe keeps the build-machine
					# CodeView path and only resolves symbols via a debugger's module-adjacent
					# basename heuristic
					assert patch_pe_pdb_path(main_binary_path, bin_out_name + '.pdb'), 'Host exe RSDS not patched: ' + main_binary_path

	def package_linux(self) -> None:
		if self.args.target == 'Server' and not self.has_pack('NoRes'):
			self.package_all_client_runtime_update_payloads()

		all_linux_variants = self.iter_linux_variants()
		cross_variant_excluded = self.build_client_runtime_companion_names('.so') if self.args.target == 'Client' else set()

		# Mirrors the postfix tagging in package_all_client_runtime_update_payloads
		# so the Linux client's PACKAGED_BUILD_NAME matches the server-staged payload
		# for this binary_output_postfix variant
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
					runtime_out_name = bin_out_name
					self.package_platform_binary(bin_path, runtime_input_name, runtime_out_name, '.so', additional_config_data, excluded_companions=excluded_companions)

				output_file_path = self.package_platform_binary(bin_path, bin_name, bin_out_name, '', additional_config_data, excluded_companions)

				self.record_logical_file_mode(output_file_path, 0o755)
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

		preload_roots = [
			(Path(self.target_output_path) / self.client_res_dir, self.client_res_dir),
		]
		preload_files = [
			(file_path, '/' + virtual_root + '/' + file_path.relative_to(root).as_posix())
			for root, virtual_root in preload_roots
			for file_path in root.rglob('*') if file_path.is_file()
		]
		package_web_resources(Path(self.target_output_path), Path(file_packager_path), preload_files)

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

	def copy_android_java_sources(self, android_config: foconfig.ConfigSection, package_name: str) -> None:
		output_java_dir = os.path.join(self.target_output_path, 'app', 'src', 'main', 'java', self.build_android_java_package_path(package_name))
		os.makedirs(output_java_dir, exist_ok=True)

		for key, value in sorted(android_config.content.items()):
			if not key.startswith(ANDROID_JAVA_SOURCE_CONFIG_PREFIX):
				continue

			name = key[len(ANDROID_JAVA_SOURCE_CONFIG_PREFIX):].strip()
			assert name, 'Android.JavaSource.* key must include a source name'
			if not value:
				continue

			source_path = self.resolve_config_relative_path(value)
			assert os.path.isfile(source_path), 'Android Java source file not found: ' + source_path
			source_name = os.path.basename(source_path)
			assert source_name.endswith('.java'), 'Android.JavaSource.' + name + ' must point to a .java file: ' + source_path
			assert source_name != ANDROID_ACTIVITY_CLASS + '.java', 'Android.JavaSource.* must not override ' + ANDROID_ACTIVITY_CLASS + '.java'

			output_path = os.path.join(output_java_dir, source_name)
			shutil.copy(source_path, output_path)
			patch_file(output_path, '$PACKAGE$', package_name)
			patch_file(output_path, '$CONFIG$', self.args.config)
			log('Android Java source', source_path, '=>', output_path)

	def try_read_android_icon_png(self, icon_path: str) -> bytes | None:
		with open(icon_path, 'rb') as file:
			icon_data = file.read()

		return icon_data if is_png_data(icon_data) else None

	def patch_android_icon(self) -> None:
		configured_icon = self.get_effective_config_section().getStr('Android.Icon', 'Engine/Resources/Radiation.png')
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

		# Read Android config from the baked target config so SubConfig overrides affect APK metadata
		android_config = self.get_effective_config_section()
		package_name = android_config.getStr('Android.PackageName', 'com.fonline.app')
		version_code = android_config.getStr('Android.VersionCode', '1')
		version_name = self.args.buildhash[:8] if self.args.buildhash else '1.0'
		min_sdk = android_config.getStr('Android.MinSdk', '23')
		target_sdk = android_config.getStr('Android.TargetSdk', '35')
		compile_sdk = android_config.getStr('Android.CompileSdk', '35')
		screen_orientation = android_config.getStr('Android.ScreenOrientation', 'landscape')
		release_store_file = self.resolve_optional_config_relative_path(android_config.getStr('Android.Keystore', ''))
		release_store_password = android_config.getStr('Android.KeystorePassword', '')
		release_key_alias = android_config.getStr('Android.KeyAlias', '')
		release_key_password = android_config.getStr('Android.KeyPassword', '')
		android_env = buildtools.resolve_env()
		android_home = android_env.get('FO_ANDROID_HOME', '') or android_env.get('FO_ANDROID_SDK_ROOT', '')
		android_ndk_root = android_env.get('FO_ANDROID_NDK_ROOT', '')
		android_ndk_version = read_android_ndk_revision(android_ndk_root)

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
		patch_file(app_build_gradle, '$NDK_VERSION$', escape_groovy_string(android_ndk_version))
		patch_file(app_build_gradle, '$NDK_PATH$', escape_groovy_string(Path(android_ndk_root).as_posix() if android_ndk_root else ''))
		patch_file(app_build_gradle, '$ANDROID_GRADLE_DEPENDENCIES$', build_android_gradle_dependencies(android_config))
		patch_file(os.path.join(self.target_output_path, 'build.gradle'), '$ANDROID_GRADLE_MAVEN_REPOSITORIES$', build_android_gradle_maven_repositories(android_config))
		patch_file(os.path.join(self.target_output_path, 'app', 'proguard-rules.pro'), '$PACKAGE$', package_name)
		self.patch_android_activity(package_name)
		self.copy_android_java_sources(android_config, package_name)

		# Patch AndroidManifest.xml
		manifest_path = os.path.join(self.target_output_path, 'app', 'src', 'main', 'AndroidManifest.xml')
		patch_file(manifest_path, '$VERSION_CODE$', version_code)
		patch_file(manifest_path, '$VERSION_NAME$', version_name)
		patch_file(manifest_path, '$APP_NAME$', self.args.nicename)
		patch_file(manifest_path, '$SCREEN_ORIENTATION$', screen_orientation)
		patch_file(manifest_path, '$ANDROID_MANIFEST_META_DATA$', build_android_manifest_meta_data(android_config))

		# Patch strings.xml
		strings_path = os.path.join(self.target_output_path, 'app', 'src', 'main', 'res', 'values', 'strings.xml')
		patch_file(strings_path, '$APP_NAME$', self.args.nicename)
		self.patch_android_icon()

		if android_home:
			local_properties_path = os.path.join(self.target_output_path, 'local.properties')
			with open(local_properties_path, 'w', encoding='utf-8', newline='\n') as file:
				file.write('sdk.dir=' + Path(android_home).as_posix() + '\n')
			log('Android local.properties', local_properties_path)

		# Build APK if requested
		if self.has_pack('Apk'):
			log('Building APK...')
			gradlew_name = 'gradlew.bat' if os.name == 'nt' else 'gradlew'
			gradlew = os.path.join(self.target_output_path, gradlew_name)
			if os.name != 'nt':
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
		# Packaging.CodeSigningHook is an owner-provided executable script called once per PE as `<hook> <abs-path>`;
		# the script owns the tool (osslsigncode / signtool / Azure Trusted Signing / SSL.com eSigner), the
		# certificate, the timestamp URL and any secrets (kept out of the repo and the main config). Empty hook =
		# unsigned (today's behavior). A signing failure is fatal so a release that asked to be signed never ships
		# unsigned
		if self.args.platform != 'Windows':
			return

		hook = self.resolve_optional_config_relative_path(self.fomain.mainSection().getStr('Packaging.CodeSigningHook', ''))
		if not hook:
			log('Code signing: skipped (Packaging.CodeSigningHook not set)')
			return

		assert os.path.isfile(hook), 'Packaging.CodeSigningHook script not found: ' + hook

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
			assert result == 0, 'Packaging.CodeSigningHook failed (exit ' + str(result) + ') for: ' + binary
		log('Code signing: done')

	def finalize_output(self) -> None:
		logical_file_modes = getattr(self, 'logical_file_modes', {})
		self.sign_windows_binaries()

		if self.has_pack('Zip'):
			log('Create zipped archive')
			make_zip(self.target_output_path + '.zip', self.target_output_path, self.zip_compress_level, mode_overrides=logical_file_modes)

		if self.has_pack('SingleZip'):
			log('Add to single zip archive')
			single_zip_path = os.path.join(self.output_path, os.path.basename(self.output_path) + '.zip')
			make_zip(single_zip_path, self.target_output_path, self.zip_compress_level, 'a', logical_file_modes)

		if self.has_pack('Tar'):
			log('Create tar archive')
			make_tar(self.target_output_path + '.tar', self.target_output_path, 'w', logical_file_modes)

		if self.has_pack('TarGz'):
			log('Create tar.gz archive')
			make_tar(self.target_output_path + '.tar.gz', self.target_output_path, 'w:gz', logical_file_modes)

		if self.has_pack('Root'):
			shutil.copytree(self.target_output_path, self.output_path, dirs_exist_ok=True)

		if self.has_pack('Wix'):
			self.make_wix_installer()

		if not self.has_pack('Raw'):
			shutil.rmtree(self.target_output_path, True)

		self.persist_logical_file_modes()

	def resolve_game_version(self) -> str:
		# Resolve Common.GameVersion to a concrete value. The main config commonly points it at a file
		# (e.g. `Common.GameVersion = $FILE{VERSION}`); foconfig keeps directives verbatim, so resolve the
		# `$FILE{...}` indirection here relative to the main config directory
		raw = self.fomain.mainSection().getStr('Common.GameVersion', '0.0.0').strip()
		file_match = re.match(r'^\$FILE\{(.+)\}$', raw)
		if file_match:
			version_path = self.resolve_config_relative_path(file_match.group(1).strip())
			assert os.path.isfile(version_path), 'Common.GameVersion $FILE not found: ' + version_path
			with open(version_path, 'r', encoding='utf-8-sig') as version_file:
				raw = version_file.read().strip()
		return raw

	def ensure_msi_toolset(self) -> str:
		# The MSI is required when the Wix pack is requested, so verify the toolset up front and fail with a
		# clear message instead of a cryptic subprocess error. The host OS decides the toolset: WiX
		# (candle/light) on Windows, GNOME wixl elsewhere — matching msicreator/createmsi.py. On
		# Debian/Ubuntu wixl ships in its own "wixl" apt package (the "msitools" package carries only
		# msiinfo/msibuild/msidiff/msiextract and does NOT include wixl)
		if os.name == 'nt':
			candidate_roots: list[Path] = []
			configured_root = os.environ.get('FO_WIX_ROOT', '')
			if configured_root:
				candidate_roots.append(Path(configured_root))
			for input_path in self.args.input:
				candidate = Path(input_path).resolve().parent / 'wix3'
				if candidate not in candidate_roots:
					candidate_roots.append(candidate)

			for candidate_root in candidate_roots:
				if all((candidate_root / (tool + '.exe')).is_file() for tool in ('candle', 'light')):
					return str(candidate_root)

			missing = [tool for tool in ('candle', 'light') if shutil.which(tool) is None]
			assert not missing, 'Wix pack requires the WiX Toolset (' + ', '.join(missing) + ' not found); run buildtools.py prepare-workspace wix'
			return ''
		else:
			wixl = shutil.which('wixl')
			assert wixl is not None, 'Wix pack requires the "wixl" toolset on PATH (install the "wixl" package, e.g. apt-get install wixl)'
			version_output = subprocess.check_output([wixl, '--version'], text=True).strip()
			version_match = re.search(r'(\d+)\.(\d+)(?:\.(\d+))?', version_output)
			assert version_match is not None, 'Unable to determine wixl version from: ' + version_output
			version = tuple(int(part or '0') for part in version_match.groups())
			assert version >= (0, 102, 0), 'Wix pack directory UI requires wixl 0.102 or newer (found %s)' % version_output
			return ''

	def make_wix_installer(self) -> None:
		# Build a Windows MSI from the just-staged client payload (self.target_output_path) and register
		# the deep-link URI scheme so site login works without the client editing the registry at
		# runtime (installer-time registration is the AV-friendly path; the runtime self-register in
		# SourceExt/DeepLink.cpp stays as the fallback for the portable/zip/Steam builds). The MSI is a
		# required artifact: a missing toolset or a generator/build error fails the package. All
		# game-specific values come from the project config, so the engine packager stays game-agnostic
		assert self.args.platform == 'Windows' and self.args.target == 'Client', 'Wix pack is only valid for the Windows Client target'

		wix_root = self.ensure_msi_toolset()

		scheme = self.fomain.mainSection().getStr('Auth.UriScheme', '').strip()
		assert scheme, 'Wix pack requires Auth.UriScheme to register the deep-link URI scheme'

		game_name = self.fomain.mainSection().getStr('Common.GameName', self.args.nicename).strip() or self.args.nicename

		upgrade_code = self.fomain.mainSection().getStr('Packaging.MsiUpgradeCode', '').strip()
		assert re.match(r'^[0-9A-Fa-f]{8}-([0-9A-Fa-f]{4}-){3}[0-9A-Fa-f]{12}$', upgrade_code), 'Wix pack requires Packaging.MsiUpgradeCode to be a stable GUID (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)'
		upgrade_code = upgrade_code.upper()

		version = self.resolve_game_version()
		assert re.match(r'^\d+(\.\d+){1,3}$', version), 'Wix pack requires a numeric Common.GameVersion (x.y.z[.w]), got: ' + version

		icon_path = self.resolve_optional_config_relative_path(self.fomain.mainSection().getStr('Packaging.AppIcon', ''))
		if icon_path:
			assert os.path.isfile(icon_path), 'Packaging.AppIcon file not found: ' + icon_path

		arches = self.iter_arches()
		assert len(arches) == 1, 'Wix pack requires exactly one Windows architecture per package entry'
		input_arch = buildtools.resolve_windows_binary_arch(arches[0])
		msi_arch = 32 if input_arch == 'win32' else 64
		binary_output_postfix = self.args.binary_output_postfix
		name_base = self.args.nicename + ('_' + binary_output_postfix if binary_output_postfix else '')

		exe_name = name_base + '.exe'
		command_value = '"[INSTALLDIR]%s" --DeepLinkUri "%%1"' % exe_name
		registry_entries = [
			{'root': 'HKCU', 'key': 'Software\\Classes\\%s' % scheme, 'action': 'createAndRemoveOnUninstall',
			 'name': '', 'type': 'string', 'value': 'URL:%s Protocol' % scheme, 'key_path': 'yes'},
			{'root': 'HKCU', 'key': 'Software\\Classes\\%s' % scheme, 'action': 'createAndRemoveOnUninstall',
			 'name': 'URL Protocol', 'type': 'string', 'value': '', 'key_path': 'no'},
			{'root': 'HKCU', 'key': 'Software\\Classes\\%s\\shell\\open\\command' % scheme, 'action': 'createAndRemoveOnUninstall',
			 'name': '', 'type': 'string', 'value': command_value, 'key_path': 'no'},
		]
		install_location_registry = {
			'root': 'HKCU',
			'key': 'Software\\' + self.args.nicename,
			'name': 'InstallLocation',
			'win64': 'yes' if msi_arch == 64 else 'no',
		}
		registry_entries.append({
			'root': install_location_registry['root'],
			'key': install_location_registry['key'],
			'action': 'createAndRemoveOnUninstall',
			'name': install_location_registry['name'],
			'type': 'string',
			'value': '[INSTALLDIR]',
			'key_path': 'no',
		})

		staged_dir = os.path.basename(self.target_output_path)
		work_dir = os.path.dirname(self.target_output_path)
		config: dict[str, object] = {
			'product_name': game_name,
			'manufacturer': game_name,
			'name': game_name,
			'name_base': name_base,
			'version': version,
			'comments': game_name + ' game client',
			# Named after the project rather than the game: the client resolves its writable root by the
			# project name, so a default install is that same directory instead of a neighbour of it
			'installdir': self.args.nicename,
			'license_file': '',
			'upgrade_guid': upgrade_code,
			'major_upgrade': {'AllowSameVersionUpgrades': 'yes', 'DowngradeErrorMessage': 'A newer version is already installed.'},
			'arch': msi_arch,
			'registry_entries': registry_entries,
			'install_location_registry': install_location_registry,
			'startmenu_shortcut': exe_name,
			'desktop_shortcut': exe_name,
			'parts': [{'id': 'MainProgram', 'title': game_name, 'description': 'Game client', 'staged_dir': staged_dir}],
		}
		if icon_path:
			config['addremove_icon'] = icon_path

		config_path = os.path.join(work_dir, name_base + '.wix.json')
		with open(config_path, 'w', encoding='utf-8') as config_file:
			json.dump(config, config_file)

		# Drop the installed-build marker into the staged payload so the MSI-installed client uses
		# the per-user writable data dir (cache/logs/self-update overlay) instead of the read-only
		# install dir. Added only for the MSI and removed afterwards, so the sibling Raw/Zip
		# portable artifacts (already finalized earlier in finalize_output) stay portable
		marker_path = os.path.join(self.target_output_path, 'INSTALLED')
		try:
			with open(marker_path, 'w', encoding='utf-8') as marker_file:
				marker_file.write('installed\n')

			createmsi = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'msicreator', 'createmsi.py')
			log('Wix: building MSI installer', config_path)
			# createmsi.py requires a bare json filename (no path segment) and resolves it plus the staged
			# payload relative to its working directory, so invoke it with the basename and cwd=work_dir
			command = [sys.executable, createmsi]
			if wix_root:
				command.extend(['--wix-dir', wix_root])
			command.append(os.path.basename(config_path))
			subprocess.run(command, cwd=work_dir, check=True)
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
				if self.args.target == 'Client':
					self.package_client_managed_runtime_resources()

			self.select_platform_packager()()

			self.finalize_output()
			log('Complete!')
		except Exception:
			self.cleanup_output()
			raise


def main() -> None:
	if len(sys.argv) > 1 and sys.argv[1] == 'include':
		args = parse_include_args(sys.argv[2:])
		fomain = foconfig.ConfigParser()
		fomain.loadFromFile(args.maincfg)
		compress_level = fomain.mainSection().getInt('Baking.ZipCompressLevel')
		include_package_files(args.input, args.source, args.output, args.target, args.singlezip, compress_level)
		return

	args = parse_args()
	fomain = foconfig.ConfigParser()
	fomain.loadFromFile(args.maincfg)
	Packager(args, fomain).run()


if __name__ == '__main__':
	main()
