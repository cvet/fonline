from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class DocumentationReleaseOperationsTests(unittest.TestCase):
    def test_guide_owns_reusable_release_lifecycle(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/how-to/release/operations.md").read_text(encoding="utf-8")

        for heading in (
            "## Establish the operating boundary",
            "## Choose the server process",
            "## Define readiness and health",
            "## Prepare a deployment",
            "## Roll out and verify",
            "## Stop safely",
            "## Roll back",
            "## Failure routing",
            "## Validate the runbook",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "Start server complete!",
            "Server stopped!",
            "Server.WriteHealthFile",
            "Server.HealthFilePeriodMs",
            "Server.ShutdownGraceMs",
            "SERVICE_CONTROL_STOP",
            "SIGTERM",
            "immutable release unit",
            "project-owned functional probe",
            "not a substitute for a backup",
        ):
            self.assertIn(contract, guide)

    def test_source_still_exposes_documented_lifecycle_signals(self) -> None:
        server = (ENGINE_ROOT / "Source/Server/Server.cpp").read_text(encoding="utf-8")
        service = (ENGINE_ROOT / "Source/Applications/ServerServiceApp.cpp").read_text(encoding="utf-8")
        daemon = (ENGINE_ROOT / "Source/Applications/ServerDaemonApp.cpp").read_text(encoding="utf-8")
        signals = (ENGINE_ROOT / "Source/Frontend/ApplicationInit.cpp").read_text(encoding="utf-8")

        for marker in (
            'WriteLog("Start server complete!")',
            'WriteLog("Server stopped!")',
            "Settings->ShutdownGraceMs",
            'WriteHealthFile("Starting...")',
        ):
            self.assertIn(marker, server)
        self.assertIn("SERVICE_CONTROL_STOP", service)
        self.assertIn("SetFOServiceStatus(SERVICE_RUNNING)", service)
        self.assertIn("GetApp()->RequestQuit(false)", service)
        self.assertIn("Data->ServerThread.join()", service)
        self.assertIn('wstring path = ::GetCommandLineW();', service)
        self.assertIn('path.append(L" --server-service-start")', service)
        self.assertNotIn('append(L" --server-service")', service)
        self.assertIn(
            'FO_VERIFY_AND_THROW(Platform::ForkProcess(), "Failed to fork server daemon process")',
            daemon,
        )
        self.assertIn("std::signal(SIGTERM, SignalHandler)", signals)

    def test_manifest_registers_public_human_release_operations_guide(self) -> None:
        manifest = json.loads((ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(encoding="utf-8"))
        entry = manifest["documents"]["Docs/en/how-to/release/operations.md"]
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")

        self.assertEqual(entry["id"], "release-operations")
        self.assertEqual(entry["owner"], "build-release")
        self.assertEqual(entry["state"], "current")
        self.assertEqual(entry["classification"]["visibility"], "public")
        self.assertTrue(entry["classification"]["human"])
        self.assertEqual(entry["classification"]["translation"], "required")
        self.assertIn("python3 BuildTools/tests/test_docs_release_operations.py", workflow)


if __name__ == "__main__":
    unittest.main()
