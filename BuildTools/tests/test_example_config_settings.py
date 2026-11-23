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
