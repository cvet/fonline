from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import patch


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

    def _manifest_with_timeout(self, field: str, token: str) -> str:
        manifest = copy.deepcopy(self.manifest)
        target = manifest
        if field != "default_timeout_seconds":
            target = manifest["scenarios"][0]
        if field == "ready_timeout_seconds":
            target = target["processes"][0]
        target[field] = "__TIMEOUT__"
        return json.dumps(manifest).replace('"__TIMEOUT__"', token)

    def test_timeout_fields_reject_invalid_numbers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "timeout.json"
            for field in ("default_timeout_seconds", "timeout_seconds", "ready_timeout_seconds"):
                for token in ("NaN", "Infinity", "1e309", "-Infinity", "0", "-1", "true", '"5"', "1" + "0" * 400):
                    with self.subTest(field=field, token=token):
                        path.write_text(self._manifest_with_timeout(field, token), encoding="utf-8")
                        with self.assertRaises(gameplay_test_runner.ManifestError) as error:
                            gameplay_test_runner.load_manifest(path)
                        self.assertIn(field, str(error.exception))

    def test_invalid_timeouts_return_two_before_process_startup(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "timeout.json"
            for field in ("default_timeout_seconds", "timeout_seconds", "ready_timeout_seconds"):
                for token in ("NaN", "Infinity", "1e309", "-Infinity", "0", "-1", "true", '"5"', "1" + "0" * 400):
                    with self.subTest(field=field, token=token):
                        path.write_text(self._manifest_with_timeout(field, token), encoding="utf-8")
                        with patch.object(gameplay_test_runner, "run_manifest", return_value={"status": "passed"}) as run, \
                                patch.object(gameplay_test_runner.subprocess, "Popen") as popen, \
                                redirect_stdout(StringIO()), redirect_stderr(StringIO()) as stderr:
                            self.assertEqual(gameplay_test_runner.main(["--manifest", str(path)]), 2)
                            self.assertIn(field, stderr.getvalue())
                            self.assertIn("finite positive number", stderr.getvalue())
                            run.assert_not_called()
                            popen.assert_not_called()

    def test_timeout_fields_accept_finite_positive_numbers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "timeout.json"
            for field in ("default_timeout_seconds", "timeout_seconds", "ready_timeout_seconds"):
                for token in ("1", "0.25", "1e308"):
                    with self.subTest(field=field, token=token):
                        path.write_text(self._manifest_with_timeout(field, token), encoding="utf-8")
                        manifest = gameplay_test_runner.load_manifest(path)
                        target = manifest
                        if field != "default_timeout_seconds":
                            target = manifest["scenarios"][0]
                        if field == "ready_timeout_seconds":
                            target = target["processes"][0]
                        self.assertEqual(target[field], json.loads(token))

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
