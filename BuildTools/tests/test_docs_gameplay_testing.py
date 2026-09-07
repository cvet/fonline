from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class GameplayTestingDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_defines_reusable_layers_runner_and_project_boundary(self) -> None:
        guide = self._read("Docs/en/how-to/testing/gameplay-and-integration.md")

        for heading in (
            "## Select the narrowest owning boundary",
            "## Deterministic fixture contract",
            "## Process runner contract",
            "## Readiness and marker semantics",
            "## Deadlines and cleanup",
            "## Result contract",
            "## CMake and CI integration",
            "## Failure routing",
            "## Project-owned boundary",
            "## Source paths inspected",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "boundary-based test selection",
            "Run narrow-first",
            "fixed sleep",
            "common monotonic deadline",
            "required_markers",
            "forbidden_markers",
            "expected_exit_code",
            "requests termination",
            "JSON schema version 1",
            "script test registration API",
        ):
            self.assertIn(contract, guide)

        self.assertNotIn("Last Frontier", guide)
        self.assertNotIn("TLA", guide)

    def test_runner_fixture_and_real_example_expose_the_same_contract(self) -> None:
        synthetic = json.loads(self._read("Examples/GameplayTestHarness/synthetic-smoke.json"))
        tutorial = json.loads(self._read("Examples/MinimalMultiplayer/tutorial-smoke.json"))
        wrapper = self._read("Examples/MinimalMultiplayer/run_tutorial_smoke.py")
        cmake = self._read("Examples/MinimalMultiplayer/CMakeLists.txt")

        self.assertEqual(synthetic["schema_version"], 1)
        self.assertEqual(tutorial["schema_version"], 1)
        self.assertEqual([scenario["id"] for scenario in tutorial["scenarios"]], ["content-test", "server-client-interaction"])
        self.assertIn("gameplay_test_runner.main", wrapper)
        self.assertNotIn("class CapturedProcess", wrapper)
        self.assertIn("tutorial-smoke.json", cmake)
        self.assertIn("RunTutorialChecks", cmake)

    def test_helper_cli_manifest_owns_the_runner(self) -> None:
        interface = json.loads(self._read("BuildTools/HelperCliInterface.json"))
        helper = next(helper for helper in interface["helpers"] if helper["id"] == "helper-cli.gameplay-test-runner")

        self.assertEqual(helper["source"], "BuildTools/gameplay_test_runner.py")
        self.assertEqual(helper["owner"], "quality")
        self.assertIn("embedding-project-build-system", helper["audiences"])

    def test_guide_is_routed_evaluated_promoted_and_checked(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        document = manifest["documents"]["Docs/en/how-to/testing/gameplay-and-integration.md"]
        self.assertEqual(document["id"], "gameplay-testing")
        self.assertEqual(document["state"], "current")
        self.assertEqual(document["disposition"], "retain")
        self.assertEqual(document["classification"]["visibility"], "public")
        self.assertEqual(document["classification"]["translation"], "required")
        legacy = manifest["documents"]["Docs/GameplayTesting.md"]
        self.assertEqual(legacy["id"], "legacy-gameplay-testing-route")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "gameplay-testing")
        self.assertIn("gameplay-testing", manifest["ai_delivery"]["llms"]["start_document_ids"])
        quality = next(group for group in manifest["site_delivery"]["navigation"] if group["id"] == "quality")
        self.assertIn("gameplay-testing", quality["document_ids"])

        evidence = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(record for record in evidence["records"] if record["id"] == "gameplay-test-harness")
        self.assertEqual(record["disposition"], "promoted")
        self.assertIn("Docs/en/how-to/testing/gameplay-and-integration.md", record["engine_targets"])
        self.assertEqual(record["planned_targets"], [])

        evaluation = json.loads(self._read("Docs/ai-evaluation.json"))
        task = next(task for task in evaluation["tasks"] if task["id"] == "testing-gameplay-harness")
        self.assertEqual(task["primary_document_id"], "gameplay-testing")
        self.assertGreaterEqual(len(task["retrieval_checks"]), 2)
        self.assertGreaterEqual(len(task["answer_checks"]), 3)

        workflow = self._read(".github/workflows/validate.yml")
        self.assertIn("python3 BuildTools/tests/test_gameplay_test_runner.py", workflow)
        self.assertIn("python3 BuildTools/tests/test_docs_gameplay_testing.py", workflow)


if __name__ == "__main__":
    unittest.main()
