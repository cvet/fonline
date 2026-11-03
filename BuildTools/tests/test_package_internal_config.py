from __future__ import annotations

from pathlib import Path
import json
import stat
import sys
from types import SimpleNamespace

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


CAPACITY = 400


BUILD_HASH = 'fixture-build'
BAKING_CONFIG_HASH = 'fixture-baking-config'


def write_usage_manifest(baking_path: Path, packs: dict[str, list[str]]) -> None:
    (baking_path / 'Resources.build-hash').write_text(BUILD_HASH, encoding='utf-8')
    (baking_path / 'Resources.baking-config-hash').write_text(BAKING_CONFIG_HASH, encoding='utf-8')
    (baking_path / 'Resources.usage.json').write_text(json.dumps({
        'Version': 1,
        'BuildHash': BUILD_HASH,
        'BakingConfigHash': BAKING_CONFIG_HASH,
        'Packs': packs,
    }), encoding='utf-8')


def metadata_fixture(tmp_path: Path) -> tuple[Path, list[str]]:
    configs = tmp_path / 'Metadata'
    configs.mkdir()
    for role in ('client', 'server', 'mapper'):
        (configs / ('Metadata.fometa-' + role)).write_bytes(role.encode())
    (configs / 'shared.json').write_bytes(b'metadata-sidecar')
    for name in ('(Root)', 'Production', 'Development'):
        (configs / (name + '.fomain-client')).write_text('Common.GameName=' + name + '\n', encoding='utf-8', newline='\n')
        (configs / (name + '.fomain-server')).write_text('Server.DbStorage=JSON\nCommon.GameName=' + name + '\n', encoding='utf-8', newline='\n')
    listed = sorted(path.name for path in configs.iterdir())
    write_usage_manifest(tmp_path, {'Metadata': listed})
    return configs, listed


@pytest.mark.parametrize('target', ['Client', 'Server'])
@pytest.mark.parametrize('selected', ['', 'Production', 'Development'])
def test_resources_use_the_same_subconfig_as_the_embedded_config(tmp_path: Path, target: str, selected: str) -> None:
    configs, listed = metadata_fixture(tmp_path)
    packager = _package.Packager.__new__(_package.Packager)
    packager.baking_path = str(tmp_path)
    packager.args = SimpleNamespace(config=selected, buildhash=BUILD_HASH)

    config_name, embedded = packager.read_config_data(target)
    resources = packager.collect_resource_files('Metadata', target)

    expected_names = sorted((config_name, 'Metadata.fometa-' + target.lower(), 'shared.json'))
    assert [Path(path).name for path in resources] == expected_names
    assert resources == sorted(str(configs / name) for name in expected_names)
    assert (configs / config_name).read_bytes() == embedded
    expected_name = selected or '(Root)'
    expected = ('Server.DbStorage=JSON\n' if target == 'Server' else '') + 'Common.GameName=' + expected_name + '\n'
    assert embedded == expected.encode()

    other_configs = sorted(name for name in listed if '.fomain-' in name and name != config_name)
    assert other_configs
    assert not set(other_configs) & set(expected_names)

    archive = tmp_path / 'Metadata.fores'
    _package.write_resource_pack(archive, [(name, configs / name) for name in expected_names], 6, 5)
    _package.validate_resource_pack(archive, expected_names)

    wrong_profiles = [name for name in other_configs if name.endswith('-' + target.lower())]
    assert wrong_profiles
    wrong_entries = sorted([*expected_names, wrong_profiles[0]])
    wrong_archive = tmp_path / 'Metadata.nonselected.fores'
    _package.write_resource_pack(wrong_archive, [(name, configs / name) for name in wrong_entries], 6, 5)
    with pytest.raises(AssertionError):
        _package.validate_resource_pack(wrong_archive, expected_names)


@pytest.mark.parametrize('target', ['Client', 'Server'])
def test_missing_selected_config_with_existing_metadata_fails(tmp_path: Path, target: str) -> None:
    configs, _ = metadata_fixture(tmp_path)
    selected = 'Production'
    (configs / (selected + '.fomain-' + target.lower())).unlink()
    packager = _package.Packager.__new__(_package.Packager)
    packager.baking_path = str(tmp_path)
    packager.args = SimpleNamespace(config=selected, buildhash=BUILD_HASH)

    # The selected profile's config is missing while the metadata payload is present;
    # packaging must fail instead of shipping a pack without its configuration
    with pytest.raises(AssertionError):
        packager.read_config_data(target)
    with pytest.raises(AssertionError, match='Resource usage'):
        packager.collect_resource_files('Metadata', target)


