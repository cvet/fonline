from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Docs/documentation-manifest.json"
TRANSLATION_GUIDE = ROOT / "Docs/en/contributing/documentation/translation.md"
SNIPPET_GUIDE = ROOT / "Docs/en/contributing/documentation/snippets.md"
AI_GUIDE = ROOT / "Docs/en/contributing/documentation/ai-evaluation.md"


class DocumentationQualityFoundationsTests(unittest.TestCase):
    def test_translation_contract_is_source_backed(self) -> None:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        localization = manifest["localization"]
        source = (ROOT / "BuildTools/docs_localization.py").read_text(encoding="utf-8")
        guide = TRANSLATION_GUIDE.read_text(encoding="utf-8")

        self.assertEqual(localization["canonical_locale"], "en")
        self.assertEqual(localization["path_strategy"], "mirrored-relative-path")
        self.assertEqual(localization["translation_hash"], "normalized-sha256")
        self.assertEqual(localization["enforcement"], "complete")
        self.assertEqual(
            {entry["id"]: entry["status"] for entry in localization["locales"]},
            {"en": "canonical", "ru": "complete"},
        )
        for marker in (
            'text.replace("\\r\\n", "\\n").replace("\\r", "\\n")',
            "_fenced_bodies(source_text) != _fenced_bodies(russian_text)",
            "_validate_language_preserving_links",
            "enforce_complete",
        ):
            self.assertIn(marker, source)
        for marker in (
            "canonical source and Russian as a whole-document mirror",
            "identical ordered fenced code blocks",
            "language-preserving links",
            "Machine checks protect structure and freshness",
            "physical page parity",
            "Generated model prose",
        ):
            self.assertIn(marker, guide)

        generated = manifest["generated_artifacts"]["description_translation_status"]
        catalog = json.loads(
            (ROOT / generated["source_catalog"]).read_text(encoding="utf-8")
        )
        status = json.loads((ROOT / generated["path"]).read_text(encoding="utf-8"))
        workflow = (ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")
        self.assertEqual(catalog["enforcement"], "complete")
        self.assertEqual(status["summary"]["entry_count"], 4969)
        self.assertEqual(status["summary"]["current_count"], 4969)
        self.assertEqual(status["summary"]["missing_count"], 0)
        self.assertTrue(status["summary"]["complete"])
        for domain in (
            "ai-control-protocol",
            "api",
            "audio",
            "cli",
            "cmake",
            "helper-cli",
            "image-format",
            "map-format",
            "model-format",
            "native-extension",
            "package",
            "prototype-format",
            "public-examples",
            "support-matrix",
            "text-format",
            "video",
        ):
            self.assertTrue(status["domains"][domain]["complete"])
        self.assertIn("BuildTools/tests/test_docs_description_translations.py", workflow)
        self.assertIn(
            "BuildTools/docs_description_translations.py --check --enforce-complete",
            workflow,
        )

    def test_snippet_policy_and_ci_match_the_guide(self) -> None:
        policy = json.loads((ROOT / "BuildTools/SnippetPolicy.json").read_text(encoding="utf-8"))
        workflow = (ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")
        guide = SNIPPET_GUIDE.read_text(encoding="utf-8")
        expected = {
            "angelscript": ("normative", "c-family-parse"),
            "bash": ("normative", "bash-parse"),
            "cmake": ("normative", "cmake-parse"),
            "cpp": ("normative", "c-family-parse"),
            "glsl": ("normative", "c-family-parse"),
            "ini": ("normative", "ini-parse"),
            "json": ("normative", "json-parse"),
            "powershell": ("normative", "powershell-parse"),
            "python": ("normative", "python-parse"),
            "text": ("evidence", "text-contract"),
            "xml": ("normative", "xml-parse"),
        }

        self.assertEqual(
            {name: (entry["contract"], entry["harness"]) for name, entry in policy["languages"].items()},
            expected,
        )
        for language, (_, harness) in expected.items():
            self.assertIn(f"`{language}`", guide)
            self.assertIn(f"`{harness}`", guide)
        self.assertIn("documentation-snippets:", workflow)
        self.assertIn("BuildTools/docs_snippets.py --check --external", workflow)
        self.assertIn("lexical structure, not C++ or AngelScript type", guide)

    def test_ai_evaluation_threshold_counts_and_ranking_are_explicit(self) -> None:
        source = json.loads((ROOT / "Docs/ai-evaluation.json").read_text(encoding="utf-8"))
        tasks = source["tasks"]
        query_count = sum(len(task["retrieval_checks"]) for task in tasks)
        answer_count = sum(len(task["answer_checks"]) for task in tasks)
        category_counts = {
            category["id"]: sum(task["category"] == category["id"] for task in tasks)
            for category in source["categories"]
        }
        guide = AI_GUIDE.read_text(encoding="utf-8")
        python_search = (ROOT / "BuildTools/docs_site.py").read_text(encoding="utf-8")
        browser_search = (ROOT / "assets/js/docs.js").read_text(encoding="utf-8")

        self.assertEqual(source["minimum_retrieval_success_rate"], 1.0)
        self.assertEqual((len(tasks), query_count, answer_count), (27, 65, 92))
        self.assertTrue(all(count >= 2 for count in category_counts.values()))
        self.assertIn("27 tasks, 65 retrieval checks, and 92 answer", guide)
        self.assertIn("threshold is 100 percent", guide)
        self.assertIn("(effective_token_count * 3 + 4) // 5", python_search)
        self.assertIn("prefix_matches == 32", python_search)
        self.assertIn("Math.ceil(effectiveTokenCount * 0.6)", browser_search)
        self.assertIn("prefixMatches < 32", browser_search)

    def test_canonical_and_legacy_routes_are_owned(self) -> None:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))["documents"]
        routes = (
            (
                "Docs/en/contributing/documentation/translation.md",
                "documentation-translation-workflow",
                "Docs/TranslationWorkflow.md",
                "legacy-documentation-translation-workflow-route",
            ),
            (
                "Docs/en/contributing/documentation/snippets.md",
                "documentation-snippet-validation",
                "Docs/SnippetValidation.md",
                "legacy-documentation-snippet-validation-route",
            ),
            (
                "Docs/en/contributing/documentation/ai-evaluation.md",
                "ai-documentation-evaluation",
                "Docs/AiEvaluation.md",
                "legacy-ai-documentation-evaluation-route",
            ),
        )
        for canonical_path, document_id, legacy_path, legacy_id in routes:
            canonical = manifest[canonical_path]
            legacy = manifest[legacy_path]
            self.assertEqual((canonical["id"], canonical["state"], canonical["disposition"]), (document_id, "current", "retain"))
            self.assertEqual((legacy["id"], legacy["state"], legacy["disposition"]), (legacy_id, "redirect", "replace"))
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertFalse(legacy["classification"]["human"])
            self.assertEqual(legacy["classification"]["translation"], "not-required")

        evidence = json.loads((ROOT / "BuildTools/ExternalProjectEvidence.json").read_text(encoding="utf-8"))
        localization = next(item for item in evidence["records"] if item["id"] == "documentation-localization-governance")
        self.assertIn("Docs/en/contributing/documentation/translation.md", localization["engine_targets"])
        self.assertNotIn("Docs/TranslationWorkflow.md", localization["engine_targets"])


if __name__ == "__main__":
    unittest.main()
