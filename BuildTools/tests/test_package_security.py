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
        packager.args = SimpleNamespace(platform="Windows", target="Client")

        self.assertEqual(
            packager.get_effective_config_section().getStr("Android.PackageName"),
            "baked.package",
        )

    def test_android_client_metadata_comes_from_matching_server_bake_without_embedding_it(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            baking = Path(temp_dir)
            configs = baking / "Configs"
            configs.mkdir()
            client_data = b"Client.Multithreading=1\nNetwork.ServerPort=31825\n"
            (configs / "Parallel.fomain-client").write_bytes(client_data)
            (configs / "Parallel.fomain-server").write_text(
                "Android.PackageName=com.fonline.parallel\nAndroid.CompileSdk=35\nServerOnlySentinel=keep-out\n",
                encoding="utf-8",
            )
            (configs / "Serial.fomain-server").write_text("Android.PackageName=com.fonline.serial\n", encoding="utf-8")
            packager = package_tool.Packager.__new__(package_tool.Packager)
            packager.fomain = _config(["Android.PackageName = root.package"])
            packager.args = SimpleNamespace(platform="Android", target="Client", config="Parallel")
            packager.baking_path = str(baking)
            packager.embedded_data = b""
            packager.load_config_data()

            metadata = packager.get_effective_config_section()

            self.assertEqual(metadata.getStr("Android.PackageName", "com.fonline.app"), "com.fonline.parallel")
            self.assertEqual(metadata.getInt("Android.CompileSdk"), 35)
            self.assertEqual(packager.config_data, client_data)
            self.assertEqual(packager.target_config.mainSection().getStr("Android.PackageName", ""), "")
            self.assertNotIn(b"ServerOnlySentinel", packager.config_data)

    def test_android_client_missing_matching_server_bake_fails_before_packaging(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            configs = Path(temp_dir) / "Configs"
            configs.mkdir()
            (configs / "Parallel.fomain-client").write_text("Network.ServerPort=31825\n", encoding="utf-8")
            (configs / "Serial.fomain-server").write_text("Android.PackageName=com.fonline.serial\n", encoding="utf-8")
            packager = package_tool.Packager.__new__(package_tool.Packager)
            packager.fomain = _config(["Android.PackageName = root.package"])
            packager.args = SimpleNamespace(platform="Android", target="Client", config="Parallel")
            packager.baking_path = str(configs.parent)
            packager.embedded_data = b""
            packager.load_config_data()

            with self.assertRaisesRegex(AssertionError, "Config file not found"):
                packager.get_effective_config_section()

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
