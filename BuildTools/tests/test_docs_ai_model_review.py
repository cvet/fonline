from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_ai_model_review  # noqa: E402


class DocumentationAiModelReviewTests(unittest.TestCase):
    def _source(self) -> dict[str, object]:
        return {
            "tasks": [
                {
                    "id": "task",
                    "category": "architecture",
                    "primary_document_id": "guide",
                    "answer_checks": [{"id": "check", "description": "Explain it."}],
                    "forbidden_assumptions": ["Do not invent."],
                }
            ]
        }

    def _run(self) -> dict[str, object]:
        return {
            "schema_version": 2,
            "source_ref": "master",
            "input_hashes": {"source": "0" * 64},
            "model_family": "fixture",
            "model": "fixture:1",
            "model_info": {"digest": "1" * 64},
            "provider": {"id": "fixture", "version": "1.0"},
            "parameters": {"temperature": 0},
            "completed_at": "2026-08-04T00:00:00Z",
            "tasks": [
                {
                    "id": "task",
                    "status": "completed",
                    "observations": {"primary_document_selected": True},
                }
            ],
        }

    def test_template_and_finalization_compute_success(self) -> None:
        review = docs_ai_model_review.build_template(
            self._source(),
            self._run(),
            run_path="Workspace/run.json",
            run_sha256="sha",
            reviewer="reviewer",
        )
        task = review["tasks"][0]
        task["source_selection"]["status"] = "pass"
        task["grounding"]["status"] = "pass"
        task["answer_checks"][0]["status"] = "pass"
        task["forbidden_assumptions"][0]["status"] = "pass"

        finalized = docs_ai_model_review.validate_and_finalize(
            self._source(), review, complete=True
        )

        self.assertTrue(finalized["tasks"][0]["final_task_success"])
        self.assertEqual(finalized["summary"]["final_task_success_rate"], 1.0)

    def test_incomplete_review_is_rejected(self) -> None:
        review = docs_ai_model_review.build_template(
            self._source(),
            self._run(),
            run_path="Workspace/run.json",
            run_sha256="sha",
            reviewer="reviewer",
        )

        with self.assertRaisesRegex(ValueError, "not reviewed"):
            docs_ai_model_review.validate_and_finalize(self._source(), review, complete=True)

    def test_suggestion_populates_review_statuses(self) -> None:
        review = docs_ai_model_review.build_template(
            self._source(),
            self._run(),
            run_path="Workspace/run.json",
            run_sha256="sha",
            reviewer="reviewer",
        )
        suggestion = {
            "tasks": [
                {
                    "id": "task",
                    "source_selection": "pass",
                    "grounding": "fail",
                    "answer_checks": [{"id": "check", "status": "pass"}],
                    "forbidden_assumptions": ["pass"],
                    "notes": "One unsupported command.",
                }
            ]
        }

        populated = docs_ai_model_review.apply_suggestion(review, suggestion)

        self.assertEqual(populated["tasks"][0]["grounding"]["status"], "fail")
        self.assertEqual(populated["tasks"][0]["notes"], "One unsupported command.")

    def test_run_provenance_checks_hash_and_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_path = root / "Workspace" / "run.json"
            run_path.parent.mkdir(parents=True)
            run_path.write_text(json.dumps(self._run()), encoding="utf-8")
            review = docs_ai_model_review.build_template(
                self._source(),
                self._run(),
                run_path="Workspace/run.json",
                run_sha256=docs_ai_model_review._sha256(run_path),
                reviewer="reviewer",
            )

            docs_ai_model_review.validate_run_provenance(
                root, review, require_run=True
            )
            run_path.write_text("{}", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                docs_ai_model_review.validate_run_provenance(
                    root, review, require_run=True
                )

    def test_missing_raw_run_is_optional_or_required(self) -> None:
        review = docs_ai_model_review.build_template(
            self._source(),
            self._run(),
            run_path="Workspace/run.json",
            run_sha256="0" * 64,
            reviewer="reviewer",
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs_ai_model_review.validate_run_provenance(
                root, review, require_run=False
            )
            with self.assertRaisesRegex(ValueError, "run is unavailable"):
                docs_ai_model_review.validate_run_provenance(
                    root, review, require_run=True
                )

    def test_compact_review_requires_exact_model_metadata(self) -> None:
        review = docs_ai_model_review.build_template(
            self._source(),
            self._run(),
            run_path="Workspace/run.json",
            run_sha256="0" * 64,
            reviewer="reviewer",
        )
        review["run"]["provider"] = None
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "exact provider version"):
                docs_ai_model_review.validate_run_provenance(
                    Path(directory), review, require_run=False
                )


if __name__ == "__main__":
    unittest.main()
