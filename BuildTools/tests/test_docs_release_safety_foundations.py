from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
OPERATIONS_GUIDE = "Docs/en/how-to/release/operations.md"
BACKUP_GUIDE = "Docs/en/how-to/release/backup-and-recovery.md"
SECURITY_GUIDE = "Docs/en/how-to/release/security-and-secrets.md"


class ReleaseSafetyFoundationsDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_windows_service_registration_and_log_locations_match_the_runbook(self) -> None:
        service = self._read("Source/Applications/ServerServiceApp.cpp")
        application = self._read("Source/Frontend/ApplicationInit.cpp")
        server = self._read("Source/Server/Server.cpp")
        guide = " ".join(self._read(OPERATIONS_GUIDE).split())

        self.assertIn('wstring path = ::GetCommandLineW();', service)
        self.assertIn('path.append(L" --server-service-start")', service)
        self.assertNotIn('append(L" --server-service")', service)
        self.assertIn('LogToFile(GetExeLogFileName()', application)
        self.assertIn('if (!settings.UserWritablePath.empty())', application)
        self.assertIn('fs_make_writable_path(settings.UserWritablePath, GetExeLogFileName())', application)
        self.assertIn('fs_make_path(_healthFileName)', server)

        for marker in (
            "exact current command line with `--server-service-start`",
            "`--server-service-delete` removes the registration",
            "moves below the resolved `Client.UserWritablePath`",
            "health file uses an executable-derived name and remains in the working directory",
        ):
            self.assertIn(marker, guide)

    def test_backup_guide_preserves_backend_and_restore_safety_boundaries(self) -> None:
        database = self._read("Source/Server/DataBase.cpp")
        sqlite = self._read("Source/Server/DataBase-SQLite.cpp")
        guide = self._read(BACKUP_GUIDE)

        for connection in (
            'options.front() == "JSON" && options.size() == 2',
            'options.front() == "DbSQLite" && options.size() == 2',
            'options.front() == "Mongo" && options.size() == 3',
            'options.front() == "Memory" && options.size() == 1',
        ):
            self.assertIn(connection, database)
        self.assertIn('Execute("PRAGMA journal_mode = WAL"', sqlite)
        self.assertIn('Execute("PRAGMA synchronous = NORMAL"', sqlite)

        for marker in (
            "plausible size/count comparison with prior recovery points",
            "empty, explicitly allowlisted destination",
            "prove the exact destination before the first write",
            "delete a successful disposable destination only through an exact-name cleanup guard",
            "Backend integrity alone cannot prove game semantics",
        ):
            self.assertIn(marker, guide)

    def test_secret_resolution_and_redaction_limits_remain_source_backed(self) -> None:
        settings = self._read("Source/Common/Settings.cpp")
        baker = self._read("Source/Tools/ConfigBaker.cpp")
        packager = self._read("BuildTools/package.py")
        guide = self._read(SECURITY_GUIDE)

        self.assertIn('"$TARGET_ENV{"', settings)
        self.assertIn('"$TARGET_FILE{"', settings)
        self.assertIn('(!_bakingMode && (is_target_env || is_target_file))', settings)
        self.assertIn('WriteLog("Unknown setting {} = {}", key, value)', baker)
        self.assertIn("ANDROID_RELEASE_STORE_PASSWORD_ENV", packager)
        self.assertIn("ANDROID_RELEASE_KEY_PASSWORD_ENV", packager)

        self.assertIn("target-time directives are the default for runtime secrets", guide)
        self.assertIn("Repository-secret masking is not a content scanner", guide)
        self.assertIn("unique synthetic canary", guide)

    def test_canonical_and_legacy_release_safety_routes_are_explicit(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        routes = (
            (OPERATIONS_GUIDE, "release-operations", "Docs/ReleaseOperations.md", "legacy-release-operations-route"),
            (BACKUP_GUIDE, "backup-and-recovery", "Docs/BackupAndRecovery.md", "legacy-backup-and-recovery-route"),
            (SECURITY_GUIDE, "security-and-secrets", "Docs/SecurityAndSecrets.md", "legacy-security-and-secrets-route"),
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
        for old_path in ("Docs/ReleaseOperations.md", "Docs/BackupAndRecovery.md", "Docs/SecurityAndSecrets.md"):
            self.assertNotIn(old_path, evidence)
        for canonical_path, _, _, _ in routes:
            self.assertIn(canonical_path, evidence)


if __name__ == "__main__":
    unittest.main()
