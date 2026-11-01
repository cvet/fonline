from __future__ import annotations

import importlib.util
from pathlib import Path
import sys

import pytest


ENGINE = Path(__file__).resolve().parents[2]
EXAMPLES = ("ContentShowcase", "MinimalMultiplayer", "PackagingMatrix")
EXPECTED_RUNTIME = {
    "Common.UnpackagedSubConfig", "Common.UserWritablePath", "Network.CompatibilityVersion",
    "Common.Packaged", "Render.RenderDebug", "View.MonitorWidth", "View.MonitorHeight",
}


def load_generator(example: str):
    name = "example_runtime_settings_" + example
    spec = importlib.util.spec_from_file_location(name, ENGINE / "Examples" / example / "generate_config.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize("example", EXAMPLES)
def test_runtime_settings_follow_actual_native_constant_names(example: str) -> None:
    generator = load_generator(example)
    assert generator.parse_runtime_managed_settings(ENGINE / "Source/Common/Settings.cpp") == EXPECTED_RUNTIME


@pytest.mark.parametrize("example", EXAMPLES)
def test_runtime_settings_ignore_comments_and_literals(example: str, tmp_path: Path) -> None:
    fixture = tmp_path / "Settings.cpp"
    fixture.write_text(
        '// _appliedSettings.emplace(SettingNames::Fake_Line);\n'
        '/*\n_appliedSettings.emplace(SettingNames::Fake_Block);\n*/\n'
        '// continued comment \\\n_appliedSettings.emplace(SettingNames::Fake_Continued);\n'
        'const char* raw = R"tag(\n_appliedSettings.emplace(SettingNames::Fake_Raw);\n)tag";\n'
        'const char* normal = "_appliedSettings.emplace(SettingNames::Fake_String);";\n'
        '_appliedSettings.emplace(SettingNames::Common_UserWritablePath);\n'
        '_appliedSettings.emplace(SettingNames::View_MonitorWidth);\n',
        encoding="utf-8", newline="\n",
    )
    assert load_generator(example).parse_runtime_managed_settings(fixture) == {
        "Common.UserWritablePath", "View.MonitorWidth",
    }


@pytest.mark.parametrize("example", EXAMPLES)
def test_enum_defaults_resolve_to_numeric_values(example: str) -> None:
    generator = load_generator(example)
    constants = generator.parse_enum_constants(ENGINE / "Source")
    assert generator.resolve_enum_constant("CritterStateAnim::None", constants) == "0"
    assert generator.resolve_enum_constant("CritterActionAnim::None", constants) == "0"
    assert "CritterActionAnim::None" not in generator.render_config()


@pytest.mark.parametrize("example", EXAMPLES)
def test_enum_constants_follow_declaration_rules(example: str, tmp_path: Path) -> None:
    (tmp_path / "Enums.h").write_text(
        '// enum class Fake : int { Hidden = 7 };\n'
        'enum class Sample : uint16_t\n{\n'
        '    First, // First, value\n'
        '    Second = 0x10,\n'
        '    Third,\n'
        '    Computed = First | Second,\n'
        '    AfterComputed,\n'
        '};\n'
        'enum class Clash : int { Value = 1 };\n',
        encoding="utf-8", newline="\n",
    )
    (tmp_path / "Other.h").write_text('enum class Clash : int { Value = 2 };\n', encoding="utf-8", newline="\n")

    generator = load_generator(example)
    constants = generator.parse_enum_constants(tmp_path)
    assert constants == {"Sample::First": 0, "Sample::Second": 16, "Sample::Third": 17}
    assert generator.resolve_enum_constant("Sample::Third", constants) == "17"
    assert generator.resolve_enum_constant("Plain text", constants) == "Plain text"

    for unresolved in ("Sample::Computed", "Sample::AfterComputed", "Clash::Value", "Fake::Hidden"):
        with pytest.raises(ValueError, match="unresolved enum constant"):
            generator.resolve_enum_constant(unresolved, constants)
