from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_ai_delivery  # noqa: E402


class DocumentationAiDeliveryTests(unittest.TestCase):
    def test_repository_full_context_uses_reviewed_budget(self) -> None:
        root = BUILDTOOLS_DIR.parent
        manifest = json.loads(
            (root / docs_ai_delivery.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        max_bytes = manifest["ai_delivery"]["full_context"]["max_bytes"]
        outputs = docs_ai_delivery.render_outputs(root)
        output_bytes = len(
            outputs[docs_ai_delivery.DEFAULT_FULL_CONTEXT_OUTPUT].encode("utf-8")
        )

        self.assertEqual(max_bytes, 2_097_152)
        self.assertLessEqual(output_bytes, max_bytes)

    def _document(
        self,
        document_id: str,
        title: str,
        *,
        visibility: str = "public",
        state: str = "current",
        diataxis: str = "reference",
        human: bool = True,
    ) -> dict[str, object]:
        return {
            "id": document_id,
            "title": title,
            "audiences": ["ai-agent"],
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
        (root / "Docs/en/reference/api").mkdir(parents=True)
        (root / "Source").mkdir()
        (root / "Source/example.txt").write_text("source\n", encoding="utf-8")
        files = {
            "README.md": "# Engine\n\nRepository entry.\n",
            "AGENTS.md": "# Agents\n\nMaintainer route.\n",
            "Docs/Guide.md": "# Guide\n\nUse the engine.\n",
            "Docs/generated/api/index.md": "# API index\n\nReference route.\n",
            "Docs/generated/api/methods.md": "# API methods\n\nLarge detail.\n",
            "Docs/en/reference/api/types.md": (
                "---\n"
                "title: API types\n"
                "document_id: generated-api-types\n"
                "locale: en\n"
                "generated: true\n"
                "---\n\n"
                "# API types\n\nLocalized generated detail.\n"
            ),
            "Docs/Internal.md": "# Internal\n\nPrivate plan.\n",
            "Docs/generated/internal.json": '{"private":true}\n',
            "PUBLIC_API.md": "> Placeholder route.\n\n# Public API\n",
        }
        for relative_path, content in files.items():
            path = root / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        (root / "Docs/generated/api.json").write_text(
            json.dumps({"schema_version": 1}) + "\n",
            encoding="utf-8",
        )
        (root / "_data").mkdir()
        (root / "assets").mkdir()
        (root / "_data/docs-site.json").write_text('{"navigation":[]}\n', encoding="utf-8")
        (root / "assets/docs-search.json").write_text('{"documents":[]}\n', encoding="utf-8")
        (root / "Docs/generated/document-routes.json").write_text(
            '{"routes":[]}\n',
            encoding="utf-8",
        )

        manifest = {
            "schema_version": 1,
            "description": "Fixture documentation manifest.",
            "publishing": {
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
                "entrypoint_targets": {
                    "repository-home": {
                        "en": "README.md",
                        "ru": "README.ru.md",
                    }
                },
            },
            "ai_delivery": {
                "schema_version": docs_ai_delivery.SCHEMA_VERSION,
                "canonical_locale": "en",
                "source_ref": "master",
                "llms": {
                    "path": docs_ai_delivery.DEFAULT_LLMS_OUTPUT,
                    "start_document_ids": ["repository-home", "documentation-guide"],
                },
                "full_context": {
                    "path": docs_ai_delivery.DEFAULT_FULL_CONTEXT_OUTPUT,
                    "max_bytes": 65536,
                    "generated_pages": "indexes-only",
                },
                "public_manifest": {
                    "path": docs_ai_delivery.DEFAULT_PUBLIC_MANIFEST_OUTPUT,
                },
            },
            "generated_artifacts": {
                "api_model": {
                    "path": "Docs/generated/api.json",
                    "generator": "fixture",
                },
                "internal_model": {
                    "visibility": "internal",
                    "model": "Docs/generated/internal.json",
                    "generator": "fixture",
                },
                "ai_delivery": {
                    "source_manifest": docs_ai_delivery.DEFAULT_MANIFEST,
                    "generator": docs_ai_delivery.GENERATED_BY,
                    "schema_version": docs_ai_delivery.SCHEMA_VERSION,
                    "paths": list(docs_ai_delivery.OUTPUT_PATHS),
                },
                "site_delivery": {
                    "source_manifest": docs_ai_delivery.DEFAULT_MANIFEST,
                    "generator": "BuildTools/docs_site.py",
                    "schema_version": 2,
                    "paths": [
                        "_data/docs-site.json",
                        "assets/docs-search.json",
                        "Docs/generated/document-routes.json",
                    ],
                },
            },
            "owners": {"documentation": "Documentation owner."},
            "documents": {
                "README.md": self._document("repository-home", "Engine", diataxis="none"),
                "AGENTS.md": self._document(
                    "ai-maintainer-entry",
                    "Agents",
                    diataxis="none",
                    human=False,
                ),
                "Docs/Guide.md": self._document(
                    "documentation-guide",
                    "Guide",
                    diataxis="how-to",
                ),
                "Docs/generated/api/index.md": self._document(
                    "generated-api-index",
                    "API index",
                ),
                "Docs/generated/api/methods.md": self._document(
                    "generated-api-methods",
                    "API methods",
                ),
                "Docs/en/reference/api/types.md": self._document(
                    "generated-api-types",
                    "API types",
                ),
                "Docs/Internal.md": self._document(
                    "internal-plan",
                    "Internal",
                    visibility="internal",
                ),
                "PUBLIC_API.md": self._document(
                    "legacy-public-api",
                    "Public API",
                    state="placeholder",
                ),
            },
        }
        (root / docs_ai_delivery.DEFAULT_MANIFEST).write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        return temporary_directory, root

    def test_outputs_share_manifest_ownership_and_filter_context(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        outputs = docs_ai_delivery.render_outputs(root)
        llms = outputs[docs_ai_delivery.DEFAULT_LLMS_OUTPUT]
        full_context = outputs[docs_ai_delivery.DEFAULT_FULL_CONTEXT_OUTPUT]
        public_manifest = json.loads(outputs[docs_ai_delivery.DEFAULT_PUBLIC_MANIFEST_OUTPUT])

        self.assertLess(llms.index("repository-home"), llms.index("documentation-guide"))
        self.assertIn("generated-api-methods", llms)
        self.assertNotIn("internal-plan", llms)
        self.assertNotIn("legacy-public-api", llms)

        self.assertIn("BEGIN DOCUMENT repository-home", full_context)
        self.assertIn("BEGIN DOCUMENT generated-api-index", full_context)
        self.assertNotIn("BEGIN DOCUMENT generated-api-methods", full_context)
        self.assertNotIn("BEGIN DOCUMENT generated-api-types", full_context)
        self.assertNotIn("BEGIN DOCUMENT internal-plan", full_context)
        self.assertNotIn("BEGIN DOCUMENT legacy-public-api", full_context)

        documents = {document["id"]: document for document in public_manifest["documents"]}
        artifacts = {artifact["id"]: artifact for artifact in public_manifest["artifacts"]}
        self.assertIn("legacy-public-api", documents)
        self.assertNotIn("internal-plan", documents)
        guide = documents["documentation-guide"]
        guide_text = (root / "Docs/Guide.md").read_text(encoding="utf-8").replace("\r\n", "\n")
        expected_hash = hashlib.sha256(guide_text.encode("utf-8")).hexdigest()
        self.assertEqual(guide["content_sha256"], expected_hash)
        self.assertEqual(guide["canonical_url"], "https://fonline.ru/Docs/Guide.html")
        self.assertEqual(
            guide["markdown_url"],
            "https://raw.githubusercontent.com/cvet/fonline/master/Docs/Guide.md",
        )
        self.assertIn(guide["markdown_url"], llms)
        self.assertIn(f"[HTML]({guide['canonical_url']})", llms)
        generated_index = documents["generated-api-index"]
        self.assertEqual(
            generated_index["canonical_url"],
            "https://fonline.ru/Docs/generated/api/",
        )
        self.assertEqual(guide["locale"], "en")
        self.assertEqual(public_manifest["version"]["channel"], "current")
        self.assertEqual(public_manifest["version"]["source_ref"], "master")
        self.assertEqual(public_manifest["release_versions"]["status"], "deferred")
        self.assertEqual(public_manifest["localization"]["canonical_locale"], "en")
        self.assertEqual(public_manifest["full_context"]["excluded_document_ids"], [])
        self.assertIn("site-docs-site", artifacts)
        self.assertIn("site-docs-search", artifacts)
        self.assertIn("site-document-routes", artifacts)
        self.assertNotIn("internal_model", artifacts)
        self.assertNotIn("Docs/generated/internal.json", llms)
        self.assertIn("assets/docs-search.json", llms)
        self.assertIn("Docs/generated/document-routes.json", llms)

    def test_outputs_are_byte_deterministic(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        first = docs_ai_delivery.render_outputs(root)
        second = docs_ai_delivery.render_outputs(root)

        self.assertEqual(first, second)
        self.assertTrue(all(content.endswith("\n") for content in first.values()))

    def test_full_context_budget_is_enforced(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_ai_delivery.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["ai_delivery"]["full_context"]["max_bytes"] = 100
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        with self.assertRaisesRegex(ValueError, "full-context bundle is .* limit is 100"):
            docs_ai_delivery.render_outputs(root)

    def test_reviewed_full_context_exclusion_retains_discovery_routes(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_ai_delivery.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["ai_delivery"]["full_context"]["exclude_document_ids"] = [
            "generated-api-index"
        ]
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        outputs = docs_ai_delivery.render_outputs(root)
        full_context = outputs[docs_ai_delivery.DEFAULT_FULL_CONTEXT_OUTPUT]
        llms = outputs[docs_ai_delivery.DEFAULT_LLMS_OUTPUT]
        public_manifest = json.loads(
            outputs[docs_ai_delivery.DEFAULT_PUBLIC_MANIFEST_OUTPUT]
        )

        self.assertNotIn("BEGIN DOCUMENT generated-api-index", full_context)
        self.assertIn("generated-api-index", llms)
        self.assertIn(
            "generated-api-index",
            {document["id"] for document in public_manifest["documents"]},
        )
        self.assertEqual(
            public_manifest["full_context"]["excluded_document_ids"],
            ["generated-api-index"],
        )

    def test_unknown_or_start_document_exclusion_is_rejected(self) -> None:
        for excluded_id, expected in (
            ("missing-guide", "not bundle-eligible"),
            ("repository-home", "cannot exclude llms start documents"),
        ):
            with self.subTest(excluded_id=excluded_id):
                temporary_directory, root = self._create_fixture()
                try:
                    manifest_path = root / docs_ai_delivery.DEFAULT_MANIFEST
                    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                    manifest["ai_delivery"]["full_context"]["exclude_document_ids"] = [
                        excluded_id
                    ]
                    manifest_path.write_text(
                        json.dumps(manifest, indent=2) + "\n",
                        encoding="utf-8",
                    )
                    with self.assertRaisesRegex(ValueError, expected):
                        docs_ai_delivery.render_outputs(root)
                finally:
                    temporary_directory.cleanup()

    def test_unknown_start_document_is_rejected(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_ai_delivery.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["ai_delivery"]["llms"]["start_document_ids"].append("missing-guide")
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        with self.assertRaisesRegex(ValueError, "missing-guide"):
            docs_ai_delivery.render_outputs(root)

    def test_version_source_ref_drift_is_rejected(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_ai_delivery.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["versioning"]["current"]["source_ref"] = "stable"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        with self.assertRaisesRegex(
            ValueError,
            "ai_delivery source_ref must match documentation versioning current",
        ):
            docs_ai_delivery.render_outputs(root)

    def test_locale_contract_drift_is_rejected(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_ai_delivery.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["localization"]["locales"][1]["path_prefix"] = "Docs/RU"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        with self.assertRaisesRegex(
            ValueError,
            "documentation localization locale 1 path_prefix must be Docs/ru",
        ):
            docs_ai_delivery.render_outputs(root)

    def test_write_and_check_modes_detect_stale_output(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        self.assertEqual(docs_ai_delivery.main(["--root", str(root), "--write"]), 0)
        self.assertEqual(docs_ai_delivery.main(["--root", str(root), "--check"]), 0)
        (root / docs_ai_delivery.DEFAULT_LLMS_OUTPUT).write_text("stale\n", encoding="utf-8")
        self.assertEqual(docs_ai_delivery.main(["--root", str(root), "--check"]), 1)


if __name__ == "__main__":
    unittest.main()
