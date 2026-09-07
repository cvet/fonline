from __future__ import annotations

import json
import re
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOLS = ROOT / "BuildTools"
if str(BUILD_TOOLS) not in sys.path:
    sys.path.insert(0, str(BUILD_TOOLS))

import docs_contract_diff  # noqa: E402


MANIFEST_PATH = ROOT / "Docs/documentation-manifest.json"
CONTRACT_GUIDE = ROOT / "Docs/en/contributing/contract-change-management.md"
DECISION_ROOT = ROOT / "Docs/en/contributing/decisions"


class DocumentationGovernanceFoundationsTests(unittest.TestCase):
    def test_contract_guide_covers_every_live_comparator_domain(self) -> None:
        guide = CONTRACT_GUIDE.read_text(encoding="utf-8")

        self.assertEqual(len(docs_contract_diff.DOMAIN_ORDER), 18)
        self.assertEqual(set(docs_contract_diff.DOMAIN_ORDER), set(docs_contract_diff.MODEL_FILES))
        for domain in docs_contract_diff.DOMAIN_ORDER:
            model_name = docs_contract_diff.MODEL_FILES[domain]
            self.assertIn(f"generated/{model_name}", guide)

        for marker in (
            "BuildTools/docs_ai_control_protocol.py --write",
            "`ai-control-protocol`",
            "all eighteen canonical models",
            "github.event.pull_request.base.sha",
            "github.event.before",
        ):
            self.assertIn(marker, guide)

    def test_decisions_match_current_delivery_configuration(self) -> None:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        adr1 = (DECISION_ROOT / "0001-github-pages-markdown-publication.md").read_text(
            encoding="utf-8"
        )
        adr3 = (
            DECISION_ROOT / "0003-manifest-backed-ai-documentation-delivery.md"
        ).read_text(encoding="utf-8")
        adr4 = (
            DECISION_ROOT / "0004-manifest-backed-site-navigation-search.md"
        ).read_text(encoding="utf-8")
        adr6 = (
            DECISION_ROOT / "0006-documentation-version-locale-routing.md"
        ).read_text(encoding="utf-8")

        publishing = manifest["publishing"]
        self.assertEqual(
            (publishing["provider"], publishing["generator"], publishing["content_format"]),
            ("github-pages", "jekyll", "markdown"),
        )
        self.assertEqual(publishing["domain"], "fonline.ru")
        self.assertEqual(
            publishing["source"],
            {
                "status": "verified",
                "build_type": "legacy",
                "branch": "master",
                "folder": "/",
                "verified_on": "2026-08-02",
            },
        )
        self.assertEqual(publishing["dns"]["ownership_verification"], "not-observed")
        self.assertIn("GitHub Pages remains the production publisher", adr1)
        self.assertIn("`CNAME = fonline.ru`", adr1)

        delivery = manifest["ai_delivery"]
        self.assertEqual(delivery["source_ref"], "master")
        self.assertEqual(delivery["full_context"]["max_bytes"], 2 * 1024 * 1024)
        for path in ("llms.txt", "llms-full.txt", "docs-manifest.json"):
            self.assertIn(f"`{path}`", adr3)
        self.assertEqual(
            re.findall(r"^(\d+)\. ", adr3, flags=re.MULTILINE),
            [str(index) for index in range(1, 13)],
        )

        search = manifest["site_delivery"]["search"]
        self.assertEqual(search["max_bytes"], 1_835_008)
        self.assertEqual(
            search["locale_paths"],
            {"en": "assets/docs-search.json", "ru": "assets/docs-search.ru.json"},
        )
        self.assertIn("1.75 MiB (1,835,008 byte)", adr4)
        self.assertIn("authoritative coverage snapshot", adr6)
        self.assertNotIn("first two linked tutorials", adr4)
        self.assertNotIn("Five README-style", adr6)

    def test_canonical_and_legacy_governance_routes_are_owned(self) -> None:
        documents = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))["documents"]
        routes = (
            (
                "Docs/en/contributing/contract-change-management.md",
                "api-change-management",
                "Docs/ApiChangeManagement.md",
                "legacy-api-change-management-route",
            ),
            *tuple(
                (
                    f"Docs/en/contributing/decisions/000{number}-{slug}.md",
                    document_id,
                    f"Docs/Decisions/000{number}-{slug}.md",
                    legacy_id,
                )
                for number, slug, document_id, legacy_id in (
                    (1, "github-pages-markdown-publication", "adr-github-pages-markdown-publication", "legacy-adr-github-pages-markdown-publication-route"),
                    (2, "public-api-stability-contract", "adr-public-api-stability-contract", "legacy-adr-public-api-stability-contract-route"),
                    (3, "manifest-backed-ai-documentation-delivery", "adr-manifest-backed-ai-documentation-delivery", "legacy-adr-manifest-backed-ai-documentation-delivery-route"),
                    (4, "manifest-backed-site-navigation-search", "adr-manifest-backed-site-navigation-search", "legacy-adr-manifest-backed-site-navigation-search-route"),
                    (5, "public-example-repository-ownership", "adr-public-example-repository-ownership", "legacy-adr-public-example-repository-ownership-route"),
                    (6, "documentation-version-locale-routing", "adr-documentation-version-locale-routing", "legacy-adr-documentation-version-locale-routing-route"),
                )
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
            self.assertIn(canonical_path.removeprefix("Docs/"), legacy_text)
            self.assertIn(canonical_path.replace("Docs/en/", "Docs/ru/").removeprefix("Docs/"), legacy_text)

    def test_governance_foundations_are_required_by_ci(self) -> None:
        workflow = (ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")
        self.assertIn(
            "python3 BuildTools/tests/test_docs_governance_foundations.py", workflow
        )


if __name__ == "__main__":
    unittest.main()
