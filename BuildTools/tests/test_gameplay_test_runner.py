from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILDTOOLS_DIR = ENGINE_ROOT / "BuildTools"
FIXTURE = ENGINE_ROOT / "Examples/GameplayTestHarness/fixture_process.py"
MANIFEST = ENGINE_ROOT / "Examples/GameplayTestHarness/synthetic-smoke.json"
sys.path.insert(0, str(BUILDTOOLS_DIR))

import gameplay_test_runner  # noqa: E402


class GameplayTestRunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.manifest = gameplay_test_runner.load_manifest(MANIFEST)
        self.values = {"python": sys.executable, "fixture": str(FIXTURE)}

    def _run(self, manifest: dict[str, object]) -> dict[str, object]:
        with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
            return gameplay_test_runner.run_manifest(manifest, self.values)

    def test_synthetic_server_client_scenario_passes_and_reports(self) -> None:
        report = self._run(self.manifest)

        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["scenario_count"], 1)
        self.assertEqual(report["passed_count"], 1)
        scenario = report["scenarios"][0]
        self.assertEqual(scenario["status"], "passed")
        self.assertFalse(scenario["timed_out"])
        self.assertEqual([process["id"] for process in scenario["processes"]], ["server", "client"])

        with tempfile.TemporaryDirectory() as temp_dir:
            report_path = Path(temp_dir) / "nested" / "report.json"
            gameplay_test_runner.write_report(report_path, report)
            saved = json.loads(report_path.read_text(encoding="utf-8"))
        self.assertEqual(saved["suite"], "synthetic-gameplay-smoke")
        self.assertNotIn("command", saved["scenarios"][0]["processes"][0])

    def test_missing_and_forbidden_markers_fail(self) -> None:
        missing_manifest = copy.deepcopy(self.manifest)
        missing_manifest["scenarios"][0]["processes"][1]["required_markers"].append("never_emitted")
        missing_report = self._run(missing_manifest)
        self.assertEqual(missing_report["status"], "failed")
        self.assertIn("never_emitted", missing_report["scenarios"][0]["processes"][1]["missing_markers"])

        forbidden_manifest = copy.deepcopy(self.manifest)
        forbidden_manifest["forbidden_markers"] = ["synthetic_client_connected"]
        forbidden_report = self._run(forbidden_manifest)
        self.assertEqual(forbidden_report["status"], "failed")
        self.assertEqual(
            forbidden_report["scenarios"][0]["processes"][1]["forbidden_markers"],
            ["synthetic_client_connected"],
        )

    def test_common_deadline_times_out_and_stops_children(self) -> None:
        timeout_manifest = {
            "schema_version": 1,
            "name": "timeout",
            "default_timeout_seconds": 0.1,
            "scenarios": [
                {
                    "id": "slow-child",
                    "processes": [
                        {
                            "id": "child",
                            "command": ["{python}", "{fixture}", "--delay-ms", "3000"],
                        }
                    ],
                }
            ],
        }

        report = self._run(timeout_manifest)

        scenario = report["scenarios"][0]
        self.assertEqual(report["status"], "failed")
        self.assertTrue(scenario["timed_out"])
        self.assertTrue(any("exceeded" in reason for reason in scenario["reasons"]))
        self.assertIsNotNone(scenario["processes"][0]["exit_code"])

    def test_invalid_manifest_and_unresolved_placeholder_return_two(self) -> None:
        invalid = copy.deepcopy(self.manifest)
        invalid["unknown"] = True
        with tempfile.TemporaryDirectory() as temp_dir:
            invalid_path = Path(temp_dir) / "invalid.json"
            invalid_path.write_text(json.dumps(invalid), encoding="utf-8")
            with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                self.assertEqual(gameplay_test_runner.main(["--manifest", str(invalid_path)]), 2)

        with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
            self.assertEqual(gameplay_test_runner.main(["--manifest", str(MANIFEST)]), 2)


if __name__ == "__main__":
    unittest.main()
