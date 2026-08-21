from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_ai_delivery  # noqa: E402
import docs_localization  # noqa: E402
import docs_site  # noqa: E402


class DocumentationSiteTests(unittest.TestCase):
    def test_repository_search_uses_reviewed_budget(self) -> None:
        root = BUILDTOOLS_DIR.parent
        manifest = json.loads((root / docs_site.DEFAULT_MANIFEST).read_text(encoding="utf-8"))
        max_bytes = manifest["site_delivery"]["search"]["max_bytes"]
        outputs = docs_site.render_outputs(root)
        output_bytes = len(outputs[docs_site.DEFAULT_SEARCH_OUTPUT].encode("utf-8"))
        russian_output_bytes = len(
            outputs[docs_site.DEFAULT_RUSSIAN_SEARCH_OUTPUT].encode("utf-8")
        )

        self.assertEqual(max_bytes, 1_835_008)
        self.assertLessEqual(output_bytes, max_bytes)
        self.assertLessEqual(russian_output_bytes, max_bytes)

    def _document(
        self,
        document_id: str,
        title: str,
        *,
        visibility: str = "public",
        state: str = "current",
        human: bool = True,
        diataxis: str = "reference",
    ) -> dict[str, object]:
        return {
            "id": document_id,
            "title": title,
            "audiences": ["game-developer"],
            "classification": {
                "diataxis": diataxis,
                "visibility": visibility,
                "human": human,
                "translation": "required" if human else "not-required",
            },
            "owner": "documentation",
            "state": state,
            "disposition": "retain",
            "target": "Docs/en/" + document_id + ".md",
            "sources": ["Source/example.txt"],
        }

    def _create_fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary_directory = tempfile.TemporaryDirectory()
        root = Path(temporary_directory.name)
        (root / "Docs/generated/api").mkdir(parents=True)
        (root / "Source").mkdir()
        (root / "Source/example.txt").write_text("fixture source\n", encoding="utf-8")
        (root / "Docs/translation-glossary.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "source_locale": "en",
                    "target_locale": "ru",
                    "terms": [
                        {
                            "term": "guide",
                            "russian": "руководство",
                            "policy": "translate",
                            "note": "Fixture terminology.",
                        }
                    ],
                },
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )
        files = {
            "README.md": "# FOnline\n\nBuild multiplayer games.\n",
            "Docs/Guide.md": "# Guide\n\nUse `Game.Sync` before 1048576 shared records.\n\n## Synchronization\n",
            "Docs/generated/api/index.md": "# API index\n\nBrowse native declarations.\n",
            "Docs/generated/api/methods.md": "# Methods\n\n## StartParallelTestSuite\n\nRuns tests.\n",
            "Docs/Internal.md": "# Internal\n\nPrivate planning.\n",
            "AGENTS.md": "# Agents\n\nAI route.\n",
        }
        for relative_path, content in files.items():
            path = root / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")

        manifest = {
            "schema_version": 1,
            "publishing": {
                "title": "FOnline Engine",
                "site_description": "Documentation for building games with FOnline.",
                "production_url": "https://fonline.ru",
                "repository": "cvet/fonline",
            },
            "versioning": {
                "schema_version": docs_ai_delivery.VERSIONING_SCHEMA_VERSION,
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
                "schema_version": docs_ai_delivery.LOCALIZATION_SCHEMA_VERSION,
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
                "glossary": "Docs/translation-glossary.json",
                "status_output": "Docs/generated/translation-status.json",
                "enforcement": "existing-translations-current",
                "entrypoint_targets": {},
            },
            "ai_delivery": {
                "schema_version": docs_ai_delivery.SCHEMA_VERSION,
                "canonical_locale": "en",
                "source_ref": "master",
                "llms": {
                    "path": docs_ai_delivery.DEFAULT_LLMS_OUTPUT,
                    "start_document_ids": ["repository-home"],
                },
                "full_context": {
                    "path": docs_ai_delivery.DEFAULT_FULL_CONTEXT_OUTPUT,
                    "max_bytes": 65536,
                    "generated_pages": "indexes-only",
                },
                "public_manifest": {"path": docs_ai_delivery.DEFAULT_PUBLIC_MANIFEST_OUTPUT},
            },
            "site_delivery": {
                "schema_version": docs_site.SCHEMA_VERSION,
                "layout": "default",
                "navigation_data_path": docs_site.DEFAULT_NAVIGATION_OUTPUT,
                "search": {
                    "path": docs_site.DEFAULT_SEARCH_OUTPUT,
                    "locale_paths": {
                        "en": docs_site.DEFAULT_SEARCH_OUTPUT,
                        "ru": docs_site.DEFAULT_RUSSIAN_SEARCH_OUTPUT,
                    },
                    "max_bytes": 65536,
                    "minimum_query_length": 2,
                },
                "routing": {
                    "path": docs_site.DEFAULT_ROUTES_OUTPUT,
                    "current_permalink_strategy": "source-markdown-path",
                    "planned_permalink_strategy": "manifest-target-path",
                    "legacy_route_strategy": "durable-markdown-pointer",
                    "migration_status": "planned",
                },
                "navigation": [
                    {
                        "id": "start",
                        "title": "Start",
                        "title_ru": "Начало",
                        "document_ids": ["repository-home", "guide", "generated-api-index"],
                    }
                ],
            },
            "documents": {
                "README.md": self._document("repository-home", "FOnline", diataxis="none"),
                "Docs/Guide.md": self._document("guide", "Guide", diataxis="how-to"),
                "Docs/generated/api/index.md": self._document("generated-api-index", "API index"),
                "Docs/generated/api/methods.md": self._document("generated-api-methods", "Methods"),
                "Docs/Internal.md": self._document("internal", "Internal", visibility="internal"),
                "AGENTS.md": self._document("agents", "Agents", human=False),
            },
        }
        (root / docs_site.DEFAULT_MANIFEST).write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        return temporary_directory, root

    def test_navigation_uses_manifest_order_and_top_level_coverage(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        outputs = docs_site.render_outputs(root)
        navigation = json.loads(outputs[docs_site.DEFAULT_NAVIGATION_OUTPUT])

        self.assertEqual(navigation["source_ref"], "master")
        self.assertEqual(navigation["version"]["channel"], "current")
        self.assertEqual(navigation["version"]["value"], "master")
        self.assertEqual(navigation["canonical_locale"], "en")
        self.assertEqual(navigation["locale_pair_count"], 0)
        self.assertEqual(navigation["routes_path"], docs_site.DEFAULT_ROUTES_OUTPUT)
        self.assertEqual(navigation["navigation_item_count"], 3)
        self.assertEqual(
            [item["id"] for item in navigation["navigation"][0]["items"]],
            ["repository-home", "guide", "generated-api-index"],
        )
        self.assertEqual(navigation["navigation"][0]["items"][0]["url"], "/")
        self.assertEqual(navigation["navigation"][0]["items"][1]["url"], "/Docs/Guide.html")
        self.assertEqual(
            navigation["navigation"][0]["items"][2]["url"],
            "/Docs/generated/api/",
        )

    def test_route_catalog_freezes_current_planned_and_locale_paths(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        routes = json.loads(docs_site.render_outputs(root)[docs_site.DEFAULT_ROUTES_OUTPUT])
        routes_by_id = {route["id"]: route for route in routes["routes"]}
        guide = routes_by_id["guide"]
        generated_api_index = routes_by_id["generated-api-index"]

        self.assertEqual(routes["version"]["kind"], "rolling-branch")
        self.assertEqual(routes["release_versions"]["status"], "deferred")
        self.assertEqual(routes["localization"]["canonical_locale"], "en")
        self.assertEqual(guide["current_path"], "/Docs/Guide.html")
        self.assertEqual(guide["planned_path"], "/Docs/en/guide.html")
        self.assertEqual(generated_api_index["current_path"], "/Docs/generated/api/")
        self.assertEqual(
            generated_api_index["planned_path"],
            "/Docs/en/generated-api-index.html",
        )
        self.assertTrue(guide["redirect_required"])
        self.assertEqual(
            {route["locale"]: route["path"] for route in guide["locale_routes"]},
            {
                "en": "/Docs/en/guide.html",
                "ru": "/Docs/ru/guide.html",
            },
        )
        self.assertIn(
            {
                "document_id": "guide",
                "canonical_document_id": "guide",
                "from": "/Docs/Guide.html",
                "to": "/Docs/en/guide.html",
                "strategy": "durable-markdown-pointer",
                "status": "required-before-route-migration",
            },
            routes["legacy_redirects"],
        )

    def test_route_catalog_allows_replace_aliases_with_one_canonical_owner(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Alias.md").write_text(
            "> Legacy route.\n\n# Alias\n",
            encoding="utf-8",
        )
        manifest_path = root / docs_site.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        alias = self._document(
            "guide-alias",
            "Guide alias",
            state="redirect",
            human=False,
        )
        alias["disposition"] = "replace"
        alias["target"] = "Docs/en/guide.md"
        alias["redirect_to"] = "guide"
        manifest["documents"]["Docs/Alias.md"] = alias
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        routes = json.loads(docs_site.render_outputs(root)[docs_site.DEFAULT_ROUTES_OUTPUT])
        routes_by_id = {route["id"]: route for route in routes["routes"]}
        self.assertEqual(routes_by_id["guide-alias"]["canonical_document_id"], "guide")
        self.assertEqual(routes_by_id["guide-alias"]["planned_path"], "/Docs/en/guide.html")

        manifest["documents"]["Docs/Alias.md"]["redirect_to"] = "repository-home"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "must name canonical owner guide"):
            docs_site.render_outputs(root)

        manifest["documents"]["Docs/Alias.md"]["redirect_to"] = "guide"
        manifest["documents"]["Docs/Guide.md"]["disposition"] = "replace"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "exactly one non-replace owner"):
            docs_site.render_outputs(root)

    def test_translation_scoped_entrypoint_requires_explicit_locale_targets(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_site.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["documents"]["Docs/Guide.md"]["target"] = "Docs/Guide.md"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        with self.assertRaisesRegex(
            ValueError,
            "must target Docs/en/ or declare explicit entrypoint targets",
        ):
            docs_site.render_outputs(root)

    def test_search_indexes_public_human_details_and_technical_tokens(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        search = json.loads(docs_site.render_outputs(root)[docs_site.DEFAULT_SEARCH_OUTPUT])
        document_ids = [document["id"] for document in search["documents"]]

        self.assertIn("generated-api-methods", document_ids)
        self.assertNotIn("internal", document_ids)
        self.assertNotIn("agents", document_ids)
        self.assertIn("game.sync", search["terms"])
        self.assertIn("startparalleltestsuite", search["terms"])
        self.assertIn("parallel", search["terms"])
        self.assertNotIn("1048576", search["terms"])

        long_query_results = docs_site.search_documents(
            search,
            "Game.Sync unrelated absent tokens",
        )
        self.assertEqual(long_query_results[0]["document"]["id"], "guide")
        prefix_results = docs_site.search_documents(search, "startparalleltest")
        self.assertEqual(
            prefix_results[0]["document"]["id"],
            "generated-api-methods",
        )
        russian_search = json.loads(
            docs_site.render_outputs(root)[docs_site.DEFAULT_RUSSIAN_SEARCH_OUTPUT]
        )
        self.assertEqual(russian_search["locale"], "ru")
        self.assertEqual(russian_search["document_count"], 0)

    def test_current_russian_translation_drives_navigation_and_search(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        source_path = root / "Docs/Guide.md"
        source_hash = docs_localization.normalized_sha256(
            source_path.read_text(encoding="utf-8")
        )
        russian_path = root / "Docs/ru/guide.md"
        russian_path.parent.mkdir(parents=True)
        russian_path.write_text(
            docs_localization.translation_metadata_line(
                "guide", "Docs/Guide.md", source_hash
            )
            + "\n\n# Руководство\n\nИспользуйте `Game.Sync` для синхронизации.\n",
            encoding="utf-8",
        )

        outputs = docs_site.render_outputs(root)
        navigation = json.loads(outputs[docs_site.DEFAULT_NAVIGATION_OUTPUT])
        guide = navigation["navigation"][0]["items"][1]
        self.assertEqual(navigation["locale_pair_count"], 1)
        self.assertEqual(guide["locales"]["en"]["path"], "Docs/Guide.md")
        self.assertEqual(guide["locales"]["ru"]["title"], "Руководство")
        self.assertEqual(guide["locales"]["ru"]["url"], "/Docs/ru/guide.html")

        russian_rendered = outputs[docs_site.DEFAULT_RUSSIAN_SEARCH_OUTPUT]
        self.assertIn("Руководство", russian_rendered)
        self.assertNotIn("\\u0420", russian_rendered)
        russian_search = json.loads(russian_rendered)
        results = docs_site.search_documents(russian_search, "синхронизации")
        self.assertEqual(russian_search["document_count"], 1)
        self.assertEqual(results[0]["document"]["id"], "guide")
        self.assertEqual(results[0]["document"]["locale"], "ru")

    def test_search_omits_terms_present_in_most_documents(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        manifest = json.loads((root / docs_site.DEFAULT_MANIFEST).read_text(encoding="utf-8"))
        for relative_path, document in manifest["documents"].items():
            classification = document["classification"]
            if (
                classification["visibility"] == "public"
                and classification["human"]
                and document["state"] == "current"
            ):
                path = root / relative_path
                path.write_text(path.read_text(encoding="utf-8") + "\nCorpusWideMarker\n", encoding="utf-8")

        search = json.loads(docs_site.render_outputs(root)[docs_site.DEFAULT_SEARCH_OUTPUT])

        self.assertEqual(search["maximum_document_frequency_ratio"], 0.60)
        self.assertNotIn("corpuswidemarker", search["terms"])

    def test_navigation_rejects_duplicate_unknown_and_missing_documents(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_site.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

        manifest["site_delivery"]["navigation"].append(
            {
                "id": "other",
                "title": "Other",
                "title_ru": "Другое",
                "document_ids": ["guide"],
            }
        )
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "repeats navigation document id: guide"):
            docs_site.render_outputs(root)

        manifest["site_delivery"]["navigation"] = [
            {
                "id": "start",
                "title": "Start",
                "title_ru": "Начало",
                "document_ids": ["repository-home", "missing"],
            }
        ]
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "not public current human top-level documentation: missing"):
            docs_site.render_outputs(root)

        manifest["site_delivery"]["navigation"][0]["document_ids"] = ["repository-home"]
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "navigation omits .*guide"):
            docs_site.render_outputs(root)

    def test_outputs_are_deterministic_and_search_budget_is_enforced(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        first = docs_site.render_outputs(root)
        self.assertEqual(first, docs_site.render_outputs(root))
        self.assertTrue(all(content.endswith("\n") for content in first.values()))

        manifest_path = root / docs_site.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["site_delivery"]["search"]["max_bytes"] = 100
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "search index is .* limit is 100"):
            docs_site.render_outputs(root)

    def test_write_and_check_modes_detect_stale_output(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        self.assertEqual(docs_site.main(["--root", str(root), "--write"]), 0)
        self.assertEqual(docs_site.main(["--root", str(root), "--check"]), 0)
        (root / docs_site.DEFAULT_SEARCH_OUTPUT).write_text("stale\n", encoding="utf-8")
        self.assertEqual(docs_site.main(["--root", str(root), "--check"]), 1)


if __name__ == "__main__":
    unittest.main()
