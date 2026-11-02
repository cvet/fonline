from __future__ import annotations

import contextlib
import copy
import io
import json
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
        self.assertEqual(self.model["summary"]["snapshot_count"], 4)
        self.assertEqual(self.model["summary"]["record_count"], 33)
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
                (root / "Docs/Guide.md").write_text(
                    "dirty worktree overlay\n", encoding="utf-8"
                )
            docs_external_evidence.verify_external_sources(model, roots)

            missing_model = copy.deepcopy(model)
            missing_model["records"][0]["sources"][0]["path"] = "Docs/Missing.md"
            with self.assertRaisesRegex(ValueError, "pinned source is missing"):
                docs_external_evidence.verify_external_sources(missing_model, roots)

    def test_external_verification_rejects_missing_commit(self) -> None:
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
            with self.assertRaisesRegex(ValueError, "cat-file.*failed"):
                docs_external_evidence.verify_external_sources(model, roots)

    def test_external_verification_reads_multiple_pins_from_one_repository(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            old_revision = self._create_repository(root, "Docs/Guide.md")
            later_path = root / "Docs/Later.md"
            later_path.write_text("later committed source\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(root), "add", "."], check=True)
            subprocess.run(["git", "-C", str(root), "commit", "-q", "-m", "later"], check=True)
            later_revision = subprocess.run(
                ["git", "-C", str(root), "rev-parse", "HEAD"],
                check=True, capture_output=True, text=True, encoding="utf-8",
            ).stdout.strip()
            model = {
                "snapshots": {
                    "old": {"revision": old_revision},
                    "later": {"revision": later_revision},
                },
                "records": [{"sources": [
                    {"snapshot": "old", "path": "Docs/Guide.md"},
                    {"snapshot": "later", "path": "Docs/Later.md"},
                ]}],
            }
            roots = {"old": root, "later": root}
            (root / "Docs/Guide.md").unlink()
            later_path.write_text("dirty overlay\n", encoding="utf-8")
            docs_external_evidence.verify_external_sources(model, roots)

            wrong_pin = copy.deepcopy(model)
            wrong_pin["records"][0]["sources"][0]["path"] = "Docs/Later.md"
            with self.assertRaisesRegex(ValueError, "old pinned source is missing"):
                docs_external_evidence.verify_external_sources(wrong_pin, roots)
            with self.assertRaisesRegex(ValueError, "roots must match snapshots exactly"):
                docs_external_evidence.verify_external_sources(model, {"old": root})

    def test_cli_verifies_every_snapshot_with_existing_repository_roots(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            lf_root = root / "lf"
            tla_root = root / "tla"
            lf_revisions = [self._create_repository(lf_root, "Docs/Guide.md")]
            tla_revision = self._create_repository(tla_root, "Docs/Guide.md")

            for index in range(2):
                (lf_root / "Docs/Guide.md").write_text(f"revision {index}\n", encoding="utf-8")
                subprocess.run(["git", "-C", str(lf_root), "add", "."], check=True)
                subprocess.run(["git", "-C", str(lf_root), "commit", "-q", "-m", f"revision {index}"], check=True)
                lf_revisions.append(subprocess.run(
                    ["git", "-C", str(lf_root), "rev-parse", "HEAD"],
                    check=True, capture_output=True, text=True, encoding="utf-8",
                ).stdout.strip())

            manifest = {
                "schema_version": 1,
                "title": "Source verification fixture",
                "policy": copy.deepcopy(self.model["policy"]),
                "snapshots": {},
                "records": [],
            }
            ids = ("last-frontier", "last-frontier-map-acceptance", "last-frontier-client-path-cut", "fonline-tla")
            for index, snapshot_id in enumerate(ids):
                is_tla = snapshot_id == "fonline-tla"
                manifest["snapshots"][snapshot_id] = {
                    "repository": "cvet/fonline-tla" if is_tla else "lastfrontierdev/lf",
                    "url": "https://example.invalid/repository",
                    "revision": tla_revision if is_tla else lf_revisions[index],
                    "availability": "test-owned",
                    "role": "Pinned source fixture",
                }
                record = copy.deepcopy(self.model["records"][0])
                record.update({
                    "id": f"source-fixture-{index}",
                    "sources": [{"snapshot": snapshot_id, "path": "Docs/Guide.md"}],
                    "disposition": "project-owned",
                    "engine_targets": [],
                    "planned_targets": [],
                    "external_targets": [snapshot_id + ":Docs/Guide.md"],
                })
                manifest["records"].append(record)

            manifest_path = root / docs_external_evidence.DEFAULT_MANIFEST
            manifest_path.parent.mkdir(parents=True)
            policy_path = root / docs_external_evidence.DOCUMENTATION_MANIFEST
            policy_path.parent.mkdir(parents=True)
            policy_path.write_text(json.dumps({
                "owners": self.model["owners"],
                "owner_review_requirements": self.model["owner_review_requirements"],
            }), encoding="utf-8")
            arguments = [
                "--root", str(root), "--check", "--verify-sources",
                "--last-frontier-root", str(lf_root), "--tla-root", str(tla_root),
            ]
            for bad_repository in (False, True):
                with self.subTest(bad_repository=bad_repository):
                    if bad_repository:
                        manifest["snapshots"][ids[1]]["repository"] = "unknown/project"
                    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
                    for relative_path, content in docs_external_evidence.render_outputs(root).items():
                        output = root / relative_path
                        output.parent.mkdir(parents=True, exist_ok=True)
                        output.write_text(content, encoding="utf-8")
                    (lf_root / "Docs/Guide.md").write_text("dirty uncommitted overlay\n", encoding="utf-8")
                    stdout, stderr = io.StringIO(), io.StringIO()
                    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                        result = docs_external_evidence.main(arguments)
                    self.assertEqual(result, 1 if bad_repository else 0)
                    if bad_repository:
                        self.assertIn("unsupported external repository", stderr.getvalue())
                    else:
                        self.assertIn("External project source snapshots are verified", stdout.getvalue())

    def test_ci_checks_generator_and_test(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("BuildTools/tests/test_docs_external_evidence.py", workflow)
        self.assertIn("BuildTools/docs_external_evidence.py --check", workflow)


if __name__ == "__main__":
    unittest.main()
