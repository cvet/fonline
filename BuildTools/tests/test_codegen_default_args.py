from __future__ import annotations

import sys
from argparse import Namespace
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import codegen as _codegen  # noqa: E402


def test_engine_config_is_emitted_as_one_macro_only_header(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    class CompatibilityHasher:
        @staticmethod
        def hexdigest() -> str:
            return "0123456789abcdef-extra"

    output = _codegen.GeneratedOutput()
    monkeypatch.setattr(_codegen, "args", Namespace(
        buildhash="build-hash",
        devname="DEV",
        enginedefine=["FO_GEOMETRY=1", "FO_EMPTY_DEFINE"],
        genoutput=str(tmp_path),
        nicename="Nice Name",
    ))
    monkeypatch.setattr(_codegen, "compatibility_hasher", CompatibilityHasher())
    monkeypatch.setattr(_codegen, "generated_output", output)
    monkeypatch.setattr(_codegen, "try_get_git_branch", lambda: "test-branch")

    _codegen.write_engine_config()

    generated_lines = output.files[str(tmp_path / "EngineConfig.gen.h")]
    assert generated_lines == [
        "// FOnline Engine generated configuration. Do not edit.",
        "",
        "#define FO_GEOMETRY 1",
        "#define FO_EMPTY_DEFINE",
        '#define FO_BUILD_HASH "build-hash"',
        '#define FO_DEV_NAME "DEV"',
        '#define FO_NICE_NAME "Nice Name"',
        f'#define FO_GENERATED_SOURCE_DIR "{tmp_path.as_posix()}"',
        '#define FO_COMPATIBILITY_VERSION "0123456789abcdef"',
        '#define FO_GIT_BRANCH "test-branch"',
    ]
    assert all(not line or line.startswith(("//", "#define ")) for line in generated_lines)


def test_split_engine_args_respects_nested_default_commas() -> None:
    args = _codegen.split_engine_args(
        'readonly_map<string, string> post, string_view label = "a,b", int32_t count = MakeCount(1, 2)'
    )

    assert args == [
        "readonly_map<string, string> post",
        'string_view label = "a,b"',
        "int32_t count = MakeCount(1, 2)",
    ]


def test_parse_method_args_preserves_trailing_defaults() -> None:
    args = _codegen.parse_method_args(
        'int32_t value, string_view label = "fresh, crisp", bool enabled = true',
        {"int32", "string", "bool"},
    )

    assert [(arg.arg_type, arg.name, arg.default_value) for arg in args] == [
        ("int32", "value", None),
        ("string", "label", '"fresh, crisp"'),
        ("bool", "enabled", "true"),
    ]


def test_parse_method_args_rejects_non_trailing_defaults() -> None:
    with pytest.raises(AssertionError, match="Default argument is followed by non-default argument"):
        _codegen.parse_method_args("int32_t value = 1, int32_t extra", {"int32"})


def test_parse_export_method_signature_normalizes_null_default(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        _codegen,
        "game_entities_info",
        {
            "Game": _codegen.EntityInfo("ServerEngine", "ClientEngine", True, False, False, False, False, True, []),
            "Critter": _codegen.EntityInfo("Critter", "CritterView", False, False, False, False, False, True, []),
        },
    )

    target, entity, name, ret, args, ret_nullable, ret_wrapper, ret_container_element_wrapper, receiver_wrapper = _codegen.parse_export_method_signature(
        "FO_SCRIPT_API string Client_Game_FormatTags(nptr<ClientEngine> client, string_view text, nptr<CritterView> talker = nullptr)",
        {"void", "bool", "int32", "string", "Game", "Critter"},
        ["Game", "Critter"],
    )

    assert (target, entity, name, ret, ret_nullable) == ("Client", "Game", "FormatTags", "string", False)
    assert ret_container_element_wrapper == ""
    assert receiver_wrapper
    assert [(arg.arg_type, arg.name, arg.nullable, arg.default_value) for arg in args] == [
        ("string", "text", False, None),
        ("Critter", "talker", True, "null"),
    ]


def test_parse_method_args_normalizes_value_type_defaults(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        _codegen,
        "custom_types_native_map",
        {
            "any_t": "any",
            "fpos32": "fpos",
            "fsize32": "fsize",
            "ucolor": "ucolor",
        },
    )

    args = _codegen.parse_method_args(
        "fpos32 uv0 = fpos32 {0.0f, 0.0f}, fpos32 uv1 = fpos32 {1.0f, 1.0f}, ucolor color = ucolor {}, any_t payload = any_t {}",
        {"fpos", "fsize", "ucolor", "any"},
    )

    assert [(arg.arg_type, arg.name, arg.default_value) for arg in args] == [
        ("fpos", "uv0", "fpos(0.0f, 0.0f)"),
        ("fpos", "uv1", "fpos(1.0f, 1.0f)"),
        ("ucolor", "color", "ucolor()"),
        ("any", "payload", "any()"),
    ]
