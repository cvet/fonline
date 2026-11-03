from __future__ import annotations

import hashlib
import sys
from argparse import Namespace
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import codegen as _codegen  # noqa: E402


@pytest.fixture(autouse=True)
def clean_codegen_state():
    _codegen.reset_metadata_parse_state()
    _codegen.errors.clear()
    yield
    _codegen.reset_metadata_parse_state()
    _codegen.errors.clear()


def _parse_map_position(monkeypatch: pytest.MonkeyPatch, enginedefine: list[str]) -> _codegen.ExportValueTypeTag:
    monkeypatch.setattr(_codegen, "args", Namespace(enginedefine=enginedefine))
    _codegen.tag_metas["ExportValueType"] = [
        _codegen.TagMetaRecord("Geometry.h", 0, "Layout = mcoord-x+mcoord-y", "struct mpos : ipos<mcoord_t>", ["x: X", "y: Y"])
    ]
    _codegen.parse_export_value_type_tags(set())

    assert not _codegen.errors
    [tag] = _codegen.codegen_tags["ExportValueType"]
    return tag


def _registration_lines() -> list[str]:
    helper_lines: list[str] = []
    register_lines: list[str] = []
    _codegen.append_value_type_registration(helper_lines, register_lines)
    return helper_lines


@pytest.mark.parametrize(("wide", "field_type"), [("0", "int16"), ("1", "int32")])
def test_map_coordinate_layout_follows_wide_coords_define(monkeypatch: pytest.MonkeyPatch, wide: str, field_type: str) -> None:
    tag = _parse_map_position(monkeypatch, [f"FO_MAP_WIDE_COORDS={wide}"])

    assert _codegen.get_value_type_layout(tag) == [("mcoord", "x"), ("mcoord", "y")]
    assert _codegen.get_resolved_value_type_layout(tag) == [(field_type, "x"), (field_type, "y")]
    registration = "\n".join(_registration_lines())
    assert f'{{string_view("x"), string_view("{field_type}")}},' in registration
    assert "mcoord" not in registration


def test_map_coordinate_width_changes_compatibility_version(monkeypatch: pytest.MonkeyPatch) -> None:
    digests: list[str] = []

    for wide in ("0", "1"):
        _codegen.reset_metadata_parse_state()
        monkeypatch.setattr(_codegen, "compatibility_hasher", hashlib.new("sha256"))
        _parse_map_position(monkeypatch, [f"FO_MAP_WIDE_COORDS={wide}"])
        _codegen.hash_resolved_value_type_layouts()
        digests.append(_codegen.compatibility_hasher.hexdigest())

    assert not _codegen.errors
    assert digests[0] != digests[1]


@pytest.mark.parametrize("enginedefine", [[], ["FO_MAP_WIDE_COORDS=2"], ["FO_MAP_WIDE_COORDS"]])
def test_map_coordinate_layout_requires_wide_coords_define(monkeypatch: pytest.MonkeyPatch, enginedefine: list[str]) -> None:
    tag = _parse_map_position(monkeypatch, enginedefine)

    with pytest.raises(AssertionError, match="FO_MAP_WIDE_COORDS"):
        _codegen.get_resolved_value_type_layout(tag)


def test_plain_layout_field_types_need_no_define(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(_codegen, "args", Namespace(enginedefine=[]))
    _codegen.tag_metas["ExportValueType"] = [
        _codegen.TagMetaRecord("ExtendedTypes.h", 0, "Name = ipos Layout = int32-x+int32-y", "using ipos32 = ipos<int32_t>;", [])
    ]
    _codegen.parse_export_value_type_tags(set())

    assert not _codegen.errors
    [tag] = _codegen.codegen_tags["ExportValueType"]
    assert _codegen.get_resolved_value_type_layout(tag) == [("int32", "x"), ("int32", "y")]
