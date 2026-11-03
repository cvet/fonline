from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import codegen


@pytest.fixture(autouse=True)
def clean_codegen_state():
    codegen.reset_metadata_parse_state()
    yield
    codegen.reset_metadata_parse_state()


@pytest.mark.parametrize("name", [
    "CheckViewMapCritterVisibilityHook",
    "CheckViewMapItemVisibilityHook",
])
def test_view_map_visibility_hook_is_a_supported_extension(name):
    codegen.tag_metas["EngineHook"] = [
        codegen.TagMetaRecord("Extension.cpp", 0, "", f"FO_SCRIPT_API bool {name}(ptr<const Map> map);", [])
    ]
    codegen.parse_engine_hook_tags()

    assert not codegen.errors
    assert [hook.name for hook in codegen.codegen_tags["EngineHook"]] == [name]
    assert name in codegen.ENGINE_HOOK_NAMES


def test_unknown_visibility_hook_is_rejected():
    codegen.tag_metas["EngineHook"] = [
        codegen.TagMetaRecord("Extension.cpp", 0, "", "FO_SCRIPT_API bool UnknownVisibilityHook();", [])
    ]
    codegen.parse_engine_hook_tags()

    assert codegen.errors
    assert not codegen.codegen_tags["EngineHook"]
