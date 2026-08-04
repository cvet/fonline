from __future__ import annotations

import contextlib
import copy
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_external_evidence  # noqa: E402


class ExternalProjectEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_external_evidence.generate_model(ENGINE_ROOT)

    def test_live_inventory_is_complete_and_deterministic(self) -> None:
        self.assertEqual(
            self.model,
            docs_external_evidence.generate_model(ENGINE_ROOT),
        )
        self.assertEqual(self.model["summary"]["snapshot_count"], 2)
        self.assertEqual(self.model["summary"]["record_count"], 30)
        self.assertEqual(
            set(self.model["summary"]["records_by_disposition"]),
            docs_external_evidence.VALID_DISPOSITIONS,
        )
        self.assertEqual(
            set(self.model["owner_review_requirements"]),
            set(self.model["owners"]),
        )
        self.assertIn("localization", self.model["owners"])

    def test_every_record_has_owner_target_and_review_gate(self) -> None:
        for record in self.model["records"]:
            self.assertIn(record["owner"], record["required_reviews"])
            self.assertTrue(record["promotion_gate"])
            self.assertTrue(record["decision"])
            if record["disposition"] == "promoted":
                self.assertTrue(record["engine_targets"])
            elif record["disposition"] == "boundary-owned":
                self.assertTrue(record["engine_targets"])
                self.assertTrue(record["external_targets"])
            elif record["disposition"] == "promotion-candidate":
                self.assertTrue(record["planned_targets"])
            elif record["disposition"] == "project-owned":
                self.assertFalse(record["engine_targets"])
                self.assertFalse(record["planned_targets"])
                self.assertTrue(record["external_targets"])

    def test_owner_review_policy_must_cover_every_owner(self) -> None:
        manifest = {
            "owners": {"documentation": "Docs", "quality": "Tests"},
            "owner_review_requirements": {
                "documentation": {
                    "scope": "Docs",
                    "required_evidence": ["validation"],
                    "co_review_when": ["contracts change"],
                }
            },
        }
        with self.assertRaisesRegex(ValueError, "must match owners exactly"):
            docs_external_evidence._validate_owner_policy(manifest)

    def test_checked_outputs_are_current(self) -> None:
        rendered = docs_external_evidence.render_outputs(ENGINE_ROOT)
        self.assertEqual(
            set(rendered),
            set(docs_external_evidence.OUTPUT_PATHS),
        )
        self.assertIn(
            "External Project Evidence And Promotion Inventory",
            rendered[docs_external_evidence.DEFAULT_INDEX],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_external_evidence.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

    @staticmethod
    def _create_repository(root: Path, source_path: str) -> str:
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        subprocess.run(
            ["git", "-C", str(root), "config", "user.email", "test@example.invalid"],
            check=True,
        )
        subprocess.run(
            ["git", "-C", str(root), "config", "user.name", "Test"],
            check=True,
        )
        path = root / source_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("source\n", encoding="utf-8")
        subprocess.run(["git", "-C", str(root), "add", "."], check=True)
        subprocess.run(
            ["git", "-C", str(root), "commit", "-q", "-m", "fixture"],
            check=True,
        )
        return subprocess.run(
            ["git", "-C", str(root), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        ).stdout.strip()

    def test_external_verification_reads_the_pinned_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            roots = {
                "last-frontier": temporary_root / "last-frontier",
                "fonline-tla": temporary_root / "fonline-tla",
            }
            revisions = {
                snapshot: self._create_repository(root, "Docs/Guide.md")
                for snapshot, root in roots.items()
            }
            model = {
                "snapshots": {
                    snapshot: {"revision": revision}
                    for snapshot, revision in revisions.items()
                },
                "records": [
                    {
                        "sources": [
                            {"snapshot": snapshot, "path": "Docs/Guide.md"}
                            for snapshot in roots
                        ]
                    }
                ],
            }
            for root in roots.values():
                (root / "Docs/Guide.md").write_text(
                    "dirty worktree overlay\n", encoding="utf-8"
                )
            docs_external_evidence.verify_external_sources(model, roots)

            missing_model = copy.deepcopy(model)
            missing_model["records"][0]["sources"][0]["path"] = "Docs/Missing.md"
            with self.assertRaisesRegex(ValueError, "pinned source is missing"):
                docs_external_evidence.verify_external_sources(missing_model, roots)

    def test_external_verification_rejects_wrong_head(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            roots = {
                "last-frontier": temporary_root / "last-frontier",
                "fonline-tla": temporary_root / "fonline-tla",
            }
            revisions = {
                snapshot: self._create_repository(root, "Docs/Guide.md")
                for snapshot, root in roots.items()
            }
            model = {
                "snapshots": {
                    snapshot: {"revision": revision}
                    for snapshot, revision in revisions.items()
                },
                "records": [],
            }
            model["snapshots"]["fonline-tla"]["revision"] = "0" * 40
            with self.assertRaisesRegex(ValueError, "checkout HEAD must be"):
                docs_external_evidence.verify_external_sources(model, roots)

    def test_ci_checks_generator_and_test(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("BuildTools/tests/test_docs_external_evidence.py", workflow)
        self.assertIn("BuildTools/docs_external_evidence.py --check", workflow)


if __name__ == "__main__":
    unittest.main()
