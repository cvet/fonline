from __future__ import annotations

import os
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
    def test_effective_project_config_matches_parent_and_child_precedence(self) -> None:
        config = _config(
            [
                "Common.GameName = Root",
                "Android.PackageName = root.package",
                "[SubConfig]",
                "Name = SigningBase",
                "Android.PackageName = parent.package",
                "Android.KeyAlias = parent",
                "[SubConfig]",
                "Name = Release",
                "Parent = SigningBase",
                "Android.KeyAlias = release",
            ]
        )

        effective = package_tool.build_effective_project_config(config, "Release")

        self.assertEqual(effective.getStr("Common.GameName"), "Root")
        self.assertEqual(effective.getStr("Android.PackageName"), "parent.package")
        self.assertEqual(effective.getStr("Android.KeyAlias"), "release")
        self.assertNotIn("Name", effective.content)
        self.assertNotIn("Parent", effective.content)

    def test_effective_project_config_rejects_forward_parent(self) -> None:
        config = _config(
            [
                "Common.GameName = Root",
                "[SubConfig]",
                "Name = Release",
                "Parent = Missing",
            ]
        )

        with self.assertRaisesRegex(AssertionError, "must be declared earlier: Missing"):
            package_tool.build_effective_project_config(config, "Release")

    def test_build_host_directives_resolve_env_and_protected_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "credential.txt").write_text("  file-value\n", encoding="utf-8")
            environment = {"SIGNING_VALUE": "environment-value"}

            self.assertEqual(
                package_tool.resolve_build_host_config_value(
                    "$TARGET_ENV{SIGNING_VALUE}", "Android.KeyPassword", temp_dir, environment
                ),
                "environment-value",
            )
            self.assertEqual(
                package_tool.resolve_build_host_config_value(
                    "prefix-$FILE{credential.txt}", "Android.KeystorePassword", temp_dir, environment
                ),
                "prefix-file-value",
            )

    def test_missing_build_host_secret_fails_without_a_value(self) -> None:
        with self.assertRaisesRegex(AssertionError, "Android.KeyPassword: MISSING_SIGNING_VALUE") as raised:
            package_tool.resolve_build_host_config_value(
                "$ENV{MISSING_SIGNING_VALUE}", "Android.KeyPassword", ".", {}
            )

        self.assertNotIn("password-value", str(raised.exception))

    def test_android_package_config_uses_unbaked_project_signing_values(self) -> None:
        config = _config(
            [
                "Android.PackageName = com.example.fixture",
                "Android.KeystorePassword = $TARGET_ENV{FIXTURE_STORE_PASSWORD}",
                "Android.KeyAlias = release-key",
                "Android.KeyPassword = $TARGET_ENV{FIXTURE_KEY_PASSWORD}",
                "[SubConfig]",
                "Name = Release",
                "Android.PackageName = com.example.release",
            ]
        )
        packager = package_tool.Packager.__new__(package_tool.Packager)
        packager.fomain = config
        packager.project_config = package_tool.build_effective_project_config(config, "Release")
        packager.target_config = _config(["Common.GameName = Embedded client config"])
        packager.args = SimpleNamespace(maincfg=str(ENGINE_ROOT / "Fixture.fomain"))

        environment = {
            "FIXTURE_STORE_PASSWORD": "fixture-store-value",
            "FIXTURE_KEY_PASSWORD": "fixture-key-value",
        }
        with mock.patch.dict(os.environ, environment, clear=False):
            android = packager.get_android_package_config_section()

        self.assertEqual(android.getStr("Android.PackageName"), "com.example.release")
        self.assertEqual(android.getStr("Android.KeystorePassword"), "fixture-store-value")
        self.assertEqual(android.getStr("Android.KeyPassword"), "fixture-key-value")
        self.assertNotIn("Android.KeyPassword", packager.target_config.mainSection().content)

    def test_windows_signing_hook_resolves_on_packaging_host(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            hook = root / "sign-hook"
            hook.write_text("fixture", encoding="utf-8")
            binary = root / "Fixture.dll"
            binary.write_bytes(b"fixture")

            config = _config(["Packaging.CodeSigningHook = $TARGET_ENV{FIXTURE_SIGN_HOOK}"])
            packager = package_tool.Packager.__new__(package_tool.Packager)
            packager.fomain = config
            packager.project_config = package_tool.build_effective_project_config(config, "")
            packager.args = SimpleNamespace(platform="Windows", maincfg=str(root / "Fixture.fomain"))
            packager.target_output_path = str(root)

            with mock.patch.dict(os.environ, {"FIXTURE_SIGN_HOOK": str(hook)}, clear=False):
                with mock.patch.object(package_tool.subprocess, "call", return_value=0) as call:
                    packager.sign_windows_binaries()

            call.assert_called_once_with([str(hook), str(binary)])


if __name__ == "__main__":
    unittest.main()
