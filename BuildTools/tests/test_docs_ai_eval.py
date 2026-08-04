from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_ai_eval  # noqa: E402


class DocumentationAiEvaluationTests(unittest.TestCase):
    def _create_fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary_directory = tempfile.TemporaryDirectory()
        root = Path(temporary_directory.name)
        (root / "Docs").mkdir()
        (root / "Source").mkdir()
        (root / "assets").mkdir()
        (root / "Docs/Guide.md").write_text(
            "# Guide\n\nFixture evidence.\n\n## Details\n\nStable contract.\n",
            encoding="utf-8",
        )
        (root / "Source/example.txt").write_text("source\n", encoding="utf-8")
        manifest = {
            "publishing": {
                "production_url": "https://fonline.ru",
                "repository": "cvet/fonline",
            },
            "versioning": {
                "schema_version": docs_ai_eval.docs_ai_delivery.VERSIONING_SCHEMA_VERSION,
                "current": {
                    "channel": "current",
                    "kind": "rolling-branch",
                    "label": "Current",
                    "source_ref": "master",
                    "path_prefix": "",
                    "support": "latest-development-revision",
                },
                "releases": {
                    "status": "deferred",
                    "source": "git-tag",
                    "path_template": "/versions/{version}/",
                    "requires_support_policy": True,
                },
                "history": {"mode": "commit-addressable-ci-artifacts"},
            },
            "localization": {
                "schema_version": docs_ai_eval.docs_ai_delivery.LOCALIZATION_SCHEMA_VERSION,
                "canonical_locale": "en",
                "locales": [
                    {
                        "id": "en",
                        "label": "English",
                        "path_prefix": "Docs/en",
                        "status": "canonical",
                    },
                    {
                        "id": "ru",
                        "label": "Russian",
                        "path_prefix": "Docs/ru",
                        "status": "complete",
                    },
                ],
                "path_strategy": "mirrored-relative-path",
                "translation_hash": "normalized-sha256",
                "translation_pending": "pre-production-only",
                "entrypoint_targets": {},
            },
            "ai_delivery": {
                "schema_version": docs_ai_eval.docs_ai_delivery.SCHEMA_VERSION,
                "canonical_locale": "en",
                "source_ref": "master",
                "llms": {"path": "llms.txt", "start_document_ids": ["guide"]},
                "full_context": {
                    "path": "llms-full.txt",
                    "max_bytes": 65536,
                    "generated_pages": "indexes-only",
                },
                "public_manifest": {"path": "docs-manifest.json"},
            },
            "documents": {
                "Docs/Guide.md": {
                    "id": "guide",
                    "title": "Guide",
                    "audiences": ["ai-agent"],
                    "classification": {
                        "diataxis": "reference",
                        "visibility": "public",
                        "human": True,
                        "translation": "required",
                    },
                    "owner": "documentation",
                    "state": "current",
                    "disposition": "retain",
                    "target": "Docs/en/guide.md",
                    "sources": ["Source/example.txt"],
                }
            },
        }
        (root / docs_ai_eval.DEFAULT_MANIFEST).write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        search = {
            "documents": [
                {
                    "id": "guide",
                    "title": "Guide",
                    "url": "/Docs/Guide.html",
                    "path": "Docs/Guide.md",
                }
            ],
            "terms": {"guide": [[0, 20]]},
        }
        (root / docs_ai_eval.DEFAULT_SEARCH).write_text(
            json.dumps(search) + "\n",
            encoding="utf-8",
        )
        tasks = []
        for category in docs_ai_eval.REQUIRED_CATEGORIES:
            for task_number in range(2):
                tasks.append(
                    {
                        "id": f"{category}-task-{task_number + 1}",
                        "category": category,
                        "question": f"How does the guide cover {category}?",
                        "primary_document_id": "guide",
                        "supporting_document_ids": [],
                        "retrieval_checks": [
                            {
                                "query": "guide",
                                "expected_document_ids": ["guide"],
                                "max_rank": 1,
                            },
                            {
                                "query": "guide absent token",
                                "expected_document_ids": ["guide"],
                                "max_rank": 1,
                            },
                        ],
                        "answer_checks": [
                            {
                                "id": "fixture-evidence",
                                "description": "Use current fixture evidence.",
                                "document_id": "guide",
                                "anchor": "guide",
                                "required_terms": ["Fixture evidence"],
                            },
                            {
                                "id": "stable-contract",
                                "description": "Use the stable contract section.",
                                "document_id": "guide",
                                "anchor": "details",
                                "required_terms": ["Stable contract"],
                            },
                        ],
                        "forbidden_assumptions": [],
                    }
                )
        source = {
            "schema_version": 1,
            "source_ref": "master",
            "minimum_retrieval_success_rate": 1.0,
            "categories": [
                {"id": category, "title": category.title()}
                for category in docs_ai_eval.REQUIRED_CATEGORIES
            ],
            "tasks": tasks,
        }
        (root / docs_ai_eval.DEFAULT_SOURCE).write_text(
            json.dumps(source, indent=2) + "\n",
            encoding="utf-8",
        )
        return temporary_directory, root

    def test_repository_evaluation_is_green(self) -> None:
        report = docs_ai_eval.evaluate(BUILDTOOLS_DIR.parent)

        self.assertEqual(report["error_count"], 0)
        self.assertEqual(report["task_count"], 27)
        self.assertEqual(report["retrieval"]["query_count"], 65)
        self.assertEqual(report["retrieval"]["success_rate"], 1.0)

    def test_fixture_write_check_and_stale_detection(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        self.assertEqual(docs_ai_eval.main(["--root", str(root), "--write"]), 0)
        self.assertEqual(docs_ai_eval.main(["--root", str(root), "--check"]), 0)
        (root / docs_ai_eval.DEFAULT_OUTPUT).write_text("stale\n", encoding="utf-8")
        self.assertEqual(docs_ai_eval.main(["--root", str(root), "--check"]), 1)

    def test_source_ref_and_project_assumptions_are_rejected(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        source_path = root / docs_ai_eval.DEFAULT_SOURCE
        source = json.loads(source_path.read_text(encoding="utf-8"))
        source["source_ref"] = "old"
        source["tasks"][0]["question"] = "What does Last Frontier do?"
        source_path.write_text(json.dumps(source, indent=2) + "\n", encoding="utf-8")

        report = docs_ai_eval.evaluate(root)

        self.assertTrue(
            any("source_ref must match" in error for error in report["errors"])
        )
        self.assertTrue(
            any("embedding-project assumption" in error for error in report["errors"])
        )

    def test_retrieval_regression_fails_the_threshold(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        (root / docs_ai_eval.DEFAULT_SEARCH).write_text(
            json.dumps({"documents": [], "terms": {}}) + "\n",
            encoding="utf-8",
        )

        report = docs_ai_eval.evaluate(root)

        self.assertEqual(report["retrieval"]["success_rate"], 0.0)
        self.assertTrue(
            any("below 1.000" in error for error in report["errors"])
        )


if __name__ == "__main__":
    unittest.main()