@pytest.mark.parametrize('target', ['Client', 'Server'])
def test_selected_config_missing_from_usage_manifest_is_rejected(tmp_path: Path, target: str) -> None:
    configs, listed = metadata_fixture(tmp_path)
    selected_name = 'Production.fomain-' + target.lower()
    packager = _package.Packager.__new__(_package.Packager)
    packager.baking_path = str(tmp_path)
    packager.args = SimpleNamespace(config='Production', buildhash=BUILD_HASH)
    assert (configs / selected_name).is_file()
    assert packager.read_config_data(target)[0] == selected_name

    manifest_path = tmp_path / 'Resources.usage.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    manifest['Packs']['Metadata'] = [name for name in listed if name != selected_name]
    manifest_path.write_text(json.dumps(manifest), encoding='utf-8')

    with pytest.raises(AssertionError, match='not listed in the resource usage manifest'):
        packager.collect_resource_files('Metadata', target)


def make_binary(path: Path) -> None:
    filler = b"0" * (CAPACITY - len(_package.INTERNAL_CONFIG_MARKER) - len(_package.INTERNAL_CONFIG_END_MARKER))
    path.write_bytes(b"head" + _package.INTERNAL_CONFIG_MARKER + filler + _package.INTERNAL_CONFIG_END_MARKER + b"tail")


def read_patched_config(path: Path) -> str:
    content = path.read_bytes()
    assert content.startswith(b"head") and content.endswith(b"tail")
    return content[len(b"head"):len(b"head") + CAPACITY].rstrip(b"#").decode("utf-8")


def test_unknown_pack_token_is_rejected_before_packaging(tmp_path: Path) -> None:
    assert "OGL" not in _package.load_package_pack_names(str(BUILDTOOLS_DIR))
    with pytest.raises(AssertionError, match="Unknown package pack token: OGL"):
        _package.Packager(SimpleNamespace(pack="OGL+Raw", output=str(tmp_path)), None)


def test_baked_config_is_written_unchanged(tmp_path: Path) -> None:
    binary = tmp_path / "LastFrontier.exe"
    make_binary(binary)
    packager = _package.Packager.__new__(_package.Packager)
    packager.config_data = b"Common.GameName=LastFrontier\n[ResourcePack]\nName=Art\nClientOnly=1\n"

    packager.patch_config(str(binary))

    assert read_patched_config(binary) == packager.config_data.decode("utf-8")


def test_all_reserved_fields_preserve_surrounding_native_binary_bytes(tmp_path: Path) -> None:
    binary = tmp_path / "server"
    prefix = b"native-prefix\x00" + b"p" * (1024 * 1024)
    separator = b"\x00native-separator\x00"
    suffix = b"native-suffix\x00" + b"s" * (1024 * 1024)
    config_filler = b"0" * (CAPACITY - len(_package.INTERNAL_CONFIG_MARKER) - len(_package.INTERNAL_CONFIG_END_MARKER))
    name_filler = b"\x00" * (_package.PACKAGED_BUILD_NAME_CAPACITY - len(_package.PACKAGED_BUILD_NAME_MARKER))
    binary.write_bytes(
        prefix + _package.make_embedded_marker(10000) + separator
        + _package.INTERNAL_CONFIG_MARKER + config_filler + _package.INTERNAL_CONFIG_END_MARKER
        + separator + _package.PACKAGED_BUILD_NAME_MARKER + name_filler + suffix
    )
    mode_before = stat.S_IMODE(binary.stat().st_mode)
    packager = _package.Packager.__new__(_package.Packager)
    packager.pack_args = {"Raw"}
    packager.embedded_data = b"embedded-archive-fixture"
    packager.config_data = b"Common.GameName=Fixture\n[ResourcePack]\nName=Art\n"

    packager.patch_packaged_binary(str(binary), "Fixture_Server")

    expected = (
        prefix + packager.embedded_data.ljust(10000, b"#") + separator
        + packager.config_data.ljust(CAPACITY, b"#") + separator
        + b"Fixture_Server".ljust(_package.PACKAGED_BUILD_NAME_CAPACITY, b"\x00") + suffix
    )
    assert binary.read_bytes() == expected
    assert stat.S_IMODE(binary.stat().st_mode) == mode_before


def test_only_the_first_matching_reserved_field_is_patched(tmp_path: Path) -> None:
    binary = tmp_path / "duplicate-marker"
    binary.write_bytes(b"prefixFIELDseparatorFIELDsuffix")

    _package.patch_data(binary, b"FIELD", b"xy", 5)

    assert binary.read_bytes() == b"prefixxy###separatorFIELDsuffix"


@pytest.mark.parametrize(
    "marker,payload,capacity",
    [(b"FIELD", b"x", 32), (b"MISSING", b"x", 5), (b"FIELD", b"too-long", 5)],
    ids=["truncated-reservation", "missing-marker", "oversized-payload"],
)
def test_invalid_reserved_field_does_not_mutate_the_binary(
    tmp_path: Path, marker: bytes, payload: bytes, capacity: int
) -> None:
    binary = tmp_path / "invalid-field"
    original = b"prefixFIELDsuffix"
    binary.write_bytes(original)

    with pytest.raises(AssertionError):
        _package.patch_data(binary, marker, payload, capacity)

    assert binary.read_bytes() == original
