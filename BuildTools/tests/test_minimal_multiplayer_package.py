from __future__ import annotations

import importlib.util
import json
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock


ENGINE_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ENGINE_ROOT / "Examples/MinimalMultiplayer/verify_tutorial_package.py"
SPEC = importlib.util.spec_from_file_location("verify_tutorial_package", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
verify_tutorial_package = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(verify_tutorial_package)

import secure_channel_key  # noqa: E402


class MinimalMultiplayerPackageTests(unittest.TestCase):
    def _fixture(self, root: Path) -> Path:
        payload = root / "FOMM-Client-TutorialSmoke"
        (payload / "Resources").mkdir(parents=True)
        (payload / "client.bin").write_bytes(b"client")
        (payload / "Resources/content.bin").write_bytes(b"content")
        return payload

    def test_zip_and_tar_archives_match_payload_inventory(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            payload = self._fixture(root)
            zip_path = Path(str(payload) + ".zip")
            with zipfile.ZipFile(zip_path, "w") as archive:
                for path in payload.rglob("*"):
                    if path.is_file():
                        archive.write(path, f"{payload.name}/{path.relative_to(payload).as_posix()}")
            verify_tutorial_package.verify_archive(payload, zip_path)

            tar_path = Path(str(payload) + ".tar.gz")
            with tarfile.open(tar_path, "w:gz") as archive:
                archive.add(payload, arcname=payload.name)
            verify_tutorial_package.verify_archive(payload, tar_path)

    def test_archive_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            payload = self._fixture(root)
            archive_path = Path(str(payload) + ".zip")
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(f"{payload.name}/unexpected.bin", b"unexpected")
            with self.assertRaisesRegex(RuntimeError, "archive inventory mismatch"):
                verify_tutorial_package.verify_archive(payload, archive_path)

    def test_packaged_server_gets_the_local_channel_secret_for_one_run(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            example_root = root / "example"
            server_root = root / "server"
            server_root.mkdir()
            secret = example_root / verify_tutorial_package.CHANNEL_SECRET
            public_key = secure_channel_key.ensure_key_pair(secret, secret.with_suffix(".pub"))

            with mock.patch.object(verify_tutorial_package, "ROOT", example_root):
                with verify_tutorial_package.deployed_channel_secret(server_root):
                    deployed = server_root / verify_tutorial_package.CHANNEL_SECRET
                    self.assertEqual(secure_channel_key.read_public_key(deployed), public_key)

            self.assertEqual(list(server_root.iterdir()), [])

    def test_packaged_server_requires_the_baked_channel_key(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            with mock.patch.object(verify_tutorial_package, "ROOT", root / "example"):
                with self.assertRaisesRegex(RuntimeError, "missing local channel key"):
                    with verify_tutorial_package.deployed_channel_secret(root):
                        pass

    def test_verifier_imports_from_standalone_repository_layout(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            buildtools = root / "Engine/BuildTools"
            buildtools.mkdir(parents=True)
            shutil.copy2(ENGINE_ROOT / "BuildTools/gameplay_test_runner.py", buildtools)
            script = root / "verify_tutorial_package.py"
            shutil.copy2(SCRIPT, script)
            spec = importlib.util.spec_from_file_location("standalone_verify_tutorial_package", script)
            assert spec is not None and spec.loader is not None
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)

            # The verifier anchors on its resolved location; a temporary directory may be an unresolved 8.3 alias
            self.assertEqual(module.BUILDTOOLS_ROOT, buildtools.resolve())

    def test_checked_package_manifest_and_cmake_cover_both_hosts(self) -> None:
        manifest = json.loads((ENGINE_ROOT / "Examples/MinimalMultiplayer/package-smoke.json").read_text(encoding="utf-8"))
        cmake = (ENGINE_ROOT / "Examples/MinimalMultiplayer/CMakeLists.txt").read_text(encoding="utf-8")
        generator = (ENGINE_ROOT / "Examples/MinimalMultiplayer/generate_config.py").read_text(encoding="utf-8")

        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual(verify_tutorial_package.BUILDTOOLS_ROOT, ENGINE_ROOT / "BuildTools")
        self.assertEqual(manifest["scenarios"][0]["id"], "packaged-server-client-interaction")
        self.assertEqual(verify_tutorial_package.PACKAGE_CONFIG, "TutorialSmoke")
        self.assertIn("DefinePackage(Tutorial", cmake)
        self.assertIn("Raw+Zip+Headless", cmake)
        self.assertIn("Raw+TarGz+Headless", cmake)
        self.assertIn("RunTutorialPackageChecks", cmake)
        self.assertIn("CheckTutorialConfig", cmake)
        tutorial_checks = (
            cmake.split("add_custom_target(RunTutorialChecks", 1)[1]
            .split("if(FO_WINDOWS OR FO_LINUX)", 1)[0]
        )
        self.assertIn("FOMM_ClientHeadless ForceBakeResources", tutorial_checks)
        self.assertNotIn("FOMM_ClientHeadless BakeResources", tutorial_checks)
        self.assertIn("Settings.inc", generator)

    def test_channel_key_is_provisioned_before_every_bake_and_never_tracked(self) -> None:
        example = ENGINE_ROOT / "Examples/MinimalMultiplayer"
        config = (example / "FOnlineMinimalMultiplayer.fomain").read_text(encoding="utf-8")
        cmake = (example / "CMakeLists.txt").read_text(encoding="utf-8")
        runner = (example / "run_tutorial_smoke.py").read_text(encoding="utf-8")
        secret = verify_tutorial_package.CHANNEL_SECRET.as_posix()
        public = "Workspace/TutorialChannel.pub"

        self.assertIn(f"ServerNetwork.ChannelSecretKey = $TARGET_FILE{{{secret}}}", config)
        self.assertIn(f"ClientNetwork.ChannelServerKeys = $FILE{{{public}}}", config)
        provision = cmake.split("add_custom_target(ProvisionTutorialChannelKey", 1)[1].split(")", 1)[0]
        self.assertIn(f"ensure-pair\n            {secret}\n            {public}", provision)
        self.assertIn("add_dependencies(BakeResources CheckTutorialConfig ProvisionTutorialChannelKey)", cmake)
        self.assertIn("add_dependencies(ForceBakeResources CheckTutorialConfig ProvisionTutorialChannelKey)", cmake)
        self.assertIn(f'Path("{secret}")', runner)
        self.assertIn("secure_channel_key.ensure_key_pair", runner)
        self.assertIn("/Workspace/", (example / ".gitignore").read_text(encoding="utf-8").splitlines())

    def test_generated_distribution_config_is_current(self) -> None:
        result = subprocess.run(
            [sys.executable, "generate_config.py", "--check"],
            cwd=ENGINE_ROOT / "Examples/MinimalMultiplayer",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
