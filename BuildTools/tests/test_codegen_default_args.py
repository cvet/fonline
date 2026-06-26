from __future__ import annotations

import sys
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import codegen as _codegen  # noqa: E402


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

    target, entity, name, ret, args, ret_nullable, ret_wrapper, receiver_wrapper = _codegen.parse_export_method_signature(
        "FO_SCRIPT_API string Client_Game_FormatTags(ClientEngine* client, string_view text, CritterView* talker = nullptr)",
        {"void", "bool", "int32", "string", "Game", "Critter"},
        ["Game", "Critter"],
    )

    assert (target, entity, name, ret, ret_nullable) == ("Client", "Game", "FormatTags", "string", False)
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
