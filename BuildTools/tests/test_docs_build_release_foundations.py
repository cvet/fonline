from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
PIPELINE_GUIDE = "Docs/en/reference/cmake-and-buildtools/pipeline.md"
UPGRADE_GUIDE = "Docs/en/how-to/migration/engine-upgrade.md"
SUPPORT_GUIDE = "Docs/en/reference/platforms/support-matrix.md"
PACKAGING_GUIDE = "Docs/en/how-to/release/packaging.md"


class BuildReleaseFoundationsDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_pipeline_stage_order_matches_the_live_project_interface(self) -> None:
        interface = json.loads(self._read("BuildTools/cmake/ProjectInterface.json"))
        expected_names = [stage["name"] for stage in interface["stages"]]
        self.assertEqual(
            expected_names,
            [
                "Init",
                "ProjectOptions",
                "ThirdParty",
                "EngineSources",
                "Codegen",
                "CoreLibs",
                "Applications",
                "ScriptsAndBaking",
                "Packages",
                "Finalize",
            ],
        )

        guide = self._read(PIPELINE_GUIDE)
        positions = [guide.index(f"### `{name}.cmake`") for name in expected_names]
        self.assertEqual(positions, sorted(positions))
        self.assertLess(guide.index("### `Applications.cmake`"), guide.index("### `ScriptsAndBaking.cmake`"))
        self.assertIn("There is no generic Editor application or validation target.", guide)

    def test_required_workflow_rejects_retired_generic_editor_targets(self) -> None:
        workflow = self._read(".github/workflows/validate.yml")
        for retired in ("win64-editor", "linux-editor", "linux-gcc-editor"):
            self.assertNotIn(retired, workflow)

        support = json.loads(self._read("BuildTools/SupportMatrix.json"))
        required_targets = {
            target
            for profile in support["platforms"]
            for target in profile["ci_validation_targets"]
        }
        self.assertTrue(required_targets)
        for target in required_targets:
            self.assertIn(target, workflow)

    def test_upgrade_guide_pins_contract_diff_and_generated_dependency_order(self) -> None:
        guide = self._read(UPGRADE_GUIDE)
        for argument in (
            "--root Engine",
            "--baseline-git-ref <old-engine-revision>",
            "--current-dir Docs/generated",
            "--dispositions Docs/contract-change-dispositions.json",
            "--write",
            "--enforce",
        ):
            self.assertIn(argument, guide)

        ordered_markers = (
            "project-generated references and snippet inventory",
            "localization status",
            "site routes, navigation, and search",
            "AI evaluation and delivery artifacts",
        )
        positions = [guide.index(marker) for marker in ordered_markers]
        self.assertEqual(positions, sorted(positions))

    def test_support_and_package_models_preserve_capability_boundaries(self) -> None:
        support = json.loads(self._read("BuildTools/SupportMatrix.json"))
        levels = [profile["level"] for profile in support["platforms"]]
        self.assertEqual(len(levels), 10)
        self.assertEqual(levels.count("build_gated"), 5)
        self.assertEqual(levels.count("smoke_gated"), 3)
        self.assertEqual(levels.count("source_capable"), 2)

        package = json.loads(self._read("BuildTools/PackageInterface.json"))
        platforms = {entry["name"]: entry["status"] for entry in package["platforms"]}
        self.assertEqual(
            platforms,
            {
                "Windows": "implemented",
                "Linux": "implemented",
                "Android": "implemented",
                "Web": "implemented",
                "macOS": "unsupported",
                "iOS": "unsupported",
            },
        )
        self.assertNotIn("Editor", {entry["name"] for entry in package["targets"]})

        support_guide = self._read(SUPPORT_GUIDE)
        packaging_guide = self._read(PACKAGING_GUIDE)
        self.assertIn("project-qualified", support_guide.lower())
        self.assertIn("project-qualified", packaging_guide.lower())
        self.assertIn("currently aborts for both `macOS` and `iOS`", packaging_guide)

    def test_canonical_and_legacy_build_release_routes_are_explicit(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        routes = (
            (PIPELINE_GUIDE, "buildtools-pipeline", "Docs/BuildToolsPipeline.md", "legacy-buildtools-pipeline-route"),
            (UPGRADE_GUIDE, "engine-upgrade-guide", "Docs/EngineUpgradeGuide.md", "legacy-engine-upgrade-guide-route"),
            (SUPPORT_GUIDE, "support-matrix", "Docs/SupportMatrix.md", "legacy-support-matrix-route"),
            (PACKAGING_GUIDE, "packaging-and-release", "Docs/PackagingAndRelease.md", "legacy-packaging-and-release-route"),
        )
        for canonical_path, document_id, legacy_path, legacy_id in routes:
            canonical = manifest["documents"][canonical_path]
            legacy = manifest["documents"][legacy_path]
            self.assertEqual(canonical["id"], document_id)
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(canonical["classification"]["translation"], "required")
            self.assertEqual(legacy["id"], legacy_id)
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["disposition"], "replace")
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertFalse(legacy["classification"]["human"])

            legacy_text = self._read(legacy_path)
            self.assertIn("> Legacy route.", legacy_text)
            self.assertIn(canonical_path.removeprefix("Docs/"), legacy_text)
            russian_path = canonical_path.replace("Docs/en/", "Docs/ru/").removeprefix("Docs/")
            self.assertIn(russian_path, legacy_text)

        evidence = self._read("BuildTools/ExternalProjectEvidence.json")
        for old_path in (
            "Docs/BuildToolsPipeline.md",
            "Docs/EngineUpgradeGuide.md",
            "Docs/SupportMatrix.md",
            "Docs/PackagingAndRelease.md",
        ):
            self.assertNotIn(old_path, evidence)
        for canonical_path, _, _, _ in routes:
            self.assertIn(canonical_path, evidence)


if __name__ == "__main__":
    unittest.main()
