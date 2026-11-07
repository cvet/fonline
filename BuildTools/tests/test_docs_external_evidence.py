from __future__ import annotations

import contextlib
import copy
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


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
        self.assertEqual(self.model["summary"]["snapshot_count"], 3)
        self.assertEqual(self.model["summary"]["record_count"], 38)
        self.assertIn(
            "private-item-policy-and-map-spectators",
            {record["id"] for record in self.model["records"]},
        )
        self.assertEqual(
            set(self.model["summary"]["records_by_disposition"]),
            docs_external_evidence.VALID_DISPOSITIONS,
        )
        self.assertEqual(
            set(self.model["owner_review_requirements"]),
            set(self.model["owners"]),
        )
        self.assertIn("localization", self.model["owners"])

    def test_source_digest_is_independent_of_line_endings(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            lf_path = root / "lf.json"
            crlf_path = root / "crlf.json"
            lf_path.write_bytes(b'{\n  "schema_version": 1\n}\n')
            crlf_path.write_bytes(b'{\r\n  "schema_version": 1\r\n}\r\n')

            self.assertEqual(
                docs_external_evidence._normalized_text_sha256(lf_path),
                docs_external_evidence._normalized_text_sha256(crlf_path),
            )

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
                self._create_repository(root, "Docs/NewerOnly.md")
                (root / "Docs/Guide.md").write_text(
                    "dirty worktree overlay\n", encoding="utf-8"
                )
            docs_external_evidence.verify_external_sources(model, roots)

            missing_model = copy.deepcopy(model)
            missing_model["records"][0]["sources"][0]["path"] = "Docs/Missing.md"
            with self.assertRaisesRegex(ValueError, "pinned source is missing"):
                docs_external_evidence.verify_external_sources(missing_model, roots)

            missing_model["records"][0]["sources"][0]["path"] = "Docs/NewerOnly.md"
            with self.assertRaisesRegex(ValueError, "pinned source is missing"):
                docs_external_evidence.verify_external_sources(missing_model, roots)

            for wrong_roots in (
                {"last-frontier": roots["last-frontier"]},
                {**roots, "extra": roots["last-frontier"]},
            ):
                with self.assertRaisesRegex(ValueError, "must match snapshots exactly"):
                    docs_external_evidence.verify_external_sources(model, wrong_roots)

    def test_external_verification_rejects_missing_pinned_commit(self) -> None:
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
            with self.assertRaises(ValueError) as failure:
                docs_external_evidence.verify_external_sources(model, roots)
            self.assertIn("0" * 40, str(failure.exception))

            model["snapshots"]["fonline-tla"]["revision"] = docs_external_evidence._git_output(
                roots["fonline-tla"], "rev-parse", f"{revisions['fonline-tla']}:Docs/Guide.md"
            )
            with self.assertRaisesRegex(ValueError, "pinned revision must name a commit"):
                docs_external_evidence.verify_external_sources(model, roots)

    def test_cli_verifies_multiple_pins_from_one_checkout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            lf_root = temporary_root / "last-frontier"
            tla_root = temporary_root / "fonline-tla"
            first = self._create_repository(lf_root, "Docs/First.md")
            second = self._create_repository(lf_root, "Docs/Second.md")
            latest = self._create_repository(lf_root, "Docs/HeadOnly.md")
            tla = self._create_repository(tla_root, "Docs/Guide.md")
            dirty_path = lf_root / "Docs/First.md"
            dirty_path.write_text("dirty overlay\n", encoding="utf-8")
            model = {
                "snapshots": {
                    "last-frontier": {"repository": "lastfrontierdev/lf", "revision": first},
                    "last-frontier-map-acceptance": {"repository": "lastfrontierdev/lf", "revision": second},
                    "fonline-tla": {"repository": "cvet/fonline-tla", "revision": tla},
                },
                "records": [{"sources": [
                    {"snapshot": "last-frontier", "path": "Docs/First.md"},
                    {"snapshot": "last-frontier-map-acceptance", "path": "Docs/Second.md"},
                    {"snapshot": "fonline-tla", "path": "Docs/Guide.md"},
                ]}],
            }
            with mock.patch.object(docs_external_evidence, "generate_model", return_value=model), mock.patch.object(
                docs_external_evidence, "_write_or_check", return_value=0
            ), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = docs_external_evidence.main([
                    "--check", "--verify-sources", "--last-frontier-root", str(lf_root), "--tla-root", str(tla_root)
                ])

            self.assertEqual(result, 0)
            self.assertEqual(docs_external_evidence._git_output(lf_root, "rev-parse", "HEAD"), latest)
            self.assertEqual(dirty_path.read_text(encoding="utf-8"), "dirty overlay\n")

    def test_cli_rejects_unsupported_snapshot_repository(self) -> None:
        model = {"snapshots": {"other": {"repository": "unknown/repository", "revision": "0" * 40}}, "records": []}
        errors = io.StringIO()
        with mock.patch.object(docs_external_evidence, "generate_model", return_value=model), mock.patch.object(
            docs_external_evidence, "_write_or_check", return_value=0
        ), contextlib.redirect_stderr(errors):
            result = docs_external_evidence.main([
                "--check", "--verify-sources", "--last-frontier-root", str(ENGINE_ROOT), "--tla-root", str(ENGINE_ROOT)
            ])

        self.assertEqual(result, 1)
        self.assertIn("unsupported external repository", errors.getvalue())

    def test_ci_checks_generator_and_test(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("BuildTools/tests/test_docs_external_evidence.py", workflow)
        self.assertIn("BuildTools/docs_external_evidence.py --check", workflow)


if __name__ == "__main__":
    unittest.main()
