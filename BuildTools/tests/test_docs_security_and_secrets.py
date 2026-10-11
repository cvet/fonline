from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class DocumentationSecurityAndSecretsTests(unittest.TestCase):
    def test_guide_owns_secret_flow_and_operational_boundaries(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/how-to/release/security-and-secrets.md").read_text(encoding="utf-8")

        for heading in (
            "## Threat model and ownership",
            "## Choose the right configuration form",
            "## Understand redaction limits",
            "## Package and sign without copying credentials",
            "## CI trust boundaries",
            "## Provision, rotate, and revoke",
            "## Verification workflow",
            "## Failure routing",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "`$TARGET_ENV{NAME}`",
            "`$TARGET_FILE{path}`",
            "Common.SecretSettingTokens",
            "FO_ANDROID_RELEASE_STORE_PASSWORD",
            "FO_ANDROID_RELEASE_KEY_PASSWORD",
            "Packaging.CodeSigningHook",
            "synthetic canary",
            "revoke",
            "project-owned",
        ):
            self.assertIn(contract, guide)

        self.assertNotIn("Android.KeyPassword = $ENV{", guide)
        self.assertNotIn("Auth.SessionSigningSecret = $ENV{", guide)

    def test_packager_uses_baked_config_and_gradle_env_handoff(self) -> None:
        package_source = (ENGINE_ROOT / "BuildTools/package.py").read_text(encoding="utf-8")
        gradle_source = (ENGINE_ROOT / "BuildTools/android-project/app/build.gradle").read_text(encoding="utf-8")

        for symbol in (
            "load_config_data",
            "get_effective_config_section",
            "ANDROID_RELEASE_STORE_PASSWORD_ENV",
            "ANDROID_RELEASE_KEY_PASSWORD_ENV",
        ):
            self.assertIn(symbol, package_source)
        self.assertNotIn("resolve_build_host_config_value", package_source)
        self.assertIn("android_config.getStr('Android.KeystorePassword', '')", package_source)
        self.assertIn("android_config.getStr('Android.KeyPassword', '')", package_source)
        self.assertIn("System.getenv('FO_ANDROID_RELEASE_STORE_PASSWORD')", gradle_source)
        self.assertIn("System.getenv('FO_ANDROID_RELEASE_KEY_PASSWORD')", gradle_source)
        self.assertNotIn("$RELEASE_STORE_PASSWORD$", gradle_source)
        self.assertNotIn("$RELEASE_KEY_PASSWORD$", gradle_source)

    def test_manifest_registers_public_human_security_guide(self) -> None:
        manifest = json.loads((ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(encoding="utf-8"))
        entry = manifest["documents"]["Docs/en/how-to/release/security-and-secrets.md"]

        self.assertEqual(entry["id"], "security-and-secrets")
        self.assertEqual(entry["owner"], "build-release")
        self.assertEqual(entry["state"], "current")
        self.assertEqual(entry["classification"]["visibility"], "public")
        self.assertTrue(entry["classification"]["human"])
        self.assertEqual(entry["classification"]["translation"], "required")


if __name__ == "__main__":
    unittest.main()
