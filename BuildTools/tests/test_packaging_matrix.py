from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_ROOT = ENGINE_ROOT / "Examples/PackagingMatrix"


def load_generator():
    spec = importlib.util.spec_from_file_location(
        "packaging_matrix_generate_config",
        FIXTURE_ROOT / "generate_config.py",
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load packaging matrix config generator")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class PackagingMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.generator = load_generator()

    def test_generated_config_is_current_and_covers_source_settings(self) -> None:
        rendered = self.generator.render_config(ENGINE_ROOT)
        current = (FIXTURE_ROOT / "FOnlinePackagingMatrix.fomain").read_text(encoding="utf-8")
        self.assertEqual(current, rendered)

        settings = self.generator.parse_settings(ENGINE_ROOT / "Source/Common/Settings.inc")
        managed = self.generator.parse_runtime_managed_settings(ENGINE_ROOT / "Source/Common/Settings.cpp")
        emitted_keys = {
            line.split("=", 1)[0].strip()
            for line in rendered.splitlines()
            if "=" in line and not line.startswith("#")
        }
        self.assertEqual(
            {setting.name for setting in settings} - managed,
            emitted_keys - {"Name", "InputDirs", "IncludePatterns", "Bakers", "ServerOnly", "PackageSmoke.Automation"},
        )
        self.assertIn("Web.BackgroundColor = rgb(0, 0, 0)", rendered)

    def test_fixture_packages_host_roles_and_runs_embedded_configs(self) -> None:
        cmake = (FIXTURE_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        script = (FIXTURE_ROOT / "Scripts/PackageSmoke.fos").read_text(encoding="utf-8")
        verifier = (FIXTURE_ROOT / "verify_package.py").read_text(encoding="utf-8")
        for marker in (
            "Raw+Zip+Headless+Service",
            "Raw+TarGz+Headless+Daemon",
            "add_dependencies(MakePackage-PackageSmoke",
            "DEPENDS MakePackage-PackageSmoke",
        ):
            self.assertIn(marker, cmake)
        for marker in ("Settings.Common.Packaged", "packaging_matrix_server_passed", "packaging_matrix_client_passed"):
            self.assertIn(marker, script)
        for marker in ("packaging-manifest.json", "verify_archive", "run_packaged", "engine_revision"):
            self.assertIn(marker, verifier)

    def test_package_smoke_is_fixture_owned_and_not_a_required_engine_lane(self) -> None:
        sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))
        import buildtools

        self.assertNotIn("win64-package-smoke", buildtools.VALIDATION_TARGETS)
        self.assertNotIn("linux-package-smoke", buildtools.VALIDATION_TARGETS)

        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")
        self.assertNotIn("package-smoke", workflow)

        support = json.loads((ENGINE_ROOT / "BuildTools/SupportMatrix.json").read_text(encoding="utf-8"))
        targets = {
            target
            for platform in support["platforms"]
            for target in platform["ci_validation_targets"]
        }
        self.assertNotIn("win64-package-smoke", targets)
        self.assertNotIn("linux-package-smoke", targets)


if __name__ == "__main__":
    unittest.main()
