from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILDTOOLS_DIR = ENGINE_ROOT / "BuildTools"
sys.path.insert(0, str(BUILDTOOLS_DIR))

import foconfig  # noqa: E402
import package as package_tool  # noqa: E402


def _config(lines: list[str]) -> foconfig.ConfigParser:
    config = foconfig.ConfigParser()
    config.loadFromLines(lines)
    return config


class PackageSecurityTests(unittest.TestCase):
    def test_packaging_uses_the_baked_target_config_when_loaded(self) -> None:
        packager = package_tool.Packager.__new__(package_tool.Packager)
        packager.fomain = _config(["Android.PackageName = root.package"])
        packager.target_config = _config(["Android.PackageName = baked.package"])

        self.assertEqual(
            packager.get_effective_config_section().getStr("Android.PackageName"),
            "baked.package",
        )

    def test_android_signing_fields_are_read_directly_from_baked_config(self) -> None:
        source = (BUILDTOOLS_DIR / "package.py").read_text(encoding="utf-8")
        for marker in (
            "android_config = self.get_effective_config_section()",
            "android_config.getStr('Android.Keystore', '')",
            "android_config.getStr('Android.KeystorePassword', '')",
            "android_config.getStr('Android.KeyAlias', '')",
            "android_config.getStr('Android.KeyPassword', '')",
            "Android release signing requires Android.Keystore",
        ):
            self.assertIn(marker, source)
        self.assertNotIn("resolve_build_host_config_value", source)

    def test_windows_signing_hook_is_a_non_secret_project_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            hook = root / "sign-hook"
            hook.write_text("fixture", encoding="utf-8")
            binary = root / "Fixture.dll"
            binary.write_bytes(b"fixture")

            config = _config(["Packaging.CodeSigningHook = sign-hook"])
            packager = package_tool.Packager.__new__(package_tool.Packager)
            packager.fomain = config
            packager.args = SimpleNamespace(platform="Windows", maincfg=str(root / "Fixture.fomain"))
            packager.target_output_path = str(root)

            with mock.patch.object(package_tool.subprocess, "call", return_value=0) as call:
                packager.sign_windows_binaries()

            call.assert_called_once_with([str(hook), str(binary)])


if __name__ == "__main__":
    unittest.main()
