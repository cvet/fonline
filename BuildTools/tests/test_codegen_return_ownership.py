from __future__ import annotations

import sys
import re
import shutil
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import codegen  # noqa: E402
import docs_api  # noqa: E402


@pytest.fixture
def entity_types(monkeypatch: pytest.MonkeyPatch) -> set[str]:
    monkeypatch.setattr(codegen, "game_entities_info", {
        "Critter": codegen.EntityInfo("Critter", "CritterView", False, False, False, False, False, True, []),
    })
    return {"void", "Game", "Critter", "CritterFindType"}


def parse_return(return_type: str, entity_types: set[str]) -> tuple:
    return codegen.parse_export_method_signature(
        f"FO_SCRIPT_API {return_type} Server_Game_GetCritters(ptr<ServerEngine> server, CritterFindType findType)",
        entity_types,
        ["Game", "Critter"],
    )


def test_owning_entity_vector_preserves_script_surface(entity_types: set[str]) -> None:
    borrowed = parse_return("vector<ptr<Critter>>", entity_types)
    owned = parse_return("vector<refcount_ptr<Critter>>", entity_types)
    assert owned[:7] == borrowed[:7]
    assert owned[8:] == borrowed[8:]
    assert borrowed[7] == "ptr"
    assert owned[7] == "refcount_ptr"


@pytest.mark.parametrize("native_type", ["vector<Critter*>", "vector<ptr<Critter>>", "vector<refcount_ptr<Critter>>"])
def test_owning_return_keeps_exact_native_signature(native_type: str) -> None:
    assert codegen.apply_container_element_wrapper(native_type, "refcount_ptr") == "vector<refcount_ptr<Critter>>"


@pytest.mark.parametrize("return_type", ["refcount_ptr<Critter>", "readonly_vector<refcount_ptr<Critter>>", "vector<refcount_ptr<Critter>>&"])
def test_owning_return_is_limited_to_owned_vectors(return_type: str, entity_types: set[str]) -> None:
    with pytest.raises(AssertionError, match="Owning handles are supported only in returned entity vectors"):
        parse_return(return_type, entity_types)


@pytest.mark.parametrize("argument", ["readonly_vector<refcount_ptr<Critter>> values", "vector<refcount_ptr<Critter>>& values"])
def test_owning_handles_are_not_accepted_as_script_inputs(argument: str, entity_types: set[str]) -> None:
    with pytest.raises(AssertionError, match="Owning handles are supported only in returned entity vectors"):
        codegen.parse_method_args(argument, entity_types)


def test_owning_native_snapshot_preserves_actual_runtime_contract(tmp_path: Path) -> None:
    engine = Path(__file__).resolve().parents[2]
    fixture = tmp_path / "engine"
    for source in docs_api.collect_engine_metadata_inputs(engine):
        target = fixture / source.relative_to(engine)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)

    owned = docs_api.generate_api_model(fixture)
    owned_hash = codegen.compatibility_hasher.hexdigest()
    source = fixture / "Source/Scripting/ServerGlobalScriptMethods.cpp"
    text = source.read_text(encoding="utf-8")
    pattern = re.compile(r"FO_SCRIPT_API vector<refcount_ptr<Critter>> Server_Game_GetCritters\([^\n]+\)\n\{\n.*?^\}", re.M | re.S)
    match = pattern.search(text)
    assert match is not None
    borrowed_body = match[0].replace("vector<refcount_ptr<Critter>> Server_Game_GetCritters", "vector<ptr<Critter>> Server_Game_GetCritters")
    borrowed_body = borrowed_body.replace("return critters;", "return MakeScriptHandleVector<Critter>(critters);")
    borrowed_body = borrowed_body.replace("return result;", "return MakeScriptHandleVector<Critter>(result);")
    source.write_text(text[:match.start()] + borrowed_body + text[match.end():], encoding="utf-8")
    borrowed = docs_api.generate_api_model(fixture)
    assert borrowed == owned
    assert codegen.compatibility_hasher.hexdigest() == owned_hash

    wrong_body = borrowed_body.replace("CritterFindType findType)", "int32_t rawFindType)")
    wrong_body = wrong_body.replace("\n{\n", "\n{\n    CritterFindType findType = static_cast<CritterFindType>(rawFindType);\n", 1)
    assert wrong_body != borrowed_body
    source.write_text(text[:match.start()] + wrong_body + text[match.end():], encoding="utf-8")
    docs_api.generate_api_model(fixture)
    assert codegen.compatibility_hasher.hexdigest() != owned_hash
