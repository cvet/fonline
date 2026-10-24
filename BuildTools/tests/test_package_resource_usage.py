from __future__ import annotations

import json
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package  # noqa: E402


def usage_fixture(tmp_path: Path) -> package.Packager:
    (tmp_path / "Art").mkdir()
    (tmp_path / "Art/Used.png").write_bytes(b"used")
    (tmp_path / "Art/Unused.png").write_bytes(b"unused")
    (tmp_path / "Resources.build-hash").write_text("fixture", encoding="utf-8")
    (tmp_path / "Resources.baking-config-hash").write_text("configuration", encoding="utf-8")
    (tmp_path / "Resources.usage.json").write_text(json.dumps({
        "Version": 1,
        "BuildHash": "fixture",
        "BakingConfigHash": "configuration",
        "Packs": {"Art": ["Used.png"]},
    }), encoding="utf-8")
    packager = package.Packager.__new__(package.Packager)
    packager.baking_path = str(tmp_path)
    packager.args = SimpleNamespace(buildhash="fixture")
    return packager


def test_collect_resources_excludes_unreferenced_baked_files(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)

    assert packager.collect_resource_files("Art", "Client") == [str(tmp_path / "Art/Used.png")]


def test_collect_resources_rejects_missing_usage_manifest(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)
    (tmp_path / "Resources.usage.json").unlink()

    with pytest.raises(AssertionError, match="usage"):
        packager.collect_resource_files("Art", "Client")


def test_usage_rejects_pack_directory_outside_baking_tree(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)
    outside = tmp_path.parent / (tmp_path.name + "-external")
    outside.mkdir()
    (outside / "Used.png").write_bytes(b"external")
    (tmp_path / "Art/Used.png").unlink()
    (tmp_path / "Art/Unused.png").unlink()
    (tmp_path / "Art").rmdir()
    try:
        (tmp_path / "Art").symlink_to(outside, target_is_directory=True)
    except OSError:
        pytest.skip("Directory symlink creation is unavailable")

    with pytest.raises(AssertionError, match="usage"):
        packager.collect_resource_files("Art", "Client")


def test_usage_includes_explicit_hidden_filename(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)
    (tmp_path / "Art/.used.png").write_bytes(b"used")
    manifest_path = tmp_path / "Resources.usage.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["Packs"]["Art"] = [".used.png"]
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    assert packager.collect_resource_files("Art", "Client") == [str(tmp_path / "Art/.used.png")]


def test_cached_usage_is_invalidated_when_bake_manifest_disappears(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)
    assert packager.collect_resource_files("Art", "Client")
    (tmp_path / "Resources.usage.json").unlink()

    with pytest.raises(AssertionError, match="usage"):
        packager.collect_resource_files("Art", "Client")


@pytest.mark.parametrize("update", [
    {"Version": True}, {"Version": 2}, {"Packs": []}, {"BuildHash": "other"},
    {"BakingConfigHash": "other"}, {"Packs": {"Art": ["../escape.png"]}},
    {"Packs": {"Art": ["/absolute.png"]}}, {"Packs": {"Art": ["C:/drive.png"]}},
    {"Packs": {"Art": ["Sub\\Used.png"]}}, {"Packs": {"Art": ["./Used.png"]}},
    {"Packs": {"Art": ["Used.png", "Used.png"]}}, {"Packs": {"Art": ["missing.png"]}},
    {"Packs": {"Art": ["used.png"]}}, {"Packs": {"../Art": ["Used.png"]}},
])
def test_usage_rejects_invalid_manifest_data(tmp_path: Path, update: dict) -> None:
    packager = usage_fixture(tmp_path)
    path = tmp_path / "Resources.usage.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    data.update(update)
    path.write_text(json.dumps(data), encoding="utf-8")

    with pytest.raises(AssertionError, match="usage"):
        packager.collect_resource_files("Art", "Client")


@pytest.mark.parametrize("text", ['{', '[]', '{"Version": 1, "Version": 1}'])
def test_usage_rejects_malformed_and_duplicate_json(tmp_path: Path, text: str) -> None:
    packager = usage_fixture(tmp_path)
    (tmp_path / "Resources.usage.json").write_text(text, encoding="utf-8")

    with pytest.raises(AssertionError, match="usage"):
        packager.collect_resource_files("Art", "Client")


def test_usage_keeps_pack_identity_unicode_names_and_target_filtering(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)
    (tmp_path / "Other").mkdir()
    for base in ("Art", "Other"):
        folder = tmp_path / base / "Кадры"
        folder.mkdir()
        (folder / "Кадр 1.png").write_bytes(base.encode())
    (tmp_path / "Art/Data.fopro-bin-server").write_bytes(b"server")
    (tmp_path / "Art/Data.fopro-bin-client").write_bytes(b"client")
    path = tmp_path / "Resources.usage.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    data["Packs"] = {"Art": ["Data.fopro-bin-client", "Data.fopro-bin-server", "Кадры/Кадр 1.png"],
                     "Other": ["Кадры/Кадр 1.png"]}
    path.write_text(json.dumps(data), encoding="utf-8")

    assert packager.collect_resource_files("Art", "Client") == [
        str(tmp_path / "Art/Data.fopro-bin-client"), str(tmp_path / "Art/Кадры/Кадр 1.png")]
    assert packager.collect_resource_files("Art", "Server") == [
        str(tmp_path / "Art/Data.fopro-bin-server"), str(tmp_path / "Art/Кадры/Кадр 1.png")]
    assert Path(packager.collect_resource_files("Other", "Client")[0]).read_bytes() == b"Other"


def test_usage_supports_empty_pack_and_real_empty_archive(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)
    path = tmp_path / "Resources.usage.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    data["Packs"]["Art"] = []
    path.write_text(json.dumps(data), encoding="utf-8")

    assert packager.collect_resource_files("Art", "Client") == []
    archive = tmp_path / "Empty.fores"
    package.write_resource_pack(archive, [], 6, 5)
    package.validate_resource_pack(archive, [])


def test_usage_cache_reloads_selection_after_new_manifest(tmp_path: Path) -> None:
    packager = usage_fixture(tmp_path)
    assert packager.collect_resource_files("Art", "Client") == [str(tmp_path / "Art/Used.png")]
    path = tmp_path / "Resources.usage.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    data["Packs"]["Art"] = ["Unused.png"]
    path.write_text(json.dumps(data), encoding="utf-8")

    assert packager.collect_resource_files("Art", "Client") == [str(tmp_path / "Art/Unused.png")]
