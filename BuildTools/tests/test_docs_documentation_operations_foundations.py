from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "Docs/documentation-manifest.json"
MAINTENANCE_GUIDE = ROOT / "Docs/en/contributing/documentation/index.md"
PUBLICATION_GUIDE = ROOT / "Docs/en/contributing/documentation/site-publication.md"


class DocumentationOperationsFoundationsTests(unittest.TestCase):
    def test_internal_plans_are_meta_owned_and_excluded_from_publication(self) -> None:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        documents = manifest["documents"]
        config = (ROOT / "_config.yml").read_text(encoding="utf-8")
        names = (
            "DocumentationBacklog.md",
            "DocumentationExpansionPlan.md",
            "DocumentationResearchTemplate.md",
            "DocumentationVerificationReport.md",
        )

        for name in names:
            old_path = f"Docs/{name}"
            canonical_path = f"Docs/_meta/{name}"
            self.assertTrue((ROOT / old_path).is_file())
            self.assertTrue((ROOT / canonical_path).is_file())
            self.assertEqual(documents[old_path]["target"], canonical_path)
            self.assertEqual(documents[old_path]["disposition"], "replace")
            self.assertFalse(documents[old_path]["classification"]["human"])
            self.assertEqual(documents[canonical_path]["target"], canonical_path)
            self.assertEqual(documents[canonical_path]["disposition"], "retain")
            self.assertTrue(documents[canonical_path]["classification"]["human"])
            self.assertIn(f"](_meta/{name})", (ROOT / old_path).read_text(encoding="utf-8"))
            self.assertIn(f"  - {old_path}", config)

        self.assertIn("  - Docs/_meta/", config)
        self.assertEqual(
            manifest["generated_artifacts"]["external_project_evidence"]["visibility"],
            "internal",
        )

    def test_maintenance_and_revision_reconciliation_are_source_backed(self) -> None:
        guide = MAINTENANCE_GUIDE.read_text(encoding="utf-8")
        agents = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
        evidence = json.loads(
            (ROOT / "BuildTools/ExternalProjectEvidence.json").read_text(encoding="utf-8")
        )
        update_record = next(
            record for record in evidence["records"] if record["id"] == "engine-update-reconciliation"
        )

        for marker in (
            "Every incoming commit is a candidate contract change",
            "Record the current engine SHA and target branch/ref",
            "compare all eighteen runtime domains",
            'python -m unittest discover -s BuildTools/tests -p "test_docs_*.py"',
            "authoritative CI expansion",
        ):
            self.assertIn(marker, guide)
        self.assertIn("Docs/en/contributing/documentation/index.md#revision-update-reconciliation", agents)
        self.assertIn("Docs/en/contributing/documentation/index.md", update_record["engine_targets"])
        self.assertEqual(
            {source["snapshot"] for source in update_record["sources"]},
            {"last-frontier", "fonline-tla"},
        )

    def test_publication_contract_matches_repository_configuration(self) -> None:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        publishing = manifest["publishing"]
        workflow = (ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")
        browser_package = json.loads(
            (ROOT / "BuildTools/docs-browser/package.json").read_text(encoding="utf-8")
        )
        guide = PUBLICATION_GUIDE.read_text(encoding="utf-8")

        self.assertEqual(
            (publishing["provider"], publishing["generator"], publishing["content_format"]),
            ("github-pages", "jekyll", "markdown"),
        )
        self.assertEqual(publishing["production_url"], "https://fonline.ru")
        self.assertEqual((ROOT / "CNAME").read_text(encoding="utf-8").strip(), "fonline.ru")
        self.assertEqual((ROOT / ".ruby-version").read_text(encoding="utf-8").strip(), "3.3.4")
        self.assertIn('gem "github-pages", "= 232"', (ROOT / "Gemfile").read_text(encoding="utf-8"))
        self.assertEqual(browser_package["dependencies"]["playwright"], "1.62.0")
        self.assertEqual(browser_package["dependencies"]["axe-core"], "4.12.1")
        self.assertIn("actions/jekyll-build-pages@v1", workflow)
        self.assertIn("node-version: 24.16.0", workflow)
        for marker in (
            "Versioned Markdown in this repository",
            "all 197 required counterparts",
            "three manifest-owned profiles",
            "`zoom-200-russian-documentation.png`",
            "source mode `legacy`, branch `master`, and folder `/`",
        ):
            self.assertIn(marker, guide)

    def test_generator_and_ci_dependency_order_is_explicit(self) -> None:
        workflow = (ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")
        guide = MAINTENANCE_GUIDE.read_text(encoding="utf-8")
        commands = (
            "python3 BuildTools/docs_snippets.py --check",
            "python3 BuildTools/docs_site.py --check",
            "python3 BuildTools/docs_ai_eval.py --check",
            "python3 BuildTools/docs_ai_delivery.py --check",
        )

        positions = [workflow.index(command) for command in commands]
        self.assertEqual(positions, sorted(positions))
        for marker in (
            "Regenerate translation status after these source assets",
            "Then regenerate `_data/docs-site.json`",
            "Regenerate `Docs/generated/ai-evaluation-report.json`",
            "Finally regenerate `llms.txt`",
        ):
            self.assertIn(marker, guide)

    def test_canonical_and_legacy_routes_are_owned(self) -> None:
        documents = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))["documents"]
        routes = (
            (
                "Docs/en/contributing/documentation/index.md",
                "documentation-maintenance",
                "Docs/DocumentationMaintenance.md",
                "legacy-documentation-maintenance-route",
            ),
            (
                "Docs/en/contributing/documentation/site-publication.md",
                "documentation-site-publication",
                "Docs/SitePublication.md",
                "legacy-documentation-site-publication-route",
            ),
        )
        for canonical_path, document_id, legacy_path, legacy_id in routes:
            canonical = documents[canonical_path]
            legacy = documents[legacy_path]
            self.assertEqual(
                (canonical["id"], canonical["state"], canonical["disposition"]),
                (document_id, "current", "retain"),
            )
            self.assertEqual(
                (legacy["id"], legacy["state"], legacy["disposition"]),
                (legacy_id, "redirect", "replace"),
            )
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertFalse(legacy["classification"]["human"])
            self.assertEqual(legacy["classification"]["translation"], "not-required")

            legacy_text = (ROOT / legacy_path).read_text(encoding="utf-8")
            self.assertIn("> Legacy route.", legacy_text)
            english_path = canonical_path.removeprefix("Docs/").removesuffix("index.md")
            self.assertIn(english_path, legacy_text)
            russian_path = canonical_path.replace("Docs/en/", "Docs/ru/").removeprefix("Docs/")
            self.assertIn(russian_path.removesuffix("index.md"), legacy_text)


if __name__ == "__main__":
    unittest.main()
