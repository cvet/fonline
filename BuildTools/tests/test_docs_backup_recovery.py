from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class DocumentationBackupRecoveryTests(unittest.TestCase):
    def test_guide_owns_reusable_backup_and_restore_boundary(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/how-to/release/backup-and-recovery.md").read_text(encoding="utf-8")

        for heading in (
            "## Establish the recovery boundary",
            "## Identify the durable set",
            "## Understand the recovery oplog",
            "## Define the backup contract",
            "## Take a quiesced backup",
            "## Capture a database incident",
            "## Restore safely",
            "## Rehearse disaster recovery",
            "## Failure routing",
            "## Validate the runbook",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "recovery point objective (RPO)",
            "recovery time objective (RTO)",
            "DbPendingChanges-committed.oplog",
            "The recovery oplog is not a backup",
            "Storage.sqlite",
            "WAL",
            "synchronous=NORMAL",
            "provider-native",
            "Server stopped!",
            "Start server complete!",
            "project-owned semantic probe",
            "Never mix binaries, resources, and data",
            "explicitly allowlisted destination",
            "prove the exact destination before the first write",
        ):
            self.assertIn(contract, guide)

    def test_source_still_exposes_documented_backend_and_replay_contracts(self) -> None:
        settings = (ENGINE_ROOT / "Source/Common/Settings.inc").read_text(encoding="utf-8")
        database = (ENGINE_ROOT / "Source/Server/DataBase.cpp").read_text(encoding="utf-8")
        json_backend = (ENGINE_ROOT / "Source/Server/DataBase-Json.cpp").read_text(encoding="utf-8")
        sqlite_backend = (ENGINE_ROOT / "Source/Server/DataBase-SQLite.cpp").read_text(encoding="utf-8")
        mongo_backend = (ENGINE_ROOT / "Source/Server/DataBase-Mongo.cpp").read_text(encoding="utf-8")

        for marker in (
            "OpLogEnabled",
            'OpLogPath, "DbPendingChanges.oplog"',
            "ReconnectRetryPeriod",
            "PanicOpLogSizeThreshold",
            "PanicShutdownTimeout",
            'DbStorage, "Memory"',
        ):
            self.assertIn(marker, settings)
        for marker in (
            "InitializeOpLogs()",
            "RestorePendingChanges()",
            "Empty oplog path in settings",
            'strex(_settings->OpLogPath).replace(".oplog", "-committed.oplog")',
            "Pending database insert replay conflict",
            "StartPanic",
            "fsync(_fd)",
        ):
            self.assertIn(marker, database)
        self.assertIn('strex("{}.tmp", path)', json_backend)
        self.assertIn("fs_rename(tmp_path, path)", json_backend)
        self.assertIn('strex("{}/Storage.sqlite", _storageDir)', sqlite_backend)
        self.assertIn('Execute("PRAGMA journal_mode = WAL"', sqlite_backend)
        self.assertIn('Execute("PRAGMA synchronous = NORMAL"', sqlite_backend)
        self.assertIn("mongoc_client_new_from_uri", mongo_backend)

    def test_manifest_registers_public_human_recovery_guide(self) -> None:
        manifest = json.loads((ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(encoding="utf-8"))
        entry = manifest["documents"]["Docs/en/how-to/release/backup-and-recovery.md"]
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")

        self.assertEqual(entry["id"], "backup-and-recovery")
        self.assertEqual(entry["owner"], "runtime")
        self.assertEqual(entry["state"], "current")
        self.assertEqual(entry["classification"]["visibility"], "public")
        self.assertTrue(entry["classification"]["human"])
        self.assertEqual(entry["classification"]["translation"], "required")
        self.assertIn("python3 BuildTools/tests/test_docs_backup_recovery.py", workflow)


if __name__ == "__main__":
    unittest.main()
