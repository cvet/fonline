"""Validate the resource selection produced by one successful bake."""

from __future__ import annotations

import json
import os
from typing import Any

USAGE_MANIFEST_NAME = 'Resources.usage.json'
BUILD_HASH_MARKER_NAME = 'Resources.build-hash'
CONFIG_HASH_MARKER_NAME = 'Resources.baking-config-hash'
MANIFEST_VERSION = 1
MANIFEST_KEYS = frozenset({'Version', 'BuildHash', 'BakingConfigHash', 'Packs', 'Reasons'})


def load_usage_allowlists(baking_path: str, expected_build_hash: str) -> dict[str, frozenset[str]]:
    """Load and validate the usage manifest; returns pack name -> used pack-relative paths."""
    manifest_path = os.path.join(baking_path, USAGE_MANIFEST_NAME)
    assert os.path.isfile(manifest_path), 'Resource usage manifest not found: ' + manifest_path
    try:
        with open(manifest_path, 'r', encoding='utf-8-sig') as manifest_file:
            manifest_text = manifest_file.read()
    except (OSError, UnicodeError) as error:
        raise AssertionError('Resource usage manifest is unreadable: ' + manifest_path) from error
    try:
        manifest: Any = json.loads(manifest_text, object_pairs_hook=reject_duplicate_json_keys)
    except ValueError as error:
        raise AssertionError('Resource usage manifest is not valid JSON: ' + manifest_path + ': ' + str(error)) from error

    assert isinstance(manifest, dict), 'Resource usage manifest root must be an object: ' + manifest_path
    unknown_keys = sorted(set(manifest) - MANIFEST_KEYS)
    assert not unknown_keys, 'Resource usage manifest has unknown fields: ' + ', '.join(unknown_keys)

    version = manifest.get('Version')
    assert isinstance(version, int) and not isinstance(version, bool) and version == MANIFEST_VERSION, (
        'Resource usage manifest version is unsupported: ' + repr(version))

    build_hash = manifest.get('BuildHash')
    assert isinstance(build_hash, str) and build_hash, 'Resource usage manifest BuildHash must be a nonempty string'
    assert build_hash == expected_build_hash, 'Resource usage manifest BuildHash does not match the packaging build hash'
    assert build_hash == read_usage_marker(baking_path, BUILD_HASH_MARKER_NAME), (
        'Resource usage manifest BuildHash does not match the baked build-hash marker')

    config_hash = manifest.get('BakingConfigHash')
    assert isinstance(config_hash, str) and config_hash, 'Resource usage manifest BakingConfigHash must be a nonempty string'
    assert config_hash == read_usage_marker(baking_path, CONFIG_HASH_MARKER_NAME), (
        'Resource usage manifest BakingConfigHash does not match the baked baking-config-hash marker')

    packs = manifest.get('Packs')
    assert isinstance(packs, dict), 'Resource usage manifest Packs must be an object'
    allowlists: dict[str, frozenset[str]] = {}
    directory_names: dict[str, set[str]] = {}

    for pack_name, pack_paths in packs.items():
        assert isinstance(pack_name, str) and pack_name, 'Resource usage manifest pack name must be a nonempty string'
        validate_canonical_relative(pack_name, 'pack name ' + pack_name)
        assert isinstance(pack_paths, list), 'Resource usage manifest pack file list must be an array: ' + pack_name
        used_paths: set[str] = set()

        for relative_path in pack_paths:
            assert isinstance(relative_path, str) and relative_path, (
                'Resource usage manifest file path must be a nonempty string in pack ' + pack_name)
            validate_canonical_relative(relative_path, 'file path ' + relative_path + ' in pack ' + pack_name)
            assert relative_path not in used_paths, (
                'Resource usage manifest lists the same file twice: ' + pack_name + '/' + relative_path)
            used_paths.add(relative_path)

        validate_pack_files_exist(baking_path, pack_name, used_paths, directory_names)
        allowlists[pack_name] = frozenset(used_paths)

    return allowlists


def reject_duplicate_json_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    unique: dict[str, Any] = {}
    for key, value in pairs:
        assert key not in unique, 'Resource usage manifest holds duplicate key: ' + key
        unique[key] = value
    return unique


def read_usage_marker(baking_path: str, marker_name: str) -> str:
    marker_path = os.path.join(baking_path, marker_name)
    assert os.path.isfile(marker_path), 'Resource usage marker not found: ' + marker_path
    with open(marker_path, 'r', encoding='utf-8-sig') as marker_file:
        value = marker_file.read().strip()
    assert value, 'Resource usage marker is empty: ' + marker_path
    return value


def validate_canonical_relative(path: str, description: str) -> None:
    assert '\0' not in path, 'Resource usage manifest ' + description + ' contains a NUL character'
    assert '\\' not in path, 'Resource usage manifest ' + description + ' contains a backslash'
    assert ':' not in path, 'Resource usage manifest ' + description + ' contains a drive separator'
    assert not path.startswith('/'), 'Resource usage manifest ' + description + ' is absolute'
    assert all(part not in ('', '.', '..') for part in path.split('/')), (
        'Resource usage manifest ' + description + ' is not a canonical relative path')


def resolve_exact_path(base: str, relative_path: str, directory_names: dict[str, set[str]]) -> str:
    current = base
    for part in relative_path.split('/'):
        try:
            if current not in directory_names:
                directory_names[current] = set(os.listdir(current))
            names = directory_names[current]
        except OSError as error:
            raise AssertionError('Resource usage directory is unreadable: ' + current) from error
        assert part in names, 'Resource usage path was not baked (case-sensitive): ' + relative_path
        current = os.path.join(current, part)
    return current


def validate_pack_files_exist(baking_path: str, pack_name: str, used_paths: set[str], directory_names: dict[str, set[str]]) -> None:
    pack_root = resolve_exact_path(baking_path, pack_name, directory_names)
    assert os.path.isdir(pack_root), 'Resource usage manifest pack is not a directory: ' + pack_name
    bake_real = os.path.normcase(os.path.realpath(baking_path))
    pack_real = os.path.normcase(os.path.realpath(pack_root))
    assert os.path.commonpath((bake_real, pack_real)) == bake_real, (
        'Resource usage pack is outside the baking tree: ' + pack_name)

    for relative_path in sorted(used_paths):
        file_path = resolve_exact_path(pack_root, relative_path, directory_names)
        assert os.path.isfile(file_path), 'Resource usage output is not a file: ' + pack_name + '/' + relative_path
        file_real = os.path.normcase(os.path.realpath(file_path))
        assert os.path.commonpath((pack_real, file_real)) == pack_real, (
            'Resource usage output is outside its pack: ' + pack_name + '/' + relative_path)


def usage_cache_key(baking_path: str, expected_build_hash: str) -> tuple:
    states = []
    for name in (USAGE_MANIFEST_NAME, BUILD_HASH_MARKER_NAME, CONFIG_HASH_MARKER_NAME):
        try:
            info = os.stat(os.path.join(baking_path, name))
        except OSError as error:
            raise AssertionError('Resource usage input is unavailable: ' + name) from error
        states.append((info.st_ino, info.st_mtime_ns, info.st_size))
    return (os.path.normcase(os.path.realpath(baking_path)), expected_build_hash, *states)
